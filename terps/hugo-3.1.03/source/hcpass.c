/*
	HCPASS.C

	contains:

		Pass1
		Pass2
		Pass3
                WriteAdditionalData

	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


/* Function prototypes: */
void PrintFrameLine(void);
void WriteAdditionalData(void);

unsigned int codestart = 0;     /* of executable code              */
char endofgrammar = 0;          /* true after grammar is defined   */
int passnumber = 0;             /* number of current pass          */

/* Initial counts of various data types */
int objinitial = 0, routineinitial = 0, eventinitial = 0, labelinitial = 0,
	propinitial = 0;

/* Totals */
int objects = 0, routines = 0, events = 0, labels = 0;

unsigned int initaddr = 0;        /* address of "init" routine  */
unsigned int mainaddr = 0;        /* address of "main" routine  */
unsigned int parseaddr = 0;       /* address of "parse" routine */
unsigned int parseerroraddr = 0;  /* address of "parseerror"    */
unsigned int findobjectaddr = 0;  /* address of "findobject"    */
unsigned int endgameaddr = 0;	  /* address of "endgame"       */
unsigned int speaktoaddr = 0;	  /* address of "speakto"       */
unsigned int performaddr = 0;	  /* address of "perform"       */

unsigned int linkproptable = 0;

unsigned int textcount = 0;	/* for linking text bank */


/* PASS1

   	Define all objects, routines, property names, attribute names, etc.
	so that they are recognizable to the compiler during Pass2().
	Here, everything contained in an object, routine, or event is
	written into the temporary allfile, which is a contiguous master
	file of parsed lines (i.e. broken down into tokens).
*/

void Pass1(void)
{
	char sourceupper[MAXPATH];
	char linkedgrammar = 0;
	int i;
	int hash, flag;

	if (percent) printf("\r");

	passnumber = 1;

	/* Pass1() gets called for #included files, too */
	totalfiles++;

	strcpy(sourceupper, PRINTED_FILENAME(sourcefilename));

	if (fprintf(allfile, "!%s\n", sourcefilename)<0) FatalError(WRITE_E, allfilename);

	while (!feof(sourcefile))
	{
		GetLine(0);

		if (percent)
			printf("\rCompiling %5d lines of %s", totallines, sourceupper);

		if (words > 0)
		{
			/* The first word must match a valid compiler
			   directive */
			if (word[1][0]=='#')	  /* compiler directives */
				CompilerDirective();

			else if (word[1][0]=='$') /* memory settings */
				CompilerMem();

			else if (word[1][0]=='@')
				AddDirectory(word[1]);

			else if (!strcmp(word[1], "object") || !strcmp(word[1], "class"))
			{
DefineanObject:
				PrinttoAll();
				CheckSymbol(word[2]);
				object[objectctr] = MakeString(word[2]);
				object_hash[objectctr] = FindHash(object[objectctr]);
				parent[objectctr] = sibling[objectctr] =
					child[objectctr] = 0;
				oreplace[objectctr] = 0;

				objectctr++;

				if (objectctr==MAXOBJECTS)
					{objectctr = MAXOBJECTS-1;
					sprintf(line, "Maximum of %d objects exceeded", MAXOBJECTS);
					Error(line);}

				DefOther();
			}

			else if (!strcmp(word[1], "routine"))
			{
				PrinttoAll();
				CheckSymbol(word[2]);
				routine[routinectr] = MakeString(word[2]);
				routine_hash[routinectr] = FindHash(routine[routinectr]);
				rreplace[routinectr] = 0;
				raddr[routinectr] = 0;
				routinectr++;

				if (routinectr==MAXROUTINES)
					{routinectr = MAXROUTINES-1;
					sprintf(line, "Maximum of %d routines exceeded", MAXROUTINES);
					Error(line);}

				DefOther();
			}

			else if (!strcmp(word[1], "replace"))
			{
				flag = IDWord(2);
				if (flag==ROUTINE_T)
				{
					PrinttoAll();
					ReplaceRoutine();
					token_val = flag;
					DefOther();
				}
				else if (flag==OBJECTNUM_T)
				{
					if (token_val<objinitial)
					{
						sprintf(line, "?Precompiled references to old \"%s\" may be invalid", object[token_val]);
						Error(line);
					}

					sprintf(line, "<replaced \"%s\">", object[token_val]);
					free(object[token_val]);
					object[token_val] = MakeString(line);
					object_hash[token_val] = 0;
					oreplace[token_val]++;
					word[1] = "object";
					token_val = flag;

					goto DefineanObject;
				}
				else
				{
					sprintf(line, "Not an object or routine:  %s", word[2]);
					Error(line);
				}
			}

			else if (!strcmp(word[1], "event"))
			{
				if (hlb)
					Error("Event illegal in precompiled header");

				PrinttoAll();
				eventctr++;

				if (eventctr==MAXEVENTS)
					{eventctr = MAXEVENTS-1;
					sprintf(line, "Maximum of %d events exceeded", MAXEVENTS);
					Error(line);}

				DefOther();
			}

			else if (word[1][0]=='*' || !strcmp(word[1], "verb") || !strcmp(word[1], "xverb"))
			{
				if (hlb && !linkedgrammar)
					{Error("Grammar illegal in precompiled header");
					linkedgrammar = 1;}
		       		else if (endofgrammar)
					Error("Grammar definition after start of code");
				else
                                        PrinttoAll();
			}

			else if (!strcmp(word[1], "resource"))
			{
				PrinttoAll();
				DefOther();
			}

			else if (!strcmp(word[1], "attribute")) DefAttribute();
			else if (!strcmp(word[1], "property")) 	DefProperty();
			else if (!strcmp(word[1], "global")) 	DefGlobal();
			else if (!strcmp(word[1], "array")) 	DefArray();
			else if (!strcmp(word[1], "constant")) 	DefConstant();
			else if (!strcmp(word[1], "enumerate"))	DefEnum();

			else if (!strcmp(word[1], "compound"))  PrinttoAll();
			else if (!strcmp(word[1], "removal"))   PrinttoAll();
			else if (!strcmp(word[1], "synonym"))   PrinttoAll();
			else if (!strcmp(word[1], "punctuation")) PrinttoAll();


			/* Must be a user-defined object class */
			else
			{
				hash = FindHash(word[1]);

				flag = 0;
				for (i=0; i<objectctr; i++)
				{
					if (HashMatch(hash, object_hash[i], word[1], object[i]))
					{
						PrinttoAll();
						CheckSymbol(word[2]);
						object[objectctr] = MakeString(word[2]);
						object_hash[objectctr] = FindHash(object[objectctr]);
						parent[objectctr] = 0;
						sibling[objectctr] = 0;
						child[objectctr] = 0;
						objectctr++;

						if (objectctr==MAXOBJECTS)
							{objectctr = MAXOBJECTS-1;
							sprintf(line, "Maximum of %d objects exceeded", MAXOBJECTS);
							Error(line);}

						DefOther();
						flag = 1;
						break;
					}
				}

				if (!flag)
					{sprintf(line, "Unknown compiler directive:  %s", word[1]);
					Error(line);}
			}
		}
	}
}


/* PASS2

   	Where the bulk of the actual compilation takes place.
*/

void Pass2(void)
{
	int i, hash;
	int startcode = 0, flag;
	long fpos, flen;


	passnumber = 2;

	fseek(allfile, 0, SEEK_END);
	flen = ftell(allfile);
	if (ferror(allfile)) FatalError(READ_E, allfilename);

	if (percent)
	{
		printf("\n");
		if (listing)
			if (fprintf(listfile, "Compiling %5d lines of %s\n", totallines, PRINTED_FILENAME(sourcefilename)) < 0)
				FatalError(WRITE_E, listfilename);

#if defined (STDPRN_SUPPORTED)
		if (printer)
			if (fprintf(stdprn, "Compiling %5d lines of %s\n\r", totallines, PRINTED_FILENAME(sourcefilename)) < 0)
				FatalError(WRITE_E, "printer");
#endif
	}

	objects = objectctr;
	routines = routinectr;
	events = eventctr;
	labels = labelctr;

	proptable = propctr * 2 + 2;    /* prop. defaults + # of props. */
	proptable += propctr;           /* additive/complex flags */

	proptable += linkproptable;

	/* Reset counters */
	objectctr = objinitial;
	routinectr = routineinitial;
	eventctr = eventinitial;
	labelctr = labelinitial;

	/* Switch input to all-inclusive source */
	if (fclose(sourcefile)==EOF) FatalError(READ_E, sourcefilename);
	rewind(allfile);

	while (!feof(allfile))
	{
		if ((fpos = ftell(allfile))==-1L) FatalError(READ_E, allfilename);
		if (percent)
			printf("\r%3d percent complete", (int)(fpos * 100 / flen));
		if (fpos==flen) goto LeavePass2;

		GetWords();

		if (!strcmp(word[1], "verb") || !strcmp(word[1], "xverb"))
			BuildVerb();

		else if (!strcmp(word[1], "*")) CodeLine();

		else if (!strcmp(word[1], "synonym")) DefSynonym();
		else if (!strcmp(word[1], "removal")) DefRemovals();
		else if (!strcmp(word[1], "compound")) DefCompound();
		else if (!strcmp(word[1], "punctuation")) DefRemovals();
		
		else if (!strcmp(word[1], "}"));

		else
		{
			/* Do initializations for end-of-grammar/
			   start-of-code:
			*/
			if (startcode==0)
			{
				/* end-of-grammar indicator */
				WriteCode(255, 0);
				startcode = 1;
				Boundary();
				codestart = (unsigned int)codeptr;

		/* Replace "object" and "xobject" since they cease to become
		   grammar tokens and are now predefined globals
		*/

				token[102] = "";
				token[103] = "";

		/* Link in code, if applicable */

				if (linked==1) LinkerPass2();
			}

			localctr = 0;

			if (!strcmp(word[1], "object") || !strcmp(word[1], "class"))
			{
BuildanObject:
				oprop[objectctr] = propheap;
				objpropaddr[objectctr] = proptable;
				BuildObject(0);
PostBuildObject:
				for (i=0; i<MAXATTRIBUTES/32; i++)
					objattr[i][objectctr] = attr[i];

				/* Mark the end of this object in the
				   property table. */
				SavePropData(PROP_END);

				proptable++;
				objectctr++;
			}

			else if (!strcmp(word[1], "routine"))
			{
				BuildRoutine();
				routinectr++;
				WriteCode(RETURN_T, 1);
				WriteCode(EOL_T, 1);
			}

			else if (!strcmp(word[1], "replace"))
			{
				flag = IDWord(2);
				if (flag==ROUTINE_T)
				{
					ReplaceRoutine();
					WriteCode(RETURN_T, 1);
					WriteCode(EOL_T, 1);
				}
				else if (flag==OBJECTNUM_T)
				{
					goto BuildanObject;
				}
				else
				{
					sprintf(line, "Syntax error:  %s", word[2]);
					Error(line);
				}
			}

			else if (!strcmp(word[1], "event"))
			{
				BuildEvent();
				eventctr++;
				WriteCode(RETURN_T, 1);
				WriteCode(EOL_T, 1);
			}

			else if (!strcmp(word[1], "resource"))
			{
				CreateResourceFile();
			}

			else
			{
				hash = FindHash(word[1]);

				flag = 0;
				for (i=0; i<objects; i++)
				{
					if (HashMatch(hash, object_hash[i], word[1], object[i]))
					{
						if (i>objectctr)
						{
							/* Okay to do this if we're
							   replacing it anyway
							*/
							if (!oreplace[objectctr])
							{
								sprintf(object_id, "object %s", object[objectctr]);
								sprintf(line, "?Cannot inherit from unbuilt \"%s\"", object[i]);
								Error(line);
							}
							i = 0;
						}

						flag = 1;

						oprop[objectctr] = propheap;
						objpropaddr[objectctr] = proptable;
						BuildObject(i);

						goto PostBuildObject;
					}
				}

				if (flag==0)
				{
					if (word[1][0]=='\0') break;

					sprintf(line, "Unknown build directive:  %s", word[1]);
					Error(line);
				}
			}
		}
	}

LeavePass2:

	return;
}


/* PASS3

   	Where all the data tables following the code are written and
	address references are resolved.  Also, if an .HLB linkable file
	is being produced, all the appropriate additional data are
	written here (such as attribute and property names).
*/

unsigned int arraytableaddr;	/* global because ResolveAddr() needs it */

void Pass3(void)
{
	char e[256];                  	/* dictionary entry */
	char date[9];
	int i, j, k, len;
	int p, d;                       /* for reading properties */
	unsigned int pcount = 0;
	unsigned int a;
	unsigned int resolvetable = 0;
	long tempptr;
	long endofcode;
	time_t now;

	unsigned int objtableaddr;      /* header table addresses */
	unsigned int proptableaddr;
	unsigned int eventtableaddr;
	unsigned int synaddr;
	unsigned int dictaddr;
	unsigned int textbankaddr;

	char *tbuf;			/* for linking text bank */
	int tcount;


	passnumber = 3;

	if (percent) printf("\r100 percent complete\n");

	if (mainaddr==0 && !hlb) Error("No \"main\" routine");

	if (codeptr >= 65536L*address_scale)
		{sprintf(line, "Grammar and executable code exceed %dK", 64*address_scale);
		Error(line);}


	/*
	 * Write the object table:
	 *
	 */

	endofcode = codeptr;

	FillCode();		/* write zeroes to start of next block */
	objtableaddr = (unsigned)(codeptr / 16);

	WriteCode((unsigned)objects, 2);        /* # of objects */

	a = 0;
	for (i=0; i<objects; i++)
	{
#if defined (DEBUG_FULLOBJ)
		if (fullobj)
		{
			sprintf(line, "\nOBJECT %d:  %s", i, object[i]);
			Printout(line);

			sprintf(line, "Parent:  %s    Sibling:  %s    Child:  %s",
					object[parent[i]], object[sibling[i]], object[child[i]]);
                        Printout(line);

			sprintf(line, "Property table address:  %s", PrintHex((unsigned)objpropaddr[i], 2));
			Printout(line);

			/* Property number */
			p = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
			a++;
			while (p!=PROP_END)
			{
				if (strlen(property[p]) > 12)
				{
					sprintf(buffer, "Property:  %s", property[p]);
					Printout(buffer);
					strcpy(buffer, "                         ");
				}
				else sprintf(buffer, "Property:  %12s  ", property[p]);

				/* # of data words */
				d = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
				a++;

				/* j is the code offset--initially 0, it will change if
				   this is a property routine being linked
				*/
				j = 0;

				/* linker giveaway */
				if (d==PROP_LINK_ROUTINE)
					{d = PROP_ROUTINE;
					j = (codestart-(unsigned int)HEADER_LENGTH)/address_scale;}

				if (d==PROP_ROUTINE)
				{

					d = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
					a++;
					sprintf(line, "Routine address:  %6s", PrintHex((long)(d+j)*address_scale, 3));
					strcat(buffer, line);
				}
				else
				{

					/* Get property data */
					for (k=1; k<=d; k++)
					{
						sprintf(line, "%s ", PrintHex(propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE], 2));
						a++;
						strcat(buffer, line);
					}
				}
				Printout(buffer);

				p = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
				a++;
			}

			for (j=0; j<attrctr; j++)
			{
				if (objattr[j/32][i] & 1L<<(j%32))
				{
					sprintf(line, "\tAttribute:  %s", attribute[j]);
					Printout(line);
				}
			}
		}
#endif  /* defined (DEBUG_FULLOBJ) */

		/* If we're building an .HLB file, various symbol data
		   such as (at this point) object names and property
		   table locations are written into the object table to
		   be retrieved during linking by CompilerLink().
		*/
		if (hlb)
		{
			WriteCode(len=strlen(object[i]), 1);
			if (fputs(object[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
			codeptr+=len;
			WriteCode(oprop[i], 2);
		}

		for (j=0; j<MAXATTRIBUTES/32; j++)
		{
			WriteCode((unsigned)(objattr[j][i]%65536L), 2);
			WriteCode((unsigned)(objattr[j][i]/65536L), 2);
		}

		WriteCode(parent[i], 2);
		WriteCode(sibling[i], 2);
		WriteCode(child[i], 2);

		WriteCode(objpropaddr[i], 2);
	}


	/*
	 * Write the property table:
	 *
	 */
	if (!hlb) FillCode();

	proptableaddr = (unsigned)(codeptr / 16);

	WriteCode(propctr, 2);             /* # of properties */

	if (propctr)
	{
		/* First write the property defaults: */
                for (i=0; i<propctr; i++)
		{
			if (i >= ENGINE_PROPERTIES)
			{
				/* Write the property names if this is an
				   .HLB build. */
				if (hlb)
				{
					WriteCode(len=strlen(property[i]), 1);
					if (fputs(property[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
					codeptr+=len;
				}
			}

			WriteCode(propdef[i], 2);
		}

		for (i=0; i<propctr; i++)       /* additive flags */
			WriteCode(propadd[i], 1);


		k = 0;                  	/* object counter */
		a = 0;                  	/* property heap position */
		while (a < propheap)
		{
			p = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
			a++;
			pcount++;

			WriteCode(p, 1);

			if (p != PROP_END)
			{
				/* # of data words */
				d = propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE];
				a++;
				j = 0;

				if (d==PROP_LINK_ROUTINE)
					{d = PROP_ROUTINE;
					j = (codestart-(unsigned int)HEADER_LENGTH)/address_scale;}

				WriteCode(d, 1);

				if (d==PROP_ROUTINE) d = 1;

				/* Write property data */
				for (i=1; i<=d; i++)
				{
					WriteCode(propdata[a/PROPBLOCKSIZE][a%PROPBLOCKSIZE]+j, 2);
					a++;
				}
			}
			else k++;       /* next object */
		}
	}

	if (hlb)
	{
		WriteAdditionalData();
	}

	/*
	 * Write the event table:
	 *
	 */
	if (!hlb) FillCode();
	eventtableaddr = (unsigned)(codeptr / 16);

	WriteCode(events, 2);              /* # of events */

	for (i=0; i<events; i++)
		{WriteCode(eventin[i], 2);
		WriteCode(eventaddr[i], 2);}


	/*
	 * Write the array table:
	 *
	 */
	if (!hlb)
	{
		FillCode();
		arraytableaddr = (unsigned)(codeptr / 16);

		/* global defaults */
		for (i=0; i<MAXGLOBALS; i++)
			WriteCode(globaldef[i], 2);

		/* program array space */
		for (i=0; i<arrayctr; i++)
		{
			/* Write the array length... */
			WriteCode(arraylen[i], 2);

			/* ...followed by blank array values.  If any values
			   were defined at compile time, they will be filled 
			   in by ResolveAddr()
			*/
			for (j=0; (unsigned int)j<arraylen[i]; j++)
				WriteCode(0, 2);
		}

		/* The Debugger uses 256 bytes in the array table as
		   a workspace for assignments and watch expression
		   evaluation:
		*/
		if (builddebug && !hlb)
		{
			if (arraysize > 65535-256)
				Error(".HDX array table too large");

			for (i=0; i<256; i++) WriteCode(0, 1);
		}
	}
	else
		arraytableaddr = (unsigned)(codeptr / 16);


	/*
	 * Dictionary, synonyms, etc.:
	 *
	 */

	/*
	 * Write synonyms/removals/compounds/punctuation:
	 *
	 */
	if (!hlb) FillCode();
	synaddr = (unsigned)(codeptr / 16);
	WriteCode(syncount, 2);            /* # of synonyms */

	if (syncount)
	{
		for (i=0; i<syncount; i++)
		{
			WriteCode(syndata[i].syntype, 1);
			WriteCode(syndata[i].syn1, 2);
			WriteCode(syndata[i].syn2, 2);
		}
	}

	/*
	 * Write dictionary:
	 *
	 */
	if (!hlb) FillCode();

	dictaddr = (unsigned)(codeptr / 16);
	WriteCode(dictcount, 2);           /* # of dictionary entries */

	/* Write a null string "" */
	WriteCode(0, 1);

	if (dictcount)
	{
		for (i=1; i<dictcount; i++)
		{
			strcpy(e, lexentry[i]);
			WriteCode(strlen(e), 1);

			for (j=1; j<=(int)strlen(e); j++)
				WriteCode(e[j-1] + CHAR_TRANSLATION, 1);
		}
	}

	if (hlb) WriteCode(MAXDICTEXTEND, 2);

	/* Write space for dictionary extension, if applicable */
	if (!hlb)
	        for (i=1; i<=MAXDICTEXTEND; i++)
			WriteCode(0, 1);

	if (hlb) WriteCode(verbs, 2);


	/*
	 * Link text bank:
	 *
	 */
        if (!hlb) FillCode();
	textbankaddr = (unsigned)(codeptr / 16);

	tcount = 16384;			/* starting ideal buffer size */
	while (true)
	{
		if ((tbuf = malloc(tcount * sizeof(char)))!=NULL)
			break;
		tcount /= 2;
		if (tcount < HEADER_LENGTH) FatalError(MEMORY_E, "");
	}

	rewind(textfile);

	while (!feof(textfile))
	{
		j = fread(tbuf, sizeof(char), (size_t)tcount, textfile);
		for (i=0; i<j; i++)
			WriteCode(tbuf[i], 1);
		if (j < tcount) break;
	}
	if (ferror(textfile)) FatalError(READ_E, "text file");


	/*
	 * Resolve addresses/values (or write resolve tables if .HLB):
	 *
	 */
	if (hlb)
	{
		while (ftell(objectfile)%64L)
			WriteCode(0, 1);
		resolvetable = (unsigned int)(ftell(objectfile)/64L);
	}

	ResolveAddr();


	/*
	 * Write header:
	 *
	 */
	tempptr = codeptr;
	codeptr = 0;

#if !defined (COMPILE_V25)
	if (compile_v25)
		WriteCode(25, 1);
	else
#endif
	WriteCode(HCVERSION*10+HCREVISION, 1);

	time(&now);
	strftime(date, 9, "%m-%d-%y", localtime(&now));

	if (!hlb)
	{
		/* Generate a unique ID for this game... */
		srand((unsigned)now);
		WriteCode((int)rand()%128, 1);
		WriteCode((int)rand()%128, 1);
	}
	else
	{
		/* ...or use "$$" to signal it's a linkable
		   .HLB header. */
		WriteCode('$', 1);
		WriteCode('$', 1);
	}

	/* Write date string: */
	for (i=1; i<=8; i++)
		WriteCode((int)date[i-1], 1);

        WriteCode(codestart, 2);

	WriteCode(objtableaddr, 2);
	WriteCode(proptableaddr, 2);
	WriteCode(eventtableaddr, 2);
	WriteCode(arraytableaddr, 2);
	WriteCode(dictaddr, 2);
	WriteCode(synaddr, 2);

	WriteCode(initaddr, 2);
	WriteCode(mainaddr, 2);
	WriteCode(parseaddr, 2);
	WriteCode(parseerroraddr, 2);
	WriteCode(findobjectaddr, 2);
	WriteCode(endgameaddr, 2);
	WriteCode(speaktoaddr, 2);
	WriteCode(performaddr, 2);

	WriteCode(textbankaddr, 2);

	/* The .HLB file overwrites some of the unnecessary bits of the
	   header with some information it will need for linking.
	*/
	if (hlb)
	{
		WriteCode(textcount, 2);

		/* Go back and overwrite the date stamp: */
		codeptr = 3;
		WriteCode(pcount, 2);			    /* # properties */
		WriteCode((unsigned)(endofcode%65536L), 2); /* code length  */
		WriteCode((unsigned)(endofcode/65536L), 1);
		WriteCode(addrctr, 2);			    /* # resolves   */
		WriteCode(resolvetable, 2);		  /* table location */
	}

	codeptr = tempptr;


	/* If building an .HDX file, figure out where the names and data
	   are going to start, and write the reference at byte 58 in the
	   header.
	*/
	if (builddebug && !hlb)
	{
		FillCode();
		tempptr = codeptr;
		codeptr = 58;

		/* Flag this as an .HDX file */
		WriteCode(1, 1);

		/* Where to find names and data */
                WriteCode((unsigned)(tempptr%65536L), 2);
		WriteCode((unsigned)(tempptr/65536L), 1);

		/* Where to find workspace in the array table */
		WriteCode(arraysize, 2);

		codeptr = tempptr;
	}

	if (memmap)
	{
		sprintf(line, "\nMEMORY USAGE FOR:  %s", PRINTED_FILENAME(objectfilename));
		Printout(line);
		sprintf(line, "\n          (Top:  $%6s)", PrintHex(codeptr, 3));
		Printout(line);
                PrintFrameLine();

		if (hlb)
			Printout("| Link tables and |               |");

		sprintf(line, "| Text bank       | $%6s bytes |", PrintHex(codeptr-textbankaddr*16L, 3));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Dictionary      |   $%4s bytes |",
			PrintHex(textbankaddr*16L-dictaddr*16L, 2));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Special words   |   $%4s bytes |",
			PrintHex(dictaddr*16L-synaddr*16L, 2));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Array space     |   $%4s bytes |",
			PrintHex(synaddr*16L-arraytableaddr*16L, 2));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Event table     |   $%4s bytes |",
			PrintHex(arraytableaddr*16L-eventtableaddr*16L, 2));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Property table  |   $%4s bytes |",
			PrintHex(eventtableaddr*16L-proptableaddr*16L, 2));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Object table    |   $%4s bytes |",
			PrintHex(proptableaddr*16L-objtableaddr*16L, 2));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Executable code | $%6s bytes |",
			PrintHex(objtableaddr*16L-codestart, 3));
		Printout(line);
                PrintFrameLine();
	   	sprintf(line, "| Grammar table   |   $%4s bytes |",
			PrintHex(codestart-HEADER_LENGTH, 2));
		Printout(line);
                PrintFrameLine();
		sprintf(line, "| Header          |   $%4s bytes |",
			PrintHex(HEADER_LENGTH, 2));
		Printout(line);
                PrintFrameLine();
		Printout("        (Bottom:  $000000)");
	}

	if (builddebug && !hlb) WriteAdditionalData();
}

void PrintFrameLine(void)	/* called only by Pass3 */
{
	Printout("+-----------------+---------------+");
}


/* WRITEADDITIONALDATA

	Writes additional data to an .HLB linkable file or .HDX
	debuggable executable (names of objects, properties, routine
	addresses, etc.).  Called at different points by Pass3
	depending on what kind of file is being generated.
*/

void WriteAdditionalData(void)
{
	int i;
	unsigned int len;

	/* objects */
	if (builddebug && !hlb)
	{
		for (i=0; i<objects; i++)
		{
			WriteCode(len=strlen(object[i]), 1);
			if (fputs(object[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
			codeptr+=len;
		}
	}

	/* properties */
	if (builddebug && !hlb)
	{
		WriteCode(propctr, 2);
		for (i=0; i<propctr; i++)
		{
			WriteCode(len=strlen(property[i]), 1);
			if (fputs(property[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
			codeptr+=len;
		}
	}

	/* attributes */
	WriteCode(attrctr, 2);
	for (i=0; i<attrctr; i++)
	{
		WriteCode(len=strlen(attribute[i]), 1);
		if (fputs(attribute[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
		codeptr+=len;
	}

	/* aliases */
	WriteCode(aliasctr, 2);
	for (i=0; i<aliasctr; i++)
	{
		WriteCode(len=strlen(alias[i]), 1);
		if (fputs(alias[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
		codeptr+=len;
		WriteCode(aliasof[i], 2);
	}

	/* globals */
	WriteCode(globalctr, 2);
	for (i=0; i<globalctr; i++)
	{
		WriteCode(len=strlen(global[i]), 1);
		if (fputs(global[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
		codeptr+=len;

		if (hlb) WriteCode(globaldef[i], 2);
	}

	/* constants (only if .HLB) */
	if (hlb)
	{
		WriteCode(constctr, 2);
		for (i=0; i<constctr; i++)
		{
			WriteCode(len=strlen(constant[i]), 1);
			if (fputs(constant[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
			codeptr+=len;
			WriteCode(constantval[i], 2);
		}
	}

	/* routines */
	WriteCode(routinectr, 2);
	for (i=0; i<routinectr; i++)
	{
		WriteCode(len=strlen(routine[i]), 1);
		if (fputs(routine[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
		codeptr+=len;
		WriteCode(raddr[i], 2);
	}

	/* events */
	if (builddebug && !hlb)
	{
		WriteCode(events, 2);
		for (i=0; i<events; i++)
		{
			WriteCode(eventin[i], 2);
			WriteCode(eventaddr[i], 2);
		}
	}

	/* arrays */
	WriteCode(arrayctr, 2);
	if (hlb) WriteCode(arraysize, 2);
	for (i=0; i<arrayctr; i++)
	{
		WriteCode(len=strlen(array[i]), 1);
		if (fputs(array[i], objectfile)==EOF) FatalError(WRITE_E, objectfilename);
		codeptr+=len;
		WriteCode(arrayaddr[i], 2);
		if (hlb) WriteCode(arraylen[i], 2);
	}
}
