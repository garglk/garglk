//
//  scott_opcodes.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Condition and action opcodes of the Scott Adams action tables,
//  and their TI-99/4A bytecode counterparts.
//

#ifndef scott_opcodes_h
#define scott_opcodes_h

/* Condition codes (0-19) — used in action table condition fields */
#define COND_PARAMETER       0
#define COND_CARRIED         1
#define COND_IN_ROOM         2
#define COND_PRESENT         3
#define COND_AT_LOC          4
#define COND_NOT_IN_ROOM     5
#define COND_NOT_CARRIED     6
#define COND_NOT_AT_LOC      7
#define COND_FLAG_SET        8
#define COND_FLAG_CLEAR      9
#define COND_CARRYING_ANY   10
#define COND_CARRYING_NONE  11
#define COND_NOT_PRESENT    12
#define COND_IN_PLAY        13
#define COND_NOT_IN_PLAY    14
#define COND_COUNTER_LE     15
#define COND_COUNTER_GT     16
#define COND_NOT_MOVED      17
#define COND_MOVED          18
#define COND_COUNTER_EQ     19

/* Opcode message ranges: codes 1-51 print that message directly,
   codes 102+ print message (code - EXTENDED_MSG_OFFSET) */
#define FIRST_MESSAGE        1
#define LAST_DIRECT_MESSAGE 51
#define EXTENDED_MSG_BASE  102
#define EXTENDED_MSG_OFFSET 50

/* Action opcodes (52-90) — game state operations.
   Equivalent to corresponding TI99OP_ definitions in ti99_4a_terp.c. */
#define OP_GET_ITEM         52
#define OP_DROP_ITEM        53
#define OP_GOTO_ROOM        54
#define OP_DESTROY_ITEM     55
#define OP_SET_DARK         56
#define OP_SET_LIGHT        57
#define OP_SET_FLAG         58
#define OP_DESTROY_ITEM_2   59
#define OP_CLEAR_FLAG       60
#define OP_DIE              61
#define OP_MOVE_ITEM        62
#define OP_GAME_OVER        63
#define OP_LOOK             64
#define OP_PRINT_SCORE      65
#define OP_LIST_INVENTORY   66
#define OP_SET_FLAG_0       67
#define OP_CLEAR_FLAG_0     68
#define OP_REFILL_LIGHT     69
#define OP_CLEAR_SCREEN     70
#define OP_SAVE             71
#define OP_SWAP_ITEMS       72
#define OP_CONTINUE         73
#define OP_FORCE_CARRY      74
#define OP_MOVE_TO_LOC_OF   75
#define OP_LOOK_2           76
#define OP_DEC_COUNTER      77
#define OP_PRINT_COUNTER    78
#define OP_SET_COUNTER      79
#define OP_GOTO_STORED      80
#define OP_SWAP_COUNTER     81
#define OP_ADD_COUNTER      82
#define OP_SUB_COUNTER      83
#define OP_PRINT_NOUN       84
#define OP_PRINTLN_NOUN     85
#define OP_NEWLINE          86
#define OP_SWAP_ROOM        87
#define OP_DELAY            88
#define OP_GAME_SPECIFIC    89
#define OP_DRAW_IMAGE       90

/* --- TI-99/4A bytecode opcode definitions ---
   Conditions (183–201): each takes one parameter byte.
   Most are equivalent to the COND_XXX conditions above
   (but with different numbers). */
#define TI99CND_CARRIED        183
#define TI99CND_IN_ROOM        184
#define TI99CND_PRESENT        185
#define TI99CND_NOT_IN_ROOM    186
#define TI99CND_NOT_CARRIED    187
#define TI99CND_NOT_PRESENT    188
#define TI99CND_IN_PLAY        189
#define TI99CND_NOT_IN_PLAY    190
#define TI99CND_AT_LOC         191
#define TI99CND_NOT_AT_LOC     192
#define TI99CND_FLAG_SET       193
#define TI99CND_FLAG_CLEAR     194
#define TI99CND_CARRYING_ANY   195
#define TI99CND_CARRYING_NONE  196
#define TI99CND_COUNTER_LE     197
#define TI99CND_COUNTER_GT     198
#define TI99CND_COUNTER_EQ     199
#define TI99CND_NOT_MOVED      200
#define TI99CND_MOVED          201

/* Commands (212–254), equivalent to the OP_XXX opcodes above
   (but with different numbers). */
#define TI99OP_CLEAR_SCREEN     212
#define TI99OP_DRAW             213
#define TI99OP_AUTO_INV_ON      214
#define TI99OP_AUTO_INV_OFF     215
#define TI99OP_SUCCESS_OFF      216
#define TI99OP_SUCCESS_ON       217
#define TI99OP_TRY              218
#define TI99OP_GET_ITEM         219
#define TI99OP_DROP_ITEM        220
#define TI99OP_GOTO_ROOM        221
#define TI99OP_DESTROY_ITEM     222
#define TI99OP_SET_DARK         223
#define TI99OP_SET_LIGHT        224
#define TI99OP_SET_FLAG         225
#define TI99OP_CLEAR_FLAG       226
#define TI99OP_SET_FLAG_0       227
#define TI99OP_CLEAR_FLAG_0     228
#define TI99OP_DIE              229
#define TI99OP_MOVE_ITEM        230
#define TI99OP_GAME_OVER        231
#define TI99OP_PRINT_SCORE      232
#define TI99OP_LIST_INVENTORY   233
#define TI99OP_REFILL_LIGHT     234
#define TI99OP_SAVE             235
#define TI99OP_SWAP_ITEMS       236
#define TI99OP_FORCE_CARRY      237
#define TI99OP_MOVE_TO_LOC_OF   238
#define TI99OP_CLEAR            239
#define TI99OP_LOOK             240
#define TI99OP_LOOK_2           241
#define TI99OP_INC_COUNTER      242
#define TI99OP_DEC_COUNTER      243
#define TI99OP_PRINT_COUNTER    244
#define TI99OP_SET_COUNTER      245
#define TI99OP_ADD_COUNTER      246
#define TI99OP_SUB_COUNTER      247
#define TI99OP_GOTO_STORED      248
#define TI99OP_SWAP_ROOM        249
#define TI99OP_SWAP_COUNTER     250
#define TI99OP_PRINT_NOUN       251
#define TI99OP_PRINTLN_NOUN     252
#define TI99OP_NEWLINE          253
#define TI99OP_DELAY            254
#define TI99OP_SUCCESS          255

#endif /* scott_opcodes_h */
