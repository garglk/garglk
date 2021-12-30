// vim: set ft=c:

#ifndef ZTERP_PATCHES_H
#define ZTERP_PATCHES_H

#include <stdbool.h>

enum PatchStatus {
    PatchStatusOk,
    PatchStatusSyntaxError,
    PatchStatusNotFound,
};

void apply_patches(void);
enum PatchStatus apply_user_patch(const char *patchstr);

#endif
