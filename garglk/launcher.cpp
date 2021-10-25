/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2009 by Baltasar García Perez-Schofield.                     *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "glk.h"
#include "glkstart.h"
#include "gi_blorb.h"
#include "garglk.h"
#include "launcher.h"

#define T_ADRIFT    "scare"
#define T_ADVSYS    "advsys"
#define T_AGT       "agility"
#define T_ALAN2     "alan2"
#define T_ALAN3     "alan3"
#define T_GLULX     "git"
#define T_HUGO      "hugo"
#define T_JACL      "jacl"
#define T_LEV9      "level9"
#define T_MGSR      "magnetic"
#define T_QUEST     "geas"
#define T_SCOTT     "scott"
#define T_TADS2     "tadsr"
#define T_TADS3     "tadsr"
#define T_ZCODE     "bocfel"
#define T_ZSIX      "bocfel"

#define ID_ZCOD (giblorb_make_id('Z','C','O','D'))
#define ID_GLUL (giblorb_make_id('G','L','U','L'))

struct Launch {
    std::string terp;
    std::string flags;
};

#define MaxBuffer 1024

static bool call_winterp(const std::string &path, const std::string &interpreter, const std::string &flags, const std::string &game)
{
    std::string exe = std::string(GARGLKPRE) + interpreter;

    return winterp(path, exe, flags, game);
}

static bool runblorb(const std::string &path, const std::string &game, const struct Launch &launch)
{
    class BlorbError : public std::exception {
    public:
        explicit BlorbError(const std::string &message) : m_message(message) {
        }

        const std::string &message() const {
            return m_message;
        }

    private:
        std::string m_message;
    };

    try
    {
        char magic[4];
        giblorb_result_t res;
        std::string found_interpreter;
        giblorb_map_t *basemap;
        auto close_stream = [](strid_t file) { glk_stream_close(file, nullptr); };

        auto file = garglk::unique(glkunix_stream_open_pathname(const_cast<char *>(game.c_str()), 0, 0), close_stream);
        if (!file)
            throw BlorbError("Unable to open file");

        if (giblorb_create_map(file.get(), &basemap) != giblorb_err_None)
            throw BlorbError("Does not appear to be a Blorb file");

        auto map = garglk::unique(basemap, giblorb_destroy_map);

        if (giblorb_load_resource(map.get(), giblorb_method_FilePos, &res, giblorb_ID_Exec, 0) != giblorb_err_None)
            throw BlorbError("Does not contain a story file (look for a corresponding game file to load instead)");

        glk_stream_set_position(file.get(), res.data.startpos, 0);
        if (glk_get_buffer_stream(file.get(), magic, 4) != 4)
            throw BlorbError("Unable to read story file (possibly corrupted Blorb file)");

        switch (res.chunktype)
        {
        case ID_ZCOD:
            if (!launch.terp.empty())
                found_interpreter = launch.terp;
            else if (magic[0] == 6)
                found_interpreter = T_ZSIX;
            else
                found_interpreter = T_ZCODE;
            break;

        case ID_GLUL:
            if (!launch.terp.empty())
                found_interpreter = launch.terp;
            else
                found_interpreter = T_GLULX;
            break;

        default: {
            char msg[64];
            std::snprintf(msg, sizeof msg, "Unknown game type: 0x%08lx", static_cast<unsigned long>(res.chunktype));
            throw BlorbError(msg);
        }
        }

        return call_winterp(path, found_interpreter, launch.flags, game);
    }
    catch (const BlorbError &e)
    {
        winmsg("Could not load Blorb file %s:\n%s", game.c_str(), e.message().c_str());
        return false;
    }
}

static bool findterp(const std::string &file, const std::string &target, struct Launch &launch)
{
    std::vector<std::string> matches = {target};

    garglk::config_entries(file, false, matches, [&launch](const std::string &cmd, const std::string &arg) {
        if (cmd == "terp")
        {
            std::stringstream argstream(arg);
            std::string opt;

            if (argstream >> launch.terp)
            {
                if (argstream >> opt && opt[0] == '-')
                    launch.flags = opt;
                else
                    launch.flags = "";
            }
        }
    });

    return !launch.terp.empty();
}

static void configterp(const std::string &exedir, const std::string &gamepath, struct Launch &launch)
{
    std::string config;
    std::string story = gamepath;

    /* set up story */
    auto slash = story.rfind('\\');
    if (slash == std::string::npos)
        slash = story.find_last_of('/');
    if (slash != std::string::npos)
        story = story.substr(slash + 1);

    if (story.empty())
        return;

    std::string ext = story;
    auto dot = story.rfind('.');
    if (dot != std::string::npos)
        ext.replace(0, dot, "*");
    else
        ext = "*.*";

    for (const auto &config : garglk::configs(exedir, gamepath))
    {
        if (findterp(config, story, launch) || findterp(config, ext, launch))
            return;
    }
}

struct Interpreter {
    std::string interpreter;
    std::string ext;
    std::string flags;
};

/* Case-insensitive string comparison */
static bool equal_strings(const std::string &a, const std::string &b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
            [](char l, char r) { return std::tolower(static_cast<unsigned char>(l)) == std::tolower(static_cast<unsigned char>(r)); });
}

int rungame(const std::string &path, const std::string &game)
{
    std::vector<const char *> blorbs = {"blb", "blorb", "glb", "gbl", "gblorb", "zlb", "zbl", "zblorb"};
    std::vector<Interpreter> interpreters = {
        { T_ADRIFT, "taf" },
        { T_AGT, "agx", "-gl" },
        { T_AGT, "d$$", "-gl" },
        { T_ALAN2, "acd" },
        { T_ALAN3, "a3c" },
        { T_GLULX, "ulx" },
        { T_HUGO, "hex" },
        { T_JACL, "j2" },
        { T_JACL, "jacl" },
        { T_LEV9, "l9" },
        { T_LEV9, "sna" },
        { T_MGSR, "mag" },
        { T_QUEST, "asl" },
        { T_QUEST, "cas" },
        { T_SCOTT, "saga" },
        { T_TADS2, "gam" },
        { T_TADS3, "t3" },
        { T_ZCODE, "dat" },
        { T_ZCODE, "z1" },
        { T_ZCODE, "z2" },
        { T_ZCODE, "z3" },
        { T_ZCODE, "z4" },
        { T_ZCODE, "z5" },
        { T_ZCODE, "z7" },
        { T_ZCODE, "z8" },
        { T_ZSIX, "z6" },
    };
    Launch launch;

    configterp(path, game, launch);

    std::string ext = "";
    auto dot = game.rfind('.');
    if (dot != std::string::npos)
        ext = game.substr(dot + 1);

    if (std::any_of(blorbs.begin(), blorbs.end(),
                    [&ext](const auto &blorb) { return equal_strings(ext, blorb); }))
    {
        return runblorb(path, game, launch);
    }

    if (!launch.terp.empty())
        return call_winterp(path, launch.terp, launch.flags, game);

    // Both Z-machine and AdvSys games claim the .dat extension. In
    // general Gargoyle uses extensions to determine the interpreter to
    // run, but since there's a conflict, check the file header in the
    // case of .dat files. If it looks like AdvSys, call the AdvSys
    // interpreter. Otherwise, use the Z-machine interpreter.
    if (equal_strings(ext, "dat"))
    {
        std::vector<uint8_t> header(6);
        const std::vector<uint8_t> advsys_magic = {0xa0, 0x9d, 0x8b, 0x8e, 0x88, 0x8e};
        std::ifstream f(game, std::ios::binary);
        if (f.is_open() &&
            f.seekg(2) &&
            f.read(reinterpret_cast<char *>(&header[0]), header.size()) &&
            header == advsys_magic)
        {
            return call_winterp(path, T_ADVSYS, "", game);
        }
    }

    auto interpreter = std::find_if(interpreters.begin(), interpreters.end(),
                                    [&ext](const auto &interpreter) { return equal_strings(ext, interpreter.ext); });
    if (interpreter != interpreters.end())
        return call_winterp(path, interpreter->interpreter, interpreter->flags, game);

    winmsg("Unknown file type: \"%s\"\nSorry.", ext.c_str());

    return 0;
}
