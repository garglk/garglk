// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <initializer_list>
#include <string>

#include "types.h"
#include "util.h"
#include "zterp.h"

extern "C" {
#include <glk.h>
}

#ifdef ZTERP_GLK_BLORB
extern "C" {
#include <gi_blorb.h>
}
#endif

static void load_resources();

#ifdef ZTERP_GLK_UNIX
extern "C" {
#include <glkstart.h>
}

// The “standard” function (provided by remglk as an extension) for
// getting a filename from a fileref is glkunix_fileref_get_filename;
// Gargoyle has always had the function garglk_fileref_get_name for this
// purpose, but has now adopted the remglk name for greater
// compatibility. If building under garglk, and the newer name is not
// available, fall back to the known-to-exist original name. Otherwise,
// use the new name.
#if defined(GARGLK) && !defined(GLKUNIX_FILEREF_GET_FILENAME)
#define glkunix_fileref_get_filename garglk_fileref_get_name
#endif

glkunix_argumentlist_t glkunix_arguments[] = {
#include "help.h"
    { const_cast<char *>(""),		glkunix_arg_ValueFollows,	const_cast<char *>("file to load") },
    { nullptr, glkunix_arg_End, nullptr }
};

#ifdef ZTERP_GLK_BLORB
static strid_t load_file(const std::string &file)
{
    return glkunix_stream_open_pathname(const_cast<char *>(file.c_str()), 0, 0);
}
#endif

int glkunix_startup_code(glkunix_startup_t *data)
{
    process_arguments(data->argc, data->argv);

    if (arg_status == ArgStatus::Help) {
        return 1;
    }

#ifdef GARGLK
    garglk_set_program_name("Bocfel");
    if (options.show_version) {
        return 1;
    }
    if (!game_file.empty()) {
        auto story_name = game_file;
        auto slash = story_name.find_last_of('/');

        if (slash != std::string::npos) {
            story_name.erase(0, slash + 1);
        }

        garglk_set_story_name(story_name.c_str());
    } else {
        frefid_t ref;

        ref = glk_fileref_create_by_prompt(fileusage_Data | fileusage_BinaryMode, filemode_Read, 0);
        if (ref != nullptr) {
            const char *filename = glkunix_fileref_get_filename(ref);
            if (filename != nullptr) {
                game_file = filename;
            }
            glk_fileref_destroy(ref);
        }
    }
#endif

    if (!game_file.empty()) {
#ifndef ZTERP_DOS
        glkunix_set_base_file(&game_file[0]);
#endif
        load_resources();
    }

    return 1;
}
#elif defined(ZTERP_GLK_WINGLK)
#include <cstdlib>
#include <sstream>

#include <WinGlk.h>

extern "C" {
int InitGlk(unsigned int);
}

#ifdef ZTERP_GLK_BLORB
static strid_t load_file(const std::string &file)
{
    frefid_t ref = winglk_fileref_create_by_name(fileusage_BinaryMode | fileusage_Data, const_cast<char *>(file.c_str()), 0, 0);

    if (ref == nullptr) {
        return nullptr;
    }

    strid_t stream = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    return stream;
}
#endif

static void startup()
{
    winglk_app_set_name("Bocfel");
    winglk_set_menu_name("&Bocfel");
    winglk_set_about_text("Windows Bocfel " ZTERP_VERSION);
    winglk_show_game_dialog();

    process_arguments(__argc, __argv);

    if (arg_status == ArgStatus::Help) {
        return;
    }

    if (game_file.empty()) {
        const char *patterns = "*.z1;*.z2;*.z3;*.z4;*.z5;*.z6;*.z7.*.z8;*.zblorb;*.zlb;*.blorb;*.blb";
        std::ostringstream filter;
        const char *filename;

        filter << "Z-Code Files (" << patterns << ")|" << patterns << "|All Files (*.*)|*.*||";

        filename = winglk_get_initial_filename(nullptr, "Choose a Z-Code Game", filter.str().c_str());
        if (filename != nullptr) {
            game_file = filename;
        }
    }

    if (!game_file.empty()) {
        auto slash = game_file.find_last_of("/\\");
        std::string game_dir, filename;

        if (slash != std::string::npos) {
            game_dir = game_file.substr(0, slash);
            filename = game_file.substr(slash + 1);
        } else {
            game_dir = ".";
            filename = game_file;
        }

        winglk_set_resource_directory(game_dir.c_str());

        if (!filename.empty()) {
            auto dot = filename.find_last_of('.');
            if (dot != std::string::npos) {
                filename.resize(dot);
            }

            sglk_set_basename(&filename[0]);
        }

        load_resources();
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    if (!InitGlk(0x00000700)) {
        std::exit(EXIT_FAILURE);
    }

    startup();

    glk_main();
    glk_exit();

    return 0;
}
#else
#define load_file(file) nullptr
#error Glk on this platform is not supported.
#endif

// A Blorb file can contain the story file, or it can simply be an
// external package of resources. Try loading the main story file first;
// if it contains Blorb resources, use it. Otherwise, try to find a
// Blorb file that goes with the selected story.
static void load_resources()
{
#ifdef ZTERP_GLK_BLORB
    auto set_map = [](const std::string &blorb_file) {
        strid_t file = load_file(blorb_file);
        if (file != nullptr) {
            if (giblorb_set_resource_map(file) == giblorb_err_None) {
                return true;
            }
            glk_stream_close(file, nullptr);
        }

        return false;
    };

    if (set_map(game_file)) {
        return;
    }

    for (const auto &ext : {".blb", ".blorb"}) {
        std::string blorb_file = game_file;
        auto dot = blorb_file.rfind('.');
        if (dot != std::string::npos) {
            blorb_file.replace(dot, std::string::npos, ext);
        } else {
            blorb_file += ext;
        }

        if (set_map(blorb_file)) {
            return;
        }
    }
#endif
}
