// Copyright 2025 Andrew Plotkin.
//
// SPDX-License-Identifier: MIT

#include <string>

#include "glkautosave.h"
#include "osdep.h"
#include "screen.h"

#ifdef ZTERP_GLK_UNIX
extern "C" {
#include <glk.h>
#include <gi_blorb.h>
#include <glkstart.h>
}
#endif

#ifdef GLKUNIX_AUTOSAVE_FEATURES
static strid_t open_autosave(bool writemode)
{
    auto autosavepath = zterp_os_autosave_name();
    if (autosavepath == nullptr) {
        return nullptr;
    }
    std::string pathname = *autosavepath + ".json";

    return glkunix_stream_open_pathname_gen(const_cast<char *>(pathname.c_str()), writemode, FALSE, 1);
}
#endif

bool glkautosave_library_autosave()
{
#ifdef GLKUNIX_AUTOSAVE_FEATURES
    strid_t jsavefile = open_autosave(true);
    if (jsavefile == nullptr) {
        return false;
    }

    glkunix_save_library_state(jsavefile, jsavefile, nullptr, nullptr);

    glk_stream_close(jsavefile, nullptr);
    jsavefile = nullptr;
#endif // GLKUNIX_AUTOSAVE_FEATURES

    return true;
}

bool glkautosave_library_autorestore()
{
#ifdef GLKUNIX_AUTOSAVE_FEATURES
    strid_t jsavefile = open_autosave(false);
    if (jsavefile == nullptr) {
        return false;
    }

    glkunix_library_state_t library_state = glkunix_load_library_state(jsavefile, nullptr, nullptr);

    glk_stream_close(jsavefile, nullptr);
    jsavefile = nullptr;

    if (library_state == nullptr) {
        return false;
    }

    // The interpreter has already opened its windows. This will close
    // them and open new ones, which is clunky but that’s the sequence
    // we’re working with.
    if (!glkunix_update_from_library_state(library_state)) {
        glkunix_library_state_free(library_state);
        return false;
    }

    // We’re done with the library state object.
    glkunix_library_state_free(library_state);
    library_state = nullptr;

    // Recover the blorb stream ID, if we have one open.
    if (giblorb_get_resource_map() != nullptr) {
        // It’s inefficient to throw away the blorb chunk map, which we
        // just loaded, and then recreate it. Oh well.
        if (giblorb_unset_resource_map() != giblorb_err_None) {
            return false;
        }

        glui32 rock = 0;
        for (auto str = glk_stream_iterate(nullptr, &rock); str != nullptr; str = glk_stream_iterate(str, &rock)) {
            if (rock == static_cast<glui32>(StreamRock::BlorbStream)) {
                if (giblorb_set_resource_map(str) != giblorb_err_None) {
                    return false;
                }
                break;
            }
        }
    }

    // Let the interpreter recover window IDs.
    screen_recover_glk_windows();
    screen_recover_glk_streams();
#endif // GLKUNIX_AUTOSAVE_FEATURES

    return true;
}
