// vim: set ft=cpp:

#ifndef ZTERP_TABLES_H
#define ZTERP_TABLES_H

#include <array>

#include "types.h"

constexpr uint16_t UNICODE_DELETE          = 8;
constexpr uint16_t UNICODE_LINEFEED        = 10;
constexpr uint16_t UNICODE_CARRIAGE_RETURN = 13;
constexpr uint16_t UNICODE_ESCAPE          = 27;
constexpr uint16_t UNICODE_SPACE           = 32;
constexpr uint16_t UNICODE_REPLACEMENT     = 65533;

constexpr uint8_t LATIN1_LINEFEED     = 10;
constexpr uint8_t LATIN1_QUESTIONMARK = 63;

constexpr uint8_t ZSCII_DELETE       = 8;
constexpr uint8_t ZSCII_NEWLINE      = 13;
constexpr uint8_t ZSCII_ESCAPE       = 27;
constexpr uint8_t ZSCII_SPACE        = 32;
constexpr uint8_t ZSCII_PERIOD       = 46;
constexpr uint8_t ZSCII_QUESTIONMARK = 63;
constexpr uint8_t ZSCII_UP           = 129;
constexpr uint8_t ZSCII_DOWN         = 130;
constexpr uint8_t ZSCII_LEFT         = 131;
constexpr uint8_t ZSCII_RIGHT        = 132;
constexpr uint8_t ZSCII_F1           = 133;
constexpr uint8_t ZSCII_F2           = 134;
constexpr uint8_t ZSCII_F3           = 135;
constexpr uint8_t ZSCII_F4           = 136;
constexpr uint8_t ZSCII_F5           = 137;
constexpr uint8_t ZSCII_F6           = 138;
constexpr uint8_t ZSCII_F7           = 139;
constexpr uint8_t ZSCII_F8           = 140;
constexpr uint8_t ZSCII_F9           = 141;
constexpr uint8_t ZSCII_F10          = 142;
constexpr uint8_t ZSCII_F11          = 143;
constexpr uint8_t ZSCII_F12          = 144;
constexpr uint8_t ZSCII_KEY0         = 145;
constexpr uint8_t ZSCII_KEY1         = 146;
constexpr uint8_t ZSCII_KEY2         = 147;
constexpr uint8_t ZSCII_KEY3         = 148;
constexpr uint8_t ZSCII_KEY4         = 149;
constexpr uint8_t ZSCII_KEY5         = 150;
constexpr uint8_t ZSCII_KEY6         = 151;
constexpr uint8_t ZSCII_KEY7         = 152;
constexpr uint8_t ZSCII_KEY8         = 153;
constexpr uint8_t ZSCII_KEY9         = 154;
constexpr uint8_t ZSCII_CLICK_MENU   = 252;
constexpr uint8_t ZSCII_CLICK_DOUBLE = 253;
constexpr uint8_t ZSCII_CLICK_SINGLE = 254;

extern std::array<uint16_t, UINT8_MAX + 1> zscii_to_unicode;
extern std::array<uint8_t, UINT16_MAX + 1> unicode_to_zscii;
extern std::array<uint8_t, UINT16_MAX + 1> unicode_to_zscii_q;
extern std::array<uint8_t, UINT16_MAX + 1> unicode_to_latin1;
extern std::array<uint16_t, UINT8_MAX + 1> zscii_to_font3;
extern std::array<int, UINT8_MAX + 1> atable_pos;

void parse_unicode_table(uint16_t utable);
void setup_tables();

uint16_t unicode_tolower(uint16_t c);
#ifdef ZTERP_GLK
uint16_t char_to_unicode(char c);
#endif

bool valid_unicode(uint16_t c);

#ifdef ZTERP_OS_DOS
uint8_t unicode_to_437(uint16_t c);
#endif

#endif
