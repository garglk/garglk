//
//  loaddatabase.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "common_utils.h"
#include "gameinfo.h"
#include "graphics.h"
#include "read_le16.h"
#include "dumpactions.h"
#include "loaddatabase.h"
#include "opcodes.h"
#include "apple2detect.h"

Synonym *Substitutions;

/* Reads a complete line from the plaintext database, stripping surrounding
   quote marks. Used to read comment/annotation lines that follow the main
   game data. Returns NULL on EOF, or else an allocated string, empty for an
   empty quoted line. Non-ASCII characters are replaced with '?', and what
   does not fit in 1023 characters is dropped. */
static char *ReadWholeLine(FILE *f)
{
    char tmp[1024];
    int c;
    int ct = 0;

    /* Skip whitespace and find the opening quote mark */
    do {
        c = fgetc(f);
        if (c == EOF)
            return NULL;
        if (c == '"') {
            c = fgetc(f);
            if (c == '"')
                return MemCalloc(1);
            break;
        }
    } while (isspace((unsigned char)c));

    /* Read characters until end of line or EOF */
    while (c != '\n' && c != '\r' && c != EOF) {
        /* Pass only ASCII to Glk; the other reasonable option
         * would be to pass Latin-1, but it's probably safe to
         * assume that Scott Adams games are ASCII only.
         */
        if (ct < (int)sizeof tmp - 1)
            tmp[ct++] = (c >= 32 && c <= 126) ? c : '?';
        c = fgetc(f);
    }

    /* Find the closing quote and truncate there */
    int closequote = 0;
    for (int i = ct - 1; i > 0; i--) {
        if (tmp[i] == '"') {
            closequote = i;
            break;
        }
    }

    if (closequote)
        ct = closequote;
    else if (ct > 1)
        ct--;
    else
        return MemCalloc(1);

    tmp[ct] = 0;
    char *t = MemAlloc(ct + 1);
    memcpy(t, tmp, ct + 1);
    return t;
}

/* Reads a quoted string from the plaintext database file. Strings are
   delimited by double quotes; a pair of consecutive quotes ("") inside a
   string represents a literal quote character. Backticks are converted to
   double quotes. Non-ASCII characters are replaced with '?', and bare CR
   characters are stripped (assumed to be part of CRLF line endings).
   Returns the allocated string and sets *length to its size including the
   null terminator, or returns NULL if the file ends first. */
#define MAX_STRING_LENGTH 1000

static char *ReadQuoted(FILE *f, size_t *length)
{
    char tmp[MAX_STRING_LENGTH + 1];
    int c;
    int ct = 0;
    int cut_short = 0;

    /* Skip everything until the opening quote */
    do {
        c = fgetc(f);
        if (c == EOF)
            return NULL;
    } while (c != '"');

    /* Read string contents until the closing quote. What does not fit in
       MAX_STRING_LENGTH is dropped, but read, so that the next field starts
       in the right place. */
    for (;;) {
        c = fgetc(f);
        if (c == EOF)
            return NULL;
        /* A pair of consecutive quotes is an escaped literal quote;
           a single quote followed by something else ends the string */
        if (c == '"') {
            int nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"';

        /* Special case: assume CR is part of CRLF in a
         * DOS-formatted file, and ignore it.
         */
        if (c == 13)
            continue;
        /* Pass only ASCII to Glk, and a newline as a Glk newline; the
         * other reasonable option would be to pass Latin-1, but it's
         * probably safe to assume that Scott Adams games are ASCII only.
         */
        if (c != '\n' && (c < 32 || c > 126))
            c = '?';
        if (ct < MAX_STRING_LENGTH)
            tmp[ct++] = c;
        else
            cut_short = 1;
    }
    if (cut_short)
        debug_print("ReadString: string longer than %d characters cut short\n", MAX_STRING_LENGTH);

    tmp[ct] = 0;
    *length = ct + 1;
    char *t = MemAlloc(*length);
    memcpy(t, tmp, *length);
    return t;
}

/* ReadQuoted for the sections after the title, where the end of the file
   means a damaged database of a known game. */
static char *ReadString(FILE *f, size_t *length)
{
    char *t = ReadQuoted(f, length);
    if (t == NULL)
        Fatal("EOF in string");
    return t;
}

/* Makes room in a growing array for element number index (and the
   sentinel after it), doubling its capacity as needed. New elements are
   zeroed. */
static void *GrowArray(void *array, int *capacity, int index, size_t elemsize)
{
    if (array != NULL && index + 1 < *capacity)
        return array;
    int old = array == NULL ? 0 : *capacity;
    int newcapacity = old == 0 ? 64 : old * 2;
    while (index + 1 >= newcapacity)
        newcapacity *= 2;
    array = MemRealloc(array, (size_t)newcapacity * elemsize);
    memset((char *)array + (size_t)old * elemsize, 0, (size_t)(newcapacity - old) * elemsize);
    *capacity = newcapacity;
    return array;
}

/* A dictionary while it is read: words, a NULL Word sentinel after them */
typedef struct {
    DictWord *words;
    int count;
    int capacity;
} DictBuilder;

/* Splits one comma-separated dictionary string into individual words and
   appends them, tagged with group, to db. The string is modified in place. */
static void ParseDictString(char *str, size_t length, int group, DictBuilder *db)
{
    int lastcomma = 0;
    int commapos = 0;
    for (size_t j = 0; j < length && str[j] != '\0'; j++) {
        if (str[j] != ',')
            continue;

        while (str[j] == ',') {
            str[j] = '\0';
            commapos = j;
            j++;
        }

        int seglen = commapos - lastcomma;
        if (seglen > 0) {
            db->words = GrowArray(db->words, &db->capacity, db->count, sizeof(DictWord));
            DictWord *entry = &db->words[db->count];
            entry->Word = MemAlloc(seglen);
            memcpy(entry->Word, &str[lastcomma + 1], seglen);
            entry->Group = group;
            debug_print("Dictword %d: %s (%d)\n", db->count, entry->Word, group);
            db->count++;
        }
        lastcomma = commapos;
    }
}

/* Returns the dictionary, terminated by a NULL Word sentinel. */
static DictWord *FinishDictionary(DictBuilder *db)
{
    db->words = GrowArray(db->words, &db->capacity, db->count, sizeof(DictWord));
    DictWord *finaldict = db->words;
    finaldict[db->count].Word = NULL;

    for (int j = 0; finaldict[j].Word != NULL; j++)
        debug_print("Dictionary entry %d: \"%s\", group %d\n", j, finaldict[j].Word, finaldict[j].Group);

    return finaldict;
}

/* Reads dictionary words from the plaintext database. Each string read from
   the file contains comma-separated words that belong to the same synonym
   group (e.g. "GET,TAKE,GRAB,"). Words within a group are interchangeable.
   The group number increments with each string read from the file.
   Returns an array terminated by a NULL Word sentinel. */
static DictWord *ReadDictWordsPC(FILE *f, int numstrings)
{
    DictBuilder db = { NULL, 0, 0 };

    /* Each string is one group of synonyms, numbered by its place. An
       empty string is a group without words. (The empty strings of the
       binary format are padding and are not counted, see ReadDictWords.) */
    for (int group = 0; group < numstrings; group++) {
        size_t length;
        char *str = ReadString(f, &length);
        ParseDictString(str, length, group, &db);
        free(str);
    }

    return FinishDictionary(&db);
}

/* Accumulates synonym entries while a substitution section is parsed. */
typedef struct {
    Synonym *syn;
    int capacity;
    char *replace;
    int index;
    int firstsyn;
} SynonymBuilder;

/* Parses one comma-separated substitution string into sb. */
static void ParseSynonymString(SynonymBuilder *sb, char *str, size_t length)
{
    int lastcomma = 0;
    int commapos = 0;
    int foundrep = 0;
    int nextisrep = 0;

    for (size_t j = 0; j < length && str[j] != '\0'; j++) {
        if (str[j] != ',')
            continue;

        while (str[j] == ',') {
            str[j] = '\0';
            commapos = j;
            j++;
        }

        if (nextisrep) {
            foundrep = 1;
            nextisrep = 0;
        } else if (str[j] == '=') {
            nextisrep = 1;
        }

        if (foundrep) {
            int seglen = commapos - lastcomma - 1;
            if (seglen > 0) {
                free(sb->replace);
                sb->replace = MemAlloc(seglen);
                memcpy(sb->replace, &str[lastcomma + 2], seglen);
                debug_print("Found new replacement string \"%s\"\n", sb->replace);
            }
        } else {
            int seglen = commapos - lastcomma;
            if (seglen > 0) {
                sb->syn = GrowArray(sb->syn, &sb->capacity, sb->index, sizeof(Synonym));
                Synonym *entry = &sb->syn[sb->index];
                entry->SynonymString = MemAlloc(seglen);
                memcpy(entry->SynonymString, &str[lastcomma + 1], seglen);
                debug_print("Found new synonym string \"%s\"\n", entry->SynonymString);
                sb->index++;
            }
        }

        if (foundrep) {
            size_t replen = strlen(sb->replace) + 1;
            for (int k = sb->firstsyn; k < sb->index; k++) {
                sb->syn[k].ReplacementString = MemAlloc(replen);
                memcpy(sb->syn[k].ReplacementString, sb->replace, replen);
                debug_print("Setting replacement string of \"%s\" (%d) to \"%s\"\n", sb->syn[k].SynonymString, k, sb->syn[k].ReplacementString);
            }
            sb->firstsyn = sb->index;
            foundrep = 0;
        }

        lastcomma = commapos;
    }
}

/* Returns the builder's list, terminated by a NULL SynonymString. */
static Synonym *FinishSynonyms(SynonymBuilder *sb)
{
    free(sb->replace);
    sb->replace = NULL;
    sb->syn = GrowArray(sb->syn, &sb->capacity, sb->index, sizeof(Synonym));
    Synonym *finalsyns = sb->syn;
    finalsyns[sb->index].SynonymString = NULL;

    for (int j = 0; finalsyns[j].SynonymString != NULL; j++)
        debug_print("Synonym entry %d: \"%s\", Replacement \"%s\"\n", j, finalsyns[j].SynonymString, finalsyns[j].ReplacementString);

    return finalsyns;
}

/* Reads substitution (synonym) entries from the plaintext database.
   Each string contains comma-separated tokens. An '=' prefix marks the next
   token as a replacement string; all preceding synonym strings in the current
   group are mapped to that replacement. For example, the string
   ",HELLO,HI,=,GREET," means both "HELLO" and "HI" expand to "GREET". */
static Synonym *ReadSubstitutions(FILE *f, int numstrings)
{
    SynonymBuilder sb = { .syn = NULL, .replace = NULL, .index = 0, .firstsyn = 0 };

    for (int i = 0; i < numstrings; i++) {
        size_t length;
        char *str = ReadString(f, &length);
        debug_print("Read synonym string \"%s\"\n", str);
        if (str[0] == '\0') {
            free(str);
            continue;
        }
        ParseSynonymString(&sb, str, length);
        free(str);
    }

    return FinishSynonyms(&sb);
}

/* The end of the binary database being loaded: one past its last byte. Set by
   LoadDatabaseBinary(). */
static const uint8_t *DataEnd;

/* Every read from the binary database is announced here first: gives up if
   there are fewer than n bytes left at ptr. A database cut short would
   otherwise be read past its end. */
static void NeedBytes(const uint8_t *ptr, size_t n)
{
    if (ptr > DataEnd || (size_t)(DataEnd - ptr) < n)
        Fatal("Broken database!");
}

static uint8_t *ReadPlusString(uint8_t *ptr, char **string, size_t *length);

/* Binary-format variant of ReadSubstitutions. Reads substitution entries from
   an in-memory binary database using length-prefixed strings (via ReadPlusString)
   instead of file I/O. Same comma-delimited format and '=' replacement logic
   as the plaintext version. Advances *startpointer past the consumed data. */
static Synonym *ReadSubstitutionsBinary(uint8_t **startpointer, int numstrings)
{
    SynonymBuilder sb = { .syn = NULL, .replace = NULL, .index = 0, .firstsyn = 0 };
    uint8_t *ptr = *startpointer;

    for (int i = 0; i < numstrings; i++) {
        char *str = NULL;
        size_t length;
        ptr = ReadPlusString(ptr, &str, &length);
        while (length == 0 && i == 0) {
            free(str);
            ptr = ReadPlusString(ptr, &str, &length);
        }
        debug_print("Read synonym string %d, \"%s\"\n", i, str);
        if (str[0] == 0) {
            free(str);
            continue;
        }
        ParseSynonymString(&sb, str, length);
        free(str);
    }

    *startpointer = ptr;
    return FinishSynonyms(&sb);
}

static char **Comments;

/* Reads the comment/annotation section that follows the main game data in
   the plaintext database. These are human-readable descriptions of each action,
   used by DumpActions() to annotate the debug output. The resulting NULL-
   terminated array is stored in the global Comments pointer. */
static void ReadComments(FILE *f)
{
    char **strings = NULL;
    int count = 0, capacity = 0;

    char *comment;
    while ((comment = ReadWholeLine(f)) != NULL) {
        strings = GrowArray(strings, &capacity, count, sizeof(char *));
        strings[count] = comment;
        debug_print("Comment %d:\"%s\"\n", count, comment);
        count++;
    }

    /* GrowArray leaves a NULL after the last one */
    Comments = strings;
}

/* Reads a single numeric value from the plaintext database.
   Each value occupies its own line. Returns 0 on parse failure. */
static uint8_t ReadNum(FILE *f)
{
    int num;
    if (fscanf(f, "%d\n", &num) != 1)
        return 0;
    return num;
}

#define MAX_CONDITION_ARGS 1024

/* Append one condition to an action's condition list. Each condition is a
   16-bit value: the lower 15 bits encode a 5-bit condition opcode and a
   10-bit argument, stored as a condition/argument pair. The high bit
   signals the last condition in the chain; returns nonzero for that one. */
static int AddCondition(uint16_t raw, uint16_t *conditions, int *condargs)
{
    if (*condargs + 2 > MAX_CONDITION_ARGS)
        Fatal("Broken database!");
    conditions[(*condargs)++] = raw & 0x1f;
    conditions[(*condargs)++] = (raw >> 5) & 0x3ff;
    return raw & 0x8000;
}

/* Copy the condition list built by AddCondition() into the action,
   terminated by a 255 sentinel. */
static void SetActionConditions(Action *ap, const uint16_t *conditions, int condargs)
{
    ap->Conditions = MemAlloc((condargs + 1) * sizeof(uint16_t));
    memcpy(ap->Conditions, conditions, condargs * sizeof(uint16_t));
    ap->Conditions[condargs] = 255;
}

/* The unfinished X-Men has a typing error in the action that makes the
   Aquarian follow the player ("aquarian in rooms 24-78"): its first
   condition reads "2 32 24", is item 32 here, where "21 32 24", is
   counter 32 (the current room) at least 24, was meant. As written he
   never follows, and without him the Bio Gem cannot be had, as he is
   what keeps the Natter energy egg next to it from going off. */
static void PatchXMenAquarian(void)
{
    static const uint16_t typo[] = { 2, 32, 0, 24, 31, 3, 21, 32, 0, 79 };
    const int len = sizeof typo / sizeof typo[0];

    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        uint16_t *cond = Actions[ct].Conditions;
        int i = 0;
        while (i < len && cond[i] != 255 && cond[i] == typo[i])
            i++;
        if (i == len) {
            cond[0] = 21;
            return;
        }
    }
}

/* Reads a single action entry from the plaintext database.
   The first byte encodes two 4-bit fields: the high nibble is the number of
   extra word parameters, and the low nibble is the command byte count.
   Next come verb and noun (or auto-trigger chance), then extra words,
   then a chain of 16-bit condition entries (high bit set on the last one),
   and finally the command bytes. */
static void ReadAction(FILE *f, Action *ap)
{
    /* High nibble = number of extra words, low nibble = command length */
    uint8_t length = ReadNum(f);
    ap->NumWords = length >> 4;
    ap->CommandLength = length & 0xf;
    ap->Verb = ReadNum(f);
    if (ap->Verb & 128) {
        Fatal("2-byte verbs unimplemented");
    }
    ap->NounOrChance = ReadNum(f);
    if (ap->NounOrChance & 128) {
        Fatal("2-byte nouns unimplemented");
    }
    ap->Words = MemAlloc(ap->NumWords);

    for (int i = 0; i < ap->NumWords; i++) {
        ap->Words[i] = ReadNum(f);
    }

    /* Read conditions: each is a big-endian 16-bit value, written as
       two numbers */
    uint16_t conditions[MAX_CONDITION_ARGS];
    int condargs = 0;
    for (;;) {
        uint16_t raw = ReadNum(f) * 256 + ReadNum(f);
        if (AddCondition(raw, conditions, &condargs))
            break;
    }
    SetActionConditions(ap, conditions, condargs);

    /* Read command bytes directly into a heap allocation */
    int cmdlen = ap->CommandLength + 1;
    ap->Commands = MemAlloc(cmdlen);
    for (int i = 0; i < cmdlen; i++) {
        ap->Commands[i] = ReadNum(f);
    }
}

/* The plaintext database header has 18 numbers, in the order of the
   fields of Header. */
#define HEADER_FIELDS 18

static Header HeaderFromFields(const int f[HEADER_FIELDS])
{
    Header h;
    h.NumItems = f[0];
    h.ActionSum = f[1];
    h.NumNouns = f[2];
    h.NumVerbs = f[3];
    h.NumRooms = f[4];
    h.MaxCarry = f[5];
    h.PlayerRoom = f[6];
    h.NumMessages = f[7];
    h.TreasureRoom = f[8];
    h.LightTime = f[9];
    h.NumPreps = f[10];
    h.NumAdverbs = f[11];
    h.NumActions = f[12];
    h.Treasures = f[13];
    h.NumSubStr = f[14];
    h.Unknown1 = f[15];
    h.NumObjImg = f[16];
    h.Unknown2 = f[17];
    return h;
}

/* Set the counters and flags that mirror header values, and put the
   player in the start room. */
static void SetCountersFromHeader(void)
{
    LastItem = GameHeader.NumItems;
    MyLoc = GameHeader.PlayerRoom;
    DeadRoom = GameHeader.NumRooms;
    TreasureRoomCounter = GameHeader.TreasureRoom;
    CarryLimit = GameHeader.MaxCarry;
    SetBit(GRAPHICSBIT); // Graphics on
}

/* Allocates the item, room and object image tables for GameHeader, zeroed
   so that FreeGameData can free a half-filled one. */
static void AllocateTables(void)
{
    Items = MemCalloc(sizeof(Item) * (GameHeader.NumItems + 1));
    Rooms = MemCalloc(sizeof(Room) * (GameHeader.NumRooms + 1));
    ObjectImages = MemCalloc(sizeof(ObjectImage) * (GameHeader.NumObjImg + 1));
}

/* Whether every object picture is of an item that exists: the picture
   code looks the item up to see whether it is in the room. (Object is
   unsigned, so a negative number in a plain text database is caught as a
   large one.) */
static int ObjectImageItemsValid(void)
{
    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].Object > (uint32_t)GameHeader.NumItems) {
            debug_print("Object image %d is of item %u, which does not exist\n", ct, ObjectImages[ct].Object);
            return 0;
        }
    return 1;
}

/* An item's noun word, which the databases store as 16 bits. Item.Dictword
   (like DictWord.Group) is 8 bits: no game has more than 96 noun groups.
   A larger value can only come from a broken database, and means no noun. */
static uint8_t ItemNounWord(int word, int item)
{
    if (word < 0 || word > UINT8_MAX) {
        debug_print("Item %d has a noun word out of range (%d)\n", item, word);
        return 0;
    }
    return (uint8_t)word;
}

/* Look up each room's description text: the EXIT_DESCRIPTION column holds
   the message as a long message command number, or 0 for no description.
   Call after loading Messages. */
static void SetRoomDescriptions(void)
{
    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        int message = Rooms[i].Exits[EXIT_DESCRIPTION] - LONG_MESSAGE_OFFSET;
        if (Rooms[i].Exits[EXIT_DESCRIPTION] == 0 || message < 0 || message > GameHeader.NumMessages)
            Rooms[i].Text = "";
        else
            Rooms[i].Text = Messages[message];
        debug_print("Room description of room %d: \"%s\"\n", i, Rooms[i].Text);
    }
}

/* Matches the game's title/ID string against the known games database.
   Sets the global Game pointer on success. Returns 1 if a match was found,
   0 if the game is unrecognized. */
/* The title must be the whole ID string: a damaged database whose title
   is empty or cut short is not one of the games. */
static int SetGame(const char *id_string)
{
    for (int i = 0; games[i].title != NULL; i++)
        if (strcmp(id_string, games[i].ID_string) == 0) {
            Game = &games[i];
            return 1;
        }
    return 0;
}

static void FreeDictionary(DictWord **dict)
{
    if (*dict != NULL)
        for (int i = 0; (*dict)[i].Word != NULL; i++)
            free((*dict)[i].Word);
    free(*dict);
    *dict = NULL;
}

/* Frees whatever a database load that failed part of the way had
   allocated, and forgets the game it thought it had found, so that the
   next attempt starts from nothing. The arrays that hold pointers are
   allocated zeroed, which is what makes a half-filled one safe to free.
   Room descriptions are not freed: they point into Messages. */
static void FreeGameData(void)
{
    if (Items != NULL)
        for (int i = 0; i <= GameHeader.NumItems; i++)
            free(Items[i].Text);
    free(Items);
    Items = NULL;

    if (Actions != NULL)
        for (int i = 0; i <= GameHeader.NumActions; i++) {
            free(Actions[i].Words);
            free(Actions[i].Conditions);
            free(Actions[i].Commands);
        }
    free(Actions);
    Actions = NULL;

    if (Messages != NULL)
        for (int i = 0; i <= GameHeader.NumMessages; i++)
            free(Messages[i]);
    free(Messages);
    Messages = NULL;

    free(Rooms);
    Rooms = NULL;
    free(ObjectImages);
    ObjectImages = NULL;

    FreeDictionary(&Verbs);
    FreeDictionary(&Nouns);
    FreeDictionary(&Adverbs);
    FreeDictionary(&Prepositions);

    if (Substitutions != NULL)
        for (int i = 0; Substitutions[i].SynonymString != NULL; i++) {
            free(Substitutions[i].SynonymString);
            free(Substitutions[i].ReplacementString);
        }
    free(Substitutions);
    Substitutions = NULL;

    if (Comments != NULL)
        for (int i = 0; Comments[i] != NULL; i++)
            free(Comments[i]);
    free(Comments);
    Comments = NULL;

    memset(&GameHeader, 0, sizeof GameHeader);
    Game = NULL;
}

/* Attempts to load a .PAK image file by constructing its full path from the
   game directory and a short name (e.g. "R00", "B03", "S01"). On success,
   populates the imgrec with the file data, size, and filename. Returns 1 if
   the file was found and loaded, 0 otherwise. */
int FindAndAddImageFile(const char *shortname, imgrec *rec)
{
    int result = 0;
    size_t pathlen = DirPathLength + 9;
    char *filename = MemAlloc(pathlen);
    int n = snprintf(filename, pathlen, "%s%s.PAK", DirPath, shortname);
    if (n > 0) {
        rec->Data = ReadFileIfExists(filename, &rec->Size);
        if (rec->Data == NULL) {
            debug_print("Could not find or read image file %s\n", filename);
        } else {
            size_t namelen = strlen(shortname) + 1;
            rec->Filename = MemAlloc(namelen);
            memcpy(rec->Filename, shortname, namelen);
            result = 1;
            debug_print("Found and read image file %s\n", filename);
        }
    }
    free(filename);
    return result;
}

/* Builds the image list of an MS-DOS game from the .PAK files next to the
   database. Images are organized in three groups: room images ('R'),
   item/object images ('B'), and special images ('S'), each numbered from 0.
   The games list how many of each there can be; the missing ones are
   skipped. */
static void BuildPakImageList(void)
{
    const struct {
        char type;
        int count;
    } groups[] = {
        { 'R', Game->no_of_room_images },
        { 'B', Game->no_of_item_images },
        { 'S', Game->no_of_special_images },
    };

    int numimages = 0;
    for (size_t g = 0; g < ARRAY_SIZE(groups); g++)
        numimages += groups[g].count;

    Images = NewImageList(numimages);
    debug_print("\nTotal number of images:%d\n", numimages);

    int recidx = 0;
    for (size_t g = 0; g < ARRAY_SIZE(groups); g++)
        for (int imgidx = 0; imgidx < groups[g].count; imgidx++) {
            char *shortname = ShortNameFromType(groups[g].type, imgidx);
            if (FindAndAddImageFile(shortname, &Images[recidx]))
                recidx++;
            free(shortname);
        }
    Images[recidx].Filename = NULL;
}

/* Loads a game database from a plaintext (DOS/PC) format file. The plaintext
   format starts with a quoted title string (used to identify the game),
   followed by comma-separated header values, then sections for actions,
   dictionary words, rooms, messages, items, adverbs, prepositions,
   substitutions, object images, mystery values, and comments.
   On success, closes the file and returns the game ID. On failure (unrecognized
   game or malformed data), returns UNKNOWN_GAME with the file left open so
   the caller can retry with the binary loader. */
int LoadDatabasePlaintext(FILE *f)
{
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;

    /* The title string comes first and identifies which game this is */
    size_t length;
    char *title = ReadQuoted(f, &length);

    if (title == NULL || !SetGame(title)) {
        free(title);
        return UNKNOWN_GAME;
    }

    /* Parse the 18 comma-separated header values that follow the title */
    int raw[HEADER_FIELDS] = { 0 };
    if (fscanf(f, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            &raw[0], &raw[1], &raw[2], &raw[3], &raw[4], &raw[5], &raw[6],
            &raw[7], &raw[8], &raw[9], &raw[10], &raw[11], &raw[12],
            &raw[13], &raw[14], &raw[15], &raw[16], &raw[17])
        < HEADER_FIELDS) {
        debug_print("Invalid database(bad header)\n");
        free(title);
        Game = NULL;
        return UNKNOWN_GAME;
    }
    free(title);

    GameHeader = HeaderFromFields(raw);

    int num_items = GameHeader.NumItems;
    int num_actions = GameHeader.NumActions;
    int num_rooms = GameHeader.NumRooms;
    int num_messages = GameHeader.NumMessages;
    int num_obj_img = GameHeader.NumObjImg;
    int unknown2 = GameHeader.Unknown2;

    AllocateTables();
    Actions = MemCalloc(sizeof(Action) * (num_actions + 1));
    Messages = MemCalloc(sizeof(char *) * (num_messages + 1));

    SetCountersFromHeader();

    if (DEBUG_PRINT)
        PrintHeaderInfo(GameHeader);

    /* Load actions — each action has a verb/noun trigger, optional extra
       words, a condition chain, and a command sequence */
    ct = 0;
    ap = Actions;
    debug_print("Reading %d actions.\n", num_actions);
    while (ct < num_actions + 1) {
        ReadAction(f, ap);
        ap++;
        ct++;
    }

    /* Load verb and noun dictionaries */
    debug_print("Reading %d verbs.\n", GameHeader.NumVerbs);
    Verbs = ReadDictWordsPC(f, GameHeader.NumVerbs + 1);
    debug_print("Reading %d nouns.\n", GameHeader.NumNouns);
    Nouns = ReadDictWordsPC(f, GameHeader.NumNouns + 1);

    /* Load the room table: 6 compass directions, the description message
       and the room image (see NUM_EXIT_COLUMNS) */
    rp = Rooms;
    debug_print("Reading %d rooms.\n", num_rooms);
    for (ct = 0; ct < num_rooms + 1; ct++) {
        if (fscanf(f, "%d,%d,%d,%d,%d,%d,%d,%d\n", &rp->Exits[0], &rp->Exits[1],
                &rp->Exits[2], &rp->Exits[3], &rp->Exits[4],
                &rp->Exits[5], &rp->Exits[EXIT_DESCRIPTION], &rp->Exits[EXIT_IMAGE])
            != 8) {
            debug_print("Bad room line (%d)\n", ct);
            FreeGameData();
            return UNKNOWN_GAME;
        }
        rp++;
    }

    /* Load message strings */
    debug_print("Reading %d messages.\n", num_messages);
    for (ct = 0; ct < num_messages + 1; ct++) {
        Messages[ct] = ReadString(f, &length);
        debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }

    SetRoomDescriptions();

    /* Load items: each has a quoted text string followed by comma-separated
       location, dictionary word index, and flag values */
    ct = 0;
    debug_print("Reading %d items.\n", num_items);
    int loc, dictword, flag;
    ip = Items;
    while (ct < num_items + 1) {
        ip->Text = ReadString(f, &length);
        debug_print("Item %d: \"%s\"\n", ct, ip->Text);
        if (fscanf(f, ",%d,%d,%d\n", &loc, &dictword, &flag) != 3) {
            debug_print("Bad item line (%d)\n", ct);
            FreeGameData();
            return UNKNOWN_GAME;
        }
        /* -1 is carried; the location is 16 bits, as in the binary files */
        ip->Location = loc == -1 ? CARRIED : (uint16_t)loc;
        ip->Dictword = ItemNounWord(dictword, ct);
        ip->Flag = (uint16_t)flag;
        if (ip->Location == CARRIED || ip->Location <= GameHeader.NumRooms)
            debug_print("Location of item %d: %d, \"%s\"\n", ct, ip->Location,
                ip->Location == CARRIED ? "CARRIED" : Rooms[ip->Location].Text);
        ip->InitialLoc = ip->Location;
        ip++;
        ct++;
    }

    /* Load additional dictionaries: adverbs and prepositions */
    debug_print("Reading %d adverbs.\n", GameHeader.NumAdverbs);
    Adverbs = ReadDictWordsPC(f, GameHeader.NumAdverbs + 1);
    debug_print("Reading %d prepositions.\n", GameHeader.NumPreps);
    Prepositions = ReadDictWordsPC(f, GameHeader.NumPreps + 1);

    /* Load text substitutions (synonym expansions) */
    Substitutions = ReadSubstitutions(f, GameHeader.NumSubStr + 1);

    /* Load object image mappings: each entry maps a room and object to an
       image index, controlling which graphics are shown for item sprites */
    ObjectImage *objimg = ObjectImages;
    debug_print("Reading %d object image values.\n", num_obj_img + 1);
    for (ct = 0; ct <= num_obj_img; ct++) {
        if (fscanf(f, "%d,%d,%d\n", &objimg->Room, &objimg->Object, &objimg->Image)
            != 3) {
            debug_print("Bad object image line (%d)\n", ct);
            FreeGameData();
            return UNKNOWN_GAME;
        }
        debug_print("Object image %d ", ct);
        debug_print("room: %d object: %d image: %d\n", objimg->Room, objimg->Object, objimg->Image);
        objimg++;
    }
    if (!ObjectImageItemsValid()) {
        FreeGameData();
        return UNKNOWN_GAME;
    }

    /* Skip the values of the header's Unknown2 field: their purpose is not
       known, and the binary databases do not have them */
    debug_print("Skipping %d unknown values.\n", unknown2 + 1);
    for (ct = 0; ct < unknown2 + 1; ct++)
        ReadNum(f);

    /* Load action comments (human-readable annotations) */
    ReadComments(f);

    if (CurrentGame == XMEN)
        PatchXMenAquarian();

    if (DEBUG_PRINT) {
        debug_print("\nLoad Complete.\n\n");
        DumpActions(Comments);
    }
    fclose(f);

    BuildPakImageList();

    CurrentSys = SYS_MSDOS;

    return CurrentGame;
}

/* Reads a length-prefixed string from an in-memory binary database.
   The first byte is the string length; 0 or 255 indicate an empty string.
   Backticks are converted to double quotes (same convention as the plaintext
   format). Returns the advanced pointer past the consumed bytes. */
static uint8_t *ReadPlusString(uint8_t *ptr, char **string, size_t *len)
{
    NeedBytes(ptr, 1);
    uint8_t length = *ptr++;
    if (length == 0 || length == 255) {
        *string = MemCalloc(1);
        *len = 0;
        return ptr;
    }

    NeedBytes(ptr, length);
    char *str = MemAlloc(length + 1);
    for (int i = 0; i < length; i++) {
        str[i] = *ptr++;
        if (str[i] == '`')
            str[i] = '"';
    }
    str[length] = 0;

    *string = str;
    *len = length;
    return ptr;
}

/* Loads message strings from an in-memory binary database. On Atari ST,
   empty strings (except the first) are skipped — the ST format sometimes
   inserts padding zero bytes between messages. Advances *ptr past all
   consumed data. */
static char **LoadMessages(int numstrings, uint8_t **ptr)
{
    if (numstrings == 0)
        numstrings = 0xffff;
    char *str;
    char **target = MemAlloc(numstrings * sizeof(char *));
    int i;
    for (i = 0; i < numstrings; i++) {
        size_t length;
        *ptr = ReadPlusString(*ptr, &str, &length);
        if (CurrentSys == SYS_ST) {
            while (str[0] == 0 && i != 0) {
                free(str);
                *ptr = ReadPlusString(*ptr, &str, &length);
            }
        }
        debug_print("Message %d: \"%s\"\n", i, str);
        target[i] = str;
    }
    return target;
}

/* The binary header starts at this offset in the raw file image, and is
   16 words long. */
#define BINARY_HEADER_OFFSET 0x32
#define BINARY_HEADER_WORDS 16

/* Reads the 16-bit values of the binary header, and returns them in the
   order of the 18 fields of the plaintext header. The binary format has no
   explicit action count, treasure count or count of "mystery values" (they
   are set to 0); the action count is determined by parsing until ActionSum
   bytes are consumed. The purpose of words 14 and 15 is unknown.
   Atari ST uses big-endian byte order; all other platforms use
   little-endian. Returns a pointer to the start of the data following the
   header (backs up 2 bytes because the last header word overlaps with the
   action data that follows). */
static uint8_t *ReadHeader(uint8_t *ptr, int fields[HEADER_FIELDS])
{
    uint16_t h[BINARY_HEADER_WORDS];
    for (int i = 0; i < BINARY_HEADER_WORDS; i++) {
        if (CurrentSys == SYS_ST)
            h[i] = READ_BE_UINT16_AND_ADVANCE(&ptr);
        else
            h[i] = READ_LE_UINT16_AND_ADVANCE(&ptr);
        debug_print("Header value %d: %d\n", i, h[i]);
    }

    /* Items, action sum, nouns, verbs, rooms, max carried, start room,
       messages, treasure room, light time, prepositions and adverbs come
       in the same order as in the plaintext header */
    for (int i = 0; i < 12; i++)
        fields[i] = h[i];
    fields[12] = 0;     /* Number of actions */
    fields[13] = 0;     /* Number of treasures */
    fields[14] = h[12]; /* Number of substitution strings */
    fields[15] = h[15]; /* Unknown1 */
    fields[16] = h[13]; /* Number of object images */
    fields[17] = 0;     /* Unknown2 */

    return ptr - 2;
}

/* Binary-format variant of ReadDictWordsPC. Reads dictionary words from an
   in-memory binary database using length-prefixed strings (via ReadPlusString).
   Same comma-delimited grouping as the plaintext version. Empty strings
   (padding bytes) are skipped; ReadPlusString() stops the skipping at the end
   of the database. Advances *pointer past all consumed data. */
static DictWord *ReadDictWords(uint8_t **pointer, int numstrings)
{
    uint8_t *ptr = *pointer;
    DictBuilder db = { NULL, 0, 0 };
    int group = 0;

    for (int i = 0; i < numstrings; i++) {
        char *str = NULL;
        size_t strlength;
        ptr = ReadPlusString(ptr, &str, &strlength);
        /* Skip padding zero bytes that appear in some binary formats */
        while (str[0] == 0) {
            free(str);
            ptr = ReadPlusString(ptr, &str, &strlength);
        }
        debug_print("Read dictionary string \"%s\"\n", str);
        ParseDictString(str, strlength, group, &db);
        free(str);
        group++;
    }

    *pointer = ptr;
    return FinishDictionary(&db);
}

/* A binary database starts with a table of 24 16-bit pointers, one to each
   section of the data and a last one to the end of it. On the 8-bit systems
   the sections follow each other directly, so the table is not needed to
   find them. On Atari ST every section starts on an even address (the
   68000 cannot read a word from an odd one), so there may be a pad byte
   after any section with an odd length, and there the table is used.
   The Atari ST pointers are big-endian offsets into the file. */
enum {
    SECT_ACTIONS,
    SECT_VERBS,
    SECT_NOUNS,
    SECT_ROOMS, /* The first of eight, one for each column */
    SECT_MESSAGES = SECT_ROOMS + 8,
    SECT_ITEM_NAMES,
    SECT_ITEM_LOCATIONS,
    SECT_ITEM_WORDS,
    SECT_ITEM_FLAGS,
    SECT_ADVERBS,
    SECT_PREPOSITIONS,
    SECT_SUBSTITUTIONS,
    SECT_TITLE,
    SECT_OBJIMG_ROOMS,
    SECT_OBJIMG_OBJECTS,
    SECT_OBJIMG_IMAGES,
    SECT_END,
    NUM_SECTIONS
};

/* The section table follows the two bytes at the head of the file (a load
   address on the 8-bit systems; stdetect.c adds two to an Atari ST file
   to match) */
#define SECTION_TABLE_OFFSET 2

/* Returns the start of a section. On Atari ST this is looked up in the
   section table. On the other systems it is ptr, the end of the section
   before it. */
static uint8_t *SectionStart(uint8_t *ptr, int section)
{
    if (CurrentSys != SYS_ST)
        return ptr;
    const uint8_t *entry = mem + SECTION_TABLE_OFFSET + section * 2;
    size_t offset = (size_t)(entry[0] << 8 | entry[1]) + SECTION_TABLE_OFFSET;
    if (offset > memlen)
        Fatal("Broken database!");
    return mem + offset;
}

/* Reads a 16-bit value: big-endian on Atari ST, little-endian on the other
   systems. */
static int ReadWord(uint8_t **ptr)
{
    if (CurrentSys == SYS_ST)
        return READ_BE_UINT16_AND_ADVANCE(ptr);
    return READ_LE_UINT16_AND_ADVANCE(ptr);
}

/* Apple II stores image addresses as two bytes that combine into a memory
   address: high * 0x1000 + low * 0x100. The high byte has already been
   read; the low byte is read from *ptr. */
static int ReadApple2ImageAddress(int high, uint8_t **ptr)
{
    int adr = high * 0x100;
    NeedBytes(*ptr, 1);
    adr += *(*ptr)++ * 0x10;
    adr *= 0x10;
    return adr;
}

/* Binary-format variant of ReadAction: the same fields, as bytes, with the
   conditions as big-endian 16-bit values on every system. Returns the
   pointer past the action. */
static uint8_t *ReadActionBinary(uint8_t *ptr, Action *ap)
{
    NeedBytes(ptr, 3);
    /* High nibble = number of extra words, low nibble = command length */
    uint8_t length = *ptr++;
    ap->NumWords = length >> 4;
    ap->CommandLength = length & 0xf;
    ap->Verb = *ptr++;
    ap->NounOrChance = *ptr++;

    NeedBytes(ptr, ap->NumWords);
    ap->Words = MemAlloc(ap->NumWords);
    for (int i = 0; i < ap->NumWords; i++)
        ap->Words[i] = *ptr++;

    uint16_t conditions[MAX_CONDITION_ARGS];
    int condargs = 0;
    for (;;) {
        NeedBytes(ptr, 2);
        uint16_t raw = READ_BE_UINT16_AND_ADVANCE(&ptr);
        if (AddCondition(raw, conditions, &condargs))
            break;
    }
    SetActionConditions(ap, conditions, condargs);

    int cmdlen = ap->CommandLength + 1;
    NeedBytes(ptr, cmdlen);
    ap->Commands = MemAlloc(cmdlen);
    memcpy(ap->Commands, ptr, cmdlen);
    return ptr + cmdlen;
}

/* Validates that key header fields fall within expected ranges for a
   legitimate Plus game database. Returns 1 if valid, 0 if any field
   is out of range. Used by the binary loader to reject corrupt data. */
static int SanityCheckHeader(const Header *h)
{
    if (h->NumItems < 10 || h->NumItems > 500)
        return 0;
    if (h->NumNouns < 50 || h->NumNouns > 190)
        return 0;
    if (h->NumVerbs < 30 || h->NumVerbs > 190)
        return 0;
    if (h->NumRooms < 10 || h->NumRooms > 100)
        return 0;
    /* The player must start in a room that exists (PlayerRoom() stops
       the interpreter otherwise) */
    if (h->PlayerRoom < 0 || h->PlayerRoom > h->NumRooms)
        return 0;
    /* The other counts are used as array sizes */
    if (h->ActionSum < 0 || h->NumMessages < 0 || h->NumPreps < 0 ||
        h->NumAdverbs < 0 || h->NumSubStr < 0 || h->NumObjImg < 0)
        return 0;

    return 1;
}

/* Whether data, read as a binary database, has a plausible header. This
   reads nothing but the header, so a detector can ask it before anything
   is loaded. */
int LooksLikePlusDatabase(uint8_t *data, size_t size)
{
    if (size < BINARY_HEADER_OFFSET + BINARY_HEADER_WORDS * 2)
        return 0;
    int raw[HEADER_FIELDS];
    ReadHeader(data + BINARY_HEADER_OFFSET, raw);
    Header h = HeaderFromFields(raw);
    return SanityCheckHeader(&h);
}

/* Loads a game database from an in-memory binary image (previously read from
   a platform-specific file: C64, Apple II, Atari 8-bit, or Atari ST).
   The binary format is more compact than the plaintext version: strings are
   length-prefixed, header values are 16-bit words, and the action count is
   not stored explicitly — actions are read until ActionSum bytes are consumed.
   On Atari ST the 16-bit values are big-endian and the sections are padded
   to an even length; see SectionStart().
   Returns 1 on success, 0 on failure. */
int LoadDatabaseBinary(void)
{
    int ct;

    Action *ap;

    if (memlen < BINARY_HEADER_OFFSET + BINARY_HEADER_WORDS * 2)
        return 0;

#pragma mark header

    int raw[HEADER_FIELDS];
    uint8_t *ptr = ReadHeader(mem + BINARY_HEADER_OFFSET, raw);
    DataEnd = mem + memlen;

    Header h = HeaderFromFields(raw);

    PrintHeaderInfo(h);

    /* Reject the file before anything is changed if this cannot be a
       Plus database */
    if (!SanityCheckHeader(&h))
        return 0;

    GameHeader = h;

    int num_items = h.NumItems;
    int num_rooms = h.NumRooms;
    int num_obj_img = h.NumObjImg;

    AllocateTables();

    SetCountersFromHeader();

#pragma mark actions

    /* The binary format does not store an explicit action count. Instead,
       actions are read sequentially until ActionSum bytes have been consumed.
       The preliminary array grows dynamically if needed. */
    ct = 0;
    int act_capacity = 524;
    Action *PrelActions = MemAlloc(sizeof(Action) * act_capacity);

    ap = PrelActions;

    uint8_t *origptr = ptr;

    while (ptr - origptr <= GameHeader.ActionSum) {
        ptr = ReadActionBinary(ptr, ap);
        ap++;
        ct++;

        if (ct >= act_capacity) {
            act_capacity *= 2;
            PrelActions = MemRealloc(PrelActions, sizeof(Action) * act_capacity);
            ap = PrelActions + ct;
        }
    }

    GameHeader.NumActions = ct - 1;

    /* Copy the preliminary action array into a right-sized final allocation */
    Actions = MemAlloc(sizeof(Action) * ct);
    memcpy(Actions, PrelActions, sizeof(Action) * ct);
    free(PrelActions);

#pragma mark dictionary

    ptr = SectionStart(ptr, SECT_VERBS);
    Verbs = ReadDictWords(&ptr, GameHeader.NumVerbs + 1);
    ptr = SectionStart(ptr, SECT_NOUNS);
    Nouns = ReadDictWords(&ptr, GameHeader.NumNouns + 1);

#pragma mark room connections

    /* The room connections are stored column-major: all rooms' North exits
       come first, then South, East, West, Up, Down, then two extra columns
       (EXIT_DESCRIPTION and EXIT_IMAGE). The first six columns are one
       byte a room, the last two are 16-bit. On Apple II, the image column
       holds a memory address, which LookForApple2Images turns into a number. */
    for (int j = 0; j < NUM_EXIT_COLUMNS; j++) {
        ptr = SectionStart(ptr, SECT_ROOMS + j);
        for (ct = 0; ct <= num_rooms; ct++) {
            if (CurrentSys == SYS_APPLE2 && j == EXIT_IMAGE) {
                /* The second byte is checked where it is read */
                NeedBytes(ptr, 1);
                Rooms[ct].Exits[j] = ReadApple2ImageAddress(*ptr++, &ptr);
                debug_print("Room image %d address:%x\n", ct, Rooms[ct].Exits[j]);
            } else if (j >= NUM_DIRECTIONS) {
                NeedBytes(ptr, 2);
                Rooms[ct].Exits[j] = ReadWord(&ptr);
            } else {
                NeedBytes(ptr, 1);
                Rooms[ct].Exits[j] = *ptr++;
            }

            debug_print("Room %d exit %d: %d\n", ct, j, Rooms[ct].Exits[j]);
        }
    }

#pragma mark messages

    ptr = SectionStart(ptr, SECT_MESSAGES);
    Messages = LoadMessages(GameHeader.NumMessages + 1, &ptr);

    SetRoomDescriptions();

#pragma mark items

    /* Item data is split across multiple columnar passes: first all item
       names, then all locations, then all dictionary word indices, then
       all flags — similar to the room exit layout. */
    size_t length;

    ptr = SectionStart(ptr, SECT_ITEM_NAMES);
    for (ct = 0; ct <= num_items; ct++)
        ptr = ReadPlusString(ptr, &Items[ct].Text, &length);

#pragma mark item locations
    ptr = SectionStart(ptr, SECT_ITEM_LOCATIONS);
    NeedBytes(ptr, num_items + 1);
    for (ct = 0; ct <= num_items; ct++) {
        Items[ct].Location = *ptr++;
        Items[ct].InitialLoc = Items[ct].Location;
    }

    /* The dictionary words and the flags, two bytes an item each */
    ptr = SectionStart(ptr, SECT_ITEM_WORDS);
    NeedBytes(ptr, (num_items + 1) * 2);
    for (ct = 0; ct <= num_items; ct++)
        Items[ct].Dictword = ItemNounWord(ReadWord(&ptr), ct);

    ptr = SectionStart(ptr, SECT_ITEM_FLAGS);
    NeedBytes(ptr, (num_items + 1) * 2);
    for (ct = 0; ct <= num_items; ct++) {
        Items[ct].Flag = ReadWord(&ptr);
        debug_print("Item %d: \"%s\", %d, %d, %d\n", ct, Items[ct].Text, Items[ct].Location, Items[ct].Dictword, Items[ct].Flag);
    }

    ptr = SectionStart(ptr, SECT_ADVERBS);
    Adverbs = ReadDictWords(&ptr, GameHeader.NumAdverbs + 1);
    ptr = SectionStart(ptr, SECT_PREPOSITIONS);
    Prepositions = ReadDictWords(&ptr, GameHeader.NumPreps + 1);

#pragma mark substitutions

    ptr = SectionStart(ptr, SECT_SUBSTITUTIONS);
    Substitutions = ReadSubstitutionsBinary(&ptr, GameHeader.NumSubStr + 1);

    /* In the binary format, the title string comes after the substitutions
       (unlike plaintext where it's at the very beginning) */
    char *title = NULL;
    ptr = SectionStart(ptr, SECT_TITLE);
    ptr = ReadPlusString(ptr, &title, &length);
    debug_print("Title: %s\n", title);

    if (!SetGame(title)) {
        free(title);
        FreeGameData();
        return 0;
    }
    free(title);

    /* Object image mappings are stored in three separate columnar passes:
       room assignments, object assignments, and image indices. On Apple II,
       image indices are two-byte memory addresses like room images, on the
       other systems they are 16-bit values. */
    ptr = SectionStart(ptr, SECT_OBJIMG_ROOMS);
    NeedBytes(ptr, num_obj_img + 1);
    for (ct = 0; ct <= num_obj_img; ct++)
        ObjectImages[ct].Room = *ptr++;

    ptr = SectionStart(ptr, SECT_OBJIMG_OBJECTS);
    NeedBytes(ptr, num_obj_img + 1);
    for (ct = 0; ct <= num_obj_img; ct++)
        ObjectImages[ct].Object = *ptr++;
    if (!ObjectImageItemsValid())
        Fatal("Broken database!");

    ptr = SectionStart(ptr, SECT_OBJIMG_IMAGES);
    for (ct = 0; ct <= num_obj_img; ct++) {
        if (CurrentSys == SYS_APPLE2) {
            /* The second byte is checked where it is read */
            NeedBytes(ptr, 1);
            ObjectImages[ct].Image = ReadApple2ImageAddress(*ptr++, &ptr);
            debug_print("Object image %d address:%x\n", ct, ObjectImages[ct].Image);
        } else {
            NeedBytes(ptr, 2);
            ObjectImages[ct].Image = ReadWord(&ptr);
        }
    }

    if (CurrentSys == SYS_APPLE2)
        ReadApple2SpecialImages(mem, (size_t)(ptr - mem), memlen);

    for (ct = 0; ct <= num_obj_img; ct++)
        debug_print("ObjectImages %d: room:%d object:%d image:%x\n", ct, ObjectImages[ct].Room, ObjectImages[ct].Object, ObjectImages[ct].Image);

    if (DEBUG_PRINT)
        DumpActions(Comments);

    return 1;
}
