// vim: set ft=cpp:

#ifndef ZTERP_PATCHES_H
#define ZTERP_PATCHES_H

#include <exception>
#include <stdexcept>
#include <string>

namespace PatchStatus {
    class SyntaxError : public std::runtime_error {
    public:
        explicit SyntaxError(const std::string &what) : std::runtime_error(what) {
        }
    };
    class NotFound : public std::exception {
    };
}

void apply_patches();
void apply_v6_patches();
void apply_user_patch(std::string patchstr);
void patch_load_file(const std::string &file);

#endif
