//
//  parser.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-19.

#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"

#include "bsd.h"

#define MAX_BUFFER 128
#define INPUT_BUFFER_SIZE 512

/* "y.m.c.a." is 8 characters; "Dr."/"Mr." titles are 3 */
#define YMCA_PATTERN_LEN 8
#define TITLE_PATTERN_LEN 3

/* Direction array layout (see EnglishDirections[]):
   indices 1-6 = full names (N/S/E/W/U/D),
   indices 7-12 = single-letter abbreviations,
   index 13 = alternate west (Spanish 'w' for 'oeste'). */
#define DIR_ABBREV_OFFSET 6
#define DIR_WEST 4
#define DIR_ALT_WEST 13
#define NUM_DIRECTION_NOUNS 6

/* Sanity cap for item count in TAKE ALL / DROP ALL */
#define MAX_ITEM_LIMIT 2048

/* TI-99/4A repurposes ASCII form-feed (0x0C) for ö */
#define TI99_O_UMLAUT 12

/* Game-specific verb/noun indices for workarounds */
#define SHERWOOD_REST_VERB 56
#define GERMAN_FALLEN_NOUN 123

/* Tokenized player input: parallel arrays of Unicode and ASCII word strings */
glui32 **UnicodeWords = NULL;
char **CharWords = NULL;
static int WordsInInput = 0;

static int lastnoun = 0; /* Last noun used, for "IT" pronoun resolution */

static glui32 *FirstErrorMessage = NULL; /* Deferred error message (shown after command processing) */

/* Direction words by language — includes both full names and single-letter
   abbreviations. Index 1-6 = full names (N/S/E/W/U/D), 7-12 = abbreviations,
   13 = alternate west ('w' in Spanish is used for 'oeste', and in Italian
   for 'ovest'). */
const char *EnglishDirections[NUM_DIRECTIONS] = {
    NULL, "north", "south", "east", "west", "up", "down",
    "n", "s", "e", "w", "u", "d", " "
};
const char *SpanishDirections[NUM_DIRECTIONS] = {
    NULL, "norte", "sur", "este", "oeste", "arriba", "abajo",
    "n", "s", "e", "o", "u", "d", "w"
};
const char *GermanDirections[NUM_DIRECTIONS] = {
    NULL, "norden", "sueden", "osten", "westen", "oben", "unten",
    "n", "s", "o", "w", "u", "d", " "
};
/* The tape has the letters NSEOSG: S is both sud and su there */
const char *ItalianDirections[NUM_DIRECTIONS] = {
    NULL, "nord", "sud", "est", "ovest", "su", "giu",
    "n", "s", "e", "o", "u", "g", "w"
};

const char *Directions[NUM_DIRECTIONS];

/* Meta-command words recognized by the parser but not in the game's
   dictionary. Includes save/restore, undo, transcript, RAM save/load,
   and command-chain operators (EXCEPT/BUT). The '#' prefix forms are
   for compatibility with interpreters that use '#' as a command prefix.
   The German, Spanish and Italian tables share the English words up to EXCEPT. */
#define COMMON_EXTRA_COMMANDS \
    NULL,                     \
    "restart",                \
    "#restart",               \
    "save",                   \
    "#save",                  \
    "restore",                \
    "load",                   \
    "#restore",               \
    "transcript",             \
    "#transcript",            \
    "script",                 \
    "#script",                \
    "oops",                   \
    "undo",                   \
    "bom",                    \
    "#undo",                  \
    "ram",                    \
    "ramload",                \
    "ramrestore",             \
    "qload",                  \
    "quickload",              \
    "#qload",                 \
    "ramsave",                \
    "qsave",                  \
    "quicksave",              \
    "#qsave"

const char *ExtraCommands[NUM_EXTRA_COMMANDS] = {
    COMMON_EXTRA_COMMANDS,
    "except",
    "but",
    "#flicker",
    "", "", "", "", ""
};

const char *GermanExtraCommands[NUM_EXTRA_COMMANDS] = {
    COMMON_EXTRA_COMMANDS,
    "ausser",
    "bis",
    "#flicker",
    "laden",
    "wiederherstellen",
    "transkript",
    "rueckgaengig",
    "neustarten"
};

const char *SpanishExtraCommands[NUM_EXTRA_COMMANDS] = {
    COMMON_EXTRA_COMMANDS,
    "excepto",
    "menos",
    "#flicker",
    "reanuda",
    "cargar",
    "transcripcion",
    "deshacer",
    "reinicia"
};

const char *ItalianExtraCommands[NUM_EXTRA_COMMANDS] = {
    COMMON_EXTRA_COMMANDS,
    "eccetto",
    "tranne",
    "#flicker",
    "ripristina",
    "carica",
    "trascrizione",
    "annulla",
    "ricomincia"
};

#undef COMMON_EXTRA_COMMANDS

/* Maps each ExtraCommands[] entry to its canonical command enum.
   Multiple strings can map to the same command (e.g. "restore", "load",
   "#restore" all map to RESTORE). */
static const extra_command ExtraCommandsKey[NUM_EXTRA_COMMANDS] = {
    NO_COMMAND, RESTART, RESTART, SAVE, SAVE, RESTORE, RESTORE,
    RESTORE, SCRIPT, SCRIPT, SCRIPT, SCRIPT, UNDO, UNDO, UNDO, UNDO,
    RAM, RAMLOAD, RAMLOAD, RAMLOAD, RAMLOAD, RAMLOAD, RAMSAVE,
    RAMSAVE, RAMSAVE, RAMSAVE, EXCEPT, EXCEPT, FLICKER,
    RESTORE, RESTORE, SCRIPT, UNDO, RESTART
};

/* Extra noun words for meta-commands: "SAVE GAME", "TRANSCRIPT ON/OFF",
   "TAKE ALL", "DROP IT", etc. */
const char *EnglishExtraNouns[NUM_EXTRA_NOUNS] = {
    NULL,
    "game",
    "story",
    "on",
    "off",
    "load",
    "restore",
    "save",
    "move",
    "command",
    "turn",
    "all",
    "everything",
    "it",
    " ",
    " ",
};

const char *GermanExtraNouns[NUM_EXTRA_NOUNS] = {
    NULL, "spiel", "story", "on", "off", "wiederherstellen",
    "laden", "speichern", "move", "verschieben", "runde",
    "alle", "alles", "es", "einschalten", "ausschalten"
};

const char *SpanishExtraNouns[NUM_EXTRA_NOUNS] = {
    NULL, "juego", "story", "on", "off", "cargar",
    "reanuda", "conserva", "move", "command", "jugada",
    "toda", "todo", "eso", "activar", "desactivar"
};

const char *ItalianExtraNouns[NUM_EXTRA_NOUNS] = {
    NULL, "gioco", "storia", "on", "off", "carica",
    "ripristina", "salva", "mossa", "comando", "turno",
    "tutto", "tutti", "esso", "attiva", "disattiva"
};

const char *ExtraNouns[NUM_EXTRA_NOUNS];

const extra_command ExtraNounsKey[NUM_EXTRA_NOUNS] = {
    NO_COMMAND, GAME, GAME, ON, OFF, RAMLOAD,
    RAMLOAD, RAMSAVE, COMMAND, COMMAND, COMMAND,
    ALL, ALL, IT, ON, OFF
};

#define NUM_ABBREVIATIONS 6

/* Single-letter command abbreviations and their expansions */
static const char *Abbreviations[NUM_ABBREVIATIONS] = { NULL, "i", "l",
    "x", "z", "q" };

static const char *const AbbreviationsKey[NUM_ABBREVIATIONS] = {
    NULL, "inventory", "look", "examine", "wait", "quit"
};

/* Filler words silently ignored by the parser (articles, prepositions, adverbs) */
const char *EnglishSkipList[NUM_SKIPPABLE_WORDS] = {
    NULL, "at", "to", "in", "into", "the",
    "a", "an", "my", "quickly", "carefully", "quietly",
    "slowly", "violently", "fast", "hard", "now", "room"
};

const char *GermanSkipList[NUM_SKIPPABLE_WORDS] = {
    NULL, "nach", "die", "der", "das", "im", "mein", "meine", "an",
    "auf", "den", "lassen", "lass", "fallen", "in", "ins", "zur", "zum"
};

/* Not "i", which is the inventory */
const char *ItalianSkipList[NUM_SKIPPABLE_WORDS] = {
    NULL, "il", "lo", "la", "gli", "le", "un", "uno", "una", "a",
    "al", "alla", "in", "nel", "nella", "verso", "mio", "mia"
};

const char *SkipList[NUM_SKIPPABLE_WORDS];

/* Words that separate multiple commands in a single input line */
const char *EnglishDelimiterList[NUM_DELIMITERS] = { NULL, ",", "and",
    "then", " " };

const char *GermanDelimiterList[NUM_DELIMITERS] = { NULL, ",", "und",
    "dann", "and" };

/* Not "e", which is east */
const char *ItalianDelimiterList[NUM_DELIMITERS] = { NULL, ",", "poi",
    "quindi", " " };

const char *DelimiterList[NUM_DELIMITERS];

/* Free all tokenized input strings and the deferred error message */
static void FreeStrings(void)
{
    if (FirstErrorMessage != NULL) {
        free(FirstErrorMessage);
        FirstErrorMessage = NULL;
    }
    if (WordsInInput == 0) {
        if (UnicodeWords != NULL || CharWords != NULL) {
            Fatal("ERROR! Wordcount 0 but word arrays not empty!");
        }
        return;
    }
    for (int i = 0; i < WordsInInput; i++) {
        if (UnicodeWords[i] != NULL)
            free(UnicodeWords[i]);
        if (CharWords[i] != NULL)
            free(CharWords[i]);
    }
    free(UnicodeWords);
    UnicodeWords = NULL;
    free(CharWords);
    CharWords = NULL;
    WordsInInput = 0;
}

/* Append the (possibly NULL) string src to buffer at position length, up to
   MAX_BUFFER characters in all. Returns the new length. */
static int AppendUnicode(glui32 *buffer, int length, const glui32 *src)
{
    for (int i = 0; src != NULL && length < MAX_BUFFER && src[i] != 0; i++)
        buffer[length++] = src[i];
    return length;
}

/* Build a deferred error message from up to three parts (prefix + word + suffix).
   Only the first error per input line is kept; subsequent calls are ignored. */
static void CreateErrorMessage(const char *fchar, glui32 *second, const char *tchar)
{
    if (FirstErrorMessage != NULL)
        return;
    glui32 *first = ToUnicode(fchar);
    glui32 *third = ToUnicode(tchar);
    glui32 buffer[MAX_BUFFER];
    int length = 0;
    length = AppendUnicode(buffer, length, first);
    length = AppendUnicode(buffer, length, second);
    length = AppendUnicode(buffer, length, third);
    FirstErrorMessage = MemAlloc((length + 1) * sizeof(glui32));
    memcpy(FirstErrorMessage, buffer, length * sizeof(glui32));
    FirstErrorMessage[length] = 0;
    free(first);
    free(third);
}

/* Single-byte mapping for bytes >= 0x80. Default: map byte to same codepoint (Latin-1). Overrides below. */
static glui32 MapLatin1(unsigned char b)
{
    return (glui32)b;
}

typedef struct {
    unsigned char byte;
    glui32 codepoint;
} CharMapping;

/* Returns the codepoint b maps to in table, or b itself */
static glui32 MapWithTable(unsigned char b, const CharMapping *table, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (table[i].byte == b)
            return table[i].codepoint;
    return (glui32)b;
}

/* C64 PETSCII codes used by Spanish Gremlins */
static const CharMapping spanish_mapping[] = {
    { 0x83, 0x00BF }, /* ¿ */
    { 0x80, 0x00A1 }, /* ¡ */
    { 0x82, 0x00FC }, /* ü */
    { '{',  0x00E1 }, /* á */
    { '}',  0x00ED }, /* í */
    /* The C64 disk has this one í of its own, in "todavía" */
    { 0x92, 0x00ED }, /* í */
    { '|',  0x00F3 }, /* ó */
    { '~',  0x00F1 }, /* ñ */
    { 0x7f, 0x00E9 }, /* é */
    { 0x81, 0x00FA }, /* ú */
    /* The game has no 0x84 or 0x85; they are the é and ú of the messages
       added in ai_uk/gremlins.c */
    { 0x84, 0x00E9 }, /* é */
    { 0x85, 0x00FA }, /* ú */
};

/* TI-99/4A character codes */
static const CharMapping ti994a_mapping[] = {
    { '@',            0x00A9 }, /* © */
    { '}',            0x00FC }, /* ü */
    { TI99_O_UMLAUT,  0x00F6 }, /* ö */
    { '{',            0x00E4 }, /* ä */
};

/* Map C64 PETSCII codes to Unicode for Spanish Gremlins */
static glui32 MapSpanish(unsigned char b)
{
    return MapWithTable(b, spanish_mapping, sizeof(spanish_mapping) / sizeof(spanish_mapping[0]));
}

/* Map TI-99/4A character codes to Unicode */
static glui32 MapTI994A(unsigned char b)
{
    return MapWithTable(b, ti994a_mapping, sizeof(ti994a_mapping) / sizeof(ti994a_mapping[0]));
}

static int IsGerman(void)
{
    return (CurrentGame == GREMLINS_GERMAN || CurrentGame == GREMLINS_GERMAN_C64);
}

static int IsSpanish(void)
{
    return (CurrentGame == GREMLINS_SPANISH || CurrentGame == GREMLINS_SPANISH_C64);
}

/* Determine mapping function pointer based on CurrentGame */
typedef glui32(*map_fn)(unsigned char);

static map_fn SelectMapper(void)
{
    if (Game && IsSpanish())
        return MapSpanish;
    if (Game && CurrentGame == TI994A)
        return MapTI994A;
    /* default: Latin-1 */
    return MapLatin1;
}

/* German digraphs folded into umlauts. A digraph is left alone when it
   follows not_after (no 'ü' in 'Abenteuer'). */
static const struct {
    glui32 first, second, folded, not_after;
} german_digraphs[] = {
    { 'u', 'e', 0x00FC, 'e' }, /* ü */
    { 'U', 'E', 0x00DC, 0 },   /* Ü */
    { 'U', 'e', 0x00DC, 0 },   /* Ü */
    { 'o', 'e', 0x00F6, 0 },   /* ö */
    { 'a', 'e', 0x00E4, 0 },   /* ä */
};

/* The game text has ss for ß. It is an ß in these words, which the two
   letters before the ss are enough to tell from the others:

     au  außer, außerhalb, draußen
     ra  Straße
     ro  groß, große, Großer
     eu  scheußlicher
     ie  schießt
     ei  weiß, festgeschweißt, zusammengeschweißt

   None of the words that keep their ss (Wasser, Schlüssel, geschlossen,
   verlassen, gerissen, ...) has one of these pairs before it. The words that
   had an ß only before the spelling reform of 1996 (muss, passt, Ablass,
   Netzanschluss, misslungen, Verfasst) are left as they are. */
static int IsEszettPosition(const glui32 *in, size_t i)
{
    return i > 1 &&
        ((in[i - 2] == 'a' && in[i - 1] == 'u') ||
         (in[i - 2] == 'r' && in[i - 1] == 'a') ||
         (in[i - 2] == 'r' && in[i - 1] == 'o') ||
         (in[i - 2] == 'e' && in[i - 1] == 'u') ||
         (in[i - 2] == 'i' && in[i - 1] == 'e') ||
         (in[i - 2] == 'e' && in[i - 1] == 'i'));
}

/* Fold German digraph sequences into proper Unicode characters:
   ue→ü, oe→ö, ae→ä, ss→ß (contextual), and "→'. */
static glui32 *FoldGermanSequences(const glui32 *in, size_t in_len,
                                     size_t *out_len)
{
    /* at most as large as input */
    glui32 *out = MemAlloc((in_len + 1) * sizeof(glui32));
    size_t write_pos = 0;
    for (size_t i = 0; i < in_len; ++i) {
        glui32 cp = in[i];
        /* Not a sequence, just a single character substitution. Also for
           the last character: the error messages are put together from
           pieces, one of which ends with the opening quotation mark. */
        if (cp == '"') {
            out[write_pos++] = 0x2019; /* ’ */
            continue;
        }
        if (i + 1 < in_len) {
            glui32 next = in[i + 1];
            int folded = 0;
            for (size_t d = 0; d < sizeof(german_digraphs) / sizeof(german_digraphs[0]); d++) {
                if (cp == german_digraphs[d].first && next == german_digraphs[d].second &&
                    !(german_digraphs[d].not_after && i > 0 && in[i - 1] == german_digraphs[d].not_after)) {
                    out[write_pos++] = german_digraphs[d].folded;
                    ++i;
                    folded = 1;
                    break;
                }
            }
            if (folded)
                continue;
            if (cp == 's' && next == 's' && IsEszettPosition(in, i)) {
                out[write_pos++] = 0x00DF; /* ß */
                ++i;
                continue;
            }
        }
        out[write_pos++] = cp;
    }
    out[write_pos] = 0;
    *out_len = write_pos;
    return out;
}

/* Convert a byte string to a NUL-terminated Unicode (glui32) string.
   Applies platform-specific character mapping (Latin-1, Spanish PETSCII,
   or TI-99/4A), normalizes line endings, and folds German digraphs
   for German Gremlins variants. */
glui32 *ToUnicode(const char *string)
{
    if (string == NULL)
        return NULL;

    size_t in_len = strlen(string);
    map_fn mapper = SelectMapper();

    size_t cap = in_len + 1;
    glui32 *tmp = MemAlloc(cap * sizeof(glui32));
    size_t out_len = 0;

    for (size_t i = 0; i < in_len; i++) {
        unsigned char b = (unsigned char)string[i];
        glui32 mapped = mapper(b);

        /* Normalize any carriage returns to line feeds */
        if (mapped == '\r')
            mapped = '\n';

        /* Special handling: We want the copyright symbol */
        /* to be followed by a space in TI-99/4A games.   */
        if (mapper == MapTI994A && mapped == 0x00A9 && out_len < cap) {
            tmp[out_len++] = mapped;
            mapped = ' ';
        }

        if (out_len + 1 >= cap) {
            cap = cap * 2 + 8;
            tmp = MemRealloc(tmp, cap * sizeof(glui32));
        }
        tmp[out_len++] = mapped;
    }

    if (out_len + 1 >= cap) {
        cap = out_len + 2;
        tmp = MemRealloc(tmp, cap * sizeof(glui32));
    }
    tmp[out_len] = 0;

    /* If the current game is German, do sequence folding */
    if (Game && IsGerman()) {
        size_t folded_len;
        glui32 *folded = FoldGermanSequences(tmp, out_len, &folded_len);
        free(tmp);
        tmp = folded;
        out_len = folded_len;
    }

    tmp = MemRealloc(tmp, (out_len + 1) * sizeof(glui32));
    return tmp;
}

/* ASCII spellings of the diacritical characters the games use */
static const struct {
    glui32 codepoint;
    const char *ascii;
} diacritic_spellings[] = {
    { 0xf6, "oe" }, /* ö */
    { 0xe4, "ae" }, /* ä */
    { 0xdf, "ss" }, /* ß */
    { 0xed, "i" },  /* í */
    { 0xe1, "a" },  /* á */
    { 0xf3, "o" },  /* ó */
    { 0xf1, "n" },  /* ñ */
    { 0xe9, "e" },  /* é */
};

/* Returns the ASCII spelling of diacritical character c, or NULL */
static const char *AsciiForDiacritic(glui32 c)
{
    for (size_t i = 0; i < sizeof(diacritic_spellings) / sizeof(diacritic_spellings[0]); i++)
        if (diacritic_spellings[i].codepoint == c)
            return diacritic_spellings[i].ascii;
    return NULL;
}

/* Convert a Unicode string back to ASCII for dictionary matching.
   Diacritical characters are replaced with their base-letter equivalents
   (ö→oe, ä→ae, ü→ue/u, ß→ss, á→a, etc.). Lone punctuation (.,;) is
   converted to "and" as a command delimiter. */
static char *FromUnicode(glui32 *unicode_string, int origlength)
{
    int destpos = 0;

    char *dest = MemAlloc(MAX_WORDLENGTH);
    for (int i = 0; i < origlength && destpos + 3 < MAX_WORDLENGTH; i++) {
        glui32 unichar = unicode_string[i];
        if (unichar == 0)
            break;
        const char *replacement = NULL;
        if ((unichar == '.' || unichar == ',' || unichar == ';') && origlength == 1)
            replacement = "and";
        else if (unichar == 0xfc) // ü
            replacement = IsGerman() ? "ue" : "u";
        else
            replacement = AsciiForDiacritic(unichar);

        if (replacement) {
            for (const char *r = replacement; *r; r++)
                dest[destpos++] = *r;
        } else {
            dest[destpos++] = (char)unichar;
        }
    }
    if (destpos == 0) {
        free(dest);
        return NULL;
    }
    dest = MemRealloc(dest, destpos + 1);
    dest[destpos] = 0;
    return dest;
}

/* Check if the string at the given index starts with "y.m.c.a." (the Gremlins
   item). Returns the number of characters matched (8 on full match). */
static int MatchYMCA(glui32 *string, int length, int index)
{
    const char *ymca = "y.m.c.a.";
    int i;
    for (i = 0; i < YMCA_PATTERN_LEN; i++) {
        if (i + index >= length || string[index + i] != (unsigned char)ymca[i])
            return i;
    }
    return i;
}

/* Check if the string at the given index starts with "mr." or "dr.".
   Returns TITLE_PATTERN_LEN (3) on a match, 0 otherwise. */
static int MatchTitleWithPeriod(glui32 *string, int length, int index)
{
    if (length - index >= TITLE_PATTERN_LEN) {
        if ((string[index] == 'm' || string[index] == 'd') &&
            string[index + 1] == 'r' &&
            string[index + 2] == '.')
            return TITLE_PATTERN_LEN;
    }
    return 0;
}

/* Whitespace, punctuation treated as whitespace, and Unicode spaces */
static int IsSpaceChar(glui32 c)
{
    switch (c) {
    case ' ':
    case '\t':
    case '!':
    case '?':
    case '\"':
    case 0x83:   // ¿
    case 0x80:   // ¡
    case 0xa0:   // non-breaking space
    case 0x2000: // en quad
    case 0x2001: // em quad
    case 0x2003: // em space
    case 0x2004: // three-per-em space
    case 0x2005: // four-per-em space
    case 0x2006: // six-per-em space
    case 0x2007: // figure space
    case 0x2009: // thin space
    case 0x200A: // hair space
    case 0x202f: // narrow no-break space
    case 0x205f: // medium mathematical space
    case 0x3000: // ideographic space
        return 1;
    default:
        return 0;
    }
}

/* Punctuation delimiters, emitted as their own one-character tokens */
static int IsDelimiterChar(glui32 c)
{
    return (c == '.' || c == ',' || c == ';');
}

/* Start positions and lengths of the words found in an input line */
typedef struct {
    int start[MAX_WORDS];
    int length[MAX_WORDS];
    int count;
} WordSpans;

/* Record a word starting at start. Returns 0 if MAX_WORDS are already found. */
static int AddWordSpan(WordSpans *spans, int start, int length)
{
    if (spans->count >= MAX_WORDS)
        return 0;
    spans->start[spans->count] = start;
    spans->length[spans->count] = length;
    spans->count++;
    return 1;
}

/* Tokenize a Unicode input string into words.
   Lowercases the input, coalesces whitespace, splits on spaces and
   various Unicode space variants, and emits commas/periods/semicolons
   as single-character delimiter tokens (matched by DelimiterList).
   Produces parallel UnicodeWords[] and CharWords[] arrays.
   Special-cases "y.m.c.a." (Gremlins) and "Dr."/"Mr." titles
   to keep them as single tokens despite the embedded periods. */
static void SplitIntoWords(glui32 *string, int length)
{
    if (length < 1)
        return;

    glk_buffer_to_lower_case_uni(string, INPUT_BUFFER_SIZE, MIN(length, INPUT_BUFFER_SIZE));
    glk_buffer_canon_normalize_uni(string, INPUT_BUFFER_SIZE, MIN(length, INPUT_BUFFER_SIZE));

    WordSpans spans = { .count = 0 };
    int lastwasspace = 1;

    for (int i = 0; string[i] != 0 && i < length; i++) {
        glui32 c = string[i];

        if (c == 'd' || c == 'm' || c == 'y') {
            /* Keep "y.m.c.a." and "Dr."/"Mr." as single tokens */
            int title_len = (c == 'y') ? 0 : MatchTitleWithPeriod(string, length, i);
            int match_len = MatchYMCA(string, length, i);
            if (title_len > match_len)
                match_len = title_len;
            if (match_len > YMCA_PATTERN_LEN / 2 || title_len > TITLE_PATTERN_LEN - 1) {
                if (!AddWordSpan(&spans, i, match_len))
                    break;
                i += match_len - 1; /* -1: the for-loop increment adds 1 */
                lastwasspace = 1;
                continue;
            }
        }

        if (IsSpaceChar(c)) {
            lastwasspace = 1;
        } else if (IsDelimiterChar(c)) {
            /* Emit the delimiter character as a one-character word token
               so downstream parsing can match it in DelimiterList */
            if (!AddWordSpan(&spans, i, 1))
                break;
            lastwasspace = 1;
        } else {
            if (lastwasspace && !AddWordSpan(&spans, i, 0))
                break;
            spans.length[spans.count - 1]++;
            lastwasspace = 0;
        }
    }

    int words_found = spans.count;
    if (words_found == 0)
        return;

    /* Convert start-position/length pairs into allocated string arrays */
    glui32 **words = MemAlloc(words_found * sizeof(*words));
    char **words8 = MemAlloc(words_found * sizeof(*words8));

    for (int i = 0; i < words_found; i++) {
        int len = spans.length[i];
        words[i] = (glui32 *)MemAlloc((len + 1) * sizeof(glui32));
        memcpy(words[i], string + spans.start[i], len * sizeof(glui32));
        words[i][len] = 0;
        words8[i] = FromUnicode(words[i], len);
    }
    UnicodeWords = words;
    WordsInInput = words_found;
    CharWords = words8;
}

/* Prompt the player and read a line of input via Glk.
   Loops until at least one word is recognized. */
static void LineInput(void)
{
    event_t ev;
    glui32 unibuf[INPUT_BUFFER_SIZE];

    for (;;) {
        Display(Bottom, "\n%s", sys[WHAT_NOW]);
        glk_request_line_event_uni(Bottom, unibuf, (glui32)(INPUT_BUFFER_SIZE - 1), 0);

        do {
            glk_select(&ev);
            if (ev.type != evtype_LineInput)
                Updates(ev);
        } while (ev.type != evtype_LineInput);

        unibuf[ev.val1] = 0;
        lastwasnewline = 1;

        SplitIntoWords(unibuf, ev.val1);

        if (WordsInInput != 0 && CharWords != NULL)
            return;

        Output(sys[HUH]);
    }
}

/* Search a word list for a match, comparing up to word_length characters.
   Synonym entries (prefixed with '*') share the index of the preceding
   canonical entry. Returns the matched index, or 0 if not found. */
int WhichWord(const char *word, const char **list, int word_length,
              int list_length)
{
    int n = 1;
    for (int ne = 1; ne < list_length; ne++) {
        const char *tp = list[ne];
        if (!tp)
            continue;
        if (*tp == '*')
            tp++;
        else
            n = ne;
        if (xstrncasecmp(word, tp, word_length) == 0)
            return n;
    }
    return 0;
}

/* List identifiers for static spec tables
 * (so tables can be const). */
typedef enum {
    L_VERBS,
    L_DIRECTIONS,
    L_ABBREVS,
    L_SKIP,
    L_NOUNS,
    L_EXTRACMD,
    L_EXTRANOUN,
    L_DELIM
} ListId;

typedef struct {
    ListId list_id;
    int use_header_word_length; /* 1 => use
                                   GameHeader.WordLength, 0 =>
                                   use strlen(word) */
} SearchSpec;

/* Map ListId to actual pointers and lengths (lengths when
 * dynamic are provided at call) */
static int GetListAndLength(ListId id, const char ***plist)
{
    switch (id) {
        case L_VERBS:
            *plist = Verbs;
            return GameHeader.NumWords + 1;
        case L_DIRECTIONS:
            *plist = Directions;
            return NUM_DIRECTIONS;
        case L_ABBREVS:
            *plist = Abbreviations;
            return NUM_ABBREVIATIONS;
        case L_SKIP:
            *plist = SkipList;
            return NUM_SKIPPABLE_WORDS;
        case L_NOUNS:
            *plist = Nouns;
            return GameHeader.NumWords + 1;
        case L_EXTRACMD:
            *plist = ExtraCommands;
            return NUM_EXTRA_COMMANDS;
        case L_EXTRANOUN:
            *plist = ExtraNouns;
            return NUM_EXTRA_NOUNS;
        case L_DELIM:
            *plist = DelimiterList;
            return NUM_DELIMITERS;
        default:
            *plist = NULL;
            return 0;
    }
}

/* Static const search orders to reduce wrapper code repetition
 */
static const SearchSpec verb_search_order[] = {
    {L_VERBS, 1}, {L_DIRECTIONS, 1}, {L_ABBREVS, 1},   {L_SKIP, 0},
    {L_NOUNS, 1}, {L_EXTRACMD, 0},   {L_EXTRANOUN, 0}, {L_DELIM, 0}};

/* The filler words come before the extra nouns: those two lists are matched
   on as many letters as were typed, and "a" is also how "all" begins. */
static const SearchSpec noun_search_order[] = {
    {L_NOUNS, 1}, {L_DIRECTIONS, 1}, {L_SKIP, 0},
    {L_EXTRANOUN, 0}, {L_VERBS, 1},  {L_DELIM, 0}};

/* Unified search routine used by FindVerb() and FindNoun().
 * - order: array of SearchSpec describing priority order to
 * try.
 * - out_list: returns the matched list pointer (may be set to
 * NULL if no match).
 *
 * Return values:
 * - For normal matches: index value returned by WhichWord().
 * - For directions: direction index.
 * - For mapped extra commands/nouns: (mapped_value +
 * GameHeader.NumWords).
 * - For skip list matches: 0.
 * - If nothing matched: 0 and *out_list = NULL.
 */
static int FindVerbOrNoun(const char *word, const SearchSpec *order, int list_size,
                          const char ***out_list)
{
    *out_list = NULL;

    for (int s = 0; s < list_size; s++) {
        const SearchSpec *spec = &order[s];
        const char **list = NULL;
        int list_length = GetListAndLength(spec->list_id, &list);
        if (!list || list_length <= 1)
            continue;
        *out_list = list;
        int match_len = spec->use_header_word_length ? GameHeader.WordLength
                                                     : (int)strlen(word);
        int idx = WhichWord(word, list, match_len, list_length);
        if (!idx)
            continue;

        switch (spec->list_id) {
            case L_VERBS:
            case L_NOUNS:
            case L_DELIM:
                return idx;

            case L_DIRECTIONS:
                /* Convert dictionary word index to direction index */
                if (idx == DIR_ALT_WEST)
                    idx = DIR_WEST;
                if (idx > NUM_DIRECTION_NOUNS)
                    idx -= DIR_ABBREV_OFFSET;
                return idx;

            case L_ABBREVS: {
                if (idx > 0 && idx < NUM_ABBREVIATIONS && AbbreviationsKey[idx]) {
                    idx =
                    WhichWord(AbbreviationsKey[idx], Verbs,
                              GameHeader.WordLength, GameHeader.NumWords + 1);
                    if (idx) {
                        *out_list = Verbs;
                        return idx;
                    }
                }
                break; /* fall through to next spec if abbrev didn't
                        map */
            }

            case L_SKIP:
                return 0;

            case L_EXTRACMD:
                if (idx > 0 && idx < NUM_EXTRA_COMMANDS && ExtraCommandsKey[idx])
                    return ExtraCommandsKey[idx] + GameHeader.NumWords;
                break;

            case L_EXTRANOUN:
                if (idx > 0 && idx < NUM_EXTRA_NOUNS && ExtraNounsKey[idx])
                    return ExtraNounsKey[idx] + GameHeader.NumWords;
                break;
        }
    }

    *out_list = NULL;
    return 0;
}

static int FindVerb(const char *string, const char ***list)
{
    return FindVerbOrNoun(string, verb_search_order, sizeof(verb_search_order) / sizeof(verb_search_order[0]), list);
}

static int FindNoun(const char *string, const char ***list)
{
    return FindVerbOrNoun(string, noun_search_order, sizeof(noun_search_order) / sizeof(noun_search_order[0]), list);
}

static Command *CommandFromStrings(int index, Command *previous);

static int IsExceptWord(int i)
{
    int except = WhichWord(CharWords[i], ExtraCommands, strlen(CharWords[i]),
        NUM_EXTRA_COMMANDS);
    return (ExtraCommandsKey[except] == EXCEPT);
}

/* Whatever follows the verb and its noun is ignored, as the original
   interpreters do: LOOK UP AT THE GREMLIN is LOOK UP, and GET FLASHLIGHT
   AT THE XYZZY takes the flashlight. Returns the index of the word where
   the next command begins: a delimiter, the end of the input or, after
   ALL, the word EXCEPT. */
static int SkipTrailingWords(int index, int after_all)
{
    while (index < WordsInInput) {
        const char **list = NULL;
        FindVerb(CharWords[index], &list);
        if (list == DelimiterList || (after_all && IsExceptWord(index)))
            break;
        index++;
    }
    return index;
}

/* Allocate a Command node and recursively parse any remaining words
   into a linked list of subsequent commands. */
static Command *CreateCommandStruct(int verb, int noun, int verbindex,
    int nounindex, int nextindex, Command *previous)
{
    Command *command = MemAlloc(sizeof(Command));
    command->verb = verb;
    command->noun = noun;
    command->allflag = 0;
    command->item = 0;
    command->previous = previous;
    command->verbwordindex = verbindex;
    if (noun && nounindex > 0) {
        command->nounwordindex = nounindex - 1;
    } else {
        command->nounwordindex = 0;
    }
    command->next = CommandFromStrings(nextindex, command);
    return command;
}

static int IsNounList(const char **list)
{
    return (list == Nouns || list == Directions || list == ExtraNouns);
}

static void DontKnowHowTo(int wordindex)
{
    CreateErrorMessage(sys[I_DONT_KNOW_HOW_TO], UnicodeWords[wordindex],
        sys[SOMETHING]);
}

/* A direction word at `i - 1` becomes GO + direction number. */
static Command *DirectionCommand(int direction, int i, Command *previous)
{
    if (direction == 0)
        return NULL;
    /* German has the verb last: "nach unten gehen" */
    if (i < WordsInInput && (CurrentGame == GREMLINS_GERMAN || CurrentGame == GREMLINS_GERMAN_C64)) {
        const char **verblist = NULL;
        int go = FindVerb(CharWords[i], &verblist);
        if (verblist == Verbs) {
            while (go > 1 && Verbs[go][0] == '*')
                go--;
            if (go == GO)
                i++;
        }
    }
    return CreateCommandStruct(GO, direction, 0, i, SkipTrailingWords(i, 0), previous);
}

/* Parse tokenized words starting at `index` into a Command node.
   Tries to identify a verb, then a noun, handling special cases:
   - Directions become GO + direction_number
   - German word order (noun before verb)
   - Verb inheritance from the previous command in a chain
   - Delimiter words start a new command
   Returns NULL if the words can't form a valid command. */
static Command *CommandFromStrings(int index, Command *previous)
{
    if (index < 0 || index >= WordsInInput) {
        return NULL;
    }
    const char **list = NULL;
    int verb = 0;
    int i = index;

    do {
        /* Checking if it is a verb */
        verb = FindVerb(CharWords[i++], &list);
    } while ((list == SkipList || list == DelimiterList) && i < WordsInInput);

    int verbindex = i - 1;

    if (list == Directions)
        return DirectionCommand(verb, i, previous);

    int found_noun_at_verb_position = 0;
    int lastverb = 0;

    if (IsNounList(list)) {
        /* It is a noun */
        /* If we find no verb, we try copying the verb from the previous command */
        if (previous) {
            lastverb = previous->verb;
        }
        /* Unless the game is German, where we allow the noun to come before the verb */
        if (!IsGerman()) {
            if (!previous) {
                DontKnowHowTo(i - 1);
                return NULL;
            } else {
                verbindex = previous->verbwordindex;
            }
            int after_all = (list == ExtraNouns && verb - GameHeader.NumWords == ALL);
            return CreateCommandStruct(lastverb, verb, verbindex, i,
                SkipTrailingWords(i, after_all), previous);
        } else {
            found_noun_at_verb_position = 1;
        }
    }

    if (list == NULL || list == SkipList) {
        DontKnowHowTo(i - 1);
        return NULL;
    }

    if (i == WordsInInput) {
        if (lastverb) {
            return CreateCommandStruct(lastverb, verb, previous->verbwordindex, i,
                i, previous);
        } else if (found_noun_at_verb_position) {
            DontKnowHowTo(i - 1);
            return NULL;
        } else {
            return CreateCommandStruct(verb, 0, i - 1, i, i, previous);
        }
    }

    int noun = 0;

    do {
        /* Check if it is a noun */
        noun = FindNoun(CharWords[i++], &list);
    } while (list == SkipList && i < WordsInInput);

    if (IsNounList(list)) {
        /* It is a noun */

        int after_all = (list == ExtraNouns && noun - GameHeader.NumWords == ALL);
        /* If we found a noun where a verb was expected, check
           again to see if it matches a verb as well */
        if (found_noun_at_verb_position) {
            int realverb = WhichWord(CharWords[i - 1], Verbs, GameHeader.WordLength,
                GameHeader.NumWords + 1);
            if (realverb) {
                noun = verb;
                verb = realverb;
            } else if (lastverb) {
                noun = verb;
                verb = lastverb;
            }
        }
        return CreateCommandStruct(verb, noun, verbindex, i,
            SkipTrailingWords(i, after_all), previous);
    }

    if (list == DelimiterList) {
        /* It is a delimiter */
        return CreateCommandStruct(verb, 0, verbindex, i, i, previous);
    }

    if (list == Verbs && found_noun_at_verb_position) {
        /* It is a verb */
        int after_all = (verb - GameHeader.NumWords == ALL);
        return CreateCommandStruct(noun, verb, i - 1, i,
            SkipTrailingWords(i, after_all), previous);
    }

    CreateErrorMessage(sys[I_DONT_KNOW_WHAT_A], UnicodeWords[i - 1], sys[IS]);
    return NULL;
}

/* Expand a TAKE ALL or DROP ALL command into a linked list of individual
   commands, one per eligible item. Handles EXCEPT/BUT exclusions by
   scanning the following command nodes. Items with AutoGet starting
   with '*' (treasures) are excluded from ALL. Returns 0 if no items
   matched (with an appropriate error message). */
static int CreateAllCommands(Command *command)
{
    if (GameHeader.NumItems > MAX_ITEM_LIMIT)
        Fatal("Bad number of items");
    int exceptions[MAX_ITEM_LIMIT];
    int exceptioncount = 0;

    int location = TakeOrDropSource(command->verb);

    Command *next = command->next;
    /* Check if the ALL command is followed by EXCEPT */
    while (next && next->verb == GameHeader.NumWords + EXCEPT) {
        const char *word = CharWords[next->nounwordindex];
        /* The item's own word, or a synonym of it */
        int noun = WhichWord(word, Nouns, GameHeader.WordLength,
            GameHeader.NumWords + 1);
        /* ALL EXCEPT IT */
        if (next->noun == GameHeader.NumWords + IT)
            noun = lastnoun;
        for (int i = 0; i <= GameHeader.NumItems; i++) {
            if (exceptioncount >= MAX_ITEM_LIMIT || !Items[i].AutoGet)
                continue;
            if (xstrncasecmp(Items[i].AutoGet, word, GameHeader.WordLength) == 0
                || (noun && noun == WhichWord(Items[i].AutoGet, Nouns, GameHeader.WordLength, GameHeader.NumWords + 1))) {
                exceptions[exceptioncount++] = i;
            }
        }
        /* Remove the EXCEPT command from the linked list of commands */
        next = next->next;
        free(command->next);
        command->next = next;
    }

    Command *c = command;
    int found = 0;
    /* Items are indexed 0..NumItems inclusive; <= so TAKE ALL / DROP ALL can
       reach the last item in the database (the EXCEPT loop above and every
       other item loop already use <=). */
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].AutoGet != NULL && Items[i].AutoGet[0] != '*' && Items[i].Location == location) {
            int exception = 0;
            for (int j = 0; j < exceptioncount; j++) {
                if (exceptions[j] == i) {
                    exception = 1;
                    break;
                }
            }
            if (!exception) {
                if (found) {
                    c->next = MemAlloc(sizeof(Command));
                    c->next->previous = c;
                    c = c->next;
                }
                found = 1;
                c->verb = command->verb;
                /* The dictionary holds NumWords + 1 entries and WhichWord stops
                   at list_length - 1, so passing NumWords here would make the
                   last dictionary word unmatchable, leaving noun = 0 for an
                   item whose AutoGet word is that entry. */
                c->noun = WhichWord(Items[i].AutoGet, Nouns, GameHeader.WordLength,
                    GameHeader.NumWords + 1);
                c->item = i;
                c->next = NULL;
                c->nounwordindex = 0;
                c->allflag = 1;
            }
        }
    }
    if (found == 0) {
        if (command->verb == TAKE)
            CreateErrorMessage(sys[NOTHING_HERE_TO_TAKE], NULL, NULL);
        else
            CreateErrorMessage(sys[YOU_HAVE_NOTHING], NULL, NULL);
        return 0;
    } else {
        c->next = next;
        c->allflag = 1 | LASTALL;
    }
    return 1;
}

/* Free the entire command chain (rewind to head, then free forward) and
   release all associated tokenized input strings. */
void FreeCommands(void)
{
    while (CurrentCommand && CurrentCommand->previous)
        CurrentCommand = CurrentCommand->previous;
    while (CurrentCommand) {
        struct Command *temp = CurrentCommand;
        CurrentCommand = CurrentCommand->next;
        free(temp);
    }
    CurrentCommand = NULL;
    FreeStrings(); /* also frees FirstErrorMessage */
}

static void PrintPendingError(void)
{
    if (FirstErrorMessage) {
        glk_put_string_stream_uni(glk_window_get_stream(Bottom), FirstErrorMessage);
        free(FirstErrorMessage);
        FirstErrorMessage = NULL;
        StopTime = 1;
    }
}

/* Get the next verb/noun pair for the game engine to process.

   If commands remain in the current chain, advance to the next one.
   Otherwise, prompt for new input and parse it into a command chain.
   Handles meta-commands (verb > NumWords), ALL expansion, IT pronoun
   resolution, and game-specific fixups (German "ALLE FALLEN LASSEN",
   Robin of Sherwood "RESTORE"/"RESTART" vs "REST").

   Returns 0 with vb/no set on success, or 1 to signal the main loop
   should re-run without executing actions (meta-command handled or error). */
int GetInput(int *vb, int *no)
{
    if (CurrentCommand && CurrentCommand->next) {
        CurrentCommand = CurrentCommand->next;
    } else {
        PrintPendingError();
        if (CurrentCommand)
            FreeCommands();
        LineInput();
        CurrentCommand = CommandFromStrings(0, NULL);
    }

    if (CurrentCommand == NULL) {
        PrintPendingError();
        return 1;
    }

    /* Hack to make ALLE FALLEN LASSEN work in German Gremlins */
    /* The normal verb <-> noun switching mechanism gets confused
       by the fact that the game lists FALLEN and LASSEN as both
       verbs and nouns. */

    if (IsGerman() && CurrentCommand->verb - GameHeader.NumWords == ALL && CurrentCommand->noun == GERMAN_FALLEN_NOUN) {
        CurrentCommand->verb = DROP;
        CurrentCommand->noun = ALL + GameHeader.NumWords;
    }

    /* Hack to make RESTORE and RESTART work in Robin of Sherwood */
    /* instead of being understood as REST, a synonym of WAIT.     */
    if (Game->type == SHERWOOD_VARIANT && CurrentCommand->verb == SHERWOOD_REST_VERB) {
        char *verbword = CharWords[CurrentCommand->verbwordindex];
        if (xstrncasecmp(verbword, "restore", strlen("restore")) == 0)
            CurrentCommand->verb = GameHeader.NumWords + RESTORE;
        else if (xstrncasecmp(verbword, "restart", strlen("restart")) == 0)
            CurrentCommand->verb = GameHeader.NumWords + RESTART;
    }

    /* We use NumWords + verb for our extra commands */
    /* such as UNDO and TRANSCRIPT */
    if (CurrentCommand->verb > GameHeader.NumWords) {
        if (!PerformExtraCommand(0)) {
            CreateErrorMessage(sys[I_DONT_UNDERSTAND], NULL, NULL);
        }
        return 1;
        /* And NumWords + noun for our extra nouns */
        /* such as ALL */
    } else if (CurrentCommand->noun > GameHeader.NumWords) {
        CurrentCommand->noun -= GameHeader.NumWords;
        if (CurrentCommand->noun == ALL) {
            if (CurrentCommand->verb != TAKE && CurrentCommand->verb != DROP) {
                CreateErrorMessage(sys[CANT_USE_ALL], NULL, NULL);
                return 1;
            }
            if (!CreateAllCommands(CurrentCommand))
                return 1;
        } else if (CurrentCommand->noun == IT) {
            CurrentCommand->noun = lastnoun;
        }
    }

    *vb = CurrentCommand->verb;
    *no = CurrentCommand->noun;

    if (*no > NUM_DIRECTION_NOUNS) {
        lastnoun = *no;
    }

    return 0;
}

/* Look up input word wordindex among the extra nouns (GAME, ON, ALL...).
   Returns its extra_command value, or NO_COMMAND if it isn't one. */
int FindExtraNoun(int wordindex)
{
    const char *NounWord = CharWords[wordindex];
    return ExtraNounsKey[WhichWord(NounWord, ExtraNouns, strlen(NounWord),
        NUM_EXTRA_NOUNS)];
}

/* Re-check a command that the action table didn't recognize, looking for
   meta-commands (save, undo, etc.) that might have been masked by a
   game dictionary word with the same prefix. */
int RecheckForExtraCommand(void)
{
    const char *VerbWord = CharWords[CurrentCommand->verbwordindex];

    int ExtraVerb = WhichWord(VerbWord, ExtraCommands, GameHeader.WordLength,
        NUM_EXTRA_COMMANDS);
    if (!ExtraVerb) {
        return 0;
    }
    int ExtraNoun = NO_COMMAND;
    if (CurrentCommand->noun)
        ExtraNoun = FindExtraNoun(CurrentCommand->nounwordindex);
    CurrentCommand->verb = ExtraCommandsKey[ExtraVerb];
    if (ExtraNoun)
        CurrentCommand->noun = ExtraNoun;

    return PerformExtraCommand(1);
}
