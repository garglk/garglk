/*
	HDUPDATE.C

	contains debugger display updating routines:

		ConverttoChar
		HardCopy
		HighlightCurrent
		Navigate
		PrintCodeLine
		PrintOtherLine
			PrintAliasLine
			PrintBlankLine
			PrintBreakpoint
			PrintCallLine
			PrintLocalLine
			PrintWatch
		SwitchtoView
		UpdateDebugScreen
			UpdateCodeWindow
				UpdateFullCodeWindow
			UpdateOtherWindow

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman

	(Note that these updating functions expect a window to be
	80+ columns wide and cursor-positionable via screen-coordinates
	assuming or approximating full-screen.)
*/


#include "heheader.h"
#include "hdheader.h"


/* Function prototypes: */
void ConverttoChar(int *i, char *c);
void PrintAliasLine(unsigned int n);
void PrintBlankLine(unsigned int l);
void PrintBreakpoint(unsigned int n);
void PrintCallLine(unsigned int n);
void PrintCodeLine(unsigned int n, int horizontal, int width);
void PrintLocalLine(unsigned int n);
void PrintOtherLine(int v, unsigned int i);
void PrintWatch(unsigned int n);
void UpdateOtherWindow(int v);


struct window_structure window[WINDOW_COUNT];

char hard_copy = false;         /* true when sending to printer */
FILE *printer;
char watch_buffer[MAXBUFFER];

int active_window;                      /* CODE_WINDOW or n	*/
int active_view;                        /* VIEW_CONSTANT	*/
int currently_updating = 0;		/* VIEW_CONSTANT	*/
int local_view_type;
int active_screen = INACTIVE_SCREEN;	/* DEBUGGER, GAME, etc.	*/


/* CONVERTTOCHAR

	Converts an int string to a char array.
*/

void ConverttoChar(int *i, char *c)
{
	int a;

	for (a=0; a<int_strlen(i); a++)
		c[a] = (char)(i[a]&0xFF);
	c[int_strlen(i)] = '\0';
}


/* HARDCOPY

	Sends the contents of the current window line-by-line
	to the printer using debug_hardcopy(), which is responsible
	for all system-specific interfacing.  Uses the regular line-
	printing routines.
*/

char printer_name[MAXPATH];	/* for printing hardcopy  */
int device_error;		/* i.e., a printing error */

void HardCopy(void)
{
	unsigned int i, j, m;
	unsigned int start, end;
	struct window_structure *win;

	win = &window[active_window];

	hard_copy = true;
	device_error = false;   

	if ((printer = fopen(printer_name, "wt"))==NULL)
		goto DeviceError;

	sprintf(debug_line, "\"%s\"\n", gamefile);
	debug_hardcopy(printer, debug_line);

	if (active_window==CODE_WINDOW)
	{
		start = win->first;
		end = win->first+win->height;
		if (end>=win->count) end = win->count-1;

		strcpy(debug_line, "Code Window:\n");
	}
	else
	{
		start = 0;
		end = win->count - 1;

		/* Copy the name of the active window */
		m = (MENU_VIEW-1)/0x10;
		j = 0;
		for (i=0; i<strlen(menu[m][active_view]); i++)
		{
			if (menu[m][active_view][i]!='&')
				debug_line[j++] = menu[m][active_view][i];
		}
		sprintf(debug_line+j, ":\n");
	}

	/* Print the name of the active window */
	debug_hardcopy(printer, debug_line);

	if (win->count==0) goto FinishPrinting;

	for (i=start; i<=end; i++)
	{
		switch (active_window)
		{
			case VIEW_WATCH:
			case VIEW_LOCALS:
			case VIEW_CALLS:
			case VIEW_BREAKPOINTS:
			case VIEW_ALIASES:
			  {PrintOtherLine(active_view, i); break;}
			case CODE_WINDOW:
			  {PrintCodeLine(i, 0, win->width); break;}
		}

		if (device_error) break;
	}

FinishPrinting:
	debug_hardcopy(printer, "");
	hard_copy = false;
	fclose(printer);

	if (!device_error) return;

DeviceError:
	sprintf(debug_line, "Unable to print to %s", printer_name);
	DebugMessageBox("Printing Error", debug_line);
	device_error = false;
}


/* HIGHLIGHTCURRENT

	Depending on the value of a, either draws (1) or erases (0)
	the highlight from the current line in the active window.
*/

void HighlightCurrent(int a)
{
	struct window_structure *win;
	unsigned int temp_selected;

	win = &window[active_window];   /* shorthand */

	/* Don't try to draw it if the window's empty... */
	if (!win->count) return;

	/* ...or if it's technically off the screen (which may happen if
	   the engine is forced back to the debugger by, e.g., a warning
	   message being printed)
	*/
	if (win->selected >= win->first+win->height) return;

	temp_selected = win->selected;

	debug_settextpos(2, (win->top - win->first + win->selected));

	/* If erasing current highlight, cheat the currently selected
	   line and restore it later.
	*/
	if (!a) win->selected = -1;
	
	switch (active_window)
	{
		case CODE_WINDOW:
			{PrintCodeLine(temp_selected, win->horiz, win->width); break;}
		case VIEW_CALLS:
		case VIEW_WATCH:
		case VIEW_LOCALS:
		case VIEW_BREAKPOINTS:
		case VIEW_ALIASES:
			{PrintOtherLine(active_window, temp_selected); break;}
	}

	win->changed = true;
	win->selected = temp_selected;
}


/* NAVIGATE

	The main window navigation routine called by the main event loop
	in Debugger().
*/

void Navigate(void)
{
	unsigned int new_selected, new_first;
	struct window_structure *win;

	win = &window[active_window];   /* shorthand */

	/* Don't bother if the window is empty */
	if (!win->count) return;

	/* Since the position may change */
	new_selected = win->selected;

	if (event.action==MOVE)
	{
		switch (event.object)
		{
			case UP:
			case DOWN:
			{
				if (event.object==UP && win->selected > 0)
				{
					if (--new_selected < win->first)
					{
						debug_windowscroll(2, win->top, 
							D_SCREENWIDTH-1, win->top+win->height-1, 
							DOWN, 1);
						win->first--;
					}
				}
				else if (event.object==DOWN && new_selected<win->count-1)
				{
					if (++new_selected > win->first+win->height-1)
					{
						debug_windowscroll(2, win->top, D_SCREENWIDTH-1, win->top+win->height-1, UP, 1);
						win->first++;
					}
				}

				if (new_selected!=win->selected)
				{
RepositionSelected:
					HighlightCurrent(0);
					win->selected = new_selected;
					HighlightCurrent(1);
				}
				break;
			}
			case START:
			{
				win->horiz = 0;
				new_selected = new_first = 0;
				goto RedrawAll;
			}
			case FINISH:
			{
				win->horiz = 0;
				new_first = win->count - win->height;
				if ((int)new_first < 0) new_first = 0;
				new_selected = win->count - 1;

RedrawAll:
				if (new_selected!=win->selected)
				{
					HighlightCurrent(0);
					win->selected = new_selected;
					win->first = new_first;
				
ForceRedrawAll:
					switch (active_window)
					{
						case CODE_WINDOW:
						{
							UpdateFullCodeWindow(win->first, win->horiz, win->width);
							break;
						}
						default:
						{
							UpdateOtherWindow(active_view);
							break;
						}
					}
				
					HighlightCurrent(1);
				}
				break;
			}
			case PAGEUP:
			{
				if (win->selected==(new_first=win->first))
				{
					new_first -= win->height;
					if ((int)new_first < 0) new_first = 0;
					new_selected = new_first;
					goto RedrawAll;
				}
				else
				{
					new_selected = new_first;
					goto RepositionSelected;
				}
			}
			case PAGEDOWN:
			{
				if (win->selected==(new_first=win->first) + win->height - 1)
				{
					new_first += win->height;
					if (new_first > win->count - win->height)
						new_first = win->count - win->height;
					if ((int)new_first < 0) new_first = 0;
					new_selected = new_first + win->height - 1;
					goto RedrawAll;
				}
				else
				{
					new_selected = new_first+win->height-1;
					if (new_selected > win->count - 1) new_selected = win->count - 1;
					goto RepositionSelected;
				}
			}
			case LEFT:
			case RIGHT:
			{
				new_selected = win->horiz;

				if (event.object==LEFT) win->horiz-=10;
				else if (event.object==RIGHT) win->horiz+=10;

CheckLeftRight:
				if (active_window==CODE_WINDOW)
				{
					if (win->horiz > (int)int_strlen(codeline[win->selected])-win->width)
						win->horiz = int_strlen(codeline[win->selected])-win->width;
					if (win->horiz < 0) win->horiz = 0;
				}
				else if (active_window==VIEW_WATCH)
				{
					if (win->horiz > (int)watch[win->selected].strlen-win->width)
						win->horiz = watch[win->selected].strlen-win->width;
					if (win->horiz < 0) win->horiz = 0;
				}

				/* If the horizontal position changed,
				   redraw the window.
				*/
				if ((unsigned)win->horiz!=new_selected) goto ForceRedrawAll;

				break;
			}
			case HOME:
			case END:
			{
				new_selected = win->horiz;

				/* Move to the start of the line */
				if (win->horiz!=0 && event.object==HOME)
					win->horiz = 0;

				/* Move to the end of the line */
				else if (event.object==END)
					if ((active_window==CODE_WINDOW && win->horiz < (int)int_strlen(codeline[win->selected])-win->width)
					|| (active_window==VIEW_WATCH && win->horiz < (int)watch[win->selected].strlen-win->width))

						/* Cheat a large enough value to force
						   the end-of-line-figuring mechanism
						   to trigger:
						*/
						win->horiz = 32767;

				/* Now move the line position if anything changed */
				if ((unsigned)win->horiz!=new_selected) goto CheckLeftRight;

				break;
			}
		}
	}

	else if (event.action==DELETE)
	{
		if (active_window==VIEW_BREAKPOINTS)
			DeleteBreakpoint(breakpoint[window[VIEW_BREAKPOINTS].selected].addr);
		if (active_window==VIEW_WATCH)
			DeleteWatch(win->selected);
	}

	else if (event.action==SELECT && event.object==0)
	{
		/* Toggle watch expression break on/off */
		if (active_window==VIEW_WATCH)
		{
			watch[win->selected].isbreak = (char)!watch[win->selected].isbreak;
			HighlightCurrent(1);
		}
	}
}


/* PRINTALIASLINE */

void PrintAliasLine(unsigned int i)
{
	int l[256], col, printflag;

	if (aliasof[i]<MAXATTRIBUTES)
	{
		AddString("  Attribute:  ", l, 0, TOKEN_TEXT);
		col = OBJECT_TEXT;
	}               
	else
	{
		AddString("  Property:   ", l, 0, TOKEN_TEXT);
		col = PROPERTY_TEXT;
	}

	if (i==window[VIEW_ALIASES].selected && active_window==VIEW_ALIASES)
		printflag = color[SELECT_BACK];
	else
		printflag = -1;

	AddString(aliasname[i], l, int_strlen(l), col);
	AddString(" alias of ", l, int_strlen(l), TOKEN_TEXT);
	AddString((aliasof[i]<MAXATTRIBUTES)?attributename[aliasof[i]]:propertyname[aliasof[i]-MAXATTRIBUTES], l, int_strlen(l), col);

	if (hard_copy)
	{
		ConverttoChar(l, debug_line);
		debug_hardcopy(printer, debug_line);
		return;
	}

	memset(screen_line, ' ', window[VIEW_ALIASES].width-2-int_strlen(l));
	screen_line[window[VIEW_ALIASES].width-int_strlen(l)] = '\0';
	AddString(screen_line, l, int_strlen(l), TOKEN_TEXT);

	int_print(l, printflag, 0, window[VIEW_ALIASES].width);
}


/* PRINTBLANKLINE */

void PrintBlankLine(unsigned int l)
{
	memset(screen_line, ' ', window[active_view].width);
	screen_line[window[active_view].width] = '\0';
	debug_settextpos(2, l+window[active_view].top);
	debug_settextcolor(color[NORMAL_TEXT]);
	debug_setbackcolor(color[NORMAL_BACK]);

	debug_print(screen_line);
}       


/* PRINTBREAKPOINT */

void PrintBreakpoint(unsigned int i)
{
	sprintf(screen_line, "  %6s:", PrintHex(breakpoint[i].addr));
	sprintf(screen_line+strlen(screen_line), "  in %s", ((breakpoint[i].in)[0]=='\0')?"(Unknown)":breakpoint[i].in);

	if (hard_copy)
	{
		debug_hardcopy(printer, screen_line);
		return;
	}

	memset(screen_line+strlen(screen_line), ' ', window[VIEW_BREAKPOINTS].width-strlen(screen_line));
	screen_line[window[VIEW_BREAKPOINTS].width] = '\0';

	if (i==window[VIEW_BREAKPOINTS].selected && active_window==VIEW_BREAKPOINTS)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[SELECT_BACK]);
	}
	else
	{
		debug_settextcolor(color[NORMAL_TEXT]);
		debug_setbackcolor(color[NORMAL_BACK]);
	}

	debug_print(screen_line);
}


/* PRINTCALLLINE */

void PrintCallLine(unsigned int i)
{
	char *r;
	int indent;

	indent = i-window[VIEW_CALLS].first;

	memset(screen_line, ' ', indent);
	sprintf(screen_line+indent, "%s", (r = RoutineName(call[i].addr)));
	
	/* If this isn't a property routine or an event, print either "()"
	   or "(...)" depending on whether any arguments were passed
	   when the routine was called.
	*/
	if (strchr(r, '.')==NULL && strstr(r, "vent ")==NULL)
		sprintf(screen_line+strlen(screen_line), "(%s)", (call[i].param)?"...":"");

	if (hard_copy)
	{
		debug_hardcopy(printer, screen_line);
		return;
	}

	memset(screen_line+strlen(screen_line), ' ', window[VIEW_BREAKPOINTS].width-strlen(screen_line));
	screen_line[window[VIEW_BREAKPOINTS].width] = '\0';

	if (i==window[VIEW_CALLS].selected && active_window==VIEW_CALLS)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[SELECT_BACK]);
	}
	else if (i==window[VIEW_CALLS].count-1)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[CURRENT_BACK]);
	}
	else
	{
		debug_settextcolor(color[NORMAL_TEXT]);
		debug_setbackcolor(color[NORMAL_BACK]);
	}

	debug_print(screen_line);
}


/* PRINTCODELINE */

void PrintCodeLine(unsigned int l, int horizontal, int width)
{
	int printflag;

	currently_updating = CODE_WINDOW;

	/* If there's no code loaded to print */
	if (game==NULL) return;

	if (l==window[CODE_WINDOW].selected && active_window==CODE_WINDOW)
		printflag = color[SELECT_BACK];
	else if (l==window[CODE_WINDOW].count-1)
		printflag = color[CURRENT_BACK];
	else if (IsBreakpoint(StealAddress(l-window[CODE_WINDOW].first)))
		printflag = color[BREAKPOINT_BACK];
	else
		printflag = -1;

	if (hard_copy)
	{
		ConverttoChar(codeline[l], debug_line);
		debug_hardcopy(printer, debug_line);
		return;
	}

	int_print(codeline[l], printflag, horizontal, width);
}


/* PRINTLOCALLINE */

void PrintLocalLine(unsigned int i)
{
	if (i==window[VIEW_LOCALS].selected && active_window==VIEW_LOCALS)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[SELECT_BACK]);
	}
	else
	{
		debug_settextcolor(color[NORMAL_TEXT]);
		debug_setbackcolor(color[NORMAL_BACK]);
	}

	/* In case the local variable name has been blanked, likely by
	   stepping backward
	*/
	sprintf(screen_line, "  %s : ", localname[i]);
	strcpy(debug_line, "");
	ReturnWatchValue(debug_line, var[MAXGLOBALS+i], local_view_type);
	strcat(screen_line, debug_line);

	if (hard_copy)
	{
		debug_hardcopy(printer, screen_line);
		return;
	}

	memset(screen_line+strlen(screen_line), ' ', window[VIEW_LOCALS].width-strlen(screen_line));
	screen_line[window[VIEW_LOCALS].width] = '\0';

	debug_print(screen_line);	
}


/* PRINTOTHERLINE */

void PrintOtherLine(int v, unsigned int i)
{
	currently_updating = v;

	switch (v)
	{
		case VIEW_CALLS:
			{PrintCallLine(i); break;}
		case VIEW_WATCH:
			{PrintWatch(i); break;}
		case VIEW_LOCALS:
			{PrintLocalLine(i); break;}
		case VIEW_BREAKPOINTS:
			{PrintBreakpoint(i); break;}
		case VIEW_ALIASES:
			{PrintAliasLine(i); break;}
	}
}


/* PRINTWATCH */

void PrintWatch(unsigned int w)
{
	char c[3];                      /* following character(s) */
	char startofline = true;
	int pos = 0;
	int val;
	struct token_structure tok;
	struct window_structure *win;

	win = &window[VIEW_WATCH];

	/* If the watch is set to break when true, prefix it
	   with a "B:"
	*/
	strcpy(watch_buffer, (watch[w].isbreak)?"B: ":"   ");

	/* Copy the watch expression to the array-table workspace and
	   evaluate it:
	*/
	SetupWatchEval(w);

	if (!watch[w].illegal)
		val = EvalWatch();
	else
	{
		val = 0;
		debug_eval_error = true;
	}

	/* Write the watch expression that has been set up in the array-
	   table workspace as a string representation in the watch_buffer
	   array:
	*/
	while (watch[w].expr[pos]!=EOL_T)
	{
		tok = GetToken(mem, (long)arraytable*16L + (long)debug_workspace + (long)pos, startofline);
		startofline = false;

		strcat(watch_buffer, tok.token);
		if (tok.following)
		{
			sprintf(c, "%c", tok.following);
			if (tok.following=='+' || tok.following=='-')
			{
				strcat(c, " ");
			}
			strcat(watch_buffer, c);
		}

		pos+=tok.len;
	}

	/* Once that's done, add the actual watch value */
	strcat(watch_buffer, ":  ");
	if (!debug_eval_error)
		ReturnWatchValue(watch_buffer, val, watch[w].watchas);
	else
	{
		strcat(watch_buffer, "<illegal value>");
		watch[w].illegal = true;
	}

	watch[w].strlen = strlen(watch_buffer);

	/* Determine the print color */
	if (w==win->selected && active_window==VIEW_WATCH)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[SELECT_BACK]);
	}
	else if (watch[w].isbreak)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(color[BREAKPOINT_BACK]);
	}
	else
	{
		debug_settextcolor(color[NORMAL_TEXT]);
		debug_setbackcolor(color[NORMAL_BACK]);
	}

	debug_settextpos(2, win->top+(w-win->first));

	if (hard_copy)
	{
		debug_hardcopy(printer, watch_buffer);
		return;
	}

	if (win->horiz<=(int)strlen(watch_buffer))
	{
		watch_buffer[win->horiz+win->width] = '\0';
		debug_print(watch_buffer+win->horiz);
		pos = strlen(watch_buffer+win->horiz);
	}
	else
		pos = 0;

	/* Fill the rest of the line, if necessary, with spaces */
	if (pos < win->width)
	{
		memset(watch_buffer, ' ', win->width);
		watch_buffer[win->width] = '\0';
		debug_print(watch_buffer+pos);
	}
}


/* SWITCHTOVIEW */

void SwitchtoView(int v)
{
	if (v==VIEW_LOCALS)
	{
		int vt;

		if ((vt = SelectWatchType())==0) return;
		local_view_type = vt;
		window[VIEW_LOCALS].changed = true;
	}
	else if (v==active_view)
		return;

	HighlightCurrent(0);

	if (window[v].selected > window[v].count)
		window[v].selected = window[v].count;
	if (window[v].selected > window[v].first+window[v].height-1)
		window[v].first = window[v].selected - window[v].height;
	if (window[v].first > window[v].count)
		window[v].first = window[v].count;

	active_view = v;
	if (active_window!=CODE_WINDOW) active_window = v;
	debug_clearview(v);

	UpdateDebugScreen();
}


/* UPDATECODEWINDOW

	Draws only the most recently buffered (i.e., unprinted) lines.
	A redraw of the entire code window can be forced by setting
	buffered_code_lines to a number greater than the window height,
	e.g., FORCE_REDRAW.
*/

void UpdateCodeWindow(void)
{
	unsigned int i, selected_line, pos, start, amount_to_scroll;
	struct window_structure *win;
	win = &window[CODE_WINDOW];     /* shorthand */

	currently_updating = CODE_WINDOW;

	if ((unsigned)buffered_code_lines > win->height)
	{
		start = (win->count > win->height)?win->count - win->height:0;

		window[CODE_WINDOW].first = start;
		UpdateFullCodeWindow(start, win->horiz, win->width);
		buffered_code_lines = 0;
		return;
	}
	else if (buffered_code_lines==0) return;

	/* The next line to be printed */
	selected_line = win->count - buffered_code_lines;

	/* Unhighlight the old last-line highlight, if any */
	if (selected_line > 0)
	{
		pos = win->top + win->count-1-buffered_code_lines;
		if (pos > win->top + win->height-1) pos = win->top + win->height-1;
		debug_settextpos(2, pos);
		PrintCodeLine(selected_line-1, win->horiz, win->width);
	}

	/* Scroll the existing window up if need be */
	if (win->count > win->first + win->height)
	{
		if (win->count - (win->first+win->height) < (unsigned)buffered_code_lines)
			amount_to_scroll = win->count - (win->first+win->height);
		else
			amount_to_scroll = buffered_code_lines;

		debug_windowscroll(2, win->top, 
			D_SCREENWIDTH-1, win->top+win->height-1, UP, amount_to_scroll);

		win->first += amount_to_scroll;
	}

	/* Now print the buffered lines */
	for (i=0; buffered_code_lines>0; i++, buffered_code_lines--)
	{
		pos = win->top + min(win->height, win->count) - buffered_code_lines;
		debug_settextpos(2, pos);
		PrintCodeLine(selected_line+i, win->horiz, win->width);
	}

	buffered_code_lines = 0;
}


/* UPDATEDEBUGSCREEN */

void UpdateDebugScreen(void)
{
	static int last_view;
	static unsigned int last_count, last_selected, last_first;
	struct window_structure *win;

	win = &window[active_view];     /* shorthand */

	PrintMenubar();
	PrintScreenBorders();

	UpdateCodeWindow();

	if (win->count<=win->selected) win->selected = win->count-1;

	switch (active_view)
	{
		case VIEW_LOCALS:
			win->changed = true;
		case VIEW_CALLS:
		case VIEW_WATCH:
		case VIEW_BREAKPOINTS:
		case VIEW_ALIASES:
		{
			if (!win->count) debug_clearview(active_view);

			if (win->changed || win->count!=last_count ||
				win->selected!=last_selected ||
				win->first!=last_first ||
				active_view!=last_view)
			{
				UpdateOtherWindow(active_view);
			}
			last_count = win->count;
			last_first = win->first;
			last_selected = win->selected;
			break;
		}
	}

	last_view = active_view;
	win->changed = false;

	HighlightCurrent(1);
}


/* UPDATEFULLCODEWINDOW */

void UpdateFullCodeWindow(unsigned int start, int horizontal, int width)
{
	unsigned int j;

	if (start>=window[CODE_WINDOW].count)
		return;

	currently_updating = CODE_WINDOW;

	debug_clearview(CODE_WINDOW);

	for (j=0; j<window[CODE_WINDOW].height; j++)
	{
		debug_settextpos(2, window[CODE_WINDOW].top+j);
		PrintCodeLine(j+start, horizontal, width);
		if (j+start >= window[CODE_WINDOW].count-1) break;
	}
}


/* UPDATEOTHERWINDOW */

void UpdateOtherWindow(int v)
{
	unsigned int i;
	struct window_structure *win;

	currently_updating = v;

	win = &window[v];

	if (v==VIEW_LOCALS && debugger_has_stepped_back)
	{
		debug_clearview(VIEW_LOCALS);
		debug_settextpos(2, win->top);
		debug_print("<Local variables unavailable after stepping backward>");
		return;
	}

	if (!win->count)
	{
		debug_clearview(v);
		return;
	}

	if (win->selected < win->first)
		win->selected = win->first;

	for (i=win->first; i<win->first+win->height && i<win->count; i++)
	{
		debug_settextpos(2, win->top+i-win->first);

		PrintOtherLine(v, i);
	}

	if (i-win->first < win->height)
		PrintBlankLine(i-win->first);

	for (;i<win->first+win->height; i++)
		PrintBlankLine(i-win->first);
}
