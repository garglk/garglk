//
//  parser.h
//  scott
//
//  Created by Administrator on 2022-01-19.
//

#ifndef parser_h
#define parser_h

#include <stdio.h>
#include "glk.h"

struct Command {
    int verb;
    int noun;
    int item;
    int verbwordindex;
    int nounwordindex;
    int allflag;
    struct Command *previous;
    struct Command *next;
};

typedef enum
{
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
    EXCEPT
} extra_command;

int GetInput(int *vb, int *no);
void FreeCommands(void);
glui32 *ToUnicode(const char *string);
int RecheckForExtraCommand(void);
int WhichWord(const char *word, const char **list, int word_length,
    int list_length);

extern glui32 **UnicodeWords;
extern char **CharWords;

#define NUMBER_OF_DIRECTIONS 14

extern const char *Directions[];
extern const char *GermanDirections[];
extern const char *SpanishDirections[];
extern const char *EnglishDirections[];

#define NUMBER_OF_SKIPPABLE_WORDS 18

extern const char *SkipList[];
extern const char *EnglishSkipList[];
extern const char *GermanSkipList[];

#define NUMBER_OF_DELIMITERS 5

extern const char *EnglishDelimiterList[];
extern const char *GermanDelimiterList[];
extern const char *DelimiterList[];

#define NUMBER_OF_EXTRA_COMMANDS 32
extern const char *ExtraCommands[];

#define NUMBER_OF_EXTRA_NOUNS 16

extern const char *EnglishExtraNouns[];
extern const char *GermanExtraNouns[];
extern const char *SpanishExtraNouns[];
extern const char *ExtraNouns[];
extern const extra_command ExtraNounsKey[];

#endif /* parser_h */
