//
//  parser.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-19.
//

#ifndef parser_h
#define parser_h

#include "glk.h"

typedef struct Command {
    int verb;
    int noun;
    int item;
    int verbwordindex;
    int nounwordindex;
    int allflag;
    struct Command *previous;
    struct Command *next;
} Command;

typedef enum {
    NO_COMMAND,
    RESTART,
    SAVE,
    RESTORE,
    SCRIPT,
    ON,
    OFF,
    UNDO,
    RAM,
    RAMSAVE,
    RAMLOAD,
    GAME,
    COMMAND,
    ALL,
    IT,
    EXCEPT,
    FLICKER
} extra_command;

int GetInput(int *vb, int *no);
void FreeCommands(void);
glui32 *ToUnicode(const char *string);
int RecheckForExtraCommand(void);
int FindExtraNoun(int wordindex);
int WhichWord(const char *word, const char **list, int word_length,
    int list_length);

extern glui32 **UnicodeWords;
extern char **CharWords;

#define NUM_DIRECTIONS 14

extern const char *Directions[];
extern const char *GermanDirections[];
extern const char *SpanishDirections[];
extern const char *ItalianDirections[];
extern const char *EnglishDirections[];

#define NUM_SKIPPABLE_WORDS 18

extern const char *SkipList[];
extern const char *EnglishSkipList[];
extern const char *GermanSkipList[];
extern const char *ItalianSkipList[];

#define NUM_DELIMITERS 5

extern const char *EnglishDelimiterList[];
extern const char *GermanDelimiterList[];
extern const char *ItalianDelimiterList[];
extern const char *DelimiterList[];

#define NUM_EXTRA_COMMANDS 34
extern const char *GermanExtraCommands[];
extern const char *SpanishExtraCommands[];
extern const char *ItalianExtraCommands[];
extern const char *ExtraCommands[];

#define NUM_EXTRA_NOUNS 16

extern const char *EnglishExtraNouns[];
extern const char *GermanExtraNouns[];
extern const char *SpanishExtraNouns[];
extern const char *ItalianExtraNouns[];
extern const char *ExtraNouns[];
extern const extra_command ExtraNounsKey[];

#endif /* parser_h */
