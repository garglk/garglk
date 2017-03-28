#ifndef ZTERP_TABLES_H
#define ZTERP_TABLES_H

#include <stdint.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#define UNICODE_DELETE		8
#define UNICODE_LINEFEED	10
#define UNICODE_CARRIAGE_RETURN	13
#define UNICODE_ESCAPE		27
#define UNICODE_SPACE		32
#define UNICODE_REPLACEMENT	65533

#define LATIN1_QUESTIONMARK	63

#define ZSCII_DELETE		8
#define ZSCII_NEWLINE		13
#define ZSCII_ESCAPE		27
#define ZSCII_SPACE		32
#define ZSCII_QUESTIONMARK	63
#define ZSCII_UP		129
#define ZSCII_DOWN		130
#define ZSCII_LEFT		131
#define ZSCII_RIGHT		132
#define ZSCII_F1		133
#define ZSCII_F2		134
#define ZSCII_F3		135
#define ZSCII_F4		136
#define ZSCII_F5		137
#define ZSCII_F6		138
#define ZSCII_F7		139
#define ZSCII_F8		140
#define ZSCII_F9		141
#define ZSCII_F10		142
#define ZSCII_F11		143
#define ZSCII_F12		144
#define ZSCII_KEY0		145
#define ZSCII_KEY1		146
#define ZSCII_KEY2		147
#define ZSCII_KEY3		148
#define ZSCII_KEY4		149
#define ZSCII_KEY5		150
#define ZSCII_KEY6		151
#define ZSCII_KEY7		152
#define ZSCII_KEY8		153
#define ZSCII_KEY9		154
#define ZSCII_CLICK_MENU	252
#define ZSCII_CLICK_DOUBLE	253
#define ZSCII_CLICK_SINGLE	254

/* This variable controls whether Unicode is used for screen
 * output.  This affects @check_unicode as well as the ZSCII to
 * Unicode table.  With Glk it is set based on whether the Glk
 * implementation supports Unicode (checked with the Unicode
 * gestalt), and determines whether Unicode IO functions should
 * be used; otherwise, it is kept in parallel with use_utf8_io.
 */
extern int have_unicode;

extern uint16_t zscii_to_unicode[];
extern uint8_t unicode_to_zscii[];
extern uint8_t unicode_to_zscii_q[];
extern uint8_t unicode_to_latin1[];
extern uint16_t zscii_to_font3[];
extern int atable_pos[];

void parse_unicode_table(uint16_t);
void setup_tables(void);

uint16_t unicode_tolower(uint16_t);
uint16_t char_to_unicode(char);

/* Standard 1.1 notes that Unicode characters 0–31 and 127–159
 * are invalid due to the fact that they’re control codes.
 */
static inline int valid_unicode(uint16_t c) { return (c >= 32 && c <= 126) || c >= 160; }

#endif
