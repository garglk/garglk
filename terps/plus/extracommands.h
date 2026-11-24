/*
 *  extracommands.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-06-28.
 */

#ifndef extracommands_h
#define extracommands_h

#include "definitions.h"

typedef enum {
    COM_IT = 1,
    COM_AGAIN = 2,
    COM_WHERE = 4,
    COM_UNDO = 5,
    COM_TRANSCRIPT = 6,
    COM_TRANSCRIPT_OFF = 7,
    COM_RESTART = 8,
    COM_RAMSAVE = 9,
    COM_RAMRESTORE = 10,
    COM_ON = 11,
    COM_OFF = 12,
    COM_MAND = 13,
    COM_LOAD = 14,
    COM_SAVE = 15,
    COM_RAM = 16,
    COM_GAME = 17,
} ExtraWordType;

typedef enum {
    RESULT_AGAIN,
    RESULT_ONE_WORD,
    RESULT_TWO_WORDS,
    RESULT_NOT_UNDERSTOOD
} ExtraCommandResult;

extern DictWord ExtraWords[];

/* Carry out one of the interpreter's own commands, which are not in the game's
   database: AGAIN, UNDO, RESTART, TRANSCRIPT and the RAM saves. command and
   nextcommand are word numbers from ExtraWords, nextcommand being the word
   after it, or 0. RESULT_NOT_UNDERSTOOD leaves the input to the normal parser,
   so the game's own verbs can use the same words. */
ExtraCommandResult PerformExtraCommand(int command, int nextcommand);

#endif /* extracommands_h */
