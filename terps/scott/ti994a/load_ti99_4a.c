//
//  load_ti99_4a.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-02-12.
//

#include <stdlib.h>
#include <string.h>

#include "load_ti99_4a.h"
#include "rtpi_graphics.h"
#include "scott.h"

#include "detect_game.h"
#include "scott_game_info.h"

#define PACKED __attribute__((__packed__))

// clang-format off

typedef struct {
    uint8_t    num_objects;                         /* number of objects */
    uint8_t    num_verbs;                           /* number of verbs */
    uint8_t    num_nouns;                           /* number of nouns */
    uint8_t    red_room;                            /* the red room (dead room) */
    uint8_t    max_items_carried;                   /* max number of items can be carried */
    uint8_t    begin_locn;                          /* room to start in */
    uint8_t    num_treasures;                       /* number of treasures */
    uint8_t    cmd_length;                          /* number of letters in commands */
    uint16_t   light_turns           PACKED;        /* max number of turns light lasts */
    uint8_t    treasure_locn;                       /* location of where to store treasures */
    uint8_t    strange;                             /* !?! not known. */

    uint16_t   p_obj_table           PACKED;        /* pointer to object table */
    uint16_t   p_orig_items          PACKED;        /* pointer to original items */
    uint16_t   p_obj_link            PACKED;        /* pointer to link table from noun to object */
    uint16_t   p_obj_descr           PACKED;        /* pointer to object descriptions */
    uint16_t   p_message             PACKED;        /* pointer to message pointers */
    uint16_t   p_room_exit           PACKED;        /* pointer to room exits table */
    uint16_t   p_room_descr          PACKED;        /* pointer to room descr table */

    uint16_t   p_noun_table          PACKED;        /* pointer to noun table */
    uint16_t   p_verb_table          PACKED;        /* pointer to verb table */

    uint16_t   p_explicit            PACKED;        /* pointer to explicit action table */
    uint16_t   p_implicit            PACKED;        /* pointer to implicit actions */
} DATAHEADER;

// clang-format on

static int max_messages;
static int max_item_descr;

/* Copies of the implicit and explicit action bytecode, extracted from
   the game file so they survive after entire_file is freed. */
uint8_t *ti99_implicit_actions = NULL;
uint8_t *ti99_explicit_actions = NULL;
size_t ti99_implicit_extent = 0;
size_t ti99_explicit_extent = 0;

/* Per-verb pointers into ti99_explicit_actions. */
uint8_t **VerbActionOffsets;

/* Convert a TI-99/4A logical address to a file offset.  The constant
   0x380 is the base address of the GROM data in the TI-99/4A memory
   map; file_baseline_offset compensates for any header prefix in the
   disk image. */
static uint16_t FixAddress(uint16_t grom_address)
{
    return (grom_address - 0x380 + file_baseline_offset);
}

/* Read a big-endian 16-bit word from the file buffer with bounds
   checking.  Returns 0 if the read would be out of range. */
static uint16_t GetWord(uint8_t *mem)
{
    if (mem < entire_file || mem + 1 >= entire_file + file_length)
        return 0;
    return READ_BE_UINT16(mem);
}

/* Derive the number of entries in a pointer table (given as the raw
   big-endian header field): the first entry points past the table
   itself, so dividing the span by 2 (each entry is a 16-bit pointer)
   gives the count. */
static int CountTI99TableEntries(const void *header_field)
{
    uint16_t table = FixAddress(READ_BE_UINT16(header_field));
    uint16_t first_entry = FixAddress(GetWord((uint8_t *)entire_file + table));
    return (first_entry - table) / 2;
}

static GameIDType TryLoadingTI994A(const DATAHEADER *dh);

/* Check whether the file matches a known Return to Pirate's Isle
   disk image format (several container variants exist). */
static int IsRTPICandidate(void)
{
    if (file_length == 0x755f || file_length == 0x5e5c)
        return 1;
    if (file_length == 0x2d000
        && (!memcmp(entire_file, "PIRATE", 6)
            || !memcmp(entire_file, "RTNPIRAT", 8)))
        return 1;
    if (file_length == 0x16800
        && !memcmp(entire_file, "ADV*RPI*CM", 10))
        return 1;
    return 0;
}

/* Entry point: try to identify and load a TI-99/4A Scott Adams game.
   First checks for the RTPI (Return to Pirate's Isle) disk format,
   then searches for the standard TI-99/4A header signature. */
GameIDType DetectTI994A(void)
{
    if (IsRTPICandidate()) {
        GameIDType result = DetectRTPI(entire_file, file_length);
        if (result == RETURN_TO_PIRATES_ISLE)
            return result;
    }

    /* Locate the header by searching for a known byte sequence that
       appears at a fixed logical offset (0x589) in the GROM data. */
    int signature_offset = FindCode(entire_file, file_length, "\x30\x30\x30\x30\x00\x30\x30\x00\x28\x28", 10);
    if (signature_offset == -1)
        return UNKNOWN_GAME;

    file_baseline_offset = signature_offset - 0x589;

    size_t header_offset = 0x8a0 + file_baseline_offset;
    if (header_offset + sizeof(DATAHEADER) > file_length)
        return UNKNOWN_GAME;

    DATAHEADER dh;
    memcpy(&dh, entire_file + header_offset, sizeof(DATAHEADER));

    max_messages = CountTI99TableEntries(&dh.p_message);
    max_item_descr = CountTI99TableEntries(&dh.p_obj_descr);

    /* Validate that every pointer in the header falls within the file. */
    const uint16_t pointers[] = {
        dh.p_obj_table, dh.p_orig_items, dh.p_obj_link, dh.p_obj_descr,
        dh.p_message, dh.p_room_exit, dh.p_room_descr, dh.p_noun_table,
        dh.p_verb_table, dh.p_explicit, dh.p_implicit
    };
    for (size_t i = 0; i < sizeof pointers / sizeof pointers[0]; i++) {
        if (file_length < FixAddress(READ_BE_UINT16(&pointers[i])))
            return UNKNOWN_GAME;
    }

    return TryLoadingTI994A(&dh);
}

/* Point data_ptr at a section of raw bytes, given the header field (a
   big-endian GROM address) that locates it.  SeekIfNeeded returns
   data_ptr with file_baseline_offset baked in; subtract it so that the
   pointer indexes directly into entire_file.  Returns 0 on failure. */
static int SeekToRawData(uint16_t header_field, size_t *seek_offset,
    uint8_t **data_ptr)
{
    if (SeekIfNeeded(FixAddress(READ_BE_UINT16(&header_field)), seek_offset, data_ptr) == 0)
        return 0;
    *data_ptr -= file_baseline_offset;
    return 1;
}

/* Construct a C string from a string table. */

/* The game stores strings as sequences of length-prefixed words,
   presumably to allow the original interpreter to quickly determine
   whether a word will fit on the current line when laying out text.
   The pointer table at `table` maps `table_offset` to a pair of
   pointers (start_offset, end_offset) delimiting the word list for
   this string.  Words are joined with spaces. */
static char *GetTI994AString(uint16_t table, int table_offset)
{
    uint8_t *endptr = entire_file + file_length;
    uint8_t *table_entry = entire_file + FixAddress(READ_BE_UINT16(&table));

    table_entry += table_offset * 2;
    if (table_entry + 3 >= endptr)
        return NULL;

    uint16_t start_offset = FixAddress(GetWord(table_entry));
    uint16_t end_offset = FixAddress(GetWord(table_entry + 2));

    uint8_t *words_end = entire_file + end_offset;
    uint8_t *word_ptr = entire_file + start_offset;

    if (words_end > endptr)
        words_end = endptr;

    uint8_t buffer[1024];
    size_t total_length = 0;

    while (word_ptr < words_end) {
        size_t word_length = word_ptr[0];
        word_ptr++;
        if (word_length == 0 || word_length > 100)
            break;
        if (word_ptr + word_length > words_end || total_length + word_length >= 1024)
            break;
        memcpy(buffer + total_length, word_ptr, word_length);
        word_ptr += word_length;
        total_length += word_length;
        if (word_ptr < words_end)
            buffer[total_length++] = ' ';
    }

    if (total_length == 0)
        return NULL;

    char *result = MemAlloc(total_length + 1);
    memcpy(result, buffer, total_length);
    result[total_length] = '\0';
    return result;
}

/* Load a verb or noun dictionary from the game file.  The table is
   an array of 16-bit pointers; consecutive pointers delimit each
   word's raw bytes (word_len = word_table[i+1] - word_table[i]). */
static void LoadTI994ADict(uint16_t table, int num_words,
    const char **dict)
{
    uint8_t *endptr = entire_file + file_length;
    uint16_t *word_table = (uint16_t *)(entire_file + FixAddress(READ_BE_UINT16(&table)));

    for (int i = 0; i <= num_words; i++) {
        dict[i] = ".";

        if ((uint8_t *)&word_table[i + 1] + 1 >= endptr)
            continue;

        uint16_t word_start = FixAddress(GetWord((uint8_t *)&word_table[i]));
        uint16_t word_end = FixAddress(GetWord((uint8_t *)&word_table[i + 1]));

        if (word_start >= word_end || word_end > file_length)
            continue;

        char *word_data = (char *)entire_file + word_start;
        int word_len = word_end - word_start;

        if (word_len > 0 && word_len < 20 && word_data + word_len <= (char *)endptr) {
            char *text = MemAlloc(word_len + 1);
            memcpy(text, word_data, word_len);
            text[word_len] = 0;
            dict[i] = text;
        }
    }
}

/* Copy the implicit (auto-run) action bytecode out of the game file.
   Actions are stored as variable-length blocks: byte 0 is a tag and
   byte 1 is the block length.  A zero length marks the last block,
   whose own size is therefore not recorded, so everything up to the
   end of the file is copied. */
static void ReadTI99ImplicitActions(const DATAHEADER *dh)
{
    uint8_t *endptr = entire_file + file_length;
    uint8_t *block_start = entire_file + FixAddress(READ_BE_UINT16(&dh->p_implicit));

    if (block_start >= endptr)
        return;

    ti99_implicit_extent = endptr - block_start;
    ti99_implicit_actions = MemAlloc(ti99_implicit_extent);
    memcpy(ti99_implicit_actions, block_start, ti99_implicit_extent);
}

/* Copy the explicit (player-triggered) action bytecode out of the
   game file.  The explicit table is indexed by verb number: each
   entry is a 16-bit pointer to a chain of action blocks for that
   verb.  We find the overall extent of the data (start..end),
   copy it into a single allocation, and rebase VerbActionOffsets
   to point into the copy. */
static void ReadTI99ExplicitActions(const DATAHEADER *dh)
{
    uint8_t *endptr = entire_file + file_length;

    size_t explicit_offset = FixAddress(READ_BE_UINT16(&dh->p_explicit));
    if (explicit_offset >= file_length)
        return;
    uint8_t *verb_table = entire_file + explicit_offset;

    VerbActionOffsets = MemAlloc((dh->num_verbs + 1) * sizeof(uint8_t *));

    uint8_t *data_start = endptr;
    uint8_t *data_end = entire_file;

    for (int i = 0; i <= dh->num_verbs; i++) {
        VerbActionOffsets[i] = NULL;

        if (verb_table + (i + 1) * 2 > endptr)
            continue;
        uint16_t verb_address = GetWord(verb_table + i * 2);
        if (verb_address == 0)
            continue;

        uint8_t *scan = entire_file + FixAddress(verb_address);
        if (scan >= endptr)
            continue;

        if (scan < data_start)
            data_start = scan;
        VerbActionOffsets[i] = scan;

        /* The last block of a chain has a zero length byte, so its own
           size is not recorded: keep everything up to the end of the
           file rather than cutting that block short. */
        data_end = endptr;
    }

    if (data_end <= data_start)
        return;

    ti99_explicit_extent = data_end - data_start;
    ti99_explicit_actions = MemAlloc(ti99_explicit_extent);
    memcpy(ti99_explicit_actions, data_start, ti99_explicit_extent);

    /* Rebase pointers from the original file buffer into the copy. */
    for (int i = 0; i <= dh->num_verbs; i++) {
        if (VerbActionOffsets[i] != NULL) {
            VerbActionOffsets[i] = ti99_explicit_actions + (VerbActionOffsets[i] - data_start);
        }
    }
}

/* Extract the 24x40 title screen stored at a fixed offset in the
   game file.  The TI-99/4A uses a non-standard 7-bit character mapping:
   '\\' is blank, '|' followed by a non-space triggers a form feed,
   and ')' without a preceding '(' becomes '@'. */
static uint8_t *LoadTitleScreen(void)
{
    char screen_buf[3074];
    int write_pos = 0;

    uint8_t *endptr = entire_file + file_length;
    uint8_t *src = entire_file + 0x80 + file_baseline_offset;
    if (src >= endptr)
        return NULL;

    int in_parens = 0;
    for (int lines = 0; lines < 24; lines++) {
        for (int i = 0; i < 40; i++) {
            if (src >= endptr)
                return NULL;
            char ch = *(src++) & 0x7f;
            switch (ch) {
            case 0x7f:
                ch = '#';
                break;
            case '\\':
                ch = ' ';
                break;
            case '(':
                in_parens = 1;
                break;
            case ')':
                if (!in_parens)
                    ch = '@';
                in_parens = 0;
                break;
            case '|':
                if (src < endptr && *src != ' ')
                    ch = 12;
                break;
            default:
                break;
            }
            screen_buf[write_pos++] = ch;
            if (write_pos >= 3072)
                return NULL;
        }
        screen_buf[write_pos++] = '\n';
    }

    screen_buf[write_pos] = 0;
    uint8_t *result = MemAlloc(write_pos + 1);
    memcpy(result, screen_buf, write_pos + 1);
    return result;
}

/* Room, message and item texts: compressed strings, each reconstructed
   by GetTI994AString from its pointer table. */
static void LoadTI99Texts(const DATAHEADER *dh)
{
    for (int idx = 0; idx <= GameHeader.NumRooms; idx++) {
        Room *room = &Rooms[idx];
        room->Text = GetTI994AString(dh->p_room_descr, idx);
        if (room->Text == NULL)
            room->Text = ".";
        LoadTrace("Room %d: %s\n", idx, room->Text);
        room->Image = 255;
    }

    for (int idx = 0; idx <= GameHeader.NumMessages; idx++) {
        Messages[idx] = GetTI994AString(dh->p_message, idx);
        if (Messages[idx] == NULL)
            Messages[idx] = ".";
        LoadTrace("Message %d: %s\n", idx, Messages[idx]);
    }

    /* Items whose text begins with '*' are treasures. They are counted
       here only for a header that gives no number: the ruby of Pyramid
       of Doom is starred too, and has to be destroyed, not stored. */
    int num_treasures = 0;
    for (int idx = 0; idx <= GameHeader.NumItems; idx++) {
        Item *item = &Items[idx];
        item->Text = GetTI994AString(dh->p_obj_descr, idx);
        if (item->Text == NULL) {
            item->Text = ".";
            item->AutoGet = NULL;
        }
        if (item->Text[0] == '*')
            num_treasures++;
        LoadTrace("Item %d: %s\n", idx, item->Text);
    }

    GameHeader.Treasures = dh->num_treasures ? dh->num_treasures : num_treasures;
    LoadTrace("Number of treasures %d\n", GameHeader.Treasures);
}

/* Verb and noun dictionaries, loaded from separate pointer tables.  The
   verb and noun counts may differ, so the shorter array is padded with
   "." entries to keep both arrays the same length for uniform indexing. */
static void LoadTI99Dictionary(const DATAHEADER *dh)
{
    LoadTI994ADict(dh->p_verb_table, dh->num_verbs + 1, Verbs);
    LoadTI994ADict(dh->p_noun_table, dh->num_nouns + 1, Nouns);

    for (int i = 1; i <= dh->num_nouns - dh->num_verbs; i++)
        Verbs[dh->num_verbs + i] = ".";

    for (int i = 1; i <= dh->num_verbs - dh->num_nouns; i++)
        Nouns[dh->num_nouns + i] = ".";

    if (Options & DEBUGGING) {
        for (int i = 0; i <= GameHeader.NumWords; i++)
            LoadTrace("Verb %d: %s\n", i, Verbs[i]);
        for (int i = 0; i <= GameHeader.NumWords; i++)
            LoadTrace("Noun %d: %s\n", i, Nouns[i]);
    }
}

/* Noun-to-object links (AutoGet): each item has a one-byte noun index
   that lets the parser automatically resolve "GET <noun>" to the correct
   item.  A zero or out-of-range index means the item has no AutoGet.
   Returns 0 if the table runs past the end of the file. */
static int ReadNounLinks(uint8_t **data_ptr, const uint8_t *endptr)
{
    for (int idx = 0; idx <= GameHeader.NumItems; idx++) {
        if (*data_ptr >= endptr)
            return 0;
        int noun = *((*data_ptr)++);
        Item *item = &Items[idx];
        if (noun && noun <= GameHeader.NumWords) {
            item->AutoGet = Nouns[noun];
            /* Colossal Cave item 3 ("bird") has no noun link in the data.
               A hack like this is probably overkill for such a broken game. */
            if (idx == 3 && strncmp("bird", item->Text, 4) == 0)
                item->AutoGet = "BIRD";
        } else {
            item->AutoGet = NULL;
        }
    }
    return 1;
}

/* Parse all game data sections from the file into the global game
   state (Rooms, Items, Messages, Verbs, Nouns, actions).  Sections
   are located via the DATAHEADER pointers; SeekIfNeeded positions
   the read pointer (with baseline offset compensation) for each.
   Returns TI994A on success or UNKNOWN_GAME on failure. */
static GameIDType TryLoadingTI994A(const DATAHEADER *dh)
{
    uint8_t *endptr = entire_file + file_length;

    /* Unpack the binary header.  The header uses big-endian layout
       matching the TI-99/4A's native byte order.  Treasures are counted
       from the item texts below, and the actions are TI-99/4A bytecode
       rather than Action entries, so both counts start at 0. */
    Header hdr = { 0 };
    hdr.NumItems = dh->num_objects;
    hdr.NumWords = MAX(dh->num_verbs, dh->num_nouns);
    hdr.NumRooms = dh->red_room;
    hdr.MaxCarry = dh->max_items_carried;
    hdr.PlayerRoom = dh->begin_locn;
    hdr.TreasureRoom = dh->treasure_locn;
    hdr.WordLength = dh->cmd_length;
    hdr.LightTime = READ_BE_UINT16(&dh->light_turns);
    hdr.NumMessages = max_messages; /* derived earlier by CountTI99TableEntries */

    /* Populate the global GameHeader and allocate (zero-filled) arrays
       for all game data sections. */
    SetGameHeader(&hdr);
    AllocateGameData();

    uint8_t *data_ptr = entire_file;
    size_t seek_offset;

    if (SeekIfNeeded(FixAddress(READ_BE_UINT16(&dh->p_room_descr)), &seek_offset, &data_ptr) == 0)
        return UNKNOWN_GAME;

    LoadTI99Texts(dh);

    /* The remaining tables are raw bytes, not pointer-table compressed
       strings, so they are read sequentially via data_ptr.

       Room exits: six bytes per room (N, S, E, W, Up, Down), each giving
       the destination room number (0 = no exit). */
    if (!SeekToRawData(dh->p_room_exit, &seek_offset, &data_ptr))
        return UNKNOWN_GAME;

    if (!ReadRoomExits(&data_ptr, GameHeader.NumRooms, endptr))
        return UNKNOWN_GAME;

    /* Initial item locations: one byte per item, the room number where
       the item starts.  Also saved as InitialLoc so the engine can detect
       whether an item has been moved (opcode 200/201). */
    if (!SeekToRawData(dh->p_orig_items, &seek_offset, &data_ptr))
        return UNKNOWN_GAME;

    if (!ReadItemLocations(&data_ptr, GameHeader.NumItems, endptr))
        return UNKNOWN_GAME;

    LoadTI99Dictionary(dh);

    if (!SeekToRawData(dh->p_obj_link, &seek_offset, &data_ptr))
        return UNKNOWN_GAME;

    if (!ReadNounLinks(&data_ptr, endptr))
        return UNKNOWN_GAME;

    /* --- Action bytecode ---
       Copy implicit (auto-run per turn) and explicit (player-triggered)
       action bytecode out of the file buffer into dedicated allocations
       so they survive after entire_file is freed below. */
    ReadTI99ImplicitActions(dh);
    ReadTI99ExplicitActions(dh);

    /* TI-99/4A games always start with inventory visible. */
    AutoInventory = 1;
    sys[INVENTORY] = "I'm carrying: ";

    title_screen = (char *)LoadTitleScreen();

    /* The file buffer is no longer needed — all data has been extracted
       into game-state structures and action bytecode copies. */
    free(entire_file);
    entire_file = NULL;

    /* Install TI-99/4A system messages (e.g. "OK", "What?", etc.). */
    for (int i = 0; i < MAX_SYSMESS && sysdict_TI994A[i] != NULL; i++) {
        sys[i] = sysdict_TI994A[i];
    }

    Options |= TI994A_STYLE;
    CurrentSys = SYS_TI994A;
    return TI994A;
}
