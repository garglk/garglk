/*----------------------------------------------------------------------

  word
  
  ----------------------------------------------------------------------*/
#include "word.h"

/* IMPORTS */
#include "types.h"
#include "memory.h"
#include "syserr.h"
#include "lists.h"


/* CONSTANTS */


/* PUBLIC DATA */

/* List of parsed words, index into dictionary */
Word *playerWords = NULL;
int currentWordIndex; /* An index into it the list of playerWords */
int firstWord, lastWord;  /* Index for the first and last words for this command */

/* Some variable for dynamically allocating the playerWords, which will happen in scan() */
static int playerWordsLength = 0;
#define PLAYER_WORDS_EXTENT 20

/* What did the user say? */
int verbWord; /* The word he used as a verb, dictionary index */
int verbWordCode; /* The code for that verb */


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

void ensureSpaceForPlayerWords(int size) {
  int newLength = playerWordsLength+PLAYER_WORDS_EXTENT;

  if (playerWordsLength < size+1) {
    playerWords = realloc(playerWords, newLength*sizeof(Word));
    if (playerWords == NULL)
      syserr("Out of memory in 'ensureSpaceForPlayerWords()'");
    playerWordsLength = newLength;
  }
}


/*======================================================================*/
char *playerWordsAsCommandString(void) {
    char *commandString;
    int size = playerWords[lastWord].end - playerWords[firstWord].start;
    commandString = allocate(size + 1);
    strncpy(commandString, playerWords[firstWord].start, size);
    commandString[size] = '\0';
    return commandString;
}


/*======================================================================*/
void clearWordList(Word list[]) {
    implementationOfSetEndOfArray((Aword *)list);
}
