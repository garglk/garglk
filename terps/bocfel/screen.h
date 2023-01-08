// vim: set ft=cpp:

#ifndef ZTERP_SCREEN_H
#define ZTERP_SCREEN_H

#include <bitset>
#include <string>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

#include "iff.h"
#include "io.h"
#include "types.h"
#include "util.h"

// Represents a Z-machine color.
//
// If mode is ANSI, value is a color in the range [1, 12], representing
// the colors as described in §8.3.1.
//
// If mode is True, value is a 15-bit color as described in §8.3.7 and
// §15.
struct Color {
    enum class Mode { ANSI, True } mode;
    uint16_t value;

    explicit Color() : mode(Mode::ANSI), value(1) {
    }

    Color(Mode mode_, uint16_t value_) : mode(mode_), value(value_) {
    }
};

void init_screen(bool first_run);

bool create_mainwin();
bool create_statuswin();
bool create_upperwin();
void get_screen_size(unsigned int &width, unsigned int &height);
void close_upper_window();

uint32_t screen_convert_color(uint16_t color);

// Text styles.
using Style = std::bitset<4>;

enum StyleBit {
    STYLE_REVERSE = 0,
    STYLE_BOLD    = 1,
    STYLE_ITALIC  = 2,
    STYLE_FIXED   = 3,
};

zprintflike(1, 2)
void show_message(const char *fmt, ...);
void screen_print(const std::string &s);
zprintflike(1, 2)
void screen_printf(const char *fmt, ...);
void screen_puts(const std::string &s);
void screen_message_prompt(const std::string &message);
void screen_flush();

#ifdef ZTERP_GLK
void term_keys_reset();
void term_keys_add(uint8_t key);
#endif

#ifdef GLK_MODULE_GARGLKTEXT
void update_color(int which, unsigned long color);
#endif

// Output streams.
constexpr uint16_t OSTREAM_SCREEN = 1;
constexpr uint16_t OSTREAM_SCRIPT = 2;
constexpr uint16_t OSTREAM_MEMORY = 3;
constexpr uint16_t OSTREAM_RECORD = 4;

// Input streams.
constexpr int ISTREAM_KEYBOARD = 0;
constexpr int ISTREAM_FILE     = 1;

void screen_set_header_bit(bool set);

bool output_stream(int16_t number, uint16_t table);
bool input_stream(int which);

int print_handler(uint32_t addr, void (*outc)(uint8_t));
void put_char(uint8_t c);

std::string screen_format_time(long hours, long minutes);
void screen_read_scrn(IO &io, uint32_t size);
IFF::TypeID screen_write_scrn(IO &io);
void screen_read_bfhs(IO &io, bool autosave);
IFF::TypeID screen_write_bfhs(IO &io);
void screen_read_bfts(IO &io, uint32_t size);
IFF::TypeID screen_write_bfts(IO &io);
void screen_save_persistent_transcript();

void zoutput_stream();
void zinput_stream();
void zprint();
void znew_line();
void zprint_ret();
void zerase_window();
void zerase_line();
void zset_cursor();
void zget_cursor();
void zset_colour();
void zset_true_colour();
void zset_text_style();
void zset_font();
void zprint_table();
void zprint_char();
void zprint_num();
void zprint_addr();
void zprint_paddr();
void zsplit_window();
void zset_window();
void zread_char();
void zshow_status();
void zread();
void zprint_unicode();
void zcheck_unicode();
void zpicture_data();
void zget_wind_prop();
void zprint_form();
void zmake_menu();
void zbuffer_screen();

#endif
