//
//  opcodes.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  The table of action commands and their operands.
//

#include <stddef.h>

#include "opcodes.h"

#define NUM OPERAND_NUMBER
#define CNT OPERAND_COUNTER
#define ITEM OPERAND_ITEM
#define NOUN OPERAND_NOUN

// clang-format off

/* The operand kinds are those of the MS-DOS interpreters, which are alike
   in X-Men, Fantastic Four and Spider-Man. Numbers 87 and 96 are not
   commands in any of them. */
static const CommandInfo Commands[CMD_FIRST_LONG_MESSAGE] = {
    [CMD_NOP]                   = { "NOP" },
    [CMD_GET]                   = { "GET",        { ITEM } },
    [CMD_DROP]                  = { "DROP",       { ITEM } },
    [CMD_GOTO]                  = { "GOTO",       { NUM } },            /* room */
    [CMD_MEANS]                 = { "MEANS",      { NUM, NUM } },       /* verb, noun */
    [CMD_LIGHTS_OUT]            = { "LIGHTSOUT" },
    [CMD_LIGHTS_ON]             = { "LIGHTSON" },
    [CMD_SET_FLAG]              = { "SET",        { NUM } },
    [CMD_DESTROY]               = { "DESTROY",    { ITEM } },
    [CMD_CLEAR_FLAG]            = { "CLEAR",      { NUM } },
    [CMD_DEATH]                 = { "DEATH" },
    [CMD_PUT_IN_ROOM]           = { "DROPIN",     { ITEM, NUM } },      /* item, room */
    [CMD_GAME_OVER]             = { "GAMEOVER" },
    [CMD_NOUN_TO_COUNTER]       = { "SETCNT",     { NOUN, CNT } },
    [CMD_MULTIPLY]              = { "MULTIPLY",   { CNT, CNT } },
    [CMD_INVENTORY]             = { "INVENTORY" },
    [CMD_SET_FLAG0]             = { "SET0FLAG" },
    [CMD_CLEAR_FLAG0]           = { "CLEAR0FLAG" },
    [CMD_DONE]                  = { "DONE" },
    [CMD_CLEAR_SCREEN]          = { "CLS" },
    [CMD_SAVE]                  = { "SAVE" },
    [CMD_SWAP_ITEMS]            = { "SWAP",       { ITEM, ITEM } },
    [CMD_CONTINUE]              = { "CONTINUE" },
    [CMD_TAKE]                  = { "STEAL",      { ITEM } },
    [CMD_PUT_WITH]              = { "SAME",       { ITEM, ITEM } },
    [CMD_LOOK]                  = { "LOOK" },
    [CMD_DECREMENT_CURRENT]     = { "DECCUR" },
    [CMD_PRINT_CURRENT]         = { "PRINTCUR" },
    [CMD_SET_CURRENT]           = { "SETCUR",     { NUM } },
    [CMD_ITEM_ROOM_TO_COUNTER]  = { "CNT2LOC",    { ITEM, CNT } },
    [CMD_SWAP_CURRENT]          = { "SWAPCUR",    { CNT } },
    [CMD_ADD_CURRENT]           = { "ADD",        { NUM } },
    [CMD_SUBTRACT_CURRENT]      = { "SUB",        { NUM } },
    [CMD_PRINT_NOUN]            = { "ECHONOUN" },
    [CMD_PRINT_ITEM]            = { "PRINTOBJ",   { ITEM } },
    [CMD_PRINT_ITEM_NOUN]       = { "ECHODICT",   { ITEM } },
    [CMD_PAUSE]                 = { "PAUSE" },
    [CMD_DRAW_ROOM]             = { "REDRAW" },
    [CMD_SHOW_IMAGE]            = { "SHOWIMG",    { NUM } },
    [CMD_DIVIDE]                = { "DIVMOD",     { CNT, CNT, CNT } },  /* divisor, dividend and quotient, remainder */
    [CMD_STOP_SEARCH]           = { "NODOMORE" },
    [CMD_DELAY]                 = { "DELAYQS",    { CNT } },
    [CMD_GET_EXIT]              = { "GETEXIT",    { CNT, CNT, CNT } },  /* result, room, direction */
    [CMD_SET_EXIT]              = { "SETEXIT",    { CNT, CNT, CNT } },  /* room, direction, destination */
    [CMD_SWAP_COUNTERS]         = { "SWAPCNT",    { CNT, CNT } },
    [CMD_COPY_COUNTER]          = { "SETTOC",     { CNT, CNT } },       /* from, to */
    [CMD_SET_COUNTER]           = { "SETTOA",     { NUM, CNT } },
    [CMD_ADD]                   = { "ADDCNT",     { NUM, CNT } },
    [CMD_SUBTRACT]              = { "DECCNT",     { NUM, CNT } },
    [CMD_PRINT_COUNTER]         = { "PRNCNT",     { CNT } },
    [CMD_ABS]                   = { "ABS",        { CNT } },
    [CMD_NEGATE]                = { "NEG",        { CNT } },
    [CMD_AND]                   = { "AND",        { CNT, CNT } },       /* operand, operand and result */
    [CMD_OR]                    = { "OR",         { CNT, CNT } },
    [CMD_XOR]                   = { "XOR",        { CNT, CNT } },
    [CMD_RANDOM]                = { "RAND",       { CNT } },
    [CMD_CLEAR_COUNTER]         = { "CLRCNT",     { CNT } },
    [CMD_SYSTEM]                = { "SYSCMD",     { NUM, NUM } },
    [CMD_START_OVER]            = { "GOTO0" },
    [CMD_LOOP_BEGIN]            = { "BEGIN" },
    [CMD_LOOP_BACK]             = { "LOOPBACK" },
    [CMD_LOOP_POP]              = { "SKIPLP" },
    [CMD_LOOP_POP_AT_BACK]      = { "SKIPLPNXT" },
    [CMD_SEARCH_ON]             = { "DOMORE" },
    [CMD_FOUND_MATCH]           = { "FMATCH" },
    [CMD_COMMAND_DONE]          = { "CMDDONE" },
    [CMD_SET_PROMPT]            = { "SETPROMPT",  { NUM } },            /* message, numbered as a command */
    [CMD_WORDS_TO_COUNTERS]     = { "GETWORDS" },
    [CMD_COUNTERS_TO_WORDS]     = { "SETWORDS" },
    [CMD_PRINT_ITEM_ARTICLE]    = { "PRNOBJART",  { ITEM } },
    [CMD_ADD_COUNTER]           = { "ADDCNTCNT",  { CNT, CNT } },       /* from, to */
    [CMD_LOOP_RESET]            = { "BREAK" },
    [CMD_COPY_FLAG]             = { "CPYFLAG",    { NUM, NUM } },       /* from, to */
    [CMD_COUNTER_FROM_INDIRECT] = { "INDTOCNT",   { CNT, CNT } },
    [CMD_COUNTER_TO_INDIRECT]   = { "CNTTOIND",   { CNT, CNT } },
};

// clang-format on

const CommandInfo *GetCommandInfo(int command)
{
    if (command < 0 || command >= CMD_FIRST_LONG_MESSAGE || Commands[command].name == NULL)
        return NULL;
    return &Commands[command];
}

int NextCommandNumber(const uint8_t *commands, int *cc, int length)
{
    if (*cc > length)
        return 0;
    int value = commands[(*cc)++];
    if ((value & 128) && *cc <= length)
        value = (value & 127) * 256 + commands[(*cc)++];
    return value;
}
