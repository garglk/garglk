/*
	HCCOMP.C

	contains Compiler directive routines
	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


/* Function prototypes: */
void CompilerIf(void);

#define MAXIFSETNEST    32
char ifset[MAXIFSETNEST];
int ifsetnest = 0;


/* COMPILERDIRECTIVE */

void CompilerDirective(void)
{
	char source[MAXPATH];
	int temptotal;
	double testver, comparever;
	long sourcepos;

	if (!strcmp(word[1], "#if") || !strcmp(word[1], "#else") ||
		!strcmp(word[1], "#elseif") ||
		!strcmp(word[1], "#ifset") || !strcmp(word[1], "#ifclear"))
		CompilerIf();

	else if (!strcmp(word[1], "#set") || !strcmp(word[1], "#clear"))
		CompilerSet();

	else if (!strcmp(word[1], "#endif"))
	{
		if (--ifsetnest < 0) ifsetnest = 0;
	}

	else if (!strcmp(word[1], "#version"))
	{
		testver = strtod(word[2], NULL);
		if (testver==0)
			Error("Invalid version specifier; use:  #version x.x");
		else
		{
			comparever = (double)HCVERSION + (double)HCREVISION/10;
#if !defined (COMPILE_V25)
			/* v3.0 and later can use v2.5 library files */
			if (comparever>=3.0 && testver==2.5)
				comparever = 2.5;
#endif

			if (testver > comparever)
			{
				sprintf(line, "?File requires compiler version %s", word[2]);
				Error(line);
			}
			else if (testver < comparever)
			{
				sprintf(line, "?Compiler is v%d.%d; file is only v%s", HCVERSION, HCREVISION, word[2]);
				Error(line);
			}
		}
	}

	else if (!strcmp(word[1], "#message"))
	{
		if (!strcmp(word[2], "warning"))
		{
			StripQuotes(word[3]);
			sprintf(line, "?%s", word[3]);
			Error(line);
		}
		else if (!strcmp(word[2], "error"))
		{
			StripQuotes(word[3]);
			Error(word[3]);
		}
		else
		{
			StripQuotes(word[2]);
			if (percent || expandederr) Printout("");
			Printout(word[2]);
		}
	}

	else if (!strcmp(word[1], "#switches"))
	{
		if (word[2][0]=='-')
			SetSwitches(word[2]+1);
		else
			SetSwitches(word[2]);
	}

	else if (!strcmp(word[1], "#link"))
		LinkerPass1();

	else if (!strcmp(word[1], "#include"))
	{
		StripQuotes(word[2]);
		strcpy(source, sourcefilename);
		sourcepos = ftell(sourcefile);
		if (ferror(sourcefile)) FatalError(READ_E, sourcefilename);
		fclose(sourcefile);

		temptotal = totallines;
		totallines = 0;

		if (percent)
			{sprintf(line, "\rCompiling       lines of %s", sourcefilename);
			strnset(line+26, ' ', strlen(sourcefilename));
			printf(line);}

		strcpy(sourcefilename, word[2]);
		if (!(sourcefile = TrytoOpen(sourcefilename, "rb", "source")))
		{
			if (!(sourcefile = TrytoOpen(sourcefilename, "rb", "lib")))
				FatalError(OPEN_E, sourcefilename);
		}
		Pass1();

		if (percent)
		{
			printf("\n");
			if (listing)
			{
				if (fprintf(listfile, "Compiling %5d lines of %s\n", totallines, sourcefilename) < 0)
					FatalError(WRITE_E, listfilename);
			}
#if defined (STDPRN_SUPPORTED)
			if (printer)
			{
				if (fprintf(stdprn, "Compiling %5d lines of %s\n", totallines, sourcefilename) < 0)
					FatalError(WRITE_E, "printer");
			}
#endif
		}

		totallines = temptotal;
		if (fclose(sourcefile)==EOF) FatalError(READ_E, sourcefilename);
		strcpy(sourcefilename, source);
		if (!(sourcefile = HUGO_FOPEN(sourcefilename, "rb"))) FatalError(OPEN_E, sourcefilename);
		if (fseek(sourcefile, sourcepos, SEEK_SET)) FatalError(READ_E, sourcefilename);

		if (fprintf(allfile, "!%s\n", sourcefilename)<0) FatalError(WRITE_E, allfilename);
	}
	else
		Error("Unknown compiler directive");
}


/* COMPILERIF

	Basically handles the constructions "#if", "#ifset", "#ifclear",
	"#elseif", and "#else".  If a condition is met, ifset[ifsetnest]
	is set to true.  This is used by any "else"-type flag-check to
	see if the block should be included or ignored.
*/

extern char silent_IDWord;              /* from hccode.c */

void CompilerIf(void)
{
	int i, n;

	if (!strcmp(word[1], "#if"))
	{
TranslateIf:
		if (!strcmp(word[2], "clear"))
			word[1] = "#ifclear";
		else if (!strcmp(word[2], "set"))
			word[1] = "#ifset";
		else if (!strcmp(word[2], "defined"))
			word[1] = "#ifdefined";
		else if (!strcmp(word[2], "undefined"))
			word[1] = "#ifundefined";
		else
		{
			word[1] = "#ifset";
			goto CheckConditions;
		}

		KillWord(2);    /* everything except straight "#if" */
	}

CheckConditions:

	if (!strcmp(word[1], "#ifset"))
	{
		if (++ifsetnest>=MAXIFSETNEST)
		{
IfSetError:
			ifsetnest = MAXIFSETNEST-1;
			sprintf(line, "#if nested more than %d deep", MAXIFSETNEST-1);
			Error(line);
			return;
		}

		for (i=0; i<MAXFLAGS; i++)
			if (!strcmp(word[2], sets[i]))
			{
				ifset[ifsetnest] = true;
				return;
			}

		ifset[ifsetnest] = false;
	}

	else if (!strcmp(word[1], "#ifclear"))
	{
		if (++ifsetnest>=MAXIFSETNEST) goto IfSetError;

		for (i=0; i<MAXFLAGS; i++)
			if (!strcmp(word[2], sets[i]))
			{
				ifset[ifsetnest] = false;
				goto SkiptoEndif;
			}

		ifset[ifsetnest] = true;
		return;
	}

	else if (!strcmp(word[1], "#ifdefined"))
	{
		silent_IDWord = true;
		n = IDWord(2);
		silent_IDWord = false;

		if (n)
		{
			ifset[ifsetnest] = true;
			return;
		}

		ifset[ifsetnest] = false;
	}

	else if (!strcmp(word[1], "#ifundefined"))
	{
		silent_IDWord = true;
		n = IDWord(2);
		silent_IDWord = false;

		if (!n)
		{
			ifset[ifsetnest] = true;
			return;
		}

		ifset[ifsetnest] = false;
	}

	else if (!strcmp(word[1], "#elseif"))
	{
		if (ifsetnest==0)
		{
NoIfError:
			Error("#else/#elseif without #if");
			return;
		}

		if (ifset[ifsetnest]) goto SkiptoEndif;
		else goto TranslateIf;
	}

	else if (!strcmp(word[1], "#else"))
	{
		if (ifsetnest==0) goto NoIfError;
		if (ifset[ifsetnest]) goto SkiptoEndif;
		else
			/* Use the rest of this #else block */
			return;
	}

SkiptoEndif:

	n = 1;
	while (n > 0)
	{
		if (feof(sourcefile)) FatalError(EOF_ENDIF_E, sourcefilename);
			
		GetLine(0);
		if (word[1][0]=='#')
		{
			if (!strcmp(word[1], "#if") ||
				!strcmp(word[1], "#ifset") || !strcmp(word[1], "#ifclear"))
				n++;
			else if (!strcmp(word[1], "#endif"))
			{
				if (--n==0)
					if (--ifsetnest < 0) ifsetnest = 0;
			}
			else if (!strcmp(word[1], "#else") && n==1 && !ifset[ifsetnest])
				/* Use the rest of the #else block */
				return;
			else if (!strcmp(word[1], "#elseif") && n==1 && !ifset[ifsetnest])
			{
				KillWord(1);
				goto TranslateIf;
			}
		}
	}
}


/* COMPILERMEM

	Deals with all "$<limit>=<new limit>" commands.
*/

void CompilerMem(void)
{
	char e[64];

	strcpy(e, "");

	if (!strcmp(word[1], "$list"))
		{ListLimits();
		return;}
	else if (words>1) /* (word[2][0]!='=' || words!=3) */
	{
		/* Make "$a" "=" "b" into "$a=b" */
		if (word[2][0]=='=')
		{
			strcpy(buffer, word[1]);
			strcat(buffer, word[2]);
			strcat(buffer, word[3]);
			word[1] = buffer;
			words = 1;
		}
		else
		{
			Error("Illegal inline limit setting");
			return;
		}
	}

	if (!strncmp(word[1], "$maxobjects", 11))
	{
		if (objectctr) strcpy(e, "Object(s)");
		else
		{
			free(object);
			free(object_hash);
			free(oprop);
			free(objattr);
			free(objpropaddr);
			free(parent);
			free(sibling);
			free(child);
			alloc_objects = 0;
		}
	}
	else if (!strncmp(word[1], "$maxproperties", 14))
	{
		if (propctr) strcpy(e, "Property/properties");
		else
		{
			free(property);
			free(property_hash);
			free(propdef);
			free(propset);
			free(propadd);
			alloc_properties = 0;
		}
	}
	else if (!strncmp(word[1], "$maxaliases", 11))
	{
		if (aliasctr) strcpy(e, "Alias/aliases");
		else
		{
			free(alias);
			free(alias_hash);
			free(aliasof);
			alloc_aliases = 0;
		}
	}
	else if (!strncmp(word[1], "$maxarrays", 10))
	{
		if (arrayctr) strcpy(e, "Array(s)");
		else
		{
			free(array);
			free(array_hash);
			free(arrayaddr);
			alloc_arrays = 0;
		}
	}
	else if (!strncmp(word[1], "$maxconstants", 13))
	{
		if (constctr) strcpy(e, "Constant(s)");
		else
		{
			free(constant);
			free(constant_hash);
			free(constantval);
			alloc_constants = false;
		}
	}
	else if (!strncmp(word[1], "$maxevents", 10))
	{
		if (eventctr) strcpy(e, "Event(s)");
		else
		{
			free(eventin);
			free(eventaddr);
			alloc_events = 0;
		}
	}
	else if (!strncmp(word[1], "$maxroutines", 12))
	{
		if (routinectr) strcpy(e, "Routine(s)");
		else
		{
			free(routine);
			free(routine_hash);
			free(raddr);
			free(rreplace);
			alloc_routines = 0;
		}
	}
	else if (!strncmp(word[1], "$maxlabels", 10))
	{
		if (labelctr) strcpy(e, "Label(s)");
		else
		{
			free(label);
			free(label_hash);
			free(laddr);
			alloc_labels = 0;
		}
	}
	else if (!strncmp(word[1], "$maxdict", 8))
	{
		if (dictcount > 3) strcpy(e, "Dictionary entry/entries");
		else
		{
			free(lexentry[1]);
			free(lexentry[2]);
			free(lexentry);
			free(lexnext);
			free(lexaddr);
			alloc_dict = 0;
		}
	}
	else if (!strncmp(word[1], "$maxspecialwords", 16))
	{
		if (syncount) strcpy(e, "Synonyms/compounds/removals/punctuation");
		else
		{
			free(syndata);
			alloc_syn = 0;
		}
	}
	else if (!strncmp(word[1], "$maxflags", 9))
	{
		free(sets);
		alloc_sets = 0;
	}
	else if (!strncmp(word[1], "$maxdirectories", 15))
	{
		if (directoryctr) strcpy(e, "Directory/directories");
		else
		{
			free(directory);
			alloc_directories = 0;
		}
	}

	if (strcmp(e, ""))              /* if there was an error */
	{
		sprintf(line, "%s already defined", e);
		Error(line);
	}
	else
	{
		SetLimit(word[1]);
		SetMem();
	}
}


/* COMPILERSET */

void CompilerSet(void)
{
	int i;

	if (strlen(word[2]) > 32)
		{sprintf(line, "Flag name \"%s\" too long", word[2]);
		Error(line);
		return;}

	if (!strcmp(word[1], "#set"))
	{
		for (i=0; i<MAXFLAGS; i++)
		{
			if (!strcmp(sets[i], word[2]) || !strcmp(sets[i], ""))
			{
				free(sets[i]);
				sets[i] = MakeString(word[2]);
				/* In case this came from the non-
				   lowercased command line:
				*/
				strlwr(sets[i]);
				return;
			}
		}

		sprintf(line, "Maximum of %d compiler flags exceeded", MAXFLAGS);
		Error(line);
	}
	if (!strcmp(word[1], "#clear"))
	{
		for (i=0; i<MAXFLAGS; i++)
		{
			if (!strcmp(word[2], sets[i]))
			{
				free(sets[i]);
				sets[i] = MakeString("");
				return;
			}
		}

		sprintf(line, "Flag not set:  %s", word[2]);
		Error(line);
	}
}
