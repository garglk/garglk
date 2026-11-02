//
//  parseinput.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "extracommands.h"
#include "gamedefs.h"
#include "parseinput.h"
#include "taylor.h"
#include "utility.h"

/* Whether two strings are the same apart from the case of their letters. */
int EqualsIgnoringCase(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a++) != toupper((unsigned char)*b++))
            return 0;
    }
    return *a == *b;
}

char **InputWordStrings = NULL; /* Input split into word strings */
uint8_t Word[WORDS_PER_COMMAND]; /* Dictionary codes for the parsed words (0 = not found) */
int FirstWordPosition = 0;     /* Index into InputWordStrings of the first known word */
int KnownWords = 0;            /* Number of dictionary words in the command */
int WordsInInput = 0;          /* Number of word strings in InputWordStrings */
int WordIndex = 0;             /* Position in InputWordStrings of the next command */

int FoundExtraCommand = 0;     /* Set by Parser when a word of the command is not
                                  in the dictionary but is an extra command */

/* Free all allocated word strings and reset input state. */
void FreeInputWords(void)
{
    WordIndex = 0;
    if (InputWordStrings != NULL) {
        for (int i = 0; i < WordsInInput && InputWordStrings[i] != NULL; i++) {
            free(InputWordStrings[i]);
            InputWordStrings[i] = NULL;
        }
        free(InputWordStrings);
        InputWordStrings = NULL;
    }
    WordsInInput = 0;
}

/* Split a raw input string into individual word strings. Words are
   delimited by whitespace; commas, periods, and semicolons also start
   a new word (for command chaining like "GO NORTH,LOOK"). Results are
   stored in the global InputWordStrings array. */
static void SplitIntoWords(const char *string, int length)
{
    if (length < 1) {
        return;
    }

    int startpos[MAX_WORDS];
    int wordlength[MAX_WORDS];

    int words_found = 0;
    int foundcomma = 0;
    int lastwasspace = 1;
    for (int i = 0; string[i] != 0 && i < length; i++) {
        char c = string[i];
        if (c == '.' || c == ',' || c == ';')
            foundcomma = 1;
        if (!isspace((unsigned char)c)) {
            if (lastwasspace || foundcomma) {
                /* Start a new word */
                if (words_found >= MAX_WORDS)
                    break;
                startpos[words_found] = i;
                wordlength[words_found] = 0;
                words_found++;
                foundcomma = 0;
            }
            wordlength[words_found - 1]++;
            lastwasspace = 0;
        } else {
            lastwasspace = 1;
            foundcomma = 0;
        }
    }

    if (words_found == 0) {
        return;
    }

    /* Now we've created two arrays, one for starting positions
     and one for word length. Now we convert these into an array of strings */
    char **words = MemAlloc(words_found * sizeof(*words));

    debug_print("%d words found\n", words_found);
    for (int i = 0; i < words_found; i++) {
        words[i] = (char *)MemAlloc((wordlength[i] + 1));
        memcpy(words[i], string + startpos[i], wordlength[i]);
        words[i][wordlength[i]] = 0;
        debug_print("Word %d: \"%s\"\n", i, words[i]);
    }

    WordsInInput = words_found;
    InputWordStrings = words;
}

/* Read a line of input from the player via Glk, displaying a prompt
   appropriate to the game version. Loops until at least one word is
   entered. Handles non-input events (timer, resize) while waiting. */
static void LineInput(void)
{
    event_t ev;
    char buf[512];
    /* Discard any leftover word state from the previous turn and flush
       pending output so the prompt appears below it. */
    FreeInputWords();
    OutFlush();
    FirstAfterInput = 0;
    while (1) {
        /* Pretend the previous character was a newline so OutChar's
           capitalization logic treats the prompt as a fresh sentence. */
        LastChar = '\n';
        if (Version == QUESTPROBE3_TYPE) {
            /* QP3 prefixes the prompt with the active character's name
               ("THING I want you to" / "HUMAN TORCH I want you to"),
               except in the opening room. */
            if (MyLoc != Q3_ROOM_OPENING) {
                if (IsThing == 0)
                    SysMessage(Q3_MSG_PROMPT_TORCH);
                else
                    SysMessage(Q3_MSG_PROMPT_THING);
            }
            OutCaps();
            SysMessage(Q3_MSG_I_WANT_YOU_TO);
        } else
            OutString("> ");
        OutFlush();
        /* One byte short of the buffer, for the terminator added below */
        glk_request_line_event(Bottom, buf, (glui32)(sizeof buf - 1), 0);

        /* Pump Glk events until the line-input completes. Non-input events
           (timer, resize, etc.) are dispatched to Updates() so the
           interpreter keeps rendering. Transcript is parked across those
           dispatches to keep repeated room descriptions out of the saved log
           when redrawing. */
        while (1) {
            glk_select(&ev);

            if (ev.type == evtype_LineInput)
                break;
            else {
                strid_t ParkedTranscript = Transcript;
                Transcript = NULL;
                Updates(ev);
                Transcript = ParkedTranscript;
            }
        }

        FirstAfterInput = 1;

        int length = ev.val1;
        buf[length] = 0;

        SplitIntoWords(buf, length);

        /* The splitter stops at MAX_WORDS words, so a line that fills
           it was probably cut short: discard it and reprompt. */
        if (WordsInInput >= MAX_WORDS) {
            Display(Bottom, "Too many words!\n");
            FreeInputWords();
        }

        if (WordsInInput > 0 && InputWordStrings) {
            return;
        }

        /* Empty input (whitespace only) — print "Huh?" and reprompt. */
        Display(Bottom, "Huh?");
        FirstAfterInput = 0;
    }
}

/* Single-letter abbreviations and the dictionary words they stand for */
static const struct {
    char letter;
    const char *word;
} Abbreviations[] = {
    { 'I', "INVE" },
    { 'L', "LOOK" },
    { 'X', "EXAM" },
    { 'Z', "WAIT" },
    { 'Q', "QUIT" },
    { 'Y', "YES " },
};

/* The dictionary in the game file ends with the entry "XXXX", code 0xff */
static int IsDictionaryEnd(const unsigned char *entry)
{
    return entry + 5 > EndOfData || (memcmp(entry, "XXXX", 4) == 0 && entry[4] == 0xff);
}

/* Look up a word in the game dictionary. The dictionary is a flat list of
   5-byte entries (4-char word + 1-byte code), see IsDictionaryEnd().
   Returns the dictionary code, or 0 if not found (after also checking
   single-letter abbreviations). */
int ParseWord(const char *word)
{
    char key[5];
    size_t word_len = strlen(word);
    unsigned char *entry = FileImage + VerbBase;

    if (word_len >= 4) {
        memcpy(key, word, 4);
    } else {
        memcpy(key, word, word_len);
        memset(key + word_len, ' ', 4 - word_len);
    }
    key[4] = 0;
    for (int i = 0; i < 4; i++)
        key[i] = (char)toupper((unsigned char)key[i]);

    while (!IsDictionaryEnd(entry)) {
        if (memcmp(entry, key, 4) == 0)
            return entry[4];
        entry += 5;
    }

    if (word_len == 1) {
        for (size_t i = 0; i < sizeof Abbreviations / sizeof Abbreviations[0]; i++) {
            if (Abbreviations[i].letter == key[0])
                return ParseWord(Abbreviations[i].word);
        }
    }
    return 0;
}

/* Words that separate chained commands in a single input line */
static const char *Delimiters[] = { ",", ".", ";", "AND", "THEN", NULL };

/* Check if a word is a command delimiter (case-insensitive). */
static int IsDelimiterWord(const char *word)
{
    for (int i = 0; Delimiters[i] != NULL; i++) {
        if (EqualsIgnoringCase(word, Delimiters[i]))
            return 1;
    }
    return 0;
}

/* Advance WordIndex past the next command delimiter in InputWordStrings.
   Skips consecutive delimiters. Returns 1 if another command follows,
   0 if no more commands remain (triggering a new LineInput). */
static int FindNextCommandDelimiter(void)
{
    if (WordIndex >= WordsInInput - 1 || WordsInInput < 2)
        return 0;
    while (++WordIndex < WordsInInput) {
        if (IsDelimiterWord(InputWordStrings[WordIndex])) {
            WordIndex++;
            while (WordIndex < WordsInInput && IsDelimiterWord(InputWordStrings[WordIndex]))
                WordIndex++;
            if (WordIndex >= WordsInInput)
                return 0;
            else
                return 1;
        }
    }

    return 0;
}

/* The original parser does not keep the words in the order they were
   typed. Each of the first two words it knows goes in the verb or the noun
   slot by its dictionary code: codes below VerbCodeLimit() are verbs, the
   rest nouns. FLAME OUT and OUT FLAME are both OUT FLAME. A later word of
   the same kind replaces the earlier one. Words after the first two go in
   the remaining slots in order. */
static int VerbCodeLimit(void)
{
    switch (BaseGame) {
    case QUESTPROBE3:
        return 50;
    case REBEL_PLANET:
    case BLIZZARD_PASS:
        return 85;
    case HEMAN:
        return 66;
    default:
        return 63;
    }
}

/* Questprobe 3 and Rebel Planet leave codes 16 and 44 where they were
   typed, as the first or second word. */
static int KeepsPosition(int code)
{
    return (BaseGame == QUESTPROBE3 || BaseGame == REBEL_PLANET) && (code == 16 || code == 44);
}

/* Blizzard Pass has nouns that are also verbs. A noun that comes before
   any verb is the verb of the same name instead. */
static int BlizzardPassVerb(int code)
{
    switch (code) {
    case BP_WORD_SKIN:
        return BP_WORD_VERB_SKIN;
    case BP_WORD_POLISH:
        return BP_WORD_VERB_POLISH;
    case BP_WORD_DRAWBRIDGE:
        return BP_WORD_VERB_DRAW;
    default:
        return 0;
    }
}

/* Put a known word in its slot. known is how many known words of the
   command came before it. */
static void PlaceWord(int code, int known)
{
    if (known >= 2) {
        /* Questprobe 3 has room for two of these, the others three */
        if (known < (Version == QUESTPROBE3_TYPE ? 4 : WORDS_PER_COMMAND))
            Word[known] = code;
    } else if (KeepsPosition(code)) {
        Word[known] = code;
    } else if (code < VerbCodeLimit()) {
        Word[0] = code;
    } else if (CurrentGame == BLIZZARD_PASS && Word[0] == 0 && BlizzardPassVerb(code)) {
        Word[0] = BlizzardPassVerb(code);
    } else {
        Word[1] = code;
    }
}

/* Parse the next command from the input. If there are remaining chained
   commands (separated by delimiters), parse the next one; otherwise read
   a new line. Looks up each word in the dictionary and puts the codes in
   the Word[] slots (see PlaceWord()). A chained command without a verb
   keeps the verb of the one before: GET LAMP, SWORD. */
void Parser(void)
{
    FoundExtraCommand = 0;
    uint8_t verb = 0;

    if (!FindNextCommandDelimiter()) {
        LineInput();
        if (WordsInInput == 0 || InputWordStrings == NULL)
            return;
    } else {
        FirstAfterInput = 0;
        OutChar(' ');
        verb = Word[0];
    }

    memset(Word, 0, sizeof Word);
    Word[0] = verb;
    FirstWordPosition = 0;
    KnownWords = 0;

    for (int offset = 0; offset < MAX_WORDS && WordIndex + offset < WordsInInput; offset++) {
        int input_pos = WordIndex + offset;
        int code = ParseWord(InputWordStrings[input_pos]);
        if (code) {
            debug_print("Word %d is %d\n", KnownWords, code);
            if (KnownWords == 0)
                FirstWordPosition = input_pos;
            PlaceWord(code, KnownWords);
            KnownWords++;
        } else {
            if (IsExtraCommand(InputWordStrings[input_pos]))
                FoundExtraCommand = 1;
            if (KnownWords > 0 && IsDelimiterWord(InputWordStrings[input_pos]))
                break;
            debug_print("Word at position %d, %s, was not recognized\n", input_pos, InputWordStrings[input_pos]);
        }
    }

    /* A command of unknown words has no verb */
    if (KnownWords == 0)
        Word[0] = 0;

    if (Version == QUESTPROBE3_TYPE) {
        /* Understand WALL and FIRE as the wall of fire */
        if ((Word[1] == Q3_WORD_WALL || Word[1] == Q3_WORD_FIRE) && (ObjectLoc[Q3_OBJ_WALL_OF_FIRE] == MyLoc || ObjectLoc[Q3_OBJ_WALL_OF_FIRE_2] == MyLoc))
            Word[1] = Q3_WORD_FLAME;
        /* Understand MASTERS as Alicia Masters rather than the
           Ringmaster when she is here */
        if (Word[1] == Q3_WORD_RINGMASTER && ObjectLoc[Q3_OBJ_ALICIA_MASTERS] == MyLoc)
            Word[1] = Q3_WORD_ALICIA;
    }
}
