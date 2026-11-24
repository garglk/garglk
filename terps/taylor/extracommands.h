/*
 *  extracommands.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-04-05.
 */

#ifndef extracommands_h
#define extracommands_h

#include "glk.h"

int TryExtraCommand(void);
/* Whether word is one of the interpreter's own commands (SAVE, UNDO, ...) */
int IsExtraCommand(const char *word);

/* The transcript stream, NULL while none is on. */
extern strid_t Transcript;
/* Closes the transcript and exits. The shared common_utils.c Fatal() calls
   this, so every interpreter provides it. */
void CleanupAndExit(void);

#endif /* extracommands_h */
