/*
	HCCODE.C

	contains BeginCodeBlock, Code..., and IDWord routines
	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


long token_val = 0;		/* from IDWord() */
/* Overrides IDWord()'s error-printing */
char silent_IDWord = false;

int nest = 0;                   /* for recursive calls             */
int arguments = 0;              /* for routines                    */
char hex[7];                    /* string for hex output           */


/* BEGINCODEBLOCK

	Begins a new block of code, whether or not it is actually
	enclosed in "{...}", since a single line following an 'if'
	or 'window', etc. may not be.
*/

void BeginCodeBlock(void)
{
	GetWords();

	if (word[1][0]=='{')
		DoBrace();
	else
	{
		if (words==MAXWORDS)
			{Printout("");
			PrintWords(1);
			FatalError(OVERFLOW_E, "buffer");}
		word[words+1] = "}";
		words++;
		word[words+1] = "";
	}
}


/* CODEDO */

void CodeDo(void)
{
	long loopptr, returnpos, tempptr, skipaddr;

	Expect(2, "NULL", "new line following:  do");

	WriteCode(DO_T, 1);
	returnpos = codeptr;
	WriteCode(0, 2);

	BeginCodeBlock();

	Boundary();

	loopptr = codeptr;
	nest++;
	full_buffer = 1;
	BuildCode(DO_T);

	if (word[1][0]=='}')
		{KillWord(1);
		if (words==0) GetWords();}
	else
		GetWords();

	Expect(1, "while", "closing while in do-while");

	CodeLine();
	skipaddr = codeptr;
	tempptr = codeptr;
	codeptr = returnpos;
	WriteCode((unsigned int)(skipaddr-returnpos), 2);
	codeptr = tempptr;
}


/* CODEFILEIO

   Codes both writefile and readfile constructions.
*/

void CodeFileIO(void)
{
	long skipaddr, returnpos;

	if (!strcmp(word[1], "writefile")) infileblock = WRITEFILE_T;
	else infileblock = READFILE_T;

	skipaddr = codeptr + 1;

	CodeLine();

	BeginCodeBlock();

	nest++;
	full_buffer = 1;
	BuildCode(WRITEFILE_T);
	Boundary();

	returnpos = codeptr;
	codeptr = skipaddr;
	WriteCode((unsigned int)(returnpos/address_scale), 2);
	codeptr = returnpos;

	infileblock = 0;		/* clear it when finished */
}


/* CODEFOR

	Translates a "for (<assign.>; <test>; <modifier>)" expression
	into:

		<assign>
	:Loop
		<test>
		...code block...
		<modifier>
		jump Loop

	(If necessary, also translates "for x in object" into a standard
	FOR loop first.)
*/

void CodeFor(void)
{
	char f[256];
	char inquote = 0, brackets = 0;
	int i, p;
	unsigned int a = 0;
	int initexpr = 0, endexpr = 0, modexpr = 0;
	long startpos, returnpos = 0, skipaddr;

	strcpy(f, "");

	if (words<4) goto ForError;

	/* First, translate 'for x in object', if applicable */
	if (word[2][0] != '(')
	{
		strcat(f, "for(");
		for (i=2; i<=words; i++)
		{
			if (!strcmp(word[i], "in") && !inquote && !brackets)
				break;
			if (word[i][0]=='(' || word[i][0]=='[') brackets++;
			if (word[i][0]==')' || word[i][0]==']') brackets--;
                        strcat(f, word[i]);
			strcat(f, " ");
		}
		if (i++==words || brackets)
			{strcpy(f, "");
			goto ForError;}
		p = i;			/* start of parent obj. expr. */
		strcat(f, "=child(");	/* build initial assignment */
		for (i=p; i<=words; i++)
			{strcat(f, word[i]);
			strcat(f, " ");}
		strcat(f, ");");
		for (i=2; i<p-1; i++)	/* build end expression */
			{strcat(f, word[i]);
			strcat(f, " ");}
		strcat(f, ";");
		for (i=2; i<p-1; i++)	/* build modifying expression */
			{strcat(f, word[i]);
			strcat(f, " ");}
		strcat(f, "=sibling(");
		for (i=2; i<p-1; i++)
			{strcat(f, word[i]);
			strcat(f, " ");}
		strcat(f, "))");	/* finish building expression */

		strcpy(buffer, f);
		strcpy(f, "");
		SeparateWords();
	}

	/* Build standard 'for (...; ...; ...)' expression */
	if (word[2][0] != '(' || word[words][0] != ')')
		goto ForError;

	for (i=1; i<=words; i++)
		{strcat(f, word[i]);
		strcat(f, " ");}
	strcpy(buffer, Left(f, strlen(f)-1));

	while (buffer[a] != '(' && !inquote)
	{
		if ((buffer[a]=='\"' || buffer[a]=='\'') && buffer[a-1]!='\\')
			inquote = (char)(inquote==0);
		if (++a > strlen(buffer)) goto ForError;
	}
	if (++a > strlen(buffer)) goto ForError;

	initexpr = a;                      /* mark initial expr. */

	while (buffer[a] != ';' || inquote)
	{
		if ((buffer[a]=='\"' || buffer[a]=='\'') && buffer[a-1]!='\\')
			inquote = (char)(inquote==0);
		if (++a > strlen(buffer)) goto ForError;
	}
	if (++a > strlen(buffer)) goto ForError;

	endexpr = a;                       /* mark end expr. */

	while (buffer[a] != ';' || inquote)
	{
		if ((buffer[a]=='\"' || buffer[a]=='\'') && buffer[a-1]!='\\')
			inquote = (char)(inquote==0);
		if (++a > strlen(buffer)) goto ForError;
	}
	if (++a > strlen(buffer)) goto ForError;

	modexpr = a;                       /* mark modifying expr. */

	if (initexpr < endexpr - 2)        /* build initial expr. */
	{
		strcpy(buffer, "");
		inquote = 0;
		for (i=initexpr; i<endexpr-1; i++)
		{
			if (f[i]==',' && !inquote)
			{
				buffer[i-initexpr] = '\0';
				SeparateWords();
				CodeLine();
				initexpr = i+1;
			}
			else
			{
                                buffer[i-initexpr] = f[i];
				if ((f[i]=='\"' || f[i]=='\'') && f[i-1]!='\\')
					inquote = (char)(inquote==0);
			}
		}
		buffer[i-initexpr-1] = '\0';
		SeparateWords();
		CodeLine();
	}

	Boundary();
	startpos = codeptr;

	strcpy(buffer, "for");

	if (endexpr < modexpr - 2)         /* build end expr. */
	{
		a = 3;
		inquote = 0;
		for (i=endexpr; i<modexpr-1; i++)
		{
			if (f[i]==',' && !inquote)
				{strcat(buffer, " and ");
				a+=4;}
			else
			{
                                buffer[i-endexpr+a] = f[i];
				if ((f[i]=='\"' || f[i]=='\'') && f[i-1]!='\\')
					inquote = (char)(inquote==0);
			}
		}
		buffer[i-endexpr+2] = '\0';
	}
	else
		strcat(buffer, " 1");

	SeparateWords();
	returnpos = codeptr + 1;
	CodeLine();

	BeginCodeBlock();

	nest++;
	full_buffer = 1;
	BuildCode(FOR_T);

	if (f[modexpr+2] != ')')           /* build modifying expr. */
	{
		brackets = 0;
		for (i=modexpr; f[i] != ')' || brackets || inquote; i++)
		{
			buffer[i-modexpr] = f[i];
			if ((f[i]=='\"' || f[i]=='\'') && f[i-1]!='\\')
				inquote = (char)(inquote==0);
			else if (f[i]=='(' && !inquote)
				brackets++;
			else if (f[i]==')' && !inquote)
				brackets--;
			else if (f[i]==',' && !inquote && !brackets)
			{
				buffer[i-modexpr] = '\0';
				SeparateWords();
				CodeLine();
				modexpr = i+1;
			}
		}
		buffer[i-modexpr-1] = '\0';
		SeparateWords();
		CodeLine();
		full_buffer = 0;
	}

	skipaddr = codeptr;       	/* ...then write it once we know it */
	codeptr = returnpos;
	WriteCode((unsigned int)(skipaddr+3-returnpos), 2);
	codeptr = skipaddr;

	WriteCode(JUMP_T, 1);

	if (hlb) RememberAddr(codeptr, VALUE_T, (unsigned int)(startpos/address_scale));

	WriteCode((unsigned int)(startpos/address_scale), 2);
	full_buffer = 0;
	return;

ForError:
	if (strcmp(f, ""))
	{
		strcpy(buffer, f);
		SeparateWords();
	}
	Error("Illegal construction:  for");
}


/* CODEIF */

void CodeIf(void)
{
	long returnpos, skipaddr;

	returnpos = codeptr + 1;        /* put skip addr. here */
	CodeLine();

	BeginCodeBlock();

	nest++;
	full_buffer = 1;
	BuildCode(IF_T);

	skipaddr = codeptr;             /* ...then write it once we know it */
	codeptr = returnpos;
	WriteCode((unsigned int)(skipaddr-returnpos), 2);
	codeptr = skipaddr;
}


/* CODELINE

	Compiles the line currently loaded in word[].  A brief overview is:

		1.  Do any start-of-line analysis
		2.  Loop through the line word-by-word
			- Check current state/adjustments
			- Main section:  Code tokens appropriately,
			  setting up syntax flags
			- Throw up any current-word errors
		3.  Throw up any end-of-line errors
		4.  Do any end-of-line adjustments
*/

#define SOME_VALUE	(-1)

char halfline;			/* for ending with "and" or "or"  */

void CodeLine(void)
{
	int enternest;			/* initial nest level		  */
	char eol = 0, scolon = 0;       /* flags for end-of-line coding   */
	char assign = 0;                /* when '=' is required (or 'is') */
	char assignment = 0;		/* when an assignment is coded 	  */
	int evalue = 0;                 /* expecting a value              */
	char isneedsval = 0;		/* if 'is' needs a value	  */
	int invalid_is_val = 0;		/* if "is..." deserves a warning  */
	char iline = 0;                 /* illegal at start of line 	  */
	char eline = 0;                 /* illegal at end of line         */
	char tline = 0;                 /* text-only line 		  */
	char gline = 0;			/* a grammar-definition line 	  */
	int nline = 0;                  /* expecting newline before 	  */
	char glegal;			/* legal grammar token 		  */
	char pexpr = 0;			/* possible expression 		  */
	char expr = 0;     		/* if expression is allowed 	  */
	char brackets = 0; 		/* to check parentheses count 	  */
	char sbrackets = 0;		/* to check square brackets 	  */
	char subscript = 0;		/* to validate an array[] 	  */
	int oporsep = 0;                /* expecting operator, separator  */
	char paramlist = 0;             /* counting arguments 		  */
	char parambrackets = 0;		/* bracket level in arg. list 	  */
	char condexpr = 0;              /* true if conditional expression */
	char singleval = 0;		/* true if only one val allowed   */
	char multipleval = 0;		/* >0 if more than 1 param needed */
	char moveneedsto = 0;		/* true if 'move' needs 'to'      */

	/* i.e., if x = 1, 2, 3 */
	long compstart = 0;             /* start of initial comparison	  */
	long compend = 0;               /* where it ends		  */
	char compbrackets = 0;          /* nested level of '=' or '~='	  */
	int comptype = 0;		/* '=' or '~='			  */
	char definite_ambiguity = 0, possible_ambiguity = 0;

	int lastt = 0;			/* last token, or SOME_VALUE 	  */
	int nextt = 0;			/* counting non-values		  */
	int firstvalue = 0;             /* set to token # of value if
					     first token in line 	  */

	long verbskipaddr = 0, tempptr, skipptr;
	int i;
	int a, j, l, t;


	enternest = nest;
	halfline = false;

	/* If line starts with a label */
	if (word[1][0]=='~')
	{
		Boundary();
		laddr[labelctr] = (unsigned int)(codeptr/address_scale);
		labelctr++;
		KillWord(1);
		KillWord(1);
		WriteCode(LABEL_T, 1);
		WriteCode(nest-enternest, 1);
		if (word[1][0]=='\0') return;
	}

	for (i=1; i<=words; i++)
	{
		t = IDWord(i);

		/* If this is a continuation of a previous line, the
		   first word might get mistaken for printed text (i.e.,
		   from the text bank) instead of a dictionary entry.
		*/
		if (halfline)
			if (t==TEXTDATA_T) t = DICTENTRY_T;


		/* Also have to correct for 'print Routine("...")...'
		   where one of the arguments may be a dictionary entry
		   --although the "print" token caused IDWord to call it
		   an in-code string.
		*/
		if (brackets)
		{
			if (t==STRINGDATA_T)
			{
				t = DICTENTRY_T;
				token_val = AddDictionary(word[i]);
			}
		}

		eline = 0;
		glegal = 0;

		if (eol==1 && i==words && token[t][0]=='}')
			{WriteCode(EOL_T, 1);
			eol = 0;}

		/* For verb grammar definition lines: */
		if (i==1 && token[t][0]=='*')
			verbskipaddr = codeptr + 1;

		WriteCode(t, 1);

		switch(t)
		{
			case OPEN_BRACKET_T:
			{
				brackets++;
				iline = 1;
				eline = 1;
				nextt = 0;
				glegal = 1;
				lastt = 0;
				if (word[i+1][0]!=')') evalue = i;

				if (!parambrackets || brackets>parambrackets)
					compstart = codeptr;

				break;
			}

			case CLOSE_BRACKET_T:
			{
				brackets--;
				iline = 1;
				glegal = 1;
				lastt = SOME_VALUE;

				if (paramlist)
				{
					if (brackets<parambrackets)
					{
						parambrackets--;
						paramlist--;
					}
				}

				goto NextWord;
			}

			case OPEN_SQUARE_T:
			{
				if (!subscript)
					Error("Illegal subscript on non-array");
				else
					/* array assignment */
					if ((assign) && !assignment)
						firstvalue = t;

				iline = 1;
				eline = 1;

				/* If array[]--used to get the size of an
				   array...
				*/
				if (word[i+1][0]==']')
				{
					if ((assign) && !assignment)
						Error("Illegal assignment to array length");

					WriteCode(CLOSE_SQUARE_T, 1);
					i++;
					eline = 0;
					lastt = SOME_VALUE;
				}

				/* ...or normal array[n]. */
				else
				{
					sbrackets++;
					evalue = i;
					lastt = 0;
				}
				goto NextWord;
			}

			case CLOSE_SQUARE_T:
			{
				sbrackets--;
				iline = 1;
				lastt = SOME_VALUE;
				if (subscript) subscript--;
                                goto NextWord;
			}

			case DICTENTRY_T:
			{
				if (lastt==SOME_VALUE && !gline) oporsep = i;

				WriteCode((unsigned int)token_val, 2);
				iline = 1;
				lastt = SOME_VALUE;
				nextt = 0;
				glegal = 1;
				evalue = 0;

				if (!gline) goto NextWord;

				break;
			}

			case OBJECTNUM_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;

				WriteCode((unsigned int)token_val, 2);
				if (i==1 && !halfline)
					{eol = 1;
					assign = 1;}

				lastt = SOME_VALUE;
				nextt = 0;
				glegal = 1;
				evalue = 0;
				goto NextWord;
			}

			case PROP_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;
				
				else if (lastt==IS_T || lastt==NOT_T)
				{
					sprintf(line, "?Property \"%s\" used as attribute", word[i]);
					Error(line);
				}

				WriteCode((unsigned int)token_val, 1);
				iline = 1;
				nextt = 0;
                                evalue = 0;
				lastt = SOME_VALUE;
				goto NextWord;
			}

			case ATTR_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;

				else if (lastt==DECIMAL_T)
				{
					sprintf(line, "?Attribute \"%s\" used as property", word[i]);
					Error(line);
				}

				WriteCode((unsigned int)token_val, 1);
				iline = 1;
				nextt = 0;
                                glegal = 1;
				evalue = 0;
				lastt = SOME_VALUE;
				goto NextWord;
			}

			case RETURN_T:
				expr = 1;
			case PRINT_T:
				scolon = true;
			case PRINTCHAR_T:
			case WRITEVAL_T:
			case LOCATE_T:
			case COLOR_T:
			case COLOUR_T:
#if !defined (COMPILE_V25)
			case ADDCONTEXT_T:
#endif
			{
				eol = 1;
				if (t != RETURN_T) eline = 1;
				nline = i;
				if (t!=PRINT_T && t!=RETURN_T) evalue = i;
				break;
			}

			case ARRAY_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;

				if (i==1 && !halfline)
					{eol = 1;
					assign = 1;}
				nextt = 1;
				eline = 1;
				evalue = i;
				subscript++;
				break;
			}

			case CASE_T:
			case IF_T:
			case ELSEIF_T:
                        case WHILE_T:
                        case FOR_T:
			{
				WriteCode(0, 2);
				eol = 1;
				expr = 1;
				condexpr = true;
				compstart = codeptr;

				/* fall through */
			}
			case RUN_T:
			{
				eline = 1;
				nline = i;
				evalue = i;
				eol = true;
				if (t==RUN_T) singleval = true;
				break;
			}

			case STRINGDATA_T:
			{
				l = strlen(word[i]);
				WriteCode((unsigned int)l, 2);
				for (j=0; j<l; j++)
					WriteCode((unsigned int)word[i][j] + CHAR_TRANSLATION, 1);
				if (spellcheck) Printout(word[i]);
				break;
			}

			case TEXTDATA_T:
			{
				WriteCode((unsigned int)(token_val/65536), 1);
				WriteCode((unsigned int)(token_val%65536), 2);
				tline = true;
				scolon = true;	 /* trailing semicolon OK */
				break;
			}

			case VAR_T:
			{
				if (lastt==SOME_VALUE && !gline) oporsep = i;
				
				else if (token_val<ENGINE_GLOBALS &&
					(lastt==DECIMAL_T || lastt==IS_T))
				{
					sprintf(line, "?Improper use of predefined global \"%s\"", word[i]);
					Error(line);
				}

				WriteCode((unsigned int)token_val, 1);

				if (token_val>=MAXGLOBALS) unused[token_val-MAXGLOBALS] = 0;

				lastt = SOME_VALUE;

				if (i==1 && !halfline)
				{
					assign = 1;
                                        eol = 1;
					goto NextWord;
				}

				nextt = 0;
				evalue = 0;
				goto NextWord;
			}

			case CHILDREN_T:
				iline = 1;
			case PARENT_T:
				if (t==PARENT_T && word[i+1][0]!='(' && word[1][0]=='*') glegal = 1;
			case SIBLING_T:
			case CHILD_T:
			case YOUNGEST_T:
			case ELDEST_T:
			case YOUNGER_T:
			case ELDER_T:
			case SYSTEM_T:		/* use same syntax for system() */
			{
				if (!glegal)
				{
					if (t!=SYSTEM_T)
					  Expect(i+1, "( )", "object number in parentheses");
					else
					  Expect(i+1, "( )", "system function number in parentheses");
					parambrackets = (char)(brackets+1), paramlist++;
				}
				if (!glegal && i==1 && !halfline && t!=SYSTEM_T)
					{eol = 1;
					assign = 1;}
				eline = 1;
				nextt = 0;
				evalue = 0;
				break;
			}

			case WORD_T:
			{
				if (!gline)
                                        Expect(i+1, "[ ]", "word number in brackets");
				else glegal = true;
				if (i==1 && !halfline)
					{eol = 1;
					assign = 1;}
				nextt = 0;
				evalue = 0;
				subscript++;
				break;
			}

			case ROUTINE_T:
			{
				if (lastt==SOME_VALUE)
				{
					if (!gline && !incompprop) oporsep = i;
				}

				if (incompprop)
				{
					if (!strcmp(word[i], "parse"))
						Error("Obsolete usage:  remove \"Parse\"");
					if (word[i+1][0]==',')
						{i++;
						lastt = COMMA_T;
						evalue = i;}
				}
				else if (word[i+1][0]=='(')
					parambrackets = (char)(brackets+1), paramlist++;

				RememberAddr(codeptr, ROUTINE_T, (unsigned int)token_val);
				WriteCode((unsigned int)token_val, 2);

				lastt = SOME_VALUE;
				nextt = 0;
                                glegal = 1;
				evalue = 0;

				if (i==1) pexpr = 1;

				goto NextWord;
			}

                        case LABEL_T:
			{
				codeptr--;
				RememberAddr(codeptr, LABEL_T, (unsigned int)token_val);
				WriteCode((unsigned int)token_val, 2);
				if ((words>2 && word[words][0]!='}') ||
					(i>1 && strcmp(word[1], "jump")))
				{
					sprintf(line, "Improper use of label:  %s", word[i]);
					Error(line);
				}
				evalue = 0;
				break;
			}

			case VALUE_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;

				WriteCode((unsigned int)token_val, 2);
				
				if ((lastt==POUND_T) && (int)token_val<=0)
				{
					Error("Property element <= 0");
				}

				lastt = SOME_VALUE;
				nextt = 0;
                                iline = 1;
				evalue = 0;
				goto NextWord;
			}

			case ARRAYDATA_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;

				WriteCode((unsigned int)token_val, 2);
				if (i==1 && !halfline)
					{eol = 1;
					assign = 1;}
				nextt = 0;
				evalue = 0;
				subscript++;
				break;
			}

			case RANDOM_T:
			{
				if (lastt==SOME_VALUE) oporsep = i;

				Expect(i+1, "( )", "limit in parentheses");
					parambrackets = (char)(brackets+1), paramlist++;

				iline = 1;
				nextt = 0;
				evalue = 0;
				break;
			}

			case ELSE_T:
				WriteCode(0, 2);
			case BREAK_T:
				if (word[i+1][0]=='}' && t==BREAK_T)
					break;
			case RUNEVENTS_T:
			case CLS_T:
			{
				nline = i;
				if ((words > 1) && (word[2][0]!='}' || t==ELSE_T))
				{
					sprintf(line, "Expecting end of line after token:  %s", word[i]);
					Error(line);
				}
				break;
			}

			case EQUALS_T:
				if (paramlist)
				{
					if (parambrackets>brackets)
						Error("Expecting parentheses around equivalence test");
				}
				else if (!assign && !expr && !pexpr && !brackets)
					Error("Illegal assignment");
				else if (!brackets) assignment = true;

				if (lastt==t)
					Error("Illegal equivalence operator: ==");

				if (!brackets) expr = true;

				if (i < words)
					evalue = i;
				if (compstart)
				{
					compend = codeptr;
					comptype = EQUALS_T;
				}

				/* fall through */

			case IS_T:               /* "is" */
			{
				if (brackets>parambrackets) condexpr = true;

				assign = 0;         /* no longer waiting for it */
				if (pexpr && !paramlist) eol = 1;
				if (t==IS_T)
				{
					if (condexpr)
					{
						comptype = NOT_EQUAL_T;
						if (!strcmp(word[i+1], "not"))
							compend = codeptr + 1;
						else
							compend = codeptr;
					}
					isneedsval = true;
				}
			}

			case TILDE_T:
			case NOT_T:
			{
				iline = 1;
				nextt = 1;
				if (t!=EQUALS_T)
				{
					eline = 1;
					evalue = i;

					if ((t==IS_T) && lastt!=SOME_VALUE)
						nextt++;

				}
				break;
			}

			case COMMA_T:
			{
				iline = 1;
				eline = 1;
				evalue = i;

				if ((singleval) && !brackets) singleval = 2;
				if (multipleval) multipleval++;

	/* See if this is an "if x = 1, 2, 3" case, where the comma(s) 
	   must be replaced by the appropriate "or" or "and", plus the
	   original comparision (e.g., "x = ").

	   (Somewhat inappropriately for blanket usage, EQUALS_T and
	   NOT_EQUAL_T are used for differentiation; EQUALS_T usage
	   results in "or" comparisons and NOT_EQUAL_T usage results
	   in "and" comparisons.)
	*/
				if (condexpr && ((!paramlist) || parambrackets<brackets))
				{
					codeptr--;         /* replace comma */

					if (compend<=compstart)
						goto CommaError;

					if (possible_ambiguity>0 && possible_ambiguity==brackets)
						Error("?Ambiguous ',':  use 'and', 'or', or '( )'");
					else
						possible_ambiguity = false;
					definite_ambiguity = brackets?brackets:(char)(-1);

					if (comptype==EQUALS_T)
						WriteCode(OR_T, 1);
					else if (comptype)
						WriteCode(AND_T, 1);
					else
						goto CommaError;

					compbrackets = brackets;

					for (j=0; j<(int)(compend-compstart); j++)
					{
						int resolve_data[15];

						fseek(objectfile, compstart+j, SEEK_SET);
						a = fgetc(objectfile);
						fseek(objectfile, 0, SEEK_END);
						if (ferror(objectfile))
							FatalError(READ_E, "primary file");
						
						/* If the initial value is a routine or array address,
						   we'll need to add it to the list to be resolved at the end:
						*/
						if ((j>=3) &&
							(resolve_data[j-3]==ROUTINE_T || resolve_data[j-3]==ARRAY_T))
						{
							RememberAddr(codeptr-2, resolve_data[j-3], resolve_data[j-1]*256+resolve_data[j-2]);
						}
						else
							resolve_data[j] = a;
						
						WriteCode(a, 1);
					}

					goto DoneComma;

CommaError:
					Error("Error in expression");
					return;
				}

DoneComma:
				if (i==words)
				{
					GetWords();
					i = 0;
					halfline = true;
					evalue = FAILED;
				}

				break;
			}

			case IN_T:
			{
				iline = 1;
				eline = 1;
				a = 0;
				if (!strcmp(word[i-1], "not") && 
					((i>=2) && strcmp(word[i-2], "is")))
				{
					a = NOT_T;
					nextt = 1;
				}
				else
					nextt++;
				evalue = i;
				if (condexpr)
				{
					if (a==NOT_T)
						comptype = NOT_EQUAL_T;
					else
						comptype = EQUALS_T;
					compend = codeptr;
				}
				break;
			}

			case AND_T:
			case OR_T:
			{
				if (paramlist && brackets==parambrackets)
					Error("Enclose 'and'/'or' test in parentheses");

				if (definite_ambiguity==-1 || (definite_ambiguity>0 && definite_ambiguity==brackets))
					Error("?Ambiguous ',':  use 'and', 'or', or '( )'");
				else definite_ambiguity = false;
				possible_ambiguity = brackets;

				if (lastt!=SOME_VALUE) nextt = 1;
				evalue = i;
				compstart = codeptr;

				/* A conditional expression line may end with
				   "and" or "or", signalling the compiler
				   to get another line.
				*/
				if (i==words)
				{
					GetWords();
					i = 0;
					halfline = true;
					evalue = FAILED;
				}
				break;
			}

			case ASTERISK_T:
				if (verbskipaddr)
					WriteCode(0, 1);
				if (i==1) expr = true, gline = 1;
			case FORWARD_SLASH_T:
				glegal = 1;
				if (word[i-1][0]=='=')
				{
					nextt = 2;
					break;
				}

				/* fall through */

			case MINUS_T:
			case PLUS_T:
			case PIPE_T:
			case AMPERSAND_T:
				if (t==MINUS_T || t==PLUS_T)
				{
					/* increment (++) or decrement (--) */
					if (word[i+1][0]==token[t][0])
					{
						if (i==1) firstvalue = MINUS_T;
                                                if (assign && i==2)
						{
							if (word[i+2][0]!='}')
								Expect(i+2, "NULL", "end of line after:  ++ or --");
						}
						i++;

						WriteCode(t, 1);
						expr = true;
						eol = true;
						assign = false;

                                                /* following value or '=' */
						if (lastt==SOME_VALUE || lastt==EQUALS_T)
						{
							nextt = 0;
							if (lastt==EQUALS_T)
								lastt = 0;
							else
								lastt = SOME_VALUE;
						}

						/* end of line */
						else if (((int)i<words-1) || ((int)i==words-1 && word[words][0]!='}'))
						{
							evalue = i;
							lastt = 0;
						}

						goto NextWord;
					}
				}

				/* It may also be a '+=', '-=', etc. */
				if (word[i+1][0]=='=')
				{
					i++;
					WriteCode(EQUALS_T, 1);
					assign = 0;
					evalue = i;
					lastt = t;
                                        goto NextWord;
				}

			case GREATER_EQUAL_T:
			case LESS_EQUAL_T:
			case NOT_EQUAL_T:
				if (t==NOT_EQUAL_T)
				{
					compend = codeptr;
					comptype = NOT_EQUAL_T;
				}

				/* fall through */

			case GREATER_T:
			case LESS_T:
			{
				nextt = 0;

				eline = 1;
				if (lastt==t)
					{sprintf(line, "Illegal duplicate operator:  %s%s", token[t], token[t]);
					Error(line);}

				if (!expr && !brackets && !sbrackets)
					Error("Expecting parentheses around expression");
				if (t != ASTERISK_T)
					iline = 1;
				if (i!=1) evalue = i;

				break;
			}

			case TRUE_T:
			case FALSE_T:
				iline = 1;
			case CALL_T:
			{
				nextt = 0;
				evalue = 0;
				if (t==CALL_T)
				{
					evalue = i;
					if (i==1) pexpr = 1;
					eol = true;
				}
				break;
			}

			case HELD_T:
			case MULTI_T:
			case MULTIHELD_T:
			case ANYTHING_T:
			case NOTHELD_T:
			case MULTINOTHELD_T:
			case OBJECT_T:
			case XOBJECT_T:
				glegal = 2;
				break;

			case NUMBER_T:
				glegal = 1;

				/* Allow double "number" in grammar line */
				if ((gline) && lastt==t) lastt = 0;

				if ((!gline) && strcmp(word[1], "print"))
					Error("Illegal use of 'number'");

			case CAPITAL_T:
			case HEX_T:
				if (!gline)
				{
					if (i<words-1 && word[i+1][0]==';')
					{
						evalue = i;
						i = words;	/* jump to EOL */
					}
				}
				break;

			case PARSE_T:
			case SERIAL_T:
				nextt = 0;
				evalue = 0;
				break;

			case STRING_T:
				glegal = 1;
			case DICT_T:
				multipleval = 1;
				if (!gline)
				{
                                        Expect(i+1, "( )", "formal parameter(s) in parentheses");
						parambrackets = (char)(brackets+1), paramlist++;
				}

				if (t==DICT_T && !MAXDICTEXTEND)
					Error("?'dict' without $MAXDICTEXTEND allowance");

				/* fall through */

			case SAVE_T:
			case RESTORE_T:
			case SCRIPTON_T:
			case SCRIPTOFF_T:
			case RESTART_T:
			case UNDO_T:
			case RECORDON_T:
			case RECORDOFF_T:
			case READVAL_T:
			case PLAYBACK_T:
			{
				if (lastt==SOME_VALUE && !gline) oporsep = i;

				if (!gline) lastt = SOME_VALUE;
				nextt = 0;
                                if (t!=STRING_T) iline = 1;
				evalue = 0;
				goto NextWord;
			}

			case WRITEFILE_T:
			case READFILE_T:
			{
				evalue = true;
				nline = i;
				eol = true;
				WriteCode(0, 2);
				singleval = true;
				break;
			}

			case MOVE_T:
				moveneedsto = true;
			case REMOVE_T:
				eol = true;
			case JUMP_T:
				singleval = true;
				nline = i;
			case TO_T:
			{
				evalue = i;
				if (t==TO_T) moveneedsto = false;
				break;
			}

			case DECIMAL_T:
			{
				if (word[i+1][0]=='.')	/* allow ".." */
				{
					if (word[i-1][0]=='.')
						evalue = i;  /* not "..." */
				}
				else
				{
					if (lastt==t) lastt = SOME_VALUE;
					evalue = i;
				}
				
			/* This is to check if an explicit object.property is not defined,
			   i.e., if there is not "property" for "object".  It's fairly
			   involved since it has to skim the propdata (stolen from
			   Inherit() in hcbuild.c), but the "if (obj >= objectctr)" 
			   check should mean it doesn't run often (since not-yet-built 
			   objects cannot be checked yet).
			*/
				if (IDWord(i-1)==OBJECTNUM_T)
				{
				  unsigned int obj, prop, o, p = 0, n;
				  
				  obj = (unsigned int)token_val;
				  if (obj >= (unsigned int)objectctr) goto SkipObjPropCheck;
				  o = oprop[obj];
				  if (IDWord(i+1)==PROP_T)
				  {
				  	prop = (unsigned int)token_val;
				  	a = 0;	/* found flag */
				  	while (p!=PROP_END)
				  	{
				  	  p = propdata[o/PROPBLOCKSIZE][o%PROPBLOCKSIZE];
				  	  o++;
				  	  if (p!=PROP_END)
				  	  {
						n = propdata[o/PROPBLOCKSIZE][o%PROPBLOCKSIZE];
						if (n==PROP_ROUTINE || n==PROP_LINK_ROUTINE)
							n = 1;
						o += (++n);
				      
						if (prop==p)
						{
							a = 1;
							break;
						}
					  }
					}
					if (!a)
					{
						sprintf(line, "?\"%s\" has no property \"%s\"",
							object[obj], property[prop]);
						Error(line);
					}
				  }
SkipObjPropCheck:;
				}
				
				/* Validate an assignment, e.g., 
					Routine.prop = ...
				*/
				if (firstvalue && !assignment)
					firstvalue = DECIMAL_T;

				invalid_is_val = false;

				break;
			}

			case TEXT_T:
			{
				nline = i;
				if ((words > 1) && word[2][0]!='}')
				{
					if (strcmp(word[i+1], "to"))
					{
						sprintf(line, "Expecting 'to' after '%s'", word[i]);
						Error(line);
					}
					eol = 1;
					singleval = true;
				}
				break;
			}

			case WINDOW_T:
			{
				nline = i;
				if ((words > 1) && word[2][0]!='}')
					evalue = i;
				eol = 1;
				break;
			}

			case NEWLINE_T:
			{
				if ((words > i) && word[i+1][0]!='}')
					Expect(i+1, ";", "semicolon following 'newline'");
				break;
			}

#if !defined (COMPILE_V25)
			case VIDEO_T:
#endif
			case PICTURE_T:
			case SOUND_T:
			case MUSIC_T:
				evalue = true;
				nline = i;
				eol = true;
				break;

			case REPEAT_T:
			{
				nextt = 0;	/* cancel evalue */
				if (lastt!=MUSIC_T && lastt!=SOUND_T && lastt!=VIDEO_T)
					Error("Unexpected 'repeat'");
				break;
			}

			case SEMICOLON_T:
			{
				if (((i!=words) && lastt!=TEXTDATA_T)
					&& strcmp(word[1], "print") && strcmp(word[1], "for"))
				{
					Error("Illegal semicolon");
				}
				break;
			}

			case OPEN_BRACE_T:
				Error("Illegal leading brace:  {");
				break;
#if defined (COMPILE_V25)
			case ADDCONTEXT_T:
			case VIDEO_T:
			{
				sprintf(line, "'%s' not supported in v2.5", word[i]);
				Error(line);
				break;
			}
#endif
		}

		if ((t) && t==lastt)
		{
			sprintf(line, "Unexpected second \"%s\"", word[i]);
			Error(line);
		}

		if ((incompprop) && !brackets && evalue)
		{
			sprintf(line, "Illegal in complex property block heading:  %s", token[t]);
			Error(line);
		}

		lastt = t;

NextWord:
		/* If the line started with a value, remember what type
		   of value it was.
		*/
		if ((i==1) && (assign || pexpr))
			firstvalue = t;

		if (nline > 1)
		{
			sprintf(line, "Expecting new line before:  %s", word[nline]);
			Error(line);
			nline = 0;
		}
		else if (i==1 && iline && !halfline)
		{
			Error("Illegal at start of line");
                        iline = 0;
		}

		if (gline)
		{
			if (!glegal)
				{sprintf(line, "Illegal grammar token:  %s", word[i]);
				Error(line);}
		}
		else if (glegal==2)
			{sprintf(line, "Illegal use of grammar token:  %s", word[i]);
			Error(line);}

		if (oporsep)
		{
			sprintf(line, "Expecting operator or separator before:  %s", word[oporsep]);
			Error(line);
			oporsep = 0;
		}
		else if (nextt > 1 && !halfline)
		{
			sprintf(line, "Expecting value before:  %s", word[i]);
			Error(line);
			nextt = 0;
		}

		/* "<x> is <object>" and "<x> is <property>" get a warning,
                   but "<x> is <object>.<property> doesn't--hitting '.' later
		   will clear invalid_is_val
		*/
		if (isneedsval && lastt==SOME_VALUE)
		{
			if (t==OBJECTNUM_T || t==PROP_T)
				invalid_is_val = i;
			isneedsval = 0;
		}

		if (nextt) nextt++;
	}

	if (!halfline)
	{
		if (assign)
		{
			if (!incompprop)
				Error("Missing assignment:  =");
		}
		else if (assignment)
		{
			switch (firstvalue)
			{
				/* Assignable values: */
				case 0:		    /* not set		 */
				case DECIMAL_T:	    /* '.' property 	 */
				case VAR_T:	    /* variable 	 */
				case OPEN_SQUARE_T: /* '[' array element */
					break;
				default:
					Error("Illegal assignment to non-variable");
			}
		}
	}

	if (brackets) Error("Unmatched parentheses");

	if (sbrackets) Error("Unmatched brackets");

	if (eline) Error("Illegal at end of line");

	if (evalue > 0)
	{
		sprintf(line, "Expecting %s following:  %s", (!strcmp(word[1], "jump"))?"label":"value", word[evalue]);
		Error(line);
	}

	if (singleval==2)
	{
		sprintf(line, "Multiple values illegal after:  %s", word[1]);
		Error(line);
	}
	else if (multipleval==1 && !gline)
	{
		sprintf(line, "'%s' needs more than one parameter", word[1]);
		Error(line);
	}

	if (moveneedsto) Error("'move' requires 'to'");

	/* Illegal semicolon check */
	if (!scolon)
	{
                if ((word[words]!=NULL) &&
			(word[words][0]==';' || ((words>0) && word[words][0]=='}' && (word[words-1]!=NULL)
				&& word[words-1][0]==';')))
		{
			Error("Illegal semicolon");
		}
	}

	/* Check for "<obj> is <obj>" instead of "<obj> is <obj>.<prop>"
	   (since the former was probably supposed to be an "=")
	*/
	if (invalid_is_val)
	{
		sprintf(line, "?\"%s\" used as attribute test", word[invalid_is_val]);
		Error(line);
	}

	/* Text-only line */
	if (tline)
	{
		if (word[words][0]=='}') words--;

		/* Nothing else is allowed on the line besides "<text>" and
		   possibly a trailing semicolon.
		*/
		if ((words==2 && word[2][0]!=';') || words > 2)
			Error("Illegal print statement");
	}

	/* Before/after (i.e., complex) property header */
	if (incompprop && nest > 1)
	{
		if (words==1 && firstvalue==ROUTINE_T)
			Error("Invalid complex property header");

		WriteCode(JUMP_T, 1);
		skipptr = codeptr;
		WriteCode(0, 2);

		GetWords();
		j = er;                 /* error count pre-DoBrace() 	*/
		DoBrace();
		/* A sanity check in order to prevent getting
		   trapped if no brace is found
		*/
		if (j<er) nest--;

		nest++;
		full_buffer = 1;
		incompprop = false;
		BuildCode(PROP_T);
		incompprop = true;

		Boundary();
		tempptr = codeptr;
		codeptr = skipptr;

		if (hlb) RememberAddr(codeptr, VALUE_T, (unsigned int)(tempptr/address_scale));

		WriteCode((unsigned int)(tempptr/address_scale), 2);
		codeptr = tempptr;
	}
	else
		if (eol==1) WriteCode(EOL_T, 1);

	if (verbskipaddr)
	{
		tempptr = codeptr;
		codeptr = verbskipaddr;
		WriteCode((unsigned int)(tempptr-verbskipaddr), 1);
		codeptr = tempptr;
	}
}


/* CODESELECT

	As in:

		select <expr>
		case <case1>:
			{...}
		case <case2>:
			{...}
		...
*/

#define MAXCASELENGTH 1024
#define MAXSELECTVARLENGTH 256

void CodeSelect(void)
{
	char b[MAXCASELENGTH] = "";
	char selectvar[MAXSELECTVARLENGTH] = "";
	char inbrackets = 0;
	int i;
	int casecount = 0;

	WriteCode(SELECT_T, 1);

	/* Build the original expression against which all the following
	   cases will be tested:
	*/
	if (words==2)
	{
		if (strlen(word[2])>=MAXSELECTVARLENGTH-1)
		{
			Error("'select' statement overflow");
			return;
		}

		sprintf(selectvar, "%s ", word[2]);
	}

	/* If more than two words, use a temporary local so that
	   we're not doing things like ++ and 'random' more than
	   once
	*/	   
	else
	{
		strcpy(line, "");
		for (i=2; i<=words; i++)
		{
			strcat(selectvar, word[i]);
			strcat(selectvar, " ");
		}

		word[1] = "";
		sprintf(buffer, "__selectvar%02d", nest);
		word[2] = &buffer[0];
		words = 2;
		silent_IDWord = 1;
		if (!IDWord(2)) DefLocals(0);
		silent_IDWord = 0;

		sprintf(buffer, "__selectvar%02d= ", nest);
		strcat(buffer, selectvar);
		SeparateWords();

		/* Code the "__selectvar0n = ..." assignment */
		CodeLine();

		/* Now use __selectvar0n for the comparisons */
		sprintf(selectvar, "__selectvar%02d ", nest);
	}

	GetWords();

	while (!strcmp(word[1], "case"))
	{
		casecount++;
		if (!strcmp(word[2], "else"))
			strcpy(buffer, "else");
		else
		{
			/* Make up each "case <case>" statement: */
			sprintf(b, "case %s= ", selectvar);

			for (i=2; i<=words; i++)
			{
				if (word[i][0]==',' && !inbrackets)
				{
					if (strlen(b) > MAXCASELENGTH-5)
					{
CaseOverflow:
						Error("'case' statement overflow");
						break;
					}
					strcat(b, "or ");
					strcat(b, selectvar);
					strcat(b, "= ");
				}
				else
				{
					if (strlen(b)+strlen(word[i])>=MAXCASELENGTH-1)
						goto CaseOverflow;
						
					if (word[i][0]=='(')
						inbrackets++;
					else if (word[i][0]==')')
						inbrackets--;
					strcat(b, word[i]);
					strcat(b, " ");
				}
			}
			strcpy(buffer, b);
		}

		SeparateWords();
		CodeIf();

		GetWords();
	}

	if (!casecount) Error("'select' without 'case'");

	full_buffer = 1;
}


/* CODEWHILE

	As in:

		while <condition>
			{...}
*/

void CodeWhile(void)
{
	long startpos, returnpos, skipaddr;

	Boundary();
	startpos = codeptr;

	returnpos = codeptr + 1;
	CodeLine();

	BeginCodeBlock();

	nest++;
	full_buffer = 1;
	BuildCode(WHILE_T);

	/* Write the skip address to jump to when <condition> fails: */
        skipaddr = codeptr;
	codeptr = returnpos;
	WriteCode((unsigned int)(skipaddr+3-returnpos), 2);
	codeptr = skipaddr;

	WriteCode(JUMP_T, 1);

	if (hlb) RememberAddr(codeptr, VALUE_T, (unsigned int)(startpos/address_scale));

	WriteCode((unsigned int)(startpos/address_scale), 2);
}


/* CODEWINDOW

	Checks to see if there's a following code block, such as
	"window n {...}" vs. simply "window 0" (which has no following
	code block).
*/

void CodeWindow(void)
{
	CodeLine();

	if (word[2][0]!='0')
	{
		BeginCodeBlock();
		nest++;
		full_buffer = 1;
		BuildCode(WINDOW_T);
	}

	full_buffer = 0;
}


/* IDWORD

	Returns the token type of word[a].
*/

int IDWord(int a)
{
	char b[33];
	int i, hash;

	if (word[a][0]=='\0' || a>words) return 0;

	hash = FindHash(word[a]);
	token_val = 0xffff;

	/* Token (from htokens.h) */
	for (i=0; i<=TOKENS; i++)
	{
		if (HashMatch(hash, token_hash[i], word[a], token[i]))
		{
			switch (i)
			{
			  case MINUS_T:
			  {

				/* Because numbers > 32767 cannot
				   normally be parsed
				*/
				if (!strcmp(word[a+1], "32768"))
				{
					KillWord(i);
					token_val = 0x8000;
					return VALUE_T;
				}
			  	break;
			  }
			}

			return i;
		}
	}

	/* Something in ""--this could be any one of three things:  a
	   printed string to be written to the text bank, in-code text
	   to be coded at the current code location, or a dictionary
	   entry to be recorded then its location returned.
	*/
	if (word[a][0]=='\"')
	{
		/* Text string */
		if (a==1 && !halfline)		/* first word in line */
			{StripQuotes(word[a]);
			token_val = WriteText(word[a]);
			return TEXTDATA_T;}

		/* Dictionary entry */
		else if (strcmp(word[1], "print"))
			{StripQuotes(word[a]);
			token_val = AddDictionary(word[a]);
			return DICTENTRY_T;}

		/* In-code printed text */
		else
			{StripQuotes(word[a]);
			return STRINGDATA_T;}
	}

	/* A number, i.e., a numerical value */
	if (word[a][0]>='0' && word[a][0]<='9')
	{
		while ((word[a][0]=='0') && word[a][1]!='\0')
			word[a]++;

		if (!strcmp(word[a], itoa(atoi(word[a]), b, 10)))
		{
			token_val = (unsigned int)atoi(word[a]);
			/* Keep within 16 bits */
			if (token_val<-32768 || token_val>32767) Error("Integer value out of range");
			return VALUE_T;
		}
	}

	/* Global variable */
	for (i=0; i<globalctr; i++)
	{
		if (HashMatch(hash, global_hash[i], word[a], global[i]))
			{token_val = i;
			return VAR_T;}
	}

	/* Local variable */
	for (i=0; i<localctr; i++)
	{
		if (HashMatch(hash, local_hash[i], word[a], local[i]))
			{token_val = i + MAXGLOBALS;
			return VAR_T;}
	}

	/* Routine */
	for (i=0; i<(passnumber==1?routinectr:routines); i++)
	{
		if (HashMatch(hash, routine_hash[i], word[a], routine[i]))
		{
			token_val = i;
			return ROUTINE_T;
		}
	}

	/* Attribute */
	for (i=0; i<attrctr; i++)
	{
		if (HashMatch(hash, attribute_hash[i], word[a], attribute[i]))
			{token_val = i;
			return ATTR_T;}
	}

	/* Property */
	for (i=0; i<propctr; i++)
	{
		if (HashMatch(hash, property_hash[i], word[a], property[i]))
			{token_val = i;
			return PROP_T;}
	}

	/* Object */
	for (i=0; i<(passnumber==1?objectctr:objects); i++)
	{
		if (HashMatch(hash, object_hash[i], word[a], object[i]))
			{token_val = i;
			return OBJECTNUM_T;}
	}

	/* Array */
	for (i=0; i<arrayctr; i++)
	{
		if (HashMatch(hash, array_hash[i], word[a], array[i]))
			{token_val = arrayaddr[i]/2;
			return ARRAYDATA_T;}
	}

	/* Alias */
	for (i=0; i<aliasctr; i++)
	{
		if (HashMatch(hash, alias_hash[i], word[a], alias[i]))
		{
			/* Property alias */
			if (aliasof[i]>=MAXATTRIBUTES)
				{token_val = aliasof[i] - MAXATTRIBUTES;
				return PROP_T;}

			/* Attribute alias */
			token_val = aliasof[i];
			return ATTR_T;
		}
	}

	/* Constant--which is really a value, since the conversion from an
	   object, dictionary entry, etc. has been done by DefConstant()
	*/
	for (i=0; i<constctr; i++)
	{
		if (HashMatch(hash, constant_hash[i], word[a], constant[i]))
			{token_val = constantval[i];
			return VALUE_T;}
	}

	/* Label */
	for (i=0; i<(passnumber==1?labelctr:labels); i++)
	{
		if (HashMatch(hash, label_hash[i], word[a], label[i]))
			{token_val = i;
			return LABEL_T;}
	}

	/* An ASCII character, e.g., 'A' */
	if (word[a][0]=='\'')
	{
		if ((word[a][2]!='\'' && word[a][1]!='\\') || (word[a][1]=='\\' && word[a][3]!='\''))
		{
			strcpy(line, "Unknown ASCII value:  ");
			strncpy(line+strlen(line), word[a], MAXBUFFER-64);
			Error(line);
		}
		if (word[a][1]=='\\') token_val = toascii(word[a][2]);
		else token_val = toascii(word[a][1]);
		return VALUE_T;
	}

	if (!silent_IDWord)
	{
		strcpy(line, "Syntax error:  ");
		strncpy(line+strlen(line), word[a], MAXBUFFER-64);
		Error(line);
	}

	return 0;
}

