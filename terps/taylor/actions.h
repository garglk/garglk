/*
 *  actions.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-04-05.
 */

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
