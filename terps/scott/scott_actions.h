/*
 *  scott_actions.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Declarations for game state operations and the action engine.
 */

#ifndef scott_actions_h
#define scott_actions_h

#include "scott_defines.h"

/* Define DEBUG_ACTIONS to trace every condition and opcode as it runs */
#ifdef DEBUG_ACTIONS
#define ACT_DEBUG(...) debug_print(__VA_ARGS__)
#else
#define ACT_DEBUG(...) ((void)0)
#endif

void ClearBitFlag(int bit);
int ConditionHolds(int condition, int arg);
int CountCarried(void);
void Delay(float seconds);
void GameOver(void);
void GoTo(int loc);
void GoToStoredLoc(void);
int ItemExists(int item);
void MoveItemAToLocOfItemB(int itemA, int itemB);
ExplicitResultType PerformActions(int vb, int no);
void OutputNumber(int a);
void PlayerIsDead(void);
void PrintCounter(void);
void PrintMessage(int index);
void PrintNoun(void);
int PrintScore(void);
void PutItemAInRoomB(int itemA, int roomB);
int RandomPercent(int n);
void RefillLight(void);
void SetBitFlag(int bit);
int TestBitFlag(int bit);
void SetDark(void);
void SetItemLocation(int item, int loc);
void SetLight(void);
void SwapCounters(int index);
void SwapItemLocations(int itemA, int itemB);
void SwapLocAndRoomflag(int index);
int TakeOrDropSource(int verb);

#endif /* scott_actions_h */
