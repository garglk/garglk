//
//  opcodes.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  The conditions and commands of the action table: their numbers, and
//  for each command its name and what operands follow it. PerformLine()
//  and the debug dump both read a line of commands through this table.
//

#ifndef opcodes_h
#define opcodes_h

#include <stdint.h>

/* The conditions of an action. Each comes with one argument; those marked
   "+ arg" take a second one from a COND_ARGUMENT entry that follows. */
typedef enum {
    COND_ARGUMENT = 0,          /* A further argument of the condition before */
    COND_CARRIED = 1,           /* Item is carried */
    COND_HERE = 2,              /* Item is in the room */
    COND_PRESENT = 3,           /* Item is carried or in the room */
    COND_AT = 4,                /* Player is in room */
    COND_ROOM_DESCRIPTION = 5,  /* Room in counter is described by message (+ arg) */
    COND_FLAG_SET = 8,
    COND_FLAG_CLEAR = 9,
    COND_CARRYING_ANYTHING = 10,
    COND_IN_PLAY = 13,          /* Item is not in room 0 */
    COND_NOT_IN_PLAY = 14,
    COND_CURRENT_LE = 15,       /* Current counter <= number */
    COND_CURRENT_GT = 16,
    COND_COUNTERS_EQUAL = 17,   /* Counter == counter (+ arg) */
    COND_COUNTER_GT_COUNTER = 18, /* Counter > counter (+ arg) */
    COND_CURRENT_EQ = 19,       /* Current counter == number */
    COND_COUNTER_EQ = 20,       /* Counter == number (+ arg) */
    COND_COUNTER_GE = 21,       /* Counter >= number (+ arg) */
    COND_COUNTER_ZERO = 22,
    COND_ITEM_IN_COUNTER_ROOM = 23, /* Item is in the room in counter (+ arg) */
    COND_ITEM_IN_ROOM = 26,     /* Item is in room (+ arg) */
    COND_NOUN_IS = 28,          /* Noun is noun (+ arg) */
    COND_ITEM_NOUN_IS = 29,     /* The noun of item is noun (+ arg) */
    COND_ITEM_FLAG = 30,        /* Item has any of these flag bits (+ arg) */
    COND_CONTROL = 31,          /* How results are combined: a LogicControl */
    NUM_CONDITIONS
} Condition;

/* The commands of an action. Numbers 1 to 51 print the message with that
   number, and numbers from 128 on message number - 76. */
typedef enum {
    CMD_NOP = 0,
    CMD_LAST_SHORT_MESSAGE = 51,
    CMD_GET = 52,
    CMD_DROP = 53,
    CMD_GOTO = 54,
    CMD_MEANS = 55,
    CMD_LIGHTS_OUT = 56,
    CMD_LIGHTS_ON = 57,
    CMD_SET_FLAG = 58,
    CMD_DESTROY = 59,
    CMD_CLEAR_FLAG = 60,
    CMD_DEATH = 61,
    CMD_PUT_IN_ROOM = 62,
    CMD_GAME_OVER = 63,
    CMD_NOUN_TO_COUNTER = 64,
    CMD_MULTIPLY = 65,
    CMD_INVENTORY = 66,
    CMD_SET_FLAG0 = 67,
    CMD_CLEAR_FLAG0 = 68,
    CMD_DONE = 69,
    CMD_CLEAR_SCREEN = 70,
    CMD_SAVE = 71,
    CMD_SWAP_ITEMS = 72,
    CMD_CONTINUE = 73,
    CMD_TAKE = 74,
    CMD_PUT_WITH = 75,
    CMD_LOOK = 76,
    CMD_DECREMENT_CURRENT = 77,
    CMD_PRINT_CURRENT = 78,
    CMD_SET_CURRENT = 79,
    CMD_ITEM_ROOM_TO_COUNTER = 80,
    CMD_SWAP_CURRENT = 81,
    CMD_ADD_CURRENT = 82,
    CMD_SUBTRACT_CURRENT = 83,
    CMD_PRINT_NOUN = 84,
    CMD_PRINT_ITEM = 85,
    CMD_PRINT_ITEM_NOUN = 86,
    CMD_PAUSE = 88,
    CMD_DRAW_ROOM = 89,
    CMD_SHOW_IMAGE = 90,
    CMD_DIVIDE = 91,
    CMD_STOP_SEARCH = 92,
    CMD_DELAY = 93,
    CMD_GET_EXIT = 94,
    CMD_SET_EXIT = 95,
    CMD_SWAP_COUNTERS = 97,
    CMD_COPY_COUNTER = 98,
    CMD_SET_COUNTER = 99,
    CMD_ADD = 100,
    CMD_SUBTRACT = 101,
    CMD_PRINT_COUNTER = 102,
    CMD_ABS = 103,
    CMD_NEGATE = 104,
    CMD_AND = 105,
    CMD_OR = 106,
    CMD_XOR = 107,
    CMD_RANDOM = 108,
    CMD_CLEAR_COUNTER = 109,
    CMD_SYSTEM = 110,
    CMD_START_OVER = 111,
    CMD_LOOP_BEGIN = 112,
    CMD_LOOP_BACK = 113,
    CMD_LOOP_POP = 114,
    CMD_LOOP_POP_AT_BACK = 115,
    CMD_SEARCH_ON = 116,
    CMD_FOUND_MATCH = 117,
    CMD_COMMAND_DONE = 118,
    CMD_SET_PROMPT = 119,
    CMD_WORDS_TO_COUNTERS = 120,
    CMD_COUNTERS_TO_WORDS = 121,
    CMD_PRINT_ITEM_ARTICLE = 122,
    CMD_ADD_COUNTER = 123,
    CMD_LOOP_RESET = 124,
    CMD_COPY_FLAG = 125,
    CMD_COUNTER_FROM_INDIRECT = 126,
    CMD_COUNTER_TO_INDIRECT = 127,
    CMD_FIRST_LONG_MESSAGE = 128
} Command;

/* What a command's messages from CMD_FIRST_LONG_MESSAGE on are offset by:
   the 76 command numbers that come between the two ranges of messages. */
#define LONG_MESSAGE_OFFSET (CMD_FIRST_LONG_MESSAGE - CMD_GET)

/* The first operand of CMD_SYSTEM: which system command it is. The second
   is the counter, item or mode it works on. */
typedef enum {
    SYS_LOAD = 1,          /* ask, and restore a saved game */
    SYS_ADD_SPECIAL = 2,   /* add a special picture to the animation */
    SYS_CLEAR_ANIM = 3,    /* clear the animation buffer */
    SYS_ANIMATE = 4,       /* start the animation */
    SYS_SET_BACKGROUND = 5,/* the special picture behind the animation */
    SYS_BEEP_WAIT = 6,     /* beep and wait for a key */
    SYS_DRAW_ITEM = 7,     /* draw an item picture */
    SYS_DRAW_ROOM = 8,     /* draw a room picture */
    SYS_SWAP_ITEM_FLAGS = 9, /* swap an item's flags with the item register */
    SYS_ADD_ITEM = 10,     /* add an item picture to the animation */
    SYS_ADD_ROOM = 11,     /* add a room picture to the animation */
    SYS_NEXT_ITEM = 12     /* the next item at a place, see NextItemAt */
} SysCommandType;

/* What an operand of a command is. All are read the same way; the kind
   says what ARG_NOUN, ARG_NOUN2 and ARG_WITH_LIST turn into, which is
   what the MS-DOS interpreters do for each command. */
typedef enum {
    OPERAND_NONE = 0,
    OPERAND_NUMBER,  /* The number as it is: a room, flag, message, value... */
    OPERAND_COUNTER, /* The number of a counter */
    OPERAND_ITEM,    /* An item; the three special numbers are the items of the input */
    OPERAND_NOUN     /* A noun; the three special numbers are the nouns of the input */
} OperandKind;

#define MAX_OPERANDS 3

typedef struct {
    const char *name; /* NULL: not a command */
    uint8_t operands[MAX_OPERANDS];
} CommandInfo;

/* The table entry of a command number, or NULL if there is no such
   command (which includes the numbers that print a message). */
const CommandInfo *GetCommandInfo(int command);

/* Read the number at commands[*cc], an opcode or an operand, and step
   over it. A byte of 128 or more is the high byte of a two-byte number.
   The last byte of the commands is commands[length]; past it the number
   is 0. */
int NextCommandNumber(const uint8_t *commands, int *cc, int length);

#endif /* opcodes_h */
