// vim: set ft=cpp:

#ifndef ZTERP_PATCHES_H
#define ZTERP_PATCHES_H

#include <exception>
#include <string>

namespace PatchStatus {
    class SyntaxError : public std::exception {
    };
    class NotFound : public std::exception {
    };
}

void apply_patches();
void apply_v6_patches();
void apply_user_patch(std::string patchstr);
void patch_load_file(const std::string &file);

#endif
