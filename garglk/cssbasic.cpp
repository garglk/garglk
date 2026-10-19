// Copyright (C) 2026 by the Gargoyle contributors.
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

// The basic profile of Dannii Willis' CSS Glk extension: games may
// attach a small set of CSS declarations either to a style (a "hint",
// which behaves much like glk_stylehint_set), via a Style_* selector,
// or to the text about to be printed (an "inline" declaration).
// glk_css_supports is part of the full profile and is not implemented.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "glk.h"
#include "garglk.h"

namespace {

// Interned CSS font-family entries (ids stored on attr_t / style_t).
std::vector<CssFontFamily> gli_css_families;
std::unordered_map<std::string, std::uint16_t> gli_css_family_keys;

// Hints are stored per window type, per style, and per level (span or
// paragraph).
using HintTable = std::array<std::array<CssProps, 2>, style_NUMSTYLES>;

HintTable gli_css_buffer_hints;
HintTable gli_css_grid_hints;
CssProps gli_css_buffer_window_hints;
CssProps gli_css_grid_window_hints;
bool gli_css_have_hints = false;

std::string trim(const std::string &str)
{
    const auto *whitespace = " \t\r\n\f\v";

    auto begin = str.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return "";
    }

    auto end = str.find_last_not_of(whitespace);

    return str.substr(begin, end - begin + 1);
}

std::string unquote(const std::string &str)
{
    if (str.size() >= 2 &&
            ((str.front() == '"' && str.back() == '"') ||
             (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.size() - 2);
    }

    return str;
}

std::vector<std::string> split(const std::string &str, char delim)
{
    std::vector<std::string> parts;
    std::string::size_type start = 0;

    while (true) {
        auto pos = str.find(delim, start);
        if (pos == std::string::npos) {
            parts.push_back(str.substr(start));
            break;
        }
        parts.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }

    return parts;
}

struct CssColor {
    bool valid = false;
    bool transparent = false;
    Color color = Color(0, 0, 0);
};

CssColor parse_color(const std::string &value)
{
    static const std::map<std::string, unsigned long> named = {
        {"black", 0x000000}, {"silver", 0xc0c0c0}, {"gray", 0x808080},
        {"grey", 0x808080}, {"white", 0xffffff}, {"maroon", 0x800000},
        {"red", 0xff0000}, {"purple", 0x800080}, {"fuchsia", 0xff00ff},
        {"green", 0x008000}, {"lime", 0x00ff00}, {"olive", 0x808000},
        {"yellow", 0xffff00}, {"navy", 0x000080}, {"blue", 0x0000ff},
        {"teal", 0x008080}, {"aqua", 0x00ffff}, {"orange", 0xffa500},
        {"cyan", 0x00ffff}, {"magenta", 0xff00ff}, {"pink", 0xffc0cb},
    };

    CssColor result;

    auto v = garglk::downcase(trim(value));
    if (v.empty()) {
        return result;
    }

    if (v == "transparent") {
        result.valid = true;
        result.transparent = true;
        return result;
    }

    auto named_it = named.find(v);
    if (named_it != named.end()) {
        auto c = named_it->second;
        result.valid = true;
        result.color = Color((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
        return result;
    }

    if (v[0] != '#') {
        return result;
    }

    auto hex = v.substr(1);
    if (hex.find_first_not_of("0123456789abcdef") != std::string::npos) {
        return result;
    }

    unsigned long raw = std::strtoul(hex.c_str(), nullptr, 16);
    unsigned long r, g, b, a = 255;

    switch (hex.size()) {
    case 3:
        r = ((raw >> 8) & 0xf) * 0x11;
        g = ((raw >> 4) & 0xf) * 0x11;
        b = (raw & 0xf) * 0x11;
        break;
    case 4:
        r = ((raw >> 12) & 0xf) * 0x11;
        g = ((raw >> 8) & 0xf) * 0x11;
        b = ((raw >> 4) & 0xf) * 0x11;
        a = (raw & 0xf) * 0x11;
        break;
    case 6:
        r = (raw >> 16) & 0xff;
        g = (raw >> 8) & 0xff;
        b = raw & 0xff;
        break;
    case 8:
        r = (raw >> 24) & 0xff;
        g = (raw >> 16) & 0xff;
        b = (raw >> 8) & 0xff;
        a = raw & 0xff;
        break;
    default:
        return result;
    }

    result.valid = true;
    result.transparent = a == 0;
    result.color = Color(r, g, b);

    return result;
}

// Absolute lengths (pt, px, bare numbers) are CSS logical units and must be
// scaled by gli_zoom so they match gli_conf_propsize / gli_conf_monosize,
// which already include zoom and the display backing scale. Relative units
// (em, %) are computed against a base that is already in zoomed coordinates.
// pt and px are treated identically, as in Spatterlight's CSS Basic mapper.
double parse_length(const std::string &value, double base)
{
    auto v = garglk::downcase(trim(value));
    if (v.empty()) {
        return 0;
    }

    auto number = [](const std::string &str) {
        try {
            return std::stod(str);
        } catch (const std::exception &) {
            return 0.0;
        }
    };

    auto ends_with = [&v](const std::string &suffix) {
        return v.size() > suffix.size() &&
               v.compare(v.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    if (v.back() == '%') {
        return base * (number(v.substr(0, v.size() - 1)) / 100.0);
    }
    if (ends_with("em")) {
        return base * number(v.substr(0, v.size() - 2));
    }
    if (ends_with("pt") || ends_with("px")) {
        return number(v.substr(0, v.size() - 2)) * gli_zoom;
    }

    return number(v) * gli_zoom;
}

// Intern a resolved family under a stable key so repeated CSS values
// reuse the same id.
std::uint16_t intern_family(const std::string &key, FontFiles files, bool monospace)
{
    auto it = gli_css_family_keys.find(key);
    if (it != gli_css_family_keys.end()) {
        return it->second;
    }

    if (gli_css_families.size() >= 0xffff) {
        return 0;
    }

    auto id = static_cast<std::uint16_t>(gli_css_families.size());
    gli_css_families.push_back({std::move(files), monospace});
    gli_css_family_keys.emplace(key, id);
    return id;
}

bool name_looks_monospace(const std::string &name)
{
    return name == "monospace" || name == "mono" || name == "terminal" ||
           name == "consolas" || name == "monaco" || name == "menlo" ||
           name.find("courier") != std::string::npos ||
           name.find("ocr") != std::string::npos;
}

// Resolve a CSS font-family list like a browser: walk left to right and
// take the first family that can be honored. Generics map to configured
// mono/prop fonts or a platform sans; named families use fontlookup.
std::optional<std::uint16_t> resolve_font_family(const std::string &value, bool &monospace)
{
    for (const auto &family : split(value, ',')) {
        auto name = garglk::downcase(unquote(trim(family)));
        if (name.empty()) {
            continue;
        }

        if (name == "monospace") {
            monospace = true;
            return intern_family("\1monospace", gli_conf_mono, true);
        }

        if (name == "serif") {
            monospace = false;
            return intern_family("\1serif", gli_conf_prop, false);
        }

        if (name == "sans-serif") {
            static const char *const sans_candidates[] = {
                "Helvetica", "Arial", "DejaVu Sans", "sans-serif",
            };
            for (const char *candidate : sans_candidates) {
                auto files = garglk::fontlookup(candidate);
                if (files.has_value()) {
                    monospace = false;
                    return intern_family("\1sans-serif", *files, false);
                }
            }
            monospace = false;
            return intern_family("\1sans-serif", gli_conf_prop, false);
        }

        auto original = unquote(trim(family));
        auto files = garglk::fontlookup(original);
        if (!files.has_value() && original != name) {
            files = garglk::fontlookup(name);
        }
        if (files.has_value()) {
            monospace = name_looks_monospace(name);
            return intern_family(name, *files, monospace);
        }
    }

    return std::nullopt;
}

bool parse_bool(const std::string &value)
{
    return value == "1" || value == "true" || value == "yes";
}

std::optional<glui32> parse_justification(const std::string &value)
{
    if (value == "left") {
        return stylehint_just_LeftFlush;
    }
    if (value == "right") {
        return stylehint_just_RightFlush;
    }
    if (value == "center") {
        return stylehint_just_Centered;
    }
    if (value == "justify") {
        return stylehint_just_LeftRight;
    }

    return std::nullopt;
}

HintTable &hint_table(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_css_grid_hints : gli_css_buffer_hints;
}

Styles &global_styles(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_gstyles : gli_tstyles;
}

const Styles &default_styles(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_gstyles_def : gli_tstyles_def;
}

bool valid_wintype(glui32 wintype)
{
    return wintype == wintype_TextGrid || wintype == wintype_TextBuffer;
}

// Restore whatever a single property had set on a style back to its
// configured default.
void reset_style_property(style_t &style, const style_t &def, const std::string &prop)
{
    if (prop == "color") {
        style.fg = def.fg;
    } else if (prop == "background-color") {
        // Span and paragraph levels both use this property name; clear both
        // so reapplying the remaining level can restore the right one.
        style.bg = def.bg;
        style.para_bg = def.para_bg;
    } else if (prop == "reverse" || prop == "--glk-reverse" || prop == "-iftf-reverse-video") {
        style.reverse = def.reverse;
    } else if (prop == "font-weight") {
        style.font.bold = def.font.bold;
    } else if (prop == "font-style") {
        style.font.italic = def.font.italic;
    } else if (prop == "font-family") {
        style.font.monospace = def.font.monospace;
        style.family_id = def.family_id;
    } else if (prop == "monospace") {
        style.font.monospace = def.font.monospace;
    } else if (prop == "font-size") {
        style.size = def.size;
    } else if (prop == "text-decoration" || prop == "text-decoration-line") {
        style.underline = def.underline;
    } else if (prop == "text-align") {
        style.justification = def.justification;
    } else if (prop == "margin-left") {
        style.margin_left = def.margin_left;
    } else if (prop == "margin-right") {
        style.margin_right = def.margin_right;
    } else if (prop == "text-indent") {
        style.text_indent = def.text_indent;
    }
}

void apply_hint_levels(style_t &style, const style_t &def, const std::array<CssProps, 2> &levels)
{
    attr_t unused;

    // Relative font sizes are measured against the style's configured
    // size; start from that so that reapplying the hints (which happens
    // every time one of them changes) doesn't compound them.
    style.size = def.size;
    style.family_id = def.family_id;
    style.font.monospace = def.font.monospace;

    gli_css_apply_props(unused, &style, levels[CSS_Span], true, false);
    gli_css_apply_props(unused, &style, levels[CSS_Paragraph], true, true);
}

void apply_hints_to_style(glui32 wintype, glui32 styl)
{
    apply_hint_levels(global_styles(wintype)[styl], default_styles(wintype)[styl],
            hint_table(wintype)[styl]);
}

void hint_set(glui32 wintype, glui32 styl, glui32 par_or_span, const std::string &prop, const std::string &val)
{
    if (wintype == wintype_AllTypes) {
        hint_set(wintype_TextGrid, styl, par_or_span, prop, val);
        hint_set(wintype_TextBuffer, styl, par_or_span, prop, val);
        return;
    }

    if (!valid_wintype(wintype) || styl >= style_NUMSTYLES) {
        return;
    }

    hint_table(wintype)[styl][par_or_span == CSS_Paragraph ? CSS_Paragraph : CSS_Span][prop] = val;
    gli_css_have_hints = true;

    apply_hints_to_style(wintype, styl);
}

void hint_clear(glui32 wintype, glui32 styl, glui32 par_or_span, const std::string &prop)
{
    if (wintype == wintype_AllTypes) {
        hint_clear(wintype_TextGrid, styl, par_or_span, prop);
        hint_clear(wintype_TextBuffer, styl, par_or_span, prop);
        return;
    }

    if (!valid_wintype(wintype) || styl >= style_NUMSTYLES) {
        return;
    }

    hint_table(wintype)[styl][par_or_span == CSS_Paragraph ? CSS_Paragraph : CSS_Span].erase(prop);

    reset_style_property(global_styles(wintype)[styl], default_styles(wintype)[styl], prop);
    apply_hints_to_style(wintype, styl);
}

void hint_clear_all(glui32 wintype, glui32 styl)
{
    if (wintype == wintype_AllTypes) {
        hint_clear_all(wintype_TextGrid, styl);
        hint_clear_all(wintype_TextBuffer, styl);
        return;
    }

    if (!valid_wintype(wintype) || styl >= style_NUMSTYLES) {
        return;
    }

    auto &table = hint_table(wintype)[styl];
    style_t &style = global_styles(wintype)[styl];
    const style_t &def = default_styles(wintype)[styl];

    for (const auto &level : table) {
        for (const auto &[prop, val] : level) {
            reset_style_property(style, def, prop);
        }
    }

    for (auto &level : table) {
        level.clear();
    }
}

std::string glk_string(const char *str, glui32 len)
{
    if (str == nullptr || len == 0) {
        return "";
    }

    return {str, str + len};
}

bool attr_has_css(const attr_t &attr)
{
    return attr.bold.has_value() ||
           attr.italic.has_value() ||
           attr.monospace.has_value() ||
           attr.underline.has_value() ||
           attr.size.has_value() ||
           attr.justification.has_value() ||
           attr.family_id.has_value() ||
           attr.margin_left.has_value() ||
           attr.margin_right.has_value() ||
           attr.text_indent.has_value() ||
           attr.para_bgcolor.has_value() ||
           attr.css_paint ||
           attr.fg_transparent;
}

const Styles &window_styles(window_t *win)
{
    return win->type == wintype_TextGrid ? win->wingrid()->styles
                                         : win->winbuffer()->styles;
}

// Inline CSS declarations affect text about to be printed in open windows.
// Style-level hints only affect windows opened after they are set.
void refresh_all_windows()
{
    for (winid_t win = glk_window_iterate(nullptr, nullptr); win != nullptr; win = glk_window_iterate(win, nullptr)) {
        gli_css_refresh_window_attr(win);
    }
}

}

bool gli_css_active()
{
    return gli_css_have_hints;
}

void gli_css_apply_props(attr_t &attr, style_t *style, const CssProps &props,
        bool is_style_level, bool is_paragraph, double base_size)
{
    if (props.empty()) {
        return;
    }

    style_t *target = is_style_level ? style : nullptr;
    if (is_style_level && target == nullptr) {
        return;
    }

    // font-family is resolved up front, since switching between the
    // proportional and monospace font changes the size that relative
    // font sizes are measured against.
    auto family = props.find("font-family");
    auto mono_prop = props.find("monospace");
    std::optional<bool> monospace;
    std::optional<std::uint16_t> family_id;
    if (family != props.end()) {
        bool mono = false;
        family_id = resolve_font_family(family->second, mono);
        if (family_id.has_value()) {
            monospace = mono;
        }
    }
    if (mono_prop != props.end()) {
        auto val = garglk::downcase(trim(mono_prop->second));
        monospace = parse_bool(val) || val == "monospace";
    }
    if (family_id.has_value()) {
        if (target != nullptr) {
            target->family_id = *family_id;
        } else {
            attr.family_id = *family_id;
        }
    }
    if (monospace.has_value()) {
        if (target != nullptr) {
            target->font.monospace = *monospace;
        } else {
            attr.monospace = *monospace;
        }
    }

    if (!monospace.has_value()) {
        if (target != nullptr) {
            monospace = target->font.monospace;
        } else if (attr.monospace.has_value()) {
            monospace = *attr.monospace;
        } else if (style != nullptr) {
            monospace = style->font.monospace;
        } else {
            monospace = false;
        }
    }

    if (base_size <= 0) {
        if (style != nullptr && style->size.has_value()) {
            base_size = *style->size;
        } else {
            base_size = *monospace ? gli_conf_monosize : gli_conf_propsize;
        }
    }

    for (const auto &[prop, raw] : props) {
        auto val = garglk::downcase(trim(raw));

        if (prop == "color") {
            auto color = parse_color(raw);
            if (color.valid) {
                if (target != nullptr) {
                    // Style-level transparent has no alpha channel; leave fg alone
                    // so the style keeps its previous foreground.
                    if (!color.transparent) {
                        target->fg = color.color;
                    }
                } else if (color.transparent) {
                    attr.fgcolor.reset();
                    attr.fg_transparent = true;
                    attr.css_paint = true;
                } else {
                    attr.fgcolor = color.color;
                    attr.fg_transparent = false;
                    attr.css_paint = true;
                }
            }
        } else if (prop == "background-color") {
            auto color = parse_color(raw);
            if (color.valid && !color.transparent) {
                if (is_paragraph) {
                    if (target != nullptr) {
                        target->para_bg = color.color;
                    } else {
                        attr.para_bgcolor = color.color;
                    }
                } else if (target != nullptr) {
                    target->bg = color.color;
                } else {
                    attr.bgcolor = color.color;
                    attr.css_paint = true;
                }
            } else if (color.transparent) {
                if (is_paragraph) {
                    if (target != nullptr) {
                        target->para_bg.reset();
                    } else {
                        attr.para_bgcolor.reset();
                    }
                } else if (target == nullptr) {
                    attr.bgcolor.reset();
                    attr.css_paint = true;
                }
            }
        } else if (prop == "reverse" || prop == "--glk-reverse" ||
                prop == "-iftf-reverse-video") {
            bool reverse;
            if (prop == "-iftf-reverse-video") {
                reverse = (val == "reverse" || val == "1" || val == "true" || val == "yes");
            } else {
                reverse = parse_bool(val);
            }
            if (target != nullptr) {
                target->reverse = reverse;
            } else {
                attr.reverse = reverse;
            }
        } else if (prop == "font-weight") {
            std::optional<bool> bold;
            if (val == "bold" || val == "bolder" || val == "700") {
                bold = true;
            } else if (val == "normal" || val == "lighter" || val == "400") {
                bold = false;
            }
            if (bold.has_value()) {
                if (target != nullptr) {
                    target->font.bold = *bold;
                } else {
                    attr.bold = bold;
                }
            }
        } else if (prop == "font-style") {
            std::optional<bool> italic;
            if (val == "italic" || val == "oblique") {
                italic = true;
            } else if (val == "normal") {
                italic = false;
            }
            if (italic.has_value()) {
                if (target != nullptr) {
                    target->font.italic = *italic;
                } else {
                    attr.italic = italic;
                }
            }
        } else if (prop == "text-decoration" || prop == "text-decoration-line") {
            bool underline = val.find("underline") != std::string::npos;
            if (underline || val == "none") {
                if (target != nullptr) {
                    target->underline = underline;
                } else {
                    attr.underline = underline;
                }
            }
        } else if (prop == "font-size") {
            double size;
            if (val == "small" || val == "smaller") {
                size = base_size * 0.83;
            } else if (val == "medium") {
                size = base_size;
            } else if (val == "large" || val == "larger") {
                size = base_size * 1.2;
            } else {
                size = parse_length(raw, base_size);
            }
            if (size > 0) {
                if (target != nullptr) {
                    target->size = size;
                } else {
                    attr.size = size;
                }
            }
        } else if (prop == "text-align") {
            auto just = parse_justification(val);
            if (just.has_value()) {
                if (target != nullptr) {
                    target->justification = *just;
                } else {
                    attr.justification = just;
                }
            }
        } else if (prop == "margin-left") {
            auto length = parse_length(raw, base_size);
            if (target != nullptr) {
                target->margin_left = length;
            } else {
                attr.margin_left = static_cast<float>(length);
            }
        } else if (prop == "margin-right") {
            auto length = parse_length(raw, base_size);
            if (target != nullptr) {
                target->margin_right = length;
            } else {
                attr.margin_right = static_cast<float>(length);
            }
        } else if (prop == "text-indent") {
            auto length = parse_length(raw, base_size);
            if (target != nullptr) {
                target->text_indent = length;
            } else {
                attr.text_indent = static_cast<float>(length);
            }
        }
    }
}

void gli_css_apply_hints_to_styles(Styles &styles, glui32 wintype)
{
    if (!valid_wintype(wintype) || !gli_css_have_hints) {
        return;
    }

    const auto &table = hint_table(wintype);
    const Styles &def = default_styles(wintype);

    for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
        apply_hint_levels(styles[styl], def[styl], table[styl]);
    }
}

// Style-level CSS hints are snapshotted into each window's styles at
// creation (like stylehints). Only inline declarations affect the current
// attributes of an already-open window.
void gli_css_refresh_window_attr(window_t *win)
{
    if (win == nullptr || !valid_wintype(win->type)) {
        return;
    }

    attr_t &attr = win->attr;

    if (!gli_css_have_hints && win->css_inline.empty() && win->css_inline_para.empty() &&
            !win->css_fgcolor.has_value() && !win->css_bgcolor.has_value() &&
            !win->css_reverse && !attr_has_css(attr)) {
        return;
    }

    // Undo the colors set by the previous refresh, but leave alone any
    // set since then by garglk_set_zcolors().
    if (win->css_fgcolor.has_value() && attr.fgcolor == win->css_fgcolor) {
        attr.fgcolor.reset();
    }
    if (win->css_bgcolor.has_value() && attr.bgcolor == win->css_bgcolor) {
        attr.bgcolor.reset();
    }
    if (win->css_reverse && attr.reverse) {
        attr.reverse = false;
    }
    win->css_fgcolor.reset();
    win->css_bgcolor.reset();
    win->css_reverse = false;

    attr.clear_css();

    if (win->css_inline.empty() && win->css_inline_para.empty()) {
        return;
    }

    glui32 styl = attr.style < style_NUMSTYLES ? attr.style : 0;

    auto old_fgcolor = attr.fgcolor;
    auto old_bgcolor = attr.bgcolor;
    bool old_reverse = attr.reverse;

    style_t style = window_styles(win)[styl];
    // Span first, then paragraph (paragraph props such as text-align win).
    gli_css_apply_props(attr, &style, win->css_inline, false, false);
    gli_css_apply_props(attr, &style, win->css_inline_para, false, true);

    if (attr.fgcolor != old_fgcolor) {
        win->css_fgcolor = attr.fgcolor;
    }
    if (attr.bgcolor != old_bgcolor) {
        win->css_bgcolor = attr.bgcolor;
    }
    if (attr.reverse != old_reverse) {
        win->css_reverse = attr.reverse;
    }
}

namespace {

CssProps &window_hint_table(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_css_grid_window_hints
                                       : gli_css_buffer_window_hints;
}

void window_hint_set(glui32 wintype, const std::string &prop, const std::string &val, bool clear)
{
    if (wintype == wintype_AllTypes) {
        window_hint_set(wintype_TextGrid, prop, val, clear);
        window_hint_set(wintype_TextBuffer, prop, val, clear);
        return;
    }
    if (!valid_wintype(wintype)) {
        return;
    }

    auto &store = window_hint_table(wintype);
    if (clear) {
        store.erase(prop);
    } else {
        store[prop] = val;
        gli_css_have_hints = true;
    }
}

void clear_stylehints_for_style(glui32 wintype, glui32 styl)
{
    for (glui32 hint = 0; hint < stylehint_NUMHINTS; hint++) {
        glk_stylehint_clear(wintype, styl, hint);
    }
}

void clear_level_hints(glui32 wintype, glui32 styl, glui32 span_or_par)
{
    if (wintype == wintype_AllTypes) {
        clear_level_hints(wintype_TextGrid, styl, span_or_par);
        clear_level_hints(wintype_TextBuffer, styl, span_or_par);
        return;
    }
    if (!valid_wintype(wintype) || styl >= style_NUMSTYLES) {
        return;
    }

    auto &level = hint_table(wintype)[styl][span_or_par == CSS_Paragraph ? CSS_Paragraph : CSS_Span];
    style_t &style = global_styles(wintype)[styl];
    const style_t &def = default_styles(wintype)[styl];
    for (const auto &[prop, val] : level) {
        reset_style_property(style, def, prop);
    }
    level.clear();
    apply_hints_to_style(wintype, styl);
}

// Map a Style_* / Style_*_para selector, or empty (window-level). Returns false
// for unrecognized selectors.
bool parse_selector(const std::string &sel_in, glui32 &styl, bool &is_para, bool &is_window)
{
    styl = 0;
    is_para = false;
    is_window = false;

    auto sel = trim(sel_in);
    if (sel.empty()) {
        is_window = true;
        return true;
    }

    if (sel.front() == '.') {
        sel = sel.substr(1);
    }

    static const std::map<std::string, glui32> style_map = {
        {"Style_normal", style_Normal},
        {"Style_emphasized", style_Emphasized},
        {"Style_preformatted", style_Preformatted},
        {"Style_header", style_Header},
        {"Style_subheader", style_Subheader},
        {"Style_alert", style_Alert},
        {"Style_note", style_Note},
        {"Style_blockquote", style_BlockQuote},
        {"Style_input", style_Input},
        {"Style_user1", style_User1},
        {"Style_user2", style_User2},
    };

    if (sel.size() > 5 && sel.compare(sel.size() - 5, 5, "_para") == 0) {
        is_para = true;
        sel = sel.substr(0, sel.size() - 5);
    }

    auto it = style_map.find(sel);
    if (it == style_map.end()) {
        return false;
    }
    styl = it->second;
    return true;
}

window_t *css_current_window()
{
    if (!gli_conf_stylehint) {
        return nullptr;
    }

    stream_t *str = glk_stream_get_current();
    if (str == nullptr || !str->writable || str->type != strtype_Window || str->win == nullptr) {
        return nullptr;
    }

    return valid_wintype(str->win->type) ? str->win : nullptr;
}

} // namespace

void gli_css_apply_window_hints(window_t *win)
{
    if (win == nullptr || !valid_wintype(win->type)) {
        return;
    }

    const auto &hints = (win->type == wintype_TextGrid)
        ? gli_css_grid_window_hints : gli_css_buffer_window_hints;
    auto it = hints.find("background-color");
    if (it == hints.end()) {
        return;
    }
    auto color = parse_color(it->second);
    if (color.valid && !color.transparent) {
        win->bgcolor = color.color;
    }
}

//
// The Glk API itself
//

void glk_css_hint_set(glui32 wintype, glui32 styl, glui32 span_or_par,
    const char *prop, glui32 proplen, const char *val, glui32 vallen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    hint_set(wintype, styl, span_or_par, property, trim(glk_string(val, vallen)));
    refresh_all_windows();
}

void glk_css_hint_set_num(glui32 wintype, glui32 styl, glui32 span_or_par,
    const char *prop, glui32 proplen, glsi32 val)
{
    auto number = std::to_string(val);
    glk_css_hint_set(wintype, styl, span_or_par, prop, proplen,
            number.c_str(), number.size());
}

void glk_css_hint_clear(glui32 wintype, glui32 styl, glui32 span_or_par,
    const char *prop, glui32 proplen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    hint_clear(wintype, styl, span_or_par, property);
    refresh_all_windows();
}

void glk_css_hint_selector_set(glui32 wintype, const char *sel, glui32 sellen,
    const char *prop, glui32 proplen, const char *val, glui32 vallen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    glui32 styl = 0;
    bool is_para = false;
    bool is_window = false;
    if (!parse_selector(glk_string(sel, sellen), styl, is_para, is_window)) {
        return;
    }

    auto value = trim(glk_string(val, vallen));
    if (is_window) {
        window_hint_set(wintype, property, value, false);
    } else {
        hint_set(wintype, styl, is_para ? CSS_Paragraph : CSS_Span, property, value);
        refresh_all_windows();
    }
}

void glk_css_hint_selector_set_num(glui32 wintype, const char *sel, glui32 sellen,
    const char *prop, glui32 proplen, glsi32 val)
{
    auto number = std::to_string(val);
    glk_css_hint_selector_set(wintype, sel, sellen, prop, proplen,
            number.c_str(), number.size());
}

void glk_css_hint_selector_clear(glui32 wintype, const char *sel, glui32 sellen,
    const char *prop, glui32 proplen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    glui32 styl = 0;
    bool is_para = false;
    bool is_window = false;
    if (!parse_selector(glk_string(sel, sellen), styl, is_para, is_window)) {
        return;
    }

    if (is_window) {
        window_hint_set(wintype, property, "", true);
    } else {
        hint_clear(wintype, styl, is_para ? CSS_Paragraph : CSS_Span, property);
        refresh_all_windows();
    }
}

void glk_css_hint_clear_all_by_style(glui32 wintype, glui32 styl)
{
    if (!gli_conf_stylehint) {
        return;
    }

    hint_clear_all(wintype, styl);
    clear_stylehints_for_style(wintype, styl);
    refresh_all_windows();
}

void glk_css_hint_clear_all_by_selector(glui32 wintype, const char *sel, glui32 sellen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    glui32 styl = 0;
    bool is_para = false;
    bool is_window = false;
    if (!parse_selector(glk_string(sel, sellen), styl, is_para, is_window)) {
        return;
    }

    if (is_window) {
        if (wintype == wintype_AllTypes) {
            gli_css_buffer_window_hints.clear();
            gli_css_grid_window_hints.clear();
        } else if (valid_wintype(wintype)) {
            if (wintype == wintype_TextGrid) {
                gli_css_grid_window_hints.clear();
            } else {
                gli_css_buffer_window_hints.clear();
            }
        }
        return;
    }

    clear_level_hints(wintype, styl, is_para ? CSS_Paragraph : CSS_Span);
    clear_stylehints_for_style(wintype, styl);
    refresh_all_windows();
}

void glk_css_hint_clear_all_by_window(glui32 wintype)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto clear_one = [](glui32 wt) {
        for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
            hint_clear_all(wt, styl);
            clear_stylehints_for_style(wt, styl);
        }
        if (wt == wintype_TextGrid) {
            gli_css_grid_window_hints.clear();
        } else {
            gli_css_buffer_window_hints.clear();
        }
    };

    if (wintype == wintype_AllTypes) {
        clear_one(wintype_TextGrid);
        clear_one(wintype_TextBuffer);
    } else if (valid_wintype(wintype)) {
        clear_one(wintype);
    }
    refresh_all_windows();
}

void glk_css_inline_set(glui32 span_or_par, const char *prop, glui32 proplen,
    const char *val, glui32 vallen)
{
    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    window_t *win = css_current_window();
    if (win == nullptr) {
        return;
    }

    auto &store = (span_or_par == CSS_Paragraph) ? win->css_inline_para : win->css_inline;
    store[property] = trim(glk_string(val, vallen));
    gli_css_refresh_window_attr(win);
}

void glk_css_inline_set_num(glui32 span_or_par, const char *prop, glui32 proplen, glsi32 val)
{
    auto number = std::to_string(val);
    glk_css_inline_set(span_or_par, prop, proplen, number.c_str(), number.size());
}

void glk_css_inline_clear(glui32 span_or_par, const char *prop, glui32 proplen)
{
    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    window_t *win = css_current_window();
    if (win == nullptr) {
        return;
    }

    auto &store = (span_or_par == CSS_Paragraph) ? win->css_inline_para : win->css_inline;
    store.erase(property);
    gli_css_refresh_window_attr(win);
}

void glk_css_hint_clear_all_inline()
{
    window_t *win = css_current_window();
    if (win == nullptr) {
        return;
    }

    win->css_inline.clear();
    win->css_inline_para.clear();
    gli_css_refresh_window_attr(win);

    // Also clear Gargoyle text-formatting extensions (reverse / zcolors).
    garglk_set_reversevideo(0);
    garglk_set_zcolors(zcolor_Default, zcolor_Default);
}

const CssFontFamily *gli_css_get_family(std::uint16_t id)
{
    if (id >= gli_css_families.size()) {
        return nullptr;
    }

    return &gli_css_families[id];
}
