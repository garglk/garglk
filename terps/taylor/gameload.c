//
//  gameload.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  Loading the game file, identifying which game it is, and locating the
//  data tables (dictionary, tokens, messages, rooms, actions, ...) inside it.
//

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"
#include "c64decrunch.h"
#include "common_file_utils.h"
#include "common_utils.h"
#include "extracommands.h"
#include "gamedefs.h"
#include "extracttape.h"
#include "graphics.h"
#include "loading_screen.h"
#include "player.h"
#include "textoutput.h"
#include "ui.h"
#include "utility.h"

#include "gameload.h"

uint8_t *FileImage = NULL;  /* Raw game file loaded into memory */
uint8_t *EndOfData = NULL;  /* One past the last byte of FileImage */
size_t FileImageLen = 0;
size_t VerbBase;            /* Offset of the verb/noun dictionary in FileImage */
size_t TokenBase;           /* Offset of the compressed token (text fragment) table */
size_t MessageBase;         /* Offset of the primary message text table */
size_t Message2Base;        /* Offset of the secondary message text table */
size_t RoomBase;            /* Offset of room description text */
size_t ObjectBase;          /* Offset of object description text */
size_t ExitBase;            /* Offset of room exit/connection table */
size_t ObjLocBase;          /* Offset of initial object location table */
size_t StatusBase;          /* Offset of the status table, run every turn */
size_t EventBase;           /* Offset of the event table, matched against the command */
size_t FlagBase;            /* Offset of initial flag values */
size_t AnimationData = 0;   /* Offset of Kayleth animation data (0 if none) */

/* Offset added to all table addresses to account for the file's load
   position differing from the original Z80 memory layout. */
long FileBaselineOffset = 0;

static char *Filename;

/* Search FileImage for a byte pattern starting at base. Returns the
   offset if found, or NOT_FOUND. */
size_t FindCode(const char *code, size_t base, size_t len)
{
    if (len > FileImageLen || base > FileImageLen - len)
        return NOT_FOUND;
    unsigned char *p = FileImage + base;
    unsigned char *last = FileImage + FileImageLen - len;
    while (p <= last) {
        if (memcmp(p, code, len) == 0)
            return p - FileImage;
        p++;
    }
    return NOT_FOUND;
}

/* The dictionary always starts with "NORT" + code 1 + "N" (the first
   direction word, "NORTH", with code 1 followed by its alias "N"). */
size_t FindVerbTable(void)
{
    return FindCode("NORT\001N", 0, 6);
}

/* Locate the initial flag data in the file image by searching for known
   byte patterns. Falls back to the Game struct's recorded offset. */
static size_t FindFlags(void)
{
    /* Questprobe */
    size_t pos = FindCode("\xE7\x97\x51\x95\x5B\x7E\x5D\x7E\x76\x93", 0, 10);
    if (pos != NOT_FOUND)
        return pos + 11;

    /* Look for the flag initial block copy */
    pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0, 8);
    if (pos != NOT_FOUND)
        return pos + 5;

    if (Game == NULL)
        Fatal("Cannot find initial flag data");
    return Game->start_of_flags + FileBaselineOffset;
}

/* Heuristic: check if the data at pos looks like a token table by
   counting lowercase ASCII characters in a fixed-size sample window. */
static int LooksLikeTokens(size_t pos)
{
    enum {
        TOKEN_SAMPLE_SIZE      = 512, /* bytes to inspect */
        TOKEN_SAMPLE_THRESHOLD = 300, /* >this many lowercase letters = likely tokens */
    };

    if (pos > FileImageLen || FileImageLen - pos < TOKEN_SAMPLE_SIZE)
        return 0;

    unsigned char *sample = FileImage + pos;
    int lowercase_count = 0;
    for (int i = 0; i < TOKEN_SAMPLE_SIZE; i++) {
        unsigned char c = sample[i] & TOKEN_BYTE_MASK;
        if (c >= 'a' && c <= 'z')
            lowercase_count++;
    }
    return lowercase_count > TOKEN_SAMPLE_THRESHOLD;
}

/* Search for the token (text fragment) table: the game's own signature, if
   it has one, then generic Z80 instruction patterns and the "You are in "
   string. Returns NOT_FOUND if nothing matches. */
static size_t SearchTokens(const char *signature)
{
    size_t addr;
    /* The Z80 pattern is preceded by the table's address */
    size_t pos = 1;

    if (signature) {
        addr = FindCode(signature, 0, TOKEN_SIGNATURE_LENGTH);
        if (addr != NOT_FOUND)
            return addr;
    }

    do {
        pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1, 8);
        if (pos == NOT_FOUND) {
            /* Questprobe */
            pos = FindCode("\x58\x58\x58\x58\xFF", 0, 5);
            if (pos != NOT_FOUND)
                return pos + 6;
            /* Last resort */
            addr = FindCode("You are in ", 0, 11);
            if (addr == NOT_FOUND || addr == 0)
                return NOT_FOUND;
            return addr - 1;
        }
        addr = READ_LE_UINT16(FileImage + pos - 2) - ZX_RAM_BASE + FileBaselineOffset;
    } while (LooksLikeTokens(addr) == 0);
    return addr;
}

/* The token table of the game in Game, or where the games[] row says it
   is if the search finds nothing. */
static size_t FindTokens(void)
{
    size_t pos = SearchTokens(Game->token_signature);
    if (pos == NOT_FOUND)
        return Game->start_of_tokens + FileBaselineOffset;
    return pos;
}

/* Resolve all data table offsets from the Game struct, applying
   FileBaselineOffset to convert from original Z80 addresses. */
void FindTables(void)
{
    TokenBase = FindTokens();
    RoomBase = Game->start_of_room_descriptions + FileBaselineOffset;
    ObjectBase = Game->start_of_item_descriptions + FileBaselineOffset;
    StatusBase = Game->start_of_status_table + FileBaselineOffset;
    EventBase = Game->start_of_event_table + FileBaselineOffset;
    ExitBase = Game->start_of_room_connections + FileBaselineOffset;
    FlagBase = FindFlags();
    ObjLocBase = Game->start_of_item_locations + FileBaselineOffset;
    MessageBase = Game->start_of_messages + FileBaselineOffset;
    Message2Base = Game->start_of_messages_2 + FileBaselineOffset;

    /* The offsets come from where the dictionary was found. In a file that
       was cut short, or where something else matched, they can point past
       the end of it (or before the start, which wraps around). */
    const struct {
        size_t address; /* 0 if the game has no such table */
        size_t base;
    } tables[] = {
        { Game->start_of_room_descriptions, RoomBase },
        { Game->start_of_item_descriptions, ObjectBase },
        { Game->start_of_status_table, StatusBase },
        { Game->start_of_event_table, EventBase },
        { Game->start_of_room_connections, ExitBase },
        { Game->start_of_item_locations, ObjLocBase },
        { Game->start_of_messages, MessageBase },
        { Game->start_of_messages_2, Message2Base },
        { 1, TokenBase },
        { 1, FlagBase },
    };
    for (size_t i = 0; i < sizeof tables / sizeof tables[0]; i++) {
        if (tables[i].address != 0 && tables[i].base >= FileImageLen)
            Fatal("This game file is damaged or incomplete");
    }

    if (BaseGame == KAYLETH) {
        AnimationData = FindCode("\xff\x00\x00\x00\x0f\x00\x5d\x0f\x00\x61", 0, 10);
        if (AnimationData == NOT_FOUND)
            AnimationData = FindCode("\xff\x00\x00\x15\x0f\x00\x5d\x0f\x00\x61", 0, 10);
    }
}

/* Guess where "low" (room-description) objects end for Rebel Planet.
   Scans object descriptions looking for a comma at the end of an entry,
   which signals the start of "high" (standalone) objects. */
static int GuessLowObjectEndRebelPlanet(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *t = NULL;
    unsigned char c = 0, lc;
    int n = 0;

    while (p < EndOfData) {
        if (t == NULL)
            t = TokenText(*p++);
        if (t >= EndOfData)
            break;
        lc = c;
        c = *t & TOKEN_BYTE_MASK;
        if (c == MSG_END_SPACE || c == MSG_END) {
            if (lc == ',' && n > 20)
                return n;
            n++;
        }
        if (*t++ & TOKEN_LAST_BYTE)
            t = NULL;
    }
    return -1;
}

/* Guess the boundary between low objects (embedded in room descriptions)
   and high objects (listed separately after "You see:"). Low object
   descriptions end with a comma in the last token. */
int GuessLowObjectEnd(void)
{
    /* Can't automatically guess in this case */
    if (CurrentGame == BLIZZARD_PASS)
        return BP_NUM_LOW_OBJECTS;
    /* Questprobe 3 writes every object into the room description */
    else if (Version == QUESTPROBE3_TYPE)
        return Q3_NUM_OBJECTS;

    if (Version == REBEL_PLANET_TYPE)
        return GuessLowObjectEndRebelPlanet();

    unsigned char *obj_text = FileImage + ObjectBase;
    int obj_index = 0;

    while (obj_index < NumObjects()) {
        while (obj_text < EndOfData && *obj_text != MSG_END && *obj_text != MSG_END_SPACE) {
            obj_text++;
        }
        if (obj_text >= EndOfData)
            break;
        /* obj_text[-1] is the last token index of the description; find
           that token's last byte to check whether it ends in a comma. */
        unsigned char *last_byte = TokenText(obj_text[-1]);
        if (last_byte >= EndOfData)
            break;
        while (last_byte < EndOfData - 1 && !(*last_byte & TOKEN_LAST_BYTE)) {
            last_byte++;
        }
        if ((*last_byte & TOKEN_BYTE_MASK) == ',')
            return obj_index;
        obj_index++;
        obj_text++;
    }
    fprintf(stderr, "Unable to guess the last description object.\n");
    return 0;
}

static char *CopyString(const char *string)
{
    size_t length = strlen(string);
    char *copy = MemAlloc(length + 1);
    memcpy(copy, string, length + 1);
    return copy;
}

/* Spatterlight has preferences for the options in the #ifndef block.
   Elsewhere (in Gargoyle, through a "terp taylor -x" line in garglk.ini)
   they are set here; the palette follows the game file unless one is
   forced. */
glkunix_argumentlist_t glkunix_arguments[] = {
    { "-n", glkunix_arg_NoValue, "-n        No delays" },
#ifndef SPATTERLIGHT
    { "-g", glkunix_arg_NoValue,
        "-g        Draw loading screens gradually, as the original games do" },
    { "-c", glkunix_arg_NoValue,
        "-c        Draw pictures in the Commodore 64 palette, whatever the game file" },
    { "-x", glkunix_arg_NoValue,
        "-x        Draw pictures in the ZX Spectrum palette, whatever the game file" },
    { "-a", glkunix_arg_NoValue,
        "-a        Always list what you carry in the upper window" },
#endif
    { "", glkunix_arg_ValueFollows, "filename    file to load" },

    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    int arg = 1;
    for (; arg < argc && argv[arg][0] == '-'; arg++) {
        switch (argv[arg][1]) {
        case 'n':
            Options |= NO_DELAYS;
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
#endif
        }
    }

    /* No game file: a usage line on stderr, and refusing to start makes the
       library exit. Only a command line gets here; the GUI front ends
       always pass a file. */
    if (arg >= argc) {
        fprintf(stderr, "usage: %s", argv[0]);
        for (glkunix_argumentlist_t *a = glkunix_arguments; a->argtype != glkunix_arg_End; a++) {
            if (a->name[0] == '-')
                fprintf(stderr, " [%s]", a->name);
        }
        fprintf(stderr, " <game file>\n");
        return 0;
    }

    Filename = CopyString(argv[arg]);

    /* A file that cannot be read is reported in a window too: in a GUI
       the file was chosen by the user, and stderr goes nowhere. */
    FileImage = ReadFileIfExists(Filename, &FileImageLen);
    if (FileImage == NULL) {
        char message[1024];
        snprintf(message, sizeof message, "Cannot read %s: %s", Filename, strerror(errno));
        Fatal(message);
    }

    FileImage = ProcessFile(FileImage, &FileImageLen);
    EndOfData = FileImage + FileImageLen;

#ifdef GARGLK
    garglk_set_program_name("TaylorMade 0.5");
    garglk_set_program_info("TaylorMade 0.5 by Alan Cox\n"
                            "Glk port, graphics and Questprobe 3 support by Petter Sjölund\n");
    const char *s;
    if ((s = strrchr(Filename, '/')) != NULL || (s = strrchr(Filename, '\\')) != NULL) {
        garglk_set_story_name(s + 1);
    } else {
        garglk_set_story_name(Filename);
    }
#endif

    return 1;
}

/* Look up a game's entry in the games[] registry, or NULL. */
GameInfo *FindGameByID(GameIDType id)
{
    for (int i = 0; i < NUMGAMES && games[i].Title != NULL; i++) {
        if (games[i].gameID == id)
            return &games[i];
    }
    return NULL;
}

/* Identify the game by trying each entry in the games[] table. For each
   candidate, compute the baseline offset from the verb table and check
   whether the token table falls at the expected relative position.
   FileBaselineOffset is left as it was: the caller sets it from the
   game found. */
GameInfo *DetectGame(size_t LocalVerbBase)
{
    long parked_offset = FileBaselineOffset;
    GameInfo *found = NULL;

    for (int i = 0; i < NUMGAMES && games[i].gameID != UNKNOWN_GAME; i++) {
        GameInfo *LocalGame = &games[i];
        /* SearchTokens() reads the candidate's offset. It scans the file
           again for each one, which takes no measurable time. The
           signatures are left out: only C64 rows have them, and C64 files
           are identified by c64decrunch.c before this. */
        FileBaselineOffset = (long)LocalVerbBase - (long)LocalGame->start_of_dictionary;
        long diff = SearchTokens(NULL) - LocalVerbBase;
        if ((LocalGame->start_of_tokens - LocalGame->start_of_dictionary) == diff) {
            debug_print("This is %s\n", LocalGame->Title);
            found = LocalGame;
            break;
        }
        debug_print("Diff for game %s: %d. Looking for %ld\n", LocalGame->Title, LocalGame->start_of_tokens - LocalGame->start_of_dictionary, diff);
    }
    FileBaselineOffset = parked_offset;
    return found;
}

/* Temple of Terror has separate graphics and text-only versions. If we
   loaded one, look for the other by swapping the last character before
   the extension ('a' <-> 'b'). If found, offer to create a hybrid that
   uses the text-only version's longer prose with the graphics version's
   images. */
void LookForSecondTOTGame(void)
{
    size_t namelen = strlen(Filename);
    if (namelen == 0)
        return;
    char *secondfile = CopyString(Filename);

    char *period = strrchr(secondfile, '.');
    if (period == NULL)
        period = &secondfile[namelen - 1];
    else if (period > secondfile)
        period--;

    GameIDType wanted;
    if (CurrentGame == TEMPLE_OF_TERROR) {
        *period = 'b';
        wanted = TOT_TEXT_ONLY;
    } else {
        *period = 'a';
        wanted = TEMPLE_OF_TERROR;
    }

    size_t filelength;
    uint8_t *companion = ReadFileIfExists(secondfile, &filelength);
    free(secondfile);
    if (companion == NULL)
        return;
    companion = ProcessFile(companion, &filelength);

    /* Look at the companion file through the FileImage globals */
    uint8_t *parked_file = FileImage;
    size_t parked_length = FileImageLen;
    long parked_offset = FileBaselineOffset;
    FileImage = companion;
    FileImageLen = filelength;

    size_t alt_verb_base = FindVerbTable();
    GameInfo *alt_game = NULL;
    long alt_offset = 0;
    if (alt_verb_base != NOT_FOUND)
        alt_game = DetectGame(alt_verb_base);
    if (alt_game != NULL)
        alt_offset = (long)alt_verb_base - (long)alt_game->start_of_dictionary;

    int combine = 0;
    if (alt_game != NULL && alt_game->gameID == wanted) {
        /* Nothing has picked a window yet, and YesOrNo prints its echo
           and "Please answer Y or N." through the current one. */
        CurrentWindow = Bottom;
        Display(Bottom, "Found files for both the text-only version and the graphics version of Temple of Terror.\n"
                        "Would you like to use the longer texts from the text-only version along with the graphics from the other file? (Y/N) ");
        combine = YesOrNo();
    }

    FileImage = parked_file;
    FileImageLen = parked_length;
    FileBaselineOffset = parked_offset;

    if (!combine) {
        free(companion);
        return;
    }

    /* The pictures are read from the graphics version, which then stays
       in memory for the picture instructions, and the game is played
       from the text-only version. */
    if (CurrentGame == TOT_TEXT_ONLY) {
        Game = FindGameByID(TOT_HYBRID);
        FileImage = companion;
        FileImageLen = filelength;
        FileBaselineOffset = alt_offset;
        InitGraphics();
        FileImage = parked_file;
        FileImageLen = parked_length;
        FileBaselineOffset = parked_offset;
    } else {
        InitGraphics();
        Game = FindGameByID(TOT_HYBRID);
        FileImage = companion;
        FileImageLen = filelength;
        VerbBase = alt_verb_base;
    }

    EndOfData = FileImage + FileImageLen;
}
