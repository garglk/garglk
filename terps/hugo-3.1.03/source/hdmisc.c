/*
	HDMISC.C

	contains miscellaneous debugging routines:

		About
		AllocMemory
		ArrayName
		CheckinRange
		DebuggerFatal
		DebugRunRoutine
		DeleteBreakpoint
		EnterBreakpoint
		EnterHelpTopic
		EnterSearch
		HextoDec
		IsBreakpoint
		LoadDebuggableFile
			ReadDebugSymbol
			ReadFileValue
			ReserveNameArray
		ObjectNumber
		ObjPropAddr
		PropertyNumber
		RecoverLastGood
		RoutineAddress
		RoutineName
		RuntimeWarning
		SearchHelp
		SearchLineFor
		SelectWatchType
		SetBreakpoint
		StealAddress

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "heheader.h"
#include "hdheader.h"


#define ENTER_KEY       13


/* Function prototypes: */
long ObjPropAddr(char *a);
int PropertyNumber(char *prop);
unsigned int ReadFileValue(void);
char *ReadDebugSymbol(void);
char **ReserveNameArray(int n);
unsigned int RoutineAddress(char *r);
int SearchLineFor(int l, char *a);


char invocation_path[MAXPATH];		/* of debugger		   */

/* These need to be set by the port: */
int D_SCREENWIDTH;			/* debug screen width      */
int D_SCREENHEIGHT;			/* debug screen height     */

struct breakpoint_structure breakpoint[MAXBREAKPOINTS];
char complex_prop_breakpoint = false;
char *context_help = "help";
char last_search[33] = "";
char in_help_mode = false;

/* object names */
extern int objects;	/* from hemisc.c */
char **objectname;
/* property names */
int properties; char **propertyname;
/* attribute names */
int attributes; char **attributename;
/* alias names, of */
int aliases; char **aliasname; unsigned int *aliasof;
/* global names */
int globals; char **globalname;
/* routine names, addresses */
int routines = 0; char **routinename; unsigned int *raddr;
/* event in, addresses */
extern int events;	/* from hemisc.c */
unsigned int *eventin, *eventaddr;
/* array names, addresses */
int arrays; char **arrayname; unsigned int *arrayaddr;


/* ABOUT */

void About(void)
{
	int l;

	debug_switchscreen(HELP);
	currently_updating = VIEW_HELP;
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	debug_clearview(VIEW_HELP);

	sprintf(debug_line, "About the Hugo v%d.%d%s Debugger\n\n", HEVERSION, HEREVISION, HEINTERIM);
	debug_settextpos(Center(debug_line), 1);
	debug_settextcolor(color[SELECT_TEXT]);
	debug_setbackcolor(color[SELECT_BACK]);
	debug_print(debug_line);

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);

	/* For formatting Help window text: */
	memset(debug_line, ' ', (size_t)(l = ((window[VIEW_HELP].width-64)/2+4)));
	debug_line[l] = '\0';

	debug_print(debug_line);
	debug_print("Written by Kent Tessman\n");
	debug_print(debug_line);
	debug_print("Copyright (c) 1995-2006\n");
	debug_print(debug_line);
	debug_print("The General Coffee Company Film Productions\n\n");
	debug_print(debug_line);
	debug_print("(");
	debug_print(PORT_NAME);
	debug_print(" port by ");
	debug_print(PORTER_NAME);
	debug_print(")\n\n");

	debug_print(debug_line);
	debug_print("Select \"Topic\" from the Help menu, then type \"help\".\n");

#if !defined (NO_WINDOW_PROMPTS)
	debug_windowbottomrow("Press any key...");
	hugo_getkey();
	debug_windowbottomrow("");

	debug_switchscreen(DEBUGGER);
#endif
}


/* ALLOCMEMORY

	Used in place of malloc() when it may be necessary to cannibalize
	the memory allocated for code-line history if needed, e.g., for
	saving a rectangle of screen text or adding a watch value.
*/

void *AllocMemory(size_t size)
{
	void *m;

	while ((m = malloc(size))==NULL) ShiftCodeLines();

	return m;
}


/* ARRAYNAME

	Returns the array name of a given address in the array table.
*/

char *ArrayName(unsigned int addr)
{
	int i;

	for (i=0; i<arrays; i++)
	{
		if (addr*2==arrayaddr[i])
			return arrayname[i];
	}
	return "<array>";
}


/* CHECKINRANGE

	Checks to make sure a given type of value is within the allowable
	range.  Returns true if in range.
*/

int CheckinRange(unsigned int val, unsigned int max, char *what)
{
	if (runtime_warnings)
	{
		if (val < 0 || val > max)
		{
			sprintf(debug_line, "%c%s out of range:  %ld", toupper(what[0]), what+1, (long)val);
			RuntimeWarning(debug_line);
			return false;
		}
	}
	return true;
}


/* CURRENTROUTINENAME

	Similar to RoutineName, except that it might also return the
	name of an event, object property, etc.
*/

char *CurrentRoutineName(unsigned int addr)
{
	static char cr[80];
	int i;

	for (i=0; i<routines; i++)
	{
		if (addr==raddr[i])
			return routinename[i];
	}

	for (i=0; i<events; i++)
	{
		if (addr==eventaddr[i])
		{
			if (eventin[i]==0)
				strcpy(cr, "Global event");
			else
				sprintf(cr, "Event in %s", objectname[eventin[i]]);
			return cr;
		}
	}

	return "<Routine>";
}


/* DEBUGGERFATAL */

#if !defined DEBUGGER_PRINTFATALERROR
/* from hewindow.c */
void DrawBox(int top, int bottom, int left, int right, int fcol, int bcol);
#endif

void DebuggerFatal(int n)
{
#if !defined (DEBUGGER_PRINTFATALERROR)
	int top, left, right, width;
#endif

	switch (n)
	{
		case D_MEMORY_ERROR:
		{
			strcpy(debug_line, "Out of memory");
			break;
		}
		case D_VERSION_ERROR:
		{
			sprintf(debug_line, "File must be compiled with version %d.%d", HEVERSION, HEREVISION);
			break;
		}
	}

#if !defined (DEBUGGER_PRINTFATALERROR)
	hugo_clearfullscreen();

	top = D_SCREENHEIGHT/2-3;
	width = (strlen(debug_line)>40)?strlen(debug_line):40;
	left = D_SCREENWIDTH/2-width/2-2;
	right = D_SCREENWIDTH/2+width/2+2;

	DrawBox(top, top+6, left, right, color[SELECT_TEXT], color[BREAKPOINT_BACK]);

	/* Tweak the borders a little to draw a shadow */
	debug_windowshadow(left, top, right+2, top+7);

	debug_settextpos(Center(debug_line), top+3);
	debug_print(debug_line);

	strcpy(debug_line, "DEBUGGER FATAL ERROR");
	debug_settextpos(Center(debug_line), top+1);
	debug_print(debug_line);

	debug_cursor(1);
	strcpy(debug_line, "[Press Enter to terminate...]");
	debug_settextpos(Center(debug_line), top+5);
	debug_print(debug_line);

	while (hugo_waitforkey()!=ENTER_KEY);

	hugo_cleanup_screen();
#else
	DEBUGGER_PRINTFATALERROR(debug_line);
#endif

	exit(n);
}


/* DEBUGRUNROUTINE

	Used as a wrapper for RunRoutine() in herun.c to save/restore
	local variable routines using the C stack, thereby saving
	manual labor.
*/

int debug_call_stack = 0;

void DebugRunRoutine(long addr)
{
	int i;
	char **temp_localname;
	char temp_debugger_interrupt;
	char temp_debugger_step_over;
	int temp_current_locals, temp_dbnest;
	struct window_structure temp_locals_window;

	temp_localname = AllocMemory((size_t)MAXLOCALS*sizeof(char *));

	for (i=0; i<MAXLOCALS; i++)
	{
		temp_localname[i] = AllocMemory((size_t)(strlen(localname[i])+1)*sizeof(char));

		strcpy(temp_localname[i], localname[i]);
		sprintf(localname[i], "local:%d", i+1);
	}
	temp_current_locals = current_locals;
	temp_locals_window = window[VIEW_LOCALS];
	window[VIEW_LOCALS].count = current_locals = 0;
	window[VIEW_LOCALS].changed = true;
	window[VIEW_LOCALS].selected = 0;

	temp_debugger_step_over = debugger_step_over;
	temp_debugger_interrupt = debugger_interrupt;
	debug_call_stack++;

	temp_dbnest = dbnest;
	dbnest = 0;

	RunRoutine(addr);

	dbnest = temp_dbnest;

	debug_call_stack--;
	debugger_step_over = temp_debugger_step_over;
	/* Don't reset this, since it will mean we can't step OUT of
	   a routine we didn't step INTO.
	debugger_interrupt = temp_debugger_interrupt;
	*/

	if (debugger_run) debugger_interrupt = 0;

	window[VIEW_LOCALS] = temp_locals_window;
	current_locals = temp_current_locals;
	window[VIEW_LOCALS].changed = true;
	for (i=0; i<MAXLOCALS; i++)
	{
		strcpy(localname[i], temp_localname[i]);
		free(temp_localname[i]);
	}
	free(temp_localname);
}


/* DELETEBREAKPOINT */

void DeleteBreakpoint(long baddr)
{
	unsigned int i, j;

	if (!IsBreakpoint(baddr)) return;

	for (i=0; i<window[VIEW_BREAKPOINTS].count; i++)
	{
		if (breakpoint[i].addr==baddr)
		{
			free(breakpoint[i].in);
			for (j=i; j<window[VIEW_BREAKPOINTS].count-1; j++)
				breakpoint[j] = breakpoint[j+1];
			window[VIEW_BREAKPOINTS].count--;

			window[VIEW_BREAKPOINTS].changed = true;

			/* Force Code window redraw */
			buffered_code_lines = FORCE_REDRAW;
			window[CODE_WINDOW].selected = window[CODE_WINDOW].count - 1;

			UpdateDebugScreen();
		}
	}
}


/* ENTERBREAKPOINT */

void EnterBreakpoint(void)
{
	char *a, *t;
	char addr[7];
	long paddr = 0;         /* potential address for breakpoint */

	t = "Set Breakpoint";

	/* First try getting the highlighted line from the code window: */
	if (window[CODE_WINDOW].count)
		paddr = StealAddress(window[CODE_WINDOW].selected-window[CODE_WINDOW].first);

	sprintf(addr, "%s", PrintHex(paddr));

	a = InputBox(t, "Enter breakpoint address, routine, or object.property:",
		MAXBUFFER, (paddr)?addr:"");

	if ((paddr = HextoDec(a))==0)
		if ((paddr = (RoutineAddress(a)*address_scale))==0)
			if ((paddr = (ObjPropAddr(a)*address_scale))==0)
			{
				if (event.action==CANCEL) return;

				DebugMessageBox(t, "Invalid breakpoint address");
				return;
			}

	SetBreakpoint(paddr);
}


/* ENTERHELPTOPIC */

void EnterHelpTopic(void)
{
	char *t;
	int i;

	in_help_mode = true;

	t = InputBox("Help", "Enter help topic keyword:", 33, "");

	in_help_mode = false;

	if (!strcmp(t, "")) return;

	/* Limit help searches to one word */
	for (i=0; i<(int)strlen(t); i++)
		if (t[i]==' ') t[i] = '\0';

	SearchHelp(t);
}


/* ENTERSEARCH */

void EnterSearch(void)
{
	char *search;
	unsigned int l, found = 0;
	struct window_structure *win;

	win = &window[CODE_WINDOW];

	search = InputBox("Search", "Search code for:", 33, last_search);

	if (!strcmp(search, "")) return;

	debug_windowbottomrow("Searching...");

	/* Skim through lines, first from the next line to
	   the last-recorded code line
	*/
	for (l=win->selected+1; l<win->count; l++)
	{
		if (SearchLineFor(l, search))
		{
			found = true;
			break;
		}
	}

	/* If we haven't matched yet, skim from the oldest code
	  line to the previous starting point (i.e., the current
	  line)
	*/
	if (!found)
	{
		for (l=0; l<=win->selected; l++)
		{
			if (SearchLineFor(l, search))
			{
				found = true;
				break;
			}
		}
	}

	debug_windowbottomrow("");

	if (!found)
	{
		sprintf(debug_line, "Not found:  %s", search);
		DebugMessageBox("Search", debug_line);
		return;
	}

	/* If we get to this point, then l holds the (next) line
	   containing the search string.
	*/
	strcpy(last_search, search);

	HighlightCurrent(0);

	win->selected = l;

	/* If we're still in the original window space... */
	if (win->selected > win->first && win->selected-win->first < win->height)
		goto LeaveSearch;

	/* ...or else we'll have to shift the view */
	win->first = win->selected - 4;		/* 4 is arbitrary; not top-of-window */
	if (win->first + win->height > win->count)
	{
		if ((int)win->count - (int)win->height > 0)
			win->first = win->count - win->height;
		else
			win->first = 0;
	}
	else if ((int)win->first < 0)
	{
		win->first = 0;
	}

	UpdateFullCodeWindow(win->first, win->horiz, win->width);

LeaveSearch:
	HighlightCurrent(1);
}


/* HEXTODEC

	Returns the unsigned decimal equivalent of the supplied string,
	or 0 if invalid.
*/

long HextoDec(char *hex)
{
	int i;
	long n, factor = 1, dec = 0;

	for (i=strlen(hex); i>0; i--)
	{
		n = toupper(hex[i-1]);

		/* not a hexadecimal digit */
		if (n<'0' || (n>'9' && n<'A') || n>'F')
			return 0;
		if (n>='A') n = n-'A'+10;
		else n-='0';
		n*=factor;

		dec+=n;
		factor*=16L;
	}
	return dec;
}


/* ISBREAKPOINT

	Returns false if the given address is not in the breakpoint list,
	or n+1 where the address is breakpoint[n].
*/

int IsBreakpoint(long addr)
{
	unsigned int i;

	for (i=0; i<window[VIEW_BREAKPOINTS].count; i++)
	{
		if (breakpoint[i].addr==addr)
			return ++i;
		else if (complex_prop_breakpoint)
		{
			complex_prop_breakpoint = false;
			return ++i;
		}
	}

	return false;
}


/* LOADDEBUGGABLEFILE */

void LoadDebuggableFile(void)
{
	int i;
	int compare_version;
	long debugdata;

	defseg = gameseg;

	if (game==NULL)
	{
		initaddr = mainaddr = 0;
		objects = properties = attributes = aliases = globals =
			routines = events = arrays = 0;
		return;
	}
	else if (Peek(H_DEBUGGABLE)!=1)
	{
		hugo_clearfullscreen();
		hugo_cleanup_screen();
		sprintf(debug_line, "File \"%s\" not compiled as .HDX file.\n", gamefile);
		sprintf(debug_line+strlen(debug_line), "(Use -d switch during compilation.)\n");

#if defined (DEBUGGER_PRINTFATALERROR)
		DEBUGGER_PRINTFATALERROR(debug_line);
#else
		printf(debug_line);
#endif
		
		exit(READ_E);                           /* unable to read */
	}

	compare_version = HEVERSION*10 + HEREVISION;

#if !defined (COMPILE_V25)
	if (game_version==25) compare_version = 25;
#else
	if (game_version==30) compare_version = 30;
#endif
	if (game_version!=compare_version)
		DebuggerFatal(D_VERSION_ERROR);

	/* Get location of debug data in the game file: */
	debugdata = PeekWord(H_DEBUGDATA) + Peek(H_DEBUGDATA+2)*65536L;

	/* Get location of workspace in the array table: */
	debug_workspace = PeekWord(H_DEBUGWORKSPACE);

	if (fseek(game, debugdata, SEEK_SET)) FatalError(READ_E);


	/*
	 * Create arrays for all the symbol names (and other symbol data):
	 *
	 */

	objectname = ReserveNameArray(objects);
	for (i=0; i<objects; i++)
		objectname[i] = ReadDebugSymbol();

	properties = ReadFileValue();
	propertyname = ReserveNameArray(properties);
	for (i=0; i<properties; i++)
		propertyname[i] = ReadDebugSymbol();

	attributes = ReadFileValue();
	attributename = ReserveNameArray(attributes);
	for (i=0; i<attributes; i++)
		attributename[i] = ReadDebugSymbol();

	aliases = ReadFileValue();
	aliasname = ReserveNameArray(aliases);
	if ((aliasof = malloc((size_t)aliases*sizeof(unsigned *)))==NULL)
		FatalError(MEMORY_E);
	for (i=0; i<aliases; i++)
	{
		aliasname[i] = ReadDebugSymbol();
		aliasof[i] = ReadFileValue();
	}

	globals = ReadFileValue();
	globalname = ReserveNameArray(globals);
	for (i=0; i<globals; i++)
		globalname[i] = ReadDebugSymbol();

	routines = ReadFileValue();
	routinename = ReserveNameArray(routines);
	if ((raddr = malloc((size_t)routines*sizeof(unsigned *)))==NULL)
		FatalError(MEMORY_E);
	for (i=0; i<routines; i++)
	{
		routinename[i] = ReadDebugSymbol();
		routinename[i][0] = (char)toupper(routinename[i][0]);
		raddr[i] = ReadFileValue();
	}

	events = ReadFileValue();
	if ((eventin = malloc((size_t)events*sizeof(unsigned *)))==NULL)
		FatalError(MEMORY_E);
	if ((eventaddr = malloc((size_t)events*sizeof(unsigned *)))==NULL)
		FatalError(MEMORY_E);
	for (i=0; i<events; i++)
	{
		eventin[i] = ReadFileValue();
		eventaddr[i] = ReadFileValue();
	}

	arrays = ReadFileValue();
	arrayname = ReserveNameArray(arrays);
	if ((arrayaddr = malloc((size_t)arrays*sizeof(unsigned *)))==NULL)
		FatalError(MEMORY_E);
	for (i=0; i<arrays; i++)
	{
		arrayname[i] = ReadDebugSymbol();
		arrayaddr[i] = ReadFileValue();
	}


	/* Reserve space for window information: */

	if ((codeline = malloc((size_t)MAX_CODE_LINES*sizeof(int *)))==NULL)
		FatalError(MEMORY_E);
}


/* OBJECTNUMBER

	Returns the object number of the supplied object name, or
	-1 if the name doesn't refer to an object.
*/

int ObjectNumber(char *obj)
{
	int i;

	for (i=0; i<objects; i++)
		if (!STRICMP(obj, objectname[i])) return i;

	return -1;
}


/* OBJPROPADDR

	Returns the address of the property routine given as a string
	"object.property", or 0 if the object/property combination
	doesn't exist or isn't a routine.
*/

long ObjPropAddr(char *o)
{
	char *p;
	int obj, prop;
	unsigned int propaddr;

	/* No '.' in the string */
	if ((p = strchr(o, '.'))==NULL) return 0;

	/* Split o into two strings:  o and p+1 */
	memset(p, '\0', 1);

	/* No object/property match */
	if ((obj = ObjectNumber(o))==-1 || (prop = PropertyNumber(p+1))==-1)
		return 0;

	getaddress = true;

	/* No property, or not a routine */
	if ((propaddr = GetProp(obj, prop, 1, 0))==0) return 0;

	return propaddr;
}


/* PROPERTYNUMBER

	Returns the property number of the supplied property name, or
	-1 if the name doesn't refer to a property.
*/

int PropertyNumber(char *prop)
{
	int i;

	for (i=0; i<properties; i++)
		if (!STRICMP(prop, propertyname[i])) return i;

	for (i=0; i<aliases; i++)
		if (!STRICMP(prop, aliasname[i]) && aliasof[i]>=MAXATTRIBUTES)
			return aliasof[i]-MAXATTRIBUTES;

	return -1;
}


/* READDEBUGSYMBOL */

char *ReadDebugSymbol(void)
{
	char *a;
	int len;

	if ((len = fgetc(game))==EOF) FatalError(READ_E);

	if ((a = malloc(++len*sizeof(char)))==NULL)
		FatalError(MEMORY_E);                /* out of memory */

	if (fgets(a, len, game)==NULL) FatalError(READ_E);

	return a;
}


/* READFILEVALUE */

unsigned int ReadFileValue(void)
{
	int n1, n2;

	if ((n1 = fgetc(game))==EOF) FatalError(READ_E);
	if ((n2 = fgetc(game))==EOF) FatalError(READ_E);

	return (n1 + n2*256);
}


/* RECOVERLASTGOOD

	Called in instances such as an engine error or while stepping
	backward in order to find the last good code statement.
*/

void RecoverLastGood(void)
{
	int c, i;

	history_count--;
	if (--history_last<0) history_last = MAX_CODE_HISTORY-1;

	i = window[CODE_WINDOW].count;
	c = i - window[CODE_WINDOW].selected;
	free(codeline[--i]);

	if (!runtime_error)
	{
		while((i>0) && (codeline[i-1][0]&0xFF)!='0')
			free(codeline[--i]);
	}

	window[CODE_WINDOW].count = i;
	window[CODE_WINDOW].selected = i-c;

	buffered_code_lines = FORCE_REDRAW;

	/* lost track of calls */
	window[VIEW_CALLS].count = 0;
	if (active_view==VIEW_CALLS) debug_clearview(VIEW_CALLS);
}


/* RESERVENAMEARRAY */

char **ReserveNameArray(int n)
{
	char **a;

	if ((a = malloc((size_t)n*sizeof(char *)))==NULL)
		FatalError(MEMORY_E);                /* out of memory */

	return a;
}


/* ROUTINEADDRESS

	Returns the (packed) address of the supplied routine name, or 0
	if the name doesn't refer to a routine.
*/

unsigned int RoutineAddress(char *r)
{
	char rout[33];
	int i;

	for (i=0; i<routines; i++)
	{
		strcpy(rout, routinename[i]);
		if (!STRICMP(r, rout))
			return raddr[i];
	}

	return 0;
}


/* ROUTINENAME

	Returns the routine name of a given code address.
*/

char *RoutineName(unsigned int addr)
{
	static char r[80];
	int i, obj;
	int proplen;
	int ploc;		/* location in property table */

	for (i=0; i<routines; i++)
	{
		if ((unsigned short)addr==raddr[i])
			return routinename[i];
	}

	for (i=0; i<events; i++)
	{
		if ((unsigned short)addr==eventaddr[i])
		{
			/* There's an extra space in "Global event " for
			   using "vent " as the sub-string to identify
			   events.
			*/
			if (eventin[i])
				sprintf(r, "Event in %s", objectname[eventin[i]]);
			else
				strcpy(r, "Global event ");
			return r;
		}
	}

	obj = 0;
	ploc = properties*3+2;
	defseg = proptable;

	while (obj < objects)
	{
		proplen = Peek(ploc+1);
		if (Peek(ploc)==PROP_END)
			obj++, ploc++;
		else if (proplen==PROP_ROUTINE && PeekWord(ploc+2)==(unsigned int)addr)
		{
			sprintf(r, "%s.%s", objectname[obj], propertyname[Peek(ploc)]);
			defseg = gameseg;
			return r;
		}
		else
			ploc+=((proplen!=PROP_ROUTINE)?proplen:1)*2+2;
	}


	defseg = gameseg;

/* Unknown: */
	return "<Routine>";
}


/* RUNTIMEWARNING */

void RuntimeWarning(char *a)
{
	char switch_to_game = true;

	if (active_screen!=GAME) switch_to_game = false;
	SwitchtoDebugger();
	DebugMessageBox("Runtime Warning", a);
	debugger_interrupt = true;
	if (switch_to_game) SwitchtoGame();
}


/* SEARCHHELP */

int SearchHelp(char *search_topic)
{
	char drive[MAXDRIVE], dir[MAXDIR], fname[MAXFILENAME], ext[MAXEXT];
	char help_path[MAXPATH];
	char *t, topic[33], compare_topic[33];
	int i, l, linecount;
	FILE *helpfile = NULL;

	if (in_help_mode) return false;
	in_help_mode = true;

	/* Strip search_topic of any unwanted characters--like menu
	   shortcut key indicators--and limit it to one word
	*/
	strcpy(topic, "");
	i = 0;
	for (l=0; l<=(int)strlen(search_topic); l++)
	{
		while (search_topic[l]=='&') l++;
		topic[i] = search_topic[l];
		if (topic[i]==' ') topic[i] = '\0';
		i++;
	}

	t = "Help Error";

	hugo_splitpath(invocation_path, drive, dir, fname, ext);
	hugo_makepath(help_path, drive, dir, HELP_FILE, "");

	if ((helpfile = fopen(help_path, "rb"))==NULL)
	{
		sprintf(debug_line, "Unable to load help file:  %s", HELP_FILE);
		DebugMessageBox(t, debug_line);
		goto ReturnFalse;
	}

	debug_switchscreen(HELP);
	currently_updating = VIEW_HELP;
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	debug_clearview(VIEW_HELP);

	while (!feof(helpfile))
	{
		if (fgets(debug_line, MAXBUFFER, helpfile)==NULL) goto HelpError;

		if (!STRICMP(debug_line, "$end\n")) break;

		/* Lines of keywords are in quotation marks--check for
		   a match against the topic
		*/
		else if (debug_line[0]=='\"')
		{
			/* First off, change the last quote to a space */
			debug_line[strlen(debug_line)+1] = '\0';
			debug_line[strlen(debug_line)-1] = ' ';

			/* Check if a match is found */
			l = 0;
			for (i=0; (size_t)i<strlen(debug_line)-strlen(topic); i++)
			{
				if (strlen(debug_line) < strlen(topic) || strlen(topic) > 32)
					break;

				/* The first word begins at 1, since 0 is
				   the opening '"' */
				if (i > 1)
					/* Find the start of the next word */
					while (debug_line[i++]!=' ')
						if (debug_line[i]=='\0') break;

				strncpy(compare_topic, debug_line+i, strlen(topic));
				compare_topic[strlen(topic)] = '\0';
				if (!STRICMP(topic, compare_topic))
				{
					l = 1;
					break;
				}
			}

			/* If a match is found: */

			if (l==1)
			{
				/* Get the true topic and remove the '\n' */
				if (fgets(help_path, MAXBUFFER, helpfile)==NULL)
					goto HelpError;
				help_path[strlen(help_path)-1] = '\0';

				sprintf(debug_line, "Help on \"%s\"\n", help_path);
				debug_settextpos(Center(debug_line), 1);
				debug_settextcolor(color[SELECT_TEXT]);
				debug_setbackcolor(color[SELECT_BACK]);
				debug_print(debug_line);
				linecount = 2;

GetAnotherLine:
				/* Help file lines are a maximum of 64 characters
				   long; this is to center them in the Help
				   window:
				*/
				memset(debug_line, ' ', (size_t)(l = ((window[VIEW_HELP].width-64)/2)));

				do
				{
					if (fgets(debug_line+l, MAXBUFFER, helpfile)==NULL)
						goto HelpError;
				}
				while (linecount==0 && debug_line[l]=='\n');

				/* If the line doesn't start with a helpfile
				   control character, it must be printed help
				   text
				*/
				if (debug_line[l]!='\"' && debug_line[l]!='$' && debug_line[l]!='#')
				{
					debug_settextcolor(color[NORMAL_TEXT]);
					debug_setbackcolor(color[NORMAL_BACK]);
					debug_print(debug_line);
#if !defined (NO_WINDOW_PROMPTS)
					if (++linecount >= D_SCREENHEIGHT)
					{
						debug_windowbottomrow("Enter to continue, Escape to quit");
						debug_settextcolor(color[NORMAL_TEXT]);
						debug_setbackcolor(color[NORMAL_BACK]);

						do
							debug_getevent();
						while (!(event.action==SELECT && event.object==0) && event.action!=CANCEL);

						if (event.action==CANCEL) goto ReturnTrue;

						hugo_clearfullscreen();
						debug_windowbottomrow("");
						debug_settextpos(1, 1);
						linecount = 0;
					}
#endif
					goto GetAnotherLine;
				}

				/* Otherwise, we must be done printing this
				   section of help text
				*/
				else
				{
#if !defined (NO_WINDOW_PROMPTS)
					debug_windowbottomrow("Finished - Press any key...");
					hugo_waitforkey();
					debug_windowbottomrow("");
ReturnTrue:
					fclose(helpfile);
					debug_switchscreen(DEBUGGER);
					debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);
#else
					debug_windowbottomrow("");
					fclose(helpfile);
#endif
					in_help_mode = false;
					return true;
				}
			}
		}
	}

	sprintf(debug_line, "Unable to find topic:  %s", topic);
	DebugMessageBox(t, debug_line);
	goto ReturnFalse;

HelpError:
	DebugMessageBox(t, "Error reading from help file");

ReturnFalse:
	if (helpfile) fclose(helpfile);
	debug_switchscreen(DEBUGGER);
	debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);
	in_help_mode = false;
	return false;
}


/* SEARCHLINEFOR */

int SearchLineFor(int ln, char *a)
{
	int i, len, start, nomatch = true;

	if ((len = int_strlen(codeline[ln])) < (int)strlen(a)) return false;

	len-=strlen(a);

	for (start=0; start<=len; start++)
	{
		nomatch = 0;
		for (i=0; i<(int)strlen(a); i++)
		{
			if (toupper((int)(codeline[ln][i+start]&0xFF))!=toupper(a[i]))
			{
				nomatch = true;
				continue;
			}
		}
		if (!nomatch) break;
	}

	if (nomatch) return false;

	return true;
}


/* SELECTWATCHTYPE */

int SelectWatchType(void)
{
	AllocateChoices(7);

	choice[0] = "Watch As";
	choice[1] = "Value";
	choice[2] = "Object";
	choice[3] = "Dictionary Entry";
	choice[4] = "Routine Address";
	choice[5] = "Array Address";
	choice[6] = "Property";
	choice[7] = "Attribute";

	switch (SelectBox(7, 1))
	{
		case 0:  return 0;
		case 1:  return VALUE_T;
		case 2:  return OBJECTNUM_T;
		case 3:  return DICTENTRY_T;
		case 4:  return ROUTINE_T;
		case 5:  return ARRAYDATA_T;
		case 6:  return PROP_T;
		case 7:  return ATTR_T;
		default: return 0;
	}
}


/* SETBREAKPOINT */

void SetBreakpoint(long addr)
{
	char *a;

	/* if already in breakpoint list */
	if (IsBreakpoint(addr)) return;

	if (window[VIEW_BREAKPOINTS].count==MAXBREAKPOINTS)
	{
		sprintf(debug_line, "Maximum of %d breakpoints", MAXBREAKPOINTS);
		DebugMessageBox("Set Breakpoint", debug_line);
		return;
	}

	breakpoint[window[VIEW_BREAKPOINTS].count].addr = addr;

	a = RoutineName((unsigned int)(addr/address_scale));

	/* Try to figure out the name of the location where the
	   breakpoint is.  This is possible if it is a.) the
	   current routine, or b.) the starting address of a
	   routine identifiable by RoutineName().
	*/
	breakpoint[window[VIEW_BREAKPOINTS].count].in =
		 strdup((codeptr==addr)?(RoutineName(currentroutine)):
			(a[0]!='<')?a:"<Unknown>");

	++window[VIEW_BREAKPOINTS].count;
	window[VIEW_BREAKPOINTS].changed = true;

	UpdateFullCodeWindow(window[CODE_WINDOW].first, window[CODE_WINDOW].horiz, window[CODE_WINDOW].width);

	UpdateDebugScreen();
}


/* STEALADDRESS

	Attempts in a backwards manner to get the address at the start of
	the given line in the Code window.  Returns false if it doesn't
	start with an address.
*/

long StealAddress(int l)
{
	int i;

	l+=window[CODE_WINDOW].first;

	if ((unsigned)l >= window[CODE_WINDOW].count)
		return 0;

	/* not even a potential address */
	if (((!window[CODE_WINDOW].count) || (codeline[l][0]&0xFF)=='\0') || int_strlen(codeline[l])<6)
		return 0;

	for (i=0; i<=5; i++)
		debug_line[i] = (unsigned char)(codeline[l][i]&0xFF);
	debug_line[6] = '\0';

	return HextoDec(debug_line);
}
