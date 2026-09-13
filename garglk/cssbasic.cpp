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
// attach a small set of CSS declarations either as hints (before a
// window opens, via glk_css_hint_set) or inline (to text about to be
// printed, via glk_css_inline_set). Targets include span, paragraph,
// hyperlink, image, input, and window. glk_css_supports answers local
// feature probes matching Spatterlight.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "glk.h"
#include "garglk.h"

namespace {

// Hints are stored per window type, per style, and per level (span or
// paragraph). Hyperlink hints are per style; window/input/image hints
// ignore style.
using HintTable = std::array<std::array<CssProps, 2>, style_NUMSTYLES>;
using HyperlinkHintTable = std::array<CssProps, style_NUMSTYLES>;

HintTable gli_css_buffer_hints;
HintTable gli_css_grid_hints;
HyperlinkHintTable gli_css_buffer_hyperlink_hints;
HyperlinkHintTable gli_css_grid_hyperlink_hints;
CssProps gli_css_buffer_window_hints;
CssProps gli_css_grid_window_hints;
CssProps gli_css_buffer_input_hints;
CssProps gli_css_grid_input_hints;
CssProps gli_css_buffer_image_hints;
CssProps gli_css_grid_image_hints;
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

    // light-dark(<color>, <color>) — pick by system appearance (windark()).
    static const std::string light_dark_prefix = "light-dark(";
    if (v.size() > light_dark_prefix.size() + 1 &&
            v.compare(0, light_dark_prefix.size(), light_dark_prefix) == 0 &&
            v.back() == ')') {
        auto inner = v.substr(light_dark_prefix.size(),
                v.size() - light_dark_prefix.size() - 1);
        int depth = 0;
        std::string::size_type comma = std::string::npos;
        for (std::string::size_type i = 0; i < inner.size(); i++) {
            if (inner[i] == '(') {
                depth++;
            } else if (inner[i] == ')') {
                depth--;
            } else if (inner[i] == ',' && depth == 0) {
                if (comma != std::string::npos) {
                    return result; // more than two args
                }
                comma = i;
            }
        }
        if (comma == std::string::npos || depth != 0) {
            return result;
        }
        auto light = parse_color(inner.substr(0, comma));
        auto dark = parse_color(inner.substr(comma + 1));
        if (!light.valid || !dark.valid) {
            return result;
        }
        return windark() ? dark : light;
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

// Decide whether a font-family list asks for the monospace font. Only
// the first family that Gargoyle can honor is considered; since
// Gargoyle has exactly two font families, anything that isn't
// recognizably monospace maps to the proportional font.
std::optional<bool> parse_font_family(const std::string &value)
{
    for (const auto &family : split(value, ',')) {
        auto name = garglk::downcase(unquote(trim(family)));
        if (name.empty()) {
            continue;
        }

        if (name == "monospace" || name == "mono" || name == "terminal" ||
                name == "consolas" || name == "monaco" || name == "menlo" ||
                name.find("courier") != std::string::npos ||
                name.find("ocr") != std::string::npos) {
            return true;
        }

        return false;
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

HyperlinkHintTable &hyperlink_hint_table(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_css_grid_hyperlink_hints
                                       : gli_css_buffer_hyperlink_hints;
}

CssProps &window_hint_table(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_css_grid_window_hints
                                       : gli_css_buffer_window_hints;
}

CssProps &input_hint_table(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_css_grid_input_hints
                                       : gli_css_buffer_input_hints;
}

CssProps &image_hint_table(glui32 wintype)
{
    return wintype == wintype_TextGrid ? gli_css_grid_image_hints
                                       : gli_css_buffer_image_hints;
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
        style.fg_explicit = false;
    } else if (prop == "background-color") {
        // Span and paragraph levels both use this property name; clear both
        // so reapplying the remaining level can restore the right one.
        style.bg = def.bg;
        style.bg_explicit = false;
        style.para_bg = def.para_bg;
    } else if (prop == "-iftf-reverse-video") {
        style.reverse = def.reverse;
        style.reverse_explicit = false;
    } else if (prop == "font-weight") {
        style.font.bold = def.font.bold;
    } else if (prop == "font-style") {
        style.font.italic = def.font.italic;
    } else if (prop == "font-family" || prop == "monospace") {
        style.font.monospace = def.font.monospace;
    } else if (prop == "font-size") {
        style.size = def.size;
    } else if (prop == "text-decoration" || prop == "text-decoration-line") {
        style.underline = def.underline;
        style.underline_explicit = false;
    } else if (prop == "text-align") {
        style.justification = def.justification;
        style.just_explicit = false;
    } else if (prop == "margin-left") {
        style.margin_left = def.margin_left;
    } else if (prop == "margin-right") {
        style.margin_right = def.margin_right;
    } else if (prop == "text-indent") {
        style.text_indent = def.text_indent;
        style.indent_explicit = false;
    } else if (prop == "border-style") {
        style.span_border = def.span_border;
        style.para_border = def.para_border;
    }
}

void apply_hint_levels(style_t &style, const style_t &def, const std::array<CssProps, 2> &levels)
{
    attr_t unused;

    // Relative font sizes are measured against the style's configured
    // size; start from that so that reapplying the hints (which happens
    // every time one of them changes) doesn't compound them.
    style.size = def.size;

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

    hyperlink_hint_table(wintype)[styl].clear();
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
           attr.margin_left.has_value() ||
           attr.margin_right.has_value() ||
           attr.text_indent.has_value() ||
           attr.para_bgcolor.has_value() ||
           attr.span_border.has_value() ||
           attr.para_border.has_value() ||
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
    if (family != props.end()) {
        monospace = parse_font_family(family->second);
    }
    if (mono_prop != props.end()) {
        auto val = garglk::downcase(trim(mono_prop->second));
        monospace = parse_bool(val) || val == "monospace";
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
                        target->fg_explicit = true;
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
                    // Span background only — never touches CSS_Window chrome.
                    target->bg = color.color;
                    target->bg_explicit = true;
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
                } else if (target != nullptr) {
                    // Clearing an explicit span bg returns to inheritance.
                    target->bg_explicit = false;
                } else if (target == nullptr) {
                    attr.bgcolor.reset();
                    attr.css_paint = true;
                }
            }
        } else if (prop == "-iftf-reverse-video") {
            if (val == "reverse" || val == "none") {
                bool reverse = (val == "reverse");
                if (target != nullptr) {
                    target->reverse = reverse;
                    target->reverse_explicit = true;
                } else {
                    attr.reverse = reverse;
                }
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
                    target->underline_explicit = true;
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
                    target->just_explicit = true;
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
                target->indent_explicit = true;
            } else {
                attr.text_indent = static_cast<float>(length);
            }
        } else if (prop == "border-style") {
            if (val == "solid" || val == "none") {
                bool solid = (val == "solid");
                if (is_paragraph) {
                    if (target != nullptr) {
                        target->para_border = solid;
                    } else {
                        attr.para_border = solid;
                    }
                } else if (target != nullptr) {
                    target->span_border = solid;
                } else {
                    attr.span_border = solid;
                }
            }
        }
    }
}

// Apply per-style Span/Paragraph CSS hints onto a window's style snapshot.
void gli_css_apply_hints_to_styles(Styles &styles, glui32 wintype)
{
    if (!valid_wintype(wintype)) {
        return;
    }

    if (gli_css_have_hints) {
        const auto &table = hint_table(wintype);
        const Styles &def = default_styles(wintype);

        for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
            apply_hint_levels(styles[styl], def[styl], table[styl]);
        }
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
            win->css_inline_hyperlink.empty() &&
            !win->css_fgcolor.has_value() && !win->css_bgcolor.has_value() &&
            !win->css_reverse && !attr_has_css(attr) && attr.hyperlink() == 0) {
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

    bool hyper_active = attr.hyperlink() != 0;
    glui32 styl = attr.style < style_NUMSTYLES ? attr.style : 0;
    bool have_hyper = hyper_active &&
            (!win->css_hyperlink_hints[styl].empty() || !win->css_inline_hyperlink.empty());

    if (win->css_inline.empty() && win->css_inline_para.empty() && !have_hyper) {
        return;
    }

    auto old_fgcolor = attr.fgcolor;
    auto old_bgcolor = attr.bgcolor;
    bool old_reverse = attr.reverse;

    style_t style = window_styles(win)[styl];
    // Span first, then paragraph (paragraph props such as text-align win).
    gli_css_apply_props(attr, &style, win->css_inline, false, false);
    gli_css_apply_props(attr, &style, win->css_inline_para, false, true);
    if (have_hyper) {
        gli_css_apply_props(attr, &style, win->css_hyperlink_hints[styl], false, false);
        gli_css_apply_props(attr, &style, win->css_inline_hyperlink, false, false);
    }

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

void props_hint_set(CssProps &store, const std::string &prop, const std::string &val, bool clear)
{
    if (clear) {
        store.erase(prop);
    } else {
        store[prop] = val;
        gli_css_have_hints = true;
    }
}

void target_hint_set(glui32 wintype, glui32 csstarget, glui32 style,
        const std::string &prop, const std::string &val, bool clear)
{
    if (wintype == wintype_AllTypes) {
        target_hint_set(wintype_TextGrid, csstarget, style, prop, val, clear);
        target_hint_set(wintype_TextBuffer, csstarget, style, prop, val, clear);
        return;
    }
    if (!valid_wintype(wintype)) {
        return;
    }

    switch (csstarget) {
    case CSS_Window:
        props_hint_set(window_hint_table(wintype), prop, val, clear);
        return;
    case CSS_Input:
        props_hint_set(input_hint_table(wintype), prop, val, clear);
        return;
    case CSS_Image:
        props_hint_set(image_hint_table(wintype), prop, val, clear);
        return;
    case CSS_Hyperlink:
        if (style >= style_NUMSTYLES) {
            return;
        }
        props_hint_set(hyperlink_hint_table(wintype)[style], prop, val, clear);
        return;
    case CSS_Paragraph:
        if (clear) {
            hint_clear(wintype, style, CSS_Paragraph, prop);
        } else {
            hint_set(wintype, style, CSS_Paragraph, prop, val);
        }
        return;
    case CSS_Span:
    default:
        if (clear) {
            hint_clear(wintype, style, CSS_Span, prop);
        } else {
            hint_set(wintype, style, CSS_Span, prop, val);
        }
        return;
    }
}

void clear_stylehints_for_style(glui32 wintype, glui32 styl)
{
    for (glui32 hint = 0; hint < stylehint_NUMHINTS; hint++) {
        glk_stylehint_clear(wintype, styl, hint);
    }
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

CssProps *inline_store_for_target(window_t *win, glui32 csstarget)
{
    switch (csstarget) {
    case CSS_Paragraph:
        return &win->css_inline_para;
    case CSS_Hyperlink:
        return &win->css_inline_hyperlink;
    case CSS_Input:
        return &win->css_inline_input;
    case CSS_Image:
        return &win->css_inline_image;
    case CSS_Span:
        return &win->css_inline;
    default:
        return nullptr;
    }
}

bool border_style_is_solid(const std::string &value)
{
    return garglk::downcase(trim(value)) == "solid";
}

bool effective_solid_border(const CssProps &inline_props, const CssProps &hint_props)
{
    auto it = inline_props.find("border-style");
    if (it == inline_props.end()) {
        it = hint_props.find("border-style");
        if (it == hint_props.end()) {
            return false;
        }
    }
    return border_style_is_solid(it->second);
}

} // namespace

void gli_css_apply_window_hints(window_t *win)
{
    if (win == nullptr || !valid_wintype(win->type)) {
        return;
    }

    const auto &hints = window_hint_table(win->type);
    Styles &styles = (win->type == wintype_TextGrid)
        ? win->wingrid()->styles : win->winbuffer()->styles;
    const auto &style_hints = hint_table(win->type);

    auto style_has_prop = [&](glui32 styl, const std::string &prop) {
        return style_hints[styl][CSS_Span].count(prop) != 0 ||
               style_hints[styl][CSS_Paragraph].count(prop) != 0;
    };

    // background-color is page chrome only (not inherited). Styles without an
    // explicit span background stay transparent and show this color through.
    auto bg = hints.find("background-color");
    if (bg != hints.end()) {
        auto color = parse_color(bg->second);
        if (color.valid && !color.transparent) {
            win->bgcolor = color.color;
        }
    }

    // color is inherited by every style that did not set its own color.
    auto fg = hints.find("color");
    if (fg != hints.end()) {
        auto color = parse_color(fg->second);
        if (color.valid && !color.transparent) {
            win->fgcolor = color.color;
            for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
                if (!styles[styl].fg_explicit && !style_has_prop(styl, "color")) {
                    styles[styl].fg = color.color;
                }
            }
        }
    }

    // -iftf-reverse-video expands to a color swap: inherited color becomes
    // the current background, and chrome background-color becomes the current
    // foreground. The property itself is not inherited.
    auto reverse_it = hints.find("-iftf-reverse-video");
    if (reverse_it != hints.end()) {
        auto val = garglk::downcase(trim(reverse_it->second));
        if (val == "reverse") {
            Color cur_bg = win->bgcolor;
            Color cur_fg = styles[style_Normal].fg;
            win->bgcolor = cur_fg;
            win->fgcolor = cur_bg;
            for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
                if (!styles[styl].fg_explicit && !style_has_prop(styl, "color")) {
                    styles[styl].fg = cur_bg;
                }
            }
        }
    }

    // Remaining inherited window properties apply to each style that did not
    // set the property itself. Non-inherited props are skipped above/here.
    win->css_window_border = false;
    auto border_it = hints.find("border-style");
    if (border_it != hints.end()) {
        win->css_window_border = border_style_is_solid(border_it->second);
    }

    for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
        CssProps for_style;
        style_t &style = styles[styl];
        for (const auto &[prop, val] : hints) {
            if (prop == "background-color" || prop == "color" ||
                    prop == "margin-left" || prop == "margin-right" ||
                    prop == "border-style" || prop == "border" ||
                    prop == "-iftf-reverse-video") {
                continue;
            }
            if (style_has_prop(styl, prop)) {
                continue;
            }
            if ((prop == "text-decoration" || prop == "text-decoration-line") &&
                    style.underline_explicit) {
                continue;
            }
            if (prop == "text-align" && style.just_explicit) {
                continue;
            }
            if (prop == "text-indent" && style.indent_explicit) {
                continue;
            }
            for_style[prop] = val;
        }
        if (!for_style.empty()) {
            attr_t unused;
            gli_css_apply_props(unused, &style, for_style, true, false);
        }
    }
}

void gli_css_snapshot_targets(window_t *win)
{
    if (win == nullptr || !valid_wintype(win->type)) {
        return;
    }

    const auto &hyper = hyperlink_hint_table(win->type);
    for (glui32 styl = 0; styl < style_NUMSTYLES; styl++) {
        win->css_hyperlink_hints[styl] = hyper[styl];
    }
    win->css_input_hints = input_hint_table(win->type);
    win->css_image_hints = image_hint_table(win->type);
}

bool gli_css_input_wants_border(const window_t *win)
{
    if (win == nullptr || !gli_conf_stylehint) {
        return false;
    }
    return effective_solid_border(win->css_inline_input, win->css_input_hints);
}

bool gli_css_image_wants_border(const window_t *win)
{
    if (win == nullptr || !gli_conf_stylehint) {
        return false;
    }
    return effective_solid_border(win->css_inline_image, win->css_image_hints);
}

bool gli_css_window_wants_border(const window_t *win)
{
    if (win == nullptr || !gli_conf_stylehint) {
        return false;
    }
    return win->css_window_border;
}

//
// The Glk API itself
//

void glk_css_hint_set(glui32 wintype, glui32 csstarget, glui32 style,
    const char *prop, glui32 proplen, const char *val, glui32 vallen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    target_hint_set(wintype, csstarget, style, property, trim(glk_string(val, vallen)), false);
    if (csstarget == CSS_Span || csstarget == CSS_Paragraph) {
        refresh_all_windows();
    }
}

void glk_css_hint_set_num(glui32 wintype, glui32 csstarget, glui32 style,
    const char *prop, glui32 proplen, glsi32 val)
{
    auto number = std::to_string(val);
    glk_css_hint_set(wintype, csstarget, style, prop, proplen,
            number.c_str(), number.size());
}

void glk_css_hint_clear(glui32 wintype, glui32 csstarget, glui32 style,
    const char *prop, glui32 proplen)
{
    if (!gli_conf_stylehint) {
        return;
    }

    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    target_hint_set(wintype, csstarget, style, property, "", true);
    if (csstarget == CSS_Span || csstarget == CSS_Paragraph) {
        refresh_all_windows();
    }
}

void glk_css_hint_clear_all_by_style(glui32 wintype, glui32 style)
{
    if (!gli_conf_stylehint) {
        return;
    }

    hint_clear_all(wintype, style);
    clear_stylehints_for_style(wintype, style);
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
        window_hint_table(wt).clear();
        input_hint_table(wt).clear();
        image_hint_table(wt).clear();
    };

    if (wintype == wintype_AllTypes) {
        clear_one(wintype_TextGrid);
        clear_one(wintype_TextBuffer);
    } else if (valid_wintype(wintype)) {
        clear_one(wintype);
    }
    refresh_all_windows();
}

void glk_css_inline_set(glui32 csstarget, const char *prop, glui32 proplen,
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

    CssProps *store = inline_store_for_target(win, csstarget);
    if (store == nullptr) {
        return;
    }

    (*store)[property] = trim(glk_string(val, vallen));
    if (csstarget == CSS_Span || csstarget == CSS_Paragraph || csstarget == CSS_Hyperlink) {
        gli_css_refresh_window_attr(win);
    }
}

void glk_css_inline_set_num(glui32 csstarget, const char *prop, glui32 proplen, glsi32 val)
{
    auto number = std::to_string(val);
    glk_css_inline_set(csstarget, prop, proplen, number.c_str(), number.size());
}

void glk_css_inline_clear(glui32 csstarget, const char *prop, glui32 proplen)
{
    auto property = garglk::downcase(trim(glk_string(prop, proplen)));
    if (property.empty()) {
        return;
    }

    window_t *win = css_current_window();
    if (win == nullptr) {
        return;
    }

    CssProps *store = inline_store_for_target(win, csstarget);
    if (store == nullptr) {
        return;
    }

    store->erase(property);
    if (csstarget == CSS_Span || csstarget == CSS_Paragraph || csstarget == CSS_Hyperlink) {
        gli_css_refresh_window_attr(win);
    }
}

void glk_css_hint_clear_all_inline()
{
    window_t *win = css_current_window();
    if (win == nullptr) {
        return;
    }

    win->css_inline.clear();
    win->css_inline_para.clear();
    win->css_inline_hyperlink.clear();
    win->css_inline_input.clear();
    win->css_inline_image.clear();
    gli_css_refresh_window_attr(win);

    // Also clear Gargoyle text-formatting extensions (reverse / zcolors).
    garglk_set_reversevideo(0);
    garglk_set_zcolors(zcolor_Default, zcolor_Default);
}

#ifdef GLK_MODULE_CSS_SUPPORTS

namespace {

// Hardcoded probe matching Spatterlight's cssbasic.c (+ CSS Basic profile).

bool css_known_property(const std::string &prop)
{
    return prop == "color"
        || prop == "background-color"
        || prop == "-iftf-reverse-video"
        || prop == "font-weight"
        || prop == "font-style"
        || prop == "font-size"
        || prop == "font-family"
        || prop == "text-decoration"
        || prop == "text-decoration-line"
        || prop == "text-align"
        || prop == "margin-left"
        || prop == "margin-right"
        || prop == "text-indent"
        || prop == "border-style";
}

bool css_is_length(const std::string &v, bool positive_only)
{
    if (v.empty()) {
        return false;
    }

    std::size_t i = 0;
    if (v[i] == '+' || v[i] == '-') {
        if (positive_only && v[i] == '-') {
            return false;
        }
        i++;
    }

    bool saw_digit = false;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
        saw_digit = true;
        i++;
    }
    if (i < v.size() && v[i] == '.') {
        i++;
        while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
            saw_digit = true;
            i++;
        }
    }
    if (!saw_digit) {
        return false;
    }
    if (i == v.size()) {
        return true; // unitless number (incl. 0)
    }
    auto unit = v.substr(i);
    return unit == "px" || unit == "pt" || unit == "em" || unit == "%";
}

bool css_contains_token(const std::string &hay, const std::string &needle)
{
    if (hay.empty() || needle.empty()) {
        return false;
    }

    std::size_t pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) {
        bool before_ok = (pos == 0) || std::isspace(static_cast<unsigned char>(hay[pos - 1]));
        auto after = pos + needle.size();
        bool after_ok = (after == hay.size()) ||
                std::isspace(static_cast<unsigned char>(hay[after]));
        if (before_ok && after_ok) {
            return true;
        }
        pos += needle.size();
    }
    return false;
}

bool css_value_supported(const std::string &prop, const std::string &val)
{
    if (prop == "color" || prop == "background-color") {
        return parse_color(val).valid;
    }

    if (prop == "-iftf-reverse-video") {
        return val == "reverse" || val == "none"
            || val == "1" || val == "0"
            || val == "true" || val == "false"
            || val == "yes" || val == "no";
    }

    if (prop == "font-weight") {
        return val == "normal" || val == "bold"
            || val == "400" || val == "700"
            || val == "bolder" || val == "lighter";
    }

    if (prop == "font-style") {
        return val == "normal" || val == "italic" || val == "oblique";
    }

    if (prop == "font-size") {
        if (val == "small" || val == "medium" || val == "large"
                || val == "larger" || val == "smaller") {
            return true;
        }
        return css_is_length(val, true);
    }

    if (prop == "font-family") {
        return !val.empty();
    }

    if (prop == "text-decoration" || prop == "text-decoration-line") {
        if (val == "none") {
            return true;
        }
        return css_contains_token(val, "underline");
    }

    if (prop == "text-align") {
        return val == "left" || val == "right"
            || val == "center" || val == "justify";
    }

    if (prop == "margin-left" || prop == "margin-right" || prop == "text-indent") {
        return css_is_length(val, false);
    }

    if (prop == "border-style") {
        return val == "solid" || val == "none";
    }

    return false;
}

} // namespace

glui32 glk_css_supports(const char *prop, glui32 proplen,
    const char *val, glui32 vallen)
{
    if (prop == nullptr || proplen == 0) {
        return 0;
    }

    auto nprop = garglk::downcase(trim(glk_string(prop, proplen)));
    if (nprop.empty() || !css_known_property(nprop)) {
        return 0;
    }

    if (vallen == 0 || val == nullptr) {
        // Empty value: "is this property known?"
        return 1;
    }

    // font-family: preserve original case/quotes; any non-empty list is ok.
    if (nprop == "font-family") {
        return trim(glk_string(val, vallen)).empty() ? 0 : 1;
    }

    auto nval = garglk::downcase(trim(glk_string(val, vallen)));
    return css_value_supported(nprop, nval) ? 1 : 0;
}

glui32 glk_css_supports_num(const char *prop, glui32 proplen, glsi32 val)
{
    auto numbuf = std::to_string(val);
    return glk_css_supports(prop, proplen, numbuf.c_str(),
            static_cast<glui32>(numbuf.size()));
}

#endif /* GLK_MODULE_CSS_SUPPORTS */
