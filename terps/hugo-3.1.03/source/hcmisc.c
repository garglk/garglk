/*
	HCMISC.C

	contains miscellaneous compiler routines:

	AddDictionary           ListLimits              RememberAddr
	AddDirectory            MakeString              RemoveCommas
	Boundary                ParseCommand            ResolveAddr
	ClearLocals             PrintErrorLocation      SavePropData
	DoBrace                 PrintHex                SeparateWords
	DrawBranch              PrintLimit              SetAttribute
	DrawTree                PrintLine               SetLimit
	Error                   Printout                SetMem
	Expect                  PrintStatistics         SetSwitches
	FatalError              PrinttoAll              StripQuotes
	FillCode                PrintWords
	KillWord                PutinTree

	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


/* Function prototypes: */
void DrawBranch(int obj);
void PrintErrorLocation(void);
void PrintLimit(char *a, unsigned int n, int nl);

int argc;                       /* parameters passed from command line */
char **argv, **envp;

char errfile[MAXPATH];          /* for locating errors in Pass2    */
unsigned int errline;
int words;			/* number of words in input line   */
char *word[MAXWORDS+1];         /* the words themselves            */
char line[MAXBUFFER+1];            /* output line                     */
char full_buffer;		/* true if word[] isn't empty	   */

/* Command line switch flags: */
char    listing = 0,                    /* output to .LST file          */
	objecttree = 0,                 /* print object tree when done  */
	fullobj = 0,                    /* output full object summaries */
	printer = 0,                    /* output to standard printer   */
	statistics = 0,                 /* print compilation statistics */
	printdebug = 0,                 /* print debuggine information  */
	override = 0,                   /* override switches in source  */
	aborterror = 0,                 /* abort on first error         */
	memmap = 0,                     /* display map of memory usage  */
	hlb = 0,                        /* generate .HLB linkable file  */
	builddebug = 0,                 /* build .HDX debuggable file   */
	expandederr = 0,		/* issue generic-format errors  */
	spellcheck = 0,			/* print any text to .LST	*/
	writeanyway = 0;		/* write .HEX even with errors  */
#if !defined (COMPILE_V25)
	char compile_v25 = 0;
#else
	char compile_v25 = 1;
#endif

int percent = 0, totallines = 0, tlines = 0;  /* percentage completion */

int er = 0;                     /* error counter */
int warn = 0;                   /* warning counter */

/* Compiler flag setting names (strings) */
char **sets;

/* Directory settings */
char **directory; int directoryctr = 0;

/* Flags set to true once allocated */
char alloc_objects = 0, alloc_properties = 0, alloc_labels = 0,
	alloc_routines = 0, alloc_events = 0, alloc_aliases = 0,
	alloc_constants = 0, alloc_arrays = 0, alloc_sets = 0,
	alloc_dict = 0, alloc_syn = 0, alloc_directories = 0;

/* For dictionary searching:  The first and last entry beginning with
   each letter (or non-alphabetic character), the complete array of
   dictionary words, and arrays for linking the list.
*/

/* Size of dictionary table */
unsigned int dicttable = 0;

unsigned int lexstart[27], lexlast[27];
char **lexentry;
unsigned int *lexnext;
unsigned int *lexaddr;

/* Total number of verbs, dictionary words, synonyms, plus the array of
   synonym/compound/removal structures
*/
int verbs = 0; int dictcount = 0, syncount = 0;
struct synstruct *syndata = NULL;


/* ADDDICTIONARY

	A (hopefully) quick way of searching and sorting the existing
	dictionary entries before adding the new one <a>.  There are
	27 "chains" or before- and after-linked lists (26 letters, and
	1 for non-alphabetical words).  AddDictionary() quickly searches
	through each list to see if the entry exists.  If it does, the
	function returns the established dictionary address.  If not, it
	adds it and returns the new address (i.e., the next sequentially
	added dictionary entry).
*/

unsigned int AddDictionary(char *a)
{
	char *b;
	int letter, n;

	if (a[0]=='\0') return 0;

	letter = (char)(toupper(a[0]) - 64);
	if (letter < 1 || letter > 26)  /* if not A-Z */
		letter = 0;

	if (strlen(a) > 255)
		{word[1] = a, words = 1;
		Error("Dictionary entry too long");
		return 0;}

	b = a;

	if (lexstart[letter])
	{
		for (n=lexstart[letter]; n; n=lexnext[n])
		{
			if (lexentry[n][1]==b[1])
				if (!strcmp((char *)lexentry[n], b))
					return lexaddr[n];
		}
	}
	else
		lexstart[letter] = dictcount;


	/* No match, or no entries with letter */

	if (dictcount >= MAXDICT)
		{sprintf(line, "Maximum number of %d dictionary entries exceeded", MAXDICT);
		Error(line);
		return 0;}

	if ((lexentry[dictcount] = malloc((strlen(a)+1)*sizeof(char)))==NULL)
		FatalError(MEMORY_E, "");
	strcpy(lexentry[dictcount], b);
	lexnext[dictcount] = 0;
	lexnext[lexlast[letter]] = dictcount;
	lexaddr[dictcount] = dicttable;
	lexlast[letter] = dictcount;

	dicttable += strlen(a) + 1;
	dictcount++;

	if (spellcheck) Printout(a);

	return lexaddr[dictcount-1];
}


/* ADDDIRECTORY

	Registers the directory <path> as a searchable location for
	files of type HUGO_<directory type>.  Directories are passed
	without the leading '@' in the command line or source.
*/

void AddDirectory(char *d)
{
	if (!STRICMP(d, "@list"))
	{
		PrintLine(68, '-');
		Printout("VALID DIRECTORY NAMES:\n");
		Printout("source   Source files");
		Printout("object   Where the new .HEX file will reside");
		Printout("lib      Both #included and #linked library files");
		Printout("list     The .LST file, if specified");
		Printout("resource Resources for a \"resource\" file list");
		Printout("temp     Any compile-time temporary files");
		Printout("\nSpecify a directory as:  @<directory name>=<actual directory>");
		Printout("\nDirectories may also be preset with environment variables, with the");
		Printout("prefix \"HUGO_<directory name>\", e.g. HUGO_SOURCE=<actual directory>.");
		PrintLine(68, '-');
		return;
	}

	if (directoryctr==MAXDIRECTORIES)
	{
		sprintf(line, "Maximum of %d directory settings exceeded", MAXDIRECTORIES);
		Error(line);
		return;
	}

	directory[directoryctr++] = MakeString(d);
}


/* BOUNDARY

	Fills null characters (i.e., 0) to the start of the next address
	boundary--an address evenly divisible by address_scale.
*/

void Boundary(void)
{
	while (codeptr % address_scale)
		WriteCode(0, 1);
}


/* CLEARLOCALS

	Must be called before building each new routine in order to
	clear local variable names (except for any arguments for the
	new routine).
*/

void ClearLocals(void)
{
	int i;

	for (i=arguments; i<MAXLOCALS; i++)
	{
		strcpy(local[i], "");
		unused[i] = 0;
	}
	localctr = arguments;
}


/* DOBRACE

	Passes over the expected opening brace for a new code block.
*/

void DoBrace(void)
{
	Expect(1, "{", "leading brace");

	if (words==1)
		GetWords();
	else
		KillWord(1);
}


/* DRAWBRANCH

	For printing the object tree.
*/

void DrawBranch(int obj)
{
	int i;

	if (nest)
	{
		strcpy(line, "");
		for (i=1; i<nest; i++)
			strcat(line, ". . ");
		strcat(line, "\\;");
		Printout(line);
	}
	sprintf(line, "[%d] %s", obj, object[obj]);
	Printout(line);

	for (i=0; i<objects; i++)
	{
		if (parent[i]==obj && i != obj)
			{nest++;
			DrawBranch(i);
			nest--;}
	}
}


/* DRAWTREE */

void DrawTree(void)
{
	if (objects)
	{
		Printout("OBJECT TREE:");
		PrintLine(12, '=');
		nest = 0;
		DrawBranch(0);
	}
}


/* ERROR

	For non-fatal errors.  Prints the offending line of (reconstructed)
	code followed by the error message.  Warnings begin with a '?'.
*/

void Error(char *a)
{
	char iswarning = 0;
	char errbuf[256];

	if (percent && passnumber==1)
	{
		printf("\rCompiling %5d lines of %s", totallines, PRINTED_FILENAME(sourcefilename));
		if (listing)
			if (fprintf(listfile, "Compiling %5d lines of %s\n", totallines, PRINTED_FILENAME(sourcefilename)) < 0)
				FatalError(WRITE_E, listfilename);

#ifdef STDPRN_SUPPORTED
		if (printer)
			if (fprintf(stdprn, "Compiling %5d lines of %s\n\r", totallines, PRINTED_FILENAME(sourcefilename)) < 0)
				FatalError(WRITE_E, "printer");
#endif
	}

	if (percent && passnumber) Printout("");
	if (expandederr) Printout("");

	PrintErrorLocation();

	if (passnumber<3 && expandederr)
	{
		if ((words>=2) && word[1][0]=='#' && !strcmp(word[2], "message"))
			goto SkipPrintingWords;

		PrintWords(1);
	}

SkipPrintingWords:

	if (a[0]!='?')
	{
		sprintf(errbuf, "%s:  ", (expandederr)?"ERROR":"Error");
		strncpy(errbuf+strlen(errbuf), a, 256-strlen(errbuf));
		er++;
	}
	else
	{
		if (expandederr)
			strcpy(errbuf, "WARNING:  ");
		else
			strcpy(errbuf, "Warning:  ");
		strncpy(errbuf+strlen(errbuf), a+1, 256-strlen(errbuf));
		warn++;
		iswarning = true;
	}
	Printout(errbuf);

	/* Don't continue with the line-count/percentage-completion after
	   an error--it gets messy.
	*/
	percent = false;

	if ((aborterror && !iswarning) || er>50)
	{
		CleanUpFiles();
		exit FAILED;
	}
	else if (exitvalue==0 && !iswarning)
		exitvalue = FAILED;
}


/* FATALERROR

	For fatal errors.
*/

void FatalError(int n, char *a)
{
	int flag = 0;

	if (percent && passnumber==1)
	{
		printf("\rCompiling %5d lines of %s", totallines, PRINTED_FILENAME(sourcefilename));
		if (listing==true)
			fprintf(listfile, "Compiling %5d lines of %s\n", totallines, PRINTED_FILENAME(sourcefilename));

#if defined (STDPRN_SUPPORTED)
		if (printer)
			fprintf(stdprn, "Compiling %5d lines of %s\n\r", totallines, PRINTED_FILENAME(sourcefilename));
#endif
	}

	/* Make sure if it's a temporary file we're reading/writing that
	   it gets reported as "work file" instead of some temp file name
	*/
	if (!strcmp(a, objectfilename) || !strcmp(a, listfilename) ||
		!strcmp(a, linkfilename) ||
		(!strcmp(a, sourcefilename) && passnumber<=1))
	{
		flag = 1;
	}
	else
		strcpy(a, "work file");

	if (passnumber && percent) Printout("");
	/* if (expandederr) */ Printout("");

	PrintErrorLocation();

	Printout("Fatal error:  \\;");

	switch (n)
	{
		case MEMORY_E:
			{Printout("Unable to allocate memory");
			break;}

		case OPEN_E:
			{sprintf(line, "Unable to open %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}

		case READ_E:
			{sprintf(line, "Cannot read from %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}

		case WRITE_E:
			{sprintf(line, "Cannot write to %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}

		case OVERFLOW_E:
			{sprintf(line, "Line overflow in %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}

		case EOF_COMMENT_E:
			{sprintf(line, "Unexpected end-of-file in comment in %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}

		case EOF_TEXT_E:
			{sprintf(line, "Unexpected end-of-file in text in %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}
			
		case EOF_ENDIF_E:
			{sprintf(line, "Unexpected end-of-file before #endif in %s", PRINTED_FILENAME(a));
			Printout(line);
			break;}

		case COMP_LINK_LIMIT_E:
			{Printout(line);
			break;}
	}
	CleanUpFiles();
	exit(n);
}


/* EXPECT

	Prints an error if word[a] is not the expected token <d>.
*/

int Expect(int a, char *t, char *d)
{
	if (!strcmp(t, "NULL"))
		{t = "";
		if (word[a][0]=='\0') return true;}

	if (word[a][0]!=t[0])
	{
		sprintf(line, "Expecting %s", d);
		if (t[0]!='\0')
			sprintf(line+strlen(line), ":  '%s'", t);
		Error(line);
		return false;
	}
	return true;
}


/* FILLCODE

	Fills null characters (i.e., 0) to the start of the next segment
	boundary--an address evenly divisible by 16.
*/

void FillCode(void)
{
	while (codeptr % 16)
		WriteCode(0, 1);
}


/* KILLWORD

	Could have been DeleteWord(), but that would've been less flashy.
*/

void KillWord(int i)
{
	if (words==0)
		{word[1] = "";
		return;}
	else if (i>words) return;

	for (; i<words; i++)
		word[i] = word[i+1];
	word[i] = "";
	words--;
}


/* LISTLIMITS */

void ListLimits(void)
{
	PrintLine(63, '-');
	Printout("Static limits (non-modifiable):");
	PrintLimit("MAXATTRIBUTES", MAXATTRIBUTES, 1);
	PrintLimit("MAXGLOBALS", MAXGLOBALS, 0);
	PrintLimit("MAXLOCALS", MAXLOCALS, 0);

	PrintLine(63, '-');
	Printout("Default limits:");
	PrintLimit("MAXALIASES", MAXALIASES, 1);
	PrintLimit("MAXARRAYS", MAXARRAYS, 0);
	PrintLimit("MAXCONSTANTS", MAXCONSTANTS, 1);
	PrintLimit("MAXDICT", MAXDICT, 0);
	PrintLimit("MAXDICTEXTEND", MAXDICTEXTEND, 1);
	PrintLimit("MAXDIRECTORIES", MAXDIRECTORIES, 0);
	PrintLimit("MAXEVENTS", MAXEVENTS, 1);
	PrintLimit("MAXFLAGS", MAXFLAGS, 0);
	PrintLimit("MAXLABELS", MAXLABELS, 1);
	PrintLimit("MAXOBJECTS", MAXOBJECTS, 0);
	PrintLimit("MAXPROPERTIES", MAXPROPERTIES, 1);
	PrintLimit("MAXROUTINES", MAXROUTINES, 0);
	PrintLimit("MAXSPECIALWORDS", MAXSPECIALWORDS, 0);

	Printout("\nModify non-static default limits using:  $<setting>=<new limit>");
	PrintLine(63, '-');
}


/* MAKESTRING

	Dynamically (and permanently) allocates space for a string.
*/

char *MakeString(char *a)
{
	char *s;

	if (!(s = malloc((strlen(a)+1)*sizeof(char)))) FatalError(MEMORY_E, "");
	strcpy(s, a);

	return s;
}


/* PARSECOMMAND */

void ParseCommand(int argc, char *argv[])
{
	char source[MAXFILENAME], drive[MAXDRIVE], dir[MAXDIR], ext[MAXEXT];
	int i, arg1 = 1;

	strcpy(sourcefilename, "");

	if (argc==1)
	{
		sprintf(line, "\nSYNTAX:   %s [options] sourcefile [objectfile[.HEX]]", argv[0]); //, PROGRAM_NAME);
		Printout(line);
		Printout("OPTIONS:  -<switches>  $<limit setting>  #<compiler flag>  @<directory>");
		Printout("\nSWITCHES:");
		Printout("  -a  Abort on first error                -d  build .HDX Debuggable executable");
#if defined (DEBUG_FULLOBJ)
		Printout("  -f  Full object summaries               \\;");
#endif
		Printout("-h  generate .HLB precompiled Header");
#if !defined (LITTLE_VERSION)
		Printout("  -i  print debugging Information         \\;");
#endif
		Printout("-l  List compilation output to disk");
		Printout("  -o  display Object tree                 \\;");
#if defined (STDPRN_SUPPORTED)
		Printout("-p  Print output to standard printer");
#else
		Printout("");
#endif
		Printout("  -s  display compilation Statistics      -t  Text to .LST for spellchecking");
		Printout("  -u  show memory Usage for objectfile    -v  Verbose compilation");
		Printout("  -w  Write objectfile despite errors     -x  override switches in source code");
/*
#if !defined (COMPILE_V25)
		Printout("  -25 compile with v2.5 compatibility");
#endif
*/
		Printout("\nLIMIT SETTINGS:  $<setting>=<new limit> (or $list to view)");
		Printout("DIRECTORIES:     @<directory name>=<actual directory> (or @list to view)");
		exit(0);
	}

	while (argv[arg1]!=NULL && ((argv[arg1][0]=='-' || argv[arg1][0]=='$' || argv[arg1][0]=='#' || argv[arg1][0]=='@') && arg1<argc))
	{
		if (argv[arg1][0]=='-')
		{
			SetSwitches(argv[arg1]);
			arg1++;
		}

		/* Limit settings */
		else if (argv[arg1][0]=='$')
		{
			if (!STRICMP(argv[arg1], "$list"))
			{
				ListLimits();
				exit(0);
			}
			if (!linked) linked = -1;
			SetLimit(argv[arg1]);
			arg1++;
		}

		/* Compiler flags */
		else if (argv[arg1][0]=='#')
		{
			if (!alloc_sets)
			{
				if ((sets = malloc(MAXFLAGS*sizeof(char *)))==NULL)
					FatalError(MEMORY_E, "");
				for (i=0; i<MAXFLAGS; i++)
					sets[i] = MakeString("");
/* Because sets[] gets initialized both here and in SetMem(),
   the following has to be done both places, too: */
#if defined (COMPILE_V25)
				sets[0] = MakeString("_version2");
#else
				sets[0] = MakeString("_version3");
#endif
				alloc_sets = true;
			}
			word[1] = "#set";
			word[2] = argv[arg1]+1;
			words = 2;
			CompilerSet();
			arg1++;
		}

		/* Directories */
		else if (argv[arg1][0]=='@')
		{
			if (!alloc_directories)
			{
				if ((directory = malloc(MAXDIRECTORIES*sizeof(char *)))==NULL)
					FatalError(MEMORY_E, "");
				alloc_directories = true;
			}
			AddDirectory(argv[arg1++]);
			if (!STRICMP(argv[arg1-1], "@list"))
				exit(0);
		}
	}

	if (argc<=arg1)
	{
		Printout("\nFatal Error:  No sourcefile specified");
		exit(-1);
	}

	hugo_splitpath(argv[arg1], drive, dir, source, ext);
	if (!strcmp(ext, "")) strcpy(ext, "hug");
	hugo_makepath(sourcefilename, drive, dir, source, ext);
	
	arg1++;

	if (argc > arg1)
		{strcpy(objectfilename, argv[arg1]);
		hugo_splitpath(argv[arg1++], drive, dir, source, ext);}
	else
	{
		char testsource[MAXPATH];
		
		if (hlb)
			hugo_makepath(objectfilename, "", "", source, "hlb");
		else if (builddebug)
			hugo_makepath(objectfilename, "", "", source, "hdx");
		else
			hugo_makepath(objectfilename, "", "", source, "hex");
			
		hugo_makepath(testsource, "", "", source, ext);
		if (!strcmp(testsource, objectfilename))
		{
			Printout("\nFatal Error:  Illegal filename(s)");
			exit(-1);
		}
	}

	hugo_makepath(listfilename, "", "", source, "lst");

	if (argc > arg1)
		{Printout("\nFatal Error:  Too many parameters");
		exit(-1);}
}


/* PRINTERRORLOCATION

	Called by Error() or FatalError().
*/

void PrintErrorLocation(void)
{
	char errbuf[256];

	/* errline is recorded in the allfile for reading during
	   Pass2...
	*/
	if (passnumber==2 && errline)
	{
		if (expandederr)
		{
			sprintf(errbuf, "%s:  Line %u, in %s", PRINTED_FILENAME(errfile), errline, object_id);
			Printout(errbuf);
		}
		else
		{
			sprintf(errbuf, "%s:%u:  %s:  \\;", errfile, errline, object_id);
			Printout(errbuf);
		}
	}

	/* ...before which, we can use totallines and the name of
	   the current source file.
	*/
	else if (passnumber==1 && totallines)
	{
		if (expandederr)
		{
			sprintf(errbuf, "%s:  Line %u", PRINTED_FILENAME(sourcefilename), totallines);
			Printout(errbuf);
		}
		else
		{
			sprintf(errbuf, "%s:%u:  \\;", sourcefilename, totallines);
			Printout(errbuf);
		}
	}
}


/* PRINTHEX

	Returns <a> as a hex-number string, in XX, XXXX, or XXXXXX format,
	depending on whether <b> is 1, 2, or 3.
*/

char *PrintHex(long a, char b)
{
	char hbuf[7] = "";
	int h = 0;

	strcpy(hex, "");

	if (a<0L) a = 0;

	if (b==3)
	{
		hex[h++] = '0';
		if (a<65536L) hex[h++] = '0';
		else
		{
			sprintf(hex+h, "%1X", (unsigned int)(a/65536L));
			a = a%65536L;
			h++;
		}
	}

	if (b>=2)
	{
		if (a < 4096L) hex[h++] = '0';
		if (a < 256L) hex[h++] = '0';
	}
	if (a < 16L) hex[h++] = '0';

	hex[h] = '\0';
	sprintf(hbuf, "%X", (unsigned int)a);
	strcat(hex+h, hbuf);

	return hex;
}


/* PRINTLIMIT

	Called by ListLimits().
*/

void PrintLimit(char *a, unsigned int n, int nl)
{
	if (n) sprintf(line, "\t%-15s %5u\\;", a, n);
	else sprintf(line, "\t%-15s   (0)\\;", a);
	Printout(line);
	if (!nl) Printout("");
}


/* PRINTLINE

	Prints a line of <n> repetitions of character <a>.
*/

void PrintLine(int n, char a)
{
	char l[81];
	int i;

	for (i=0; i<n; i++)
		l[i] = a;
	l[i] = '\0';
	Printout(l);
}


/* PRINTOUT

	The replacement for simply printf(), since output may also need to
	be piped to the printer and/or the list file.
*/

void Printout(char *p_original)
{
	char p[MAXBUFFER];
	int l, sticky = 0;

	strcpy(p, p_original);	/* to prevent access violations when
				   modifying static strings */

	if ((l = strlen(p)) >= 2)
	{
		if (p[l-1]==';' && p[l-2]=='\\')
			{p[l-2] = '\0';
			sticky = true;}
	}

	printf("%s", p);

#ifdef STDPRN_SUPPORTED
	if (printer)
		if (fprintf(stdprn, "%s", p) < 0) FatalError(WRITE_E, "printer");
#endif

	if (listing==true)
		if (fprintf(listfile, "%s", p) < 0) FatalError(WRITE_E, listfilename);

	if (!sticky)
		{printf("\n");

#ifdef STDPRN_SUPPORTED
		if (printer)
			if (fprintf(stdprn, "\n\r") < 0) FatalError(WRITE_E, "printer");
#endif

		if (listing==true)
			if (fprintf(listfile, "\n") < 0) FatalError(WRITE_E, listfilename);}
	else
		p[l-2] = '\\';
}


/* PRINTSTATISTICS */

void PrintStatistics(void)
{
	char atype[24];
	time_t tick2;
	int i;
	int obj, p, d;
	unsigned int a, n;
	int dflag = 0;

	time(&tick2);
	tick = tick2 - tick;

	if (objecttree)
		{Printout("");
		DrawTree();}

#if !defined (LITTLE_VERSION)
	if (printdebug)
	{
		Printout("\nDEBUGGING INFORMATION:");

		if (attrctr) Printout("\n  ATTRIBUTE NUMBERS:");

		for (i=0; i<attrctr; i++)
			{sprintf(line, "     $%2s (%2d):  %s", PrintHex((long)i, 1), i, attribute[i]);
			Printout(line);}
		for (i=0; i<aliasctr; i++)
		{
			if (aliasof[i] < MAXATTRIBUTES)
				{sprintf(line, "\t\t%s ALIAS OF %s", alias[i], attribute[aliasof[i]]);
				Printout(line);}
		}

		Printout("\n  PROPERTY NUMBERS:");
		for (i=0; i<propctr; i++)
		{
			strcpy(atype, "");
			if (propadd[i]&ADDITIVE_FLAG) strcat(atype, "$ADDITIVE ");
			if (propadd[i]&COMPLEX_FLAG) strcat(atype, "$COMPLEX ");

			if (propdef[i])
				sprintf(line, "    $%2s (%3d):  %s %s (DEFAULT VALUE:  %d)", PrintHex((long)i, 1), i, property[i], atype, (short)propdef[i]);
			else
				sprintf(line, "    $%2s (%3d):  %s %s", PrintHex((long)i, 1), i, property[i], atype);
			Printout(line);
		}
		for (i=0; i<aliasctr; i++)
		{
			if (aliasof[i]>=MAXATTRIBUTES)
				{sprintf(line, "\t\t%s ALIAS OF %s", alias[i], property[aliasof[i]-MAXATTRIBUTES]);
				Printout(line);}
		}

		Printout("\n  GLOBAL VARIABLE NUMBERS:");
		for (i=0; i<globalctr; i++)
			{sprintf(line, "    $%2s (%3d):  %s", PrintHex((long)i, 1), i, global[i]);
			Printout(line);}

		if (arrayctr) Printout("\n  ARRAY ADDRESSES:");
		for (i=0; i<arrayctr; i++)
			{sprintf(line, "    $%4s:  %s", PrintHex((long)arrayaddr[i], 2), array[i]);
			Printout(line);}

		Printout("\n  PROPERTY ROUTINE ADDRESSES:");
		obj = 0;

		a = 0;

		while (a < propheap)
		{
			/* Property number */
			p = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
			a++;

			if (p != PROP_END)
			{
				/* # of data words */
				d = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
				a++;

				if (d==PROP_ROUTINE || d==PROP_LINK_ROUTINE)
					{d=1; dflag=1;}

				for (i=1; i<=d; i++)
				{
					/* Property data */
					n = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
					a++;
					if (dflag==1)
						{sprintf(line, "    $%6s:  %s.%s",
							PrintHex((long)n*address_scale, 3), object[obj], property[p]);
						Printout(line);
						dflag = 0;}
				}
			}
			else obj++;
		}

		Printout("\n  ROUTINE ADDRESSES:");
		for (i=0; i<routines; i++)
			{sprintf(line, "    $%6s:  %s", PrintHex((long)raddr[i]*address_scale, 3), routine[i]);
			Printout(line);}

		if (events) Printout("\n  EVENT ADDRESSES:");
		for (i=0; i<events; i++)
		{
			sprintf(line, "    $%6s:  \\;", PrintHex((long)eventaddr[i]*address_scale, 3));
			Printout(line);
			if (eventin[i])
				{sprintf(line, "Event in:  %s", object[eventin[i]]);
				Printout(line);}
			else Printout("Global event");
		}
	}
#endif  /* !defined (LITTLE_VERSION) */

	if (statistics)
	{
		Printout("");
		PrintLine(79, '=');
		sprintf(line, "HUGO COMPILER v%d.%d%s STATISTICS FOR:  %s",
			HCVERSION, HCREVISION, HCINTERIM, PRINTED_FILENAME(sourcefilename));
		Printout(line);
		strftime(line, 32, "%m/%d/%y %H:%M:%S", localtime(&tick2));
		Printout(line);
		PrintLine(79, '=');

		sprintf(line, "Compiled %d lines in %d file(s)", tlines, totalfiles);
		Printout(line);
		sprintf(line, "\nObjects:    %5d (maximum %5d)      Routines: %5d (maximum %5d)", objectctr, MAXOBJECTS, routinectr, MAXROUTINES);
		Printout(line);
		sprintf(line, "Attributes: %5d (maximum %5d)      Events:   %5d (maximum %5d)", attrctr, MAXATTRIBUTES, eventctr, MAXEVENTS);
		Printout(line);
		sprintf(line, "Properties: %5d (maximum %5d)      Labels:   %5d (maximum %5d)", propctr, MAXPROPERTIES, labelctr, MAXLABELS);
		Printout(line);
		sprintf(line, "Aliases:    %5d (maximum %5d)      Globals:  %5d (maximum %5d)", aliasctr, MAXALIASES, globalctr, MAXGLOBALS);
		Printout(line);
		sprintf(line, "Constants:  %5d (maximum %5d)      Arrays:   %5d (maximum %5d)", constctr, MAXCONSTANTS, arrayctr, MAXARRAYS);
		Printout(line);

		sprintf(line, "\nWords in dictionary: %5d    Special words: %5d    Verbs: %5d", dictcount, syncount, verbs);
		Printout(line);

		sprintf(line, "\nObject file:  %s (%ld bytes)", PRINTED_FILENAME(objectfilename), codeptr);
		Printout(line);

		if (hlb || builddebug)
		{
			strcpy(line, "(");
			if (hlb) strcat(line, "precompiled ");
			if (builddebug) strcat(line, "debuggable ");
			strcat(line, "format)");
			Printout(line);
		}

		if (listing)
			{sprintf(line, "List file:    %s", PRINTED_FILENAME(listfilename));
			Printout(line);}

		sprintf(line, "\nElapsed compile time:  %u seconds", (unsigned int)tick);
		Printout(line);

		if (er || warn)
		{
			strcpy(line, "\n");
			if (er) sprintf(line+1, "ERRORS:  %d    ", er);
			if (warn) sprintf(line+strlen(line), "WARNINGS:  %d", warn);
			Printout(line);
		}

		PrintLine(79, '=');
	}
}


/* PRINTTOALL

	Prints the current word[] set to allfile.
*/

void PrinttoAll(void)
{
	int i;

	if (words > 0)
	{
		for (i=1; i<=words; i++)
		{
			if ((word[i][0]=='{' || word[i][0]=='}') && i != 1)
				if (fputs(":\n", allfile)==EOF) FatalError(WRITE_E, allfilename);

			fputs(word[i], allfile);
			fputc('\n', allfile);
			if (ferror(allfile)) FatalError(WRITE_E, allfilename);

			if (word[i][0]=='}' && i != words)
				if (fputs(":\n", allfile)==EOF) FatalError(WRITE_E, allfilename);
		}
		if (fputs(":!\n", allfile)==EOF) FatalError(WRITE_E, allfilename);

		/* then print line number for later error trapping */
		WriteWord(totallines, allfile);
	}
}


/* PRINTWORDS

	Contains a bunch of adjustments to properly space punctuation/
	symbols/operators which are only one character.  I.e.,

		Routine(x, y)

	instead of

		Routine ( x , y )
*/

void PrintWords(int n)
{
	char this, next;
	char printed_squote = false;
	char a[MAXBUFFER];

	if (words==0) return;

	strcpy(a, "");
	for (; n<=words; n++)
	{
		if (word[n][0]=='\0')
		{
			n++;
			if (n>words) break;
		}

		strcat(a, word[n]);

		/* Skip printing a space in all the following cases: */
		this = word[n][0];
		next = word[n+1][0];
		if (this=='(' || this=='[') continue;
		if (next==')' || next=='[' || next==']') continue;
		if (next==',' || next==';') continue;
		if (this=='\'')
		{
			if (printed_squote)
			{
				printed_squote = false;
				continue;
			}
			else printed_squote = true;
		}
		if (this=='#' && n>1) continue;
		if (this=='.' || next=='.') continue;
		if (next=='=' && (this=='+' || this=='-' || this=='/' || this=='*' || this=='&' || this=='|'))
			continue;
		if ((this=='+' || this=='-') && next==this)
		{
			if (word[n-1][0]!=this)
				strcat(a, word[n++]);
		}
		if (this=='&')
		{
			if ((words>1) && word[n-1][1]=='\0')
				continue;
		}

		strcat(a, " ");
	}
	Printout(a);
}


/* PUTINTREE

	Places object <obj> as next child of parent <p>.
*/

void PutinTree(int obj, int p)
{
	int nextobj;

	/* Don't bother doing anything other than defining the parent
	   as 0 if that's where obj is being placed
	*/
	if ((parent[obj] = p)==0) return;

	if (child[p]==0)
		child[p] = obj;
	else
	{
		nextobj = child[p];
		while (sibling[nextobj])
			nextobj = sibling[nextobj];
		sibling[nextobj] = obj;
	}
}


/* REMEMBERADDR

	Until it can be properly resolved by Pass3().
*/

void RememberAddr(long a, int b, unsigned int c)
{
	unsigned int q;
	unsigned int r;

	q = addrctr/ADDRBLOCKSIZE;
	r = addrctr%ADDRBLOCKSIZE;

	/* Check if the current block of memory for storing address
	   information has been used up
	*/
	if (r==0)
	{
		if (q==MAXADDRBLOCKS) FatalError(MEMORY_E, "");

		if ((addrdata[q] = malloc(sizeof(struct addrstruct)*ADDRBLOCKSIZE))==NULL)
			FatalError(MEMORY_E, "");
	}

	addrdata[q][r].addrptr = a;
	addrdata[q][r].addrtype = (char)b;
	addrdata[q][r].addrval = c;

	addrctr++;
}


/* REMOVECOMMAS

	Removes every "," word from word[] array.
*/

void RemoveCommas(void)
{
	int i;

	for (i=1; i<=words; i++)
	{
		if (word[i][0]==',')
			KillWord(i);
	}
}


/* RESOLVEADDR

	Resolves addresses remembered during compilation by RememberAddr().
	(If an .HLB file is being generated, addresses are not resolved;
	they are written as a table.)

	Routines and labels are resolved as their addresses.  Other
	addresses that will need to be refigured by the linker (i.e., in an
	.HLB file) are recorded as values.

	ResolveAddr() also resolves array values defined at compile time,
	but first it has to convert the stored array table offset into an
	absolute memory value.
*/

extern unsigned int arraytableaddr;

void ResolveAddr(void)
{
	unsigned int i;
	unsigned int a, b, c;
	long tempptr;

	tempptr = codeptr;

	for (i=0; i<addrctr; i++)
	{
		a = i/ADDRBLOCKSIZE;
		b = i%ADDRBLOCKSIZE;

		if (!hlb)
		{
			codeptr = addrdata[a][b].addrptr;
			/* Convert from an array table offset if necessary */
			if (addrdata[a][b].addrtype==ARRAY_T || addrdata[a][b].addrtype==(char)(ARRAY_T+ROUTINE_T))
				codeptr += (long)(arraytableaddr * 16L);

			c = addrdata[a][b].addrval;

			switch (addrdata[a][b].addrtype)
			{
				case ROUTINE_T:
				case (char)(ARRAY_T+ROUTINE_T):
					{WriteCode(raddr[c], 2);
					break;}

				case LABEL_T:
					{WriteCode(laddr[c], 2);
					break;}

				case VALUE_T:
					{WriteCode(c+(codestart-(unsigned int)HEADER_LENGTH)/address_scale, 2);
					break;}

				case ARRAY_T:
					{WriteCode(c, 2);
					break;}
			}
		}
		else
		{
			/* since labels are not written into an .HLB file's
			   linking information */
			if (addrdata[a][b].addrtype==LABEL_T)
			{
				addrdata[a][b].addrtype = VALUE_T;
				addrdata[a][b].addrval = laddr[addrdata[a][b].addrval];
			}

			WriteCode((unsigned int)((addrdata[a][b].addrptr)%65536L), 2);
			WriteCode((unsigned int)((addrdata[a][b].addrptr)/65536L), 1);
			WriteCode(addrdata[a][b].addrtype, 1);
			WriteCode(addrdata[a][b].addrval, 2);
		}
	}

	if (!hlb) codeptr = tempptr;
}


/* SAVEPROPDATA

	Writes the provided unsigned 16-bit integer to the property
	data heap.
*/

void SavePropData(unsigned int v)
{
	/* Add space to the property heap if needed */
	if (propheap%PROPBLOCKSIZE==0)
	{
		if (propheap/PROPBLOCKSIZE==MAXPROPBLOCKS) FatalError(MEMORY_E, "");

		if ((propdata[propheap/PROPBLOCKSIZE] = malloc((size_t)(PROPBLOCKSIZE*sizeof(unsigned int))))==NULL)
			FatalError(MEMORY_E, "");
	}

	propdata[propheap/PROPBLOCKSIZE][propheap%PROPBLOCKSIZE] = v;

	propheap++;
}


/* SEPARATEWORDS

	Tokenizes <buffer> into word[] array.
*/

void SeparateWords(void)
{
	char a[MAXBUFFER];
	char b;
	char compiler_directive = 0;
	int bloc = 0;         /* location in buffer buffer */
	int i, slen;
	int quote = 0;        /* quote = 1 when inside quotes */
	int squote = 0;       /* 1 when inside single quotes */
	int flag;


	words = 1;
	word[0] = "";
	word[1] = buffer;

	strcpy(a, buffer);
	strcpy(buffer, "");

	if (a[0]=='@' || a[0]=='#' || a[0]=='$') compiler_directive = true;

SeparateLine:
	slen = (int)strlen(a);

	for (i=0; i<slen; i++)
	{
		b = a[i];

		/* Not a quoted string */
		if (!quote && !squote)
		{
			flag = 0;
			
			/* Ignore trailing ':', but only when not in a string */
			if (b==':' && i==slen-1) break;

			switch (b = (char)tolower(b))
			{
				case ' ':
				case '\t':
				{
					if (word[words][0]!='\0')
					{
						if (++words>MAXWORDS) words=MAXWORDS;
						bloc++;
						word[words] = buffer+bloc;
						strcpy(word[words], "");
					}
					flag = 1;
					break;
				}
				case '\'':
				case '\"':
				{
					if (b=='\"')
						quote = 1;
					else
						squote = 1;
					flag = 1;
					buffer[bloc++] = b;
					break;
				}
				case '!':
				{
					buffer[bloc] = '\0';

					/* warning for C-ish '!=' */
					if (a[i+1]=='=') Error("?'=' commented out");

					goto DoneLine;
				}
				case ':':
				{
					if (i==0) b = '~';
					break;
				}
			}

			if (!flag)
			{
				/* check for tokens, operators, separators, etc.
				   (ASCII values)
				*/
				if (((b>='!' && b<='/') || (b>=':' && b<='@') ||
					(b>='[' && b<='^') || (b>='{' && b<='~'))
					&& (b!='$'))
				{
					if (compiler_directive) goto AddAnotherChar;

					if (word[words][0]!='\0')
					{
						bloc++;
						if (++words>MAXWORDS) words=MAXWORDS;
						word[words] = buffer+bloc;
					}
					word[words][0] = b;
					word[words][1] = '\0';
					bloc += 2;
					words++;
					if (words>MAXWORDS) words = MAXWORDS;
					word[words] = buffer+bloc;
					strcpy(word[words], "");

					flag = 1;
				}
			}

			if (!flag)               /* add another character */
			{
AddAnotherChar:
				buffer[bloc] = b;
				buffer[++bloc] = '\0';
			}
		}

		/* If a quote */
		else
		{
#if defined (NO_LATIN1_CHARSET)
			if (!isascii(b))
#else
			if ((unsigned char)b < 32)
#endif
			{
				sprintf(line, "?Non-ASCII character value:  %d", (unsigned char)b);
				Error(line);
			}
			buffer[bloc] = b;
			buffer[++bloc] = '\0';
			if (quote && b=='\"' && a[i-1]!='\\')
				{quote = 0;
				bloc++;
				if (++words > MAXWORDS) words = MAXWORDS;
				word[words] = buffer+bloc;
				strcpy(word[words], "");}
			else if (squote && b=='\'' && a[i-1]!='\\')
				{squote = 0;
				bloc++;
				if (++words > MAXWORDS) words = MAXWORDS;
				word[words] = buffer+bloc;
				strcpy(word[words], "");}
		}
	}

DoneLine:

	if (quote)
	{
		buffer[bloc++] = ' ';
		GetLine(bloc);
		if (buffer[bloc]=='\"' || feof(sourcefile))
		{
			Error("Unterminated string constant or text block");
		}
		else
		{
			strcpy(a, buffer+bloc);
			goto SeparateLine;
		}
	}

	if (word[words][0]=='\0') words--;

	for (i=1; i<=words; i++)
	{
		if (word[i][0]=='~' && word[i+1][0]=='=')
			{KillWord(i);
			word[i] = "~=";}
		else if (word[i][0]=='<' && word[i+1][0]=='=')
			{KillWord(i);
			word[i] = "<=";}
		else if (word[i][0]=='>' && word[i+1][0]=='=')
			{KillWord(i);
			word[i] = ">=";}
	}
	word[words+1] = "";
/*
strcpy(line, "");
for (i=1; i<=words; i++)
	sprintf(line+strlen(line), "\"%s\" ", word[i]);
Printout(line);
*/
}


/* SETATTRIBUTE */

void SetAttribute(int a)
{
	attr[a/32] = attr[a/32] | 1L<<a%32;
}


/* SETLIMIT */

void SetLimit(char *a)
{
	char flag = 0;
	int i;
	unsigned int j, limit = 32768, least = 0;

	a = &a[1];

	for (i=0; a[i]!='\0'; i++)
	{
		if (a[i]=='=')
		{
			a[i] = 0;
			j = atoi(a+i+1);
			if ((j==0) && (a[i+1]!='0'))
			{
				sprintf(line, "Invalid limit setting:  $%s=%s", a, a+i+1);
				Error(line);
				return;
			}

			/* Modifiable: */
			if (!STRICMP(a, "MAXOBJECTS"))
				MAXOBJECTS = j, flag = 1;
			if (!STRICMP(a, "MAXDICT"))
				MAXDICT = j, flag = 1;
			if (!STRICMP(a, "MAXDICTEXTEND"))
				MAXDICTEXTEND = j, flag = 1;
			if (!STRICMP(a, "MAXDIRECTORIES"))
				MAXDIRECTORIES = j, flag = 1;
			if (!STRICMP(a, "MAXEVENTS"))
				MAXEVENTS = j, flag = 1;
			if (!STRICMP(a, "MAXROUTINES"))
				MAXROUTINES = j, flag = 1;
			if (!STRICMP(a, "MAXLABELS"))
				MAXLABELS = j, flag = 1;
			if (!STRICMP(a, "MAXPROPERTIES"))
				MAXPROPERTIES = j, flag = 1, limit = 254,
				least = ENGINE_PROPERTIES;
			if (!STRICMP(a, "MAXALIASES"))
				MAXALIASES = j, flag = 1;
			if (!STRICMP(a, "MAXARRAYS"))
				MAXARRAYS = j, flag = 1;
			if (!STRICMP(a, "MAXCONSTANTS"))
				MAXCONSTANTS = j, flag = 1;
			if (!STRICMP(a, "MAXFLAGS"))
				MAXFLAGS = j, flag = 1;
			if (!STRICMP(a, "MAXSPECIALWORDS"))
				MAXSPECIALWORDS = j, flag = 1;

			/* Not modifiable: */
			if (!STRICMP(a, "MAXATTRIBUTES")) flag = 2;
			if (!STRICMP(a, "MAXGLOBALS")) flag = 2;
			if (!STRICMP(a, "MAXLOCALS")) flag = 2;

			if (flag==1 && j > limit) flag = 3;

			if (flag==1 && j < least) flag = 4;

			if (flag==0)
			{
				sprintf(line, "No such limit:  %s", a);
				Error(line);
			}
			if (flag==2)
			{
				sprintf(line, "Limit not modifiable:  %s", a);
				Error(line);
			}
			if (flag==3)
			{
				sprintf(line, "Limit exceeds maximum of %u:  $%s=%s", limit, a, a+i+1);
				Error(line);
			}
			if (flag==4)
			{
				sprintf(line, "Limit must be at least %u:  $%s=%s", limit, a, a+i+1);
				Error(line);
			}

			return;
		}
	}
	Error("Syntax error in limit setting:  $<setting>=<new limit>");
}


/* SETMEM

	Allocates memory for all dynamic program elements.
*/

char token_hash_table_built = false;

void SetMem(void)
{
	int i;

	/* Object arrays */
	if (!alloc_objects)
	{
		if (!(object = malloc(MAXOBJECTS*sizeof(char *)))) goto MemoryError;
		if (!(object_hash = malloc(MAXOBJECTS*sizeof(int *)))) goto MemoryError;
		if (!(oprop = malloc(MAXOBJECTS*sizeof(unsigned int)))) goto MemoryError;
		for (i=0; i<MAXATTRIBUTES/32; i++)
			if (!(objattr[i] = malloc(MAXOBJECTS*sizeof(long)))) goto MemoryError;
		if (!(objpropaddr = malloc(MAXOBJECTS*sizeof(unsigned int)))) goto MemoryError;
		if (!(oreplace = malloc(MAXOBJECTS*sizeof(char)))) goto MemoryError;
		if (!(parent = malloc(MAXOBJECTS*sizeof(int)))) goto MemoryError;
		if (!(sibling = malloc(MAXOBJECTS*sizeof(int)))) goto MemoryError;
		if (!(child = malloc(MAXOBJECTS*sizeof(int)))) goto MemoryError;

		for (i=0; i<MAXOBJECTS; i++) oreplace[i] = 0;

		alloc_objects = true;
	}

	/* Property arrays */
	if (!alloc_properties)
	{
		if (!(property = malloc(MAXPROPERTIES*sizeof(char *)))) goto MemoryError;
		if (!(property_hash = malloc(MAXPROPERTIES*sizeof(int *)))) goto MemoryError;
		if (!(propdef = malloc(MAXPROPERTIES*sizeof(unsigned int)))) goto MemoryError;
		if (!(propset = malloc(MAXPROPERTIES*sizeof(char)))) goto MemoryError;
		if (!(propadd = malloc(MAXPROPERTIES*sizeof(char)))) goto MemoryError;

		for (i=0; i<MAXPROPERTIES; i++) propdef[i] = 0, propadd[i] = 0;

		/* Pre-declare engine properties */
		property[0] = "name";
		property[1] = "before";
		property[2] = "after";
		property[3] = "noun";
		property[4] = "adjective";
		property[5] = "article";
		propctr = 6;
		ENGINE_PROPERTIES = 6;

		for (i=0; i<ENGINE_PROPERTIES; i++) property_hash[i] = FindHash(property[i]);

		/* Since both before and after are additive and complex,
		   they have the first two bits set in the propadd flag */
		propadd[1] = ADDITIVE_FLAG + COMPLEX_FLAG;
		propadd[2] = ADDITIVE_FLAG + COMPLEX_FLAG;

		alloc_properties = true;
	}

	/* Alias arrays */
	if (!alloc_aliases)
	{
		if (!(alias = malloc(MAXALIASES*sizeof(char *)))) goto MemoryError;
		if (!(alias_hash = malloc(MAXALIASES*sizeof(int *)))) goto MemoryError;
		if (!(aliasof = malloc(MAXALIASES*sizeof(int)))) goto MemoryError;
		alloc_aliases = true;
	}

	/* Array arrays */
	if (!alloc_arrays)
	{
		if (!(array = malloc(MAXARRAYS*sizeof(char *)))) goto MemoryError;
		if (!(array_hash = malloc(MAXARRAYS*sizeof(int *)))) goto MemoryError;
		if (!(arrayaddr = malloc(MAXARRAYS*sizeof(unsigned int)))) goto MemoryError;
		if (!(arraylen = malloc(MAXARRAYS*sizeof(unsigned int)))) goto MemoryError;
		alloc_arrays = true;
	}

	/* Constant arrays */
	if (!alloc_constants)
	{
		if (!(constant = malloc(MAXCONSTANTS*sizeof(char *)))) goto MemoryError;
		if (!(constant_hash = malloc(MAXCONSTANTS*sizeof(int *)))) goto MemoryError;
		if (!(constantval = malloc(MAXCONSTANTS*sizeof(unsigned int)))) goto MemoryError;
		alloc_constants = true;
	}

	/* Event arrays */
	if (!alloc_events)
	{
		if (!(eventin = malloc(MAXEVENTS*sizeof(unsigned int)))) goto MemoryError;
		if (!(eventaddr = malloc(MAXEVENTS*sizeof(unsigned int)))) goto MemoryError;
		alloc_events = true;
	}

	/* Routine arrays */
	if (!alloc_routines)
	{
		if (!(routine = malloc(MAXROUTINES*sizeof(char *)))) goto MemoryError;
		if (!(routine_hash = malloc(MAXROUTINES*sizeof(int *)))) goto MemoryError;
		if (!(raddr = malloc(MAXROUTINES*sizeof(unsigned int)))) goto MemoryError;
		if (!(rreplace = malloc(MAXROUTINES*sizeof(char)))) goto MemoryError;
		alloc_routines = true;
	}

	/* Label arrays */
	if (!alloc_labels)
	{
		if (!(label = malloc(MAXLABELS*sizeof(char *)))) goto MemoryError;
		if (!(label_hash = malloc(MAXLABELS*sizeof(int *)))) goto MemoryError;
		if (!(laddr = malloc(MAXLABELS*sizeof(unsigned int)))) goto MemoryError;
		alloc_labels = true;
	}

	/* Set up dictionary arrays */
	if (!alloc_dict)
	{
		if ((lexentry = malloc(MAXDICT*sizeof(char *)))==NULL) goto MemoryError;
		if ((lexnext = malloc(MAXDICT*sizeof(unsigned int)))==NULL) goto MemoryError;
		if ((lexaddr = malloc(MAXDICT*sizeof(unsigned int)))==NULL) goto MemoryError;
		for (i=0; i<=26; i++)
			lexstart[i] = 0;
		alloc_dict = true;

		dicttable = 1;
		dictcount = 1;
		AddDictionary(".");
		AddDictionary(",");
	}

	/* Allocate special words */
	if (!alloc_syn)
	{
		if ((syndata = malloc((size_t)(MAXSPECIALWORDS*sizeof(struct synstruct))))==NULL) goto MemoryError;
		alloc_syn = true;
	}

	/* Set up setting arrays */
	if (!alloc_sets)
	{
		if (MAXFLAGS==0) MAXFLAGS = 1;
		if ((sets = malloc(MAXFLAGS*sizeof(char *)))==NULL) goto MemoryError;
		for (i=0; i<MAXFLAGS; i++)
			sets[i] = MakeString("");
#if defined (COMPILE_V25)
		sets[0] = MakeString("_version2");
#else
		sets[0] = MakeString("_version3");
#endif
		alloc_sets = true;
	}

	/* Set up directory arrays */
	if (!alloc_directories)
	{
		if ((directory = malloc(MAXDIRECTORIES*sizeof(char *)))==NULL) goto MemoryError;
		alloc_directories = true;
	}

	/* Build the token and global hash tables, if necessary */
	if (!token_hash_table_built)
	{
		for (i=0; i<MAXGLOBALS; i++)
			globaldef[i] = 0;

		for (i=0; i<=TOKENS; i++)
			token_hash[i] = FindHash(token[i]);

		/* Pre-declare engine globals */
		global[0] = "object";
		global[1] = "xobject";
		global[2] = "self";
		global[3] = "words";
		global[4] = "player";
		global[5] = "actor";
		global[6] = "location";
		global[7] = "verbroutine";
		global[8] = "endflag";
		global[9] = "prompt";
		global[10] = "objects";
		global[11] = "system_status";
		globalctr = 12;
		ENGINE_GLOBALS = 12;

		for (i=0; i<ENGINE_GLOBALS; i++)
			global_hash[i] = FindHash(global[i]);

		token_hash_table_built = true;
	}


	return;         /* return safely */

MemoryError:
	FatalError(MEMORY_E, "");
}


/* SETSWITCHES */

void SetSwitches(char *a)
{
	char b, c = 0;
	int i;

	if (override) return;

	if (a[0]=='-') c = 1;

	for (i=c; i<(int)strlen(a); i++)
	{
		b = a[i];
		if (b>='A' && b<='Z') b+=('a'-'A');

		if (b=='a') aborterror = true;
		else if (b=='d') builddebug = true;
#if defined (DEBUG_FULLOBJ)
		else if (b=='f') fullobj = true;
#endif
		else if (b=='e') expandederr = true;
		else if (b=='h')
		{
			if (passnumber==1)
				Error("-h switch must be specified at invocation");
			else hlb = true;
		}
		else if (b=='i') printdebug = true;
		else if (b=='l' || b=='t')
		{
			if (b=='t') spellcheck = true;
			if (!listing && strcmp(sourcefilename, ""))
			{
				if (!(listfile = TrytoOpen(listfilename, "wt", "list")))
					FatalError(OPEN_E, listfilename);
				listing = true;
			}
			else if (!listing) listing = 2;
		}
		else if (b=='o') objecttree = true;
#ifdef STDPRN_SUPPORTED
		else if (b=='p') printer = true;
#endif
		else if (b=='s') statistics = true;
		else if (b=='u') memmap = true;
		else if (b=='v') percent = true;
		else if (b=='w') writeanyway = true;
		else if (b=='x') override = true;
#if !defined (COMPILE_V25)
		else if ((b=='2') && a[i+1]=='5') compile_v25 = true, address_scale = 4, i++;
		else if ((b=='3') && a[i+1]=='0') address_scale = 4, i++;
#endif

		else
		{
			sprintf(line, "Invalid switch:  %c", b);
			Error(line);
		}
	}

	if (spellcheck) percent = false;
}


/* STRIPQUOTES */

void StripQuotes(char *a)
{
	if (a[0]=='\"' && a[strlen(a)-1]=='\"')
		{a[strlen(a)-1] = '\0';
		strcpy(a, a+1);}
	else
		{sprintf(line, "Missing quotes:  %s", a);
		Error(line);}
}

