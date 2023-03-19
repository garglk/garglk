//
//  parseinput.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "extracommands.h"
#include "parseinput.h"
#include "taylor.h"
#include "utility.h"

// Input separated into word strings
char **InputWordStrings = NULL;
// The indices of the words in InputWordStrings, 0 if not found
uint8_t Word[5];
// The positions in InputWordStrings of the words in Word
int WordPositions[5];
// The number of word strings in InputWordStrings
int WordsInInput = 0;
// The index in InputWordStrings of the next command
int WordIndex = 0;

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

void SplitIntoWords(const char *string, int length)
{
    if (length < 1) {
        return;
    }

    int startpos[MAX_WORDS];
    int wordlength[MAX_WORDS];

    int words_found = 0;
    int word_index = 0;
    int foundcomma = 0;
    startpos[0] = 0;
    wordlength[0] = 0;
    int lastwasspace = 1;
    for (int i = 0; string[i] != 0 && i < length && word_index < MAX_WORDS; i++) {
        char c = string[i];
        if (c == '.' || c == ',' || c == ';')
            foundcomma = 1;
        if (!isspace(c)) {
            if (lastwasspace || foundcomma) {
                /* Start a new word */
                startpos[words_found] = i;
                words_found++;
                wordlength[words_found] = 0;
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

    wordlength[words_found]--; /* Don't count final newline character */

    /* Now we've created two arrays, one for starting postions
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

void LineInput(void)
{
    event_t ev;
    char buf[512];
    FreeInputWords();
    OutFlush();
    FirstAfterInput = 0;
    LastVerb = 0;
    do {
        LastChar = '\n';
        if (Version == QUESTPROBE3_TYPE) {
            if (MyLoc != 6) {
                if (IsThing == 0)
                    SysMessage(8);
                else
                    SysMessage(9);
            }
            OutCaps();
            SysMessage(10);
        } else
            OutString("> ");
        OutFlush();
        glk_request_line_event(Bottom, buf, (glui32)511, 0);

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

        if (Transcript) {
            glk_put_string_stream(Transcript, buf);
            glk_put_string_stream(Transcript, "\n");
        }

        SplitIntoWords(buf, length);

        if (WordsInInput >= MAX_WORDS) {
            Display(Bottom, "Too many words!\n");
            FreeInputWords();
        }

        if (WordsInInput > 0 && InputWordStrings) {
            return;
        }

        WordsInInput = 0;
        Display(Bottom, "Huh?");
        FirstAfterInput = 0;

    } while (WordsInInput == 0 || InputWordStrings == NULL);
}

static const char *Abbreviations[] = { "I   ", "L   ", "X   ", "Z   ", "Q   ", "Y   ", NULL };
static const char *AbbreviationValue[] = { "INVE", "LOOK", "EXAM", "WAIT", "QUIT", "YES ", NULL };

int ParseWord(char *p)
{
    char buf[5];
    size_t len = strlen(p);
    unsigned char *words = FileImage + VerbBase;
    int i;

    if (len >= 4) {
        memcpy(buf, p, 4);
        buf[4] = 0;
    } else {
        memcpy(buf, p, len);
        memset(buf + len, ' ', 4 - len);
    }
    for (i = 0; i < 4; i++) {
        if (buf[i] == 0)
            break;
        if (islower(buf[i]))
            buf[i] = toupper(buf[i]);
    }
    while (*words != 126) {
        if (memcmp(words, buf, 4) == 0)
            return words[4];
        words += 5;
    }

    words = FileImage + VerbBase;
    for (i = 0; Abbreviations[i] != NULL; i++) {
        if (memcmp(Abbreviations[i], buf, 4) == 0) {
            while (*words != 126) {
                if (memcmp(words, AbbreviationValue[i], 4) == 0)
                    return words[4];
                words += 5;
            }
        }
    }
    return 0;
}

static const char *Delimiters[] = { ",", ".", ";", "AND", "THEN", NULL };

static int IsDelimiterWord(char *word)
{
    size_t len1 = strlen(word);
    for (int i = 0; Delimiters[i] != NULL; i++) {
        size_t len2 = strlen(Delimiters[i]);
        if (len2 == len1) {
            int found = 1;
            for (int j = 0; j < len1; j++) {
                if (toupper(word[j]) != Delimiters[i][j]) {
                    found = 0;
                    break;
                }
            }
            if (found)
                return 1;
        }
    }
    return 0;
}

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

void Parser(void)
{
    int i;

    /* Is there input remaining to be analyzed? */
    if (!FindNextCommandDelimiter()) {
        LineInput();
        if (WordsInInput == 0 || InputWordStrings == NULL)
            return;
    } else {
        FirstAfterInput = 0;
        OutChar(' ');
    }

    int wn = 0;

    for (i = 0; i < 5 && WordIndex + i < WordsInInput; i++) {
        Word[wn] = ParseWord(InputWordStrings[WordIndex + i]);
        /* Hack for Blizzard Pass verbs */
        if (CurrentGame == BLIZZARD_PASS && wn == 0) {
            /* Change SKIN to VSKI */
            if (Word[0] == 249)
                Word[0] = 43;
            /* Change POLI to VPOL */
            else if (Word[0] == 175)
                Word[0] = 44;
            /* Change noun DRAW(bridge) to verb DRAW */
            else if (Word[0] == 134)
                Word[0] = 49;
        }
        if (Version == QUESTPROBE3_TYPE && wn == 1) {
            /* Understand WALL and FIRE as the wall of fire */
            if ((Word[1] == 54 || Word[1] == 44) && (ObjectLoc[10] == MyLoc || ObjectLoc[11] == MyLoc))
                Word[1] = 77;
            /* Understand MASTERS as Alicia Masters */
            if (Word[1] == 106 && ObjectLoc[46] == MyLoc)
                Word[1] = 121;
        }
        if (Word[wn]) {
            debug_print("Word %d is %d\n", wn, Word[wn]);
            WordPositions[wn] = WordIndex + i;
            wn++;
        } else {
            if (wn > 0 && IsDelimiterWord(InputWordStrings[WordIndex + i]))
                break;
            debug_print("Word at position %d, %s, was not recognized\n", WordIndex + i, InputWordStrings[WordIndex + i]);
        }
    }
    for (i = wn; i < 5; i++) {
        Word[i] = 0;
        WordPositions[i] = 0;
    }
}
