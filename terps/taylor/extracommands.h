//
//  extracommands.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

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
