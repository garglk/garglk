//
//  dumpactions.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Debug dumps of a loaded database. Everything here prints through
//  debug_print(), which compiles to nothing unless DEBUG is defined.
//

#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "dumpactions.h"
#include "opcodes.h"

/* Names of the conditions for DumpActions(). debug_print() compiles to
   nothing unless DEBUG is defined. */
#ifdef DEBUG
static const char * const ConditionList[NUM_CONDITIONS] = {
    [COND_CARRIED] = "CARRIED",
    [COND_HERE] = "HERE",
    [COND_PRESENT] = "PRESENT",
    [COND_AT] = "AT",
    [COND_ROOM_DESCRIPTION] = "ROOMDESC",
    [COND_FLAG_SET] = "NOTZERO",
    [COND_FLAG_CLEAR] = "ISZERO",
    [COND_CARRYING_ANYTHING] = "SOMETHING",
    [COND_IN_PLAY] = "NODESTROYED",
    [COND_NOT_IN_PLAY] = "DESTROYED",
    [COND_CURRENT_LE] = "CURLE",
    [COND_CURRENT_GT] = "CURGT",
    [COND_COUNTERS_EQUAL] = "CNTEQCNT",
    [COND_COUNTER_GT_COUNTER] = "CNTGTCNT",
    [COND_CURRENT_EQ] = "CUREQ",
    [COND_COUNTER_EQ] = "CNTEQ",
    [COND_COUNTER_GE] = "CNTGTE",
    [COND_COUNTER_ZERO] = "CNTZERO",
    [COND_ITEM_IN_COUNTER_ROOM] = "INROOMCNT",
    [COND_ITEM_IN_ROOM] = "OBJIN",
    [COND_NOUN_IS] = "NOUNIS",
    [COND_ITEM_NOUN_IS] = "DICTWORD",
    [COND_ITEM_FLAG] = "OBJFLAG",
};

static const char *ConditionName(int condition)
{
    if (condition < 0 || condition >= NUM_CONDITIONS || ConditionList[condition] == NULL)
        return "UNKNOWN";
    return ConditionList[condition];
}
#endif

/* Prints the first dictionary word whose group matches the given index.
   Falls back to printing the raw numeric index if no match is found. */
void PrintDictWord(int idx, const DictWord *dict)
{
    for (; dict->Word != NULL; dict++) {
        if (dict->Group == idx) {
            debug_print("%s", dict->Word);
            return;
        }
    }
    debug_print("%d", idx);
}

/* Prints a single condition opcode and argument for debug output.
   Condition 0 is a bare argument (extra parameter for the previous condition).
   Condition 31 is a modifier: OR, NOT, parentheses, depending on its argument.
   Tracks negation and OR state across calls via the output parameters. */
static void PrintCondition(uint8_t condition, uint16_t argument, int *negate, int *mult, int *or)
{
    if (condition == COND_ARGUMENT) {
        debug_print(" %d", argument);
    } else if (condition == COND_CONTROL) {
        if (argument == 1) {
            debug_print(" OR(31) %d ", argument); // Arg 1 == OR
            * or = 1;
        } else if (argument == 3 || argument == 4) {
            debug_print(" NOT(31) %d ", argument);
            *negate = 1;
            *mult = (argument != 3);
        } else if (argument == 6) {
            debug_print(" ( ");
        } else if (argument == 7) {
            debug_print(" ) ");
        } else {
            debug_print(" MOD(31) %d ", argument);
            if (argument == 0)
                * or = 0;
        }
    } else {
        debug_print(" ");
        if (*negate) {
            debug_print("!");
            if (!*mult)
                *negate = 0;
        }
        debug_print("%s(%d) %d", ConditionName(condition), condition, argument);
    }
}

/* Prints the text of a message by index, if it exists and is non-empty.
   Used by PrintCommand to show the actual message text alongside opcodes. */
static void PrintDebugMess(int index)
{
    if (index > GameHeader.NumMessages)
        return;
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        debug_print("\"%s\" ", message);
    }
}

/* Prints the command at commands[*cc] with its operands, and steps over
   it. The line is read as PerformLine() reads it: through the command
   table, any number taking two bytes when the first is 128 or more. */
static void PrintCommand(const uint8_t *commands, int *cc, int length)
{
    int command = NextCommandNumber(commands, cc, length);

    if ((command > CMD_NOP && command <= CMD_LAST_SHORT_MESSAGE) || command >= CMD_FIRST_LONG_MESSAGE) {
        if (command >= CMD_FIRST_LONG_MESSAGE)
            command -= LONG_MESSAGE_OFFSET;
        debug_print(" MESSAGE %d ", command);
        PrintDebugMess(command);
        debug_print("\n");
        return;
    }

    const CommandInfo *info = GetCommandInfo(command);
    if (info == NULL) {
        debug_print(" UNKNOWN(%d)\n", command);
        return;
    }

    debug_print(" %s(%d) ", info->name, command);
    for (int i = 0; i < MAX_OPERANDS && info->operands[i] != OPERAND_NONE; i++)
        debug_print("%d ", NextCommandNumber(commands, cc, length));
    debug_print("\n");
}

/* Prints a complete human-readable dump of all loaded actions, including
   verb/noun pairs, extra word parameters, condition chains, and command
   sequences. Annotates entries with comment strings if available. This is
   the main diagnostic tool for verifying that a database was loaded correctly. */
void DumpActions(char **comments)
{
    int negate_condition = 0;
    int negate_multiple = 0;
    int or_condition = 0;
    Action *ap = Actions;
    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        debug_print("\nAction %d: ", ct);
        if (ap->Verb > 0) {
            PrintDictWord(ap->Verb, Verbs);
            debug_print("(%d) ", ap->Verb);
            PrintDictWord(ap->NounOrChance, Nouns);
            debug_print("(%d)", ap->NounOrChance);
        } else {
            debug_print("(%d)(%d)", ap->Verb, ap->NounOrChance);
        }

        if (comments && comments[ct][0] != 0)
            debug_print("\nComment: \"%s\"\n", comments[ct]);
        else
            debug_print("\n");
        /* Print extra word parameters. The top 2 bits of each byte indicate
           the word type: 0=spare, 1=adverb, 2=participle (followed by an
           object word), 3=preposition. The lower 6 bits are the word index. */
        if (ap->NumWords) {
            debug_print("Extra words: ");
            int isobject = 0;
            for (int i = 0; i < ap->NumWords; i++) {
                debug_print("%d ", ap->Words[i]);
                if (isobject) {
                    debug_print("(object:) ");
                    isobject = 0;
                } else {
                    switch (ap->Words[i] >> 6) {
                    case 3:
                        debug_print("(prep:) ");
                        break;
                    case 2:
                        debug_print("(participle:) ");
                        isobject = 1;
                        break;
                    case 1:
                        debug_print("(adverb:) ");
                        break;
                    case 0:
                        debug_print("(spare:) ");
                        break;
                    }
                }
                if (ap->Words[i] <= GameHeader.NumNouns) {
                    debug_print("(");
                    PrintDictWord(ap->Words[i], Nouns);
                    debug_print(") ");
                } else if ((ap->Words[i] & 0x3f) <= GameHeader.NumPreps) {
                    debug_print("(");
                    PrintDictWord((ap->Words[i] & 0x3f), Prepositions);
                    debug_print(") ");
                } else {
                    debug_print("(%d) ", (ap->Words[i] & 0x3f));
                }
            }
            debug_print("\n");
        }
        negate_condition = 0;
        negate_multiple = 0;
        if (ap->Conditions[0] == 0 && ap->Conditions[1] == 0 && ap->Conditions[2] == 255)
            debug_print("No conditions");
        else {
            debug_print("Conditions:");
            for (int i = 0; ap->Conditions[i] != 255; i += 2) {
                PrintCondition(ap->Conditions[i], ap->Conditions[i + 1], &negate_condition, &negate_multiple, &or_condition);
            }
        }
        negate_condition = 0;
        negate_multiple = 0;
        debug_print("\n");

        for (int i = 0; i <= ap->CommandLength;)
            PrintCommand(ap->Commands, &i, ap->CommandLength);
        debug_print("\n");
        ap++;
    }
}

/* Prints all parsed header fields for diagnostic purposes. */
void PrintHeaderInfo(const Header h)
{
    debug_print("Number of items =\t%d\n", h.NumItems);
    debug_print("sum of actions =\t%d\n", h.ActionSum);
    debug_print("Number of nouns =\t%d\n", h.NumNouns);
    debug_print("Number of verbs =\t%d\n", h.NumVerbs);
    debug_print("Number of rooms =\t%d\n", h.NumRooms);
    debug_print("Max carried items =\t%d\n", h.MaxCarry);
    debug_print("Player start location =\t%d\n", h.PlayerRoom);
    debug_print("Number of messages =\t%d\n", h.NumMessages);
    debug_print("Treasure room =\t%d\n", h.TreasureRoom);
    debug_print("Light source turns =\t%d\n", h.LightTime);
    debug_print("Number of prepositions =\t%d\n", h.NumPreps);
    debug_print("Number of adverbs =\t%d\n", h.NumAdverbs);
    debug_print("Number of actions =\t%d\n", h.NumActions);
    debug_print("Number of treasures =\t%d\n", h.Treasures);
    debug_print("Number of synonym strings =\t%d\n", h.NumSubStr);
    debug_print("Unknown1 =\t%d\n", h.Unknown1);
    debug_print("Number of object images =\t%d\n", h.NumObjImg);
    debug_print("Unknown2 =\t%d\n", h.Unknown2);
}
