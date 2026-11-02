//
//  textoutput.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  Output buffering (smart spacing, punctuation and capitalization) and
//  decoding of the token-compressed text tables.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "glk.h"
#include "gamedefs.h"
#include "gameload.h"
#include "player.h"
#include "ui.h"

#include "textoutput.h"

char LastChar = 0;       /* Buffered output character (for punctuation lookahead) */
static int Upper = 0;    /* Capitalize the next alphabetic character */
int PendSpace = 0;       /* A space is pending before the next character */
int FirstAfterInput = 0; /* First output character after player input */

strid_t room_description_stream = NULL;

/* Write formatted text to the room description stream (used by the
   graphics system to capture room text for layout purposes). */
void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    va_start(ap, fmt);
    PrintToStreamV(room_description_stream, fmt, ap);
    va_end(ap);
}

/* Emit a single character, applying auto-capitalization if set. */
static void OutWrite(char c)
{
    if (isalpha((unsigned char)c) && Upper) {
        c = toupper((unsigned char)c);
        Upper = 0;
    }
    PrintCharacter(c);
}

/* Flush the buffered output character and any pending space. */
void OutFlush(void)
{
    if (LastChar)
        OutWrite(LastChar);
    if (PendSpace && LastChar != '\n' && !FirstAfterInput)
        OutWrite(' ');
    LastChar = 0;
    PendSpace = 0;
}

static int Q3Upper = 0;

/* Flush any buffered character and set capitalize-next flag. */
void OutCaps(void)
{
    if (LastChar) {
        OutWrite(LastChar);
        LastChar = 0;
    }
    Upper = 1;
    Q3Upper = 1;
}

static int periods = 0;
int JustWrotePeriod = 0;

/* Buffer a character for output with smart punctuation handling:
   - ']' is treated as newline
   - Periods are accumulated and deferred (for ellipsis detection)
   - Spaces are deferred to allow punctuation to suppress them
   - Auto-capitalization after sentence-ending punctuation */
void OutChar(char c)
{
    /* Taylor's games use ']' as an in-game newline marker. */
    if (c == ']')
        c = '\n';

    if (c == '.') {
        /* Accumulate periods so we can collapse runs into ellipses below.
           But if this period follows punctuation/whitespace, demote it
           to a space — it's not part of a sentence-ending mark. */
        periods++;
        if (LastChar == '?' || FirstAfterInput || isspace((unsigned char)LastChar) || LastChar == '.') {
            c = ' ';
            if (LastChar == ' ')
                LastChar = 0;
        }
        PendSpace = 0;
    } else {
        /* Non-period: flush any accumulated periods now (unless the next
           char is itself terminal punctuation, in which case the periods
           are part of an abbreviation and stay associated with it). */
        if (periods && !JustWrotePeriod && c != ',' && c != '?' && c != '!') {
            /* Normalize a double-period to a single period; three or more
               periods (an ellipsis) print verbatim. */
            if (periods == 2)
                periods = 1;
            for (int i = 0; i < periods; i++) {
                JustWrotePeriod = 0;
                PrintCharacter('.');
            }
            JustWrotePeriod = 1;
            LastChar = 0;
            PendSpace = 0;
            Upper = 0;
        }
        periods = 0;
    }

    /* Spaces are deferred via PendSpace so the next non-space character
       can decide whether to actually emit one (or suppress it, e.g.
       before punctuation). */
    if (c == ' ') {
        PendSpace = 1;
        return;
    }
    /* First character after a player command: eat leading whitespace and
       capitalize the first real character. */
    if (FirstAfterInput) {
        if (isspace((unsigned char)LastChar)) {
            LastChar = 0;
            Upper = 1;
        } else if (!isspace((unsigned char)c)) {
            FirstAfterInput = 0;
        }
        PendSpace = 0;
    }
    /* Flush the one-character lookahead buffer. Suppress it if it was a
       period we just wrote ourselves above (ellipsis flush). */
    if (LastChar) {
        if (isspace((unsigned char)LastChar))
            PendSpace = 0;
        if (LastChar == '.' && JustWrotePeriod) {
            LastChar = 0;
        } else {
            OutWrite(LastChar);
        }
    }
    /* Emit the deferred space, capitalizing afterwards if it sits between
       a period and the next sentence (skipped for Kayleth, which has its
       own casing rules). */
    if (PendSpace) {
        if (JustWrotePeriod && BaseGame != KAYLETH)
            Upper = 1;
        OutWrite(' ');
        PendSpace = 0;
    }
    /* Buffer this character; the next call will flush it. Newlines reset
       capitalization (both base and QP3-specific). */
    LastChar = c;
    if (LastChar == '\n' || LastChar == '\r') {
        Upper = 1;
        Q3Upper = 1;
    }
}

void OutReplace(char c)
{
    LastChar = c;
}

void OutKillSpace(void)
{
    PendSpace = 0;
}

void OutString(const char *p)
{
    while (*p)
        OutChar(*p++);
}

/* Where each token starts. Finding token n means walking past the n
   tokens before it, and Rebel Planet has to expand every token of every
   message it skips, so the walk is done once and kept. The table is
   rebuilt when the game data it was made from changes (a restart, or
   the other part of a two-part game). */
static unsigned char *TokenStart[256];
static const unsigned char *TokenStartImage, *TokenStartEnd;
static size_t TokenStartBase;

static void FindTokenStarts(void)
{
    unsigned char *p = FileImage + TokenBase;
    for (int i = 0; i < 256; i++) {
        TokenStart[i] = p;
        if (p < EndOfData) {
            while (p < EndOfData && (*p & TOKEN_LAST_BYTE) == 0)
                p++;
            p++;
        }
    }
    TokenStartImage = FileImage;
    TokenStartEnd = EndOfData;
    TokenStartBase = TokenBase;
}

/* Return a pointer to the start of token n in the token table.
   Tokens are variable-length with the high bit marking the last byte. */
unsigned char *TokenText(unsigned char n)
{
    if (Version == QUESTPROBE3_TYPE)
        n -= QP3_TOKEN_BASE;
    if (TokenStartImage != FileImage || TokenStartEnd != EndOfData
        || TokenStartBase != TokenBase)
        FindTokenStarts();
    return TokenStart[n];
}

/* Print a character using Questprobe 3's capitalization rules:
   auto-capitalize after sentence-ending punctuation. */
static void Q3PrintChar(uint8_t c)
{
    /* Drop control characters except newline — QP3 token data sometimes
       contains stray low bytes we don't want to emit. */
    if (c < ' ' && c != '\n')
        return;

    /* Treat the first character after a player command as sentence-start. */
    if (FirstAfterInput)
        Q3Upper = 1;

    /* If a capital is pending and this is a lowercase letter, uppercase it. */
    if (Q3Upper && c >= 'a') {
        c -= 'a' - 'A';
    }
    OutChar(c);
    /* Any "real" printable character (past space and '!') consumes the
       pending-capital flag; space and '!' deliberately preserve it so a
       sequence like "Hello! World" still capitalizes the W. */
    if (c > '!') {
        Q3Upper = 0;
        Upper = 0;
    }
    /* Sentence-ending punctuation arms the flag for the next letter. */
    if (c == '!' || c == '?' || c == ':' || c == '.') {
        Q3Upper = 1;
    }
}

/* Expand and print token n from the token table. Each byte's low 7 bits
   are the character; the high bit marks the end of the token. */
static void PrintToken(unsigned char n)
{
    /* Blizzard Pass: the long room descriptions are plain characters, and
       the one byte among them that is not its own token is the hyphen of
       "rough-hewn" in room 49. */
    if (CurrentGame == BLIZZARD_PASS && MyLoc == BP_ROOM_CAVERN_WEST_END && n == BP_TOKEN_YOUR) {
        OutChar('-');
        return;
    }
    unsigned char *p = TokenText(n);
    unsigned char c;
    if (p >= EndOfData)
        return;
    do {
        c = *p++;
        if (Version == QUESTPROBE3_TYPE)
            Q3PrintChar(c & TOKEN_BYTE_MASK);
        else
            OutChar(c & TOKEN_BYTE_MASK);
    } while (p < EndOfData && !(c & TOKEN_LAST_BYTE));
}

/* Print the nth text entry from a QP3 text table. Entries are delimited
   by QP3_ENTRY_DELIM (0x1f), with QP3_TABLE_END (0x18) marking end-of-table. Bytes >=
   QP3_TOKEN_BASE (0x7B) are tokens. */
static void Q3PrintText(unsigned char *stream, int text_index)
{
    /* Skip phase: walk past `text_index` entries by jumping to the next
       delimiter each time. The inner loop stops at QP3_ENTRY_DELIM,
       QP3_TABLE_END, or end of buffer. */
    while (text_index > 0 && stream < EndOfData) {
        while (stream < EndOfData && *stream != QP3_ENTRY_DELIM && *stream != QP3_TABLE_END)
            stream++;
        text_index--;
        stream++;
    }
    /* Print phase: emit every byte of the target entry. Bytes below
       QP3_TOKEN_BASE are literal characters; bytes at or above are token
       indices that expand to multi-byte text. Stops at QP3_ENTRY_DELIM
       (post-test below), QP3_TABLE_END, or end of buffer. */
    do {
        if (stream >= EndOfData || *stream == QP3_TABLE_END)
            return;
        if (*stream >= QP3_TOKEN_BASE)
            PrintToken(*stream);
        else
            Q3PrintChar(*stream);
    } while (*stream++ != QP3_ENTRY_DELIM);
}

/* Print the nth text entry (version 1+). Entries are delimited by MSG_END
   (0x7e) or MSG_END_SPACE (0x5E, trailing space). Each byte is a token index. */
static void PrintText1(unsigned char *stream, int text_index)
{
    /* Skip phase: walk past `text_index` entries. Each entry is a run of
       token-index bytes ending in MSG_END or MSG_END_SPACE. Bail out if
       the stream runs out before we've skipped enough — the table is
       shorter than the caller thinks. */
    while (text_index > 0 && stream < EndOfData) {
        while (stream < EndOfData && *stream != MSG_END && *stream != MSG_END_SPACE)
            stream++;
        if (stream >= EndOfData)
            return;
        text_index--;
        stream++;
    }
    /* Print phase: expand each token index until we hit the terminator. */
    while (stream < EndOfData && *stream != MSG_END && *stream != MSG_END_SPACE)
        PrintToken(*stream++);
    /* MSG_END_SPACE marks an entry whose printed form should be followed
       by a space — defer it via PendSpace so the next OutChar emits it. */
    if (stream < EndOfData && *stream == MSG_END_SPACE) {
        PendSpace = 1;
    }
}

/*
 *	Version 0 is different
 */

static int InventoryLower = 0;

/* Print the nth text entry (version 0 / Rebel Planet). End markers
   MSG_END_SPACE (0x5e) and MSG_END (0x7e) are embedded within the token stream rather
   than between token indices, so we must fully expand each token to find
   them. */
static void PrintTextRebelPlanet(unsigned char *stream, int text_index)
{
    if (stream > EndOfData)
        return;
    /* `token` walks through one token's expansion at a time. When NULL,
       we fetch the next token by consuming a byte from `stream`. */
    unsigned char *token = NULL;
    while (stream < EndOfData) {
        if (token == NULL)
            token = TokenText(*stream++);
        /* A token table that the file ends in the middle of */
        if (token == NULL || token >= EndOfData)
            return;
        /* Mask off the high "last-byte" marker to get the character. */
        unsigned char c = *token & TOKEN_BYTE_MASK;
        if (c == MSG_END_SPACE || c == MSG_END) {
            /* Terminator: either we're done with the target entry, or
               this was a divider between earlier entries to skip past. */
            if (text_index == 0) {
                if (c == MSG_END_SPACE)
                    PendSpace = 1;
                return;
            }
            text_index--;
        } else if (text_index == 0) {
            /* Inside the target entry — emit the character.
               Rebel Planet's inventory header is followed by lowercase
               items; InventoryLower flips the first letter and clears. */
            if (InventoryLower) {
                c = tolower(c);
                InventoryLower = 0;
            }
            OutChar(c);
        }
        /* Advance within the current token; the high bit on this byte
           marks the token's last byte, so reset `token` to fetch the
           next one on the following iteration. */
        if (token >= EndOfData || (*token++ & TOKEN_LAST_BYTE))
            token = NULL;
    }
}

/* Dispatch to the correct text printer based on game version. */
static void PrintText(unsigned char *stream, int text_index)
{
    switch (Version) {
    case REBEL_PLANET_TYPE: /* In-stream end markers */
        PrintTextRebelPlanet(stream, text_index);
        break;
    case QUESTPROBE3_TYPE:
        Q3PrintText(stream, text_index);
        break;
    default: /* Out-of-stream end markers (faster) */
        PrintText1(stream, text_index);
        break;
    }
}

/* Print message number m from the primary message table. */
void Message(unsigned char message_index)
{
    unsigned char *stream = FileImage + MessageBase;
    PrintText(stream, message_index);
    /* Most games want a separator space after each message; Blizzard
       Pass handles its own spacing so we skip it there. */
    if (CurrentGame != BLIZZARD_PASS)
        OutChar(' ');
    /* Rebel Planet's message 156 is the "You are carrying" header — the
       inventory list that follows should be lowercased. Reset the flag
       on any other message so a stray 156 doesn't leak into later output. */
    if (BaseGame == REBEL_PLANET && message_index == 156)
        InventoryLower = 1;
    else
        InventoryLower = 0;
}

/* Print message number m from the secondary message table. */
void Message2(unsigned int message_index)
{
    unsigned char *stream = FileImage + Message2Base;
    PrintText(stream, message_index);
    OutChar(' ');
    InventoryLower = 0;
}

/* Questprobe 3 keeps its system messages in the main message table. */
#define Q3_SYSMESSAGE_BASE 210
#define Q3_EXITS_MESSAGE 217

/* Print a system message (inventory prompt, error text, etc.).
   QP3 maps system message IDs to offsets in the main message table. */
void SysMessage(unsigned char message_index)
{
    if (Version == QUESTPROBE3_TYPE) {
        if (message_index == EXITS)
            message_index = Q3_EXITS_MESSAGE;
        else
            message_index = Q3_SYSMESSAGE_BASE + message_index;
    }

    Message(message_index);
}

void PrintObject(unsigned char object_index)
{
    /* Temple of Terror has an object described as "locked door",
       but there is no way to unlock it and you can just walk
       through it anyway. This seems a bit unfair, so we change its
       description to just "door" here. */
    if (BaseGame == TEMPLE_OF_TERROR && object_index == TOT_OBJ_LOCKED_DOOR) {
        OutString("door.");
        return;
    }
    unsigned char *stream = FileImage + ObjectBase;
    if (Version == QUESTPROBE3_TYPE)
        stream--;
    PrintText(stream, object_index);
}

/* Blizzard Pass: the long room descriptions of the text-only mode, and
   how many rooms have one. They are at the start of RAM page 6, which is the
   seventh page of the 128K image (pages 5, 2, 0, 1, 3, 4, 6, 7). */
#define BLIZZARD_LONG_ROOM_TEXT (6 * 0x4000)
#define BLIZZARD_LONG_ROOMS 102

void PrintRoom(unsigned char room_index)
{
    unsigned char *stream = FileImage + RoomBase;
    /* Blizzard Pass allows switching between graphics and short room descriptions,
       or text-only and long descriptions. We always use the long ones.*/
    if (CurrentGame == BLIZZARD_PASS && room_index < BLIZZARD_LONG_ROOMS)
        stream = FileImage + BLIZZARD_LONG_ROOM_TEXT + FileBaselineOffset;
    PrintText(stream, room_index);
}

/* Print a small integer (used by the PRINT action). Zero is printed as
   "00" to match the original game's two-digit display for that case. */
void PrintNumber(unsigned char number)
{
    char buf[4];
    if (number == 0)
        snprintf(buf, sizeof buf, "00");
    else
        snprintf(buf, sizeof buf, "%d", (int)number);
    OutString(buf);
}
