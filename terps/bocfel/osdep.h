// vim: set ft=c:

#ifndef ZTERP_OSDEP_H
#define ZTERP_OSDEP_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "screen.h"

long zterp_os_filesize(FILE *);
void zterp_os_rcfile(char *, size_t);

#ifndef ZTERP_GLK
void zterp_os_get_screen_size(unsigned *, unsigned *);
void zterp_os_init_term(void);
bool zterp_os_have_style(int);
bool zterp_os_have_colors(void);
void zterp_os_set_style(int, const struct color *, const struct color *);
#endif

#endif
