/*
	HDDECODE.C

	contains line-decoding routines:

		AddLinetoCodeWindow
			AddAddress
			AddSymbol
			AddString
			DecodeLine
			ShiftCodeLines
		AddStringtoCodeWindow
		GetToken

		int_print
		int_strcpy
		int_strlen

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "heheader.h"
#include "hdheader.h"


#define WORD_VAL(x) (unsigned int)(arr[x]+arr[x+1]*256)

/* Function prototypes: */
void AddAddress(long a, int *l, int lpos, int coltype);
int AddSymbol(unsigned char *arr, long *addr, int *l, int lpos);


long this_codeptr;			/* for error recovery    */
long next_codeptr;			/* for skipping over     */
int **codeline;                         /* code window contents  */
int buffered_code_lines = 0;            /* waiting to be printed */

char format_nesting = 1;	/* for spacing in Code window */
char screen_line[256];


/* ADDLINETOCODEWINDOW

	Calls DecodeLine to decode the next instruction, and looks
	after storing it in the codeline array.
*/

void AddLinetoCodeWindow(long addr)
{
	int *decoded_line;
	struct window_structure *win;

	win = &window[CODE_WINDOW];

	while (win->count>=MAX_CODE_LINES)
	{
		ShiftCodeLines();
	}

	decoded_line = DecodeLine(addr);

	/* Allocate memory for the new line */
	while ((codeline[win->count]=malloc((size_t)(int_strlen(decoded_line)+1)*sizeof(int)))==NULL)
	{
		ShiftCodeLines();
	}

	int_strcpy(codeline[win->count], decoded_line);

	/* Only need to count buffered code lines to a point */
	if (++buffered_code_lines > FORCE_REDRAW) buffered_code_lines = FORCE_REDRAW;

        win->count++;

	/* Current code line--set it to the new line, and if it was more
	   than one full window height back of the new line, force a full-
	   window redraw
	*/
	if (win->selected < win->count - win->height) buffered_code_lines = FORCE_REDRAW;
	win->selected = win->count-1;
	win->horiz = 0;
}


/* ADDSTRINGTOCODEWINDOW */

void AddStringtoCodeWindow(char *a)
{
	struct window_structure *win;

	win = &window[CODE_WINDOW];

	if (win->count>=MAX_CODE_LINES)
	{
		ShiftCodeLines();
	}

	/* Allocate memory for the new line */
	while ((codeline[win->count]=malloc((size_t)(strlen(a)+1)*sizeof(int)))==NULL)
	{
		ShiftCodeLines();
	}

	AddString(a, codeline[win->count], 0, ROUTINE_TEXT);

        buffered_code_lines++;
        win->count++;

	/* Current code line adjustment--see AddLinetoCodeWindow() */
	if (win->selected < win->count - win->height) buffered_code_lines = 100;
	win->selected = win->count-1;
	win->horiz = 0;
}


/* ADDADDRESS

	Adds the supplied address to the line <l> at the given position,
	in the specified color type.
*/

void AddAddress(long a, int *l, int lpos, int coltype)
{
	char *s;
	int i;

	s = PrintHex(a);

	for (i=lpos; i<lpos+(int)strlen(s); i++)
	{
		l[i] = (unsigned int)(s[i-lpos]);
		l[i] |= (unsigned int)(coltype*256);  /* color-type byte */
	}
	l[lpos + strlen(s)] = '\0';
}


/* ADDSTRING

	Adds the supplied string to the line <l> at the given position,
	in the specified color type.
*/

void AddString(char *s, int *l, int lpos, int coltype)
{
	int i;

	for (i=0; i<(int)strlen(s); i++)
	{
		l[lpos+i] = (unsigned int)(s[i]);
		l[lpos+i] |= (unsigned int)(coltype*256);  /* color-type byte */
	}
	l[lpos + strlen(s)] = '\0';
}


/* ADDSYMBOL

	Adds the current symbol (taken from the supplied array, usually
	mem[]), to the line <l> at the given position, and returns the
	number of characters added.
*/

int AddSymbol(unsigned char *arr, long *addr, int *l, int lpos)
{
	char c[3];
	struct token_structure tok;

	switch (arr[*addr])
	{
		/* Special handling due to embedded skip information: */
		case IF_T:
		case ELSEIF_T:
		case WHILE_T:
		case CASE_T:
		case FOR_T:
		case ELSE_T:
		case DO_T:
		case JUMP_T:
		case READFILE_T:
		case WRITEFILE_T:
		{
			AddString((tok.token = token[arr[*addr]]), l, lpos, TOKEN_TEXT);
			if (arr[*addr]!=JUMP_T) tok.len = 3;
			else tok.len = 1;
			tok.following = ' ';
			break;
		}
		default:
		{
			tok = GetToken(arr, *addr, lpos);
			AddString(tok.token, l, lpos, tok.col);
		}
	}

	if (tok.following)
	{
		sprintf(c, "%c", tok.following);
		if (tok.following=='+' || tok.following=='-')
		{
			strcat(c, " ");
			tok.following = 2;
		}
		else
			tok.following = 1;
		AddString(c, l, lpos+strlen(tok.token), TOKEN_TEXT);
	}

	*addr+=tok.len;		/* Move to end of this token */

	/* Return length of added string */
	return strlen(tok.token) + tok.following;
}


/* DECODELINE

	The main line-decoding routine, decodes the chunk of code at the
	supplied address into a line of pseudo-source, and returns a
	pointer to the int array containing the line.  It's an int
	array since the low byte contains the character, and the high
	byte contains the color.
*/

int *DecodeLine(long addr)
{
	int b;
	static int l[1032];
	int lpos = 0;

	this_codeptr = addr;	/* needed for error recovery */

	AddAddress(addr, l, lpos, TOKEN_TEXT);
	AddString(":  ", l, lpos+6, TOKEN_TEXT);
	lpos += 9;

	/* Indent current line if this option is selected */
	if (format_nesting)
	{
		for (b=0; b<dbnest; b++)
		{
			AddString("  ", l, lpos, TOKEN_TEXT);
			lpos+=2;
		}
	}

	/* Based on what the first token in the line is, we can determine
	   what the rest of the line will look like:
	*/
	switch (MEM(addr))
	{
		case CLOSE_BRACE_T:
			break;

		/* Normal case:  lines with EOL at the end */
		case RETURN_T:
		case RUN_T:
		case PRINT_T:
		case COLOR_T:

		case PARENT_T:
		case CHILD_T:
		case SIBLING_T:
		case CHILDREN_T:
		case ELDER_T:
		case ELDEST_T:
		case YOUNGEST_T:

		case REMOVE_T:
		case MOVE_T:
		case LOCATE_T:
		case VAR_T:
		case OBJECTNUM_T:
		case VALUE_T:
		case WORD_T:
		case ARRAYDATA_T:
		case CALL_T:
		case ARRAY_T:
		case PRINTCHAR_T:
		case WRITEFILE_T:
		case READFILE_T:
		case WRITEVAL_T:
		case PICTURE_T:
		case MUSIC_T:
		case SOUND_T:
		case ADDCONTEXT_T:
		case VIDEO_T:

		case PLUS_T:
		case MINUS_T:

		case IF_T:
		case ELSEIF_T:
		case WHILE_T:
		case FOR_T:
		case CASE_T:
		{
PrinttoEnd:
			while (MEM(addr)!=EOL_T)
			{
				lpos += AddSymbol(mem, &addr, l, lpos);
			}
			addr++;		/* eol */
			break;
		}

		/* Special case:  "window" */
		case WINDOW_T:
		{
			if (game_version >= 24) goto PrinttoEnd;
			goto AddSingleSymbol;
		}

		/* Special case:  "jump <label>" */
		case JUMP_T:
		{
			lpos+=AddSymbol(mem, &addr, l, lpos);
			AddAddress((long)(PeekWord(addr)*address_scale), l, lpos, TOKEN_TEXT);
			addr+=3;
			break;
		}

		/* Special case:  "text" token */
		case TEXT_T:
		{
			if (MEM(addr+1)==TO_T) goto PrinttoEnd;
			goto AddSingleSymbol;
			break;
		}

		/* Special case:  printing from text bank */
		case TEXTDATA_T:
		{
			debug_line[0] = '\"';
			strncpy(debug_line+1, GetText((long)(MEM(addr+1)*65536+PeekWord(addr+2))), D_SCREENWIDTH-4-lpos);
			sprintf(debug_line+D_SCREENWIDTH-7-lpos, "...");
			debug_line[strlen(debug_line)+1] = '\0';
			debug_line[strlen(debug_line)] = '\"';
			addr+=4;
			if (MEM(addr)==SEMICOLON_T)
			{
				strcat(debug_line, ";");
				addr++;
			}
			else strcat(debug_line, " ");
			AddString(debug_line, l, lpos, STRING_TEXT);
			lpos+=strlen(debug_line);
			break;
		}

		/* Special case:  "Routine()" vs. "Routine().something = ..." */
		case ROUTINE_T:
		case STRING_T:
		case SYSTEM_T:
		{
			lpos+=AddSymbol(mem, &addr, l, lpos);

			if (token[MEM(addr)][0]=='(')
			{
				b = 0;		/* count brackets */
				do
				{
					if (token[MEM(addr)][0]=='(') b++;
					else if (token[MEM(addr)][0]==')') b--;
					lpos+=AddSymbol(mem, &addr, l, lpos);
				}
				while (b);
			}
			if (token[MEM(addr)][0]!='.' && strcmp(token[MEM(addr)], "is"))
		       		break;

			goto PrinttoEnd;
			break;
		}

		/* Anything else is a one-word line */
		default:
AddSingleSymbol:
			lpos += AddSymbol(mem, &addr, l, lpos);
	}

	next_codeptr = addr;	/* for skipping over this line */

	return l;
}


/* GETTOKEN

	Returns a token_structure (i.e., token, length, and color)
	based on the TOKEN_T value at the given position in the
	supplied array.
*/

struct token_structure GetToken(unsigned char *arr, long addr, int firsttoken)
{
	char *a;
	int this, next, thistoken;
	static int lasttoken;
	int following = ' ';
	int i, j;
	int tlen, col, n;
	static struct token_structure tok;

	/* If start of the line, there was no first token */
	if (firsttoken==1) lasttoken = 0;

	switch (arr[addr])
	{
		case PROP_T:
		{
			col = PROPERTY_TEXT;
			a = propertyname[arr[addr+1]];
			tlen = 2;
			break;
		}
		case ATTR_T:
		{
			col = PROPERTY_TEXT;
			a = attributename[arr[addr+1]];
			tlen = 2;
			break;
		}
		case VAR_T:
		{
			col = VARIABLE_TEXT;
			tlen = 2;

			/* If a global variable... */
			if ((n = arr[addr+1])<MAXGLOBALS)
				a = globalname[n];

			/* ...or a local */
			else a = localname[n-MAXGLOBALS];

			break;
		}
		case DICTENTRY_T:
		{
PrintDictWord:
			col = STRING_TEXT;
			sprintf(screen_line, "\"%s\"", GetWord(WORD_VAL(addr+1)));
			a = screen_line;
			tlen = 3;
			break;
		}
		case OBJECTNUM_T:
		{
			col = OBJECT_TEXT;
			a = objectname[WORD_VAL(addr+1)];
			tlen = 3;
			break;
		}
		case VALUE_T:
		{
			if (lasttoken==PRINT_T) goto PrintDictWord;
			col = VALUE_TEXT;
			itoa((short)WORD_VAL(addr+1), screen_line, 10);
			a = screen_line;
			tlen = 3;
			break;
		}
		case ROUTINE_T:
		{
			col = ROUTINE_TEXT;
			a = RoutineName(WORD_VAL(addr+1));
			tlen = 3;
			break;
		}
		case ARRAYDATA_T:
		{
			col = VARIABLE_TEXT;
			a = ArrayName(WORD_VAL(addr+1));
			tlen = 3;
			break;
		}
		case STRINGDATA_T:
		{
			int nlen;	/* Don't overflow screen_line */
			col = STRING_TEXT;
			n = arr[addr+1]+arr[addr+2]*256;
			nlen = n;
			if (nlen >= 251) nlen = 250;
			screen_line[0] = '\"';
			j = 1;
			for (i=0; i<nlen; i++)
				screen_line[j++] = (char)(arr[addr+3+i]-CHAR_TRANSLATION);
			if (nlen < n)
			{
				for (i=1; i<=3; i++)
					screen_line[j++] = '.';
			}
			screen_line[j] = '\"';
			screen_line[j+1] = '\0';
			tlen = n+3;
			a = screen_line;
			break;
		}

		default:
		{
			col = TOKEN_TEXT;
			if (arr[addr]==WORD_T && arr[addr+1]==OPEN_SQUARE_T)
				col = VARIABLE_TEXT;
			a = token[arr[addr]];
			tlen = 1;
		}
	}


	/* Adapted from PrintWords in hcmisc.c to adjust the spacing of
	   certain symbols/operators in a line: */

	/* Only worry if a single-character token is involved */
	if (((arr[addr]<=TOKENS && arr[addr+tlen]<=TOKENS)) &&
		(token[arr[addr]][1]=='\0'  || token[arr[addr+tlen]][1]=='\0'))
	{
		thistoken = arr[addr];
		this = token[thistoken][0];

		if (arr[addr+tlen] > TOKENS)
			next = 0;
		else
                        next = token[arr[addr+tlen]][0];

		switch (this)
		{
			case '(':
			case '[':
			case '#':
			case '.':
			case '\'':
			{
				following = 0;
				break;
			}
			case '&':
			case '-':
			{
				/* If the last token was an operator (i.e.,
				   a single character, then we must be
				   modifying an upcoming value:  either
				   "&value", or "-value", so no space.
				*/
				if (token[lasttoken][1]=='\0') following = 0;
				break;
			}
		}
		switch (next)
		{
			case '(':
			{
				if (thistoken==ROUTINE_T) following = 0;
				break;
			}
			case ')':
			case '[':
			case ']':
			case ',':
			case ';':
			case '.':
				following = 0;
		}

		/* Check if it's "+=", "-=", etc. */
		if ((next=='=') && (this=='+' || this=='-' || this=='/' || this=='*'))
			following = 0;

		if ((this=='+' || this=='-') && next==this)
		{
			/* Double it for "++" or "--" */
			following = this;
			tlen = 2;
		}
	}

	tok.token = a;
	tok.len = tlen;
	tok.col = col;
	tok.following = following;

	lasttoken = arr[addr];

	return tok;
}


/* SHIFTCODELINES

	Shifts the array of window information up, deleting the
	50 oldest lines.  Used by the AllocMem() function to
	cannibalize code lines for other purposes.
*/

void ShiftCodeLines(void)
{
	unsigned int i;

	/* Can only cannibalize as many lines as aren't visible */
	if (window[CODE_WINDOW].count<=(unsigned)window[CODE_WINDOW].height + 50)
		DebuggerFatal(D_MEMORY_ERROR);

	/* Delete 50 oldest lines */
	for (i=0; i<50; i++)
		free(codeline[i]);

	/* Shift lines up */
	for (i=50; i<window[CODE_WINDOW].count; i++)
		codeline[i-49] = codeline[i];

	/* Delete the oldest line, and add "..." to the start */
	if ((codeline[0]=malloc(4*sizeof(int)))==NULL)
		DebuggerFatal(D_MEMORY_ERROR);
	AddString("...", codeline[0], 0, ROUTINE_TEXT);

	window[CODE_WINDOW].count -= 49;
	window[CODE_WINDOW].first -= 49;
}


/* Additional functions for managing int array strings: */

/* int_print

	Assumes that the high byte contains the color type, and the low
	byte contains the ASCII character.  If the supplied color is
	not -1, it is used as the background color and the text color
	is set to color[SELECT_TEXT].  The printed string begins with
	a[pos], and prints a maximum of width characters.
*/

void int_print(int *a, int col, int pos, int width)
{
	int i, printed = 0;

	if (col!=-1)
	{
		debug_settextcolor(color[SELECT_TEXT]);
		debug_setbackcolor(col);
	}
	else
	{
		debug_setbackcolor(color[NORMAL_BACK]);
	}

	if (a[0]>0)
	{
		for (i=pos; i<int_strlen(a); i++)
		{
			sprintf(screen_line, "%c", a[i]&0x00FF);
			if (col==-1) debug_settextcolor(color[(a[i]&0xFF00)/256]);
			debug_print(screen_line);
			if (++printed==width) return;
		}
	}
	else printed = 0;

	if (col==-1)
	{
		debug_settextcolor(color[NORMAL_TEXT]);
		debug_setbackcolor(color[NORMAL_BACK]);
	}
	memset(screen_line, ' ', width-printed);
	screen_line[width-printed] = '\0';
	debug_print(screen_line);
}

void int_strcpy(int *a, int *b)
{
	int i = 0;

	while ((a[i] = b[i])!='\0') i++;
}

int int_strlen(int *a)
{
	int i = 0;

	while ((a[i++]&0xFF)!=0);

	return i-1;
}
