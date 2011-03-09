#ifndef ZTERP_OSDEP_H
#define ZTERP_OSDEP_H

#include <stdio.h>
#include <stddef.h>

long zterp_os_filesize(FILE *);
int zterp_os_have_unicode(void);
void zterp_os_rcfile(char *, size_t);
void zterp_os_reopen_binary(FILE *);

#ifndef ZTERP_GLK
void zterp_os_get_winsize(unsigned *, unsigned *);
void zterp_os_init_term(void);
int zterp_os_have_style(int);
int zterp_os_have_colors(void);
void zterp_os_set_style(int, int, int);
#endif

#endif
