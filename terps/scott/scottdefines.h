//
//  scottdefines.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef scottdefines_h
#define scottdefines_h

#include <stdint.h>

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define FOLLOWS 0xffff

#define GO 1
#define TAKE 10
#define DROP 18

#define LASTALL 128

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

typedef enum {
    ER_NO_RESULT,
    ER_SUCCESS = 0,
    ER_RAN_ALL_LINES_NO_MATCH = -1,
    ER_RAN_ALL_LINES = -2
} ExplicitResultType;

typedef enum {
    ACT_SUCCESS = 0,
    ACT_FAILURE = 1,
    ACT_CONTINUE,
    ACT_GAMEOVER
} ActionResultType;

typedef enum {
    SYS_UNKNOWN,
    SYS_MSDOS,
    SYS_C64,
    SYS_ATARI8,
    SYS_APPLE2,
    SYS_TI994A
} MachineType;


typedef enum {
    IMG_ROOM,
    IMG_ROOM_OBJ,
    IMG_INV_OBJ,
} USImageType;

typedef enum {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    PLAY_AGAIN,
    IVE_STORED,
    TREASURES,
    ON_A_SCALE_THAT_RATES,
    DROPPED,
    TAKEN,
    OK,
    YOUVE_SOLVED_IT,
    I_DONT_UNDERSTAND,
    YOU_CANT_DO_THAT_YET,
    HUH,
    DIRECTION,
    YOU_HAVENT_GOT_IT,
    YOU_HAVE_IT,
    YOU_DONT_SEE_IT,
    THATS_BEYOND_MY_POWER,
    DANGEROUS_TO_MOVE_IN_DARK,
    YOU_FELL_AND_BROKE_YOUR_NECK,
    YOU_CANT_GO_THAT_WAY,
    I_DONT_KNOW_HOW_TO,
    SOMETHING,
    I_DONT_KNOW_WHAT_A,
    IS,
    TOO_DARK_TO_SEE,
    YOU_ARE,
    YOU_SEE,
    EXITS,
    INVENTORY,
    NOTHING,
    WHAT_NOW,
    HIT_ENTER,
    LIGHT_HAS_RUN_OUT,
    LIGHT_RUNS_OUT_IN,
    TURNS,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    RESUME_A_SAVED_GAME,
    NONE,
    NOTHING_HERE_TO_TAKE,
    YOU_HAVE_NOTHING,
    LIGHT_GROWING_DIM,
    EXITS_DELIMITER,
    MESSAGE_DELIMITER,
    ITEM_DELIMITER,
    WHAT,
    YES,
    NO,
    ANSWER_YES_OR_NO,
    ARE_YOU_SURE,
    MOVE_UNDONE,
    CANT_UNDO_ON_FIRST_TURN,
    NO_UNDO_STATES,
    SAVED,
    CANT_USE_ALL,
    TRANSCRIPT_OFF,
    TRANSCRIPT_ON,
    NO_TRANSCRIPT,
    TRANSCRIPT_ALREADY,
    FAILED_TRANSCRIPT,
    TRANSCRIPT_START,
    TRANSCRIPT_END,
    BAD_DATA,
    STATE_SAVED,
    STATE_RESTORED,
    NO_SAVED_STATE,
    LAST_SYSTEM_MESSAGE
} SysMessageType;

#define MAX_SYSMESS LAST_SYSTEM_MESSAGE

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

typedef enum {
    NO_TYPE,
    GREMLINS_VARIANT,
    SHERWOOD_VARIANT,
    SAVAGE_ISLAND_VARIANT,
    SECRET_MISSION_VARIANT,
    SEAS_OF_BLOOD_VARIANT,
    US_VARIANT,
    OLD_STYLE,
} GameType;

typedef enum {
    ENGLISH = 0x1,
    MYSTERIOUS = 0x2,
    LOCALIZED = 0x4,
    C64 = 0x8
} Subtype;

typedef enum { NO_PALETTE, ZX, ZXOPT, C64A, C64B, C64C, VGA } palette_type;

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
    INDIANS_C64_HEADER
} HeaderType;

typedef enum {
    UNKNOWN_ACTIONS_TYPE,
    COMPRESSED,
    UNCOMPRESSED,
    HULK_ACTIONS
} ActionTableType;

struct GameInfo {
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

    int start_of_characters;
    int start_of_image_data;
    int image_address_offset; /* This is the difference between the value given by
                               the image data lookup table and a usable file
                               offset */
    int number_of_pictures;
    palette_type palette;
    int picture_format_version;
    int start_of_intro_text;
};

#endif /* scottdefines_h */
