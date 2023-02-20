//
//  player.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include <stdarg.h>
#include <strings.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "glkstart.h"
#include "restorestate.h"
#include "decompressz80.h"
#include "extracttape.h"
#include "decrypttotloader.h"
#include "utility.h"
#include "sagadraw.h"
#include "extracommands.h"
#include "c64decrunch.h"
#include "parseinput.h"

#include "taylor.h"

uint8_t Flag[128];
uint8_t ObjectLoc[256];

uint8_t *FileImage = NULL;
uint8_t *EndOfData = NULL;
size_t FileImageLen = 0;
size_t VerbBase;
static size_t TokenBase;
static size_t MessageBase;
static size_t Message2Base;
static size_t RoomBase;
static size_t ObjectBase;
static size_t ExitBase;
static size_t ObjLocBase;
static size_t StatusBase;
static size_t ActionBase;
static size_t FlagBase;
size_t AnimationData = 0;

int NumLowObjects;

static int ActionsExecuted;
static int FoundMatch;
static int PrintedOK;
int Redraw = 0;

#define Location (Flag[0])
#define OtherGuyLoc (Flag[1])
#define OtherGuyInv (Flag[3])
#define TurnsLow (Flag[26])
#define TurnsHigh (Flag[27])
#define ThingAsphyx (Flag[47])
#define TorchAsphyx (Flag[48])
#define DrawImages (Flag[52])
#define Q3SwitchedWatch (Flag[126])

int FirstAfterInput = 0;

int StopTime = 0;
int JustStarted = 1;
int ShouldRestart = 0;

int NoGraphics = 0;

long FileBaselineOffset = 0;

char DelimiterChar = '_';

static char *Filename;
static uint8_t *CompanionFile;

char LastChar = 0;
static int Upper = 0;
int PendSpace = 0;

static int LastNoun = 0;
int LastVerb = 0;
static int GoVerb = 0;

struct GameInfo *Game = NULL;
extern struct GameInfo games[];

strid_t room_description_stream = NULL;

extern int AnimationRunning;

int DeferredGoto = 0;

#ifdef DEBUG

/*
 *	Debugging
 */
static unsigned char WordMap[256][5];

static char *Condition[]={
    "<ERROR>",
    "AT",
    "NOTAT",
    "ATGT",
    "ATLT",
    "PRESENT",
    "HERE",
    "ABSENT",
    "NOTHERE",
    "CARRIED",
    "NOTCARRIED",
    "WORN",
    "NOTWORN",
    "NODESTROYED",
    "DESTROYED",
    "ZERO",
    "NOTZERO",
    "WORD1",
    "WORD2",
    "WORD3",
    "CHANCE",
    "LT",
    "GT",
    "EQ",
    "NE",
    "OBJECTAT",
    "COND26",
    "COND27",
    "COND28",
    "COND29",
    "COND30",
    "COND31",
};

static char *Action[]={
    "<ERROR>",
    "LOAD?",
    "QUIT",
    "INVENTORY",
    "ANYKEY",
    "SAVE",
    "DROPALL",
    "LOOK",
    "OK",		/* Guess */
    "GET",
    "DROP",
    "GOTO",
    "GOBY",
    "SET",
    "CLEAR",
    "MESSAGE",
    "CREATE",
    "DESTROY",
    "PRINT",
    "DELAY",
    "WEAR",
    "REMOVE",
    "LET",
    "ADD",
    "SUB",
    "PUT",		/* ?? */
    "SWAP",
    "SWAPF",
    "MEANS",
    "PUTWITH",
    "BEEP",		/* Rebel Planet at least */
    "REFRESH?",
    "RAMSAVE",
    "RAMLOAD",
    "CLSLOW?",
    "OOPS",
    "DIAGNOSE",
    "SWITCHINVENTORY",
    "SWITCHCHARACTER",
    "CONTINUE",
    "IMAGE",
    "ACT41",
    "ACT42",
    "ACT43",
    "ACT44",
    "ACT45",
    "ACT46",
    "ACT47",
    "ACT48",
    "ACT49",
    "ACT50",
};

static void LoadWordTable(void)
{
    unsigned char *p = FileImage + VerbBase;

    while(1) {
        if(p[4] == 255)
            break;
        if(WordMap[p[4]][0] == 0)
            memcpy(WordMap[p[4]], p, 4);
        p+=5;
    }
}

static void PrintWord(unsigned char word)
{
    if(word == 126)
        fprintf(stderr, "*	  ");
    else if(word == 0 || WordMap[word][0] == 0)
        fprintf(stderr, "%-4d ", word);
    else {
        fprintf(stderr, "%c%c%c%c ",
                WordMap[word][0],
                WordMap[word][1],
                WordMap[word][2],
                WordMap[word][3]);
    }
}

#endif

static char Q3Condition[] = {
    CONDITIONERROR,
    AT,
    NOTAT,
    ATGT,
    ATLT,
    PRESENT,
    ABSENT,
    CARRIED,
    NOTCARRIED,
    NODESTROYED,
    DESTROYED,
    ZERO,
    NOTZERO,
    WORD1,
    WORD2,
    CHANCE,
    LT,
    GT,
    EQ,
    OBJECTAT,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static char Q3Action[]={
    ACTIONERROR,
    SWITCHINVENTORY, /* Swap inventory and dark flag */
    DIAGNOSE, /* Print Reed Richards' watch status message */
    LOADPROMPT,
    QUIT,
    SHOWINVENTORY,
    ANYKEY,
    SAVE,
    DONE, /* Set "condition failed" flag, Flag[118], to 1 */
    GET,
    DROP,
    GOTO,
    SWITCHCHARACTER, /* Go to the location of the other guy */
    SET,
    CLEAR,
    MESSAGE,
    CREATE,
    DESTROY,
    LET,
    ADD,
    SUB,
    PUT,
    SWAP,
    IMAGE, /* Draw image on top of room image */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

size_t FindCode(const char *x, size_t base, size_t len)
{
    unsigned char *p = FileImage + base;
    while(p < FileImage + FileImageLen - len) {
        if(memcmp(p, x, len) == 0)
            return p - FileImage;
        p++;
    }
    return (size_t)-1;
}

static size_t FindFlags(void)
{
    size_t pos;

    /* Questprobe */
    pos = FindCode("\xE7\x97\x51\x95\x5B\x7E\x5D\x7E\x76\x93", 0, 10);
    if(pos == -1) {
        /* Look for the flag initial block copy */
        pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0,8 );
        if(pos == -1) {
            if (Game)
                return Game->start_of_flags + FileBaselineOffset;
            else {
                fprintf(stderr, "Cannot find initial flag data.\n");
                glk_exit();
            }
        } else return pos + 5;
    }
    return pos + 11;
}

static int LooksLikeTokens(size_t pos)
{
    if (pos > FileImageLen)
        return 0;
    unsigned char *p = FileImage + pos;
    int n = 0;
    int t = 0;
    while(n < 512) {
        unsigned char c = p[n] & 0x7F;
        if(c >= 'a' && c <= 'z')
            t++;
        n++;
    }
    if(t > 300)
        return 1;
    return 0;
}

static void TokenClassify(size_t pos)
{
    unsigned char *p = FileImage + pos;
    int n = 0;
    while(n++ < 256) {
        do {
            if(*p == 0x5E || *p == 0x7E)
                Version = 0;
        } while(!(*p++ & 0x80));
    }
}

static size_t FindTokens(void)
{
    size_t addr;
    size_t pos = 0;

    if (Game)
        switch(CurrentGame) {
            case TOT_TEXT_ONLY_64:
            case TOT_HYBRID_64:
                if ((pos = FindCode("\x80\x59\x6f\x75\x20\x61\x72\x65\x20\x69", 0, 10)) != -1)
                    return pos;
                break;
            case TEMPLE_OF_TERROR_64:
                if ((pos = FindCode("\x80\x20\x54\x68\x65\x72\x65\x20\x69\x73", 0, 10)) != -1)
                    return pos;
                break;
            case REBEL_PLANET_64:
                if  ((pos = FindCode("\xa7\x2e\xfe\x20\xfe\x2c\xfe\x21\xfe\x3f", 0, 10)) != -1)
                    return pos;
                break;
            case QUESTPROBE3_64:
                if ((pos = FindCode("\x61\xa0\x64\xa0\x65\xa0\x67\xa0\x69\xa0", 0, 10)) != -1)
                    return pos;
                break;
            case HEMAN_64:
            case KAYLETH_64:
                if ((pos = FindCode("\x80\x59\x6f\x75\x20\x61\x72\x65\x20\x69", 0, 10)) != -1)
                    return pos;
                break;
            default:
                break;
        }

    do {
        pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1, 8);
        if(pos == -1) {
            /* Questprobe */
            pos = FindCode("\x58\x58\x58\x58\xFF", 0, 5);
            if(pos == -1) {
                /* Last resort */
                addr = FindCode("You are in ", 0, 11) - 1;
                if(addr == -1) {
                    if (Game)
                        return Game->start_of_tokens + FileBaselineOffset;
                    fprintf(stderr, "Unable to find token table.\n");
                    return 0;
                }
                return addr;
            } else
                return pos + 6;
        }
        addr = (FileImage[pos-1] <<8 | FileImage[pos-2]) - 0x4000 + FileBaselineOffset;
    } while(LooksLikeTokens(addr) == 0);
    TokenClassify(addr);
    return addr;
}

void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    char msg[2048];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

static void OutWrite(char c)
{
    if(isalpha(c) && Upper)
    {
        c = toupper(c);
        Upper = 0;
    }
    PrintCharacter(c);
}

void OutFlush(void)
{
    if(LastChar)
        OutWrite(LastChar);
    if(PendSpace && LastChar != '\n' && !FirstAfterInput)
        OutWrite(' ');
    LastChar = 0;
    PendSpace = 0;
}

static void OutReset(void)
{
    OutFlush();
}

static int Q3Upper = 0;

void OutCaps(void)
{
    if (LastChar) {
        OutWrite(LastChar);
        LastChar = 0;
    }
    Upper = 1;
    Q3Upper = 1;
}

static int periods = 0;
int JustWrotePeriod = 0;

void OutChar(char c)
{
    if(c == ']')
        c = '\n';

    if (c == '.') {
        periods++;
        if (LastChar == '?' || FirstAfterInput || isspace(LastChar) || LastChar == '.') {
            c = ' ';
            if (LastChar == ' ')
                LastChar = 0;
        }
        PendSpace = 0;
    } else {
        if (periods && !JustWrotePeriod && c !=',' && c != '?' && c != '!') {
            if (periods == 2)
                periods = 1;
            for (int i = 0; i < periods; i++) {
                JustWrotePeriod = 0;
                PrintCharacter('.');
            }
            JustWrotePeriod = 1;
            LastChar = 0;
            PendSpace = 0;
            Upper = 0;
        }
        periods = 0;
    }

    if(c == ' ') {
        PendSpace = 1;
        return;
    }
    if (FirstAfterInput) {
        if (isspace(LastChar)) {
            LastChar = 0;
            Upper = 1;
        } else if (!isspace(c)) {
            FirstAfterInput = 0;
        }
        PendSpace = 0;
    }
    if(LastChar) {
        if (isspace(LastChar))
            PendSpace = 0;
        if (LastChar == '.' && JustWrotePeriod) {
            LastChar = 0;
        } else {
            OutWrite(LastChar);
        }
    }
    if(PendSpace) {
        if (JustWrotePeriod && BaseGame != KAYLETH)
            Upper = 1;
        OutWrite(' ');
        PendSpace = 0;
    }
    LastChar = c;
    if (LastChar == 10 || LastChar == 13) {
        Upper = 1;
        Q3Upper = 1;
    }
}

static void OutReplace(char c)
{
    LastChar = c;
}

static void OutKillSpace()
{
    PendSpace = 0;
}

void OutString(char *p)
{
    while(*p)
        OutChar(*p++);
}

static unsigned char *TokenText(unsigned char n)
{
    unsigned char *p = FileImage + TokenBase;
    if (Version == QUESTPROBE3_TYPE)
        n -= 0x7b;

    while(n > 0 && p < EndOfData) {
        while((*p & 0x80) == 0)
            p++;
        n--;
        p++;
    }
    return p;
}

void Q3PrintChar(uint8_t c) { // Print character
    if (c == 0x0d)
        return;

    if (FirstAfterInput)
        Q3Upper = 1;

    if (Q3Upper && c >= 'a') {
        c -= 0x20; // token is made uppercase
    }
    OutChar(c);
    if (c > '!') {
        Q3Upper = 0;
        Upper = 0;
    }
    if (c == '!' || c == '?' || c == ':' || c == '.') {
        Q3Upper = 1;
    }
}

static void PrintToken(unsigned char n)
{
    if (CurrentGame == BLIZZARD_PASS && MyLoc == 49 && n == 0x2d) {
        OutChar(0x2d);
        return;
    }
    unsigned char *p = TokenText(n);
    unsigned char c;
    do {
        c = *p++;
        if (Version == QUESTPROBE3_TYPE)
            Q3PrintChar(c & 0x7F);
        else
            OutChar(c & 0x7F);
    } while(p < EndOfData && !(c & 0x80));
}

static void Q3PrintText(unsigned char *p, int n)
{
    while (n > 0) {
        while (*p != 0x1f && *p != 0x18) {
            p++;
        }
        n--;
        p++;
    }
    do  {
        if (*p == 0x18)
            return;
        if (*p >= 0x7b) // if c is >= 0x7b it is a token
            PrintToken(*p);
        else
            Q3PrintChar(*p);
    } while (*p++ != 0x1f);
}

static void PrintText1(unsigned char *p, int n)
{
    while(n > 0) {
        while(p < EndOfData && *p != 0x7E && *p != 0x5E)
            p++;
        n--;
        p++;
    }
    while(p < EndOfData && *p != 0x7E && *p != 0x5E)
        PrintToken(*p++);
    if(*p == 0x5E) {
        PendSpace = 1;
    }
}

/*
 *	Version 0 is different
 */

static int InventoryLower = 0;

static void PrintText0(unsigned char *p, int n)
{
    if (p > EndOfData)
        return;
    unsigned char *t = NULL;
    unsigned char c;
    while(1) {
        if(t == NULL)
            t = TokenText(*p++);
        c = *t & 0x7F;
        if(c == 0x5E || c == 0x7E) {
            if(n == 0) {
                if(c == 0x5E) {
                    PendSpace = 1;
                }
                return;
            }
            n--;
        } else if (n == 0) {
            if (InventoryLower) {
                c = tolower(c);
                InventoryLower = 0;
            }
            OutChar(c);
        }
        if(t >= EndOfData || (*t++ & 0x80))
            t = NULL;
    }
}

static void PrintText(unsigned char *p, int n)
{
    if (Version == REBEL_PLANET_TYPE) {	/* In stream end markers */
        PrintText0(p, n);
    } else if (Version == QUESTPROBE3_TYPE) {
        Q3PrintText(p, n);
    } else {			/* Out of stream end markers (faster) */
        PrintText1(p, n);
    }
}

static void Message(unsigned char m)
{
    unsigned char *p = FileImage + MessageBase;
    PrintText(p, m);
    if (CurrentGame != BLIZZARD_PASS)
        OutChar(' ');
    if (BaseGame == REBEL_PLANET && m == 156)
        InventoryLower = 1;
    else
        InventoryLower = 0;
}

static void Message2(unsigned int m)
{
    unsigned char *p = FileImage + Message2Base;
    PrintText(p, m);
    OutChar(' ');
}

void SysMessage(unsigned char m)
{
    if (Version != QUESTPROBE3_TYPE) {
        Message(m);
        return;
    }
    if (m == EXITS)
        m = 217;
    else
        m = 210 + m;
    Message(m);
}

static void PrintObject(unsigned char obj)
{
    if (BaseGame == TEMPLE_OF_TERROR && obj == 41) {
        OutString("door.");
        return;
    }
    unsigned char *p = FileImage + ObjectBase;
    if (Version == QUESTPROBE3_TYPE)
        p--;
    PrintText(p, obj);
}

static void PrintRoom(unsigned char room)
{
    unsigned char *p = FileImage + RoomBase;
    if (CurrentGame == BLIZZARD_PASS && room < 102)
        p = FileImage + 0x18000 + FileBaselineOffset;
    PrintText(p, room);
}


static void PrintNumber(unsigned char n)
{
    char buf[4];
    char *p = buf;
    snprintf(buf, sizeof buf, "%d", (int)n);
    while(*p)
        OutChar(*p++);
}


static unsigned char Destroyed()
{
    return 252;
}

static unsigned char Carried()
{
    return Flag[2];
}

static unsigned char Worn()
{
    if (Version == QUESTPROBE3_TYPE)
        return 0;
    return Flag[3];
}

static unsigned char NumObjects()
{
    if (Version == QUESTPROBE3_TYPE)
        return 49;
    return Flag[6];
}

static int WaitFlag()
{
    if (Version == QUESTPROBE3_TYPE)
        return 5;
    else if (BaseGame != REBEL_PLANET && BaseGame != KAYLETH)
        return -1;
    else
        return 7;
}

static int CarryItem(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 1;
    /* Flag 5: items carried, flag 4: max carried */
    if(Flag[5] == Flag[4] && CurrentGame != BLIZZARD_PASS)
        return 0;
    if(Flag[5] < 255)
        Flag[5]++;
    return 1;
}

static int DarkFlag(void) {
    if (Version == QUESTPROBE3_TYPE)
        return 43;
    return 1;
}

static void DropItem(void)
{
    if(Version != QUESTPROBE3_TYPE && Flag[5] > 0)
        Flag[5]--;
}

static void Put(unsigned char obj, unsigned char loc)
{
    /* Will need refresh logics somewhere, maybe here ? */
    if(ObjectLoc[obj] == MyLoc || loc == MyLoc)
        Redraw = 1;
    ObjectLoc[obj] = loc;
}

static int Present(unsigned char obj)
{
    if (obj >= NumObjects())
        return 0;
    unsigned char v = ObjectLoc[obj];
    if(v == MyLoc|| v == Worn() || v == Carried() || (Version == QUESTPROBE3_TYPE && v == OtherGuyInv && OtherGuyLoc == MyLoc))
        return 1;
    return 0;
}

static int Chance(int n)
{
    unsigned long v = rand() >> 12;
    v%=100;
    if(v > n)
        return 0;
    return 1;
}

void Look(void);

static void NewGame(void)
{
    Redraw = 1;
    memset(Flag, 0, 128);
    memcpy(Flag, FileImage + FlagBase, 7);
    if (Version == QUESTPROBE3_TYPE) {
        for (int i = 0; i < 124; i++) {
            Flag[4 + i] = 0;
        }
        Flag[42] = 0;
        Flag[43] = 0;
        Flag[2] = 254;
        Flag[3] = 253;
    }
    Location = 0;
    memcpy(ObjectLoc, FileImage + ObjLocBase, NumObjects());
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
    Look();
    PrintedOK = 1;
}

static int GetFileLength(strid_t stream) {
    glk_stream_set_position(stream, 0, seekmode_End);
    return glk_stream_get_position(stream);
}

int YesOrNo(void)
{
    while(1) {
        uint8_t c = WaitCharacter();
        if (c == 250)
            c = 0;
        OutChar(c);
        OutChar('\n');
        OutFlush();
        if(c == 'n' || c == 'N')
            return 0;
        if(c == 'y' || c == 'Y')
            return 1;
        OutString("Please answer Y or N.\n");
        OutFlush();
    }
}

int LoadGame(void)
{
        frefid_t fileref = glk_fileref_create_by_prompt (fileusage_SavedGame,
                                                         filemode_Read, 0);
        if (!fileref)
        {
            OutFlush();
            return 0;
        }

        /*
         * Reject the file reference if we're expecting to read from it, and the
         * referenced file doesn't exist.
         */
        if (!glk_fileref_does_file_exist (fileref))
        {
            OutString("Unable to open file.\n");
            glk_fileref_destroy (fileref);
            OutFlush();
            return 0;
        }

        strid_t stream = glk_stream_open_file(fileref, filemode_Read, 0);
        if (!stream)
        {
            OutString("Unable to open file.\n");
            glk_fileref_destroy (fileref);
            OutFlush();
            return 0;
        }

        struct SavedState *state = SaveCurrentState();

        /* Restore saved game data. */

        if (glk_get_buffer_stream(stream, (char *)Flag, 128) != 128 || glk_get_buffer_stream(stream, (char *)ObjectLoc, 256) != 256 || GetFileLength(stream) != 384) {
            RecoverFromBadRestore(state);
        } else {
            glk_window_clear(Bottom);
            Look();
            free(state);
        }
        glk_stream_close (stream, NULL);
        glk_fileref_destroy (fileref);
        return 1;
}

static int LoadPrompt(void)
{
    OutCaps();
    glk_window_clear(Bottom);
    SysMessage(RESUME_A_SAVED_GAME);
    OutFlush();

    if (!YesOrNo()) {
        glk_window_clear(Bottom);
        if (DeferredGoto == 1) {
            Location = 1;
            DeferredGoto = 0;
        }
        return 0;
    } else {
        return LoadGame();
    }
}

static int RecursionGuard = 0;

static void QuitGame(void)
{
    SaveUndo();
    if (LastChar == '\n')
        OutReplace(' ');
    OutFlush();
    Look();
    OutCaps();
    SysMessage(PLAY_AGAIN);
    OutFlush();
    if (YesOrNo()) {
        ShouldRestart = 1;
        StopTime = 2;
        return;
    } else {
        glk_exit();
    }
}

static void Inventory(void)
{
    int i;
    int f = 0;
    if (BaseGame != REBEL_PLANET)
        OutCaps();
    SysMessage(INVENTORY);
    for(i = 0; i < NumObjects(); i++) {
        if(ObjectLoc[i] == Carried() || ObjectLoc[i] == Worn()) {
            f = 1;
            PrintObject(i);
            if(ObjectLoc[i] == Worn()) {
                OutReplace(0);
                SysMessage(NOWWORN);
                if (CurrentGame == REBEL_PLANET) {
                    OutKillSpace();
                    OutFlush();
                    OutChar(',');
                }

            }
        }
    }
    if(f == 0)
        SysMessage(NOTHING);
    else {
        OutReplace('.');
        OutChar(' ');
        OutCaps();
    }
}

static void AnyKey(void) {
    SysMessage(HIT_ENTER);
    OutFlush();
    WaitCharacter();
}

static void Okay(void) {
    SysMessage(OKAY);
    OutChar(' ');
    OutCaps();
    PrintedOK = 1;
}

void SaveGame(void) {

    strid_t file;
    frefid_t ref;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
                                       filemode_Write, 0);
    if (ref == NULL) {
        OutString("Save failed.\n");
        OutFlush();
        return;
    }

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL) {
        OutString("Save failed.\n");
        OutFlush();
        return;
    }

    /* Write game state. */
    glk_put_buffer_stream (file, (char *)Flag, 128);
    glk_put_buffer_stream (file, (char *)ObjectLoc, 256);
    glk_stream_close(file, NULL);
    OutString("Saved.\n");
    OutFlush();
}

static void TakeAll(int start)
{
    if (Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        return;
    }
    int found = 0;
    for (int i = start; i < NumObjects(); i++) {
        if (ObjectLoc[i] == MyLoc) {
            if (found)
                OutChar('\n');
            found = 1;
            PrintObject(i);
            OutReplace(0);
            OutString("......");
            if(CarryItem() == 0) {
                SysMessage(YOURE_CARRYING_TOO_MUCH);
                return;
            }
            OutKillSpace();
            OutString("Taken");
            OutFlush();
            Put(i, Carried());
        }
    }
    if (!found) {
        Message(31);
    }
}

static void DropAll(int loud) {
    int i;
    int found = 0;
    for(i = 0; i < NumObjects(); i++) {
        if(ObjectLoc[i] == Carried() && ObjectLoc[i] != Worn()) {
            if (loud) {
                if (found)
                    OutChar('\n');
                found = 1;
                PrintObject(i);
                OutReplace(0);
                OutString("......");
                OutKillSpace();
                OutString("Dropped");
                OutFlush();
            }
            Put(i, MyLoc);
        }
    }
    if (loud & !found) {
        OutString("You have nothing to drop. ");
    }
    Flag[5] = 0;
}

static int GetObject(unsigned char obj) {
    if(ObjectLoc[obj] == Carried() || ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_HAVE_IT);
        return 0;
    }
    if (!(Version == QUESTPROBE3_TYPE && Flag[1] == MyLoc && ObjectLoc[obj] == Flag[3])) {
        if(ObjectLoc[obj] != MyLoc) {
            SysMessage(YOU_DONT_SEE_IT);
            return 0;
        }
    }
    if(CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return 0;
    }

    Put(obj, Carried());
    return 1;
}

static int DropObject(unsigned char obj) {
    /* FIXME: check if this is how the real game behaves */
    if(ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return 0;
    }
    if(ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return 0;
    }

    DropItem();
    Put(obj, MyLoc);
    return 1;
}

static void ListExits(int caps)
{
    unsigned char locw = 0x80 | MyLoc;
    unsigned char *p;
    int f = 0;
    p = FileImage + ExitBase;

    while(*p != locw)
        p++;
    p++;
    while(*p < 0x80) {
        if(f == 0) {
            if(CurrentGame == BLIZZARD_PASS && LastChar == ',')
                LastChar = 0;
            OutCaps();
            SysMessage(EXITS);
        }
        f = 1;
        if (caps)
            OutCaps();
        SysMessage(*p);
        p += 2;
    }
    if(f == 1) {
        OutReplace('.');
        OutChar('\n');
    }
}

static void RunStatusTable(void);
static void DrawExtraQP3Images(void);
extern uint8_t buffer[768][9];

void Look(void) {
    if (MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91) || NoGraphics)
        CloseGraphicsWindow();
    else
        OpenGraphicsWindow();
    int i;
    int f = 0;

    PendSpace = 0;
    if (!(CurrentWindow == Bottom && LineEvent))
        OutReset();
    TopWindow();

    Redraw = 0;
    if (Transcript)
        glk_put_char_stream(Transcript, '\n');

    OutCaps();

    if(Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        OutString("\n\n");
        DrawBlack();
        BottomWindow();
        return;
    }
    if (BaseGame == REBEL_PLANET && MyLoc > 0)
        OutString("You are ");
    PrintRoom(MyLoc);

    for(i = 0; i < NumLowObjects; i++) {
        if(ObjectLoc[i] == MyLoc) {
            if(f == 0) {
                if (Version == QUESTPROBE3_TYPE) {
                    OutReplace(0);
                    SysMessage(0);
                } else if (BaseGame == HEMAN || BaseGame == REBEL_PLANET) {
                    OutChar(' ');
                }
            }
            f = 1;
            PendSpace = 1;
            PrintObject(i);
        }
    }
    if(f == 1 && !isalpha(LastChar))
        OutReplace('.');

    if (Version == QUESTPROBE3_TYPE) {
        ListExits(1);
    } else {
        f = 0;
        for(; i < NumObjects(); i++) {
            if(ObjectLoc[i] == MyLoc) {
                if(f == 0) {
                    /* Only the text-only and hybrid games */
                    if (BaseGame == TEMPLE_OF_TERROR
                        && CurrentGame != TEMPLE_OF_TERROR
                        && CurrentGame != TEMPLE_OF_TERROR_64) {
                        OutChar(' ');
                    }
                    SysMessage(YOU_SEE);
                    if (CurrentGame == BLIZZARD_PASS) {
                        PendSpace = 0;
                        OutString(":- ");
                    }
                    if (Version == REBEL_PLANET_TYPE)
                        OutReplace(0);
                }
                f = 1;
                PrintObject(i);
            }
        }
        if(f == 1)
            OutReplace('.');
        else
            OutChar('.');
        ListExits(BaseGame != TEMPLE_OF_TERROR && BaseGame != HEMAN);
    }

    if (LastChar != '\n')
        OutChar('\n');

    if ((Options & FORCE_INVENTORY) && !(MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91))) {
        OutChar('\n');
        Inventory();
        OutChar('\n');
    }

    OutChar('\n');

    BottomWindow();

    if (MyLoc != 0 && !NoGraphics) {
        if (Resizing) {
            DrawSagaPictureFromBuffer();
            return;
        }
        if (Version == QUESTPROBE3_TYPE) {
            int tempstop = StopTime;
            StopTime = 0;
            DrawImages = 255;
            RunStatusTable();
            DrawExtraQP3Images();
            StopTime = tempstop;
        } else {
            glk_window_clear(Graphics);
            DrawRoomImage();
        }
    }
}


static void Goto(unsigned char loc) {
    if (BaseGame == QUESTPROBE3 && !PrintedOK)
        Okay();
    if (BaseGame == HEMAN && Location == 0 && loc == 1) {
        DeferredGoto = 1;
    } else {
        Location = loc;
        Redraw = 1;
    }
}

static void Delay(unsigned char seconds) {
    OutChar(' ');
    OutFlush();

    if (Options & NO_DELAYS)
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    glk_request_timer_events(1000 * seconds);

    event_t ev;

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(AnimationRunning);
}

static void Wear(unsigned char obj) {
    if(ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return;
    }
    if(ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return;
    }
    DropItem();
    Put(obj, Worn());
}

static void Remove(unsigned char obj) {
    if(ObjectLoc[obj] != Worn()) {
        SysMessage(YOU_ARE_NOT_WEARING_IT);
        return;
    }
    if(CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return;
    }
    Put(obj, Carried());
}

static void Means(unsigned char vb, unsigned char no) {
    Word[0] = vb;
    Word[1] = no;
}

static void Q3SwitchInvFlags(unsigned char a, unsigned char b) {
    if (Flag[2] == a) {
        Flag[2] = b;
        Flag[3] = a;
    }
}

static void Q3UpdateFlags(void) {
    if (ObjectLoc[7] == 253)
        ObjectLoc[7] = 254;
    if (IsThing) {
        if (ObjectLoc[2] == 0xfc) {
            /* If the "holding HUMAN TORCH by the hands" object is destroyed (i.e. not held) */
            /* the "location of the other guy" flag is set to the location of the Human Torch object */
            OtherGuyLoc = ObjectLoc[18];
        } else {
            OtherGuyLoc = MyLoc;
        }
        Q3SwitchInvFlags(253, 254);
    } else { /* I'm the HUMAN TORCH */
        if (ObjectLoc[1] == 0xfc) {
            /* If the "holding THING by the hands" object is destroyed (i.e. not held) */
            /* The "location of the other guy" flag is set to the location of the Thing object */
            OtherGuyLoc = ObjectLoc[17];
        } else {
            OtherGuyLoc = MyLoc;
        }
        Q3SwitchInvFlags(254, 253);
    }

    /* Reset flag 39 when Xandu is knocked out */
    if (ObjectLoc[33] != 22)
        Flag[39] = 0;
    /* And set it when he is present */
    else if (Present(33))
        Flag[39] = 1;

    /* Make sure that:
     - The watch isn't carried in the "intro"
     - That Thing has it when the game starts, no
     matter who we begin as
     */
    if (!Q3SwitchedWatch) {
        if (MyLoc == 6 || MyLoc == 0) {
            ObjectLoc[37] = 0xfc;
        } else {
            ObjectLoc[37] = 254;
            Q3SwitchedWatch = 1;
        }
    }

    if (DrawImages)
        return;

    TurnsLow++; /* Turns played % 100 */
    if (TurnsLow == 100) {
        TurnsHigh++; /* Turns divided by 100 */
        TurnsLow = 0;
    }

    ThingAsphyx++; // Turns since Thing started holding breath
    if (ThingAsphyx == 0)
        ThingAsphyx = 0xff;
    TorchAsphyx++; // Turns since Torch started holding breath
    if (TorchAsphyx == 0)
        TorchAsphyx = 0xff;
}

/* Questprobe 3 numbers the flags differently, so we have to offset them by 4 */
static void Q3AdjustConditions(unsigned char op, unsigned char *arg1)
{
    switch (op) {
        case ZERO:
        case NOTZERO:
        case LT:
        case GT:
        case EQ:
        case NE:
            *arg1 += 4;
            break;
        default:
            break;
    }
}

static void Q3AdjustActions(unsigned char op, unsigned char *arg1, unsigned char *arg2)
{
    switch (op) {
        case SET:
        case CLEAR:
        case LET:
        case ADD:
        case SUB:
            if (arg1 != NULL)
                *arg1 += 4;
            break;
        case SWAPF:
            if (arg1 != NULL)
                *arg1 += 4;
            if (arg2 != NULL)
                *arg2 += 4;
            break;
        default:
            break;
    }
}

static int TwoConditionParameters() {
    if (Version == QUESTPROBE3_TYPE)
        return 16;
    else
        return 21;
}

static int TwoActionParameters() {
    if (Version == QUESTPROBE3_TYPE)
        return 18;
    else
        return 22;
}

static void ExecuteLineCode(unsigned char *p, int *done)
{
    unsigned char arg1 = 0, arg2 = 0;
    int n;
    do {
        unsigned char op = *p;

        if(op & 0x80)
            break;
        p++;
        arg1 = *p++;

#ifdef DEBUG
        if (Version == QUESTPROBE3_TYPE) {
            unsigned char debugarg1 = arg1;
            Q3AdjustConditions(Q3Condition[op], &debugarg1);
            fprintf(stderr, "%s %d ", Condition[Q3Condition[op]], debugarg1);
        } else {
            fprintf(stderr, "%s %d ", Condition[op], arg1);
        }
#endif
        if (op >= TwoConditionParameters())
        {
            arg2 = *p++;
#ifdef DEBUG
            fprintf(stderr, "%d ", arg2);
#endif
        }

        if (Version == QUESTPROBE3_TYPE) {
            op = Q3Condition[op];
            Q3AdjustConditions(op, &arg1);
        }

        switch(op) {
            case AT:
                if(MyLoc == arg1)
                    continue;
                break;
            case NOTAT:
                if(MyLoc != arg1)
                    continue;
                break;
            case ATGT:
                if(MyLoc > arg1)
                    continue;
                break;
            case ATLT:
                if(MyLoc < arg1)
                    continue;
                break;
            case PRESENT:
                if(Present(arg1))
                    continue;
                break;
            case HERE:
                if(ObjectLoc[arg1] == MyLoc)
                    continue;
                break;
            case ABSENT:
                if(!Present(arg1))
                    continue;
                break;
            case NOTHERE:
                if(ObjectLoc[arg1] != MyLoc)
                    continue;
                break;
            case CARRIED:
                if(ObjectLoc[arg1] == Carried() || ObjectLoc[arg1] == Worn())
                    continue;
                break;
            case NOTCARRIED:
                if(ObjectLoc[arg1] != Carried() && ObjectLoc[arg1] != Worn())
                    continue;
                break;
            case WORN:
                if(ObjectLoc[arg1] == Worn())
                    continue;
                break;
            case NOTWORN:
                if(ObjectLoc[arg1] != Worn())
                    continue;
                break;
            case NODESTROYED:
                if(ObjectLoc[arg1] != Destroyed())
                    continue;
                break;
            case DESTROYED:
                if(ObjectLoc[arg1] == Destroyed())
                    continue;
                break;
            case ZERO:
                if (BaseGame == TEMPLE_OF_TERROR) {
                    /* Unless we have kicked sand in the eyes of the guard, tracked by flag 63, make sure he kills us if we try to pass, by setting flag 28 to zero */
                    if (arg1 == 28 && Flag[63] == 0 && Word[0] == 20 && Word[1] == 162)
                        Flag[28] = 0;
                }
                if(Flag[arg1] == 0)
                    continue;
                break;
            case NOTZERO:
                if(Flag[arg1] != 0)
                    continue;
                break;
            case WORD1:
                if(Word[2] == arg1)
                    continue;
                break;
            case WORD2:
                if(Word[3] == arg1)
                    continue;
                break;
            case WORD3:
                if(Word[4] == arg1)
                    continue;
                break;
            case CHANCE:
                if(Chance(arg1))
                    continue;
                break;
            case LT:
                if(Flag[arg1] < arg2)
                    continue;
                break;
            case GT:
                if(Flag[arg1] > arg2)
                    continue;
                break;
            case EQ:
                /* Fix final puzzle (Flag 12 conflict) */
                if (BaseGame == TEMPLE_OF_TERROR) {
                    if (arg1 == 12 && arg2 == 4)
                        arg1 = 60;
                }
                if(Flag[arg1] == arg2) {
                    continue;
                }
                break;
            case NE:
                if(Flag[arg1] != arg2)
                    continue;
                break;
            case OBJECTAT:
                if(ObjectLoc[arg1] == arg2)
                    continue;
                break;
            default:
                fprintf(stderr, "Unknown condition %d.\n",
                        op);
                break;
        }
#ifdef DEBUG
        fprintf(stderr, "\n");
#endif
        return;
    } while(1);

    ActionsExecuted = 1;

    do {
        unsigned char op = *p;
        if(!(op & 0x80))
            break;

#ifdef DEBUG
        if(op & 0x40)
            fprintf(stderr, "DONE:");
        if (Version == QUESTPROBE3_TYPE)
            fprintf(stderr,"%s(%d) ", Action[Q3Action[op & 0x3F]], op & 0x3F);
        else
            fprintf(stderr,"%s(%d) ", Action[op & 0x3F], op & 0x3F);
#endif

        p++;
        if(op & 0x40)
            *done = 1;
        op &= 0x3F;

        if(op > 8) {
            arg1 = *p++;
#ifdef DEBUG
            unsigned char debugarg1 = arg1;
            if (Version == QUESTPROBE3_TYPE)
                Q3AdjustActions(Q3Action[op], &debugarg1, NULL);
            fprintf(stderr, "%d ", debugarg1);
#endif
        }
        if(op >= TwoActionParameters()) {
            arg2 = *p++;
#ifdef DEBUG
            unsigned char debugarg2 = arg2;
            if (Version == QUESTPROBE3_TYPE)
                Q3AdjustActions(Q3Action[op], NULL, &debugarg2);
            fprintf(stderr, "%d ", debugarg2);
#endif
        }

        if (Version == QUESTPROBE3_TYPE) {
            op = Q3Action[op];
            Q3AdjustActions(op, &arg1, &arg2);

            if (!PrintedOK)
                Okay();
        }

        int WasDark = Flag[DarkFlag()];

        switch(op) {
            case LOADPROMPT:
                if (LoadPrompt()) {
                    *done = 1;
                    return;
                }
                break;
            case QUIT:
                if (!RecursionGuard) {
                    RecursionGuard = 1;
                    QuitGame();
                }
                *done = 1;
                return;
            case SHOWINVENTORY:
                Inventory();
                break;
            case ANYKEY:
                AnyKey();
                break;
            case SAVE:
                StopTime = 1;
                SaveGame();
                break;
            case DROPALL:
                if ((BaseGame == REBEL_PLANET && (Word[0] != 20 || Word[1] != 141)) ||
                    (BaseGame == KAYLETH && (Word[0] != 20 || Word[1] != 254)))
                    DropAll(0);
                else
                    DropAll(1);
                break;
            case LOOK:
                Look();
                break;
            case PRINTOK:
                /* Guess */
                Okay();
                break;
            case GET:
                if (GetObject(arg1) == 0 && Version == QUESTPROBE3_TYPE)
                    *done = 1;
                break;
            case DROP:
                if (DropObject(arg1) == 0 && BaseGame == REBEL_PLANET) {
                    *done = 1;
                    return;
                }
                break;
            case GOTO:
                /*
                 He-Man moves the the player to a special "By the power of Grayskull" room
                 and then issues an undo to return to the previous room
                 */
                if (BaseGame == HEMAN && arg1 == 83)
                    SaveUndo();
                Goto(arg1);
                break;
            case GOBY:
                /* Blizzard pass era */
                if(Version == BLIZZARD_PASS_TYPE)
                    Goto(ObjectLoc[arg1]);
                else
                    Message2(arg1);
                break;
            case SET:
                Flag[arg1] = 255;
                break;
            case CLEAR:
                Flag[arg1] = 0;
                break;
            case MESSAGE:
                /* Prevent repeated "Blob returns to his post" messages */
                if (CurrentGame == QUESTPROBE3_64 && arg1 == 44)
                    Flag[59] = 0;
                Message(arg1);
                if (CurrentGame == BLIZZARD_PASS && arg1 != 160)
                    OutChar('\n');
                break;
            case CREATE:
                Put(arg1, MyLoc);
                break;
            case DESTROY:
                Put(arg1, Destroyed());
                break;
            case PRINT:
                PrintNumber(Flag[arg1]);
                break;
            case DELAY:
                Delay(arg1);
                break;
            case WEAR:
                Wear(arg1);
                break;
            case REMOVE:
                Remove(arg1);
                break;
            case LET:
                if (BaseGame == TEMPLE_OF_TERROR) {
                    if (arg1 == 28 && arg2 == 2) {
                        /* If the serpent guard is present, we have just kicked sand in his eyes. Set flag 63 to track this */
                        Flag[63] = (ObjectLoc[48] == MyLoc);
                    }
                }
                Flag[arg1] = arg2;
                break;
            case ADD:
                /* Fix final puzzle (Flag 12 conflict) */
                if (BaseGame == TEMPLE_OF_TERROR) {
                    if (arg1 == 12 && arg2 == 1)
                        arg1 = 60;
                }
                n = Flag[arg1] + arg2;
                if(n > 255)
                    n = 255;
                Flag[arg1] = n;
                break;
            case SUB:
                n = Flag[arg1] - arg2;
                if(n < 0)
                    n = 0;
                Flag[arg1] = n;
                break;
            case PUT:
                Put(arg1, arg2);
                break;
            case SWAP:
                n = ObjectLoc[arg1];
                Put(arg1, ObjectLoc[arg2]);
                Put(arg2, n);
                break;
            case SWAPF:
                n = Flag[arg1];
                Flag[arg1] = Flag[arg2];
                Flag[arg2] = n;
                break;
            case MEANS:
                Means(arg1, arg2);
                break;
            case PUTWITH:
                Put(arg1, ObjectLoc[arg2]);
                break;
            case BEEP:
#if defined(GLK_MODULE_GARGLKBLEEP)
                garglk_zbleep(1 + (arg1 == 250));
#elif defined(SPATTERLIGHT)
                fprintf(stderr, "BEEP: arg1: %d arg2: %d\n", arg1, arg2);
                win_beep(1 + (arg1 == 250));
#else
                putchar('\007');
                fflush(stdout);
#endif
                break;
            case REFRESH:
                if (BaseGame == KAYLETH)
                    TakeAll(78);
                if (BaseGame == HEMAN)
                    TakeAll(45);
                Redraw = 1;
                break;
            case RAMSAVE:
                RamSave(1);
                break;
            case RAMLOAD:
                RamLoad();
                break;
            case CLSLOW:
                OutFlush();
                glk_window_clear(Bottom);
                break;
            case OOPS:
                RestoreUndo(0);
                Redraw = 1;
                break;
            case DIAGNOSE:
                Message(223);
                char buf[5];
                char *q = buf;
                /* TurnsLow = turns % 100, TurnsHigh == turns / 100 */
                snprintf(buf, sizeof buf, "%04d", TurnsLow + TurnsHigh * 100);
                while(*q)
                    OutChar(*q++);
                SysMessage(14);
                if (IsThing)
                /* THING is always 100 percent rested */
                    OutString("100");
                else {
                    /* Calculate "restedness" percentage */
                    /* Flag[7] == 80 means 100 percent rested */
                    q = buf;
                    snprintf(buf, sizeof buf, "%d", (Flag[7] >> 2) + Flag[7]);
                    while(*q)
                        OutChar(*q++);
                }
                SysMessage(15);
                break;
            case SWITCHINVENTORY:
            {
                uint8_t temp = Flag[2]; /* Switch inventory */
                Flag[2] = OtherGuyInv;
                OtherGuyInv = temp;
                temp = Flag[42]; /* Switch dark flag */
                Flag[42] = Flag[43];
                Flag[43] = temp;
                Redraw = 1;
                break;
            }
            case SWITCHCHARACTER:
                /* Go to the location of the other guy */
                Location = ObjectLoc[arg1];
                /* Pick him up, so that you don't see yourself */
                GetObject(arg1);
                Redraw = 1;
                break;
            case DONE:
                *done = 1;
                break;
            case IMAGE:
                if (MyLoc == 3 || Flag[DarkFlag()]) {
                    DrawBlack();
                    break;
                }
                if (arg1 == 0) {
                    ClearGraphMem();
                    DrawSagaPictureNumber(MyLoc - 1);
                } else if (arg1 == 45 && ObjectLoc[48] != MyLoc) {
                    break;
                } else {
                    DrawSagaPictureNumber(arg1 - 1);
                }
                DrawSagaPictureFromBuffer();
                break;
            default:
                fprintf(stderr, "Unknown command %d.\n", op);
                break;
        }
        if (WasDark != Flag[DarkFlag()])
            Redraw = 1;
    }
    while(1);
#ifdef DEBUG
    fprintf(stderr, "\n");
#endif
}

static unsigned char *NextLine(unsigned char *p)
{
    unsigned char op;
    while(!((op = *p) & 0x80)) {
        p+=2;
        if(op >= TwoConditionParameters())
            p++;
    }
    while(((op = *p) & 0x80)) {
        op &= 0x3F;
        p++;
        if (op > 8)
            p++;
        if(op >= TwoActionParameters())
            p++;
    }
    return p;
}

static void DrawExtraQP3Images(void) {
    if (MyLoc == 34 && ObjectLoc[29] == 34) {
        DrawSagaPictureNumber(46);
        buffer[32 * 8 + 25][8] &= 191;
        buffer[32 * 9 + 25][8] &= 191;
        buffer[32 * 9 + 26][8] &= 191;
        buffer[32 * 9 + 27][8] &= 191;
        DrawSagaPictureFromBuffer();
    } else if (MyLoc == 2 && ObjectLoc[17] == 2 && Flag[26] > 16 && Flag[26] < 20) {
        DrawSagaPictureNumber(53);
        DrawSagaPictureFromBuffer();
    }
}

static void RunStatusTable(void)
{
    if (StopTime) {
        StopTime--;
        return;
    }
    unsigned char *p = FileImage + StatusBase;

    int done = 0;
    ActionsExecuted = 0;

    if (Version == QUESTPROBE3_TYPE) {
        Q3UpdateFlags();
    }

    while(*p != 0x7F) {
        while (Version == QUESTPROBE3_TYPE && *p == 0x7e) {
            p++;
        }
        ExecuteLineCode(p, &done);
        if(done) {
            return;
        }
        p = NextLine(p);
    }
    if (Version == QUESTPROBE3_TYPE)
        DrawImages = 0;
}

static void RunCommandTable(void)
{
    unsigned char *p = FileImage + ActionBase;

    int done = 0;
    ActionsExecuted = 0;
    FoundMatch = 0;

    while(*p != 0x7F) {
        /* Match input to table entry as VERB NOUN or NOUN VERB */
        /* 126 is wildcard that matches any word */
        if(((*p == 126 || *p == Word[0]) &&
           (p[1] == 126 || p[1] == Word[1])) ||
           (*p == Word[1] && p[1] == Word[0]) ||
           (BaseGame != QUESTPROBE3 && (*p == 126 || *p == Word[1]) &&
           (p[1] == 126 || p[1] == Word[0]))
           ) {
#ifdef DEBUG
            PrintWord(p[0]);
            PrintWord(p[1]);
#endif
            if (p[0] != 126) {
                FoundMatch = 1;
            }
            /* Work around a Questprobe 3 bug */
            if (Version == QUESTPROBE3_TYPE) {
                /* In great room, Xandu present */
                if (Present(33)) {
                    Flag[39] = 1;
                    Message(24);
                    Goto(26);
                    ActionsExecuted = 1;
                    return;
                }
            }
            ExecuteLineCode(p + 2, &done);
            if(done)
                return;
        }
        p = NextLine(p + 2);
    }
}

static int AutoExit(unsigned char v)
{
    unsigned char *p = FileImage + ExitBase;
    unsigned char want = MyLoc | 0x80;
    while(*p != want) {
        if(*p == 0xFE)
            return 0;
        p++;
    }
    p++;
    while(*p < 0x80) {
        if(*p == v) {
            Goto(p[1]);
            return 1;
        }
        p+=2;
    }
    return 0;
}

static int IsDir(unsigned char word)
{
    if (word == 0)
        return 0;
    if (Version == QUESTPROBE3_TYPE) {
        return (word <= 4 || word == 57 || word == 60);
    }
    else return (word <= 10);
}

static void RunOneInput(void)
{
    PrintedOK = 0;
    if(Word[0] == 0 && Word[1] == 0) {
        if (TryExtraCommand() == 0) {
            OutCaps();
            SysMessage(I_DONT_UNDERSTAND);
            StopTime = 2;
        } else {
            if (Redraw)
                Look();
        }
        return;
    }
    if (IsDir(Word[0]) || (Word[0] == GoVerb && IsDir(Word[1]))) {
        if(AutoExit(Word[0]) || AutoExit(Word[1])) {
            StopTime = 0;
            RunStatusTable();
            if(Redraw)
                Look();
            return;
        }
    }

    /* Handle IT */
    if (Word[1] == 128)
        Word[1] = LastNoun;
    if (Word[1] != 0)
        LastNoun = Word[1];

    OutCaps();
    RunCommandTable();

    if(ActionsExecuted == 0) {
        int OriginalVerb = Word[0];
        if (TryExtraCommand() == 0) {
            if (LastVerb) {
                Word[4] = Word[3];
                Word[3] = Word[2];
                Word[2] = Word[1];
                Word[1] = Word[0];
                Word[0] = LastVerb;
                RunCommandTable();
            }
            if (ActionsExecuted == 0) {
                if(IsDir(OriginalVerb) || (Word[0] == GoVerb && IsDir(Word[1])))
                    SysMessage(YOU_CANT_GO_THAT_WAY);
                else if (FoundMatch)
                    SysMessage(THATS_BEYOND_MY_POWER);
                else
                    SysMessage(I_DONT_UNDERSTAND);
                OutFlush();
                StopTime = 1;
                return;
            }
        } else return;
    }

    if (Word[0] != 0)
        LastVerb = Word[0];

    if(Redraw && !(BaseGame == REBEL_PLANET && MyLoc == 250)) {
        Look();
    }

    Redraw = 0;

    if (Version == QUESTPROBE3_TYPE && Flag[WaitFlag()] > 1)
        Flag[WaitFlag()]++;

    do {
        if (Version == QUESTPROBE3_TYPE) {
            DrawImages = 0;
            RunStatusTable();
            DrawImages = 255;
            int tempstop = StopTime;
            StopTime = 0;
            RunStatusTable();
            DrawExtraQP3Images();
            StopTime = tempstop;
        } else {
            RunStatusTable();
        }

        if(Redraw) {
            Look();
        }
        Redraw = 0;
        if (WaitFlag() != -1 && Flag[WaitFlag()]) {
            Flag[WaitFlag()]--;
            if (LastChar != '\n')
                OutChar('\n');
        }
    } while (WaitFlag() != -1 && Flag[WaitFlag()] > 0);
    if (AnimationRunning)
        glk_request_timer_events(AnimationRunning);
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
}

void PrintFirstTenBytes(size_t offset) {
    fprintf(stderr, "\nFirst 10 bytes at 0x%04zx: ", offset);
    for (int i = 0; i < 10; i++)
        fprintf(stderr, "0x%02x ", FileImage[offset + i]);
    fprintf(stderr, "\n");
}


static void FindTables(void)
{
    TokenBase = FindTokens();
    RoomBase = Game->start_of_room_descriptions + FileBaselineOffset;
    ObjectBase = Game->start_of_item_descriptions + FileBaselineOffset;
    StatusBase = Game->start_of_automatics + FileBaselineOffset;
    ActionBase = Game->start_of_actions + FileBaselineOffset;
    ExitBase = Game->start_of_room_connections + FileBaselineOffset;
    FlagBase = FindFlags();
    ObjLocBase = Game->start_of_item_locations  + FileBaselineOffset;
    MessageBase = Game->start_of_messages + FileBaselineOffset;
    Message2Base = Game->start_of_messages_2 + FileBaselineOffset;

    if (BaseGame == KAYLETH) {
        AnimationData = FindCode("\xff\x00\x00\x00\x0f\x00\x5d\x0f\x00\x61", 0, 10);
        if (AnimationData == -1)
            AnimationData = FindCode("\xff\x00\x00\x15\x0f\x00\x5d\x0f\x00\x61", 0, 10);
    }
}

/*
 *	Version 0 is different
 */

static int GuessLowObjectEnd0(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *t = NULL;
    unsigned char c = 0, lc;
    int n = 0;

    while(p < EndOfData) {
        if(t == NULL)
            t = TokenText(*p++);
        lc = c;
        c = *t & 0x7F;
        if(c == 0x5E || c == 0x7E) {
            if(lc == ',' && n > 20)
                return n;
            n++;
        }
        if(*t++ & 0x80)
            t = NULL;
    }
    return -1;
}


static int GuessLowObjectEnd(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *x;
    int n = 0;

    /* Can't automatically guess in this case */
    if (CurrentGame == BLIZZARD_PASS)
        return 70;
    else if (Version == QUESTPROBE3_TYPE)
        return 49;

    if (Version == REBEL_PLANET_TYPE)
        return GuessLowObjectEnd0();

    while(n < NumObjects()) {
        while(*p != 0x7E && *p != 0x5E) {
            p++;
        }
        x = TokenText(p[-1]);
        while(!(*x & 0x80)) {
            x++;
        }
        if((*x & 0x7F) == ',')
            return n;
        n++;
        p++;
    }
    fprintf(stderr, "Unable to guess the last description object.\n");
    return 0;
}

static void RestartGame(void)
{
    RecursionGuard = 0;
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    OutFlush();
    glk_window_clear(Bottom);
    Look();
    RunStatusTable();
    ShouldRestart = 0;
    Look();
}

int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    if (argc > 1)
        while (argv[1]) {
            if (*argv[1] != '-')
                break;
            switch (argv[1][1]) {
                case 'n':
                    Options |= NO_DELAYS;
                    break;
            }
            argv++;
            argc--;
        }

    if(argv[1] == NULL)
    {
        fprintf(stderr, "%s: <file>.\n", argv[0]);
        glk_exit();
    }

    size_t namelen = strlen(argv[1]);
    Filename = MemAlloc(namelen + 1);
    strncpy(Filename, argv[1], namelen);
    Filename[namelen] = '\0';

    FILE *f = fopen(Filename, "r");
    if(f == NULL)
    {
        perror(Filename);
        glk_exit();
    }

    fseek(f, 0, SEEK_END);
    FileImageLen = ftell(f);
    if (FileImageLen == -1) {
        fclose(f);
        glk_exit();
    }

    FileImage = MemAlloc(FileImageLen);

    fseek(f, 0, SEEK_SET);
    if (fread(FileImage, 1, FileImageLen, f) != FileImageLen) {
        fprintf(stderr, "File read error!\n");
    }

    FileImage = ProcessFile(FileImage, &FileImageLen);

    EndOfData = FileImage + FileImageLen;

    fclose(f);

#ifdef GARGLK
    garglk_set_program_name("TaylorMade 0.4");
    garglk_set_program_info("TaylorMade 0.4 by Alan Cox\n"
                            "Glk port and graphics support by Petter Sjölund\n");
    const char *s;
    if ((s = strrchr(Filename, '/')) != NULL || (s = strrchr(Filename, '\\')) != NULL) {
        garglk_set_story_name(s + 1);
    } else {
        garglk_set_story_name(Filename);
    }
#endif

    return 1;
}

#ifdef DEBUG

void PrintConditionAddresses(void) {
    fprintf(stderr, "Memory adresses of conditions\n\n");
    uint16_t conditionsOffsets = 0x56A8 + FileBaselineOffset;
    uint8_t *conditions;
    conditions = &FileImage[conditionsOffsets];
    for (int i = 1; i < 20; i++) {
        uint16_t address = *conditions++;
        address += *conditions * 256;
        conditions++;
        fprintf(stderr, "Condition %02d: 0x%04x (%s)\n", i, address, Condition[Q3Condition[i]]);
    }
    fprintf(stderr, "\n");
}

void PrintActionAddresses(void) {
    fprintf(stderr, "Memory adresses of actions\n\n");
    uint16_t actionOffsets = 0x591C + FileBaselineOffset;
    uint8_t *actions;
    actions = &FileImage[actionOffsets];
    for (int i = 1; i < 24; i++) {
        uint16_t address = *actions++;
        address += *actions * 256;
        actions++;
        fprintf(stderr, "   Action %02d: 0x%04x (%s)\n", i, address, Action[Q3Action[i]]);
    }
    fprintf(stderr, "\n");
}

#endif

struct GameInfo *DetectGame(size_t LocalVerbBase)
{
    struct GameInfo *LocalGame;

    for (int i = 0; i < NUMGAMES; i++) {
        LocalGame = &games[i];
        FileBaselineOffset = (long)LocalVerbBase - (long)LocalGame->start_of_dictionary;
        long diff = FindTokens() - LocalVerbBase;
        if ((LocalGame->start_of_tokens - LocalGame->start_of_dictionary) == diff) {
#ifdef DEBUG
            fprintf(stderr, "This is %s\n", LocalGame->Title);
#endif
            return LocalGame;
        } else {
#ifdef DEBUG
            fprintf(stderr, "Diff for game %s: %d. Looking for %ld\n", LocalGame->Title, LocalGame->start_of_tokens - LocalGame->start_of_dictionary, diff);
#endif
        }
    }
    return NULL;
}

static void UnparkFileImage(uint8_t *ParkedFile, size_t ParkedLength, long ParkedOffset, int FreeCompanion)
{
    FileImage = ParkedFile;
    FileImageLen = ParkedLength;
    FileBaselineOffset = ParkedOffset;
    if (FreeCompanion)
        free(CompanionFile);
}

static void LookForSecondTOTGame(void)
{
    size_t namelen = strlen(Filename);
    char *secondfile = MemAlloc(namelen + 1);
    strncpy(secondfile, Filename, namelen);
    secondfile[namelen] = '\0';

    char *period = strrchr(secondfile, '.');
    if (period == NULL)
        period = &secondfile[namelen - 1];
    else
        period--;

    if (CurrentGame == TEMPLE_OF_TERROR)
        *period = 'b';
    else
        *period = 'a';

    FILE *f = fopen(secondfile, "r");
    if (f == NULL)
        return;

    fseek(f, 0, SEEK_END);
    size_t filelength = ftell(f);
    if (filelength == -1) {
        return;
    }

    CompanionFile = MemAlloc(filelength);

    fseek(f, 0, SEEK_SET);
    if (fread(CompanionFile, 1, filelength, f) != filelength) {
        fprintf(stderr, "File read error!\n");
    }

    fclose(f);

    uint8_t *ParkedFile = FileImage;
    size_t ParkedLength = FileImageLen;
    size_t ParkedOffset = FileBaselineOffset;

    CompanionFile = ProcessFile(CompanionFile, &filelength);

    FileImage = CompanionFile;
    FileImageLen = filelength;

    size_t AltVerbBase = FindCode("NORT\001N", 0, 6);

    if (AltVerbBase == -1) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    struct GameInfo *AltGame = DetectGame(AltVerbBase);

    if ((CurrentGame == TOT_TEXT_ONLY && AltGame->gameID != TEMPLE_OF_TERROR) ||
        (CurrentGame == TEMPLE_OF_TERROR && AltGame->gameID != TOT_TEXT_ONLY)) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    Display(Bottom, "Found files for both the text-only version and the graphics version of Temple of Terror.\n"
            "Would you like to use the longer texts from the text-only version along with the graphics from the other file? (Y/N) ");
    if (!YesOrNo()) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    int index = 0;

    if (CurrentGame == TOT_TEXT_ONLY) {
        while (Game->gameID != TOT_HYBRID) {
            Game = &games[index++];
        }
        SagaSetup();
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
    } else {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
        SagaSetup();
        while (Game->gameID != TOT_HYBRID) {
            Game = &games[index++];
        }
        FileImage = CompanionFile;
        FileImageLen = filelength;
        VerbBase = AltVerbBase;
    }

    EndOfData = FileImage + FileImageLen;
}

void glk_main(void)
{
    if (DetectC64(&FileImage, &FileImageLen) != UNKNOWN_GAME) {
        EndOfData = FileImage + FileImageLen;
    } else {
        fprintf(stderr, "DetectC64 did not recognize the game\n");
	}

#ifdef DEBUG
    fprintf(stderr, "Loaded %zu bytes.\n", FileImageLen);
#endif

    VerbBase = FindCode("NORT\001N", 0, 6);
    if(VerbBase == -1) {
        fprintf(stderr, "No verb table!\n");
        glk_exit();
    }

    if (!Game)
        Game = DetectGame(VerbBase);
    if (Game == NULL) {
        fprintf(stderr, "Did not recognize game!\n");
        glk_exit();
    } else {
        FileBaselineOffset = VerbBase - Game->start_of_dictionary;
    }
#ifdef DEBUG
    fprintf(stderr, "FileBaselineOffset: %ld\n", FileBaselineOffset);
#endif

#ifdef SPATTERLIGHT
    if (gli_determinism) {
        srand(1234);
    } else
#endif
        srand((unsigned int)time(NULL));

    DisplayInit();

    if (CurrentGame == TEMPLE_OF_TERROR || CurrentGame == TOT_TEXT_ONLY) {
        LookForSecondTOTGame();
    }

    GoVerb = ParseWord("GO");

    SagaSetup();

    if (CurrentGame == QUESTPROBE3 || CurrentGame == REBEL_PLANET)
        DelimiterChar = '=';

    FindTables();
#ifdef DEBUG
    if (Version != BLIZZARD_PASS_TYPE && Version != QUESTPROBE3_TYPE)
        Action[12] = "MESSAGE2";
    LoadWordTable();
#endif
    NewGame();
    NumLowObjects = GuessLowObjectEnd();
    InitialState = SaveCurrentState();

    RunStatusTable();
    if(Redraw) {
        OutFlush();
        Look();
    }
    while(1) {
        if (ShouldRestart) {
            RestartGame();
        } else if (!StopTime)
            SaveUndo();
        Parser();
        FirstAfterInput = 1;
        RunOneInput();
        if (StopTime)
            StopTime--;
        else
            JustStarted = 0;
    }
}
