// vim: set ft=cpp:

#ifndef ZTERP_ZTERP_H
#define ZTERP_ZTERP_H

#include <array>
#include <exception>
#include <memory>
#include <string>

#include "stack.h"
#include "types.h"

class Exit : std::exception {
public:
    explicit Exit(int code) : m_code(code) {
    }

    int code() const {
        return m_code;
    }

private:
    int m_code;
};

constexpr unsigned long DEFAULT_INT_NUMBER = 1; // DEC

struct Options {
    unsigned long eval_stack_size = DEFAULT_STACK_SIZE;
    unsigned long call_stack_size = DEFAULT_CALL_DEPTH;
    bool disable_color = false;
    bool disable_config = false;
    bool disable_timed = false;
    bool disable_sound = false;
    bool enable_escape = false;
    std::unique_ptr<std::string> escape_string = std::make_unique<std::string>("1m");
    bool disable_fixed = false;
    bool assume_fixed = false;
    bool disable_graphics_font = false;
    bool enable_alt_graphics = false;
    bool disable_history_playback = false;
    bool show_id = false;
    bool disable_term_keys = false;
    std::unique_ptr<std::string> username = nullptr;
    bool disable_meta_commands = false;
    unsigned long int_number = DEFAULT_INT_NUMBER;
    unsigned char int_version = 'C';
    bool disable_patches = false;
    bool replay_on = false;
    std::unique_ptr<std::string> replay_name = nullptr;
    bool record_on = false;
    std::unique_ptr<std::string> record_name = nullptr;
    bool transcript_on = false;
    std::unique_ptr<std::string> transcript_name = nullptr;
    unsigned long undo_slots = 100;
    bool show_version = false;
    bool disable_abbreviations = false;
    bool enable_censorship = false;
    bool overwrite_transcript = false;
    bool override_undo = false;
    std::unique_ptr<unsigned long> random_seed;
    std::unique_ptr<std::string> random_device = nullptr;

    bool autosave = false;
    bool persistent_transcript = false;
    std::unique_ptr<std::string> editor = nullptr;
};

extern std::string game_file;
extern Options options;

#define ZTERP_VERSION	"2.1.1"

// v3
constexpr uint8_t FLAGS1_STATUSTYPE  = 1U << 1;
constexpr uint8_t FLAGS1_STORYSPLIT  = 1U << 2;
constexpr uint8_t FLAGS1_CENSOR      = 1U << 3;
constexpr uint8_t FLAGS1_NOSTATUS    = 1U << 4;
constexpr uint8_t FLAGS1_SCREENSPLIT = 1U << 5;
constexpr uint8_t FLAGS1_VARIABLE    = 1U << 6;

// v4
constexpr uint8_t FLAGS1_COLORS   = 1U << 0;
constexpr uint8_t FLAGS1_PICTURES = 1U << 1;
constexpr uint8_t FLAGS1_BOLD     = 1U << 2;
constexpr uint8_t FLAGS1_ITALIC   = 1U << 3;
constexpr uint8_t FLAGS1_FIXED    = 1U << 4;
constexpr uint8_t FLAGS1_SOUND    = 1U << 5;
constexpr uint8_t FLAGS1_TIMED    = 1U << 7;

constexpr uint16_t FLAGS2_TRANSCRIPT = 1U << 0;
constexpr uint16_t FLAGS2_FIXED      = 1U << 1;
constexpr uint16_t FLAGS2_STATUS     = 1U << 2;
constexpr uint16_t FLAGS2_PICTURES   = 1U << 3;
constexpr uint16_t FLAGS2_UNDO       = 1U << 4;
constexpr uint16_t FLAGS2_MOUSE      = 1U << 5;
constexpr uint16_t FLAGS2_COLORS     = 1U << 6;
constexpr uint16_t FLAGS2_SOUND      = 1U << 7;
constexpr uint16_t FLAGS2_MENUS      = 1U << 8;

#define status_is_time()	(zversion == 3 && (byte(0x01) & FLAGS1_STATUSTYPE))
#define timer_available()	(zversion >= 4 && (byte(0x01) & FLAGS1_TIMED))
#define mouse_available()	(zversion == 5 && (word(0x10) & FLAGS2_MOUSE))

struct Header {
    uint16_t pc;
    uint16_t release;
    uint16_t dictionary;
    uint16_t objects;
    uint16_t globals;
    uint32_t static_start;
    uint16_t static_end;
    uint16_t abbr;
    uint32_t file_length;
    uint8_t  serial[6];
    uint16_t checksum;
    uint32_t R_O;
    uint32_t S_O;
    uint16_t terminating_characters_table;
    uint16_t extension_table;
    uint16_t extension_entries;
};

extern int zversion;
extern Header header;
extern std::array<uint8_t, 26 * 3> atable;

const std::string &get_story_id();

enum class Game {
    Infocom1234,
    Journey,
    LurkingHorror,
    Planetfall,
    Stationfall,
};

bool is_game(Game game);

void write_header();
void start_story();

uint32_t unpack_routine(uint16_t addr);
uint32_t unpack_string(uint16_t addr);
void store(uint16_t v);

void zterp_mouse_click(uint16_t x, uint16_t y);

void znop();
void zrestart();
void zquit();
void zverify();
void zpiracy();
void zsave5();
void zrestore5();

#endif
