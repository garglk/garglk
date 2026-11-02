//
//  definitions.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef definitions_h
#define definitions_h

#include <stddef.h>
#include <stdint.h>

#include "common_defines.h"
#include "minmax.h"

#define NO_DELAYS 128 /* Skip all pauses */
#define FORCE_INVENTORY 1024 /* Inventory in upper window always on */
#define FORCE_INVENTORY_OFF 2048 /* Inventory in upper window always off */

/* Argument values in conditions and commands that stand for what the
   player typed rather than for a fixed item or dictionary word */
#define ARG_WITH_LIST 997 /* A noun of the "with" list, or the item in counter 45 */
#define ARG_NOUN 998      /* The current noun, or the item it refers to */
#define ARG_NOUN2 999     /* The second noun, or the item it refers to */

/* The number of counters (16-bit registers) the action table can use */
#define NUM_COUNTERS 64

/* The counters the system itself uses. Counters 0-29 and 54-59 belong to the game. 36-41 are "old holding rooms" that games may redefine, and 60-62 are reserved. */
#define COUNTER_NOUN 30            /* The item ARG_NOUN stands for */
#define COUNTER_NOUN2 31           /* The item ARG_NOUN2 stands for */
#define COUNTER_ROOM 32            /* The current room */
#define COUNTER_INVENTORY 33       /* Number of items carried, as of the last Get (action 52) */
#define COUNTER_TREASURE_ROOM 34
#define COUNTER_DEAD_ROOM 35       /* Where the player goes on dying, initially the last room */
#define COUNTER_MAX_CARRY 42       /* The number of items that can be carried */
#define COUNTER_NUM_ITEMS 43       /* The number of the last item */
#define COUNTER_WITH_INDEX 44      /* Which noun of the "with" list ARG_WITH_LIST stands for */
#define COUNTER_WITH_OBJECT 45     /* The item ARG_WITH_LIST stands for */
#define COUNTER_WITH_COUNT 46      /* The number of nouns in the "with" list */
#define COUNTER_MAIN 47            /* The "main counter" of actions 77-83 */
#define COUNTER_VERB 48            /* 48-53: the words of the command (actions 120, 121) */
#define COUNTER_NOUN_WORD 49
#define COUNTER_PREP 50
#define COUNTER_ADVERB 51
#define COUNTER_PARTP 52
#define COUNTER_NOUN2_WORD 53
#define COUNTER_ITEM_REGISTER 63   /* Item flags and places for action 110-9 and 110-12 */

#define NounObject (Counters[COUNTER_NOUN])
#define Noun2Object (Counters[COUNTER_NOUN2])
#define MyLoc (Counters[COUNTER_ROOM])
#define InventoryCount (Counters[COUNTER_INVENTORY])
#define TreasureRoomCounter (Counters[COUNTER_TREASURE_ROOM])
#define DeadRoom (Counters[COUNTER_DEAD_ROOM])
#define CarryLimit (Counters[COUNTER_MAX_CARRY])
#define LastItem (Counters[COUNTER_NUM_ITEMS])
#define WithListIndex (Counters[COUNTER_WITH_INDEX])
#define WithListObject (Counters[COUNTER_WITH_OBJECT])
#define WithListCount (Counters[COUNTER_WITH_COUNT])
#define CurrentCounter (Counters[COUNTER_MAIN])
#define VerbCounter (Counters[COUNTER_VERB])
#define NounCounter (Counters[COUNTER_NOUN_WORD])
#define PrepCounter (Counters[COUNTER_PREP])
#define AdverbCounter (Counters[COUNTER_ADVERB])
#define PartpCounter (Counters[COUNTER_PARTP])
#define Noun2Counter (Counters[COUNTER_NOUN2_WORD])
#define ItemRegister (Counters[COUNTER_ITEM_REGISTER])

#define CARRIED 255
#define NO_ROOM_IMAGE 255 /* EXIT_IMAGE of a room without a picture */
#define DESTROYED 0

/* The flags the system itself uses. The rest belong to the game; 61 and 62 are reserved. */
#define FLAG0BIT 0        /* The flag of actions 67 and 68 */
#define DARKBIT 15
#define MATCHBIT 33       /* A command matched the words, but its logics failed */
#define DRAWBIT 34        /* The room picture needs redrawing */
#define GRAPHICSBIT 35    /* Graphics are on */
#define ALWAYSMATCH 60    /* The first command of the table matches whatever is typed */
#define STOPTIMEBIT 63    /* Skip the automatic commands once */

/* Flags that particular games use, which the interpreter has to know */
#define BANZAI_BLACKOUT_BIT 6     /* IsSet: room 29 stays black after its animation */
#define SPIDERMAN_WEB_LOADED_BIT 9      /* Web animation is loaded */
#define SPIDERMAN_SANDMAN_LOADED_BIT 10 /* Sandman animation is loaded */

#define CurrentGame (Game->gameID)

typedef struct {
    char *Word;
    uint8_t Group;
} DictWord;

typedef enum {
    WORD_NOT_FOUND,
    OTHER_TYPE,
    VERB_TYPE,
    NOUN_TYPE,
    ADVERB_TYPE,
    PREPOSITION_TYPE,
    NUM_WORD_TYPES,
} WordType;

typedef enum {
    NO_IMG,
    IMG_ROOM,
    IMG_OBJECT,
    IMG_SPECIAL,
} ImgType;

typedef struct {
    WordType Type;
    uint8_t Index;
} WordToken;

typedef struct {
    char *SynonymString;
    char *ReplacementString;
} Synonym;

typedef struct {
    uint8_t Verb;
    uint8_t NounOrChance;
    uint8_t *Words;
    uint16_t *Conditions;
    uint8_t *Commands;
    uint8_t NumWords;
    uint8_t CommandLength;
} Action;

/* The columns of the room table. GETEXIT and SETEXIT take the column from
   a counter, so the games can reach all eight, not just the directions. */
#define NUM_DIRECTIONS 6
#define EXIT_DESCRIPTION 6 /* the description message, as a command number */
#define EXIT_IMAGE 7       /* the room picture */
#define NUM_EXIT_COLUMNS 8

typedef struct {
    char *Text;
    int Exits[NUM_EXIT_COLUMNS];
} Room;

typedef struct {
    char *Text;
    /* 16 bits: X-Men sends used-up items to "room 500". */
    uint16_t Location;
    uint16_t InitialLoc;
    uint16_t Flag;
    uint8_t Dictword; /* noun group; 16 bits in the files, see ItemNounWord */
} Item;

typedef struct {
    char *Title;
    short NumItems;
    short ActionSum;
    short NumNouns;
    short NumVerbs;
    short NumRooms;
    short MaxCarry;
    short PlayerRoom;
    short NumMessages;
    short TreasureRoom;
    short LightTime;
    short NumPreps;
    short NumAdverbs;
    short NumActions;
    short Treasures;
    short NumSubStr;
    short Unknown1;
    short NumObjImg;
    short Unknown2;
} Header;

typedef struct {
    uint32_t Room;
    uint32_t Object;
    uint32_t Image;
} ObjectImage;

typedef struct {
    char *Filename;
    uint8_t *Data;
    size_t DiskOffset;
    size_t Size;
} imgrec;

/* Where on a disk without a file system a picture is found */
typedef struct imglist {
    const char *filename;
    size_t offset;
    /* Nonzero if the picture is on the disk with the database rather than
       on the picture disk (Atari 8-bit only) */
    int on_boot_disk;
} imglist;

typedef enum {
    UNKNOWN_GAME,
    BANZAI,
    CLAYMORGUE,
    SPIDERMAN,
    FANTASTIC4,
    XMEN,
    NUMGAMES
} GameIDType;

typedef enum {
    ER_SUCCESS = 0,
    ER_NO_RESULT = 1,
    ER_RAN_ALL_LINES_NO_MATCH = -1,
    ER_RAN_ALL_LINES = -2
} CommandResultType;

typedef enum {
    ACT_SUCCESS = 0,
    ACT_FAILURE = 1,
    ACT_CONTINUE,
    ACT_DONE,
    ACT_LOOP_BEGIN,
    ACT_LOOP,
    ACT_GAMEOVER
} ActionResultType;

typedef enum {
    SYS_UNKNOWN,
    SYS_MSDOS,
    SYS_C64,
    SYS_ATARI8,
    SYS_APPLE2,
    SYS_ST
} SystemType;

typedef enum {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    PLAY_AGAIN,
    OK,
    I_DONT_UNDERSTAND,
    YOU_CANT_DO_THAT_YET,
    HUH,
    TOO_DARK_TO_SEE,
    YOU_ARE,
    YOU_SEE,
    EXITS,
    INVENTORY,
    NOTHING,
    WHAT_NOW,
    HIT_ANY,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    RESUME_A_SAVED_GAME,
    NONE,
    EXITS_DELIMITER,
    ITEM_DELIMITER,
    YES,
    NO,
    ANSWER_YES_OR_NO,
    ARE_YOU_SURE,
    MOVE_UNDONE,
    CANT_UNDO_ON_FIRST_TURN,
    NO_UNDO_STATES,
    SAVED,
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
    NO_PREVIOUS_ACTION,
    LAST_SYSTEM_MESSAGE
} SysMessageType;

#define MAX_SYSMESS LAST_SYSTEM_MESSAGE

typedef struct  {
    const char *title;
    const char *ID_string;
    GameIDType gameID;

    int no_of_room_images;
    int no_of_item_images;
    int no_of_special_images;
} GameInfo;

#endif /* definitions_h */
