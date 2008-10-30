/*
	HEHEADER.H

	contains definitions and prototypes
	for the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


#define HEVERSION 3
#define HEREVISION 1
#if !defined (COMPILE_V25)
#define HEINTERIM ".03"
#else
#define HEINTERIM ".03 (2.5)"
#endif

#include <string.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* The Hugo token set and their respective values are contained
   separately in htokens.h
*/
#include "htokens.h"
extern char *token[];


/* Define the compiler being used (or library) as one of:

	ACORN
	AMIGA
	BEOS
	DJGPP
	GCC_OS2
	GCC_UNIX
	GCC_BEOS
	GLK       (Glk portable interface)
	PALMOS    (Palm OS built with CodeWarrior)
	QUICKC
	RISCOS
	WIN32     (Microsoft Visual C++)
	WXWINDOWS (wxWindows for Mac or Gtk)

   (although this is typically done in the makefile)
*/

/*---------------------------------------------------------------------------
	Definitions for the Acorn Archimedes & RPC
	using Acorn C Compiler (v4)

	by Colin Turnbull
---------------------------------------------------------------------------*/

#if defined (ACORN)

#define PORT_NAME "Acorn"
#define PORTER_NAME "ct"

#define HUGO_FOPEN      arc_fopen

#define MAXPATH         256
#define MAXFILENAME     64      /* We _could_ be using LongFilenames */
#define MAXDRIVE        32      /* arcfs#SCSIFS::harddisc4 ? */
#define MAXDIR          256
#define MAXEXT          32

#define isascii(c)      ((unsigned int)(c)<0x80)
#define toascii(c)      ((c)&0x7f)

#define DEF_PRN         ""

#define DEF_FCOLOR      7
#define DEF_BGCOLOR     0
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 255

/* Added these functions for the Acorn port: */
FILE *arc_fopen(const char *, const char *);
void arc_colour(unsigned char n, unsigned int c);

#endif /* defined (ACORN) */


/*---------------------------------------------------------------------------
	Definitions for the djgpp/Allegro build(s)

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (DJGPP) && !defined (GCC_UNIX)
#include <unistd.h>
#include <dir.h>

#define PORT_NAME "32-bit"
#define PORTER_NAME "Kent Tessman"

#define EXTRA_STRING_FUNCTIONS

#define DEF_PRN "LPT1"

/* MAXPATH, MAXDRIVE, MAXDIR, MAXEXT defined in <dir.h> */
#define MAXFILENAME     MAXFILE

#define DEF_FCOLOR      7
#define DEF_BGCOLOR     0
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 255

#define HUGO_INLINE static __inline

#if defined (ALLEGRO)
#define FRONT_END
#define GRAPHICS_SUPPORTED
#define SOUND_SUPPORTED
#undef PORT_NAME
#define PORT_NAME "32-bit Allegro"

#if !defined (PROGRAM_NAME) && !defined (DEBUGGER)
#define PROGRAM_NAME "hegr"
#elif defined (DEBUGGER)
#define PROGRAM_NAME "hdgr"
#endif
#endif	/* defined (ALLEGRO) */

#endif  /* defined (DJGPP) */


/*---------------------------------------------------------------------------
	Definitions for the Amiga port

	by David Kinder
---------------------------------------------------------------------------*/

#if defined (AMIGA)

#define PORT_NAME "Amiga"
#define PORTER_NAME "David Kinder"

#define MAXPATH         256
#define MAXFILENAME     256
#define MAXDRIVE        256
#define MAXDIR          256
#define MAXEXT          256

#define DEF_PRN         "PRT:"

#define DEF_FCOLOR      7
#define DEF_BGCOLOR     0
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 255

#endif  /* defined (AMIGA) */


/*---------------------------------------------------------------------------
	Definitions for the GCC Unix port

	Originally by Bill Lash
---------------------------------------------------------------------------*/

#if defined (GCC_UNIX) || defined (GCC_BEOS)

#include <unistd.h>

#ifndef GCC_BEOS
#define PORT_NAME "Unix"
#ifndef DEBUGGER
#define PORTER_NAME "Bill Lash"
#else
#define PORTER_NAME "Kent Tessman"
#endif /* DEBUGGER */
#else
#define PORT_NAME "BeOS"
#define PORTER_NAME "Kent Tessman"
#endif /* GCC_BEOS */

#define EXTRA_STRING_FUNCTIONS

#define MAXPATH         256
#define MAXFILENAME     256
#define MAXDRIVE        256
#define MAXDIR          256
#define MAXEXT          256

#define DEF_PRN         "/dev/lp"       /* Bill suggests this is perhaps
					   a less-than-ideal choice for
					   the default printer */
#define DEF_FCOLOR      7
#define DEF_BGCOLOR     0
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 255

#define HUGO_INLINE static __inline

#endif /* defined (GCC_UNIX) */


/*---------------------------------------------------------------------------
	Definitions for the GCC OS/2 port

	by Gerald Bostock
---------------------------------------------------------------------------*/

#if defined (GCC_OS2)

#define PORT_NAME "OS/2"
#define PORTER_NAME "Gerald Bostock"

#define EXTRA_STRING_FUNCTIONS

#define MAXPATH         256
#define MAXFILENAME     256
#define MAXDRIVE        256
#define MAXDIR          256
#define MAXEXT          256

#define DEF_PRN         "LPT1"       
#define DEF_FCOLOR      7
#define DEF_BGCOLOR     0
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 255

#define HUGO_INLINE static __inline

#endif /* defined (GCC_OS2) */


/*---------------------------------------------------------------------------
	Definitions for the MacOS Glk port

	by Andrew Plotkin
---------------------------------------------------------------------------*/

#if defined (GLK) && defined (__MACOS__)

#define BUILD_RANDOM
#define RANDOM random
#define SRANDOM srandom

/* Some symbol redefinitions necessary on the Mac, due to conflicts 
   with Mac Toolbox names
*/
#define GetString HugoGetString
#define FindWord HugoFindWord

#endif	/* Mac Glk */


/*---------------------------------------------------------------------------
	Definitions for the Glk port

	by Kent Tessman and Andrew Plotkin
---------------------------------------------------------------------------*/

#if defined (GLK)

#include "glk.h"

#define PORT_NAME "Glk"
#define PORTER_NAME "Kent Tessman and Andrew Plotkin"

#define HUGO_FILE	strid_t
#define MAXPATH         256
#define MAXFILENAME     256
#define MAXDRIVE        256
#define MAXDIR          256
#define MAXEXT          256

#define DEF_PRN         ""
#define DEF_FCOLOR      0
#define DEF_BGCOLOR     15
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 1280	/* larger than normal since Glk doesn't
			   break up paragraphs (1024+256)*/

#ifndef HUGO_INLINE
#define HUGO_INLINE __inline
#endif

#define FRONT_END
#define MINIMAL_INTERFACE

int heglk_get_linelength(void);
#define ACTUAL_LINELENGTH() heglk_get_linelength()
int heglk_get_screenheight(void);
#define ACTUAL_SCREENHEIGHT() heglk_get_screenheight()

void heglk_printfatalerror(char *err);
#define PRINTFATALERROR(a) heglk_printfatalerror(a)

#define EXTRA_STRING_FUNCTIONS

#ifndef isascii
#define isascii(c)	(1)
#endif
#define tolower(c)      (glk_char_to_lower((unsigned char)c))
#define toupper(c)      (glk_char_to_upper((unsigned char)c))

/* Must always close Glk via glk_exit() */
#define exit(n)		(glk_exit())

/* Glk doesn't use stdio; mapping these will cut radically down on
   #ifdef sections in the source:
*/
#undef fclose
#define fclose(f)	(glk_stream_close(f, NULL), 0)
#undef ferror
#define ferror(f)	(0)	/* No ferror() equivalent */
#undef fgetc
#define fgetc(f)	(glk_get_char_stream(f))
#undef fgets
#define fgets(a, n, f)	(glk_get_line_stream(f, a, n))
#undef fread
#define fread(a,s,n,f)	(glk_get_buffer_stream(f, (char *)a, n))
#undef fprintf
#define fprintf(f,s,a)	(glk_put_string_stream(f, a), 0)
#undef fputc
#define fputc(c, f)	(glk_put_char_stream(f, (unsigned char)(c)), 0)
#undef fputs
#define fputs(s, f)	(glk_put_buffer_stream(f, s, strlen(s)), 0)
#undef ftell
#define ftell(f)	(glk_stream_get_position(f))
#undef fseek
#define fseek(f, p, m)	(glk_stream_set_position(f, p, m), 0)
#undef SEEK_SET
#define SEEK_SET	seekmode_Start
#undef SEEK_END
#define SEEK_END	seekmode_End

#if defined (_WINDOWS)
#define SETTITLE_SUPPORTED
#define PRINTFATALERROR_SUPPLIED
#endif

#endif /* defined (GLK) */


/*---------------------------------------------------------------------------
	Definitions for Palm OS (built with CodeWarrior)

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (PALMOS)

#include "palm_stdio.h"
#include "palm_ctype.h"
#include "palm_stdlib.h"
#include "palm_string.h"
#include "palm_time.h"

#define PORT_NAME "Palm OS"
#define PORTER_NAME "Kent Tessman"

#define HUGO_FILE	PH_FILE*
#define HUGO_FOPEN	ph_fopen

#define MAXFILENAME     MAXPATH
/* No such thing as drive, directory, or extension in Palm OS */
#define MAXDRIVE        1
#define MAXDIR          1
#define MAXEXT          1

#define DEF_PRN         ""

#define DEF_FCOLOR      16
#define DEF_BGCOLOR     17
#define DEF_SLFCOLOR	18
#define DEF_SLBGCOLOR	19

#define MAXBUFFER	256		/* max. input/output line length */

#define HUGO_INLINE __inline

#define NO_TERMINAL_LINEFEED
#define MINIMAL_WINDOWING
#define FRONT_END
#define SCROLLBACK_DEFINED
#define USE_TEXTBUFFER
#define TEXTBUFFER_FORMATTING
#define MAX_TEXTBUFFER_COUNT 100
#define USE_SMARTFORMATTING

extern void PopupMessage(char *title, char *msg);
extern char ph_debug;

#define PRINTFATALERROR PrintFatalError
#define PROMPTMORE_REPLACED
#define LOADGAMEDATA_REPLACED
#define RESTOREGAMEDATA_REPLACED
#define SAVEGAMEDATA_REPLACED
#define MEM(addr)	GetMemValue(addr)
#define SETMEM(addr, n)	SetMemValue(addr, n)
unsigned char GetMemValue(long addr);
void SetMemValue(long addr, unsigned char n);

#endif /* defined (PALMOS) */


/*---------------------------------------------------------------------------
	Definitions for the RISC OS port

	by Julian Arnold
---------------------------------------------------------------------------*/

#if defined (RISCOS)

#define PORT_NAME	"RISC OS"
#define PORTER_NAME	"Julian Arnold"

#define MAXPATH		256
#define MAXFILENAME	256
#define MAXDRIVE	256
#define MAXDIR		256
#define MAXEXT		256

#define DEF_PRN		""

#define DEF_FCOLOR	7
#define DEF_BGCOLOR	0
#define DEF_SLFCOLOR	0
#define DEF_SLBGCOLOR	7

#define MAXBUFFER	255

#define isascii(c)	((unsigned int)(c)<0x80)
extern FILE *hugo_fopen (char *filename, char *mode);
#define HUGO_FOPEN	hugo_fopen

#endif /* defined (RISCOS) */


/*---------------------------------------------------------------------------
	MS-DOS specific definitions for Microsoft QuickC

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (QUICKC)

#include <conio.h>

#define PORT_NAME "16-bit"
#define PORTER_NAME "Kent Tessman"

#define MATH_16BIT

#define MAXPATH         _MAX_PATH       /* maximum allowable lengths */
#define MAXFILENAME     _MAX_FNAME
#define MAXDRIVE        _MAX_DRIVE
#define MAXDIR          _MAX_DIR
#define MAXEXT          _MAX_EXT

#define DEF_PRN         "LPT1"          /* default printer */

#define DEF_FCOLOR      7               /* default colors, fore and back */
#define DEF_BGCOLOR     0
#define DEF_SLFCOLOR	15		/* statusline */
#define DEF_SLBGCOLOR	1

#define OMIT_EXTRA_STRING_FUNCTIONS

#define MAXBUFFER 255                   /* max. input/output line length */

#endif /* defined (QUICKC) */


/*---------------------------------------------------------------------------
	Definitions for Win32 (Microsoft Visual C++)
	and WinCE (eVC++ 4.0/WCE SDK)

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (WIN32) && !defined (WXWINDOWS)

#ifdef UNDER_CE
#define PORT_NAME "WinCE"
#else
#define PORT_NAME "Win32"
#endif
#define PORTER_NAME "Kent Tessman"

#define DEF_PRN ""

#ifdef UNDER_CE
#define MAXPATH		260
#define MAXFILENAME	256
#define MAXDRIVE	3
#define MAXDIR		256
#define MAXEXT		256
#define getenv(a) ""
#else
#define MAXPATH         _MAX_PATH
#define MAXFILENAME     _MAX_FNAME
#define MAXDRIVE        _MAX_DRIVE
#define MAXDIR          _MAX_DIR
#define MAXEXT          _MAX_EXT
#endif

#ifdef UNDER_CE
#define DEF_FCOLOR      0
#define DEF_BGCOLOR     15
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1
#else
#define DEF_FCOLOR      7
#define DEF_BGCOLOR     1
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	0
#endif

#define MAXBUFFER 255
#define MAXUNDO 1024

#define HUGO_INLINE __inline

#define NO_TERMINAL_LINEFEED
#define BUILD_RANDOM
#define FRONT_END
#define USE_TEXTBUFFER
#define SCROLLBACK_DEFINED

#ifndef UNDER_CE
#ifndef USE_NT_SOUND_ONLY
#define SPEECH_ENABLED
#endif
#define USE_SMARTFORMATTING
#else
#define MINIMAL_WINDOWING
#endif	// UNDER_CE

#undef FindResource

void PrintFatalError(char *a);
#define PRINTFATALERROR(a) PrintFatalError(a);

#endif  /* defined (WIN32) */


/*---------------------------------------------------------------------------
	Definitions for the wxWindows builds (wxGTK, wxMac, etc.)
	and BeOS

	by Kent Tessman
---------------------------------------------------------------------------*/

/* Metrowerks compiler shortcomings */
#if defined (__POWERPC__) && defined (__MWERKS__) && !defined (__MACOS__)
#define BEOS
#define	isascii(c) (((c) & ~0x7f) == 0)  /* If c is a 7 bit value */
#define resource_type hugo_resource_type
#endif

#if defined (WXWINDOWS) || (defined (BEOS) && !defined (GCC_UNIX) && !defined(GCC_BEOS))

#ifdef __MACOS__
#define GetString HugoGetString
#define FindWord HugoFindWord
#endif
#if defined (__MACOS__) || defined (BEOS)
#define USE_SMARTFORMATTING
#endif

#define PORTER_NAME "Kent Tessman"
#if defined (WXWINDOWS)
#define PORT_NAME "wxWindows"
#define PROGRAM_NAME "hewx"
#else
#define PORT_NAME "BeOS"
#define PROGRAM_NAME "BeHugo"
void exit_he_thread(int n);
#define exit(n) exit_he_thread(n)
#undef DEBUGGER
#endif

#define DEF_PRN ""

#define MAXPATH         256
#define MAXDRIVE        256
#define MAXDIR          256
#define MAXFILENAME     256
#define MAXEXT          256

#define DEF_FCOLOR      0
#define DEF_BGCOLOR     15
#define DEF_SLFCOLOR	15
#define DEF_SLBGCOLOR	1

#define MAXBUFFER 255
#define MAXUNDO 1024

#define HUGO_INLINE static __inline

#define NO_TERMINAL_LINEFEED
#define FRONT_END
#define USE_TEXTBUFFER
#ifndef __WXMAC__
#define SCROLLBACK_DEFINED
#endif

extern void PrintFatalError(char *a);
#define PRINTFATALERROR(a) PrintFatalError(a);

#endif  /* defined (WXWINDOWS) || defined (BEOS) */


/*-------------------------------------------------------------------------*/

#if !defined (PROGRAM_NAME) && !defined (DEBUGGER)
#define PROGRAM_NAME     "he"
#endif

#if !defined (MEM)
#define MEM(addr) (mem[addr])
#endif

#if !defined (SETMEM)
#define SETMEM(addr, n) (mem[addr] = n)
#endif

#if !defined (GETMEMADDR)
#define GETMEMADDR(addr) (&mem[addr])
#endif

#if !defined (HUGO_PTR)
#define HUGO_PTR
#endif

#if !defined (HUGO_FILE)
#define HUGO_FILE FILE*
#endif

#if !defined (HUGO_FOPEN)
#define HUGO_FOPEN fopen
#endif

#ifndef HUGO_INLINE
#define NO_INLINE_MEM_FUNCTIONS
#define HUGO_INLINE static
#endif

#if defined (BUILD_RANDOM)
extern int random(void);
extern void srandom(int);
#endif

#if !defined (PRINTFATALERROR)
#define PRINTFATALERROR(a)	fprintf(stderr, a)
#endif

#ifndef OMIT_EXTRA_STRING_FUNCTIONS
#define EXTRA_STRING_FUNCTIONS
#endif

#ifdef LOADGAMEDATA_REPLACED
/* If LOADGAMEDATA_REPLACED is #defined, LoadGameData() is called by
   both LoadGame() and RunRestart().  RunRestart() sets the reload flag
   to true.
*/
int LoadGameData(char reload);
#endif

/* To be used with caution; obviously, not all non-zero values are "true"
   in this usage.
*/
#define true 1
#define false 0

/* These static values are not changeable--they depend largely on internals
   of the Engine.
*/
#define MAXATTRIBUTES    128
#define MAXGLOBALS       240
#define MAXLOCALS         16
#define MAXPOBJECTS      256    /* contenders for disambiguation */
#define MAXWORDS          32    /* in an input line              */
#define MAXSTACKDEPTH    256	/* for nesting {...}		 */

#if !defined (MAXUNDO)
#define MAXUNDO          256	/* number of undoable operations */
#endif

#if !defined (COMPILE_V25)
#define MAX_CONTEXT_COMMANDS	32
#endif

enum ERROR_TYPE                 /* fatal errors */
{
	MEMORY_E = 1,           /* out of memory                */
	OPEN_E,                 /* error opening file           */
	READ_E,                 /* error reading from file      */
	WRITE_E,                /* error writing to file        */
	EXPECT_VAL_E,           /* expecting value              */
	UNKNOWN_OP_E,           /* unknown operation            */
	ILLEGAL_OP_E,           /* illegal operation            */
	OVERFLOW_E,             /* overflow                     */
	DIVIDE_E		/* divide by zero		*/
};

/* The positions of various data in the header: */
#define H_GAMEVERSION	0x00
#define H_ID		0x01
#define H_SERIAL	0x03
#define H_CODESTART	0x0B

#define H_OBJTABLE	0x0D           /* data tables */
#define H_PROPTABLE	0x0F
#define H_EVENTTABLE	0x11
#define H_ARRAYTABLE	0x13
#define H_DICTTABLE	0x15
#define H_SYNTABLE	0x17

#define H_INIT		0x19           /* junction routines */
#define H_MAIN		0x1B
#define H_PARSE		0x1D
#define H_PARSEERROR	0x1F
#define H_FINDOBJECT	0x21
#define H_ENDGAME	0x23
#define H_SPEAKTO	0x25
#define H_PERFORM	0x27

#define H_TEXTBANK	0x29

/* additional debugger header information */
#define H_DEBUGGABLE     0x3A
#define H_DEBUGDATA      0x3B
#define H_DEBUGWORKSPACE 0x3E

/* Printing control codes--embedded in strings printed by AP(). */
#define FONT_CHANGE       1
#define COLOR_CHANGE      2
#define NO_CONTROLCHAR    3
#define NO_NEWLINE        30
#define FORCED_SPACE      31	/* Can't be <= # colors/font codes + 1
				   (See AP() for the reason) */				

/* Font control codes--these bitmasks follow FONT_CHANGE codes. */
#define NORMAL_FONT	  0
#define BOLD_FONT         1
#define ITALIC_FONT       2
#define UNDERLINE_FONT    4
#define PROP_FONT         8

/* CHAR_TRANSLATION is simply a value that is added to an ASCII character
   in order to encode the text, i.e., make it unreadable to casual
   browsing.
*/
#define CHAR_TRANSLATION  0x14

/* Passed to GetWord() */
#define PARSE_STRING_VAL  0xFFF0
#define SERIAL_STRING_VAL 0xFFF1

/* Returned by FindWord() */
#define UNKNOWN_WORD      0xFFFF

/* Bitmasks for certain qualities of properties */
#define ADDITIVE_FLAG   1
#define COMPLEX_FLAG    2

/* Property-table indicators */
#define PROP_END          255
#define PROP_ROUTINE      255

enum RESOURCE_TYPE
{
	JPEG_R,			/* JPEG image */
	WAVE_R,			/* RIFF WAVE audio sample */
	MOD_R,			/* MOD music module */
	S3M_R,			/* S3M music module */
	XM_R,			/* XM music module */
	MIDI_R,			/* MIDI music */
	MP3_R,			/* MP3 audio layer */
	AVI_R,			/* Video for Windows */
	MPEG_R,			/* MPEG video */
	UNKNOWN_R
};

/* A structure used for disambiguation in MatchObject(): */
struct pobject_structure
{
	int obj;		/* the actual object number */
	char type;              /* referred to by noun or adjective */
};

/* Structure used for navigating {...} blocks: */
struct CODE_BLOCK
{
	int type;		/* see #defines, below */
	long brk;		/* break address, or 0 to indicate NOP */
	long returnaddr;	/* used only for do-while loops */
#if defined (DEBUGGER)
	int dbnest;		/* for recovering from 'break' */
#endif
};

#define RESET_STACK_DEPTH (-1)

#define RUNROUTINE_BLOCK  1
#define CONDITIONAL_BLOCK 2
#define DOWHILE_BLOCK     3

#define TAIL_RECURSION_ROUTINE          (-1)
#define TAIL_RECURSION_PROPERTY         (-2)


/*-------------------------------------------------------------------------*/

/* (The following values are defined in he.c:) */

/* Library/engine globals */
extern const int object;
extern const int xobject;
extern const int self;
extern const int wordcount;
extern const int player;
extern const int actor;
extern const int location;
extern const int verbroutine;
extern const int endflag;
extern const int prompt;
extern const int objectcount;
extern const int system_status;

/* Library/engine properties */
extern const int before;
extern const int after;
extern const int noun;
extern const int adjective;
extern const int article;

/* "display" object properties */
extern const int screenwidth;
extern const int screenheight;
extern const int linelength;
extern const int windowlines;
extern const int cursor_column;
extern const int cursor_row;
extern const int hasgraphics;
extern const int title_caption;
extern const int hasvideo;
extern const int needs_repaint;
extern const int pointer_x;
extern const int pointer_y;

void *hugo_blockalloc(long num);
void hugo_blockfree(void *block);
int hugo_hasgraphics(void);
int hugo_charwidth(char a);
void hugo_cleanup_screen(void);
void hugo_clearfullscreen(void);
void hugo_clearwindow(void);
void hugo_closefiles(void);
void hugo_font(int f);
void hugo_getfilename(char *a, char *b);
int hugo_getkey(void);
void hugo_getline(char *p);
void hugo_init_screen(void);
void hugo_makepath(char *path, char *drive, char *dir, char *fname, char *ext);
int hugo_overwrite(char *f);
void hugo_print(char *a);
void hugo_scrollwindowup(void);
void hugo_setbackcolor(int c);
void hugo_setgametitle(char *t);
void hugo_settextcolor(int c);
void hugo_settextmode(void);
void hugo_settextpos(int x, int y);
void hugo_settextwindow(int left, int top, int right, int bottom);
void hugo_splitpath(char *path, char *drive, char *dir, char *fname, char *ext);
int hugo_strlen(char *a);
int hugo_textwidth(char *a);
int hugo_waitforkey(void);
int hugo_iskeywaiting(void);
int hugo_timewait(int n);
#if defined (SCROLLBACK_DEFINED)
void hugo_sendtoscrollback(char *a);
#endif
#if !defined (COMPILE_V25)
int hugo_hasvideo(void);
#endif


/* he.c */
#if !defined (FRONT_END)
int main(int argc, char *argv[]);
#else
int he_main(int argc, char *argv[]);
#endif
void Banner(void);

extern int address_scale;
extern char program_path[];


/* hebuffer.c */
#ifdef USE_TEXTBUFFER
void TB_Init(void);
int TB_AddWord(char *w, int, int, int, int);
void TB_Remove(int n);
void TB_Scroll();
void TB_Clear(int, int, int, int);
char *TB_FindWord(int x, int y);
int TB_FirstCell(void);
#ifdef TEXTBUFFER_SAVEALL
int TB_AddWin(int, int, int, int);
#endif

extern int tb_selected;
extern char allow_text_selection;
#endif


/* heexpr.c */
int EvalExpr(int ptr);
int GetValue(void);
int Increment(int v, char inctype);
char IsIncrement(long addr);
void SetupExpr(void);

extern int var[];
extern int incdec;
extern char getaddress;
extern char inexpr;
extern char inobj;


/* hemisc.c */
void AP(char *a);
int CallRoutine(unsigned int addr);
void ContextCommand(void);
unsigned int Dict(void);
void FatalError(int e);
void FileIO(void);
void Flushpbuffer(void);
void GetCommand(void);
char *GetString(long addr);
char *GetText(long textaddr);
char *GetWord(unsigned int a);
void HandleTailRecursion(long addr);
void InitGame(void);
void LoadGame(void);
void ParseCommandLine(int argc, char *argv[]);
void PassLocals(int n);
#ifdef NO_INLINE_MEM_FUNCTIONS
unsigned char Peek(long a);
unsigned int PeekWord(long a);
void Poke(unsigned int a, unsigned char v);
void PokeWord(unsigned int a, unsigned int v);
#endif
char *PrintHex(long a);
void Printout(char *a);
/* void PrintSetting(int t, int a, int b, int c, int d); */
void PromptMore(void);
int RecordCommands(void);
void SaveUndo(int t, int a, int b, int c, int d);
void SetStackFrame(int depth, int type, long brk, long returnaddr);
void SetupDisplay(void);
char SpecialChar(char *a, int *i);
HUGO_FILE TrytoOpen(char *f, char *p, char *d);
int Undo(void);

extern int game_version;
extern int object_size;
extern HUGO_FILE game;
extern HUGO_FILE script;
extern HUGO_FILE save;
extern HUGO_FILE playback;
extern HUGO_FILE record;
extern HUGO_FILE io; extern char ioblock; extern char ioerror;
extern char gamefile[];
extern char gamepath[];
#if !defined (GLK)
extern char scriptfile[];
extern char savefile[];
extern char recordfile[];
#endif
extern char id[];
extern char serial[];
extern unsigned int codestart;
extern unsigned int objtable;
extern unsigned int eventtable;
extern unsigned int proptable;
extern unsigned int arraytable;
extern unsigned int dicttable;
extern unsigned int syntable;
extern unsigned int initaddr;
extern unsigned int mainaddr;
extern unsigned int parseaddr;
extern unsigned int parseerroraddr;
extern unsigned int findobjectaddr;
extern unsigned int endgameaddr;
extern unsigned int speaktoaddr;
extern unsigned int performaddr;
extern int objects;
extern int events;
extern int dictcount;
extern int syncount;
#if !defined (COMPILE_V25)
extern char context_command[][64];
extern int context_commands;
#endif
extern unsigned char *mem;
extern int loaded_in_memory;
extern unsigned int defseg;
extern unsigned int gameseg;
extern long codeptr;
extern long codeend;
extern char pbuffer[];
extern int currentpos;
extern int currentline;
extern int full;
extern signed char fcolor, bgcolor, icolor, default_bgcolor;
extern int currentfont;
extern char capital;
extern unsigned int textto;
extern int SCREENWIDTH, SCREENHEIGHT;
extern int physical_windowwidth, physical_windowheight,
	physical_windowtop, physical_windowleft,
	physical_windowbottom, physical_windowright;
extern int inwindow;
extern int charwidth, lineheight, FIXEDCHARWIDTH, FIXEDLINEHEIGHT;
extern int current_text_x, current_text_y;
extern int undostack[][5];
extern int undoptr;
extern int undoturn;
extern char undoinvalid;
extern char undorecord;
#ifdef USE_SMARTFORMATTING
extern int smartformatting;
extern char leftquote;
#endif

/* heobject.c */
int Child(int obj);
int Children(int obj);
int Elder(int obj);
unsigned long GetAttributes(int obj, int attribute_set);
int GetProp(int obj, int p, int n, char s);
int GrandParent(int obj);
void MoveObj(int obj, int p);
char *Name(int obj);
int Parent(int obj);
unsigned int PropAddr(int obj, int p, unsigned int offset);
void PutAttributes(int obj, unsigned long a, int attribute_set);
void SetAttribute(int obj, int attr, int c);
int Sibling(int obj);
int TestAttribute(int obj, int attr, int nattr);
int Youngest(int obj);

extern int display_object;
extern char display_needs_repaint;
extern int display_pointer_x, display_pointer_y;


/* heparse.c */
int Available(int obj, char non_grammar);
void CallLibraryParse(void);
void FindObjProp(int obj);
unsigned int FindWord(char *a);
void KillWord(int a);
int MatchCommand(void);
int ObjWord(int obj, unsigned int w);
int Parse(void);
void ParseError(int e, int a);
void RemoveWord(int a);
void SeparateWords(void);
int ValidObj(int obj);

extern char buffer[];
extern char full_buffer;
extern char errbuf[];
extern char line[];
extern int words; extern char *word[];
extern unsigned int wd[], parsed_number;
extern char parse_called_twice;
extern char punc_string[];
extern signed char remaining;
extern char parseerr[];
extern char parsestr[];
extern char xverb;
extern unsigned int grammaraddr;
extern char *obj_parselist;
extern int domain, odomain;
extern int objlist[];
extern char objcount;
extern char parse_allflag;
extern struct pobject_structure pobjlist[];
extern int pobjcount;
extern int pobj;
extern int obj_match_state;
extern char object_is_number;
extern unsigned int objgrammar;
extern int objstart;
extern int objfinish;
extern char addflag;
extern int speaking;
extern char oops[];
extern int oopscount;


/* heres.c */
void DisplayPicture(void);
long FindResource(char *filename, char *resname);
void PlayMusic(void);
void PlaySample(void);
void PlayVideo(void);

extern HUGO_FILE resource_file;
extern char loaded_filename[];
extern char loaded_resname[];
extern char resource_type;


/* herun.c */
void RunDo(void);
void RunEvents(void);
void RunGame(void);
void RunIf(char override);
void RunInput(void);
void RunMove(void);
void RunPrint(void);
int RunRestart(void);
int RunRestore(void);
void RunRoutine(long addr);
int RunSave(void);
int RunScriptSet(void);
void RunSet(int gotvalue);
int RunString(void);
int RunSystem(void);
void RunWindow(void);

extern char during_player_input;
extern int passlocal[];
extern int arguments_passed;
extern int ret; extern char retflag;
extern char game_reset;
extern struct CODE_BLOCK code_block[];
extern int stack_depth;
extern int tail_recursion;
extern long tail_recursion_addr;
extern int last_window_top, last_window_bottom,
	last_window_left, last_window_right;
extern char just_left_window;


/* heset.c */
extern char game_title[];
extern char arrexpr;
extern char multiprop;


/* stringfn.c */
char *Left(char *a, int l);
char *Ltrim(char *a);
char *Mid(char *a, int pos, int n);
char *Right(char *a, int l);
char *Rtrim(char *a);
#if defined (EXTRA_STRING_FUNCTIONS)
#ifndef UNDER_CE
int atoi(const char * str);
#endif
char *itoa(int a, char *buf, int base);
char *strlwr(char *s);
char *strnset(char *s, int c, size_t l);
char *strupr(char *s);
#endif

#ifdef MINIMAL_WINDOWING
extern unsigned char minimal_windowing, illegal_window;
#endif


/*-------------------------------------------------------------------------*/

#ifndef NO_INLINE_MEM_FUNCTIONS

HUGO_INLINE unsigned char Peek(long a)
	{ return MEM(defseg * 16L + a); }

HUGO_INLINE unsigned int PeekWord(long a)
	{ return (unsigned char)MEM(defseg*16L+a) + (unsigned char)MEM(defseg*16L+a+1)*256; }

HUGO_INLINE void Poke(unsigned int a, unsigned char v)
	{ SETMEM(defseg * 16L + a, v); }

HUGO_INLINE void PokeWord(unsigned int a, unsigned int v)
	{ SETMEM(defseg * 16L + a, (char)(v%256));
	  SETMEM(defseg * 16L + a + 1, (char)(v/256)); }

#endif


/*-------------------------------------------------------------------------*/

/* DEBUGGER should be defined (at the start of this file or in the makefile)
   to build the debugging version of the engine
*/
#if defined (DEBUGGER)
#include "hdinter.h"
#endif

