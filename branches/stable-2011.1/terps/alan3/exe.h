#ifndef _EXE_H_
#define _EXE_H_
/*----------------------------------------------------------------------*\

  exe.h

  Header file for instruction execution unit in Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include <setjmp.h>
#include "sysdep.h"
#include "acode.h"
#include "types.h"
#include "set.h"


/* CONSTANTS */
#define NO_JUMP_RETURN 0
#define ERROR_RETURN 1
#define UNDO_RETURN 2


/* DATA */

/* The text and message file */
extern FILE *textFile;

/* Long jump buffer for restart, errors and undo */
extern jmp_buf restartLabel;
extern jmp_buf returnLabel;
extern jmp_buf forfeitLabel;


/* FUNCTIONS */
extern void sys(Aword fpos, Aword len);
extern void sayInteger(int val);
extern void sayString(char *str);
extern Aptr strip(bool stripFromBeginningNotEnd, int count, bool stripWordsNotChars, int id, int atr);
extern Aptr concat(Aptr s1, Aptr s2);
extern char *getStringFromFile(Aword fpos, Aword len);
extern void print(Aword fpos, Aword len);
extern void setStyle(int style);
extern void showImage(int image, int align);
extern void playSound(int sound);
extern void score(Aword sc);
extern void visits(Aword v);
extern void undo(void);
extern void quitGame(void);
extern void restartGame(void);

extern void use(int act, int scr);
extern void stop(int act);

extern void empty(int cnt, int whr);
extern int getContainerMember(int container, int index, bool directly);
extern int randomInContainer(int cont);

extern void schedule(Aword evt, Aword whr, Aword aft);
extern void cancelEvent(Aword evt);

extern int randomInteger(int from, int to);
extern bool btw(int val, int from, int to);
extern bool contains(Aptr string, Aptr substring);
extern bool streq(char a[], char b[]);

extern void include(int instance, int atr, Aword member);
extern void exclude(int instance, int atr, Aword member);
extern void increase(int instance, int atr, Aword step);
extern void decrease(int instance, int atr, Aword step);

extern void startTranscript();
extern void stopTranscript();

#endif
