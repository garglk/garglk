//
//  detectgame.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "scott_meta.h"

#include "common_file_utils.h"
#include "decompress_text.h"
#include "detect_game.h"
#include "line_drawing.h"
#include "sagadraw.h"
#include "saga.h"
#include "scott_game_info.h"

#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"

#include "apple2detect.h"
#include "atari8detect.h"
#include "c64decrunch.h"
#include "decompressz80.h"
#include "load_ti99_4a.h"
#include "title_image.h"

#include "parser.h"

/* ZX_RAM_BASE (0x4000, from decompressz80.h) is where the RAM of the ZX
   Spectrum begins and where loaded programs live. Used to translate Z80
   addresses stored in saved .z80 snapshots to offsets into our in-memory
   file buffer. */

/* Number of 16-bit words in a game file header */
#define HEADER_WORDS 15

/* Number of system message strings stored in the game file */
#define NUM_OLD_STYLE_SYSMESS 40
#define NUM_HOWARTH_SYSMESS 45

/* The raw header words of the game file being loaded */
int header[24];

/* Byte signatures used to locate the dictionary in a game file.
   Each entry pairs a dictionary format with a short sequence of bytes
   that uniquely identifies it (typically the first few dictionary words). */
struct dictionaryKey {
    DictionaryType dict;
    const char *signature;
    int len;
};

struct dictionaryKey dictKeys[] = {
    { FOUR_LETTER_UNCOMPRESSED, "AUTO\0GO", 8 },
    { THREE_LETTER_UNCOMPRESSED, "AUT\0GO", 7 },
    { FIVE_LETTER_UNCOMPRESSED, "GO\0\0\0\0*CROSS*RUN", 17 }, // Claymorgue
    { FOUR_LETTER_COMPRESSED, "aUTOgO", 7 },
    { GERMAN_C64, "gEHENSTEIGE", 11 }, // Gremlins German C64
    { GERMAN, "\xc7"
              "EHENSTEIGE", 10 },
    { SPANISH, "\x81\0\0\0\xc9R\0\0ANDAENTR", 16 },
    { SPANISH_C64, "\x81\0\0\0iR\0\0ANDAENTR", 16 },
    { ITALIAN, "AUTO\0VAI\0\0*ENTR", 15 },
    { NOT_A_GAME, NULL, 0 }
};

/* Search `data` (of length `datasize`) for the byte sequence `pattern`
   (of length `patternLen`). Returns the offset of the first match, or
   -1 if not found. */
int FindCode(const uint8_t *data, size_t dataLen, const char *pattern, int patternLen)
{
    if (data == NULL || dataLen < (size_t)patternLen)
        return -1;
    const uint8_t *cursor = data;
    const uint8_t *lastPossibleStart = data + dataLen - patternLen;
    while (cursor <= lastPossibleStart) {
        if (memcmp(cursor, pattern, patternLen) == 0) {
            return (int)(cursor - data);
        }
        cursor++;
    }
    return -1;
}

/* Identify the game's dictionary format by scanning `data` for known
   signatures. Returns the dictionary type and sets *offset to the start
   of the dictionary. Some formats require a backward offset adjustment
   because the signature appears partway through the dictionary. */
DictionaryType GetId(const uint8_t *data, size_t dataLen, size_t *offset)
{
    for (int i = 0; dictKeys[i].dict != NOT_A_GAME; i++) {
        int found = FindCode(data, dataLen, dictKeys[i].signature, dictKeys[i].len);
        if (found != -1) {
            *offset = found;
            switch (dictKeys[i].dict) {
            case GERMAN_C64:
            case GERMAN:
                *offset -= 5;
                break;
            case FIVE_LETTER_UNCOMPRESSED:
                *offset -= 6;
                break;
            default:
                break;
            }
            return dictKeys[i].dict;
        }
    }

    return NOT_A_GAME;
}

/* Read HEADER_WORDS little-endian 16-bit values from ptr into the global header[].
   Returns a pointer to the last byte read (ptr + 29). */
uint8_t *ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < HEADER_WORDS; i++) {
        value = READ_LE_UINT16(ptr);
        header[i] = value;
        ptr += 2;
    }
    return ptr - 1;
}

/* Validate that parsed header values are within reasonable ranges.
   Rejects databases with too few/many items, actions, words, or rooms. */
static int SanityCheckHeader(void)
{
    int16_t v = GameHeader.NumItems;
    if (v < 10 || v > 500)
        return 0;
    v = GameHeader.NumActions;
    if (v < 100 || v > 500)
        return 0;
    v = GameHeader.NumWords; // word pairs
    if (v < 50 || v > 190)
        return 0;
    v = GameHeader.NumRooms; // Number of rooms
    if (v < 10 || v > 100)
        return 0;

    return 1;
}

/* Append c to the dictionary word being built, leaving room for the
   terminating zero */
static void AddDictionaryChar(char *dictword, int *charindex, char c)
{
    if (*charindex < 31)
        dictword[(*charindex)++] = c;
}

/* Parse the dictionary from a binary game file into the global Verbs[]
   and Nouns[] arrays. Handles three encoding schemes:
   - Compressed/C64: first letter case indicates synonym ('*' prefix)
   - Localized (non-English): high bit on first byte = new word
   - Uncompressed: NUL-separated words, '*' = synonym marker
   A byte > 127 terminates the dictionary. */
static uint8_t *ReadDictionary(const GameInfo *info, uint8_t *ptr)
{
    int wl = info->word_length;
    if (wl < 1 || wl > 30)
        Fatal("Bad word length");
    char dictword[32];
    char c = 0;
    int charindex = 0;
    uint8_t *endptr = entire_file + file_length;

    int nv = info->number_of_verbs;
    int nn = info->number_of_nouns;

    for (int i = 0; i < info->number_of_words + 2; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    for (int wordnum = 0; wordnum <= nv + nn; wordnum++) {
        int restarts = 0;
        for (int i = 0; i < wl; i++) {
            if (ptr >= endptr)
                return ptr;
            c = *(ptr++);

            if (info->dictionary == FOUR_LETTER_COMPRESSED || info->dictionary == GERMAN_C64 || info->dictionary == SPANISH_C64) {
                if (charindex == 0) {
                    if (c >= 'a')
                        c = toupper(c);
                    else if (c != '.' && c != 0)
                        AddDictionaryChar(dictword, &charindex, '*');
                }
                AddDictionaryChar(dictword, &charindex, c);
            } else if (info->subtype == LOCALIZED) {
                if (charindex == 0) {
                    if (c & 0x80)
                        c = c & 0x7f;
                    else if (c != '.')
                        AddDictionaryChar(dictword, &charindex, '*');
                }
                AddDictionaryChar(dictword, &charindex, c);
            } else {
                if (c == 0 && charindex == 0) {
                    if (ptr >= endptr)
                        return ptr;
                    c = *(ptr++);
                }
                if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                    i--;
                    charindex--;
                }
                if (c == '*') {
                    charindex = 0;
                    i = -1;
                    if (++restarts > wl)
                        break;
                }
                AddDictionaryChar(dictword, &charindex, c);
            }
        }
        dictword[charindex] = 0;

        char *word = MemStrndup(dictword, charindex);
        if (wordnum < nv) {
            Verbs[wordnum] = word;
            debug_print("Verb %d: \"%s\"\n", wordnum, word);
        } else {
            Nouns[wordnum - nv] = word;
            debug_print("Noun %d: \"%s\"\n", wordnum - nv, word);
        }

        if (c > 127)
            return ptr;

        charindex = 0;
    }

    return ptr;
}

/* Convert a file offset to a pointer into the loaded file buffer.
   Returns NULL if the offset is out of range. */
uint8_t *SeekToPos(int offset)
{
    if (offset < 0 || (size_t)offset > file_length || entire_file == NULL)
        return NULL;
    return entire_file + offset;
}

/* Seek to a game data section if its position is specified (not FOLLOWS).
   Applies the file_baseline_offset adjustment. Returns 0 on seek failure. */
int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr)
{
    if (expected_start != FOLLOWS) {
        *offset = expected_start + file_baseline_offset;
        *ptr = SeekToPos(*offset);
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

/* Where ParseHeader() finds one Header field: a whole raw header word, its
   low or high byte, the word minus one, or a constant. */
typedef enum {
    HDR_CONST,
    HDR_WORD,
    HDR_LOW_BYTE,
    HDR_HIGH_BYTE,
    HDR_WORD_MINUS_1,
} HeaderSourceKind;

typedef struct {
    HeaderSourceKind kind;
    int value; /* Index into the raw header, or the constant */
} HeaderSource;

#define W(i) { HDR_WORD, i }
#define LO(i) { HDR_LOW_BYTE, i }
#define HI(i) { HDR_HIGH_BYTE, i }
#define W_MINUS_1(i) { HDR_WORD_MINUS_1, i }
#define K(v) { HDR_CONST, v }

/* The header layouts of the different platforms and game generations. Some
   pack two values into one 16-bit word (e.g. max_carry + player_room in the
   low and high bytes). */
static const struct {
    HeaderType type;
    /* NumItems, NumActions, NumWords, NumRooms, MaxCarry, PlayerRoom,
       Treasures, WordLength, LightTime, NumMessages, TreasureRoom */
    HeaderSource field[11];
} header_layouts[] = {
    { EARLY,                          { W(1), W(2), W(3),   W(4), W(5),  W(6),  W(7),  W(8),  W(9),  W(10), W(11) } },
    { LATE,                           { W(1), W(2), W(3),   W(4), W(5),  K(1),  K(0),  W(6),  K(-1), W(7),  K(0) } },
    { US_HEADER,                      { W(3), W(2), W(1),   W(5), W(6),  W(7),  W(8),  W(0),  W(9),  W(4),  HI(10) } },
    { ROBIN_C64_HEADER,               { W(1), W(2), W(6),   W(4), W(5),  K(1),  K(0),  W(7),  K(-1), W(3),  K(0) } },
    { GREMLINS_C64_HEADER,            { W(1), W(2), W(5),   W(3), W(6),  W(8),  K(0),  W(7),  K(-1), K(98), K(0) } },
    { SUPERGRAN_C64_HEADER,           { W(3), W(1), W(2),   W(4), W(8),  K(1),  K(0),  W(6),  K(-1), W(5),  K(0) } },
    { SEAS_OF_BLOOD_C64_HEADER,       { W(0), W(1), K(134), W(3), W(4),  K(1),  K(0),  W(6),  K(-1), W(2),  K(0) } },
    { MYSTERIOUS_C64_HEADER,          { W(1), W(2), W(3),   W(4), LO(5), HI(5), W(6),  W(7),  W(8),  W(9),  K(0) } },
    { ARROW_OF_DEATH_PT_2_C64_HEADER, { W(3), W(1), W(2),   W(4), LO(5), HI(5), W(6),  W(7),  W(8),  W(9),  K(0) } },
    { INDIANS_C64_HEADER,             { W(1), W(2), W(3),   W(4), LO(5), HI(5), LO(6), HI(6), HI(7), HI(8), K(0) } },
    /* No leading unused word, and the light time comes last. h[8] counts the
       messages rather than naming the last one. h[9] repeats the treasure
       room and is never read by the original interpreter. */
    { SPIDERMAN_ATARI8_HEADER,        { W(0), W(1), W(2),   W(3), W(4),  W(5),  W(6),  W(7),  W(11), W_MINUS_1(8), W(10) } },
};

#undef W
#undef LO
#undef HI
#undef W_MINUS_1
#undef K

static int HeaderValue(const int *h, HeaderSource source)
{
    switch (source.kind) {
    case HDR_WORD:
        return h[source.value];
    case HDR_LOW_BYTE:
        return h[source.value] & 0xff;
    case HDR_HIGH_BYTE:
        return h[source.value] >> 8;
    case HDR_WORD_MINUS_1:
        return h[source.value] - 1;
    case HDR_CONST:
    default:
        return source.value;
    }
}

/* Extract game parameters from the raw header[] array based on the header
   layout type (see header_layouts[] above). Returns 0 for an unknown type. */
int ParseHeader(int *h, HeaderType type, Header *out)
{
    for (size_t i = 0; i < sizeof(header_layouts) / sizeof(header_layouts[0]); i++) {
        if (header_layouts[i].type != type)
            continue;
        const HeaderSource *f = header_layouts[i].field;
        out->NumItems     = HeaderValue(h, f[0]);
        out->NumActions   = HeaderValue(h, f[1]);
        out->NumWords     = HeaderValue(h, f[2]);
        out->NumRooms     = HeaderValue(h, f[3]);
        out->MaxCarry     = HeaderValue(h, f[4]);
        out->PlayerRoom   = HeaderValue(h, f[5]);
        out->Treasures    = HeaderValue(h, f[6]);
        out->WordLength   = HeaderValue(h, f[7]);
        out->LightTime    = HeaderValue(h, f[8]);
        out->NumMessages  = HeaderValue(h, f[9]);
        out->TreasureRoom = HeaderValue(h, f[10]);
        return 1;
    }
    if (type != NO_HEADER)
        debug_print("Unhandled header type!\n");
    return 0;
}

/* Print parsed header values for debugging (only active when DEBUG_PRINT is set).
   h is the raw header[] array to dump first, or NULL. */
void PrintHeaderInfo(int *h, const Header *hdr)
{
#if (DEBUG_PRINT)
    uint16_t value;
    for (int i = 0; h != NULL && i < 13; i++) {
        value = h[i];
        debug_print("b $%X %d: ", 0x494d + 0x3FE5 + i * 2, i);
        debug_print("\t%d (%04x)\n", value, value);
    }

    debug_print("Number of items =\t%d\n", hdr->NumItems);
    debug_print("Number of actions =\t%d\n", hdr->NumActions);
    debug_print("Number of words =\t%d\n", hdr->NumWords);
    debug_print("Number of rooms =\t%d\n", hdr->NumRooms);
    debug_print("Max carried items =\t%d\n", hdr->MaxCarry);
    debug_print("Word length =\t%d\n", hdr->WordLength);
    debug_print("Number of messages =\t%d\n", hdr->NumMessages);
    debug_print("Player start location: %d\n", hdr->PlayerRoom);
    debug_print("Treasure room: %d\n", hdr->TreasureRoom);
    debug_print("Lightsource time left: %d\n", hdr->LightTime);
    debug_print("Number of treasures: %d\n", hdr->Treasures);
#else
    (void)h;
    (void)hdr;
#endif
}

/* Load Mysterious Adventures-format line-drawing vector image data, if the
   game has any. Each image starts with 0xFF followed by a background colour
   byte, then drawing commands until the next 0xFF. Truncated images are
   patched out with Image=255. */
static void LoadVectorData(const GameInfo *info, uint8_t *ptr)
{
    size_t offset;

    if (info->number_of_pictures <= 0 || info->picture_format_version != HOWARTH_FORMAT)
        return;

    if (info->start_of_image_data == FOLLOWS)
        ptr++;
    else if (SeekIfNeeded(info->start_of_image_data, &offset, &ptr) == 0)
        return;

    /* Zeroed, so that the images after a cut-off one are empty rather
       than garbage. */
    LineImages = MemCalloc(info->number_of_rooms * sizeof(line_image));
    uint8_t byte = *(ptr++);

    for (int ct = 0; ct < info->number_of_rooms; ct++) {
        Rooms[ct].Image = 0;
        if (byte == 0xff) {
            LineImages[ct].bgcolour = *(ptr++);
            LineImages[ct].data = ptr;
        } else {
            debug_print("Error! Image data does not start with 0xff!\n");
        }
        do {
            byte = *(ptr++);
            if (ptr >= entire_file + file_length) {
                debug_print("Error! Image data for image %d cut off!\n", ct);
                if (GameHeader.NumRooms - ct > 1)
                    Display(Bottom, "[This copy has %d broken or missing pictures. These have been patched out.]\n\n", GameHeader.NumRooms - ct);
                LineImages[ct].size = (LineImages[ct].data < ptr) ? ptr - LineImages[ct].data - 1 : 0;
                for (int i = ct + 2; i < GameHeader.NumRooms; i++)
                    Rooms[i].Image = 255;
                return;
            }
        } while (byte != 0xff);

        LineImages[ct].size = ptr - LineImages[ct].data;
    }
}

/* Validate a parsed header against the expected GameInfo values and
   populate the global GameHeader. Returns 0 on any mismatch. */
static int ValidateAndApplyHeader(const GameInfo *info, const Header *hdr,
                                  size_t offset)
{
    PrintHeaderInfo(header, hdr);

    if (hdr->NumItems != info->number_of_items || hdr->NumActions != info->number_of_actions ||
        hdr->NumWords != info->number_of_words || hdr->NumRooms != info->number_of_rooms ||
        hdr->MaxCarry != info->max_carried) {
        debug_print("Non-matching header\n");
        return 0;
    }

    SetGameHeader(hdr);

    if (SanityCheckHeader() == 0)
        return 0;

    debug_print("Found a valid header at position 0x%zx\n", offset);
    if (offset != (size_t)(info->start_of_header + file_baseline_offset)) {
        debug_print("Expected: 0x%x\n",
                    info->start_of_header + file_baseline_offset);
    }

    AllocateGameData();
    return 1;
}

/* Set up the file baseline offset, seek to the header, read and parse it,
   then validate against the expected GameInfo values. On success the global
   GameHeader is populated and all game arrays are allocated. */
static int LoadGameHeader(const GameInfo *info, int dict_start,
                          uint8_t **ptr, size_t *offset)
{
    file_baseline_offset = dict_start - info->start_of_dictionary;
    debug_print("file_baseline_offset: %d (%x)\n",
                file_baseline_offset, file_baseline_offset);

    *offset = info->start_of_header + file_baseline_offset;
    *ptr = SeekToPos(*offset);
    if (*ptr == NULL)
        return 0;

    ReadHeader(*ptr);

    Header hdr;
    if (!ParseHeader(header, info->header_style, &hdr))
        return 0;

    return ValidateAndApplyHeader(info, &hdr, *offset);
}

/* Read room exit connections from binary data: one byte per exit,
   6 exits per room. Returns 0 if the table would run past endptr. */
int ReadRoomExits(uint8_t **ptr, int num_rooms, const uint8_t *endptr)
{
    for (int ct = 0; ct <= num_rooms; ct++) {
        if (*ptr + 6 > endptr)
            return 0;
        memcpy(Rooms[ct].Exits, *ptr, 6);
        *ptr += 6;
    }
    return 1;
}

/* Read item starting locations from binary data: one byte per item.
   Returns 0 if the table would run past endptr. */
int ReadItemLocations(uint8_t **ptr, int num_items, const uint8_t *endptr)
{
    for (int ct = 0; ct <= num_items; ct++) {
        if (*ptr >= endptr)
            return 0;
        Items[ct].Location = *((*ptr)++);
        Items[ct].InitialLoc = Items[ct].Location;
    }
    return 1;
}

/* Read NUL-terminated room descriptions from binary data, rejecting
   any string containing a byte > 127. Assigns room images based on
   whether the images are in vector format and whether the game has
   pictures. (Irmak tile style room images are already assigned in TryLoading().)
   Returns 0 on invalid data. */
static int ReadBinaryRoomDescs(uint8_t **ptr, int num_rooms, int has_pictures, int vector)
{
    for (int ct = 0; ct <= num_rooms; ct++) {
        uint8_t *p = *ptr;
        while (*p > 0 && *p <= 127)
            p++;
        if (*p > 127)
            return 0;
        Rooms[ct].Text = MemStrndup((char *)*ptr, p - *ptr);
        debug_print("Room %d: %s\n", ct, Rooms[ct].Text);
        if (vector || !has_pictures)
            Rooms[ct].Image = has_pictures ? ct - 1 : 255;
        *ptr = p + 1;
    }
    return 1;
}

/* Read NUL-or-CR-terminated strings from binary data into dest[].
   Stops after max_count strings, on out-of-bounds reads, or when a
   byte > 127 is encountered (if allowed_high is non-NULL, only bytes
   not in the string cause a stop; if NULL, all bytes are accepted).
   Returns the number of strings read. */
static int ReadTerminatedStrings(uint8_t **ptr, const char **dest, int max_count,
                                 const char *allowed_high)
{
    int count = 0;

    while (count < max_count) {
        uint8_t *string_start = *ptr;
        if (string_start == NULL)
            return count;
        uint8_t *scan = string_start;

        while (scan >= entire_file && (size_t)(scan - entire_file) < file_length
               && *scan != 0 && *scan != '\r') {
            if (*scan > 127 && allowed_high != NULL && strchr(allowed_high, *scan) == NULL) {
                *ptr = scan;
                return count;
            }
            scan++;
        }

        if (scan < entire_file || (size_t)(scan - entire_file) >= file_length) {
            debug_print("Read out of bounds!\n");
            *ptr = scan;
            return count;
        }

        size_t length = scan - string_start;
        if (length > 0) {
            int is_cr = (*scan == '\r');
            size_t alloc_size = length + (is_cr ? 2 : 1);
            char *string = MemAlloc(alloc_size);
            memcpy(string, string_start, length);
            if (is_cr) {
                string[length] = '\r';
                string[length + 1] = '\0';
            } else {
                string[length] = '\0';
            }
            dest[count] = string;
            debug_print("String %d: \"%s\"\n", count, dest[count]);
            count++;
        }

        *ptr = scan + 1;
    }

    return count;
}

/* Copy parsed header values into the global GameHeader (leaving its Unknown
   field alone) and refill the light source. */
void SetGameHeader(const Header *h)
{
    GameHeader.NumItems = h->NumItems;
    GameHeader.NumActions = h->NumActions;
    GameHeader.NumWords = h->NumWords;
    GameHeader.WordLength = h->WordLength;
    GameHeader.NumRooms = h->NumRooms;
    GameHeader.MaxCarry = h->MaxCarry;
    GameHeader.PlayerRoom = h->PlayerRoom;
    GameHeader.Treasures = h->Treasures;
    GameHeader.LightTime = h->LightTime;
    LightRefill = h->LightTime;
    GameHeader.NumMessages = h->NumMessages;
    GameHeader.TreasureRoom = h->TreasureRoom;
}

void AllocateGameData(void)
{
    /* The lamp is item LIGHT_SOURCE (9) by convention, and the engine reads
       Items[LIGHT_SOURCE] every turn for the lamp timer without consulting
       NumItems. Every real Scott database has at least ten items, but a
       malformed one need not, and the read would then be out of bounds. Always
       allocate through LIGHT_SOURCE so it reads back as a destroyed item. */
    int num_items = GameHeader.NumItems + 1;
    if (num_items < LIGHT_SOURCE + 1)
        num_items = LIGHT_SOURCE + 1;

    /* Zero-initialise: the engine treats an unset Room.Text / Item.Image /
       Item.Flag as "absent" (e.g. Look()'s `if (!r->Text) return;`), so any
       field a particular loader doesn't fill must read back as NULL/0 rather
       than as heap garbage. */
    Items = MemCalloc(sizeof(Item) * num_items);
    Actions = MemCalloc(sizeof(Action) * (GameHeader.NumActions + 1));
    Verbs = MemCalloc(sizeof(char *) * (GameHeader.NumWords + 2));
    Nouns = MemCalloc(sizeof(char *) * (GameHeader.NumWords + 2));
    Rooms = MemCalloc(sizeof(Room) * (GameHeader.NumRooms + 1));
    Messages = MemCalloc(sizeof(char *) * (GameHeader.NumMessages + 1));
}

static char *ReadBinaryString(uint8_t **ptr)
{
    size_t len = strlen((char *)*ptr);
    char *result = MemStrndup((char *)*ptr, len);
    *ptr += len + 1;
    return result;
}

/* Read NUL-terminated messages from binary data. */
static void ReadBinaryMessages(uint8_t **ptr)
{
    for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
        Messages[ct] = ReadBinaryString(ptr);
        debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }
}

/* Read action tables from binary data. In compressed format, each action
   has a packed byte encoding the number of conditions (low 5 bits) and
   commands (high 3 bits). In uncompressed format, all 5 condition and
   2 command slots are always present. */
static void ReadActions(uint8_t **ptr, int num_actions, int compressed)
{
    for (int ct = 0; ct <= num_actions; ct++) {
        Actions[ct].Vocab = READ_LE_UINT16_AND_ADVANCE(ptr);

        uint16_t cond, comm;
        if (compressed) {
            uint16_t value = *((*ptr)++);
            cond = value & 0x1f;
            if (cond > 5) {
                debug_print("Condition error at action %d!\n", ct);
                cond = 5;
            }
            comm = (value & 0xe0) >> 5;
            if (comm > 2) {
                debug_print("Command error at action %d!\n", ct);
                comm = 2;
            }
        } else {
            cond = 5;
            comm = 2;
        }
        for (int j = 0; j < 5; j++)
            Actions[ct].Condition[j] = (j < cond) ? READ_LE_UINT16_AND_ADVANCE(ptr) : 0;
        for (int j = 0; j < 2; j++)
            Actions[ct].Opcode[j] = (j < comm) ? READ_LE_UINT16_AND_ADVANCE(ptr) : 0;
    }
}

/* Read item descriptions terminated by NUL or high byte (>126).
   Each item's text is parsed for the /AutoGet/ word. */
static void ReadBinaryItemDescs(uint8_t **ptr, int num_items)
{
    for (int ct = 0; ct <= num_items; ct++) {
        uint8_t *p = *ptr;
        while (*p > 0 && *p <= 126)
            p++;
        Items[ct].Text = MemStrndup((char *)*ptr, p - *ptr);
        *ptr = p + 1;
        ParseItemSlashAutoGet(ct);
    }
}

/* Read the room descriptions of TryLoading(): compressed text, or
   NUL-terminated strings. Returns 0 on invalid data. */
static int ReadRoomDescs(uint8_t **ptr, const GameInfo *info, int compressed)
{
    if (!compressed)
        return ReadBinaryRoomDescs(ptr, GameHeader.NumRooms, info->number_of_pictures > 0, 0);

    /* Rooms hold NumRooms+1 entries (0..NumRooms); the last room is the
       death/limbo room the engine jumps to in PlayerIsDead() and the
       dark-move death. Every other room loop uses <=; this one must too,
       or Rooms[NumRooms].Text is left uninitialised and Look() reads a
       wild pointer the first time the player dies. */
    for (int ct = 0; ct <= GameHeader.NumRooms; ct++) {
        Rooms[ct].Text = DecompressText(*ptr, ct);
        if (Rooms[ct].Text == NULL)
            return 0;
        Rooms[ct].Text[0] = tolower(Rooms[ct].Text[0]);
    }
    return 1;
}

/* Read the messages of TryLoading(): compressed text, or NUL-terminated
   strings. Returns 0 on invalid data. */
static int ReadMessages(uint8_t **ptr, int compressed)
{
    if (!compressed) {
        ReadBinaryMessages(ptr);
        return 1;
    }

    for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
        Messages[ct] = DecompressText(*ptr, ct);
        debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
        if (Messages[ct] == NULL)
            return 0;
    }
    return 1;
}

/* Extract the auto-get/drop word from a compressed item description, which
   uses the format "description.WORD." (the compressed counterpart of
   ParseItemSlashAutoGet()). Upper-cases the word after its first letter. */
static void ParseItemDotAutoGet(int index)
{
    char *word = strchr(Items[index].Text, '.');
    Items[index].AutoGet = NULL;
    if (word == NULL)
        return;
    *word++ = 0;
    word++;
    char *t = strchr(word, '.');
    if (t != NULL)
        *t = 0;
    for (int i = 1; i < GameHeader.WordLength; i++)
        word[i] = toupper(word[i]);
    Items[index].AutoGet = word;
}

/* Read the item descriptions of TryLoading(): compressed text, or
   NUL-terminated strings, and extract their auto-get words. */
static void ReadItemDescs(uint8_t **ptr, int compressed)
{
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        if (compressed) {
            Items[ct].Text = DecompressText(*ptr, ct);
            Items[ct].AutoGet = NULL;
            if (Items[ct].Text != NULL && Items[ct].Text[0] != '.') {
                debug_print("Item %d: %s\n", ct, Items[ct].Text);
                ParseItemDotAutoGet(ct);
            }
        } else {
            Items[ct].Text = ReadBinaryString(ptr);
            debug_print("Item %d: %s\n", ct, Items[ct].Text);
            ParseItemSlashAutoGet(ct);
        }
    }
}

/* The system messages of the English C64 games may start a little before
   their listed offset. Step back from offset to the nearest position where
   the table, after any padding, starts with "NORTH". */
static size_t FindC64SystemMessages(size_t offset)
{
    while (offset > 0) {
        uint8_t *ptr = SeekToPos(offset);
        while (ptr < entire_file + file_length && (*ptr == 0 || *ptr == '\r'))
            ptr++;
        if (entire_file + file_length - ptr >= 5 && memcmp(ptr, "NORTH", 5) == 0)
            break;
        offset--;
    }
    return offset;
}

/* Seek to a game data section in TryLoadingOld() / TryLoading() (see
   SeekIfNeeded()), giving up on the game if the position is out of range. */
#define SEEK_OR_FAIL(start)                                \
    do {                                                   \
        if (SeekIfNeeded((start), &offset, &ptr) == 0)     \
            return UNKNOWN_GAME;                           \
    } while (0)

/* Load a game using the "old style" Brian Howarth binary format
   (Mysterious Adventures era). Reads the header, actions, room connections,
   item locations, dictionary, room descriptions, messages, item descriptions,
   vector images, and system messages sequentially from known offsets.
   Returns the game ID on success. */
static GameIDType TryLoadingOld(const GameInfo *info, int dict_start)
{
    uint8_t *ptr;
    size_t offset;

    if (!LoadGameHeader(info, dict_start, &ptr, &offset))
        return UNKNOWN_GAME;

// MARK: actions

    SEEK_OR_FAIL(info->start_of_actions);

    ReadActions(&ptr, GameHeader.NumActions, 0);

// MARK: room connections

    SEEK_OR_FAIL(info->start_of_room_connections);

    if (!ReadRoomExits(&ptr, GameHeader.NumRooms, entire_file + file_length))
        return UNKNOWN_GAME;

// MARK: item locations

    SEEK_OR_FAIL(info->start_of_item_locations);

    if (!ReadItemLocations(&ptr, GameHeader.NumItems, entire_file + file_length))
        return UNKNOWN_GAME;

// MARK: dictionary

    SEEK_OR_FAIL(info->start_of_dictionary);

    ptr = ReadDictionary(info, ptr);

// MARK: rooms

    if (info->start_of_room_descriptions != 0) {
        SEEK_OR_FAIL(info->start_of_room_descriptions);

        if (info->start_of_room_descriptions == FOLLOWS)
            ptr++;

        if (!ReadBinaryRoomDescs(&ptr, GameHeader.NumRooms, info->number_of_pictures > 0, info->picture_format_version == HOWARTH_FORMAT))
            return UNKNOWN_GAME;
    }

// MARK: messages

    SEEK_OR_FAIL(info->start_of_messages);

    ReadBinaryMessages(&ptr);

// MARK: items

    SEEK_OR_FAIL(info->start_of_item_descriptions);

    ReadBinaryItemDescs(&ptr, GameHeader.NumItems);

// MARK: line images

    LoadVectorData(info, ptr);

// MARK: System messages

    SEEK_OR_FAIL(info->start_of_system_messages);

    /* The lone '"' message is followed by two bytes of interpreter workspace.
       These are zero in a pristine image, but can be anything in a snapshot
       taken during play, so step over them rather than letting them be
       mistaken for the end of the table. */
    const int quote_index = 20;
    int count = ReadTerminatedStrings(&ptr, system_messages, quote_index + 1, "\x83\xc9");
    if (count == quote_index + 1) {
        if (strcmp(system_messages[quote_index], "\"") == 0)
            ptr += 2;
        ReadTerminatedStrings(&ptr, system_messages + count, NUM_OLD_STYLE_SYSMESS - count, "\x83\xc9");
    }

    SEEK_OR_FAIL(info->start_of_directions);

    ReadTerminatedStrings(&ptr, sys, NUM_EXITS, "");

    return info->gameID;
}

/* Load an Adventure International UK / Brian Howarth game database given its
   GameInfo descriptor and dictionary offset.
   Dispatches to the SAGA binary loader for UK Hulk, to TryLoadingOld for
   old-style formats, or handles the later binary format directly.

   Validates the header against the expected GameInfo values. Supports both
   compressed (packed condition/command counts) and uncompressed action
   formats, and compressed vs plain NUL-terminated text strings.

   On success, all global game arrays are populated and the game ID is returned. */
GameIDType TryLoading(uint8_t *data, size_t datasize, const GameInfo *info, int dict_start)
{
    /* TryLoading's downstream helpers (LoadGameHeader, SeekIfNeeded,
       SeekToPos, ReadDictionary, etc.) currently read the game buffer
       via the global entire_file / file_length pair. Adopt the caller's
       buffer into those globals here. If the caller passed a buffer
       different from the current entire_file (e.g. a PRG extracted from
       a D64 disk image), the old buffer is freed — we're committed to
       this game, so the rest of the disk image is no longer needed. */
    if (data != entire_file) {
        free(entire_file);
        entire_file = data;
    }
    file_length = datasize;

    /* The UK versions of Hulk uses the Mak Jukic binary database format */
    if (info->gameID == HULK || info->gameID == HULK_C64) {
        GameIDType result = LoadBinaryDatabase(entire_file, file_length, *info, dict_start);
        if (result != UNKNOWN_GAME) {
            HulkReadCloseups();
            AddExtraCloseups(info->gameID);
        }
        return result;
    }

    if (info->type == OLD_STYLE)
        return TryLoadingOld(info, dict_start);

    uint8_t *ptr;
    size_t offset;

    if (!LoadGameHeader(info, dict_start, &ptr, &offset))
        return UNKNOWN_GAME;

    int compressed = (info->dictionary == FOUR_LETTER_COMPRESSED);

// MARK: room images

    if (info->start_of_room_image_list != 0) {
        SEEK_OR_FAIL(info->start_of_room_image_list);

        for (int ct = 0; ct <= GameHeader.NumRooms; ct++) {
            Rooms[ct].Image = *ptr++;
            debug_print("Room %d image: %d\n", ct, Rooms[ct].Image);
        }
    }
// MARK: Item flags

    if (info->start_of_item_flags != 0) {
        SEEK_OR_FAIL(info->start_of_item_flags);

        for (int ct = 0; ct <= GameHeader.NumItems; ct++)
            Items[ct].Flag = *(ptr++);
    }

// MARK: item images

    if (info->start_of_item_image_list != 0) {
        SEEK_OR_FAIL(info->start_of_item_image_list);

        for (int ct = 0; ct <= GameHeader.NumItems; ct++)
            Items[ct].Image = *(ptr++);
        debug_print("Offset after reading item images: %lx\n",
                ptr - entire_file - file_baseline_offset);
    }

// MARK: actions

    SEEK_OR_FAIL(info->start_of_actions);

    /* An uncompressed table ends where the dictionary begins. The header of
       Escape from Pulsar 7 C64 counts five lines more than there is room
       for (195, with 191 lines in the table), so the count is cut down to
       what fits. */
    if (info->actions_style == UNCOMPRESSED && info->start_of_actions != FOLLOWS
        && info->start_of_dictionary != FOLLOWS
        && info->start_of_dictionary > info->start_of_actions) {
        int lines = (info->start_of_dictionary - info->start_of_actions) / 16;
        if (lines > 0 && GameHeader.NumActions > lines - 1)
            GameHeader.NumActions = lines - 1;
    }

    ReadActions(&ptr, GameHeader.NumActions, info->actions_style == COMPRESSED);
    debug_print("Offset after reading actions: %lx\n",
            ptr - entire_file - file_baseline_offset);

// MARK: dictionary

    SEEK_OR_FAIL(info->start_of_dictionary);

    ptr = ReadDictionary(info, ptr);

// MARK: rooms

    if (info->start_of_room_descriptions != 0) {
        SEEK_OR_FAIL(info->start_of_room_descriptions);

        if (!ReadRoomDescs(&ptr, info, compressed))
            return UNKNOWN_GAME;
    }

// MARK: room connections

    SEEK_OR_FAIL(info->start_of_room_connections);

    if (!ReadRoomExits(&ptr, GameHeader.NumRooms, entire_file + file_length))
        return UNKNOWN_GAME;

// MARK: messages

    SEEK_OR_FAIL(info->start_of_messages);

    if (!ReadMessages(&ptr, compressed))
        return UNKNOWN_GAME;

// MARK: items

    SEEK_OR_FAIL(info->start_of_item_descriptions);

    ReadItemDescs(&ptr, compressed);

// MARK: item locations

    SEEK_OR_FAIL(info->start_of_item_locations);

    if (!ReadItemLocations(&ptr, GameHeader.NumItems, entire_file + file_length))
        return UNKNOWN_GAME;

// MARK: vector images

    LoadVectorData(info, ptr);

// MARK: System messages

    if (SeekIfNeeded(info->start_of_system_messages, &offset, &ptr) == 0)
        return info->gameID;

    int is_c64_english = (info->subtype & (C64 | ENGLISH)) == (C64 | ENGLISH);

    if (is_c64_english)
        offset = FindC64SystemMessages(offset);

    ptr = SeekToPos(offset);
    if (ptr == NULL)
        return UNKNOWN_GAME;
    ReadTerminatedStrings(&ptr, system_messages, NUM_HOWARTH_SYSMESS, NULL);

    debug_print("Offset after reading system messages: %lx\n",
            ptr - entire_file);

    if (is_c64_english)
        return info->gameID;

    SEEK_OR_FAIL(info->start_of_directions);

    while (ptr < entire_file + file_length && !isalpha(*ptr))
        ptr++;
    if (ptr >= entire_file + file_length) {
        debug_print("Read out of bounds!\n");
        return info->gameID;
    }

    ReadTerminatedStrings(&ptr, sys, NUM_EXITS, "");

    return info->gameID;
}

/* Check if the loaded game is a Mysterious Adventures title.
   Checks the current Game pointer first, then falls back to matching header
   values against the game database (for .dat plaintext format). */
static int IsMysterious(void)
{
    if (Game && (Game->subtype & MYSTERIOUS) != 0)
        return 1;
    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].subtype & MYSTERIOUS) {
            if (games[i].number_of_items == GameHeader.NumItems &&
                games[i].number_of_actions == GameHeader.NumActions &&
                games[i].number_of_words == GameHeader.NumWords &&
                games[i].number_of_rooms == GameHeader.NumRooms &&
                games[i].max_carried == GameHeader.MaxCarry &&
                games[i].word_length == GameHeader.WordLength &&
                games[i].number_of_messages == GameHeader.NumMessages)
                return 1;
        }
    }

    return 0;
}

/* Decompress a Parsec-packed ZX Spectrum game file. Four "scene releases"
   by Parsec (Denmark) use this RLE scheme: Pirate Adventure, Voodoo Castle,
   Strange Odyssey, and Buckaroo Banzai. Operates backwards through memory,
   expanding either literal runs or repeated byte-pairs. */
static uint8_t *DecompressParsec(uint8_t *start, uint8_t *end, uint8_t *dataptr)
{
    if (dataptr - 3 < entire_file) {
        return NULL;
    }
    uint8_t *pos = start;

    while (pos != end && dataptr - 3 > entire_file) {
        if (pos <= entire_file) {
            return NULL;
        }
        uint8_t *nextptr = dataptr - 2;
        if ((*dataptr & 0x80) == 0) {
            int repeats = READ_LE_UINT16(dataptr - 1);
            for (int i = 0; i < repeats && nextptr > entire_file && pos > entire_file; i++) {
                *pos = *nextptr;
                pos--;
                nextptr--;
            }
        } else {
            uint8_t val1 = *(dataptr - 1);
            uint8_t val2 = *(dataptr - 2);
            nextptr = dataptr - 3;
            int repeats = *dataptr & 0x7f;
            for (int i = 0; i < repeats && pos > entire_file; i++) {
                *pos = val1;
                pos--;
                *pos = val2;
                pos--;
            }
        }
        dataptr = nextptr;
    }
    return dataptr;
}

/* ---------------------------------------------------------------------------
   Alkatraz tape decryption

   Some commercial Scott Adams tapes (notably the U.S. Gold "Scott Adams
   Scoops" compilation) are protected with the Appleby Associates "Alkatraz"
   loader, which decrypts the game with a rolling-key XOR as it streams from
   tape, so the raw .tzx contains no recognisable dictionary. We reverse the
   final decryption pass here (the same scheme TaylorMade's extracttape.c
   uses) to reconstruct the loaded Spectrum memory image, which the normal ZX
   dictionary scan below can then identify.

   The per-release parameters are recovered by tracing the loader in an
   emulator: break on the store that writes a known decrypted byte, then read
   the key seed (loacon), the destination/count and the source pointer.

   The DeAlkatraz decryption primitive itself lives in the shared decompressz80
   library (and is also used by the TaylorMade interpreter).
   --------------------------------------------------------------------------- */

/* One game on an Alkatraz compilation tape: its title (shown in the selection
   menu) and the per-game key parameters. source_offset/target/size are shared
   by every game on a tape, so they live in AlkatrazTape. */
typedef struct {
    const char *title;
    int tzx_block;     /* number of the data block the game is encrypted in */
    uint8_t loacon;    /* initial rolling key */
    uint8_t add1;      /* key-evolution constants */
    uint8_t add2;
} AlkatrazGame;

/* One Alkatraz-protected release, identified by exact file length + checksum. */
typedef struct {
    int32_t file_length;   /* exact .tzx length identifying the release */
    uint16_t file_chk;     /* sum-of-bytes checksum of the whole file */
    size_t source_offset;  /* offset of the encrypted data within each block */
    uint16_t target;       /* Spectrum address the decrypt writes to */
    uint16_t size;         /* number of bytes to decrypt */
    int num_games;         /* >1 ⇒ prompt the player to choose */
    const AlkatrazGame *games;
} AlkatrazTape;

/* Scott Adams Scoops (1987)(U.S. Gold).tzx — four games behind one Alkatraz
   loader (shared source offset 0x11, target 0x5B00, size 0x4500). Each game's
   seed + key-evolution constants were recovered by tracing the loader and
   verified against the reference snapshots: the decrypted dictionary lands at
   the address each game's GameInfo expects (Pirate 0x869A, Voodoo 0x8752,
   Strange Odyssey 0x899A, Buckaroo 0x8C54). */
static const AlkatrazGame scoops_games[] = {
    { "Pirate Adventure",  3, 0xad, 0xfd, 0xff },
    { "Voodoo Castle",     7, 0x1e, 0xfe, 0xbe },
    { "Strange Odyssey",  11, 0x5a, 0x9c, 0xdc },
    { "Buckaroo Banzai",  15, 0xdb, 0xb3, 0x21 },
};

static const AlkatrazTape alkatraz_tapes[] = {
    { 0x16644, 0x3982, 0x11, 0x5b00, 0x4500, 4, scoops_games },
    { 0, 0, 0, 0, 0, 0, NULL }
};

/* If entire_file is a known Alkatraz-protected tape, let the player pick a
   game (compilations carry several), decrypt it into a fresh Spectrum memory
   image (offset 0 == address 0x4000, as DecompressZ80 produces) and adopt it
   as entire_file. Returns 1 on success, leaving the ZX dictionary scan to
   identify the chosen game. */
static int DeAlkatrazTape(void)
{
    if (file_length < 16 || memcmp(entire_file, "ZXTape!\x1a", 8) != 0)
        return 0;

    uint16_t sum = 0;
    for (size_t i = 0; i < file_length; i++)
        sum += entire_file[i];

    const AlkatrazTape *tape = NULL;
    for (int i = 0; alkatraz_tapes[i].file_length != 0; i++) {
        if ((int32_t)file_length == alkatraz_tapes[i].file_length
            && sum == alkatraz_tapes[i].file_chk) {
            tape = &alkatraz_tapes[i];
            break;
        }
    }
    if (tape == NULL)
        return 0;

    int choice = 0;
    if (tape->num_games > 1) {
        const char *titles[16];
        int n = tape->num_games < 16 ? tape->num_games : 16;
        for (int i = 0; i < n; i++)
            titles[i] = tape->games[i].title;
        choice = SelectGameFromMenu(
            "This tape contains several games. Which would you like to play?",
            titles, n);
    }
    const AlkatrazGame *game = &tape->games[choice];

    size_t blocklen = 0;
    uint8_t *block = GetTZXBlock(game->tzx_block, entire_file, file_length, &blocklen);
    if (block == NULL)
        return 0;

    uint8_t loacon = game->loacon;
    const AlkatrazKey schedule = { game->add1, game->add2, 0, 0 };
    uint8_t *mem = ZXNewMemory();
    int ok = mem != NULL && DeAlkatraz(block, blocklen, tape->source_offset,
        mem, tape->target, tape->size, &loacon, &schedule);
    free(block);
    if (!ok) {
        free(mem);
        return 0;
    }

    uint8_t *image = MemAlloc(ZX_MEMORY_SIZE - ZX_RAM_BASE);
    memcpy(image, mem + ZX_RAM_BASE, ZX_MEMORY_SIZE - ZX_RAM_BASE);
    free(mem);

    free(entire_file);
    entire_file = image;
    file_length = 0xc000;
    return 1;
}

/* Try loading entire_file as each game that uses dictionary type dict_type.
   On success, copies the matching GameInfo into *Game and returns the result
   of TryLoading(). */
static GameIDType TryGamesWithDictionary(DictionaryType dict_type, size_t dict_offset)
{
    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].dictionary == dict_type) {
            GameIDType result = TryLoading(entire_file, file_length, &games[i], dict_offset);
            if (result != UNKNOWN_GAME) {
                *Game = games[i];
                return result;
            }
        }
    }
    return UNKNOWN_GAME;
}

/* Detect and load a ZX Spectrum game file. Handles .z80 snapshot
   decompression, dictionary signature scanning, and Parsec RLE
   decompression (a two-pass scheme using Z80 register values
   stored at fixed offsets in the snapshot). */
static GameIDType DetectZXSpectrum(void)
{
    GameIDType detectedGame = UNKNOWN_GAME;
    int was_z80_snapshot = 0;

    /* Capture a loading screen to show as a title image. For .tap/.tzx
       tapes the SCREEN$ is a data block we extract from the raw image,
       so grab it before DecompressZ80() (a no-op on tapes) runs. */
    int is_tzx = (file_length > 8 && memcmp(entire_file, "ZXTape!\x1a", 8) == 0);
    int looks_like_tap = (!is_tzx && file_length > 21 &&
        entire_file[0] == 0x13 && entire_file[1] == 0x00 && entire_file[2] == 0x00);
    if (is_tzx || looks_like_tap)
        ZXLoadingScreen = FindTapeLoadingScreen(entire_file, file_length, is_tzx);

    /* Alkatraz-protected tapes carry no plaintext dictionary. If this is a
       known one, decrypt the embedded game image now (replacing entire_file
       with a 0x4000-based Spectrum image) and skip the .z80 path below. */
    int decrypted = (is_tzx && DeAlkatrazTape());

    /* Tapes and .sna snapshots are not .z80 snapshots: read as one they are
       only rejected, with a libspectrum error on stderr. A .sna has no
       signature, but it does have one of two sizes. */
    int is_sna = (file_length == 49179 || file_length == 131103 || file_length == 147487);
    int not_z80 = (decrypted || is_tzx || looks_like_tap || is_sna);

    uint8_t *uncompressed = not_z80 ? NULL : DecompressZ80(entire_file, &file_length);
    if (uncompressed != NULL) {
        was_z80_snapshot = 1;
        free(entire_file);
        entire_file = uncompressed;
        /* In a decompressed 48K snapshot, offset 0 maps to Spectrum
           address 0x4000 (the display file), so the first 6912 bytes are
           whatever screen was visible when the snapshot was saved. */
        if (ZXLoadingScreen == NULL && file_length >= ZX_SCREEN_SIZE) {
            ZXLoadingScreen = MemAlloc(ZX_SCREEN_SIZE);
            memcpy(ZXLoadingScreen, entire_file, ZX_SCREEN_SIZE);
        }
    }

    /* Snapshots saved at a text prompt (e.g. "Resume a saved game?") have
       no picture, just black text on white. Don't show those as a title. */
    if (ZXLoadingScreen != NULL && ZXScreenIsBlackOnWhite(ZXLoadingScreen)) {
        free(ZXLoadingScreen);
        ZXLoadingScreen = NULL;
    }

    size_t dict_offset;

    DictionaryType dict_type = GetId(entire_file, file_length, &dict_offset);

    if (dict_type == NOT_A_GAME)
        return UNKNOWN_GAME;

    detectedGame = TryGamesWithDictionary(dict_type, dict_offset);

    /* If no dictionary was found and this was a .z80 snapshot, try
       Parsec RLE decompression. The decompression parameters (start/end
       addresses) are stored in saved Z80 registers at fixed offsets.
       Addresses are adjusted by -ZX_RAM_BASE (0x4000) to convert from
       the Spectrum's memory map to the file buffer. */
    if (!detectedGame && was_z80_snapshot) {
        uint8_t *file_data = entire_file;
        /* HL: source pointer for the first Parsec pass (the compressed
           data inside the snapshot). */
        uint16_t HL = READ_LE_UINT16(file_data + 0x1b42) - ZX_RAM_BASE;
        uint8_t *first_pass_result = 0;
        if (HL < file_length)
            first_pass_result = DecompressParsec(&file_data[0x0fff], &file_data[0x07ff], &file_data[HL]);
        if (first_pass_result) {
            /* BC/DE: output start/end for the second Parsec pass, which
               unpacks the first-pass result into its final position. */
            uint16_t BC = READ_LE_UINT16(file_data + 0x1b48) - ZX_RAM_BASE;
            uint16_t DE = READ_LE_UINT16(file_data + 0x1b4b) - ZX_RAM_BASE;
            DecompressParsec(&file_data[BC], &file_data[DE], first_pass_result);
            dict_type = GetId(entire_file, file_length, &dict_offset);
            if (dict_type == NOT_A_GAME)
                Fatal("Unsupported game!");
            if (TryGamesWithDictionary(dict_type, dict_offset) != UNKNOWN_GAME)
                detectedGame = Game->gameID;
        }
    }

    if (!detectedGame)
        Fatal("Unsupported game!");

    return detectedGame;
}

/* Set up the system messages, extra data, image patches and graphics of a
   detected Adventure International UK / Brian Howarth or C64 game. */
static void SetUpGameSpecifics(GameIDType detectedGame)
{
    /* Copy ZX Spectrum style system messages as a base, then apply
       game-specific overrides below. The direction words before
       PLAY_AGAIN are copied for the Hulk only, below. */
    for (int i = PLAY_AGAIN; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = sysdict_zx[i];
    }

    /* Game-specific post-load setup: load extra data (Robin of Sherwood
       forest images, Seas of Blood combat tables), fix image assignments, and map
       system_messages[] indices to the sys[] slots the engine uses. */
    switch (detectedGame) {
    case ROBIN_OF_SHERWOOD:
        LoadExtraSherwoodData(0);
        break;
    case ROBIN_OF_SHERWOOD_C64:
        LoadExtraSherwoodData(1);
        break;
    case SEAS_OF_BLOOD:
        LoadExtraSeasOfBloodData(0);
        break;
    case SEAS_OF_BLOOD_C64:
        LoadExtraSeasOfBloodData(1);
        break;
    case CLAYMORGUE: {
        int src = MapSysRange(PLAY_AGAIN, ON_A_SCALE_THAT_RATES, 2);
        MapSysRange(OK, RESUME_A_SAVED_GAME, src);
        break;
    }
    case SECRET_MISSION:
    case SECRET_MISSION_C64:
        Items[3].Image = 23;
        Items[3].Flag = 128 | 2;
        Rooms[2].Image = 0;
        if (detectedGame == SECRET_MISSION_C64) {
            SecretMission64Sysmess();
            break;
        }
        // fallthrough
    case ADVENTURELAND: {
        int src = MapSysRange(PLAY_AGAIN, ON_A_SCALE_THAT_RATES, 2);
        src = MapSysRange(OK, YOU_HAVENT_GOT_IT, src);
        MapSysRange(YOU_DONT_SEE_IT, RESUME_A_SAVED_GAME, src);
        break;
    }
    case ADVENTURELAND_C64:
        Adventureland64Sysmess();
        break;
    case CLAYMORGUE_C64:
        Claymorgue64Sysmess();
        break;
    case GREMLINS_GERMAN_C64:
        LoadExtraGermanGremlinsC64Data();
        break;
    case SPIDERMAN_C64:
        Spiderman64Sysmess();
        break;
    case SPIDERMAN_ATARI8:
        SpidermanAtari8Sysmess();
        break;
    case SUPERGRAN_C64:
        Supergran64Sysmess();
        break;
    case SAVAGE_ISLAND_C64:
        Items[20].Image = 13;
        // fallthrough
    case SAVAGE_ISLAND2_C64:
        Supergran64Sysmess();
        sys[IM_DEAD] = "I'm DEAD!! ";
        if (detectedGame == SAVAGE_ISLAND2_C64)
            Rooms[30].Image = 20;
        break;
    case SAVAGE_ISLAND:
        Items[20].Image = 13;
        // fallthrough
    case SAVAGE_ISLAND2:
        MyLoc = 30; /* Both parts of Savage Island begin in room 30 */
        // fallthrough
    case GREMLINS_GERMAN:
    case GREMLINS:
    case SUPERGRAN:
        MapGremlinsSysMessages(2);
        break;
    case GREMLINS_SPANISH:
        LoadExtraSpanishGremlinsData();
        break;
    case GREMLINS_SPANISH_C64:
        LoadExtraSpanishGremlinsC64Data();
        break;
    case HULK_C64:
    case HULK:
        for (int i = NORTH; i < PLAY_AGAIN; i++) {
            sys[i] = sysdict_zx[i];
        }
        break;
    default:
        if (!(Game->subtype & C64)) {
            if (Game->subtype & MYSTERIOUS) {
                int src = MapSysRange(PLAY_AGAIN, YOU_HAVENT_GOT_IT, 2);
                src = MapSysRange(YOU_DONT_SEE_IT, WHAT_NOW, src);
                MapSysRange(LIGHT_HAS_RUN_OUT, RESUME_A_SAVED_GAME, src);
                sys[ITEM_DELIMITER] = " - ";
                sys[MESSAGE_DELIMITER] = "\n";
                sys[YOU_SEE] = "\nThings I can see:\n";
                break;
            } else {
                MapSysRange(PLAY_AGAIN, RESUME_A_SAVED_GAME, 2);
            }
        }
        break;
    }

    switch (detectedGame) {
    case GREMLINS_GERMAN:
        LoadExtraGermanGremlinsData();
        break;
    case PERSEUS_ITALIAN:
        PerseusItalianSysmess();
        break;
    default:
        break;
    }

    if ((Game->subtype & (MYSTERIOUS | C64)) == (MYSTERIOUS | C64))
        Mysterious64Sysmess();

    /* If it is a C64 or a Mysterious Adventures game, we have set up the graphics already */
    if (!(Game->subtype & (C64 | MYSTERIOUS)) && Game->number_of_pictures > 0) {
        SagaGraphicsSetup(0);
    }
}

/* Top-level game detection: try each supported format in order.

   1. ScottFree plaintext (LoadDatabase)
   2. TI-99/4A cartridge
   3. Commodore 64 (with optional decrunch)
   4. Atari 8-bit
   5. Apple II
   6. ZX Spectrum (.z80 snapshots, Parsec-compressed)

   Once the game is identified and loaded, configures system messages,
   graphics, and game-specific overrides (system message mappings,
   item/room image patches, parser language tables). */
GameIDType DetectGame(const char *file_name)
{
    FILE *f = fopen(file_name, "rb");
    if (f == NULL)
        Fatal("Cannot open game");

    file_length = GetFileLength(f);

    if (file_length > MAX_GAMEFILE_SIZE) {
        debug_print("File too large to be a valid game file (%zu bytes, max is %d)\n",
            file_length, MAX_GAMEFILE_SIZE);
        fclose(f);
        return UNKNOWN_GAME;
    }

    SetParserWordLists(EnglishDirections, EnglishSkipList, EnglishDelimiterList,
        NULL, EnglishExtraNouns);

    Game = MemCalloc(sizeof(GameInfo));

    /* Try the ScottFree plaintext format first */
    GameIDType detectedGame = LoadDatabase(f);

    size_t result = 0;
    if (detectedGame == UNKNOWN_GAME) {
        entire_file = MemAlloc(file_length);
        fseek(f, 0, SEEK_SET);
        result = fread(entire_file, 1, file_length, f);
    }
    fclose(f);

    if (detectedGame == UNKNOWN_GAME) { /* Not a ScottFree game, check if TI99/4A */
        if (result == 0)
            Fatal("File empty or read error!");

        detectedGame = DetectTI994A();

        if (detectedGame == UNKNOWN_GAME) { /* Not a TI99/4A game, check if C64 */
            detectedGame = DetectC64(&entire_file, &file_length, file_name);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not a C64 game, check if Atari */
            detectedGame = DetectAtari8(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not an Atari game, check if Apple 2 */
            detectedGame = DetectApple2(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not an Apple 2 game, check if ZX Spectrum */
            detectedGame = DetectZXSpectrum();
        }

        if (detectedGame == UNKNOWN_GAME) {
            free(entire_file);
            free(Game);
            Game = NULL;
            return UNKNOWN_GAME;
        }
    }

    if (detectedGame == HULK_US || detectedGame == RETURN_TO_PIRATES_ISLE) {
        CurrentGame = detectedGame;
        Game->type = US_VARIANT;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A)
        CurrentGame = detectedGame;

    if (IsMysterious()) {
        Options = Options | SCOTTLIGHT | PREHISTORIC_LAMP;
        ImageWidth = 255;
        ImageHeight = 95;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A || Game->type == US_VARIANT)
        return detectedGame;

    SetUpGameSpecifics(detectedGame);

    return detectedGame;
}
