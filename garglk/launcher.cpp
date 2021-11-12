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
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
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

struct Interpreter {
    explicit Interpreter(const std::string &terp_, const std::set<std::string> &extensions_ = {}, const std::string &flags_ = "") :
        terp(terp_),
        extensions(extensions_),
        flags(flags_) {
        }

    std::string terp;
    std::set<std::string> extensions;
    std::string flags;
};

enum class Format {
    Adrift,
    AdvSys,
    AGT,
    Alan2,
    Alan3,
    Glulx,
    Hugo,
    JACL,
    Level9,
    Magnetic,
    Quest,
    Scott,
    TADS2,
    TADS3,
    ZCode,
    ZCode6,
};

// Map formats to default interpreters
static const std::map<Format, Interpreter> interpreters = {
    { Format::Adrift, Interpreter(T_ADRIFT, {"taf"}) },
    { Format::AdvSys, Interpreter(T_ADVSYS) },
    { Format::AGT, Interpreter(T_AGT, {"agx", "d$$"}, "-gl") },
    { Format::Alan2, Interpreter(T_ALAN2, {"acd"}) },
    { Format::Alan3, Interpreter(T_ALAN3, {"a3c"}) },
    { Format::Glulx, Interpreter(T_GLULX, {"ulx"}) },
    { Format::Hugo, Interpreter(T_HUGO, {"hex"}) },
    { Format::JACL, Interpreter(T_JACL, {"j2", "jacl"}) },
    { Format::Level9, Interpreter(T_LEV9, {"l9", "sna"}) },
    { Format::Magnetic, Interpreter(T_MGSR, {"mag"}) },
    { Format::Quest, Interpreter(T_QUEST, {"asl", "cas"}) },
    { Format::Scott, Interpreter(T_SCOTT, {"saga"}) },
    { Format::TADS2, Interpreter(T_TADS2, {"gam"}) },
    { Format::TADS3, Interpreter(T_TADS3, {"t3"}) },
    { Format::ZCode, Interpreter(T_ZCODE, {"dat", "z1", "z2", "z3", "z4", "z5", "z7", "z8"}) },
    { Format::ZCode6, Interpreter(T_ZSIX, {"z6"}) },
};

static bool call_winterp(const std::string &path, const Interpreter &interpreter, const std::string &game)
{
    return garglk::winterp(path, GARGLKPRE + interpreter.terp, interpreter.flags, game);
}

static bool runblorb(const std::string &path, const std::string &game, const Interpreter &interpreter)
{
    class BlorbError : public std::runtime_error {
    public:
        explicit BlorbError(const std::string &msg) : std::runtime_error(msg) {
        }
    };

    try
    {
        giblorb_result_t res;
        Interpreter found_interpreter = interpreter;
        giblorb_map_t *basemap;

        auto file = garglk::unique(glkunix_stream_open_pathname(const_cast<char *>(game.c_str()), 0, 0), [](strid_t file) { glk_stream_close(file, nullptr); });
        if (!file)
            throw BlorbError("Unable to open file");

        if (giblorb_create_map(file.get(), &basemap) != giblorb_err_None)
            throw BlorbError("Does not appear to be a Blorb file");

        auto map = garglk::unique(basemap, giblorb_destroy_map);

        if (giblorb_load_resource(map.get(), giblorb_method_FilePos, &res, giblorb_ID_Exec, 0) != giblorb_err_None)
            throw BlorbError("Does not contain a story file (look for a corresponding game file to load instead)");

        switch (res.chunktype)
        {
        case ID_ZCOD:
            if (interpreter.terp.empty())
            {
                char zversion;

                glk_stream_set_position(file.get(), res.data.startpos, 0);
                if (glk_get_buffer_stream(file.get(), &zversion, 1) != 1)
                    throw BlorbError("Unable to read story file (possibly corrupted Blorb file)");

                if (zversion == 6)
                    found_interpreter = interpreters.at(Format::ZCode6);
                else
                    found_interpreter = interpreters.at(Format::ZCode);
            }
            break;

        case ID_GLUL:
            if (interpreter.terp.empty())
                found_interpreter = interpreters.at(Format::Glulx);
            break;

        default: {
            std::stringstream msg;
            msg << "Unknown game type: 0x" << std::hex << std::setw(8) << std::setfill('0') << res.chunktype;
            throw BlorbError(msg.str());
        }
        }

        return call_winterp(path, found_interpreter, game);
    }
    catch (const BlorbError &e)
    {
        garglk::winmsg("Could not load Blorb file " + game + ":\n" + e.what());
        return false;
    }
}

static bool findterp(const std::string &file, const std::string &target, struct Interpreter &interpreter)
{
    std::vector<std::string> matches = {target};

    garglk::config_entries(file, false, matches, [&interpreter](const std::string &cmd, const std::string &arg) {
        if (cmd == "terp")
        {
            std::stringstream argstream(arg);
            std::string opt;

            if (argstream >> interpreter.terp)
            {
                if (argstream >> opt && opt[0] == '-')
                    interpreter.flags = opt;
                else
                    interpreter.flags = "";
            }
        }
    });

    return !interpreter.terp.empty();
}

static void configterp(const std::string &exedir, const std::string &gamepath, struct Interpreter &interpreter)
{
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
        if (findterp(config.path, story, interpreter) || findterp(config.path, ext, interpreter))
            return;
    }
}

int garglk::rungame(const std::string &path, const std::string &game)
{
    const std::set<std::string> blorbs = {"blb", "blorb", "glb", "gbl", "gblorb", "zlb", "zbl", "zblorb"};
    Interpreter interpreter("");

    configterp(path, game, interpreter);

    std::string ext = "";
    auto dot = game.rfind('.');
    if (dot != std::string::npos)
        ext = garglk::downcase(game.substr(dot + 1));

    if (std::any_of(blorbs.begin(), blorbs.end(),
                    [&ext](const auto &blorb) { return ext == blorb; }))
    {
        return runblorb(path, game, interpreter);
    }

    if (!interpreter.terp.empty())
        return call_winterp(path, interpreter, game);

    // Both Z-machine and AdvSys games claim the .dat extension. In
    // general Gargoyle uses extensions to determine the interpreter to
    // run, but since there's a conflict, check the file header in the
    // case of .dat files. If it looks like AdvSys, call the AdvSys
    // interpreter. Otherwise, use the Z-machine interpreter.
    if (ext == "dat")
    {
        std::vector<uint8_t> header(6);
        const std::vector<uint8_t> advsys_magic = {0xa0, 0x9d, 0x8b, 0x8e, 0x88, 0x8e};
        std::ifstream f(game, std::ios::binary);
        if (f.is_open() &&
            f.seekg(2) &&
            f.read(reinterpret_cast<char *>(&header[0]), header.size()) &&
            header == advsys_magic)
        {
            return call_winterp(path, interpreters.at(Format::AdvSys), game);
        }
    }

    auto found_interpreter = std::find_if(interpreters.begin(), interpreters.end(), [&ext](const std::pair<Format, Interpreter> &pair) {
        auto interpreter = pair.second;
        return std::any_of(interpreter.extensions.begin(), interpreter.extensions.end(), [&ext](const std::string &ext_) { return ext == ext_; });
    });

    if (found_interpreter != interpreters.end())
        return call_winterp(path, found_interpreter->second, game);

    garglk::winmsg("Unknown file type: \"" + ext + "\"\nSorry.");

    return 0;
}
