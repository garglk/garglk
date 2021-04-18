// vim: set ft=c:

#ifndef ZTERP_OSDEP_H
#define ZTERP_OSDEP_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "screen.h"

long zterp_os_filesize(FILE *fp);
bool zterp_os_rcfile(char *s, size_t n);
bool zterp_os_autosave_name(char *s, size_t n);

#ifndef ZTERP_GLK
void zterp_os_get_screen_size(unsigned *w, unsigned *h);
void zterp_os_init_term(void);
bool zterp_os_have_style(int style);
bool zterp_os_have_colors(void);
void zterp_os_set_style(int style, const struct color *fg, const struct color *bg);
#endif

#endif
