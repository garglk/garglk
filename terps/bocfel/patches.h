// vim: set ft=cpp:

#ifndef ZTERP_PATCHES_H
#define ZTERP_PATCHES_H

#include <exception>
#include <string>

namespace PatchStatus {
    class SyntaxError : std::exception {
    };
    class NotFound : std::exception {
    };
}

void apply_patches();
void apply_user_patch(std::string patchstr);

#endif
