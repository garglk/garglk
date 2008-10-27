/* glk-frotz.h
 *
 * Frotz os functions for the Glk library version 0.6.1.
 */

#include "frotz.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "glk.h"

extern int curr_status_ht;
extern int mach_status_ht;

extern winid_t gos_status;
extern winid_t gos_upper;
extern winid_t gos_lower;
extern winid_t gos_curwin;
extern int gos_linepending;
extern char *gos_linebuf;
extern winid_t gos_linewin;

extern schanid_t gos_channel;

/* from ../common/setup.h */
extern f_setup_t f_setup;

/* From input.c.  */
bool is_terminator (zchar);

/* from glkstuff */
void gos_update_width(void);
void gos_cancel_pending_line(void);
void reset_status_ht(void);

