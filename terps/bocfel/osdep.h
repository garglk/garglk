// vim: set ft=c:

#ifndef ZTERP_OSDEP_H
#define ZTERP_OSDEP_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "screen.h"

#define ZTERP_OS_PATH_SIZE	4096

long zterp_os_filesize(FILE *fp);
bool zterp_os_rcfile(char (*s)[ZTERP_OS_PATH_SIZE], bool create_parent);
bool zterp_os_autosave_name(char (*s)[ZTERP_OS_PATH_SIZE]);
bool zterp_os_aux_file(char (*aux)[ZTERP_OS_PATH_SIZE], const char *filename);
bool zterp_os_edit_file(const char *filename, char *err, size_t errsize);
bool zterp_os_edit_notes(const char *notes, size_t notes_len, char **new_notes, size_t *new_notes_len, char *err, size_t errsize);

#ifndef ZTERP_GLK
void zterp_os_get_screen_size(unsigned *w, unsigned *h);
void zterp_os_init_term(void);
bool zterp_os_have_style(int style);
bool zterp_os_have_colors(void);
void zterp_os_set_style(int style, const struct color *fg, const struct color *bg);
#endif

#endif
