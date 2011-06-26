#ifndef ZTERP_SCREEN_H
#define ZTERP_SCREEN_H

#include <stdint.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

/* Boolean flag describing whether the header bit meaning “fixed font” is set. */
extern int header_fixed_font;

extern uint32_t read_pc;

void init_screen(void);

int create_mainwin(void);
int create_statuswin(void);
int create_upperwin(void);
void get_screen_size(unsigned int *, unsigned int *);
void close_upper_window(void);
void cancel_all_events(void);

/* Text styles. */
#define STYLE_NONE	(0U     )
#define STYLE_REVERSE	(1U << 0)
#define STYLE_BOLD	(1U << 1)
#define STYLE_ITALIC	(1U << 2)
#define STYLE_FIXED	(1U << 3)

void show_message(const char *, ...);

#ifdef GLK_MODULE_LINE_TERMINATORS
void term_keys_reset(void);
void term_keys_add(uint8_t);
#endif

#ifdef GARGLK
void update_color(int, unsigned long);
#endif

/* Output streams. */
#define OSTREAM_SCREEN		1
#define OSTREAM_SCRIPT		2
#define OSTREAM_MEMORY		3
#define OSTREAM_RECORD		4

/* Input streams. */
#define ISTREAM_KEYBOARD	0
#define ISTREAM_FILE		1

int output_stream(int16_t, uint16_t);
int input_stream(int);

void set_current_style(void);

int print_handler(uint32_t, void (*)(uint8_t));
void put_char_u(uint16_t);
void put_char(uint8_t);

void zoutput_stream(void);
void zinput_stream(void);
void zprint(void);
void zprint_ret(void);
void znew_line(void);
void zerase_window(void);
void zerase_line(void);
void zset_cursor(void);
void zget_cursor(void);
void zset_colour(void);
void zset_true_colour(void);
void zset_text_style(void);
void zset_font(void);
void zprint_table(void);
void zprint_char(void);
void zprint_num(void);
void zprint_addr(void);
void zprint_paddr(void);
void zsplit_window(void);
void zset_window(void);
void zread_char(void);
void zshow_status(void);
void zread(void);
void zprint_unicode(void);
void zcheck_unicode(void);
void zpicture_data(void);
void zget_wind_prop(void);
void zprint_form(void);
void zmake_menu(void);
void zbuffer_screen(void);

#endif
