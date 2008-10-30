/*
	HCLINK.C

	contains:

		LinkerPass1
			CheckLinkLimit
		LinkerPass2
		ReadCode

	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"


/* Function prototypes: */
void CheckLinkLimit(int a, int b, char *n);

char linked = 0;                /* for linking .HLB precompiled header */


/* LINKERPASS1

   	Called in Pass1() when a '#link "<filename>"' is encountered.
	All of the to-be-linked routines, properties, attributes, etc. 
	are read.  Any actual executable code will be linked in 
	LinkerPass2(), as will the resolve table.
*/

void LinkerPass1(void)
{
	int i, len, p;
	unsigned int data;
	unsigned int j, pcount, tcount;

	unsigned int objtableaddr;            /* header table address */

	if (linked==1)
		{Error("Incompatible precompiled headers");
		return;}
	else if (linked==-1)
		{Error("Precompiled header must come before code or definitions");
		return;}

	linked = 1;

	endofgrammar = true;

	StripQuotes(word[2]);		/* .HLB file name */
	strcpy(linkfilename, word[2]);

	/* open .HLB file */
	if (!(linkfile = TrytoOpen(linkfilename, "rb", "source")))
	{
		if (!(linkfile = TrytoOpen(linkfilename, "rb", "lib")))
			FatalError(OPEN_E, linkfilename);
	}

        /* Read symbols */

	/* First making sure the header is valid */
	if (ReadCode(1)!=HCVERSION*10+HCREVISION || ReadCode(1)!='$' || ReadCode(1)!='$')
		{fclose(linkfile);
		aborterror = true;
		sprintf(line, "Invalid precompiled header version:  %s", linkfilename);
		Error(line);
		return;}

	pcount = (unsigned int)ReadCode(2);           /* # of obj. properties */

	if (fseek(linkfile, 13, SEEK_SET)) FatalError(READ_E, linkfilename);

	objtableaddr = (unsigned int)ReadCode(2);

	if (fseek(linkfile, 25, SEEK_SET)) FatalError(READ_E, linkfilename);

	initaddr = (unsigned int)ReadCode(2);
	mainaddr = (unsigned int)ReadCode(2);
	parseaddr = (unsigned int)ReadCode(2);
	parseerroraddr = (unsigned int)ReadCode(2);
	findobjectaddr = (unsigned int)ReadCode(2);
	endgameaddr = (unsigned int)ReadCode(2);
	speaktoaddr = (unsigned int)ReadCode(2);
	performaddr = (unsigned int)ReadCode(2);

	ReadCode(2);	/* skip text bank address */
	tcount = (unsigned int)ReadCode(2);   /* # of text bank entries */

	if (fseek(linkfile, objtableaddr*16, SEEK_SET)) FatalError(READ_E, linkfilename);

	/* Objects */
	CheckLinkLimit(objectctr = (unsigned int)ReadCode(2), MAXOBJECTS, "objects");
	for (i=0; i<objectctr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		object[i] = MakeString(line);
		object_hash[i] = FindHash(object[i]);
		oprop[i] = (unsigned int)ReadCode(2);
		for (j=0; j<MAXATTRIBUTES/32; j++)
		{
			objattr[j][i] = ReadCode(2);
			objattr[j][i] += ReadCode(2)*65536L;
		}
		parent[i] = (unsigned int)ReadCode(2);
		sibling[i] = (unsigned int)ReadCode(2);
		child[i] = (unsigned int)ReadCode(2);
		objpropaddr[i] = (unsigned int)ReadCode(2);
		oreplace[i] = 0;
	}
	objinitial = objectctr;

	/* Properties */
	CheckLinkLimit(propctr = (unsigned int)ReadCode(2), MAXPROPERTIES, "properties");
	propinitial = propctr;
	for (i=0; i<propctr; i++)
	{
		if (i >= ENGINE_PROPERTIES)
		{
			len = (unsigned int)ReadCode(1)+1;
			if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
			if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
	                property[i] = MakeString(line);
		}
		property_hash[i] = FindHash(property[i]);
		propdef[i] = (unsigned int)ReadCode(2);
	}
	for (i=0; i<propctr; i++)
		propadd[i] = (char)ReadCode(1);

	for (j=0; j<pcount; j++)
	{
		p = (unsigned int)ReadCode(1);
		SavePropData(p);
		if (p!=PROP_END)
		{
			len = (unsigned int)ReadCode(1);
			if (len==PROP_ROUTINE) len = PROP_LINK_ROUTINE;
			SavePropData(len);
			if (len==PROP_LINK_ROUTINE) len = 1;
	                for (i=1; i<=len; i++)
				SavePropData((unsigned int)ReadCode(2));
			linkproptable += (len+1)*2;
		}
		else
			linkproptable++;
	}

	/* Attributes */
	CheckLinkLimit(attrctr = (unsigned int)ReadCode(2), MAXATTRIBUTES, "attributes");
	for (i=0; i<attrctr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		attribute[i] = MakeString(line);
		attribute_hash[i] = FindHash(attribute[i]);
	}

	/* Aliases */
	CheckLinkLimit(aliasctr = (unsigned int)ReadCode(2), MAXALIASES, "aliases");
	for (i=0; i<aliasctr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		alias[i] = MakeString(line);
		alias_hash[i] = FindHash(alias[i]);
		aliasof[i] = (unsigned int)ReadCode(2);
	}

	/* Globals */
	CheckLinkLimit(globalctr = (unsigned int)ReadCode(2), MAXGLOBALS, "global variables");
	for (i=0; i<globalctr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		if (i>=ENGINE_GLOBALS) global[i] = MakeString(line);
		global_hash[i] = FindHash(global[i]);
		globaldef[i] = (unsigned int)ReadCode(2);
	}

	/* Constants */
	CheckLinkLimit(constctr = (unsigned int)ReadCode(2), MAXCONSTANTS, "constants");
	for (i=0; i<constctr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		constant[i] = MakeString(line);
		constant_hash[i] = FindHash(constant[i]);
		constantval[i] = (unsigned int)ReadCode(2);
	}

	/* Routines */
	CheckLinkLimit(routinectr = (unsigned int)ReadCode(2), MAXROUTINES, "routines");
	for (i=0; i<routinectr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		routine[i] = MakeString(line);
		routine_hash[i] = FindHash(routine[i]);
		raddr[i] = (unsigned int)ReadCode(2);
		rreplace[i] = 0;
	}
	routineinitial = routinectr;

	labelinitial = labelctr = 0;

	/* Arrays */
	CheckLinkLimit(arrayctr = (unsigned int)ReadCode(2), MAXARRAYS, "arrays");
	arraysize = (unsigned int)ReadCode(2);
	for (i=0; i<arrayctr; i++)
	{
		len = (unsigned int)ReadCode(1)+1;
		if (len>MAX_SYMBOL_LENGTH) FatalError(READ_E, linkfilename);
		if (!fgets(line, len, linkfile)) FatalError(READ_E, linkfilename);
		array[i] = MakeString(line);
		array_hash[i] = FindHash(array[i]);
		arrayaddr[i] = (unsigned int)ReadCode(2);
		arraylen[i] = (unsigned int)ReadCode(2);
	}

	/* Read events */
	CheckLinkLimit(eventctr = (unsigned int)ReadCode(2), MAXEVENTS, "events");
	for (i=0; i<eventctr; i++)
	{
		eventin[i] = (unsigned int)ReadCode(2);
		eventaddr[i] = (unsigned int)ReadCode(2);
	}
	eventinitial = eventctr;

	/* Read special words */
	CheckLinkLimit(syncount = (unsigned int)ReadCode(2), MAXSPECIALWORDS, "special words");

	for (i=0; i<syncount; i++)
	{
		syndata[i].syntype = (char)ReadCode(1);
		syndata[i].syn1 = (unsigned int)ReadCode(2);
		syndata[i].syn2 = (unsigned int)ReadCode(2);
	}

	/* Read dictionary */
	CheckLinkLimit(data = (unsigned int)ReadCode(2), MAXDICT, "dictionary entries");
	ReadCode(1);			/* null string */

	for (i=1; (unsigned int)i<data; i++)
	{
		len = (unsigned int)ReadCode(1);
		for (j=1; j<=(unsigned int)len; j++)
			line[j-1] = (char)(ReadCode(1)-CHAR_TRANSLATION);
		line[j-1] = '\0';
		AddDictionary(line);
	}

	data = (unsigned int)ReadCode(2);
	MAXDICTEXTEND += data;

	verbs = (unsigned int)ReadCode(2);


	/* Link text bank */
	for (j=0; j<tcount; j++)
	{
		len = (unsigned int)ReadCode(2);

		/* If we hit a block of 00 bytes before the resolve table */
		if (len==0) break;

		for (i=0; i<len; i++)
			buffer[i] = (char)(ReadCode(1)-CHAR_TRANSLATION);
                buffer[i] = '\0';
		WriteText(buffer);
	}
	if (ferror(textfile)) FatalError(WRITE_E, textfilename);

	fclose(linkfile);
}


/* CHECKLINKLIMIT

	Checks to makes sure that a linked .HLB file doesn't exceed
	a lower limit set for this compilation.
*/

void CheckLinkLimit(int a, int b, char *n)
{
	if (a > b)
	{
		sprintf(line, "Compiler limit of %d %s exceeded in:  %s", b, n, linkfilename);
		FatalError(COMP_LINK_LIMIT_E, "");
	}
}


/* LINKERPASS2

	This is Part II of the linker, where all calculations are
	made to reconcile addresses in the linked code, and the actual
	actual linked code is written to the object file.  The linked
	resolve table is transcribed to the resolve table for the
	current compilation.
*/
	
void LinkerPass2(void)
{
	unsigned char *cbuf;
	int i, j, ccount;
	unsigned int resolves, restype;
	unsigned int resolvetable, resnum;
	long c, cl, resptr, endofcode, offset;

	/* The offset is the amount of code written into the grammar
	   table; any previously set addresses must be moved up by
	   this amount
	*/
	Boundary();
	offset = codeptr - HEADER_LENGTH;

	initaddr += offset/address_scale;
	mainaddr += offset/address_scale;
	parseaddr += offset/address_scale;
	parseerroraddr += offset/address_scale;
	findobjectaddr += offset/address_scale;
	endgameaddr += offset/address_scale;
	speaktoaddr += offset/address_scale;
	performaddr += offset/address_scale;

	if (!(linkfile = HUGO_FOPEN(linkfilename, "rb")))
		FatalError(OPEN_E, linkfilename);
	if (fseek(linkfile, 3, SEEK_SET))
		FatalError(READ_E, linkfilename);

	/* Read and discard the property data count */
	ReadCode(2);

	endofcode = ReadCode(3);
	resolves = (unsigned int)ReadCode(2);
	resolvetable = (unsigned int)ReadCode(2);

	/* Starting ideal read buffer size */
	ccount = 16384;
	while (true)
	{
		if ((cbuf = malloc(ccount * sizeof(char)))!=NULL)
			break;
		ccount /= 2;
		if (ccount < 2) FatalError(MEMORY_E, "");
	}


	/* Read and write the actual code block by block: */

	if (fseek(linkfile, HEADER_LENGTH, SEEK_SET))
		FatalError(READ_E, linkfilename);

	/* i.e., right after the header */
	c = HEADER_LENGTH;
	while (c <= endofcode)
	{
		if ((cl = endofcode - c) < ccount)
			ccount = (int)cl;
		j = fread(cbuf, sizeof(char), (size_t)ccount, linkfile);
		c += j;
		if (!j) break;
		for (i=0; i<j; i++)
			WriteCode(cbuf[i], 1);
	}
	free(cbuf);
	if (ferror(linkfile)) FatalError(READ_E, linkfilename);


	/* Rewrite the resolve table: */

	if (fseek(linkfile, resolvetable*64L, SEEK_SET))
		FatalError(READ_E, linkfilename);

	for (i=0; (unsigned int)i<resolves; i++)
	{
		resptr = ReadCode(3);
		restype = (unsigned int)ReadCode(1);
		resnum = (unsigned int)ReadCode(2);
		RememberAddr(resptr+offset, restype, resnum);
	}
	for (i=0; i<routineinitial; i++)
		raddr[i] += offset/address_scale;
	for (i=0; i<eventinitial; i++)
		eventaddr[i] += offset/address_scale;
	for (i=0; i<labelinitial; i++)
		laddr[i] += offset/address_scale;
	for (i=0; i<objinitial; i++)
		objpropaddr[i] += (propctr-propinitial)*2 + propctr-propinitial;
}


/* READCODE */

long ReadCode(int num)
{
	unsigned int a, b = 0;
	long c = 0;

	if ((a = fgetc(linkfile))==EOF && ferror(linkfile))
		FatalError(READ_E, linkfilename);

	if (num>=2)
	{
		if ((b = fgetc(linkfile))==EOF && ferror(linkfile))
			FatalError(READ_E, linkfilename);
		b *= 256;
	}

	if (num==3)
	{
		if ((c = fgetc(linkfile))==EOF && ferror(linkfile))
			FatalError(READ_E, linkfilename);
		c *= 65536L;
	}

	return a + b + c;
}
