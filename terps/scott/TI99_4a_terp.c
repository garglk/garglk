#include <string.h>

#include "glk.h"
#include "load_TI99_4a.h"
#include "scott.h"
#include "TI99_4a_terp.h"

ActionResultType run_code_chunk(uint8_t *code_chunk)
{
    if (code_chunk == NULL)
        return 1;

    uint8_t *ptr = code_chunk;
    int run_code = 0;
    int index = 0;
    ActionResultType result = ACT_FAILURE;
    int opcode, param, param2;


    int try_index;
    int try[32];

    try_index = 0;
    int temp;

    while (run_code == 0) {
        opcode = *(ptr++);

        switch (opcode) {
        case 183: /* is p in inventory? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Does the player carry %s?\n", Items[*ptr].Text);
#endif
            if (Items[*(ptr++)].Location != CARRIED) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 184: /* is p in room? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s in location?\n", Items[*ptr].Text);
#endif
            if (Items[*(ptr++)].Location != MyLoc) {
                run_code = 1;
                result = ACT_FAILURE;
            }

            break;

        case 185: /* is p available? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s held or in location?\n", Items[*ptr].Text);
#endif
            if (Items[*ptr].Location != CARRIED && Items[*ptr].Location != MyLoc) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            ptr++;
            break;

        case 186: /* is p here? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s NOT in location?\n", Items[*ptr].Text);
#endif
            if (Items[*(ptr++)].Location == MyLoc) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 187: /* is p NOT in inventory? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Does the player NOT carry %s?\n", Items[*ptr].Text);
#endif
            if (Items[*(ptr++)].Location == CARRIED) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 188: /* is p NOT available? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s neither carried nor in room?\n", Items[*ptr].Text);
#endif

            if (Items[*ptr].Location == CARRIED || Items[*ptr].Location == MyLoc) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            ptr++;
            break;

        case 189: /* is p in play? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s (%d) in play?\n", Items[*ptr].Text, dv);
#endif
            if (Items[*(ptr++)].Location == 0) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 190: /* Is object p NOT in play? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s NOT in play?\n", Items[*ptr].Text);
#endif
            if (Items[*(ptr++)].Location != 0) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 191: /* Is player is in room p? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is location %s?\n", Rooms[*ptr].Text);
#endif
            if (MyLoc != *(ptr++)) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 192: /* Is player NOT in room p? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is location NOT %s?\n", Rooms[*ptr].Text);
#endif
            if (MyLoc == *(ptr++)) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 193: /* Is bitflag p clear? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is bitflag %d set?\n", *ptr);
#endif
            if ((BitFlags & (1 << *(ptr++))) == 0) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 194: /* Is bitflag p set? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is bitflag %d NOT set?\n", *ptr);
#endif
            if (BitFlags & (1 << *(ptr++))) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 195: /* Does the player carry anything? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Does the player carry anything?\n");
#endif
            if (CountCarried() == 0) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 196: /* Does the player carry nothing? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Does the player carry nothing?\n");
#endif
            if (CountCarried()) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 197: /* Is CurrentCounter <= p? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is CurrentCounter <= %d?\n", *ptr);
#endif
            if (CurrentCounter > *(ptr++)) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 198: /* Is CurrentCounter > p? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is CurrentCounter > %d?\n", *ptr);
#endif
            if (CurrentCounter <= *(ptr++)) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 199: /* Is CurrentCounter == p? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is current counter == %d?\n", *ptr);
#endif
            if (CurrentCounter != *(ptr++)) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            break;

        case 200: /* Is item p still in initial room? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Is %s still in initial room?\n", Items[*ptr].Text);
#endif
            if (Items[*ptr].Location != Items[*ptr].InitialLoc) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            ptr++;
            break;

        case 201: /* Has item p been moved? */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Has %s been moved?\n", Items[*ptr].Text);
#endif
            if (Items[*ptr].Location == Items[*ptr].InitialLoc) {
                run_code = 1;
                result = ACT_FAILURE;
            }
            ptr++;
            break;

        case 212: /* clear screen */
            glk_window_clear(Bottom);
            break;

        case 214: /* inv */
            AutoInventory = 1;
            break;

        case 215: /* !inv */
            AutoInventory = 0;
            break;

        case 216:
        case 217:
            break;

        case 218:
            if (try_index >= 32) {
                Fatal("ERROR Hit upper limit on try method.\n");
            }
            try[try_index++] = ptr - code_chunk + *ptr;
            ptr++;
            break;

        case 219: /* get item */
            if (CountCarried() == GameHeader.MaxCarry) {
                Output(sys[YOURE_CARRYING_TOO_MUCH]);
                run_code = 1;
                result = ACT_FAILURE;
                break;
            } else {
                Items[*ptr].Location = CARRIED;
            }
            ptr++;
            break;

        case 220: /* drop item */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "item %d (\"%s\") is now in location.\n", *ptr,
                Items[*ptr].Text);
#endif
            Items[*(ptr++)].Location = MyLoc;
            break;

        case 221: /* go to room */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "player location is now room %d (%s).\n", *ptr,
                Rooms[*ptr].Text);
#endif
            MyLoc = *(ptr++);
            Look();
            break;

        case 222: /* move item p to room 0 */
#ifdef DEBUG_ACTIONS
            fprintf(stderr,
                "Item %d (%s) is removed from the game (put in room 0).\n",
                    *ptr, Items[*ptr].Text);
#endif
            Items[*(ptr++)].Location = 0;
            break;

        case 223: /* darkness */
            BitFlags |= 1 << DARKBIT;
            break;

        case 224: /* light */
            BitFlags &= ~(1 << DARKBIT);
            break;

        case 225: /* set flag p */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Bitflag %d is set\n", dv);
#endif
            BitFlags |= (1 << *(ptr++));
            break;

        case 226: /* clear flag p */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Bitflag %d is cleared\n", dv);
#endif
            BitFlags &= ~(1 << *(ptr++));
            break;

        case 227: /* set flag 0 */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Bitflag 0 is set\n");
#endif
            BitFlags |= (1 << 0);
            break;

        case 228: /* clear flag 0 */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Bitflag 0 is cleared\n");
#endif
            BitFlags &= ~(1 << 0);
            break;

        case 229: /* die */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Player is dead\n");
#endif
            Output(sys[IM_DEAD]);
            dead = 1;
            BitFlags &= ~(1 << DARKBIT);
            MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
            stop_time = 1;
            break;

        case 230: /* move item p2 to room p */
            param = *(ptr++);
            param2 = *(ptr++);
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "Item %d (%s) is put in room %d (%s).\n",
                param2, Items[param2].Text, param, Rooms[param].Text);
#endif
            Items[param2].Location = param;
            break;

        case 231: /* quit */
            DoneIt();
            return ACT_SUCCESS;

        case 232: /* print score */
            PrintScore();
            stop_time = 1;
            break;

        case 233: /* list contents of inventory */
            ListInventory();
            stop_time = 1;
            break;

        case 234: /* refill lightsource */
            GameHeader.LightTime = LightRefill;
            Items[LIGHT_SOURCE].Location = CARRIED;
            BitFlags &= ~(1 << LIGHTOUTBIT);
            break;

        case 235: /* save */
            SaveGame();
            stop_time = 1;
            break;

        case 236: /* swap items p and p2 around */
            param = *(ptr++);
            param2 = *(ptr++);
            temp = Items[param].Location;
            Items[param].Location = Items[param2].Location;
            Items[param2].Location = temp;
            break;

        case 237: /* move item p to the inventory */
#ifdef DEBUG_ACTIONS
            fprintf(stderr,
                "Player now carries item %d (%s).\n",
                    *ptr, Items[*ptr].Text);
#endif
            Items[*(ptr++)].Location = CARRIED;
            break;

        case 238: /* make item p same room as item p2 */
            param = *(ptr++);
            param2 = *(ptr++);
            Items[param].Location = Items[param2].Location;
            break;

        case 239: /* nop */
            break;

        case 240: /* look at room */
            Look();
            break;

        case 241: /* unknown */
            break;

        case 242: /* add 1 to current counter */
            CurrentCounter++;
            break;

        case 243: /* sub 1 from current counter */
            if (CurrentCounter >= 1)
                CurrentCounter--;
            break;

        case 244: /* print current counter */
            OutputNumber(CurrentCounter);
            Output(" ");
            break;

        case 245: /* set current counter to p */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "CurrentCounter is set to %d.\n", dv);
#endif
            CurrentCounter = *(ptr++);
            break;

        case 246: /*  add to current counter */
#ifdef DEBUG_ACTIONS
            fprintf(stderr,
                "%d is added to currentCounter. Result: %d\n",
                    *ptr, CurrentCounter + *ptr);
#endif
            CurrentCounter += *(ptr++);
            break;

        case 247: /* sub from current counter */
            CurrentCounter -= *(ptr++);
            if (CurrentCounter < -1)
                CurrentCounter = -1;
            break;

        case 248: /* select room counter */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "switch location to stored location (%d) (%s).\n",
                SavedRoom, Rooms[SavedRoom].Text);
#endif
            temp = MyLoc;
            MyLoc = SavedRoom;
            SavedRoom = temp;
            break;

        case 249: /* swap room counter */
#ifdef DEBUG_ACTIONS
            fprintf(stderr, "swap location<->roomflag[%d]. New location: %s\n", dv, Rooms[RoomSaved[dv]].Text);
#endif
            temp = MyLoc;
            MyLoc = RoomSaved[*ptr];
            RoomSaved[*(ptr++)] = temp;
            Look();
            break;

        case 250: /* swap timer */
#ifdef DEBUG_ACTIONS
            fprintf(stderr,
                "Select a counter. Current counter is swapped with backup "
                "counter %d\n",
                dv);
#endif
            {
                int c1 = CurrentCounter;
                CurrentCounter = Counters[*ptr];
                Counters[*(ptr++)] = c1;
#ifdef DEBUG_ACTIONS
                fprintf(stderr, "Value of new selected counter is %d\n",
                    CurrentCounter);
#endif
                break;
            }

        case 251: /* print noun */
            PrintNoun();
            break;

        case 252: /* print noun + newline */
            PrintNoun();
            Output("\n");
            break;

        case 253: /* print newline */
            Output("\n");
            break;

        case 254: /* delay */
            Delay(1);
            break;

        case 255: /* end of code block. */
            result = 0;
            run_code = 1;
            try_index = 0; /* drop out of all try blocks! */
            break;

        default:
            if (opcode <= 182 && opcode <= GameHeader.NumMessages + 1) {
                const char *message = Messages[opcode];
                if (message != NULL && message[0] != 0) {
                    Output(message);
                    const char lastchar = message[strlen(message) - 1];
                    if (lastchar != 13 && lastchar != 10)
                        Output(sys[MESSAGE_DELIMITER]);
                }
            } else {
                index = ptr - code_chunk;
                fprintf(stderr, "Unknown action %d [Param begins %d %d]\n",
                        opcode, code_chunk[index], code_chunk[index + 1]);
                break;
            }
            break;
        }

        if (dead) {
            DoneIt();
            return ACT_SUCCESS;
        }

        /* we are on the 0xff opcode, or have fallen through */
        if (run_code == 1 && try_index > 0) {
            if (opcode == 0xff) {
                run_code = 1;
            } else {
                /* dropped out of TRY block */
                /* or at end of TRY block */
                index = try[try_index - 1];

                try_index -= 1;
                try[try_index] = 0;
                run_code = 0;
                ptr = code_chunk + index;
            }
        }
    }

    return result;
}

void run_implicit(void)
{
    int probability;
    uint8_t *ptr;
    int loop_flag;

    ptr = ti99_implicit_actions;
    loop_flag = 0;

    /* bail if no auto acts in the game. */
    if (*ptr == 0x0)
        loop_flag = 1;

    while (loop_flag == 0) {
        /*
         p + 0 = chance of happening
         p + 1 = size of code chunk
         p + 2 = start of code
         */

        probability = ptr[0];

        if (RandomPercent(probability))
            run_code_chunk(ptr + 2);

        if (ptr[1] == 0 || ptr - ti99_implicit_actions >= ti99_implicit_extent)
            loop_flag = 1;

        /* skip code chunk */
        ptr += 1 + ptr[1];
    }
}

/* parses verb noun actions */
ExplicitResultType run_explicit(int verb_num, int noun_num)
{
    uint8_t *p;
    ExplicitResultType flag = 1;
    ActionResultType runcode;

    p = VerbActionOffsets[verb_num];

    /* process all code blocks for this verb
     until success or end. */

    flag = ER_NO_RESULT;
    while (flag == ER_NO_RESULT) {
        /* we match VERB NOUN or VERB ANY */
        if (p[0] == noun_num || p[0] == 0) {
            /* we have verb/noun match. run code! */

            runcode = run_code_chunk(p + 2);

            if (runcode == ACT_SUCCESS) {
                return ER_SUCCESS;
            } else { /* failure */
                if (p[1] == 0)
                    flag = ER_RAN_ALL_LINES;
                else
                    p += 1 + p[1];
            }
        } else {
            if (p[1] == 0)
                flag = ER_RAN_ALL_LINES_NO_MATCH;
            else
                p += 1 + p[1];
        }
    }

    return flag;
}
