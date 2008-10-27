/* os_curse.c -- interface routines for ncurses systems   */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                        */
/* This is part of the source for the (Mostly) Universal  */
/*       AGT Interpreter                                  */



#define RVID_BOX 1   /* Make box reverse video? */
#define DEBUG_KEY 0

#if 0
#define IMMED  /* Immediate update mode? */
#endif

#define REFRESH_LINE_CNT 4  /* Redisplay screen every <n> lines scrolled */

/*---------------------------------------------------------------------*/
/*  Header Files and Prototypes of terminal functions                  */
/*---------------------------------------------------------------------*/
#include "agility.h"
#include "interp.h"

#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <sys/types.h>

#ifdef LINUX
#include <linux/kd.h>  /* Needed for the font changing commands */
#endif





#ifdef TIOCGWINSZ   /* Both of these need to be defined. */
#ifdef SIGWINCH
#define CATCH_WINDOW_CHANGE
#endif
#endif

/* 0:Black, 1:Red, 2:Green, 3:Brown, 4:Blue, 5:Magenta, 6:Cyan, 7:White */
/* 3n=foreground, 4n=background, semicolon seperated */
/* The AGT color codes are as follows: */
/*  0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Brown, 7=White */

int agt_col[8]={COLOR_BLACK,COLOR_BLUE,COLOR_GREEN,COLOR_CYAN,
		COLOR_RED,COLOR_MAGENTA,COLOR_YELLOW,COLOR_WHITE};
rbool color_on;



/*---------------------------------------------------------------------*/
/*  Misc. Global variables                                             */
/*---------------------------------------------------------------------*/

static rbool nav_arrow;  /* NAVARROW set? */
static rbool lower_file; /* Lowercase resource files? */
static rbool font_enable;
static fc_type game_fc;
static char *viewer_name; /* Name of image viewing program */

rbool have_compass;
static int status_height; /* Number of lines in status line */
 
static int scroll_count; /* Count of how many lines we have printed since
		     last player input. */
volatile int curr_y, winsize_flag; 

WINDOW *mainwin, *statwin;

/* This gives the first function key in the following table;
   earlier entries are other macro keys. */
#define FIRST_FKEY 10 

static char *fkey_text[FIRST_FKEY+64]={ /* Text for function keys */
  "exit\n",
  "w\n","e\n","n\n","s\n",
  "nw\n","sw\n","se\n","ne\n",
  "enter\n",
  "get ",  /* <== This is FIRST_FKEY */
  "drop ",
  "examine ","read ",
  "open ","close ",
  "inventory\n","look\n",
  "score\n", "help\n",  
  "save\n","restore\n",
  "","","","","","","","","","","","","",
  "","","","","","","","","","","","","",
  "","","","","","","","","","","","","",
  "","","","","","","","","","","","",""
};

/* These are the macro-bindable non-function keys. */
static const char *keylist[FIRST_FKEY]={
  "del",
  "left","right","up","down",
  "home","end","pgdn","pgup",
  "ins"};

enum {fn_del=1010,fn_left,fn_right,fn_up,fn_down,fn_home,fn_end,fn_pgdn,
      fn_pgup, fn_ins} fn_type;

#if 0
{
  "undo\n",
  "get ","drop ",
  "unlock ","read ",
  "open ","close ",
  "inventory\n","look\n",  
  "wait\n",
  "save\n","restore\n"};
#endif

/* Other candidates: view, wait (z), examine (x), again (g)   */    


/*---------------------------------------------------------------------*/
/*  Misc. minor functions                                              */
/*---------------------------------------------------------------------*/

#ifdef IMMED
#define updatesys()   print_statline();
#else
#define updatesys()   print_statline();wrefresh(mainwin);
#endif


void agt_delay(int n)
{
  if (BATCH_MODE) return;
  updatesys();
  sleep((n+1)/2); /* In theory this should be sleep(n) */
}

void agt_tone(int hz,int ms)
/* Produce a hz-Hertz sound for ms milliseconds */
{
  if (BATCH_MODE) return;
  return;
}


int agt_rand(int a,int b)
/* Return random number from a to b inclusive */
{
  int n;
  n=a+(rand()>>2)%(b-a+1);
  return n;
}

genfile agt_globalfile(int fid)
{
  char *homedir;
  FILE *cfgfile;
  char *fname;

  if (fid!=0) return badfile(fCFG);
  homedir=getenv("HOME");
  fname=rmalloc(strlen(homedir)+10);
  sprintf(fname,"%s/.agilrc",homedir);
  cfgfile=fopen(fname,"r");
  return cfgfile;
}

void script_out(const char *s)
{
  if (DEBUG_OUT) 
    fprintf(debugfile,"%s",s);
  if (script_on) fprintf(scriptfile,"%s",s);
}



/*---------------------------------------------------------------------*/
/*  Character Appearance Functions                                     */
/*---------------------------------------------------------------------*/

#define STATUS_STATE 0xA007
#define START_STATE 0x0007

/* For state:
     currently low 4 bits are color.
     bit 13 (0x2000) indicates whether we are in standout mode or not
     bit 14 (0x4000) indicates whether we are blinking or not 
     bit 15 (0x8000) indicates whether bold is on or not */

#define BOLD_BIT 0x8000
#define BLINK_BIT 0x4000
#define SL_BIT 0x2000

static int STAT_ATTR;  /* Attribute settings for status line */

typedef unsigned short vstate;
static vstate state_stack[16];
int stateptr=0;  /* Points into state stack */

#define cstate state_stack[stateptr]  /* Current state */

volatile short *r_f,*r_b;


static void set_state(vstate state)
{
  int c; /* Color */
  int attr;

  if (BATCH_MODE) return;

  attr=A_NORMAL;

  if (color_on) {
    c=state & 0xF;
    if (state & SL_BIT)    /* Status line state */
      c+=8;  
    if (state & BOLD_BIT)  /* BOLD */
      if (c==7) c=6;  /* BOLD text will be yellow if not in Status line */
    attr=COLOR_PAIR(c+1);
  }

  if (state & BOLD_BIT)  /* BOLD */    
    attr|=A_BOLD;

  if (!color_on) 
    if (state & SL_BIT)
      attr|=A_REVERSE;

  if (state & BLINK_BIT) /* Blinking */
    attr|=A_BLINK;

  wattrset(mainwin,attr);

  cstate=state;
}

static void push_state(vstate state)
{
  if (stateptr==15) fatal("State stack overflow!");
  stateptr++;
  set_state(state);
}

static void pop_state(void)
{
  if (stateptr==0) fatal("State stack error: POP without PUSH!");
  stateptr--;
  set_state(cstate);
}



void agt_textcolor(int c)
/* Set text color to color #c, where the colors are as follows: */
/*  0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Brown, */
/*  7=White("normal"), 8=Blinking, */ 
/*  9= *Just* White (not neccessarily "normal" and no need to turn off */
/*      blinking)  */
/* Also used to set other text attributes: */
/* -1=emphasized text, used (e.g.) for room titles */
/* -2=end emphasized text */
{
  vstate nstate;

  nstate=cstate;
  if (c==-1) 
    nstate=nstate|0x8000;
  else if (c==-2) 
    nstate=nstate & ~0x8000; /* BOLD off */
  else if (c==8) 
    nstate=nstate | 0x4000;  /* BLINK on */
  else if (c==7)  
    nstate=0x0007;   /* "Normal" */
  else if (c==9)
    nstate=(nstate & ~0xF) | 7; /* Set color to 7: white */
  else if (c>=0 && c<7)
    nstate=(nstate & ~0xF) | c; /* Set color to c */
  set_state(nstate);
}




/*---------------------------------------------------------------------*/
/*  Low-level Terminal Functions: gotoxy(), etc.                       */
/*---------------------------------------------------------------------*/

static void fix_loc(void)
/* This fixes the location if we are restoring from a suspended state */
/* Also corrects for a changed window size */
{
  struct winsize winsz;
  int new_width, new_height;

#ifdef CATCH_WINDOW_CHANGE
  if (winsize_flag
      && ioctl(STDOUT_FILENO,TIOCGWINSZ,&winsz)==0
      && winsz.ws_row!=0 && winsz.ws_col!=0)  {

    winsize_flag=0;
    new_width=winsz.ws_col;
    new_height=winsz.ws_row;
    resizeterm(new_height,new_width);

    set_state(7); /* Make sure we are in normal screen state */
    wresize(mainwin,new_height-status_height,new_width);
    wresize(statwin,1,new_width);

    screen_width=status_width=new_width;
    screen_height=new_height-status_height;

    if (curr_x>screen_width) 
      curr_x=screen_width-1;
    if (curr_y>screen_height) 
      curr_y=screen_height-1;

    wmove(mainwin,curr_y,curr_x);
  }
#endif
}


#ifdef CATCH_WINDOW_CHANGE
void window_change(int signum)
{
  signal(SIGWINCH,window_change);
  winsize_flag=1;
}
#endif


void int_handler(int signum)
{
  close_interface();
  exit(0);
}




/*---------------------------------------------------------------------*/
/*  Functions to read and translate keys                               */
/*---------------------------------------------------------------------*/

int read_a_key(void)
{
  int c;
  int i;

  c=wgetch(mainwin);
  switch(c) 
    {
    case KEY_HOME:
      if (nav_arrow) return fn_home; /* Otherwise fall through... */
    case '\001': return 1006; /* Ctrl-A ==> Home */
    case KEY_LEFT:
      if (nav_arrow) return fn_left; /* Otherwise fall through... */
    case '\002': return 1002; /* Ctrl-B ==> Left arrow */
    case KEY_END:
      if (nav_arrow) return fn_end; /* Otherwise fall through... */
    case '\005': return 1007; /* Ctrl-E ==> End */
    case KEY_RIGHT:
      if (nav_arrow) return fn_right; /* Otherwise fall through... */
    case '\006': return 1003; /* Ctrl-F ==> Right arrow */
    case '\010': /* Fall though... */
    case KEY_BACKSPACE:
    case '\177': return 1000;/* Ctrl-H and <Del> ==> Backspace */
    case KEY_DC: 
      if (nav_arrow) return fn_del; /* Otherwise fall through... */
      return 1001; /* Delete */
    case '\013': return 1500;  /* Ctrl-K */
    case '\014': return 1502;  /* Ctrl-L */
    case KEY_DOWN:
      if (nav_arrow) return fn_down; /* Otherwise fall through... */
    case '\016': return 1005;  /* Ctrl-N ==> Down arrow */
    case KEY_UP:
      if (nav_arrow) return fn_up; /* Otherwise fall through... */
    case '\020': return 1004;  /* Ctrl-P ==> Up arrow */
    case '\031': return 1501;  /* Ctrl-Y */
    case KEY_PPAGE: 
      if (nav_arrow) return fn_pgup; /* Otherwise fall through... */
      return 1008; /* Page Up */
    case KEY_NPAGE: 
      if (nav_arrow) return fn_pgdn; /* Otherwise fall through... */
      return 1009; /* Page Down */
    default: 
      for (i=0;i<64;i++)
	if (c==KEY_F(i)) {
	  return 1010+FIRST_FKEY+i-1;
	}
      if (isprint(c) || c=='\n' || c=='\r') return c;
      return 2000;
    }
}

/*
   1000= backspace
   1001= delete
   1002= left
   1003= right
   1004= up
   1005= down
   1006= home
   1007= end
   1008= page up
   1009= page down  
   101n= Function key F<n>. (F0==F10)
   1020,1021: F11,F12 (?)
   ... 1074 
   1500= ^K
   1501= ^Y
   1502= ^L
   2000= unknown control character
*/



/*---------------------------------------------------------------------*/
/*  Functions for the Built-In Line Editor                             */
/*---------------------------------------------------------------------*/

void agt_putc(char c)
{
  fix_loc();  /* Fixup if returning from being suspended */
  waddch(mainwin,c);
  curr_x+=1;
}


static int base_x,base_y; /* Base X value of first character of input */

/* This assumes the cursor is always positioned on the string
   being edited */
static void update_cursor(int cursor)
{
  curr_x=base_x+cursor;
  curr_y=base_y;
  while (curr_x>=screen_width) {
    curr_x-=screen_width;
    curr_y++;
  }
  wmove(mainwin,curr_y,curr_x);
}

static rbool old_yheight=0;

static void update_line(int cursor,char *buff)
{
  int yval, yheight, xpos, i;
  
  yheight=(base_x+strlen(buff)+screen_width-1)/screen_width; /* Round up */
  wmove(mainwin,screen_height-1,screen_width-1);
  while (base_y+yheight>screen_height && base_y>0) {
    base_y--;
    waddch(mainwin,'\n');
  }
  for(yval=yheight;yval<old_yheight;yval++) {
    wmove(mainwin,base_y+yval,0);
    wclrtoeol(mainwin);
  }
  old_yheight=yheight;
  xpos=cursor-1;
  if (xpos<0) xpos=0;
  update_cursor(xpos);
  for(yval=curr_y-base_y;yval<yheight;yval++) {
    wclrtoeol(mainwin);
    for(i=curr_x;i<screen_width;i++) {
      if (buff[xpos]==0) break;
      waddch(mainwin,buff[xpos++]);
    }
    if (base_y+yval+1<screen_height) 
      wmove(mainwin,base_y+yval+1,0);
    else break;
  }
  update_cursor(cursor);
}



static void redisplay_line(char *buff)
{
  update_line(0,buff);
}

#define ENABLE_HIST
static char **hist=NULL;
static long histcount=0;
static long histmax=0; /* No limit on history */



static char *line_edit(int state)
     /* state=0, input a line.
	state=1, input character with echo
	state=2, input character with no echo */
{
  static char *yank_text=NULL;
  rbool editmode, exitflag;
  int key;
  int buffleng, buffspace;  /* buffspace is space allocated for buffer,
			       buffleng is space really used */
  static int cursor; /* Where the insertion point is in buffer:
		 insert at this location */
  char *buff, *savebuff;
  int i, curr_hist;

  buff=rmalloc(2);
  savebuff=NULL;  curr_hist=histcount;
  buff[0]=buff[1]=0;
  cursor=buffleng=0;buffspace=2;
  base_x=curr_x; base_y=curr_y;
  editmode=(state==0);
  old_yheight=0;
  scroll_count=0;

  wmove(mainwin,curr_y,curr_x);
  for(exitflag=0;!exitflag;) {
    wrefresh(mainwin);
    key=read_a_key();
    if (DEBUG_KEY) wprintw(mainwin,"{%d}",key);
    if (key<1000) { /* Real character entered */
      if (state>0) { /* One key */
	if (state==1) {
	  agt_putc(key);
	}       
	buff[0]=key; buff[1]=0;
	buffleng=1;
	break;
      } 

#ifdef ENABLE_HIST
      if (savebuff!=NULL) {
	rfree(savebuff);
	buff=rstrdup(buff);
	buffspace=buffleng+1;
      }
#endif
      if (key=='\n' || key=='\r') {
	update_cursor(buffleng);      
	break;
      }
      /* add character to buffer */
      buffleng++;
      if (buffleng+1>buffspace) buffspace+=20;
      buff=rrealloc(buff,buffspace);
      for(i=buffleng;i>cursor;i--)
	buff[i]=buff[i-1];      
      buff[cursor++]=key;
      update_line(cursor,buff);
    } 
    else switch(key-1000) 
      { /* Special key */


      case 0: /* Backspace */
	if (!editmode || cursor==0) break;
        cursor--;
	/* Fall through... */


      case 1: /* Delete */
	if (!editmode || buffleng==cursor) break;
#ifdef ENABLE_HIST
	if (savebuff!=NULL) {
	  rfree(savebuff);
	  buff=rstrdup(buff);
	  buffspace=buffleng+1;
	}
#endif
	for(i=cursor;i<buffleng;i++)
	  buff[i]=buff[i+1];
	buffleng--;
	update_line(cursor,buff);
	break;


      case 2: /* Left arrow */
	if (editmode && cursor>0) 
	  update_cursor(--cursor);
	break;


      case 3: /* Right arrow */
	if (editmode && cursor<buffleng)	  
	  update_cursor(++cursor);
	break;


#ifdef ENABLE_HIST
      case 4: /* Up arrow: Command history */	
	if (!editmode || histcount==0) break;
	if (savebuff==NULL) { /* Save current line */
	  savebuff=buff;
	  curr_hist=histcount;
	}
	curr_hist--;
	if (curr_hist<0) {
	  curr_hist=0;	
	  break;
	}
	buff=hist[curr_hist];
	buffleng=strlen(buff);
	cursor=0;
	redisplay_line(buff);
	break;

      case 5: /* Down arrow: Command history */
	if (!editmode || savebuff==NULL) break;
	curr_hist++;
	if (curr_hist>=histcount) { 
	  /* Reached bottom: restore original line */
	  curr_hist=histcount;
	  buff=savebuff;
	  savebuff=NULL;
	} else
	  buff=hist[curr_hist];
	buffleng=strlen(buff);
	cursor=0;
	redisplay_line(buff);
	break;
#endif

      case 6: /* Home */
	if (!editmode) break;
	cursor=0;
	update_cursor(cursor);
	break;

      case 7: /* End */
	if (!editmode) break;
	cursor=buffleng;
	update_cursor(cursor);
	break;

      case 8: /* Page up : Scroll back */
      case 9: /* Page down */
	break;     


      case 500: /* Ctrl-K: Delete to EOL */
	if (!editmode) break;
#ifdef ENABLE_HIST
	if (savebuff!=NULL) {
	  rfree(savebuff);
	  buff=rstrdup(buff);
	  buffspace=buffleng+1;
	}
#endif
	rfree(yank_text);
	yank_text=rstrdup(buff+cursor);
	buffleng=cursor;
	buff[buffleng]=0;
	update_line(cursor,buff);
	break;


      case 501:  /* Ctrl-Y: Yank */
	if (!editmode) break;
	{ 
	  int txtleng;
#ifdef ENABLE_HIST
	  if (savebuff!=NULL) {
	    rfree(savebuff);
	    buff=rstrdup(buff);
	    buffspace=buffleng+1;
	  }
#endif
	  txtleng=strlen(yank_text);
	  buffleng+=txtleng;
	  while(buffleng+1>buffspace) buffspace+=20;
	  buff=rrealloc(buff,buffspace);
	  for(i=buffleng;i>=cursor+txtleng;i--)
	    buff[i]=buff[i-txtleng];
	  for(i=0;i<txtleng;i++)
	    buff[cursor+i]=yank_text[i];
	  update_line(cursor,buff);
	  cursor+=txtleng;
	  update_cursor(cursor);
	}
	break;


      case 502: /* Ctrl-L: Redraw screen */
	redrawwin(mainwin);
	redrawwin(statwin);
	if (editmode) 
	  redisplay_line(buff);
	break;

	/* Function key <key-1010> */
      default:
	if (key-1010<0 || key-1010>=FIRST_FKEY+64) break;
	if (!editmode) break;
	{ 
	  int txtleng;
#ifdef ENABLE_HIST
	  if (savebuff!=NULL) {
	    rfree(savebuff);
	    buff=rstrdup(buff);
	    buffspace=buffleng+1;
	  }
#endif
	  txtleng=strlen(fkey_text[key-1010]);
	  if (fkey_text[key-1010][txtleng-1]=='\n') {
	    /* Newlines should only appear at the end; they indicate
	       that this function key should cause the line to be entered. */
	    txtleng--;
	    exitflag=1; /* Finish entering this line */
	  }
	  buffleng+=txtleng;
	  while(buffleng+1>buffspace) buffspace+=20;
	  buff=rrealloc(buff,buffspace);
	  for(i=buffleng;i>=cursor+txtleng;i--)
	    buff[i]=buff[i-txtleng];
	  for(i=0;i<txtleng;i++)
	    buff[cursor+i]=fkey_text[key-1010][i];	  
	  update_line(cursor,buff);
	  cursor+=txtleng;
	  update_cursor(cursor);
	}
	break;
      }
    if (exitflag) break;
  }
  buff=rrealloc(buff,(buffleng+1));
  if (state==0) {
    if (histmax==0 || histcount<histmax) 
      hist=rrealloc(hist,(++histcount)*sizeof(char*));
    else 
      for(i=0;i<histcount-1;i++)
	hist[i]=hist[i+1];
    hist[histcount-1]=rstrdup(buff);
  }
  return buff;
}




/*---------------------------------------------------------------------*/
/*  High Level Input Functions                                         */
/*---------------------------------------------------------------------*/

char *agt_input(int in_type)  
/* read a line from the keyboard, allocating space for it using malloc */
/* See os_none.c for a discussion of in_type; we ignore it. */
{
  char *s;

  updatesys();
  fix_loc();

  s=line_edit(0);

#ifdef TEST_NONL  /* This is for debugging */
  {
    char *t;
    for(t=s+strlen(s)-1;(t>s) && (*t=='\n');t--);
    if (*t!=0 || t<s) t++;
    *t=0;
  }
#endif

  script_out(s);
  agt_newline();
  scroll_count=0;
  return s;
}




char agt_getkey(rbool echo_char)
/* Reads a character and returns it */
/* If echo_char=1, echo character. If 0, don't */
{
  char c;
  char *s;

  updatesys();
  scroll_count=0;

  s=line_edit(echo_char?1:2);
  c=s[0];
  if (echo_char==1) {
    script_out(s);
    agt_newline();
  }
  rfree(s);

  scroll_count=0;
  return c;
}




/*---------------------------------------------------------------------*/
/*  Printing the Status Line                                           */
/*---------------------------------------------------------------------*/

static void print_compass(void)
{
  int cwidth, i;

  if (status_width< 9+4*12) return;

  waddch(statwin,'\n');
  waddstr(statwin,"  EXITS: ");cwidth=9;
  for(i=0;i<12;i++)
    if (compass_rose & (1<<i)) {
      wprintw(statwin,"%s ",exitname[i]);
      cwidth+=strlen(exitname[i])+1;
    }
  for(i=cwidth;i<status_width;i++) waddch(statwin,' ');
}

void agt_statline(const char *s)
/* Output a string in reverse video (or whatever) at top of screen */
{
  if (BATCH_MODE) return;
  waddstr(statwin,s);
  if (have_compass) 
    print_compass();
  wrefresh(statwin);
}



/*---------------------------------------------------------------------*/
/*  High Level Output Functions                                        */
/*---------------------------------------------------------------------*/

void agt_more(void)
{
  scroll_count=0;
  if (BATCH_MODE) return;  
  waddstr(mainwin,"  --MORE--");
  agt_getkey(0);
  wmove(mainwin,curr_y,0);
  wclrtoeol(mainwin);
}


void agt_newline(void)  
{
  static int refresh_count=0; /* Used for "occasional" refresh */

  script_out("\n");

  if (BATCH_MODE) {
    curr_x=0;
    curr_y++;
    if (curr_y>=screen_height)
      curr_y=screen_height-1; /* Scrolling has occured */
    return;
  }

  if (curr_x<screen_width) /* Make sure we haven't already newlined. */
    waddch(mainwin,'\n');
  getyx(mainwin,curr_y,curr_x);
  scroll_count++;
  refresh_count++;

  if (scroll_count>=screen_height-1)
    agt_more();  /* Print out MORE message */ 
  else {
#ifndef IMMED    
    refresh_count=refresh_count%REFRESH_LINE_CNT;
    if (refresh_count==0) wrefresh(mainwin);
#endif
  }
  fix_loc();  /* Fixup if returning from being suspended */
}


void agt_puts(const char *s)
/* This routine assumes that s is not wider than the screen */
{
  script_out(s);

  if (BATCH_MODE) return;

  fix_loc();  /* Fixup if returning from being suspended */
  curr_x+=strlen(s);
  waddstr(mainwin,s);
  wmove(mainwin,curr_y,curr_x);
}


void agt_clrscr(void)
{
  winsize_flag=0;
  scroll_count=0;
  curr_x=0;curr_y=0;
  script_out("\n\n\n\n\n");

  if (BATCH_MODE) return;

  werase(mainwin);
  wmove(mainwin,0,0);
}




/*---------------------------------------------------------------------*/
/*  Box Printing Routines                                              */
/*---------------------------------------------------------------------*/

static unsigned long boxflags;
static int box_startx; /* Starting x of box */
static int box_width;
static int delta_scroll; /* Amount we are adding to vertical scroll 
			    with the box */

#define bhline() waddch(mainwin,ACS_HLINE)
#define bvline() waddch(mainwin,ACS_VLINE)

static void boxrule(void)
/* Draw line at top or bottom of box */
{ 
  int i;
  
  if (RVID_BOX) agt_puts("  "); 
  else {waddch(mainwin,ACS_ULCORNER);bhline();}
  
  for(i=0;i<box_width;i++)
    if (RVID_BOX) agt_puts(" "); 
    else bhline();

  if (RVID_BOX) agt_puts("  "); 
  else {bhline();waddch(mainwin,ACS_URCORNER);}
}

static void boxpos(void)
{
  int i;

  curr_x=box_startx; curr_y++;
  if (curr_y>=screen_height) curr_y=screen_height-1;
  wmove(mainwin,curr_y,curr_x);

  script_out("\n");
  for(i=0;i<box_startx;i++)
    script_out(" ");
}

void agt_makebox(int width,int height,unsigned long flags)
/* Flags: TB_TTL, TB_BORDER, TB_NOCENT */
{
  int i;

  boxflags=flags;
  box_width=width;
  if (boxflags&TB_BORDER) {  /* Add space for border */
    width+=4;
    height+=2;
  }
  if (boxflags&TB_NOCENT) box_startx=0;
  else box_startx=(screen_width-width)/2; /* Center the box horizontally */
  if (box_startx<0) box_startx=0;

/* Now we need to compute the vertical position of the box */  
  if (flags & TB_TTL) { /* Title: centered horizontally */
    curr_y=(screen_height-height)/2;
    if (curr_y+height+8>=screen_height) /* Make room for credits */
      curr_y=screen_height-height-8-1;
    if (curr_y<0) curr_y=0;
    delta_scroll=0;
    scroll_count=0;
  } else { 
    /* Compute vertical position of non-title box */
    curr_y=curr_y-height;
    if (curr_y<0) {
      delta_scroll=1-curr_y;
      curr_y=0;
    } else delta_scroll=0;
  }

  curr_x=box_startx;
  wmove(mainwin,curr_y,curr_x);

  script_out("\n");
  for(i=0;i<box_startx;i++)
    script_out(" ");

  if (boxflags&TB_BORDER) {
    push_state(cstate | 0x2000); /* Turn on reverse video */
    boxrule();
    boxpos();
    if (RVID_BOX) agt_puts("  ");
    else {bvline();agt_puts(" ");}
  }
}

void agt_qnewline(void)
{
  if (boxflags&TB_BORDER) {
    if (RVID_BOX) agt_puts("  ");
    else {agt_puts(" ");bvline();}
  }
  boxpos();
  if (boxflags&TB_BORDER) {
    if (RVID_BOX) agt_puts("  ");
    else {bvline();agt_puts(" ");}
  }
}

void agt_endbox(void)
{
  if (boxflags&TB_BORDER) {
    if (RVID_BOX) agt_puts("  ");
    else {agt_puts(" ");bvline();}
    boxpos();
    boxrule();
    if (RVID_BOX) pop_state();
  }
  scroll_count+=delta_scroll;
  agt_newline(); /* NOT agt_qnewline() */  
  if ( (boxflags&TB_TTL) ) { 
    agt_puts(" "); /* Neccessary to get things to work right w/xterm */
    while(curr_y<screen_height-8) agt_newline();
  }
}





/*---------------------------------------------------------------------*/
/*  Option Parsing Routines                                            */
/*---------------------------------------------------------------------*/



/* Duplicate s and add '/' if neccessary */
char *makepathentry(char *s, int leng)
{
  char *p;
  rbool addslash;

  addslash=(s[leng-1]!='/');
  p=rmalloc(leng+addslash+1);
  strncpy(p,s,leng);
  if (addslash)
    p[leng++]='/';
  p[leng]=0; /* Mark new end of string */
  return p; 
}

static void free_gamepath(void)
{
  char **p;

  if (gamepath==NULL) return;

  /* We skip the first element which always points to "" */
  for(p=gamepath+1;*p!=NULL;p++)
    rfree(*p);
  rfree(gamepath);
}


static void get_agt_path(void)
{
  char *path, *end;
  int pathleng; /* Number of elements in gamepath */

  free_gamepath();
  pathleng=0;
  path=getenv("AGT_PATH");
  if (path==NULL) return;
  while(*path!=0) { /* Find each componenent of the path */
    while(isspace(*path) || *path==':') path++; /* Skip excess whitespace */
    for(end=path;*end!=0 && !isspace(*end) && *end!=':';end++);
    if (end!=path) { /* There's actually something there */
      pathleng++;
      gamepath=rrealloc(gamepath,(pathleng+1)*sizeof(char*));
      gamepath[pathleng-1]=makepathentry(path,end-path);
      gamepath[pathleng]=NULL;
    }
    path=end;
  }
}


void set_fkey(char *keyname, char *words[], int numwords)
{
  int n,i,leng;
  char *s;
  rbool err;
  static long keys_defined=0;

  err=0; n=0;
  if (tolower(keyname[0])!='f') {
    err=1;
    for(i=0;i<10;i++) 
      if (0==strcasecmp(keylist[i],keyname)) {
	err=0;
	n=i;
      }
  } else {   /* Name of form 'Fnn' */
    n=strtol(keyname+1,&s,0); /* s will point to the first erroneous char */
    if (keyname[1]==0 || *s!=0) err=1;
    if (n>64) err=1;
    n+=FIRST_FKEY;
  }
  if (err) {
    writeln("Unrecognized KEY name.");
    return;
  }
  
  leng=0;
  for(i=0;i<numwords;i++)
    leng+=strlen(words[i]);
  s=rmalloc(leng+numwords+1);
  s[0]=0;

  /* This isn't very efficient, but it doesn't need to be. */
  for(i=0;i<numwords;i++) {
    strcat(s,words[i]);
    strcat(s," ");
  }
  i=strlen(s)-2;
  if (i>=0 && s[i]=='+') {
    s[i]='\n';
    s[i+1]=0;
  }
  if (keys_defined & (1<<n)) rfree(fkey_text[n]);
  fkey_text[n]=s;
  keys_defined|=(1<<n);
}



#define opt(s) (!strcasecmp(optstr[0],s))

rbool agt_option(int optnum,char *optstr[],rbool setflag)
/* If setflag is 0, then the option was prefixed with NO_ */
{
  int i;


  if (opt("COLORTERM") && optnum>=2 && setflag) {
    return 1;  /* Curses version ignores this. */
  }
#ifdef LINUX
  if (opt("FONT")) {
    font_enable=setflag;
    return 1;
  }
#endif
  if (opt("COMPASS")) {
    have_compass=setflag;
    status_height=1+setflag;
    return 1;
  }
  if (opt("KEY") && optnum>=2) {
    set_fkey(optstr[1],optstr+2,optnum-2);
    return 1;
  }
  if (opt("HISTORY")) {
    histmax=strtol(optstr[1],NULL,10);
    if (histmax<0) histmax=0;
    return 1;
  }
  if (opt("LOWER_FILE")) {
    lower_file=setflag;
    return 1;
  }
  if (opt("NAVARROW")) {
    nav_arrow=setflag;
    return 1;
  }
  if (opt("VIEWER")) {
    viewer_name=rstrdup(optstr[1]);
    return 1;
  }
  if (opt("PATH") && optnum>=2) {
    if (gamepath!=NULL) return 1;  /* First setting wins */
    gamepath=rmalloc((optnum+1)*sizeof(char*)); /* optnum-1+2 */
    gamepath[0]=""; /* ==> The current directory */
    for(i=1;i<optnum;i++) /* Starting at the 2nd entry of both */
      gamepath[i]=makepathentry(optstr[i],strlen(optstr[i]));
    gamepath[optnum]=NULL;
    return 1;
  }
  return 0;
}

#undef opt




/*---------------------------------------------------------------------*/
/*  Interface Initializations                                          */
/*---------------------------------------------------------------------*/

void init_interface(int argc,char *argv[])
{
  struct stat fileinfo;
  ino_t inode;
  dev_t devnum;

  viewer_name=NULL;
  nav_arrow=0;
  have_compass=0; status_height=1;
  script_on=0;scriptfile=NULL;
  center_on=0;
  par_fill_on=0;
  scroll_count=0;

  font_enable=0;
  
  get_agt_path();

  if (BATCH_MODE) {
    screen_height=25;
    screen_width=status_width=80;
    return;
  }

 /* DEBUG_OUT should be true if and only if debugfile and stdin 
    refer to different devices */
  fstat(STDERR_FILENO,&fileinfo);
  inode=fileinfo.st_ino;
  devnum=fileinfo.st_dev;
  fstat(STDIN_FILENO,&fileinfo);  
  DEBUG_OUT=(inode!=fileinfo.st_ino || devnum!=fileinfo.st_dev);
  debugfile=stderr;

/* DEBUG_OUT=!isatty(STDERR_FILENO); */
  setvbuf(stdout,NULL,_IONBF,BUFSIZ);

#ifdef LINUX
  /* Switch to PC character set */
  if (!fix_ascii_flag) printf("\033(U");
#endif

  initscr();
  intrflush(stdscr,TRUE);
  keypad(stdscr,TRUE);
  nonl();
  cbreak();
  noecho();

#ifndef NEXT /* ?!?! */
  if (atexit(close_interface)!=0) 
    {printf("INTERNAL ERROR: Unable to register restore_terminal.\n");
     exit(0);}
#endif

  screen_height=LINES;
  screen_width=COLS;
#ifdef CATCH_WINDOW_CHANGE
  signal(SIGWINCH,window_change);
#endif

  immedok(stdscr,TRUE);
  scrollok(stdscr,TRUE);
  mainwin=stdscr;  /* Just temporarily */

  /* Really need to check control_width here */
  status_width=screen_width;

  signal(SIGINT,int_handler);
  signal(SIGPIPE,SIG_IGN);
}

#ifdef REPLACE_BNW
static void save_old_font(void);
static void restore_font(void);
#endif



void start_interface(fc_type fc)
{
  int bkgd_color;

  if (stable_random) 
    srand(6);
  else 
    srand(time(0));
  game_fc=fc;
  free_gamepath();  /* It's not needed anymore */

  if (BATCH_MODE) {
    agt_clrscr();
    return;
  }

  if (has_colors())
    {
      int i;

      bkgd_color=COLOR_BLACK;
      start_color();
      for(i=0;i<8;i++) {/* init_pair calls to set up colors */
	init_pair(i+1,agt_col[i],bkgd_color);
	init_pair(i+9,agt_col[i],COLOR_BLUE);
      }
      color_on=1;
      STAT_ATTR=COLOR_PAIR(16)|A_BOLD;  /* White on blue */
    }
  else {
    color_on=0;
    STAT_ATTR=A_REVERSE;
  }

 


  clear();refresh(); /* Clean off the screen */

  screen_height=LINES-status_height;
  screen_width=COLS;

  /* Main window: everything but the top line */  
  mainwin=newwin(0,0,status_height,0);
  idlok(mainwin,TRUE); 
  scrollok(mainwin,TRUE);
  keypad(mainwin,TRUE);
#ifdef IMMED
  immedok(mainwin,TRUE);  /* Not sure about this. */
#endif

  /* Status line window */
  statwin=newwin(status_height,0,0,0); 
  idlok(statwin,TRUE);
  wattrset(statwin,STAT_ATTR);
  keypad(mainwin,TRUE);

  set_state(START_STATE);
  agt_clrscr();
}



void close_interface(void)
{
  if (scriptfile!=NULL) 
    close_pfile(scriptfile,0);
  if (BATCH_MODE) return;
  rfree(viewer_name);
  set_state(START_STATE);
  endwin();
}


#ifdef REPLACE_BNW 


/* This cheats a bit, using "hidden" data from filename.c */

static FILE *linopen(char *name, char *ext)
{
  FILE *f;
  char *fname;

  fname=assemble_filename(hold_fc->path,name,ext);
  f=fopen(fname,"rb");
  rfree(fname);
  return f;
}



/*------------------------------------------------------------------*/
/* FONT Routines                                                    */
/*------------------------------------------------------------------*/

static char *save_font;
static int save_font_height;

static int set_font(char *fontdef, int font_height)
{
#ifdef PIO_FONTX
  struct consolefontdesc fontdesc;

  fontdesc.charcount=256;
  fontdesc.charheight=font_height;
  fontdesc.chardata=fontdef;

  return ioctl(STDOUT_FILENO,PIO_FONTX,&fontdesc);
#else
#ifdef PIO_FONT
  return ioctl(STDOUT_FILENO,PIO_FONT,fontdef);
#endif
#endif
}


static int getfont(char *fontdef, int *font_height)
{
  int rval=-1;

#ifdef GIO_FONTX
  struct consolefontdesc fontdesc;  

  fontdesc.chardata=fontdef;
  rval=ioctl(STDOUT_FILENO,GIO_FONTX,&fontdesc);
  *font_height=fontdesc.charheight;
#else
#ifdef GIO_FONT
  rval=ioctl(STDOUT_FILENO,GIO_FONT,save_font);  /* Get current font */
  *font_height=16;
#endif
#endif
  return rval;
}


static void save_old_font(void)
{
  if (BATCH_MODE) return;

  /* Save existing font */
  save_font=rmalloc(256*32);
  if (getfont(save_font,&save_font_height))
    /* Failed... issue warning? */
    rfree(save_font);
}


static void restore_font(void)
{
  if (BATCH_MODE) return;
  if (save_font==NULL || set_font(save_font,save_font_height)) {
    /* Couldn't restore save_font */
#ifdef PIO_FONTRESET
    ioctl(STDOUT_FILENO,PIO_FONTRESET);
#endif
  }    
}



void fontcmd(int cmd,int font)
/* 0=Load font, name is fontlist[font]  
   1=Restore original font  
   2=Set startup font. (<gamename>.FNT)
*/
{
  FILE *fontfile;
  char *buff, *fontname, *s;
  int count, height;

  if (!font_enable) return;
  /* (This should be off by default since it changes the fonts
     on *all* of the VC's) */

  if (cmd==2)
    save_old_font();

  if (cmd==0 || cmd==2) {
    if (cmd==0) {
      fontname=fontlist[font]; 
      if (lower_file)
	for(s=fontname;*s!=0;s++) *s=tolower(*s);
      /* Lowercase the font names as we use them */
    }
    else fontname=game_fc->gamename;

    fontfile=linopen(fontname,".fnt");
    if (fontfile==NULL) return;
    buff=rmalloc(256*32);
    memset(buff,0,256*32);
    fseek(fontfile,0,SEEK_END);
    height=ftell(fontfile);
    if (height%256!=0) {
      /* Error message */
      return;
    }
    height=height/256;

    fseek(fontfile,0,SEEK_SET);
    for(count=0;count<256;count++)
      if (fread(buff+count*32,height,1,fontfile)!=1) break;
    if (count!=256) {
	fclose(fontfile);
	rfree(buff);
	return;
    }
    fclose(fontfile);
    if (!BATCH_MODE)
      if (set_font(buff,height)) {
	/* Print error message */
      }
    rfree(buff);
    return;
  } else if (cmd==1) 
    restore_font();
}



/*------------------------------------------------------------------*/
/* Graphics Routines                                                */
/*------------------------------------------------------------------*/

#define GFX_EXT_CNT 17
/* The extension indicates the video mode the picture was intended
   to be viewed in. */
static char *gfxext[GFX_EXT_CNT]={".pcx",
				  ".p06", /* 640x200x2 */
				  ".p40",".p41",".p42",".p43", /* 320x200x4 */
				  ".p13", /* 320x200x16 */
				  ".p19", /* 320x200x256 */
				  ".p14",".p16", /* 640x200x16, 640x350x16   */
				  ".p18", /* 640x480x16 */
				  ".gif",".png",".bmp",".jpg",
				  ".fli",".flc"}; 


void pictcmd(int cmd,int pict)
/* 1=show global picture, name is pictlist[pict]
   2=show room picture, name is pixlist[pict]
   3=show startup picture <gamename>.P..
  */
{
  char *base, *cmdstr;
  int gmode;
  FILE *pcxfile;

  if (viewer_name==NULL) return;

  if (cmd==1) base=pictlist[pict];
  else if (cmd==2) base=pixlist[pict];
  else if (cmd==3) base=game_fc->gamename;
  else return;

  /* Find graphics file; determine mode from extension... */  
  for(gmode=GFX_EXT_CNT-1;gmode>=0;gmode--) {
    pcxfile=linopen(base,gfxext[gmode]);
    if (pcxfile!=NULL) break;
  }
  if (pcxfile==NULL) return;
  fclose(pcxfile);
  
  base=assemble_filename(hold_fc->path,base,gfxext[gmode]);
  
  cmdstr=rmalloc(strlen(base)+strlen(viewer_name)+40);
  sprintf(cmdstr,"%s %s >/dev/null",viewer_name,base);

  if (!BATCH_MODE) {
    endwin();
    system(cmdstr); /* Don't bother to check return status since
		       there's nothing we could do anyhow */
    wrefresh(mainwin);
    wrefresh(statwin);
  }

  rfree(cmdstr);
  rfree(base);

  return;
}



/*------------------------------------------------------------------*/
/* Sound Routines                                                   */
/*------------------------------------------------------------------*/

#if 0
static rbool repeat_song=0;
static rbool suspend_song=0;
#endif

int musiccmd(int cmd,int song)
/* For cmd=1 or 2, the name of the song is songlist[song]
  The other commands don't take an additional argument.
   1=play song 
   2=repeat song
   3=end repeat
   4=end song
   5=suspend song
   6=resume song
   7=clean-up
   8=turn sound on
   9=turn sound off
   -1=Is a song playing? (0=false, -1=true)
   -2=Is the sound on?  (0=false, -1=true)
*/
{
  if (cmd==8) {
    sound_on=1;
    return 0;
  }
  if (cmd==9) {sound_on=0;return 0;}
  if (cmd==-2) return sound_on;
#if 0
  if (cmd==3) {repeat_song=0;return 0;}
  if (cmd==7) {
    /* Stop song */
    /* Do clean-up */
    return 0;
  }
  if (cmd==2) repeat_song=1;
  if (cmd==1 || cmd==2) {
    /* Start song playing */
  }
  if (cmd==4) {
    /* Stop song */
  }
  if (cmd==5) {
    /* Suspend song */
  }
  if (cmd==6) {
    /* Resume song */
  }
#endif
  return 0;
}

#endif /* REPLACE_BNW */



/*------------------------------------------------------------------*/
/* File Selection Menu                                              */
/*------------------------------------------------------------------*/

#ifdef REPLACE_GETFILE

void list_files(char *type, char *ext)
{
  DIR *currdir;
  struct dirent *entry;
  ITEM **filelist;
  int filecnt, listsize;
  int i,j;
  
  filelist=NULL; filecnt=listsize=0; maxleng=0;
  currdir=opendir(".");
  if (currdir==NULL) return; /* Nothing we can do except give up */
  do {
    entry=readdir(currdir);
    if ( entry!=NULL && check_fname(entry->d_name,ext) ) {
      /* It has the right extension; add it to our list of files */
      if (filecnt>=listsize) {
	listsize+=5;
	filelist=rrealloc(filelist,listsize*sizeof(ITEM*));
      }
      filelist[filecnt]=new_item(entry->d_name,"");
      filecnt++;
    }
  } while (entry!=NULL);
  closedir(currdir);
  if (filecnt==0) return; /* No files */
}

  writestr("Existing ");writestr(type);writestr("files:");


  rfree(filelist);
}




/* This opens the file refered to by fname and returns it */
static FILE *uf_open(char *fname, char *ext, char *otype)
{
  if (otype[0]=='w') { /* Check to see if we are overwriting... */
    FILE *fd;

    fd=fopen(fname,"r");
    if (fd!=NULL) { /* File already exists */
      fclose(fd);
      if (!yesno("This file already exists; overwrite?")) 
	/* That is, DON'T overwrite */
	return NULL;
    }
  }
  return fopen(fname,otype);
}



static char *last_save=NULL;
static char *last_log=NULL;
static char *last_script=NULL;

FILE *get_user_file(int ft)
/* ft= 0:script, 1:save 2:restore, 3:log(read) 4:log(write)  */
/* Should return file in open state, ready to be read or written to,
   as the case may be */
{
  /* int extlen;*/
  char *fname,*otype, *ext;
  char *defname;  /* Default file name */
  char *p,*q;
  FILE *fd;
  char *ftype;

  switch (ft) {
    case 0: ftype="script ";
      defname=last_script;
      otype="a";ext=pSCR; break;
    case 1: ftype="save ";
      defname=last_save;
      otype="wb";ext=pSAV;break;
    case 2: ftype="restore ";
      defname=last_save;
      otype="rb";ext=pSAV;break;
    case 3: ftype="log ";
      defname=last_log;
      otype="r";ext=pLOG;break;
    case 4: ftype="log ";
      defname=last_log;
      otype="w";ext=pLOG;break;
    default: writeln("<INTERNAL ERROR: invalid file type>");
      return NULL;
    }
  if (otype[0]=='r') { /* List available files. */
    list_files(ftype,ext);
    ftype=NULL;
  } else

  writestr("Enter ");
  if (ftype!=NULL) writestr(ftype);
  writestr("file name");
  if (defname!=NULL) {
    writestr(" (");
    writestr(defname);
    writestr(")");
  }
  writestr(": ");

  if (PURE_INPUT) agt_textcolor(-1);
  fname=agt_input(4);
  if (PURE_INPUT) agt_textcolor(-2);

  /* Delete whitespace before and after the file name. */
  for(p=fname;isspace(*p);p++); 
  for(q=fname;*p!=0;p++,q++) 
    *q=*p;
  q--;
  while(isspace(*q)) q--;
  q++;
  *q=0;

  if (q==fname)   /* ie we are left with the empty string */
    if (defname==NULL) {
      writeln("Never mind.");
      rfree(fname);
      return NULL;}
    else {
      rfree(fname);
      fname=defname;
    }

#ifdef UNIX 
  if ( (ft==0 || ft==4) && *fname=='|') { /* pipe => program */
    fd=popen(fname+1,"w");
    if (fd!=NULL) ispipe[ ft==0 ? 0 : 2 ]=1;
  } else if  (ft==3 && *(q-1)=='|')  {  /* pipe <= program */
    *(q-1)=0;
    fd=popen(fname,"r");
    if (fd!=NULL) ispipe[1]=1;
    *(q-1)='|'; /* For next time */
  } else
#endif
    {
      /* We've gotten a new file name; need to add default extension */    
      if (fname!=defname && strlen(ext)>0 && !check_fname(fname,ext))	
	fname=remake_fname(fname,ext);	
      
      fd=uf_open(fname,ext,otype);
    }    

  if (fd==NULL) {
    if (fname!=defname) rfree(fname);
    return NULL;
  }

  switch(ft) 
    {
    case 0: last_script=fname;break;
    case 1: last_save=fname; break;
    case 2: last_save=fname; break;
    case 3: last_log=fname; break;
    case 4: last_log=fname; break;
    }
  if (fname!=defname) rfree(defname);
  return fd;
}


void set_default_filenames(const char *gamename)
{
  fix_filename=2; /* Remove path: by default, all save files go in
		      the *current* directory, not the game's directory */
  last_save=make_fname(gamename,pSAV);
  last_log=make_fname(gamename,pLOG);
  last_script=make_fname(gamename,pSCR);
  fix_filename=0;
}

#endif  /* REPLACE_GETFILE */








