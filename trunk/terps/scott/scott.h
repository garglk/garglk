/*
 *	SCOTT FREE
 *
 *	A free Scott Adams style adventure interpreter
 *
 *	Copyright:
 *	    This software is placed under the GNU license.
 *
 *	Statement:
 *	    Everything in this program has been deduced or obtained solely
 *	from published material. No game interpreter code has been
 *	disassembled, only published BASIC sources (PC-SIG, and Byte Dec
 *	1980) have been used.
 */
 
/*
 *	Controlling block
 */
  
#define LIGHT_SOURCE	9		/* Always 9 how odd */
#define CARRIED		255		/* Carried */
#define DESTROYED	0		/* Destroyed */
#define DARKBIT		15
#define LIGHTOUTBIT	16		/* Light gone out */
 
typedef struct
{
 	short Unknown;
 	short NumItems;
 	short NumActions;
 	short NumWords;		/* Smaller of verb/noun is padded to same size */
 	short NumRooms;
 	short MaxCarry;
 	short PlayerRoom;
 	short Treasures;
 	short WordLength;
 	short LightTime;
 	short NumMessages;
 	short TreasureRoom;
} Header;

typedef struct
{
	unsigned short Vocab;
	unsigned short Condition[5];
	unsigned short Action[2];
} Action;

typedef struct
{
	char *Text;
	short Exits[6];
} Room;

typedef struct
{
	char *Text;
	/* PORTABILITY WARNING: THESE TWO MUST BE 8 BIT VALUES. */
	unsigned char Location;
	unsigned char InitialLoc;
	char *AutoGet;
} Item;

typedef struct
{
	short Version;
	short AdventureNumber;
	short Unknown;
} Tail;



#define YOUARE		1	/* You are not I am */
#define SCOTTLIGHT	2	/* Authentic Scott Adams light messages */
#define DEBUGGING	4	/* Info from database load */
#define TRS80_STYLE	8	/* Display in style used on TRS-80 */
#define PREHISTORIC_LAMP 16	/* Destroy the lamp (very old databases) */
