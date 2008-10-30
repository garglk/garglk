/*
	HCHEADER.H

	contains definitions and prototypes
	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#define HCVERSION 3
#define HCREVISION 1
#if !defined (COMPILE_V25)
#define HCINTERIM ".03"
#else
#define HCINTERIM ".03 (2.5)"
#endif


#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


/* The Hugo token set and their respective values are contained
   separately in htokens.h.
*/
#include "htokens.h"
extern char *token[];


/* DEBUG_FULLOBJ includes code for producing full object summaries. */
#define DEBUG_FULLOBJ

/* LITTLE_VERSION is a test #define used for producing the minimal-size
   compiler.
*/
#if defined (LITTLE_VERSION)
#undef DEBUG_FULLOBJ
#endif


/* Define the compiler being used as one of:

	ACORN
	AMIGA
	DJGPP
	GCC_UNIX (for gcc under Unix, Linux, BeOS)
	QUICKC
	RISCOS
	WIN32 (Microsoft Visual C++)

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

#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME "tmphc01"
#define ALLTEMPNAME  "tmphc00"
#endif

#undef STDPRN_SUPPORTED

/* Trying to fiddle the library files */
FILE *arc_fopen(const char *, const char *);

#endif  /* defined (Acorn C) */


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

#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME "T:tmphc01.tmp"
#define ALLTEMPNAME  "T:tmphc00.tmp"
#endif

#undef STDPRN_SUPPORTED

int brk();

#endif  /* defined (AMIGA) */


/*---------------------------------------------------------------------------
	Definitions for the djgpp build

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (DJGPP)

#include <unistd.h>
#include <dir.h>

#define PORT_NAME "32-bit"
#define PORTER_NAME "Kent Tessman"

#define STDPRN_SUPPORTED

/* MAXPATH, MAXDRIVE, MAXDIR, MAXEXT defined in <dir.h> */
#define MAXFILENAME     MAXFILE

#define EXTRA_STRING_FUNCTIONS
#define STRICMP(a, b)   strcasecmp(a, b)

#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME "~tmphc01.tmp"
#define ALLTEMPNAME  "~tmphc00.tmp"
#endif

#endif  /* defined (DJGPP) */


/*---------------------------------------------------------------------------
	Definitions for the GCC Unix/Linux port (and BeOS)

	Originally by Bill Lash
---------------------------------------------------------------------------*/

#if defined (GCC_UNIX) || defined (GCC_BEOS)

#include <unistd.h>

#ifndef GCC_BEOS
#define PORT_NAME "Unix"
#define PORTER_NAME "Bill Lash"
#else
#define PORT_NAME "BeOS"
#define PORTER_NAME "Kent Tessman"
#endif

#define MAXPATH         256
#define MAXFILENAME     256
#define MAXDRIVE        256
#define MAXDIR          256
#define MAXEXT          256

#define EXTRA_STRING_FUNCTIONS
#define STRICMP(a, b)   strcasecmp(a, b)

#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME "tmphc01.tmp"
#define ALLTEMPNAME  "tmphc00.tmp"
#else
void rmtmp(void);
#endif

#undef STDPRN_SUPPORTED

#endif  /* defined (GCC_UNIX) */


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

#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME	"tmphc01"
#define ALLTEMPNAME	"tmphc00"
#endif

#undef STDPRN_SUPPORTED

#define toascii(c)	((c)&0x7f)
#define STRICMP(s, t)	strcmp(strupr((s)), strupr((t)))
extern FILE *hugo_fopen (char *filename, char *mode);
#define HUGO_FOPEN	hugo_fopen

#endif  /* defined (RISCOS) */


/*---------------------------------------------------------------------------
	MS-DOS specific definitions for Microsoft QuickC,

	by Kent Tessman
---------------------------------------------------------------------------*/

#if defined (QUICKC)

#define PORT_NAME "16-bit"
#define PORTER_NAME "Kent Tessman"

#define MATH_16BIT

#define MAXPATH         _MAX_PATH       /* maximum allowable lengths */
#define MAXFILENAME     _MAX_FNAME
#define MAXDRIVE        _MAX_DRIVE
#define MAXDIR          _MAX_DIR
#define MAXEXT          _MAX_EXT

/* Define USE_TEMPFILES to use system-named tempfiles instead of these: */
#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME "~tmphc01.tmp"
#define ALLTEMPNAME  "~tmphc00.tmp"
#endif

/* The printer output stream stdprn is not ANSI. */
#define STDPRN_SUPPORTED

/* For errors, updates, etc. */
#define PRINTED_FILENAME(a)     strupr(a)

/* Case-insensitive string compare */
#define STRICMP(a, b)           stricmp(a, b)

#define OMIT_EXTRA_STRING_FUNCTIONS

#endif  /* defined (QUICKC) */


/*---------------------------------------------------------------------------
	Definitions for the Microsoft Visual C++ build

	by Kent Tessman

---------------------------------------------------------------------------*/

#if defined (WIN32)
#define PORT_NAME "Win32"
#define PORTER_NAME "Kent Tessman"

#define MAXPATH         _MAX_PATH
#define MAXFILENAME     _MAX_FNAME
#define MAXDRIVE        _MAX_DRIVE
#define MAXDIR          _MAX_DIR
#define MAXEXT          _MAX_EXT

#if !defined (USE_TEMPFILES)
#define TEXTTEMPNAME "~tmphc01.tmp"
#define ALLTEMPNAME  "~tmphc00.tmp"
#endif

#endif	/* Microsoft Visual C++ */


/*-------------------------------------------------------------------------*/

#if !defined (PROGRAM_NAME)
#define PROGRAM_NAME    "hc"
#endif

#if !defined (HUGO_FOPEN)
#define HUGO_FOPEN fopen
#endif

#if !defined (STRICMP)
#define STRICMP(a, b)   stricmp(a, b)
#endif

/* Because, unless defined otherwise (see, for example, the QUICKC section,
   above), a printed filename will simply be the filename itself
*/
#if !defined (PRINTED_FILENAME)
#define PRINTED_FILENAME(a)     a
#endif

#ifndef OMIT_EXTRA_STRING_FUNCTIONS
#define EXTRA_STRING_FUNCTIONS
#endif

/* To be used with caution; obviously, not all non-zero values are "true"
   in this usage.
*/
#define true 1
#define false 0

/* This is a macro for first fast-checking a hash match, then confirming
   a subsequent string match if necessary--the strcmp() is only performed
   if the hash values match.  See FindHash() for elaboration.
*/
#define HashMatch(hash, hash_compare, string, string_compare) \
	((hash==hash_compare) && !strcmp(string, string_compare))

/* The header takes up 64 bytes at the start of the file. */
#define HEADER_LENGTH 64L

/* CHAR_TRANSLATION is simply a value that is added to an ASCII character
   in order to encode the text, i.e., make it unreadable to casual
   browsing.
*/
#define CHAR_TRANSLATION  0x14

/* FAILED is a fairly frequently used return value. */
#define FAILED ((char)-1)

/* These are bitmasks for the propadd array, indicating special qualities
   of properties.
*/
#define ADDITIVE_FLAG   1
#define COMPLEX_FLAG    2

/* Property-table indicators */
#define PROP_END          255
#define PROP_ROUTINE      255
#define PROP_LINK_ROUTINE 254

/* For addresses and properties, the compiler will allocate a maximum
   of MAXBLOCKS of BLOCKSIZE each, one block at a time.
*/
#define ADDRBLOCKSIZE 256
#define PROPBLOCKSIZE 256
#define MAXADDRBLOCKS 65536*2/ADDRBLOCKSIZE
#define MAXPROPBLOCKS 65536*2/PROPBLOCKSIZE

/* Maximums for reading lines from a sourcefile */
#define MAXWORDS         255
#define MAXBUFFER       1024

/* These static values are not changeable--they depend largely on internals
   of both the Compiler and the Engine.
*/
#define MAXATTRIBUTES    128
#define MAXGLOBALS       240
#define MAXLOCALS         16

/* These, however, are variables in order to allow dynamic resizing of
   various arrays.  INIT_PASS is defined in he.c in order to initialize
   variables in this header file.
*/
#if defined (INIT_PASS)
int MAXOBJECTS        = 1024;
int MAXPROPERTIES     = 254;
int MAXALIASES        = 256;
int MAXEVENTS         = 256;
int MAXROUTINES       = 320;
int MAXLABELS         = 256;
int MAXCONSTANTS      = 256;
int MAXARRAYS         = 256;
int MAXDICT           = 1024;
int MAXDICTEXTEND     = 0;
int MAXFLAGS          = 256;
int MAXSPECIALWORDS   = 64;
int MAXDIRECTORIES    = 16;

int ENGINE_GLOBALS;
int ENGINE_PROPERTIES;

#else
extern int MAXOBJECTS;
extern int MAXPROPERTIES;
extern int MAXALIASES;
extern int MAXEVENTS;
extern int MAXROUTINES;
extern int MAXLABELS;
extern int MAXCONSTANTS;
extern int MAXARRAYS;
extern int MAXDICT;
extern int MAXDICTEXTEND;
extern int MAXFLAGS;
extern int MAXSPECIALWORDS;
extern int MAXDIRECTORIES;
extern int ENGINE_GLOBALS;
extern int ENGINE_PROPERTIES;
#endif

#define MAX_SYMBOL_LENGTH 32


enum ERROR_TYPE                 /* fatal errors */
{
	MEMORY_E = 1,           /* out of memory                */
	OPEN_E,                 /* error opening file           */
	READ_E,                 /* error reading from file      */
	WRITE_E,                /* error writing to file        */
	OVERFLOW_E,             /* overflow (usually buffer[])  */
	EOF_COMMENT_E,          /* unexpected end-of-file in    */
	EOF_TEXT_E,             /*   comment, text, or          */
	EOF_ENDIF_E,            /*   #if, #elseif, #else        */
	CLOSE_BRACE_E,          /* missing closing brace        */
	COMP_LINK_LIMIT_E       /* compiler limit exceeded      */
};

/* The structure of a synonym, removal, or compound */
struct synstruct
{
	char syntype;                   /* i.e., synonym, removal, compound */
	unsigned int syn1;              /* first dictionary word */
	unsigned int syn2;              /* second word (if needed), or 0 */
};

/* The structure of an address, used in final resolving */
struct addrstruct
{
	long addrptr;                   /* i.e. codeptr */
	char addrtype;                  /* label, routine, etc. */
	unsigned int addrval;           /* the number of the label, etc.
					   (or address for .HLB refiguring) */
};


/*-------------------------------------------------------------------------*/


void hugo_closefiles(void);
void hugo_makepath(char *path, char *drive, char *dir, char *fname, char *ext);
void hugo_splitpath(char *path, char *drive, char *dir, char *fname, char *ext);


/* hc.c */
extern int address_scale;
extern time_t tick;
extern int exitvalue;


/* hcbuild.c */
void BuildCode(unsigned char from);
void BuildEvent(void);
void BuildObject(int from);
void BuildProperty(int obj, int p);
void BuildRoutine(void);
void BuildVerb(void);
void Inherit(int i);
void ReplaceRoutine(void);

extern char infileblock;
extern char incompprop;
extern int *parent, *sibling, *child;
extern long *objattr[];
extern unsigned int *objpropaddr;
extern long attr[], notattr[];
extern char *oreplace;


/* hccode.c */
void BeginCodeBlock(void);
void CodeDo(void);
void CodeFileIO(void);
void CodeFor(void);
void CodeIf(void);
void CodeLine(void);
void CodeSelect(void);
void CodeWhile(void);
void CodeWindow(void);
int IDWord(int a);

extern long token_val;
extern int nest;
extern int arguments;
extern char hex[];


/* hccomp.c */
void CompilerDirective(void);
void CompilerMem(void);
void CompilerSet(void);
long ReadCode(int n);


/* hcdef.c */
void CheckSymbol(char *a);
void DefArray(void);
void DefAttribute(void);
void DefCompound(void);
void DefConstant(void);
void DefEnum(void);
void DefGlobal(void);
void DefLocals(int asargs);
void DefOther(void);
void DefProperty(void);
void DefRemovals(void);
void DefSynonym(void);
int FindHash(char *a);

extern char object_id[];
extern char **object; extern int *object_hash; extern int objectctr;
extern unsigned int *oprop;
extern char *attribute[]; extern int attribute_hash[]; extern int attrctr;
extern char **property; extern int *property_hash; extern int propctr;
extern char *propset;
extern char *propadd;
extern unsigned int proptable, *propdef;
extern unsigned int *propdata[]; extern unsigned int propheap;
extern char **label; extern int *label_hash; extern int labelctr;
extern unsigned int *laddr;
extern char **routine; extern int *routine_hash; extern int routinectr;
extern unsigned int *raddr;
extern char *rreplace;
extern unsigned int addrctr;
extern struct addrstruct *addrdata[];
extern unsigned int *eventin, *eventaddr;
extern int eventctr;
extern char **alias; extern int *alias_hash; extern int *aliasof;
extern int aliasctr;
extern char *global[], local[][33];
extern int global_hash[], local_hash[];
extern int globalctr, localctr;
extern char unused[];
extern unsigned int globaldef[];
extern char **constant; extern int *constant_hash;
extern unsigned int *constantval; extern int constctr;
extern char **array; extern int *array_hash;
extern unsigned int *arrayaddr, *arraylen;
extern unsigned int arraysize; extern int arrayctr;


/* hcfile.c */
void CleanUpFiles(void);
void GetLine(int offset);
void GetWords(void);
void OpenFiles(void);
unsigned int ReadWord(FILE *workfile);
FILE *TrytoOpen(char *f, char *p, char *d);
void WriteCode(unsigned int a, int b);
long WriteText(char *a);
void WriteWord(unsigned int val, FILE *workfile);

extern long codeptr;
extern long textptr;
extern char buffer[];
extern int totalfiles;
extern FILE *sourcefile; extern char sourcefilename[];
extern FILE *objectfile; extern char objectfilename[];
extern FILE *textfile; extern char textfilename[];
extern FILE *allfile; extern char allfilename[];
extern FILE *listfile; extern char listfilename[];
extern FILE *linkfile; extern char linkfilename[];


/* hclink.c */
void LinkerPass1(void);
void LinkerPass2(void);

extern char linked;


/* hcmisc.c */
unsigned int AddDictionary(char *a);
void AddDirectory(char *d);
void Boundary(void);
void ClearLocals(void);
void DoBrace(void);
void DrawTree(void);
int Expect(int a, char *t, char *d);
void Error(char *a);
void FatalError(int n, char *a);
void FillCode(void);
void KillWord(int a);
void ListLimits(void);
char *MakeString(char *a);
void ParseCommand(int argc, char *argv[]);
char *PrintHex(long a, char b);
void PrintLine(int n, char a);
void Printout(char *a);
void PrintStatistics(void);
void PrinttoAll(void);
void PrintWords(int n);
void PutinTree(int obj, int p);
void RememberAddr(long codeptr, int b, unsigned int z);
void RemoveCommas(void);
void ResolveAddr(void);
void SavePropData(unsigned int n);
void SeparateWords(void);
void SetAttribute(int a);
void SetLimit(char *a);
void SetMem(void);
void SetSwitches(char *a);
void StripQuotes(char *a);

extern int argc;
extern char **argv, **envp;
extern char errfile[];
extern unsigned int errline;
extern int words;
extern char *word[];
extern char line[];
extern char full_buffer;
extern char listing, objecttree, fullobj, printer, statistics, printdebug,
	override, aborterror, memmap, hlb, builddebug, expandederr,
	spellcheck, writeanyway;
extern char compile_v25;
extern int percent, totallines, tlines;
extern int er;
extern int warn;
extern char **sets;
extern char **directory; extern int directoryctr;
extern char alloc_objects, alloc_properties, alloc_labels, alloc_routines,
	alloc_events, alloc_aliases, alloc_constants, alloc_arrays,
	alloc_sets, alloc_dict, alloc_syn, alloc_directories;
extern unsigned int dicttable;
extern unsigned int lexstart[], lexlast[];
extern char **lexentry;
extern unsigned int *lexnext;
extern unsigned int *lexaddr;
extern int verbs; extern int dictcount, syncount;
extern struct synstruct *syndata;


/* hcpass.c */
void Pass1(void);
void Pass2(void);
void Pass3(void);

extern unsigned int codestart;
extern char endofgrammar;
extern int passnumber;
extern int objinitial, routineinitial, eventinitial, labelinitial,
	propinitial;
extern int objects, routines, events, labels;
extern unsigned int initaddr;
extern unsigned int mainaddr;
extern unsigned int parseaddr;
extern unsigned int parseerroraddr;
extern unsigned int findobjectaddr;
extern unsigned int endgameaddr;
extern unsigned int speaktoaddr;
extern unsigned int performaddr;
extern unsigned int linkproptable;
extern unsigned int textcount;


/* hcres.c */
void CreateResourceFile(void);


/* stringfn.c */
char *Left(char *a, int l);
char *Ltrim(char *a);
char *Mid(char *a, int pos, int n);
char *Right(char *a, int l);
char *Rtrim(char *a);
#if defined (EXTRA_STRING_FUNCTIONS)
char *itoa(int a, char *buf, int base);
char *strlwr(char *s);
char *strnset(char *s, int c, size_t l);
char *strupr(char *s);
#endif
