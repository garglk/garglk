/*
	HDINTER.H

	contains definitions and prototypes for engine/debugger interfacing
	that are referenced by both the Engine and the Debugger.

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


/*-------------------------------------------------------------------------*/


#if !defined (PROGRAM_NAME)
#define PROGRAM_NAME 	"hd"
#endif

/* screen states */
#define INACTIVE_SCREEN (-1)
#define GAME	 	 0
#define DEBUGGER 	 1
#define HELP		 2
#define AUXILIARY	 3

#define WINDOW_COUNT	 	11

/* The non-sequential numbering of the window constants accounts for
   spacers on the View menu:
*/
#define CODE_WINDOW	 	0
#define VIEW_WATCH       	1
#define VIEW_CALLS       	2
#define VIEW_BREAKPOINTS 	3
#define VIEW_LOCALS      	4
#define VIEW_ALIASES     	5
#define VIEW_HELP		7
#define VIEW_AUXILIARY		8
#define VIEW_OUTPUT      	10

#define MAX_CODE_HISTORY	256
#define MAXCALLS		48
#define MAXBREAKPOINTS		32
#define MAXWATCHES		32

#define FORCE_REDRAW		100

/* Event data: */

/* actions... */
#ifdef WIN32
#undef DELETE
#endif
enum D_ACTION_TYPE
{
	OPEN_MENUBAR = 1, OPEN_MENU, CANCEL, MOVE, SELECT,
	KEYPRESS, SHORTCUT_KEY, SWITCH_WINDOW, TAB, DELETE,
	SINGLECLICK,	/* i.e., select */
	DOUBLECLICK	/* i.e., select and close */
};

/* ...and (some) object types */
enum D_OBJECT_TYPE
{
	UP = 1, DOWN, LEFT, RIGHT, PAGEUP, PAGEDOWN,

	/* START and FINISH are used to move to the very top or bottom,
	   while HOME and END generally refer to the start and end of
	   a line, respectively.
	*/
	START, FINISH, HOME, END
};

enum D_ERROR_TYPE
{
	D_MEMORY_ERROR, D_VERSION_ERROR
};

struct event_structure
{
	int action;
	int object;
};

struct window_structure
{
	unsigned int first;	/* first line appearing in the window 	*/
	unsigned int selected;	/* selected line 			*/
	unsigned int count;	/* total lines in (or out of) window	*/
	unsigned int top;       /* where top line is on the screen      */
	unsigned int height;    /* maximum # of lines                   */
	int width;              /* maximum # of characters              */
	int horiz;              /* horizontal shift                     */
	char changed;		/* true if window needs updating	*/
};

struct call_structure
{
	unsigned int addr;	/* address of call 			*/
	char param;		/* true if arguments passed 		*/
};

struct breakpoint_structure
{
	long addr;              /* address 				*/
	char *in;               /* routine name 			*/
};

struct watch_structure
{
	int len;		/* number of bytes in watch expression  */
	unsigned char *expr;	/* the watch expression itself		*/
	int lastval;		/* value when last evaluated		*/
	int watchas;		/* type of watch value			*/
	unsigned int strlen;    /* of the expression as a string        */
	char isbreak;		/* true if it acts as a breakpoint	*/
	char illegal;		/* true if it evaluates to illegal val. */
};


/*--------------------------------------------------------------------------*/

void debug_getinvocationpath(char *argv);

/* hd.c */
void Debugger(void);
void StartDebugger(void);

extern char debug_line[];
extern struct event_structure event;
extern char debugger_run, debugger_interrupt, during_input,
	debugger_collapsing;
extern char trace_complex_prop_routine;
extern char debugger_step_over; extern int step_nest;
extern char debugger_skip, debugger_step_back;
extern char debugger_has_stepped_back;
extern char debugger_finish;
extern char runtime_error;
extern struct call_structure call[];
extern int current_locals;
extern char localname[][32];
extern unsigned int currentroutine;
extern int dbnest;
extern char runtime_warnings;
extern unsigned int runaway_counter;
extern long code_history[];
extern int dbnest_history[];
extern int history_last; extern int history_count;


/* hddecode.c */
void AddLinetoCodeWindow(long addr);
void AddStringtoCodeWindow(char *a);
void ShiftCodeLines(void);

extern long this_codeptr, next_codeptr;
extern int **codeline;
extern int buffered_code_lines;


/* hdmisc.c */
char *ArrayName(unsigned int addr);
int CheckinRange(unsigned int val, unsigned int max, char *what);
void DebuggerFatal(int n);
void DebugRunRoutine(long addr);
int IsBreakpoint(long addr);
void LoadDebuggableFile(void);
void RecoverLastGood(void);
char *RoutineName(unsigned int addr);
void RuntimeWarning(char *msg);

extern struct breakpoint_structure breakpoint[];
extern char complex_prop_breakpoint;
extern char *context_help;
extern int objects; extern char **objectname;
extern int properties; extern char **propertyname;
extern int attributes; extern char **attributename;
extern int aliases; extern char **aliasname; extern unsigned int *aliasof;
extern int globals; extern char **globalname;
extern int routines; extern char **routinename; extern unsigned int *raddr;
extern int events; extern unsigned int *eventin;
extern unsigned int *eventaddr;
extern int arrays; extern char **arrayname; extern unsigned int *arrayaddr;


/* hdupdate.c */
void UpdateDebugScreen(void);

extern struct window_structure window[];
extern int active_screen;


/* hdval.c */
int EvalWatch(void);
void SetupWatchEval(int i);

extern struct watch_structure watch[];
extern char debug_eval, debug_eval_error;
extern unsigned int debug_workspace;


/* hdwindow.c */
void DebugMessageBox(char *title, char *caption);
void SwitchtoDebugger(void);
void SwitchtoGame(void);
