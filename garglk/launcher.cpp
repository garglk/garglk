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
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#if __cplusplus >= 201703L
#include <filesystem>
#endif

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
#define T_SCOTT     "scott"
#define T_TADS2     "tadsr"
#define T_TADS3     "tadsr"
#define T_ZCODE     "bocfel"
#define T_ZSIX      "bocfel"

#define ID_ZCOD (giblorb_make_id('Z','C','O','D'))
#define ID_GLUL (giblorb_make_id('G','L','U','L'))

struct Interpreter {
    explicit Interpreter(const std::string &terp_, const std::string &flags_ = "") :
        terp(terp_),
        flags(flags_) {
        }

    std::string terp;
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
    Scott,
    TADS2,
    TADS3,
    ZCode,
    ZCode6,
};

static std::unique_ptr<Format> probe(const std::vector<char> &header)
{
    std::map<std::string, Format> magic = {
        {R"(^[\x01\x02\x03\x04\x05\x07\x08][\s\S]{17}\d{6})", Format::ZCode},
        {R"(^\x06[\s\S]{17}\d{6})", Format::ZCode6},
        {R"(^TADS2 bin\x0a\x0d\x1a)", Format::TADS2},
        {R"(^T3-image\x0d\x0a\x1a(\x01|\x02)\x00)", Format::TADS3},
        {R"(^Glul)", Format::Glulx},
        {R"(^MaSc[\s\S]{4}\x00\x00\x00\x2a\x00[\x00\x01\x02\x03\x04])", Format::Magnetic},
        {R"(^\x3c\x42\x3f\xc9\x6a\x87\xc2\xcf[\x92\x93\x94]\x45[\x3e\x37\x36]\x61)", Format::Adrift},
        {R"(^\x58\xc7\xc1\x51)", Format::AGT},
        {R"(^[\s\S]{2}\xa0\x9d\x8b\x8e\x88\x8e)", Format::AdvSys},
        {R"(^[\x16\x18\x19\x1e\x1f][\s\S]{2}\d\d-\d\d-\d\d)", Format::Hugo},
        {R"(^[\s\S]{3}\x9b\x36\x21[\s\S]{18}\xff)", Format::Level9},
        {R"(^(\x02\x07\x05|\x02\x08[\x01\x02\x03\x07]))", Format::Alan2},
        {R"(^ALAN\x03)", Format::Alan3},
    };

    for (const auto &pair : magic)
    {
        auto re = std::regex(pair.first);
        if (std::regex_search(header.begin(), header.end(), re))
            return std::make_unique<Format>(pair.second);
    }

    return nullptr;
}

// Map extensions to formats
static const std::map<std::string, Format> extensions = {
    {"taf", Format::Adrift},
    {"agx", Format::AGT},
    {"d$$", Format::AGT},
    {"acd", Format::Alan2},
    {"a3c", Format::Alan3},
    {"ulx", Format::Glulx},
    {"hex", Format::Hugo},
    {"j2", Format::JACL},
    {"jacl", Format::JACL},
    {"l9", Format::Level9},
    {"sna", Format::Level9},
    {"mag", Format::Magnetic},
    {"saga", Format::Scott},
    {"gam", Format::TADS2},
    {"t3", Format::TADS3},
    {"dat", Format::ZCode},
    {"z1", Format::ZCode},
    {"z2", Format::ZCode},
    {"z3", Format::ZCode},
    {"z4", Format::ZCode},
    {"z5", Format::ZCode},
    {"z7", Format::ZCode},
    {"z8", Format::ZCode},
    {"z6", Format::ZCode6},
};

// Map formats to default interpreters
static const std::map<Format, Interpreter> interpreters = {
    { Format::Adrift, Interpreter(T_ADRIFT) },
    { Format::AdvSys, Interpreter(T_ADVSYS) },
    { Format::AGT, Interpreter(T_AGT, "-gl") },
    { Format::Alan2, Interpreter(T_ALAN2) },
    { Format::Alan3, Interpreter(T_ALAN3) },
    { Format::Glulx, Interpreter(T_GLULX) },
    { Format::Hugo, Interpreter(T_HUGO) },
    { Format::JACL, Interpreter(T_JACL) },
    { Format::Level9, Interpreter(T_LEV9) },
    { Format::Magnetic, Interpreter(T_MGSR) },
    { Format::Scott, Interpreter(T_SCOTT) },
    { Format::TADS2, Interpreter(T_TADS2) },
    { Format::TADS3, Interpreter(T_TADS3) },
    { Format::ZCode, Interpreter(T_ZCODE) },
    { Format::ZCode6, Interpreter(T_ZSIX) },
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
#if __cplusplus >= 201703L
    story = std::filesystem::path(story)
        .filename()
        .string();
#else
    auto slash = story.rfind('\\');
    if (slash == std::string::npos)
        slash = story.find_last_of('/');
    if (slash != std::string::npos)
        story = story.substr(slash + 1);
#endif

    if (story.empty())
        return;

    for (const auto &config : garglk::configs(exedir, gamepath))
    {
        if (findterp(config.path, story, interpreter))
            return;
    }
}

int garglk::rungame(const std::string &path, const std::string &game)
{
    const std::set<std::string> blorbs = {"blb", "blorb", "glb", "gbl", "gblorb", "zlb", "zbl", "zblorb"};
    Interpreter interpreter("");
    std::vector<char> header(64);
    bool is_blorb = false;

    configterp(path, game, interpreter);

    std::ifstream f(game, std::ios::binary);
    if (f.is_open() && f.read(&header[0], header.size()))
    {
        if (interpreter.terp.empty())
        {
            auto format = probe(header);
            if (format != nullptr)
                interpreter = interpreters.at(*format);
        }

        is_blorb = std::regex_search(header.begin(), header.end(), std::regex(R"(^FORM[\s\S]{4}IFRSRIdx)"));
    }

    std::string ext = "";
    auto dot = game.rfind('.');
    if (dot != std::string::npos)
        ext = garglk::downcase(game.substr(dot + 1));

    if (is_blorb ||
        std::find(blorbs.begin(), blorbs.end(), ext) != blorbs.end())
    {
        return runblorb(path, game, interpreter);
    }

    if (!interpreter.terp.empty())
        return call_winterp(path, interpreter, game);

    try
    {
        auto format = extensions.at(ext);
        return call_winterp(path, interpreters.at(format), game);
    }
    catch (const std::out_of_range &)
    {
    }

    garglk::winmsg("Unknown file type: \"" + ext + "\"\nSorry.");

    return 0;
}
