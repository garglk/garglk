/* os_msdos.c -- interface routines for MS-DOS            */
/* Copyright (C) 1996,1997,1998    Robert Masenten        */
/*                                                        */
/* This is part of the source for the (Mostly) Universal  */
/*       AGT Interpreter                                  */

/* This has been written to work under Borland C (MSDOS16)
   and DJGPP (MSDOS32); it hasn't been tested with other compilers. */

/* NOTE: The library functions for screen location put the origin
   at (1,1), but curr_x, curr_y, box_startx, and box_starty have
   origin (0,0). */



/* What needs work for 32-bit case:
   set_font()
*/

/* What's done:
   rgetchar()
   draw_scanline()
   pictcmd():       Save/restore of text mode video memory
   setup_card():    Setting video-mode palette
*/

#define USE_EDITLINE
#define DEBUG_KEY 0  /* Turns on debugging of key functions */

#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#include "agility.h"
#include "interp.h"

#include <conio.h>

#ifdef __DJGPP__
#include <bios.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#define outp(p,d) outportb(p,d)
#endif

#define cleareol rclreol


rbool use_bios;  /* If true, use the BIOS routines for text output.
		   (for the sake of blind users.) */

static int colorset[5]={7,14,0,15,1};


/* cs_ stands for "color scheme" */
enum {cs_normal=0, cs_bold, cs_back, cs_statfore, cs_statback} color_scheme;
/* normal/highlight/background/statfore/statback */

int scroll_count; /* Count of how many lines we have printed since
		     last player input. */
int status_height;
rbool have_compass;
int top_row; /* The absolute y value of the current top row. */

static char *gamefile_name;
static rbool font_enable;
static rbool nav_arrow;
static rbool block_cursor;
static rbool tall_screen; /* 43/50 line mode? */

static char *active_font; /* Currently active font; NULL if none */
static int active_font_height;

typedef enum {MDA,CGA,MCGA,EGA64,EGA,VGA} gh_type;
static gh_type graph_hardware;


static char *fkey_text[24]={ /* Text for function keys */
  "help\n",
  "get ","drop ",
  "examine ","read ",
  "open ","close ",
  "inventory\n","look\n",
  "score\n",
  "save\n","restore\n",
  "exit\n",
  "w\n","e\n","n\n","s\n",
  "nw\n","sw\n","ne\n","se\n",
  "enter\n","up\n","down\n"
};


#if 0
{
  "undo\n",
  "get ","drop ",
  "read ","unlock ",
  "open ","close ",
  "inventory\n","look\n",  
  "wait\n",
  "save\n","restore\n"};
#endif

/* Other candidates:
  view
  help
  examine (x) 
  again (g)   */    


#ifdef __DJGPP__
/*------------------------------------------------------------------*/
/*  32-bit library Compatibility Layer  */
/*------------------------------------------------------------------*/

/* Internal coordinates have an origin of (0,0), even though the
   calling coordinates are (1,1). */

#define rgettextinfo gettextinfo
#define r_setcursortype _setcursortype

static int winx1,winy1,winx2,winy2,screen_attr;
#if 0
static int winx1=0,winy1=0,winx2=79,winy2=24,screen_attr=7;
static int winx1=1,winy1=1,winx2=80,winy2=25,screen_attr=7;
#endif

/* This has an origin of (0,0) */
void r0gotoxy(int x, int y)
{
  union REGS r;
  r.h.ah=0x02;
  r.h.dh=y;
  r.h.dl=x;
  r.h.bh=0; /* Page 0 */
  int86(0x10,&r,&r);
}


void get_cur_pos(int *px, int *py)
{
  union REGS r;  
  r.h.ah=0x03; 
  r.h.bh=0;  /* Page 0 */
  int86(0x10,&r,&r);
  *px=r.h.dl;
  *py=r.h.dh;
}

int rtextmode(int mode)
{
  union REGS r;
  struct text_info term_data;    
  int height;

  textmode(mode);
  gettextinfo(&term_data);
  height=term_data.screenheight;  /* Assume PC dimensions */

  winx1=0; winy1=0; winx2=79; winy2=height-1;
  screen_attr=7;
  if (!directvideo) {
    r.h.ah=5;
    r.h.al=0;
    int86(0x10,&r,&r);  /* Set active page to 0 */
    r0gotoxy(0,0);
  }
  return height;
}


void rtextattr(int attr)
{
  screen_attr=attr;
  if (directvideo) textattr(attr);
}

void rwindow(int x1, int y1, int x2, int y2)
{
  winx1=x1-1; winy1=y1-1; winx2=x2-1; winy2=y2-1;
  if (directvideo) window(x1,y1,x2,y2);
}

void rclrscr(void)
{
  union REGS r;
  if (directvideo) {clrscr();return;}
  r.h.ah=0x06; /* Scroll up */
  r.h.al=0x00; /* Blank window */
  r.h.ch=winy1; r.h.cl=winx1;
  r.h.dh=winy2; r.h.dl=winx2;
  r.h.bh=screen_attr;
  int86(0x10,&r,&r);
  r0gotoxy(winx1,winy1);
}

void rclreol(void)
{
  union REGS r;
  int x,y;

  if (directvideo) {clreol();return;}
  get_cur_pos(&x,&y);
  r.h.ah=0x06; /* Scroll up */
  r.h.al=0x00; /* Blank window */
  r.h.ch=y; r.h.cl=x;
  r.h.dh=y; r.h.dl=winx2;
  r.h.bh=screen_attr;
  int86(0x10,&r,&r);
}

void rinsline(void)
{
  union REGS r;
  int x,y;  
  if (directvideo) {insline();return;}
  get_cur_pos(&x,&y);
  r.h.ah=0x07; /* Scroll down */
  r.h.al=0x01;  /* One line */
  r.h.ch=y; r.h.cl=winx1;
  r.h.dh=winy2; r.h.dl=winx2;
  r.h.bh=screen_attr;
  int86(0x10,&r,&r);
}

void rnewline(int *px,int *py)
{
  union REGS r;
  *px=winx1;
  if (*py<winy2) {
    (*py)++;
  } else { /* Scroll */
    r.h.ah=0x06; /* Scroll up */
    r.h.al=0x01;  /* One line */
    r.h.ch=winy1; r.h.cl=winx1;
    r.h.dh=winy2; r.h.dl=winx2;
    r.h.bh=screen_attr;
    int86(0x10,&r,&r);      
  }
}


int rfastputch(char c,int *px, int *py)
{
  union REGS r;
  if (c=='\r') {
    return 0;
  }
  if (c=='\n') {
    rnewline(px,py);
    r0gotoxy(*px,*py);
    return 0;
  }
  r.h.ah=0x09; /* Print character w/attribute */
  r.h.al=c;
  r.h.bl=screen_attr;
  r.h.bh=0;
  r.x.cx=1;
  int86(0x10,&r,&r); /* Doesn't change cursor position */
  (*px)++; 
  if ((*px)>winx2)
    rnewline(px,py);
  r0gotoxy(*px,*py);
  return 0;
}


int rputch(char c)
{
  int x,y;
  if (directvideo) return putch(c);
  get_cur_pos(&x,&y);
  rfastputch(c,&x,&y);
  return 0;
}


int rputs(const char *s)
{
  int x,y;
  if (directvideo) return cputs(s);
  get_cur_pos(&x,&y);
  for(;*s!=0;s++)
    rfastputch(*s,&x,&y);
  return 0;
}


int rgetch(void)
{
  union REGS r;
  if (directvideo) return getch();
  r.h.ah=0x00;
  int86(0x16,&r,&r);
  return r.h.al;
}

int rgetche(void)
{
  int c;
  if (directvideo) return getche();
  c=rgetch();
  rputch(c&0xFF);
  return c;
}

void rgotoxy(int x,int y)
{
  if (directvideo) {gotoxy(x,y);return;}
  r0gotoxy(x-1+winx1,y-1+winy1);
}

int rwherex(void)
{
  int x,y;
  if (directvideo) return wherex();
  get_cur_pos(&x,&y);
  return x+1-winx1;
}

int rwherey(void)
{
  int x,y;
  if (directvideo) return wherey();
  get_cur_pos(&x,&y);
  return y+1-winy1;
}



#else /* ...if not __DJGPP__ */

#define rtextattr textattr
#define rwindow window
#define rgettextinfo gettextinfo
#define r_setcursortype _setcursortype
#define rclrscr clrscr
#define rclreol clreol
#define rinsline insline
#define rgotoxy gotoxy
#define rwherex wherex
#define rwherey wherey
#define rputs cputs
#define rputch putch
#define rgetche getche
#define rgetch getch

int rtextmode(int mode)
{
  struct text_info term_data;    

  textmode(mode);
  gettextinfo(&term_data);
  return term_data.screenheight;  /* Assume PC dimensions */
}



#endif 


/*------------------------------------------------------------------*/
/*   Text Style                                                     */
/*------------------------------------------------------------------*/

#define STATUS_STATE 0x3800
#define BOX_STATE 0x2800
#define START_STATE 0x0800

/* For state:
     currently low 4 bits are color.
     bit 11 (0x0800) indicates we should use "normal" color 
     bit 12 (0x1000) indicates we are printing the status line, so
                       supress blinking
     bit 13 (0x2000) indicates whether we are in reverse video mode or not
     bit 14 (0x4000) indicates whether we are blinking or not 
     bit 15 (0x8000) indicates whether bold is on or not 
     */

#define BOLD_BIT 0x8000
#define BLINK_BIT 0x4000
#define RVID_BIT 0x2000
#define STAT_BIT 0x1000
#define NORM_BIT 0x0800

typedef unsigned short vstate;
static vstate state_stack[16];
int stateptr=0;  /* Points into state stack */

#define cstate state_stack[stateptr]  /* Current state */

static void set_state(vstate state)
{
  int c; /* Color */
  int bkgd; /* Background and blink bits */

  /* 0=normal/1=highlight/2=background/3=statfore/4=statback */

  if (state & NORM_BIT)
    c=colorset[cs_normal];
  else
    c=state & 0xF;  /* Extract color */

  if (state & RVID_BIT) {  /* Reverse video */
    bkgd=colorset[cs_statback];
    if (state & NORM_BIT)
      c=colorset[cs_statfore];
  } else 
    bkgd=colorset[cs_back];

  if (state & BOLD_BIT)
    if (state & NORM_BIT)  
      c=colorset[cs_bold];
    else c^=0x08; /* Toggle bold bit */

  if ( (state & BLINK_BIT) && !(state & STAT_BIT) ) 
    bkgd|=0x08; /* Blinking */

  bkgd=(bkgd<<4)|c;
  rtextattr(bkgd);
  cstate=state;
}

static void reset_state(void)
{
  set_state(cstate);
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
/*  7=White("normal"), 8=Blinking.  */
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
    nstate=START_STATE;   /* "Normal" */
  else if (c==9)
    nstate=(nstate & ~0x080F) | 7; /* Set color to 7: white */
  else if (c>=0 && c<7)
    nstate=(nstate & ~0x080F) | c; /* Set color to c */
  set_state(nstate);
}



/*------------------------------------------------------------------*/
/*  Misc. Functions                                                 */
/*------------------------------------------------------------------*/


void agt_delay(int n)
{
  if (BATCH_MODE) return;
  print_statline();
  sleep(n);
}


void agt_tone(int hz,int ms)
/* Produce a hz-Hertz sound for ms milliseconds */
{
  if (!sound_on) return;
  sound(hz);
  delay(ms);
  nosound();
}

int agt_rand(int a,int b)
/* Return random number from a to b inclusive */
{
    return a+(rand()>>2)%(b-a+1);
}



/*------------------------------------------------------------------*/
/*   Key Tables                                                     */
/*------------------------------------------------------------------*/


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
   1010= ins
   1011= gray plus
   1012= gray minus
   10mm= Function key F<n> with mm=n+13 (F0==F10)
   1500= ^K
   1501= ^Y
   1502= ^L
   2000= unknown control character
*/

#define KEY_CNT 25
#define FKEY_BASE 13
#define FKEY_CNT  (12+8+4)

static char keytrans[KEY_CNT]={
   0x00,0x53,  /* Delete and backspace */
   0x4b,0x4d,0x48,0x50, /* Arrow keys */
   0x47,0x4F,  /* Home and End */
   0x49,0x51, /* PgUp, PgDown */
   0x52,0x4E,0x4A, /* Ins, Plus, Minus */
   0x44,  /* F10 */
   0x3b,0x3c,0x3d,0x3e,0x3f, /* F1-F5 */
   0x40,0x41,0x42,0x43,  /* F6-F9 */
   0x57,0x58}; /* F11-F12 */


static const char *keylist[12]={
  "del","left","right","up","down","home","end","pgup","pgdn",
  "ins", "plus", "minus"};





/*------------------------------------------------------------------*/
/*   Line Editor                                                    */
/*------------------------------------------------------------------*/

static int saved_tab=0;

unsigned xgetch(void)
{
  union REGS r;

  r.x.ax=0;
  int86(0x16,&r,&r);
  return r.x.ax;
}


int read_a_key(void)
{
  unsigned xc;
  char c;
  int i;

  scroll_count=0;
  if (saved_tab>0) {
    saved_tab--;
    return ' ';
  }
  xc=xgetch();

  if (xc==0x4e2b) xc=0x4e00; /* Gray plus */
  if (xc==0x4a2d) xc=0x4a00; /* Gray minus */
  c=xc&0xFF;
  if ( c!=0 )
    if (isprint(c)) return c;
    else if (c=='\n' || c=='\r') return '\n';
    else if (c=='\t') {
      saved_tab=4;
      return ' ';
    }
    else if (c=='\001')  /* Ctrl-A ==> Home */
      return 1006;
    else if (c=='\005')  /* Ctrl-E ==> End */
      return 1007;
    else if (c=='\010')  /* Ctrl-H ==> Backspace */
      return 1000;
    else if (c=='\013')  /* Ctrl-K */
      return 1500;
    else if (c=='\031') /* Ctrl-Y */
      return 1501;
    else if (c=='\014')  /* Ctrl-L */
      return 1502;
    else return 2000;

  /* Get extended code */
  c=(xc>>8)&0xFF;
  for(i=0;i<KEY_CNT;i++) {
    if (c==keytrans[i]) 
      if (!nav_arrow || i==0 || i>=FKEY_BASE)
	return 1000+i;
      else return 1000+FKEY_BASE+12+i-1;
  }
  return 2000;
}




#ifdef USE_EDITLINE

void agt_putc(char c)
{
#ifdef UNIX
  fix_loc();  /* Fixup if returning from being suspended */
#endif
  printf("%c",c);
  curr_x+=1;
}

static rbool old_yheight=0;
static int base_x,base_y; /* Base X value of first character of input */

/* This assumes the cursor is always positioned on the string
   being edited */
static void update_cursor(int cursor)
{
  int curr_y;

  curr_x=base_x+cursor;
  curr_y=base_y;
  while (curr_x>=screen_width) {
    curr_x-=screen_width;
    curr_y++;
  }
  rgotoxy(curr_x+1,curr_y+1);
}

/* static rbool save_accum_text=0;*/

static void update_line(int cursor,char *buff)
{
  int yval, yheight, xpos, i, curr_y;
  
  yheight=(base_x+strlen(buff)+screen_width-1)/screen_width; /* Round up */
  /* printf("<%d+%d>",base_y,yheight);*/
  rgotoxy(screen_width,screen_height);
  while (base_y+yheight+status_height>screen_height && base_y>1) {
    base_y--;
    /*    save_accum_text=1;*/ 
    rputs("\r\n");
    /* save_accum_text=0;*/
  }
  for(yval=yheight;yval<old_yheight;yval++) {
    rgotoxy(1,base_y+yval+1);
    cleareol();
  }
  old_yheight=yheight;
  xpos=cursor-1;
  if (xpos<0) xpos=0;
  update_cursor(xpos);
  curr_x=rwherex()-1;
  curr_y=rwherey()-1;
  for(yval=curr_y-base_y;yval<yheight;yval++) {
    cleareol();
    for(i=curr_x;i<screen_width;i++) {
      if (buff[xpos]==0) break;
      rputch(buff[xpos++]);
    }
    curr_x=0;
    if (base_y+yval+status_height<screen_height) 
      rgotoxy(curr_x+1,base_y+yval+2);
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
static long histmax=10; 

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
  savebuff=NULL;
  curr_hist=0;
  buff[0]=buff[1]=0;
  cursor=buffleng=0;buffspace=2;
  base_x=rwherex()-1; base_y=rwherey()-1;
  editmode=(state==0);
  saved_tab=0;
  old_yheight=0;
  scroll_count=0;
  exitflag=0;

#ifdef UNIX
  set_term_noncanon();
  keyread=keyptr=0;  /* Clear key-buffer */
#endif

  for(;;) {
    key=read_a_key();
    if (DEBUG_KEY) printf("{%d}",key);
    if (key<1000) { /* Real character entered */
      if (state>0) { /* One key */
	if (state==1) {
	  agt_putc(key);
	  agt_newline();
	}
	buff[0]=key; buff[1]=0;
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
#ifdef UNIX
	agt_clrscr();
	base_y=curr_y;
	printf("%s",accum_text); /* Redraw prompt */
	if (editmode) 
	  redisplay_line(buff);
#endif
	break;

      default:
	if (key<1000+FKEY_BASE || key>=1000+FKEY_BASE+FKEY_CNT)
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
	  txtleng=strlen(fkey_text[key-1000-FKEY_BASE]);
	  if (fkey_text[key-1000-FKEY_BASE][txtleng-1]=='\n') {
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
	    buff[cursor+i]=fkey_text[key-1000-FKEY_BASE][i];	  
	  update_line(cursor,buff);
	  cursor+=txtleng;
	  update_cursor(cursor);
	}
	break;
      }

    if (exitflag) break;
  }
#ifdef UNIX
  set_term_normal();
#endif
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






/*------------------------------------------------------------------*/
/*   Basic Input and Output Code                                    */
/*------------------------------------------------------------------*/


static void script_out(const char *s)
{
  if (DEBUG_OUT) 
    fprintf(debugfile,"%s",s);
  if (script_on) fprintf(scriptfile,"%s",s);
}



int tmpcnt=0;

char *agt_input(int in_type)
/* read a line from the keyboard, allocating space for it using malloc */
/* in_type: 0=command, 1=number, 2=question, 3=userstr, 4=filename,*/
/* 5=RESTART,RESTORE,UNDO,QUIT */
/*  Negative values are for internal use by the interface (i.e. this module) */
/*  We completely ignore the value of in_type */
{
#ifndef USE_EDITLINE  
  static int input_error=0;
#endif
  char *s;

  scroll_count=0;
  print_statline();

#ifdef USE_EDITLINE
  s=line_edit(0);
#else
  s=rmalloc(300);
  s[0]=297;
  s[1]=0;
  if (cgets(s)==NULL) { /* I have no idea if cgets _ever_ returns NULL */
    if (++input_error>10) {
      printf("Console read failure.\n");
      exit(1);
    }
  } input_error=0;

  s=rrealloc(s,s[1]+3);  
  memmove(s,s+2,s[1]+1);
  printf("\n");
#endif

  printf("\n");
  script_out(s);script_out("\n");
  curr_x=rwherex()-1;
  return s;
}

char agt_getkey(rbool echo_char)
/* Reads a character and returns it, possibly reading in a full line
  depending on the platform */
/* If echo_char=1, echo character. If 0, then the character is not
     required to be echoed (and ideally shouldn't be) */
{
  char c;
  char s[2];

  scroll_count=0;
  print_statline();

  if (echo_char) 
    c=rgetche();    /* Get with echo */
  else c=rgetch();   /* Get w/o echo */
  if (c==0) rgetch(); /* Throw away extended character */
  if (echo_char) {
    s[0]=c;s[1]=0;
    script_out(s);
    agt_newline();
  }
  curr_x=rwherex()-1;
  return c;
}


static void print_compass(void)
{
  int cwidth, i;

  if (status_width< 9+4*12) return;
  rgotoxy(1,2);
  rputs("  EXITS: ");cwidth=9;
  for(i=0;i<12;i++)
    if (compass_rose & (1<<i)) {
      rputs(exitname[i]);
      rputs(" ");
      cwidth+=strlen(exitname[i])+1;
    }
  for(i=cwidth;i<status_width;i++) rputs(" ");
}


static rbool is_fullscreen; /* True until we print the first status line */

void agt_statline(const char *s)
/* Output a string at location (x,y), with attributes attr */
/* 0=normal, 1=background(e.g. status line) */
{
   int savex, savey;

   savex=rwherex();savey=rwherey()+top_row;  /* Save old postion */
   if (is_fullscreen) { /* Need to move text down a bit */
        int i;
        rgotoxy(1,1);
        for(i=0;i<status_height;i++)
          rinsline();
   }
   rwindow(1,1,screen_width,screen_height);  /* Set window to whole screen */
   rgotoxy(1,1);
   push_state(STATUS_STATE);
   rputs(s);  /* Console put string */
   if (have_compass) {
     status_height=2;
     top_row=3;
     print_compass();
   } else {
     top_row=2;
     status_height=1;
   }
   pop_state();
   rwindow(1,status_height+1,screen_width,screen_height); is_fullscreen=0;
   /* Exclude status line from window again */
   rgotoxy(savex,savey-top_row); /* Return to old postion */
   return;
}


void agt_clrscr(void)
/* This should clear the screen and put the cursor at the upper left
  corner (or one down if there is a status line) */
{
  rclrscr();
  curr_x=0;
  if (DEBUG_OUT) fprintf(stderr,"\n\n<RCLRSCR>\n\n");
  if (script_on) fprintf(scriptfile,"\n\n\n\n");
  scroll_count=0;
}

rbool cursor_wrap;

void agt_puts(const char *s)
{
  int old_x;
  old_x=rwherex()-1;
  rputs(s);
  curr_x=rwherex()-1;
  if (curr_x<old_x+strlen(s))  /* We wrapped */
    cursor_wrap=1;
  if (DEBUG_OUT) fprintf(stderr,"%s",s);
  if (script_on) fprintf(scriptfile,"%s",s);
}

void agt_newline(void)
{
  if (!cursor_wrap) rputs("\r\n");
  curr_x=0;cursor_wrap=0;
  if (DEBUG_OUT) fprintf(stderr,"\n");
  if (script_on) fprintf(scriptfile,"\n");
  scroll_count++;
  if (scroll_count>=screen_height-status_height-1) {
    rputs("  --MORE--"); /* Notice: no newline */
    agt_waitkey();
    rgotoxy(1,rwherey()); /* Move to beginning of line */
    rclreol();  /* Clear to end of line: erase the --MORE-- */
  }
}




/*------------------------------------------------------------------*/
/*  Text Box Routines                                               */
/*------------------------------------------------------------------*/


static unsigned long boxflags;
static int box_startx; /* Starting x of box; starts from 0 */
static int box_width;
static int delta_scroll; /* Amount we are adding to vertical scroll
			    with the box */

static void box_border(char *c)
{
#ifdef DRAW_BORDER
  agt_puts(c);
#else
  agt_puts(" ");
#endif
}


#define VLINE_STR "\263"  /* 0xB3 or 0xBA */

static void boxrule(int bottom)
/* Draw line at top or bottom of box */
{ 
  int i;

  if (bottom) 
    box_border("\300"); /* 0xC0 or 0xC8 */   
  else 
    box_border("\332"); /* 0xDA or 0xC9 */
  for(i=0;i<box_width+2;i++) box_border("\304");  /* 0xC4 or 0xCD */
  if (bottom)
    box_border("\311");  /* 0xC9 or 0xBC */
  else
    box_border("\277"); /* 0xBF or 0xBB */
}


static void boxpos(void)
{
  int curr_y;

  curr_y=rwherey(); /* == y location + 1 for status line */
  if (curr_y>screen_height) curr_y=screen_height;
  rgotoxy(box_startx+1,curr_y+1);
  curr_x=box_startx;
}

void agt_makebox(int width,int height,unsigned long flags)
/* Flags: TB_TTL, TB_BORDER, TB_NOCENT */
{
  int box_starty;

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
    box_starty=(screen_height-height)/2;
    if (box_starty+height+8>screen_height) /* Make room for credits */
      box_starty=screen_height-height-8;
    if (box_starty<1) box_starty=1;
    delta_scroll=0;
    scroll_count=0;
    rwindow(1,1,screen_width,screen_height); /* Titles get the whole screen */
    is_fullscreen=1;
  } else {  /* Compute vertical position of non-title box */
    box_starty=rwherey()-1-height;
    if (box_starty<1) {
      delta_scroll=1-box_starty;
      box_starty=1;
    } else delta_scroll=0;
  }

  curr_x=box_startx;
  rgotoxy(box_startx+1,box_starty+1);

  if (boxflags&TB_BORDER) {
    push_state(BOX_STATE);
    boxrule(0);
    boxpos();
    box_border(VLINE_STR);
    agt_puts(" ");
  }
}

void agt_qnewline(void)
{
  if (boxflags&TB_BORDER) { 
    agt_puts(" ");
    box_border(VLINE_STR);
  }
  boxpos();
  if (boxflags&TB_BORDER) {
    box_border(VLINE_STR);
    agt_puts(" ");
  }
}

void agt_endbox(void)
{
  if (boxflags&TB_BORDER) {
    agt_puts(" ");
    box_border(VLINE_STR);
    boxpos();
    boxrule(1);
    pop_state();
  }
  scroll_count+=delta_scroll;
  agt_newline(); /* NOT agt_qnewline() */
  if ( (boxflags&TB_TTL) )
     while(rwherey()+1<screen_height-8) agt_newline();
}



/*------------------------------------------------------------------*/
/*   CFG Editor (In development)                                    */
/*------------------------------------------------------------------*/
#if 0

static char **readconf(fc_type fc)
{

}


static void writeconf(fc_type fc, char **conf)
{

}


static char *edops[]={"color","navarrow","tone","compass","input_bold"};


static rbool checkopt(int i)
     /* Return true if option i is true */
{
  switch(i) {
    case 1:return nav_arrow;  
    case 2:return PURE_TONE;
    case 3:return have_compass;
    case 4:return PURE_INPUT;
    default: return 1;
  }
}  


static void savechange(fc_type fc, uchar changes)
     /* Call this to save changes to configuration */
{
  char **conf, *s;
  int i;
  int lopt[5];
  /* lxxx holds the line number of xxx */

  lcolor=lnavarrow=lcompass=ltone=linputbold=-1;  
  conf=readconf(fc);
  for(numline=0;conf[numline]!=NULL;numline++) {
    /* Detect COLOR, NAVARROW, TONE, COMPASS, INPUT_BOLD */
    s=conf[numline];
    while(strncasecmp(s,"no_",3)==0) s+=3;
    for(i=0;i<5;i++)
      if (strncasecmp(s,edops[i],strlen(edops[i]))==0) {
	lopt[i]=numline;
	break;
      }
  }

  /* Rebuild config file */
  for(i=0;i<5;i++, changes>>=1)
    if (changes & 1) { /* Check bit */

      if (i==0) { /* COLOR */
	int j;
	s=rmalloc(5+5*15); /* More than enough space */
	strcpy(s,"color");
	for(j=0;j<5;j++) {
	  strcat(s," ");
	  strcat(s,colorname[colorset[j]]);
	}

      } else { /* Non-COLOR options */
	s=rmalloc(strlen(edops[i])+4); /* Room for NO_ & \0 */
	if (!checkopt(i))
	  strcpy(s,"no_");
	else s[0]=0;
	strcat(s,edops[i]);
      }

      if (i>=0) {  /* Change existing line */
	rfree(conf[numline]); 
	conf[numline]=s;       
      } else {   /* Add new line */
	conf=rrealloc(++numline);
	conf[numline-1]=s;
	conf[numline]=NULL;
      }

    }
  writeconf(fc, conf);

  for(i=0;i<numline;i++)
    rfree(conf[i]);
  rfree(conf);
}


void editconf(void)
{


  

}





#endif
/*------------------------------------------------------------------*/
/*   Parsing CONFIG Options                                         */
/*------------------------------------------------------------------*/

const char *colorname[16]=
{"black","blue","green","cyan","red","magenta","brown","lightgray",
 "darkgray","lightblue","lightgreen","lightcyan","lightred","lightmagenta",
"yellow","white"};

static int parse_color(int index,char *cname)
{
  int i;
  for(i=0;i<16;i++)
    if(strcasecmp(colorname[i],cname)==0) return i;
  return colorset[index];
}


static void getcolors(int cnum,char *clist[])
{
  int i;

  if (cnum>5) cnum=5;
  for(i=0;i<cnum;i++)
    colorset[i]=parse_color(i,clist[i]);
  colorset[cs_back]&=0x7;
  colorset[cs_statback]&=0x7;
}


/* Duplicate s and add '/' if neccessary */
char *makepathentry(char *s)
{
  char *p;
  int n;
  rbool addslash;

  addslash=0;
  n=strlen(s);
  if (s[n-1]!='\\') addslash=1;
  p=rmalloc(n+addslash+1);
  strcpy(p,s);
  if (addslash) {
    p[n]='\\';
    p[n+1]=0; /* Mark new end of string */
  }
  return p;
}

void free_gamepath(void)
{
  char **p;

  if (gamepath==NULL) return;

  /* We skip the first element which always points to "" */
  for(p=gamepath+1;*p!=NULL;p++)
    rfree(*p);
  rfree(gamepath);
}


#define opt(s) (!strcasecmp(optstr[0],s))

void set_fkey(char *keyname, char *words[], int numwords)
{
  int n,i,leng;
  char *s;
  rbool err;
  static long keys_defined=0;

  err=0; n=0;
  if (tolower(keyname[0])!='f') {
    err=1;
    for(i=0;i<12;i++) 
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

  if (optnum==0) return 1;
  if (opt("COMPASS")) {
    have_compass=1;
    return 1;
  }
  if (opt("FONT")) {
    font_enable=setflag;
    return 1;
  }
  if (opt("NAVARROW")) {
    nav_arrow=setflag;
    return 1;
  }
  if (opt("BIOS")) {
    use_bios=setflag;
    return 1;
  }
  if (opt("BLOCK_CURSOR")) {
    block_cursor=setflag;
    return 1;
  }
  if (opt("COLORS") || opt("COLOURS")) {
    getcolors(optnum-1,optstr+1);
    return 1;}
  if (opt("HISTORY")) {
    histmax=strtol(optstr[1],NULL,10);
    if (histmax<0) histmax=0;
    return 1;
  }
  if (opt("50_LINE")) {
    tall_screen=setflag;
    return 1;
  }
  if (opt("KEY") && optnum>=2) {
    set_fkey(optstr[1],optstr+2,optnum-2);
    return 1;
  }
  if (opt("PATH") && optnum>=2) {
    free_gamepath();  /* Throw away previous settings */
    gamepath=rmalloc((optnum+1)*sizeof(char*)); /* optnum-1+1 */
    gamepath[0]=""; /* ==> The current directory */
    for(i=1;i<optnum;i++)  /* Starting at the 2nd entry of both */
      gamepath[i]=makepathentry(optstr[i]);
    gamepath[optnum]=NULL;
    return 1;
  }
  return 0;
}

#undef opt

static char *progname;
static const char *cfgname="agil.cfg";

FILE *agt_globalfile(int fid)
{
  char *s,*t;
  FILE *f;

  if (fid==0 && progname!=NULL) {
    s=rstrdup(progname);
    t=s+strlen(s);
    while (t!=s && *t!='\\' && *t!=':') t--; 
    if (*t!=0) t++;
    *t=0;
    s=rrealloc(s,strlen(s)+strlen(cfgname)+1);
    strcat(s,cfgname);
    f=fopen(s,"rb");
    rfree(s);
    return f;
  }
  return NULL;
}





/*------------------------------------------------------------------*/
/*  Initialization and Shutdown                                     */
/*------------------------------------------------------------------*/

static void id_hardware(void)
{
  union REGS r;

  r.h.ah=0x12;  /* EGA+ Misc functions */
  r.h.bl=0x10;  /*   ...get information */
  r.h.bh=0x37;  /* Check value */
  r.x.cx=0x73F1; /* Random check value */
  int86(0x10,&r,&r);
  if ( (r.h.bh==0 || r.h.bh==1) &&
       (r.h.bl>=0 && r.h.bl<=3) &&
       (r.h.ch & 0xF0)==0 && (r.h.cl & 0xF0)==0) {
    /* Okay, the function works; we have an EGA/VGA */

    if (r.h.bl==0) graph_hardware=EGA64;
    else graph_hardware=EGA;

    r.x.ax=0x1A00;
    int86(0x10,&r,&r);
    if (r.h.al==0x1A) /* We have a VGA */
      graph_hardware=VGA;

  } else { /* MDA/CGA/MCGA */
    r.x.ax=0x1A00;
    int86(0x10,&r,&r);
    if (r.h.al==0x1A) { /* We have an MCGA */
      graph_hardware=MCGA;
      return;
    }

    graph_hardware=CGA;

    r.h.ah=0x0F; /* Get current video mode */
    int86(0x10,&r,&r);
    if ((r.h.al & 0x7F)==7)
      graph_hardware=MDA;
  }
}


static int save_mode;
static int save_attr;

void init_interface(int argc,char *argv[])
{
  struct text_info term_data;

  active_font=NULL;

  id_hardware();

  if (argc>0) progname=argv[0]; else progname=NULL;
  script_on=0;scriptfile=NULL;
  scroll_count=0;
  cursor_wrap=0;
  font_enable=1;
  nav_arrow=0;
  have_compass=0;status_height=0;
  block_cursor=0; tall_screen=0;
  center_on=par_fill_on=0;
  DEBUG_OUT=0;debugfile=stderr;
  rgettextinfo(&term_data);
  save_mode=term_data.currmode;
  save_attr=term_data.attribute;
  directvideo=0;   /* During startup, go through BIOS. */
  use_bios=0;  
  rtextmode(C80);  /* Change to color, 80 column mode */
  screen_height=25;  /* Assume PC dimensions */
  status_width=screen_width=80;
  rclrscr();
  rwindow(1,1,screen_width,screen_height); is_fullscreen=1;
  top_row=1;
  rgotoxy(1,1); /* Upper-left hand corner of just defined window */
}

void start_interface(fc_type fc)
{
  if (tall_screen) { 
    screen_height=rtextmode(C4350);  /* Change to color, 80 column mode */
    rclrscr();
    rgotoxy(1,1); /* Upper-left hand corner of just defined window */
  }
  directvideo=!use_bios; /* Set directvideo according to options */
  free_gamepath();
  if (stable_random)
    srand(6);
  else 
    srand(time(0));
  gamefile_name=fc->gamename;
  if (block_cursor)
    r_setcursortype(_SOLIDCURSOR);
  else
    r_setcursortype(_NORMALCURSOR);
  rclrscr();
  curr_x=0;scroll_count=0;
  if (have_compass)
     status_height=2;
  else
      status_height=1;
  rwindow(1,1,screen_width,screen_height);  /* Set window to whole screen */
  is_fullscreen=1; /* We use the whole screen until the first status line
                      is printed */
  top_row=status_height+1;
  set_state(7);
}

void close_interface(void)
{
  if (scriptfile!=NULL)
    fclose(scriptfile);
  agt_newline();agt_newline();
  if (block_cursor) r_setcursortype(_NORMALCURSOR);
  rtextmode(save_mode);
  rtextattr(save_attr);
  rgotoxy(1,25);
  rclreol();
  rgotoxy(1,24);
  rclreol();
 /* rclrscr();*/
}






#ifdef REPLACE_BNW

/*------------------------------------------------------------------*/
/*   GRAPHICS AND FONT SUPPORT                                      */
/*------------------------------------------------------------------*/


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
#ifdef MSDOS16
  struct REGPACK r;

  r.r_ax=0x1100;
  r.r_bx=font_height<<8; /* 8 bytes per character in BH; 0 in BL */
  r.r_cx=256; /* Number of characters defined */
  r.r_dx=0;  /* Start at ASCII 0. */
  r.r_es=((long)fontdef)>>16;
  r.r_bp=((long)fontdef)&0xFFFF;
  intr(0x10,&r);

#else /* MSDOS32 */

  __dpmi_regs r;

  /* If the transfer buffer isn't large enough, we just silently fail.
     We should really do something like allocate our own buffer 
     in this case. */
  if (font_height*256>_go32_info_block.size_of_transfer_buffer)
    return 1; 

  r.x.ax=0x1100;
  r.h.bh=font_height; /* 8 bytes per character in BH; 0 in BL */
  r.h.bl=0; 
  r.x.cx=256; /* Number of characters defined */
  r.x.dx=0;  /* Start at ASCII 0. */
  dosmemput(fontdef,font_height*256, __tb);
  r.x.es=__tb>>4; /* Segment */
  r.x.bp=__tb&0x0F;  /* Offset */
  __dpmi_int(0x10,&r);
#endif
  return 0;
}


static void save_old_font(void)
{
  union REGS r;

  if (graph_hardware<MCGA) {
    /* User-defined fonts aren't supported */
    font_enable=0;
    return;
  }

  r.x.ax=0x1130;
  r.h.bh=0;
  int86(0x10,&r,&r);
  save_font_height=r.x.cx;
}


static void restore_font(void)
{
    union REGS r;

    if (save_font_height==14)
      r.x.ax=0x1101;  /* 8x14 */
    else if (save_font_height==16)
      r.x.ax=0x1104;  /* 8x16 */
    else
      r.x.ax=0x1102;  /* 8x8 */
    r.h.bl=0;
    int86(0x10,&r,&r);
    /* ... if MCGA ... */
    /* if (graph_hardware==MCGA) {*/
      r.x.ax=0x1103;
      r.h.bl=0;
      int86(0x10,&r,&r);
    /* } */
}



void fontcmd(int cmd,int font)
/* 0=Load font, name is fontlist[font]
   1=Restore original font
   2=Set startup font. (<gamename>.FNT)
*/
{
  FILE *fontfile;
  char *buff, *fontname;
  int height;

  if (!font_enable) return;

  if (cmd==2)
    save_old_font();
  if (!font_enable) return;


  if (cmd==0 || cmd==2) {
    if (cmd==0) {
      fontname=fontlist[font];
    }
    else fontname=gamefile_name;

    fontfile=linopen(fontname,".fnt");
    if (fontfile==NULL) return;
    fseek(fontfile,0,SEEK_END);
    height=ftell(fontfile);
    if (height%256!=0) {
      /* Error message */
      return;
    }
    height=height/256;
    buff=rmalloc(256*height);
    memset(buff,0,256*height);

    fseek(fontfile,0,SEEK_SET);
    if (fread(buff,height,256,fontfile)!=256) {
	fclose(fontfile);
	rfree(buff);
	return;
    }
    fclose(fontfile);
    if (set_font(buff,height)) {
      /* Print error message */
    }
    rfree(active_font);
    active_font=buff;
    active_font_height=height;
    return;
  } else if (cmd==1) {
    rfree(active_font);
    restore_font();
  }
}





/*------------------------------------------------------------------*/
/*   Graphics Support                                               */
/*------------------------------------------------------------------*/

static rbool in_gmode=0;

static char *gfxext[]={".P06",
		       ".P40",".P41",".P42",".P43", /* 0-4: CGA */
		       ".P13", /* 5: EGA */
		       ".P19", /* 6: MCGA */
		       ".P14",".P16", /* 7-8: EGA */
		       ".P18"}; /* 9: VGA */

/* Modes 1-4 differ only in pallette */

/*                    0   1   2   3   4     5   6     7   8    9  */
static int paltype[]={0,  1,  1,  1,  1,    2,  3,    2,  2,   2};
static int mode_x[]={640,320,320,320,320,  320,320,  640,640, 640};
static int mode_y[]={200,200,200,200,200,  200,200,  200,350, 480};
static int mode[]  ={ 6,  4,   4,  4,  4,   13, 19,  14, 16,   18};

/* paltype is the log_2 of the bit depth */
/* Mode 16 with lowmem EGA only has 2bpp */
/* Mode to start from for each video type */
/* MDA, CGA, MCGA, EGA65, EGA, VGA */
static int start_mode[]={-1,4,6,7,8,9};
  

int pick_gmode(int gmode, int pmode, int xsize, int ysize)
{
  int save_gmode, work_gmode;

  if (gmode!=-1) /* Check that gmode is consitent with file */
    if (mode_x[gmode]>=xsize &&  mode_y[gmode]>=ysize &&
         paltype[gmode]==pmode)
      return gmode;

  save_gmode=gmode; work_gmode=-1;
  /* Okay, we don't know what mode we are; try to pick the best for
     this image */
  /* work_gmode will contain the highest resolution mode with the
     neccessary color depth; if we can't find a perfect mode, we'll
     use this and clip. */
  for(gmode=0;gmode<=start_mode[graph_hardware];gmode++) {
    if (gmode==6 && (graph_hardware==EGA || graph_hardware==EGA64))
      continue;
    if (gmode==5 && graph_hardware==MCGA) continue;

    if (paltype[gmode]!=pmode) continue;
    work_gmode=gmode;
    if (mode_x[gmode]<xsize || mode_y[gmode]<ysize) continue;
    return gmode;
  }
  return work_gmode; /* May need to be clipped */
}


int pix_per_byte[]={8,4,8,1};

/* 8421  */
/* IRGB */

#define BPI 8
#define BPR 4
#define BPG 2
#define BPB 1

#ifdef MSDOS16
typedef uchar* graphptr;
#define graphwrite(dst,src,leng) memcpy(dst,src,leng)
#else /* MSDOS32 */
typedef unsigned long graphptr;
#define graphwrite(dst,src,leng) dosmemput(src,leng,dst)
#endif

void draw_scanline(uchar *linebuff,int gmode,int y_start,
		   int linesize,int sizex)
{
  static const char mmr[4]=
         {BPB,BPG,BPR,BPI}; /* Seq Mask Map Register values */
  static int mode_bpl, writesize;
  static uchar end_mask;
  static graphptr target;
  int i;

  if (linebuff==NULL) {  /* Set up before first line */
    mode_bpl=mode_x[gmode]/pix_per_byte[paltype[gmode]];
    i=(mode_x[gmode]-sizex)/(2*pix_per_byte[paltype[gmode]]); /* Center it */

#ifdef MSDOS16
    if (gmode<=4) target=(uchar*)0xB8000000L;
    else target=(uchar*)0xA0000000L;
#else /* MSDOS32 */
    if (gmode<=4) target=0xB8000L;
    else target=0xA0000L;
#endif

    target+=i+y_start*mode_bpl;

    /* Now set i to the number of "extra" pixels */
    i=linesize*pix_per_byte[paltype[gmode]]-sizex;
    writesize=linesize-i/pix_per_byte[paltype[gmode]];
    i=i%pix_per_byte[paltype[gmode]];
    i=i*(1<<8/pix_per_byte[paltype[gmode]]); /* i=# bits of excess */
    end_mask=(0xFF<<i)&0xFF; /* ...and construct a bit mask */

    if (paltype[gmode]==2) { /* Bit plane graphics */
      /* 3CE/3CF: Graphic Control Registers */
      /*  Graphic Ctrl Reg (3CE/3CF):
	  1: 0x00 (Enable Set/reset: CPU => bitplanes)
	  3: 0x00  (Function select: replace)
	  5: 0x00  (Write and read mode 0)
	  8: 0xFF (Bit Mask) */

      /* First, basic initalization. Most of these are default values,
	 but it doesn't hurt to be careful */
      outp(0x3CE,0x01); /* Enable Set/Reset Register */
        outp(0x3CF,0x00); /* ... ignore Set/Reset register */
      outp(0x3CE,0x03); /* Function Select Register */
        outp(0x3CF,0x00); /*   ... replace and don't rotate */
      outp(0x3CE,0x05); /* Mode Register */
        outp(0x3CF,0x00); /*   ... write and read mode 0 */
      outp(0x3CE,0x08); /* Bit Mask Register */
        outp(0x3CF,0xFF); /*  ... write to everything */
    }
    return;
  }

  target+=mode_bpl; /* Move to beginning of next line */

  if (paltype[gmode]!=2) {  /* The easy case: just one plane */
    linebuff[writesize-1]&=end_mask;
    graphwrite(target,linebuff,writesize);
  } else { /* 4 planes, 1 bit/pixel/plane */
    /* 3C4/3C5: Video Sequencer Registers
         2: Map Mask Register */
    for(i=0;i<4;i++) {
      outp(0x3C4,2); /* Map Mask Register */
      outp(0x3C5,mmr[i]); /* ... select the given bitplane */
      linebuff[linesize*i+writesize-1]&=end_mask;
      graphwrite(target,linebuff+linesize*i,writesize);
    }
  }
}



/* This flips the bits and separates them by three bits */
static const uchar paltrans[]={0,8,1,9};


static uchar build_ega_pal(uchar red, uchar green, uchar blue)
     /* Construct 6-bit color code, on pattern
	00rgbRGB, from Rrxxxxxx, Ggxxxxxx, Bbxxxxxx */
{
  red>>=6;
  green>>=6;
  blue>>=6; 
  return paltrans[blue]+(paltrans[green]<<1)+(paltrans[red]<<2);
}


/* When dacflag is 0, this sets the video palette;
   when dacflag is 1, it sets the DAC registers instead */
static void bios_set_pal(uchar *palptr, unsigned leng, rbool dacflag)
{
#ifdef MSDOS16
  union REGS r;
  struct SREGS segreg;
#else /* MSDOS32 */
  __dpmi_regs r;
#endif

  if (!dacflag) 
    r.x.ax=0x1002;
  else {
    r.x.ax=0x1012;
    r.x.bx=0;
    r.x.cx=leng;
  }
#ifdef MSDOS16
  r.x.dx=((long)palptr)&0xFFFF;
  segreg.es=((long)palptr)>>16;
  int86x(0x10,&r,&r,&segreg);
#else /* MSDOS32 */
  dosmemput(palptr,leng*(dacflag?3:1), __tb);
  r.x.dx=__tb & 0x000F; /* Offset */
  r.x.es=__tb>>4;       /* Segment */
  __dpmi_int(0x10,&r);
#endif
}


#define ival(n) (header[n]+(header[n+1]<<8))


static rbool setup_card(FILE *pcxfile,uchar *header,int gmode, int pmode)
{
  int i;
  uchar *tmppal, *pal;
  int havepal; /* 0=No, 1=Yes, but not VGA, 2=Yes, maybe VGA */
  union REGS r;

  if (header[1]==5) havepal=2;
  else if (header[1]==2) havepal=1;
  else havepal=0;

  if (pmode==3 && havepal!=2) {
    writeln("ERROR: PCX file corrupted: bad color depth.");
    return 0;
  }

  /*-------------------------------------*/
  /* Read in VGA palette                 */
  /*-------------------------------------*/

  /* Next, read in the VGA palette if neccessary */
  pal=NULL;
  if (pmode==3) { /* VGA palette */
    uchar c;
    long n;

    pal=rmalloc(256*3);
    fseek(pcxfile,0,SEEK_END);
    n=ftell(pcxfile);
    if (fseek(pcxfile,n-256*3-1,SEEK_SET)
         || fread(&c,1,1,pcxfile)!=1) {
       writeln("GAME ERROR: Errors reading picture file.");
       rfree(pal);
       return 0;
    }
    if (c!=12) {
      writeln("ERROR: PCX file corrupted: bad palette.");
      rfree(pal);
      return 0;
    }
    if (fread(pal,768,1,pcxfile)!=1) {
       writeln("GAME ERROR: Errors reading palette from picture file.");
       rfree(pal);
       return 0;
    }
  }


  /*-------------------------------------*/
  /* Set up video mode                   */
  /*-------------------------------------*/

  /* -- Need to save old video state here -- */

  /* Set video mode */
  r.x.ax=mode[gmode];
  int86(0x10,&r,&r);
  in_gmode=1;

  /*-------------------------------------*/
  /* Set up Palette                      */
  /*-------------------------------------*/

#if 0
  if (pmode==2 && paltype[gmode]==3) {
    /* Set up fake EGA palette */
    pal=rmalloc(256*3);
    for(i=0;i<3*16;i++) 
      pal[i]=header[16+i];
    for(i=3*16;i<3*256;i++)
      pal[i]=0;
  } else if (pmode<=1 && paltype[gmode]==2) {
    /* This could occur with a high resolution 2 or 4 color image */

    fatal("Unsupported image class.");

  } else assert(pmode==paltype[gmode]);
#endif

  assert(pmode==paltype[gmode]);

  /* Now to set the palette */
  if (paltype[gmode]==1) { /* CGA palette */
    r.h.ah=0x0B;
    r.h.bh=1;
    if (pmode==1 && havepal!=0)
      r.h.bl=(header[19]>>6)&0x01;
    else 
      r.h.bl=(gmode-1)>>1; /* Top bit==> palette */
    int86(0x10,&r,&r);

    r.h.ah=0x0B; /* Set background and intensity bits */
    r.h.bh=0;
    if (pmode==1 && havepal!=0) 
      r.h.bl= ( (header[19]>>1) & 0x10 ) | (header[16]>>4);
    else
      r.h.bl=((gmode-1)&1)<<4; /* Background will be 0. */
  }
  else if (paltype[gmode]==2 && havepal!=0) {  /* EGA Palette */
    tmppal=rmalloc(17);
    if (graph_hardware!=VGA)
      for(i=0;i<16;i++)
	tmppal[i]=build_ega_pal(header[16+3*i],
				header[17+3*i],
				header[18+3*i]);
    else for(i=0;i<16;i++) tmppal[i]=i+1; /* For VGA hardware, point to DAC */
    tmppal[16]=0; /* Set overscan to 0 */
    bios_set_pal(tmppal,17,0);
    rfree(tmppal);

    if (graph_hardware==VGA) { /* Now reprogram the DAC */
      tmppal=rmalloc(17*3);
      for(i=0;i<16;i++) {
	tmppal[3+3*i]=header[16+3*i]>>2;
	tmppal[4+3*i]=header[17+3*i]>>2;
	tmppal[5+3*i]=header[18+3*i]>>2;	  
      }
      tmppal[0]=tmppal[1]=tmppal[2]=0; /* Overscan color is black */
      bios_set_pal(tmppal,17,1);
    }
  } else if (paltype[gmode]==3 && havepal!=0) { /* MCGA Palette */
    for(i=0;i<256*3;i++)
      pal[i]>>=2;
    bios_set_pal(pal,256,1);
  }
  return 1;
}


#ifdef DEBUG_VIDCARD
static int test_pattern(int y, int p)
{
  if (p==1) return (1<<(y%8));
  return 0;
}
#endif

static int save_pmode; /* Debugging only */



#define PCX_BUFFSIZE 16000
static uchar *fbuff;
static int fb_index;

static int readbyte(FILE *f)
{
  int n;

  if (fb_index==PCX_BUFFSIZE) { 
    n=fread(fbuff,1,PCX_BUFFSIZE,f);
    if (n==0) return EOF;
    if (n<PCX_BUFFSIZE) memset(fbuff+n,0,PCX_BUFFSIZE-n);
    fb_index=0;
  }
  return fbuff[fb_index++];
}




static void display_PCX(FILE *pcxfile, int gmode)
     /* pcxfile= the file to display.
	gmode= suggested video mode. */
{
  int i,j, k;
  int pmode; /* PCX palette mode: 0=CGA hires 1=CGA, 2=EGA, 3=MCGA */
  uchar header[128];
  int xsize,ysize;
  uchar *linebuff; /* Up to four planes */
  int linesize; /* Number of bytes in a line */
  int y_ofs;
  int c;

  /*-------------------------------------*/
  /* Read in header                      */
  /*-------------------------------------*/

  if (fread(header,128,1,pcxfile)!=1) {
    writeln("GAME ERROR: Picture file corrupted.");
    return;
  }

  /* Verify header */
  if (header[0]!=10 || header[2]!=1 ||
       (header[1]!=1 && header[1]!=2 && header[1]!=3 && header[1]!=5))
     {writeln("ERROR: Unrecognized PCX version.");return;}

  /* Determine which palette type we're using */

  if (header[3]==1 && header[65]==1) pmode=0; /* CGA 2-color */
  else if (header[3]==2 && header[65]==1) pmode=1; /* CGA 4-color */
  else if (header[3]==1 && header[65]==4) pmode=2; /* EGA/VGA 16-color*/
  else if (header[3]==8 && header[65]==1) pmode=3; /* MCGA 256-color */
  else {
       writeln("ERROR: Unsupported graphics mode.");
       return;
  }

  xsize=ival(8)-ival(4)+1;
  ysize=ival(10)-ival(6)+1;

  gmode=pick_gmode(gmode,pmode,xsize,ysize);
  if (gmode==-1) {pmode=-4; return;}

  assert(pmode==paltype[gmode]); /* Temporary */

  /* Clip if neccessary */
  if (xsize>mode_x[gmode]) xsize=mode_x[gmode];
  if (ysize>mode_y[gmode]) ysize=mode_y[gmode];

  save_pmode=pmode;

  if (!setup_card(pcxfile,header,gmode,pmode)) return;


  /*-------------------------------------*/
  /* Draw picture                       */
  /*-------------------------------------*/

  fseek(pcxfile,128,SEEK_SET); /* Beginning of picture data */

  linesize=ival(66);
  linebuff=rmalloc(linesize*header[65]);

  y_ofs=(mode_y[gmode]-ysize)/2;

  /* This will set up the scanline drawing routines */
  draw_scanline(NULL,gmode,y_ofs,linesize,xsize);

#ifdef DEBUG_VIDCARD
  for(i=0;i<ysize;i++) {
    memset(linebuff,0,linesize*header[65]);
    if (pmode==2) {
       int j;
       for(j=0;j<4;j++)
         memset(linebuff+j*linesize,
                test_pattern(i,j),linesize);
    } else if (pmode==3) {
       memset(linebuff,0,linesize);
    } else if (pmode==1) {
       memset(linebuff,0,linesize);
    } else memset(linebuff,0,linesize); /* Vertical lines */

    draw_scanline(linebuff,gmode,y_ofs+i,linesize,xsize);
  }
#endif

  fbuff=rmalloc(PCX_BUFFSIZE); /* The input buffer */
  fb_index=PCX_BUFFSIZE;

  /* Draw picture */
  for(i=0;i<ysize;i++) {
    memset(linebuff,0,linesize*header[65]);
    for(j=0;j<linesize*header[65];) {
      c=readbyte(pcxfile);
      if (c==EOF) {save_pmode=-1;return;}
      if ( (c&0xC0)==0xC0) {
	k=c&0x3F; /* Count=lower 6 bits */
	c=readbyte(pcxfile);
          assert(c!=EOF);
	if (c==EOF) {save_pmode=-2;return;}
	for(;k>0 && j<linesize*header[65];k--)
	  linebuff[j++]=c;
          if (k>0) {save_pmode=-3;return;}
      } else
	linebuff[j++]=c;
    }
    draw_scanline(linebuff,gmode,y_ofs+i,linesize,xsize);
  }
  rfree(fbuff);
  rfree(linebuff);
}




void pictcmd(int cmd,int pict)
/* 1=show global picture, name is pictlist[pict]
   2=show room picture, name is pixlist[pict]
   3=Show title picture.
  */
{
  union REGS r;
  char *base;
  int gmode;
  FILE *pcxfile=NULL;

  int savex,savey;
  char *save_vidmem;

  if (graph_hardware==MDA) return; /* MDA is out */

  if (cmd==1) base=pictlist[pict];
  else if (cmd==2) base=pixlist[pict];
  else if (cmd==3) base=gamefile_name;
  else return;

  while(kbhit()) rgetch(); /* Discard any buffered keypresses */

  /* Find graphics file; determine mode from extension... */  
  for(gmode=start_mode[graph_hardware];gmode>=0;gmode--) {
    if (gmode==6 && (graph_hardware==EGA || graph_hardware==EGA64))
      continue; /* EGA should skip MCGA mode */
    if (gmode==5 && graph_hardware==MCGA) continue; /* ... and conversely */
    pcxfile=linopen(base,gfxext[gmode]);
    if (pcxfile!=NULL) break;
  }
  if (gmode==-1) {
    pcxfile=linopen(base,".pcx");
    if (pcxfile==NULL) return;
    /* Leave gmode as -1 */
  }

  in_gmode=0;
  savex=rwherex();
  savey=rwherey();
#ifdef MSDOS16
  save_vidmem=rmalloc(2*25*80);
  memcpy(save_vidmem,(void*)0xB8000000L,2*25*80);
#else /* MSDOS32 */
  if (graph_hardware>MCGA) 
    save_vidmem=rmalloc(2*ScreenRows()*ScreenCols());
  else
    save_vidmem=rmalloc(2*25*80);
  ScreenRetrieve(save_vidmem);
#endif

  display_PCX(pcxfile,gmode);

  fclose(pcxfile);

  /*-------------------------------------*/
  /* Wait for key and restore text mode  */
  /*-------------------------------------*/

  if (in_gmode) {

    if (!BATCH_MODE) rgetch();  /* Wait for a key-press */
    
    r.x.ax=0x0003;  /* Restore text mode */
    int86(0x10,&r,&r);

#ifdef MSDOS16
    memcpy((void*)0xB8000000L,save_vidmem,2*25*80); /* Restore video mem */
#else /* MSDOS32 */
    ScreenUpdate(save_vidmem);
#endif
    rgotoxy(savex,savey);  /* Restore cursor position */
    if (active_font!=NULL)   /* Restore font, if neccessary */
      set_font(active_font,active_font_height);
    reset_state();

    /* -- Need to restore old video state here -- */
    /*   -- Primary font (16*256=4K) */
    /*   -- Screen memory (2*25*80=4K) */
    /*   -- Cursor position and current screen attributes */

  }
  /* rprintf("Mode: %d\n",save_pmode); */
  rfree(save_vidmem);
}



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
  if (cmd==8) sound_on=1;
  else if (cmd==9) sound_on=0;
  if (cmd==-1) return sound_on;
  return 0;
}

#endif /* REPLACE_BNW */
