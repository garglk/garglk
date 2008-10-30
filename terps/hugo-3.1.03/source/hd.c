/*
	HD.C

	contains main debugging routines:

		Debugger
		InitDebugger
		StartDebugger

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


#define DEBUG_INIT_PASS
#define INIT_PASS

#include "heheader.h"
#include "hdheader.h"


/* Function prototypes: */
void InitDebugger(void);

char *menu[MENU_HEADINGS][MENU_SUBHEADINGS] =
{
	{"&File", "&Restart", "&Print...", "-",
		"E&xit Debugger", ""},
	{"&View", "&Watch Window", "&Calls", "&Breakpoints",
		"&Local Variables", "Property/Attribute &Aliases",
		"-", "&Help", "A&uxiliary", "-", "&Output", ""},
	{"&Run", "&Go", "&Finish Routine", "-", "&Step",
		"Step &Over", "S&kip Next", "St&ep Back", ""},
	{"&Debug", "&Search Code...", "-", "&Watch...",
		"Set &Value...", "&Breakpoint...", "-",
		"Object &Tree...", "&Move Object...",
		"-", "Un&format Nesting", "&Runtime Warnings Off", ""},
	{"&Tools", "&Setup...", ""},
	{"&Help", "&Topic...", "&Shortcut Keys...",
		"&About the Hugo Debugger", ""}
};
struct menu_structure menu_data[MENU_HEADINGS];

struct event_structure event;           /* action and object   		*/

/* These are defined here instead of in hdwindow.c, since hdwindow.c
   can be replaced entirely (as in the Win32 port), leaving these
   externs in hdheader.h.
*/
char menu_shortcut_keys[MENU_SUBHEADINGS];  /* reset each menu     */
char menubar_isactive;                      /* true or false       */
int active_menu_heading;                    /* MENU_CONSTANT       */
char **choice;                              /* array for SelectBox */

char debugger_run = 0;                  /* true when running freely     */
char debugger_interrupt = 0;		/* true if stepping 		*/
char debugger_collapsing;		/* true if collapsing calls	*/
char during_input;			/* true in hugo_getline() 	*/
char trace_complex_prop_routine = 0;	/* true if running obj.prop	*/
char debugger_step_over = 0;		/* true if stepping over	*/
char debugger_skip = 0;			/* true if skipping next	*/
char debugger_finish = 0;		/* true if finishing routine	*/
char debugger_step_back = 0;		/* true if stepping back	*/
char debugger_has_stepped_back = 0;	/* true once stepped back	*/
int step_nest;				/* stepping over nested calls	*/

char debug_line[MAXBUFFER];

/* For engine routines: */
/* hemisc.c */
char runtime_error = 0;			/* set by FatalError()		*/
/* herun.c */
struct call_structure call[MAXCALLS];
int current_locals;                     /* locals in current routine    */
char localname[MAXLOCALS][32];		/* names of current locals	*/
unsigned int currentroutine = 0;	/* for Code window caption      */
int dbnest;				/* for nesting printed code  	*/
char runtime_warnings = 1;              /* true if checking             */
unsigned int runaway_counter = 0;	/* possible runaway loops	*/
long code_history[MAX_CODE_HISTORY];	/* for stepping backward	*/
int dbnest_history[MAX_CODE_HISTORY];	/* for keeping nesting right	*/
int history_last = 0;			/* last executed statement	*/
int history_count = 0;			/* total recorded statements	*/


/* STARTDEBUGGER

	Called by main() in HE.C.
*/

void StartDebugger(void)
{
	if ((currentroutine = initaddr)==0) currentroutine = mainaddr;
	codeptr = 0;

	debug_cursor(1);

	active_view = VIEW_WATCH;
	active_window = CODE_WINDOW;

	SwitchtoDebugger();

	InitDebugger();

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	hugo_clearfullscreen();
	UpdateDebugScreen();

	/* Setting debugger_interrupt means that Debugger() will be
	   called as soon as the engine tries to execute any instruction.
	*/
	debugger_interrupt = true;

	SwitchtoGame();
}


/* DEBUGGER

	The main debugger event cycle, called whenever normal code
	execution is interrupted to switch to the debugger.
*/

void Debugger(void)
{
	unsigned char m;
	int i;

	debugger_collapsing = false;

	debugger_run = false;
	debugger_skip = false;
	debugger_step_over = false;
	debugger_step_back = false;
	debugger_finish = false;
	runtime_error = false;

	SwitchtoDebugger();
	UpdateDebugScreen();

/* This is the first round of event-checking.  If a new action is
   requested, we jump down below to the DoNewAction label. */

MainEventLoop:
	while (true)	/* endless loop */
	{
		/* print the bottom caption */
#if !defined (USE_OTHER_MENUS)
		sprintf(debug_line, " Press %s for menu", MENUBAR_KEY);
#else
/*		sprintf(debug_line, " Hugo Debugger v%d.%d%s", HEVERSION, HEREVISION, HEINTERIM); */
		sprintf(debug_line, " %s", gamefile);
		if (RoutineName(currentroutine)[0]!='<')
			sprintf(debug_line+strlen(debug_line), ": %s", RoutineName(currentroutine));
#endif
		debug_windowbottomrow(debug_line);

		debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);

		context_help = "help";

		debug_getevent();

		switch (event.action)
		{
			case SINGLECLICK:
			case DOUBLECLICK:
				/* The SINGLECLICK and DOUBLECLICK action 
				   currently assume that the newly clicked 
				   line is already visible on the screen
                                */
				if ((unsigned)event.object >= window[active_window].count)
				{
					if (window[active_window].count==0)
						break;
					event.object = window[active_window].selected;
				}
				HighlightCurrent(0);
				window[active_window].selected = event.object;
				HighlightCurrent(1);
				
				if (event.action==SINGLECLICK) goto MainEventLoop;

				event.action = SELECT;
				event.object = 0;

				/* fall through for DOUBLECLICK */

			case SELECT:
			{
				/* If, for example, a function key */
				if (event.object!=0) goto DoNewAction;

				/* Otherwise, fall through to Navigate() */
			}
			case KEYPRESS:
			case DELETE:
			case MOVE:
			{
				Navigate();
				break;
			}
			case OPEN_MENUBAR:
			{
				OpenMenubar();
				if (event.action==SELECT)
					goto DoNewAction;
				break;
			}
			case SWITCH_WINDOW:
			{
				HighlightCurrent(0);    /* erase highlight */
				active_window = (active_window==CODE_WINDOW)?active_view:CODE_WINDOW;
				UpdateDebugScreen();
				HighlightCurrent(1);
				break;
			}
			case SHORTCUT_KEY:
			{
				for (i=0; i<MENU_HEADINGS; i++)
				{
					/* check if a shortcut key was pressed
					   for the open menu */
					if (event.object==menu_data[i].shortcut_key)
					{
						active_menu_heading = (i+1)*0x10;
						OpenMenu(active_menu_heading);
						if (event.action==SELECT)
							goto DoNewAction;
						break;
					}
				}
				break;
			}
			case TAB:
			{
ToggleGameWindow:
				if (during_input)	/* hugo_getline() */
				{
					SwitchtoGame();
					return;
				}
				else
				{
                                        SwitchtoGame();
					hugo_waitforkey();
					SwitchtoDebugger();
                                        HighlightCurrent(1);
				}
				break;
			}
		}
	}


/* At this point, the user has selected a new action for the debugger to
   carry out. */

DoNewAction:
	switch (event.action)
	{
		case SELECT:
		{
			context_help = menu[event.object/0x10-1][event.object%0x10];

		 	switch (event.object)
			{

			/* FILE */
				case MENU_FILE + FILE_RESTART:
				{
					if (game==NULL) break;

					sprintf(debug_line, "Restart %s?", gamefile);
					DebugMessageBox("Restart", debug_line);
					if (event.action==CANCEL)
						break;

				/* If debugger_collapsing is 2 instead of 1,
				   a "Normal program termination" message is
				   overridden.
				*/
					debugger_collapsing = 2;
					var[endflag] = true;
					during_input = false;
					buffered_code_lines = FORCE_REDRAW;
					return;
				}
				case MENU_FILE + FILE_PRINT:
				{
					sprintf(debug_line, "Print active window to %s", printer_name);
					DebugMessageBox("Print", debug_line);
					if (event.action!=CANCEL)
						HardCopy();
					break;
				}
				case MENU_FILE + FILE_EXIT:
				{
					DebugMessageBox("Exit", "Exit debugger?");
					if (event.action==CANCEL)
						break;

					SaveSetupFile();

					hugo_closefiles();
					hugo_cleanup_screen();
					exit(0);
				}

			/* RUN */
				case MENU_RUN + RUN_GO:
				{
					debugger_interrupt = false;
					debugger_run = true;
					goto ReturntoGame;
				}
				case MENU_RUN + RUN_FINISH:
				{
					if (during_input)
						goto NotDuringInput;

					AddStringtoCodeWindow("...");
					step_nest = 1;
					debugger_finish = true;

					/* Force redraw */
					buffered_code_lines = FORCE_REDRAW;
					window[VIEW_CALLS].changed = true;

					/* Lose track of history*/
					history_count = 0;

					if (--window[VIEW_CALLS].count <= 0)
						window[VIEW_CALLS].count = 0;
					window[VIEW_CALLS].changed = true;

					goto RunStepOver;
				}
				case MENU_RUN + RUN_SKIP:
				{
				/* Conditionals have to find the next
				   statement using the coded skip-over
				   address.
				*/
					switch (MEM(this_codeptr))
					{
						case IF_T:
						case ELSEIF_T:
						case ELSE_T:
						case DO_T:
						case WHILE_T:
						case FOR_T:
						case SELECT_T:
						{
							codeptr = (long)(MEM(codeptr+1)+MEM(codeptr+2)*256+codeptr+1);
							break;
						}

						/* For non-conditionals: */
						default:
							codeptr = next_codeptr;
					}
					debugger_skip = true;

					/* Fall through to RUN_STEP: */

				}
				case MENU_RUN + RUN_STEP:
				{
RunStep:
					debugger_interrupt = true;
					debugger_step_over = false;
					goto ReturntoGame;
				}
				case MENU_RUN + RUN_STEPOVER:
				{
					if (debug_call_stack==0) goto RunStep;
					step_nest = 0;
RunStepOver:
					debugger_step_over = true;
					debugger_interrupt = false;
					goto ReturntoGame;
				}
				case MENU_RUN + RUN_STEPBACK:
				{
					if (during_input)
					{
NotDuringInput:
						DebugMessageBox("During Input", "Function not permitted during input");
						break;
					}

					if (history_count==0)
					{
						DebugMessageBox("Step Back", "No statements in code history");
						break;
					}

					/* Make sure we don't try to step back
					   out of 'window', 'readfile', etc.
					*/
					if (history_last==0)
						m = MEM(code_history[MAX_CODE_HISTORY-1]);
					else
						m = MEM(code_history[history_last-1]);
					if (m==WINDOW_T || m==READFILE_T || m==WRITEFILE_T)
					{
						DebugMessageBox("Nested Code Block", "Unable to restore stack frame");
						break;
					}

					/* Erase current local names, since
					   they may no longer be valid
					*/
					for (i=0; i<MAXLOCALS; i++)
						sprintf(localname[i], "local:%d", i+1);
					window[VIEW_LOCALS].count = current_locals = 0;
					window[VIEW_LOCALS].changed = true;

					debugger_interrupt = true;
					debugger_step_back = true;

					/* Local variables become unavailable
					   after stepping backward
					*/
					debugger_has_stepped_back = true;

					RecoverLastGood();
					free(codeline[--window[CODE_WINDOW].count]);
					codeptr = code_history[history_last];
					dbnest = dbnest_history[history_last];

					/* Do whatever adjustments are necessary to
					   the stack
					*/
					m = MEM(codeptr);
					if (m==IF_T || m==ELSEIF_T || m==ELSE_T ||
						m==CASE_T || m==DO_T || m==WHILE_T)
					{
						if (--stack_depth < 0) stack_depth = 0;
					}

					goto ReturntoGame;
				}

			/* VIEW */
				case MENU_VIEW + VIEW_CALLS:
				case MENU_VIEW + VIEW_WATCH:
				case MENU_VIEW + VIEW_BREAKPOINTS:
				case MENU_VIEW + VIEW_LOCALS:
				case MENU_VIEW + VIEW_ALIASES:
                                {
					SwitchtoView(event.object-MENU_VIEW);
					break;
				}
				case MENU_VIEW + VIEW_HELP:
				case MENU_VIEW + VIEW_AUXILIARY:
				{
					if (event.object==MENU_VIEW + VIEW_HELP)
						debug_switchscreen(HELP);
					else debug_switchscreen(AUXILIARY);

					debug_cursor(1);
					debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);
					hugo_waitforkey();
					SwitchtoDebugger();
					break;
				}
				case MENU_VIEW + VIEW_OUTPUT:
					goto ToggleGameWindow;

			/* DEBUG */
				case MENU_DEBUG + DEBUG_SEARCH:
					{EnterSearch(); break;}
				case MENU_DEBUG + DEBUG_WATCH:
					{EnterWatch(); break;}
				case MENU_DEBUG + DEBUG_SET:
					{ModifyValue(); break;}
				case MENU_DEBUG + DEBUG_BREAKPOINT:
					{EnterBreakpoint(); break;}
				case MENU_DEBUG + DEBUG_OBJTREE:
					{DrawTree(); break;}
				case MENU_DEBUG + DEBUG_MOVEOBJ:
					{DebugMoveObj(); break;}

				case MENU_DEBUG + DEBUG_NESTING:
				{
					if (format_nesting)
					{
						format_nesting = false;
						menu[MENU_DEBUG/0x10-1][DEBUG_NESTING] = "&Format Nesting";
					}
					else
					{
						format_nesting = true;
						menu[MENU_DEBUG/0x10-1][DEBUG_NESTING] = "Un&format Nesting";
					}
					break;
				}

				case MENU_DEBUG + DEBUG_WARNINGS:
				{
					if (runtime_warnings)
					{
						runtime_warnings = false;
						menu[MENU_DEBUG/0x10-1][DEBUG_WARNINGS] = "&Runtime Warnings On";
					}
					else
					{
						runtime_warnings = true;
						runaway_counter = 0;
						menu[MENU_DEBUG/0x10-1][DEBUG_WARNINGS] = "&Runtime Warnings Off";
					}
					break;
				}
						

			/* TOOLS */
				case MENU_TOOLS + TOOLS_SETUP:
					{SetupMenu(); break;}

			/* HELP */
				case MENU_HELP + HELP_KEYS:
					{debug_shortcutkeys(); break;}
				case MENU_HELP + HELP_TOPIC:
					{EnterHelpTopic(); break;}
				case MENU_HELP + HELP_ABOUT:
					{About(); break;}


				default:
				{
					sprintf(debug_line, "Hugo Debugger v%d.%d%s", HEVERSION, HEREVISION, HEINTERIM);
					DebugMessageBox(debug_line, "Command not available");
					break;
				}
			}
			break;
		}
	}

	PrintScreenBorders();

	goto MainEventLoop;

ReturntoGame:

	SwitchtoGame();
	during_input = 0;
	return;
}


/* INITDEBUGGER */

/* Since Dict() changes dictcount */
int original_dictcount = 0;

void InitDebugger(void)
{
	int i;

	token[OBJECT_T] = "";
	token[XOBJECT_T] = "";

	if (original_dictcount==0) original_dictcount = dictcount;

	for (i=0; i<WINDOW_COUNT; i++)
		window[i].first = window[i].selected = window[i].count =
			window[i].horiz = 0;

	window[VIEW_LOCALS].count = MAXLOCALS;
	window[VIEW_ALIASES].count = aliases;

	LoadSetupFile();        /* preferences */
}

