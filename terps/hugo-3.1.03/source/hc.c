/*
	-------------------------------------------
	HUGO COMPILER by Kent Tessman
	Copyright (c) 1995-2006

	The General Coffee Company Film Productions
	-------------------------------------------

	This source code is provided to allow porting of the Hugo Compiler
	to different operating systems.  It may be distributed as is,
	providing that there has been no modification or alteration of
	the material contained therein; distribution of modified source code
	is prohibited by the copyright holder.
*/


#define INIT_PASS
#include "hcheader.h"


/* Function prototypes: */
void Banner(FILE *stream, char *name);

/* address_scale refers to the factor by which addresses are multiplied to
   get the "real" address.  In this way, a 16-bit integer can reference
   64K * 16 = 1024K of memory.
*/
int address_scale = 16;

time_t tick;                    /* to time compilation */
int exitvalue = 0;


/* MAIN

	If FRONT_END is defined, the external main() function can pass the
	calling parameters argc and argv to hc_main.
*/

#if !defined (FRONT_END)
int main(int argc, char *argv[])
#else
int hc_main(int argc, char *argv[])
#endif
{
	time(&tick);			/* get time compilation begins */

	if (argc==1) Banner(stdout, "");

	ParseCommand(argc, argv);       /* Parse command line, then open */
	OpenFiles();			/*   files upon returning.	 */

	if (listing && !spellcheck) Banner(listfile, listfilename);

#ifdef STDPRN_SUPPORTED
	if (printer && !spellcheck) Banner(stdprn, "printer");
#endif

	objectctr = 0;

	/* Define:  Read all the source file(s) into one contiguous file,
	   defining all the objects, properties, attributes, and other
	   global data. */
	Pass1();

	/* Build:  Actually write the code and construct object/property
	   tables. */
	Pass2();

	/* Resolve and link:  Write object/property tables, array data,
	   events, dictionary, etc. after code has been compiled.  Skim
	   through again and resolve any necessary addresses. */
	Pass3();

	PrintStatistics();

	CleanUpFiles();

	return exitvalue;
}


void Banner(FILE *stream, char *name)
{
	fprintf(stream, "HUGO COMPILER v%d.%d%s by Kent Tessman (c) 1995-2006\n", HCVERSION, HCREVISION, HCINTERIM);
	fprintf(stream, "The General Coffee Company Film Productions\n");
#if defined (PORT_NAME)
	fprintf(stream, "%s port by %s\n", PORT_NAME, PORTER_NAME);
#endif
	if (ferror(stream)) FatalError(WRITE_E, name);
}
