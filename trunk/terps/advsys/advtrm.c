
/* advtrm.c - terminal i/o routines */
/*
	Copyright (c) 1986, by David Michael Betz
	All rights reserved
*/

#include "header.h"

/* useful definitions */
#define TRUE	1
#define FALSE	0
#define EOS	'\0'
#define LINEMAX	200
#define WORDMAX	100

/* global variables */
char line[LINEMAX+1];

/* local variables */
static int col,maxcol,row,maxrow;
static int scnt,wcnt;
static char word[WORDMAX+1],*wptr;
static FILE *logfp = NULL;

/* forward declarations */
char *trm_line();

/* trm_init - initialize the terminal module */
void trm_init(int rows, int cols, char *name)
{
    /* initialize the terminal i/o variables */
    maxcol = cols-1; col = 0;
    maxrow = rows-1; row = 0;
    wptr = word; wcnt = 0;
    scnt = 0;

}

/* trm_done - finish terminal i/o */
void trm_done()
{
	/* Do nothing now */
}

/* trm_get - get a line */
char *trm_get(char *line)
{
 	return (trm_line(line));
}

/* trm_str - output a string */
void trm_str(char *str)
{
	glk_put_string(str);
}

/* trm_xstr - output a string without logging or word wrap */
void trm_xstr(char *str)
{
	glk_put_string(str);
}

/* trm_chr - output a character */
void trm_chr(int ch)
{
	if (ch == '\t')
		glk_put_string("    ");
	else
		glk_put_char(ch);
}

/* trm_word - output the current word */
void trm_word()
{
	/* Do nothing */
}

/* trm_eol - end the current line */
void trm_eol()
{
    glk_put_char('\n');

}

/* trm_wait - wait for the user to type return */
void trm_wait()
{
    /* Do nothing.  GLK does waiting for us */
}

/* trm_line - get an input line */
char *trm_line(char *line)
{
	event_t event;

	do {
		glk_request_line_event(window, line, LINEMAX, 0);
		glk_select(&event);
	
		switch (event.type) {
		case evtype_LineInput:
			line[event.val1] = 0;	/* Null terminate it */
			break;
		};
	} while (event.type != evtype_LineInput);

	return line;
}

