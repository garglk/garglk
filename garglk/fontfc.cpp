// Copyright (C) 2010 by Ben Cressey, Sylvain Beucler, Chris Spiegel.
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

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <fontconfig/fontconfig.h>

#include "optional.hpp"

#include "glk.h"
#include "garglk.h"

static bool initialized = false;

static nonstd::optional<std::string> findfont(const std::string &fontname)
{
    FcChar8 *strval = nullptr;

    auto p = garglk::unique(FcNameParse(reinterpret_cast<const FcChar8 *>(fontname.c_str())), FcPatternDestroy);
    if (p == nullptr) {
        return nonstd::nullopt;
    }

    auto os = garglk::unique(FcObjectSetBuild(FC_FILE, static_cast<char *>(nullptr)), FcObjectSetDestroy);
    if (os == nullptr) {
        return nonstd::nullopt;
    }

    auto fs = garglk::unique(FcFontList(nullptr, p.get(), os.get()), FcFontSetDestroy);
    if (fs->nfont == 0) {
        return nonstd::nullopt;
    }

    if (FcPatternGetString(fs->fonts[0], FC_FILE, 0, &strval) == FcResultTypeMismatch || strval == nullptr) {
        return nonstd::nullopt;
    }

    return reinterpret_cast<char *>(strval);
}

static nonstd::optional<std::string> find_font_by_styles(const std::string &basefont, const std::vector<std::string> &styles, const std::vector<int> &weights, const std::vector<std::string> &slants)
{
    // Prefer normal width fonts, but if there aren't any, allow whatever fontconfig finds.
    std::vector<std::string> widths = {":normal", ""};

    for (const auto &width : widths) {
        for (const auto &style : styles) {
            for (const auto &weight : weights) {
                for (const auto &slant : slants) {
                    std::ostringstream fontname;
                    fontname << basefont << ":style=" << style << ":weight=" << weight << ":" << slant << width;

                    auto fontpath = findfont(fontname.str());
                    if (fontpath.has_value()) {
                        return fontpath;
                    }
                }
            }
        }
    }

    return nonstd::nullopt;
}

void garglk::fontreplace(const std::string &font, FontType type)
{
    if (!initialized || font.empty()) {
        return;
    }

    nonstd::optional<std::string> sysfont;
    std::string *r, *b, *i, *z;

    if (type == FontType::Monospace) {
        r = &gli_conf_mono.r;
        b = &gli_conf_mono.b;
        i = &gli_conf_mono.i;
        z = &gli_conf_mono.z;
    } else {
        r = &gli_conf_prop.r;
        b = &gli_conf_prop.b;
        i = &gli_conf_prop.i;
        z = &gli_conf_prop.z;
    }

    // Although there are 4 "main" types of font (Regular, Italic, Bold, Bold
    // Italic), there are actually a whole lot more possibilities, and,
    // unfortunately, some fonts are inconsistent in what they report about
    // their attributes.
    //
    // It's possible to ask fontconfig for a particular style (e.g. "Regular"),
    // but that's not enough. The font Noto Serif, for example, has a font that
    // lists its style as both "Condensed ExtraLight" and "Regular". If
    // "style=Regular" were used to find regular fonts, this font would show up,
    // which is not desirable. So in addition to the style, the weight (an
    // integer) must also be requested.
    //
    // Going the other direction, Adobe Garamond has a font with the style
    // "Regular Expert", the expert character set being totally unusable in
    // Gargoyle. However, its integral weight is still regular. So asking just
    // for a regular weight isn't sufficient: the style needs to be taken into
    // account as well.
    //
    // In short, for each style, try various combinations of style name, weight,
    // and slant in hopes of getting the desired font. These are ordered from
    // highest to lowest priority, so "Italic regular italic" will be preferred
    // over "Medium Oblique book oblique", for example.
    std::vector<std::string> regular_styles = {"Regular", "Book", "Medium", "Roman"};
    std::vector<std::string> italic_styles = {"Italic", "Regular Italic", "Medium Italic", "Book Italic", "Oblique", "Regular Oblique", "Medium Oblique", "Book Oblique"};
    std::vector<std::string> bold_styles = {"Bold", "Extrabold", "Semibold", "Black"};
    std::vector<std::string> bold_italic_styles = {"Bold Italic", "Extrabold Italic", "Semibold Italic", "Black Italic", "Bold Oblique", "Extrabold Oblique", "Semibold Oblique", "Black Oblique"};

    // Fontconfig could/should accept "medium" (for example), and this
    // generally works, except that in some rare cases I've seen it
    // return weights that are not medium (100); explicitly listing the
    // weight value doesn't have this problem.

    std::vector<int> regular_weights = {FC_WEIGHT_REGULAR, FC_WEIGHT_BOOK, FC_WEIGHT_MEDIUM};
    std::vector<int> bold_weights = {FC_WEIGHT_BOLD, FC_WEIGHT_EXTRABOLD, FC_WEIGHT_DEMIBOLD, FC_WEIGHT_BLACK};

    std::vector<std::string> roman_slants = {"roman"};
    std::vector<std::string> italic_slants = {"italic", "oblique"};

    // regular or roman
    sysfont = find_font_by_styles(font, regular_styles, regular_weights, roman_slants);
    if (sysfont.has_value()) {
        *r = *sysfont;
        *b = *sysfont;
        *i = *sysfont;
        *z = *sysfont;
    } else {
        // A regular font is required to exist, so bail if it doesn't
        // (even if bold, italic, and/or bold italic are available).
        // Without this, it'd be possible to get a setup where, for
        // example, the bold font is one family and all other fonts are
        // the fallback. Bold and italic variations can be created out
        // of a regular font, so it's OK if those don't exist.
        return;
    }

    // bold
    sysfont = find_font_by_styles(font, bold_styles, bold_weights, roman_slants);
    if (sysfont.has_value()) {
        *b = *sysfont;
        *z = *sysfont;
    }

    // italic or oblique
    sysfont = find_font_by_styles(font, italic_styles, regular_weights, italic_slants);
    if (sysfont.has_value()) {
        *i = *sysfont;
        *z = *sysfont;
    }

    // bold italic or bold oblique
    sysfont = find_font_by_styles(font, bold_italic_styles, bold_weights, italic_slants);
    if (sysfont.has_value()) {
        *z = *sysfont;
    }
}

void fontload()
{
    initialized = FcInit() != 0;
}

void fontunload()
{
    if (initialized) {
        FcFini();
    }
}
