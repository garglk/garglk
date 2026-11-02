/*
 *	SCOTT FREE
 *
 *	A free Scott Adams style adventure interpreter
 *
 *	Copyright:
 *	    This software is placed under the GNU license.
 *
 *	Statement:
 *	    Everything in this program has been deduced or obtained solely
 *	from published material. No game interpreter code has been
 *	disassembled, only published BASIC sources (PC-SIG, and Byte Dec
 *	1980) have been used.
 */

/*
 *	Controlling block
 */

#ifndef scott_h
#define scott_h

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "glk.h"

#include "debugprint.h"
#include "memory_allocation.h"
#include "read_le16.h"
#include "common_utils.h"
#include "scott_defines.h"

// clang-format off

#define LIGHT_SOURCE 9         /* Always 9 how odd */
#define CARRIED      255       /* Carried */
#define DESTROYED    0         /* Destroyed */
#define DARKBIT      15
#define LIGHTOUTBIT  16        /* Light gone out */

// clang-format on

/* Number of exit directions per room (N/S/E/W/U/D) */
#define NUM_EXITS 6

/* Number of counter/room-flag save slots */
#define NUM_COUNTERS 16

/* Room image sentinel: no image assigned */
#define NO_IMAGE 255

/* Mask to extract the image index from a room/item image byte */
#define IMAGE_INDEX_MASK 127

/* Brian Howarth vector graphics format identifier */
#define HOWARTH_FORMAT 99

/* Action table encoding: vocab = verb * VOCAB_MULTIPLIER + noun,
   condition = param * CONDITION_MULTIPLIER + condition_code */
#define VOCAB_MULTIPLIER 150
#define CONDITION_MULTIPLIER 20
#define NUM_CONDITIONS 5
#define NUM_OPCODES 4

/* Display/buffer sizes */
#define DISPLAY_BUFFER_SIZE 2048
#define ROOM_DESC_BUFFER_SIZE 1000

typedef struct {
    short Unknown;
    short NumItems;
    short NumActions;
    short NumWords; /* Smaller of verb/noun is padded to same size */
    short NumRooms;
    short MaxCarry;
    short PlayerRoom;
    short Treasures;
    short WordLength;
    short LightTime;
    short NumMessages;
    short TreasureRoom;
} Header;

typedef struct {
    unsigned short Vocab;
    unsigned short Condition[NUM_CONDITIONS];
    unsigned short Opcode[2];
} Action;

typedef struct {
    char *Text;
    uint8_t Exits[NUM_EXITS];
    uint8_t Image;
} Room;

typedef struct {
    char *Text;
    /* PORTABILITY WARNING: THESE TWO MUST BE 8 BIT VALUES. */
    uint8_t Location;
    uint8_t InitialLoc;
    const char *AutoGet;
    uint8_t Flag;
    uint8_t Image;
} Item;

// clang-format off

#define YOUARE                 0x1     /* You are not I am */
#define SCOTTLIGHT             0x2     /* Authentic Scott Adams light messages */
#define DEBUGGING              0x4     /* Info from database load */
#define TRS80_STYLE            0x8     /* Display in style used on TRS-80 */
#define PREHISTORIC_LAMP      0x10     /* Destroy the lamp (very old databases) */
#define SPECTRUM_STYLE        0x20     /* Display in style used on ZX Spectrum */
#define TI994A_STYLE          0x40     /* Display in style used on TI-99/4A */
#define NO_DELAYS             0x80     /* Skip all pauses */
#define FORCE_PALETTE_ZX     0x100     /* Force ZX Spectrum image palette */
#define FORCE_PALETTE_C64    0x200     /* Force CBM 64 image palette */
#define FORCE_INVENTORY      0x400     /* Inventory in upper window always on */
#define FORCE_INVENTORY_OFF  0x800     /* Inventory in upper window always off */
#define PC_STYLE            0x1000     /* Display in style used on IBM PC (MS-DOS) */
#define FLICKER_ON          0x2000     /* Flicker room description like some original games */

// clang-format on

#define MAX_GAMEFILE_SIZE 250000

/* Anything used by the other source files goes here */

#define MyLoc (GameHeader.PlayerRoom)

#define CurrentGame (Game->gameID)

void Output(const char *a);
void Display(winid_t w, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;
void vDisplay(winid_t w, const char *fmt, va_list ap)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 2, 0)))
#endif
    ;
void PutUnicodeString(strid_t stream, const char *string);
GameIDType LoadDatabase(FILE *f);
void Updates(event_t ev);
const char *MapSynonym(int noun);
void UpdateSettings(void);
void FreeDatabase(void);

extern GameInfo *Game;
extern Header GameHeader;
extern Room *Rooms;
extern Item *Items;
extern Action *Actions;
extern const char **Verbs, **Nouns, **Messages;
extern char *title_screen;
extern uint8_t *ZXLoadingScreen;
extern winid_t Bottom, Top, Graphics;
extern const char *sys[];
extern const char *system_messages[];
extern uint8_t *entire_file;
extern size_t file_length;
extern int file_baseline_offset;
extern int split_screen;
extern uint64_t BitFlags;
extern int LightRefill;
extern int Counters[];
extern int AnimationFlag;
extern int SavedRoom;
extern int AutoInventory;
extern int CurrentCounter;
extern int RoomSaved[];
extern int Options;
/* The load trace of the -d option (DEBUGGING): what the database loaders
   read, on stderr. */
#define LoadTrace(...)                    \
    do {                                  \
        if (Options & DEBUGGING)          \
            fprintf(stderr, __VA_ARGS__); \
    } while (0)
extern int StopTime;
/* StopTime value that keeps the implicit actions from running on the next
   turn (e.g. after SCORE or INVENTORY). */
#define SKIP_IMPLICIT_TURNS 2
extern int should_look_in_transcript;
extern int ImageWidth;
extern int ImageHeight;
extern const char *game_file;
extern int showing_inventory;
extern MachineType CurrentSys;
extern int lastwasnewline;
extern char *DirPath;
extern int showing_closeup;
extern int last_image_index;
extern int gli_slowdraw;
extern int should_draw_image;
struct Command;
extern struct Command *CurrentCommand;
extern strid_t Transcript;
extern glui32 TopWidth, TopHeight;
extern int JustStarted;
extern int should_restart;

GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void);
int MatchUpItem(int noun, int loc);
void ParseItemSlashAutoGet(int index);

/* Is the item carried or in the current room? */
static inline int IsPresent(int item)
{
    return Items[item].Location == CARRIED || Items[item].Location == MyLoc;
}

#endif /* scott_h */
