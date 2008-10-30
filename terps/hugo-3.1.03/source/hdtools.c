/*
	HDTOOLS.C

	contains Tools menu routines:

		DebugMoveObj
		DrawTree
			DrawBranch
		LoadSetupFile
		SaveSetupFile
		SetupMenu
			SetupColors
				PrintSampleText
			SetupFilename

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "heheader.h"
#include "hdheader.h"


/* Function prototypes: */
void DrawBranch(int obj);
void PrintSampleText(int c, int y);
void SetupColors(void);
void SetupFilename(char *n, int a);


char selecting_color = false;
int test_text, test_back;
char *color_choice[19] = {"Color Choices", "[Save Current Colors]",
	"Normal Text", "Normal Background",
	"Selected Text", "Selected Background",
	"Menu Text", "Menu Background",
	"Menu Selection Text", "Menu Selection Back",
	"Breakpoint Back", "Current Line Back",
	"Objects", "Properties/Attributes", "Routines", "Text",
	"Tokens", "Values", "Variables"};

int color[17] = {DEFAULT_NORMAL_TEXT, DEFAULT_NORMAL_BACK,
	DEFAULT_SELECT_TEXT, DEFAULT_SELECT_BACK,
	DEFAULT_MENU_TEXT, DEFAULT_MENU_BACK,
	DEFAULT_MENU_SELECT, DEFAULT_MENU_SELECTBACK,
	DEFAULT_BREAKPOINT_BACK, DEFAULT_CURRENT_BACK,
	DEFAULT_OBJECT_TEXT, DEFAULT_PROPERTY_TEXT,
	DEFAULT_ROUTINE_TEXT, DEFAULT_STRING_TEXT,
	DEFAULT_TOKEN_TEXT, DEFAULT_VALUE_TEXT,
	DEFAULT_VARIABLE_TEXT};
int temp_color[17];


char setup_path[MAXPATH];
char last_object_tree[33] = "";
int tree_nest;                  /* the branching level of an object tree */
int tree_lines;                 /* the number of objects printed         */


/* DEBUGMOVEOBJ */

void DebugMoveObj(void)
{
	char *a, *t;
	int obj, to_obj;

	t = "Move Object";

	a = InputBox(t, "Object to move:", 33, "");

	if (!strcmp(a, "")) return;
	else if ((obj = ObjectNumber(a))==-1) goto NoSuchObject;

	sprintf(debug_line, "Move %s to new parent:", a);

	a = InputBox(t, debug_line, 33, "");

	if (!strcmp(a, "")) return;
	else if ((to_obj = ObjectNumber(a))==-1) goto NoSuchObject;

	MoveObj(obj, to_obj);
	return;

NoSuchObject:
	sprintf(debug_line, "No such object as \"%s\"", a);
	DebugMessageBox("Object Name", debug_line);
}


/* DRAWBRANCH

	Called by DrawTree(), below.
*/

/* ContinueBranch determines the appropriate type of branch line for an 
   object tree depending on whether the given branch continues onward from
   obj.  The current parameter is either 1 or 0, depending on whether the
   choice in question is regarding the current branch or the continuation
   of a previous branch (i.e., to the left).
*/

int total_leftcolumn_objects, current_leftcolumn_object;

char ContinueBranch(int obj, char current)
{
	if (Parent(obj)==0)
	{
		if (current_leftcolumn_object<total_leftcolumn_objects-1)
			return (char)(current ? HORIZONTAL_LEFT:VERTICAL_LINE);
		else if (current)
			return BOTTOM_LEFT;
		else
			return ' ';
	}
	else
	{
		if (Sibling(obj))
			return (char)(current ? HORIZONTAL_LEFT:VERTICAL_LINE);
		else
			return BOTTOM_LEFT;
	}
}


void DrawBranch(int obj)
{
	int i, j, lastobj;

#if !defined (NO_WINDOW_PROMPTS)
	if (++tree_lines%(D_SCREENHEIGHT-1)==0)
	{
		debug_windowbottomrow("Enter to continue, Escape to quit");

		do
			debug_getevent();
		while (!(event.action==SELECT && event.object==0) && event.action!=CANCEL);

		if (event.action==CANCEL) return;
	}
#endif

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);

	/* Scroll the window up a line if necessary */
	if (tree_lines>=D_SCREENHEIGHT-1)
	{
		debug_windowscroll(1, 1, D_SCREENWIDTH, D_SCREENHEIGHT-1, UP, 1);
		debug_settextpos(1, D_SCREENHEIGHT-1);
	}

	/* Determine what sort of branch we need to draw... */
	if (++tree_nest)
	{
		for (i=1; i<tree_nest; i++)
		{
			/* First, figure out what lastobj is referring to,
			   depending on how far we're nested into the tree
			*/
			lastobj = Parent(obj);
			for (j=1; j<tree_nest-i; j++)
				lastobj = Parent(lastobj);
				
			sprintf(debug_line, "%c   ", (Sibling(lastobj) || Parent(lastobj)==0)
						? ContinueBranch(lastobj, 0):' ');
			debug_print(debug_line);
		}

		sprintf(debug_line, "%c%c%c%c", ContinueBranch(obj, 1),
			HORIZONTAL_LINE, HORIZONTAL_LINE, HORIZONTAL_LINE);
	}
	else strcpy(debug_line, "");

	/* ...before printing the object name... */

	strcat(debug_line, objectname[obj]);
	debug_print(debug_line);
	debug_print("\n");

	/* ...and recurse into its children (if any) */
	for (i=Child(obj); i; i=Sibling(i))
		DrawBranch(i);

	tree_nest--;
}


/* DRAWTREE

	The root of the object-tree-drawing algorithm.  Here, the starting
	(parent) object is entered, and subsequent calls to DrawBranch()
	draw the tree.
*/

void DrawTree(void)
{
	char *a, *t;
	int i, obj;

	if (objects==0) return;

	t = "Object Tree";

	a = InputBox(t, "Draw tree starting from:", 33, last_object_tree);

	if (event.action==CANCEL) return;
	else if (!strcmp(a, "")) obj = 0;
	else if ((obj = ObjectNumber(a))==-1) goto NoSuchObject;
	
	/* To avoid mix-ups when switching "back" to the debugger
	   from the help screen when we're actually on the
	   auxiliary screen, just disable context help for now:
	*/
	in_help_mode = true;

	debug_switchscreen(AUXILIARY);
	currently_updating = VIEW_AUXILIARY;
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	debug_clearview(VIEW_AUXILIARY);


	/* Draw this object and all its children: */

	debug_settextpos(1, 1);
	sprintf(debug_line, "%s\n", objectname[obj]);
	debug_print(debug_line);

	tree_nest = 0;
	tree_lines = 0;
	
	total_leftcolumn_objects = 0;
	for (i=0; i<objects; i++)
		if (Parent(i)==obj) total_leftcolumn_objects++;
	current_leftcolumn_object = 0;
	
	/* The reason obj's children are searched this way instead of
	   using Child() is in case obj is 0--in which case the eldest
	   child and subsequent siblings are not explicitly defined.
	*/
	for (i=0; i<objects; i++)
	{
		if (i!=obj && Parent(i)==obj)
		{
			current_leftcolumn_object++;
			DrawBranch(i);
		}

		/* If listing was cancelled in DrawBranch() prompt */
#if !defined (NO_WINDOW_PROMPTS)
		if (event.action==CANCEL) goto LeaveDrawTree;
#endif
	}

#if !defined (NO_WINDOW_PROMPTS)
	debug_windowbottomrow("Finished - Press any key...");
	hugo_waitforkey();

LeaveDrawTree:
	debug_windowbottomrow("");
	debug_switchscreen(DEBUGGER);
#endif
	in_help_mode = false;
	strcpy(last_object_tree, objectname[obj]);
	return;

NoSuchObject:
	sprintf(debug_line, "No such object as \"%s\"", a);
	DebugMessageBox("Object Name", debug_line);
}


/* LOADSETUPFILE */

void LoadSetupFile(void)
{
	char drive[MAXDRIVE], dir[MAXDIR], fname[MAXFILENAME], ext[MAXEXT];
	int i;
	FILE *setupfile;

	hugo_splitpath(invocation_path, drive, dir, fname, ext);
	hugo_makepath(setup_path, drive, dir, SETUP_FILE, "");

	if ((setupfile = fopen(setup_path, "rb"))==NULL) goto UseDefaults;

	i = fgetc(setupfile);
	if (fgets(printer_name, i+1, setupfile)==NULL)
		goto LoadSetupError;

	for (i=0; i<VARIABLE_TEXT; i++)
	{
		color[i] = fgetc(setupfile);
		color[i] += fgetc(setupfile)*256;
		color[i] += fgetc(setupfile)*65536L;
		if (ferror(setupfile)) goto LoadSetupError;
	}

	if (!feof(setupfile))
	{
		format_nesting = (char)fgetc(setupfile);
		runtime_warnings = (char)fgetc(setupfile);
		if (ferror(setupfile)) goto LoadSetupError;
	}

	fclose(setupfile);              /* close successfully */
	return;

LoadSetupError:
	fclose(setupfile);
	sprintf(debug_line, "Unable to load \"%s\"", setup_path);
	DebugMessageBox("Setup File Error", debug_line);

UseDefaults:

	strcpy(printer_name, DEFAULT_PRINTER);

	color[NORMAL_TEXT] 	= 	DEFAULT_NORMAL_TEXT;
	color[NORMAL_BACK] 	= 	DEFAULT_NORMAL_BACK;
	color[SELECT_TEXT] 	= 	DEFAULT_SELECT_TEXT;
	color[SELECT_BACK] 	= 	DEFAULT_SELECT_BACK;
	color[MENU_TEXT] 	= 	DEFAULT_MENU_TEXT;
	color[MENU_BACK] 	=	DEFAULT_MENU_BACK;
	color[MENU_SELECT] 	=	DEFAULT_MENU_SELECT;
	color[MENU_SELECTBACK]	= 	DEFAULT_MENU_SELECTBACK;
	color[BREAKPOINT_BACK] 	=	DEFAULT_BREAKPOINT_BACK;
	color[CURRENT_BACK] 	=	DEFAULT_CURRENT_BACK;
	color[OBJECT_TEXT] 	=	DEFAULT_OBJECT_TEXT;
	color[PROPERTY_TEXT] 	=	DEFAULT_PROPERTY_TEXT;
	color[ROUTINE_TEXT] 	=	DEFAULT_ROUTINE_TEXT;
	color[STRING_TEXT] 	=	DEFAULT_STRING_TEXT;
	color[TOKEN_TEXT] 	=	DEFAULT_TOKEN_TEXT;
	color[VALUE_TEXT] 	=	DEFAULT_VALUE_TEXT;
	color[VARIABLE_TEXT] 	=	DEFAULT_VARIABLE_TEXT;
}


/* PRINTSAMPLETEXT

	Called by SelectBox() in hdwindow.c in order to print an
	example of the current text/back color combination.
*/

void PrintSampleText(int c, int y)
{
	if (test_text==-1)
	{
		debug_settextcolor(c);
		debug_setbackcolor(test_back);
	}
	else
	{
		debug_settextcolor(test_text);
		debug_setbackcolor(c);
	}
	debug_settextpos(D_SCREENWIDTH/2-6, y);         /* center it */
	debug_print(" Sample Text ");
}


/* SAVESETUPFILE */

void SaveSetupFile(void)
{
	int i;
	FILE *setupfile;

	if ((setupfile = fopen(setup_path, "wb"))==NULL) goto SaveSetupError;

	fputc(strlen(printer_name), setupfile);
	fputs(printer_name, setupfile);

	for (i=0; i<VARIABLE_TEXT; i++)
	{
		/* Color values may be up to 32 bits since the
		   Win32 build (for one) uses RGB values, so we need
		   to save 3 bytes for each:
		*/
		fputc((int)(color[i]%65536L)%256, setupfile);
		fputc((int)(color[i]%65536L)/256, setupfile);
		fputc((int)(color[i]/65536L), setupfile);
		if (ferror(setupfile)) goto CloseSetupFile;
	}

	fputc((int)format_nesting, setupfile);
	fputc((int)runtime_warnings, setupfile);

	fclose(setupfile);              /* close successfully */
	return;

CloseSetupFile:
	fclose(setupfile);              /* close (unsuccessfully) */

SaveSetupError:
	sprintf(debug_line, "Unable to save \"%s\"", setup_path);
	DebugMessageBox("Setup File Error", debug_line);

	return;
}


/* SETUPCOLORS */

void SetupColors(void)
{
	int c, i, n;

	char *colorname[16] = {"Black", "Blue", "Green", "Cyan", "Red",
		"Magenta", "Brown", "White",
		"Dark Gray", "Light Blue", "Light Green", "Light Cyan",
		"Light Red", "Light Magenta", "Yellow", "Bright White"};


	/* Save the existing color set, since changes may be discarded */
	for (i=0; i<=16; i++)
		temp_color[i] = color[i];

ColorSelectLoop:

	AllocateChoices(18);
	for (i=0; i<=18; i++)
		choice[i] = color_choice[i];

	/* Get a color to change: */
	switch (n = SelectBox(18, 1))
	{
		case 0:  return;
		case 1:  goto SaveColorSet;
		default:
		{
			/* Figure out the related text/back color for
			   printing the sample text:
			*/
			c = n - 2;
			if (c < BREAKPOINT_BACK)
			{
				if (c%2)        /* backgrounds */
				{
					test_text = temp_color[c-1];
					test_back = -1;
				}
				else            /* foregrounds */
				{
					test_text = -1;
					test_back = temp_color[c+1];
				}
			}
			else if (c > CURRENT_BACK)
			{
				test_text = -1;
				test_back = temp_color[NORMAL_BACK];
			}
			else
			{
				test_text = temp_color[SELECT_TEXT];
				test_back = -1;
			}


			/* Now get the new color choice... */

			AllocateChoices(16);
			choice[0] = color_choice[n];
			for (i=0; i<16; i++)
				choice[i+1] = colorname[i];

			/* ...with current color as default */

			selecting_color = true;
			c = SelectBox(16, temp_color[n-2]+1);
			selecting_color = false;

			if (c==0) break;        /* cancel */

			temp_color[n-2] = c-1;
		}
	}
	goto ColorSelectLoop;

SaveColorSet:
	for (i=0; i<=16; i++)
		color[i] = temp_color[i];
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	hugo_clearfullscreen();
	buffered_code_lines = FORCE_REDRAW;	/* force Code window redraw */
	UpdateDebugScreen();
}


/* SETUPFILENAME

	Set up an external path/device name.
*/

void SetupFilename(char *n, int f)
{
	char *a, *b, *c;

	if (f==1) a = "Printer";  /* only device for now */
	else a = "(undefined)";

	sprintf(debug_line, "%s Name", a);

	if (f==1) c = "Enter path or device name:";
	else c = "(undefined)";
		
	b = InputBox(a, c, 64, n);

	if (event.action==CANCEL) return;

	if (f==1) strcpy(printer_name, b);
}


/* SETUPMENU */

void SetupMenu(void)
{

SetupMenuLoop:

	AllocateChoices(3);

#if defined (NO_COLOR_SETUP)
	choice[0] = "Setup";
	choice[1] = "[Save Current Setup]";
	choice[2] = "Printer Path/Device name";

	switch (SelectBox(2, 1))
	{
		case 1:  SaveSetupFile();
		case 0:  return;

		case 2:  {SetupFilename(printer_name, 1); break;}
	}
	goto SetupMenuLoop;
#else
	choice[0] = "Setup";
	choice[1] = "[Save Current Setup]";
	choice[2] = "Colors";
	choice[3] = "Printer Path/Device name";

	switch (SelectBox(3, 1))
	{
		case 1:  SaveSetupFile();
		case 0:  return;

		case 2:  {SetupColors(); break;}
		case 3:  {SetupFilename(printer_name, 1); break;}
	}
	goto SetupMenuLoop;

#endif  /* defined (AMIGA) */
}

