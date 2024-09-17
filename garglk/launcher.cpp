// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2009 by Baltasar García Perez-Schofield.
// Copyright (C) 2010 by Ben Cressey.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <ios>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if __cplusplus >= 201703L
#include <filesystem>
#endif

#include "format.h"
#include "optional.hpp"

#include "glk.h"
#include "glkstart.h"
#include "gi_blorb.h"
#include "garglk.h"
#include "launcher.h"

#define T_ADRIFT    "scare"
#define T_ADRIFT5   "FrankenDrift.GlkRunner.Gargoyle"
#define T_ADVSYS    "advsys"
#define T_AGT       "agility"
#define T_ALAN2     "alan2"
#define T_ALAN3     "alan3"
#define T_GLULX     "git"
#define T_HUGO      "hugo"
#define T_JACL      "jacl"
#define T_LEV9      "level9"
#define T_MGSR      "magnetic"
#define T_PLUS      "plus"
#define T_SCOTT     "scott"
#define T_TADS      "tadsr"
#define T_TAYLOR    "taylor"
#define T_ZCODE     "bocfel"

#define ID_ZCOD (giblorb_make_id('Z', 'C', 'O', 'D'))
#define ID_GLUL (giblorb_make_id('G', 'L', 'U', 'L'))
#define ID_ADRI (giblorb_make_id('A', 'D', 'R', 'I'))

namespace {

struct Interpreter {
    explicit Interpreter(std::string terp_, std::vector<std::string> flags_ = {}) :
        terp(std::move(terp_)),
        flags(std::move(flags_))
    {
    }

    std::string terp;
    std::vector<std::string> flags;
};

}

enum class Format {
    Adrift,
    Adrift5,
    AdvSys,
    AGT,
    Alan2,
    Alan3,
    Glulx,
    Hugo,
    JACL,
    Level9,
    Magnetic,
    Plus,
    Scott,
    TADS,
    Taylor,
    ZCode,
};

static nonstd::optional<Format> probe(const std::array<char, 32> &header)
{
    std::vector<std::pair<std::string, Format>> magic = {
        {R"(^[\x01-\x08][\s\S]{17}\d{6})", Format::ZCode},
        {R"(^TADS2 bin\x0a\x0d\x1a)", Format::TADS},
        {R"(^T3-image\x0d\x0a\x1a[\x01\x02]\x00)", Format::TADS},
        {R"(^Glul)", Format::Glulx},
        {R"(^MaSc[\s\S]{4}\x00\x00\x00\x2a\x00[\x00\x01\x02\x03\x04])", Format::Magnetic},
        {R"(^\x3c\x42\x3f\xc9\x6a\x87\xc2\xcf[\x93\x94]\x45)", Format::Adrift},
        {R"(^\x3c\x42\x3f\xc9\x6a\x87\xc2\xcf[\x92]\x45)", Format::Adrift5},
        {R"(^\x58\xc7\xc1\x51)", Format::AGT},
        {R"(^[\s\S]{2}\xa0\x9d\x8b\x8e\x88\x8e)", Format::AdvSys},
        {R"(^[\x16\x18\x19\x1e\x1f][\s\S]{2}\d\d-\d\d-\d\d)", Format::Hugo},
        {R"(^[\s\S]{3}\x9b\x36\x21[\s\S]{18}\xff)", Format::Level9},
        {R"(^\x02(\x07\x05|\x08[\x01\x02\x03\x07]))", Format::Alan2},
        {R"(^ALAN\x03)", Format::Alan3},
    };

    for (const auto &pair : magic) {
        if (std::regex_search(header.begin(), header.end(), std::regex(pair.first))) {
            return pair.second;
        }
    }

    return nonstd::nullopt;
}

// Map extensions to formats
static const std::unordered_map<std::string, Format> extensions = {
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
    {"plus", Format::Plus},
    {"saga", Format::Scott},
    {"gam", Format::TADS},
    {"t3", Format::TADS},
    {"tay", Format::Taylor},
    {"dat", Format::Scott},
    {"z1", Format::ZCode},
    {"z2", Format::ZCode},
    {"z3", Format::ZCode},
    {"z4", Format::ZCode},
    {"z5", Format::ZCode},
    {"z6", Format::ZCode},
    {"z7", Format::ZCode},
    {"z8", Format::ZCode},
};

// Map formats to default interpreters
static const std::unordered_map<Format, Interpreter> interpreters = {
    {Format::Adrift, Interpreter(T_ADRIFT)},
#ifdef WITH_FRANKENDRIFT
    {Format::Adrift5, Interpreter(T_ADRIFT5)},
#endif
    {Format::AdvSys, Interpreter(T_ADVSYS)},
    {Format::AGT, Interpreter(T_AGT, {"-gl"})},
    {Format::Alan2, Interpreter(T_ALAN2)},
    {Format::Alan3, Interpreter(T_ALAN3)},
    {Format::Glulx, Interpreter(T_GLULX)},
    {Format::Hugo, Interpreter(T_HUGO)},
    {Format::JACL, Interpreter(T_JACL)},
    {Format::Level9, Interpreter(T_LEV9)},
    {Format::Magnetic, Interpreter(T_MGSR)},
    {Format::Plus, Interpreter(T_PLUS)},
    {Format::Scott, Interpreter(T_SCOTT)},
    {Format::TADS, Interpreter(T_TADS)},
    {Format::Taylor, Interpreter(T_TAYLOR)},
    {Format::ZCode, Interpreter(T_ZCODE)},
};

static bool call_winterp(const Interpreter &interpreter, const std::string &game)
{
    return garglk::winterp(GARGLKPRE + interpreter.terp, interpreter.flags, game);
}

static bool call_winterp(Format format, const std::string &game)
{
    try {
        return call_winterp(interpreters.at(format), game);
    } catch (const std::out_of_range &) {
        // The FrankenDrift interpreter for Adrift 5 games is not supported on all
        // platforms. Let the user know when this is the case.
#ifndef WITH_FRANKENDRIFT
        if (format == Format::Adrift5) {
            garglk::winmsg("Adrift 5 games are not supported by this build of Gargoyle");
            return false;
        }
#endif

        // This should never occur: apart from Adrift 5, handled above,
        // all known game formats always have interpreters.
        garglk::winmsg("This game type is not supported");
        return false;
    }
}

// Adrift 5 can create invalid Blorb files: the reported file size is
// smaller than the actual file size. gi_blorb (correctly) fails to load
// these files, but they still exist, so must be dealt with. If the
// overall size is ignored and the Blorb file is just read as though
// it's valid, all the chunks can be found (which is to say, the only
// problem with them is the file size, and that's not needed to iterate
// through the file). So, do just enough work here to determine if
// there's an Exec resource of type ADRI. If so, assume this is an
// Adrift file and leave it to the interpreter to run it properly.
static nonstd::optional<Format> find_adrift_blorb_format(const std::string &game)
{
    std::ifstream f(game, std::ios::binary);
    try {
        f.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);

        auto be32 = [&f]() {
            std::array<char, 4> bytes;
            f.read(bytes.data(), bytes.size());

            return (static_cast<std::uint32_t>(bytes[0]) << 24) |
                   (static_cast<std::uint32_t>(bytes[1]) << 16) |
                   (static_cast<std::uint32_t>(bytes[2]) <<  8) |
                   (static_cast<std::uint32_t>(bytes[3]) <<  0);
        };

        // Probing has already determined that this file starts with
        // FORM....IFRSRidx, so there's no need to validate that.
        f.seekg(20, std::ios::beg);

        auto nresources = be32();

        for (std::uint32_t i = 0; i < nresources; i++) {
            if (be32() == giblorb_ID_Exec) {
                f.seekg(4, std::ios::cur);
                f.seekg(be32(), std::ios::beg);
                if (be32() != ID_ADRI) {
                    return nonstd::nullopt;
                }

                f.seekg(12, std::ios::cur);
                unsigned char version;
                f.read(reinterpret_cast<char *>(&version), 1);

                if (version == 0x92) {
                    return Format::Adrift5;
                } else if (version == 0x93 || version == 0x94) {
                    return Format::Adrift;
                } else {
                    return nonstd::nullopt;
                }
            } else {
                f.seekg(8, std::ios::cur);
            }
        }
    } catch (const std::ifstream::failure &) {
    }

    return nonstd::nullopt;
}

static bool runblorb(const std::string &game)
{
    class BlorbError : public std::runtime_error {
    public:
        explicit BlorbError(const std::string &msg) : std::runtime_error(msg) {
        }
    };

    auto adrift_format = find_adrift_blorb_format(game);
    if (adrift_format.has_value()) {
        return call_winterp(*adrift_format, game);
    }

    try {
        giblorb_result_t res;
        giblorb_map_t *basemap;

        auto file = garglk::unique(glkunix_stream_open_pathname(const_cast<char *>(game.c_str()), 0, 0), [](strid_t file) {
            glk_stream_close(file, nullptr);
        });
        if (!file) {
            throw BlorbError("Unable to open file");
        }

        if (giblorb_create_map(file.get(), &basemap) != giblorb_err_None) {
            throw BlorbError("Does not appear to be a valid Blorb file");
        }

        auto map = garglk::unique(basemap, giblorb_destroy_map);

        if (giblorb_load_resource(map.get(), giblorb_method_FilePos, &res, giblorb_ID_Exec, 0) != giblorb_err_None) {
            throw BlorbError("Does not contain a story file (look for a corresponding game file to load instead)");
        }

        if (res.chunktype == ID_ZCOD) {
            return call_winterp(Format::ZCode, game);
        } else if (res.chunktype == ID_GLUL) {
            return call_winterp(Format::Glulx, game);
        }

        std::string name;
        auto val = [](unsigned char c) -> char {
            return std::isprint(c) ? c : '?';
        };
        auto ck = res.chunktype;

        auto msg = Format("Unknown game type: {:#08x} ({}{}{}{})", ck, val(ck >> 24), val(ck >> 16), val(ck >> 8), val(ck));
        throw BlorbError(msg);
    } catch (const BlorbError &e) {
        garglk::winmsg(Format("Could not load Blorb file {}:\n{}", game, e.what()));
        return false;
    }
}

static nonstd::optional<Interpreter> findterp(const std::string &file, const std::string &target)
{
    std::vector<std::string> matches = {target};

    nonstd::optional<Interpreter> interpreter;

    garglk::config_entries(file, false, matches, [&interpreter](const std::string &cmd, const std::string &arg, int) {
        if (cmd == "terp") {
            std::istringstream argstream(arg);
            std::string terp, flag;
            std::vector<std::string> flags;

            if (argstream >> terp) {
                while (argstream >> flag) {
                    flags.push_back(flag);
                }

                interpreter.emplace(terp, flags);
            }
        }
    });

    return interpreter;
}

// Find a possible interpreter specified in the config file.
static nonstd::optional<Interpreter> configterp(const std::string &gamepath)
{
    std::string story = gamepath;

    // set up story
#if __cplusplus >= 201703L
    story = std::filesystem::path(story)
        .filename()
        .string();
#else
    auto slash = story.rfind('\\');
    if (slash == std::string::npos) {
        slash = story.find_last_of('/');
    }
    if (slash != std::string::npos) {
        story = story.substr(slash + 1);
    }
#endif

    if (story.empty()) {
        return nonstd::nullopt;
    }

    for (const auto &config : garglk::configs(gamepath)) {
        auto interpreter = findterp(config.path, story);
        if (interpreter.has_value()) {
            return interpreter;
        }
    }

    return nonstd::nullopt;
}

bool garglk::rungame(const std::string &game)
{
    std::array<char, 32> header;

    auto interpreter = configterp(game);
    if (interpreter.has_value()) {
        return call_winterp(*interpreter, game);
    }

    std::ifstream f(game, std::ios::binary);
    if (!f.is_open()) {
        garglk::winmsg(Format("Unable to open {}", game));
        return false;
    }

    if (f.read(header.data(), header.size())) {
        auto is_blorb = std::regex_search(header.begin(), header.end(), std::regex(R"(^FORM[\s\S]{4}IFRSRIdx)"));
        if (is_blorb) {
            return runblorb(game);
        }

        auto format = probe(header);
        if (format.has_value()) {
            return call_winterp(*format, game);
        }
    }

    std::string ext = "";
    auto dot = game.rfind('.');
    if (dot != std::string::npos) {
        ext = garglk::downcase(game.substr(dot + 1));
    }

    try {
        return call_winterp(extensions.at(ext), game);
    } catch (const std::out_of_range &) {
        garglk::winmsg("Unknown file type");
    }

    return false;
}
