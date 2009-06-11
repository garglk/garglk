/*
$Header: d:/cvsroot/tads/TADS2/TIO.H,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tio.h - text I/O interface
Function
  Formatted text input and output interface definition
Notes
  None
Modified
  04/11/99 CNebel        - Move extern C.
  09/05/92 MJRoberts     - add length parameter to getstring()
  02/16/92 MJRoberts     - creation
*/

#ifndef TIO_INCLUDED
#define TIO_INCLUDED

#include <time.h>

#ifndef ERR_INCLUDED
# include "err.h"
#endif
#ifndef OBJ_INCLUDED
# include "obj.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* forward decls */
struct runcxdef;

/* text i/o context */
struct tiocxdef
{
    errcxdef *tiocxerr;                           /* error handling context */
};
typedef struct tiocxdef tiocxdef;

/*
 *   Initialize the output formatter subsystem.  This must be called once
 *   at startup. 
 */
void out_init(void);


/* redirect all tioxxx routines to TADS v1.x outxxx equivalents */
#define tioflushn(ctx, nl) outflushn(nl)
#define tioflush(ctx)      outflush()
#define tioblank(ctx)      outblank()
#define tioreset(ctx)      outreset()
#define tiogets(ctx, prompt, str, siz) getstring(prompt, str, siz)
#define tioputs(ctx, str)  outformat(str)
#define tioputslen(ctx, str, len) outformatlen(str, len)
#define tiocaps(ctx)       outcaps()
#define tionocaps(ctx)     outnocaps()
#define tioshow(ctx)       outshow()
#define tiohide(ctx)       outhide()
#define tioscore(ctx, s1, s2) os_score(s1, s2)
#define tiostrsc(ctx, s)   os_strsc(s)

/* set up format strings in output subsystem */
void tiosetfmt(tiocxdef *ctx, struct runcxdef *rctx, uchar *fmtbase,
               uint fmtlen);

/* tell tio subsystem the current actor */
void tiosetactor(struct tiocxdef *ctx, objnum actor);

/* get the current tio subsystem actor */
objnum tiogetactor(struct tiocxdef *ctx);

/* turn output capture on/off */
void tiocapture(struct tiocxdef *tioctx, struct mcmcxdef *memctx,
                int flag);

/* get the capture object handle */
mcmon tiogetcapture(struct tiocxdef *ctx);

/* get the amount of text captured */
uint tiocapturesize(struct tiocxdef *ctx);

/* format a length-prefixed (runtime-style) string to the display */
void outfmt(tiocxdef *ctx, uchar *txt);

/* format a null-terminated (C-style) string to the display */
int outformat(char *s);

/* format a counted-length string, which may not be null-terminated */
int outformatlen(char *s, uint len);

/* flush output, with specified newline mode */
void outflushn(int nl);

/* flush output */
void outflush(void);

/* reset output state */
void outreset(void);

/* 
 *   Get a string from the keyboard.  Returns non-zero if an error occurs
 *   (in particular, if no more input is available from the keyboard),
 *   zero on success.  
 */
int getstring(char *prompt, char *buf, int bufl);

/* set capitalize-next-character mode on/off */
void outcaps(void);
void outnocaps(void);

/* open/close output log file */
int tiologopn(tiocxdef *ctx, char *fn);
int tiologcls(tiocxdef *ctx);

/*
 *   Write text explicitly to the log file.  This can be used to add
 *   special text (such as prompt text) that would normally be suppressed
 *   from the log file.  When more mode is turned off, we don't
 *   automatically copy text to the log file; any text that the caller
 *   knows should be in the log file during times when more mode is turned
 *   off can be explicitly added with this function.
 *   
 *   If nl is true, we'll add a newline at the end of this text.  The
 *   caller should not include any newlines in the text being displayed
 *   here.  
 */
void out_logfile_print(char *txt, int nl);


/*
 *   Check output status.  Indicate whether output is currently hidden,
 *   and whether any hidden output has occurred. 
 */
void outstat(int *hidden, int *output_occurred);

/* hide/show output */
void outhide(void);
int outshow(void);

/* set the flag to indicate that output has occurred */
void outsethidden(void);

/* write a blank line */
void outblank(void);

/* start/end watchpoint evaluation */
void outwx(int flag);

/* Begin/end capturing */
void tiocapture(tiocxdef *tioctx, mcmcxdef *memctx, int flag);

/* clear all captured output */
void tioclrcapture(tiocxdef *tioctx);

/* 
 *   clear captured output back to a given point -- this can be used to
 *   remove captured output in an inner capture from an enclosing capture 
 */
void tiopopcapture(tiocxdef *tioctx, uint orig_size);

/* get the object handle of the captured output */
mcmon tiogetcapture(tiocxdef *ctx);

/* get the amount of text captured */
uint tiocapturesize(tiocxdef *ctx);

/* turn MORE mode on or off */
int setmore(int state);

/* explicitly activate the "MORE" prompt */
void out_more_prompt();

/*
 *   QA controller functions
 */
int qasopn(char *scrnam, int quiet);
void qasclose(void);
char *qasgets(char *buf, int bufl);

/*
 *   Set an HTML entity expansion.  This is called during initialization
 *   when we read a character mapping table that includes HTML entity
 *   expansions.  The HTML run-time uses its own expansion mechanism, so
 *   it will ignore this information.  The standard character-mode TADS
 *   run-time, however, uses this information to map HTML entities to the
 *   local character set. 
 */
void tio_set_html_expansion(unsigned int html_char_val,
                            const char *expansion, size_t expansion_len);

/* check for HTML mode - returns true if an "\H+" sequence is active */
int tio_is_html_mode();

/* set the user output filter function */
void out_set_filter(objnum filter_fn);

/* set the double-space mode */
void out_set_doublespace(int dbl);

/*
 *   Ask for a filename, using a system-defined dialog (via os_askfile) if
 *   possible.  Uses the same interface as os_askfile(), which we will
 *   call directly for graphical implementations.  We'll use formatted
 *   text for text-only implementations.  
 */
int tio_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
                int prompt_type, os_filetype_t file_type);

/*
 *   Display a dialog, using a system-defined dialog (via os_input_dialog)
 *   if possible.  Uses the same interface as os_input_dialog(), which we
 *   will call directly for graphical implementations.  We'll use
 *   formatted text for text-only implementations.  
 */
int tio_input_dialog(int icon_id, const char *prompt, int standard_button_set,
                     const char **buttons, int button_count,
                     int default_index, int cancel_index);


#ifdef __cplusplus
}
#endif

#endif /* TIO_INCLUDED */

