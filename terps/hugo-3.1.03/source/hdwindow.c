/*
	HDWINDOW.C

	contains window/display routines:

		OpenMenu
			DisplayMenu
			PrintMenuItem
			PrintSelectedLine
		OpenMenubar
		PrintMenubar

		SwitchtoDebugger
		SwitchtoGame
		UpdateDebugScreen
			PrintLineandCaption
			PrintScreenBorders

		DrawBox
		InputBox
		DebugMessageBox
		SelectBox
			AllocateChoices

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman

	This is essentially a drop-in window/menu management system
	(in conjunction with HDUPDATE.C) that may adapted to an
	existing library for the OS/compiler in question.

	Note that all output to the debugging window(s) is assumed
	to be in a fixed-width font.  Note also these display functions
	are formatted for a screen width of at 80+ characters.
*/


#include "heheader.h"
#include "hdheader.h"


#define MAX_BOX_WIDTH	40

#define ENTER_KEY	13
#define ESCAPE_KEY      27

/* Function prototypes: */
void DisplayMenu(int m, int selection);
void DrawBox(int top, int bottom, int left, int right, int fcol, int bcol);
void DrawBoxwithCaptions(char *title, char *caption, int height, int width);
void PrintLineandCaption(int row, char *a, int highlight);
char PrintMenuItem(char *a, int letters, int highlight);
void PrintSelectedLine(int m, int s, int highlight);

void *saved_box;
void *saved_menu;

char debug_message_break = true;


/* ALLOCATECHOICES

	Allocates memory for n choices (actually title + n choices)
	before calling SelectBox.
*/

void AllocateChoices(int n)
{
	if ((choice = AllocMemory((size_t)(n+1)*sizeof(char *)))==NULL)
		DebuggerFatal(D_MEMORY_ERROR);
}


/* DEBUGMESSAGEBOX

	Draws a box with the given title and caption, and waits for
	a keypress.
*/

void DebugMessageBox(char *title, char *caption)
{
	int height, width;

	debug_cursor(1);

	width = (strlen(title)>strlen(caption))?strlen(title):strlen(caption);
	width+=4;

	/* height is: title + 1 + caption + 1 + prompt */
	height = 5;

	DrawBoxwithCaptions(title, caption, height, width);

	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);
	debug_settextpos(D_SCREENWIDTH/2-1, D_SCREENHEIGHT/2+3);
	debug_print("[OK]");

	event.action = 0;
	while ((event.action!=SELECT || event.object!=0) && event.action!=CANCEL)
        	debug_getevent();

	debug_windowrestore(saved_box, D_SCREENWIDTH/2-width/2, D_SCREENHEIGHT/2-height/2);

	debug_cursor(0);

	debugger_interrupt = true;
}


/* DISPLAYMENU

	The menu argument is not a MENU_CONSTANT, but an integer
	counting from 0.
*/

void DisplayMenu(int m, int selection)
{
        int i = 1, count = 0;
	unsigned len = 16;

	debug_windowbottomrow("");	/* erase current caption */

	/* Determine the number of menu items, and the length of the
	   longest item.
	*/
	while (menu[m][i][0]!='\0')
	{
		count++;
		if (strlen(menu[m][i])-1>len) len = strlen(menu[m][i])-1;
		i++;
	}

	menu_data[m].items = count;
	menu_data[m].longest = len;

	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);

	/* Save the existing screen text from:
		row 2 (top border) to
		row 2 + count + 1 (bottom border) + 1 (bottom shadow)
	   and:
	   	column position - 2 (left border) to
		column position - 2 + len + 4 (borders) + 2 (right shadow)
	*/
	saved_menu = debug_windowsave(menu_data[m].position-2, 2, 
		menu_data[m].position+len+3, count+4);

	/* Print top line of menu */
	debug_settextpos(menu_data[m].position-2, 2);
	debug_line[0] = TOP_LEFT;
	memset(debug_line+1, HORIZONTAL_LINE, len+2);
	sprintf(debug_line+len+3, "%c", TOP_RIGHT);
	debug_print(debug_line);

	/* Print each line of menu */
	for (i=1; i<=count; i++)
	{
		debug_settextpos(menu_data[m].position-2, i+2);

		if (menu[m][i][0]=='-')		/* separator */
		{
			debug_line[0] = HORIZONTAL_LEFT;
			memset(debug_line+1, HORIZONTAL_LINE, len+2);
			sprintf(debug_line+len+3, "%c", HORIZONTAL_RIGHT);
			debug_print(debug_line);
			menu_shortcut_keys[i] = 0;
		}
		else
		{
			PrintSelectedLine(m, i, (selection==i));
		}
	}

	/* Print bottom line of menu */
	debug_settextpos(menu_data[m].position-2, 3+menu_data[m].items);
	debug_line[0] = BOTTOM_LEFT;
	memset(debug_line+1, HORIZONTAL_LINE, len+2);
	sprintf(debug_line+len+3, "%c", BOTTOM_RIGHT);
	debug_print(debug_line);
}


/* DRAWBOX

	Draws a basic framed rectangle in the given color from (left, top)
	to (right, bottom).  (Note:  the screen area must be saved
	beforehand if it's going to be restored.)
*/

void DrawBox(int top, int bottom, int left, int right, int fcol, int bcol)
{
	char l[255];
	int i;

	debug_settextcolor(fcol);
	debug_setbackcolor(bcol);

	/* top */
	debug_settextpos(left, top);
	l[0] = TOP_LEFT;
	memset(l+1, HORIZONTAL_LINE, right-left);
	sprintf(l+right-left, "%c", TOP_RIGHT);
	debug_print(l);

	/* sides */
	for (i=top+1; i<bottom; i++)
	{
		debug_settextpos(left, i);
		l[0] = VERTICAL_LINE;
		memset(l+1, ' ', right-left);
		sprintf(l+right-left, "%c", VERTICAL_LINE);
		debug_print(l);
	}

	/* bottom */
	debug_settextpos(left, bottom);
	l[0] = BOTTOM_LEFT;
	memset(l+1, HORIZONTAL_LINE, right-left);
	sprintf(l+right-left, "%c", BOTTOM_RIGHT);
	debug_print(l);
}


/* DRAWBOXWITHCAPTIONS */

void DrawBoxwithCaptions(char *title, char *caption, int height, int width)
{
	int x, y;

	x = D_SCREENWIDTH/2 - width/2;
	y = D_SCREENHEIGHT/2 - height/2;

	/* Saved box size includes borders and shadow */
	saved_box = debug_windowsave(x, y, x+width+3, y+height+2);

	/* Drawn box size includes borders */
	DrawBox(y, y+height+1, x, x+width+1, color[MENU_TEXT], color[MENU_BACK]);

	debug_settextpos(Center(caption)+1, y+3);
	debug_print(caption);

	debug_settextcolor(color[SELECT_TEXT]);
	debug_setbackcolor(color[SELECT_BACK]);

        debug_settextpos(Center(title)+1, y+1);
	debug_print(title);
}


/* INPUTBOX

	Draws a box with the given title and caption, accepts an
	input of maxlen characters, and returns the input string.
	(The def argument gives the default string.)
*/

char *InputBox(char *title, char *caption, int maxlen, char *def)
{
	char *a;
	int height, width;

	debug_cursor(1);

	if (maxlen>D_SCREENWIDTH-12) maxlen = D_SCREENWIDTH-12;

	width = (strlen(title)>strlen(caption))?strlen(title):strlen(caption);
	width = (width>maxlen)?width:maxlen;
	width+=4;

	if ((int)strlen(caption)>width) caption[width] = '\0';
	if ((int)strlen(title)>width) title[width] = '\0';

	/* height = title + 1 + caption + 1 + prompt */
	height = 5;

	DrawBoxwithCaptions(title, caption, height, width);

	debug_settextcolor(color[MENU_SELECT]);
	debug_setbackcolor(color[MENU_SELECTBACK]);
	a = debug_input(D_SCREENWIDTH/2-maxlen/2+1, D_SCREENHEIGHT/2+3, maxlen, def);

	debug_windowrestore(saved_box, D_SCREENWIDTH/2-width/2, D_SCREENHEIGHT/2-height/2);

	debug_cursor(0);

	return a;
}


/* OPENMENU

	Takes a MENU_CONSTANT as an argument.  Sets the object of a SELECT
	event.action to MENU_CONSTANT + MENU_SUBHEADING.

	(Note:  Here, the given	MENU_CONSTANT is converted to an integer
	counting from 0, i.e., 0x10 becomes 0, 0x20 becomes 1, etc.,
	before calling DisplayMenu.)
*/

void OpenMenu(int m)
{
	int i;
	int selection = 1;

	/* deactivate menubar */
	menubar_isactive = false;
	active_menu_heading = m;
	PrintMenubar();

	m = m/0x10 - 1;

ReprintMenu:

	/* Highlight this menu's menubar heading w/no letter: */
	debug_settextpos(menu_data[m].position, 1);
	PrintMenuItem(menu[m][0], false, true);

	DisplayMenu(m, selection);

	while (true)		/* endless event loop */
	{
		debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);

		debug_getevent();

		switch (event.action)
		{
			case SELECT:
			{
				/* If a function key was used to select */
				if (event.object!=0)
					goto CloseThisMenu;
SelectThisItem:
				event.action = SELECT;
				event.object = active_menu_heading + selection;
			}
			case CANCEL:
			{
				goto CloseThisMenu;
			}
			case MOVE:
			{
				/* Erase the old highlight */
				debug_settextpos(menu_data[m].position-2, selection+2);
				PrintSelectedLine(m, selection, false);

				switch (event.object)
				{
					case UP:
					{
						/* if previous item is a separator */
	                                        if (menu[m][--selection][0]=='-')
							selection--;

						if (selection<=0)
							selection = menu_data[m].items;

						break;
					}

					case DOWN:
					{
						/* If next item is a separator */
						if (menu[m][++selection][0]=='-')
							selection++;

						if (selection>menu_data[m].items)
							selection = 1;

						break;
					}
					case RIGHT:
					case LEFT:
					{
						/* Unhighlight menubar heading */
						debug_settextpos(menu_data[m].position, 1);
						PrintMenuItem(menu[m][0], false, false);

						debug_windowrestore(saved_menu, menu_data[m].position-2, 2);

						switch (event.object)
						{
							case RIGHT:
							{
								if (++m > (MENU_HEADINGS-1))
									m = 0;
								break;
							}
							case LEFT:
							{
								if (--m < 0)
									m = MENU_HEADINGS-1;
								break;
							}
						}
						active_menu_heading = (m+1)*0x10;
						selection = 1;

						goto ReprintMenu;
					}
				}

				/* Highlight the new selection */
				debug_settextpos(menu_data[m].position-2, selection+2);
				PrintSelectedLine(m, selection, true);

				break;
			}
			case KEYPRESS:
			case SHORTCUT_KEY:
			{
				for (i=1; i<=menu_data[m].items; i++)
				{
					/* check if a shortcut key was pressed */
					if (event.object==menu_shortcut_keys[i])
					{
						selection = i;
						goto SelectThisItem;
					}
				}
				break;
			}
		}
	}

CloseThisMenu:

	debug_windowrestore(saved_menu, menu_data[m].position-2, 2);
	menubar_isactive = false;
        active_menu_heading = MENU_INACTIVE;
	PrintMenubar();

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
}


/* OPENMENUBAR

	Sets the object of a SELECT event.action to MENU_CONSTANT + 
	MENU_SUBHEADING.
*/

void OpenMenubar(void)
{
	int i, m = 0;

	/* Erase the current caption */
	debug_windowbottomrow("");

ReprintMenubar:
	active_menu_heading = (m+1)*0x10;
	menubar_isactive = true;
	PrintMenubar();

	while (true)
	{
		debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);

		context_help = "help";

		debug_getevent();

		switch (event.action)
		{
			case CANCEL:
			{
CancelMenubar:
				menubar_isactive = false;
				active_menu_heading = MENU_INACTIVE;
				PrintMenubar();
				return;
			}
			case MOVE:
			{
				if (event.object==LEFT || event.object==RIGHT)
				{
					if (event.object==LEFT)
					{
						if (--m < 0)
							m = MENU_HEADINGS-1;
					}
					if (event.object==RIGHT)
					{
						if (++m >= MENU_HEADINGS)
							m = 0;
					}
					goto ReprintMenubar;
				}
				if (event.object==DOWN)
				{
					goto OpenActiveMenuHeading;
				}
				break;
			}
			case SELECT:
			{
				/* If a function key was used to select, erase
				   the menubar (borrowing Cancel's code), and
				   return.
				*/
				if (event.object!=0)
					goto CancelMenubar;
OpenActiveMenuHeading:
				OpenMenu(active_menu_heading);
				return;
			}
			case KEYPRESS:
			case SHORTCUT_KEY:
			{
				for (i=0; i<MENU_HEADINGS; i++)
				{
					/* Check if a shortcut key was pressed */
					if (event.object==menu_data[i].shortcut_key)
					{
						active_menu_heading = (i+1)*0x10;
						OpenMenu(active_menu_heading);
						return;
					}
				}
				break;
			}
		}
	}
}


/* PRINTLINEANDCAPTION

	Prints a horizontal line with the specified caption centered in
	the middle.
*/

void PrintLineandCaption(int row, char *a, int highlight)
{
	char *l;	/* since 'a' may be debug_line */
	l = strdup(a);
	
	debug_settextpos(2, row);

	memset(debug_line, HORIZONTAL_LINE, D_SCREENWIDTH/2-strlen(l)/2-1);
	debug_line[D_SCREENWIDTH/2-strlen(l)/2-1] = '\0';
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	debug_print(debug_line);

	if (highlight)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[SELECT_BACK]);
	}
	debug_print(l);

	memset(debug_line, HORIZONTAL_LINE, D_SCREENWIDTH/2-strlen(l)/2-1);
	debug_line[D_SCREENWIDTH/2-strlen(l)/2-1-strlen(l)%2] = '\0';
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
	debug_print(debug_line);
	
	free(l);
}


/* PRINTMENUBAR */

void PrintMenubar(void)
{
	int i, pos = 1;

	debug_settextpos(1, 1);

	for (i=0; i<MENU_HEADINGS; i++)
	{
		debug_settextcolor(color[MENU_TEXT]);
		debug_setbackcolor(color[MENU_BACK]);
		debug_print("  ");

		menu_data[i].position = pos+=2;
		menu_data[i].shortcut_key = PrintMenuItem(&menu[i][0][0], menubar_isactive, ((i+1)*0x10==active_menu_heading));
		pos+=strlen(menu[i][0])-1;
	}

	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);
#if !defined (USE_OTHER_MENUS)
/* If USE_OTHER_MENUS is #defined, "Press ALT [or whatever] for menu"
   isn't printed
*/
	sprintf(debug_line, "Hugo Debugger v%d.%d%s", HEVERSION, HEREVISION, HEINTERIM);
	while (pos++ <= D_SCREENWIDTH-strlen(debug_line)-1) debug_print(" ");
	debug_print(debug_line);
	pos+=strlen(debug_line)-1;
#endif
	while (pos++ <= D_SCREENWIDTH) debug_print(" ");

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);
}


/* PRINTMENUITEM

	If letters is true, the shortcut letter of the selection (preceded
	by '&') is highlighted.  If highlight is true, the whole selection
	is highlighted.

	Returns the uppercase shortcut key of the printed selection, if any.
*/

char PrintMenuItem(char *a, int letters, int highlight)
{
	char printed_letter, short_key = 0, h;
	char letter[2];
	int i;

	for (i=0; i<(int)strlen(a); i++)
	{
		debug_settextcolor(color[MENU_TEXT]);
		debug_setbackcolor(color[MENU_BACK]);
		printed_letter = false;
		if (a[i]=='&')
		{
			h = 0;
			if (letters)
			{
				debug_settextcolor(color[SELECT_TEXT]);
				debug_setbackcolor(color[SELECT_BACK]);
				printed_letter = true;
			}
			i++;
			short_key = (char)toupper(a[i]);
		}

		if (highlight && !printed_letter)
		{
			debug_settextcolor(color[MENU_SELECT]);
			debug_setbackcolor(color[MENU_SELECTBACK]);
		}

		letter[0] = a[i];
		letter[1] = '\0';
		debug_print(letter);
	}

	if (highlight)
	{
		debug_settextcolor(color[MENU_SELECT]);
		debug_setbackcolor(color[MENU_SELECTBACK]);
	}
	else
	{
		debug_settextcolor(color[MENU_TEXT]);
		debug_setbackcolor(color[MENU_BACK]);
	}

	return short_key;
}


/* PRINTSCREENBORDERS */

void PrintScreenBorders(void)
{
	char *a;
	int i, j;

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);

	/* print vertical lines */
	sprintf(debug_line, "%c", VERTICAL_LINE);
	for (i=3; i<D_SCREENHEIGHT-1; i++)
	{
		if (i==D_SEPARATOR) continue;

		debug_settextpos(1, i);
		debug_print(debug_line);
		debug_settextpos(D_SCREENWIDTH, i);
		debug_print(debug_line);
	}

	/* print corners */
	sprintf(debug_line, "%c", TOP_LEFT);
	debug_settextpos(1, 2);
	debug_print(debug_line);
	sprintf(debug_line, "%c", TOP_RIGHT);
	debug_settextpos(D_SCREENWIDTH, 2);
	debug_print(debug_line);
	sprintf(debug_line, "%c", HORIZONTAL_LEFT);
	debug_settextpos(1, D_SEPARATOR);
	debug_print(debug_line);
	sprintf(debug_line, "%c", HORIZONTAL_RIGHT);
	debug_settextpos(D_SCREENWIDTH, D_SEPARATOR);
	debug_print(debug_line);
	sprintf(debug_line, "%c", BOTTOM_LEFT);
	debug_settextpos(1, D_SCREENHEIGHT-1);
	debug_print(debug_line);
	sprintf(debug_line, "%c", BOTTOM_RIGHT);
	debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT-1);
	debug_print(debug_line);

	/* non-code window */
	strcpy(debug_line, menu[MENU_VIEW/0x10-1][active_view]);
	if (active_view==VIEW_LOCALS)
	{
		strcat(debug_line, " (as ");
		switch (local_view_type)
		{
			case VALUE_T:
				{a = "values)"; break;}
			case OBJECTNUM_T:
				{a = "objects)"; break;}
			case DICTENTRY_T:
				{a = "dictionary entries)"; break;}
			case ROUTINE_T:
				{a = "routine addresses)"; break;}
			case ARRAYDATA_T:
				{a = "array addresses)"; break;}
			case PROP_T:
				{a = "properties)"; break;}
			case ATTR_T:
				{a = "attributes)"; break;}
			default:
				{a = "UNDEFINED)"; break;}
		}
		strcat(debug_line, a);
	}
	for (i=0; i<(int)strlen(debug_line); i++)
	{
		if (debug_line[i]=='&')
		{
			for (j=i; j<(int)strlen(debug_line); j++)
				debug_line[j] = debug_line[j+1];
			goto ErasedAmpersand;
		}
	}
ErasedAmpersand:
	PrintLineandCaption(2, debug_line, (active_window!=CODE_WINDOW));

	/* Code window */
	sprintf(debug_line, "Current:  %s", RoutineName(currentroutine));
	PrintLineandCaption(D_SEPARATOR, debug_line, (active_window==CODE_WINDOW));

	/* bottom of Code window */
	PrintLineandCaption(D_SCREENHEIGHT-1, "", 0);

	debug_windowbottomrow("");	/* no caption yet */
}


/* PRINTSELECTEDLINE

	The menu argument (m) is given as a converted integer (counting
	from 0), not as a MENU_CONSTANT.
*/

void PrintSelectedLine(int m, int i, int highlight)
{
	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);

	sprintf(debug_line, "%c", VERTICAL_LINE);
	debug_print(debug_line);

	if (highlight)
	{
		debug_settextcolor(color[MENU_SELECT]);
		debug_setbackcolor(color[MENU_SELECTBACK]);
	}

	debug_print(" ");

	if (highlight) context_help = menu[m][i];

	menu_shortcut_keys[i] = PrintMenuItem(menu[m][i], true, highlight);
	memset(debug_line, ' ', menu_data[m].longest-strlen(menu[m][i])+3);
	debug_line[menu_data[m].longest-strlen(menu[m][i])+2] = '\0';
	debug_print(debug_line);

	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);
	sprintf(debug_line, "%c", VERTICAL_LINE);
	debug_print(debug_line);
}


/* SELECTBOX

	Returns the selected choice, or 0 if nothing is selected.
	AllocateChoices(n) must be called first for n choices,
	and the choice[] array must hold the choices (with 0 being
	the title).  The default choice is given by def.
*/

/* These two extern definitions, both from hdtools.c, are necessary
   in case the SelectBox is a color-selection box--in which case
   selecting_color is true:
*/
extern char selecting_color;
extern void PrintSampleText(int n, int y);

int SelectBox(int n, int def)
{
	char *a;
	int i, this, first = 1, width = 0, height, x, y, visible;

	for (i=0; i<=n; i++)
		if (width<(int)strlen(choice[i])) width = strlen(choice[i]);
	width+=2;

	/* Number of visible choices (max. 8) */
	visible = (n>8)?8:n;

	/* Center the starting position nicely */
	this = def;
	first = (first+visible-1>=this)?1:this-visible/2;
	if (first+visible-1>n) first = n-visible+1;

	/* height = title + 1 + caption + 2 + visible choices + 1 */
	height = 6 + visible;

	/* Make space for the "Sample Text" line, if this is a
	   color selection box
	*/
	if (selecting_color) height+=2;

	x = D_SCREENWIDTH/2 - width/2 - 1;
	y = D_SCREENHEIGHT/2 - height/2 - 1;

	/* Saved box size includes borders and shadow */
	saved_box = debug_windowsave(x, y, x+width+3, y+height+2);

	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);

	/* Drawn box size includes borders */
	DrawBox(y, y+height+1, x, x+width+1, color[MENU_TEXT], color[MENU_BACK]);

	/* Draw separator */
	debug_line[0] = HORIZONTAL_LEFT;
	memset(debug_line+1, HORIZONTAL_LINE, width);
	sprintf(debug_line+width+1, "%c", HORIZONTAL_RIGHT);
	debug_settextpos(x, y+4);
	debug_print(debug_line);

	debug_settextpos(x+2, y+3);
	debug_print("Select:");

	debug_settextcolor(color[SELECT_TEXT]);
	debug_setbackcolor(color[SELECT_BACK]);
        debug_settextpos(Center(choice[0]), y+1);	/* title */
	debug_print(choice[0]);

PrintChoices:

	/* Print "..." at top or bottom of list if some choices are
	   not visible.
	*/
	debug_settextcolor(color[MENU_TEXT]);
	debug_setbackcolor(color[MENU_BACK]);
	debug_settextpos(x+1, y+5);
	if (first > 1)
		debug_print(" ...");
	else debug_print("    ");
	debug_settextpos(x+1, y+6+visible);
	if (n>=first+visible)
		debug_print(" ...");
	else debug_print("    ");

	/* If this is a color selection box */
	if (selecting_color) PrintSampleText(this-1, y+height);

	for (i=0; i<visible; i++)
	{
		if (first+i > n) a = "";
		else a = choice[first+i];

		/* create line padded w/spaces */
		memset(debug_line, ' ', width);
		debug_line[width] = '\0';
                sprintf(debug_line, " %s", a);
		debug_line[strlen(debug_line)] = ' ';

		if (this==first+i)
		{
			debug_settextcolor(color[MENU_SELECT]);
			debug_setbackcolor(color[MENU_SELECTBACK]);
		}
		else
		{
			debug_settextcolor(color[MENU_TEXT]);
			debug_setbackcolor(color[MENU_BACK]);
		}
		debug_settextpos(x+1, y+6+i);
		debug_print(debug_line);
	}

	/* endless loop */
	while (true)
	{
		debug_settextpos(D_SCREENWIDTH, D_SCREENHEIGHT);

		debug_getevent();

		switch (event.action)
		{
			case SELECT:
			{
				goto ReturnChoice;
			}
			case CANCEL:
			{
				this = 0;
				goto ReturnChoice;
			}
			case MOVE:
			{
				switch (event.object)
				{
					case UP:
					{
						if (this==1) break;
						if (--this < first) first--;
						goto PrintChoices;
					}
                                        case DOWN:
					{
MoveDown:
						if (this==n) break;
						if (++this > first+visible-1) first++;
						goto PrintChoices;
					}
					case HOME:
					case START:
					{
						this = first = 1;
						goto PrintChoices;
					}
					case END:
					case FINISH:
					{
MovetoEnd:
						this = n-1;
						if ((first=n-visible)<1) first = 1;
						goto MoveDown;
					}
					case PAGEUP:
					{
						if (this>first)
							this = first;
						else
						{					
							first-=visible;
							if (first<1) first = 1;
							this = first;
						}
						goto PrintChoices;
					}
					case PAGEDOWN:
					{
						if (this==first+visible-1)
						{
							first+=visible;
							if (first+visible-1>n) first = n-visible+1;
							if (first<1) first = 1;
						}
						if ((this=first+visible-1)>n) goto MovetoEnd;
						goto PrintChoices;
					}
				}
				break;
			}
			case KEYPRESS:
			{
				for (i=this+1; i<=n; i++)
				{
					if (toupper(choice[i][0])==toupper(event.object))
					{
						if ((this = i) > first+visible-1)
							first = this-visible+1;
						goto PrintChoices;
					}
				}
				for (i=1; i<this; i++)
				{
					if (toupper(choice[i][0])==toupper(event.object))
					{
						if ((this = i) < first)
							first = i;
						goto PrintChoices;
					}
				}
			}
		}
	}

ReturnChoice:

	debug_windowrestore(saved_box, x, y);
	free(choice);

	return this;
}


int saved_currentline = 0;


/* SWITCHTODEBUGGER */

void SwitchtoDebugger(void)
{
	saved_currentline = currentline;

	debug_switchscreen(DEBUGGER);
	active_screen = DEBUGGER;

	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);

	menubar_isactive = false;
	active_menu_heading = MENU_INACTIVE;
}


/* SWITCHTOGAME */

void SwitchtoGame(void)
{
	currentline = saved_currentline;

	HighlightCurrent(0);
	debug_switchscreen(GAME);
	active_screen = GAME;
}
