/* os_termcap.c -- interface routines for termcap systems   */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                        */
/* This is part of the source for the (Mostly) Universal  */
/*       AGT Interpreter                                  */

/* This interface is not quite as careful as it should be; */
/* in particular, it doesn't check for changed appearance modes */
/* before moving the cursor around, which could possibly cause */
/* problems on some terminals. */

/* The internal line editor */
#define USE_EDITLINE

/* If you use this, you'll also need to link with the readline library.*/
/* I'm not certain it even still works; I haven't tested it lately */
/* #define USE_READLINE*/


#define RVID_BOX 1   /* Make box reverse video? */
#define DEBUG_KEY 0

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
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>

#ifdef LINUX
#include <linux/kd.h>
#undef HAVE_TPARAM
#endif

#ifdef BSD_TERM
#define tcgetattr(fd,data) ioctl(fd,TIOCGETA,data)
#define tcsetattr(fd,junk,data) ioctl(fd,TIOCSETAF,data)
/* junk needs to be equal to TCSAFLUSH */
#endif

/* Need one of the following */

#ifdef NO_TERMCAP_H
char *tgoto();
char *tparam();
int tputs();
extern char *BC, *UP;
extern int PC;
int tgetent();
int tgetnum();
char *tgetstr(); 
#else
#include <term.h>
#include <termcap.h>
#endif 

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>  
#endif

#ifdef TIOCGWINSZ   /* Both of these need to be defined. */
#ifdef SIGWINCH
#define CATCH_WINDOW_CHANGE
#endif
#endif

#ifdef LINUX_COLOR
/* 0:Black, 1:Red, 2:Green, 3:Brown, 4:Blue, 5:Magenta, 6:Cyan, 7:White */
/* 3n=foreground, 4n=background, semicolon seperated */
#define TS_LINUX_SL "\033[37;44m"
#define TS_LINUX_NORM "\033[m"
#define TS_LINUX_SETCOLOR "\033[3%dm"
/* The AGT color codes are as follows: */
/*  0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Brown, 7=White */
int color_trans[8]={0,4,2,6,1,5,3,7};
rbool color_on;
#endif

#define pFNT ".fnt"


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
 
static int control_width; 
static int scroll_count; /* Count of how many lines we have printed since
		     last player input. */
volatile int curr_y, winsize_flag, cont_flag;



/* NUM_KEYS must be less than 32 */
#define NUM_KEYS 23
#define FUN_KEY_BASE 11
#define FUN_KEY_END 22
#define LAST_FUN_KEY (FUN_KEY_END+10)
static char *term_keycap[NUM_KEYS]=
{"kb","kD",
 "kl","kr","ku","kd",
 "kh","@7",
 "kN","kP","kI",
 "k0","k1","k2","k3","k4","k5","k6","k7","k8","k9",
 "F1","F2"};
static char *term_key[NUM_KEYS];

static char *fkey_text[22]={ /* Text for function keys */
  "help\n",
  "get ","drop ",
  "examine ","read ",
  "open ","close ",
  "inventory\n","look\n",
  "score\n",
  "save\n","restore\n",
  "exit\n",
  "w\n","e\n","n\n","s\n",
  "nw\n","sw\n","se\n","ne\n",
  "enter\n"
};

static const char *keylist[12]={
  "del",
  "left","right","up","down",
  "home","end","pgdn","pgup",
  "ins"};


#if 0
  "undo\n",
  "get ","drop ",
  "unlock ","read ",
  "open ","close ",
  "inventory\n","look\n",  
  "wait\n",
  "save\n","restore\n"
#endif

/* Other candidates: view, wait (z), examine (x), again (g)   */    


/* These are used to hold termcap strings */
static char term_buff[3000];  
static char term_cmd_buff[3000];

static char *term_cmd[16];

#define TC_GOXY 0
#define TC_CLRSCR 1
#define TC_HIDE 2
#define TC_SHOW 3
#define TC_SL 4
#define TC_ENDSL 5
#define TC_BLINK 6
#define TC_NORM 7
#define TC_DELLINE 8
#define TC_SETSCROLL 9
#define TC_CR 10
#define TC_CLEARLINE 11
#define TC_BOLD 12
#define TC_NOBOLD 13
#define TC_INITKEY 14
#define TC_RESTOREKEY 15


/*---------------------------------------------------------------------*/
/*  Misc. minor functions                                              */
/*---------------------------------------------------------------------*/

void agt_delay(int n)
{
  if (fast_replay || BATCH_MODE) return;
  print_statline();
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
  if (script_on) textputs(scriptfile,s);
}


/*---------------------------------------------------------------------*/
/*  Low-level Terminal Functions: gotoxy(), etc.                       */
/*---------------------------------------------------------------------*/

void fix_loc(void)
/* This fixes the location if we are restoring from a suspended state */
{
#ifdef LOCAL_FIX_CONT
  char *cmd;
#endif

  if (cont_flag || winsize_flag) 
    agt_clrscr();

#ifdef LOCAL_FIX_CONT
    {
      cmd=tgoto(term_cmd[TC_GOXY],curr_x,curr_y);
      cont_flag=winsize_flag=0;
      tputs(cmd,1,putchar);  
    }
#endif
}

static void gotoxy(int x,int y)
{
  char *cmd;

  if (BATCH_MODE) return;
  cmd=tgoto(term_cmd[TC_GOXY],x,y);
  tputs(cmd,1,putchar);
}

static void cleareol(void)
{
  char *cmd;
  int i;

  if (term_cmd[TC_CLEARLINE]!=NULL)
    tputs(term_cmd[TC_CLEARLINE],1,putchar);
  else {
    for(i=0;i<screen_width-1-curr_x;i++)
      putchar(' ');
    cmd=tgoto(term_cmd[TC_GOXY],curr_x,curr_y);
    tputs(cmd,1,putchar);  
  }
}

static void clearline(void)
/* Clears current line and leaves cursor at beginning of it */
{
  char *cmd;

  if (term_cmd[TC_CR]!=NULL)
    tputs(term_cmd[TC_CR],1,putchar);
  else {
    cmd=tgoto(term_cmd[TC_GOXY],0,curr_y);
    tputs(cmd,1,putchar);  
  }
  curr_x=0;
  cleareol();
}


static int acc_ptr;
static char accum_text[81];  /* Used for prompt */

void set_new_line(void)
/* Does the math to update variables when we skip down a line */
{
  curr_x=0;
  curr_y++;
  if (curr_y>=screen_height)
    curr_y=screen_height-1; /* Scrolling has occured */
  scroll_count++;
  accum_text[0]=0;acc_ptr=0;
}


void add_accum_text(const char *s)
{
  while(*s!=0 && acc_ptr<80)
    accum_text[acc_ptr++]=*(s++);
  accum_text[acc_ptr]=0;
}



void set_scroll(int top)
{
#ifdef HAVE_TPARAM
  char buff[20];
  char *buff2;

  if (term_cmd[TC_SETSCROLL]!=NULL) {
    buff2=tparam(term_cmd[TC_SETSCROLL],buff,20,top,screen_height); 
    tputs(buff2,1,putchar);
    if (buff2!=buff) free(buff2);
  }
#endif
}


void show_cursor(rbool b)
/* 0=hide, 1=show */
{
  if (b==0 && term_cmd[TC_HIDE]!=NULL) 
    tputs(term_cmd[TC_HIDE],1,putchar);
  if (b==1 && term_cmd[TC_HIDE]!=NULL)
    tputs(term_cmd[TC_SHOW],1,putchar);
}



static struct termios termsave;

/* Put the terminal in noncanonical mode */
static void set_term_noncanon(void)
{
  struct termios newterm;
  fflush(stdin);
  tcgetattr(STDIN_FILENO,&termsave); /* Save terminal settings */
  tcgetattr(STDIN_FILENO,&newterm);
  newterm.c_lflag&=~ICANON;   /* Set noncanonical mode */
  newterm.c_lflag&=~ECHO;     /* ...with no echo */
  newterm.c_cc[VMIN]=1;       /* Read one character at a time */
  newterm.c_cc[VTIME]=0;      /* No time limit */
  tcsetattr(STDIN_FILENO,TCSAFLUSH,&newterm);
}

/* Restore terminal to previous state */
static void set_term_normal(void)
{
  tcsetattr(STDIN_FILENO,TCSAFLUSH,&termsave); /* Restore terminal */
}


#ifdef CATCH_WINDOW_CHANGE
void window_change(int signum)
{
  struct winsize winsz;

  signal(SIGWINCH,window_change);
  if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&winsz)!=0) return;
  if (winsz.ws_row==0 || winsz.ws_col==0) return;
  status_width=screen_width=winsz.ws_col;
  screen_height=winsz.ws_row;
  if (curr_x>screen_width) 
    {curr_x=screen_width;winsize_flag=1;}
  if (curr_y>screen_height) 
    {curr_y=screen_height;winsize_flag=1;}
}
#endif


static struct termios term_save;

void restore_terminal(void)
{
  show_cursor(1);
  set_scroll(0);
#ifdef BSD_TERM
  ioctl(STDIN_FILENO,TIOCSETA,&term_save);  
#else
  tcsetattr(STDIN_FILENO,TCSANOW,&term_save);  
#endif
}


/* SIGTSTP, SIGCONT */
void cont_handler(int signum)
{
  signal(SIGCONT,cont_handler);
  cont_flag=1;
}




/*---------------------------------------------------------------------*/
/*  Character Appearance Functions                                     */
/*---------------------------------------------------------------------*/

#define STATUS_STATE 0xA800
#define START_STATE 0x0800

/* For state:
     currently low 4 bits are color.
     bit 11 (0x0800) indicates normal color
     bit 13 (0x2000) indicates whether we are in standout mode or not
     bit 14 (0x4000) indicates whether we are blinking or not 
     bit 15 (0x8000) indicates whether bold is on or not */

#define NORM_BIT 0x0800
#define BOLD_BIT 0x8000
#define BLINK_BIT 0x4000
#define SL_BIT 0x2000

typedef unsigned short vstate;
static vstate state_stack[16];
int stateptr=0;  /* Points into state stack */

#define cstate state_stack[stateptr]  /* Current state */

static void set_state(vstate state)
{
  vstate diff;
  char sbuff[10];
  int c; /* Color */

  if (BATCH_MODE) return;

  diff=state^cstate;

  tputs(term_cmd[TC_NORM],1,putchar); /* First put the screen in known state */

  c=state & 0xF;
  if (state & NORM_BIT) c=7;

/*    if (term_cmd[TC_NOBOLD]!=NULL)
	tputs(term_cmd[TC_NOBOLD],1,putchar);*/
/*  tputs(term_cmd[TC_ENDSL],1,putchar);*/

  if (term_cmd[TC_BOLD]!=NULL) { 
    if (state & BOLD_BIT) { /* BOLD */
      tputs(term_cmd[TC_BOLD],1,putchar);
      if ((state&NORM_BIT) && !(state&SL_BIT)) c=6; 
         /* BOLD text will be yellow */
    }
  } else 
    if (diff & BOLD_BIT) {
      if (state & BOLD_BIT) writestr("<<");
      else writestr(">>");
    }
    
  if ((state & BLINK_BIT) && term_cmd[TC_BLINK]!=NULL)  /* Blinking */
    tputs(term_cmd[TC_BLINK],1,putchar);  

  if ((state & SL_BIT) && term_cmd[TC_SL]!=NULL)   /* Status line state */
    tputs(term_cmd[TC_SL],1,putchar);

#ifdef LINUX_COLOR
  if (color_on) {
    sprintf(sbuff,TS_LINUX_SETCOLOR,color_trans[c]);
    tputs(sbuff,1,putchar);
  }
#endif

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
  if (c==-1)   /* Bold on */
    nstate=nstate|0x8000;
  else if (c==-2) 
    nstate=nstate & ~0x8000; /* BOLD off */
  else if (c==8) 
    nstate=nstate | 0x4000;  /* BLINK on */
  else if (c==7)  
    nstate=0x0800;   /* "Normal" */
  else if (c==9)
    nstate=(nstate & ~0x80F) | 7; /* Set color to 7: white */
  else if (c>=0 && c<7)
    nstate=(nstate & ~0x80F) | c; /* Set color to c */
  set_state(nstate);
}


/*---------------------------------------------------------------------*/
/*  Functions to read and translate keys                               */
/*---------------------------------------------------------------------*/

#ifdef USE_EDITLINE

#define KEYBUFF_SIZE 64 /* Needs to be power of 2 */
static int keyptr=0; /* Where new characters go */
static int keyread=0;  /* Where old characters get lifted from */
static char keybuff[KEYBUFF_SIZE];

static void push_keychar(char c)
{
  keyread=(keyread+KEYBUFF_SIZE-1)&(KEYBUFF_SIZE-1);
  keybuff[keyread]=c;
}

static void key_backup(int n)
{
  assert(n>=0);
  keyread=(keyread+KEYBUFF_SIZE-n)&(KEYBUFF_SIZE-1);
}

static int get_next_char(rbool timed_input)
{
  char c;

  if (keyread!=keyptr) {
    c=keybuff[keyread];
  } else {
    if (timed_input) {
      read(STDIN_FILENO,&c,1); /* Actually read in the character */
    } else
      read(STDIN_FILENO,&c,1); /* Actually read in the character */
    if (DEBUG_KEY) printf("<%d>",c);
    keybuff[keyptr]=c;
    keyptr=(keyptr+1)&(KEYBUFF_SIZE-1);
  }
  keyread=(keyread+1)&(KEYBUFF_SIZE-1);
  return c;
}


int read_a_key(void)
{
  unsigned long keyflag;
  int c;
  int keymatch; /* Possible matched keys */
  int key_leng; /* Length of matched key */
  int i,j;
  rbool nomatch;

  scroll_count=0;
  keyflag=(1<<NUM_KEYS)-1;  /* All keys are possible */
  j=0;  /* Start with first character */
  keymatch=1000; /* No keys matched yet. */
  key_leng=0;

  do {
    c=get_next_char(j!=0);
    if (c==500) { /* Out of time */
      j++;
      break; 
    }
    for(i=0;i<NUM_KEYS;i++)
      if (keyflag&(1<<i)) { /* Still in consideration */
	if (term_key[i]!=NULL && term_key[i][j]!=c)
	  nomatch=1;
	else nomatch=0;	 
	if (nomatch && j==1 && term_key[i][0]=='\033') {
	    /* Check alternative VT-100 codes */
	    if ( (c=='O' || c=='[') && 
		 (term_key[i][1]=='O' || term_key[i][1]=='[') )
	      nomatch=0;
	  }
	if (term_key[i]==NULL || nomatch)
	  keyflag&=~(1<<i);
	else if (term_key[i][j+1]==0) {
	  keymatch=i;
	  key_leng=j+1;
	}
      }
    if (DEBUG_KEY) printf("[%lx]",keyflag);
    j++;
  } while (keyflag!=0 && keymatch==1000);
  if (key_leng==0) key_leng=1; /* Always skip at least one character */
  key_backup(j-key_leng);
  if (keymatch==1000) {
    key_backup(1);
    c=get_next_char(0);
    if (isprint(c)) return c;
    else if (c=='\n' || c=='\r') return '\n';
    else if (c=='\t') {
      for(i=0;i<4;i++)
	push_keychar(' ');
      return ' ';
    }
    else switch(c) 
      {
      case '\001': return 1006; /* Ctrl-A ==> Home */
      case '\002': return 1002; /* Ctrl-B ==> Left arrow */
      case '\005': return 1007; /* Ctrl-E ==> End */
      case '\006': return 1003; /* Ctrl-F ==> Right arrow */
      case '\010': /* Fall though... */
      case '\177': return 1000;/* Ctrl-H and <Del> ==> Backspace */
      case '\013': return 1500;  /* Ctrl-K */
      case '\014': return 1502;  /* Ctrl-L */
      case '\016': return 1005;  /* Ctrl-N ==> Down arrow */
      case '\020': return 1004;  /* Ctrl-P ==> Up arrow */
      case '\031': return 1501;  /* Ctrl-Y */
      default: return 2000;
      }
  }
  if (nav_arrow) {
    /* Translate keymatches to macro codes */
    if (keymatch>=1 && keymatch<=1010) 
      keymatch+=FUN_KEY_END-1;
  }
  return keymatch+1000;
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
  printf("%c",c);
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
  gotoxy(curr_x,curr_y);
}

static rbool save_accum_text=0;
static rbool old_yheight=0;

static void update_line(int cursor,char *buff)
{
  int yval, yheight, xpos, i;
  
  yheight=(base_x+strlen(buff)+screen_width-1)/screen_width; /* Round up */
  gotoxy(screen_width-1,screen_height-1);
  while (base_y+yheight>screen_height && base_y>1) {
    base_y--;
    save_accum_text=1;
    agt_newline();
    save_accum_text=0;
  }
  for(yval=yheight;yval<old_yheight;yval++) {
    gotoxy(0,base_y+yval);
    cleareol();
  }
  old_yheight=yheight;
  xpos=cursor-1;
  if (xpos<0) xpos=0;
  update_cursor(xpos);
  for(yval=curr_y-base_y;yval<yheight;yval++) {
    cleareol();
    for(i=curr_x;i<screen_width;i++) {
      if (buff[xpos]==0) break;
      putchar(buff[xpos++]);
    }
    curr_x=0;
    if (base_y+yval+1<screen_height) 
      gotoxy(curr_x,base_y+yval+1);
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

  set_term_noncanon();
  keyread=keyptr=0;  /* Clear key-buffer */

  for(exitflag=0;!exitflag;) {
    key=read_a_key();
    if (DEBUG_KEY) printf("{%d}",key);
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
      if (key=='\n') {
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
	  if (yank_text==NULL) 
	    txtleng=0;
	  else 
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
	agt_clrscr();
	base_y=curr_y;
	printf("%s",accum_text); /* Redraw prompt */
	if (editmode) 
	  redisplay_line(buff);
	break;

	/* Function key <key-1010> */
      default:
	if (key-1000<FUN_KEY_BASE || key-1000>LAST_FUN_KEY) 
	  break;
	
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
  set_term_normal();
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

#endif





/*---------------------------------------------------------------------*/
/*  High Level Input Functions                                         */
/*---------------------------------------------------------------------*/

char *agt_input(int in_type)  
/* read a line from the keyboard, allocating space for it using malloc */
/* See os_none.c for a discussion of in_type; we ignore it. */
{
#ifndef USE_EDITLINE
  static int input_error=0;
#endif
  char *s;

  print_statline();
  fix_loc();

#ifdef USE_READLINE   /* This doesn't quite work yet. */
  if (in_type==4)  /* Filename */
    rl_bind_key('\t',rl_complete);
  clearline();
  s=readline(accum_text);
  if (in_type==0)  /* Normal input */
    add_history(s);
  if (in_type==4)
    rl_bind_key('\t',rl_insert);
#elif defined(USE_EDITLINE)
  s=line_edit(0);
#else
  s=rmalloc(1001);
  if (fgets(s,1000,stdin)==NULL) {
    if (++input_error>10) {
      printf("Read error on input\n");
      exit(EXIT_FAILURE);
    }
    s[0]=0;
    clearerr(stdin);
  } else input_error=0; /* Reset counter */
#endif

#ifdef TEST_NONL
  {
    char *t;
    for(t=s+strlen(s)-1;(t>s) && (*t=='\n');t--);
    if (*t!=0 || t<s) t++;
    *t=0;
  }
#endif

  script_out(s);

#ifdef USE_EDITLINE
  agt_newline();
#else
  curr_x+=strlen(s);
  while(curr_x>=screen_width) {
    curr_x-=screen_width;
    set_new_line();
  } 
  set_new_line();
#endif

  scroll_count=0;
  return s;
}




char agt_getkey(rbool echo_char)
/* Reads a character and returns it */
/* If echo_char=1, echo character. If 0, don't */
{
  char c;
#ifdef USE_EDITLINE
  char *s;
#endif

  print_statline();
  scroll_count=0;

#ifdef USE_EDITLINE
  s=line_edit(echo_char?1:2);
  c=s[0];
  if (echo_char==1) {
    script_out(s);
    agt_newline();
  }
  rfree(s);
#else
  set_term_noncanon();
  read(STDIN_FILENO,&c,1); /* Actually read in the character */
  set_term_normal();
  if (echo_char) {
    char cstr[2];

    cstr[0]=c;cstr[1]=0;
    agt_puts(cstr);
    agt_newline();
  }
#endif

  scroll_count=0;
  if (!save_accum_text) {
    acc_ptr=0;accum_text[0]=0;
  }
  return c;
}




/*---------------------------------------------------------------------*/
/*  Printing the Status Line                                           */
/*---------------------------------------------------------------------*/

static void print_compass(void)
{
  int cwidth, i;

  if (status_width< 9+4*12) return;

  printf("\n  EXITS: ");cwidth=9;
  for(i=0;i<12;i++)
    if (compass_rose & (1<<i)) {
      printf("%s ",exitname[i]);
      cwidth+=strlen(exitname[i])+1;
    }
  for(i=cwidth;i<status_width;i++) putchar(' ');
}

void agt_statline(const char *s)
/* Output a string in reverse video (or whatever) at top of screen */
{
  if (BATCH_MODE) return;

  show_cursor(0);
  set_scroll(0);
  gotoxy(0,0);

  push_state(STATUS_STATE);
  printf("%s",s);
  if (have_compass) 
    print_compass();
  pop_state();

  set_scroll(status_height);
  winsize_flag=0;
  gotoxy(curr_x,curr_y);
  putchar(' '); /* To supress spurious bold square */
  gotoxy(curr_x,curr_y);
  show_cursor(1);
}



/*---------------------------------------------------------------------*/
/*  High Level Output Functions                                        */
/*---------------------------------------------------------------------*/

void agt_more(void)
{
   if (BATCH_MODE) return;  
   printf("  --MORE--");
   agt_waitkey();
   clearline();
   scroll_count=0;
}


void agt_newline(void)  
{
  if (!save_accum_text) {
    script_out("\n");
    set_new_line();
  }

  if (BATCH_MODE) return;

  if (curr_y!=screen_height ||    /* No need to scroll */
	   term_cmd[TC_SETSCROLL]!=NULL || /*   ..or automatic scrolling */
	   term_cmd[TC_DELLINE]==NULL)    /* ... or nothing we can do */
    printf("\n");
  else {
    show_cursor(0);
    gotoxy(0,status_height);
    tputs(term_cmd[TC_DELLINE],screen_height-1,putchar);
    gotoxy(0,screen_height-1);
    show_cursor(1);
  }
  if (!save_accum_text) {
    if (scroll_count>=screen_height-(have_compass? 3 : 2) ) 
      agt_more();  /* Print out MORE message */ 
    fix_loc();  /* Fixup if returning from being suspended */
  }
}


void agt_puts(const char *s)
/* This routine assumes that s is not wider than the screen */
{
  script_out(s);
  curr_x+=strlen(s);
  add_accum_text(s);

  if (BATCH_MODE) return;

  fix_loc();  /* Fixup if returning from being suspended */
  printf("%s",s);
}


void agt_clrscr(void)
{
  cont_flag=0;winsize_flag=0;
  scroll_count=0;
  curr_x=0;
  if (have_compass) curr_y=2;
  else curr_y=1;
  script_out("\n\n\n\n\n");

  if (BATCH_MODE) return;

  tputs(term_cmd[TC_CLRSCR],screen_height,putchar);
  printf("\n"); /* To make space for status line */
  if (have_compass) printf("\n"); 
}




/*---------------------------------------------------------------------*/
/*  Box Printing Routines                                              */
/*---------------------------------------------------------------------*/

static unsigned long boxflags;
static int box_startx; /* Starting x of box */
static int box_width;
static int delta_scroll; /* Amount we are adding to vertical scroll 
			    with the box */

static void boxrule(void)
/* Draw line at top or bottom of box */
{ 
  int i;
  
  if (RVID_BOX) agt_puts("  "); 
  else agt_puts("+-");

  for(i=0;i<box_width;i++)
    if (RVID_BOX) agt_puts(" "); 
    else agt_puts("-");

  if (RVID_BOX) agt_puts("  "); 
  else agt_puts("-+");
}

static void boxpos(void)
{
  int i;

  curr_x=box_startx; curr_y++;
  if (curr_y>=screen_height) curr_y=screen_height-1;
  gotoxy(curr_x,curr_y);

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
    if (curr_y+height+8>screen_height) /* Make room for credits */
      curr_y=screen_height-height-8;
    if (curr_y<0) curr_y=0;
    delta_scroll=0;
    scroll_count=0;
  } else {  /* Compute vertical position of non-title box */
    curr_y=curr_y-height;
    if (curr_y<1) {
      delta_scroll=1-curr_y;
      curr_y=1;
    } else delta_scroll=0;
  }

  curr_x=box_startx;
  gotoxy(curr_x,curr_y);

  script_out("\n");
  for(i=0;i<box_startx;i++)
    script_out(" ");

  if (boxflags&TB_BORDER) {
    push_state(cstate | 0x2000); /* Turn on reverse video */
    boxrule();
    boxpos();
    if (RVID_BOX) agt_puts("  ");
    else agt_puts("| ");
  }
}

void agt_qnewline(void)
{
  if (boxflags&TB_BORDER) {
    if (RVID_BOX) agt_puts("  ");
    else agt_puts(" |");
  }
  boxpos();
  if (boxflags&TB_BORDER) {
    if (RVID_BOX) agt_puts("  ");
    else agt_puts("| ");
  }
}

void agt_endbox(void)
{
  if (boxflags&TB_BORDER) {
    if (RVID_BOX) agt_puts("  ");
    else agt_puts(" |");
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


#ifdef LINUX_COLOR
/* It would be good to make this list user-customizable */
int NUM_COLOR_TERM=0;
char **color_term=NULL;

void add_colorterm(char *term_name)
{
  NUM_COLOR_TERM++;
  color_term=rrealloc(color_term,NUM_COLOR_TERM*sizeof(char*));
  color_term[NUM_COLOR_TERM-1]=rstrdup(term_name);
}

#endif



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
	n=i+12;
      }
  } else {   /* Name of form 'Fnn' */
    n=strtol(keyname+1,&s,0); /* s will point to the first erroneous char */
    if (keyname[1]==0 || *s!=0) err=1;
    if (n>12) err=1;
    if (n==10) n=0;
    if (n>10) n--;
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

#ifdef LINUX_COLOR 
  if (opt("COLORTERM") && optnum>=2 && setflag) {
    for(i=1;i<optnum;i++)
      add_colorterm(optstr[i]);
    return 1;
  }
#endif
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

#ifdef LINUX_COLOR

/* Return true if tty supports colors */
/* It also frees the color_term array, so it can only be called once */
rbool color_tty(char *termtype)
{
  int i;
  rbool ctty;

  ctty=0;
  for(i=0;i<NUM_COLOR_TERM;i++) {
    if (strcmp(termtype,color_term[i])==0) ctty=1;
    rfree(color_term[i]);
  }
  rfree(color_term);
  return ctty;
}

#endif /* LINUX_COLOR */



static char *tcptr;
static char *termtype;

void init_interface(int argc,char *argv[])
{
  int i;
  struct stat fileinfo;
  ino_t inode;
  dev_t devnum;

  viewer_name=NULL;
  have_compass=0; status_height=1;
  script_on=0;scriptfile=BAD_TEXTFILE;
  center_on=0;
  par_fill_on=0;
  scroll_count=0;
  nav_arrow=0;

  font_enable=0;
  
  acc_ptr=0;  accum_text[0]=0;

  if (BATCH_MODE) {
    screen_height=25;
    screen_width=status_width=80;
    return;
  }

  get_agt_path();

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

  if (!isatty(STDIN_FILENO)) {
      printf("Sorry, this program requires standard input to be a tty.");
      exit(0);
    }
  tcgetattr(STDIN_FILENO,&term_save);  

#ifndef NEXT /* ?!?! */
  if (atexit(restore_terminal)!=0) 
    {printf("INTERNAL ERROR: Unable to register restore_terminal.\n");
     exit(0);}
#endif

#ifndef NO_TERMCAP_H
  ospeed=cfgetospeed(&term_save);
#endif

  termtype=getenv("TERM");
  if (termtype==NULL) {
    printf("ERROR: No terminal type defined.");
    exit(0);}
  switch(tgetent(term_buff,termtype)) {
     case -1:printf("ERROR: Unable to access TERMCAP database.\n");
       exit(0);
     case 0:printf("ERROR: Unknown terminal type.\n");
       exit(0);
     }

  screen_height=screen_width=0;
#ifdef CATCH_WINDOW_CHANGE
  window_change(SIGWINCH);
#endif

  if (screen_width==0) screen_width=tgetnum("co");
  if (screen_width==0) screen_width=80;
  if (screen_height==0) screen_height=tgetnum("li");
  if (screen_height==0) screen_height=24;

  tcptr=term_cmd_buff;
  BC=tgetstr("le",&tcptr);
  UP=tgetstr("up",&tcptr);
  PC=tgetnum("pc");

/* First, the essentials */
  term_cmd[TC_GOXY]=tgetstr("cm",&tcptr);
  term_cmd[TC_CLRSCR]=tgetstr("cl",&tcptr);
  if (term_cmd[TC_GOXY]==NULL || term_cmd[TC_CLRSCR]==NULL) {
    printf("You terminal is missing features required by this program.");
    exit(0);
  }

/* Now a bunch of optional features */
  term_cmd[TC_CR]=tgetstr("cr",&tcptr);
  term_cmd[TC_CLEARLINE]=tgetstr("ce",&tcptr);
  term_cmd[TC_HIDE]=tgetstr("vi",&tcptr);
  term_cmd[TC_SHOW]=tgetstr("ve",&tcptr);
  term_cmd[TC_BLINK]=tgetstr("mb",&tcptr);
  term_cmd[TC_NORM]=tgetstr("me",&tcptr);
  term_cmd[TC_DELLINE]=tgetstr("dl",&tcptr);
#ifdef HAVE_TPARAM
  term_cmd[TC_SETSCROLL]=tgetstr("cs",&tcptr);
#else
  term_cmd[TC_SETSCROLL]=NULL;
#endif
  term_cmd[TC_BOLD]=tgetstr("md",&tcptr);
  term_cmd[TC_NOBOLD]=tgetstr("me",&tcptr);
  term_cmd[TC_INITKEY]=tgetstr("ks",&tcptr);
  term_cmd[TC_RESTOREKEY]=tgetstr("ke",&tcptr);

  /* And key codes */
  for(i=0;i<NUM_KEYS;i++)
    term_key[i]=tgetstr(term_keycap[i],&tcptr);  

  if (term_key[10]==NULL) /* If at first you don't succeed... */
    term_key[10]=tgetstr("k;",&tcptr);


  control_width=tgetnum("sg"); /* Width of control markers */
  if (control_width<0) control_width=0;
  status_width=screen_width-2*control_width;
  signal(SIGCONT,cont_handler);
  signal(SIGPIPE,SIG_IGN);

#ifdef USE_READLINE
  rl_bind_key('\t',rl_insert);  /* Turn off tab-completion */
  using_history();
#endif
}

#ifdef REPLACE_BNW
static void save_old_font(void);
static void restore_font(void);
#endif

void start_interface(fc_type fc)
{
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

#ifdef LINUX_COLOR
  color_on=0;
  add_colorterm("linux");
  add_colorterm("xterm");
  if (color_tty(termtype)) 
    {
      strcpy(tcptr,TS_LINUX_SL);
      term_cmd[TC_SL]=tcptr;
      tcptr+=strlen(TS_LINUX_SL)+1;
      strcpy(tcptr,TS_LINUX_NORM);
      term_cmd[TC_ENDSL]=tcptr;
      tcptr+=strlen(TS_LINUX_NORM)+1;
      tputs(term_cmd[TC_ENDSL],1,putchar);
      color_on=1;
    /* Switch to PC character set */
      if (!fix_ascii_flag) printf("\033(U");
    }
  else 
#endif
    {
      term_cmd[TC_SL]=tgetstr("mr",&tcptr); /* Status line attributes */
      if (term_cmd[TC_SL]!=NULL)
	term_cmd[TC_ENDSL]=term_cmd[TC_NORM];
      else {  /* Reverse video unavailable; try standout */
	term_cmd[TC_SL]=tgetstr("so",&tcptr);
	if (term_cmd[TC_SL]!=NULL)
	  term_cmd[TC_ENDSL]=tgetstr("se",&tcptr);
	else {  /* That didn't work either... we'll just have to use vanilla
		   text */
	  term_cmd[TC_SL]=term_cmd[TC_ENDSL]="";
	}
      }
    }
  set_state(START_STATE);
  tputs(term_cmd[TC_INITKEY],1,putchar);  
  agt_clrscr();
}


void close_interface(void)
{
  rfree(viewer_name);
  set_state(START_STATE);
  tputs(term_cmd[TC_RESTOREKEY],1,putchar);
#ifdef LINUX_COLOR
  if (!fix_ascii_flag) printf("\033c"); /* Reset console. */
#endif 
  if (filevalid(scriptfile,fSCR))
    close_pfile(scriptfile,0);
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

    fontfile=linopen(fontname,pFNT);
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

  if (!BATCH_MODE)
    system(cmdstr); /* Don't bother to check return status since
		       there's nothing we could do anyhow */

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
