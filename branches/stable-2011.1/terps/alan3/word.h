#ifndef WORD_H_
#define WORD_H_
/*----------------------------------------------------------------------*\

  word

\*----------------------------------------------------------------------*/

/* IMPORTS */


/* CONSTANTS */


/* TYPES */

typedef struct WordEntry {
  int code;			/* The dictionary index for that word */
  char *start;			/* Where does it start */
  char *end;			/* .. and end */
} Word;


/* DATA */

extern Word *playerWords;	/* List of Parsed Word */
extern int currentWordIndex;		/* and an index into it */
extern int firstWord;
extern int lastWord;

extern int verbWord;
extern int verbWordCode;



/* FUNCTIONS */
extern void ensureSpaceForPlayerWords(int count);
extern char *playerWordsAsCommandString(void);
extern void clearWordList(Word *list);


#endif /* WORD_H_ */
