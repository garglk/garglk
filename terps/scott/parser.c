//
//  parser.c
//  scott
//
//  Created by Administrator on 2022-01-19.
//
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "parser.h"
#include "scott.h"

#include "bsd.h"

#define MAX_WORDLENGTH 128
#define MAX_WORDS 128

#define MAX_BUFFER 128

extern strid_t Transcript;
extern struct Command *CurrentCommand;

glui32 **UnicodeWords = NULL;
char **CharWords = NULL;
static int WordsInInput = 0;

static int lastnoun = 0;

static glui32 *FirstErrorMessage = NULL;

static void FreeStrings(void)
{
    if (FirstErrorMessage != NULL) {
        free(FirstErrorMessage);
        FirstErrorMessage = NULL;
    }
    if (WordsInInput == 0) {
        if (UnicodeWords != NULL || CharWords != NULL) {
            Fatal("ERROR! Wordcount 0 but word arrays not empty!\n");
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

static void CreateErrorMessage(const char *fchar, glui32 *second, const char *tchar)
{
    if (FirstErrorMessage != NULL)
        return;
    glui32 *first = ToUnicode(fchar);
    glui32 *third = ToUnicode(tchar);
    glui32 buffer[MAX_BUFFER];
    int i, j = 0, k = 0;
    for (i = 0; first[i] != 0 && i < MAX_BUFFER; i++)
        buffer[i] = first[i];
    if (second != NULL) {
        for (j = 0; second[j] != 0 && i + j < MAX_BUFFER; j++)
            buffer[i + j] = second[j];
    }
    if (third != NULL) {
        for (k = 0; third[k] != 0 && i + j + k < MAX_BUFFER; k++)
            buffer[i + j + k] = third[k];
        free(third);
    }
    int length = i + j + k;
    FirstErrorMessage = MemAlloc((length + 1) * 4);
    memcpy(FirstErrorMessage, buffer, length * 4);
    FirstErrorMessage[length] = 0;
    free(first);
}

glui32 *ToUnicode(const char *string)
{
    if (string == NULL)
        return NULL;
    glui32 unicode[2048];
    int i;
    int dest = 0;
    for (i = 0; string[i] != 0 && i < 2047; i++) {
        char c = string[i];
        if (c == '\n')
            c = 10;
        glui32 unichar = (glui32)c;
        if (Game && CurrentGame == TI994A) {
            switch (c) {
            case '@':
                unicode[dest++] = 0xa9;
                unichar = ' ';
                break;
            case '}':
                unichar = 0xfc;
                break;
                case 12:
                    unichar = 0xf6;
                break;
            case '{':
                unichar = 0xe4;
                break;
            }
        }
        unicode[dest++] = unichar;
    }
    unicode[dest] = 0;
    glui32 *result = MemAlloc((dest + 1) * 4);
    memcpy(result, unicode, (dest + 1) * 4);
    return result;
}

static char *FromUnicode(glui32 *unicode_string, int origlength)
{
    int sourcepos = 0;
    int destpos = 0;

    char dest[MAX_WORDLENGTH];
    glui32 unichar = unicode_string[sourcepos];
    while (unichar != 0 && destpos < MAX_WORDLENGTH && sourcepos < origlength) {
        switch (unichar) {
        case '.':
            if (origlength == 1) {
                dest[destpos++] = 'a';
                dest[destpos++] = 'n';
                dest[destpos++] = 'd';
            } else {
                dest[destpos] = (char)unichar;
            }
            break;
        case 0xf6: // ö
            dest[destpos++] = 'o';
            dest[destpos] = 'e';
            break;
        case 0xe4: // ä
            dest[destpos++] = 'a';
            dest[destpos] = 'e';
            break;
        case 0xfc: // ü
            dest[destpos] = 'u';
            break;
        case 0xdf: // ß
            dest[destpos++] = 's';
            dest[destpos] = 's';
            break;
        case 0xed: // í
            dest[destpos] = 'i';
            break;
        case 0xe1: // á
            dest[destpos] = 'a';
            break;
        case 0xf3: // ó
            dest[destpos] = 'o';
            break;
        case 0xf1: // ñ
            dest[destpos] = 'n';
            break;
        case 0xe9: // é
            dest[destpos] = 'e';
            break;
        default:
            dest[destpos] = (char)unichar;
            break;
        }
        sourcepos++;
        destpos++;
        unichar = unicode_string[sourcepos];
    }
    if (destpos == 0)
        return NULL;
    char *result = MemAlloc(destpos + 1);
    memcpy(result, dest, destpos);

    result[destpos] = 0;
    return result;
}

/* Turns a unicode glui32 string into lower-case ASCII. */
/* Converts German and Spanish diacritical characters into non-diacritical equivalents */
/* (for the translated Gremlins variants.) Coalesces all runs of whitespace into a single standard space. */
/* Turns ending commas and periods into separate strings. */
static char **SplitIntoWords(glui32 *string, int length)
{
    if (length < 1) {
        return NULL;
    }

    glk_buffer_to_lower_case_uni(string, 512, MIN(length, 512));
    glk_buffer_canon_normalize_uni(string, 512, MIN(length, 512));

    int startpos[MAX_WORDS];
    int wordlength[MAX_WORDS];

    int words_found = 0;
    int word_index = 0;
    int foundspace = 0;
    int foundcomma = 0;
    startpos[0] = 0;
    wordlength[0] = 0;
    int lastwasspace = 1;
    for (int i = 0; string[i] != 0 && i < length && word_index < MAX_WORDS; i++) {
        foundspace = 0;
        switch (string[i]) {
            /* Unicode space and tab variants */
        case ' ':
        case '\t':
        case '!':
        case '?':
        case '\"':
        case 0x83: // ¿
        case 0x80: // ¡
        case 0xa0: // non-breaking space
        case 0x2000: // en quad
        case 0x2001: // em quad
        case 0x2003: // em
        case 0x2004: // three-per-em
        case 0x2005: // four-per-em
        case 0x2006: // six-per-em
        case 0x2007: // figure space
        case 0x2009: // thin space
        case 0x200A: // hair space
        case 0x202f: // narrow no-break space
        case 0x205f: // medium mathematical space
        case 0x3000: // ideographic space
            foundspace = 1;
            break;
            case '.':
            case ',':
                foundcomma = 1;
                break;
        default:
            break;
        }
        if (!foundspace) {
            if (lastwasspace || foundcomma) {
                /* Start a new word */
                startpos[words_found] = i;
                words_found++;
                wordlength[words_found] = 0;
            }
            wordlength[words_found - 1]++;
            lastwasspace = 0;
        } else {
            /* Check if the last character of previous word was a period or comma */
            lastwasspace = 1;
            foundcomma = 0;
        }
    }

    if (words_found == 0) {
        return NULL;
    }

    wordlength[words_found]--; /* Don't count final newline character */

    /* Now we've created two arrays, one for starting postions
       and one for word length. Now we convert these into an array of strings */
    glui32 **words = MemAlloc(words_found * sizeof(*words));
    char **words8 = MemAlloc(words_found * sizeof(*words8));

    for (int i = 0; i < words_found; i++) {
        words[i] = (glui32 *)MemAlloc((wordlength[i] + 1) * 4);
        memcpy(words[i], string + startpos[i], wordlength[i] * 4);
        words[i][wordlength[i]] = 0;
        words8[i] = FromUnicode(words[i], wordlength[i]);
    }
    UnicodeWords = words;
    WordsInInput = words_found;

    return words8;
}

static int ReadLineFromRecording(glui32 *buf, glui32 *length)
{
    if (InputRecording == NULL)
        return 0;

    char charbuf[512];

    *length = 0;
    glsi32 c = 0;
    int i;
    for (i = 0; i < 512; i++) {
        c = glk_get_char_stream(InputRecording);
        if (c == -1) {
            glk_stream_close(InputRecording, NULL);
            InputRecording = NULL;
            break;
        }
        if (c == '\n' || c == '\r' || c == 10) {
            break;
        }
        buf[i] = c;
        charbuf[i] = c;
    }
    buf[i] = 0;
    charbuf[i] = 0;
    Display(Bottom, "%s\n", charbuf);
    *length = i;
    return 1;
}

static char **LineInput(void)
{
    event_t ev;
    glui32 unibuf[512];

    do {
        Display(Bottom, "\n%s", sys[WHAT_NOW]);

        if (ReadLineFromRecording(unibuf, &ev.val1) == 0) {
            glk_request_line_event_uni(Bottom, unibuf, (glui32)511, 0);

            while (1) {
                glk_select(&ev);

                if (ev.type == evtype_LineInput)
                    break;
                else
                    Updates(ev);
            }

            unibuf[ev.val1] = 0;
        }

        if (Transcript) {
            glk_put_string_stream_uni(Transcript, unibuf);
            glk_put_char_stream_uni(Transcript, 10);
        }

        CharWords = SplitIntoWords(unibuf, ev.val1);

        if (WordsInInput == 0 || CharWords == NULL)
            Output(sys[HUH]);
        else {
            return CharWords;
        }

    } while (WordsInInput == 0 || CharWords == NULL);
    return NULL;
}

int WhichWord(const char *word, const char **list, int word_length, int list_length)
{
    int n = 1;
    int ne = 1;
    const char *tp;
    while (ne < list_length) {
        tp = list[ne];
        if (*tp == '*')
            tp++;
        else
            n = ne;
        if (xstrncasecmp(word, tp, word_length) == 0)
            return (n);
        ne++;
    }
    return (0);
}

const char *EnglishDirections[NUMBER_OF_DIRECTIONS] = { NULL, "north", "south", "east", "west", "up", "down", "n", "s", "e", "w", "u", "d", " " };
const char *SpanishDirections[NUMBER_OF_DIRECTIONS] = { NULL, "norte", "sur", "este", "oeste", "arriba", "abajo", "n", "s", "e", "o", "u", "d", "w" };
const char *GermanDirections[NUMBER_OF_DIRECTIONS] = { NULL, "norden", "sueden", "osten", "westen", "oben", "unten", "n", "s", "o", "w", "u", "d", " " };

const char *Directions[NUMBER_OF_DIRECTIONS];

const char *ExtraCommands[NUMBER_OF_EXTRA_COMMANDS] = {
    NULL,
    "restart",
    "#restart",
    "save",
    "#save",
    "restore",
    "load",
    "#restore",
    "transcript",
    "script",
    "#script",
    "oops",
    "undo",
    "bom",
    "#undo",
    "ram",
    "ramload",
    "ramrestore",
    "qload",
    "quickload",
    "#qload",
    "ramsave",
    "qsave",
    "quicksave",
    "#qsave",
    "except",
    "but",
    "", "", "", "", ""
};

extra_command ExtraCommandsKey[NUMBER_OF_EXTRA_COMMANDS] = {
    NO_COMMAND, RESTART, RESTART, SAVE, SAVE, RESTORE, RESTORE,
    RESTORE, SCRIPT, SCRIPT, SCRIPT, UNDO, UNDO, UNDO, UNDO,
    RAM, RAMLOAD, RAMLOAD, RAMLOAD, RAMLOAD, RAMLOAD, RAMSAVE, RAMSAVE, RAMSAVE, RAMSAVE, EXCEPT, EXCEPT,
    RESTORE, RESTORE, SCRIPT, UNDO, RESTART
};

const char *EnglishExtraNouns[NUMBER_OF_EXTRA_NOUNS] = {
    NULL, "game", "story", "on", "off", "load",
    "restore", "save", "move", "command", "turn",
    "all", "everything", "it", " ", " ",
};
const char *GermanExtraNouns[NUMBER_OF_EXTRA_NOUNS] = {
    NULL, "spiel", "story", "on", "off", "wiederherstellen",
    "laden", "speichern", "move", "verschieben", "runde",
    "alle", "alles", "es", "einschalten", "ausschalten"
};
const char *SpanishExtraNouns[NUMBER_OF_EXTRA_NOUNS] = {
    NULL, "juego", "story", "on", "off", "cargar",
    "reanuda", "conserva", "move", "command", "jugada",
    "toda", "todo", "eso", "activar", "desactivar"
};

const char *ExtraNouns[NUMBER_OF_EXTRA_NOUNS];

const extra_command ExtraNounsKey[NUMBER_OF_EXTRA_NOUNS] = {
    NO_COMMAND, GAME, GAME, ON, OFF, RAMLOAD,
    RAMLOAD, RAMSAVE, COMMAND, COMMAND, COMMAND,
    ALL, ALL, IT, ON, OFF
};

#define NUMBER_OF_ABBREVIATIONS 6
const char *Abbreviations[NUMBER_OF_ABBREVIATIONS] = { NULL, "i", "l",
    "x", "z", "q" };
const char *AbbreviationsKey[NUMBER_OF_ABBREVIATIONS] = {
    NULL, "inventory", "look", "examine", "wait", "quit"
};

const char *EnglishSkipList[NUMBER_OF_SKIPPABLE_WORDS] = {
    NULL, "at", "to", "in", "into", "the",
    "a", "an", "my", "quickly", "carefully", "quietly",
    "slowly", "violently", "fast", "hard", "now", "room"
};
const char *GermanSkipList[NUMBER_OF_SKIPPABLE_WORDS] = {
    NULL, "nach", "die", "der", "das", "im", "mein", "meine", "an",
    "auf", "den", "lassen", "lass", "fallen", " ", " ", " ", " "
};
const char *SkipList[NUMBER_OF_SKIPPABLE_WORDS];

const char *EnglishDelimiterList[NUMBER_OF_DELIMITERS] = { NULL, ",", "and",
    "then", " " };
const char *GermanDelimiterList[NUMBER_OF_DELIMITERS] = { NULL, ",", "und",
    "dann", "and" };
const char *DelimiterList[NUMBER_OF_DELIMITERS];

/* For the verb position in a command string sequence, we try the following
 lists in this order: Verbs, Directions, Abbreviations, SkipList, Nouns,
 ExtraCommands, Delimiters */
static int FindVerb(const char *string, const char ***list)
{
    *list = Verbs;
    int verb = WhichWord(string, *list, GameHeader.WordLength, GameHeader.NumWords + 1);
    if (verb) {
        return verb;
    }
    *list = Directions;

    verb = WhichWord(string, *list, GameHeader.WordLength, NUMBER_OF_DIRECTIONS);
    if (verb) {
        if (verb == 13)
            verb = 4;
        if (verb > 6)
            verb -= 6;
        return verb;
    }
    *list = Abbreviations;
    verb = WhichWord(string, *list, GameHeader.WordLength, NUMBER_OF_ABBREVIATIONS);
    if (verb) {
        verb = WhichWord(AbbreviationsKey[verb], Verbs, GameHeader.WordLength,
            GameHeader.NumWords + 1);
        if (verb) {
            *list = Verbs;
            return verb;
        }
    }

    int stringlength = strlen(string);

    *list = SkipList;
    verb = WhichWord(string, *list, stringlength, NUMBER_OF_SKIPPABLE_WORDS);
    if (verb) {
        return 0;
    }
    *list = Nouns;
    verb = WhichWord(string, *list, GameHeader.WordLength, GameHeader.NumWords + 1);
    if (verb) {
        return verb;
    }

    *list = ExtraCommands;
    verb = WhichWord(string, *list, stringlength, NUMBER_OF_EXTRA_COMMANDS);
    if (verb) {
        verb = ExtraCommandsKey[verb];
        return verb + GameHeader.NumWords;
    }

    *list = ExtraNouns;
    verb = WhichWord(string, *list, stringlength, NUMBER_OF_EXTRA_NOUNS);
    if (verb) {
        verb = ExtraNounsKey[verb];
        return verb + GameHeader.NumWords;
    }

    *list = DelimiterList;
    verb = WhichWord(string, *list, stringlength, NUMBER_OF_DELIMITERS);
    if (!verb)
        *list = NULL;
    return verb;
}

/* For the noun position in a command string sequence, we try the following
 lists in this order:
 Nouns, Directions, ExtraNouns, SkipList, Verbs, Delimiters */
static int FindNoun(const char *string, const char ***list)
{
    *list = Nouns;
    int noun = WhichWord(string, *list, GameHeader.WordLength, GameHeader.NumWords + 1);
    if (noun) {
        return noun;
    }

    *list = Directions;
    noun = WhichWord(string, *list, GameHeader.WordLength, NUMBER_OF_DIRECTIONS);
    if (noun) {
        if (noun > 6)
            noun -= 6;
        *list = Nouns;
        return noun;
    }

    int stringlength = strlen(string);

    *list = ExtraNouns;

    noun = WhichWord(string, *list, stringlength, NUMBER_OF_EXTRA_NOUNS);
    if (noun) {
        noun = ExtraNounsKey[noun];
        return noun + GameHeader.NumWords;
    }

    *list = SkipList;
    noun = WhichWord(string, *list, stringlength, NUMBER_OF_SKIPPABLE_WORDS);
    if (noun) {
        return 0;
    }

    *list = Verbs;
    noun = WhichWord(string, *list, GameHeader.WordLength, GameHeader.NumWords + 1);
    if (noun) {
        return noun;
    }

    *list = DelimiterList;
    noun = WhichWord(string, *list, stringlength, NUMBER_OF_DELIMITERS);

    if (!noun)
        *list = NULL;
    return 0;
}

static struct Command *CommandFromStrings(int index, struct Command *previous);

static int FindExtaneousWords(int *index, int noun)
{
    /* Looking for extraneous words that should invalidate the command */
    int original_index = *index;
    if (*index >= WordsInInput) {
        return 0;
    }
    const char **list = NULL;
    int verb = 0;
    int stringlength = strlen(CharWords[*index]);

    list = SkipList;
    do {
        verb = WhichWord(CharWords[*index], SkipList, stringlength,
            NUMBER_OF_SKIPPABLE_WORDS);
        if (verb)
            *index = *index + 1;
    } while (verb && *index < WordsInInput);

    if (*index >= WordsInInput)
        return 0;

    verb = FindVerb(CharWords[*index], &list);

    if (list == DelimiterList) {
        if (*index > original_index)
            *index = *index - 1;
        return 0;
    }

    if (list == Nouns && noun) {
        if (MapSynonym(noun) == MapSynonym(verb)) {
            *index = *index + 1;
            return 0;
        }
    }

    if (list == NULL) {
        if (*index >= WordsInInput)
            *index = WordsInInput - 1;
        CreateErrorMessage(sys[I_DONT_KNOW_WHAT_A], UnicodeWords[*index], sys[IS]);
    } else {
        CreateErrorMessage(sys[I_DONT_UNDERSTAND], NULL, NULL);
    }

    return 1;
}

static struct Command *CreateCommandStruct(int verb, int noun, int verbindex, int nounindex, struct Command *previous)
{
    struct Command *command = MemAlloc(sizeof(struct Command));
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
    command->next = CommandFromStrings(nounindex, command);
    return command;
}

static struct Command *CommandFromStrings(int index, struct Command *previous)
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

    if (list == Directions) {
        /* It is a direction */
        if (verb == 0 || FindExtaneousWords(&i, 0) != 0)
            return NULL;
        return CreateCommandStruct(GO, verb, 0, i, previous);
    }

    int found_noun_at_verb_position = 0;
    int lastverb = 0;

    if (list == Nouns || list == ExtraNouns) {
        /* It is a noun */
        /* If we find no verb, we try copying the verb from the previous command */
        if (previous) {
            lastverb = previous->verb;
            verbindex = previous->verbwordindex;
        } else {
            CreateErrorMessage(sys[I_DONT_KNOW_HOW_TO], UnicodeWords[i - 1], sys[SOMETHING]);
            return NULL;
        }
        if (FindExtaneousWords(&i, verb) != 0)
            return NULL;

        return CreateCommandStruct(lastverb, verb, verbindex, i, previous);

    }

    if (list == NULL || list == SkipList) {
        CreateErrorMessage(sys[I_DONT_KNOW_HOW_TO], UnicodeWords[i - 1], sys[SOMETHING]);
        return NULL;
    }

    if (i == WordsInInput) {
        if (lastverb)
            return CreateCommandStruct(lastverb, verb, previous->verbwordindex, i, previous);
        else
            return CreateCommandStruct(verb, 0, i - 1, i, previous);
    }

    int noun = 0;

    do {
        /* Check if it is a noun */
        noun = FindNoun(CharWords[i++], &list);
    } while (list == SkipList && i < WordsInInput);

    if (list == Nouns || list == ExtraNouns) {
        /* It is a noun */

        /* Check if it is an ALL followed by EXCEPT */
        int except = 0;
        if (list == ExtraNouns && i < WordsInInput && noun - GameHeader.NumWords == ALL) {
            int stringlength = strlen(CharWords[i]);
            except = WhichWord(CharWords[i], ExtraCommands, stringlength,
                                   NUMBER_OF_EXTRA_COMMANDS);
        }
        if (ExtraCommandsKey[except] != EXCEPT && FindExtaneousWords(&i, noun) != 0)
            return NULL;
        if (found_noun_at_verb_position) {
            int realverb = WhichWord(CharWords[i - 1], Verbs, GameHeader.WordLength,
                GameHeader.NumWords);
            if (realverb) {
                noun = verb;
                verb = realverb;
            } else if (lastverb) {
                noun = verb;
                verb = lastverb;
            }
        }
        return CreateCommandStruct(verb, noun, verbindex, i, previous);
    }

    if (list == DelimiterList) {
        /* It is a delimiter */
        return CreateCommandStruct(verb, 0, verbindex, i, previous);
    }

    if (list == Verbs && found_noun_at_verb_position) {
        /* It is a verb */
        /* Check if it is an ALL followed by EXCEPT */
        int except = 0;
        if (i < WordsInInput && verb - GameHeader.NumWords == ALL) {
            int stringlength = strlen(CharWords[i]);
            except = WhichWord(CharWords[i], ExtraCommands, stringlength,
                               NUMBER_OF_EXTRA_COMMANDS);
        }
        if (ExtraCommandsKey[except] != EXCEPT && FindExtaneousWords(&i, 0) != 0)
            return NULL;
        return CreateCommandStruct(noun, verb, i - 1, i, previous);
    }

    CreateErrorMessage(sys[I_DONT_KNOW_WHAT_A], UnicodeWords[i - 1], sys[IS]);
    return NULL;
}

static int CreateAllCommands(struct Command *command)
{

    int exceptions[GameHeader.NumItems];
    int exceptioncount = 0;

    int location = CARRIED;
    if (command->verb == TAKE)
        location = MyLoc;

    struct Command *next = command->next;
    /* Check if the ALL command is followed by EXCEPT */
    /* and if it is, build an array of items to be excepted */
    while (next && next->verb == GameHeader.NumWords + EXCEPT) {
        for (int i = 0; i <= GameHeader.NumItems; i++) {
            if (Items[i].AutoGet && xstrncasecmp(Items[i].AutoGet, CharWords[next->nounwordindex], GameHeader.WordLength) == 0) {
                exceptions[exceptioncount++] = i;
            }
        }
        /* Remove the EXCEPT command from the linked list of commands */
        next = next->next;
        free(command->next);
        command->next = next;
    }

    struct Command *c = command;
    int found = 0;
    for (int i = 0; i < GameHeader.NumItems; i++) {
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
                    c->next = MemAlloc(sizeof(struct Command));
                    c->next->previous = c;
                    c = c->next;
                }
                found = 1;
                c->verb = command->verb;
                c->noun = WhichWord(Items[i].AutoGet, Nouns, GameHeader.WordLength,
                                    GameHeader.NumWords);
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
    FreeStrings();
    if (FirstErrorMessage)
        free(FirstErrorMessage);
    FirstErrorMessage = NULL;
}

static void PrintPendingError(void)
{
    if (FirstErrorMessage) {
        glk_put_string_stream_uni(glk_window_get_stream(Bottom), FirstErrorMessage);
        free(FirstErrorMessage);
        FirstErrorMessage = NULL;
        stop_time = 1;
    }
}

int GetInput(int *vb, int *no)
{
    if (CurrentCommand && CurrentCommand->next) {
        CurrentCommand = CurrentCommand->next;
    } else {
        PrintPendingError();
        if (CurrentCommand)
            FreeCommands();
        CharWords = LineInput();

        if (WordsInInput == 0 || CharWords == NULL)
            return 0;

        CurrentCommand = CommandFromStrings(0, NULL);
    }

    if (CurrentCommand == NULL) {
        PrintPendingError();
        return 1;
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

    if (*no > 6) {
        lastnoun = *no;
    }

    return 0;
}

int RecheckForExtraCommand(void)
{
    const char *VerbWord = CharWords[CurrentCommand->verbwordindex];

    int ExtraVerb = WhichWord(VerbWord, ExtraCommands, GameHeader.WordLength,
        NUMBER_OF_EXTRA_COMMANDS);
    if (!ExtraVerb) {
        return 0;
    }
    int ExtraNoun = 0;
    if (CurrentCommand->noun) {
        const char *NounWord = CharWords[CurrentCommand->nounwordindex];
        ExtraNoun = WhichWord(NounWord, ExtraNouns, strlen(NounWord),
            NUMBER_OF_EXTRA_NOUNS);
    }
    CurrentCommand->verb = ExtraCommandsKey[ExtraVerb];
    if (ExtraNoun)
        CurrentCommand->noun = ExtraNounsKey[ExtraNoun];

    return PerformExtraCommand(1);
}
