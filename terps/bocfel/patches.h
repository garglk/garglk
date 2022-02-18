// vim: set ft=cpp:

#ifndef ZTERP_PATCHES_H
#define ZTERP_PATCHES_H

#include <string>

enum class PatchStatus {
    Ok,
    SyntaxError,
    NotFound,
};

void apply_patches();
PatchStatus apply_user_patch(std::string patchstr);

#endif
