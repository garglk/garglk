#ifndef ZTERP_TABLES_H
#define ZTERP_TABLES_H

#include <stdint.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#define UNICODE_LINEFEED	10
#define UNICODE_SPACE		32
#define UNICODE_QUESTIONMARK	63

#define ZSCII_NEWLINE		13
#define ZSCII_SPACE		32
#define ZSCII_QUESTIONMARK	63

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

/* Standard 1.1 notes that Unicode characters 0–31 and 127–159
 * are invalid due to the fact that they’re control codes.
 */
static inline int valid_unicode(uint16_t c) { return (c >= 32 && c <= 126) || c >= 160; }

/* Somehwat ugly hack to get around the fact that Unicode
 * functions may not exist.  If GLK_MODULE_UNICODE is not
 * defined, then have_unicode must not be set to true.  This is
 * solely to prevent link errors.
 */
#if defined(ZTERP_GLK) && !defined(GLK_MODULE_UNICODE)
#define glk_put_char_uni(...)		die("bug %s:%d: glk_put_char_uni() called with no unicode", __FILE__, __LINE__)
#define glk_put_string_uni(...)		die("bug %s:%d: glk_put_string_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_char_event_uni(...)	die("bug %s:%d: glk_request_char_event_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_line_event_uni(...)	die("bug %s:%d: glk_request_line_event_uni() called with no unicode", __FILE__, __LINE__)
#endif

#endif
