/*
	HCBUILD.C

	contains:

		BuildCode
		BuildEvent
		BuildObject
		BuildProperty
		BuildRoutine
		BuildVerb
		Inherit
		ReplaceRoutine

	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


/* Object information:
	parent, sibling, and child,
	attribute set(s),
	address in property table
*/
int *parent, *sibling, *child;
long *objattr[MAXATTRIBUTES/32];
unsigned int *objpropaddr;

/* For each object construction */
long attr[MAXATTRIBUTES/32], notattr[MAXATTRIBUTES/32];

/* Flag if object is replaced (or count if replaced more than once) */
char *oreplace;

char infileblock = 0;           /* true when in readfile/writefile */
char incompprop = 0;            /* true when in before/after/etc.  */


/* BUILDCODE

	This is the main coding loop, called when a block of code is
	to be compiled.  Here are made any decisions as to special
	handling of loops and other constructions; the default is
	simply to code the line as is by calling CodeLine().  The
	'from' argument is for when BuildCode() is called for a
	particular reason, such as from CodeLine().
*/

void BuildCode(unsigned char from)
{
	char checkcompprop;		/* for checking illegal coding     */
					/*   inside a complex property 	   */
					/*   block 			   */
	int i, hash;
	int enternest,			/* to know when this block is done */
		sinceif = 0;		/* catch hanging "elseifs"/"elses" */
	int block_finished = 0;

	long returnpos, skipaddr;	/* to relocate after coding an in- */
					/*   line property routine assign. */

	enternest = nest;
	do
	{
		if (full_buffer==1)	/* if the buffer is new...  */
			full_buffer = 0;
		else			/* ...or else get new words */
			GetWords();

		/* Now check to see if we've hit unreachable code; of course,
		   anything after a label can potentially be jumped to
		*/
		if (word[1][0]=='~')	/* a label */
			block_finished = 0;
		if ((block_finished==nest) && word[1][0]!='}')
		{
			Error("?Statement will never be reached");
			block_finished = 0;
		}

		/* Pop the leading brace if there is one... */
		if (word[1][0]=='{')
		{
			nest++;
			DoBrace();
		}

		/* ...or drop back a level if we've hit the end
		   of a block. */
		if (word[words][0]=='}') nest--;


		checkcompprop = 0;		/* will be tested later */


		/* Check if the following code is an anonymous routine, to be
		   assigned as a property later, i.e.:

			object.property = {...}

		   The engine will assign object.property the address of
		   the block of code.
		*/

		hash = FindHash(word[1]);

		if (word[words][0]=='=')
		{
			/* If the last word is "=", change it into

				object.property = <address>
			*/

			CodeLine();
			returnpos = codeptr;
			WriteCode(0,2);          /* write a blank address */

			BeginCodeBlock();

			nest = nest + 1;
			full_buffer = 1;

			Boundary();
			BuildCode(PROP_T);
			Boundary();

			/* The assignment will jump to codeptr instead of
			   running the property routine; store codeptr in
			   skipaddr so we can write it.
			*/
			skipaddr = codeptr;
			codeptr = returnpos;

			if (hlb) RememberAddr(codeptr, VALUE_T, (unsigned int)(skipaddr/address_scale));

			WriteCode((unsigned int)(skipaddr/address_scale), 2);
			codeptr = skipaddr;
		}

		else if (!strcmp(word[1], "local"))
		{
			DefLocals(0);
			full_buffer = 0;
		}

		else if (HashMatch(hash, token_hash[IF_T], word[1], "if"))
		{
			CodeIf();
			sinceif = 0;
		}

		else if (HashMatch(hash, token_hash[ELSEIF_T], word[1], "elseif")
			|| HashMatch(hash, token_hash[ELSE_T], word[1], "else"))
		{
			if (sinceif > 1)
			{
				sprintf(line, "'%s' statement does not follow 'if'/'elseif'", word[1]);
				Error(line);
			}
			sinceif = 0;
			CodeIf();
		}

		else if (HashMatch(hash, token_hash[SELECT_T], word[1], "select"))
			CodeSelect();

		else if (HashMatch(hash, token_hash[CASE_T], word[1], "case"))
                	Error("'case' without 'select'");

		else if (HashMatch(hash, token_hash[WHILE_T], word[1], "while"))
			CodeWhile();

		else if (HashMatch(hash, token_hash[DO_T], word[1], "do"))
			CodeDo();

		else if (HashMatch(hash, token_hash[FOR_T], word[1], "for"))
			CodeFor();

		else if (HashMatch(hash, token_hash[WINDOW_T], word[1], "window"))
			CodeWindow();

		else if (HashMatch(hash, token_hash[WRITEFILE_T], word[1], "writefile")
			|| HashMatch(hash, token_hash[READFILE_T], word[1], "readfile"))
		{
			/* Cannot nest file i/o blocks within one another */

			if (infileblock)
				Error("Illegally nested file i/o block");
			else CodeFileIO();
		}

		else if (HashMatch(hash, token_hash[JUMP_T], word[1], "jump") ||
			word[1][0]=='~')
		{
			if (from==WINDOW_T || from==WRITEFILE_T)
			{
				sprintf(line, "'jump' or label illegal in '%s' block", token[from]);
				Error(line);
			}

			if (word[1][0]=='~') goto ChecktheLine;
			else goto CodetheLine;
		}

		else
                {
			/* First of all, check to see if we're coding a line
			   that will never be reached.  A previous 'return',
			   'jump', or 'break' at this nesting level is the signal,
			   which is erased if a label is encountered.
			*/
ChecktheLine:
			if (HashMatch(hash, token_hash[RETURN_T], word[1], "return") ||
				HashMatch(hash, token_hash[JUMP_T], word[1], "jump") ||
				HashMatch(hash, token_hash[BREAK_T], word[1], "break"))
			{
			  	block_finished = nest;
			}

			/* Here, the default is simply to code the line.
			   The checkcompprop flag is set to true because
			   none of the above constructions is legal in a
			   complex property outside the code block itself.
			*/
CodetheLine:
			CodeLine();
			full_buffer = 0;
			checkcompprop = true;
		}

		if (incompprop)
		{
			if (!checkcompprop)
				Error("Illegal in complex property outside code block");
		}

		sinceif++;

	} while (nest >= enternest);

	/* Once the block has been coded, check to see if any locals
	   were declared and not used.
	*/
	if (enternest==1)
	{
        	for (i=0; i<localctr; i++)
		{
			if (unused[i])
			{
				words = 0;
				sprintf(line, "?Unused local variable \"%s\"",  local[i]);
				Error(line);
			}
		}
	}

	return;
}


/* BUILDEVENT */

void BuildEvent(void)
{
	int i;
	char evin[33];		/* event in object, or 0 for global event */

	/* Allow:

		event
		event <object>
		event in <object>
	*/
	if (!strcmp(word[2], "in"))
		strcpy(evin, word[3]);
	else
	{
		strcpy(evin, word[2]);
	}

	GetWords();
	DoBrace();
	nest = 1;

	Boundary();
	eventaddr[eventctr] = (unsigned int)(codeptr/address_scale);

	/* If an object is supplied to attach the event to... */
	if (strcmp(evin, ""))
	{
		sprintf(object_id, "event in %s", evin);
		for (i = 0; i < objects; i++)
		{
			   /* what it is attached to */

			   if (!strcmp(object[i], evin))
				{eventin[eventctr] = i;
				break;}
		}

		if (i==objects)
			Error("No such object");
	}

	/* ...or else it is a global event */
	else
	{
		eventin[eventctr] = 0;
		strcpy(object_id, "event");
	}

	arguments = 0;	// why was this 1?
	ClearLocals();
	full_buffer = 1;

	/* Build the actual event code */
	BuildCode(0);

	return;
}


/* BUILDOBJECT

   	Called when the compiler encounters an object or class declaration,
   	and where all the contained properties and attributes are assigned.
	If the parameter is non-zero, properties and attributes are
	inherited (first) from the given object.
*/

void PropertyAlreadyDefined(int i)
{
	sprintf(line, "?Property \"%s\" already defined", property[i]);
	Error(line);
}

void BuildObject(int from)
{
	char objtype[8];
        int notflag;
	int i, j;
	int objstack[16],
		objstackcount = 0;	/* for multiple-object inheritance */

	int inobj = 0, linesdone = 0,
		isnot = 0,		/* isnot is true if attributes are */
					/*   being enumerated as beginning */
					/*   with "is not..."		   */
		flag = 0;
	long tempattr[MAXATTRIBUTES/32];

	if (strcmp(word[1], "class"))
		strcpy(objtype, "object");
	else
		strcpy(objtype, "class");
	KillWord(1);

	for (i=0; i<MAXATTRIBUTES/32; i++)
		attr[i] = 0, notattr[i] = 0;

	/* Clear property-set flags */
	for (i=0; i<propctr; i++)
		propset[i] = 0;

	sprintf(object_id, "%s %s", objtype, word[1]);
	KillWord(1);

	/* If this is a previous version of the object that needs
	   replacing
	*/
	if (oreplace[objectctr])
	{
		i = 0;
		do
		{
			GetWords();
			if (word[1][0]=='{') i++;
			if (word[1][0]=='}') i--;
			if (word[1][0]=='~') labelctr++;
		}
		while (i);
		oreplace[objectctr]--;
		return;
	}


	/* Write object name if given as:  object <object> "<name>" */
	if ((words) && word[1][0]=='\"')
	{
		StripQuotes(word[1]);
		strcpy(line, word[1]);
		KillWord(1);

		/* property 0 (name) */
		SavePropData(0);

		/* 1 data word */
		SavePropData(1);

		/* Write the dictionary position */
		SavePropData(AddDictionary(line));

		if (!(propadd[0] & ADDITIVE_FLAG)) propset[0] = 1;
		proptable = proptable + 4;
	}

	/* Since it's possible to put an "in" or "nearby" directive on
	   the same line as the declaration, only get a new line if there
	   are no more words remaining on this one.
	*/
	if (!words)
	{
		GetWords();
		DoBrace();
		inobj = true;
	}

	nest = 1;

	do
	{
		/* linesdone is gives the number of lines already
		   processed */
		if (linesdone++)
		{
			GetWords();
		}

		/* In an object definition, the following are valid:

			1.  inherits <class>[, <class2>...]
			2.  in <object> | nearby [<object>]
			3.  is <attribute>[, <attribute2>...]
			4.  <property> <value>
			5.  <property>
				{...property routine...}
		*/

		/* If a parent is specified */

		if (!strcmp(word[1], "in") || !strcmp(word[1], "nearby"))
		{
			if (parent[objectctr])
			{
				sprintf(line, "\"%s\" already placed in object tree", object[objectctr]);
				Error(line);
			}

			if (!strcmp(word[1], "nearby") && !strcmp(word[2], "") && objectctr)
				PutinTree(objectctr, parent[objectctr - 1]);
			else
			{
				for (i=0; i<objects; i++)
				{
					if (!strcmp(word[2], object[i]))
					{
						if (!strcmp(word[1], "nearby"))
							PutinTree(objectctr, parent[i]);
						else
							PutinTree(objectctr, i);
						break;
					}
				}
				if (i==objects)
					Error("No such object");
			}

			if (words>2) Expect(3, "NULL", "end-of-line after object");

			/* Just in case "in" or "nearby" comes on the first line of
			   the object declaration
			*/
			if (linesdone==1 && !inobj)
			{
				GetWords();
				DoBrace();
				inobj = true;
				linesdone = 0;
				continue;
			}

			goto NextCase;
		}


		/* Inheriting additional classes */

		if (!strcmp(word[1], "inherits"))
		{
			KillWord(1);

			while (words)
			{
				int t = IDWord(1);
				if (t==COMMA_T)
				{
					KillWord(1);
					t = IDWord(1);
				}
				KillWord(1);
				
				if (t!=OBJECTNUM_T)
					Error("Cannot inherit from non-object/class");
				else if (objstackcount==16)
					Error("Too many classes:  Maximum is 16");
				else
					objstack[objstackcount++] = token_val;
			}
			goto NextCase;
		}


		/* Attributes */

		if (!strcmp(word[1], "is"))
		{
			RemoveCommas();

			for (i=2; i<=words; i++)
			{
				if (isnot==1)
				{
					for (j=0; j<MAXATTRIBUTES/32; j++)
						notattr[j] = attr[j],
						attr[j] = tempattr[j];
					isnot = 0;
				}

				if (word[i][0]=='}') goto LeaveBuildObject;

				notflag = false;
				if (!strcmp(word[i], "not"))
				{
					isnot = 1;
					for (j=0; j<MAXATTRIBUTES/32; j++)
						tempattr[j] = attr[j],
						attr[j] = notattr[j];
					i++;
					notflag = true;
				}

				flag = 0;

				/* If it's a valid attribute */
				for (j=0; j<attrctr; j++)
				{
					if (!strcmp(attribute[j], word[i]))
					{
						SetAttribute(j);
						flag = 1;
						break;
					}
				}

				/* Check if it's a valid attribute alias,
				   since we haven't matched an attribute. */
				if (flag==0)
				{
					for (j=0; j<aliasctr; j++)
					{
						if (!strcmp(alias[j], word[i]))
						{
							SetAttribute(aliasof[j]);
							flag = 1;

							break;
						}
					}
				}

				/* If not a valid attribute or alias */
				if (flag == 0)
				{
					sprintf(line, "Undefined attribute \"%s\"", word[i]);
					Error(line);
				}
			}

			if (isnot==1)
			{
				for (j=0; j<MAXATTRIBUTES/32; j++)
					notattr[j] = attr[j],
					attr[j] = tempattr[j];
				isnot = 0;
			}

			goto NextCase;
		}

		if (word[1][0]=='}')
			{nest--;
			goto NextCase;}


		/* Haven't hit a keyword or attribute yet; check
		   for properties
		*/
		flag = 0;
		for (i=0; i<propctr; i++)
		{
			if (!strcmp(word[1], property[i]))
			{
				if (propset[i]) PropertyAlreadyDefined(i);
				
				BuildProperty(objectctr, i);

				/* If not an additive property */
				if (!(propadd[i]&ADDITIVE_FLAG)) propset[i] = 1;
				flag = 1;
				break;
			}
		}


		/* Not a property, so check for a property alias */

		if (flag==0)
		{
			for (i=0; i<aliasctr; i++)
			{
				if (!strcmp(word[1], alias[i]))
				{
					if (propset[aliasof[i]-MAXATTRIBUTES])
						PropertyAlreadyDefined(aliasof[i]-MAXATTRIBUTES);
					
					BuildProperty(objectctr, aliasof[i]-MAXATTRIBUTES);

					/* If not an additive property */
					if (!(propadd[aliasof[i]-MAXATTRIBUTES] & ADDITIVE_FLAG))
						propset[aliasof[i]-MAXATTRIBUTES] = 1;
					flag = 1;
					break;
				}
			}
		}

		/* Neither a property or an alias */
		if (flag == 0)
		{
			sprintf(line, "Undefined property \"%s\"", word[1]);
			Error(line);
		}

NextCase:;

	} while (nest > 0);

LeaveBuildObject:

	/* Check if the main class object (if any) to see if anything needs
	   to be inherited...
	*/
	if (from > 0) Inherit(from);

	/* ...then run through the to-be-inherited stack (if any) */
	while (objstackcount) Inherit(objstack[--objstackcount]);

	/* Finally, if no textual name was given, create one: */
	if (!propset[0])
	{
		sprintf(line, "(%s)", object[objectctr]);

		SavePropData(0);
		SavePropData(1);
		SavePropData(AddDictionary(line));

		if (!(propadd[0] & ADDITIVE_FLAG)) propset[0] = 1;
		proptable = proptable + 4;
	}

	return;
}


/* BUILDPROPERTY */

void BuildProperty(int obj, int p)
{
	char old_object_id[64];
	int i, j, neg, neg_count, t;
	unsigned int pin;	/* pin is used to check table overflow */

	pin = proptable;

	RemoveCommas();

	arguments = 0;
	ClearLocals();

	/* Write the property number */
	SavePropData(p);

	/* In an object, "<property> = <value>" is illegal; it's just
	   "<property> <value>"
	*/
	if (word[2][0]=='=') Error("Invalid assignment:  =");

	/* A simple property assignment */
	if (words > 1)
	{
		/* If the number of elements is given up front, make
		   space for them, assume they're blank, and get out
		*/
		if (word[2][0]=='#')
		{
			if (words!=3)
			{
				Error("Expecting length value after:  #");
				return;
			}
			IDWord(3);
			t = token_val;

			/* Have to check this lest, with very long properties,
			   we run into confusion with the high-numbered marker
			   used for PROP_LINK_ROUTINE/LINK_ROUTINE
			*/
			if (t >= PROP_LINK_ROUTINE)
			{
				sprintf(line, "Properties limited to %d elements", PROP_LINK_ROUTINE-1);
				Error(line);
				return;
			}

			SavePropData(t);		/* length */
			for (i=0; i<t; i++)
				SavePropData(0);	/* blank data */
			proptable = proptable + (t+1) * 2;
			return;
		}

		if (word[words][0]=='}')
			{KillWord(words);
			nest--;}

		/* neg counts the number of '-' symbols in order to get
		   an accurate word count (and '-' is parsed as a
		   separate word.
		*/
		neg = 0;
		for (i=2; i<=words; i++) if (word[i][0]=='-') neg++;

		/* Have to check this lest, with very long properties,
		   we run into confusion with the high-numbered marker
		   used for PROP_LINK_ROUTINE/LINK_ROUTINE
		*/
		if (words-1-neg >= PROP_LINK_ROUTINE)
		{
			sprintf(line, "Properties limited to %d elements", PROP_LINK_ROUTINE-1);
			Error(line);
			return;
		}
		neg_count = neg;

		/* Write the number of data words */
		SavePropData(words-1-neg);

		for (i=2; i<=words; i++)
		{
			if (word[i][0]=='.')
				Error("?Property reference undefined at compile time");
			else if (word[i][0]=='&')
				Error("Address reference illegal at compile time");

                        neg = 1;
			if (word[i][0]=='-')
				{neg = -1;
				i++;}

			if ((t = IDWord(i))==ROUTINE_T)
                        	Error("?Routine reference undefined at compile time");

			/* Presumably a variable name in a property is meant
			   to refer to the variable value, not its number
			*/
			else if (t==VAR_T && token_val<MAXGLOBALS)
				token_val = globaldef[token_val];

			else if (t==TRUE_T || t==FALSE_T)
			{
				if (t==TRUE_T) token_val = 0x01;
				else token_val = 0x00;
			}
			
			/* Check for uppercase characters in noun/adjective 
			   property */
			else if ((p==3 || p==4) && t==DICTENTRY_T)
			{
				for (j=0; word[i][j]!='\0'; j++)
				{
					if (word[i][j]>='A' && word[i][j]<='Z')
					{
						sprintf(line,
							"?Uppercase character(s) in %s",
							property[p]);
						Error(line);
						break;
					}
				}
			}

			else if (token_val==0xffff)
				Error("Illegal as property value");

			SavePropData((unsigned int)neg*(unsigned int)token_val);
		}
		proptable = proptable + (words-neg_count) * 2;
	}

	/* If it's not a simple property assignment, it must be a property
	   routine, in which case a block of code is compiled at the current
	   code address, and that address is stored as the property value.
	*/
	else
	{
		strcpy(old_object_id, object_id);
		sprintf(object_id, "%s.%s", object[obj], property[p]);

		/* Flag for one word of code */
		SavePropData(PROP_ROUTINE);

		BeginCodeBlock();

		Boundary();

		/* Address of the routine */
		SavePropData((unsigned int)(codeptr/address_scale));

		nest = 2;
		full_buffer = 1;
		if (propadd[p]&COMPLEX_FLAG) incompprop = true;
		BuildCode(0);

		/* Write a "return true", which is the default behavior of
		   property routines upon completion.
		*/
		WriteCode(RETURN_T, 1);
		WriteCode(TRUE_T, 1);
		WriteCode(EOL_T, 1);

		incompprop = false;
		nest = 1;

		proptable = proptable + 4;

		strcpy(object_id, old_object_id);
	}

	if (proptable < pin) Error("Property table exceeds 64K");
}


/* BUILDROUTINE */

void BuildRoutine(void)
{
	int i;

	sprintf(object_id, "routine %s", word[2]);

	/* If this is a previous version of the routine that needs
 	   replacing
	*/
	if (rreplace[routinectr])
	{
		nest = 0;
		do
		{
			GetWords();
			if (word[1][0]=='{') nest++;
			if (word[1][0]=='}') nest--;
			if (word[1][0]=='~') labelctr++;
		}
		while (nest);
		rreplace[routinectr]--;
		return;
	}

	Boundary();
	raddr[routinectr] = (unsigned int)(codeptr/address_scale);

	if (!strcmp(word[2], "init")) initaddr = raddr[routinectr];
	if (!strcmp(word[2], "main")) mainaddr = raddr[routinectr];
	if (!strcmp(word[2], "parse")) parseaddr = raddr[routinectr];
	if (!strcmp(word[2], "parseerror")) parseerroraddr = raddr[routinectr];
	if (!strcmp(word[2], "findobject")) findobjectaddr = raddr[routinectr];
	if (!strcmp(word[2], "endgame")) endgameaddr = raddr[routinectr];
	if (!strcmp(word[2], "speakto")) speaktoaddr = raddr[routinectr];
	if (!strcmp(word[2], "perform")) performaddr = raddr[routinectr];

	/* If any arguments are given, as in "Routine(a, b, c)", then
	   a, b, and c must be defined as locals. */
	arguments = 0;
	if (word[3][0]=='(')		/* count arguments */
	{
		for (i=1; i<3; i++)
			KillWord(1);
		KillWord(words);	/* ')' */
		word[1] = "";

		DefLocals(1);

		arguments = localctr;
		for (i=0; i<arguments; i++)
			unused[i] = 0;
	}

	/* Then just go ahead and build the block of code as normal */
	GetWords();
	DoBrace();
	nest = 1;
	ClearLocals();
	full_buffer = 1;
	BuildCode(0);
}


/* BUILDVERB

	To be honest, BuildVerb() really only codes the first line of a
	verb definition.  All the following lines are handled by CodeLine(),
	as called by BuildCode().
*/

void BuildVerb(void)
{
	char verb[8];
	int i, n;
	char objflag = 0;

	strcpy(verb, word[1]);
	KillWord(1);

	RemoveCommas();

	if (!strcmp(verb, "verb")) WriteCode(VERB_T, 1);
	else WriteCode(XVERB_T, 1);

	sprintf(object_id, "%s %s", verb, word[1]);

	n = words;
	WriteCode((unsigned int)n, 1);        	/* number of verb synonyms */

	for (i=1; i<=words; i++)      		/* write them */
	{
		/* If a dictionary word (in quotes)... */
		if (word[i][0]=='\"')
		{
			StripQuotes(word[i]);
			WriteCode(AddDictionary(word[i]), 2);
		}

		/* ...otherwise assume it's an object number (i.e., a value) */

		/* (This will rarely, rarely be the case, as it is not the
		   typical method of grammar definition in Hugo)
		*/
		else
		{
			int t ;
			WriteCode(0xffff, 2);	/* flag for non-word */
			WriteCode((t = IDWord(i)), 1);
			WriteCode((unsigned int)token_val, 2);

			if (!objflag)
			{
				switch (t)
				{
					case OBJECTNUM_T:
					case ROUTINE_T:
					case OPEN_BRACKET_T:
					case VALUE_T:
					case VAR_T:
					case ARRAYDATA_T:
						break;
					default:
						Error("Illegal in verb definition header");
				}
			}

			objflag = 1;
		}
	}

	verbs = verbs + 1;
}


/* INHERIT

   	Inherits the properties and attributes of <obj> for the current
	object, assuming that the property has not been assigned already,
	or that the attribute has not been specifically excluded via
	"is not <attribute>"
*/

void Inherit(int obj)
{
	int i, p = 0, n;
	unsigned int a, d;

	if (obj >= objectctr)
	{
		sprintf(line, "?Cannot inherit from unbuilt \"%s\"", object[obj]);
		Error(line);
		return;
	}

	/* Copy attributes */
	for (i=0; i<MAXATTRIBUTES/32; i++)
		attr[i] = attr[i] | (objattr[i][obj] & ~notattr[i]);

	a = oprop[obj];

	/* A brute-force way of skimming through the property table from
	   the entry point of obj, the object from which properties are
	   being inherited.
	*/
	while (p != PROP_END)
	{
		/* Read property number */
		p = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
		a++;

		if (p != PROP_END)
		{
			/* Read number of words */
			n = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
			a++;

			/* Copy the property so long as it hasn't already
			   been defined in the new object.
			*/
			if (!propset[p])
			{
				/* Write property number and count */
				SavePropData(p);
				SavePropData(n);
			}

			if (n==PROP_ROUTINE || n==PROP_LINK_ROUTINE) n = 1;

			for (i=1; i<=n; i++)
			{
				d = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
				a++;
				if (!propset[p]) SavePropData(d);
			}
			if (!propset[p]) proptable += 2+n*2;

			if (!(propadd[p]&ADDITIVE_FLAG)) propset[p] = 1;
		}
	}
}


/* REPLACEROUTINE */

void ReplaceRoutine(void)
{
	int i;
	int flag = 0, tempctr;

	for (i=0; i<routinectr; i++)
	{
		if (!strcmp(routine[i], word[2]))
		{
			flag = 1;
			rreplace[i]++;
			break;
		}
	}

	if (flag && passnumber>1)
		{tempctr = routinectr, routinectr = i;
		rreplace[i] = 0;
		BuildRoutine();
		routinectr = tempctr;}
}
