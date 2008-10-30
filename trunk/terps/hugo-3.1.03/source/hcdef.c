/*
	HCDEF.C

	contains CheckInitializer, CheckSymbol and Def... routines
	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


char object_id[64];             /* name of current obj., routine, etc. */

/* Object names, counter, and location in the property table */
char **object; int *object_hash; int objectctr = 0;
unsigned int *oprop;            /* needed for inheritance */

/* Attribute names and counter */
char *attribute[MAXATTRIBUTES]; int attribute_hash[MAXATTRIBUTES];
int attrctr = 0;

/* Property names, counter, set/not-set flags, and additive/complex flags
   (i.e., bitmasks).  Also property table size, property defaults,
   property data heap array.
*/
char **property; int *property_hash; int propctr = 0; char *propset;
char *propadd;
unsigned int proptable = 0, *propdef;
unsigned int *propdata[MAXPROPBLOCKS]; unsigned int propheap = 0;

/* Label names, count, addresses */
char **label; int *label_hash; int labelctr = 0; unsigned int *laddr;

/* Routine names, count, addresses */
char **routine; int *routine_hash; int routinectr = 0;
unsigned int *raddr;

/* Flag if routine is replaced (or count if replaced more than once) */
char *rreplace;

/* Total number of addresses and array for address storage */
unsigned int addrctr = 0;
struct addrstruct *addrdata[MAXADDRBLOCKS];

/* Event data, including object the event is in, the address,
   and counter
*/
unsigned int *eventin, *eventaddr; int eventctr = 0;

/* Alias names, attribute/property alias is of, counter */
char **alias; int *alias_hash; int *aliasof; int aliasctr = 0;

/* Global variable names, local variable names, count for each */
char *global[MAXGLOBALS], local[MAXLOCALS][33];
int global_hash[MAXGLOBALS], local_hash[MAXLOCALS];
int globalctr = 0, localctr = 0;
char unused[MAXLOCALS];

/* Global variable initial values */
unsigned int globaldef[MAXGLOBALS];

/* Constant names, values, count */
char **constant; int *constant_hash;
unsigned int *constantval; int constctr = 0;

/* Array names, addresses in array table, length of each array, total
   size of array table, counter
*/
char **array; int *array_hash; unsigned int *arrayaddr, *arraylen;
unsigned int arraysize = MAXGLOBALS*2; int arrayctr = 0;


/* CHECKINITIALIZER */

static int initializer_type;

int CheckInitializer(int wordnum)
{
	int t;

	initializer_type = VALUE_T;
	
	t = IDWord(wordnum);

	switch (t)
	{
		case MINUS_T:
			KillWord(wordnum);
			t = IDWord(wordnum);
			return token_val*-1;

		case TRUE_T:
			return 1;
		case FALSE_T:
			return 0;

		case AMPERSAND_T:
		{
			KillWord(wordnum);
			t = IDWord(wordnum);
			if (passnumber > 1)
			{
				switch (t)
				{
					case ROUTINE_T:
					{
						if ((routinectr) && token_val < routinectr)
							return raddr[token_val];
					}
				}
			}

			/* Can init not-yet-known addresses for arrays in Pass1
			   because they actually get written afterward */
			else if (!strcmp(word[1], "array"))
			{
				switch (t)
				{
					case ROUTINE_T:
					{
						initializer_type = ROUTINE_T;
						return token_val;
					}
				}
			}

			Error("?Address unavailable at compile-time");
			return 0;
		}

		/* Valid initializers: */
		case ARRAY_T:
			token_val = arrayaddr[token_val];
			break;
		case ATTR_T:
		case DICTENTRY_T:
		case OBJECTNUM_T:
		case PROP_T:
		case VALUE_T:
			break;

#ifdef MATH_16BIT
		case (-1):
#else
		case 0xffff:	/* see IDWord() */
#endif
			return 0;

		default:
		{
			sprintf(line, "Illegal initializer:  %s", word[wordnum]);
			Error(line);
			return 0;
		}
	}
	
	return token_val;
}


/* CHECKSYMBOL */

void CheckSymbol(char *a)
{
	int i, hash;
	char *b = "";

	if (strlen(a) > MAX_SYMBOL_LENGTH)
		{sprintf(line, "Symbol name \"%s\" too long", a);
		Error(line);
		return;}

	hash = FindHash(a);

	/* Tokens */
	for (i=1; i<=TOKENS; i++)
		if (HashMatch(hash, token_hash[i], a, token[i]))
			{b = "a token";
			goto Checkb;}

	/* Attributes */
	for (i=0; i<attrctr; i++)
		if (HashMatch(hash, attribute_hash[i], a, attribute[i]))
			{b = "an attribute";
			goto Checkb;}

	/* Properties */
	for (i=0; i<propctr; i++)
		if (HashMatch(hash, property_hash[i], a, property[i]))
			{b = "a property";
			goto Checkb;}

	/* Aliases */
	for (i=0; i<aliasctr; i++)
		if (HashMatch(hash, alias_hash[i], a, alias[i]))
			{b = "an alias";
			goto Checkb;}

	/* Routines */
	for (i=0; i<routinectr; i++)
		if (HashMatch(hash, routine_hash[i], a, routine[i]))
			{b = "a routine";
			goto Checkb;}

	/* Objects */
	for (i=0; i<objectctr; i++)
		if (HashMatch(hash, object_hash[i], a, object[i]))
			{b = "an object";
			goto Checkb;}

	/* Globals */
	for (i=0; i<globalctr; i++)
		if (HashMatch(hash, global_hash[i], a, global[i]))
			{b = "a global variable";
			goto Checkb;}

	/* Locals */
	for (i=0; i<localctr; i++)
		if (HashMatch(hash, local_hash[i], a, local[i]))
			{b = "a local variable";
			goto Checkb;}

	/* Arrays */
	for (i=0; i<arrayctr; i++)
		if (HashMatch(hash, array_hash[i], a, array[i]))
			{b = "an array";
			goto Checkb;}

	/* Constants */
	for (i=0; i<constctr; i++)
		if (HashMatch(hash, constant_hash[i], a, constant[i]))
			{b = "a constant";
			goto Checkb;}

	/* Labels */
	for (i=0; i<labelctr; i++)
		if (HashMatch(hash, label_hash[i], a, label[i]))
			{b = "a label";
			goto Checkb;}

	/* If b has been set */
Checkb:
	if (b[0]!='\0')
	{
		sprintf(line, "\"%s\" previously defined as %s", a, b);
		Error(line);
	}
}


/* DEFARRAY */

void DefArray(void)
{
	unsigned int count = 0;
	unsigned int array_location = arraysize;

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	Expect(3, "[ ]", "array size in brackets");

	CheckSymbol(word[2]);

	array[arrayctr] = MakeString(word[2]);
	array_hash[arrayctr] = FindHash(array[arrayctr]);
	arrayaddr[arrayctr] = arraysize;
	if (IDWord(4)==0)
		arraylen[arrayctr] = 0;
	else
		arraylen[arrayctr] = (unsigned int)token_val;

	/* The size of an array entry includes two bytes for
	   the length.
	*/
	arraysize = arraysize + (unsigned int)token_val*2 + 2;

	arrayctr++;

	if (arrayctr==MAXARRAYS)
		{arrayctr = MAXARRAYS-1;
		sprintf(line, "Maximum of %d arrays exceeded", MAXARRAYS);
		Error(line);}

	/* Process any default array values, remembering them for writing
	   in Pass3()
	*/
	if (words > 5)
	{
		unsigned int val;

		if (!Expect(6, "=", "end of line or array assignment(s)"))
			goto SkipAssign;
		KillWord(6);	/* '=' */
		RemoveCommas();
		while (words > 5)
		{
			if (count >= arraylen[arrayctr-1])
			{
				Error("?Too many array initializers");
				return;
			}

			val = CheckInitializer(6);
			if (initializer_type==ROUTINE_T)
				RememberAddr(array_location+2+count*2, ARRAY_T+ROUTINE_T, val);
			else
				RememberAddr(array_location+2+count*2, ARRAY_T, val);
			KillWord(5);
			count++;
		}
	}
SkipAssign:

	if (words!=5) Error("Illegal array definition");
}


/* DEFATTRIBUTE */

void DefAttribute(void)
{
	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	CheckSymbol(word[2]);

	if (!strcmp(word[3], "alias"))
	{
		if (IDWord(4)!=ATTR_T)
		{
			sprintf(line, "Not an attribute:  %s", word[4]);
			Error(line);
		}
		else
		{
			aliasof[aliasctr] = (unsigned)token_val;
			alias[aliasctr] = MakeString(word[2]);
			alias_hash[aliasctr] = FindHash(alias[aliasctr]);
			aliasctr++;

			if (aliasctr==MAXALIASES)
				{attrctr = MAXALIASES-1;
				sprintf(line, "Maximum of %d aliases exceeded", MAXALIASES);
				Error(line);}
		}
		KillWord(4);
		KillWord(3);
	}
	else
	{
		attribute[attrctr] = MakeString(word[2]);
		attribute_hash[attrctr] = FindHash(attribute[attrctr]);
		attrctr++;

		if (attrctr==MAXATTRIBUTES)
			{attrctr = MAXATTRIBUTES-1;
			sprintf(line, "Maximum of %d attributes exceeded", MAXATTRIBUTES);
			Error(line);}
	}
	if (words > 2) Error("Illegal attribute definition");
}


/* DEFCOMPOUND */

void DefCompound(void)
{
	unsigned int c1, c2;
	char c[257];

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	RemoveCommas();

	if (words!=3) Error("Illegal compound definition");

	StripQuotes(word[2]);
	StripQuotes(word[3]);
	c1 = AddDictionary(word[2]);
	c2 = AddDictionary(word[3]);
	strcpy(c, word[2]);
	strcat(c, word[3]);
	AddDictionary(c);

	/* 2 = compound (0 = synonym, 1 = removal, 3 = punctuation) */
	syndata[syncount].syntype = 2;
	syndata[syncount].syn1 = c1;
	syndata[syncount].syn2 = c2;
	if (++syncount==MAXSPECIALWORDS)
	{
		syncount--;
		sprintf(line, "Maximum of %d special words exceeded", MAXSPECIALWORDS);
		Error(line);
	}
}


/* DEFCONSTANT */

void DefConstant(void)
{
	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	CheckSymbol(word[2]);

	constant[constctr] = MakeString(word[2]);
	constant_hash[constctr] = FindHash(constant[constctr]);

	if (words>=3)
		constantval[constctr] = (unsigned int)CheckInitializer(3);
	else
		constantval[constctr] = (unsigned int)constctr;

	if (words<2 || words>3) Error("Illegal constant definition");

	constctr++;

	if (constctr == MAXCONSTANTS)
		{constctr = MAXCONSTANTS-1;
		sprintf(line, "Maximum of %d constants exceeded", MAXCONSTANTS);
		Error(line);}
}


/* DEFENUM

	enumerate [constants:globals] [, step [+:-:*:/] s] [, start = n]
		{a, b, c,...}
*/

void DefEnum(void)
{
	int i, j, s;
	int tempwords, thisword;
	char *tempword[256];

	char firstval;
	char enumtype = 0;      /* constants */
	char step = '+';        /* default step type */
	int stepby = 1;
	int val = 0;
	char valstring[8];

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	RemoveCommas();

	thisword = 2;

	while (words>=thisword)
	{
		if (!strcmp(word[thisword], "constants"))
		{
			enumtype = 0;
			thisword++;
		}

		else if (!strcmp(word[thisword], "globals"))
		{
			enumtype = 1;
			thisword++;
		}

		else if (!strcmp(word[thisword], "step"))
		{
			thisword++;
			if (word[thisword][0]=='+' ||
				word[thisword][0]=='-' ||
				word[thisword][0]=='*' ||
				word[thisword][0]=='/')
			{
				step = word[thisword++][0];
			}
			s = 1;
			if (word[thisword][0]=='-')
			{
				thisword++;
				s = -1;
			}
			IDWord(thisword++);
			stepby = (int)(token_val*s);
		}

		else if (!strcmp(word[thisword], "start"))
		{
			Expect(++thisword, "=", "assignment following 'start'");
			s = 1;
			if (word[++thisword][0]=='-')
			{
				thisword++;
				s = -1;
			}
			IDWord(thisword++);
			val = (int)(token_val*s);
		}

		else
		{
			sprintf(line, "Syntax error in declaration:  %s", word[thisword]);
			Error(line);
			thisword++;
		}
	}

	GetLine(0);
	if (Expect(1, "{", "leading brace")) GetLine(0);

	firstval = true;

	while (word[1][0]!='}')
	{
		/* Get a line and copy it, because it's going to
		   be changed.
		*/
		for (i=1; i<=words; i++)
			tempword[i] = word[i];
		tempwords = words;

		word[1] = "enumerate";
		word[2] = tempword[1];
		words = 3;

		if (enumtype==1)                /* globals */
			word[words++] = "=";

		/* Count the number of words to be deleted from
		   the original line.
		*/
		j = 2;

		s = 1;  /* default positive sign */

		if ((tempwords>=2) && tempword[2][0]!=',')
		{
			if (tempword[2][0]!='=')
			{
				words-=2;
				Error("Expecting ',' or '=' after token");
				words+=2;
			}

			word[words] = tempword[3];
			if (word[words][0]=='-')
			{
				s = -1;
				word[++words] = tempword[4];
			}
			IDWord(words);
			val = (int)(token_val);

			j+=((s==1)?2:3);
		}
		else if (!firstval)
		{
			switch (step)
			{
				case '+':  val += stepby; break;
				case '-':  val -= stepby; break;
				case '*':  val *= stepby; break;
				case '/':  val /= stepby; break;
			}
		}

		/* Discrete values in a line of code must be positive
		   integers
		*/
		if (val < 0)
		{
			s = -1;
			word[words++] = "-";
			val*=-1;
		}

		itoa(val, valstring, 10);
		word[words] = valstring;

		/* The current value must now be made negative if it was
		   preceded by a '-'
		*/
		if (s==-1)
		{
			val*=-1;
		}

		switch (enumtype)
		{
			case 0:  DefConstant(); break;
			case 1:  DefGlobal(); break;
		}
		firstval = false;

		for (i=1; i<=tempwords; i++)
			word[i] = tempword[i];
		words = tempwords;

		for (i=1; i<=j; i++)
			KillWord(1);

		if (words==0) GetLine(0);
	}
}


/* DEFGLOBAL */

void DefGlobal(void)
{
	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	CheckSymbol(word[2]);

	global[globalctr] = MakeString(word[2]);
	global_hash[globalctr] = FindHash(global[globalctr]);

	/* If a default value is given */
	if (word[3][0]=='=')
	{
		globaldef[globalctr] = (unsigned int)CheckInitializer(4);
		KillWord(4);
		KillWord(3);
	}

	if (words > 2)
		Error("Illegal global variable definition");

	globalctr++;
	if (globalctr == MAXGLOBALS)
		{globalctr = MAXGLOBALS-1;
		sprintf(line, "Maximum of %d globals exceeded", MAXGLOBALS);
		Error(line);}
}


/* DEFLOCALS

	If <asargs> is true, then we're defining local variables as
	routine arguments.
*/

void DefLocals(int asargs)
{
	int i;
	unsigned int j;

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	i = 2;

	while (i<=words)
	{
		if (localctr==MAXLOCALS)
		{
			localctr = MAXLOCALS-1;
			sprintf(line, "Maximum of %d locals exceeded", MAXLOCALS);
			Error(line);
			return;
		}

		if (word[i][0]==',') i++;

		CheckSymbol(word[i]);

		strcpy(local[localctr], word[i]);
		local_hash[localctr] = FindHash(local[localctr]);
		unused[localctr] = 1;

		/* Write a label for the local variable name if we're
		   building a debuggable file, so that the textual name
		   of the local can be recovered by the debugger
		*/
		if (builddebug)
		{
			WriteCode(DEBUGDATA_T, 1);
			WriteCode(VAR_T, 1);
			WriteCode(strlen(local[localctr]), 1);
			for (j=0; j<strlen(local[localctr]); j++)
				WriteCode(local[localctr][j], 1);
		}

		/* local variable assignment */
		if (word[i+1][0]=='=')
		{
			if (words < i+2) goto LocalAssignmentError;

			if (asargs)
				Error("?Argument preassignment overrides runtime initialization");

			/* write "<local:n> = <val>" */
			WriteCode(VAR_T, 1);
			WriteCode(MAXGLOBALS+localctr, 1);
			WriteCode(EQUALS_T, 1);
			WriteCode(VALUE_T, 1);
			WriteCode((unsigned int)CheckInitializer(i+2), 2);
			WriteCode(EOL_T, 1);

			i+=3;

			if ((i<words) && word[i][0]!=',')
			{
LocalAssignmentError:
				Error("Expecting single constant value as assignment");
			}
		}
		else i++;

		localctr++;
	}
}


/* DEFOTHER

	Use the same basic frame to define routines, events, and objects
	(since they essentially look the same):

		definition
		{...}
*/

void DefOther(void)
{
	char other[33], oname[33];
	int i;
	int blank = 0;
	int starting_line = totallines;

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	if (!strcmp(word[1], "replace"))
	{
		if (token_val==ROUTINE_T) strcpy(other, "routine");
		else strcpy(other, "object");
	}
	else strcpy(other, word[1]);

	if (!strcmp(word[1], "event"))
	{
		if (words==1) strcpy(oname, "global event");
		else if (words==2) strcpy(oname, word[2]);
		else strcpy(oname, word[3]);
	}

	if (!strcmp(other, "routine"))
	{
		if (words==1 || (words > 2 && word[3][0]!='('))
			Error("Illegal routine declaration");
		else strcpy(oname, word[2]);
	}
	else if (!strcmp(other, "event"))
	{
		if (words > 3)
			Error("Illegal event declaration");
	}
	else if (!strcmp(other, "resource"))
	{
		/* Handle everything in CreateResourceFile() from Pass2() */
	}

	/* If not a routine or an event, it must be an object */
	else
	{
		if (words==1 || words > 3)
		{
			if ((words==1) || (strcmp(word[4], "in") && strcmp(word[4], "nearby")))
				Error("Illegal object declaration");
		}
		strcpy(oname, word[2]);
	}

	endofgrammar = 1;
	nest = 0;
	do
	{
		GetLine(0);

		if (word[1][0]=='#')
		{
			if (!strcmp(word[1], "#include") || !strcmp(word[2], "#link"))
				goto NoClosingBrace;
			CompilerDirective();
		}
		else
		{
			PrinttoAll();

			if (word[1][0]=='~')
			{
				CheckSymbol(word[2]);
				label[labelctr] = MakeString(word[2]);
				label_hash[labelctr] = FindHash(label[labelctr]);
				labelctr++;

				if (labelctr == MAXLABELS)
					{labelctr = MAXLABELS-1;
					sprintf(line, "Maximum of %d labels exceeded", MAXLABELS);
					Error(line);}

				if (strcmp(word[3], ""))
					Error("New line expected after label");
			}

			for (i=1; i<=words; i++)
			{
				if (word[i][0]=='{') nest++;
				if (word[i][0]=='}') nest--;
			}
		}

		/* Check for suspicious number of blank lines.  32 lines of
		   white space is a bit much to intentionally leave in a
		   program, don't you think?
		*/
		if (buffer[0]=='\0')
		{
			blank++;
			if (blank > 32)
			{
NoClosingBrace:
				sprintf(line, "Closing brace missing in %s:  %s", other, oname);

				/* Force termination */
				aborterror = true;
				totallines = starting_line;

				Error(line);
			}
		}
	} while (nest > 0);
}


/* DEFPROPERTY */

void DefProperty(void)
{
	char additive = 0;
	char complex = 0;
	int i;

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	for (i=1; i<=2; i++)
	{
		if (word[3][0]!='\0')
		{
			if (!strcmp(word[3], "$additive"))
				{additive = true;
				KillWord(3);}
			if (!strcmp(word[3], "$complex"))
				{complex = true;
				KillWord(3);}
		}
	}

	CheckSymbol(word[2]);

	if (!strcmp(word[3], "alias"))
	{
		if (IDWord(4)!=PROP_T)
		{
			sprintf(line, "Not a property:  %s", word[4]);
			Error(line);
		}
		else
		{
			aliasof[aliasctr] = (unsigned)token_val + MAXATTRIBUTES;
			alias[aliasctr] = MakeString(word[2]);
			alias_hash[aliasctr] = FindHash(alias[aliasctr]);
			aliasctr++;

			if (aliasctr==MAXALIASES)
				{attrctr = MAXALIASES-1;
				sprintf(line, "Maximum of %d aliases exceeded", MAXALIASES);
				Error(line);}
		}
		KillWord(4);
		KillWord(3);
	}
	else
	{
		property[propctr] = MakeString(word[2]);
		property_hash[propctr] = FindHash(property[propctr]);
		token_val = 0;
		propdef[propctr] = 0;
		propadd[propctr] = 0;

		if (additive) propadd[propctr] = propadd[propctr] | (char)ADDITIVE_FLAG;
		if (complex) propadd[propctr] = propadd[propctr] | (char)COMPLEX_FLAG;

		/* If default value for the property is provided */
		if (word[3][0]!='\0')
		{
			propdef[propctr] = CheckInitializer(3);
		}
		KillWord(3);
		propctr++;

		if (propctr==MAXPROPERTIES)
			{attrctr = MAXPROPERTIES-1;
			sprintf(line, "Maximum of %d properties exceeded", MAXPROPERTIES);
			Error(line);}
	}

	if (words > 2) Error("Illegal property definition");
}


/* DEFREMOVALS */

void DefRemovals(void)
{
	unsigned int s;

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	RemoveCommas();

	while (words >= 2)
	{

		StripQuotes(word[2]);
		s = AddDictionary(word[2]);

		/* 1 = removal, 3 = punctuation (0 = synonym, 2 = compound) */
		if (!strcmp(word[1], "removal"))
			syndata[syncount].syntype = 1;
		else
			syndata[syncount].syntype = 3;
		syndata[syncount].syn1 = s;
		syndata[syncount].syn2 = 0;
		if (++syncount==MAXSPECIALWORDS)
		{
			syncount--;
			sprintf(line, "Maximum of %d special words exceeded", MAXSPECIALWORDS);
			Error(line);
		}

		KillWord(2);
	}
}


/* DEFSYNONYM */

void DefSynonym(void)
{
	unsigned int s, sof;

	/* Linking is illegal after definitions */
	if (!linked) linked = FAILED;

	Expect(3, "for", "keyword");

	if (words!=4) Error("Illegal synonym definition");

	StripQuotes(word[2]);
	StripQuotes(word[4]);

	s = AddDictionary(word[2]);
	sof = AddDictionary(word[4]);

	/* 0 = synonym, (1 = removal, 2 = compound, 3 = punctuation) */
	syndata[syncount].syntype = 0;
	syndata[syncount].syn1 = s;
	syndata[syncount].syn2 = sof;
	if (++syncount==MAXSPECIALWORDS)
	{
		syncount--;
		sprintf(line, "Maximum of %d special words exceeded", MAXSPECIALWORDS);
		Error(line);
	}
}


/* FINDHASH

	This is actually a bit of a misnomer, since it isn't really a
	hash table per se that is being used.  What is done is to come
	up with an _almost_ unique key for each token.  Only if the
	hash key matches against another token does a (more expensive)
	strcmp() need to be performed to verify the match.
*/

int FindHash(char *a)
{
	int i, hash = 0;

	for (i=0; a[i]!='\0'; i++)
		hash += tolower(a[i])*HASH_KEY;

	return hash;
}
