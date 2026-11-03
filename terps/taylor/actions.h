//
//  actions.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#ifndef actions_h
#define actions_h

#include "taylor.h"

void RunStatusTable(void);
void RunEventTable(void);
void QP3DrawExtraImages(void);
void Goto(unsigned char destination);
void Okay(void);

#ifdef DEBUG
void InitActionDebugging(void);
#else
#define InitActionDebugging()
#endif

extern int ActionsExecuted;
extern int RecursionGuard;

#endif /* actions_h */
