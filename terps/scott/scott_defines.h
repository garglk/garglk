//
//  scott_defines.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef scott_defines_h
#define scott_defines_h

#include <stdint.h>

#include "aiukpalette.h"
#include "minmax.h"
#include "common_defines.h"
#include "scott_opcodes.h"
#include "scott_sysmess.h"

/* Sentinel used in the parsed-command stream to mean "the next action
   line continues this one" (i.e. multi-line action with a CONTINUE op). */
#define FOLLOWS 0xffff

/* Built-in verb codes that the interpreter needs to recognize directly
   for movement and ALL-handling, regardless of the game's dictionary. */
#define GO 1
#define TAKE 10
#define DROP 18

/* Sentinel noun value meaning "the player typed ALL" (TAKE/DROP ALL). */
#define LASTALL 128

/* US-variant game-specific image */
#define US_CLOSEUP_IMAGE 80

/* "Return to Pirate's Isle" winning-screen image index. */
#define RTPI_WIN_IMAGE 26

/* All supported games and ports. Each title has separate IDs for its
   different releases (US edition, C64, localized translations, etc.)
   because the interpreter applies per-release quirks. */
typedef enum {
    UNKNOWN_GAME,
    ADVENTURELAND_US = 1,
    PIRATE_US = 2,
    SECRET_MISSION_US = 3,
    VOODOO_CASTLE_US = 4,
    COUNT_US = 5,
    STRANGE_ODYSSEY_US = 6,
    MYSTERY_FUNHOUSE_US = 7,
    PYRAMID_OF_DOOM_US = 8,
    GHOST_TOWN_US = 9,
    SAVAGE_ISLAND_US = 10,
    SAVAGE_ISLAND_2_US = 11,
    GOLDEN_VOYAGE_US = 12,
    CLAYMORGUE_US = 13,
    CLAYMORGUE_US_126,
    RETURN_TO_PIRATES_ISLE,
    ADVENTURELAND,
    PIRATE,
    SECRET_MISSION,
    VOODOO_CASTLE,
    COUNT,
    STRANGE_ODYSSEY,
    MYSTERY_FUNHOUSE,
    PYRAMID_OF_DOOM,
    GHOST_TOWN,
    SAVAGE_ISLAND,
    SAVAGE_ISLAND2,
    GOLDEN_VOYAGE,
    CLAYMORGUE,
    BANZAI,
    BATON,
    BATON_C64,
    TIME_MACHINE,
    TIME_MACHINE_C64,
    ARROW1,
    ARROW1_C64,
    ARROW2,
    ARROW2_C64,
    PULSAR7,
    PULSAR7_C64,
    CIRCUS,
    CIRCUS_C64,
    FEASIBILITY,
    FEASIBILITY_C64,
    AKYRZ,
    AKYRZ_C64,
    PERSEUS,
    PERSEUS_C64,
    PERSEUS_ITALIAN,
    INDIANS,
    INDIANS_C64,
    WAXWORKS,
    WAXWORKS_C64,
    HULK,
    HULK_C64,
    HULK_US,
    HULK_US_PREL,
    ADVENTURELAND_C64,
    SECRET_MISSION_C64,
    CLAYMORGUE_C64,
    SPIDERMAN,
    SPIDERMAN_C64,
    SPIDERMAN_ATARI8,
    SAVAGE_ISLAND_C64,
    SAVAGE_ISLAND2_C64,
    GREMLINS,
    GREMLINS_C64,
    GREMLINS_GERMAN,
    GREMLINS_GERMAN_C64,
    GREMLINS_SPANISH,
    GREMLINS_SPANISH_C64,
    SUPERGRAN,
    SUPERGRAN_C64,
    ROBIN_OF_SHERWOOD,
    ROBIN_OF_SHERWOOD_C64,
    SEAS_OF_BLOOD,
    SEAS_OF_BLOOD_C64,
    SCOTTFREE,
    TI994A,
    NUMGAMES
} GameIDType;

/* Outcome of running an action table to completion: success, or one of
   two "exhausted without matching" variants for the caller to handle. */
typedef enum {
    ER_SUCCESS = 0,
    ER_NO_RESULT = 1,
    ER_RAN_ALL_LINES_NO_MATCH = -1,
    ER_RAN_ALL_LINES = -2
} ExplicitResultType;

/* Result of executing a single action line — drives the action-table
   loop (continue scanning, stop, or end the game). */
typedef enum {
    ACT_SUCCESS = 0,
    ACT_FAILURE = 1,
    ACT_CONTINUE,
    ACT_GAMEOVER
} ActionResultType;

/* Source platform of the loaded game image. Used to switch between
   format-specific loaders and graphics decoders. */
typedef enum {
    SYS_UNKNOWN,
    SYS_MSDOS,
    SYS_C64,
    SYS_ATARI8,
    SYS_APPLE2,
    SYS_TI994A,
    SYS_APPLE2_LINES,
    SYS_ATARI8_LINES,
    SYS_C64_TINY
} MachineType;

/* Categories of pictures in the US releases: room-only vs. room+object
   overlays vs. inventory close-ups. */
typedef enum {
    IMG_ROOM,
    IMG_ROOM_OBJ,
    IMG_INV_OBJ,
    IMG_INV_AND_ROOM_OBJ
} USImageType;

/* Dictionary encoding used by the game. Word length and compression
   vary across versions; the loader picks the right reader from this. */
typedef enum {
    NOT_A_GAME,
    FOUR_LETTER_UNCOMPRESSED,
    THREE_LETTER_UNCOMPRESSED,
    FIVE_LETTER_UNCOMPRESSED,
    FOUR_LETTER_COMPRESSED,
    GERMAN,
    GERMAN_C64,
    SPANISH,
    SPANISH_C64,
    ITALIAN
} DictionaryType;

/* Coarse "game family" used to dispatch per-family interpreter quirks.
   A given GameIDType maps to exactly one of these. */
typedef enum {
    NO_TYPE,
    GREMLINS_VARIANT,
    SHERWOOD_VARIANT,
    SAVAGE_ISLAND_VARIANT,
    SECRET_MISSION_VARIANT,
    SEAS_OF_BLOOD_VARIANT,
    US_VARIANT,
    OLD_STYLE
} GameType;

/* Bitfield describing language and target platform of a game image. */
typedef enum {
    ENGLISH = 0x1,
    MYSTERIOUS = 0x2,
    LOCALIZED = 0x4,
    C64 = 0x8
} Subtype;

/* Layout of the data header (table sizes and counts) at the start of
   the game image. Each shipped variant used a slightly different one. */
typedef enum {
    NO_HEADER,
    EARLY,
    LATE,
    US_HEADER,
    GREMLINS_C64_HEADER,
    ROBIN_C64_HEADER,
    SUPERGRAN_C64_HEADER,
    SEAS_OF_BLOOD_C64_HEADER,
    MYSTERIOUS_C64_HEADER,
    ARROW_OF_DEATH_PT_2_C64_HEADER,
    INDIANS_C64_HEADER,
    SPIDERMAN_ATARI8_HEADER
} HeaderType;

/* Encoding of the action table inside the game image. Hulk has a
   custom format; other games are either packed (COMPRESSED) or not. */
typedef enum {
    COMPRESSED,
    UNCOMPRESSED,
    HULK_ACTIONS
} ActionTableType;

/* Per-game metadata record: title, identity, table sizes and the file
   offsets the loader uses to locate every table in the raw image. */
typedef struct {
    const char *Title;

    GameIDType gameID;
    GameType type;
    int subtype;
    DictionaryType dictionary;

    int number_of_items;
    int number_of_actions;
    int number_of_words;
    int number_of_rooms;
    int max_carried;
    int word_length;
    int number_of_messages;

    int number_of_verbs;
    int number_of_nouns;

    int start_of_header;
    HeaderType header_style;

    int start_of_room_image_list;
    int start_of_item_flags;
    int start_of_item_image_list;

    int start_of_actions;
    ActionTableType actions_style;
    int start_of_dictionary;
    int start_of_room_descriptions;
    int start_of_room_connections;
    int start_of_messages;
    int start_of_item_descriptions;
    int start_of_item_locations;

    int start_of_system_messages;
    int start_of_directions;

    int start_of_tiles;
    int start_of_image_data;
    int image_address_offset; /* This is the difference between the value given by
                               the image data lookup table and a usable file
                               offset */
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
    int start_of_intro_text;
} GameInfo;

#endif /* scott_defines_h */
