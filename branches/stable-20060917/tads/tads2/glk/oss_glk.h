/*
** oss_glk.h - Glk-specific function definitons which are only called by
**             Glk-specific TADS functions. Think of this as another layer
**             between the TADS OS-independent functions and Glk.
**
** Notes:
**   10 Jan 99: Initial creation
*/

#ifndef OSS_INCLUDED
#define OSS_INCLUDED

#include "glk.h"

/* Just in case */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


/* ------------------------------------------------------------------------ */
/*
** Some global variables
*/

/* The initial arguments to TADS. Each port must set these up appropriately
   in its OS-specific startup code */
extern int tads_argc;
extern char **tads_argv;

/* The windows */
extern winid_t status_win, story_win;

/* The status line info */
#define OSS_STATUS_STRING_LEN 80
extern char status_left[OSS_STATUS_STRING_LEN],
    status_right[OSS_STATUS_STRING_LEN];
extern int status_mode;

/* Can the current interpreter support timer events? */
extern glui32 flag_timer_supported;

/* System clock handling */
extern glui32 time_zero;

#ifdef GLKUNIX
/* The directory in which the executable resides */
extern char exec_dir[255];
#endif


/* ------------------------------------------------------------------------ */
/*
** Conversions from Glk structures to TADS ones and back.
*/

/* Change a TADS prompt type (OS_AFP_*) into a Glk prompt type */
glui32 oss_convert_prompt_type(int type);

/* Change a TADS file type (OSFT*) into a Glk file type */
glui32 oss_convert_file_type(int type);

/* The character (or characters) which mark the beginning of a
   special fileref string.  The important thing is that the string
   be one that is either not allowed in filenames on your platform or
   is unlikely to be the first part of a filename. */
#define OSS_FILEREF_STRING_PREFIX ":"

/* The character (or characters) which mark the end of a special
   fileref string. Using this and OSS_FILEREF_STRING_PREFIX, you
   should be able to come up with something which forms an invalid
   filename */
#define OSS_FILEREF_STRING_SUFFIX ""

/* Change a fileref ID (frefid_t) to a special string and put it in the
   buffer which is passed to it. */
glui32 oss_convert_fileref_to_string(frefid_t file, char *buffer,
                                     int buf_len);

/* Turn a filename or a special fileref string into an actual fileref */
frefid_t oss_convert_string_to_fileref(char *buffer, glui32 usage);

/* Tell us if the passed string is a hashed fileref or not */
int oss_is_string_a_fileref(char *buffer);

/* Convert a Glk keystroke into a TADS key */
unsigned char oss_convert_keystroke_to_tads(glui32 key);

/* If a filename contains path information, change dirs to that path */
int oss_check_path(char *filename);

/* In case we changed directories in oss_check_path, change back to the
   original executable directory */
void oss_revert_path(void);


/* ---------------------------------------------------------------------- */
/*
** Input/output routines
*/

/* Open a stream, given a string, usage, and a filemode */
osfildef *oss_open_stream(char *buffer, glui32 tadsusage, glui32 tbusage, 
                          glui32 fmode, glui32 rock);

/* Process hilighting codes while printing a string */
void oss_put_string_with_hilite(winid_t win, char *str, size_t len);

/* Get a keystroke from a window */
int oss_getc_from_window(winid_t win);


/* ---------------------------------------------------------------------- */
/*
** Status line handling
*/

void oss_draw_status_line(void);
void oss_change_status_string(char *dest, const char *src, size_t len);
void oss_change_status_left(const char *str, size_t len);
void oss_change_status_right(const char *str);


/* ---------------------------------------------------------------------- */
/*
** Window fiddling
*/

void oss_arrange_windows(void);


/* ------------------------------------------------------------------------ */
/*
** Other #defines
*/

/* Status mode codes */
#define OSS_STATUS_MODE_STORY 0
#define OSS_STATUS_MODE_STATUS 1

/* Characters to turn hilighting on and off */
#define OSS_HILITE_ON 2
#define OSS_HILITE_OFF 1

#endif /* OSS_INCLUDED */
