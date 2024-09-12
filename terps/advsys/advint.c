
/* advint.c - an interpreter for adventure games */
/*
	Copyright (c) 1986, by David Michael Betz
	All rights reserved
*/

#include "advint.h"
#include "advdbs.h"
#ifndef MAC
#include <setjmp.h>
#endif

/* global variables */
jmp_buf restart;

/* external variables */
extern int h_init;
extern int h_update;
extern int h_before;
extern int h_after;
extern int h_error;

/* main - the main routine */
main(argc,argv)
  int argc; char *argv[];
{
    char *fname,*lname;
    int rows,cols,i;

#ifdef MAC
    char name[50];
    macinit(name);
    fname = name;
    lname = NULL;
    rows = 20;
    cols = 80;
#else
    printf("ADVINT v1.2 - Copyright (c) 1986, by David Betz\n");
    fname = NULL;
    lname = NULL;
    rows = 24;
    cols = 80;

    /* parse the command line */
    for (i = 1; i < argc; i++)
	if (argv[i][0] == '-')
	    switch (argv[i][1]) {
	    case 'r':
	    case 'R':
		    rows = atoi(&argv[i][2]);
		    break;
	    case 'c':
	    case 'C':
		    cols = atoi(&argv[i][2]);
		    break;
	    case 'l':
	    case 'L':
		    lname = &argv[i][2];
	    	    break;
	    }
	else
	    fname = argv[i];
    if (fname == NULL) {
	printf("usage: advint [-r<rows>] [-c<columns>] [-l<log-file>] <file>\n");
	exit();
    }
#endif

    /* initialize terminal i/o */
    trm_init(rows,cols,lname);

    /* initialize the database */
    db_init(fname);

    /* play the game */
    play();
}

/* play - the main loop */
play()
{
    /* establish the restart point */
    setjmp(restart);

    /* execute the initialization code */
    execute(h_init);

    /* turn handling loop */
    for (;;) {

	/* execute the update code */
	execute(h_update);

	/* parse the next input command */
	if (parse()) {
	    if (single())
		while (next() && single())
		    ;
	}

	/* parse error, call the error handling code */
	else
	    execute(h_error);
    }
}

/* single - handle a single action */
int single()
{
    /* execute the before code */
    switch (execute(h_before)) {
    case ABORT:	/* before handler aborted sequence */
	return (FALSE);
    case CHAIN:	/* execute the action handler */
	if (execute(getafield(getvalue(V_ACTION),A_CODE)) == ABORT)
	    return (FALSE);
    case FINISH:/* execute the after code */
	if (execute(h_after) == ABORT)
	    return (FALSE);
	break;
    }
    return (TRUE);
}

/* error - print an error message and exit */
error(msg)
  char *msg;
{
    trm_str(msg);
    trm_chr('\n');
    exit();
}
