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

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}

#ifdef ZERP_GLK_WINGLK
// rpcndr.h, eventually included via WinGlk.h, defines a type “byte”
// which conflicts with the “byte” from memory.h. Temporarily redefine
// it to avoid a compile error.
#define byte rpc_byte
// Windows Glk puts non-standard declarations (specifically for this
// file, those guarded by GLK_MODULE_GARGLKTEXT) in WinGlk.h, so include
// it to get color/style extensions.
#include <WinGlk.h>
#undef byte
#endif
#endif

#include "screen.h"
#include "branch.h"
#include "dict.h"
#include "iff.h"
#include "io.h"
#include "meta.h"
#include "memory.h"
#include "objects.h"
#include "osdep.h"
#include "process.h"
#include "sound.h"
#include "stack.h"
#include "stash.h"
#include "types.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

// Somewhat ugly hack to get around the fact that some Glk functions may
// not exist. These function calls should all be guarded (e.g.
// if (have_unicode), with have_unicode being set iff GLK_MODULE_UNICODE
// is defined) so they will never be called if the Glk implementation
// being used does not support them, but they must at least exist to
// prevent link errors.
#ifdef ZTERP_GLK
#ifndef GLK_MODULE_UNICODE
#define glk_put_char_uni(...)		die("bug %s:%d: glk_put_char_uni() called with no unicode", __FILE__, __LINE__)
#define glk_put_char_stream_uni(...)	die("bug %s:%d: glk_put_char_stream_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_char_event_uni(...)	die("bug %s:%d: glk_request_char_event_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_line_event_uni(...)	die("bug %s:%d: glk_request_line_event_uni() called with no unicode", __FILE__, __LINE__)
#endif
#ifndef GLK_MODULE_LINE_ECHO
#define glk_set_echo_line_event(...)	die("bug: %s %d: glk_set_echo_line_event() called with no line echo", __FILE__, __LINE__)
#endif
#endif

// Flag describing whether the header bit meaning “fixed font” is set.
static bool header_fixed_font;

// This variable stores whether Unicode is supported by the current Glk
// implementation, which determines whether Unicode or Latin-1 Glk
// functions are used. In addition, for both Glk and non-Glk versions,
// this affects the behavior of @check_unicode. In non-Glk versions,
// this is always true.
static bool have_unicode;

struct Window {
    Style style;
    Color fg_color = Color(), bg_color = Color();

    enum class Font { Query, Normal, Picture, Character, Fixed } font = Font::Normal;

#ifdef ZTERP_GLK
    winid_t id = nullptr;
    long x = 0, y = 0; // Only meaningful for window 1
    bool has_echo = false;
#endif
};

static std::array<Window, 8> windows;
static Window *mainwin = &windows[0], *curwin = &windows[0];
#ifdef ZTERP_GLK
// This represents a line of input from Glk; if the global variable
// “have_unicode” is true, then the “unicode” member is used; otherwise,
// “latin1”.
struct Line {
    union {
        std::array<char, 256> latin1;
        std::array<glui32, 256> unicode;
    };
    glui32 len;
};

static Window *upperwin = &windows[1];
static Window statuswin;
static long upper_window_height = 0;
static long upper_window_width = 0;
static winid_t errorwin;
#endif

// In all versions but 6, styles and colors are global and stored in
// mainwin. For V6, they’re tracked per window and thus stored in each
// individual window. For convenience, this macro returns the “style
// window” for any version.
static Window *style_window()
{
    return zversion == 6 ? curwin : mainwin;
}

// Output stream bits.
enum {
    STREAM_SCREEN = 1,
    STREAM_TRANS  = 2,
    STREAM_MEMORY = 3,
    STREAM_SCRIPT = 4,
};

static std::bitset<5> streams;
static std::unique_ptr<IO> scriptio, transio, perstransio;

class StreamTables {
public:
    void push(uint16_t addr) {
        ZASSERT(m_tables.size() < MAX_STREAM_TABLES, "too many stream tables");
        m_tables.emplace_back(addr);
    }

    void pop() {
        if (!m_tables.empty()) {
            m_tables.pop_back();
        }
    }

    void write(uint16_t c) {
        ZASSERT(!m_tables.empty(), "invalid stream table");
        m_tables.back().write(c);
    }

    size_t size() const {
        return m_tables.size();
    }

    void clear() {
        m_tables.clear();
    }

private:
    static constexpr size_t MAX_STREAM_TABLES = 16;
    class Table {
    public:
        explicit Table(uint16_t addr) : m_addr(addr) {
            user_store_word(m_addr, 0);
        }

        Table(const Table &) = delete;
        Table &operator=(const Table &) = delete;

        ~Table() {
            user_store_word(m_addr, m_idx - 2);
            if (zversion == 6) {
                store_word(0x30, m_idx - 2);
            }
        }

        void write(uint8_t c) {
            user_store_byte(m_addr + m_idx++, c);
        }

    private:
        uint16_t m_addr;
        uint16_t m_idx = 2;
    };

    std::list<Table> m_tables;
};

static StreamTables stream_tables;

static int istream = ISTREAM_KEYBOARD;
static std::unique_ptr<IO> istreamio;

struct Input {
    enum class Type { Char, Line } type;

    // ZSCII value of key read for @read_char.
    uint8_t key;

    // Unicode line of chars read for @read.
    std::array<uint16_t, 256> line;
    uint8_t maxlen;
    uint8_t len;
    uint8_t preloaded;

    // Character used to terminate input. If terminating keys are not
    // supported by the Glk implementation being used (or if Glk is not
    // used at all) this will be ZSCII_NEWLINE; or in the case of
    // cancellation, 0.
    uint8_t term;
};

// Convert a 15-bit color to a 24-bit color.
uint32_t screen_convert_color(uint16_t color)
{
    // Map 5-bit color values to 8-bit.
    const uint32_t table[] = {
        0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3a,
        0x42, 0x4a, 0x52, 0x5a, 0x63, 0x6b, 0x73, 0x7b,
        0x84, 0x8c, 0x94, 0x9c, 0xa5, 0xad, 0xb5, 0xbd,
        0xc5, 0xce, 0xd6, 0xde, 0xe6, 0xef, 0xf7, 0xff
    };

    return table[(color >>  0) & 0x1f] << 16 |
           table[(color >>  5) & 0x1f] <<  8 |
           table[(color >> 10) & 0x1f] <<  0;
}

#ifdef GLK_MODULE_GARGLKTEXT
static glui32 zcolor_map[] = {
    static_cast<glui32>(zcolor_Current),
    static_cast<glui32>(zcolor_Default),

    0x000000,	// Black
    0xef0000,	// Red
    0x00d600,	// Green
    0xefef00,	// Yellow
    0x006bb5,	// Blue
    0xff00ff,	// Magenta
    0x00efef,	// Cyan
    0xffffff,	// White
    0xb5b5b5,	// Light grey
    0x8c8c8c,	// Medium grey
    0x5a5a5a,	// Dark grey
};

void update_color(int which, unsigned long color)
{
    if (which < 2 || which > 12) {
        return;
    }

    zcolor_map[which] = color;
}

// Provide descriptive aliases for Gargoyle styles.
enum {
    GStyleBoldItalicFixed = style_Note,
    GStyleBoldItalic      = style_Alert,
    GStyleBoldFixed       = style_User1,
    GStyleItalicFixed     = style_User2,
    GStyleBold            = style_Subheader,
    GStyleItalic          = style_Emphasized,
    GStyleFixed           = style_Preformatted,
};

static int gargoyle_style(const Style &style)
{
    if (style.test(STYLE_BOLD) && style.test(STYLE_ITALIC) && style.test(STYLE_FIXED)) {
        return GStyleBoldItalicFixed;
    } else if (style.test(STYLE_BOLD) && style.test(STYLE_ITALIC)) {
        return GStyleBoldItalic;
    } else if (style.test(STYLE_BOLD) && style.test(STYLE_FIXED)) {
        return GStyleBoldFixed;
    } else if (style.test(STYLE_ITALIC) && style.test(STYLE_FIXED)) {
        return GStyleItalicFixed;
    } else if (style.test(STYLE_BOLD)) {
        return GStyleBold;
    } else if (style.test(STYLE_ITALIC)) {
        return GStyleItalic;
    } else if (style.test(STYLE_FIXED)) {
        return GStyleFixed;
    }

    return style_Normal;
}

static glui32 gargoyle_color(const Color &color)
{
    switch (color.mode) {
    case Color::Mode::ANSI:
        return zcolor_map[color.value];
    case Color::Mode::True:
        return screen_convert_color(color.value);
    }

    return zcolor_Current;
}
#endif

#ifdef ZTERP_GLK
// These functions make it so that code elsewhere needn’t check have_unicode before printing.
static inline void xglk_put_char(uint16_t c)
{
    if (!have_unicode) {
        glk_put_char(unicode_to_latin1[c]);
    } else {
        glk_put_char_uni(c);
    }
}

static inline void xglk_put_char_stream(strid_t s, uint32_t c)
{
    if (!have_unicode) {
        glk_put_char_stream(s, unicode_to_latin1[c]);
    } else {
        glk_put_char_stream_uni(s, c);
    }
}
#endif

static void set_window_style(Window *win)
{
#ifdef ZTERP_GLK
    auto style = win->style;
    if (curwin->id == nullptr) {
        return;
    }

#ifdef GLK_MODULE_GARGLKTEXT
    if (curwin->font == Window::Font::Fixed || header_fixed_font) {
        style.set(STYLE_FIXED);
    }

    if (options.disable_fixed) {
        style.reset(STYLE_FIXED);
    }

    glk_set_style(gargoyle_style(style));

    garglk_set_reversevideo(style.test(STYLE_REVERSE));

    garglk_set_zcolors(gargoyle_color(win->fg_color), gargoyle_color(win->bg_color));
#else
    // Yes, there are three ways to indicate that a fixed-width font should be used.
    bool use_fixed_font = style.test(STYLE_FIXED) || curwin->font == Window::Font::Fixed || header_fixed_font;

    // Glk can’t mix other styles with fixed-width, but the upper window
    // is always fixed, so if it is selected, there is no need to
    // explicitly request it here. In addition, the user can disable
    // fixed-width fonts or tell Bocfel to assume that the output font is
    // already fixed (e.g. in an xterm); in either case, there is no need
    // to request a fixed font.
    // This means that another style can also be applied if applicable.
    if (use_fixed_font && !options.disable_fixed && !options.assume_fixed && curwin != upperwin) {
        glk_set_style(style_Preformatted);
        return;
    }

    // According to standard 1.1, if mixed styles aren’t available, the
    // priority is Fixed, Italic, Bold, Reverse.
    if (style.test(STYLE_ITALIC)) {
        glk_set_style(style_Emphasized);
    } else if (style.test(STYLE_BOLD)) {
        glk_set_style(style_Subheader);
    } else if (style.test(STYLE_REVERSE)) {
        glk_set_style(style_Alert);
    } else {
        glk_set_style(style_Normal);
    }
#endif
#else
    zterp_os_set_style(win->style, win->fg_color, win->bg_color);
#endif
}

static void set_current_style()
{
    set_window_style(style_window());
}

// The following implements a circular buffer to track the state of the
// screen so that recent history can be stored in save files for
// playback on restore.
constexpr size_t HISTORY_SIZE = 2000;

class History {
public:
    struct Entry {
// Suppress a dubious shadow warning (see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55776)
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
        // These values are part of the Bfhs chunk so must remain stable.
        enum class Type {
            Style = 0,
            FGColor = 1,
            BGColor = 2,
            InputStart = 3,
            InputEnd = 4,
            Char = 5,
        } type;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

        union Contents {
            Color color;
            uint8_t style;
            uint32_t c;

            Contents() : c(0) { }
            explicit Contents(Color color_) : color(color_) { }
            explicit Contents(uint8_t style_) : style(style_) { }
            explicit Contents(uint32_t c_) : c(c_) { }
        } contents;

        explicit Entry(Type type_) : type(type_) { }
        Entry(Type type_, const Color &color) : type(type_), contents(color) { }
        Entry(Type type_, uint8_t style) : type(type_), contents(style) { }
        Entry(Type type_, uint32_t c) : type(type_), contents(c) { }

        static Entry style(uint8_t style) {
            return {Type::Style, style};
        }

        static Entry color(Type color_type, const Color &color) {
            return {color_type, color};
        }

        static Entry character(uint32_t c) {
            return {Type::Char, c};
        }
    };

    std::deque<Entry>::size_type size() {
        return m_entries.size();
    }

    void add_style() {
        auto style = mainwin->style;

        if (mainwin->font == Window::Font::Fixed || header_fixed_font) {
            style.set(STYLE_FIXED);
        }

        add(Entry::style(style.to_ulong()));
    }

    void add_fg_color(const Color &color) {
        add(Entry::color(Entry::Type::FGColor, color));
    }

    void add_bg_color(const Color &color) {
        add(Entry::color(Entry::Type::BGColor, color));
    }

    void add_input(const uint16_t *string, size_t len) {
        add(Entry(Entry::Type::InputStart));

        for (size_t i = 0; i < len; i++) {
            add(Entry::character(string[i]));
        }

        add(Entry::character(UNICODE_LINEFEED));
        add(Entry(Entry::Type::InputEnd));
    }

    void add_input_start() {
        add(Entry(Entry::Type::InputStart));
    }

    void add_input_end() {
        add(Entry(Entry::Type::InputEnd));
    }

    void add_input_char(uint32_t c) {
        add(Entry::character(c));
    }

    void add_char(uint32_t c) {
        add(Entry::character(c));
    }

    const std::deque<Entry> &entries() {
        return m_entries;
    }

private:
    void add(const Entry &entry) {
        while (m_entries.size() >= HISTORY_SIZE) {
            m_entries.pop_front();
        }

        m_entries.push_back(entry);
    }

    std::deque<Entry> m_entries;
};

static History history;

void screen_set_header_bit(bool set)
{
    if (set != header_fixed_font) {
        header_fixed_font = set;
        history.add_style();
        set_current_style();
    }
}

static void transcribe(uint32_t c)
{
    if (streams.test(STREAM_TRANS)) {
        transio->putc(c);
    }

    if (perstransio != nullptr) {
        perstransio->putc(c);
    }
}

// Print out a character. The character is in “c” and is either Unicode
// or ZSCII; if the former, “unicode” is true. This is meant for any
// output produced by the game, as opposed to output produced by the
// interpreter, which should use Glk (or standard I/O) calls only, to
// avoid interacting with the Z-machine’s streams: interpreter-provided
// output should not be considered part of a transcript, nor should it
// be included in the memory stream.
static void put_char_base(uint16_t c, bool unicode)
{
    if (c == 0) {
        return;
    }

    if (streams.test(STREAM_MEMORY)) {
        // When writing to memory, ZSCII should always be used (§7.5.3).
        if (unicode) {
            c = unicode_to_zscii_q[c];
        }

        stream_tables.write(c);
    } else {
        // For screen and transcription, always prefer Unicode.
        if (!unicode) {
            c = zscii_to_unicode[c];
        }

        if (c != 0) {
            uint8_t zscii = 0;

            // §16 makes no mention of what a newline in font 3 should map to.
            // Other interpreters that implement font 3 assume it stays a
            // newline, and this makes the most sense, so don’t do any
            // translation in that case.
            if (curwin->font == Window::Font::Character && !options.disable_graphics_font && c != UNICODE_LINEFEED) {
                zscii = unicode_to_zscii[c];

                // These four characters have a “built-in” reverse video (see §16).
                if (zscii >= 123 && zscii <= 126) {
                    style_window()->style.flip(STYLE_REVERSE);
                    set_current_style();
                }

                c = zscii_to_font3[zscii];
            }
#ifdef ZTERP_GLK
            if (streams.test(STREAM_SCREEN) && curwin->id != nullptr) {
                if (curwin == upperwin) {
                    // Interpreters seem to have differing ideas about what
                    // happens when the cursor reaches the end of a line in the
                    // upper window. Some wrap, some let it run off the edge (or,
                    // at least, stop the text at the edge). The standard, from
                    // what I can see, says nothing on this issue. Follow Windows
                    // Frotz and don’t wrap.

                    if (c == UNICODE_LINEFEED) {
                        if (upperwin->y < upper_window_height) {
                            // Glk wraps, so printing a newline when the cursor has
                            // already reached the edge of the screen will produce two
                            // newlines.
                            if (upperwin->x < upper_window_width) {
                                xglk_put_char(c);
                            }

                            // Even if a newline isn’t explicitly printed here
                            // (because the cursor is at the edge), setting
                            // upperwin->x to 0 will cause the next character to be on
                            // the next line because the text will have wrapped.
                            upperwin->x = 0;
                            upperwin->y++;
                        }
                    } else if (upperwin->x < upper_window_width && upperwin->y < upper_window_height) {
                        upperwin->x++;
                        xglk_put_char(c);
                    }
                } else {
                    xglk_put_char(c);
                }
            }
#else
            if (streams.test(STREAM_SCREEN) && curwin == mainwin) {
#ifdef ZTERP_DOS
                // DOS doesn’t support Unicode, but instead uses code
                // page 437. Special-case non-Glk DOS here, by writing
                // bytes (not UTF-8 characters) from code page 437.
                IO::standard_out().write8(unicode_to_437(c));
#else
                IO::standard_out().putc(c);
#endif
            }
#endif

            if (curwin == mainwin) {
                // Don’t check streams here: for quote boxes (which are in the
                // upper window, and thus not transcribed), both Infocom and
                // Inform games turn off the screen stream and write a duplicate
                // copy of the quote, so it appears in a transcript (if any is
                // occurring). In short, assume that if a game is writing text
                // with the screen stream turned off, it’s doing so with the
                // expectation that it appear in a transcript, which means it also
                // ought to appear in the history.
                history.add_char(c);

                transcribe(c);
            }

            // If the reverse video bit was flipped (for the character font), flip it back.
            if (zscii >= 123 && zscii <= 126) {
                style_window()->style.flip(STYLE_REVERSE);
                set_current_style();
            }
        }
    }
}

static void put_char_u(uint16_t c)
{
    put_char_base(c, true);
}

void put_char(uint8_t c)
{
    put_char_base(c, false);
}

// Print a C string (UTF-8) directly to the main window. This is meant
// to print text from the interpreter, not the game. Text will also be
// written to any transcripts which are active, as well as to the
// history buffer. For convenience, carriage returns are ignored under
// the assumption that they are coming from a Windows text stream.
void screen_print(const char *s)
{
    auto io = std::make_unique<IO>(std::vector<uint8_t>(s, s + strlen(s)), IO::Mode::ReadOnly);
#ifdef ZTERP_GLK
    strid_t stream = glk_window_get_stream(mainwin->id);
#endif
    for (long c = io->getc(false); c != -1; c = io->getc(false)) {
        if (c != UNICODE_CARRIAGE_RETURN) {
            transcribe(c);
            history.add_char(c);
#ifdef ZTERP_GLK
            xglk_put_char_stream(stream, c);
#endif
        }
    }
#ifndef ZTERP_GLK
    std::cout << s;
#endif
}

void screen_printf(const char *fmt, ...)
{
    std::va_list ap;
    std::string message;

    va_start(ap, fmt);
    message = vstring(fmt, ap);
    va_end(ap);

    screen_print(message.c_str());
}

void screen_puts(const char *s)
{
    screen_print(s);
    screen_print("\n");
}

void show_message(const char *fmt, ...)
{
    std::va_list ap;
    std::string message;

    va_start(ap, fmt);
    message = vstring(fmt, ap);
    va_end(ap);

#ifdef ZTERP_GLK
    static glui32 error_lines = 0;

    if (errorwin != nullptr) {
        glui32 w, h;

        // Allow multiple messages to stack, but force at least 5 lines to
        // always be visible in the main window. This is less than perfect
        // because it assumes that each message will be less than the width
        // of the screen, but it’s not a huge deal, really; even if the
        // lines are too long, at least Gargoyle and glktermw are graceful
        // enough.
        glk_window_get_size(mainwin->id, &w, &h);

        if (h > 5) {
            glk_window_set_arrangement(glk_window_get_parent(errorwin), winmethod_Below | winmethod_Fixed, ++error_lines, errorwin);
        }

        glk_put_char_stream(glk_window_get_stream(errorwin), LATIN1_LINEFEED);
    } else {
        errorwin = glk_window_open(mainwin->id, winmethod_Below | winmethod_Fixed, error_lines = 2, wintype_TextBuffer, 0);
    }

    // If windows are not supported (e.g. in cheapglk or no Glk), messages
    // will not get displayed. If this is the case, print to the main
    // window.
    if (errorwin != nullptr) {
        strid_t stream = glk_window_get_stream(errorwin);

        glk_set_style_stream(stream, style_Alert);
        for (size_t i = 0; message[i] != 0; i++) {
            xglk_put_char_stream(stream, char_to_unicode(message[i]));
        }
    } else
#endif
    {
        screen_printf("\n[%s]\n", message.c_str());
    }
}

void screen_message_prompt(const char *message)
{
    screen_puts(message);
    if (curwin == mainwin) {
        screen_print("\n>");
    }
}

// See §7.
// This returns true if the stream was successfully selected.
// Deselecting a stream is always successful.
bool output_stream(int16_t number, uint16_t table)
{
    ZASSERT(std::labs(number) <= (zversion >= 3 ? 4 : 2), "invalid output stream selected: %ld", (long)number);

    if (number == 0) {
        return true;
    } else if (number > 0) {
        streams.set(number);
    } else if (number < 0) {
        if (number != -3 || stream_tables.size() == 1) {
            streams.reset(-number);
        }
    }

    if (number == 2) {
        store_word(0x10, word(0x10) | FLAGS2_TRANSCRIPT);
        if (transio == nullptr) {
            try {
                transio = std::make_unique<IO>(options.transcript_name.get(), options.overwrite_transcript ? IO::Mode::WriteOnly : IO::Mode::Append, IO::Purpose::Transcript);
            } catch (const IO::OpenError &) {
                store_word(0x10, word(0x10) & ~FLAGS2_TRANSCRIPT);
                streams.reset(STREAM_TRANS);
                warning("unable to open the transcript");
            }
        }
    } else if (number == -2) {
        store_word(0x10, word(0x10) & ~FLAGS2_TRANSCRIPT);
    }

    if (number == 3) {
        stream_tables.push(table);
    } else if (number == -3) {
        stream_tables.pop();
    }

    if (number == 4) {
        if (scriptio == nullptr) {
            try {
                scriptio = std::make_unique<IO>(options.record_name.get(), IO::Mode::WriteOnly, IO::Purpose::Input);
            } catch (const IO::OpenError &) {
                streams.reset(STREAM_SCRIPT);
                warning("unable to open the script");
            }
        }
    }
    // XXX V6 has even more handling

    return number < 0 || streams.test(number);
}

void zoutput_stream()
{
    output_stream(as_signed(zargs[0]), zargs[1]);
}

// See §10.
// This returns true if the stream was successfully selected.
bool input_stream(int which)
{
    istream = which;

    if (istream == ISTREAM_KEYBOARD) {
        istreamio.reset();
    } else if (istream == ISTREAM_FILE) {
        if (istreamio == nullptr) {
            try {
                istreamio = std::make_unique<IO>(options.replay_name.get(), IO::Mode::ReadOnly, IO::Purpose::Input);
            } catch (const IO::OpenError &) {
                warning("unable to open the command script");
                istream = ISTREAM_KEYBOARD;
            }
        }
    } else {
        ZASSERT(0, "invalid input stream: %d", istream);
    }

    return istream == which;
}

void zinput_stream()
{
    input_stream(zargs[0]);
}

// This does not even pretend to understand V6 windows.
static void set_current_window(Window *window)
{
    curwin = window;

#ifdef ZTERP_GLK
    if (curwin == upperwin && upperwin->id != nullptr) {
        upperwin->x = upperwin->y = 0;
        glk_window_move_cursor(upperwin->id, 0, 0);
    }

    glk_set_window(curwin->id);
#endif

    set_current_style();
}

// Find and validate a window. If window is -3 and the story is V6,
// return the current window.
static Window *find_window(uint16_t window)
{
    int16_t w = as_signed(window);

    ZASSERT(zversion == 6 ? w == -3 || (w >= 0 && w < 8) : w == 0 || w == 1, "invalid window selected: %d", w);

    if (w == -3) {
        return curwin;
    }

    return &windows[w];
}

#ifdef ZTERP_GLK
static void perform_upper_window_resize(long new_height)
{
    glui32 actual_height;

    glk_window_set_arrangement(glk_window_get_parent(upperwin->id), winmethod_Above | winmethod_Fixed, new_height, upperwin->id);
    upper_window_height = new_height;

    // Glk might resize the window to a smaller height than was requested,
    // so track the actual height, not the requested height.
    glk_window_get_size(upperwin->id, nullptr, &actual_height);
    if (actual_height != upper_window_height) {
        // This message probably won’t be seen in a window since the upper
        // window is likely covering everything, but try anyway.
        show_message("Unable to fulfill window size request: wanted %ld, got %lu", new_height, static_cast<unsigned long>(actual_height));
        upper_window_height = actual_height;
    }
}

// When resizing the upper window, the screen’s contents should not
// change (§8.6.1); however, the way windows are handled with Glk makes
// this slightly impossible. When an Inform game tries to display
// something with “box”, it expands the upper window, displays the quote
// box, and immediately shrinks the window down again. This is a
// problem under Glk because the window immediately disappears. Other
// games, such as Bureaucracy, expect the upper window to shrink as soon
// as it has been requested. Thus the following system is used:
//
// If a request is made to shrink the upper window, it is granted
// immediately if there has been user input since the last window resize
// request. If there has not been user input, the request is delayed
// until after the next user input is read.
static long delayed_window_shrink = -1;
static bool saw_input;

static void update_delayed()
{
    if (delayed_window_shrink != -1 && upperwin->id != nullptr) {
        perform_upper_window_resize(delayed_window_shrink);
        delayed_window_shrink = -1;
    }
}

static void clear_window(Window *window)
{
    if (window->id == nullptr) {
        return;
    }

    glk_window_clear(window->id);

    window->x = window->y = 0;
}
#endif

static void resize_upper_window(long nlines, bool from_game)
{
#ifdef ZTERP_GLK
    if (upperwin->id == nullptr) {
        return;
    }

    long previous_height = upper_window_height;

    if (from_game) {
        delayed_window_shrink = nlines;
        if (upper_window_height <= nlines || saw_input) {
            update_delayed();
        }
        saw_input = false;

        // §8.6.1.1.2
        if (zversion == 3) {
            clear_window(upperwin);
        }
    } else {
        perform_upper_window_resize(nlines);
    }

    // If the window is being created, or if it’s shrinking and the cursor
    // is no longer inside the window, move the cursor to the origin.
    if (previous_height == 0 || upperwin->y >= nlines) {
        upperwin->x = upperwin->y = 0;
        if (nlines > 0) {
            glk_window_move_cursor(upperwin->id, 0, 0);
        }
    }

    // As in a few other areas, changing the upper window causes reverse
    // video to be deactivated, so reapply the current style.
    set_current_style();
#endif
}

void close_upper_window()
{
    // The upper window is never destroyed; rather, when it’s closed, it
    // shrinks to zero height.
    resize_upper_window(0, true);

#ifdef ZTERP_GLK
    delayed_window_shrink = -1;
    saw_input = false;
#endif

    set_current_window(mainwin);
}

void get_screen_size(unsigned int &width, unsigned int &height)
{
#ifdef ZTERP_GLK
    glui32 w, h;

    // The main window can be proportional, and if so, its width is not
    // generally useful because games tend to care about width with a
    // fixed font. If a status window is available, or if an upper window
    // is available, use that to calculate the width, because these
    // windows will have a fixed-width font. The height is the combined
    // height of all windows.
    glk_window_get_size(mainwin->id, &w, &h);
    height = h;
    if (statuswin.id != nullptr) {
        glk_window_get_size(statuswin.id, &w, &h);
        height += h;
    }
    if (upperwin->id != nullptr) {
        glk_window_get_size(upperwin->id, &w, &h);
        height += h;
    }
    width = w;
#else
    std::tie(width, height) = zterp_os_get_screen_size();
#endif

    // XGlk does not report the size of textbuffer windows, and
    // zterp_os_get_screen_size() may not be able to get the screen
    // size, so use reasonable defaults in those cases.
    if (width == 0) {
        width = 80;
    }
    if (height == 0) {
        height = 24;
    }

    // Terrible hack: Because V6 is not properly supported, the window to
    // which Journey writes its story is completely covered up by window
    // 1. For the same reason, only the bottom 6 lines of window 1 are
    // actually useful, even though the game expands it to cover the whole
    // screen. By pretending that the screen height is only 6, the main
    // window, where text is actually sent, becomes visible.
    if (is_game(Game::Journey) && height > 6) {
        height = 6;
    }
}

#ifdef ZTERP_GLK
#ifdef GLK_MODULE_LINE_TERMINATORS
std::vector<uint32_t> term_keys;
#endif
static bool term_mouse = false;

void term_keys_reset()
{
#ifdef GLK_MODULE_LINE_TERMINATORS
    term_keys.clear();
#endif
    term_mouse = false;
}

static void insert_key(uint32_t key)
{
#ifdef GLK_MODULE_LINE_TERMINATORS
    term_keys.push_back(key);
#endif
}

void term_keys_add(uint8_t key)
{
    switch (key) {
    case ZSCII_UP:    insert_key(keycode_Up); break;
    case ZSCII_DOWN:  insert_key(keycode_Down); break;
    case ZSCII_LEFT:  insert_key(keycode_Left); break;
    case ZSCII_RIGHT: insert_key(keycode_Right); break;
    case ZSCII_F1:    insert_key(keycode_Func1); break;
    case ZSCII_F2:    insert_key(keycode_Func2); break;
    case ZSCII_F3:    insert_key(keycode_Func3); break;
    case ZSCII_F4:    insert_key(keycode_Func4); break;
    case ZSCII_F5:    insert_key(keycode_Func5); break;
    case ZSCII_F6:    insert_key(keycode_Func6); break;
    case ZSCII_F7:    insert_key(keycode_Func7); break;
    case ZSCII_F8:    insert_key(keycode_Func8); break;
    case ZSCII_F9:    insert_key(keycode_Func9); break;
    case ZSCII_F10:   insert_key(keycode_Func10); break;
    case ZSCII_F11:   insert_key(keycode_Func11); break;
    case ZSCII_F12:   insert_key(keycode_Func12); break;

    // Keypad 0–9 should be here, but Glk doesn’t support that.
    case ZSCII_KEY0: case ZSCII_KEY1: case ZSCII_KEY2: case ZSCII_KEY3:
    case ZSCII_KEY4: case ZSCII_KEY5: case ZSCII_KEY6: case ZSCII_KEY7:
    case ZSCII_KEY8: case ZSCII_KEY9:
        break;

    case ZSCII_CLICK_SINGLE:
        term_mouse = true;
        break;

    case ZSCII_CLICK_MENU: case ZSCII_CLICK_DOUBLE:
        break;

    case 255:
        for (int i = 129; i <= 154; i++) {
            term_keys_add(i);
        }
        for (int i = 252; i <= 254; i++) {
            term_keys_add(i);
        }
        break;

    default:
        break;
    }
}

// Look in the terminating characters table (if any) and reset the
// current state of terminating characters appropriately. Then enable
// terminating characters for Glk line input on the specified window. If
// there are no terminating characters, disable the use of terminating
// characters for Glk input.
static void check_terminators(Window *window)
{
    if (header.terminating_characters_table != 0) {
        term_keys_reset();

        for (uint32_t addr = header.terminating_characters_table; user_byte(addr) != 0; addr++) {
            term_keys_add(user_byte(addr));
        }

#ifdef GLK_MODULE_LINE_TERMINATORS
        if (glk_gestalt(gestalt_LineTerminators, 0)) {
            glk_set_terminators_line_event(window->id, &term_keys[0], term_keys.size());
        }
    } else {
        if (glk_gestalt(gestalt_LineTerminators, 0)) {
            glk_set_terminators_line_event(window->id, nullptr, 0);
        }
#endif
    }
}
#endif

// Decode and print a zcode string at address “addr”. This can be
// called recursively thanks to abbreviations; the initial call should
// have “in_abbr” set to false.
// Each time a character is decoded, it is passed to the function
// “outc”.
static int print_zcode(uint32_t addr, bool in_abbr, void (*outc)(uint8_t))
{
    enum class TenBit { None, Start, Half } tenbit = TenBit::None;
    int abbrev = 0, shift = 0;
    int c, lastc = 0; // initialize to appease gcc
    uint16_t w;
    uint32_t counter = addr;
    int current_alphabet = 0;

    do {
        ZASSERT(counter < memory_size - 1, "string runs beyond the end of memory");

        w = word(counter);

        for (int i : {10, 5, 0}) {
            c = (w >> i) & 0x1f;

            if (tenbit == TenBit::Start) {
                lastc = c;
                tenbit = TenBit::Half;
            } else if (tenbit == TenBit::Half) {
                outc((lastc << 5) | c);
                tenbit = TenBit::None;
            } else if (abbrev != 0) {
                uint32_t new_addr;

                new_addr = user_word(header.abbr + 64 * (abbrev - 1) + 2 * c);

                // new_addr is a word address, so multiply by 2
                print_zcode(new_addr * 2, true, outc);

                abbrev = 0;
            } else {
                switch (c) {
                case 0:
                    outc(ZSCII_SPACE);
                    shift = 0;
                    break;
                case 1:
                    if (zversion == 1) {
                        outc(ZSCII_NEWLINE);
                        shift = 0;
                        break;
                    }
                    // fallthrough
                case 2: case 3:
                    if (zversion >= 3 || (zversion == 2 && c == 1)) {
                        ZASSERT(!in_abbr, "abbreviation being used recursively");
                        abbrev = c;
                        shift = 0;
                    } else {
                        shift = c - 1;
                    }
                    break;
                case 4: case 5:
                    if (zversion <= 2) {
                        current_alphabet = (current_alphabet + (c - 3)) % 3;
                        shift = 0;
                    } else {
                        shift = c - 3;
                    }
                    break;
                case 6:
                    if (zversion <= 2) {
                        shift = (current_alphabet + shift) % 3;
                    }

                    if (shift == 2) {
                        shift = 0;
                        tenbit = TenBit::Start;
                        break;
                    }
                    // fallthrough
                default:
                    if (zversion <= 2 && c != 6) {
                        shift = (current_alphabet + shift) % 3;
                    }

                    outc(atable[(26 * shift) + (c - 6)]);
                    shift = 0;
                    break;
                }
            }
        }

        counter += 2;
    } while ((w & 0x8000) == 0);

    return counter - addr;
}

// Prints the string at addr “addr”.
//
// Returns the number of bytes the string took up. “outc” is passed as
// the character-print function to print_zcode(); if it is nullptr,
// put_char is used.
int print_handler(uint32_t addr, void (*outc)(uint8_t))
{
    return print_zcode(addr, false, outc != nullptr ? outc : put_char);
}

void zprint()
{
    pc += print_handler(pc, nullptr);
}

void znew_line()
{
    put_char(ZSCII_NEWLINE);
}

void zprint_ret()
{
    zprint();
    znew_line();
    zrtrue();
}

void zerase_window()
{
#ifdef ZTERP_GLK
    switch (as_signed(zargs[0])) {
    case -2:
        for (auto &window : windows) {
            clear_window(&window);
        }
        break;
    case -1:
        close_upper_window();
        // fallthrough
    case 0:
        // 8.7.3.2.1 says V5+ should have the cursor set to 1, 1 of the
        // erased window; V4 the lower window goes bottom left, the upper
        // to 1, 1. Glk doesn’t give control over the cursor when
        // clearing, and that doesn’t really seem to be an issue; so just
        // call glk_window_clear().
        clear_window(mainwin);
        break;
    case 1:
        clear_window(upperwin);
        break;
    default:
        break;
    }

    // glk_window_clear() kills reverse video in Gargoyle. Reapply style.
    set_current_style();
#endif
}

void zerase_line()
{
#ifdef ZTERP_GLK
    // XXX V6 does pixel handling here.
    if (zargs[0] != 1 || curwin != upperwin || upperwin->id == nullptr) {
        return;
    }

    for (long i = upperwin->x; i < upper_window_width; i++) {
        xglk_put_char(UNICODE_SPACE);
    }

    glk_window_move_cursor(upperwin->id, upperwin->x, upperwin->y);
#endif
}

// XXX This is more complex in V6 and needs to be updated when V6 windowing is implemented.
static void set_cursor(uint16_t y, uint16_t x)
{
#ifdef ZTERP_GLK
    // All the windows in V6 can have their cursor positioned; if V6 ever
    // comes about this should be fixed.
    if (curwin != upperwin) {
        return;
    }

    // -1 and -2 are V6 only, but at least Zracer passes -1 (or it’s
    // trying to position the cursor to line 65535; unlikely!)
    if (as_signed(y) == -1 || as_signed(y) == -2) {
        return;
    }

    // §8.7.2.3 says 1,1 is the top-left, but at least one program (Paint
    // and Corners) uses @set_cursor 0 0 to go to the top-left; so
    // special-case it.
    if (y == 0) {
        y = 1;
    }

    // This handles 0, but also takes care of working around a bug in Inform’s
    // “box" statement, which causes “x” to be negative if the box’s text is
    // wider than the screen.
    if (as_signed(x) < 1) {
        x = 1;
    }

    // This is actually illegal, but some games (e.g. Beyond Zork) expect it to work.
    if (y > upper_window_height) {
        resize_upper_window(y, true);
    }

    if (upperwin->id != nullptr) {
        upperwin->x = x - 1;
        upperwin->y = y - 1;

        glk_window_move_cursor(upperwin->id, x - 1, y - 1);
    }
#endif
}

void zset_cursor()
{
    set_cursor(zargs[0], zargs[1]);
}

void zget_cursor()
{
#ifdef ZTERP_GLK
    user_store_word(zargs[0] + 0, upperwin->y + 1);
    user_store_word(zargs[0] + 2, upperwin->x + 1);
#else
    user_store_word(zargs[0] + 0, 1);
    user_store_word(zargs[0] + 2, 1);
#endif
}

static bool prepare_color_opcode(int16_t &fg, int16_t &bg, Window *&win)
{
    // Glk (apart from Gargoyle) has no color support.
#if !defined(ZTERP_GLK) || defined(GLK_MODULE_GARGLKTEXT)
    if (options.disable_color) {
        return false;
    }

    fg = as_signed(zargs[0]);
    bg = as_signed(zargs[1]);
    win = znargs == 3 ? find_window(zargs[2]) : style_window();

    return true;
#else
    return false;
#endif
}

void zset_colour()
{
    int16_t fg, bg;
    Window *win;

    if (prepare_color_opcode(fg, bg, win)) {
        // XXX -1 is a valid color in V6.
        if (fg >= 1 && fg <= 12) {
            win->fg_color = Color(Color::Mode::ANSI, fg);
        }
        if (bg >= 1 && bg <= 12) {
            win->bg_color = Color(Color::Mode::ANSI, bg);
        }

        if (win == mainwin) {
            history.add_fg_color(win->fg_color);
            history.add_bg_color(win->bg_color);
        }

        set_current_style();
    }
}

void zset_true_colour()
{
    int16_t fg, bg;
    Window *win;

    if (prepare_color_opcode(fg, bg, win)) {
        if (fg >= 0) {
            win->fg_color = Color(Color::Mode::True, fg);
        } else if (fg == -1) {
            win->fg_color = Color();
        }

        if (bg >= 0) {
            win->bg_color = Color(Color::Mode::True, bg);
        } else if (bg == -1) {
            win->bg_color = Color();
        }

        if (win == mainwin) {
            history.add_fg_color(win->fg_color);
            history.add_bg_color(win->bg_color);
        }

        set_current_style();
    }
}

// V6 has per-window styles, but all others have a global style; in this
// case, track styles via the main window.
void zset_text_style()
{
    // A style of 0 means all others go off.
    if (zargs[0] == 0) {
        style_window()->style.reset();
    } else if (zargs[0] < 16) {
        style_window()->style |= zargs[0];
    }

    if (style_window() == mainwin) {
        history.add_style();
    }

    set_current_style();
}

static bool is_valid_font(Window::Font font)
{
    return font == Window::Font::Normal ||
          (font == Window::Font::Character && !options.disable_graphics_font) ||
          (font == Window::Font::Fixed     && !options.disable_fixed);
}

void zset_font()
{
    Window *win = curwin;

    if (zversion == 6 && znargs == 2 && as_signed(zargs[1]) != -3) {
        ZASSERT(zargs[1] < 8, "invalid window selected: %d", as_signed(zargs[1]));
        win = &windows[zargs[1]];
    }

    if (static_cast<Window::Font>(zargs[0]) == Window::Font::Query) {
        store(static_cast<uint16_t>(win->font));
    } else if (is_valid_font(static_cast<Window::Font>(zargs[0]))) {
        store(static_cast<uint16_t>(win->font));
        win->font = static_cast<Window::Font>(zargs[0]);
        set_current_style();
        if (win == mainwin) {
            history.add_style();
        }
    } else {
        store(0);
    }
}

void zprint_table()
{
    uint16_t text = zargs[0], width = zargs[1], height = zargs[2], skip = zargs[3];
    uint16_t n = 0;

#ifdef ZTERP_GLK
    uint16_t start = 0; // initialize to appease gcc

    if (curwin == upperwin) {
        start = upperwin->x + 1;
    }
#endif

    if (znargs < 3) {
        height = 1;
    }
    if (znargs < 4) {
        skip = 0;
    }

    for (uint16_t i = 0; i < height; i++) {
        for (uint16_t j = 0; j < width; j++) {
            put_char(user_byte(text + n++));
        }

        if (i + 1 != height) {
            n += skip;
#ifdef ZTERP_GLK
            if (curwin == upperwin) {
                set_cursor(upperwin->y + 2, start);
            } else
#endif
            {
                put_char(ZSCII_NEWLINE);
            }
        }
    }
}

void zprint_char()
{
    // Check 32 (space) first: a cursory examination of story files
    // indicates that this is the most common value passed to @print_char.
    // This appears to be due to V4+ games blanking the upper window.
#define valid_zscii_output(c)	((c) == 32 || (c) == 0 || (c) == 9 || (c) == 11 || (c) == 13 || ((c) > 32 && (c) <= 126) || ((c) >= 155 && (c) <= 251))
    ZASSERT(valid_zscii_output(zargs[0]), "@print_char called with invalid character: %u", static_cast<unsigned int>(zargs[0]));
#undef valid_zscii_output

    put_char(zargs[0]);
}

void zprint_num()
{
    std::stringstream ss;

    ss << as_signed(zargs[0]);
    for (const auto &c : ss.str()) {
        put_char(c);
    }
}

void zprint_addr()
{
    print_handler(zargs[0], nullptr);
}

void zprint_paddr()
{
    print_handler(unpack_string(zargs[0]), nullptr);
}

// XXX This is more complex in V6 and needs to be updated when V6 windowing is implemented.
void zsplit_window()
{
    if (zargs[0] == 0) {
        close_upper_window();
    } else {
        resize_upper_window(zargs[0], true);
    }
}

void zset_window()
{
    set_current_window(find_window(zargs[0]));
}

#ifdef ZTERP_GLK
static void window_change()
{
    // When a textgrid (the upper window) in Gargoyle is rearranged, it
    // forgets about reverse video settings, so reapply any styles to the
    // current window (it doesn’t hurt if the window is a textbuffer). If
    // the current window is not the upper window that’s OK, because
    // set_current_style() is called when a @set_window is requested.
    set_current_style();

    // Track the new size of the upper window.
    if (zversion >= 3 && upperwin->id != nullptr) {
        glui32 w, h;

        glk_window_get_size(upperwin->id, &w, &h);

        upper_window_width = w;

        resize_upper_window(h, false);
    }

    // §8.4
    // Only 0x20 and 0x21 are mentioned; what of 0x22 and 0x24? Zoom and
    // Windows Frotz both update the V5 header entries, so do that here,
    // too.
    //
    // Also, no version restrictions are given, but assume V4+ per §11.1.
    if (zversion >= 4) {
        unsigned width, height;

        get_screen_size(width, height);

        store_byte(0x20, height > 254 ? 254 : height);
        store_byte(0x21, width > 255 ? 255 : width);

        if (zversion >= 5) {
            store_word(0x22, width > UINT16_MAX ? UINT16_MAX : width);
            store_word(0x24, height > UINT16_MAX ? UINT16_MAX : height);
        }
    } else {
        zshow_status();
    }
}
#endif

#ifdef ZTERP_GLK
static bool timer_running;

static void start_timer(uint16_t n)
{
    if (!timer_available()) {
        return;
    }

    if (timer_running) {
        die("nested timers unsupported");
    }
    glk_request_timer_events(n * 100);
    timer_running = true;
}

static void stop_timer()
{
    if (!timer_available()) {
        return;
    }

    glk_request_timer_events(0);
    timer_running = false;
}

static void request_char()
{
    if (have_unicode) {
        glk_request_char_event_uni(curwin->id);
    } else {
        glk_request_char_event(curwin->id);
    }
}

static void request_line(Line &line, glui32 maxlen)
{
    check_terminators(curwin);

    if (have_unicode) {
        glk_request_line_event_uni(curwin->id, &line.unicode[0], maxlen, line.len);
    } else {
        glk_request_line_event(curwin->id, &line.latin1[0], maxlen, line.len);
    }
}

// If an interrupt is called, cancel any pending read events. They will
// be restarted after the interrupt returns. If this was a line input
// event, “line” will be updated with the length of the input that had
// been entered at the time of cancellation.
static void cancel_read_events(Window *window, Line &line)
{
    event_t ev;

    glk_cancel_char_event(window->id);
    glk_cancel_line_event(window->id, &ev);
    if (upperwin->id != nullptr) {
        glk_cancel_mouse_event(upperwin->id);
    }

    // If the pending read was a line input, set the line length to the
    // amount read before cancellation (this will already have been
    // stored in the line struct by Glk).
    if (ev.type == evtype_LineInput) {
        line.len = ev.val1;
    }
}

// This is a wrapper around internal_call() which handles screen-related
// issues: input events are canceled (in case the interrupt routine
// calls input functions) and the window is restored if the interrupt
// changes it. The value from the internal call is returned. This does
// not restart the canceled read events.
static uint16_t handle_interrupt(uint16_t addr, Line *line)
{
    Window *saved = curwin;
    uint16_t ret;

    if (line != nullptr) {
        cancel_read_events(curwin, *line);
    }

    ret = internal_call(addr);

    // It’s possible for an interrupt to switch windows; if it does,
    // simply switch back. This is the easiest way to deal with an
    // undefined bit of the Z-machine.
    if (curwin != saved) {
        set_current_window(saved);
    }

    return ret;
}

// All read events are canceled during an interrupt, but after an
// interrupt returns, it’s possible that the read should be restarted,
// so this function does that.
static void restart_read_events(Line &line, const Input &input, bool enable_mouse)
{
    switch (input.type) {
    case Input::Type::Char:
        request_char();
        break;
    case Input::Type::Line:
        request_line(line, input.maxlen);
        break;
    }

    if (enable_mouse) {
        glk_request_mouse_event(upperwin->id);
    }
}
#endif

#ifdef ZTERP_GLK
void screen_flush()
{
    event_t ev;

    glk_select_poll(&ev);
    switch (ev.type) {
    case evtype_None:
        break;
    case evtype_Arrange:
        window_change();
        break;
#ifdef GLK_MODULE_SOUND
    case evtype_SoundNotify:
        if (sound_routine != 0) {
            uint16_t current_routine = sound_routine;
            sound_routine = 0;
            handle_interrupt(current_routine, nullptr);
        }
        sound_stopped();
        break;
#endif
    default:
        // No other events should arrive. Timers are only started in
        // get_input() and are stopped before that function returns.
        // Input events will not happen with glk_select_poll(), and no
        // other event type is expected to be raised.
        break;
    }
}
#else
void screen_flush()
{
}
#endif

#define special_zscii(c) ((c) >= 129 && (c) <= 154)

// This is called when input stream 1 (read from file) is selected. If
// it succefully reads a character/line from the file, it fills the
// struct at “input” with the appropriate information and returns true.
// If it fails to read (likely due to EOF) then it sets the input stream
// back to the keyboard and returns false.
static bool istream_read_from_file(Input &input)
{
    if (input.type == Input::Type::Char) {
        long c;

        // If there are carriage returns in the input, this is almost
        // certainly a command script from a Windows system being run on a
        // non-Windows system, so ignore them.
        do {
            c = istreamio->getc(true);
        } while (c == UNICODE_CARRIAGE_RETURN);

        if (c == -1) {
            input_stream(ISTREAM_KEYBOARD);
            return false;
        }

        // Don’t translate special ZSCII characters (cursor keys, function keys, keypad).
        if (special_zscii(c)) {
            input.key = c;
        } else {
            input.key = unicode_to_zscii_q[c];
        }
    } else {
        std::vector<uint16_t> line;

        try {
            line = istreamio->readline();
        } catch (const IO::EndOfFile &) {
            input_stream(ISTREAM_KEYBOARD);
            return false;
        }

        // As above, ignore carriage returns.
        if (!line.empty() && line.back() == UNICODE_CARRIAGE_RETURN) {
            line.pop_back();
        }

        if (line.size() > input.maxlen) {
            line.resize(input.maxlen);
        }

        input.len = line.size();

#ifdef ZTERP_GLK
        if (curwin->id != nullptr) {
            glk_set_style(style_Input);
            for (const auto &c : line) {
                xglk_put_char(c);
            }
            xglk_put_char(UNICODE_LINEFEED);
            set_current_style();
        }
#else
        for (const auto &c : line) {
            IO::standard_out().putc(c);
        }
        IO::standard_out().putc(UNICODE_LINEFEED);
#endif

        std::copy(line.begin(), line.end(), input.line.begin());
    }

#ifdef ZTERP_GLK
    // It’s possible that output is buffered, meaning that until
    // glk_select() is called, output will not be displayed. When reading
    // from a command-script, flush on each command so that output is
    // visible while the script is being replayed.
    screen_flush();

    saw_input = true;
#endif

    return true;
}

#ifdef GLK_MODULE_LINE_TERMINATORS
// Glk returns terminating characters as keycode_*, but we need them as
// ZSCII. This should only ever be called with values that are matched
// in the switch, because those are the only ones that Glk was told are
// terminating characters. In the event that another keycode comes
// through, though, treat it as Enter.
static uint8_t zscii_from_glk(glui32 key)
{
    switch (key) {
    case 13:             return ZSCII_NEWLINE;
    case keycode_Up:     return ZSCII_UP;
    case keycode_Down:   return ZSCII_DOWN;
    case keycode_Left:   return ZSCII_LEFT;
    case keycode_Right:  return ZSCII_RIGHT;
    case keycode_Func1:  return ZSCII_F1;
    case keycode_Func2:  return ZSCII_F2;
    case keycode_Func3:  return ZSCII_F3;
    case keycode_Func4:  return ZSCII_F4;
    case keycode_Func5:  return ZSCII_F5;
    case keycode_Func6:  return ZSCII_F6;
    case keycode_Func7:  return ZSCII_F7;
    case keycode_Func8:  return ZSCII_F8;
    case keycode_Func9:  return ZSCII_F9;
    case keycode_Func10: return ZSCII_F10;
    case keycode_Func11: return ZSCII_F11;
    case keycode_Func12: return ZSCII_F12;
    }

    return ZSCII_NEWLINE;
}
#endif

// Attempt to read input from the user. The input type can be either a
// single character or a full line. If “timer” is not zero, a timer is
// started that fires off every “timer” tenths of a second (if the value
// is 1, it will timeout 10 times a second, etc.). Each time the timer
// times out the routine at address “routine” is called. If the routine
// returns true, the input is canceled.
//
// The function returns true if input was stored, false if there was a
// cancellation as described above.
static bool get_input(uint16_t timer, uint16_t routine, Input &input)
{
    // If either of these is zero, no timeout should happen.
    if (timer == 0) {
        routine = 0;
    }
    if (routine == 0) {
        timer = 0;
    }

    // Flush all streams when input is requested.
#ifndef ZTERP_GLK
    IO::standard_out().flush();
#endif
    if (scriptio != nullptr) {
        scriptio->flush();
    }
    if (transio != nullptr) {
        transio->flush();
    }
    if (perstransio != nullptr) {
        perstransio->flush();
    }

    // Generally speaking, newline will be the reason the line input
    // stopped, so set it by default. It will be overridden where
    // necessary.
    input.term = ZSCII_NEWLINE;

    if (istream == ISTREAM_FILE && istream_read_from_file(input)) {
#ifdef ZTERP_GLK
        saw_input = true;
#endif
        return true;
    }
#ifdef ZTERP_GLK
    enum class InputStatus { Waiting, Received, Canceled } status = InputStatus::Waiting;
    Line line;
    Window *saved = nullptr;
    // Mouse support is turned on if the game requests it via Flags2
    // and, for @read, if it adds single click to the list of
    // terminating keys; in addition, an upper window must be available
    // since that’s the only window we use to handle mouse events.
    bool enable_mouse = mouse_available() &&
                        ((input.type == Input::Type::Char) ||
                         (input.type == Input::Type::Line && term_mouse)) &&
                        upperwin->id != nullptr;

    // In V6, input might be requested on an unsupported window. If so,
    // switch to the main window temporarily.
    if (curwin->id == nullptr) {
        saved = curwin;
        curwin = mainwin;
        glk_set_window(curwin->id);
    }

    if (enable_mouse) {
        glk_request_mouse_event(upperwin->id);
    }

    switch (input.type) {
    case Input::Type::Char:
        request_char();
        break;
    case Input::Type::Line:
        line.len = input.preloaded;
        for (int i = 0; i < input.preloaded; i++) {
            if (have_unicode) {
                line.unicode[i] = input.line[i];
            } else {
                line.latin1 [i] = input.line[i];
            }
        }

        request_line(line, input.maxlen);
        break;
    }

    if (timer != 0) {
        start_timer(timer);
    }

    while (status == InputStatus::Waiting) {
        event_t ev;

        glk_select(&ev);

        switch (ev.type) {
        case evtype_Arrange:
            window_change();
            break;

        case evtype_Timer:
            ZASSERT(timer != 0, "got unexpected evtype_Timer");

            stop_timer();

            if (handle_interrupt(routine, &line) != 0) {
                status = InputStatus::Canceled;
                input.term = 0;
            } else {
                restart_read_events(line, input, enable_mouse);
                start_timer(timer);
            }

            break;

        case evtype_CharInput:
            ZASSERT(input.type == Input::Type::Char, "got unexpected evtype_CharInput");
            ZASSERT(ev.win == curwin->id, "got evtype_CharInput on unexpected window");

            status = InputStatus::Received;

            // As far as the Standard is concerned, there is no difference in
            // behavior for @read_char between versions 4 and 5, as §10.7.1 claims
            // that all characters defined for input can be returned by @read_char.
            // However, according to Infocom’s EZIP documentation, “keys which do
            // not have a single ASCII value are ignored,” with the exception that
            // up, down, left, and right map to 14, 13, 11, and 7, respectively.
            // From looking at V4 games’ source code, it seems like only Bureaucracy
            // makes use of these codes, and then only partially. Since the down
            // arrow maps to 13, it is indistinguishable from ENTER. Bureaucracy
            // treats 14 like ^, meaning “back up a field”. The other arrows aren’t
            // handled specifically, but what Bureaucracy does with all characters
            // it doesn’t handle specifically is print them out. In the DOS version
            // of Bureaucracy, hitting the left arrow inserts a mars symbol (which
            // is what code page 437 uses to represent value 11). Hitting the right
            // arrow results in a beep since 7 is the bell in ASCII. Up and down
            // work as expected, but left and right appear to have just been ignored
            // and untested. On the Apple II, up and down work, but left and right
            // cause nothing to be printed. However, Bureaucracy does advance its
            // cursor location when the arrows are hit (as it does with every
            // character), which causes interesting behavior: hit the right arrow a
            // couple of times and then hit backspace: the cursor will jump to the
            // right first due to the fact that the cursor position was advanced
            // without anything actually being printed. This can’t be anything but a
            // bug.
            //
            // At any rate, given how haphazard the arrow keys act under V4, they
            // are just completely ignored here. This does no harm for Bureaucracy,
            // since ENTER and ^ can be used. In fact, all input which is not also
            // defined for output (save for BACKSPACE, given that Bureaucracy
            // properly handles it) is ignored. As a result, Bureaucracy will not
            // accidentally print out invalid values (since only valid values will
            // be read). This violates both the Standard (since that allows all
            // values defined for input) and the EZIP documentation (since that
            // mandates ASCII only), but hopefully provides the best experience
            // overall: Unicode values are allowed, but Bureaucracy no longer tries
            // to print out invalid characters.
            //
            // Note that if function keys are supported in V4, Bureaucracy will pass
            // them along to @print_char, even though they’re not valid for output,
            // so ignoring things provides better results than following the
            // Standard.
            if (zversion == 4) {
                switch (ev.val1) {
                case keycode_Delete: input.key = ZSCII_DELETE; break;
                case keycode_Return: input.key = ZSCII_NEWLINE; break;

                default:
                    if (ev.val1 >= 32 && ev.val1 <= 126) {
                        input.key = ev.val1;
                    } else if (ev.val1 < UINT16_MAX) {
                        uint8_t c = unicode_to_zscii[ev.val1];

                        if (c != 0) {
                            input.key = c;
                        }
                    } else {
                        status = InputStatus::Waiting;
                        request_char();
                    }
                }
            } else {
                switch (ev.val1) {
                case keycode_Delete: input.key = ZSCII_DELETE; break;
                case keycode_Return: input.key = ZSCII_NEWLINE; break;
                case keycode_Escape: input.key = ZSCII_ESCAPE; break;
                case keycode_Up:     input.key = ZSCII_UP; break;
                case keycode_Down:   input.key = ZSCII_DOWN; break;
                case keycode_Left:   input.key = ZSCII_LEFT; break;
                case keycode_Right:  input.key = ZSCII_RIGHT; break;
                case keycode_Func1:  input.key = ZSCII_F1; break;
                case keycode_Func2:  input.key = ZSCII_F2; break;
                case keycode_Func3:  input.key = ZSCII_F3; break;
                case keycode_Func4:  input.key = ZSCII_F4; break;
                case keycode_Func5:  input.key = ZSCII_F5; break;
                case keycode_Func6:  input.key = ZSCII_F6; break;
                case keycode_Func7:  input.key = ZSCII_F7; break;
                case keycode_Func8:  input.key = ZSCII_F8; break;
                case keycode_Func9:  input.key = ZSCII_F9; break;
                case keycode_Func10: input.key = ZSCII_F10; break;
                case keycode_Func11: input.key = ZSCII_F11; break;
                case keycode_Func12: input.key = ZSCII_F12; break;

                default:
                    input.key = ZSCII_QUESTIONMARK;

                    if (ev.val1 <= UINT16_MAX) {
                        uint8_t c = unicode_to_zscii[ev.val1];

                        if (c != 0) {
                            input.key = c;
                        }
                    }

                    break;
                }
            }

            break;

        case evtype_LineInput:
            ZASSERT(input.type == Input::Type::Line, "got unexpected evtype_LineInput");
            ZASSERT(ev.win == curwin->id, "got evtype_LineInput on unexpected window");
            line.len = ev.val1;
#ifdef GLK_MODULE_LINE_TERMINATORS
            if (zversion >= 5) {
                input.term = zscii_from_glk(ev.val2);
            }
#endif
            status = InputStatus::Received;
            break;
        case evtype_MouseInput:
            zterp_mouse_click(ev.val1 + 1, ev.val2 + 1);
            status = InputStatus::Received;

            switch (input.type) {
            case Input::Type::Char:
                input.key = ZSCII_CLICK_SINGLE;
                break;
            case Input::Type::Line:
                glk_cancel_line_event(curwin->id, &ev);
                input.len = ev.val1;
                input.term = ZSCII_CLICK_SINGLE;
                break;
            }

            break;
#ifdef GLK_MODULE_SOUND
        case evtype_SoundNotify:
            if (sound_routine != 0) {
                uint16_t current_routine = sound_routine;
                sound_routine = 0;
                handle_interrupt(current_routine, &line);
                restart_read_events(line, input, enable_mouse);
            }
            sound_stopped();
            break;
#endif
        }
    }

    stop_timer();

    if (enable_mouse) {
        glk_cancel_mouse_event(upperwin->id);
    }

    switch (input.type) {
    case Input::Type::Char:
        glk_cancel_char_event(curwin->id);
        break;
    case Input::Type::Line:
        // Copy the Glk line into the internal input structure.
        input.len = line.len;
        for (glui32 i = 0; i < line.len; i++) {
            if (have_unicode) {
                input.line[i] = line.unicode[i] > UINT16_MAX ? UNICODE_REPLACEMENT : line.unicode[i];
            } else {
                input.line[i] = static_cast<unsigned char>(line.latin1[i]);
            }
        }

        // When line input echoing is turned off (which is the case on
        // Glk implementations that support it), input won’t be echoed
        // to the screen after it’s been entered. This will echo it
        // where appropriate, for both canceled and completed input.
        if (curwin->has_echo) {
            glk_set_style(style_Input);
            for (glui32 i = 0; i < input.len; i++) {
                xglk_put_char(input.line[i]);
            }

            if (input.term == ZSCII_NEWLINE) {
                xglk_put_char(UNICODE_LINEFEED);
            }
            set_current_style();
        }

        // If the current window is the upper window, the position of
        // the cursor needs to be tracked, so after a line has
        // successfully been read, advance the cursor to the initial
        // position of the next line, or if a terminating key was used
        // or input was canceled, to the end of the input.
        if (curwin == upperwin) {
            if (input.term != ZSCII_NEWLINE) {
                upperwin->x += input.len;
            }

            if (input.term == ZSCII_NEWLINE || upperwin->x >= upper_window_width) {
                upperwin->x = 0;
                if (upperwin->y < upper_window_height) {
                    upperwin->y++;
                }
            }

            glk_window_move_cursor(upperwin->id, upperwin->x, upperwin->y);
        }
    }

    saw_input = true;

    if (errorwin != nullptr) {
        glk_window_close(errorwin, nullptr);
        errorwin = nullptr;
    }

    if (saved != nullptr) {
        curwin = saved;
        glk_set_window(curwin->id);
    }

    return status != InputStatus::Canceled;
#else
    switch (input.type) {
    case Input::Type::Char: {
        std::vector<uint16_t> line;

        try {
            line = IO::standard_in().readline();
        } catch (const IO::EndOfFile &) {
            zquit();
        }

        if (line.empty()) {
            input.key = ZSCII_NEWLINE;
        } else if (line[0] == UNICODE_DELETE) {
            input.key = ZSCII_DELETE;
        } else if (line[0] == UNICODE_ESCAPE) {
            input.key = ZSCII_ESCAPE;
        } else {
            input.key = unicode_to_zscii[line[0]];
            if (input.key == 0) {
                input.key = ZSCII_NEWLINE;
            }
        }
        break;
    }
    case Input::Type::Line:
        input.len = input.preloaded;

        if (input.maxlen > input.preloaded) {
            std::vector<uint16_t> line;

            try {
                line = IO::standard_in().readline();
            } catch (const IO::EndOfFile &) {
                zquit();
            }

            if (line.size() > input.maxlen - input.preloaded) {
                line.resize(input.maxlen - input.preloaded);
            }

            std::copy(line.begin(), line.end(), &input.line[input.preloaded]);
            input.len += line.size();
        }
        break;
    }

    return true;
#endif
}

void zread_char()
{
    uint16_t timer = 0;
    uint16_t routine = zargs[2];
    Input input;

    input.type = Input::Type::Char;

    if (options.autosave && !in_interrupt()) {
        do_save(SaveType::Autosave, SaveOpcode::ReadChar);
    }

    if (zversion >= 4 && znargs > 1) {
        timer = zargs[1];
    }

    if (!get_input(timer, routine, input)) {
        store(0);
        return;
    }

#ifdef ZTERP_GLK
    update_delayed();
#endif

    if (streams.test(STREAM_SCRIPT)) {
        // Values 127 to 159 are not valid Unicode, and these just happen to
        // match up to the values needed for special ZSCII keys, so store
        // them as-is.
        if (special_zscii(input.key)) {
            scriptio->putc(input.key);
        } else {
            scriptio->putc(zscii_to_unicode[input.key]);
        }
    }

    store(input.key);
}
#undef special_zscii

#ifdef ZTERP_GLK
static void status_putc(uint8_t c)
{
    xglk_put_char_stream(glk_window_get_stream(statuswin.id), zscii_to_unicode[c]);
}
#endif

// §8.2.3.2 says the hours can be assumed to be in the range [0, 23] and
// can be reduced modulo 12 for 12-hour time. Cutthroats, however, sets
// the hours to 111 if the watch is dropped, on the assumption that the
// interpreter will convert 24-hour time to 12-hour time simply by
// subtracting 12, resulting in an hours of 99 (the minutes are set to
// 99 for a final time of 99:99). Perform the calculation as in
// Cutthroats, which results in valid times for [0, 23] as well as
// allowing the 99:99 hack to work.
std::string screen_format_time(long hours, long minutes)
{
    return fstring("Time: %ld:%02ld%s ", hours <= 12 ? (hours + 11) % 12 + 1 : hours - 12, minutes, hours < 12 ? "am" : "pm");
}

void zshow_status()
{
#ifdef ZTERP_GLK
    glui32 width, height;
    std::string rhs;
    long first = as_signed(variable(0x11)), second = as_signed(variable(0x12));
    strid_t stream;

    if (statuswin.id == nullptr) {
        return;
    }

    stream = glk_window_get_stream(statuswin.id);

    glk_window_clear(statuswin.id);

    glk_window_get_size(statuswin.id, &width, &height);

#ifdef GLK_MODULE_GARGLKTEXT
    garglk_set_reversevideo_stream(stream, 1);
#else
    glk_set_style_stream(stream, style_Alert);
#endif
    for (glui32 i = 0; i < width; i++) {
        xglk_put_char_stream(stream, UNICODE_SPACE);
    }

    glk_window_move_cursor(statuswin.id, 1, 0);

    // Variable 0x10 is global variable 1.
    print_object(variable(0x10), status_putc);

    if (status_is_time()) {
        rhs = screen_format_time(first, second);
        if (rhs.size() > width) {
            rhs = fstring("%02ld:%02ld", first, second);
        }
    } else {
        // Planetfall and Stationfall are score games, except the value
        // in the second global variable is the current Galactic
        // Standard Time, not the number of moves. Most of Infocom’s
        // interpreters displayed the score and moves as “score/moves”
        // so it didn’t look flat-out wrong to have a value that wasn’t
        // actually the number of moves. When it’s spelled out as
        // “Moves”, though, then the display is obviously wrong. For
        // these two games, rewrite “Moves” as “Time”. Note that this is
        // how it looks in the Solid Gold version, which is V5, meaning
        // Infocom had full control over the status line, implying it is
        // the most correct display.
        if (is_game(Game::Planetfall) || is_game(Game::Stationfall)) {
            rhs = fstring("Score: %ld  Time: %ld ", first, second);
        } else {
            rhs = fstring("Score: %ld  Moves: %ld ", first, second);
        }

        if (rhs.size() > width) {
            rhs = fstring("%ld/%ld", first, second);
        }
    }

    if (rhs.size() <= width) {
        glk_window_move_cursor(statuswin.id, width - rhs.size(), 0);
        glk_put_string_stream(stream, &rhs[0]);
    }
#endif
}

#ifdef ZTERP_GLK
// These track the position of the cursor in the upper window at the
// beginning of a @read call so that the cursor can be replaced in case
// of a meta-command. They are also stored in the Scrn chunk of any
// meta-saves. They are in Z-machine coordinate format, not Glk, which
// means they are 1-based.
static long starting_x, starting_y;
#endif

// Attempt to read and parse a line of input. On success, return true.
// Otherwise, return false to indicate that input should be requested
// again.
static bool read_handler()
{
    uint16_t text = zargs[0], parse = zargs[1];
    ZASSERT(zversion >= 5 || user_byte(text) > 0, "text buffer cannot be zero sized");
    uint8_t maxchars = zversion >= 5 ? user_byte(text) : user_byte(text) - 1;
    uint8_t zscii_string[256];
    Input input;
    input.type = Input::Type::Line;
    input.maxlen = maxchars;
    input.preloaded = 0;
    uint16_t timer = 0;
    uint16_t routine = zargs[3];

    if (options.autosave && !in_interrupt()) {
        do_save(SaveType::Autosave, SaveOpcode::Read);
    }

#ifdef ZTERP_GLK
    starting_x = upperwin->x + 1;
    starting_y = upperwin->y + 1;
#endif

    if (zversion <= 3) {
        zshow_status();
    }

    if (zversion >= 4 && znargs > 2) {
        timer = zargs[2];
    }

    if (zversion >= 5) {
        int i;

        input.preloaded = user_byte(text + 1);
        ZASSERT(input.preloaded <= maxchars, "too many preloaded characters: %d when max is %d", input.preloaded, maxchars);

        for (i = 0; i < input.preloaded; i++) {
            input.line[i] = zscii_to_unicode[user_byte(text + i + 2)];
        }
        // Under Gargoyle, preloaded input generally works as it’s supposed to.
        // Under Glk, it can fail one of two ways:
        // 1. The preloaded text is printed out once, but is not editable.
        // 2. The preloaded text is printed out twice, the second being editable.
        // I have chosen option #2. For non-Glk, option #1 is done by necessity.
        //
        // The “normal” mode of operation for preloaded text seems to be that a
        // particular string is printed, and then that string is preloaded,
        // allowing the already-printed text to be edited. However, there is no
        // requirement that the preloaded text already be on-screen. For
        // example, what should happen if the text “ZZZ” is printed out, but the
        // text “AAA” is preloaded? The interpreter for Hitchiker’s Guide that
        // comes with the Lost Treasures of Infocom displays “ZZZ” (and not
        // “AAA”), and allows the user to edit it; but any characters not edited
        // by the user (that is, not backspaced over) are actually stored as “A”
        // characters. The on-screen “Z” characters are not stored.
        //
        // This isn’t possible under Glk, and as it stands, isn’t possible under
        // Gargoyle either (although it could be extended to support it). For
        // now I am not extending Gargoyle: the usual case (preloaded text
        // matching on-screen text) does work with Gargoyle’s unput extensions,
        // and the unusual/unexpected case (preloaded text not matching) at
        // least is usable: the whole preloaded string is displayed and can be
        // edited. I suspect nobody but Infocom ever uses preloaded text, and
        // Infocom uses it in the normal way, so things work just fine.
#ifdef GARGLK
        input.line[i] = 0;
        if (curwin->id != nullptr) {
            // Convert the Z-machine's 16-bit string to a 32-bit string for Glk.
            std::vector<glui32> line32;
            std::copy(input.line.begin(), input.line.begin() + input.preloaded + 1, std::back_inserter(line32));
            // If the preloaded text would wrap backward in the upper window
            // (to the previous line), limit it to just the current line. For
            // example:
            //
            // |This text bre|
            // |aks          |
            //
            // If the string “breaks” is preloaded, only try to unput the “aks”
            // part. Backing all the way up to “bre” works (as in, is
            // successfully unput) in Gargoyle, but input won’t work: the cursor
            // will be placed at line 1, right after the “e”, and no input will
            // be allowed. At least by keeping the cursor on the second line,
            // proper user input will occur.
            if (curwin == upperwin) {
                long max = input.preloaded > upperwin->x ? upperwin->x : input.preloaded;
                long start = input.preloaded - max;
                glui32 unput = garglk_unput_string_count_uni(&line32[start]);

                // Since the preloaded text might not have been on the screen
                // (or only partially so), reduce the current and starting X
                // coordinates by the number of unput characters, since that is
                // where Gargoyle will logically be starting input.
                curwin->x -= unput;
                starting_x -= unput;
            } else {
                garglk_unput_string_uni(&line32[0]);
            }
        }
#endif
    }

    if (!get_input(timer, routine, input)) {
        if (zversion >= 5) {
            store(0);
        }
        return true;
    }

#ifdef ZTERP_GLK
    update_delayed();
#endif

    if (curwin == mainwin) {
        history.add_input(&input.line[0], input.len);
    }

    if (options.enable_escape) {
        transcribe(033);
        transcribe('[');
        for (const auto c : *options.escape_string) {
            transcribe(c);
        }
    }

    for (int i = 0; i < input.len; i++) {
        transcribe(input.line[i]);
        if (streams.test(STREAM_SCRIPT)) {
            scriptio->putc(input.line[i]);
        }
    }

    if (options.enable_escape) {
        transcribe(033);
        transcribe('[');
        transcribe('0');
        transcribe('m');
    }

    transcribe(UNICODE_LINEFEED);
    if (streams.test(STREAM_SCRIPT)) {
        scriptio->putc(UNICODE_LINEFEED);
    }

    if (!options.disable_meta_commands) {
        input.line[input.len] = 0;

        if (input.line[0] == '/') {
#ifdef ZTERP_GLK
            // If the game is currently in the upper window, blank out
            // everything the user typed so that a re-request of input has a
            // clean slate to work with. Replace the cursor where it was at the
            // start of input.
            if (curwin == upperwin) {
                set_cursor(starting_y, starting_x);
                for (int i = 0; i < input.len; i++) {
                    put_char_u(UNICODE_SPACE);
                }
                set_cursor(starting_y, starting_x);
            }
#endif

            auto result = handle_meta_command(&input.line[0], input.len);
            switch (result.first) {
            case MetaResult::Rerequest:
                // The game still wants input, so try again. If this is the main
                // window, print a prompt that hopefully meshes well with the
                // game. If it’s the upper window, don’t print anything, because
                // that will almost certainly do more harm than good.
#ifdef ZTERP_GLK
                if (curwin != upperwin)
#endif
                {
                    screen_print("\n>");
                }

                // Any preloaded text is probably going to have been printed by
                // the game first, which allows Gargoyle to unput the preloaded
                // text. But when re-requesting input, the preloaded text will
                // not be on the screen anymore. Print it again so that it can
                // be unput.
                //
                // If this implementation is not Gargoyle, don’t print
                // anything. The original text (if there in fact was
                // any) will still be on the screen, and the preloaded
                // text will be displayed (and editable) by Glk.
#ifdef GARGLK
                for (int i = 0; i < input.preloaded; i++) {
                    put_char(zscii_to_unicode[user_byte(text + i + 2)]);
                }
#endif

                return false;
            case MetaResult::Say: {
                // Convert the UTF-8 result to Unicode using memory-backed I/O.
                IO io(std::vector<uint8_t>(result.second.begin(), result.second.end()), IO::Mode::ReadOnly);
                std::vector<uint16_t> string;

                for (auto c = io.getc(true); c != -1; c = io.getc(true)) {
                    string.push_back(c);
                }

                input.len = string.size();
                std::copy(string.begin(), string.end(), input.line.begin());
            }
            }
        }

        // V1–4 do not have @save_undo, so simulate one each time @read is
        // called.
        //
        // Although V5 introduced @save_undo, not all games make use of it
        // (e.g. Hitchhiker’s Guide). Until @save_undo is called, simulate
        // it each @read, just like in V1–4. If @save_undo is called, all
        // of these interpreter-generated save states are forgotten and the
        // game’s calls to @save_undo take over.
        //
        // Because V1–4 games will never call @save_undo, seen_save_undo
        // will never be true. Thus there is no need to test zversion.
        if (!seen_save_undo) {
            push_save(SaveStackType::Game, SaveType::Meta, SaveOpcode::Read, nullptr);
        }
    }

    for (int i = 0; i < input.len; i++) {
        zscii_string[i] = unicode_to_zscii_q[unicode_tolower(input.line[i])];
    }

    if (zversion >= 5) {
        user_store_byte(text + 1, input.len); // number of characters read

        for (int i = 0; i < input.len; i++) {
            user_store_byte(text + i + 2, zscii_string[i]);
        }

        if (parse != 0) {
            tokenize(text, parse, 0, false);
        }

        store(input.term);
    } else {
        for (int i = 0; i < input.len; i++) {
            user_store_byte(text + i + 1, zscii_string[i]);
        }

        user_store_byte(text + input.len + 1, 0);

        tokenize(text, parse, 0, false);
    }

    return true;
}

void zread()
{
    while (!read_handler()) {
    }
}

void zprint_unicode()
{
    if (valid_unicode(zargs[0])) {
        put_char_u(zargs[0]);
    } else {
        put_char_u(UNICODE_REPLACEMENT);
    }
}

void zcheck_unicode()
{
    uint16_t res = 0;

    // valid_unicode() will tell which Unicode characters can be printed;
    // and if the Unicode character is in the Unicode input table, it can
    // also be read. If Unicode is not available, then any character >255
    // is invalid for both reading and writing.
    if (have_unicode || zargs[0] < 256) {
        // §3.8.5.4.5: “Unicode characters U+0000 to U+001F and U+007F to
        // U+009F are control codes, and must not be used.”
        //
        // Even though control characters can be read (e.g. delete and
        // linefeed), when they are looked at through a Unicode lens, they
        // should be considered invalid. I don’t know if this is the right
        // approach, but nobody seems to use @check_unicode, so it’s not
        // especially important. One implication of this is that it’s
        // impossible for this implementation of @check_unicode to return 2,
        // since a character must be valid for output before it’s even
        // checked for input, and all printable characters are also
        // readable.
        //
        // For what it’s worth, interpreters seem to disagree on this pretty
        // drastically:
        //
        // • Zoom 1.1.5 returns 1 for all control characters.
        // • Fizmo 0.7.8 returns 3 for characters 8, 10, 13, and 27, 1 for
        //   all other control characters.
        // • Frotz 2.44 and Nitfol 0.5 return 0 for all control characters.
        // • Filfre 1.1.1 returns 3 for all control characters.
        // • Windows Frotz 1.19 returns 2 for characters 8, 13, and 27, 0
        //   for other control characters in the range 00 to 1f. It returns
        //   a mixture of 2 and 3 for control characters in the range 7F to
        //   9F based on whether the specified glyph is available.
        if (valid_unicode(zargs[0])) {
            res |= 0x01;
            if (unicode_to_zscii[zargs[0]] != 0) {
                res |= 0x02;
            }
        }
    }

#ifdef ZTERP_GLK
    if (glk_gestalt(gestalt_CharOutput, zargs[0]) == gestalt_CharOutput_CannotPrint) {
        res &= ~static_cast<uint16_t>(0x01);
    }
    if (!glk_gestalt(gestalt_CharInput, zargs[0])) {
        res &= ~static_cast<uint16_t>(0x02);
    }
#endif

    store(res);
}

// Should picture_data and get_wind_prop be moved to a V6 source file?
void zpicture_data()
{
    if (zargs[0] == 0) {
        user_store_word(zargs[1] + 0, 0);
        user_store_word(zargs[1] + 2, 0);
    }

    // No pictures means no valid pictures, so never branch.
    branch_if(false);
}

void zget_wind_prop()
{
    uint8_t font_width = 1, font_height = 1;
    uint16_t val;
    Window *win;

    win = find_window(zargs[0]);

    // These are mostly bald-faced lies.
    switch (zargs[1]) {
    case 0: // y coordinate
        val = 0;
        break;
    case 1: // x coordinate
        val = 0;
        break;
    case 2:  // y size
        val = word(0x24) * font_height;
        break;
    case 3:  // x size
        val = word(0x22) * font_width;
        break;
    case 4:  // y cursor
        val = 0;
        break;
    case 5:  // x cursor
        val = 0;
        break;
    case 6: // left margin size
        val = 0;
        break;
    case 7: // right margin size
        val = 0;
        break;
    case 8: // newline interrupt routine
        val = 0;
        break;
    case 9: // interrupt countdown
        val = 0;
        break;
    case 10: // text style
        val = win->style.to_ulong();
        break;
    case 11: // colour data
        val = (9 << 8) | 2;
        break;
    case 12: // font number
        val = static_cast<uint16_t>(win->font);
        break;
    case 13: // font size
        val = (font_height << 8) | font_width;
        break;
    case 14: // attributes
        val = 0;
        break;
    case 15: // line count
        val = 0;
        break;
    case 16: // true foreground colour
        val = 0;
        break;
    case 17: // true background colour
        val = 0;
        break;
    default:
        die("unknown window property: %u", static_cast<unsigned int>(zargs[1]));
    }

    store(val);
}

// This is not correct, because @output_stream does not work as it
// should with a width argument; however, this does print out the
// contents of a table that was sent to stream 3, so it’s at least
// somewhat useful.
//
// Output should be to the currently-selected window, but since V6 is
// only marginally supported, other windows are not active. Send to the
// main window for the time being.
void zprint_form()
{
    Window *saved = curwin;

    curwin = mainwin;
#ifdef ZTERP_GLK
    glk_set_window(mainwin->id);
#endif

    for (uint16_t i = 0; i < user_word(zargs[0]); i++) {
        put_char(user_byte(zargs[0] + 2 + i));
    }

    put_char(ZSCII_NEWLINE);

    curwin = saved;
#ifdef ZTERP_GLK
    glk_set_window(curwin->id);
#endif
}

void zmake_menu()
{
    branch_if(false);
}

void zbuffer_screen()
{
    store(0);
}

#ifdef GLK_MODULE_GARGLKTEXT
// Glk does not guarantee great control over how various styles are
// going to look, but Gargoyle does. Abusing the Glk “style hints”
// functions allows for quite fine-grained control over style
// appearance. First, clear the (important) attributes for each style,
// and then recreate each in whatever mold is necessary. Re-use some
// that are expected to be correct (emphasized for italic, subheader for
// bold, and so on).
static void set_default_styles()
{
    std::array<int, 7> styles = { style_Subheader, style_Emphasized, style_Alert, style_Preformatted, style_User1, style_User2, style_Note };

    for (const auto &style : styles) {
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Size, 0);
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Weight, 0);
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Oblique, 0);

        // This sets wintype_TextGrid to be proportional, which of course is
        // wrong; but text grids are required to be fixed, so Gargoyle
        // simply ignores this hint for those windows.
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Proportional, 1);
    }
}
#endif

bool create_mainwin()
{
#ifdef ZTERP_GLK

#ifdef GLK_MODULE_UNICODE
    have_unicode = glk_gestalt(gestalt_Unicode, 0);
#endif

#ifdef GLK_MODULE_GARGLKTEXT
    set_default_styles();

    glk_stylehint_set(wintype_AllTypes, GStyleBold, stylehint_Weight, 1);

    glk_stylehint_set(wintype_AllTypes, GStyleItalic, stylehint_Oblique, 1);

    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalic, stylehint_Weight, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalic, stylehint_Oblique, 1);

    glk_stylehint_set(wintype_AllTypes, GStyleFixed, stylehint_Proportional, 0);

    glk_stylehint_set(wintype_AllTypes, GStyleBoldFixed, stylehint_Weight, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldFixed, stylehint_Proportional, 0);

    glk_stylehint_set(wintype_AllTypes, GStyleItalicFixed, stylehint_Oblique, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleItalicFixed, stylehint_Proportional, 0);

    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalicFixed, stylehint_Weight, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalicFixed, stylehint_Oblique, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalicFixed, stylehint_Proportional, 0);
#endif

    mainwin->id = glk_window_open(nullptr, 0, 0, wintype_TextBuffer, 1);
    if (mainwin->id == nullptr) {
        return false;
    }
    glk_set_window(mainwin->id);

#ifdef GLK_MODULE_LINE_ECHO
    mainwin->has_echo = glk_gestalt(gestalt_LineInputEcho, 0);
    if (mainwin->has_echo) {
        glk_set_echo_line_event(mainwin->id, 0);
    }
#endif

    return true;
#else
    return true;
#endif
}

bool create_statuswin()
{
#ifdef ZTERP_GLK
    statuswin.id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, 0);
    return statuswin.id != nullptr;
#else
    return false;
#endif
}

bool create_upperwin()
{
#ifdef ZTERP_GLK
    // The upper window appeared in V3. */
    if (zversion >= 3) {
        upperwin->id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 0, wintype_TextGrid, 0);
        upperwin->x = upperwin->y = 0;
        upper_window_height = 0;

        if (upperwin->id != nullptr) {
            glui32 w, h;

            glk_window_get_size(upperwin->id, &w, &h);
            upper_window_width = w;

            if (h != 0 || upper_window_width == 0) {
                glk_window_close(upperwin->id, nullptr);
                upperwin->id = nullptr;
            }
        }
    }

    return upperwin->id != nullptr;
#else
    return false;
#endif
}

// Write out the screen state for a Scrn chunk.
//
// This implements version 0 as described in stack.c.
IFF::TypeID screen_write_scrn(IO &io)
{
    io.write32(0);

    io.write8(curwin - &windows[0]);
#ifdef ZTERP_GLK
    io.write16(upper_window_height);
    io.write16(starting_x);
    io.write16(starting_y);
#else
    io.write16(0);
    io.write16(0);
    io.write16(0);
#endif

    for (int i = 0; i < (zversion == 6 ? 8 : 2); i++) {
        io.write8(windows[i].style.to_ulong());
        io.write8(static_cast<uint8_t>(windows[i].font));
        io.write8(static_cast<uint8_t>(windows[i].fg_color.mode));
        io.write16(windows[i].fg_color.value);
        io.write8(static_cast<uint8_t>(windows[i].bg_color.mode));
        io.write16(windows[i].bg_color.value);
    }

    return IFF::TypeID(&"Scrn");
}

static void try_load_color(Color::Mode mode, uint16_t value, Color &color)
{
    switch (mode) {
    case Color::Mode::ANSI:
        if (value >= 1 && value <= 12) {
            color = Color(Color::Mode::ANSI, value);
        }
        break;
    case Color::Mode::True:
        if (value < 32768) {
            color = Color(Color::Mode::True, value);
        }
        break;
    }
}

// Read and restore the screen state from a Scrn chunk. Since this
// actively touches the screen, it should only be called once the
// restore function has determined that the restore is successful.
void screen_read_scrn(IO &io, uint32_t size)
{
    uint32_t version;
    size_t data_size = zversion == 6 ? 71 : 23;
    uint8_t current_window;
    uint16_t new_upper_window_height;
    struct {
        uint8_t style;
        Window::Font font;
        Color::Mode fgmode;
        uint16_t fgvalue;
        Color::Mode bgmode;
        uint16_t bgvalue;
    } window_data[8];
#ifdef ZTERP_GLK
    uint16_t new_x, new_y;
#endif

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        throw RestoreError("short read");
    }

    if (version != 0) {
        show_message("Unsupported Scrn version %lu; ignoring chunk", static_cast<unsigned long>(version));
        return;
    }

    if (size != data_size + 4) {
        throw RestoreError(fstring("invalid size: %lu", static_cast<unsigned long>(size)));
    }

    try {
        current_window = io.read8();
        new_upper_window_height = io.read16();
#ifdef ZTERP_GLK
        new_x = io.read16();
        new_y = io.read16();
#else
        io.read32();
#endif

        for (int i = 0; i < (zversion == 6 ? 8 : 2); i++) {
            window_data[i].style = io.read8();
            window_data[i].font = static_cast<Window::Font>(io.read8());
            window_data[i].fgmode = static_cast<Color::Mode>(io.read8());
            window_data[i].fgvalue = io.read16();
            window_data[i].bgmode = static_cast<Color::Mode>(io.read8());
            window_data[i].bgvalue = io.read16();
        }
    } catch (const IO::IOError &) {
        throw RestoreError("short read");
    }

    if (current_window > 7) {
        throw RestoreError(fstring("invalid window: %d", current_window));
    }

    set_current_window(&windows[current_window]);
#ifdef ZTERP_GLK
    delayed_window_shrink = -1;
    saw_input = false;
#endif
    resize_upper_window(new_upper_window_height, false);

#ifdef ZTERP_GLK
    if (new_x > upper_window_width) {
        new_x = upper_window_width;
    }
    if (new_y > upper_window_height) {
        new_y = upper_window_height;
    }
    if (new_y != 0 && new_x != 0) {
        set_cursor(new_y, new_x);
    }
#endif

    for (int i = 0; i < (zversion == 6 ? 8 : 2); i++) {
        if (window_data[i].style < 16) {
            windows[i].style = window_data[i].style;
        }

        if (is_valid_font(window_data[i].font)) {
            windows[i].font = window_data[i].font;
        }

        try_load_color(window_data[i].fgmode, window_data[i].fgvalue, windows[i].fg_color);
        try_load_color(window_data[i].bgmode, window_data[i].bgvalue, windows[i].bg_color);
    }

    set_current_style();
}

IFF::TypeID screen_write_bfhs(IO &io)
{
    io.write32(0); // version
    io.write32(history.size());

    for (const auto &entry : history.entries()) {
        switch (entry.type) {
        case History::Entry::Type::Style:
            io.write8(static_cast<uint8_t>(entry.type));
            io.write8(entry.contents.style);
            break;
        case History::Entry::Type::FGColor: case History::Entry::Type::BGColor:
            io.write8(static_cast<uint8_t>(entry.type));
            io.write8(static_cast<uint8_t>(entry.contents.color.mode));
            io.write16(entry.contents.color.value);
            break;
        case History::Entry::Type::InputStart: case History::Entry::Type::InputEnd:
            io.write8(static_cast<uint8_t>(entry.type));
            break;
        case History::Entry::Type::Char:
            io.write8(static_cast<uint8_t>(entry.type));
            io.putc(entry.contents.c);
        }
    };

    return IFF::TypeID(&"Bfhs");
}

class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> fn) : m_fn(std::move(fn)) {
    }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    ~ScopeGuard() {
        m_fn();
    }

private:
    std::function<void()> m_fn;
};

void screen_read_bfhs(IO &io, bool autosave)
{
    uint32_t version;
    Window saved = *mainwin;
    auto original_style = mainwin->style;
    uint32_t size;
#ifdef ZTERP_GLK
    // Write directly to the main window’s stream instead of going
    // through screen_print() or similar: history playback should be
    // “transparent”, so to speak. It will have already been added to a
    // transcript during the previous round of play (if the user so
    // desired), and this function itself recreates all history directly
    // from the Bfhs chunk.
    strid_t stream = glk_window_get_stream(mainwin->id);
#endif

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        show_message("Unable to read history version");
        return;
    }

    if (version != 0) {
        show_message("Unsupported history version: %lu", static_cast<unsigned long>(version));
        return;
    }

    mainwin->fg_color = Color();
    mainwin->bg_color = Color();
    mainwin->style.reset();
    set_window_style(mainwin);

#ifdef ZTERP_GLK
    auto write = [&stream](std::string msg) {
        glk_put_string_stream(stream, &msg[0]);
    };
#else
    auto write = [](const std::string &msg) {
        std::cout << msg;
    };
#endif

    ScopeGuard guard([&autosave, &saved, &write] {
        if (!autosave) {
            write("[End of history playback]\n\n");
        }
        *mainwin = saved;
        set_window_style(mainwin);
    });

    if (!autosave) {
        write("[Starting history playback]\n");
    }

    try {
        size = io.read32();
    } catch (const IO::IOError &) {
        return;
    }

    if (size == 0 && autosave) {
        warning("empty history record");
        screen_print(">");
        return;
    }

    for (uint32_t i = 0; i < size; i++) {
        uint8_t type;
        long c;

        try {
            type = io.read8();
        } catch (const IO::IOError &) {
            return;
        }

        // Each history entry is added to the history buffer. This is so
        // that new saves, after a restore, continue to include the
        // older save file’s history.
        switch (static_cast<History::Entry::Type>(type)) {
        case History::Entry::Type::Style: {
            uint8_t style;

            try {
                style = io.read8();
            } catch (const IO::IOError &) {
                return;
            }

            mainwin->style = style;
            set_window_style(mainwin);

            history.add_style();

            break;
        }
        case History::Entry::Type::FGColor: case History::Entry::Type::BGColor: {
            uint8_t mode;
            uint16_t value;

            try {
                mode = io.read8();
                value = io.read16();
            } catch (const IO::IOError &) {
                return;
            }

            Color &color = static_cast<History::Entry::Type>(type) == History::Entry::Type::FGColor ? mainwin->fg_color : mainwin->bg_color;

            try_load_color(static_cast<Color::Mode>(mode), value, color);
            set_window_style(mainwin);

            if (static_cast<History::Entry::Type>(type) == History::Entry::Type::FGColor) {
                history.add_fg_color(color);
            } else {
                history.add_bg_color(color);
            }

            break;
        }
        case History::Entry::Type::InputStart:
            original_style = mainwin->style;
            mainwin->style.reset();
            set_window_style(mainwin);
#ifdef ZTERP_GLK
            glk_set_style_stream(stream, style_Input);
#endif
            history.add_input_start();
            break;
        case History::Entry::Type::InputEnd:
            mainwin->style = original_style;
            set_window_style(mainwin);
            history.add_input_end();
            break;
        case History::Entry::Type::Char:
            c = io.getc(false);
            if (c == -1) {
                return;
            }
#ifdef ZTERP_GLK
            xglk_put_char_stream(stream, c);
#else
            IO::standard_out().putc(c);
#endif
            history.add_input_char(c);
            break;
        default:
            return;
        }
    }
}

IFF::TypeID screen_write_bfts(IO &io)
{
    if (!options.persistent_transcript || perstransio == nullptr) {
        return IFF::TypeID();
    }

    io.write32(0); // Version

    auto buf = perstransio->get_memory();
    io.write_exact(buf.data(), buf.size());

    return IFF::TypeID(&"Bfts");
}

void screen_read_bfts(IO &io, uint32_t size)
{
    uint32_t version;
    std::vector<uint8_t> buf;

    if (size < 4) {
        show_message("Corrupted Bfts entry (too small)");
    }

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        show_message("Unable to read persistent transcript size from save file");
        return;
    }

    if (version != 0) {
        show_message("Unsupported Bfts version %lu", static_cast<unsigned long>(version));
        return;
    }

    // The size of the transcript is the size of the whole chunk minus
    // the 32-bit version.
    size -= 4;

    if (size > 0) {
        try {
            buf.resize(size);
        } catch (const std::bad_alloc &) {
            show_message("Unable to allocate memory for persistent transcript");
            return;
        }

        try {
            io.read_exact(&buf[0], size);
        } catch (const IO::IOError &) {
            show_message("Unable to read persistent transcript from save file");
            return;
        }
    }

    perstransio = std::make_unique<IO>(buf, IO::Mode::WriteOnly);

    try {
        perstransio->seek(0, IO::SeekFrom::End);
    } catch (const IO::IOError &) {
        perstransio.reset();
    }
}

static std::unique_ptr<std::vector<uint8_t>> perstrans_backup;

static void perstrans_stash_backup()
{
    perstrans_backup.reset();

    if (options.persistent_transcript && perstransio != nullptr) {
        auto buf = perstransio->get_memory();
        try {
            perstrans_backup = std::make_unique<std::vector<uint8_t>>(buf);
        } catch (const std::bad_alloc &) {
        }
    }
}

// Never fail: this is an optional chunk so isn’t fatal on error.
static bool perstrans_stash_restore()
{
    if (perstrans_backup == nullptr) {
        return true;
    }

    perstransio = std::make_unique<IO>(*perstrans_backup, IO::Mode::WriteOnly);

    try {
        perstransio->seek(0, IO::SeekFrom::End);
    } catch (const IO::IOError &) {
        perstransio.reset();
    }

    perstrans_backup.reset();

    return true;
}

static void perstrans_stash_free()
{
    perstrans_backup.reset();
}

void screen_save_persistent_transcript()
{
    if (!options.persistent_transcript) {
        screen_puts("[Persistent transcripting is turned off]");
        return;
    }

    if (perstransio == nullptr) {
        screen_puts("[Persistent transcripting failed to start]");
        return;
    }

    auto buf = perstransio->get_memory();

    try {
        IO io(nullptr, IO::Mode::WriteOnly, IO::Purpose::Transcript);

        try {
            io.write_exact(buf.data(), buf.size());
            screen_puts("[Saved]");
        } catch (const IO::IOError &) {
            screen_puts("[Error writing transcript to file]");
        }
    } catch (const IO::OpenError &) {
        screen_puts("[Failed to open file]");
        return;
    }
}

void init_screen(bool first_run)
{
    for (auto &window : windows) {
        window.style.reset();
        window.fg_color = window.bg_color = Color();
        window.font = Window::Font::Normal;

#ifdef ZTERP_GLK
        clear_window(&window);
#endif
    }

    close_upper_window();

#ifdef ZTERP_GLK
    if (statuswin.id != nullptr) {
        glk_window_clear(statuswin.id);
    }

    if (errorwin != nullptr) {
        glk_window_close(errorwin, nullptr);
        errorwin = nullptr;
    }

    stop_timer();

#else
    have_unicode = true;
#endif

    if (first_run && options.persistent_transcript) {
        try {
            perstransio = std::make_unique<IO>(std::vector<uint8_t>(), IO::Mode::WriteOnly);
        } catch (const IO::OpenError &) {
            warning("Failed to start persistent transcripting");
        }

        stash_register(perstrans_stash_backup, perstrans_stash_restore, perstrans_stash_free);
    }

    // On restart, deselect stream 3 and select stream 1. This allows
    // the command script and transcript to persist across restarts,
    // while resetting memory output and ensuring screen output.
    streams.reset(STREAM_MEMORY);
    streams.set(STREAM_SCREEN);
    stream_tables.clear();

    set_current_window(mainwin);
}
