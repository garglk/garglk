#ifndef DICTIONARY_H_
#define DICTIONARY_H_
/*----------------------------------------------------------------------*\

  Dictionary of player words in Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "acode.h"
#include "types.h"

/* CONSTANTS */


/* TYPES */


/* DATA */
extern DictionaryEntry *dictionary;
extern int dictionarySize;
extern int conjWord;        /* First conjunction in dictionary */


/* FUNCTIONS */
extern bool isVerbWord(int wordIndex);
extern bool isConjunctionWord(int wordIndex);
extern bool isExceptWord(int wordIndex);
extern bool isThemWord(int wordIndex);
extern bool isItWord(int wordIndex);
extern bool isNounWord(int wordIndex);
extern bool isAdjectiveWord(int wordIndex);
extern bool isPrepositionWord(int wordIndex);
extern bool isAllWord(int wordIndex);
extern bool isDirectionWord(int wordIndex);
extern bool isPronounWord(int wordIndex);
extern bool isLiteralWord(int wordIndex);

extern bool isConjunction(int wordCode);
extern bool isAll(int wordCode);
extern bool isNoise(int wordCode);
extern bool isPronoun(int wordCode);

#endif /* DICTIONARY_H_ */
