/*----------------------------------------------------------------------*\

  Read line from user, with editing and history

  \*----------------------------------------------------------------------*/

#include "readline.h"

#include "sysdep.h"
#include "output.h"
#include "term.h"
#include "exe.h"
#include "save.h"
#include "Location.h"

#define LINELENGTH 1000

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
            return TRUE;
        }
    }
    return FALSE;
}

#endif


/*======================================================================

  readline()

  Read a line from the user, with history and editing

*/

/* TODO - length of user buffer should be used */
bool readline(char buffer[])
{
    event_t event;
    static bool readingCommands = FALSE;
    static frefid_t commandFileRef;
    static strid_t commandFile;
#ifdef HAVE_WINGLK
    static frefid_t logFileRef;
    INT_PTR e;
#endif

    if (readingCommands) {
        if (glk_get_line_stream(commandFile, buffer, 255) == 0) {
            glk_stream_close(commandFile, NULL);
            readingCommands = FALSE;
        } else {
            glk_set_style(style_Input);
            printf(buffer);
            glk_set_style(style_Normal);
        }
    } else {
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
                        if (transcriptOption || logOption) {
                            glk_stream_close(logFile, NULL);
                            transcriptOption = FALSE;
                            logOption = FALSE;
                        }
                        logFileRef = glk_fileref_create_by_prompt(fileusage_InputRecord+fileusage_TextMode, filemode_Write, 0);
                        if (logFileRef == NULL) break;
                        logFile = glk_stream_open_file(logFileRef, filemode_Write, 0);
                        if (logFile != NULL)
                            logOption = TRUE;
                        break;
                    case ID_MENU_PLAYBACK:
                        commandFileRef = glk_fileref_create_by_prompt(fileusage_InputRecord+fileusage_TextMode, filemode_Read, 0);
                        if (commandFileRef == NULL) break;
                        commandFile = glk_stream_open_file(commandFileRef, filemode_Read, 0);
                        if (commandFile != NULL)
                            if (glk_get_line_stream(commandFile, buffer, 255) != 0) {
                                readingCommands = TRUE;
                                printf(buffer);
                                return TRUE;
                            }
                        break;
                    case ID_MENU_TRANSCRIPT:
                        if (transcriptOption || logOption) {
                            glk_stream_close(logFile, NULL);
                            transcriptOption = FALSE;
                            logOption = FALSE;
                        }
                        logFileRef = glk_fileref_create_by_prompt(fileusage_Transcript+fileusage_TextMode, filemode_Write, 0);
                        if (logFileRef == NULL) break;
                        logFile = glk_stream_open_file(logFileRef, filemode_Write, 0);
                        if (logFile != NULL) {
                            transcriptOption = TRUE;
                            glk_put_string_stream(logFile, "> ");
                        }
                        break;
                    case ID_MENU_ABOUT:
                        e = DialogBox(myInstance, MAKEINTRESOURCE(IDD_ABOUT), NULL, &AboutDialogProc);
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
                    readingCommands = TRUE;
                    glk_set_style(style_Input);
                    printf(buffer);
                    glk_set_style(style_Normal);
                }
        } else
            buffer[event.val1] = 0;
    }
    return TRUE;
}

#else

#include "sysdep.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_TERMIO
#include <termios.h>
#endif

#ifdef __PACIFIC__
#include <unixio.h>
#else
#include <unistd.h>
#endif

#include "readline.h"

#include "memory.h"


#ifdef HAVE_TERMIO
/*----------------------------------------------------------------------

  termio handling

  ----------------------------------------------------------------------*/

static struct termios term;

static void newtermio()
{
    struct termios newterm;
    tcgetattr(0, &term);
    newterm=term;
    newterm.c_lflag&=~(ECHO|ICANON);
    newterm.c_cc[VMIN]=1;
    newterm.c_cc[VTIME]=0;
    tcsetattr(0, TCSANOW, &newterm);
}

static void restoretermio()
{
    tcsetattr(0, TCSANOW, &term);
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
static int endOfInput = 0;
static bool change;
static bool insert = TRUE;


/*----------------------------------------------------------------------*\

  Character map types and maps

  \*----------------------------------------------------------------------*/

typedef struct {unsigned char min, max; void (*hook)(char ch);} KeyMap;

/* Forward declaration of hooks */
static void escHook(char ch);
static void insertCh(char ch);
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

#ifdef __unix__
/* Only used on unix */
static void escapeBracket3Hook(char ch);

static KeyMap keymap[] = {
    {0x00, 0x03, NULL},
    {0x04, 0x04, delFwd},
    {0x05, 0x07, delBwd},
    {0x08, 0x08, delBwd},
    {0x09, 0x09, NULL},
    {0x0a, 0x0a, newLine},
    {0x1b, 0x1b, escHook},
    {0x1c, 0x7e, insertCh},
#ifdef __macosx__
    {0x7f, 0x7f, delBwd},		/* Standard UNIX : delFwd, MACOSX : delBwd */
#elif __linux__
    {0x7f, 0x7f, delBwd},		/* Standard UNIX : delFwd, MACOSX : delBwd */
#else
    {0x7f, 0x7f, delFwd},		/* Standard UNIX : delFwd, MACOSX : delBwd */
#endif
    {0x80, 0xff, insertCh},
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
    read(0, &ch, 1);
    execute(escapeBracket3map, ch);
}

#endif

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
    {0x1c, 0x7e, insertCh},
    {0x7f, 0x7f, delFwd},
    {0x80, 0xff, insertCh},
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

#endif

#ifdef __dos__
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
    {0x1c, 0x7e, insertCh},
    {0x7f, 0x7f, delFwd},
    {0x80, 0xff, insertCh},
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

#endif


static void doBeep(void)
{
    write(1, "\7", 1);
}


static void backspace(void)
{
    write(1, "\b", 1);
}


static void erase()
{
    int i;

    for (i = 0; i < bufidx; i++) backspace(); /* Backup to beginning of text */
    for (i = 0; i < strlen((char *)buffer); i++) write(1, " ", 1); /* Erase all text */
    for (i = 0; i < strlen((char *)buffer); i++) backspace(); /* Backup to beginning of text */
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
    /* Is there more history ? */
    if (history[(histp+HISTORYLENGTH-1)%HISTORYLENGTH] == NULL ||
        (histp+HISTORYLENGTH-1)%HISTORYLENGTH == histidx) {
        write(1, "\7", 1);
        return;
    }

    erase();

    /* Backup history pointer */
    histp = (histp+HISTORYLENGTH-1)%HISTORYLENGTH;

    /* Copy the history and write it */
    strcpy((char *)buffer, (char *)history[histp]);
    bufidx = strlen((char *)buffer);
    write(1, (void *)buffer, strlen((char *)buffer));

}


static void downArrow(char ch)
{
    /* Is there more history ? */
    if (histp == histidx) {
        write(1, "\7", 1);
        return;
    }

    erase();

    /* Advance history pointer */
    histp = (histp+1)%HISTORYLENGTH;

    /* If we are not at the most recent history entry, copy history and write it */
    if (histp != histidx) {
        strcpy((char *)buffer, (char *)history[histp]);
        bufidx = strlen((char *)buffer);
        write(1, (void *)buffer, strlen((char *)buffer));
    } else {
        bufidx = 0;
        buffer[0] = '\0';
    }
}


static void rightArrow(char ch)
{
    if (bufidx > LINELENGTH || buffer[bufidx] == '\0')
        doBeep();
    else {
        write(1, (void *)&buffer[bufidx], 1);
        bufidx++;
    }
}


static void leftArrow(char ch)
{
    if (bufidx == 0)
        doBeep();
    else {
        bufidx--;
        backspace();
    }
}


static void insertToggle(char ch)
{
    read(0, &ch, 1);
    if (ch != 'z')
        doBeep();
    else
        insert = !insert;
}


static void delBwd(char ch)
{
    if (bufidx == 0)
        doBeep();
    else {
        int i;

        change = TRUE;
        backspace();
        bufidx--;
        for (i = 0; i <= strlen((char *)&buffer[bufidx+1]); i++)
            buffer[bufidx+i] = buffer[bufidx+1+i];
        write(1, (void *)&buffer[bufidx], strlen((char *)&buffer[bufidx]));
        write(1, " ", 1);
        for (i = 0; i <= strlen((char *)&buffer[bufidx]); i++) backspace();
    }
}

static void delFwd(char ch)
{
    if (bufidx > LINELENGTH || buffer[bufidx] == '\0')
        doBeep();
    else {
        int i;

        change = TRUE;
        strcpy((char *)&buffer[bufidx], (char *)&buffer[bufidx+1]);
        write(1, (void *)&buffer[bufidx], strlen((char *)&buffer[bufidx]));
        write(1, " ", 1);
        for (i = 0; i <= strlen((char *)&buffer[bufidx]); i++) backspace();
    }
}

static void escHook(char ch) {
    read(0, &ch, 1);
    execute(escmap, ch);
}

static void arrowHook(char ch) {
    read(0, &ch, 1);
    execute(arrowmap, ch);
}

static void newLine(char ch)
{
    endOfInput = 1;
    write(1, "\n", 1);

    /* If the input is not the same as the previous, save it in the history */
    if (change && strlen((char *)buffer) > 0) {
        if (history[histidx] == NULL)
            history[histidx] = (unsigned char *)allocate(LINELENGTH+1);
        strcpy((char *)history[histidx], (char *)buffer);
        histidx = (histidx+1)%HISTORYLENGTH;
    }
}


static void insertCh(char ch) {
    if (bufidx > LINELENGTH)
        doBeep();
    else {
        /* If at end advance the NULL */
        if (buffer[bufidx] == '\0')
            buffer[bufidx+1] = '\0';
        else if (insert) {
            int i;

            /* If insert mode is on, move the characters ahead */
            for (i = strlen((char *)buffer); i >= bufidx; i--)
                buffer[i+1] = buffer[i];
            write(1, (void *)&buffer[bufidx], strlen((char *)&buffer[bufidx]));
            for (i = strlen((char *)&buffer[bufidx]); i > 0; i--) backspace();
        }
        change = TRUE;
        buffer[bufidx] = ch;
        write(1, &ch, 1);
        bufidx++;
    }
}

#ifdef __win__
#include <windows.h>
#include <winbase.h>
#include <wincon.h>
#endif

/*----------------------------------------------------------------------*/
static void echoOff()
{
#ifdef HAVE_TERMIO
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
#ifdef HAVE_TERMIO
    restoretermio();
#else
#ifdef __win__

    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    (void) SetConsoleMode(handle, ENABLE_ECHO_INPUT);

#endif
#endif
}

static void stripNewline(char *buffer) {
	if (buffer[strlen(buffer)-1] == '\n')
		buffer[strlen(buffer)-1] = '\0';
}

static void copyToUserBuffer(char *usrbuf, char *buffer) {
	strcpy(usrbuf, (char *)buffer);
}

/*======================================================================

  readline()

  Read a line from the user, with history, editing and command
  reading from file

*/

/* 4f - length of user buffer should be used */
bool readline(char usrbuf[])
{
    static bool readingCommands = FALSE;
    static FILE *commandFile;

    if (readingCommands) {
        fflush(stdout);
        if (!fgets(buffer, 255, commandFile)) {
            fclose(commandFile);
            readingCommands = FALSE;
        } else
            printf("%s", buffer);
    } else {
        fflush(stdout);
        bufidx = 0;
        histp = histidx;
        buffer[0] = '\0';
        change = TRUE;
        echoOff();
        endOfInput = 0;
        while (!endOfInput) {
            if (read(0, (void *)&ch, 1) != 1) {
                echoOn();
                return FALSE;
            }
            execute(keymap, ch);
        }
        echoOn();

        if (buffer[0] == '@')
            if ((commandFile = fopen(&buffer[1], "r")) != NULL)
                if (fgets(buffer, 255, commandFile)) {
                    readingCommands = TRUE;
                    printf("%s", buffer);
                }
        /* Reset line counter only if we read actual player input */
        lin = 1;
    }
    stripNewline(buffer);
    copyToUserBuffer(usrbuf, buffer);
    return TRUE;
}

#endif
