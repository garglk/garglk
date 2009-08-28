/* os_none.c -- interface routine template for generic system */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                        */
/* This is part of the source for AGiliTy, the (Mostly) Universal  */
/*       AGT Interpreter                                           */

/* This should work on any system supporting ANSI C, at the price
     of playing without a status line or other system-specific 
     i/o features. */



#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <ctype.h>
#include <limits.h>

#include "agility.h"
#include "interp.h"


int scroll_count; /* Count of how many lines we have printed since
		     last player input. */
void print_statline(void);




void agt_delay(int n)
/* This should wait for n seconds */
{
  if (fast_replay || BATCH_MODE) return;
  print_statline();
  /* sleep(n);*/
}


void agt_tone(int hz,int ms)
/* Produce a hz-Hertz sound for ms milliseconds */
{
  return;
}

int agt_rand(int a,int b)
/* Return random number from a to b inclusive */
{
    return a+(rand()>>2)%(b-a+1);
}


char *agt_input(int in_type)
/* read a line from the keyboard, allocating space for it using malloc */
/* in_type: 0=command, 1=number, 2=question, 3=userstr, 4=filename,*/
/*             5=RESTART,RESTORE,UNDO,QUIT */
/*  Negative values are for internal use by the interface (i.e. this module) */
/*      and so are free to be defined by the porter. */
{
  static int input_error=0;
  char *s;

  scroll_count=0;
  print_statline();
  s=rmalloc(1000);

  if (fgets(s,1000,stdin)==NULL) {
    if (++input_error>25) {
      printf("Read error on input\n");
      exit(EXIT_FAILURE);
    }
    s[0]=0;
    clearerr(stdin);
  } else input_error=0; /* Reset counter */

  if (DEBUG_OUT) 
    fprintf(debugfile,"%s\n",s);
  if (script_on) textputs(scriptfile,s);
  return s;
}

char agt_getkey(rbool echo_char)
/* Reads a character and returns it, possibly reading in a full line
  depending on the platform */
/* If echo_char=1, echo character. If 0, then the character is not 
     required to be echoed (and ideally shouldn't be) */
{
  char *t, *s, c;

  print_statline();

/* We use agt_input here only because in pure ANSI C there is no way of */
/*   reading in input from the keyboard without waiting for return. */
/* In general agt_getkey() and agt_input() will be independent. */
  t=agt_input(-1); 
  for(s=t;isspace(*s);s++);
  c=*s;
  rfree(t);

  return c;
}

void agt_textcolor(int c)
/* Set text color to color #c, where the colors are as follows: */
/*  0=Black, 1=Blue,    2=Green, 3=Cyan, */
/*  4=Red,   5=Magenta, 6=Brown,  */
/*  7=Normal("White"-- which may actually be some other color) */
/*     This should turn off blinking, bold, color, etc. and restore */
/*     the text mode to its default appearance. */
/*  8=Turn on blinking.  */
/*  9= *Just* White (not neccessarily "normal" and no need to turn off */
/*      blinking)  */
/* 10=Turn on fixed pitch font.  */
/* 11=Turn off fixed pitch font */
/* Also used to set other text attributes: */
/*   -1=emphasized text, used (e.g.) for room titles */
/*   -2=end emphasized text */
{
#if 0
  if (c==-1) writestr("<<");
  if (c==-2) writestr(">>");
#endif
  return;
}



void agt_statline(const char *s)
/* Output a string on the status line, if possible */
{
  return;
}


void agt_clrscr(void)
/* This should clear the screen and put the cursor at the upper left
  corner (or one down if there is a status line) */
{
  int i;

  for(i=0;i<screen_height;i++)
    printf("\n");
  if (DEBUG_OUT) fprintf(debugfile,"\n\n<CLRSCR>\n\n");
  if (script_on) textputs(scriptfile,"\n\n\n\n");
  scroll_count=0;
}


void agt_puts(const char *s)
{
  printf("%s",s);
  curr_x+=strlen(s);
  if (DEBUG_OUT) fprintf(debugfile,"%s",s);
  if (script_on) textputs(scriptfile,s);
}

void agt_newline(void)
{
  printf("\n");
  curr_x=0;
  if (DEBUG_OUT) fprintf(debugfile,"\n");
  if (script_on) textputs(scriptfile,"\n");
  scroll_count++;
  if (scroll_count>=screen_height-2) {
    printf("  --MORE--");  /* If possible, this should disappear after 
			      a key is pressed */
    agt_waitkey();
  }
}


static unsigned long boxflags;
static int box_startx; /* Starting x of box */
static int box_width;
static int delta_scroll; /* Amount we are adding to vertical scroll 
			    with the box */
static void boxrule(void)
/* Draw line at top or bottom of box */
{ 
  int i;
  
  agt_puts("+");
  for(i=0;i<box_width;i++) agt_puts("-");
  agt_puts("+");
}

static void boxpos(void)
{
  int i;

  agt_newline();
  for(i=0;i<box_startx;i++) agt_puts(" ");
}

void agt_makebox(int width,int height,unsigned long flags)
/* Flags: TB_TTL, TB_BORDER */
{
  boxflags=flags;
  box_width=width;
  if (boxflags&TB_BORDER) width+=2;  /* Add space for border */
  box_startx=(screen_width-width)/2; /* Center the box horizontally */
  delta_scroll=height;
  boxpos();
  if (boxflags&TB_BORDER) {
    boxrule();
    boxpos();
    agt_puts("|");
  }
}

void agt_qnewline(void)
{
  if (boxflags&TB_BORDER) agt_puts("|");
  boxpos();
  if (boxflags&TB_BORDER) agt_puts("|");
}

void agt_endbox(void)
{
  if (boxflags&TB_BORDER) {
    agt_puts("|");
    boxpos();
    boxrule();
  }
  scroll_count+=delta_scroll;
  agt_newline(); /* NOT agt_qnewline() */
}


#define opt(s) (!strcmp(optstr[0],s))

rbool agt_option(int optnum,char *optstr[],rbool setflag)
/* If setflag is 0, then the option was prefixed with NO_ */
{
  /* Return 1 if the option is recognized */
  return 0; 
}

#undef opt

genfile agt_globalfile(int fid)
{
  if (fid==0) 
    return badfile(fCFG); /* Should return FILE* to open
			      global configuration file */
  else return badfile(fCFG);
}


void init_interface(int argc,char *argv[])
{
  script_on=0;scriptfile=badfile(fSCR);
  scroll_count=0;
  center_on=par_fill_on=0;
  debugfile=stderr;
  DEBUG_OUT=0; /* If set, we echo all output to debugfile */
       /* This should be 1 if stderr points to a different */
       /* device than stdin. (E.g. to a file instead of to the */
       /* console) */
  screen_height=25;  /* Assume PC dimensions */
  status_width=screen_width=80;
  agt_clrscr();
}

void start_interface(fc_type fc)
{
  if (stable_random)  
    srand(6);
  else 
    srand(time(0));
}

void close_interface(void)
{
  if (filevalid(scriptfile,fSCR))
    close_pfile(scriptfile,0);
}
