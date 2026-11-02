//
//  taylor.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#ifndef taylor_h
#define taylor_h

#include <stdlib.h>
#include "aiukpalette.h"
#include "glk.h"

/* Shortcuts for fields of the currently loaded GameInfo record. */
#define CurrentGame (Game->gameID)
#define Version (Game->type)
#define BaseGame (Game->base_game)

/* Runtime query for whether the Glk host can show graphics. Under
   Spatterlight this is a user preference; elsewhere we ask Glk. */
#ifdef SPATTERLIGHT
#define TAYLOR_GRAPHICS_ENABLED gli_enable_graphics
#else
#define TAYLOR_GRAPHICS_ENABLED glk_gestalt(gestalt_Graphics, 0)
#endif

/* Named aliases for game-state flag slots. The slot numbers are baked
   into the Taylor data format and referenced directly by action
   opcodes, so they can't be renumbered. */
#define MyLoc (Flag[0])
#define OtherGuyLoc (Flag[1])
#define CarriedLoc (Flag[2]) /* Location code of carried objects */
#define OtherGuyInv (Flag[3])
#define MaxCarried (Flag[4])
#define ItemsCarried (Flag[5])
#define TurnsLow (Flag[26])
#define TurnsHigh (Flag[27])
#define IsThing (Flag[31])
#define ThingAsphyx (Flag[47])
#define TorchAsphyx (Flag[48])
#define DrawImages (Flag[52])
#define Q3SwitchedWatch (Flag[126])

/* Token table conventions: each byte's low 7 bits carry the character,
   and the high bit marks the final byte of a token's expansion. */
#define TOKEN_LAST_BYTE 0x80
#define TOKEN_BYTE_MASK 0x7F

/* First byte value treated as a token in QP3 text streams; raw bytes
   below this are emitted as literal characters. */
#define QP3_TOKEN_BASE 0x7b

/* QP3 text-table framing: each entry ends with QP3_ENTRY_DELIM, and the
   table itself ends with QP3_TABLE_END. */
#define QP3_ENTRY_DELIM 0x1f
#define QP3_TABLE_END   0x18

/* QP3 object-location codes: an object can be in a room (room number) or
   in one of these special locations. */
#define LOC_DESTROYED   0xfc /* Object is removed from play */
#define LOC_INV_TORCH   253  /* In Human Torch's inventory */
#define LOC_INV_THING   254  /* In Thing's inventory */

/* Message text terminators in non-QP3 versions: MSG_END ends an entry,
 MSG_END_SPACE ends it and indicates a trailing space is pending. */
#define MSG_END         0x7e
#define MSG_END_SPACE   0x5e

/* Action-table opcode byte layout: high bit marks an action byte (vs. a
   condition byte); bit 6 on an action byte stops further table scanning;
   the low 6 bits hold the opcode number. */
#define ACTION_BIT      0x80
#define ACTION_DONE_BIT 0x40
#define ACTION_OP_MASK  0x3F

/* Terminator byte that marks the end of a status or command action table. */
#define ACTION_TABLE_END 0x7F

/* Word code used as a wildcard in event-table verb/noun entries — matches
   any input word. */
#define WORD_WILDCARD    126

/* All supported game variants. Each title has separate IDs for the ZX
   Spectrum and Commodore 64 releases; Temple of Terror also has a
   text-only/hybrid split. */
typedef enum {
    QUESTPROBE3,
    QUESTPROBE3_64,
    REBEL_PLANET,
    REBEL_PLANET_64,
    BLIZZARD_PASS,
    HEMAN,
    HEMAN_64,
    TEMPLE_OF_TERROR,
    TEMPLE_OF_TERROR_64,
    TOT_TEXT_ONLY,
    TOT_HYBRID,
    TOT_TEXT_ONLY_64,
    TOT_HYBRID_64,
    KAYLETH,
    KAYLETH_64,
    UNKNOWN_GAME,
    NUMGAMES
} GameIDType;

/* The game's own system messages (compass directions, "Inventory", "OK",
   "You can't go that way", etc.) by their number in its message table;
   SysMessage() maps them for Questprobe 3. The interpreter's own messages
   (undo, transcript, save states) are not in the games and are written
   out where they are printed. */
typedef enum {
    YOU_SEE,
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    NORTHEAST,
    NORTHWEST,
    SOUTHEAST,
    SOUTHWEST,
    I_DONT_UNDERSTAND,
    THATS_BEYOND_MY_POWER,
    EXITS,
    WHAT_NOW,
    YOURE_CARRYING_TOO_MUCH,
    INVENTORY,
    NOTHING,
    PLAY_AGAIN,
    OKAY,
    HIT_ENTER,
    YOU_HAVE_IT,
    YOU_DONT_SEE_IT,
    YOU_HAVENT_GOT_IT,
    YOU_CANT_GO_THAT_WAY,
    TOO_DARK_TO_SEE,
    RESUME_A_SAVED_GAME,
    EMPTY_MESSAGE,
    YOU_ARE_NOT_WEARING_IT,
    YOU_ARE_WEARING_IT,
    NOWWORN,
    NO_HELP_AVAILABLE,
    I_DONT_UNDERSTAND_THAT_VERB,
} SysMessageType;

/* Condition opcodes from Taylor's action tables. Values match the
   byte codes stored in the original game data and must stay in order. */
typedef enum {
    CONDITIONERROR,
    AT,
    NOTAT,
    ATGT,
    ATLT,
    PRESENT,
    HERE,
    ABSENT,
    NOTHERE,
    CARRIED,
    NOTCARR,
    WORN,
    NOTWORN,
    CREATED,
    NOTCREATED,
    ZERO,
    NOTZERO,
    W3,
    W4,
    W5,
    CHANCE,
    LT,
    GT,
    EQ,
    NE,
    OBJECT,
    COND26,
    COND27,
    COND28,
    COND29,
    COND30,
    COND31,
} ConditionType;

/* Action opcodes from Taylor's action tables. Values match the byte
   codes stored in the original game data and must stay in order. */
typedef enum {
    ACTIONERROR,
    RESTORE,
    END,
    INVENT,
    ANYKEY,
    SAVE,
    DROPALL,
    LOOK,
    OK,
    GET,
    DROP,
    GOTO,
    GOBY,
    SET,
    CLEAR,
    MESSPRINT,
    CREATE,
    DESTROY,
    PRINT,
    DELAY,
    WEAR,
    REMOVE,
    LET,
    PLUS,
    MINUS,
    PLACE,
    SWAP,
    SWAPF,
    MEANS,
    PUTWITH,
    BEEP,
    REFRESH,
    RAMSAVE,
    RAMLOAD,
    CLSLOW,
    OOPS,
    WATCH,
    SWITCH,
    GOTOH,
    PICTURE,
    ACT40,
    ACT41,
    ACT42,
    ACT43,
    ACT44,
    ACT45,
    ACT46,
    ACT47,
    ACT48,
    ACT49,
    ACT50,
} ActionType;

/* Bitfield of runtime options, mostly driven by user preferences. */
typedef enum {
    DEBUGGING = 1, /* Info from database load */
    NO_DELAYS = 2, /* Skip all pauses */
    FORCE_PALETTE_ZX = 4, /* Force ZX Spectrum image palette */
    FORCE_PALETTE_C64 = 8, /* Force CBM 64 image palette */
    FORCE_INVENTORY = 16, /* Inventory in upper window always on */
    FORCE_INVENTORY_OFF = 32 /* Inventory in upper window always off */
} OptionsType;

/* Coarse data-format family. Several game IDs may share one format,
   so loader/interpreter quirks switch on this rather than on gameID. */
typedef enum {
    NO_TYPE,
    REBEL_PLANET_TYPE,
    BLIZZARD_PASS_TYPE,
    HEMAN_TYPE,
    QUESTPROBE3_TYPE
} GameVersion;

/* Per-game metadata: title, identity, and the file offsets the loader
   needs to locate each table inside the raw game data. */
typedef struct {
    const char *Title;

    GameIDType gameID;
    GameVersion type;
    GameIDType base_game;

    int start_of_dictionary;
    int start_of_tokens;
    /* The first TOKEN_SIGNATURE_LENGTH bytes of the token table, for the
       games where it is found by looking for them. */
    const char *token_signature;
    int start_of_room_descriptions;
    int start_of_item_descriptions;
    int start_of_status_table;
    int start_of_event_table;
    int start_of_room_connections;
    int start_of_flags;
    int start_of_item_locations;
    int start_of_messages;
    int start_of_messages_2;

    int start_of_tiles;
    int start_of_image_blocks;
    int image_patterns_lookup;
    int number_of_patterns;
    int pattern_end_marker;
    int start_of_image_instructions;
    int number_of_pictures;
    palette_type palette;
} GameInfo;

#define TOKEN_SIGNATURE_LENGTH 10

/* Registry of all supported games. The rows are not in GameIDType order:
   use FindGameByID(). */
extern GameInfo games[NUMGAMES];

#include "actions.h"
#include "gameload.h"
#include "player.h"
#include "textoutput.h"
#include "ui.h"

#endif /* taylor_h */
