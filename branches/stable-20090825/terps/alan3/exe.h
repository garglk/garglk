#ifndef _EXE_H_
#define _EXE_H_
/*----------------------------------------------------------------------*\

  exe.h

  Header file for instruction execution unit in Alan interpreter

\*----------------------------------------------------------------------*/


/* Functions: */
extern void sys(Aword fpos, Aword len);
extern Bool confirm(MsgKind msgno);
extern Aptr getAttribute(AttributeEntry *attributeTable, Aint attributeCode);
extern void setAttribute(AttributeEntry *attributeTable, Aint attributeCode, Aptr value);
extern Aptr attributeOf(Aint instance, Aint atr);
extern void sayInstance(Aint id);
extern void say(Aint instance);
extern void sayForm(Aint instance, SayForm form);
extern void sayInteger(Aword val);
extern void sayString(char *str);
extern Aptr getStringAttribute(Aint id, Aint atr);
extern Aptr strip(Abool stripFromBeginningNotEnd, Aint count, Abool stripWordsNotChars, Aint id, Aint atr);
extern Aptr concat(Aptr s1, Aptr s2);
extern void setStringAttribute(Aint id, Aint atr, char *str);
extern Aptr getSetAttribute(Aint id, Aint atr);
extern void include(Aint id, Aint atr, Aword member);
extern void exclude(Aint id, Aint atr, Aword member);
extern char *getStringFromFile(Aword fpos, Aword len);
extern void print(Aword fpos, Aword len);
extern void setStyle(Aint style);
extern void look(void);
extern void showImage(Aword image, Aword align);
extern void playSound(Aword sound);
extern void setValue(Aint id, Aint atr, Aptr val);
extern void setSetAttribute(Aint id, Aint atr, Aptr set);
extern void increase(Aint id, Aint atr, Aword step);
extern void decrease(Aint id, Aint atr, Aword step);
extern void use(Aword act, Aword scr);
extern void stop(Aword act);
extern void describe(Aint id);
extern void list(Aword cnt);
extern void locate(Aint id, Aword whr);
extern void empty(Aword cnt, Aword whr);
extern void score(Aword sc);
extern void visits(Aword v);
extern void schedule(Aword evt, Aword whr, Aword aft);
extern void cancelEvent(Aword evt);
extern void pushGameState(void);
extern Bool popGameState(void);
extern void quitGame(void);
extern void restartGame(void);
extern void save(void);
extern void restore(void);
extern Aword randomInteger(Aword from, Aword to);
extern Aword randomInContainer(Aint cont);
extern Abool btw(Aint val, Aint from, Aint to);
extern Aptr contains(Aptr string, Aptr substring);
extern Abool streq(char a[], char b[]);
extern Abool at(Aint theInstance, Aint other, Abool directly);
extern Abool in(Aint theInstance, Aint theContainer, Abool directly);
extern Aword where(Aint instance, Abool directly);
extern Aword location(Aint instance);
extern Aint containerSize(Aint where, Abool directly);
extern Aint getContainerMember(Aint container, Aint index, Abool directly);
extern Abool isHere(Aint instance, Abool directly);
extern Abool isNearby(Aint instance, Abool directly);
extern Abool isNear(Aint instance, Aint other, Abool directly);
extern Abool isA(Aint instance, Aint class);

#endif
