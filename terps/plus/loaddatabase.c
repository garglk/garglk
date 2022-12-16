//
//  loaddatabase.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "gameinfo.h"
#include "loaddatabase.h"
#include "graphics.h"

Synonym *Substitutions;
uint8_t *MysteryValues;


static const char *ConditionList[]={
    "<ERROR>",
    "CARRIED",
    "HERE",
    "PRESENT",
    "AT",
    "ROOMDESC",
    "NOP6",
    "NOP7",
    "NOTZERO",
    "ISZERO",
    "SOMETHING",
    "UNK11",
    "UNK12",
    "NODESTROYED",
    "DESTROYED",
    "CURLE",
    "CURGT",
    "CNTEQCNT",
    "CNTGTCNT", // 18
    "CUREQ", // 19
    "CNTEQ",
    "CNTGTE",
    "CNTZERO",
    "INROOMCNT",
    "NOP24",
    "NOP25",
    "OBJIN",
    "UNK27",
    "NOUNIS",
    "DICTWORD",
    "OBJFLAG",
    "MODE",
    "OUT OF RANGE",
};

struct Keyword
{
    char     *name;
    int      opcode;
    int      count;
};

static const struct Keyword CommandList[] =
{
    {"NOP",           0, 0 },
    {"GET",          52, 1 },
    {"DROP",         53, 1 },
    {"GOTO",         54, 1 },
    {"MEANS",        55, 2 },
    {"LIGHTSOUT",    56, 0 },
    {"LIGHTSON",     57, 0 },
    {"SET",          58, 1 },
    {"DESTROY",      59, 1 },
    {"CLEAR",        60, 1 },
    {"DEATH",        61, 0 },
    {"DROPIN",       62, 2 },
    {"GAMEOVER",     63, 0 },
    {"SETCNT",       64, 2 },
    {"MULTIPLY?",    65, 2 },
    {"INVENTORY",    66, 0 },
    {"SET0FLAG",     67, 0 },
    {"CLEAR0FLAG",   68, 0 },
    {"DONE",         69, 0 },
    {"CLS",          70, 0 },
    {"SAVE",         71, 0 },
    {"SWAP",         72, 2 },
    {"CONTINUE",     73, 0 },
    {"STEAL",        74, 1 },
    {"SAME",         75, 2 },
    {"LOOK",         76, 0 },
    {"DECCUR",       77, 0 },
    {"PRINTCUR",     78, 0 },
    {"SETCUR",       79, 1 },
    {"CNT2LOC",      80, 2 },
    {"SWAPCUR",      81, 1 },
    {"ADD",          82, 1 },
    {"SUB",          83, 1 },
    {"ECHONOUN",     84, 0 },
    {"PRINTOBJ",     85, 1 },
    {"ECHODICT",     86, 1 },
//  {"SWAPLOC?",     87, 0 },
    {"PAUSE",        88, 0 },
    {"REDRAW?",      89, 0 },
    {"SHOWIMG",      90, 1 },
    {"DIVMOD",       91, 3 },
    {"NODOMORE",     92, 0 },
    {"DELAYQS",      93, 1 },
    {"GETEXIT",      94, 3 },
    {"SETEXIT",      95, 3 },
    {"SWAPCNT",      97, 2 },
    {"SETTOC",       98, 2 },
    {"SETTOA",       99, 2 },
    {"ADDCNT",      100, 2 },
    {"DECCNT",      101, 2 },
    {"PRNCNT",      102, 1 },
    {"ABS",         103, 1 },
    {"NEG",         104, 1 },
    {"AND",         105, 2 },
    {"OR",          106, 2 },
    {"XOR",         107, 2 },
    {"RAND",        108, 1 },
    {"CLRCNT",      109, 1 },
    {"SYSCMD",      110, 2 },
    {"GOTO0",       111, 0 },
    {"BEGIN",       112, 0 },
    {"LOOPBACK",    113, 0 },
    {"SKIPLP",      114, 0 },
    {"SKIPLPNXT",   115, 0 },
    {"DOMORE",      116, 0 },
    {"FMATCH",      117, 0 },
    {"DONE",        118, 0 },
    {"SETPROMPT",   119, 1 },
    {"GETWORDS",    120, 0 },
    {"SETWORDS",    121, 0 },
    {"PRNOBJART",   122, 1 },
    {"ADDCNT",      123, 2 },
    {"BREAK",       124, 0 },
    {"CPYFLAG",     125, 2 },
    {"CPY2CNT",     126, 2 },
    {"CNT2CPY",     127, 2 },
    {"PRINTMESS",   128, 1 },
    {"",             -1, 0 }
};


static const char *ReadWholeLine(FILE *f)
{
    char tmp[1024];
    char *t;
    int c;
    int ct = 0;
    do {
        c = fgetc(f);
        if (c == EOF)
            return NULL;
        if (c == '\"') {
            c = fgetc(f);
            if (c == '\"')
                return "\0";
            else
                break;
        }
    } while (c != EOF && (isspace((unsigned char)c) || c == '\"')); // Strip first hyphen
    do {
        if (c == '\n' || c == 10 || c == 13 || c == EOF)
            break;
        /* Pass only ASCII to Glk; the other reasonable option
         * would be to pass Latin-1, but it's probably safe to
         * assume that Scott Adams games are ASCII only.
         */
        else if ((c >= 32 && c <= 126))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
        c = fgetc(f);
    } while (1);
    
    for (int i = ct - 1; i > 0; i--) // Look for last hyphen
        if (tmp[i] == '\"') {
            ct = i + 1;
            break;
        }
    if (ct > 1)
        ct--;
    else
        return "\0";
    tmp[ct] = 0;
    t = MemAlloc(ct + 1);
    memcpy(t, tmp, ct + 1);
    return (t);
}


static char *ReadString(FILE *f, size_t *length)
{
    char tmp[1024];
    char *t;
    int c, nc;
    int ct = 0;
lookdeeper:
    do {
        c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c != '"') {
        goto lookdeeper;
    }
    do {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        if (c == '"') {
            nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"'; /* pdd */
        
        /* Ensure a valid Glk newline is sent. */
        if (c == '\n')
            tmp[ct++] = 10;
        /* Special case: assume CR is part of CRLF in a
         * DOS-formatted file, and ignore it.
         */
        else if (c == 13)
            ;
        /* Pass only ASCII to Glk; the other reasonable option
         * would be to pass Latin-1, but it's probably safe to
         * assume that Scott Adams games are ASCII only.
         */
        else if ((c >= 32 && c <= 126))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    } while (ct < 1000);
    tmp[ct] = 0;
    *length = ct + 1;
    t = MemAlloc((int)*length);
    memcpy(t, tmp, *length);
    return (t);
}

static DictWord *ReadDictWordsPC(FILE *f, int numstrings, int loud) {
    DictWord dictionary[1024];
    DictWord *dw = &dictionary[0];
    char *str = NULL;
    int group = 0;
    int index = 0;
    for (int i = 0; i < numstrings; i++) {
        size_t length;
        str = ReadString(f, &length);
        if (str == NULL || str[0] == '\0')
            continue;
        int lastcomma = 0;
        int commapos = 0;
        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] == ',') {
                while(str[j] == ',') {
                    str[j] = '\0';
                    commapos = j;
                    j++;
                }
                int remaining = commapos - lastcomma;
                if (remaining > 0) {
                    dw->Word = MemAlloc(remaining);
                    memcpy(dw->Word, &str[lastcomma + 1], remaining);
                    dw->Group = group;
                    dw = &dictionary[++index];
                }
                lastcomma = commapos;
            }
        }
        free(str);
        str = NULL;
        group++;
    }
    dictionary[index].Word = NULL;
    int dictsize = (index + 1) * sizeof(DictWord);
    DictWord *finaldict = (DictWord *)MemAlloc(dictsize);
    memcpy(finaldict, dictionary, dictsize);
    if (loud)
        for (int j = 0; dictionary[j].Word != NULL; j++)
            debug_print("Dictionary entry %d: \"%s\", group %d\n", j, finaldict[j].Word, finaldict[j].Group);
    return finaldict;
}

static Synonym *ReadSubstitutions(FILE *f, int numstrings, int loud) {
    Synonym syn[1024];
    Synonym *s = &syn[0];
    
    char *str = NULL;
    char *replace = NULL;
    int index = 0;
    int firstsyn = 0;
    for (int i = 0; i < numstrings; i++) {
        size_t length;
        str = ReadString(f, &length);
        if (loud)
            debug_print("Read synonym string \"%s\"\n", str);
        if (str == NULL || str[0] == '\0')
            continue;
        int lastcomma = 0;
        int commapos = 0;
        int foundrep = 0;
        int nextisrep = 0;
        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] == ',') {
                while(str[j] == ',') {
                    str[j] = '\0';
                    commapos = j;
                    j++;
                }
                if (nextisrep) {
                    foundrep = 1;
                    nextisrep = 0;
                } else if (str[j] == '=') {
                    nextisrep = 1;
                }
                int remaining = commapos - lastcomma - foundrep;
                if (remaining > 0) {
                    if (foundrep) {
                        if (replace) {
                            free(replace);
                        }
                        replace = MemAlloc(remaining);
                        memcpy(replace, &str[lastcomma + 2], remaining);
                        if (loud)
                            debug_print("Found new replacement string \"%s\"\n", replace);
                    } else {
                        s->SynonymString = MemAlloc(remaining);
                        memcpy(s->SynonymString, &str[lastcomma + 1], remaining);
                        if (loud)
                            debug_print("Found new synonym string \"%s\"\n", s->SynonymString);
                        s = &syn[++index];
                    }
                }
                if (foundrep) {
                    for (int k = firstsyn; k < index; k++) {
                        int len = (int)strlen(replace) + 1;
                        syn[k].ReplacementString = MemAlloc(len);
                        memcpy(syn[k].ReplacementString, replace, len);
                        if (loud)
                            debug_print("Setting replacement string of \"%s\" (%d) to \"%s\"\n", syn[k].SynonymString, k, syn[k].ReplacementString);
                    }
                    firstsyn = index;
                    foundrep = 0;
                }
                lastcomma = commapos;
            }
        }
        free(str);
        str = NULL;
    }
    if (replace)
        free(replace);
    syn[index].SynonymString = NULL;
    int synsize = (index + 1) * sizeof(Synonym);
    Synonym *finalsyns = (Synonym *)MemAlloc(synsize);
    
    memcpy(finalsyns, syn, synsize);
    if (loud)
        for (int j = 0; syn[j].SynonymString != NULL; j++)
            debug_print("Synonym entry %d: \"%s\", Replacement \"%s\"\n", j, finalsyns[j].SynonymString, finalsyns[j].ReplacementString);
    return finalsyns;
}

static uint8_t *ReadPlusString(uint8_t *ptr, char **string, size_t *length);

static Synonym *ReadSubstitutionsBinary(uint8_t **startpointer, int numstrings, int loud) {
    Synonym syn[1024];
    Synonym *s = &syn[0];

    uint8_t *ptr = *startpointer;

    char *str = NULL;
    char *replace = NULL;
    int index = 0;
    int firstsyn = 0;
    for (int i = 0; i < numstrings; i++) {
        size_t length;
        ptr = ReadPlusString(ptr, &str, &length);
        while (length == 0 && i == 0)
            ptr = ReadPlusString(ptr, &str, &length);
        if (loud)
            debug_print("Read synonym string %d, \"%s\"\n", i, str);
        if (str == NULL || str[0] == 0)
            continue;
        int lastcomma = 0;
        int commapos = 0;
        int foundrep = 0;
        int nextisrep = 0;
        for (int j = 0; j < length && str[j] != '\0'; j++) {
            if (str[j] == ',') {
                while(str[j] == ',') {
                    str[j] = '\0';
                    commapos = j;
                    j++;
                }
                if (nextisrep) {
                    foundrep = 1;
                    nextisrep = 0;
                } else if (str[j] == '=') {
                    nextisrep = 1;
                }
                int remaining = commapos - lastcomma - foundrep;
                if (remaining > 0) {
                    if (foundrep) {
                        if (replace) {
                            free(replace);
                        }
                        replace = MemAlloc(remaining);
                        memcpy(replace, &str[lastcomma + 2], remaining);
                        if (loud)
                            debug_print("Found new replacement string \"%s\"\n", replace);
                    } else {
                        s->SynonymString = MemAlloc(remaining);
                        memcpy(s->SynonymString, &str[lastcomma + 1], remaining);
                        if (loud)
                            debug_print("Found new synonym string \"%s\"\n", s->SynonymString);
                        s = &syn[++index];
                    }
                }
                if (foundrep) {
                    for (int k = firstsyn; k < index; k++) {
                        int len = (int)strlen(replace) + 1;
                        syn[k].ReplacementString = MemAlloc(len);
                        memcpy(syn[k].ReplacementString, replace, len);
                        if (loud)
                            debug_print("Setting replacement string of \"%s\" (%d) to \"%s\"\n", syn[k].SynonymString, k, syn[k].ReplacementString);
                    }
                    firstsyn = index;
                    foundrep = 0;
                }
                lastcomma = commapos;
            }
        }
        free(str);
        str = NULL;
    }
    if (replace)
        free(replace);
    syn[index].SynonymString = NULL;
    int synsize = (index + 1) * sizeof(Synonym);
    Synonym *finalsyns = (Synonym *)MemAlloc(synsize);

    memcpy(finalsyns, syn, synsize);
    if (loud)
        for (int j = 0; syn[j].SynonymString != NULL; j++)
            debug_print("Synonym entry %d: \"%s\", Replacement \"%s\"\n", j, finalsyns[j].SynonymString, finalsyns[j].ReplacementString);
    *startpointer = ptr;
    return finalsyns;
}

char **Comments;

static void ReadComments(FILE *f, int loud) {
    const char *strings[1024];
    const char *comment;
    int index = 0;
    do {
        comment = ReadWholeLine(f);
        strings[index++] = comment;
    } while (comment != NULL);
    if (index <= 1)
        return;
    else
        index--;
    Comments = MemAlloc((index + 1) * sizeof(char *));
    for (int i = 0; i < index; i++) {
        int len = (int)strlen(strings[i]);
        Comments[i] = MemAlloc(len + 1);
        memcpy(Comments[i], strings[i], len + 1);
        if (loud)
            debug_print("Comment %d:\"%s\"\n", i, Comments[i]);
    }
    Comments[index] = NULL;
}

static int NumberOfArguments(int opcode) {
    int i = 0;
    while (CommandList[i].opcode != -1) {
        if (CommandList[i].opcode == opcode)
            return CommandList[i].count;
        i++;
    }
    return 0;
}

void PrintDictWord(int idx, DictWord *dict) {
    for (int i = 0; dict->Word != NULL; i++) {
        if (dict->Group == idx) {
            debug_print("%s", dict->Word);
            return;
        }
        dict++;
    }
    debug_print("%d", idx);
}

static void PrintCondition(uint8_t condition, uint16_t argument, int *negate, int *mult, int *or) {
    if (condition == 0) {
        debug_print(" %d", argument);
    } else if (condition == 31) {
        if (argument == 1) {
            debug_print(" OR(31) %d ", argument); // Arg 1 == OR
            *or = 1;
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
                *or = 0;
        }
    } else if (condition > 40) {
        debug_print(" Condition %d out of range! Arg:%d\n", condition, argument);
    } else {
        debug_print(" ");
        if (*negate) {
            debug_print("!");
            if (!*mult)
                *negate = 0;
        }
        debug_print("%s(%d) %d", ConditionList[condition], condition, argument);
    }
}


static void PrintDebugMess(int index)
{
    if (index > GameHeader.NumMessages)
        return;
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        debug_print("\"%s\" ", message);
    }
}

static void PrintCommand(uint8_t opcode, uint8_t arg, uint8_t arg2, uint8_t arg3, int numargs) {
    if (opcode > 0 && opcode <= 51) {
        debug_print(" MESSAGE %d ", opcode);
        PrintDebugMess(opcode);
        debug_print("\n");
        return;
    }
    
    int i = 0;
    int found = 0;
    while (CommandList[i].opcode != -1) {
        if (CommandList[i].opcode == opcode) {
            found = 1;
            debug_print(" %s(%d) ", CommandList[i].name, opcode);
            
            if (opcode == 128) {
                arg -= 76;
                debug_print("%d ", arg);
                PrintDebugMess(arg);
            } else if (numargs >= 1) {
                debug_print("%d ", arg);
                if (numargs >= 2) {
                    debug_print("%d ", arg2);
                    if (numargs >= 3) {
                        debug_print("%d ", arg3);
                    }
                }
            }
            
            break;
        }
        i++;
    }
    if (found)
        debug_print("\n");
    else {
        debug_print(" UNKNOWN(%d)\n", opcode);
    }
}

static void DumpActions(void) {
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
            debug_print("(%d)(%d)",ap->Verb, ap->NounOrChance);
        }

        if (Comments && Comments[ct][0] != 0)
            debug_print("\nComment: \"%s\"\n", Comments[ct]);
        else
            debug_print("\n");
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
                PrintCondition(ap->Conditions[i], ap->Conditions[i+1], &negate_condition, &negate_multiple, &or_condition);
            }
        }
        negate_condition = 0;
        negate_multiple = 0;
        debug_print("\n");
        
        for (int i = 0; i <= ap->CommandLength; i++) {
            uint8_t command = ap->Commands[i];
            int numargs = NumberOfArguments(command);
            
            if (numargs > 0 && i < ap->CommandLength - 2 && ap->Commands[i + 1] == 131 && ap->Commands[i + 2] == 230) {
                numargs++;
            } else if (command == 99 && ap->Commands[i + 2] > 63) {
                numargs = 3;
            } else if (command == 131 && ap->Commands[i + 2] > 31) {
                numargs = 3;
            } else if ((command == 82 || command == 119) && ap->Commands[i + 1] > 127) {
                numargs = 2;
            }

            int arg1 = 0;
            if (numargs >= 1)
                arg1 = ap->Commands[i + 1];
            int arg2 = 0;
            if (numargs >= 2)
                arg2 = ap->Commands[i + 2];
            int arg3 = 0;
            if (numargs >= 3)
                arg3 = ap->Commands[i + 3];
            PrintCommand(command, arg1, arg2, arg3, numargs);
            i += numargs;
        }
        debug_print("\n");
        ap++;
    }
}

static uint8_t ReadNum(FILE *f) {
    int num;
    if (fscanf(f, "%d\n", &num) != 1)
        return 0;
    return num;
}

static void ReadAction(FILE *f, Action *ap) {
    uint8_t length = ReadNum(f);
    int i;
    ap->NumWords = length >> 4;
    ap->CommandLength = length & 0xf;
    ap->Verb = ReadNum(f);
    if (ap->Verb & 128) {
        Fatal("2-byte verbs unimplemented\n");
    }
    ap->NounOrChance = ReadNum(f);
    if (ap->NounOrChance & 128) {
        Fatal("2-byte nouns unimplemented\n");
    }
    ap->Words = MemAlloc(ap->NumWords);
    
    for (i = 0; i < ap->NumWords; i++) {
        ap->Words[i] = ReadNum(f);
    }
    
    int reading_conditions = 1;
    int conditions_read = 0;
    uint16_t conditions[1024];
    int condargs = 0;
    while (reading_conditions) {
        uint16_t argcond = ReadNum(f) * 256 + ReadNum(f);
        if (argcond & 0x8000)
            reading_conditions = 0;
        argcond = argcond & 0x7fff;
        uint16_t argument = argcond >> 5;
        uint16_t condition = argcond & 0x1f;
        conditions[condargs++] = condition;
        conditions[condargs++] = argument;
        if (condition != 0)
            conditions_read++;
    }
    ap->Conditions = MemAlloc((condargs + 1) * sizeof(uint16_t));
    for (i = 0; i < condargs; i++)
        ap->Conditions[i] = conditions[i];
    ap->Conditions[condargs] = 255;
    uint8_t commands[1024];
    for (i = 0; i <= ap->CommandLength; i++) {
        commands[i] = ReadNum(f);
    }
    
    ap->Commands = MemAlloc(i);
    memcpy(ap->Commands, commands, i);
}

static void PrintHeaderInfo(Header h)
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
    debug_print("Unknown3 =\t%d\n", h.Unknown2);
}

static int SetGame(const char *id_string, size_t length) {
    for (int i = 0; games[i].title != NULL; i++)
        if (strncmp(id_string, games[i].ID_string, length) == 0) {
            Game = &games[i];
            return 1;
        }
    return 0;
}

int FindAndAddImageFile(char *shortname, struct imgrec *rec) {
    int result = 0;
    char filename[2048];
    int n = sprintf(filename, "%s%s.PAK", DirPath, shortname);
    if (n > 0) {
        FILE *infile=fopen(filename,"rb");
        if (infile) {
            fseek(infile, 0, SEEK_END);
            size_t length = ftell(infile);
            if (length > 0) {
                debug_print("Found and read image file %s\n", filename);
                size_t namelen = strlen(shortname) + 1;
                rec->Filename = MemAlloc(namelen);
                memcpy(rec->Filename, shortname, namelen);
                fseek(infile, 0, SEEK_SET);
                rec->Data = MemAlloc(length);
                rec->Size = fread(rec->Data, 1, length, infile);
                result = 1;
            }
            fclose(infile);
        } else {
            debug_print("Could not find or read image file %s\n", filename);
        }
    }
    return result;
}

int LoadDatabasePlaintext(FILE *f, int loud)
{
    int ni, as, na, nv, nn, nr, mc, pr, tr, lt, mn, trm, adv, prp, ss, unk1, oi, unk2;
    int ct;
    
    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    size_t length;
    char *title = ReadString(f, &length);

    if (!SetGame(title, length)) {
        free(title);
        return UNKNOWN_GAME;
    }
    
    if (fscanf(f, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", &ni, &as, &nn, &nv, &nr, &mc, &pr,
               &mn, &trm, &lt, &prp, &adv, &na, &tr, &ss, &unk1, &oi, &unk2)
        < 10) {
        if (loud)
            debug_print("Invalid database(bad header)\n");
        return UNKNOWN_GAME;
    }
    GameHeader.NumItems = ni;
    Counters[43] = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    GameHeader.ActionSum = as;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumVerbs = nv;
    GameHeader.NumNouns = nn;
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    MyLoc = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;
    GameHeader.NumAdverbs = adv;
    GameHeader.NumPreps = prp;
    GameHeader.Unknown1 = unk1;
    GameHeader.NumSubStr = ss;
    GameHeader.Unknown2 = unk2;
    GameHeader.NumObjImg = oi;
    ObjectImages = (ObjectImage *)MemAlloc(sizeof(ObjectImage) * (oi + 1));
    MysteryValues = MemAlloc(unk2 + 1);

    Counters[35] = nr;
    Counters[34] = trm;
    Counters[42] = mc;
    SetBit(35); // Graphics on

    if (loud) {
        PrintHeaderInfo(GameHeader);
    }
    
    /* Load the actions */
    
    ct = 0;
    ap = Actions;
    if (loud)
        debug_print("Reading %d actions.\n", na);
    while (ct < na + 1) {
        ReadAction(f, ap);
        ap++;
        ct++;
    }
    
    if (loud)
        debug_print("Reading %d verbs.\n", nv);
    Verbs = ReadDictWordsPC(f, GameHeader.NumVerbs + 1, loud);
    if (loud)
        debug_print("Reading %d nouns.\n", nn);
    Nouns = ReadDictWordsPC(f, GameHeader.NumNouns + 1, loud);
    
    rp = Rooms;
    if (loud)
        debug_print("Reading %d rooms.\n", nr);
    for (ct = 0; ct < nr + 1; ct++) {
        if (fscanf(f, "%d,%d,%d,%d,%d,%d,%d,%d\n", &rp->Exits[0], &rp->Exits[1],
                   &rp->Exits[2], &rp->Exits[3], &rp->Exits[4],
                   &rp->Exits[5], &rp->Exits[6], &rp->Image)
            != 8) {
            debug_print("Bad room line (%d)\n", ct);
            //            FreeDatabase();
            return UNKNOWN_GAME;
        }
        rp++;
    }
    
    
    if (loud)
        debug_print("Reading %d messages.\n", mn);
    for (ct = 0; ct < mn + 1; ct++) {
        Messages[ct] = ReadString(f, &length);
        if (loud)
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }
    
    for (ct = 0; ct < nr + 1; ct++) {
        Rooms[ct].Text = Messages[Rooms[ct].Exits[6] - 76];
        if (loud)
            debug_print("Room description of room %d: \"%s\"\n", ct, Rooms[ct].Text);
    }
    
    ct = 0;
    if (loud)
        debug_print("Reading %d items.\n", ni);
    int loc, dictword, flag;
    ip = Items;
    while (ct < ni + 1) {
        ip->Text = ReadString(f, &length);
        if (loud)
            debug_print("Item %d: \"%s\"\n", ct, ip->Text);
        if (fscanf(f, ",%d,%d,%d\n", &loc, &dictword, &flag) != 3) {
            debug_print("Bad item line (%d)\n", ct);
            //            FreeDatabase();
            return UNKNOWN_GAME;
        }
        ip->Location = (uint8_t)loc;
        ip->Dictword = (uint8_t)dictword;
        ip->Flag = (uint8_t)flag;
        if (loud && (ip->Location == CARRIED || ip->Location <= GameHeader.NumRooms))
            debug_print("Location of item %d: %d, \"%s\"\n", ct, ip->Location,
                    ip->Location == CARRIED ? "CARRIED" : Rooms[ip->Location].Text);
        ip->InitialLoc = ip->Location;
        ip++;
        ct++;
    }
    
    if (loud)
        debug_print("Reading %d adverbs.\n", GameHeader.NumAdverbs);
    Adverbs = ReadDictWordsPC(f, GameHeader.NumAdverbs + 1, loud);
    if (loud)
        debug_print("Reading %d prepositions.\n",  GameHeader.NumPreps);
    Prepositions = ReadDictWordsPC(f, GameHeader.NumPreps + 1, loud);
    
    Substitutions = ReadSubstitutions(f, GameHeader.NumSubStr + 1, loud);
    
    ObjectImage *objimg = ObjectImages;
    if (loud)
        debug_print("Reading %d object image values.\n", oi + 1);
    for (ct = 0; ct <= oi; ct++) {
        if (fscanf(f, "%d,%d,%d\n", &objimg->Room, &objimg->Object, &objimg->Image)
            != 3) {
            debug_print("Bad object image line (%d)\n", ct);
            return UNKNOWN_GAME;
        }
        if (loud) {
            debug_print("Object image %d ", ct);
            debug_print("room: %d object: %d image: %d\n", objimg->Room, objimg->Object, objimg->Image);
        }
        objimg++;
    }
    
    if (loud)
        debug_print("Reading %d unknown values.\n", unk2 + 1);
    for (ct = 0; ct < unk2 + 1; ct++) {
        MysteryValues[ct] = ReadNum(f);
        if (MysteryValues[ct] > 255) {
            debug_print("Bad unknown value (%d)\n", ct);
            //            FreeDatabase();
            return UNKNOWN_GAME;
        }
    }
    
    ReadComments(f, loud);
    
    if (loud) {
        debug_print("\nLoad Complete.\n\n");
        DumpActions();
    }
    fclose(f);

    int numimages = Game->no_of_room_images + Game->no_of_item_images + Game->no_of_special_images;

    Images = MemAlloc((numimages + 1) * sizeof(struct imgrec));
    debug_print("\nTotal number of images:%d\n", numimages);


    int i, recidx = 0, imgidx = 0;
    char type = 'R';
    for (i = 0; i < numimages; i++) {
        if (i == Game->no_of_room_images) {
            type = 'B';
            imgidx = 0;
        }
        if (i == Game->no_of_room_images + Game->no_of_item_images) {
            type = 'S';
            imgidx = 0;
        }
        char *shortname = ShortNameFromType(type, imgidx);
        if (FindAndAddImageFile(shortname, &Images[recidx]))
            recidx++;
        free(shortname);
        imgidx++;
    }
    Images[recidx].Filename = NULL;

    CurrentSys = SYS_MSDOS;
    
    return CurrentGame;
}

static uint8_t *ReadPlusString(uint8_t *ptr, char **string, size_t *len)
{
    char tmp[1024];
    uint8_t length = *ptr++;
    if (length == 0 || length == 255) {
        *string = MemAlloc(1);
        (*string)[0] = 0;
        *len = 0;
        return ptr;
    }
    for (int i = 0; i < length; i++) {
        tmp[i] = *ptr++;
        if (tmp[i] == '`')
            tmp[i] = '\"';
    }

    tmp[length] = 0;
    char *t = MemAlloc(length + 1);
    memcpy(t, tmp, length + 1);
    *string = t;
    *len = length;
    return ptr;
}


char **LoadMessages(int numstrings, uint8_t **ptr)
{
    if (numstrings == 0)
        numstrings = 0xffff;
    char *str;
    char **target = MemAlloc(numstrings * sizeof(char *));
    int i;
    for (i = 0; i < numstrings; i++) {
        size_t length;
        *ptr = ReadPlusString(*ptr, &str, &length);
        if (CurrentSys == SYS_ST) {
            while (str[0] == 0 && i != 0)
                *ptr = ReadPlusString(*ptr, &str, &length);
        }
        debug_print("Message %d: \"%s\"\n", i, str);
        target[i] = str;
    }
    return target;
}

uint8_t *SeekToPos(uint8_t *buf, int offset)
{
    if (offset > 0x10000)
        return 0;
    return buf + offset;
}

int SeekIfNeeded(int expected_start, int *offset, uint8_t **ptr)
{
    if (expected_start != FOLLOWS) {
        *offset = expected_start;
        uint8_t *ptrbefore = *ptr;
        *ptr = SeekToPos(mem, *offset);
        if (*ptr == ptrbefore)
            fprintf(stderr, "Seek unnecessary, could have been set to FOLLOWS.\n");
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

void ParseHeader(uint16_t *h, int *ni, int *as, int *nn, int*nv, int *nr, int *mc, int *pr, int *mn, int *trm, int *lt, int *prp, int *adv, int *na, int *tr, int *ss, int *unk1, int *oi, int *unk2)
{
    *ni = h[0];
    *as = h[1];
    *nn = h[2];
    *nv = h[3];
    *nr = h[4];
    *mc = h[5];
    *pr = h[6];
    *mn = h[7];
    *trm = h[8];
    *lt = h[9];
    *prp = h[10];
    *adv = h[11];
    *na = 0;
    *tr = 0;
    *ss = h[12];
    *unk1 = h[15];
    *oi = h[13];
    *unk2 = 0;
}

static uint8_t *ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 16; i++) {
        if (CurrentSys == SYS_ST)
            value = *ptr * 256 + *(ptr + 1);
        else
            value = *ptr + 256 * *(ptr + 1);
        header[i] = value;
        debug_print("Header value %d: %d\n", i, header[i]);
        ptr += 2;
    }
    return ptr - 2;
}

DictWord *ReadDictWords(uint8_t **pointer, int numstrings, int loud) {
    uint8_t *ptr = *pointer;
    DictWord dictionary[1024];
    DictWord *dw = dictionary;
    char *str = NULL;
    int group = 0;
    int index = 0;
    for (int i = 0; i < numstrings; i++) {
        size_t strlength;
        ptr = ReadPlusString(ptr, &str, &strlength);
        while (str[0] == 0)
            ptr = ReadPlusString(ptr, &str, &strlength);
        debug_print("Read dictionary string \"%s\"\n", str);
        int lastcomma = 0;
        int commapos = 0;
        for (int j = 0; j <= strlength && str[j] != '\0'; j++) {
            if (str[j] == ',') {
                while(str[j] == ',') {
                    str[j] = '\0';
                    commapos = j;
                    j++;
                }
                int length = commapos - lastcomma;
                if (length > 0) {
                    dw->Word = MemAlloc(length);
                    memcpy(dw->Word, &str[lastcomma + 1], length);
                    dw->Group = group;
                    debug_print("Dictword %d: %s (%d)\n", index, dw->Word, dw->Group);
                    dw = &dictionary[++index];
                }
                lastcomma = commapos;
            }
        }
        free(str);
        str = NULL;
        group++;
    }
    dictionary[index].Word = NULL;
    int dictsize = (index + 1) * sizeof(DictWord);
    DictWord *finaldict = (DictWord *)MemAlloc(dictsize);
    memcpy(finaldict, dictionary, dictsize);
    *pointer = ptr;
    if (loud)
        for (int j = 0; dictionary[j].Word != NULL; j++)
            debug_print("Dictionary entry %d: \"%s\", group %d\n", j, finaldict[j].Word, finaldict[j].Group);
    return finaldict;
}

int SanityCheckHeader(void)
{
    int16_t v = GameHeader.NumItems;
    if (v < 10 || v > 500)
        return 0;
    v = GameHeader.NumNouns; // Nouns
    if (v < 50 || v > 190)
        return 0;
    v = GameHeader.NumVerbs; // Verbs
    if (v < 30 || v > 190)
        return 0;
    v = GameHeader.NumRooms; // Number of rooms
    if (v < 10 || v > 100)
        return 0;

    return 1;
}

int LoadDatabaseBinary(void)
{
    int ni, as, na, nv, nn, nr, mc, pr, tr, lt, mn, trm, adv, prp, ss, unk1, oi, unk2;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    uint8_t *ptr = mem;

    int offset = 0x32;

    int isSTSpiderman = 0, isSTFF = 0;

    ptr = SeekToPos(mem, offset);
    if (ptr == 0)
        return 0;

#pragma mark header

    ptr = ReadHeader(ptr);

    ParseHeader(header, &ni, &as, &nn, &nv, &nr, &mc, &pr,
                    &mn, &trm, &lt, &prp, &adv, &na, &tr, &ss, &unk1, &oi, &unk2);

    if (CurrentSys == SYS_ST && as == 5159)
        isSTSpiderman = 1;

    if (CurrentSys == SYS_ST && as == 6991)
        isSTFF = 1;

    GameHeader.NumItems = ni;
    Counters[43] = ni;
    GameHeader.ActionSum = as;
    GameHeader.NumVerbs = nv;
    GameHeader.NumNouns = nn;
    GameHeader.NumRooms = nr;

    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    MyLoc = pr;
    GameHeader.Treasures = tr;
    GameHeader.NumPreps = prp;
    GameHeader.NumAdverbs = adv;
    GameHeader.NumSubStr = ss;
    GameHeader.NumObjImg = oi;
    GameHeader.NumMessages = mn;
    GameHeader.TreasureRoom = trm;

    PrintHeaderInfo(GameHeader);

    if (!SanityCheckHeader())
        return 0;

    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    Verbs = MemAlloc(sizeof(char *) * (nv + 1));
    Nouns = MemAlloc(sizeof(char *) * (nn + 1));
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    ObjectImages = (ObjectImage *)MemAlloc(sizeof(ObjectImage) * (oi + 1));
    Messages = MemAlloc(sizeof(char *) * (mn + 1));


    Counters[35] = nr;
    Counters[34] = trm;
    Counters[42] = mc;
    SetBit(35); // Graphics on

#pragma mark actions

    Actions = (Action *)MemAlloc(sizeof(Action) * 500);

    ct = 0;

    Action *PrelActions = (Action *)MemAlloc(sizeof(Action) * 524);

    ap = PrelActions;

    uint8_t *origptr = ptr;

    while (ptr - origptr <= GameHeader.ActionSum) {
        uint8_t length = *ptr++;
        ap->NumWords = length >> 4;
        ap->CommandLength = length & 0xf;
        ap->Verb = *ptr++;
        ap->NounOrChance = *ptr++;

        ap->Words = MemAlloc(ap->NumWords);

        for (int i = 0; i < ap->NumWords; i++) {
            ap->Words[i] = *ptr++;
        }

        int reading_conditions = 1;
        int conditions_read = 0;
        uint16_t conditions[1024];
        int condargs = 0;
        while (reading_conditions) {
            uint8_t hi = *ptr++;
            uint8_t lo = *ptr++;
            uint16_t argcond = hi * 256 + lo;
            if (argcond & 0x8000)
                reading_conditions = 0;
            argcond = argcond & 0x7fff;
            uint16_t argument = argcond >> 5;
            uint16_t condition = argcond & 0x1f;
            conditions[condargs++] = condition;
            conditions[condargs++] = argument;
            if (condition != 0)
                conditions_read++;
        }
        ap->Conditions = MemAlloc((condargs + 1) * sizeof(uint16_t));
        int i;
        for (i = 0; i < condargs; i++)
            ap->Conditions[i] = conditions[i];
        ap->Conditions[condargs] = 255;
        uint8_t commands[1024];
        for (i = 0; i <= ap->CommandLength; i++) {
            commands[i] = *ptr++;
        }

        ap->Commands = MemAlloc(i);
        memcpy(ap->Commands, commands, i);

        ap++;
        ct++;
    }

    GameHeader.NumActions = ct - 1;

    size_t actsize = sizeof(Action) * ct;
    Actions = (Action *)MemAlloc((int)actsize);
    memcpy(Actions, PrelActions, actsize);
    free(PrelActions);

#pragma mark dictionary

    Verbs = ReadDictWords(&ptr, GameHeader.NumVerbs + 1, 1);
    Nouns = ReadDictWords(&ptr, GameHeader.NumNouns + 1, 1);

#pragma mark room connections

    /* The room connections are ordered by direction, not room, so all the North
     * connections for all the rooms come first, then the South connections, and
     * so on. */
    for (int j = 0; j < 8; j++) {
        ct = 0;
        rp = Rooms;

        while (ct < nr + 1) {
            rp->Exits[j] = *ptr++;

            if (CurrentSys == SYS_APPLE2 && j == 7) {
                int adr = rp->Exits[j] * 0x100;
                adr += *ptr++ * 0x10;
                adr *= 0x10;
                debug_print("Room image %d address:%x\n",ct, adr);
                rp->Exits[j] = adr;
            } else if (j > 5) {
                rp->Exits[j] |= *ptr;
                ptr++;
            }

            if (j == 7) {
                rp->Image = rp->Exits[j];
            }

            debug_print("Room %d exit %d: %d\n", ct, j, rp->Exits[j]);
            ct++;
            rp++;
        }
        if (isSTSpiderman || isSTFF)
            ptr++;
    }

    if (isSTSpiderman || isSTFF) {
        ptr -= 2;
    }

#pragma mark messages

    Messages = LoadMessages(GameHeader.NumMessages + 1, &ptr);

    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        if (Rooms[i].Exits[6] == 0)
            Rooms[i].Text = "";
        else
            Rooms[i].Text = Messages[Rooms[i].Exits[6] - 76];
        debug_print("Room description of room %d: \"%s\"\n", i, Rooms[i].Text);
    }

#pragma mark items

    ct = 0;
    ip = Items;
    size_t length;

    if (CurrentSys == SYS_ST && !isSTFF)
        ptr++;

    while (ct < ni + 1) {
        ptr = ReadPlusString(ptr, &ip->Text, &length);
        ip++;
        ct++;
    }

#pragma mark item locations
    if (isSTSpiderman || isSTFF ) {
        ptr++;
    }
    
    for (ct = 0, ip = Items; ct <= ni; ct++, ip++) {
        ip->Location = *ptr++;
        ip->InitialLoc = ip->Location;
    }

    if (CurrentSys == SYS_ST) {
        ptr += 2;
    }

    for (ct = 0, ip = Items; ct <= ni; ct++, ip++) {
        ip->Dictword = *ptr++;
        ip->Dictword += *ptr++ * 256;
    }

    for (ct = 0, ip = Items; ct <= ni; ct++, ip++) {
        ip->Flag = *ptr++;
        ip->Flag += *ptr++ * 256;
        debug_print("Item %d: \"%s\", %d, %d, %d\n", ct, ip->Text, ip->Location, ip->Dictword, ip->Flag);
    }

    if (CurrentSys == SYS_ST)
        ptr -= 1;

    Adverbs = ReadDictWords(&ptr, GameHeader.NumAdverbs + 1, 1);
    Prepositions = ReadDictWords(&ptr, GameHeader.NumPreps + 1, 1);


#pragma mark substitutions

    Substitutions = ReadSubstitutionsBinary(&ptr, GameHeader.NumSubStr + 1, 1);

    char *title = NULL;
    ptr = ReadPlusString(ptr, &title, &length);
    debug_print("Title: %s\n", title);

    if (!SetGame(title, length)) {
        free(title);
        return 0;
    }

    if (isSTFF)
        ptr++;

    for (ct = 0; ct <= oi; ct++)
        ObjectImages[ct].Room = *ptr++;

    if (CurrentSys == SYS_ST && !isSTSpiderman)
        ptr++;

    for (ct = 0; ct <= oi; ct++)
        ObjectImages[ct].Object = *ptr++;

    if (CurrentSys == SYS_ST) {
        ptr++;
        if (!isSTSpiderman)
            ptr++;
    }

    for (ct = 0; ct <= oi; ct++) {
        ObjectImages[ct].Image = *ptr++;
        if (CurrentSys == SYS_APPLE2) {
            int adr = ObjectImages[ct].Image * 0x100;
            adr += *ptr++ * 0x10;
            adr *= 0x10;
            debug_print("Object image %d address:%x\n",ct, adr);
            ObjectImages[ct].Image = adr;
        } else
            ptr++;
    }

    for (ct = 0; ct <= oi; ct++)
        debug_print("ObjectImages %d: room:%d object:%d image:%x\n", ct, ObjectImages[ct].Room, ObjectImages[ct].Object, ObjectImages[ct].Image);

    DumpActions();

    return 1;
}
