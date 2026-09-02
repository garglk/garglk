// Copyright (C) 2026 by Chris Spiegel.
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

// Look fonts up by X Logical Font Description, e.g.
//
//     -misc-fixed-medium-r-normal--14-130-75-75-c-70-iso8859-1
//
// This reads fonts.dir and fonts.alias directly, so no X server is needed.

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "format.h"

#include "font.h"
#include "glk.h"
#include "garglk.h"

namespace {

// An XLFD is 14 fields, each introduced by a hyphen:
//
// -foundry-family-weight-slant-setwidth-addstyle-pixelsize-pointsize-resx-resy-spacing-avgwidth-registry-encoding
constexpr std::size_t XLFD_FIELDS = 14;
constexpr std::size_t XLFD_WEIGHT = 2;
constexpr std::size_t XLFD_SLANT = 3;
constexpr std::size_t XLFD_PIXELSIZE = 6;
constexpr std::size_t XLFD_AVGWIDTH = 11;
constexpr std::size_t XLFD_REGISTRY = 12;

using XlfdFields = std::array<std::string, XLFD_FIELDS>;

struct XFont {
    // Keep both forms: matching uses the full name; ranking uses its fields.
    std::string xlfd;
    XlfdFields fields;
    // Bitmap size from the XLFD.
    int size;
    std::string path;
    // Position in the font path, including order within each directory.
    std::size_t order;
};

std::vector<XFont> xfonts;
std::vector<std::pair<std::string, std::string>> xaliases;

std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) {
        return "";
    }

    return s.substr(start, s.find_last_not_of(" \t\r") - start + 1);
}

// XLFD globs are case-insensitive and may span fields.
bool xlfd_match(const std::string &pattern, const std::string &name)
{
    std::size_t p = 0, n = 0, star = std::string::npos, retry = 0;

    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
            p++;
            n++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            retry = n;
        } else if (star != std::string::npos) {
            p = star + 1;
            n = ++retry;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        p++;
    }

    return p == pattern.size();
}

std::optional<XlfdFields> xlfd_fields(const std::string &xlfd)
{
    if (xlfd.empty() || xlfd[0] != '-') {
        return std::nullopt;
    }

    XlfdFields fields;
    std::size_t count = 0;
    std::size_t start = 1;
    for (std::size_t i = 1; i <= xlfd.size(); i++) {
        if (i == xlfd.size() || xlfd[i] == '-') {
            if (count == XLFD_FIELDS) {
                return std::nullopt;
            }

            fields[count++] = xlfd.substr(start, i - start);
            start = i + 1;
        }
    }

    if (count != XLFD_FIELDS) {
        return std::nullopt;
    }

    return fields;
}

// A zero pixel size denotes a scalable font.
std::optional<int> pixelsize(const XlfdFields &fields)
{
    try {
        int size = std::stoi(fields[XLFD_PIXELSIZE]);
        if (size > 0) {
            return size;
        }
    } catch (const std::exception &) {
    }

    return std::nullopt;
}

std::string xlfd_join(const XlfdFields &fields)
{
    std::string xlfd;
    for (const auto &field : fields) {
        xlfd += "-" + field;
    }

    return xlfd;
}

// Sort the results to make lookup order stable.
std::vector<std::string> directories_under(const std::string &root)
{
    namespace fs = std::filesystem;

    std::vector<std::string> found;
    std::error_code ec;

    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    for (fs::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
        if (it->path().filename() == "fonts.dir") {
            found.push_back(it->path().parent_path().string());
        }
    }

    std::sort(found.begin(), found.end());

    return found;
}

// Names and aliases may occur in more than one directory, so search order
// matters. Start with the font server catalogue, then user and system fonts.
// Reading the files directly keeps lookup independent of an X display.
std::vector<std::string> font_directories()
{
    std::vector<std::string> dirs;

    std::ifstream f(X11_FS_CONFIG);
    std::string line;
    std::string catalogue;
    while (std::getline(f, line)) {
        line = trim(line);
        if (catalogue.empty()) {
            if (line.compare(0, 9, "catalogue") != 0) {
                continue;
            }

            auto equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            catalogue = trim(line.substr(equals + 1));
        } else {
            catalogue += line;
        }

        // A trailing comma continues the catalogue on the next line.
        if (catalogue.empty() || catalogue.back() != ',') {
            break;
        }
    }

    std::size_t start = 0;
    while (start <= catalogue.size() && !catalogue.empty()) {
        auto comma = catalogue.find(',', start);
        auto dir = trim(catalogue.substr(start, comma - start));

        // Font path entries may be decorated, as in "/usr/share/fonts:unscaled".
        auto colon = dir.find(':');
        if (colon != std::string::npos) {
            dir = dir.substr(0, colon);
        }

        while (dir.size() > 1 && dir.back() == '/') {
            dir.pop_back();
        }

        if (!dir.empty() && dir[0] == '/') {
            dirs.push_back(dir);
        }

        if (comma == std::string::npos) {
            break;
        }

        start = comma + 1;
    }

    std::vector<std::string> roots;

    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        const char *xdg = std::getenv("XDG_DATA_HOME");
        roots.push_back(Format("{}/.fonts", home));
        roots.push_back(xdg != nullptr ? Format("{}/fonts", xdg)
                                       : Format("{}/.local/share/fonts", home));
    }

    roots.emplace_back(X11_FONT_DIR);

    for (const auto &root : roots) {
        auto found = directories_under(root);
        dirs.insert(dirs.end(), found.begin(), found.end());
    }

    std::vector<std::string> unique;
    for (const auto &dir : dirs) {
        if (std::find(unique.begin(), unique.end(), dir) == unique.end()) {
            unique.push_back(dir);
        }
    }

    return unique;
}

// XLFDs may contain spaces, so the second field runs to end of line. Either
// field may be quoted, and backslashes escape the following character.
std::optional<std::string> read_field(const std::string &line, std::size_t &pos, bool to_end)
{
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        pos++;
    }

    if (pos == line.size()) {
        return std::nullopt;
    }

    bool quoted = line[pos] == '"';
    if (quoted) {
        pos++;
    }

    std::string field;
    for (; pos < line.size(); pos++) {
        if (line[pos] == '\\' && pos + 1 < line.size()) {
            field.push_back(line[++pos]);
        } else if (quoted) {
            if (line[pos] == '"') {
                pos++;
                break;
            }

            field.push_back(line[pos]);
        } else if (!to_end && (line[pos] == ' ' || line[pos] == '\t')) {
            break;
        } else {
            field.push_back(line[pos]);
        }
    }

    field = trim(field);
    if (field.empty()) {
        return std::nullopt;
    }

    return field;
}

// Parse a name/XLFD pair, ignoring blank, comment, and malformed lines.
std::optional<std::pair<std::string, std::string>> split_entry(const std::string &line)
{
    auto entry = trim(line);
    if (entry.empty() || entry[0] == '!') {
        return std::nullopt;
    }

    std::size_t pos = 0;
    auto name = read_field(entry, pos, false);
    auto xlfd = read_field(entry, pos, true);

    if (!name.has_value() || !xlfd.has_value()) {
        return std::nullopt;
    }

    return std::pair(*name, garglk::downcase(*xlfd));
}

void load_directory(const std::string &dir)
{
    std::ifstream fontsdir(dir + "/fonts.dir");
    std::string line;

    // The first line is a count of the entries that follow.
    if (std::getline(fontsdir, line)) {
        while (std::getline(fontsdir, line)) {
            auto entry = split_entry(line);
            if (!entry.has_value()) {
                continue;
            }

            const auto &[file, xlfd] = *entry;

            auto fields = xlfd_fields(xlfd);
            if (!fields.has_value()) {
                continue;
            }

            // XLFD lookup is for bitmap fonts; fontconfig handles scalable fonts.
            auto size = pixelsize(*fields);
            if (size.has_value()) {
                xfonts.push_back({xlfd, *fields, *size, Format("{}/{}", dir, file), xfonts.size()});
            }
        }
    }

    std::ifstream fontsalias(dir + "/fonts.alias");
    while (std::getline(fontsalias, line)) {
        auto entry = split_entry(line);
        if (entry.has_value()) {
            const auto &[alias, xlfd] = *entry;
            xaliases.emplace_back(garglk::downcase(alias), xlfd);
        }
    }
}

void load_fonts()
{
    static bool loaded = false;

    if (loaded) {
        return;
    }

    loaded = true;

    for (const auto &dir : font_directories()) {
        load_directory(dir);
    }
}

// Prefer encodings FreeType exposes through a Unicode charmap. Broad patterns
// may otherwise select a JIS font before a usable Latin one.
int encoding_rank(const XlfdFields &fields)
{
    if (fields[XLFD_REGISTRY] == "iso10646") {
        return 0;
    } else if (fields[XLFD_REGISTRY] == "iso8859") {
        return 1;
    } else {
        return 2;
    }
}

// Reject unusable matches before their bitmap size can affect fallback text.
std::optional<std::string> font_is_unusable(const XFont &xfont, FontType type)
{
    FT_Library ftlib;
    FT_Error err = FT_Init_FreeType(&ftlib);
    if (err != 0) {
        return garglk::convert_ft_error(err, "FT_Init_FreeType");
    }

    std::optional<std::string> reason;
    FT_Face face;

    err = FT_New_Face(ftlib, xfont.path.c_str(), 0, &face);
    if (err != 0) {
        reason = garglk::convert_ft_error(err, "FT_New_Face");
    } else {
        double aspect = type == FontType::Monospace ? gli_conf_monoaspect : gli_conf_propaspect;
        err = FT_Set_Char_Size(face, xfont.size * aspect * 64, xfont.size * 64, 72, 72);
        if (err != 0) {
            reason = garglk::convert_ft_error(err, "FT_Set_Char_Size");
        } else {
            err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
            if (err != 0) {
                reason = garglk::convert_ft_error(err, "FT_Select_Charmap");
            }
        }

        FT_Done_Face(face);
    }

    FT_Done_FreeType(ftlib);

    return reason;
}

// Distinguish no match from matches that FreeType could not load.
struct Lookup {
    const XFont *font = nullptr;
    bool matched = false;
    std::string reason;
};

Lookup find_xfont(const std::string &name, FontType type)
{
    auto pattern = garglk::downcase(trim(name));

    // Aliases may chain or resolve to patterns.
    for (int hop = 0; hop < 5; hop++) {
        auto alias = std::find_if(xaliases.begin(), xaliases.end(), [&pattern](const auto &entry) {
            return entry.first == pattern;
        });

        if (alias == xaliases.end()) {
            break;
        }

        pattern = alias->second;
    }

    // Prefer usable encodings, then preserve font-path order.
    auto rank = [](const XFont *xfont) {
        return std::pair(encoding_rank(xfont->fields), xfont->order);
    };

    std::vector<const XFont *> matches;
    for (const auto &xfont : xfonts) {
        if (xlfd_match(pattern, xfont.xlfd)) {
            matches.push_back(&xfont);
        }
    }

    std::sort(matches.begin(), matches.end(), [&rank](const XFont *a, const XFont *b) {
        return rank(a) < rank(b);
    });

    Lookup lookup;
    lookup.matched = !matches.empty();

    for (const auto *match : matches) {
        auto unusable = font_is_unusable(*match, type);
        if (!unusable.has_value()) {
            lookup.font = match;
            break;
        }

        // Report the best-ranked failure.
        if (lookup.reason.empty()) {
            lookup.reason = *unusable;
        }
    }

    return lookup;
}

// Ask for the same font in another weight or slant. The average width is
// wildcarded because a bold face is often wider than its regular.
std::optional<XFont> find_styled(XlfdFields fields, const std::string &weight, const std::string &slant, FontType type)
{
    fields[XLFD_WEIGHT] = weight;
    fields[XLFD_SLANT] = slant;
    fields[XLFD_AVGWIDTH] = "*";

    auto lookup = find_xfont(xlfd_join(fields), type);
    if (lookup.font == nullptr) {
        return std::nullopt;
    }

    return *lookup.font;
}

}

bool garglk::x11_fonts_available()
{
    load_fonts();

    return !xfonts.empty();
}

garglk::XFontResult garglk::fontreplace_x11(const std::string &xlfd, FontType type)
{
    load_fonts();

    auto lookup = find_xfont(xlfd, type);
    if (lookup.font == nullptr) {
        if (lookup.matched) {
            return {XFontResult::Status::Unusable, lookup.reason};
        }

        return {XFontResult::Status::NoMatch, ""};
    }

    const XFont &regular = *lookup.font;

    const auto &regular_weight = regular.fields[XLFD_WEIGHT];

    FontFiller filler(type);

    filler.add(FontFiller::Style::Regular, regular.path);

    auto styled = [&regular, type](const std::string &weight, const std::vector<std::string> &slants) -> std::optional<std::string> {
        for (const auto &slant : slants) {
            auto found = find_styled(regular.fields, weight, slant, type);
            if (found.has_value() && found->path != regular.path) {
                return found->path;
            }
        }

        return std::nullopt;
    };

    // "i" is a true italic and "o" an oblique; X fonts use both.
    std::vector<std::string> upright = {"r"};
    std::vector<std::string> italic = {"i", "o"};

    filler.add(FontFiller::Style::Bold, styled("bold", upright));
    filler.add(FontFiller::Style::Italic, styled(regular_weight, italic));
    filler.add(FontFiller::Style::BoldItalic, styled("bold", italic));

    if (!filler.fill()) {
        return {XFontResult::Status::Unusable, ""};
    }

    // Bitmap font size comes from the XLFD, not monosize or propsize.
    if (type == FontType::Monospace) {
        gli_conf_monosize = regular.size;
    } else {
        gli_conf_propsize = regular.size;
    }

    return {XFontResult::Status::Loaded, ""};
}
