/*----------------------------------------------------------------------

  Read line from user, with editing and history

  ----------------------------------------------------------------------*/

#include "readline.h"

#include "sysdep.h"
#include "output.h"
#include "term.h"
#include "exe.h"
#include "save.h"
#include "Location.h"
#include "converter.h"

#include "options.h"


#define LINELENGTH 1000

#ifdef UNITTESTING
/* Transform some stdlib functions to mockable functions */
#define read(fd, buf, n) mocked_read(fd, buf, n)
extern ssize_t mocked_read (int __fd, char *__buf, size_t __nbytes);

#define write(fd, buf, n) mocked_write(fd, buf, n)
extern ssize_t write (int __fd, const void *__buf, size_t __n);

#endif


// TODO Try to split this into more obvious GLK and non-GLK modules
#ifdef HAVE_GLK

#include "options.h"

#include "glk.h"
#include "glkio.h"

#include "resources.h"


#ifdef HAVE_WINGLK
#include "WinGlk.h"

extern HINSTANCE myInstance;	/* Catched by winglk.c */


BOOL CALLBACK AboutDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            EndDialog(hwndDlg, wParam);
            return true;
        }
    }
    return false;
}

#endif



/*======================================================================

  readline() - GLK case

  Read a line from the user, with history and editing

*/

/* TODO - length of user buffer should be used */
bool readline(char buffer[])
{
    event_t event;
    static bool readingCommands = false;
    static frefid_t commandFileRef;
    static strid_t commandFile;
#ifdef HAVE_WINGLK
    static frefid_t commandLogFileRef;
    static frefid_t transcriptFileRef;
    INT_PTR e;
#endif

    if (readingCommands) {
        if (glk_get_line_stream(commandFile, buffer, 255) == 0) {
            glk_stream_close(commandFile, NULL);
            readingCommands = false;
            goto endOfCommandFile;
        } else {
            char *converted = ensureInternalEncoding(buffer);
            glk_set_style(style_Input);
            printf(converted);
            glk_set_style(style_Normal);
            free(converted);
        }
    } else {
    endOfCommandFile:
        glk_request_line_event(glkMainWin, buffer, 255, 0);
        /* FIXME: buffer size should be infallible: all existing calls use 256 or
           80 character buffers, except parse which uses LISTLEN (currently 100)
        */
        do
            {
                glk_select(&event);
                switch (event.type) {
                case evtype_Arrange:
                    statusline();
                    break;
#ifdef HAVE_WINGLK
                case winglk_evtype_GuiInput:
                    switch (event.val1) {
                    case ID_MENU_RESTART:
                        restartGame();
                        break;
                    case ID_MENU_SAVE:
                        glk_set_style(style_Input);
                        printf("save\n");
                        glk_set_style(style_Normal);
                        save();
                        para();
                        printf("> ");
                        break;
                    case ID_MENU_RESTORE:
                        glk_set_style(style_Input);
                        printf("restore\n");
                        glk_set_style(style_Normal);
                        restore();
                        look();
                        para();
                        printf("> ");
                        break;
                    case ID_MENU_RECORD:
                        if (commandLogOption) {
                            glk_stream_close(commandLogFile, NULL);
                            commandLogOption = false;
                        }
                        commandLogFileRef = glk_fileref_create_by_prompt(fileusage_InputRecord+fileusage_TextMode, filemode_Write, 0);
                        if (commandLogFileRef == NULL) break;
                        commandLogFile = glk_stream_open_file(commandLogFileRef, filemode_Write, 0);
                        if (commandLogFile != NULL)
                            commandLogOption = true;
                        break;
                    case ID_MENU_PLAYBACK:
                        commandFileRef = glk_fileref_create_by_prompt(fileusage_InputRecord+fileusage_TextMode, filemode_Read, 0);
                        if (commandFileRef == NULL) break;
                        commandFile = glk_stream_open_file(commandFileRef, filemode_Read, 0);
                        if (commandFile != NULL)
                            if (glk_get_line_stream(commandFile, buffer, 255) != 0) {
                                readingCommands = true;
                                printf(buffer);
                                return true;
                            }
                        break;
                    case ID_MENU_TRANSCRIPT:
                        if (transcriptOption) {
                            glk_stream_close(transcriptFile, NULL);
                            transcriptOption = false;
                        }
                        transcriptFileRef = glk_fileref_create_by_prompt(fileusage_Transcript+fileusage_TextMode, filemode_Write, 0);
                        if (transcriptFileRef == NULL) break;
                        transcriptFile = glk_stream_open_file(transcriptFileRef, filemode_Write, 0);
                        if (transcriptFile != NULL) {
                            transcriptOption = true;
                            glk_put_string_stream(transcriptFile, "> ");
                        }
                        break;
                    case ID_MENU_ABOUT:
                        e = DialogBox(myInstance, MAKEINTRESOURCE(IDD_ABOUT), NULL, &AboutDialogProc);
                        (void)e;
                        break;
                    }
                    break;
#endif
                }
            } while (event.type != evtype_LineInput);
        if (buffer[0] == '@') {
            buffer[event.val1] = 0;
            commandFileRef = glk_fileref_create_by_name(fileusage_InputRecord+fileusage_TextMode, &buffer[1], 0);
            commandFile = glk_stream_open_file(commandFileRef, filemode_Read, 0);
            if (commandFile != NULL)
                if (glk_get_line_stream(commandFile, buffer, 255) != 0) {
                    char *converted = ensureInternalEncoding(buffer);
                    glk_set_style(style_Input);
                    printf(converted);
                    glk_set_style(style_Normal);
                    free(converted);
                    readingCommands = true;
                }
        } else
            buffer[event.val1] = 0;
    }
    return true;
}

#else
/*---------------------------------------------------------------------------------------*/
/* Non-GLK terminal I/O using simple movements and overwrites with spaces and backspaces */
/*---------------------------------------------------------------------------------------*/

#include "sysdep.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_TERMIOS
#include <termios.h>
#endif

#include <unistd.h>

#include "memory.h"


#ifdef HAVE_TERMIOS
/*----------------------------------------------------------------------

  termio handling

  ----------------------------------------------------------------------*/

static struct termios term;

static void newtermio()
{
    if (isatty(STDIN_FILENO)) {
        struct termios newterm;
        tcgetattr(STDIN_FILENO, &term);
        newterm=term;
        newterm.c_lflag&=~(ECHO|ICANON);
        newterm.c_cc[VMIN]=1;
        newterm.c_cc[VTIME]=0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newterm);
    }
}

static void restoretermio()
{
    if (isatty(STDIN_FILENO))
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

#endif


/*----------------------------------------------------------------------*\

  Global character buffers etc.

  ----------------------------------------------------------------------*/

static char buffer[LINELENGTH+1];
static int bufidx;

static unsigned char *history[HISTORYLENGTH];
static int histidx;		/* Index where to store next history */
static int histp;		/* Points to the history recalled last */

static unsigned char ch;
static bool endOfInput;
static bool commandLineChanged;
static bool insertMode = true;


/*----------------------------------------------------------------------

  Character map types and maps

  ----------------------------------------------------------------------*/

typedef struct {unsigned char min, max; void (*hook)(char ch);} KeyMap;

/* Forward declaration of hooks */
static void escHook(char ch);
static void normalCh(char ch);
static void arrowHook(char ch);
static void upArrow(char ch);
static void downArrow(char ch);
static void rightArrow(char ch);
static void leftArrow(char ch);
static void insertToggle(char ch);
static void newLine(char ch);
static void delFwd(char ch);
static void delBwd(char ch);
static void execute(KeyMap map[], unsigned char ch);

#ifdef __windows__

static KeyMap keymap[] = {
    {0x00, 0x01, NULL},
    {0x02, 0x02, leftArrow},
    {0x03, 0x05, NULL},
    {0x06, 0x06, rightArrow},
    {0x07, 0x07, NULL},
    {0x08, 0x08, delBwd},
    {0x09, 0x09, NULL},
    {0x0a, 0x0a, newLine},
    {0x1b, 0x1b, escHook},
    {0x1c, 0x7e, normalCh},
    {0x7f, 0x7f, delFwd},
    {0x80, 0xff, normalCh},
    {0x00, 0x00, NULL}
};

static KeyMap escmap[] = {
    {0x00, 0x5a, NULL},
    {0x5b, 0x5b, arrowHook},
    {0x5c, 0xff, NULL},
    {0x00, 0x00, NULL}
};

static KeyMap arrowmap[] = {
    {0x00, 0x31, NULL},
    {0x32, 0x32, insertToggle},
    {0x33, 0x40, NULL},
    {0x41, 0x41, upArrow},
    {0x42, 0x42, downArrow},
    {0x43, 0x43, rightArrow},
    {0x44, 0x44, leftArrow},
    {0x45, 0xff, NULL},
    {0x00, 0x00, NULL}
};

#else

/* Not windows */

static void escapeBracket3Hook(char ch);
static void ignoreCh(char ch) {}

static KeyMap keymap[] = {
    {0x00, 0x03, NULL},
    {0x04, 0x04, delFwd},
    {0x05, 0x07, delBwd},
    {0x08, 0x08, delBwd},
    {0x09, 0x09, NULL},
    {0x0a, 0x0a, newLine},
    {0x0d, 0x0d, ignoreCh},
    {0x1b, 0x1b, escHook},
    {0x1c, 0x7e, normalCh},
    {0x7f, 0x7f, delBwd},
    {0x80, 0xff, normalCh},
    {0x00, 0x00, NULL}
};

static KeyMap escmap[] = {
    {0x00, 0x5a, NULL},
    {0x5b, 0x5b, arrowHook},
    {0x5c, 0xff, NULL},
    {0x00, 0x00, NULL}
};

static KeyMap arrowmap[] = {
    {0x00, 0x31, NULL},
    {0x32, 0x32, insertToggle},
    {0x33, 0x33, escapeBracket3Hook},
    {0x33, 0x40, NULL},
    {0x41, 0x41, upArrow},
    {0x42, 0x42, downArrow},
    {0x43, 0x43, rightArrow},
    {0x44, 0x44, leftArrow},
    {0x45, 0xff, NULL},
    {0x00, 0x00, NULL}
};

static KeyMap escapeBracket3map[] = {
    {0x7e, 0x7e, delFwd},
    {0x00, 0x00, NULL}
};


static void escapeBracket3Hook(char ch) {
    int rc;
    (void)rc;                   /* UNUSED */
    rc = read(STDIN_FILENO, &ch, 1);
    execute(escapeBracket3map, ch);
}

#endif


static void stripNewline(char *buffer) {
    int len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n')
        buffer[len-1] = '\0';
}

static bool is_utf8_prefix(uchar ch) {
    return (ch&0xC0) == 0xC0;
}

static bool is_utf8_follow(uchar ch) {
    return (ch&0xC0) == 0x80;
}

/* Number of characters counting multi-byte characters as one (as opposed to bytes) in a null-terminated array */
int character_length(uchar *string) {
    int count = 0;

    for (int i = 0; string[i] != '\0'; i++) {
        if (encodingOption == ENCODING_UTF && is_utf8_prefix(string[i])) {
            count++;
            i++;
            do {
                i++;
            } while ((string[i] != '\0') && is_utf8_follow(string[i]));
            i--;
        } else {
            count++;
        }
    }
    return count;
}

/* Number of bytes (as opposed to characters e.g. UTF-8) in a null-terminated array */
static int byte_length(char *bytes) {
    return strlen(bytes);
}



#ifdef UNITTESTING
#include <cgreen/mocks.h>
static void doBeep(void) { mock(); }
#else
static void doBeep(void)
{
    int rc;
    (void)rc;                   /* UNUSED */
    rc = write(1, "\7", 1);
}
#endif

static void moveCursorLeft(void)
{
    int rc;
    (void)rc;                   /* UNUSED */
    rc = write(1, "\b", 1);
}


static void writeBlank() {
    int rc;
    (void)rc;               /* UNUSED */
    rc = write(1, " ", 1);
}



static void erase()
{
    int i;

    /* Backup to beginning of text */
    /* TODO: moveCursorBackOver(n); including UTF-8 chars */
    for (i = bufidx; i > 0; i--) {
        if (encodingOption != ENCODING_UTF || !is_utf8_follow(buffer[i]))
            moveCursorLeft();
    }

    /* Overwrite with spaces */
    for (i = 0; i < character_length((uchar *)buffer); i++) {
        writeBlank();
    }

    /* Backup to beginning of text */
    for (i = 0; i < character_length((uchar *)buffer); i++)
        moveCursorLeft();
}

/*----------------------------------------------------------------------*\

  Character handling hook functions

  ----------------------------------------------------------------------*/

static void execute(KeyMap map[], unsigned char ch)
{
    int i = 0;

    // TODO Make this loop until end of KeyMap (0,0,NULL) is found instead
    // then we can remove any NULL entries in the keymaps
    for (i = 0; i <= 256; i++) {
        if (i > 0 && map[i].min == 0x00) break; /* End marker is a 0,0,NULL */
        if (map[i].min <= ch && ch <= map[i].max) {
            if (map[i].hook != NULL) {
                map[i].hook(ch);
                return;
            } else
                doBeep();
        }
    }
    doBeep();
}


static void upArrow(char ch)
{
    int rc;
    (void)rc;                   /* UNUSED */
    /* Is there more history ? */
    if (history[(histp+HISTORYLENGTH-1)%HISTORYLENGTH] == NULL ||
        (histp+HISTORYLENGTH-1)%HISTORYLENGTH == histidx) {
        rc = write(1, "\7", 1);
        return;
    }

    erase();

    /* Backup history pointer */
    histp = (histp+HISTORYLENGTH-1)%HISTORYLENGTH;

    /* Copy the history and write it */
    strcpy((char *)buffer, (char *)history[histp]);
    bufidx = strlen((char *)buffer);
    rc = write(1, (void *)buffer, strlen((char *)buffer));
}


static void downArrow(char ch)
{
    /* Is there more history ? */
    if (histp == histidx) {
        int rc;
        (void)rc;                   /* UNUSED */
        rc = write(1, "\7", 1);
        return;
    }

    erase();

    /* Advance history pointer */
    histp = (histp+1)%HISTORYLENGTH;

    /* If we are not at the most recent history entry, copy history and write it */
    if (histp != histidx) {
        int rc;
        (void)rc;                   /* UNUSED */
        strcpy((char *)buffer, (char *)history[histp]);
        bufidx = strlen((char *)buffer);
        rc = write(1, (void *)buffer, strlen((char *)buffer));
    } else {
        bufidx = 0;
        buffer[0] = '\0';
    }
}


static int byteLengthOfCharacterAt(int bufidx) {
    int count = 1;
    if (encodingOption == ENCODING_UTF) {
        if (is_utf8_prefix((uchar)buffer[bufidx])) {
            /* Next character is a multi-byte */
            while (is_utf8_follow((uchar)buffer[bufidx+1])) {
                count++;
                bufidx++;
            }
        }
    }

    return count;
}



static void rightArrow(char ch)
{
    if (bufidx > LINELENGTH || buffer[bufidx] == '\0')
        doBeep();
    else {
        int rc;
        (void)rc;                   /* UNUSED */
        int count = byteLengthOfCharacterAt(bufidx);
        rc = write(1, (void *)&buffer[bufidx], count);
        bufidx+=count;
    }
}


static int byteLengthOfPreviousCharacter(int idx) {
    int count = 1;
    if (encodingOption == ENCODING_UTF) {
        /* This is moving backwards over UTF-8 characters */
        if (is_utf8_follow(buffer[idx-1]))
            /* If top two bits are 10 then it's a UTF-8 follow-up byte, so backup till we find prefix */
            while (is_utf8_follow(buffer[--idx]))
                count++;
    }
    return count;
}



static void leftArrow(char ch)
{
    if (bufidx == 0)
        doBeep();
    else {
        bufidx -= byteLengthOfPreviousCharacter(bufidx);
        moveCursorLeft();       /* And it's still just one character on the screen */
    }
}


static void insertToggle(char ch)
{
    int rc;
    (void)rc;                   /* UNUSED */
    rc = read(STDIN_FILENO, &ch, 1);
    if (ch != 'z')
        doBeep();
    else
        insertMode = !insertMode;
}


static void shiftBufferLeftFrom(int idx, int offset) {
    for (int i = 0; i <= byte_length((char *)&buffer[idx+offset])+1; i++) {
        buffer[idx+i] = buffer[idx+i+offset];
    }
}


static void writeBufferFrom(int idx) {
    int rc;
    (void)rc;                   /* UNUSED */
    rc = write(1, (void *)&buffer[idx], byte_length(&buffer[idx]));
}


static void moveCursorFromEndBackTo(int idx) {
    for (int i = 0; i <= character_length((uchar *)&buffer[idx]); i++)
        moveCursorLeft();
}



static void delBwd(char ch)
{
    (void)ch;                   /* UNUSED - to match other keymap functions */

    if (bufidx == 0)
        doBeep();
    else {
        int deleted_length = 1;

        commandLineChanged = true;

        moveCursorLeft();            /* Move backwards over the deleted char */

        deleted_length = byteLengthOfPreviousCharacter(bufidx);
        bufidx -= deleted_length;

        /* Move up any remaning characters in the buffer ... */
        shiftBufferLeftFrom(bufidx, deleted_length);

        /* ... on the screen, print the rest of the string ... */
        writeBufferFrom(bufidx);
        writeBlank();           /* And erase the character at the end still left on screen */

        moveCursorFromEndBackTo(bufidx);   /* Move back to current position */
    }
}

static void delFwd(char ch)
{
    if (bufidx > LINELENGTH || buffer[bufidx] == '\0')
        doBeep();
    else {
        int i;

        commandLineChanged = true;

        int deleted_length = byteLengthOfCharacterAt(bufidx);
        shiftBufferLeftFrom(bufidx, deleted_length);
        writeBufferFrom(bufidx);
        writeBlank();
        for (i = 0; i <= character_length((uchar *)&buffer[bufidx]); i++)
            moveCursorLeft();
    }
}

static void escHook(char ch) {
    int rc;
    (void)rc;                   /* UNUSED */
    rc = read(STDIN_FILENO, &ch, 1);
    execute(escmap, ch);
}

static void arrowHook(char ch) {
    int rc;
    (void)rc;                   /* UNUSED */
    rc = read(STDIN_FILENO, &ch, 1);
    execute(arrowmap, ch);
}

static void newLine(char ch)
{
    int rc;
    (void)rc;                   /* UNUSED */

    endOfInput = true;
    rc = write(1, "\n", 1);

    /* If the input is not the same as the previous, save it in the history */
    if (commandLineChanged && strlen((char *)buffer) > 0) {
        if (history[histidx] == NULL)
            history[histidx] = (unsigned char *)allocate(LINELENGTH+1);
        strcpy((char *)history[histidx], (char *)buffer);
        histidx = (histidx+1)%HISTORYLENGTH;
    }
}


static void shiftBufferRightFrom(int idx, int offset) {
    for (int i = byte_length((char *)buffer); i >= idx; i--)
        buffer[i+offset] = buffer[i];
    buffer[byte_length(buffer)+1] = '\0';
}


static void moveCursorLeftTo(int idx) {
    for (int i = character_length((uchar *)&buffer[idx]); i > 0; i--)
        moveCursorLeft();
}


static void insertCh(uchar bytes[], int length) {
    int rc;
    (void)rc;

    /* Make room for the bytes @bufidx */
    shiftBufferRightFrom(bufidx, length);

    /* Fill the buffer with the collected bytes */
    for (int i=0; i < length; i++)
        buffer[bufidx+i] = bytes[i];
    writeBufferFrom(bufidx);

    bufidx += length;
    moveCursorLeftTo(bufidx);
}


static void overwriteCh(uchar bytes[], int length) {

    int current_length = byteLengthOfCharacterAt(bufidx);

    if (length < current_length)
        shiftBufferLeftFrom(bufidx, current_length-length);
    else
        shiftBufferRightFrom(bufidx, length-current_length);

    /* Fill the buffer with the collected bytes ... */
    for (int i=0; i < length; i++) {
        buffer[bufidx++] = bytes[i];
    }

    /* ... and write them */
    int rc = write(1, bytes, length);
    (void)rc;
}


static int utf8_follow_bytes(uchar ch) {
    /* 110xxxxx -> 1 extra byte */
    /* 1110xxxx -> 2 extra bytes */
    /* 11110xxx -> 3 extra bytes */
    if ((ch & 0xE0) == 0xC0)
        return 2;
    else if ((ch & 0xF0) == 0xE0)
        return 3;
    else if ((ch & 0xF8) == 0xF0)
        return 4;
    return -1;
}



static void normalCh(char ch) {
    if (bufidx > LINELENGTH) {
        doBeep();
        return;
    }
    int rc;
    (void)rc;               /* UNUSED */
    static uchar bytes[4];
    static int length;
    static int bytes_left;

    if (encodingOption == ENCODING_UTF)  {
        if (is_utf8_prefix(ch)) {
            /* Start collecting bytes in the array */
            length = utf8_follow_bytes(ch);
            bytes_left = length-1;
            bytes[0] = ch;
            return;
        } else if (bytes_left > 0) {
            bytes_left--;
            bytes[length-1 - bytes_left] = ch;
            if (bytes_left != 0)
                return;
        } else {
            length = 1;
            bytes[0] = ch;
        }
    } else {
        length = 1;
        bytes[0] = ch;
    }

    if (buffer[bufidx] == '\0' || insertMode) {
        insertCh(bytes, length);
    } else {
        overwriteCh(bytes, length);
    }

    commandLineChanged = true;
}

#ifdef __win__
#include <windows.h>
#include <winbase.h>
#include <wincon.h>
#endif

/*----------------------------------------------------------------------*/
static void echoOff()
{
#ifdef HAVE_TERMIOS
    newtermio();
#else
#ifdef __win__

    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);

    (void) SetConsoleMode(handle, 0);

#endif
#endif
}


/*----------------------------------------------------------------------*/
static void echoOn()
{
#ifdef HAVE_TERMIOS
    restoretermio();
#else
#ifdef __win__

    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    (void) SetConsoleMode(handle, ENABLE_ECHO_INPUT);

#endif
#endif
}

#include <errno.h>

/*======================================================================

  readline() - non-GLK case

  Read a line from the user, with history, editing and command
  reading from file

  NOTE that the raw characters (and thus the maps) are in the
  native/external encoding for the platform.

*/

/* TODO - length of user buffer should be used */
bool readline(char usrbuf[])
{
    static bool readingCommands = false;
    static FILE *commandFile;
    static int previousEncoding;
    static bool firstInput = true;
    static uchar BOM[3] = {0xEF,0xBB,0xBF};

    if (readingCommands) {
        fflush(stdout);
        /* TODO: Arbitrarily using 255 for buffer size */
        if (!fgets(buffer, 255, commandFile)) {
            fclose(commandFile);
            readingCommands = false;
            encodingOption = previousEncoding;
            buffer[0] = '\0';
            goto endOfCommandFile;
        } else {
            printf("%s", buffer);
        }
    } else {
    endOfCommandFile:
        fflush(stdout);
        bufidx = 0;
        histp = histidx;
        buffer[0] = '\0';
        commandLineChanged = true;
        echoOff();
        //printf("ERROR = %s\n", strerror(errno));
        endOfInput = false;
        while (!endOfInput) {
            int count = read(STDIN_FILENO, (void *)&ch, 1);
            if (count == 0) {
                /* We did not get any character at all? EOF? */
                echoOn();
                return false;
            }
            execute(keymap, ch);
        }
        echoOn();

        if (buffer[0] == '@')
            if ((commandFile = fopen(&buffer[1], "r")) != NULL)
                if (fgets(buffer, 255, commandFile) != NULL) {
                    readingCommands = true;
                    if (memcmp(buffer, BOM, 3) == 0) {
                        previousEncoding = encodingOption;
                        encodingOption = ENCODING_UTF;
                        memmove(buffer, &buffer[3], strlen(buffer)-3+1);
                    }
                    printf("%s", buffer);
                }
        /* Reset line counter only if we read actual player input */
        lin = 1;
    }
    stripNewline(buffer);
    if (firstInput) {
        firstInput = false;
        if (memcmp(buffer, BOM, 3) == 0) {
            encodingOption = ENCODING_UTF;
            memmove(buffer, &buffer[3], strlen(buffer)-3+1);
        }
    }
    char *converted = ensureInternalEncoding(buffer);
    strcpy(usrbuf, converted);
    free(converted);
    return true;
}

#endif
