//
//  load_TI99_4a.c
//  scott
//
//  Created by Administrator on 2022-02-12.
//

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "load_TI99_4a.h"

#include "detectgame.h"
#include "gameinfo.h"
#include "scott.h"

#define PACKED __attribute__((__packed__))

struct DATAHEADER {
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
};

int max_messages;
int max_item_descr;

uint8_t *ti99_implicit_actions = NULL;
uint8_t *ti99_explicit_actions = NULL;
size_t ti99_implicit_extent = 0;
size_t ti99_explicit_extent = 0;

uint8_t **VerbActionOffsets;

uint16_t FixAddress(uint16_t ina)
{
    return (ina - 0x380 + file_baseline_offset);
}

uint16_t FixWord(uint16_t word)
{
    if (WeAreBigEndian)
        return word;
    else
        return (((word & 0xFF) << 8) | ((word >> 8) & 0xFF));
}

uint16_t GetWord(uint8_t *mem)
{
    uint16_t x;
    if (WeAreBigEndian) {
        x = (*(mem + 0) << 8);
        x += (*(mem + 1) & 0xFF);
    } else {
        x = *(uint16_t *)mem;
    }
    return FixWord(x);
}

void GetMaxTI99Messages(struct DATAHEADER dh)
{
    uint8_t *msg;
    uint16_t msg1;

    msg = (uint8_t *)entire_file + FixAddress(FixWord(dh.p_message));
    msg1 = FixAddress(GetWord((uint8_t *)msg));
    max_messages = (msg1 - FixAddress(FixWord(dh.p_message))) / 2;
}

void GetMaxTI99Items(struct DATAHEADER dh)
{
    uint8_t *msg;
    uint16_t msg1;

    msg = (uint8_t *)entire_file + FixAddress(FixWord(dh.p_obj_descr));
    msg1 = FixAddress(GetWord((uint8_t *)msg));
    max_item_descr = (msg1 - FixAddress(FixWord(dh.p_obj_descr))) / 2;
}

void PrintTI99HeaderInfo(struct DATAHEADER header)
{
    fprintf(stderr, "Number of items =\t%d\n", header.num_objects);
    fprintf(stderr, "Number of verbs =\t%d\n", header.num_verbs);
    fprintf(stderr, "Number of nouns =\t%d\n", header.num_nouns);
    fprintf(stderr, "Deadroom =\t%d\n", header.red_room);
    fprintf(stderr, "Max carried items =\t%d\n", header.max_items_carried);
    fprintf(stderr, "Player start location: %d\n", header.begin_locn);
    fprintf(stderr, "Word length =\t%d\n", header.cmd_length);
    fprintf(stderr, "Treasure room: %d\n", header.treasure_locn);
    fprintf(stderr, "Lightsource time left: %d\n", FixWord(header.light_turns));
    fprintf(stderr, "Unknown: %d\n", header.strange);
}

int TryLoadingTI994A(struct DATAHEADER dh, int loud);

GameIDType DetectTI994A(uint8_t **sf, size_t *extent)
{
    int offset = FindCode("\x30\x30\x30\x30\x00\x30\x30\x00\x28\x28", 0);
    if (offset == -1)
        return UNKNOWN_GAME;

    file_baseline_offset = offset - 0x589;

    struct DATAHEADER dh;
    memcpy(&dh, entire_file + 0x8a0 + file_baseline_offset,
        sizeof(struct DATAHEADER));

    GetMaxTI99Messages(dh);
    GetMaxTI99Items(dh);

    /* test some pointers for valid game... */

    assert(file_length >= FixAddress(FixWord(dh.p_obj_table)));
    assert(file_length >= FixAddress(FixWord(dh.p_orig_items)));
    assert(file_length >= FixAddress(FixWord(dh.p_obj_link)));
    assert(file_length >= FixAddress(FixWord(dh.p_obj_descr)));
    assert(file_length >= FixAddress(FixWord(dh.p_message)));
    assert(file_length >= FixAddress(FixWord(dh.p_room_exit)));
    assert(file_length >= FixAddress(FixWord(dh.p_room_descr)));
    assert(file_length >= FixAddress(FixWord(dh.p_noun_table)));
    assert(file_length >= FixAddress(FixWord(dh.p_verb_table)));
    assert(file_length >= FixAddress(FixWord(dh.p_explicit)));
    assert(file_length >= FixAddress(FixWord(dh.p_implicit)));

    return TryLoadingTI994A(dh, Options & DEBUGGING);
}

uint8_t *GetTI994AWord(uint8_t *string, uint8_t **result, size_t *length)
{
    uint8_t *msg;

    msg = string;
    *length = msg[0];
    if (*length == 0 || *length > 100) {
        *length = 0;
        *result = NULL;
        return NULL;
    }
    msg++;
    *result = MemAlloc(*length);
    memcpy(*result, msg, *length);

    msg += *length;

    return (msg);
}

char *GetTI994AString(uint16_t table, int table_offset)
{
    uint8_t *msgx, *msgy, *nextword;
    char *result;
    uint16_t msg1, msg2;
    uint8_t buffer[1024];
    size_t length, total_length = 0;

    uint8_t *game = entire_file;

    msgx = game + FixAddress(FixWord(table));

    msgx += table_offset * 2;
    msg1 = FixAddress(GetWord((uint8_t *)msgx));
    msg2 = FixAddress(GetWord((uint8_t *)msgx + 2));

    msgy = game + msg2;
    msgx = game + msg1;

    while (msgx < msgy) {
        msgx = GetTI994AWord(msgx, &nextword, &length);
        if (length == 0 || nextword == NULL) {
            return NULL;
        }
        if (length > 100) {
            free(nextword);
            return NULL;
        }
        memcpy(buffer + total_length, nextword, length);
        free(nextword);
        total_length += length;
        if (total_length > 1000)
            break;
        if (msgx < msgy)
            buffer[total_length++] = ' ';
    }
    if (total_length == 0)
        return NULL;
    total_length++;
    result = MemAlloc(total_length);
    memcpy(result, buffer, total_length);
    result[total_length - 1] = '\0';
    return result;
}

void LoadTI994ADict(int vorn, uint16_t table, int num_words,
    const char **dict)
{
    uint16_t *wtable;
    int i;
    int word_len;
    char *w1;
    char *w2;

    /* table is either verb or noun table */
    wtable = (uint16_t *)(entire_file + FixAddress(FixWord(table)));

    for (i = 0; i <= num_words; i++) {
        do {
            w1 = (char *)entire_file + FixAddress(GetWord((uint8_t *)wtable + (i * 2)));
            w2 = (char *)entire_file + FixAddress(GetWord((uint8_t *)wtable + ((1 + i) * 2)));
        } while (w1 == w2);

        word_len = w2 - w1;

        dict[i] = MemAlloc(word_len + 1);
        strncpy((char *)dict[i], w1, word_len);
    }
}

void ReadTI99ImplicitActions(struct DATAHEADER dh)
{
    uint8_t *ptr, *implicit_start;
    int loop_flag;

    implicit_start = entire_file + FixAddress(FixWord(dh.p_implicit));
    ptr = implicit_start;
    loop_flag = 0;

    /* fall out, if no auto acts in the game. */
    if (*ptr == 0x0)
        loop_flag = 1;

    while (loop_flag == 0) {
        if (ptr[1] == 0)
            loop_flag = 1;

        /* skip code chunk */
        ptr += 1 + ptr[1];
    }

    ti99_implicit_extent = MIN(file_length, ptr - entire_file);
    if (ti99_implicit_extent) {
        ti99_implicit_actions = MemAlloc(ti99_implicit_extent);
        memcpy(ti99_implicit_actions, implicit_start, ti99_implicit_extent);
    }
}

void ReadTI99ExplicitActions(struct DATAHEADER dh)
{
    uint8_t *ptr, *start, *end, *blockstart;
    uint16_t address;
    int loop_flag;
    int i;

    start = entire_file + file_length;
    end = entire_file;

    size_t explicit_offset = FixAddress(FixWord(dh.p_explicit));
    blockstart = entire_file + explicit_offset;

    VerbActionOffsets = MemAlloc((dh.num_verbs + 1) * sizeof(uint8_t *));

    for (i = 0; i <= dh.num_verbs; i++) {
        ptr = blockstart;
        address = GetWord(ptr + ((i)*2));

        VerbActionOffsets[i] = NULL;

        if (address != 0) {
            ptr = entire_file + FixAddress(address);
            if (ptr < start)
                start = ptr;
            VerbActionOffsets[i] = ptr;
            loop_flag = 0;

            while (loop_flag != 1) {
                if (ptr[1] == 0)
                    loop_flag = 1;

                /* go to next block. */
                ptr += 1 + ptr[1];
                if (ptr > end)
                    end = ptr;
            }
        }
    }

    ti99_explicit_extent = end - start;
    ti99_explicit_actions = MemAlloc(ti99_explicit_extent);
    memcpy(ti99_explicit_actions, start, ti99_explicit_extent);
    for (i = 0; i <= dh.num_verbs; i++) {
        if (VerbActionOffsets[i] != NULL) {
            VerbActionOffsets[i] = ti99_explicit_actions + (VerbActionOffsets[i] - start);
        }
    }
}

uint8_t *LoadTitleScreen(void)
{
    char buf[3074];
    int offset = 0;
    uint8_t *p;
    int lines;

    /* title screen offset starts at 0x80 */
    p = entire_file + 0x80 + file_baseline_offset;
    if (p - entire_file > file_length)
        return NULL;
    int parens = 0;
    for (lines = 0; lines < 24; lines++) {
        for (int i = 0; i < 40; i++) {
            char c = *(p++);
            if (p - entire_file >= file_length)
                return NULL;
            if (!((c <= 127) && (c >= 0))) /* isascii() */
                c = '?';
            switch (c) {
                case '\\':
                    c = ' ';
                    break;
                case '(':
                    parens = 1;
                    break;
                case ')':
                    if (!parens)
                        c = '@';
                    parens = 0;
                    break;
                case '|':
                    if (*p != ' ')
                        c = 12;
                    break;
                default:
                    break;
            }
            buf[offset++] = c;
            if (offset >= 3072)
                return NULL;
        }
        buf[offset++] = '\n';
    }

    buf[offset] = '\0';
    uint8_t *result = MemAlloc(offset + 1);
    memcpy(result, buf, offset + 1);
    return result;
}

int TryLoadingTI994A(struct DATAHEADER dh, int loud)
{
    int ni, nw, nr, mc, pr, tr, wl, lt, mn, trm;
    int ct;

    Room *rp;
    Item *ip;
    /* Load the header */

    ni = dh.num_objects;
    nw = MAX(dh.num_verbs, dh.num_nouns);
    nr = dh.red_room;
    mc = dh.max_items_carried;
    pr = dh.begin_locn;
    tr = 0;
    trm = dh.treasure_locn;
    wl = dh.cmd_length;
    lt = FixWord(dh.light_turns);
    mn = max_messages;

    uint8_t *ptr = entire_file;

    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = 0;
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;

    int offset;

#if defined(__clang__)
#pragma mark rooms
#endif

    if (SeekIfNeeded(FixAddress(FixWord(dh.p_room_descr)), &offset, &ptr) == 0)
        return 0;

    ct = 0;
    rp = Rooms;

    do {
        rp->Text = GetTI994AString(dh.p_room_descr, ct);
        if (rp->Text == NULL)
            rp->Text = ".\0";
        if (loud)
            fprintf(stderr, "Room %d: %s\n", ct, rp->Text);
        rp->Image = 255;
        ct++;
        rp++;
    } while (ct < nr + 1);

#if defined(__clang__)
#pragma mark messages
#endif
    ct = 0;
    while (ct < mn + 1) {
        Messages[ct] = GetTI994AString(dh.p_message, ct);
        if (Messages[ct] == NULL)
            Messages[ct] = ".\0";
        if (loud)
            fprintf(stderr, "Message %d: %s\n", ct, Messages[ct]);
        ct++;
    }

#if defined(__clang__)
#pragma mark items
#endif
    ct = 0;
    ip = Items;
    do {
        ip->Text = GetTI994AString(dh.p_obj_descr, ct);
        if (ip->Text == NULL)
            ip->Text = ".\0";
        if (ip->Text && ip->Text[0] == '*')
            tr++;
        if (loud)
            fprintf(stderr, "Item %d: %s\n", ct, ip->Text);
        ct++;
        ip++;
    } while (ct < ni + 1);

    GameHeader.Treasures = tr;
    if (loud)
        fprintf(stderr, "Number of treasures %d\n", GameHeader.Treasures);

#if defined(__clang__)
#pragma mark room connections
#endif
    if (SeekIfNeeded(FixAddress(FixWord(dh.p_room_exit)), &offset, &ptr) == 0)
        return 0;

    ct = 0;
    rp = Rooms;

    while (ct < nr + 1) {
        for (int j = 0; j < 6; j++) {
            rp->Exits[j] = *(ptr++ - file_baseline_offset);
        }
        ct++;
        rp++;
    }

#if defined(__clang__)
#pragma mark item locations
#endif
    if (SeekIfNeeded(FixAddress(FixWord(dh.p_orig_items)), &offset, &ptr) == 0)
        return 0;

    ct = 0;
    ip = Items;
    while (ct < ni + 1) {
        ip->Location = *(ptr++ - file_baseline_offset);
        ip->InitialLoc = ip->Location;
        ip++;
        ct++;
    }

#if defined(__clang__)
#pragma mark dictionary
#endif
    LoadTI994ADict(0, dh.p_verb_table, dh.num_verbs + 1, Verbs);
    LoadTI994ADict(1, dh.p_noun_table, dh.num_nouns + 1, Nouns);

    for (int i = 1; i <= dh.num_nouns - dh.num_verbs; i++)
        Verbs[dh.num_verbs + i] = ".\0";

    for (int i = 1; i <= dh.num_verbs - dh.num_nouns; i++)
        Nouns[dh.num_nouns + i] = ".\0";

    if (loud) {
        for (int i = 0; i <= GameHeader.NumWords; i++)
            fprintf(stderr, "Verb %d: %s\n", i, Verbs[i]);
        for (int i = 0; i <= GameHeader.NumWords; i++)
            fprintf(stderr, "Noun %d: %s\n", i, Nouns[i]);
    }

#if defined(__clang__)
#pragma mark autoget
#endif
    ct = 0;
    ip = Items;

    int objectlinks[ni + 1];

    if (SeekIfNeeded(FixAddress(FixWord(dh.p_obj_link)), &offset, &ptr) == 0)
        return 0;

    do {
        objectlinks[ct] = *(ptr++ - file_baseline_offset);
        if (objectlinks[ct] && objectlinks[ct] <= nw) {
            ip->AutoGet = (char *)Nouns[objectlinks[ct]];
        } else {
            ip->AutoGet = NULL;
        }
        ct++;
        ip++;
    } while (ct < ni + 1);

    ReadTI99ImplicitActions(dh);
    ReadTI99ExplicitActions(dh);

    AutoInventory = 1;
    sys[INVENTORY] = "I'm carrying: ";

    title_screen = (char *)LoadTitleScreen();
    free(entire_file);

    for (int i = 0; i < MAX_SYSMESS && sysdict_TI994A[i] != NULL; i++) {
        sys[i] = sysdict_TI994A[i];
    }

    Options |= TI994A_STYLE;
    return TI994A;
}
