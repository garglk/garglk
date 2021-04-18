#ifndef ZTERP_STASH_H
#define ZTERP_STASH_H

#include <stdbool.h>

void stash_register(void (*backup)(void), bool (*restore)(void), void (*free)(void));

void stash_backup(void);
bool stash_restore(void);
void stash_free(void);
bool stash_exists(void);

#endif
