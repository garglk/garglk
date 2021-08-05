/*
** oss_glk.c - Glk-specific functions which are only called by
**             Glk-specific TADS functions. I separated them out so that
**             only TADS-specified routines are in os_glk.c.
**
** Notes:
**   10 Jan 99: Initial creation
*/

#include <stdio.h>
#include <string.h>
#include "trd.h"
#include "tio.h"

/* Define all the global variables here via some #define trickery */
#define extern
#include "oss_glk.h"
#undef extern
#include "os_glk.h"

/* ---------------------------------------------------------------------- */
/*
** Conversions from Glk structures to TADS ones and back.  
*/

/*
** Change a TADS prompt type (OS_AFP_*) into a Glk prompt type.
*/
glui32 oss_convert_prompt_type(int type)
{
    if (type == OS_AFP_OPEN)
    return filemode_Read;
    return filemode_ReadWrite;
}

/*
** Change a TADS file type (OSFT*) into a Glk file type.
*/
glui32 oss_convert_file_type(int type)
{
    if (type == OSFTSAVE)
    return fileusage_SavedGame;
    if (type == OSFTLOG || type == OSFTTEXT)
    return fileusage_Transcript;
    return fileusage_Data;
}

/* 
** Change a fileref ID (frefid_t) to a special string and put it in the
** buffer which is passed to it. The string is given by 
**   OSS_FILEREF_STRING_PREFIX + 'nnnnn' + OSS_FILEREF_STRING_SUFFIX
** where 'nnnnn' is the frefid_t pointer converted into a string of decimal
** numbers. This is only really practical for 32-bit pointers; if we use
** 64-bit pointers I'll have to start using a hash table or use hex
** numbers.
*/
glui32 oss_convert_fileref_to_string(frefid_t file_to_convert, char *buffer,
                     int buf_len)
{
    char   temp_string[32];
    glui32 value, i = 0, digit,
    digit_flag = FALSE,     /* Have we put a digit in the string yet? */
    divisor = 1e9;          /* The max 32-bit integer is 4294967295 */

    /* This could probably be done by using sprintf("%s%ld%s") but I don't
       want to risk it. */
    value = (glui32) file_to_convert;
    while (divisor != 1) {
    digit = (char)(value / divisor);
    if (digit != 0 || digit_flag) {     /* This lets us handle, eg, 102 */
        temp_string[i++] = digit + '0';
        digit_flag = TRUE;
    }
    value = value % divisor;
    divisor /= 10;
    }
    temp_string[i++] = (char) value + '0';
    temp_string[i] = 0;
    if (strlen(temp_string) + strlen(OSS_FILEREF_STRING_PREFIX) +
    strlen(OSS_FILEREF_STRING_SUFFIX) > buf_len)
        return FALSE;
    sprintf(buffer, "%s%s%s", OSS_FILEREF_STRING_PREFIX,
        temp_string, OSS_FILEREF_STRING_SUFFIX);
    return TRUE;
}

/* Turn a filename or a special fileref string into an actual fileref.
   Notice that, since Glk doesn't know paths, we take this opportunity to
   call oss_check_path, which should do the OS-dependent path changing
   in the event that the filename contains path information */
frefid_t oss_convert_string_to_fileref(char *buffer, glui32 usage)
{
    char   temp_string[32];
    glui32 value = 0, i, multiplier = 1;

    /* Does the buffer contain a hashed fileref? */
    if (oss_is_string_a_fileref(buffer)) {
    /* If so, we need only decode the string in the middle and return
       its value */
    strcpy(temp_string, buffer + strlen(OSS_FILEREF_STRING_PREFIX));
    i = strlen(temp_string) - strlen(OSS_FILEREF_STRING_SUFFIX);
    temp_string[i] = 0;
    while (i != 0) {
        i--;
        value += ((glui32)(temp_string[i] - '0') * multiplier);
        multiplier *= 10;
    }
    return ((frefid_t) value);
    }
    /* If not, return the new fileref */
    return (glk_fileref_create_by_name(usage, os_get_root_name(buffer), 0));
}

/* Tell us if the passed string is a hashed fileref or not */
int oss_is_string_a_fileref(char *buffer)
{
    if ((strncmp(buffer, OSS_FILEREF_STRING_PREFIX,
        strlen(OSS_FILEREF_STRING_PREFIX)) == 0) &&
    (strncmp(buffer + strlen(buffer) - strlen(OSS_FILEREF_STRING_SUFFIX),
         OSS_FILEREF_STRING_SUFFIX,
         strlen(OSS_FILEREF_STRING_SUFFIX)) == 0))
    return TRUE;
    return FALSE;
}

/* Change a Glk key into a TADS one, using the CMD_xxx codes from
   osifc.h */
unsigned char oss_convert_keystroke_to_tads(glui32 key)
{
    /* Characters 0 - 255 we return per normal */
    if (key <= 255)
    return ((unsigned char)key);
    switch (key) {
    case keycode_Up:
    return CMD_UP;
    case keycode_Down:
    return CMD_DOWN;
    case keycode_Left:
    return CMD_LEFT;
    case keycode_Right:
    return CMD_RIGHT;
    case keycode_PageUp:
    return CMD_PGUP;
    case keycode_PageDown:
    return CMD_PGDN;
    case keycode_Home:
    return CMD_HOME;
    case keycode_End:
    return CMD_END;
    case keycode_Func1:
    return CMD_F1;
    case keycode_Func2:
    return CMD_F2;
    case keycode_Func3:
    return CMD_F3;
    case keycode_Func4:
    return CMD_F4;
    case keycode_Func5:
    return CMD_F5;
    case keycode_Func6:
    return CMD_F6;
    case keycode_Func7:
    return CMD_F7;
    case keycode_Func8:
    return CMD_F8;
    case keycode_Func9:
    return CMD_F9;
    case keycode_Func10:
    return CMD_F10;
    default:
    return 0;
    }
}

/* If a filename contains path information, change dirs to that path.
   Returns TRUE if the path was fiddled with */
int oss_check_path(char *filename)
{
#ifdef GLKUNIX
    char *root, c;

    root = os_get_root_name(filename);
    if (root == filename) return FALSE;
    c = root[0];
    root[0] = 0;     /* Hide the filename for a moment */
    chdir(exec_dir); /* Go to the original dir, in case of relative paths */
    chdir(filename);
    root[0] = c;
    return TRUE;
#else /* GLKUNIX */
    return FALSE;
#endif /* GLKUNIX */
}

/* In case we changed directories in oss_check_path, change back to the
   original executable directory */
void oss_revert_path(void)
{
#ifdef GLKUNIX
    chdir(exec_dir);
#endif /* GLKUNIX */
}


/* ---------------------------------------------------------------------- */
/*
** Input/output routines
*/

/* Open a stream, given a string, usage, and a filemode. tadsusage is the
   TADS filemode (OSFT*); tbusage is either fileusage_TextMode or
   fileusage_BinaryMode (from Glk). */
osfildef *oss_open_stream(char *buffer, glui32 tadsusage, glui32 tbusage, 
              glui32 fmode, glui32 rock)
{
    frefid_t fileref;
    osfildef *osf;
    int      changed_dirs;

    fileref = oss_convert_string_to_fileref(buffer,
        oss_convert_file_type(tadsusage) | tbusage);
    changed_dirs = oss_check_path(buffer);
    osf = glk_stream_open_file(fileref, fmode, rock);
    if (changed_dirs)
    oss_revert_path();
    return osf;
}

/* Process hilighting codes while printing a string */
void oss_put_string_with_hilite(winid_t win, char *str, size_t len)
{
    glk_set_window(win);
    glk_put_buffer(str, len);
}

/* Accept a keystroke in the passed window */
int oss_getc_from_window(winid_t win)
{
    static unsigned char buffered_char = 0;
    int                  i;
    event_t              ev;

    if (buffered_char != 0) {
    i = (int)buffered_char;
    buffered_char = 0;
    return i;
    }
    glk_request_char_event(win);
    do {
    glk_select(&ev);
    if (ev.type == evtype_Arrange)
        oss_draw_status_line();
    } while (ev.type != evtype_CharInput);
    if (ev.val1 == keycode_Return)
    ev.val1 = '\n';
    else if (ev.val1 == keycode_Tab)
    ev.val1 = '\t';
    if (ev.val1 <= 255)
    return ((int) ev.val1);
    /* We got a special character, so handle it appropriately */
    buffered_char = oss_convert_keystroke_to_tads(ev.val1);
    return 0;
}


/* ---------------------------------------------------------------------- */
/*
** Status line handling
*/

void oss_draw_status_line(void)
{
    glui32 width, height, division;

    if (status_win == NULL) return;  /* In case this is a CheapGlk port */

    glk_window_get_size(status_win, &width, &height);
    if (height == 0) return;  /* Don't bother if status is invisible */
    division = width - strlen(status_right) - 1;
    glk_set_window(status_win);
    glk_window_clear(status_win);
    oss_put_string_with_hilite(status_win, status_left, strlen(status_left));
    glk_window_move_cursor(status_win, division, 0);
    glk_put_string(status_right);
}

void oss_change_status_string(char *dest, const char *src, size_t len)
{
    if (len > OSS_STATUS_STRING_LEN - 1)
        len = OSS_STATUS_STRING_LEN - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

void oss_change_status_left(const char *str, size_t len)
{
    oss_change_status_string(status_left, str, len);
    oss_draw_status_line();
}

void oss_change_status_right(const char *str)
{
    oss_change_status_string(status_right, str, strlen(str));
    oss_draw_status_line();
}


/* ---------------------------------------------------------------------- */
/*
** Start-up code
*/

void glk_main(void)
{
    int     err;

    /* Open the story window */
    story_win = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    if (story_win == NULL) {
    return;
    }
    glk_set_window(story_win);

    /* Open the status window (if we're not using the CheapGlk library). If
       it doesn't appear, then quit gracefully */
#ifndef CHEAPGLK
    status_win = glk_window_open(story_win, winmethod_Above |
                 winmethod_Fixed, 1, wintype_TextGrid, 0);
    if (status_win == NULL) {
    glk_put_string("Unable to create status window. Press any key \
to exit.\n");
    os_waitc();
    glk_exit();
    }
#endif /* CHEAPGLK */

    /* See if we can handle timer events. Also zero out time_zero */
    flag_timer_supported = glk_gestalt(gestalt_Timer, 0);
    time_zero = 0;

    /* Do initial set-up of status mode */
    status_mode = OSS_STATUS_MODE_STORY;

#ifdef GLKUNIX
    /* Get the directory in which the executable resides */
    getcwd(exec_dir, 255);
#endif

    os_init(&tads_argc, tads_argv, NULL, NULL, 0);
    err = trdmain(tads_argc, tads_argv, NULL, NULL);
    os_term(err);
}

int os_init(int *argc, char *argv[], const char *prompt,
        char *buf, int bufsiz)
{
    return (0);
}

void os_uninit()
{
}

void os_term(int rc)
{
    glk_exit();
}


/* ---------------------------------------------------------------------- */
/*
** Stuff that some compiler libraries don't have
*/

int memicmp(char *s1, char *s2, int len)
{
    char *x1, *x2;
    int i, result;

    x1 = malloc(len); x2 = malloc(len);

    if (!x1 || !x2) {
    glk_set_window(story_win);
    glk_put_string("memicmp has run out of memory. Quitting.\n");
    os_waitc();
    glk_exit();
    }

    for (i = 0; i < len; i++)
    {
    if (isupper(s1[i]))
        x1[i] = glk_char_to_lower((unsigned char)s1[i]);
    else x1[i] = s1[i];

    if (isupper(s2[i]))
        x2[i] = glk_char_to_lower((unsigned char)s2[i]);
    else x2[i] = s2[i];
    }

    result = memcmp(x1, x2, len);
    free(x1);
    free(x2);
    return result;
}
