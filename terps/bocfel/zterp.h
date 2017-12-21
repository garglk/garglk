#ifndef ZTERP_ZTERP_H
#define ZTERP_ZTERP_H

#include <stdint.h>
#include <stdbool.h>

struct options
{
  long eval_stack_size;
  long call_stack_size;
  bool disable_color;
  bool disable_config;
  bool disable_timed;
  bool disable_sound;
  bool enable_escape;
  char *escape_string;
  bool disable_fixed;
  bool assume_fixed;
  bool disable_graphics_font;
  bool enable_alt_graphics;
  bool show_id;
  bool disable_term_keys;
  char *username;
  bool disable_meta_commands;
  long int_number;
  bool disable_patches;
  int int_version;
  bool replay_on;
  char *replay_name;
  bool record_on;
  char *record_name;
  bool transcript_on;
  char *transcript_name;
  long max_saves;
  bool disable_undo_compression;
  bool show_version;
  bool disable_abbreviations;
  bool enable_censorship;
  bool overwrite_transcript;
  bool override_undo;
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

#define status_is_time()	(zversion == 3 && (byte(0x01) & FLAGS1_STATUSTYPE))
#define timer_available()	(zversion >= 4 && (byte(0x01) & FLAGS1_TIMED))

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
extern bool is_infocom_v1234;

bool is_beyond_zork(void);
bool is_journey(void);

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
