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

#define DEBUG_ACTIONS 0

#define debug_print(fmt, ...)                    \
    do {                                         \
        if (DEBUG_ACTIONS)                       \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

#define FOLLOWS 0xffff

#define MAX_GAMEFILE_SIZE 200000

#define MAX_WORDLENGTH 128
#define MAX_WORDS 128

#define MAX_BUFFER 512

#define YOUARE 1 /* You are not I am */
#define DEBUGGING 4 /* Info from database load */
#define NO_DELAYS 128 /* Skip all pauses */
#define FORCE_INVENTORY 1024 /* Inventory in upper window always on */
#define FORCE_INVENTORY_OFF 2048 /* Inventory in upper window always off */

#define NounObject (Counters[30])
#define Noun2Object (Counters[31])
#define CurrentCounter (Counters[47])

#define MyLoc (Counters[32])
#define CurVerb (Counters[48])
#define CurNoun (Counters[49])
#define CurPrep (Counters[50])
#define CurAdverb (Counters[51])
#define CurPartp (Counters[52])
#define CurNoun2 (Counters[53])

#define CARRIED 255
#define HELD_BY_OTHER_GUY 99
#define DESTROYED 0
#define HIDDEN 50

#define DARKBIT 15
#define MATCHBIT 33
#define DRAWBIT 34
#define GRAPHICSBIT 35
#define ALWAYSMATCH 60
#define STOPTIMEBIT 63

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

typedef struct {
    char *Text;
    int Exits[8];
    int Image;
} Room;

typedef struct {
    char *Text;
    uint8_t Location;
    uint8_t InitialLoc;
    uint8_t Flag;
    uint8_t Dictword;
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

typedef struct imgrec {
    char *Filename;
    uint8_t *Data;
    size_t DiskOffset;
    size_t Size;
} imgrec;

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
    HIT_ANY,
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
    NO_PALETTE,
    ZX,
    ZXOPT,
    C64A,
    C64B,
    VGA } palette_type;

struct GameInfo {
    const char *title;
    const char *ID_string;
    GameIDType gameID;

    int no_of_room_images;
    int no_of_item_images;
    int no_of_special_images;
};

#endif /* definitions_h */
