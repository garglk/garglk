/*
	HDVAL.C

	contains functions for watch expressions and value assignment:

		DeleteWatch
		EnterWatch
		EvalWatch
		IDWord
		ModifyValue
		ParseExpression
		ReturnWatchValue
		SetupWatchEval

	for the Debugger build of the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "heheader.h"
#include "hdheader.h"


/* Function prototypes: */
int IDWord(char *a);
int ParseExpression(char *expr);

struct watch_structure watch[MAXWATCHES];
char debug_eval = 0;			/* true if assignment/watch	*/
					/*   expression evaluation	*/
char debug_eval_error;			/* true if something is invalid	*/
unsigned int debug_workspace;		/* for watch expressions   */
unsigned int token_val = 0;

#define ReturnSymbol(sym)	return (last_symbol = sym)


/* DELETEWATCH */

void DeleteWatch(int w)
{
	unsigned int i;

	/* Free the memory allocated for the expression itself */
	free(watch[w].expr);

	for (i=w; i<window[VIEW_WATCH].count-1; i++)
		watch[i] = watch[i+1];

	window[VIEW_WATCH].count--;

	UpdateDebugScreen();
}


/* ENTERWATCH */

/* Used in case EvalWatch() gets called to update the Watch window
   while a new expression is being entered; otherwise the workspace
   will get overwritten:
*/
char entering_watch = 0;

void EnterWatch(void)
{
	char *t, *expr;
	int i, w, len, watchtype;
	int lastval;

	/* If no game has been loaded, there's no array table, and
	   hence no workspace.
	*/
	if (game==NULL) return;

	t = "Watch Expression";

	if ((w = window[VIEW_WATCH].count)==MAXWATCHES)
	{
		sprintf(debug_line, "Maximum of %d watch expressions", MAXWATCHES);
		DebugMessageBox(t, debug_line);
		return;
	}

	expr = InputBox(t, "Enter value or expression to watch:", D_SCREENWIDTH, "");
	if (!strcmp(expr, "")) return;

	/* Return if, for some reason, the expression is invalid */
	if (!(len = ParseExpression(expr))) return;

	/* Test it once to see if there's an error in the expression */
	lastval = EvalWatch();

	if (debug_eval_error)
	{
		DebugMessageBox(t, "Error in Expression");
		return;
	}

	entering_watch = true;

	watchtype = SelectWatchType();
	if (event.action==CANCEL)
	{
		entering_watch = false;
		return;
	}

	watch[w].len = len;
	watch[w].lastval = lastval;
	watch[w].watchas = watchtype;
	watch[w].isbreak = false;
	watch[w].illegal = false;

	/* Reserve room for the expression data */
	if ((watch[w].expr = AllocMemory((len+1)*sizeof(unsigned char)))==NULL)
		DebuggerFatal(D_MEMORY_ERROR);

	/* Copy the expression from the workspace to the watch structure */
	for (i=0; i<len; i++)
		watch[w].expr[i] = MEM((long)arraytable*16L + (long)debug_workspace + (long)i);

	entering_watch = false;  /* safe to use workspace now */

	window[VIEW_WATCH].changed = true;
	window[VIEW_WATCH].count++;

	UpdateDebugScreen();
}


/* EVALWATCH

	Requires that an expression be first loaded into the array-
	table workspace by calling SetupWatchEval().
*/

int EvalWatch(void)
{
	int result = 0;
	long temp_codeptr;

	if (entering_watch) return 0;	/* See EnterWatch() */

	temp_codeptr = codeptr;
	codeptr = (long)arraytable*16L + (long)debug_workspace;

	debug_eval = true;
	debug_eval_error = false;

	SetupExpr();
	result = EvalExpr(0);
	inexpr = 0;

	if (debug_eval_error) result = 0;

	codeptr = temp_codeptr;	
	debug_eval = false;

	return result;
}


/* IDWORD

	Heisted (largely) from the compiler's HCBUILD.C.  If the token
	is invalid, returns -1 and stores an appropriate error message
	in the line array.  Otherwise, it returns the token code, and
	stores the real value in z (if applicable).
*/

int IDWord(char *a)
{
	char b[17];
	static int last_symbol;
	int i;

	/* Token (from htokens.h) */
	for (i=0; i<=TOKENS; i++)
	{
		if (!strcmp(a, token[i]))
			ReturnSymbol((int)i);
	}

	/* Dictionary word */
	if (a[0]=='\"')
	{
		a = &a[1];
		a[strlen(a)-1] = '\0';
		if ((i = FindWord(a))!=UNKNOWN_WORD)
		{
			token_val = (unsigned int)i;
			ReturnSymbol(DICTENTRY_T);
		}
		
		sprintf(debug_line, "Not in dictionary:  \"%s\"", a);
		ReturnSymbol(-1);
	}

	/* A number, i.e., a numerical value */
	if (!strcmp(a, itoa(atoi(a), b, 10)))
		{token_val = (unsigned int)atoi(a);
		ReturnSymbol(VALUE_T);}

	/* Global variable */
	for (i=0; i<globals; i++)
	{
		if (!strcmp(a, globalname[i]))
			{token_val = i;
			ReturnSymbol(VAR_T);}
	}

	/* Local variable */
	for (i=0; i<current_locals; i++)
	{
		if (!strcmp(a, localname[i]))
			{token_val = i + MAXGLOBALS;
			ReturnSymbol(VAR_T);}
	}

	if (!strcmp(Left(a, 6), "local:"))
	{
		i = atoi(a+6);
		if (i<1 || i>MAXLOCALS)
		{
			sprintf(debug_line, "Local variable out of range:  %s", a);
			ReturnSymbol(-1);
		}
		else
		{
			token_val = i-1 + MAXGLOBALS;
			ReturnSymbol(VAR_T);
		}
	}

	/* Routine */
	a[0] = (char)toupper(a[0]);
	for (i=0; i<routines; i++)
	{
		if (!strcmp(a, routinename[i]))
		{
			if (last_symbol!=AMPERSAND_T)
			{
				sprintf(debug_line, "Routine call illegal in expression");
				ReturnSymbol(-1);
			}

			token_val = raddr[i];
			ReturnSymbol(ROUTINE_T);
		}
	}
	a[0] = (char)tolower(a[0]);

	/* Attribute */
	for (i=0; i<attributes; i++)
	{
		if (!strcmp(a, attributename[i]))
			{token_val = i;
			ReturnSymbol(ATTR_T);}
	}

	/* Property */
	for (i=0; i<properties; i++)
	{
		if (!strcmp(a, propertyname[i]))
			{token_val = i;
			ReturnSymbol(PROP_T);}
	}

	/* Object */
	for (i=0; i<objects; i++)
	{
		if (!strcmp(a, objectname[i]))
			{token_val = i;
			ReturnSymbol(OBJECTNUM_T);}
	}

	/* Array */
	for (i=0; i<arrays; i++)
	{
		if (!strcmp(a, arrayname[i]))
			{token_val = arrayaddr[i]/2;
			ReturnSymbol(ARRAYDATA_T);}
	}

	/* Alias */
	for (i=0; i<aliases; i++)
	{
		if (!strcmp(a, aliasname[i]))
		{
			/* Property alias */
			if (aliasof[i]>=MAXATTRIBUTES)
				{token_val = aliasof[i] - MAXATTRIBUTES;
				ReturnSymbol(PROP_T);}

			/* Attribute alias */
			token_val = aliasof[i];
			ReturnSymbol(ATTR_T);
		}
	}

	/* An ASCII character, e.g., 'A' */
	if (a[0]=='\'')
	{
		if ((a[2]!='\'' && a[1]!='\\') || (a[1]=='\\' && a[3]!='\''))
			{sprintf(debug_line, "Unknown ASCII value:  %s", a);
			ReturnSymbol(-1);}
		if (a[1]=='\\') token_val = toascii(a[2]);
		else token_val = toascii(a[1]);
		ReturnSymbol(VALUE_T);
	}

	/* If we get here, then no match was made */
	sprintf(debug_line, "Syntax error in expression:  %s", a);
	ReturnSymbol(-1);
}


/* MODIFYVALUE

	Changes a variable data type to a new value.
*/

void ModifyValue(void)
{
	char *t, *expr;
	long temp_codeptr;

	if (game==NULL) return;

	t = "Set Value";

	expr = InputBox(t, "Enter \"<variable> = <val.>\", \"<obj.> is <attribute>\", etc.:", D_SCREENWIDTH, "");
	if (!strcmp(expr, "")) return;

	/* Return if, for some reason, the expression is invalid */
	if (!ParseExpression(expr)) return;

	/* Now set codeptr to execute at the point in the array table
	   where we've just set up the assignment--practically speaking,
	   this is a temporary piece of code that is built here, run,
	   and forgotten about.
	*/
	temp_codeptr = codeptr;
	codeptr = (long)arraytable*16L + (long)debug_workspace;

	debug_eval = true;
	debug_eval_error = false;

	RunSet(-1);

	if (debug_eval_error)
	{
		DebugMessageBox(t, "Illegal assignment");
		debug_eval_error = false;
	}
	else
	{
		if (active_view==VIEW_LOCALS || active_view==VIEW_WATCH)
			window[active_view].changed = true;
		UpdateDebugScreen();
	}

	debug_eval = false;
	codeptr = temp_codeptr;
}


/* PARSEEXPRESSION

	Parses the expression in the given string, encoding it in the
	Debugger's array-table workspace.  Returns false if the expression
	is invalid, otherwise returns the length (in bytes) of the
	encoded expression.
*/

int ParseExpression(char *expr)
{
	char a, tempchar;
	char inquote, insquote;		/* quotes or single quotes */
	int i, size, tok, lasttok = 0;

	unsigned char *e;
	unsigned int m;


	/* Set up m as an absolute memory address in the e array,
	   which starts at debug_workspace--the reserved 256 bytes in
	   the array table.
	*/
	e = GETMEMADDR((long)(arraytable*16L + (long)debug_workspace));
	m = 0;

	/* Parse the string token-by-token: */

	for (i=0; i<(int)strlen(expr); i++)
	{
		/* Must be room for at least 3 more bytes (the largest any
		   expression element will be) + 1 for end-of-line.
		*/
		if (m > debug_workspace+251)
		{
			DebugMessageBox("Expression Error", "Debug workspace expression overflow");
			return false;
		}

		/* Eat up spaces between words/symbols */
		while (expr[i]==' ') i++;
		if (i >= (int)strlen(expr)) break;

		size = 0;       /* length (in characters) of this token */

		inquote = insquote = 0;

		/* All of these are valid token characters; keep
		   building the current word until a non-token
		   character is reached.
		*/
		do
		{
			a = expr[i+size];
			if (a=='\'') insquote = (char)(!insquote);
			else if (a=='\"') inquote = (char)(!inquote);

			if (!inquote && !insquote)
				expr[i+size] = (char)tolower(a);

			if (size+i > (int)strlen(expr))
			{
				strcpy(debug_line, "Missing closing quote");
				goto PrintExpressionError;
			}

			size++;
		}
		while ((a) && ((a>='0' && a<='9') || (a>='A' && a<='Z') ||
				(a>='a' && a<='z') ||
				a=='_' || a=='$' || a==':' ||
				a=='\'' || a=='\"' || inquote || insquote));

		if (size > 1) size--;

		/* Temporarily put a '\0' at the end of this token to
		   treat it as a discrete word
		*/
		tempchar = expr[i+size];
		expr[i+size] = '\0';

		tok = IDWord(expr+i);

		/* If IDWord() returns -1, the token is invalid,
		   and the line array holds the error message.
		*/
		if (tok==-1)
		{
PrintExpressionError:
			DebugMessageBox("Expression Error", debug_line);
			return false;
		}

		e[m] = (unsigned char)tok;

		/* If it's more than just a simple token, then write
		   the appropriate data with it
		*/
		switch (tok)
		{
			/* Two-byte data types: */
			case VAR_T:
			case PROP_T:
			case ATTR_T:
			{
				e[m+1] = (unsigned char)token_val;
				m+=2;
				break;
			}

			/* Three-byte data types: */
			case VALUE_T:
			case OBJECTNUM_T:
			case ROUTINE_T:
			case DICTENTRY_T:
			case ARRAYDATA_T:
			{
				e[m+1] = (unsigned char)(token_val%256);
				e[m+2] = (unsigned char)(token_val/256);
				m+=3;
				break;
			}

			case EQUALS_T:
			{
				/* Account for combinations of >=, <=, ~= */
				switch (lasttok)
				{
					case GREATER_T:
					  {e[--m] = GREATER_EQUAL_T; break;}
					case LESS_T:
					  {e[--m] = LESS_EQUAL_T; break;}
					case TILDE_T:
					  {e[--m] = NOT_EQUAL_T; break;}
				}

				/* Fall through to default m++ */
			}

			default:  m++;
		}

		/* Restore whatever '\0' took the place of, and move
		   the the position in the expression string to the
		   end of this token.
		*/
		expr[i+size] = tempchar;
		i+=size-1;
		lasttok = tok;
	}

	/* Once the expression string has been coded, write an eol byte */
	e[m++] = EOL_T;

	return (int)m;
}
	

/* RETURNWATCHVALUE

	Assumes that the watch_buffer array holds the watch expression
	itself; this simply adds the result.  ln is the array to which the
	value is added/written (i.e., typically watch_buffer)
*/

void ReturnWatchValue(char *ln, int val, int type)
{
	char *a;
	int i, len;

	switch (type)
	{
		case OBJECTNUM_T:
		{
			if (val<objects)
				strcat(ln, objectname[val]);
			break;
		}
		case PROP_T:
		{
			if (val<properties)
				strcat(ln, propertyname[val]);
			break;
		}
		case ATTR_T:
		{
			if (val<attributes)
				strcat(ln, attributename[val]);
			break;
		}
		case DICTENTRY_T:
		{
			a = GetWord(val);
			len = strlen(ln);
			ln[len] = '\"';
			for (i=0; (unsigned)i<strlen(a); i++)
			{
				if (i>30) break;
				ln[len+i+1] = a[i];
			}
			if (strlen(a)>30)
				sprintf(ln+len+30, "...\"");
			else
				sprintf(ln+len+i+1, "\"");
			break;
		}
		case ROUTINE_T:
		{
			a = RoutineName(val);

			/* if not "<Unknown>" */
			if (a[0]!='<')
				sprintf(ln+strlen(ln), "&%s", a);
			break;
		}
		case ARRAYDATA_T:
		{
			a = ArrayName(val);
			if (a[0]!='<')          /* if not unknown */
				strcat(ln, a);
			break;
		}
		default:
		{
			sprintf(ln+strlen(ln), "%d", (signed short)val);
		}
	}
}


/* SETUPWATCHEVAL

	Moves watch n into the array-table workspace for subsequent
	evaluation by EvalWatch().
*/

void SetupWatchEval(int n)
{
	int i;

	if (entering_watch) return;	/* See EnterWatch() */

	/* Read the expression into the array-table workspace */
	for (i=0; i<watch[n].len; i++)
		SETMEM((long)arraytable*16L + (long)debug_workspace + (long)i, watch[n].expr[i]);
}
