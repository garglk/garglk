// vim: set ft=c:

#ifndef ZTERP_BRANCH_H
#define ZTERP_BRANCH_H

#include <stdbool.h>

void branch_if(bool do_branch);

void zjump(void);
void zjz(void);
void zje(void);
void zjl(void);
void zjg(void);

#endif
