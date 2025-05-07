// vim: set ft=cpp:

#ifndef ZTERP_ZTERP_H
#define ZTERP_ZTERP_H

#include <array>
#include <exception>
#include <string>

#include "types.h"

class Exit : public std::exception {
public:
    explicit Exit(int code) : m_code(code) {
    }

    int code() const {
        return m_code;
    }

private:
    int m_code;
};

extern std::string game_file;

#define ZTERP_VERSION	"2.2.4"

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
#define mouse_available()	(zversion >= 5 && (word(0x10) & FLAGS2_MOUSE))

struct Header {
    uint16_t pc;
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
    uint16_t terminating_characters_table;
    uint16_t extension_table;
    uint16_t extension_entries;
};

extern int zversion;
extern Header header;
extern std::array<uint8_t, 26 * 3> atable;

const std::string &get_story_id();

enum class Game {
    Arthur,
    Infocom1234,
    Journey,
    LurkingHorror,
    Planetfall,
    Shogun,
    Stationfall,
    ZorkZero,
    MysteriousAdventures,
};

bool is_game(Game game);

void write_header();
void start_story();

uint32_t unpack_routine(uint16_t addr);
uint32_t unpack_string(uint16_t addr);
void store(uint16_t v);

void zterp_mouse_click(uint16_t x, uint16_t y);

void znop();
[[noreturn]]
void zrestart();
[[noreturn]]
void zquit();
void zverify();
void zpiracy();
void zsave5();
void zrestore5();

#endif
