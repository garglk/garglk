/*----------------------------------------------------------------------*\

    dictionary

\*----------------------------------------------------------------------*/
#include "dictionary.h"

/* IMPORTS */
#include "word.h"


/* PUBLIC DATA */
DictionaryEntry *dictionary;    /* Dictionary pointer */
int dictionarySize;
int conjWord;           /* First conjunction in dictionary, for ',' */



/* Word class query methods, move to Word.c */
/* Word classes are numbers but in the dictionary they are generated as bits */
static Bool isVerb(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&VERB_BIT)!=0;
}

Bool isVerbWord(int wordIndex) {
    return isVerb(playerWords[wordIndex].code);
}

Bool isConjunction(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&CONJUNCTION_BIT)!=0;
}

Bool isConjunctionWord(int wordIndex) {
    return isConjunction(playerWords[wordIndex].code);
}

static Bool isExcept(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&EXCEPT_BIT)!=0;
}

Bool isExceptWord(int wordIndex) {
    return isExcept(playerWords[wordIndex].code);
}

static Bool isThem(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&THEM_BIT)!=0;
}

Bool isThemWord(int wordIndex) {
    return isThem(playerWords[wordIndex].code);
}

static Bool isIt(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&IT_BIT)!=0;
}

Bool isItWord(int wordIndex) {
    return isIt(playerWords[wordIndex].code);
}

static Bool isNoun(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&NOUN_BIT)!=0;
}

Bool isNounWord(int wordIndex) {
    return isNoun(playerWords[wordIndex].code);
}

static Bool isAdjective(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&ADJECTIVE_BIT)!=0;
}

Bool isAdjectiveWord(int wordIndex) {
    return isAdjective(playerWords[wordIndex].code);
}

static Bool isPreposition(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&PREPOSITION_BIT)!=0;
}

Bool isPrepositionWord(int wordIndex) {
    return isPreposition(playerWords[wordIndex].code);
}

Bool isAll(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&ALL_BIT)!=0;
}

Bool isAllWord(int wordIndex) {
    return isAll(playerWords[wordIndex].code);
}

static Bool isDir(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&DIRECTION_BIT)!=0;
}

Bool isDirectionWord(int wordIndex) {
    return isDir(playerWords[wordIndex].code);
}

Bool isNoise(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&NOISE_BIT)!=0;
}

Bool isPronoun(int wordCode) {
  return wordCode < dictionarySize && (dictionary[wordCode].classBits&PRONOUN_BIT)!=0;
}

Bool isPronounWord(int wordIndex) {
    return isPronoun(playerWords[wordIndex].code);
}

Bool isLiteralWord(int wordIndex) {
  return playerWords[wordIndex].code >= dictionarySize;
}
