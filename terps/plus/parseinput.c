//
//  parseinput.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"

#include "common.h"
#include "definitions.h"
#include "extracommands.h"
#include "dumpactions.h"
#include "parseinput.h"
#include "restorestate.h"

static char **InputWordStrings = NULL;  /* Array of uppercased word strings from the player's input */
static int WordsInInput = 0;           /* Number of words in the current input line */
static int WordIndex = 0;              /* Current position in TokenWords during command parsing */
static int InitialIndex = 0;           /* WordIndex at the start of the current command (for multi-command lines) */
int ProtagonistString = 0;      /* Message index for the player character's name, used in the prompt */

/* Dictionary arrays loaded from the game data file. Each is a
   NULL-terminated array of DictWord structs mapping strings to
   group indices. */
DictWord *Verbs;
DictWord *Nouns;
DictWord *Adverbs;
DictWord *Prepositions;

/* Current and previous command components. The action engine reads
   Current* to match actions; Last* preserves the previous turn's
   values so AGAIN can re-execute and implicit verbs can carry forward. */
int CurrentVerb = 0, CurrentNoun = 0, CurrentPrep = 0, CurrentPartp = 0, CurrentNoun2 = 0, CurrentAdverb = 0, LastVerb = 0, LastNoun = 0, LastPrep = 0, LastPartp = 0, LastNoun2 = 0, LastAdverb = 0;

/* Free all word strings from the current input and reset word state. */
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

/* Compare two dictionary words character by character, stopping at the
   first '#' in either string. Hash signs separate homonyms in the Plus
   dictionary format, so "KEY#1" and "KEY#2" compare as equal. */
int CompareUpToHashSign(const char *word1, const char *word2)
{
    for (int i = 0; word1[i] != '#' && word2[i] != '#'; i++) {
        if (word1[i] != word2[i])
            return 0;
        if (word1[i] == 0)
            return 1;
    }
    return 1;
}

/* Prompt the player for a single-character yes/no answer. Loops until
   a valid response is given. Returns 1 for yes, 0 for no. The expected
   characters are taken from the localised system messages. */
int YesOrNo(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    const char y = tolower((unsigned char)sys[YES][0]);
    const char n = tolower((unsigned char)sys[NO][0]);

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            const char reply = tolower(ev.val1);
            if (reply == y) {
                result = 1;
            } else if (reply == n) {
                result = 2;
            } else {
                Output(sys[ANSWER_YES_OR_NO]);
                glk_request_char_event(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);

    Display(Bottom, "%c\n", (char)ev.val1);
    lastwasnewline = 1;
    return (result == 1);
}

/* The most items we will ask about when disambiguating a noun */
#define MAX_HASH_MATCHES 10

/* Disambiguate a noun when multiple items present in the current room
   (or carried) share a hash-separated dictionary word. If exactly one
   item matches, return its dictionary index silently. If several match,
   ask the player "do you mean X?" for each until one is confirmed.
   Returns 0 if no match or the player declined all options. */
static int FindNounWithHash(const char *p)
{
    int num_matches = 0;
    int matches[MAX_HASH_MATCHES];

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Dictword && (Items[i].Location == MyLoc || Items[i].Location == CARRIED))
            if (num_matches < MAX_HASH_MATCHES && CompareUpToHashSign(Nouns[GetDictWord(Items[i].Dictword)].Word, p)) {
                matches[num_matches] = i;
                num_matches++;
            }
    }

    if (num_matches == 1)
        return Items[matches[0]].Dictword;

    if (num_matches) {
        for (int i = 0; i < num_matches; i++) {
            Display(Bottom, "When you say %s, do you mean %s? ", p, Items[matches[i]].Text);
            if (YesOrNo()) {
                Output("Thank-you! ");
                return Items[matches[i]].Dictword;
            }
        }
    }
    return 0;
}

/* Look up a word in a dictionary array by exact string match.
   Returns the word's group index, or 0 if not found. */
static int ParseWord(const char *p, const DictWord *dict)
{
    for (int i = 0; dict[i].Word != NULL; i++) {
        if (strcmp(dict[i].Word, p) == 0) {
            return (dict[i].Group);
        }
    }

    return 0;
}

/* Check whether synonym is a prefix of original that ends at a word
   boundary (end of string or a space). Returns the number of matching
   characters, or 0 on mismatch. */
static int MatchingChars(const char *synonym, const char *original)
{
    if (synonym == NULL || original == NULL || synonym[0] == 0 || original[0] == 0)
        return 0;
    int i;
    for (i = 0; synonym[i] != 0 && original[i] != 0; i++) {
        if (synonym[i] != original[i])
            return 0;
    }

    return (synonym[i] == 0 && (original[i] == 0 || original[i] == ' '));
}

/* Search for synonym within original, matching only at word boundaries
   (start of string or preceded by a space). Returns the byte offset of
   the match, or -1 if not found. */
static int IsSynonymMatch(const char *synonym, const char *original)
{
    char c = synonym[0];
    if (c == 0)
        return -1;
    const char *found = strchr(original, c);
    while (found) {
        if (found == original || *(found - 1) == ' ') {
            if (MatchingChars(synonym, found))
                return (int)(found - original);
        }
        found = strchr(found + 1, c);
    }
    return -1;
}

/* Collapse runs of whitespace in-place to single spaces. Updates *len. */
static char *StripDoubleSpaces(char *result, int *len)
{
    int dst = 0;
    for (int src = 0; src < *len; src++) {
        if (src > 0 && isspace(result[src]) && isspace(result[src - 1]))
            continue;
        result[dst++] = result[src];
    }
    *len = dst;
    result[dst] = 0;
    return result;
}

/* Return a copy of buf with punctuation characters replaced by spaces.
   The input is not modified. */
static char *StripPunctuation(const char *buf, int *len)
{
    char *result = MemAlloc(*len + 1);
    for (int i = 0; i < *len; i++) {
        char c = buf[i];
        if (c == '.' || c == ',' || c == ':' || c == ';' || c == '\'' || c == '"' || c == '-' || c == '!')
            c = ' ';
        result[i] = c;
    }
    result[*len] = 0;
    return result;
}

/* Replace one occurrence of pattern in source (at offset *startpos) with
   replacement. Frees source, returns a new string, and sets *startpos to
   the new total length. Collapses any double spaces introduced by the
   substitution. */
static char *ReplaceString(char *source, const char *pattern, const char *replacement, int *startpos)
{
    int sourcelen = (int)strlen(source);
    int patternlen = (int)strlen(pattern);
    int replacelen = (int)strlen(replacement);
    int headlen = *startpos;
    int taillen = sourcelen - headlen - patternlen;
    if (taillen < 0)
        taillen = 0;

    int totallen = headlen + replacelen + taillen;
    char *result = MemAlloc(totallen + 1);
    memcpy(result, source, headlen);
    memcpy(result + headlen, replacement, replacelen);
    memcpy(result + headlen + replacelen, source + headlen + patternlen, taillen);
    result[totallen] = 0;
    free(source);

    if (totallen > 1)
        result = StripDoubleSpaces(result, &totallen);

    *startpos = totallen;
    return result;
}

/* Uppercase the input then apply all synonym substitutions from the
   game's Substitutions table. Each synonym is replaced repeatedly until
   no more matches are found (handling overlapping or chained rewrites). */
static char *ReplaceSynonyms(char *buf, int *len)
{
    for (int i = 0; i < *len; i++)
        buf[i] = toupper(buf[i]);
    buf = StripDoubleSpaces(buf, len);

    for (int i = 0; Substitutions[i].SynonymString != NULL; i++) {
        char *syn = Substitutions[i].SynonymString;
        const char *replacement = Substitutions[i].ReplacementString;
        /* A replacement that contains its own synonym would match
           forever. The unfinished X-Men lists a word as a synonym of
           itself (",BRICK,BRICKED UP,BRICKED,BRICK WALL,=BRICK,"). */
        int once = (IsSynonymMatch(syn, replacement) >= 0);
        int matchpos = IsSynonymMatch(syn, buf);
        while (matchpos >= 0) {
            debug_print("Replaced string \"%s\" with \"%s\".\n", syn, replacement);
            buf = ReplaceString(buf, syn, replacement, &matchpos);
            debug_print("Result: \"%s\"\n", buf);
            if (once)
                break;
            matchpos = IsSynonymMatch(syn, buf);
        }
    }
    *len = (int)strlen(buf);
    return buf;
}

/* Split a string into an array of individually allocated word strings,
   delimited by whitespace. Sets WordsInInput to the number of words
   found. Returns NULL if no words are found. Capped at MAX_WORDS. */
static char **SplitIntoWords(const char *string, int length)
{
    if (length < 1)
        return NULL;

    int startpos[MAX_WORDS];
    int wordlen[MAX_WORDS];
    int words_found = 0;
    int in_word = 0;

    for (int i = 0; i < length && string[i] != 0; i++) {
        if (isspace(string[i])) {
            in_word = 0;
        } else {
            if (!in_word) {
                if (words_found >= MAX_WORDS)
                    break;
                startpos[words_found] = i;
                wordlen[words_found] = 1;
                words_found++;
                in_word = 1;
            } else {
                wordlen[words_found - 1]++;
            }
        }
    }

    if (words_found == 0)
        return NULL;

    char **words = MemAlloc(words_found * sizeof(*words));
    debug_print("%d words found\n", words_found);
    for (int i = 0; i < words_found; i++) {
        words[i] = MemAlloc(wordlen[i] + 1);
        memcpy(words[i], string + startpos[i], wordlen[i]);
        words[i][wordlen[i]] = 0;
        debug_print("Word %d: \"%s\"\n", i, words[i]);
    }
    WordsInInput = words_found;
    return words;
}

/* Classified tokens, parallel to InputWordStrings. Allocated anew for
   each input line by TokenizeInputWords(), which frees the old ones. */
static WordToken *TokenWords = NULL;

/* Heuristic: was the word at position i likely intended as a verb?
   True if it's the first word in the command, or if it follows a lone
   adverb with no preceding verb (e.g. "CAREFULLY OPEN"). Used to tailor
   error messages ("I don't know how to X" vs "I don't know what X means"). */
static int IntendedAsVerb(int i)
{
    if (i == InitialIndex || i == 0)
        return 1;
    /* if the word before was an adverb and the one before that was not a verb */
    if (TokenWords[i - 1].Type == ADVERB_TYPE && (i == InitialIndex + 1 || (i > InitialIndex + 1 && TokenWords[i - 2].Type != VERB_TYPE)))
        return 1;
    return 0;
}

/* Display an error message for the unrecognized word at position i.
   The wording depends on whether the word was likely meant as a verb,
   an adverb, or a noun/other. */
static void WordNotFoundError(int i)
{
    if (TokenWords[i].Type == ADVERB_TYPE)
        Display(Bottom, "%s do what?\n", InputWordStrings[i]);
    else if (IntendedAsVerb(i))
        Display(Bottom, "I don't know how to %s something!\n", InputWordStrings[i]);
    else
        Display(Bottom, "I don't know what %s means.\n", InputWordStrings[i]);
}

/* Record what kind of word input word number i is, and its dictionary
   group. */
static void SetToken(int i, WordType type, int index)
{
    debug_print("Found word of type %d (group %d): %s at %d\n", type, index, InputWordStrings[i], i);
    TokenWords[i].Index = index;
    TokenWords[i].Type = type;
}

/* Classify each word in InputWordStrings into a token type using
   priority-ordered dictionary lookups. The priority at position 0 is:
   verb > adverb > noun > preposition > extra word.
   At other positions verbs are tried last so that nouns and prepositions
   take precedence (e.g. "OPEN" as a noun in "OPEN DOOR" vs as a verb).
   Returns 1 on success, 0 if any word was unrecognized (with errors
   reported) or a special error occurred. */
static int TokenizeInputWords(void)
{
    free(TokenWords);
    TokenWords = MemAlloc(WordsInInput * sizeof(WordToken));

    int word_not_found = -1;
    int found_verb = 0;
    int verb_position = -1;

    for (int i = 0; i < WordsInInput; i++) {
        const char *word = InputWordStrings[i];
        int result;

        /* The first word is a verb if it can be */
        if (i == 0) {
            result = ParseWord(word, Verbs);
            if (result > 0) {
                SetToken(i, VERB_TYPE, result);
                found_verb = 1;
                verb_position = i;
                continue;
            }
        }

        result = ParseWord(word, Adverbs);
        if (result > 1) {
            SetToken(i, ADVERB_TYPE, result);
            continue;
        }

        result = ParseWord(word, Nouns);
        if (result <= 0 && strcmp(word, "ANY") != 0)
            result = FindNounWithHash(word);
        if (result > 0 || strcmp(word, "ANY") == 0) {
            SetToken(i, NOUN_TYPE, result);
            LastNoun = result;
            continue;
        }

        result = ParseWord(word, Prepositions);
        if (result > 1) {
            if (result == 2 && !found_verb) {
                Output("-USE- may NOT be the first word of your command! ");
                WordIndex = 255;
                return 0;
            }
            SetToken(i, PREPOSITION_TYPE, result);
            continue;
        }

        /* Other words are verbs only if they are nothing else */
        if (i > 0) {
            result = ParseWord(word, Verbs);
            if (result > 0) {
                SetToken(i, VERB_TYPE, result);
                found_verb = 1;
                verb_position = i;
                continue;
            }
        }

        result = ParseWord(word, ExtraWords);
        if (result > 0) {
            if (result == COM_IT) {
                SetToken(i, NOUN_TYPE, LastNoun);
            } else if (result == COM_WHERE) {
                Output("I'm only your puppet! You must figure things out for yourself! ");
                WordIndex = 255;
                return 0;
            } else {
                SetToken(i, OTHER_TYPE, result);
            }
            continue;
        }

        TokenWords[i].Type = WORD_NOT_FOUND;
        word_not_found = i;
    }

    /* We wait to report "word not recognized" errors until we have tokenized all words   */
    /* as we want to give other errors priority. Also helps deciding whether to say       */
    /* "The rest of your command was ignored." */
    if (word_not_found >= 0) {
        if (!found_verb || verb_position > word_not_found)
            WordNotFoundError(0);
        for (int j = 1; j <= word_not_found; j++)
            if (TokenWords[j].Type == WORD_NOT_FOUND)
                WordNotFoundError(j);
        WordIndex = word_not_found;
        return 0;
    }

    return 1;
}

/* The "with" list: the nouns that follow the participle WITH (2) or
   AT (3), as in "HIT ROBOT WITH FIST, ROCK". The second noun is always
   the first entry. Entries are numbered from 1 and counted by counter 46. */
#define MAX_WITH_LIST 20
static int WithList[MAX_WITH_LIST + 1];

static void ClearWithList(void)
{
    memset(WithList, 0, sizeof WithList);
    WithListCount = 0;
}

/* The noun that ARG_WITH_LIST stands for: the entry of the "with" list
   that counter 44 points at, or 0 if there is no such entry. */
int WithListNoun(void)
{
    int index = WithListIndex;
    if (index < 1 || index > MAX_WITH_LIST)
        return 0;
    return WithList[index];
}

/* Called when a participle and a second noun have been found. After WITH
   or AT, puts the second noun and any nouns directly following it in the
   "with" list. */
static void CollectWithList(void)
{
    if (CurrentPartp != 2 && CurrentPartp != 3)
        return;
    ClearWithList();
    WithList[++WithListCount] = CurrentNoun2;
    while (WordIndex < WordsInInput && TokenWords[WordIndex].Type == NOUN_TYPE) {
        int noun = TokenWords[WordIndex++].Index;
        if (WithListCount < MAX_WITH_LIST)
            WithList[++WithListCount] = noun;
    }
    debug_print("With list has %d nouns\n", WithListCount);
}

/* Check whether the next token(s) match a participle+noun2 pattern
   required by the action being tested. Two forms are accepted:
   - A bare noun when partp==2 (implicit preposition): "GIVE KEY JOHN"
   - A preposition followed by a noun: "GIVE KEY TO JOHN"
   If matched, advances WordIndex and sets CurrentPartp/CurrentNoun2
   (and the "with" list).
   partp and noun2 of 0 act as wildcards; partp==1 means "no participle
   expected" and always returns 0. */
int IsNextParticiple(int partp, int noun2)
{
    if (WordIndex >= WordsInInput) {
        return 0;
    }
    if (partp == 1) { // None
        return 0;
    }
    if (TokenWords[WordIndex].Type == NOUN_TYPE && partp == 2 && (noun2 == 0 || TokenWords[WordIndex].Index == noun2)) {
        CurrentPartp = 2;
        CurrentNoun2 = TokenWords[WordIndex++].Index;
        CollectWithList();
        return 1;
    } else if (TokenWords[WordIndex].Type == PREPOSITION_TYPE && WordIndex < WordsInInput - 1 && (partp == 0 || TokenWords[WordIndex].Index == partp)) {
        if (TokenWords[WordIndex + 1].Type == NOUN_TYPE && (noun2 == 0 || TokenWords[WordIndex + 1].Index == noun2)) {
            CurrentPartp = TokenWords[WordIndex++].Index;
            CurrentNoun2 = TokenWords[WordIndex++].Index;
            CollectWithList();
            return 1;
        }
    }
    return 0;
}

/* Debug display names for WordType enum values. */
#ifdef DEBUG /* Only used by debug_print() */
static const char * const WordTypeStrings[] = {
    "WORD_NOT_FOUND",
    "OTHER_TYPE",
    "VERB_TYPE",
    "NOUN_TYPE",
    "ADVERB_TYPE",
    "PREPOSITION_TYPE"
};
#endif

/* Map a token type to its corresponding dictionary array. */
static DictWord *DictWordFromType(WordType type) {
    switch (type) {
        case VERB_TYPE:
            return Verbs;
        case NOUN_TYPE:
            return Nouns;
        case ADVERB_TYPE:
            return Adverbs;
        case PREPOSITION_TYPE:
            return Prepositions;
        default:
            return NULL;
    }
}

/* If the token at WordIndex matches the given type, consume it: store
   its dictionary index in *word, advance WordIndex, and return 1.
   Returns 0 without advancing if the token doesn't match. */
static int FindWordOfType(WordType type, int *word) {
    if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == type) {
        *word = TokenWords[WordIndex++].Index;

        debug_print("FindWordOfType: Found %s \"", WordTypeStrings[type]);
        DictWord *dict = DictWordFromType(type);
        if (dict) {
            PrintDictWord(*word, dict);
        }
        debug_print("\" (%d)\n", *word);

        return 1;
    }
    return 0;
}

/* Convenience wrappers: consume the next token if it matches the type. */
static int FindVerb(int *verb) { return FindWordOfType(VERB_TYPE, verb); }
static int FindNoun(int *noun) { return (FindWordOfType(NOUN_TYPE, noun)); }
static int FindPreposition(int *prep) { return FindWordOfType(PREPOSITION_TYPE, prep); }
static int FindAdverb(int *adverb) { return FindWordOfType(ADVERB_TYPE, adverb); }

/* Debug: print the remaining input words from start to end. */
static void DebugPrintInputWords(int start, int end) {
    for (int i = start; i < end; i++)
        debug_print("Word %d: %s\n", i, InputWordStrings[i]);
}

/* What CommandFromTokens() returns: the first two are also the results
   of GetInput(). */
typedef enum {
    COMMAND_READY = 0,     /* A command for the action engine was built */
    NO_COMMAND = 1,        /* Error, no input, or a meta-command that was carried out */
    NO_EXTRA_COMMAND_FOUND /* Only from HandleExtraCommand(): go on parsing */
} ParseResult;

/* Try to handle the current token as a meta-command (SAVE, LOAD, SCORE,
   AGAIN, toggles like BRIEF/VERBOSE, etc.). Returns NO_EXTRA_COMMAND_FOUND
   if the token isn't an extra command, COMMAND_READY if the player typed
   AGAIN (so the previous command is re-executed), or NO_COMMAND for other
   extra commands. */
static ParseResult HandleExtraCommand(void) {
    int word;
    if (FindWordOfType(OTHER_TYPE, &word)) {
        int nextword = 0;
        if (WordIndex < WordsInInput) {
            nextword = ParseWord(InputWordStrings[WordIndex], ExtraWords);
        }
        /* The extra command words before ON are verbs */
        /* and we want verb only or verb + noun/preposition, */
        /* not two verbs. */
        if (nextword < COM_ON) {
            nextword = 0;
        }
        ExtraCommandResult result = PerformExtraCommand(word, nextword);
        if (result != RESULT_NOT_UNDERSTOOD) {
            if (result == RESULT_AGAIN)
                return COMMAND_READY;
            SetBit(STOPTIMEBIT);
            WordIndex += (result == RESULT_TWO_WORDS);
            return NO_COMMAND;
        } else {
            WordIndex = InitialIndex;
        }
    }
    return NO_EXTRA_COMMAND_FOUND;
}

/* Parse the token stream into command components following the grammar:
       [adverb] verb [noun] [preposition noun2] [participle noun2]
   Adverbs may appear before or after the verb. If no direct noun is
   found, a preposition+noun pair is tried (e.g. "LOOK AT DOOR").
   A second preposition+noun extends the command (e.g. "PUT KEY IN LOCK").
   Sets Current{Verb,Noun,Prep,Partp,Noun2,Adverb} for the action engine.
   Returns 0 if a command was constructed, 1 on error or no input. On
   continuation calls (multi-command input), verb carries forward from
   the previous command. */
static int CommandFromTokens(int verb, int noun) {
    debug_print("CommandFromTokens: WordIndex: %d verb: %d\n", WordIndex, verb);

    if (WordIndex == 0 && verb != 0)
        debug_print("CommandFromTokens: We are at the first input word, but verb is not 0!\n");

    if (WordIndex >= WordsInInput) {
        FreeInputWords();
        return 1;
    }

    InitialIndex = WordIndex;
    DebugPrintInputWords(WordIndex, WordsInInput);

    ParseResult extracommand = HandleExtraCommand();
    if (extracommand != NO_EXTRA_COMMAND_FOUND) {
        return extracommand;
    }

    CurrentPartp = 0;
    CurrentPrep = 0;
    CurrentNoun2 = 0;
    CurrentAdverb = 0;
    ClearWithList();

    FindVerb(&verb);

    /* Handle adverb-first input like "CAREFULLY OPEN DOOR" */
    if (FindAdverb(&CurrentAdverb) && verb == 0) {
        FindVerb(&verb);
    }

    if (verb == 0) {
        WordNotFoundError(InitialIndex);
        Output("Try again please. ");
        StopProcessingCommand();
        SetBit(STOPTIMEBIT);
        return 1;
    }

    if (WordIndex >= WordsInInput) {
        CurrentVerb = verb;
        CurrentNoun = 0;
        return 0;
    }

    /* Try noun directly, or preposition+noun ("LOOK AT DOOR") */
    if (!FindNoun(&noun) && FindPreposition(&CurrentPrep)) {
        FindNoun(&noun);
    }

    FindAdverb(&CurrentAdverb);

    /* If a preposition was found, look for a second noun after it.
       "PUT KEY ON TABLE" → noun=KEY, prep=ON, noun2=TABLE.
       If the first noun was consumed by the preposition clause,
       try one more time for the primary noun. */
    if (CurrentPrep != 0) {
        if (noun == 0) {
            FindNoun(&noun);
        } else {
            FindNoun(&CurrentNoun2);
        }
    }

    /* Try for a second preposition acting as a participle, with its own
       noun: "UNLOCK DOOR WITH KEY" → partp=WITH, noun2=KEY.
       If a noun doesn't follow the second preposition, reclassify: the
       original preposition becomes the participle and the prep slot is
       cleared, since the grammar only has one prep+noun pair. */
    if (FindPreposition(&CurrentPartp)) {
        int SecondNounAlreadyFound = (CurrentNoun2 != 0);
        if (FindNoun(&CurrentNoun2)) {
            if (SecondNounAlreadyFound) {
                debug_print("Found a third noun. This should never happen.\n");
            }
        } else {
            CurrentPartp = CurrentPrep;
            CurrentPrep = 0;
        }
    }

    if (CurrentPartp != 0) {
        debug_print("Found participle \"");
        PrintDictWord(CurrentPartp, Prepositions);
        debug_print("\" (%d)\n", CurrentPartp);
    }

    if (CurrentNoun2 != 0)
        CollectWithList();

    if (CurrentNoun2 != 0) {
        debug_print("Found second noun \"");
        PrintDictWord(CurrentNoun2, Nouns);
        debug_print("\" (%d)\n", CurrentNoun2);
    } else if (CurrentPartp != 0) {
        debug_print("Found participle but no second noun. This should never happen.\n");
    }

    /* Safety: ensure WordIndex advances past the current command to
       prevent infinite loops in multi-command processing. */
    if (WordIndex <= InitialIndex)
        WordIndex++;

    debug_print("Index: %d Words in input: %d ", WordIndex, WordsInInput);

    if (WordIndex < WordsInInput)
        debug_print("There are %d unmatched words remaining. Word at index: %s\n", WordsInInput - WordIndex, InputWordStrings[WordIndex]);
    else
        debug_print("No remaining input words.\n");

    CurrentVerb = verb;
    CurrentNoun = noun;
    return 0;
}

/* Prompt the player, read a line of input, and preprocess it into
   words. Strips punctuation, applies synonym substitutions, and splits
   into InputWordStrings. Loops until at least one valid word is entered. */
static void LineInput(void)
{
    event_t ev;
    char buf[512];
    FreeInputWords();

    for (;;) {
        Display(Bottom, "\n\n%s, %s", Messages[ProtagonistString], sys[WHAT_NOW]);
        glk_request_line_event(Bottom, buf, (glui32)(sizeof buf - 1), 0);

        while (1) {
            glk_select(&ev);
            if (ev.type == evtype_LineInput)
                break;
            Updates(ev);
        }

        int length = ev.val1;
        buf[length] = 0;

        char *result = StripPunctuation(buf, &length);
        debug_print("After stripping punctuation: \"%s\"\n", result);
        result = ReplaceSynonyms(result, &length);
        debug_print("Final result: \"%s\"\n", result);

        InputWordStrings = SplitIntoWords(result, length);
        free(result);

        Output("\n");

        if (WordsInInput > 0 && InputWordStrings)
            return;

        WordsInInput = 0;
        Output(sys[HUH]);
    }
}

/* Check whether the unprocessed tail of the input contains another verb.
   Used to decide whether to warn "The rest of your input was ignored." */
static int RemainderContainsVerb(void)
{
    debug_print("RemainderContainsVerb: WordIndex: %d WordsInInput: %d\n", WordIndex, WordsInInput);
    if (WordIndex < WordsInInput - 1) {
        for (int i = WordIndex; i < WordsInInput; i++)
            if (TokenWords[i].Type == VERB_TYPE)
                return 1;
    }
    return 0;
}

/* Discard all remaining input. Warns the player if an unprocessed verb
   would be lost, then frees word state and sets STOPTIMEBIT to suppress
   the turn's timed events. */
void StopProcessingCommand(void)
{
    if (RemainderContainsVerb()) {
        Output("\nThe rest of your input was ignored. ");
    }
    FreeInputWords();
    WordIndex = 0;
    SetBit(STOPTIMEBIT);
}

/* Top-level input handler. If unprocessed words remain from the previous
   line (a multi-command input like "TAKE KEY. OPEN DOOR"), continues
   parsing with the current verb carried forward. Otherwise reads a fresh
   line, tokenizes it, and builds the first command from the tokens.
   Returns 0 on success, 1 on error or empty input. */
int GetInput(void)
{
    int result = 0;

    /* Is there input remaining to be analyzed? */
    if (WordIndex >= WordsInInput || WordsInInput < 2) {
        LineInput();

        if (WordsInInput == 0 || InputWordStrings == NULL)
            return 1;

        lastwasnewline = 1;

        if (!TokenizeInputWords()) {
            if (WordIndex < 255)
                Output("Try again please. ");
            SetBit(STOPTIMEBIT);
            StopProcessingCommand();
            return 1;
        }

        result = CommandFromTokens(0, 0);
    } else {
        result = CommandFromTokens(CurrentVerb, 0);
    }

    return result;
}
