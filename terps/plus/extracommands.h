//
//  extracommands.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-28.
//

#ifndef extracommands_h
#define extracommands_h

#include <stdio.h>

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

ExtraCommandResult PerformExtraCommand(int command, int nextcommand);

#endif /* extracommands_h */
