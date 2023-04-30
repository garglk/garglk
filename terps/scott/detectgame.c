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

#include "decompresstext.h"
#include "detectgame.h"
#include "line_drawing.h"
#include "sagadraw.h"
#include "scottgameinfo.h"

#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robinofsherwood.h"
#include "seasofblood.h"

#include "apple2detect.h"
#include "atari8detect.h"
#include "c64decrunch.h"
#include "decompressz80.h"
#include "load_ti99_4a.h"

#include "parser.h"

extern const char *sysdict_zx[MAX_SYSMESS];
extern int header[];

struct dictionaryKey {
    DictionaryType dict;
    const char *signature;
    int len;
};

struct dictionaryKey dictKeys[] = {
    { FOUR_LETTER_UNCOMPRESSED, "AUTO\0GO\0", 8 },
    { THREE_LETTER_UNCOMPRESSED, "AUT\0GO\0", 7 },
    { FIVE_LETTER_UNCOMPRESSED, "GO\0\0\0\0*CROSS*RUN\0", 17 }, // Claymorgue
    { FOUR_LETTER_COMPRESSED, "aUTOgO\0", 7 },
    { GERMAN_C64, "gEHENSTEIGE", 11 }, // Gremlins German C64
    { GERMAN, "\xc7"
              "EHENSTEIGE",
        10 },
    { SPANISH, "\x81\0\0\0\xc9R\0\0ANDAENTR", 16 },
    { SPANISH_C64, "\x81\0\0\0iR\0\0ANDAENTR", 16 },
    { ITALIAN, "AUTO\0VAI\0\0*ENTR", 15 },
    { NOT_A_GAME, NULL, 0 }
};

int FindCode(const char *x, int len)
{
    unsigned const char *p = entire_file;
    while (p < entire_file + file_length - len) {
        if (memcmp(p, x, len) == 0) {
            return p - entire_file;
        }
        p++;
    }
    return -1;
}

DictionaryType GetId(size_t *offset)
{
    for (int i = 0; dictKeys[i].dict != NOT_A_GAME; i++) {
        *offset = FindCode(dictKeys[i].signature, dictKeys[i].len);
        if (*offset != -1) {
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

uint8_t *ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 15; i++) {
        value = *ptr + 256 * *(ptr + 1);
        header[i] = value;
        ptr += 2;
    }
    return ptr - 1;
}

int SanityCheckHeader(void)
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

uint8_t *ReadDictionary(struct GameInfo info, uint8_t **pointer, int loud)
{
    uint8_t *ptr = *pointer;
    if (info.word_length + 2 > 1024)
        Fatal("Bad word length");
    char dictword[1024];
    char c = 0;
    int wordnum = 0;
    int charindex = 0;

    int nw = info.number_of_words;

    int nv = info.number_of_verbs;
    int nn = info.number_of_nouns;

    for (int i = 0; i < nw + 2; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    do {
        for (int i = 0; i < info.word_length; i++) {
            c = *(ptr++);

            if (info.dictionary == FOUR_LETTER_COMPRESSED || info.dictionary == GERMAN_C64 || info.dictionary == SPANISH_C64) {
                if (charindex == 0) {
                    if (c >= 'a') {
                        c = toupper(c);
                    } else if (c != '.' && c != 0) {
                        dictword[charindex++] = '*';
                    }
                }
                dictword[charindex++] = c;
            } else if (info.subtype == LOCALIZED) {
                if (charindex == 0) {
                    if (c & 0x80) {
                        c = c & 0x7f;
                    } else if (c != '.') {
                        dictword[charindex++] = '*';
                    }
                }
                dictword[charindex++] = c;
            } else {
                if (c == 0) {
                    if (charindex == 0) {
                        c = *(ptr++);
                    }
                }
                if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                    i--;
                    charindex--;
                }
                if (c == '*') {
                    if (charindex != 0)
                        charindex = 0;
                    i = -1;
                }
                dictword[charindex++] = c;
            }
        }
        dictword[charindex] = 0;
        if (wordnum < nv) {
            Verbs[wordnum] = MemAlloc(charindex + 1);
            memcpy((char *)Verbs[wordnum], dictword, charindex + 1);
            if (loud)
                debug_print("Verb %d: \"%s\"\n", wordnum, Verbs[wordnum]);
        } else {
            Nouns[wordnum - nv] = MemAlloc(charindex + 1);
            memcpy((char *)Nouns[wordnum - nv], dictword, charindex + 1);
            if (loud)
                debug_print("Noun %d: \"%s\"\n", wordnum - nv, Nouns[wordnum - nv]);
        }
        wordnum++;

        if (c > 127)
            return ptr;

        charindex = 0;
    } while (wordnum <= nv + nn);

    return ptr;
}

uint8_t *SeekToPos(uint8_t *buf, int offset)
{
    if (offset > file_length)
        return NULL;
    return buf + offset;
}

int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr)
{
    if (expected_start != FOLLOWS) {
        *offset = expected_start + file_baseline_offset;
        //        uint8_t *ptrbefore = *ptr;
        *ptr = SeekToPos(entire_file, *offset);
        //        if (*ptr == ptrbefore)
        //            debug_print("Seek unnecessary, could have been set to
        //            FOLLOWS.\n");
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

int ParseHeader(int *h, HeaderType type, int *ni, int *na, int *nw, int *nr,
    int *mc, int *pr, int *tr, int *wl, int *lt, int *mn,
    int *trm)
{
    switch (type) {
    case NO_HEADER:
        return 0;
    case EARLY:
        *ni = h[1];
        *na = h[2];
        *nw = h[3];
        *nr = h[4];
        *mc = h[5];
        *pr = h[6];
        *tr = h[7];
        *wl = h[8];
        *lt = h[9];
        *mn = h[10];
        *trm = h[11];
        break;
    case LATE:
        *ni = h[1];
        *na = h[2];
        *nw = h[3];
        *nr = h[4];
        *mc = h[5];
        *wl = h[6];
        *mn = h[7];
        *pr = 1;
        *tr = 0;
        *lt = -1;
        *trm = 0;
        break;
    case US_HEADER:
        *ni = h[3];
        *na = h[2];
        *nw = h[1];
        *nr = h[5];
        *mc = h[6];
        *pr = h[7];
        *tr = h[8];
        *wl = h[0];
        *lt = h[9];
        *mn = h[4];
        *trm = h[10] >> 8;
        break;
    case ROBIN_C64_HEADER:
        *ni = h[1];
        *na = h[2];
        *nw = h[6];
        *nr = h[4];
        *mc = h[5];
        *pr = 1;
        *tr = 0;
        *wl = h[7];
        *lt = -1;
        *mn = h[3];
        *trm = 0;
        break;
    case GREMLINS_C64_HEADER:
        *ni = h[1];
        *na = h[2];
        *nw = h[5];
        *nr = h[3];
        *mc = h[6];
        *pr = h[8];
        *tr = 0;
        *wl = h[7];
        *lt = -1;
        *mn = 98;
        *trm = 0;
        break;
    case SUPERGRAN_C64_HEADER:
        *ni = h[3];
        *na = h[1];
        *nw = h[2];
        *nr = h[4];
        *mc = h[8];
        *pr = 1;
        *tr = 0;
        *wl = h[6];
        *lt = -1;
        *mn = h[5];
        *trm = 0;
        break;
    case SEAS_OF_BLOOD_C64_HEADER:
        *ni = h[0];
        *na = h[1];
        *nw = 134;
        *nr = h[3];
        *mc = h[4];
        *pr = 1;
        *tr = 0;
        *wl = h[6];
        *lt = -1;
        *mn = h[2];
        *trm = 0;
        break;
    case MYSTERIOUS_C64_HEADER:
        *ni = h[1];
        *na = h[2];
        *nw = h[3];
        *nr = h[4];
        *mc = h[5] & 0xff;
        *pr = h[5] >> 8;
        *tr = h[6];
        *wl = h[7];
        *lt = h[8];
        *mn = h[9];
        *trm = 0;
        break;
    case ARROW_OF_DEATH_PT_2_C64_HEADER:
        *ni = h[3];
        *na = h[1];
        *nw = h[2];
        *nr = h[4];
        *mc = h[5] & 0xff;
        *pr = h[5] >> 8;
        *tr = h[6];
        *wl = h[7];
        *lt = h[8];
        *mn = h[9];
        *trm = 0;
        break;
    case INDIANS_C64_HEADER:
        *ni = h[1];
        *na = h[2];
        *nw = h[3];
        *nr = h[4];
        *mc = h[5] & 0xff;
        *pr = h[5] >> 8;
        *tr = h[6] & 0xff;
        *wl = h[6] >> 8;
        *lt = h[7] >> 8;
        *mn = h[8] >> 8;
        *trm = 0;
        break;
    default:
        debug_print("Unhandled header type!\n");
        return 0;
    }
    return 1;
}

void PrintHeaderInfo(int *h, int ni, int na, int nw, int nr, int mc, int pr,
    int tr, int wl, int lt, int mn, int trm)
{
#if (DEBUG_PRINT)
    uint16_t value;
    for (int i = 0; i < 13; i++) {
        value = h[i];
        debug_print("b $%X %d: ", 0x494d + 0x3FE5 + i * 2, i);
        debug_print("\t%d (%04x)\n", value, value);
    }

    debug_print("Number of items =\t%d\n", ni);
    debug_print("Number of actions =\t%d\n", na);
    debug_print("Number of words =\t%d\n", nw);
    debug_print("Number of rooms =\t%d\n", nr);
    debug_print("Max carried items =\t%d\n", mc);
    debug_print("Word length =\t%d\n", wl);
    debug_print("Number of messages =\t%d\n", mn);
    debug_print("Player start location: %d\n", pr);
    debug_print("Treasure room: %d\n", trm);
    debug_print("Lightsource time left: %d\n", lt);
    debug_print("Number of treasures: %d\n", tr);
#endif
}

typedef struct {
    uint8_t *imagedata;
    uint8_t background_colour;
    size_t size;
} LineImage;

void LoadVectorData(struct GameInfo info, uint8_t *ptr)
{
    size_t offset;

    if (info.start_of_image_data == FOLLOWS)
        ptr++;
    else if (SeekIfNeeded(info.start_of_image_data, &offset, &ptr) == 0)
        return;

    LineImages = MemAlloc(info.number_of_rooms * sizeof(struct line_image));
    int ct = 0;
    struct line_image *lp = LineImages;
    uint8_t byte = *(ptr++);
    do {
        Rooms[ct].Image = 0;
        if (byte == 0xff) {
            lp->bgcolour = *(ptr++);
            lp->data = ptr;
        } else {
            debug_print("Error! Image data does not start with 0xff!\n");
        }
        do {
            byte = *(ptr++);
            if (ptr - entire_file >= file_length) {
                debug_print("Error! Image data for image %d cut off!\n", ct);
                if (GameHeader.NumRooms - ct > 1)
                    Display(Bottom, "[This copy has %d broken or missing pictures. These have been patched out.]\n\n", GameHeader.NumRooms - ct);
                if (lp->data >= ptr)
                    lp->size = 0;
                else
                    lp->size = ptr - lp->data - 1;
                for (int i = ct + 2; i < GameHeader.NumRooms; i++)
                    Rooms[i].Image = 255;
                return;
            }
        } while (byte != 0xff);

        lp->size = ptr - lp->data;
        lp++;
        ct++;
    } while (ct < info.number_of_rooms);
}

struct LineImage *lineImages = NULL;

GameIDType TryLoadingOld(struct GameInfo info, int dict_start)
{
    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    uint8_t *ptr = entire_file;
    file_baseline_offset = dict_start - info.start_of_dictionary;
    debug_print("file_baseline_offset: %d\n", file_baseline_offset);
    size_t offset = info.start_of_header + file_baseline_offset;

    ptr = SeekToPos(entire_file, offset);
    if (ptr == 0)
        return UNKNOWN_GAME;

    ReadHeader(ptr);

    if (!ParseHeader(header, info.header_style, &ni, &na, &nw, &nr, &mc, &pr,
            &tr, &wl, &lt, &mn, &trm))
        return UNKNOWN_GAME;

    if (ni != info.number_of_items || na != info.number_of_actions || nw != info.number_of_words || nr != info.number_of_rooms || mc != info.max_carried) {
        //        debug_print("Non-matching header\n");
        return UNKNOWN_GAME;
    }

    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;

#pragma mark actions

    if (SeekIfNeeded(info.start_of_actions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;

    ap = Actions;

    uint16_t value, cond, comm;

    while (ct < na + 1) {
        value = *(ptr++);
        value += *(ptr++) * 0x100; /* verb/noun */
        ap->Vocab = value;

        cond = 5;
        comm = 2;

        for (int j = 0; j < 5; j++) {
            if (j < cond) {
                value = *(ptr++);
                value += *(ptr++) * 0x100;
            } else
                value = 0;
            ap->Condition[j] = value;
        }
        for (int j = 0; j < 2; j++) {
            if (j < comm) {
                value = *(ptr++);
                value += *(ptr++) * 0x100;
            } else
                value = 0;
            ap->Subcommand[j] = value;
        }
        ap++;
        ct++;
    }

#pragma mark room connections

    if (SeekIfNeeded(info.start_of_room_connections, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    rp = Rooms;

    while (ct < nr + 1) {
        for (int j = 0; j < 6; j++) {
            rp->Exits[j] = *(ptr++);
        }
        ct++;
        rp++;
    }

#pragma mark item locations

    if (SeekIfNeeded(info.start_of_item_locations, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    ip = Items;
    while (ct < ni + 1) {
        ip->Location = *(ptr++);
        ip->InitialLoc = ip->Location;
        ip++;
        ct++;
    }

#pragma mark dictionary

    if (SeekIfNeeded(info.start_of_dictionary, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ptr = ReadDictionary(info, &ptr, 0);

#pragma mark rooms

    if (SeekIfNeeded(info.start_of_room_descriptions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    if (info.start_of_room_descriptions == FOLLOWS)
        ptr++;

    ct = 0;
    rp = Rooms;

    char text[256];
    char c = 0;
    int charindex = 0;

    do {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0) {
            rp->Text = MemAlloc(charindex + 1);
            strncpy(rp->Text, text, charindex + 1);
            if (info.number_of_pictures > 0)
                rp->Image = ct - 1;
            else
                rp->Image = 255;
            ct++;
            rp++;
            charindex = 0;
        } else {
            charindex++;
            if (charindex > 255)
                break;
        }
        if (c > 127)
            return UNKNOWN_GAME;
    } while (ct < nr + 1);

#pragma mark messages

    if (SeekIfNeeded(info.start_of_messages, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    charindex = 0;

    while (ct < mn + 1) {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0) {
            Messages[ct] = MemAlloc(charindex + 1);
            strncpy((char *)Messages[ct], text, charindex + 1);
            ct++;
            charindex = 0;
        } else {
            charindex++;
            if (charindex > 255)
                break;
        }
    }

#pragma mark items

    if (SeekIfNeeded(info.start_of_item_descriptions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    ip = Items;
    charindex = 0;

    do {
        uint8_t chr = *(ptr++);
        text[charindex] = chr;
        if (chr == 0 || chr > 126) {
            ip->Text = MemAlloc(charindex + 1);
            strncpy(ip->Text, text, charindex + 1);
            ip->Text[charindex] = 0;
            ip->AutoGet = strchr(ip->Text, '/');
            /* Some games use // to mean no auto get/drop word! */
            if (ip->AutoGet && strcmp(ip->AutoGet, "//") && strcmp(ip->AutoGet, "/*")) {
                *ip->AutoGet++ = 0;
                char *t = strchr(ip->AutoGet, '/');
                if (t != NULL)
                    *t = 0;
            }
            ct++;
            ip++;
            charindex = 0;
        } else {
            charindex++;
            if (charindex > 255)
                break;
        }
    } while (ct < ni + 1);

#pragma mark line images

    if (info.number_of_pictures > 0) {
        LoadVectorData(info, ptr);
    }

#pragma mark System messages

    ct = 0;
    if (SeekIfNeeded(info.start_of_system_messages, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    charindex = 0;

    do {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                if (c == 0x0d)
                    charindex++;
                char *newmess = MemAlloc(charindex + 1);
                text[charindex] = '\0';
                strncpy(newmess, text, charindex + 1);
                system_messages[ct] = newmess;
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && c != '\x83' && c != '\xc9' && c > 127)
            break;
    } while (ct < 40);

    charindex = 0;

    if (SeekIfNeeded(info.start_of_directions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    do {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                char *newmess = MemAlloc(charindex + 2);
                if (c == 0x0d)
                    charindex++;
                text[charindex] = '\0';
                strncpy(newmess, text, charindex + 1);
                sys[ct] = newmess;
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && c > 127)
            break;
    } while (ct < 6);

    return info.gameID;
}

GameIDType TryLoading(struct GameInfo info, int dict_start, int loud)
{
    /* The UK versions of Hulk uses the Mak Jukic binary database format */
    if (info.gameID == HULK || info.gameID == HULK_C64)
        return LoadBinaryDatabase(entire_file, file_length, info, dict_start);

    if (info.type == OLD_STYLE)
        return TryLoadingOld(info, dict_start);

    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    uint8_t *ptr = entire_file;

    if (loud) {
        debug_print("dict_start:%x\n", dict_start);
        debug_print(" info.start_of_dictionary:%x\n", info.start_of_dictionary);
    }
    file_baseline_offset = dict_start - info.start_of_dictionary;

    if (loud)
        debug_print("file_baseline_offset:%x (%d)\n", file_baseline_offset,
            file_baseline_offset);

    size_t offset = info.start_of_header + file_baseline_offset;

    ptr = SeekToPos(entire_file, offset);
    if (ptr == 0)
        return UNKNOWN_GAME;

    ReadHeader(ptr);

    if (!ParseHeader(header, info.header_style, &ni, &na, &nw, &nr, &mc, &pr,
            &tr, &wl, &lt, &mn, &trm))
        return UNKNOWN_GAME;

    if (loud)
        PrintHeaderInfo(header, ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm);

    GameHeader.NumItems = ni;
    GameHeader.NumActions = na;
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    GameHeader.NumRooms = nr;
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    GameHeader.TreasureRoom = trm;

    if (SanityCheckHeader() == 0) {
        return UNKNOWN_GAME;
    }

    if (loud) {
        debug_print("Found a valid header at position 0x%zx\n", offset);
        debug_print("Expected: 0x%x\n",
            info.start_of_header + file_baseline_offset);
    }

    if (ni != info.number_of_items || na != info.number_of_actions || nw != info.number_of_words || nr != info.number_of_rooms || mc != info.max_carried) {
        if (loud)
            debug_print("Non-matching header\n");
        return UNKNOWN_GAME;
    }

    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    Messages = MemAlloc(sizeof(char *) * (mn + 1));

    int compressed = (info.dictionary == FOUR_LETTER_COMPRESSED);

#pragma mark room images

    if (info.start_of_room_image_list != 0) {
        if (SeekIfNeeded(info.start_of_room_image_list, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        rp = Rooms;

        for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
            rp->Image = *(ptr++);
            rp++;
        }
    }
#pragma mark Item flags

    if (info.start_of_item_flags != 0) {

        if (SeekIfNeeded(info.start_of_item_flags, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        ip = Items;

        for (ct = 0; ct <= GameHeader.NumItems; ct++) {
            ip->Flag = *(ptr++);
            ip++;
        }
    }

#pragma mark item images

    if (info.start_of_item_image_list != 0) {

        if (SeekIfNeeded(info.start_of_item_image_list, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        ip = Items;

        for (ct = 0; ct <= GameHeader.NumItems; ct++) {
            ip->Image = *(ptr++);
            ip++;
        }
        if (loud)
            debug_print("Offset after reading item images: %lx\n",
                ptr - entire_file - file_baseline_offset);
    }

#pragma mark actions

    if (SeekIfNeeded(info.start_of_actions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;

    ap = Actions;

    uint16_t value, cond, comm;

    while (ct < na + 1) {
        value = *(ptr++);
        value += *(ptr++) * 0x100; /* verb/noun */
        ap->Vocab = value;

        if (info.actions_style == COMPRESSED) {
            value = *(ptr++); /* count of actions/conditions */
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
        for (int j = 0; j < 5; j++) {
            if (j < cond) {
                value = *(ptr++);
                value += *(ptr++) * 0x100;
            } else
                value = 0;
            ap->Condition[j] = value;
        }
        for (int j = 0; j < 2; j++) {
            if (j < comm) {
                value = *(ptr++);
                value += *(ptr++) * 0x100;
            } else
                value = 0;
            ap->Subcommand[j] = value;
        }

        ap++;
        ct++;
    }
    if (loud)
        debug_print("Offset after reading actions: %lx\n",
            ptr - entire_file - file_baseline_offset);

#pragma mark dictionary

    if (SeekIfNeeded(info.start_of_dictionary, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ptr = ReadDictionary(info, &ptr, loud);

    if (loud)
        debug_print("Offset after reading dictionary: %lx\n",
            ptr - entire_file - file_baseline_offset);

#pragma mark rooms

    if (info.start_of_room_descriptions != 0) {
        if (SeekIfNeeded(info.start_of_room_descriptions, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        ct = 0;
        rp = Rooms;

        char text[256];
        char c = 0;
        int charindex = 0;

        if (!compressed) {
            do {
                c = *(ptr++);
                text[charindex] = c;
                if (c == 0) {
                    rp->Text = MemAlloc(charindex + 1);
                    strncpy(rp->Text, text, charindex + 1);
                    if (loud)
                        debug_print("Room %d: %s\n", ct, rp->Text);
                    ct++;
                    rp++;
                    charindex = 0;
                } else {
                    charindex++;
                    if (charindex > 255)
                        break;
                }
                if (c > 127)
                    return UNKNOWN_GAME;
            } while (ct < nr + 1);
        } else {
            do {
                rp->Text = DecompressText(ptr, ct);
                if (rp->Text == NULL)
                    return UNKNOWN_GAME;
                *(rp->Text) = tolower(*(rp->Text));
                ct++;
                rp++;
            } while (ct < nr);
        }
    }

#pragma mark room connections

    if (SeekIfNeeded(info.start_of_room_connections, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    rp = Rooms;

    while (ct < nr + 1) {
        for (int j = 0; j < 6; j++) {
            rp->Exits[j] = *(ptr++);
        }

        ct++;
        rp++;
    }

#pragma mark messages

    if (SeekIfNeeded(info.start_of_messages, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    char text[256];
    char c = 0;
    int charindex = 0;

    if (compressed) {
        while (ct < mn + 1) {
            Messages[ct] = DecompressText(ptr, ct);
            if (loud)
                debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
            if (Messages[ct] == NULL)
                return UNKNOWN_GAME;
            ct++;
        }
    } else {
        while (ct < mn + 1) {
            c = *(ptr++);
            text[charindex] = c;
            if (c == 0) {
                Messages[ct] = MemAlloc(charindex + 1);
                strncpy((char *)Messages[ct], text, charindex + 1);
                if (loud)
                    debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
                ct++;
                charindex = 0;
            } else {
                charindex++;
                if (charindex > 255)
                    break;
            }
        }
    }

#pragma mark items

    if (SeekIfNeeded(info.start_of_item_descriptions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    ip = Items;
    charindex = 0;

    if (compressed) {
        do {
            ip->Text = DecompressText(ptr, ct);
            ip->AutoGet = NULL;
            if (ip->Text != NULL && ip->Text[0] != '.') {
                if (loud)
                    debug_print("Item %d: %s\n", ct, ip->Text);
                ip->AutoGet = strchr(ip->Text, '.');
                if (ip->AutoGet) {
                    *ip->AutoGet++ = 0;
                    ip->AutoGet++;
                    char *t = strchr(ip->AutoGet, '.');
                    if (t != NULL)
                        *t = 0;
                    for (int i = 1; i < GameHeader.WordLength; i++)
                        *(ip->AutoGet + i) = toupper(*(ip->AutoGet + i));
                }
            }
            ct++;
            ip++;
        } while (ct < ni + 1);
    } else {
        do {
            c = *(ptr++);
            text[charindex] = c;
            if (c == 0) {
                ip->Text = MemAlloc(charindex + 1);
                strncpy(ip->Text, text, charindex + 1);
                if (loud)
                    debug_print("Item %d: %s\n", ct, ip->Text);
                ip->AutoGet = strchr(ip->Text, '/');
                /* Some games use // to mean no auto get/drop word! */
                if (ip->AutoGet && strcmp(ip->AutoGet, "//") && strcmp(ip->AutoGet, "/*")) {
                    *ip->AutoGet++ = 0;
                    char *t = strchr(ip->AutoGet, '/');
                    if (t != NULL)
                        *t = 0;
                }
                ct++;
                ip++;
                charindex = 0;
            } else {
                charindex++;
                if (charindex > 255)
                    break;
            }
        } while (ct < ni + 1);
    }

#pragma mark item locations

    if (SeekIfNeeded(info.start_of_item_locations, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ct = 0;
    ip = Items;
    while (ct < ni + 1) {
        ip->Location = *(ptr++);
        ip->InitialLoc = ip->Location;
        if (Items[ct].Text && ip->Location < nr && Rooms[ip->Location].Text)
            debug_print("Location of item %d, \"%s\":%d\n", ct, Items[ct].Text, ip->Location);
        ip++;
        ct++;
    }

#pragma mark vector images

    if (info.number_of_pictures > 0 && info.picture_format_version == 99) {
        LoadVectorData(info, ptr);
    }

#pragma mark System messages

    if (SeekIfNeeded(info.start_of_system_messages, &offset, &ptr) == 0)
        return info.gameID;
jumpSysMess:
    ptr = SeekToPos(entire_file, offset);
    ct = 0;
    charindex = 0;

    do {
        c = *(ptr++);
        if (ptr - entire_file > file_length || ptr < entire_file) {
            debug_print("Read out of bounds!\n");
            return UNKNOWN_GAME;
        }
        if (charindex > 255)
            charindex = 0;
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                if (c == 0x0d)
                    charindex++;
                if (ct == 0 && (info.subtype & (C64 | ENGLISH)) == (C64 | ENGLISH) && memcmp(text, "NORTH", 5) != 0) {
                    offset--;
                    goto jumpSysMess;
                }
                char *newmess = MemAlloc(charindex + 1);
                text[charindex] = '\0';
                strncpy(newmess, text, charindex + 1);
                system_messages[ct] = newmess;
                if (loud)
                    debug_print("system_messages %d: \"%s\"\n", ct,
                        system_messages[ct]);
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }
    } while (ct < 45);

    if (loud)
        debug_print("Offset after reading system messages: %lx\n",
            ptr - entire_file);

    if ((info.subtype & (C64 | ENGLISH)) == (C64 | ENGLISH)) {
        return info.gameID;
    }

    if (SeekIfNeeded(info.start_of_directions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    charindex = 0;

    ct = 0;
    do {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                char *newmess = MemAlloc(charindex + 2);
                if (c == 0x0d)
                    charindex++;
                text[charindex] = '\0';
                strncpy(newmess, text, charindex + 1);
                sys[ct] = newmess;
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && c > 127)
            break;
    } while (ct < 6);

    return info.gameID;
}

int IsMysterious(void)
{
    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].subtype & MYSTERIOUS) {
            if (games[i].number_of_items == GameHeader.NumItems && games[i].number_of_actions == GameHeader.NumActions && games[i].number_of_words == GameHeader.NumWords && games[i].number_of_rooms == GameHeader.NumRooms && games[i].max_carried == GameHeader.MaxCarry && games[i].word_length == GameHeader.WordLength && games[i].number_of_messages == GameHeader.NumMessages)
                return 1;
        }
    }

    return 0;
}

// There are four ZX Spectrum "scene releases" (by some guy in Denmark called Parsec)
// which use RLE compression: Pirate Adventure, Voodoo Castle, Strange Odyssey and Buckaroo Banzai.
// This function decompresses them.

uint8_t *DecompressParsec(uint8_t *start, uint8_t *end, uint8_t *dataptr)
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
            int repeats = *(dataptr - 1) + *dataptr * 0x100;
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

GameIDType DetectZXSpectrum(void)
{
    GameIDType detectedGame = UNKNOWN_GAME;
    int wasz80 = 0;
    uint8_t *uncompressed = DecompressZ80(entire_file, file_length);
    if (uncompressed != NULL) {
        wasz80 = 1;
        free(entire_file);
        entire_file = uncompressed;
        file_length = 0xc000;
    }

    size_t offset;

    DictionaryType dict_type = GetId(&offset);

    if (dict_type == NOT_A_GAME)
        return UNKNOWN_GAME;

    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].dictionary == dict_type) {
            detectedGame = TryLoading(games[i], offset, 0);
            if (detectedGame != UNKNOWN_GAME) {
                free(Game);
                Game = &games[i];
                break;
            }
        }
    }

    if (!detectedGame && wasz80) {
        uint8_t *mem = entire_file;
        uint16_t HL = mem[0x1b42] + mem[0x1b43] * 0x100 - 0x4000;
        uint8_t *result = 0;
        if (HL < file_length)
            result = DecompressParsec(&mem[0x0fff], &mem[0x07ff], &mem[HL]);
        if (result) {
            uint16_t BC = mem[0x1b48] + mem[0x1b49] * 0x100 - 0x4000;
            uint16_t DE = mem[0x1b4b] + mem[0x1b4c] * 0x100 - 0x4000;
            DecompressParsec(&mem[BC], &mem[DE], result);
            dict_type = GetId(&offset);
            if (dict_type == NOT_A_GAME)
                Fatal("Unsupported game!");
            for (int i = 0; games[i].Title != NULL; i++) {
                if (games[i].dictionary == dict_type) {
                    if (TryLoading(games[i], offset, 1)) {
                        free(Game);
                        Game = &games[i];
                        detectedGame = Game->gameID;
                        break;
                    }
                }
            }
        }
    }

    if (!detectedGame)
        Fatal("Unsupported game!");

    return detectedGame;
}

GameIDType DetectGame(const char *file_name)
{
    FILE *f = fopen(file_name, "r");
    if (f == NULL)
        Fatal("Cannot open game");

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = EnglishDirections[i];
    for (int i = 0; i < NUMBER_OF_SKIPPABLE_WORDS; i++)
        SkipList[i] = EnglishSkipList[i];
    for (int i = 0; i < NUMBER_OF_DELIMITERS; i++)
        DelimiterList[i] = EnglishDelimiterList[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = EnglishExtraNouns[i];

    file_length = GetFileLength(f);

    if (file_length > MAX_GAMEFILE_SIZE) {
        debug_print("File too large to be a vaild game file (%zu bytes, max is %d)\n",
            file_length, MAX_GAMEFILE_SIZE);
        return UNKNOWN_GAME;
    }

    Game = (struct GameInfo *)MemAlloc(sizeof(struct GameInfo));
    memset(Game, 0, sizeof(struct GameInfo));

    // Check if the original ScottFree LoadDatabase() function can read the file.
    GameIDType detectedGame = LoadDatabase(f, Options & DEBUGGING);

    if (detectedGame == UNKNOWN_GAME) { /* Not a ScottFree game, check if TI99/4A */
        entire_file = MemAlloc(file_length);
        fseek(f, 0, SEEK_SET);
        size_t result = fread(entire_file, 1, file_length, f);
        fclose(f);
        if (result == 0)
            Fatal("File empty or read error!");

        detectedGame = DetectTI994A();

        if (detectedGame == UNKNOWN_GAME) /* Not a TI99/4A game, check if C64 */
            detectedGame = DetectC64(&entire_file, &file_length);

        if (detectedGame == UNKNOWN_GAME) { /* Not a C64 game, check if Atari */
            detectedGame = DetectAtari8(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not an Atari game, check if Apple 2 */
            detectedGame = DetectApple2(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not an Apple 2 game, check if ZX Spectrum */
            detectedGame = DetectZXSpectrum();
        }

        if (detectedGame == UNKNOWN_GAME)
            return UNKNOWN_GAME;
    }

    if (detectedGame == HULK_US) {
        CurrentGame = HULK_US;
        Game->type = US_VARIANT;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A)
        CurrentGame = detectedGame;

    if (IsMysterious()) {
        Options = Options | SCOTTLIGHT | PREHISTORIC_LAMP;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A || Game->type == US_VARIANT)
        return detectedGame;

    /* Copy ZX Spectrum style system messages as base */
    for (int i = 6; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = (char *)sysdict_zx[i];
    }

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
    case CLAYMORGUE:
        for (int i = OK; i <= RESUME_A_SAVED_GAME; i++)
            sys[i] = system_messages[6 - OK + i];
        for (int i = PLAY_AGAIN; i <= ON_A_SCALE_THAT_RATES; i++)
            sys[i] = system_messages[2 - PLAY_AGAIN + i];
        break;
    case SECRET_MISSION:
    case SECRET_MISSION_C64:
        Items[3].Image = 23;
        Items[3].Flag = 128 | 2;
        Rooms[2].Image = 0;
        if (detectedGame == SECRET_MISSION_C64) {
            SecretMission64Sysmess();
            break;
        }
    case ADVENTURELAND:
        for (int i = PLAY_AGAIN; i <= ON_A_SCALE_THAT_RATES; i++)
            sys[i] = system_messages[2 - PLAY_AGAIN + i];
        for (int i = OK; i <= YOU_HAVENT_GOT_IT; i++)
            sys[i] = system_messages[6 - OK + i];
        for (int i = YOU_DONT_SEE_IT; i <= RESUME_A_SAVED_GAME; i++)
            sys[i] = system_messages[13 - YOU_DONT_SEE_IT + i];
        break;
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
        for (int i = DROPPED; i <= OK; i++)
            sys[i] = system_messages[2 - DROPPED + i];
        for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
            sys[i] = system_messages[6 - I_DONT_UNDERSTAND + i];
        for (int i = YOU_ARE; i <= HIT_ENTER; i++)
            sys[i] = system_messages[17 - YOU_ARE + i];
        sys[PLAY_AGAIN] = system_messages[5];
        sys[YOURE_CARRYING_TOO_MUCH] = system_messages[24];
        sys[IM_DEAD] = system_messages[25];
        sys[YOU_CANT_GO_THAT_WAY] = system_messages[14];
        break;
    case GREMLINS_SPANISH:
        LoadExtraSpanishGremlinsData();
        break;
    case GREMLINS_SPANISH_C64:
        LoadExtraSpanishGremlinsC64Data();
        break;
    case HULK_C64:
    case HULK:
        for (int i = 0; i < 6; i++) {
            sys[i] = (char *)sysdict_zx[i];
        }
        break;
    default:
        if (!(Game->subtype & C64)) {
            if (Game->subtype & MYSTERIOUS) {
                for (int i = PLAY_AGAIN; i <= YOU_HAVENT_GOT_IT; i++)
                    sys[i] = system_messages[2 - PLAY_AGAIN + i];
                for (int i = YOU_DONT_SEE_IT; i <= WHAT_NOW; i++)
                    sys[i] = system_messages[15 - YOU_DONT_SEE_IT + i];
                for (int i = LIGHT_HAS_RUN_OUT; i <= RESUME_A_SAVED_GAME; i++)
                    sys[i] = system_messages[31 - LIGHT_HAS_RUN_OUT + i];
                sys[ITEM_DELIMITER] = " - ";
                sys[MESSAGE_DELIMITER] = "\n";
                sys[YOU_SEE] = "\nThings I can see:\n";
                break;
            } else {
                for (int i = PLAY_AGAIN; i <= RESUME_A_SAVED_GAME; i++)
                    sys[i] = system_messages[2 - PLAY_AGAIN + i];
            }
        }
        break;
    }

    switch (detectedGame) {
    case GREMLINS_GERMAN:
        LoadExtraGermanGremlinsData();
        break;
    case GREMLINS_GERMAN_C64:
        LoadExtraGermanGremlinsC64Data();
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
        SagaSetup(0);
    }

    return detectedGame;
}
