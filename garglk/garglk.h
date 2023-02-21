// Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.
// Copyright (C) 2010 by Ben Cressey, Chris Spiegel.
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

// Private header file for the Gargoyle Implementation of the Glk API.
// Glk API which this implements: version 0.7.3.
// Glk designed by Andrew Plotkin <erkyrath@eblong.com>
// http://www.eblong.com/zarf/glk/index.html

#ifndef GARGLK_GARGLK_H
#define GARGLK_GARGLK_H

#ifdef __cplusplus
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "optional.hpp"

#include "glk.h"
#include "gi_dispa.h"

// This function is called whenever the library code catches an error
// or illegal operation from the game program.
inline void gli_strict_warning(const std::string &msg)
{
    std::cerr << "Glk library error: " << msg << std::endl;
}

enum class FileFilter { Save, Text, Data };
enum class FontType { Monospace, Proportional };

struct FontFace {
    FontFace() = delete;

    static FontFace propr() { return {false, false, false}; }
    static FontFace propb() { return {false, true, false}; }
    static FontFace propi() { return {false, false, true}; }
    static FontFace propz() { return {false, true, true}; }
    static FontFace monor() { return {true, false, false}; }
    static FontFace monob() { return {true, true, false}; }
    static FontFace monoi() { return {true, false, true}; }
    static FontFace monoz() { return {true, true, true}; }

    bool operator==(const FontFace &other) const {
        return monospace == other.monospace &&
               bold == other.bold &&
               italic == other.italic;
    }

    bool monospace;
    bool bold;
    bool italic;
};

// Taken from Boost 1.81.0.
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com).
inline std::size_t hash_combine(std::size_t seed, std::size_t h) noexcept {
    return seed ^ (h + 0x9e3779b9 + (seed << 6U) + (seed >> 2U));
}

namespace std {
template<>
struct hash<FontFace> {
    std::size_t operator()(const FontFace &fontface) const {
        return (fontface.monospace ? 1 : 0) |
               (fontface.bold      ? 2 : 0) |
               (fontface.italic    ? 4 : 0);
    }
};

template<>
struct hash<std::pair<FontFace, glui32>> {
    std::size_t operator()(const std::pair<FontFace, glui32> &pair) const {
        auto seed = hash_combine(0, std::hash<FontFace>()(pair.first));
        seed = hash_combine(seed, std::hash<glui32>()(pair.second));
        return seed;
    }
};
}

namespace garglk {

// This represents a possible configuration file (garglk.ini).
struct ConfigFile {
    enum class Type {
        System,
        User,
        PerGame,
    };

    ConfigFile(std::string path_, Type type_) : path(std::move(path_)), type(type_) {
    }

    std::string format_type() const {
        std::string status = "";
        std::ifstream f(path);
        if (!f.is_open()) {
            status = ", non-existent";
        }

        switch (type) {
        case Type::System:
            return "[system" + status + "]";
        case Type::User:
            return "[user" + status + "]";
        case Type::PerGame:
            return "[game specific" + status + "]";
        default:
            return "";
        }
    }

    // The path to the file itself.
    std::string path;

    // There are three types of config file:
    //
    // • System: System-wide configuration (e.g. /etc/garglk.ini)
    //   installed by the administrator, not intended to be edited by
    //   the user.
    // • User: A “user” config file, one that a user would reasonably
    //   expect to be a config file for general use. This excludes
    //   game-specific config files, for example, while considering
    //   various possibilities for config files, such as $HOME/.garglkrc
    //   or $HOME/.config/garglk.ini.
    // • PerGame: Per-game configuration. Games are able to ship their
    //   own configuration files to customize how Gargoyle looks when
    //   they are run. These are either named after the file (e.g.
    //   zork1.z3 comes with zork1.ini), or are the file “garglk.ini”
    //   placed in the game directory.
    Type type;
};

extern std::vector<garglk::ConfigFile> all_configs;

// C++17: std::clamp
template <typename T>
const T &clamp(const T &value, const T &min, const T &max)
{
    return value < min ? min : value > max ? max : value;
}

std::string winopenfile(const char *prompt, FileFilter filter);
std::string winsavefile(const char *prompt, FileFilter filter);
[[noreturn]]
void winabort(const std::string &msg);
std::string downcase(const std::string &string);
void fontreplace(const std::string &font, FontType type);
std::vector<ConfigFile> configs(const std::string &gamepath);
void config_entries(const std::string &fname, bool accept_bare, const std::vector<std::string> &matches, const std::function<void(const std::string &cmd, const std::string &arg)> &callback);
std::string user_config();
void set_lcdfilter(const std::string &filter);
std::string winfontpath(const std::string &filename);
std::vector<std::string> winappdata();
std::string winappdir();

namespace theme {
void init();
void set(std::string name);
std::vector<std::string> paths();
std::vector<std::string> names();
}

template <typename T, typename Deleter>
std::unique_ptr<T, Deleter> unique(T *p, Deleter deleter)
{
    return std::unique_ptr<T, Deleter>(p, deleter);
}

}

template <std::size_t N>
class PixelView;

// Represents an N-byte pixel, which in reality is just an array of
// bytes. It is up to the user of this class to determine the layout,
// e.g. BGR, RGBA, etc. In actuality, as far as Gargoyle is concerned,
// this will be either RGB or RGBA. An instance of Pixel owns the pixel
// data; contrast this with PixelView which holds a pointer to a pixel.
template <std::size_t N>
class Pixel {
public:
    template <typename... Args>
    explicit Pixel(Args... args) : m_pixel{static_cast<unsigned char>(args)...} {
    }

    // This is intentionally _not_ explicit so that PixelViews can be
    // used wherever a Pixel is expected.
    Pixel(const PixelView<N> &other) {
        memcpy(m_pixel.data(), other.data(), N);
    }

    bool operator==(const Pixel<N> &other) const {
        return m_pixel == other.m_pixel;
    }

    bool operator!=(const Pixel<N> &other) const {
        return !(*this == other);
    }

    const unsigned char *data() const {
        return m_pixel.data();
    }

    unsigned char operator[](std::size_t i) const {
        return m_pixel[i];
    }

private:
    std::array<unsigned char, N> m_pixel;
};

// Represents a view to existing pixel data (see Pixel). The pixel data
// is *not* owned by instances of this class. Instead, users of this
// class must pass in a pointer to an appropriately-sized buffer (at
// least N bytes) from/to which pixel data will be read/written.
// Assigning a Pixel to a PixelView will overwrite the pixel pointer
// with the new pixel data.
template <std::size_t N>
class PixelView {
public:
    explicit PixelView(unsigned char *data) : m_data(data) {
    }

    // The meaning of these is ambiguous: copy the data, or copy the
    // pointer? To prevent their accidental use, delete them.
    PixelView(const PixelView &) = delete;
    PixelView &operator=(const PixelView &) = delete;

    PixelView(PixelView &&) = default;
    PixelView &operator=(PixelView &&) = default;

    PixelView &operator=(const Pixel<N> &other) {
        std::memcpy(m_data, other.data(), N);

        return *this;
    }

    const unsigned char *data() const {
        return m_data;
    }

    unsigned char operator[](std::size_t i) const {
        return m_data[i];
    }

private:
    unsigned char *m_data;
};

template <std::size_t N>
class Row {
public:
    explicit Row(unsigned char *row) : m_row(row) {
    }

    PixelView<N> operator[](std::size_t x) {
        return PixelView<N>(&m_row[x * N]);
    }

    void fill(const Pixel<N> &pixel, int start, int end) {
        auto data = pixel.data();
        for (int i = start; i < end; i++) {
            std::memcpy(&m_row[i * N], data, N);
        }
    }

private:
    unsigned char *m_row;
};

template <std::size_t N>
class ConstRow {
public:
    explicit ConstRow(const unsigned char *row) : m_row(row) {
    }

    const unsigned char *operator[](std::size_t x) {
        return &m_row[x * N];
    }

private:
    const unsigned char *m_row;
};

template <std::size_t N>
class Canvas {
public:
    Canvas() = default;

    Canvas(int width, int height) :
        m_width(width),
        m_height(height)
    {
        resize(width, height, false);
    }

    void resize(int width, int height, bool keep) {
        if (keep) {
            auto backup = m_pixels;
            int minwidth = std::min(m_width, width);
            int minheight = std::min(m_height, height);

            m_pixels.resize(width * height * N);

            for (int y = 0; y < minheight; y++) {
                std::memcpy(&m_pixels[y * width * N], &backup[y * m_width * N], minwidth * N);
            }
        } else {
            m_pixels.resize(width * height * N);
        }

        m_pixels.shrink_to_fit();

        m_width = width;
        m_height = height;
        m_stride = width * N;
    }

    int width() {
        return m_width;
    }

    int height() {
        return m_height;
    }

    int stride() const {
        return m_stride;
    }

    void fill(const Pixel<N> &pixel) {
        for (int i = 0; i < m_width * m_height; i++) {
            std::memcpy(&m_pixels[i * N], pixel.data(), N);
        }
    }

    bool empty() const {
        return m_pixels.empty();
    }

    Row<N> operator[](std::size_t y) {
        return Row<N>(&m_pixels[y * stride()]);
    }

    ConstRow<N> operator[](std::size_t y) const {
        return ConstRow<N>(&m_pixels[y * stride()]);
    }

    unsigned char *data() {
        return m_pixels.data();
    }

    std::size_t size() const {
        return m_pixels.size();
    }

    void clear() {
        m_pixels.clear();
        m_pixels.shrink_to_fit();
    }

private:
    std::vector<unsigned char> m_pixels;
    int m_width = 0;
    int m_height = 0;
    int m_stride = 0;
};

using Color = Pixel<3>;

Color gli_parse_color(const std::string &str);

class Bleeps {
public:
    class Empty : public std::exception { };

    Bleeps();
    void update(int number, double duration, int frequency);
    void update(int number, const std::string &path);
    std::vector<std::uint8_t> &at(int number);

private:
    std::unordered_map<int, nonstd::optional<std::vector<std::uint8_t>>> m_bleeps = {
        {1, nonstd::nullopt},
        {2, nonstd::nullopt},
    };
};

extern Bleeps gli_bleeps;

// Callbacks necessary for the dispatch layer.

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock);
extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len,
    char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode,
    gidispatch_rock_t objrock);

// Some useful type declarations.

using window_t = struct glk_window_struct;
using stream_t = struct glk_stream_struct;
using fileref_t = struct glk_fileref_struct;
using channel_t = struct glk_schannel_struct;

struct window_blank_t;
struct window_pair_t;
struct window_textgrid_t;
struct window_textbuffer_t;
struct window_graphics_t;

// ----------------------------------------------------------------------
//
// Drawing operations and fonts and stuff
//

// Some globals for gargoyle

#define TBLINELEN 300
#define SCROLLBACK 512
#define HISTORYLEN 100

#define GLI_SUBPIX 8

extern std::string gli_program_name;
extern std::string gli_program_info;
extern std::string gli_story_name;
extern std::string gli_story_title;

extern bool gli_terminated;
extern bool gli_exiting;

extern window_t *gli_rootwin;
extern window_t *gli_focuswin;

extern bool gli_force_redraw;
extern bool gli_more_focus;
extern int gli_cellw;
extern int gli_cellh;

// Unicode ligatures and smart typography glyphs
#define UNI_LIG_FF	0xFB00
#define UNI_LIG_FI	0xFB01
#define UNI_LIG_FL	0xFB02
#define UNI_LIG_FFI	0xFB03
#define UNI_LIG_FFL	0xFB04
#define UNI_LSQUO	0x2018
#define UNI_RSQUO	0x2019
#define UNI_LDQUO	0x201c
#define UNI_RDQUO	0x201d
#define UNI_NDASH	0x2013
#define UNI_MDASH	0x2014

struct rect_t {
    int x0, y0;
    int x1, y1;
};

struct picture_t {
    picture_t(unsigned int id_, Canvas<4> rgba_, bool scaled_) :
        id(id_),
        rgba(std::move(rgba_)),
        w(rgba.width()),
        h(rgba.height()),
        scaled(scaled_)
    {
    }

    unsigned long id;
    Canvas<4> rgba;
    int w, h;
    bool scaled;
};

struct style_t {
    FontFace font;
    Color bg;
    Color fg;
    bool reverse;

    bool operator==(const style_t &other) const {
        return font == other.font &&
               bg == other.bg &&
               fg == other.fg &&
               reverse == other.reverse;
    }

    bool operator!=(const style_t &other) const {
        return !(*this == other);
    }
};

using Styles = std::array<style_t, style_NUMSTYLES>;

extern Canvas<3> gli_image_rgb;

//
// Config globals
//

extern std::string gli_workdir;
extern std::string gli_workfile;

extern Styles gli_tstyles;
extern Styles gli_gstyles;

extern const Styles gli_tstyles_def;
extern const Styles gli_gstyles_def;

extern Color gli_window_color;
extern Color gli_border_color;
extern Color gli_caret_color;
extern Color gli_more_color;
extern Color gli_link_color;

extern Color gli_window_save;
extern Color gli_border_save;
extern Color gli_caret_save;
extern Color gli_more_save;
extern Color gli_link_save;

extern nonstd::optional<Color> gli_override_fg;
extern nonstd::optional<Color> gli_override_bg;
extern bool gli_override_reverse;

extern bool gli_underline_hyperlinks;
extern int gli_caret_shape;
extern int gli_wborderx;
extern int gli_wbordery;

extern int gli_wmarginx;
extern int gli_wmarginy;
extern int gli_wmarginx_save;
extern int gli_wmarginy_save;
extern int gli_wpaddingx;
extern int gli_wpaddingy;
extern int gli_tmarginx;
extern int gli_tmarginy;

extern float gli_backingscalefactor;
extern float gli_zoom;

extern bool gli_conf_lcd;
extern std::array<unsigned char, 5> gli_conf_lcd_weights;

extern bool gli_conf_graphics;
extern bool gli_conf_sound;

extern bool gli_conf_fullscreen;

extern bool gli_conf_speak;
extern bool gli_conf_speak_input;

extern std::string gli_conf_speak_language;

extern bool gli_conf_stylehint;
extern bool gli_conf_safeclicks;

extern bool gli_conf_justify;
extern int gli_conf_quotes;
extern int gli_conf_dashes;
extern int gli_conf_spaces;
extern bool gli_conf_caps;

extern int gli_cols;
extern int gli_rows;

extern bool gli_conf_lockcols;
extern bool gli_conf_lockrows;

extern bool gli_conf_save_window_size;
extern bool gli_conf_save_window_location;

extern Color gli_scroll_bg;
extern Color gli_scroll_fg;
extern int gli_scroll_width;

extern int gli_baseline;
extern int gli_leading;

struct FontFiles {
    std::string r, b, i, z;
};
extern std::string gli_conf_propfont;
extern FontFiles gli_conf_prop, gli_conf_prop_override;
extern std::string gli_conf_monofont;
extern FontFiles gli_conf_mono, gli_conf_mono_override;

extern float gli_conf_gamma;
extern float gli_conf_propsize;
extern float gli_conf_monosize;
extern float gli_conf_propaspect;
extern float gli_conf_monoaspect;

extern std::vector<glui32> gli_more_prompt;
extern glui32 gli_more_prompt_len;
extern int gli_more_align;
extern FontFace gli_more_font;

extern bool gli_forceclick;
extern bool gli_copyselect;
extern bool gli_drawselect;
extern bool gli_claimselect;

extern bool gli_conf_per_game_config;

extern std::unordered_map<FontFace, std::vector<std::string>> gli_conf_glyph_substitution_files;

// XXX See issue #730.
extern bool gli_conf_redraw_hack;

template <typename T>
T gli_zoom_int(T x)
{
    return std::round(x * gli_zoom);
}

template <typename T>
T gli_unzoom_int(T x)
{
    return std::round(x / gli_zoom);
}

//
// Standard Glk I/O stuff
//

// A macro that I can't think of anywhere else to put it.

#define gli_event_clearevent(evp)  \
    ((evp)->type = evtype_None,    \
    (evp)->win = nullptr,    \
    (evp)->val1 = 0,   \
    (evp)->val2 = 0)

void gli_dispatch_event(event_t *event, bool polled);

#define MAGIC_WINDOW_NUM (9876)
#define MAGIC_STREAM_NUM (8769)
#define MAGIC_FILEREF_NUM (7698)

#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)
#define strtype_Resource (4)

struct glk_stream_struct {
    glui32 magicnum;
    glui32 rock;

    int type; // file, window, or memory stream
    bool unicode; // one-byte or four-byte chars? Not meaningful for windows

    glui32 readcount, writecount;
    bool readable, writable;

    // for strtype_Window
    window_t *win;

    // for strtype_File
    std::FILE *file;
    glui32 lastop; // 0, filemode_Write, or filemode_Read

    // for strtype_Resource
    bool isbinary;

    // for strtype_Memory and strtype_Resource. Separate pointers for
    // one-byte and four-byte streams
    unsigned char *buf;
    unsigned char *bufptr;
    unsigned char *bufend;
    unsigned char *bufeof;
    glui32 *ubuf;
    glui32 *ubufptr;
    glui32 *ubufend;
    glui32 *ubufeof;
    glui32 buflen;
    gidispatch_rock_t arrayrock;

    gidispatch_rock_t disprock;
    stream_t *next, *prev; // in the big linked list of streams
};

struct glk_fileref_struct {
    glui32 magicnum;
    glui32 rock;

    char *filename;
    int filetype;
    bool textmode;

    gidispatch_rock_t disprock;
    fileref_t *next, *prev; // in the big linked list of filerefs
};

//
// Windows and all that
//

// For some reason MinGW does "#define hyper __int64", which conflicts
// with attr_s.hyper below. Unconditionally undefine it here so any
// files which include windows.h will not cause build failures.
#undef hyper

struct attr_t {
    bool reverse = false;
    glui32 style = 0;
    nonstd::optional<Color> fgcolor;
    nonstd::optional<Color> bgcolor;
    glui32 hyper = 0;

    bool operator!=(const attr_t &other) const {
        return reverse != other.reverse ||
               style != other.style ||
               fgcolor != other.fgcolor ||
               bgcolor != other.bgcolor ||
               hyper != other.hyper;
    }

    void set(glui32 style_);
    void clear();
    FontFace font(const Styles &styles) const;
    Color bg(const Styles &styles) const;
    Color fg(const Styles &styles) const;
};

struct glk_window_struct {
    glk_window_struct(glui32 type_, glui32 rock_);
    glk_window_struct(const glk_window_struct &) = delete;
    glk_window_struct &operator=(const glk_window_struct &) = delete;
    ~glk_window_struct();

    glui32 magicnum = MAGIC_WINDOW_NUM;
    glui32 type;
    glui32 rock;

    window_t *parent = nullptr; // pair window which contains this one
    rect_t bbox;
    int yadj = 0;
    union {
        window_textgrid_t *textgrid;
        window_textbuffer_t *textbuffer;
        window_graphics_t *graphics;
        window_blank_t *blank;
        window_pair_t *pair;
    } window;

    stream_t *str;               // the window stream.
    stream_t *echostr = nullptr; // the window's echo stream, if any.

    bool line_request = false;
    bool line_request_uni = false;
    bool char_request = false;
    bool char_request_uni = false;
    bool mouse_request = false;
    bool hyper_request = false;
    bool more_request = false;
    bool scroll_request = false;
    bool image_loaded = false;

    bool echo_line_input = true;
    std::vector<glui32> line_terminators;

    attr_t attr;
    Color bgcolor = gli_window_color;
    Color fgcolor = gli_more_color;

    gidispatch_rock_t disprock;
    window_t *next, *prev; // in the big linked list of windows
};

struct window_blank_t {
    explicit window_blank_t(window_t *win) : owner(win) {
    }

    window_t *owner;
};

struct window_pair_t {
    window_pair_t(window_t *win, glui32 method, window_t *key_, glui32 size_) :
        owner(win),
        dir(method & winmethod_DirMask),
        vertical(dir == winmethod_Left || dir == winmethod_Right),
        backward(dir == winmethod_Left || dir == winmethod_Above),
        division(method & winmethod_DivisionMask),
        key(key_),
        size(size_),
        wborder((method & winmethod_BorderMask) == winmethod_Border)
    {
    }

    window_t *owner;
    window_t *child1 = nullptr, *child2 = nullptr;

    // split info...
    glui32 dir;              // winmethod_Left, Right, Above, or Below
    bool vertical, backward; // flags
    glui32 division;         // winmethod_Fixed or winmethod_Proportional
    window_t *key;           // NULL or a leaf-descendant (not a Pair)
    bool keydamage = false;  // used as scratch space in window closing
    glui32 size;             // size value
    bool wborder;            // winMethod_Border, NoBorder
};

// One line of the grid window.
struct tgline_t {
    bool dirty = false;
    std::array<glui32, 256> chars;
    std::array<attr_t, 256> attrs;
};

struct window_textgrid_t {
    window_textgrid_t(window_t *owner_, Styles styles_) :
        owner(owner_),
        styles(std::move(styles_))
    {
    }

    window_t *owner;

    int width = 0, height = 0;
    std::array<tgline_t, 256> lines;

    int curx = 0, cury = 0; // the window cursor position

    // for line input
    void *inbuf = nullptr; // unsigned char* for latin1, glui32* for unicode
    bool inunicode = false;
    int inorgx = 0, inorgy = 0;
    int inoriglen, inmax;
    int incurs, inlen;
    attr_t origattr;
    gidispatch_rock_t inarrayrock;
    std::vector<glui32> line_terminators;

    // style hints and settings
    Styles styles;
};

struct tbline_t {
    tbline_t() {
        chars.fill(' ');
    }
    int len = 0;
    bool newline = false, dirty = false, repaint = false;
    std::shared_ptr<picture_t> lpic, rpic;
    glui32 lhyper = 0, rhyper = 0;
    int lm = 0, rm = 0;
    std::array<glui32, TBLINELEN> chars;
    std::array<attr_t, TBLINELEN> attrs;
};

struct window_textbuffer_t {
    window_textbuffer_t(window_t *owner_, Styles styles_, int scrollback_) :
        owner(owner_),
        scrollback(scrollback_),
        styles(std::move(styles_))
    {
        lines.resize(scrollback);
        chars = lines[0].chars.data();
        attrs = lines[0].attrs.data();
    }

    window_t *owner;

    int width = -1, height = -1;
    int spaced = 0;
    int dashed = 0;

    std::vector<tbline_t> lines;
    int scrollback = SCROLLBACK;

    int numchars = 0; // number of chars in last line: lines[0]
    glui32 *chars;    // alias to lines[0].chars
    attr_t *attrs;    // alias to lines[0].attrs

    // adjust margins temporarily for images
    int ladjw = 0;
    int ladjn = 0;
    int radjw = 0;
    int radjn = 0;

    // Command history.
    std::deque<std::vector<glui32>> history;
    std::deque<std::vector<glui32>>::iterator history_it = history.begin();

    // for paging
    int lastseen = 0;
    int scrollpos = 0;
    int scrollmax = 0;

    // for line input
    void *inbuf = nullptr; // unsigned char* for latin1, glui32* for unicode
    bool inunicode = false;
    int inmax;
    long infence;
    long incurs;
    attr_t origattr;
    gidispatch_rock_t inarrayrock;

    bool echo_line_input = true;
    std::vector<glui32> line_terminators;

    // style hints and settings
    Styles styles;

    // for copy selection
    std::vector<glui32> copybuf;
    int copypos = 0;
};

struct window_graphics_t {
    explicit window_graphics_t(window_t *win) :
        owner(win),
        bgnd(win->bgcolor)
    {
    }

    window_t *owner;
    Color bgnd;
    bool dirty = false;
    int w = 0, h = 0;
    Canvas<3> rgb;
};

// ----------------------------------------------------------------------

extern void gli_initialize_sound();
extern void gli_initialize_tts();
extern void gli_tts_speak(const glui32 *buf, std::size_t len);
extern void gli_tts_flush();
extern void gli_tts_purge();

extern gidispatch_rock_t gli_sound_get_channel_disprock(const channel_t *chan);

// ----------------------------------------------------------------------
//
// All the annoyingly boring and tedious prototypes...
//

[[noreturn]]
extern void gli_exit(int status);

extern window_blank_t *win_blank_create(window_t *win);
extern void win_blank_destroy(window_blank_t *dwin);
extern void win_blank_rearrange(window_t *win, const rect_t *box);
extern void win_blank_redraw(window_t *win);

extern window_pair_t *win_pair_create(window_t *win, glui32 method, window_t *key, glui32 size);
extern void win_pair_destroy(window_pair_t *dwin);
extern void win_pair_rearrange(window_t *win, const rect_t *box);
extern void win_pair_redraw(window_t *win);
extern void win_pair_click(window_pair_t *dwin, int x, int y);

extern window_textgrid_t *win_textgrid_create(window_t *win);
extern void win_textgrid_destroy(window_textgrid_t *dwin);
extern void win_textgrid_rearrange(window_t *win, rect_t *box);
extern void win_textgrid_redraw(window_t *win);
extern void win_textgrid_putchar_uni(window_t *win, glui32 ch);
extern bool win_textgrid_unputchar_uni(window_t *win, glui32 ch);
extern void win_textgrid_clear(window_t *win);
extern void win_textgrid_move_cursor(window_t *win, int xpos, int ypos);
extern void win_textgrid_init_line(window_t *win, char *buf, int maxlen, int initlen);
extern void win_textgrid_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen);
extern void win_textgrid_cancel_line(window_t *win, event_t *ev);
extern void win_textgrid_click(window_textgrid_t *dwin, int x, int y);
extern void gcmd_grid_accept_readchar(window_t *win, glui32 arg);
extern void gcmd_grid_accept_readline(window_t *win, glui32 arg);

extern window_textbuffer_t *win_textbuffer_create(window_t *win);
extern void win_textbuffer_destroy(window_textbuffer_t *dwin);
extern void win_textbuffer_rearrange(window_t *win, rect_t *box);
extern void win_textbuffer_redraw(window_t *win);
extern void win_textbuffer_putchar_uni(window_t *win, glui32 ch);
extern bool win_textbuffer_unputchar_uni(window_t *win, glui32 ch);
extern void win_textbuffer_clear(window_t *win);
extern void win_textbuffer_init_line(window_t *win, char *buf, int maxlen, int initlen);
extern void win_textbuffer_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen);
extern void win_textbuffer_cancel_line(window_t *win, event_t *ev);
extern void win_textbuffer_click(window_textbuffer_t *dwin, int x, int y);
extern void gcmd_buffer_accept_readchar(window_t *win, glui32 arg);
extern void gcmd_buffer_accept_readline(window_t *win, glui32 arg);
extern bool gcmd_accept_scroll(window_t *win, glui32 arg);

// Declarations of library internal functions.

extern void gli_initialize_misc();
extern void gli_initialize_windows();
extern void gli_initialize_babel();

extern window_t *gli_window_iterate_treeorder(window_t *win);

extern void gli_window_rearrange(window_t *win, rect_t *box);
extern void gli_window_redraw(window_t *win);
extern void gli_window_put_char_uni(window_t *win, glui32 ch);
extern bool gli_window_unput_char_uni(window_t *win, glui32 ch);
extern bool gli_window_check_terminator(glui32 ch);
extern void gli_window_refocus(window_t *win);

extern void gli_windows_redraw();
extern void gli_windows_size_change(int w, int h);
extern void gli_windows_unechostream(stream_t *str);

extern void gli_window_click(window_t *win, int x, int y);

void gli_redraw_rect(int x0, int y0, int x1, int y1);

void gli_input_handle_key(glui32 key);
void gli_input_handle_click(int x, int y);
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2);

extern stream_t *gli_new_stream(int type, int readable, int writable,
    glui32 rock);
extern void gli_delete_stream(stream_t *str);
extern stream_t *gli_stream_open_window(window_t *win);
extern strid_t gli_stream_open_pathname(char *pathname, int writemode,
    int textmode, glui32 rock);
extern void gli_stream_set_current(stream_t *str);
extern void gli_stream_fill_result(stream_t *str,
    stream_result_t *result);
extern void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
extern void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);
extern void gli_streams_close_all();

void gli_initialize_fonts();
void gli_draw_pixel(int x, int y, const Color &rgb);
void gli_draw_clear(const Color &rgb);
void gli_draw_rect(int x, int y, int w, int h, const Color &rgb);
int gli_draw_string_uni(int x, int y, FontFace face, const Color &rgb, const glui32 *text, int len, int spacewidth);
int gli_string_width_uni(FontFace face, const glui32 *text, int len, int spacewidth);
void gli_draw_caret(int x, int y);
void gli_draw_picture(const picture_t *pic, int x, int y, int x0, int y0, int x1, int y1);

void gli_startup(int argc, char *argv[]);

extern void gli_select(event_t *event, bool polled);
#ifdef GARGLK_CONFIG_TICK
extern void gli_tick();
#endif

void wininit(int *argc, char **argv);
void winopen();
void wintitle();
void winmore();
void winrepaint(int x0, int y0, int x1, int y1);
bool windark();
void winexit();
void winclipstore(const glui32 *text, int len);

void fontload();
void fontunload();

void giblorb_get_resource(glui32 usage, glui32 resnum, std::FILE **file, long *pos, long *len, glui32 *type);

std::shared_ptr<picture_t> gli_picture_load(unsigned long id);
void gli_picture_store(const std::shared_ptr<picture_t> &pic);
std::shared_ptr<picture_t> gli_picture_retrieve(unsigned long id, bool scaled);
std::shared_ptr<picture_t> gli_picture_scale(const picture_t *src, int newcols, int newrows);
void gli_piclist_increment();
void gli_piclist_decrement();

window_graphics_t *win_graphics_create(window_t *win);
void win_graphics_destroy(window_graphics_t *dwin);
void win_graphics_rearrange(window_t *win, rect_t *box);
void win_graphics_get_size(window_t *win, glui32 *width, glui32 *height);
void win_graphics_redraw(window_t *win);
void win_graphics_click(window_graphics_t *dwin, int x, int y);

bool win_graphics_draw_picture(window_graphics_t *dwin,
  glui32 image, glsi32 xpos, glsi32 ypos,
  bool scale, glui32 imagewidth, glui32 imageheight);
void win_graphics_erase_rect(window_graphics_t *dwin, bool whole, glsi32 x0, glsi32 y0, glui32 width, glui32 height);
void win_graphics_fill_rect(window_graphics_t *dwin, glui32 color, glsi32 x0, glsi32 y0, glui32 width, glui32 height);
void win_graphics_set_background_color(window_graphics_t *dwin, glui32 color);

bool win_textbuffer_draw_picture(window_textbuffer_t *dwin, glui32 image, glui32 align, bool scaled, glui32 width, glui32 height);
void win_textbuffer_flow_break(window_textbuffer_t *win);

void gli_calc_padding(window_t *win, int *x, int *y);

// unicode case mapping

using gli_case_block_t = glui32[2];
// If both are 0xFFFFFFFF, you have to look at the special-case table

using gli_case_special_t = glui32[3];
// Each of these points to a subarray of the unigen_special_array
// (in cgunicode.c). In that subarray, element zero is the length,
// and that's followed by length unicode values.

using gli_decomp_block_t = glui32[2];
// The position points to a subarray of the unigen_decomp_array.
// If the count is zero, there is no decomposition.

void gli_putchar_utf8(glui32 val, std::FILE *fl);
glui32 gli_getchar_utf8(std::FILE *fl);
glui32 gli_parse_utf8(const unsigned char *buf, glui32 buflen, glui32 *out, glui32 outlen);

glui32 gli_strlen_uni(const glui32 *s);

void gli_put_hyperlink(glui32 linkval, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1);
glui32 gli_get_hyperlink(int x, int y);
void gli_clear_selection();
bool gli_check_selection(int x0, int y0, int x1, int y1);
bool gli_get_selection(int x0, int y0, int x1, int y1, int *rx0, int *rx1);
void gli_clipboard_copy(const glui32 *buf, int len);
void gli_start_selection(int x, int y);
void gli_resize_mask(unsigned int x, unsigned int y);
void gli_move_selection(int x, int y);
void gli_notification_waiting();

void gli_edit_config();

// A macro which reads and decodes one character of UTF-8. Needs no
// explanation, I'm sure.
//
// Oh, okay. The character will be written to *chptr (so pass in "&ch",
// where ch is a glui32 variable). eofcond should be a condition to
// evaluate end-of-stream -- true if no more characters are readable.
// nextch is a function which reads the next character; this is invoked
// exactly as many times as necessary.
//
// val0, val1, val2, val3 should be glui32 scratch variables. The macro
// needs these. Just define them, you don't need to pay attention to them
// otherwise.
//
// The macro itself evaluates to true if ch was successfully set, or
// false if something went wrong. (Not enough characters, or an
// invalid byte sequence.)
//
// This is not the worst macro I've ever written, but I forget what the
// other one was.

#define UTF8_DECODE_INLINE(chptr, eofcond, nextch, val0, val1, val2, val3)  ( \
    (eofcond ? 0 : ( \
        (((val0=nextch) < 0x80) ? (*chptr=val0, 1) : ( \
            (eofcond ? 0 : ( \
                (((val1=nextch) & 0xC0) != 0x80) ? 0 : ( \
                    (((val0 & 0xE0) == 0xC0) ? (*chptr=((val0 & 0x1F) << 6) | (val1 & 0x3F), 1) : ( \
                        (eofcond ? 0 : ( \
                            (((val2=nextch) & 0xC0) != 0x80) ? 0 : ( \
                                (((val0 & 0xF0) == 0xE0) ? (*chptr=(((val0 & 0xF)<<12)  & 0x0000F000) | (((val1 & 0x3F)<<6) & 0x00000FC0) | (((val2 & 0x3F))    & 0x0000003F), 1) : ( \
                                    (((val0 & 0xF0) != 0xF0 || eofcond) ? 0 : (\
                                        (((val3=nextch) & 0xC0) != 0x80) ? 0 : (*chptr=(((val0 & 0x7)<<18)   & 0x1C0000) | (((val1 & 0x3F)<<12) & 0x03F000) | (((val2 & 0x3F)<<6)  & 0x000FC0) | (((val3 & 0x3F))     & 0x00003F), 1) \
                                        )) \
                                    )) \
                                )) \
                            )) \
                        )) \
                )) \
            )) \
        )) \
    )
#endif

#endif
