/*
	HDHEADER.H

	contains definitions and prototypes
	for the Debugger build of the Hugo Engine

	Copyright (c) 1997-2006 by Kent Tessman
*/


/*-------------------------------------------------------------------------
	Definitions for the Acorn port

	by ct
---------------------------------------------------------------------------*/

#if defined (ACORN)

#define DEFAULT_NORMAL_TEXT      7      /* white        */
#define DEFAULT_NORMAL_BACK      1      /* blue         */
#define DEFAULT_SELECT_TEXT     15      /* bright white */
#define DEFAULT_SELECT_BACK      3      /* cyan         */
#define DEFAULT_MENU_TEXT        0      /* black        */
#define DEFAULT_MENU_BACK        7      /* white        */
#define DEFAULT_MENU_SELECT     15      /* bright white */
#define DEFAULT_MENU_SELECTBACK  0      /* black        */
#define DEFAULT_BREAKPOINT_BACK  4      /* red          */
#define DEFAULT_CURRENT_BACK     2      /* green        */

#define DEFAULT_OBJECT_TEXT     13      /* light magenta */
#define DEFAULT_PROPERTY_TEXT   11      /* light cyan    */
#define DEFAULT_ROUTINE_TEXT    14      /* yellow        */
#define DEFAULT_STRING_TEXT     10      /* light green   */
#define DEFAULT_TOKEN_TEXT       7      /* white         */
#define DEFAULT_VALUE_TEXT      12      /* light red     */
#define DEFAULT_VARIABLE_TEXT   15      /* bright white  */

#define HORIZONTAL_LINE         '-'
#define HORIZONTAL_LEFT         '+'
#define HORIZONTAL_RIGHT        '+'
#define VERTICAL_LINE           '|'
#define TOP_LEFT                '+'
#define TOP_RIGHT               '+'
#define BOTTOM_LEFT             '+'
#define BOTTOM_RIGHT            '+'

#define MENUBAR_KEY "ALT"               /* name of menubar activation key */

#define HELP_FILE               "hdhelp"
#define SETUP_FILE              "hdsetup"
#define DEFAULT_PRINTER         "printer:"

#define MAXPATH 256

char *itoa(int a, char *buf, int base); char *strupr(char *s);
char *strlwr(char *s);

#endif


/*---------------------------------------------------------------------------
	Definitions for the Amiga port

	by David Kinder
---------------------------------------------------------------------------*/

#if defined (AMIGA)

#define USE_OTHER_MENUS
#define NO_COLOR_SETUP

#define DEFAULT_NORMAL_TEXT      0
#define DEFAULT_NORMAL_BACK      0
#define DEFAULT_SELECT_TEXT      7
#define DEFAULT_SELECT_BACK      1
#define DEFAULT_MENU_TEXT        0
#define DEFAULT_MENU_BACK        7
#define DEFAULT_MENU_SELECT      0
#define DEFAULT_MENU_SELECTBACK  7
#define DEFAULT_BREAKPOINT_BACK  7
#define DEFAULT_CURRENT_BACK     1

#define DEFAULT_OBJECT_TEXT      15
#define DEFAULT_PROPERTY_TEXT    15
#define DEFAULT_ROUTINE_TEXT     15
#define DEFAULT_STRING_TEXT      15
#define DEFAULT_TOKEN_TEXT       15
#define DEFAULT_VALUE_TEXT       15
#define DEFAULT_VARIABLE_TEXT    15

#define HORIZONTAL_LINE         '-'
#define HORIZONTAL_LEFT         '+'
#define HORIZONTAL_RIGHT        '+'
#define VERTICAL_LINE           '|'
#define TOP_LEFT                '+'
#define TOP_RIGHT               '+'
#define BOTTOM_LEFT             '+'
#define BOTTOM_RIGHT            '+'

#define HELP_FILE               "hdhelp.hlp"
#define SETUP_FILE              "hdsetup.ini"
#define DEFAULT_PRINTER         "PRT:"

#define MAXPATH 255

char *itoa(int a, char *buf, int base);
char *strupr(char *s);
char *strlwr(char *s);

#ifndef malloc
#define malloc(a) AmigaMalloc(a)
#endif

#endif  /* defined (AMIGA) */


/*---------------------------------------------------------------------------
	Definitions for djgpp/Allegro builds

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (DJGPP)

/* Debugger default screen colors (standard Hugo color set): */
#define DEFAULT_NORMAL_TEXT      7      /* white        */
#define DEFAULT_NORMAL_BACK      1      /* blue         */
#define DEFAULT_SELECT_TEXT     15      /* bright white */
#define DEFAULT_SELECT_BACK      3      /* cyan         */
#define DEFAULT_MENU_TEXT        0      /* black        */
#define DEFAULT_MENU_BACK        7      /* white        */
#define DEFAULT_MENU_SELECT     15      /* bright white */
#define DEFAULT_MENU_SELECTBACK  0      /* black        */
#define DEFAULT_BREAKPOINT_BACK  4      /* red          */
#define DEFAULT_CURRENT_BACK     2      /* green        */

#define DEFAULT_OBJECT_TEXT     13      /* light magenta */
#define DEFAULT_PROPERTY_TEXT   11      /* light cyan    */
#define DEFAULT_ROUTINE_TEXT    14      /* yellow        */
#define DEFAULT_STRING_TEXT    	10      /* light green   */
#define DEFAULT_TOKEN_TEXT       7      /* white         */
#define DEFAULT_VALUE_TEXT      12      /* light red     */
#define DEFAULT_VARIABLE_TEXT   15      /* bright white  */

/* PC Extended-ASCII line-drawing characters (could be replaced
   with regular ASCII characters, i.e., '-', '+', and '|')
*/
#define HORIZONTAL_LINE         'Ä'
#define HORIZONTAL_LEFT         'Ã'
#define HORIZONTAL_RIGHT        '´'
#define VERTICAL_LINE           '³'
#define TOP_LEFT                'Ú'
#define TOP_RIGHT               '¿'
#define BOTTOM_LEFT             'À'
#define BOTTOM_RIGHT            'Ù'

#define MENUBAR_KEY "ALT"               /* name of menubar activation key */

#define HELP_FILE 		"HDHELP.HLP"
#define SETUP_FILE		"HDSETUP.INI"
#define DEFAULT_PRINTER		"LPT1"

#define min(x, y) ((x<y)?x:y)

#if defined (PROGRAM_NAME)
#undef PROGRAM_NAME
#endif
#if defined (ALLEGRO)
#define PROGRAM_NAME "hdgr"
#else
#define PROGRAM_NAME "hd"
#endif

#endif /* defined (DJGPP) */


/*---------------------------------------------------------------------------
	Definitions for GCC/Unix/Linux (and BeOS)

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (GCC_UNIX) || defined (GCC_BEOS)

/* Debugger default screen colors (standard Hugo color set): */
#define DEFAULT_NORMAL_TEXT      7      /* white        */
#define DEFAULT_NORMAL_BACK      1      /* blue         */
#define DEFAULT_SELECT_TEXT     15      /* bright white */
#define DEFAULT_SELECT_BACK      3      /* cyan         */
#define DEFAULT_MENU_TEXT        0      /* black        */
#define DEFAULT_MENU_BACK        7      /* white        */
#define DEFAULT_MENU_SELECT     15      /* bright white */
#define DEFAULT_MENU_SELECTBACK  0      /* black        */
#define DEFAULT_BREAKPOINT_BACK  4      /* red          */
#define DEFAULT_CURRENT_BACK     2      /* green        */

#define DEFAULT_OBJECT_TEXT     13      /* light magenta */
#define DEFAULT_PROPERTY_TEXT   11      /* light cyan    */
#define DEFAULT_ROUTINE_TEXT    14      /* yellow        */
#define DEFAULT_STRING_TEXT    	10      /* light green   */
#define DEFAULT_TOKEN_TEXT       7      /* white         */
#define DEFAULT_VALUE_TEXT      12      /* light red     */
#define DEFAULT_VARIABLE_TEXT   15      /* bright white  */

#ifdef NCURSES
#include <ncurses.h>
#else
#include <curses.h>
#endif

#ifdef USE_ACS_SYMBOLS
#define HORIZONTAL_LINE         ACS_HLINE
#define HORIZONTAL_LEFT         ACS_LTEE
#define HORIZONTAL_RIGHT        ACS_RTEE
#define VERTICAL_LINE           ACS_VLINE
#define TOP_LEFT                ACS_ULCORNER
#define TOP_RIGHT               ACS_URCORNER
#define BOTTOM_LEFT             ACS_LLCORNER
#define BOTTOM_RIGHT            ACS_LRCORNER
#else
#define HORIZONTAL_LINE         '-'
#define HORIZONTAL_LEFT         '-'
#define HORIZONTAL_RIGHT        '-'
#define VERTICAL_LINE           ' '
#define TOP_LEFT                '-'
#define TOP_RIGHT               '-'
#define BOTTOM_LEFT             '-'
#define BOTTOM_RIGHT            '-'
#endif

#define MENUBAR_KEY "Space"	/* name of menubar activation key */

#define HELP_FILE 		"hdhelp.hlp"
#define SETUP_FILE		"hdsetup"
#define DEFAULT_PRINTER		"/dev/lp"

#define min(x, y) ((x<y)?x:y)

#define MAX_CODE_LINES 16384

#if defined (PROGRAM_NAME)
#undef PROGRAM_NAME
#endif
#define PROGRAM_NAME "hd"

#define STRICMP strcasecmp

#endif /* defined (GCC_UNIX) */


/*---------------------------------------------------------------------------
	Definitions for the Win32 builds
	(Microsoft Visual C++)
	
	by Kent Tessman
--------------------------------------------------------------------------*/

#if defined (WIN32)

/* Debugger default screen colors: */
/* (Note that these are 32-bit RGB colors instead of values from
   the standard Hugo color set; they will, however, fit into int
   color values on a 32-bit compiler.) */
#define DEFAULT_NORMAL_TEXT     0x00000000  /* black        */
#define DEFAULT_NORMAL_BACK     0x00ffffff  /* bright white */
#define DEFAULT_SELECT_TEXT     0x00ffffff  /* bright white */
#define DEFAULT_SELECT_BACK     0x007f0000  /* blue         */

/* These 4 not used: */
#define DEFAULT_MENU_TEXT       0x00000000  /* black        */
#define DEFAULT_MENU_BACK       0x00cfcfcf  /* white        */
#define DEFAULT_MENU_SELECT     0x00ffffff  /* bright white */
#define DEFAULT_MENU_SELECTBACK 0x00000000  /* black        */

#define DEFAULT_BREAKPOINT_BACK 0x0000007f  /* red          */
#define DEFAULT_CURRENT_BACK    0x00007f00  /* green	    */

#define DEFAULT_OBJECT_TEXT     0x007f007f  /* magenta	    */
#define DEFAULT_PROPERTY_TEXT   0x007f7f00  /* cyan	    */
#define DEFAULT_ROUTINE_TEXT    0x00007f7f  /* brown(ish)   */
#define DEFAULT_STRING_TEXT    	0x00007f00  /* green	    */
#define DEFAULT_TOKEN_TEXT      0x00000000  /* black	    */
#define DEFAULT_VALUE_TEXT      0x0000007f  /* red	    */
#define DEFAULT_VARIABLE_TEXT   0x007f0000  /* blue	    */

#define HORIZONTAL_LINE         '-'
#define HORIZONTAL_LEFT         '+'
#define HORIZONTAL_RIGHT        '+'
#define VERTICAL_LINE           '|'
#define TOP_LEFT                '+'
#define TOP_RIGHT               '+'
#define BOTTOM_LEFT             '+'
#define BOTTOM_RIGHT            '+'

#define NO_WINDOW_PROMPTS	/* i.e., no "Press a key" after one screen
				   has been filled */
#define USE_OTHER_MENUS
/* Not used: */
#define MENUBAR_KEY "ALT"

#define HELP_FILE 		"hdhelp.hlp"
#define SETUP_FILE		"hdsetup.ini"
#define DEFAULT_PRINTER		"LPT1"

#define DEBUGGER_PRINTFATALERROR(a) PrintFatalDebuggerError(a)
void PrintFatalDebuggerError(char *a);

#define MAX_CODE_LINES 16384

#if defined (PROGRAM_NAME)
#undef PROGRAM_NAME
#endif
#define PROGRAM_NAME "hdwin"

#endif /* defined (WIN32) */


/*---------------------------------------------------------------------------
	MS-DOS specific definitions for Microsoft QuickC

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (QUICKC)

#include <conio.h>

/* Debugger default screen colors (standard Hugo color set): */
#define DEFAULT_NORMAL_TEXT      7      /* white        */
#define DEFAULT_NORMAL_BACK      1      /* blue         */
#define DEFAULT_SELECT_TEXT     15      /* bright white */
#define DEFAULT_SELECT_BACK      3      /* cyan         */
#define DEFAULT_MENU_TEXT        0      /* black        */
#define DEFAULT_MENU_BACK        7      /* white        */
#define DEFAULT_MENU_SELECT     15      /* bright white */
#define DEFAULT_MENU_SELECTBACK  0      /* black        */
#define DEFAULT_BREAKPOINT_BACK  4      /* red          */
#define DEFAULT_CURRENT_BACK     2      /* green        */

#define DEFAULT_OBJECT_TEXT     13      /* light magenta */
#define DEFAULT_PROPERTY_TEXT   11      /* light cyan    */
#define DEFAULT_ROUTINE_TEXT    14      /* yellow        */
#define DEFAULT_STRING_TEXT    	10      /* light green   */
#define DEFAULT_TOKEN_TEXT       7      /* white         */
#define DEFAULT_VALUE_TEXT      12      /* light red     */
#define DEFAULT_VARIABLE_TEXT   15      /* bright white  */

/* PC Extended-ASCII line-drawing characters (could be replaced
   with regular ASCII characters, i.e., '-', '+', and '|')
*/
#define HORIZONTAL_LINE         'Ä'
#define HORIZONTAL_LEFT         'Ã'
#define HORIZONTAL_RIGHT        '´'
#define VERTICAL_LINE           '³'
#define TOP_LEFT                'Ú'
#define TOP_RIGHT               '¿'
#define BOTTOM_LEFT             'À'
#define BOTTOM_RIGHT            'Ù'

#define MENUBAR_KEY "ALT"               /* name of menubar activation key */

#define HELP_FILE 		"HDHELP.HLP"
#define SETUP_FILE		"HDSETUP.INI"
#define DEFAULT_PRINTER		"LPT1"

#define MAXPATH		_MAX_PATH	/* maximum number of characters */

/* Case-insensitive string compare */
#define STRICMP(a, b)           stricmp(a, b)

#if defined (PROGRAM_NAME)
#undef PROGRAM_NAME
#endif
#define PROGRAM_NAME "hd"

#endif /* defined (QUICKC) */


/*--------------------------------------------------------------------------*/

#if !defined (PROGRAM_NAME)
#define PROGRAM_NAME "hd"
#endif

#if !defined (STRICMP)
#define STRICMP(a, b)   stricmp(a, b)
#endif

#define true    1
#define false   0

#ifndef MAX_CODE_LINES
#define MAX_CODE_LINES	1024
#endif
#ifndef MAX_WATCHES
#define MAX_WATCHES	  32
#endif

#define D_SEPARATOR             ((D_SCREENHEIGHT)/2)
#define Center(a)		((D_SCREENWIDTH/2)-strlen(a)/2)


/* Colors:

	The order of these constants is significant; see SetupColors()
	in hdtools.c.
*/
enum D_COLOR_TYPE
{
        NORMAL_TEXT = 0, NORMAL_BACK, SELECT_TEXT, SELECT_BACK,
	MENU_TEXT, MENU_BACK, MENU_SELECT, MENU_SELECTBACK,
	BREAKPOINT_BACK, CURRENT_BACK, OBJECT_TEXT, PROPERTY_TEXT,
        ROUTINE_TEXT, STRING_TEXT, TOKEN_TEXT, VALUE_TEXT,
	VARIABLE_TEXT
};


/* Menu data: */

#define MENU_HEADINGS     6
#define MENU_SUBHEADINGS 13

#define MENU_INACTIVE   (-1)    /* MENU_CONSTANTS */
#define MENU_FILE       0x10
#define MENU_VIEW       0x20
#define MENU_RUN        0x30
#define MENU_DEBUG      0x40
#define MENU_TOOLS      0x50
#define MENU_HELP       0x60

#define FILE_RESTART     	1
#define FILE_PRINT       	2
#define FILE_EXIT        	4

#define VIEW_WATCH       	1
#define VIEW_CALLS       	2
#define VIEW_BREAKPOINTS 	3
#define VIEW_LOCALS      	4
#define VIEW_ALIASES     	5
#define VIEW_HELP		7
#define VIEW_AUXILIARY		8
#define VIEW_OUTPUT      	10

#define RUN_GO           	1
#define RUN_FINISH	 	2
#define RUN_STEP	 	4
#define RUN_STEPOVER	 	5
#define RUN_SKIP	 	6
#define RUN_STEPBACK	 	7

#define DEBUG_SEARCH	 	1
#define DEBUG_WATCH      	3
#define DEBUG_SET        	4
#define DEBUG_BREAKPOINT 	5
#define DEBUG_OBJTREE	 	7
#define DEBUG_MOVEOBJ	 	8
#define DEBUG_NESTING           10
#define DEBUG_WARNINGS		11

#define TOOLS_SETUP      	1

#define HELP_TOPIC       	1
#define HELP_KEYS        	2
#define HELP_ABOUT       	3

struct menu_structure
{
	int position;           /* horizontal screen position 	*/
	int items;              /* in this menu 		*/
	int longest;            /* longest item 		*/
	char shortcut_key;      /* on menu bar 			*/
};

struct token_structure
{
	char *token;		/* name 			*/
	int len;		/* length in bytes of code 	*/
	int col;		/* color-coding 		*/
	int following;		/* following character or 0 	*/
};


/*-------------------------------------------------------------------------*/
void debug_clearview(int n);
void debug_cursor(int n);
void debug_getevent(void);
void debug_hardcopy(FILE *printer, char *a);
char *debug_input(int x, int y, int maxlen, char *def);
void debug_print(char *a);
void debug_setbackcolor(int c);
void debug_settextcolor(int c);
void debug_settextpos(int x, int y);
void debug_shortcutkeys(void);
void debug_switchscreen(int screen);
void debug_windowbottomrow(char *caption);
void debug_windowrestore(void *buf, int xpos, int ypos);
void *debug_windowsave(int left, int top, int right, int bottom);
void debug_windowscroll(int left, int top, int right, int bottom, int param, int lines);
void debug_windowshadow(int left, int top, int right, int bottom);


/* hd.c */
extern char *menu[][MENU_SUBHEADINGS];
extern struct menu_structure menu_data[];
extern char menu_shortcut_keys[];
extern char menubar_isactive;
extern int active_menu_heading;
extern char **choice;


/* hddecode.c */
void AddString(char *s, int *l, int lpos, int coltype);
int *DecodeLine(long addr);
struct token_structure GetToken(unsigned char *arr, long addr, int firsttoken);
void int_print(int *a, int col, int pos, int width);
int int_strlen(int *a);
void int_strcpy(int *a, int *b);

extern char format_nesting;
extern char screen_line[];


/* hdmisc.c */
void About(void);
void *AllocMemory(size_t size);
char *CurrentRoutineName(unsigned int addr);
void DeleteBreakpoint(long baddr);
void EnterBreakpoint(void);
void EnterHelpTopic(void);
void EnterSearch(void);
long HextoDec(char *hex);
int ObjectNumber(char *obj);
void ReturnWatchValue(char *ln, int val, int type);
void RuntimeWarning(char *a);
int SearchHelp(char *t);
int SelectWatchType(void);
void SetBreakpoint(long addr);
long StealAddress(int l);

extern char invocation_path[];
extern int D_SCREENWIDTH; extern int D_SCREENHEIGHT;
extern char in_help_mode;
extern int debug_call_stack;


/* hdtools.c */
void DebugMoveObj(void);
void DrawTree(void);
void LoadSetupFile(void);
void SaveSetupFile(void);
void SetupMenu(void);

extern int color[];


/* hdupdate.c */
void HardCopy(void);
void HighlightCurrent(int a);
void Navigate(void);
void SwitchtoView(int v);
void UpdateCodeWindow(void);
void UpdateFullCodeWindow(unsigned int start, int horizontal, int width);

extern int active_window;
extern int active_view;
extern int currently_updating;
extern int local_view_type;
extern int device_error; extern char printer_name[];


/* hdval.c */
void DeleteWatch(int w);
void EnterWatch(void);
void ModifyValue(void);


/* hdwindow.c */
void AllocateChoices(int n);
char *InputBox(char *title, char *caption, int maxlen, char *def);
void OpenMenubar(void);
void OpenMenu(int m);
void PrintMenubar(void);
void PrintScreenBorders(void);
int SelectBox(int n, int def);
