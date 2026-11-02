/* ti99_4a_terp.c — bytecode interpreter for TI-99/4A Scott Adams games.
 *
 * TI-99/4A adventures use a different bytecode encoding than the
 * standard Scott Adams format.  Opcodes 183–201 are condition checks
 * (each takes one parameter byte: an item, room, flag, or counter
 * index).  Opcodes 212–254 are commands (actions).  Opcode 255 marks
 * success (end of block).  Opcodes 0–182 print the corresponding
 * message.
 *
 * The "try" mechanism (opcode 218) implements if/else branching:
 * it pushes a fallback offset onto a stack; if any subsequent
 * condition fails, execution resumes at that offset instead of
 * aborting the entire action.
 *
 * See https://andwj.gitlab.io/ti99_specs for details.
 */


#include "glk.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"

#include "load_ti99_4a.h"
#include "ti99_4a_terp.h"

/* Maps the TI-99/4A condition opcodes (TI99CND_CARRIED to
   TI99CND_MOVED, in order) to the equivalent COND_XXX codes, so that
   both interpreters share ConditionHolds(). */
static const int ti99_conditions[] = {
    COND_CARRIED,
    COND_IN_ROOM,
    COND_PRESENT,
    COND_NOT_IN_ROOM,
    COND_NOT_CARRIED,
    COND_NOT_PRESENT,
    COND_IN_PLAY,
    COND_NOT_IN_PLAY,
    COND_AT_LOC,
    COND_NOT_AT_LOC,
    COND_FLAG_SET,
    COND_FLAG_CLEAR,
    COND_CARRYING_ANY,
    COND_CARRYING_NONE,
    COND_COUNTER_LE,
    COND_COUNTER_GT,
    COND_COUNTER_EQ,
    COND_NOT_MOVED,
    COND_MOVED,
};

/* Bytecode bytes 1-182 are "print message N" (modulo the range split
   handled by EXTENDED_MSG_*); 183+ are conditions or commands. */
#define MAX_MESSAGE_OPCODE  182

/* Maximum nesting depth for TRY blocks before we give up. */
#define MAX_TRY_DEPTH        32

static int NextByte(const uint8_t **ip)
{
    return *(*ip)++;
}

/* The opcodes that only change the game state or print something: all
   but the conditions and the ones that end the line or open a try block,
   which PerformTI99Line() handles itself. *ip is the next bytecode byte,
   and is moved past the parameters the opcode takes. */
static void ExecuteTI99Opcode(int opcode, const uint8_t **ip)
{
    int second_param;

    switch (opcode) {
    case TI99OP_CLEAR_SCREEN:
        /* No known TI-99/4A game actually uses this opcode. */
        glk_window_clear(Bottom);
        break;

    case TI99OP_DRAW:
        /* Does nothing in the Adventure cartridge, and takes no
           parameter there: its entry in the opcode table jumps
           straight back to the fetch loop.  No known TI-99/4A game
           uses it. */
        break;

    case TI99OP_AUTO_INV_ON:
        AutoInventory = 1;
        break;

    case TI99OP_AUTO_INV_OFF:
        AutoInventory = 0;
        break;

    case TI99OP_SUCCESS_OFF:
        /* Unimplemented. Not to be confused with TI99OP_SUCCESS or
           ACT_SUCCESS. Indicates that the game ended unsuccessfully
           (e.g. because the player died).
           The GAME_OVER opcode on the original TI99/4A interpreter
           will change the color of the screen to indicate whether the
           game was successful or not. These two TI99OP_SUCCESS_XXX
           opcodes are often found just before a game_over to indicate
           either success or failure (i.e. which color to use). This is
           not implemented here. */
        /* FALLTHROUGH */
    case TI99OP_SUCCESS_ON:
        /* Unimplemented. Not to be confused with TI99OP_SUCCESS or
           ACT_SUCCESS. Indicates that the game ended successfully,
           i.e. the player has won. See above for more details. */
        break;

    case TI99OP_DROP_ITEM:
        ACT_DEBUG("item %d (\"%s\") is now in location.\n", **ip,
            Items[**ip].Text);
        SetItemLocation(NextByte(ip), MyLoc);
        should_look_in_transcript = 1;
        break;

    case TI99OP_GOTO_ROOM:
        GoTo(NextByte(ip));
        break;

    case TI99OP_DESTROY_ITEM:
        ACT_DEBUG("Item %d (%s) is removed from the game (put in room 0).\n",
            **ip, Items[**ip].Text);
        SetItemLocation(NextByte(ip), 0);
        break;

    case TI99OP_SET_DARK:
        SetDark();
        break;

    case TI99OP_SET_LIGHT:
        SetLight();
        break;

    case TI99OP_SET_FLAG:
        SetBitFlag(NextByte(ip));
        break;

    case TI99OP_CLEAR_FLAG:
        ClearBitFlag(NextByte(ip));
        break;

    case TI99OP_SET_FLAG_0:
        SetBitFlag(0);
        break;

    case TI99OP_CLEAR_FLAG_0:
        ClearBitFlag(0);
        break;

    case TI99OP_DIE:
        /* The DIE opcode in the TI99/4A interpreter changes the
           screen color to red. This is not implemented.
           Dying does not end the game: the player is moved to the
           last room (limbo) with the light on, and the action
           carries on.  Games that want to stop follow this with
           TI99OP_GAME_OVER; the others let the player try to get
           out of limbo. */
        PlayerIsDead();
        break;

    case TI99OP_MOVE_ITEM:
        second_param = NextByte(ip);
        PutItemAInRoomB(NextByte(ip), second_param);
        break;

    case TI99OP_LIST_INVENTORY:
        ListInventory(0);
        StopTime = SKIP_IMPLICIT_TURNS;
        break;

    case TI99OP_REFILL_LIGHT:
        RefillLight();
        break;

    case TI99OP_SAVE:
        SaveGame();
        StopTime = SKIP_IMPLICIT_TURNS;
        break;

    case TI99OP_SWAP_ITEMS:
        second_param = NextByte(ip);
        SwapItemLocations(second_param, NextByte(ip));
        break;

    case TI99OP_FORCE_CARRY:
        ACT_DEBUG("Player now carries item %d (%s).\n", **ip,
            Items[**ip].Text);
        SetItemLocation(NextByte(ip), CARRIED);
        should_look_in_transcript = 1;
        break;

    case TI99OP_MOVE_TO_LOC_OF:
        second_param = NextByte(ip);
        MoveItemAToLocOfItemB(second_param, NextByte(ip));
        break;

    case TI99OP_CLEAR:
        /* Not to be confused with TI99OP_CLEAR_SCREEN. Unimplemented,
           probably should do nothing.
           We call opcode 239 TI99OP_CLEAR here, since its usage in
           adv01.fiad (Adventureland), adv02.fiad (Pirate Adventure),
           and adv05.fiad (The Count) correspond exactly to the original
           versions of those games. But the converse is not true: the
           original versions use clear in additional places which do
           not correspond to anything in the TI99/4A versions.
           The ScottCom and Bunyon interpreters both consider this a
           no-operation. */
        break;

    case TI99OP_LOOK:
        /* FALLTHROUGH */

    case TI99OP_LOOK_2:
        /* The LOOK_2 opcode (241) only appears once in all the Scott
           Adams games, in adv08.fiad (Pyramid of Doom). It occurs in
           exactly the same place as a look2 opcode in the original
           version of the game. It is probably equivalent to the look
           opcode above. */
        Look();
        should_look_in_transcript = 1;
        break;

    case TI99OP_INC_COUNTER:
        CurrentCounter++;
        break;

    case TI99OP_DEC_COUNTER:
        /* The Adventure cartridge keeps the counter as a signed
           16-bit number and lets it go below zero, here and in
           TI99OP_SUB_COUNTER, unlike the other releases, which
           stop at -1. */
        CurrentCounter--;
        break;

    case TI99OP_PRINT_COUNTER:
        PrintCounter();
        break;

    case TI99OP_SET_COUNTER:
        ACT_DEBUG("CurrentCounter is set to %d.\n", **ip);
        CurrentCounter = NextByte(ip);
        break;

    case TI99OP_ADD_COUNTER:
        ACT_DEBUG("%d is added to currentCounter. Result: %d\n", **ip,
            CurrentCounter + **ip);
        CurrentCounter += NextByte(ip);
        break;

    case TI99OP_SUB_COUNTER:
        CurrentCounter -= NextByte(ip);
        break;

    case TI99OP_GOTO_STORED:
        GoToStoredLoc();
        break;

    case TI99OP_SWAP_ROOM:
        SwapLocAndRoomflag(NextByte(ip));
        break;

    case TI99OP_SWAP_COUNTER:
        SwapCounters(NextByte(ip));
        break;

    case TI99OP_PRINT_NOUN:
        PrintNoun();
        break;

    case TI99OP_PRINTLN_NOUN:
        PrintNoun();
        Output("\n");
        break;

    case TI99OP_NEWLINE:
        Output("\n");
        break;

    case TI99OP_DELAY:
        Delay(1);
        break;

    default:
        if (opcode <= MAX_MESSAGE_OPCODE && opcode <= GameHeader.NumMessages + 1) {
            PrintMessage(opcode);
        } else {
            debug_print("Unknown action %d [Param begins %d %d]\n", opcode, (*ip)[0], (*ip)[1]);
        }
        break;
    }
}

/* Execute a single action line (a sequence of condition checks
   followed by commands).  Returns ACT_SUCCESS if the line ran to
   completion (opcode 255), ACT_FAILURE if a condition failed with
   no remaining try-block fallback, or ACT_GAMEOVER on death/quit.

   action_end is the end of the entire action buffer (implicit or
   explicit), NOT the end of this particular block.  The block length
   byte only governs chain traversal; try-block fallback offsets
   (opcode 218) can legitimately point past the nominal block
   boundary, so the interpreter must be free to read across blocks
   within the buffer. */
static ActionResultType PerformTI99Line(const uint8_t *action_line,
    const uint8_t *action_end)
{
    if (action_line == NULL)
        return ACT_FAILURE;

    const uint8_t *ip = action_line;
    int done = 0;
    int fallback_offset = 0;
    ActionResultType result = ACT_FAILURE;
    int opcode, cond_param;

    /* try-block fallback stack: each "try" opcode pushes an offset
       to resume at if the subsequent conditions fail. */
    int try_depth;
    int try_stack[MAX_TRY_DEPTH];

    try_depth = 0;

    while (done == 0) {
        /* Running off the end of a try block ends that block: drop
           its fallback so a later failing condition cannot jump back
           to it.  Nested blocks end no later than their parents, so
           the innermost one is always on top. */
        while (try_depth > 0 && ip - action_line >= try_stack[try_depth - 1])
            try_depth--;

        if (ip >= action_end)
            return ACT_FAILURE;
        opcode = *(ip++);

        switch (opcode) {
        case TI99CND_CARRIED:
        case TI99CND_IN_ROOM:
        case TI99CND_PRESENT:
        case TI99CND_NOT_IN_ROOM:
        case TI99CND_NOT_CARRIED:
        case TI99CND_NOT_PRESENT:
        case TI99CND_IN_PLAY:
        case TI99CND_NOT_IN_PLAY:
        case TI99CND_AT_LOC:
        case TI99CND_NOT_AT_LOC:
        case TI99CND_FLAG_SET:
        case TI99CND_FLAG_CLEAR:
        case TI99CND_CARRYING_ANY:
        case TI99CND_CARRYING_NONE:
        case TI99CND_COUNTER_LE:
        case TI99CND_COUNTER_GT:
        case TI99CND_COUNTER_EQ:
        case TI99CND_NOT_MOVED:
        case TI99CND_MOVED:
            /* The carrying any/none tests have no parameter byte. */
            if (opcode == TI99CND_CARRYING_ANY || opcode == TI99CND_CARRYING_NONE)
                cond_param = 0;
            else
                cond_param = *(ip++);
            if (!ConditionHolds(ti99_conditions[opcode - TI99CND_CARRIED], cond_param)) {
                done = 1;
                result = ACT_FAILURE;
            }
            break;

        case TI99OP_TRY:
            if (try_depth >= MAX_TRY_DEPTH) {
                Fatal("ERROR Hit upper limit on try method.");
            }
            try_stack[try_depth++] = ip - action_line + *ip;
            ip++;
            break;

        case TI99OP_GET_ITEM:
            if (CountCarried() >= GameHeader.MaxCarry) {
                /* A full inventory behaves like TI99OP_SUCCESS: the
                   whole action ends here, try blocks included. */
                Output(sys[YOURE_CARRYING_TOO_MUCH]);
                done = 1;
                result = ACT_SUCCESS;
                try_depth = 0;
                break;
            } else {
                SetItemLocation(*ip, CARRIED);
            }
            ip++;
            break;

        case TI99OP_GAME_OVER:
            /* In the original interpreter, the GAME_OVER opcode
               changes the screen color to indicate success or failure
               of the whole game: see the TI99OP_SUCCESS_OFF and
               TI99OP_SUCCESS_ON opcodes above. This is not
               implemented. */
            GameOver();
            return ACT_GAMEOVER;

        case TI99OP_PRINT_SCORE:
            if (PrintScore() == 1)
                return ACT_GAMEOVER;
            StopTime = SKIP_IMPLICIT_TURNS;
            break;

        case TI99OP_SUCCESS:
            /* Stops executing the current action, and produces a
               SUCCESS result. This can be used inside a try block:
               the block (and any parent blocks) are immediately
               terminated, similar to a return statement. Outside of a
               try block it also marks the physical end (within the
               file) of the action: the next action will begin
               directly after the 0xFF byte. */
            result = ACT_SUCCESS;
            done = 1;
            try_depth = 0;
            break;

        default:
            ExecuteTI99Opcode(opcode, &ip);
            break;
        }

        /* A condition failed (done == 1) but there is a try-block
           fallback on the stack: pop it and resume execution at the
           alternate code path.  On TI99OP_SUCCESS the line completed,
           so don't retry. */
        if (done == 1 && try_depth > 0) {
            if (opcode == TI99OP_SUCCESS) {
                done = 1;
            } else {
                fallback_offset = try_stack[try_depth - 1];
                try_depth -= 1;
                try_stack[try_depth] = 0;
                done = 0;
                ip = action_line + fallback_offset;
            }
        }
    }

    return result;
}

/* Run all implicit (auto-run) actions once per turn.  Each action
   block is: byte 0 = probability (1–100), byte 1 = block length,
   bytes 2.. = bytecode.  A zero length marks the last block, which
   is still a real action; a zero probability terminates the list. */
void RunImplicitTI99Actions(void)
{
    uint8_t *block = ti99_implicit_actions;

    if (block == NULL)
        return;

    while (block + 1 < ti99_implicit_actions + ti99_implicit_extent) {
        if (block[0] == 0)
            break;

        /* Pass the end of the entire implicit buffer, not the end
           of this block — try-block fallbacks may cross block
           boundaries. */
        if (RandomPercent(block[0]))
            PerformTI99Line(block + 2,
                ti99_implicit_actions + ti99_implicit_extent);

        if (block[1] == 0)
            break;
        block += 1 + block[1];
    }
}

/* Try to execute an explicit (player-triggered) action for the given
   verb/noun pair.  Walks the chain of action blocks for verb_num:
   each block starts with a noun byte (0 = any noun), then a length
   byte, then bytecode.  Returns ER_SUCCESS on the first block that
   runs to completion, ER_RAN_ALL_LINES if at least one block matched
   but none succeeded, or ER_RAN_ALL_LINES_NO_MATCH if the verb has
   no blocks matching this noun. */
ExplicitResultType RunExplicitTI99Actions(int verb_num, int noun_num)
{
    uint8_t *block = VerbActionOffsets[verb_num];
    ExplicitResultType status = ER_NO_RESULT;
    int noun_matched = 0;

    while (status == ER_NO_RESULT) {
        if (block != NULL && (block[0] == noun_num || block[0] == 0)) {
            noun_matched = 1;
            /* Pass the end of the entire explicit buffer — see
               PerformTI99Line's comment on why per-block bounds
               are too restrictive. */
            ActionResultType action_result = PerformTI99Line(block + 2,
                ti99_explicit_actions + ti99_explicit_extent);

            if (action_result == ACT_SUCCESS) {
                return ER_SUCCESS;
            } else {
                if (block[1] == 0)
                    status = ER_RAN_ALL_LINES;
                else
                    block += 1 + block[1];
            }
        } else {
            if (block == NULL || block[1] == 0)
                status = ER_RAN_ALL_LINES_NO_MATCH;
            else
                block += 1 + block[1];
        }
    }

    if (noun_matched)
        status = ER_RAN_ALL_LINES;

    return status;
}
