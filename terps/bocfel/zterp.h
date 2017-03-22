#ifndef ZTERP_ZTERP_H
#define ZTERP_ZTERP_H

#include <stdint.h>

struct options
{
  long eval_stack_size;
  long call_stack_size;
  int disable_color;
  int disable_config;
  int disable_timed;
  int disable_sound;
  int enable_escape;
  char *escape_string;
  int disable_fixed;
  int assume_fixed;
  int disable_graphics_font;
  int enable_alt_graphics;
  int show_id;
  int disable_term_keys;
  int disable_meta_commands;
  long int_number;
  int int_version;
  int replay_on;
  char *replay_name;
  int record_on;
  char *record_name;
  int transcript_on;
  char *transcript_name;
  long max_saves;
  int disable_undo_compression;
  int show_version;
  int disable_abbreviations;
  int enable_censorship;
  int overwrite_transcript;
  int override_undo;
  long random_seed;
  char *random_device;
};

extern const char *game_file;
extern struct options options;

/* v3 */
#define FLAGS1_STATUSTYPE	(1U << 1)
#define FLAGS1_STORYSPLIT	(1U << 2)
#define FLAGS1_CENSOR		(1U << 3)
#define FLAGS1_NOSTATUS		(1U << 4)
#define FLAGS1_SCREENSPLIT	(1U << 5)
#define FLAGS1_VARIABLE		(1U << 6)

/* v4 */
#define FLAGS1_COLORS		(1U << 0)
#define FLAGS1_PICTURES		(1U << 1)
#define FLAGS1_BOLD		(1U << 2)
#define FLAGS1_ITALIC		(1U << 3)
#define FLAGS1_FIXED		(1U << 4)
#define FLAGS1_SOUND		(1U << 5)
#define FLAGS1_TIMED		(1U << 7)

#define FLAGS2_TRANSCRIPT	(1U << 0)
#define FLAGS2_FIXED		(1U << 1)
#define FLAGS2_STATUS		(1U << 2)
#define FLAGS2_PICTURES		(1U << 3)
#define FLAGS2_UNDO		(1U << 4)
#define FLAGS2_MOUSE		(1U << 5)
#define FLAGS2_COLORS		(1U << 6)
#define FLAGS2_SOUND		(1U << 7)
#define FLAGS2_MENUS		(1U << 8)

#define status_is_time()	(zversion == 3 && (BYTE(0x01) & FLAGS1_STATUSTYPE))
#define timer_available()	(zversion >= 4 && (BYTE(0x01) & FLAGS1_TIMED))

struct header
{
  uint16_t release;
  uint16_t dictionary;
  uint16_t objects;
  uint16_t globals;
  uint16_t static_start;
  uint16_t static_end;
  uint16_t abbr;
  uint32_t file_length;
  uint8_t  serial[6];
  uint16_t checksum;
  uint32_t R_O;
  uint32_t S_O;
};

extern int zversion;
extern struct header header;
extern uint8_t atable[];
extern int is_infocom_v1234;

int is_beyond_zork(void);
int is_journey(void);
int is_sherlock(void);

void write_header(void);

uint32_t unpack_routine(uint16_t);
uint32_t unpack_string(uint16_t);
void store(uint16_t);

void znop(void);
void zrestart(void);
void zquit(void);
void zverify(void);
void zpiracy(void);
void zsave5(void);
void zrestore5(void);

#endif
