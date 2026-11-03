//
//  parseinput.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef parseinput_h
#define parseinput_h

/* The number of dictionary words kept from one command */
#define WORDS_PER_COMMAND 5

void FreeInputWords(void);
void Parser(void);
int ParseWord(const char *word);
int EqualsIgnoringCase(const char *a, const char *b);

extern char **InputWordStrings;
extern int FirstWordPosition;
extern int KnownWords;
extern int WordIndex;
extern int WordsInInput;
extern uint8_t Word[WORDS_PER_COMMAND];
extern int FoundExtraCommand;

#endif /* parseinput_h */
