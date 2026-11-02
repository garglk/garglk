//
//  common.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef common_h
#define common_h

#include "common_utils.h"
#include "common_file_utils.h"
#include "debugprint.h"
#include "definitions.h"
#include "memory_allocation.h"

#include "glk.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void Output(const char *string);
void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);
void Updates(event_t ev);
/* Lets the compiler check the arguments of a printf-style function: the
   format string is argument fmt, the variable arguments start at first. */
#ifdef __GNUC__
#define PLUS_PRINTF(fmt, first) __attribute__((__format__(__printf__, fmt, first)))
#else
#define PLUS_PRINTF(fmt, first)
#endif

/* A printf-style function that prints somewhere */
typedef void (*PrintFunction)(const char *fmt, ...);

void Display(winid_t w, const char *fmt, ...) PLUS_PRINTF(2, 3);

void SetBit(int bit);
void ResetBit(int bit);
int IsSet(int bit);
int IsDark(void);

int CompareUpToHashSign(const char *word1, const char *word2);
int GetDictWord(int group);
int YesOrNo(void);
void OpenTopWindow(void);
void Look(int transcript);
Room *PlayerRoom(void);
void SystemMessage(SysMessageType message);
void AnyKey(int timeout, int message);
void SetTimer(glui32 milliseconds);

extern glui32 TimerRate;
extern glui32 AnimTimerRate;

extern const char *sys[];

extern Synonym *Substitutions;
extern int16_t Counters[];

extern DictWord *Verbs;
extern DictWord *Nouns;
extern DictWord *Adverbs;
extern DictWord *Prepositions;
extern DictWord ExtraWords[];

extern ObjectImage *ObjectImages;
extern char **Messages;
extern Action *Actions;

extern uint64_t BitFlags;

extern winid_t Bottom;

extern char *DirPath;
extern size_t DirPathLength;
extern const char *game_file;

extern Header GameHeader;

extern Item *Items;
extern Room *Rooms;

extern int ProtagonistString;
extern int lastwasnewline;
extern int should_restart;

extern SystemType CurrentSys;
extern const GameInfo *Game;

extern ImgType LastImgType;
extern int LastImgIndex;

extern strid_t Transcript;

extern int LastVerb, LastNoun, LastPrep, LastPartp, LastNoun2, LastAdverb;
extern int CurrentVerb, CurrentNoun, CurrentPrep, CurrentPartp, CurrentNoun2, CurrentAdverb;

extern int showing_inventory;

extern uint8_t *mem;
extern size_t memlen;

extern imgrec *Images;

#endif /* common_h */
