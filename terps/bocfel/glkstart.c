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

#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "zterp.h"

#include <glk.h>

#ifdef ZTERP_GLK_BLORB
#include <gi_blorb.h>
#endif

static void load_resources(void);

// Both Gargoyle (regardless of platform) and DOSGlk provide a Unix startup.
#if defined(ZTERP_UNIX) || defined(GARGLK) || defined(ZTERP_DOS)
#include <glkstart.h>

glkunix_argumentlist_t glkunix_arguments[] =
{
#include "help.h"
    { "",		glkunix_arg_ValueFollows,	"file to load" },
    { NULL, glkunix_arg_End, NULL }
};

#ifdef ZTERP_GLK_BLORB
static strid_t load_file(const char *file)
{
    return glkunix_stream_open_pathname((char *)file, 0, 0);
}
#endif

int glkunix_startup_code(glkunix_startup_t *data)
{
    process_arguments(data->argc, data->argv);

    if (arg_status == ARG_HELP) {
        return 1;
    }

#ifdef GARGLK
    garglk_set_program_name("Bocfel");
    if (options.show_version) {
        return 1;
    }
    if (game_file != NULL) {
        const char *p = strrchr(game_file, '/');
        garglk_set_story_name(p == NULL ? game_file : p + 1);
    } else {
        frefid_t ref;

        ref = glk_fileref_create_by_prompt(fileusage_Data | fileusage_BinaryMode, filemode_Read, 0);
        if (ref != NULL) {
            game_file = xstrdup(garglk_fileref_get_name(ref));
            glk_fileref_destroy(ref);
        }
    }
#endif

    if (game_file != NULL) {
#ifndef ZTERP_DOS
        glkunix_set_base_file((char *)game_file);
#endif
        load_resources();
    }

    return 1;
}
#elif defined(ZTERP_WIN32)
#include <stdio.h>
#include <windows.h>

#include <WinGlk.h>

int InitGlk(unsigned int);

#ifdef ZTERP_GLK_BLORB
static strid_t load_file(const char *file)
{
    frefid_t ref = winglk_fileref_create_by_name(fileusage_BinaryMode | fileusage_Data, (char *)file, 0, 0);

    if (ref == NULL) {
        return NULL;
    }

    strid_t stream = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    return stream;
}
#endif

static void startup(void)
{
    winglk_app_set_name("Bocfel");
    winglk_set_menu_name("&Bocfel");
    winglk_set_about_text("Windows Bocfel " ZTERP_VERSION);
    winglk_show_game_dialog();

    process_arguments(__argc, __argv);

    if (arg_status == ARG_HELP) {
        return;
    }

    if (game_file == NULL) {
        const char *patterns = "*.z1;*.z2;*.z3;*.z4;*.z5;*.z6;*.z7.*.z8;*.zblorb;*.zlb;*.blorb;*.blb";
        char filter[512];

        snprintf(filter, sizeof filter, "Z-Code Files (%s)|%s|All Files (*.*)|*.*||", patterns, patterns);

        game_file = winglk_get_initial_filename(NULL, "Choose a Z-Code Game", filter);
    }

    if (game_file != NULL) {
        char *game_dir = xstrdup(game_file);
        char *p = strrchr(game_dir, '/');
        char *basename = NULL;

        if (p == NULL) {
            p = strrchr(game_dir, '\\');
        }

        if (p == NULL) {
            basename = game_dir;
            winglk_set_resource_directory(".");
        } else {
            *p = 0;
            winglk_set_resource_directory(game_dir);

            if (*++p != 0) {
                basename = p;
            }
        }

        if (basename != NULL) {
            p = strrchr(basename, '.');
            if (p != NULL) {
                *p = 0;
            }
            sglk_set_basename(basename);
        }

        free(game_dir);

        load_resources();
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previnstance, LPSTR cmdline, int cmdshow)
{
    if (!InitGlk(0x00000700)) {
        exit(EXIT_FAILURE);
    }

    startup();

    glk_main();
    glk_exit();

    return 0;
}
#else
#define load_file(file) NULL
#error Glk on this platform is not supported.
#endif

// A Blorb file can contain the story file, or it can simply be an
// external package of resources. Try loading the main story file first;
// if it contains Blorb resources, use it. Otherwise, try to find a
// Blorb file that goes with the selected story.
static void load_resources(void)
{
#ifdef ZTERP_GLK_BLORB
    strid_t file = load_file(game_file);
    if (file != NULL) {
        if (giblorb_set_resource_map(file) == giblorb_err_None) {
            return;
        }
        glk_stream_close(file, NULL);
        file = NULL;
    }

    // 7 for the worst case of needing to add .blorb to the end plus the
    // null character.
    char *blorb_file = malloc(strlen(game_file) + 7);
    if (blorb_file != NULL) {
        const char *exts[] = { ".blb", ".blorb" };

        strcpy(blorb_file, game_file);
        for (size_t i = 0; file == NULL && i < ASIZE(exts); i++) {
            char *p = strrchr(blorb_file, '.');
            if (p != NULL) {
                *p = 0;
            }
            strcat(blorb_file, exts[i]);

            file = load_file(blorb_file);
        }

        free(blorb_file);
    }

    if (file != NULL) {
        giblorb_set_resource_map(file);
    }
#endif
}
