/*
	hcreset.c
	
	Resets all compiler globals to a zero state AFTER at least one
	compilation.
*/


#include "hcheader.h"

extern char token_hash_table_built;
extern char silent_IDWord;
extern int ifsetnest;

void hc_reset_everything(void)
{
	int i;

	if (token_hash_table_built)
	{
		for (i=ENGINE_GLOBALS; i<globalctr; i++) free(global[i]);
		token[102] = "object";
		token[103] = "xobject";
		token_hash_table_built = false;
	}

	/* Object arrays */
	if (alloc_objects)
	{
		for (i=0; i<objectctr; i++) free(object[i]);
		free(object);
		free(object_hash);
		free(oprop);
		for (i=0; i<MAXATTRIBUTES/32; i++)
			free(objattr[i]);
		free(objpropaddr);
		free(oreplace);
		free(parent);
		free(sibling);
		free(child);
	}

	/* Property arrays */
	if (alloc_properties)
	{
		for (i=ENGINE_PROPERTIES; i<propctr; i++) free(property[i]);
		free(property);
		free(property_hash);
		free(propdef);
		free(propset);
		free(propadd);
		for (i=0; (unsigned)i<propheap; i++)
		{
			if (i%PROPBLOCKSIZE==0)
			{
				free(propdata[i/PROPBLOCKSIZE]);
			}
		}
	}

	/* Alias arrays */
	if (alloc_aliases)
	{
		for (i=0; i<aliasctr; i++) free(alias[i]);
		free(alias);
		free(alias_hash);
		free(aliasof);
	}

	/* Array arrays */
	if (alloc_arrays)
	{
		for (i=0; i<arrayctr; i++) free(array[i]);
		free(array);
		free(array_hash);
		free(arrayaddr);
		free(arraylen);
	}

	/* Constant arrays */
	if (alloc_constants)
	{
		for (i=0; i<constctr; i++) free(constant[i]);
		free(constant);
		free(constant_hash);
		free(constantval);
	}

	/* Event arrays */
	if (alloc_events)
	{
		free(eventin);
		free(eventaddr);
		alloc_events = true;
	}

	/* Routine arrays */
	if (alloc_routines)
	{
		for (i=0; i<routinectr; i++) free(routine[i]);
		free(routine);
		free(routine_hash);
		free(raddr);
		free(rreplace);
	}

	/* Label arrays */
	if (alloc_labels)
	{
		for (i=0; i<labelctr; i++) free(label[i]);
		free(label);
		free(label_hash);
		free(laddr);
	}

	/* Dictionary arrays */
	if (alloc_dict)
	{
		/* Start at 1, to skip "" */
		for (i=1; i<dictcount; i++) free(lexentry[i]);
		free(lexentry);
		free(lexnext);
		free(lexaddr);
	}

	/* Special words */
	if (alloc_syn)
	{
		free(syndata);
	}

	/* Setting arrays */
	if (alloc_sets)
	{
		for (i=0; i<MAXFLAGS; i++) free(sets[i]);
		free(sets);
	}

	/* Directory arrays */
	if (alloc_directories)
	{
		for (i=0; i<directoryctr; i++) free(directory[i]);
		free(directory);
	}
	
	/* Address blocks */
	for (i=0; (unsigned)i<addrctr; i++)
	{
		/* (See RememberAddr() for details) */
		unsigned int q = addrctr/ADDRBLOCKSIZE;
		unsigned int r = addrctr%ADDRBLOCKSIZE;

		if (r==0)
		{
			free(addrdata[q]);
		}
	}

		
	/* hc.c */
	
	exitvalue = 0;
	
	
	/* hcbuild.c */
	
	infileblock = 0;
	incompprop = 0;
	
	
	/* hccode.c */
	
	token_val = 0;
	silent_IDWord = false;
	nest = 0;
	arguments = 0;
	
	
	/* hccomp.c */
	
	ifsetnest = 0;
	
	
	/* hcdef.c */
	
	objectctr = 0;
	attrctr = 0;
	propctr = 0;
	proptable = 0;
	propheap = 0;
	labelctr = 0;
	routinectr = 0;
	addrctr = 0;
	eventctr = 0;
	aliasctr = 0;
	globalctr = 0, localctr = 0;
	constctr = 0;
	arrayctr = 0;
	
	
	/* hcfile.c */
	
	codeptr = 0;
	textptr = 0;
	
	
	/* hclink.c */
	
	linked = 0;
	
	listing = 0,
	objecttree = 0,
	fullobj = 0,
	printer = 0,
	statistics = 0,
	printdebug = 0,
	override = 0,
	aborterror = 0,
	memmap = 0,
	hlb = 0,
	builddebug = 0,
	expandederr = 0,
	spellcheck = 0,
	writeanyway = 0;
	#if !defined (COMPILE_V25)
	compile_v25 = 0;
	#else
	compile_v25 = 1;
	#endif
	
	percent = 0, totallines = 0, tlines = 0;
	er = 0;
	warn = 0;
	
	directoryctr = 0;
	alloc_objects = 0, alloc_properties = 0, alloc_labels = 0,
	alloc_routines = 0, alloc_events = 0, alloc_aliases = 0,
	alloc_constants = 0, alloc_arrays = 0, alloc_sets = 0,
	alloc_dict = 0, alloc_syn = 0, alloc_directories = 0;
	dicttable = 0;
	
	verbs = 0;
	dictcount = 0, syncount = 0;
	
	
	/* hcpass.c */
	
	codestart = 0;
	endofgrammar = 0;
	passnumber = 0;
	
	objinitial = 0, routineinitial = 0, eventinitial = 0, labelinitial = 0,
	propinitial = 0;
	
	objects = 0, routines = 0, events = 0, labels = 0;
	
	initaddr = 0;
	mainaddr = 0;
	parseaddr = 0;
	parseerroraddr = 0;
	findobjectaddr = 0;
	endgameaddr = 0;
	speaktoaddr = 0;
	performaddr = 0;
	
	linkproptable = 0;
	textcount = 0;
}
