#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include "taylormade.h"

static unsigned char Flag[128];
static unsigned char Object[256];
static unsigned char Word[5];

static unsigned char Image[131072];
static int ImageLen;
static int VerbBase;
static unsigned int TokenBase;
static unsigned int MessageBase;
static unsigned int Message2Base;
static unsigned int RoomBase;
static unsigned int ObjectBase;
static unsigned int ExitBase;
static unsigned int ObjLocBase;
static unsigned int StatusBase;
static unsigned int ActionBase;
static unsigned int FlagBase;
static unsigned char *TopText;

static int NumLowObjects;
static int MaxRoom;
static int MaxMessage;
static int MaxMessage2;

static int GameVersion;
static int Blizzard;

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
	"NOTDESTROYED",
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
	"ACT36",
	"ACT37",
	"ACT38",
	"ACT39",
	"ACT40",
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
	unsigned char *p = Image + VerbBase;

	printf("Words\n\n");
	while(1) {
		if(p[4] == 255)
			break;
		if(WordMap[p[4]][0] == 0)
			memcpy(WordMap[p[4]], p, 4);
		printf("%-4.4s %d\n", p, p[4]);
		p+=5;
	}
	printf("\n\n");
}

static void PrintWord(unsigned char word)
{
	if(word == 126)
		printf("*    ");
	else if(word == 0 || WordMap[word][0] == 0)
		printf("%-4d ", word);
	else {
		printf("%c%c%c%c ", 
			WordMap[word][0],
			WordMap[word][1],
			WordMap[word][2],
			WordMap[word][3]);
	}
}


static int FindCode(char *x, int base)
{
	unsigned char *p = Image + base;
	int len = strlen(x);
	while(p < Image + ImageLen - len) {
		if(memcmp(p, x, len) == 0)
			return p - Image;
		p++;
	}
	return -1;
}

static int FindFlags(void) 
{
	/* Look for the flag initial block copy */
	int pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0);
	if(pos == -1) {
		fprintf(stderr, "Cannot find initial flag data.\n");
		exit(1);
	}
	return pos + 6;
}

static int FindObjectLocations(void)
{
	int pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0);
	if(pos == -1) {
		fprintf(stderr, "Cannot find initial object data.\n");
		exit(1);
	}
	pos = Image[pos - 16] + (Image[pos - 15] << 8);
	return pos - 0x4000;
}

static int FindExits(void) 
{
	int pos = 0;
	
	while((pos = FindCode("\x1A\xBE\x28\x0B\x13", pos+1)) != -1)
	{
		pos = Image[pos - 5] + (Image[pos - 4] << 8);
		pos -= 0x4000;
		return pos;
	}
	fprintf(stderr, "Cannot find initial flag data.\n");
	exit(1);
}

static int LooksLikeTokens(int pos)
{
	unsigned char *p = Image + pos;
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

static void TokenClassify(int pos)
{
	unsigned char *p = Image + pos;
	int n = 0;
	while(n++ < 256) {
		do {
			if(*p == 0x5E || *p == 0x7E)
				GameVersion = 0;
		} while(!(*p++ & 0x80));
	}
}

static int FindTokens(void)
{
	int addr;
	int pos = 0;
	do {
		pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1);
		if(pos == -1) {
			/* Last resort */
			addr = FindCode("You are in ", 0) - 1;
			if(addr == -1) {
				fprintf(stderr, "Unable to find token table.\n");
				exit(1);
			}
			break;
		}
		addr = (Image[pos-1] <<8 | Image[pos-2]) - 0x4000;
	}
	while(LooksLikeTokens(addr) == 0);
	TokenClassify(addr);
	return addr;
}

static void OutChar(char c)
{
	putchar(c);
}

static unsigned char *TokenText(unsigned char n)
{
	unsigned char *p = Image + TokenBase;
			
	p = Image + TokenBase;
	
	while(n > 0) {
		while((*p & 0x80) == 0)
			p++;
		n--;
		p++;
	}
	return p;
}

static void PrintToken(unsigned char n)
{
	unsigned char *p = TokenText(n);
	unsigned char c;
	do {
		c = *p++;
		OutChar(c & 0x7F);
	} while(!(c & 0x80));
}

static void PrintText1(unsigned char *p, int n)
{
	while(n > 0) {
		while(*p != 0x7E && *p != 0x5E)
			p++;
		n--;
		p++;
	}
	while(*p != 0x7E && *p != 0x5E)
		PrintToken(*p++);
	if(*p == 0x5E)
		OutChar(' ');
	if (p > TopText)
		TopText = p;
}

/*
 *	Version 0 is different
 */
 
static void PrintText0(unsigned char *p, int n)
{
	unsigned char *t = NULL;
	unsigned char c;
	while(1) {
		if(t == NULL)
			t = TokenText(*p++);
		c = *t & 0x7F;
		if(c == 0x5E || c == 0x7E) {
			if(n == 0) {
				if(c == 0x5E)
					OutChar(' ');
				if (p > TopText)
					TopText = p;
				return;
			}
			n--;
		}
		else if(n == 0)
			OutChar(c);
		if(*t++ & 0x80)
			t = NULL;
	}
}

static void PrintText(unsigned char *p, int n)
{
	if(GameVersion == 0) 	/* In stream end markers */
		PrintText0(p, n);
	else			/* Out of stream end markers (faster) */
		PrintText1(p, n);
}

static int FindMessages(void)
{
	int pos = 0;
	/* Newer game format */
	while((pos = FindCode("\xF5\xE5\xC5\xD5\x3E\x2E", pos+1)) != -1) {
		if(Image[pos + 6] != 0x32)
			continue;
		if(Image[pos + 9] != 0x78)
			continue;
		if(Image[pos + 10] != 0x32)
			continue;
		if(Image[pos + 13] != 0x21)
			continue;
		return (Image[pos+14] + (Image[pos+15] << 8)) - 0x4000;
	}
	/* Try now for older game format */
	while((pos = FindCode("\xF5\xE5\xC5\xD5\x78\x32", pos+1)) != -1) {
		if(Image[pos + 8] != 0x21)
			continue;
		if(Image[pos + 11] != 0xCD)
			continue;
		/* End markers in compressed blocks */
		GameVersion = 0;
		return (Image[pos+9] + (Image[pos+10] << 8)) - 0x4000;
	}
	fprintf(stderr, "Unable to locate messages.\n");
	exit(1);
}

static int FindMessages2(void)
{
	int pos = 0;
	while((pos = FindCode("\xF5\xE5\xC5\xD5\x78\x32", pos+1)) != -1) {
		if(Image[pos + 8] != 0x21)
			continue;
		if(Image[pos + 11] != 0xC3)
			continue;
		return (Image[pos+9] + (Image[pos+10] << 8)) - 0x4000;
	}
	fprintf(stderr, "No second message block ?\n");
	return 0;
}

static void Message(unsigned int m)
{
	unsigned char *p = Image + MessageBase;
	PrintText(p, m);
}

static void Message2(unsigned int m)
{
	unsigned char *p = Image + Message2Base;
	PrintText(p, m);
}

static int FindObjects(void)
{
	int pos = 0;
	while((pos = FindCode("\xF5\xE5\xC5\xD5\x32", pos+1)) != -1) {
		if(Image[pos + 10] != 0xCD)
			continue;
		if(Image[pos +7] != 0x21)
			continue;
		return (Image[pos+8] + (Image[pos+9] << 8)) - 0x4000;
	}
	fprintf(stderr, "Unable to locate objects.\n");
	exit(1);
}

static void PrintObject(unsigned char obj)
{
	unsigned char *p = Image + ObjectBase;
	PrintText(p, obj);
}

/* Standard format */
static int FindRooms(void)
{
	int pos = 0;
	while((pos = FindCode("\x3E\x19\xCD", pos+1)) != -1) {
		if(Image[pos + 5] != 0xC3)
			continue;
		if(Image[pos + 8] != 0x21)
			continue;
		return (Image[pos+9] + (Image[pos+10] << 8)) - 0x4000;
	}
	fprintf(stderr, "Unable to locate rooms.\n");
	exit(1);
}


static void PrintRoom(unsigned char room)
{
	unsigned char *p = Image + RoomBase;
	PrintText(p, room);
}


static void NewGame(void)
{
	memset(Flag, 0, 128);
	memcpy(Flag + 1, Image + FlagBase, 6);
	memcpy(Object, Image + ObjLocBase, Flag[6]);
}

static void ExecuteLineCode(unsigned char *p)
{
	unsigned char arg1, arg2;
	int n;
	do {
		unsigned char op = *p;
		
		if(op & 0x80)
			break;
		p++;
		arg1 = *p++;
		printf("%s %d ", Condition[op], arg1); 
		if(op > 20)
		{
			arg2 = *p++;
			printf("%d ", arg2); 
		}
		if (op < 5 && arg1 > MaxRoom)
			MaxRoom = arg1;
		if (op == 25 && arg2 > MaxRoom)
			MaxRoom = arg2;
	} while(1);
	
	do {
		unsigned char op = *p;
		if(!(op & 0x80))
			break;
		
		printf("%s ", Action[op & 0x3F], op & 0x3F);
		p++;

		
		if((op & 0x3F) > 8) {
			arg1 = *p++;
			printf("%d ", arg1); 
		}
		if((op & 0x3F) > 21) {
			arg2 = *p++;
			printf("%d ", arg2);
		}
		if(op & 0x40)
			printf("DONE"); 
		op &= 0x3F;
		if (op == 11 && MaxRoom < arg1)
			MaxRoom = arg1;
		if (op == 12 && MaxMessage2 < arg1)
			MaxMessage2 = arg1;
		if (op == 15 && MaxMessage < arg1)
			MaxMessage = arg1;
		if (op == 25 && arg2 < 252 && arg2 > MaxRoom)
			MaxRoom = arg2;

	}
	while(1);
	printf("\n");
}

static unsigned char *NextLine(unsigned char *p)
{
	unsigned char op;
	while(!((op = *p) & 0x80)) {
		p+=2;
		if(op > 20)
			p++;
	}
	while(((op = *p) & 0x80)) {
		op &= 0x3F;
		p++;
		if(op > 8)
			p++;
		if(op > 21)
			p++;
	}
	return p;
}

static int FindStatusTable(void)
{
	int pos = 0;
	while((pos = FindCode("\x3E\xFF\x32", pos+1)) != -1) {
		if(Image[pos + 5] != 0x18)
			continue;
		if(Image[pos + 6] != 0x07)
			continue;
		if(Image[pos + 7] != 0x21)
			continue;
		return (Image[pos-2] + (Image[pos-1] << 8)) - 0x4000;
	}
	fprintf(stderr, "Unable to find automatics.\n");
	exit(1);
}

static void DumpStatusTable(void) 
{
	unsigned char *p = Image + StatusBase;
	
	while(*p != 0x7F) {
		ExecuteLineCode(p);
		p = NextLine(p);
	}
	printf("\n\n");
}

int FindCommandTable(void)
{
	int pos = 0;
	while((pos = FindCode("\x3E\xFF\x32", pos+1)) != -1) {
		if(Image[pos + 5] != 0x18)
			continue;
		if(Image[pos + 6] != 0x07)
			continue;
		if(Image[pos + 7] != 0x21)
			continue;
		return (Image[pos+8] + (Image[pos+9] << 8)) - 0x4000;
	}
	fprintf(stderr, "Unable to find commands.\n");
	exit(1);
}

static void DumpCommandTable(void)
{
	unsigned char *p = Image + ActionBase;

	while(*p != 0x7F) {
	   	PrintWord(p[0]);
	   	PrintWord(p[1]);
		ExecuteLineCode(p + 2);
		p = NextLine(p + 2);
	}
	printf("\n\n");
}

static int DumpExits(unsigned char v)
{
	unsigned char *p = Image + ExitBase;
	unsigned char want = v | 0x80;
	while(*p != want) {
		if(*p == 0xFE)
			return 0;
		p++;
	}
	p++;
	while(*p < 0x80) {
		printf("  ");
		PrintWord(*p);
		printf(" %d\n", p[1]);
		p+=2;
	}
	return 0;
}
		
static void FindTables(void) 
{
	printf("Verbs at %04X\n", VerbBase + 0x4000);
	TokenBase = FindTokens();
	printf("Tokens at %04X\n", TokenBase + 0x4000);
	RoomBase = FindRooms();
	printf("Rooms at %04X\n", RoomBase + 0x4000);
	ObjectBase = FindObjects();
	printf("Objects at %04X\n", ObjectBase + 0x4000);
	StatusBase = FindStatusTable();
	printf("Status at %04X\n", StatusBase + 0x4000);
	ActionBase = FindCommandTable();
	printf("Actions at %04X\n", ActionBase + 0x4000);
	ExitBase = FindExits();
	printf("Exits at %04X\n", ExitBase + 0x4000);
	FlagBase = FindFlags();
	printf("Flags at %04X\n", FlagBase + 0x4000);
	ObjLocBase = FindObjectLocations();
	printf("Object Locations at %04X\n", ObjLocBase + 0x4000);
	MessageBase = FindMessages();
	printf("Messages at %04X\n", MessageBase + 0x4000);
	Message2Base = FindMessages2();
	printf("Messages Block 2 at %04X\n", Message2Base + 0x4000);
	printf("Game version %d\n", GameVersion);
	printf("\n\n\n");
}

/*
 *	Version 0 is different
 */
 
static int GuessLowObjectEnd0(void)
{
	unsigned char *p = Image + ObjectBase;
	unsigned char *t = NULL;
	unsigned char c = 0, lc;
	int n = 0;
	
	while(1) {
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
}


static int GuessLowObjectEnd(void)
{
	unsigned char *p = Image + ObjectBase;
	unsigned char *x;
	int n = 0;
	
	if (Blizzard)
		return 70;

	if(GameVersion == 0)
		return GuessLowObjectEnd0();
		
	while(n < Flag[6]) {
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

void DumpMessages(void)
{
	int i;
	printf("%d Messages\n\n", MaxMessage);
	for (i = 0; i < MaxMessage; i++) {
		printf("%d: ", i);
		Message(i);
		printf("\n");
	}
	printf("\n\n");
}

void DumpMessages2(void)
{
	int i;
	if (Message2Base == 0 || MaxMessage2 == 0)
		return;
	printf("%d Messages2\n\n", MaxMessage2);
	for (i = 0; i < MaxMessage2; i++) {
		printf("%d: ", i);
		Message2(i);
		printf("\n");
	}
	printf("\n\n");
}

void DumpObjects(void)
{
	int n = Flag[6];
	int i;
	
	printf("%d Objects\n\n", n);
	for (i = 0; i < n; i++) {
		printf("%d: ", i);
		PrintObject(i);
		printf(" %d\n", (int)Object[i]);
	}
	printf("\n\n");
}

static int NumRooms(void)
{
	unsigned char *p = Image + ExitBase;
	unsigned char high = 0;
	
	if (Blizzard)
		return 107;
	while(1) {
		if(*p == 0xFE) {
			high &= 0x7F;
			if (high > MaxRoom)
				return high;
			return MaxRoom;
		}
		/* Found an entry for a higher room */
		if (*p & 0x80) {
			if (*p > high)
				high = *p;
			p++;
			continue;
		}
		/* Found an exit to a higher room */
		if ((p[1] | 0x80) > high) {
			high = p[1] | 0x80;
		}
		p += 2;
	}
}
		
void DumpRooms(void)
{
	int n = NumRooms();
	int i;

	for (i = 0; i < n; i++) {
		printf("%d: ", i);
		PrintRoom(i);
		printf("\n");
		DumpExits(i);
		printf("\n");
	}
}

int main(int argc, char *argv[])
{
	FILE *f;
	
	if(argv[1] == NULL)
	{
		fprintf(stderr, "%s: <file>.\n", argv[0]);
		exit(1);
	}
	
	f = fopen(argv[1], "r");
	if(f == NULL)
	{
		perror(argv[1]);
		exit(1);
	}
	fseek(f, 27L, 0);
	ImageLen = fread(Image, 1, 131072, f);
	fclose(f);
	
	/* Guess initially at He-man style */
	GameVersion = 2;
	/* Blizzard Pass */
	if(ImageLen > 49152) {
		Blizzard = 1;
		GameVersion = 1;
	}
	/* The message analyser will look for version 0 games */
	
	printf("Loaded %d bytes.\n", ImageLen);
	
	VerbBase = FindCode("NORT\001N", 0);
	if(VerbBase == -1) {
		fprintf(stderr, "No verb table!\n");
		exit(1);
	}

	FindTables();
	if (GameVersion > 1)
		Action[12] = "MESSAGE2";
	LoadWordTable();
	NewGame();
	NumLowObjects = GuessLowObjectEnd();
	DumpObjects();
	DumpStatusTable();
	DumpCommandTable();
	DumpRooms();
	DumpMessages();
	DumpMessages2();
	printf("Text End: %04X\n", TopText - Image + 0x4000);
	if (GameVersion == 2)
		printf("Working size needed: %04X\n", TopText - Image - ActionBase);
	else if (GameVersion == 1)
		printf("Working size needed: %04X (%04X - %04X)\n", ObjLocBase - RoomBase, ObjLocBase, RoomBase);
	else
		printf("Working size needed: %04X\n", TopText - Image - ExitBase);
}