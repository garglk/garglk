/*
 *	  ScottFree Revision 1.14
 *
 *
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 *
 *
 *	  You must have an ANSI C compiler to build this program.
 */

/*
 * Parts of this source file (mainly the Glk parts) are copyright 2011
 * Chris Spiegel.
 *
 * Some notes about the Glk version:
 *
 * o Room descriptions, exits, and visible items can, as in the
 *	 original, be placed in a window at the top of the screen, or can be
 *	 inline with user input in the main window.	 The former is default,
 *	 and the latter can be selected with the -w flag.
 *
 * o Game saving and loading uses Glk, which means that providing a
 *	 save game file on the command-line will no longer work.  Instead,
 *	 the verb "restore" has been special-cased to call GameLoad(), which
 *	 now prompts for a filename via Glk.
 *
 * o The local character set is expected to be compatible with ASCII, at
 *	 least in the printable character range.  Newlines are specially
 *	 handled, however, and converted to Glk's expected newline
 *	 character.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"
#include "saga.h"
#include "title_image.h"

#include "detect_game.h"
#include "restore_state.h"
#include "sagagraphics.h"
#include "sagadraw_host.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "parser.h"

#include "game_specific.h"
#include "gremlins.h"
#include "robin_of_sherwood.h"
#include "randomness.h"

#include "bsd.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"
#include "scott_game_info.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#else
/* Only Spatterlight has a setting for this: vector pictures and loading
   screens are drawn in one go unless the -g option is given. */
int gli_slowdraw = 0;
#endif

/* Defines only used in this file */
#define READSTRING_BUFFER_SIZE 1024

/* Size of system_messages[], the raw message strings read from the game
   file. Not to be confused with MAX_SYSMESS (scott_sysmess.h), the size
   of sys[], which is indexed by SysMessageType. */
#define NUM_SYSTEM_MESSAGES 60

/* Status window size in characters */
#define TRS80_STATUS_WIDTH 64
#define TRS80_STATUS_HEIGHT 11
#define DEFAULT_STATUS_WIDTH 80
#define DEFAULT_STATUS_HEIGHT 10

#define LAMP_WARNING_THRESHOLD 25
#define LAMP_WARNING_INTERVAL 5

#define HULK_DOS_ID 703
#define HULK_DOS_WIDTH 280
#define HULK_DOS_HEIGHT 158
#define DETERMINISTIC_SEED 1234

const char *game_file = NULL;
char *DirPath = "."; /* Directory containing the game file */

/* Core game data — populated by LoadDatabase or the SAGA/Plus loaders */
Header GameHeader;
Item *Items = NULL;
Room *Rooms = NULL;
const char **Verbs = NULL;
const char **Nouns = NULL;
const char **Messages = NULL;
Action *Actions = NULL;
int LightRefill; /* Initial light duration, used when refilling the lamp */
int Counters[NUM_COUNTERS] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */
int CurrentCounter;
int SavedRoom;
int RoomSaved[NUM_COUNTERS] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */

uint64_t BitFlags = 0; /* Might be >32 flags - I haven't seen >32 yet */

int AutoInventory = 0; /* Auto-display inventory in room descriptions */
int Options; /* Option flags set */
glui32 TopWidth; /* Terminal width */
glui32 TopHeight; /* Height of top window */
int ImageWidth = 256; /* 32 character columns */
int ImageHeight = 96;
int file_baseline_offset = 0; /* Byte offset adjustment for files with extra data prepended */
char *title_screen = NULL;
uint8_t *ZXLoadingScreen = NULL; /* ZX Spectrum SCREEN$ shown as a title image, or NULL */

Command *CurrentCommand = NULL; /* Current node in the parsed command chain */
GameInfo *Game; /* Metadata for the detected game (from the game database) */
MachineType CurrentSys = SYS_UNKNOWN; /* Detected platform (DOS, C64, Spectrum, etc.) */

const char *sys[MAX_SYSMESS]; /* Active system messages (localized prompts, exits, etc.) */
const char *system_messages[NUM_SYSTEM_MESSAGES];


uint8_t *entire_file; /* Raw game file loaded into memory */
size_t file_length;

int AnimationFlag = 0;

/* Display state flags — track what's currently shown to avoid redundant redraws */
int showing_inventory = 0;
int showing_closeup = 0;
int last_image_index = -1;
int lastwasnewline = 0;
/* Set from the start, so that the first room has its picture even when no
   arrange event comes first to ask for one. */
int should_draw_image = 1;

/* JustStarted is only used for the error message "Can't undo on first move" */
int JustStarted = 1;
int should_restart = 0;
int StopTime = 0; /* When > 0, suppresses automatic (implicit) actions and decrements each turn */

int should_look_in_transcript = 0;

int split_screen = 1;
static int forced_style = -1; /* Display style number of the -t option, or -1 */
winid_t Bottom, Top;
winid_t Graphics;

strid_t Transcript = NULL;

#define TRS80_LINE \
    "\n<------------------------------------------------------------>\n"

/* Convert a string to Unicode and write it to a Glk stream */
void PutUnicodeString(strid_t stream, const char *string)
{
    glui32 *unistring = ToUnicode(string);
    glk_put_string_stream_uni(stream, unistring);
    free(unistring);
}

/* Format and output text to a Glk window, converting to Unicode */
void vDisplay(winid_t w, const char *fmt, va_list ap)
{
    char msg[DISPLAY_BUFFER_SIZE];
    vsnprintf(msg, sizeof msg, fmt, ap);

    glui32 *unistring = ToUnicode(msg);
    size_t length = 0;
    while (unistring[length] != 0)
        length++;
    glk_put_string_stream_uni(glk_window_get_stream(w), unistring);
    /* Only text in the main window counts: "take all" asks whether that
       window is at the start of a line. */
    if (w == Bottom && length > 0)
        lastwasnewline = (unistring[length - 1] == '\n');
    free(unistring);
}

void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vDisplay(w, fmt, ap);
    va_end(ap);
}

#ifdef SPATTERLIGHT
/* Apply a three-way UI setting to a pair of mutually exclusive option
   flags: 0 clears both, 1 sets the first, 2 sets the second. */
static void SetExclusiveOptions(int setting, int first, int second)
{
    switch (setting) {
    case 0:
        Options &= ~(first | second);
        break;
    case 1:
        Options = (Options | first) & ~second;
        break;
    case 2:
        Options = (Options | second) & ~first;
        break;
    }
}
#endif

/* Sync game options from the Spatterlight UI settings (delays, flicker,
   inventory display mode, palette override) and refresh the display. */
void UpdateSettings(void)
{
#ifdef SPATTERLIGHT
    if (gli_sa_delays)
        Options &= ~NO_DELAYS;
    else
        Options |= NO_DELAYS;

    if (gli_flicker)
        Options |= FLICKER_ON;
    else
        Options &= ~FLICKER_ON;

    SetExclusiveOptions(gli_sa_inventory, FORCE_INVENTORY, FORCE_INVENTORY_OFF);
    SetExclusiveOptions(gli_sa_palette, FORCE_PALETTE_ZX, FORCE_PALETTE_C64);
#endif

    if (DrawingVector())
        glk_request_timer_events(TimerDelay());

    palette_type previous_pal = palchosen;
    if (Options & FORCE_PALETTE_ZX)
        palchosen = ZXOPT;
    else if (Options & FORCE_PALETTE_C64) {
        if (Game->picture_format_version == HOWARTH_FORMAT)
            palchosen = C64A;
        else
            palchosen = C64B;
    } else
        palchosen = Game->palette;
    if (palchosen != previous_pal) {
        DefinePalette();
        if (VectorState != NO_VECTOR_IMAGE)
            DrawSomeVectorPixels(1);
    }
    should_draw_image = 1;
}

/* Handle Glk events: window arrangement changes trigger a full redraw,
   timer ticks advance animations or vector drawing. */
void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        UpdateSettings();

        VectorState = NO_VECTOR_IMAGE;

        CloseGraphicsWindow();
        OpenGraphicsWindow();

        if (!GraphicsEnabled())
            glk_request_timer_events(0);

        /* Without split screen the room description and inventory are in
           the main window's scrollback: redraw only the picture. */
        if (GraphicsEnabled()) {
            int saved_slowdraw = gli_slowdraw;
            gli_slowdraw = 0;
            if (showing_inventory == 1) {
                if (split_screen)
                    UpdateUSInventory();
                else
                    InventoryUS();
            } else {
                int lastimg = last_image_index;
                if (!showing_closeup)
                    should_draw_image = 1;
                if (split_screen)
                    Look();
                else
                    DrawRoomImage();
                last_image_index = lastimg;
                if (showing_closeup) {
                    should_draw_image = 1;
                    if (Game->type == US_VARIANT) {
                        DrawUSRoom(last_image_index);
                        DrawImageOrVector();
                    } else
                        DrawImage(last_image_index);
                }
            }
            gli_slowdraw = saved_slowdraw;
        }
    } else if (ev.type == evtype_Timer) {
        switch (Game->type) {
        case SHERWOOD_VARIANT:
            UpdateRobinOfSherwoodAnimations();
            break;
        case GREMLINS_VARIANT:
            UpdateGremlinsAnimations();
            break;
        case SECRET_MISSION_VARIANT:
            UpdateSecretAnimations();
            break;
        default:
            if (DrawingVector())
                DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
            break;
        }
    }
}

GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    if (DrawingVector()) {
        gli_slowdraw = 0;
        DrawSomeVectorPixels(0);
    }
    glk_exit();
}


/* Map a noun index to its canonical (non-synonym) word.
   Synonyms in the dictionary are prefixed with '*' and share
   the word number of the preceding canonical entry. */
const char *MapSynonym(int noun)
{
    int n = 1;
    const char *lastword = ""; /* Last non synonym */
    while (n <= GameHeader.NumWords) {
        if (*Nouns[n] != '*')
            lastword = Nouns[n];
        if (n == noun)
            return (lastword);
        n++;
    }
    return (NULL);
}

/* Find the item whose AutoGet word matches the given noun at a given location.
   Returns the item index, or -1 if no match. loc=0 matches any location. */
int MatchUpItem(int noun, int loc)
{
    const char *word = MapSynonym(noun);
    int ct = 0;

    if (word == NULL)
        word = Nouns[noun];

    while (ct <= GameHeader.NumItems) {
        if (Items[ct].AutoGet && (loc == 0 || Items[ct].Location == loc) && xstrncasecmp(Items[ct].AutoGet, word, GameHeader.WordLength) == 0)
            return (ct);
        ct++;
    }
    return (-1);
}

/* Extract the auto-get/drop word from an item description.
   Item text uses the format "description/WORD/" where WORD is the
   vocabulary word the engine uses for automatic GET/DROP commands.
   This splits the text at the first '/' (NUL-terminating the
   description) and points AutoGet at the word between the slashes.
   Sets AutoGet to NULL if no word is present or if the suffix is
   "//" or "[slash]*" (conventions meaning "no auto-get word"). */
void ParseItemSlashAutoGet(int index)
{
    char *word = strchr(Items[index].Text, '/');
    Items[index].AutoGet = NULL;
    if (word == NULL || strcmp(word, "//") == 0 || strcmp(word, "/*") == 0)
        return;
    /* NUL-terminate the description and advance past the '/' */
    *word++ = 0;
    /* Strip the trailing '/' from the word */
    char *trailingslash = strchr(word, '/');
    if (trailingslash != NULL)
        *trailingslash = 0;
    Items[index].AutoGet = word;
}

/* Read a double-quoted string from a ScottFree plaintext database file.
   Handles escaped quotes (""), backtick-to-quote conversion, and
   strips non-ASCII characters for Glk compatibility. */
static char *ReadString(FILE *f)
{
    char tmp[READSTRING_BUFFER_SIZE];
    int c;
    int ct = 0;
    do {
        c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c != '"')
        Fatal("Initial quote expected");

    for (;;) {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        if (c == '"') {
            int nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"';
        /* Ignore CR (assume CRLF in DOS-formatted files) */
        if (c == '\r')
            continue;
        /* Keep consuming to the closing quote so the file stays in sync, but
           never write past the buffer: a string longer than the buffer (a
           corrupt or hostile database) would otherwise smash the stack, since
           the terminator is fully controlled by the file. Reserve one byte for
           the trailing NUL. */
        if (ct >= READSTRING_BUFFER_SIZE - 1)
            continue;
        /* Pass ASCII and newline to Glk; replace anything else */
        if (c == '\n' || (c >= ' ' && c <= '~'))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    }
    return MemStrndup(tmp, ct);
}

/* Free a database string, unless it is unset (a database that failed to
   load halfway will have NULL strings in its arrays) or the shared "."
   placeholder. */
static void FreeDatabaseString(char *string)
{
    if (string != NULL && string[0] != '.')
        free(string);
}

/* Free entries 0..last of a string array, then the array itself. The
   arrays are const for the parser, which only reads them; the loaders
   allocated the strings, so the cast is the one place that owns them. */
static void FreeStringArray(const char **array, int last)
{
    if (array == NULL)
        return;
    for (int i = 0; i <= last; i++)
        FreeDatabaseString((char *)array[i]);
    free(array);
}

/* Free all dynamically allocated game data arrays (actions, items, rooms,
   dictionary, messages). Frees individual string elements that aren't
   the shared "." placeholder, then frees the arrays and NULLs them. */
void FreeDatabase(void)
{
    if (Actions != NULL)
        free(Actions);
    if (Items != NULL) {
        for (int i = 0; i <= GameHeader.NumItems; i++)
            FreeDatabaseString(Items[i].Text);
        free(Items);
    }
    if (Rooms != NULL) {
        for (int i = 0; i <= GameHeader.NumRooms; i++)
            FreeDatabaseString(Rooms[i].Text);
        free(Rooms);
    }
    FreeStringArray(Nouns, GameHeader.NumWords);
    FreeStringArray(Verbs, GameHeader.NumWords);
    FreeStringArray(Messages, GameHeader.NumMessages);

    Items = NULL;
    Actions = NULL;
    Verbs = NULL;
    Nouns = NULL;
    Rooms = NULL;
    Messages = NULL;
}

/* Read and range-check the header. Returns 0 if it is not a usable one. */
static int LoadHeader(FILE *f)
{
    if (fscanf(f, "%*d %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd",
            &GameHeader.NumItems, &GameHeader.NumActions,
            &GameHeader.NumWords, &GameHeader.NumRooms,
            &GameHeader.MaxCarry, &GameHeader.PlayerRoom,
            &GameHeader.Treasures, &GameHeader.WordLength,
            &GameHeader.LightTime, &GameHeader.NumMessages,
            &GameHeader.TreasureRoom)
        < 10) {
        LoadTrace("Invalid database(bad header)\n");
        return 0;
    }
    /* The counts size the game arrays, and the player is placed in
       PlayerRoom without further checks */
    if (GameHeader.NumItems < 0 || GameHeader.NumActions < 0
        || GameHeader.NumWords < 0 || GameHeader.NumRooms < 0
        || GameHeader.NumMessages < 0 || GameHeader.WordLength < 1
        || GameHeader.PlayerRoom < 0
        || GameHeader.PlayerRoom > GameHeader.NumRooms) {
        LoadTrace("Invalid database(header values out of range)\n");
        return 0;
    }
    return 1;
}

static int LoadActions(FILE *f)
{
    LoadTrace("Reading %d actions.\n", GameHeader.NumActions);
    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        if (fscanf(f, "%hu %hu %hu %hu %hu %hu %hu %hu",
                &Actions[ct].Vocab,
                &Actions[ct].Condition[0],
                &Actions[ct].Condition[1],
                &Actions[ct].Condition[2],
                &Actions[ct].Condition[3],
                &Actions[ct].Condition[4],
                &Actions[ct].Opcode[0],
                &Actions[ct].Opcode[1])
            != 8) {
            debug_print("Bad action line (%d)\n", ct);
            return 0;
        }

        if (Options & DEBUGGING) {
            LoadTrace("Action %d Vocab: %d (Verb:%d/NounOrChance:%d)\n", ct, Actions[ct].Vocab,
               Actions[ct].Vocab / VOCAB_MULTIPLIER, Actions[ct].Vocab % VOCAB_MULTIPLIER);
            for (int i = 0; i < NUM_CONDITIONS; i++)
                LoadTrace("Action %d Condition[%d]: %d (%d/%d)\n", ct, i,
                    Actions[ct].Condition[i], Actions[ct].Condition[i] % CONDITION_MULTIPLIER, Actions[ct].Condition[i] / CONDITION_MULTIPLIER);
            LoadTrace("Action %d Opcode [0]]: %d (%d/%d)\n", ct, Actions[ct].Opcode[0], Actions[ct].Opcode[0] % VOCAB_MULTIPLIER, Actions[ct].Opcode[0] / VOCAB_MULTIPLIER);
            LoadTrace("Action %d Opcode [1]]: %d (%d/%d)\n", ct, Actions[ct].Opcode[1], Actions[ct].Opcode[1] % VOCAB_MULTIPLIER, Actions[ct].Opcode[1] / VOCAB_MULTIPLIER);
        }
    }
    return 1;
}

/* Load the verb/noun dictionary (word pairs) */
static void LoadWords(FILE *f)
{
    LoadTrace("Reading %d word pairs.\n", GameHeader.NumWords);
    for (int ct = 0; ct <= GameHeader.NumWords; ct++) {
        Verbs[ct] = ReadString(f);
        LoadTrace("Verbs %d:%s.\n", ct, Verbs[ct]);
        Nouns[ct] = ReadString(f);
        LoadTrace("Nouns %d:%s.\n", ct, Nouns[ct]);
    }
}

/* Load rooms: 6 exit directions followed by a description string */
static int LoadRooms(FILE *f)
{
    LoadTrace("Reading %d rooms.\n", GameHeader.NumRooms);
    for (int ct = 0; ct <= GameHeader.NumRooms; ct++) {
        int exits[NUM_EXITS];
        if (fscanf(f, "%d %d %d %d %d %d",
                &exits[0], &exits[1], &exits[2],
                &exits[3], &exits[4], &exits[5])
            != NUM_EXITS) {
            debug_print("Bad room line (%d)\n", ct);
            return 0;
        }
        for (int i = 0; i < NUM_EXITS; i++)
            Rooms[ct].Exits[i] = exits[i];

        Rooms[ct].Text = ReadString(f);
        if (Options & DEBUGGING) {
            LoadTrace("Room %d: \"%s\"\n", ct, Rooms[ct].Text);
            LoadTrace("Room connections for room %d:\n", ct);
            for (int i = 0; i < NUM_EXITS; i++)
                LoadTrace("Exit %d: %d\n", i, Rooms[ct].Exits[i]);
        }
        Rooms[ct].Image = NO_IMAGE;
    }
    return 1;
}

/* Load messages (printed by action ops 1-51 and 102+) */
static void LoadMessages(FILE *f)
{
    LoadTrace("Reading %d messages.\n", GameHeader.NumMessages);
    for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
        Messages[ct] = ReadString(f);
        LoadTrace("Message %d: \"%s\"\n", ct, Messages[ct]);
    }
}

/* Load items: description string (with optional /AutoGet/ word) and location */
static int LoadItems(FILE *f)
{
    LoadTrace("Reading %d items.\n", GameHeader.NumItems);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        Items[ct].Text = ReadString(f);
        LoadTrace("Item %d: \"%s\"\n", ct, Items[ct].Text);
        ParseItemSlashAutoGet(ct);
        int location;
        if (fscanf(f, "%d", &location) != 1) {
            debug_print("Bad item line (%d)\n", ct);
            return 0;
        }
        Items[ct].Location = (unsigned char)location;
        LoadTrace("Location of item %d: %d\n", ct, Items[ct].Location);
        Items[ct].InitialLoc = Items[ct].Location;
    }
    return 1;
}

/* Read the version and adventure number trailer */
static int LoadTrailer(FILE *f)
{
    int val;
    if (fscanf(f, "%d", &val) != 1) {
        debug_print("Cannot read version\n");
        return 0;
    }
    LoadTrace("Version %d.%02d of Adventure ", val / 100, val % 100);
    if (fscanf(f, "%d", &val) != 1) {
        debug_print("Cannot read adventure number\n");
        return 0;
    }
    LoadTrace("%d.\nLoad Complete.\n\n", val);
    return 1;
}

/* Load a ScottFree plaintext database from a FILE stream.

   Reads the header, actions, word pairs (verb/noun dictionary), rooms
   with exits, messages, items with locations, then the version/adventure
   number trailer. Returns SCOTTFREE on success, HULK_US if the DOS Hulk
   images are found, or UNKNOWN_GAME on parse failure. The caller
   closes f.

   The file format is the standard Scott Adams plain-text format as
   used by the original IBM PC releases, the ScottKit toolkit,
   and compatible tools. */
GameIDType LoadDatabase(FILE *f)
{
    if (!LoadHeader(f))
        return UNKNOWN_GAME;
    LightRefill = GameHeader.LightTime;
    AllocateGameData();

    if (Options & DEBUGGING)
        PrintHeaderInfo(NULL, &GameHeader);

    int ok = LoadActions(f);
    if (ok) {
        LoadWords(f);
        ok = LoadRooms(f);
    }
    if (ok) {
        LoadMessages(f);
        ok = LoadItems(f);
    }
    if (ok) {
        /* Discard action comment strings (one per action, used only by
           the original authoring tools for documentation) */
        for (int ct = 0; ct <= GameHeader.NumActions; ct++)
            free(ReadString(f));
        ok = LoadTrailer(f);
    }
    if (!ok) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    /* Extra value in at least Hulk */
    int extra = 0;
    if (fscanf(f, "%d", &extra) != 1) {
        debug_print("No extra value in file. This is not Hulk.\n");
        extra = 0;
    }

    if (extra == HULK_DOS_ID && LoadDOSImages()) {
        CurrentSys = SYS_MSDOS;
        ImageWidth = HULK_DOS_WIDTH;
        ImageHeight = HULK_DOS_HEIGHT;
        return HULK_US;
    }
    return SCOTTFREE;
}

void Output(const char *a) { Display(Bottom, "%s", a); }


#ifdef SPATTERLIGHT
static int RunTests(const char *supportpath) {
    return RunVectorTests(supportpath);
}
#endif

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-y", glkunix_arg_NoValue,
        "-y        Generate 'You are', 'You are carrying' type messages for games "
        "that use these instead (eg Robin Of Sherwood)" },
    { "-i", glkunix_arg_NoValue,
        "-i        Generate 'I am' type messages (default)" },
    { "-d", glkunix_arg_NoValue, "-d        Debugging info on load " },
    { "-s", glkunix_arg_NoValue,
        "-s        Generate authentic Scott Adams driver light messages rather "
        "than other driver style ones (Light goes out in n turns..)" },
    { "-t", glkunix_arg_NumberValue,
        "-t N      Lay out the room description in style N, whatever the game "
        "file: 0 plain ScottFree, 1 TRS-80, 2 ZX Spectrum, 3 TI-99/4A, 4 IBM PC" },
    { "-p", glkunix_arg_NoValue,
        "-p        Use for prehistoric databases which don't use bit 16" },
    { "-w", glkunix_arg_NoValue, "-w        Disable upper window" },
    { "-n", glkunix_arg_NoValue, "-n        No delays" },
    { "-f", glkunix_arg_NoValue, "-f        Flicker the room description like some original games" },
#ifndef SPATTERLIGHT
    { "-g", glkunix_arg_NoValue,
        "-g        Draw vector pictures and loading screens gradually, "
        "as the original games do" },
    { "-c", glkunix_arg_NoValue,
        "-c        Draw pictures in the Commodore 64 palette, whatever the game file" },
    { "-x", glkunix_arg_NoValue,
        "-x        Draw pictures in the ZX Spectrum palette, whatever the game file" },
    { "-a", glkunix_arg_NoValue,
        "-a        Always list what you carry in the upper window" },
#else
    { "-z", glkunix_arg_ValueFollows,
        "-z path   Run the vector image tests with this support path, then exit" },
#endif
    { "", glkunix_arg_ValueFollows, "filename    file to load" },

    { NULL, glkunix_arg_End, NULL }
};

/* Parse the leading -options. Returns the number of arguments consumed
   after argv[0]; the game file, if any, follows them. */
static int ParseOptions(int argc, char **argv)
{
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        const char *opt = argv[i];
        switch (opt[1]) {
        case 'y':
            Options |= YOUARE;
            break;
        case 'i':
            Options &= ~YOUARE;
            break;
        case 'd':
            Options |= DEBUGGING;
            break;
        case 's':
            Options |= SCOTTLIGHT;
            break;
        case 't':
            /* -tN or -t N. A bare -t is the TRS-80 style, as it
               always was. */
            if (isdigit((unsigned char)opt[2])) {
                forced_style = atoi(opt + 2);
            } else if (i + 1 < argc && isdigit((unsigned char)argv[i + 1][0])
                && argv[i + 1][1] == '\0') {
                forced_style = atoi(argv[++i]);
            } else {
                forced_style = 1;
            }
            break;
        case 'p':
            Options |= PREHISTORIC_LAMP;
            break;
        case 'w':
            split_screen = 0;
            break;
        case 'n':
            Options |= NO_DELAYS;
            break;
        case 'f':
            Options |= FLICKER_ON;
            break;
#ifndef SPATTERLIGHT
        case 'g':
            gli_slowdraw = 1;
            break;
        case 'c':
            Options = (Options | FORCE_PALETTE_C64) & ~FORCE_PALETTE_ZX;
            break;
        case 'x':
            Options = (Options | FORCE_PALETTE_ZX) & ~FORCE_PALETTE_C64;
            break;
        case 'a':
            Options = (Options | FORCE_INVENTORY) & ~FORCE_INVENTORY_OFF;
            break;
#else
        case 'z':
            /* Run the vector image tests with the support path that
               follows, report the result and exit. */
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: -z path\n");
                glk_exit();
            }
            fprintf(stderr, "Running tests\n");
            win_testresult(RunTests(argv[++i]));
            glk_exit();
#endif
        }
    }
    return i - 1;
}

/* Parse command-line arguments (display style, debugging, delays, etc.)
   and extract the game file path and its parent directory. */
int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    int consumed = ParseOptions(argc, argv);
    argc -= consumed;
    argv += consumed;

#ifdef GARGLK
    garglk_set_program_name("ScottFree 1.15");
    garglk_set_program_info("ScottFree 1.15 by Alan Cox\n"
                            "Glk port by Chris Spiegel\n");
#endif

    if (argc == 2) {
        game_file = argv[1];

        const char *base = strrchr(game_file, '/');
        if (base == NULL)
            base = strrchr(game_file, '\\');
        base = base ? base + 1 : game_file;

#ifdef GARGLK
        garglk_set_story_name(base);
#endif

        int dirlen = (int)(base - game_file);
        if (dirlen) {
            DirPath = MemAlloc(dirlen + 1);
            memcpy(DirPath, game_file, dirlen);
            DirPath[dirlen] = 0;
        }
    }

    return 1;
}

/* Decrement the lamp timer each turn. Brian Howarth games use -1
   for infinite light. Warn when below 25 turns; extinguish at 0. */
static void TickLampTimer(void)
{
    if (Items[LIGHT_SOURCE].Location != DESTROYED && GameHeader.LightTime != -1 && !StopTime) {
        GameHeader.LightTime--;
        if (GameHeader.LightTime < 1) {
            SetBitFlag(LIGHTOUTBIT);
            if (IsPresent(LIGHT_SOURCE)) {
                Output(sys[LIGHT_HAS_RUN_OUT]);
            }
            if ((Options & PREHISTORIC_LAMP) || (Game->subtype & MYSTERIOUS))
                Items[LIGHT_SOURCE].Location = DESTROYED;
        } else if (GameHeader.LightTime < LAMP_WARNING_THRESHOLD) {
            if (IsPresent(LIGHT_SOURCE)) {
                if ((Options & SCOTTLIGHT) || (Game->subtype & MYSTERIOUS)) {
                    Display(Bottom, "%s %d %s\n", sys[LIGHT_RUNS_OUT_IN], GameHeader.LightTime, sys[TURNS]);
                } else {
                    if (GameHeader.LightTime % LAMP_WARNING_INTERVAL == 0)
                        Output(sys[LIGHT_GROWING_DIM]);
                }
            }
        }
    }
}

/* The system messages of the US releases with graphics. */
static void ApplyUSSysmess(void)
{
    sys[MESSAGE_DELIMITER] = " ";
    sys[ITEM_DELIMITER] = ". ";
    sys[EXITS_DELIMITER] = " ";
    sys[YOU_ARE] = "I am in a ";
    sys[YOU_SEE] = ". Visible: ";
    sys[INVENTORY] = "I've got: ";
    sys[NOTHING] = "Nothing at all. ";
    sys[EXITS] = "Some exits: ";
    if (CurrentSys == SYS_MSDOS)
        sys[WHAT_NOW] = "Command me? ";
    else
        sys[WHAT_NOW] = "What shall I do? ";
    sys[NORTH] = "NORTH";
    sys[EAST] = "EAST";
    sys[SOUTH] = "SOUTH";
    sys[WEST] = "WEST";
    sys[UP] = "UP";
    sys[DOWN] = "DOWN";
}

/* The -t option: replace the display style the game file asked for.
   The system messages stay the game's own. */
static void ApplyForcedStyle(void)
{
    static const int styles[] = {
        0,                         /* 0: plain ScottFree */
        TRS80_STYLE,               /* 1: TRS-80 */
        SPECTRUM_STYLE,            /* 2: ZX Spectrum */
        TI994A_STYLE,              /* 3: TI-99/4A */
        SPECTRUM_STYLE | PC_STYLE, /* 4: IBM PC */
    };

    if (forced_style < 0 || forced_style >= (int)(sizeof styles / sizeof styles[0]))
        return;

    Options &= ~(TRS80_STYLE | SPECTRUM_STYLE | TI994A_STYLE | PC_STYLE);
    Options |= styles[forced_style];
}

/* Select the "You are" or the "I am" system message set */
static void SelectSystemMessages(void)
{
    const char **dictpointer = (Options & YOUARE) ? sysdict : sysdict_i_am;

    for (int i = 0; i < MAX_SYSMESS; i++) {
        if (dictpointer[i] != NULL)
            sys[i] = dictpointer[i];
        else
            sys[i] = sysdict[i];
    }
}

/* Detect the game format, load its database, show the title screen
   and set up the display style and windows the game wants. */
static void SetUpGame(void)
{
    GameIDType game_type = DetectGame(game_file);

    if (game_type == UNKNOWN_GAME)
        Fatal("Unsupported game!");

    if (game_type != SCOTTFREE && game_type != TI994A)
        Options |= SPECTRUM_STYLE;
    else if (game_type != TI994A)
        Options |= TRS80_STYLE;

    ApplyForcedStyle();

    if (ZXLoadingScreen != NULL && Game->type != US_VARIANT)
        DrawZXTitleImage();

    if (title_screen != NULL && CurrentGame != RETURN_TO_PIRATES_ISLE) {
        if (split_screen)
            PrintTitleScreenGrid();
        else
            PrintTitleScreenBuffer();
    }

    if (Options & TRS80_STYLE) {
        TopWidth = TRS80_STATUS_WIDTH;
        TopHeight = TRS80_STATUS_HEIGHT;
    } else {
        TopWidth = DEFAULT_STATUS_WIDTH;
        TopHeight = DEFAULT_STATUS_HEIGHT;
    }

    if (game_type == TI994A) {
        Display(Bottom, "In this adventure, you may abbreviate any word \
by typing its first %d letters, and directions by typing \
one letter.\n\nDo you want to restore previously saved game?\n",
            GameHeader.WordLength);
        if (YesOrNo())
            LoadGame();
        glk_window_clear(Bottom);
    }

    /* US-variant games use a distinct display style with first-person
       messages and DOS/Apple II graphics. If no graphics are found,
       fall back to plain ScottFree mode. */
    if (Game->type == US_VARIANT) {
        if (HasGraphics()) {
            DrawTitleImageScott();
            OpenGraphicsWindow();
            ApplyUSSysmess();

            if (CurrentGame == RETURN_TO_PIRATES_ISLE)
                UpdateRTPISystemMessages();

            if (forced_style < 0)
                Options |= PC_STYLE;
        } else {
            CurrentGame = SCOTTFREE;
            Game->type = NO_TYPE;
            game_type = SCOTTFREE;
        }
    }

    if (game_type == SCOTTFREE)
        Output("\
Scott Free, A Scott Adams game driver in C.\n\
Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
Distributed under the GNU software license\n\n");

    OpenTopWindow();
}

/* One round of the game loop: implicit actions → Look → get input →
   explicit actions → lamp timer. */
static void PlayTurn(void)
{
    int vb, no;

    if (should_restart)
        RestartGame();

    /* Run implicit (automatic) actions — these have vocab 0 and
       fire based on random chance or game state, not player input */
    if (!StopTime)
        PerformActions(0, 0);
    /* Refresh the room description (skip during TAKE ALL / DROP ALL) */
    if (!(CurrentCommand && CurrentCommand->allflag && !(CurrentCommand->allflag & LASTALL))) {
        print_look_to_transcript = should_look_in_transcript;
        Look();
        print_look_to_transcript = should_look_in_transcript = 0;
        if (!StopTime && !should_restart)
            SaveUndo();
    }

    if (should_restart)
        return;

    if (GetInput(&vb, &no) == 1)
        return;

    /* Run explicit (player-triggered) actions */
    switch (PerformActions(vb, no)) {
    case ER_RAN_ALL_LINES_NO_MATCH:
        if (!RecheckForExtraCommand()) {
            Output(sys[I_DONT_UNDERSTAND]);
            FreeCommands();
        }
        break;
    case ER_RAN_ALL_LINES:
        Output(sys[YOU_CANT_DO_THAT_YET]);
        // Failed actions should interrupt chains of commands
        // but not TAKE ALL and DROP ALL
        if (CurrentCommand == NULL || !CurrentCommand->allflag)
            FreeCommands();
        break;
    default:
        JustStarted = 0;
    }

    TickLampTimer();
    if (StopTime)
        StopTime--;
}

/* Main entry point: set the game up, then run the game loop. */
void glk_main(void)
{
    OpenBottomWindow();

    if (game_file == NULL)
        Fatal("No game provided");

    SelectSystemMessages();
    SetUpGame();

    /* Not only for Spatterlight's preferences: this is also where the
       palette is first chosen, from the game file or the -c / -x options. */
    UpdateSettings();
#ifdef SPATTERLIGHT
    if (gli_determinism)
        set_erkyrath_random(DETERMINISTIC_SEED);
    else
#endif
    set_erkyrath_random(0);

    InitialState = SaveCurrentState();

    while (1) {
        glk_tick();
        PlayTurn();
    }
}
