
/* advtrm.c - terminal i/o routines */
/*
	Copyright (c) 1986, by David Michael Betz
	All rights reserved
*/

#include <stdio.h>

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
trm_init(rows,cols,name)
  int rows,cols; char *name;
{
    /* initialize the terminal i/o variables */
    maxcol = cols-1; col = 0;
    maxrow = rows-1; row = 0;
    wptr = word; wcnt = 0;
    scnt = 0;

    /* open the log file */
    if (name && (logfp = fopen(name,"w")) == NULL)
	error("can't open log file");
}

/* trm_done - finish terminal i/o */
trm_done()
{
    if (wcnt) trm_word();
    if (logfp) fclose(logfp);
}

/* trm_get - get a line */
char *trm_get(line)
  char *line;
{
    if (wcnt) trm_word();
    while (scnt--) putchr(' ');
    row = col = scnt = 0;
    return (trm_line(line));
}

/* trm_str - output a string */
trm_str(str)
  char *str;
{
    while (*str)
	trm_chr(*str++);
}

/* trm_xstr - output a string without logging or word wrap */
trm_xstr(str)
  char *str;
{
    while (*str)
	putch(*str++,stdout);
}

/* trm_chr - output a character */
trm_chr(ch)
  int ch;
{
    switch (ch) {
    case ' ':
	    if (wcnt)
		trm_word();
	    scnt++;
	    break;
    case '\t':
	    if (wcnt)
		trm_word();
	    scnt = (col + 8) & ~7;
	    break;
    case '\n':
	    if (wcnt)
		trm_word();
	    trm_eol();
	    scnt = 0;
	    break;
    default:
	    if (wcnt < WORDMAX) {
	        *wptr++ = ch;
	        wcnt++;
	    }
	    break;
    }
}

/* trm_word - output the current word */
trm_word()
{
    if (col + scnt + wcnt > maxcol)
	trm_eol();
    else
	while (scnt--)
	    { putchr(' '); col++; }
    for (wptr = word; wcnt--; col++)
	putchr(*wptr++);
    wptr = word;
    wcnt = 0;
    scnt = 0;
}

/* trm_eol - end the current line */
trm_eol()
{
    putchr('\n');
    if (++row >= maxrow)
	{ trm_wait(); row = 0; }
    col = 0;
}

/* trm_wait - wait for the user to type return */
trm_wait()
{
    trm_xstr("  << MORE >>\r");
    waitch();
    trm_xstr("            \r");
}

/* trm_line - get an input line */
char *trm_line(line)
  char *line;
{
    char *p;
    int ch;

    p = line;
    while ((ch = getchr()) != EOF && ch != '\n')
	switch (ch) {
	case '\177':
	case '\010':
		if (p != line) {
		    if (ch != '\010') putchr('\010',stdout);
		    putchr(' ',stdout);
		    putchr('\010',stdout);
		    p--;
		}
		break;
	default:
		if ((p - line) < LINEMAX)
		    *p++ = ch;
		break;
	}
    *p = 0;
    return (ch == EOF ? NULL : line);
}

/* getchr - input a single character */
int getchr()
{
    int ch;

    if ((ch = getch()) != EOF && logfp)
	putch(ch,logfp);
    return (ch);
}

/* putchr - output a single character */
putchr(ch)
  int ch;
{
    if (logfp) putch(ch,logfp);
    putch(ch,stdout);
}
