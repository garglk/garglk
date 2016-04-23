/* 
 * header for the annoying stuff ignored in advsys - Zenki
 * michael.h.chen@yale.edu
 * ^
 * +--- Stupid for forgetting this bit.
 */

/*
 * Indicate your platform
 */

#define UNIX
/* #define WINDOWS */


#include <stdio.h>


#include <stdlib.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>

#ifdef WINDOWS
#    include <windows.h>
#endif


#include "glk.h"
#include "glkstart.h"


/* Rocks to use to keep track of stuff */
#define WINDOW		(1)
#define SOURCEFILE	(2)

/* Exported by advint.c */
extern void error(char *);
extern winid_t window;
extern strid_t screen;


/* Exported by advdbs.c */
extern int db_save();
extern void db_init(strid_t name);
extern int db_restore();
extern int db_restart();
extern void complement(char *adr,int len);
extern int findword(char *word);
extern int wtype(int wrd);
extern int match(int obj,int noun,int *adjs);
extern int checkverb(int *verbs);
extern int findaction(int *verbs,int preposition,int flag);
extern int getp(int obj,int prop);
extern int setp(int obj,int prop,int val);
extern int findprop(int obj,int prop);
extern int hasnoun(int obj,int noun);
extern int hasadjective(int obj,int adjective);
extern int hasverb(int act,int *verbs);
extern int haspreposition(int act, int preposition);
extern int inlist(int link, int word);
extern int getofield(int obj, int off);
extern int putofield(int obj,int off,int val);
extern int getafield(int act,int off);
extern int getabyte(int act,int off);
extern int getoloc(int n);
extern int getaloc(int n);
extern int getvalue(int n);
extern int setvalue(int n, int v);
extern int getwloc(int n);
extern int getword(int n);
extern int putword(int n,int w);
extern int getbyte(int n);
extern int getcbyte(int n);
extern int getcword(int n);
extern int getdword(char *p);
extern int putdword(char *p,int w);
extern void nerror(char *fmt,int n);

extern int h_init;
extern int h_update;
extern int h_before;
extern int h_after;
extern int h_error;


/* Exported by advexe.c */
extern int execute(int code);

/* Exported by advjunk.c */
extern int advsave(char *hdr,int hlen,char *save,int slen);
extern int advrestore(char *hdr,int hlen,char *save,int slen);

/* Exported by advmsg.c */
extern void msg_init(strid_t fd,int base);
extern void msg_open(unsigned int msg);
extern int msg_byte();

/* Exported by Advprs.c */
extern int parse();
extern int next();
extern void show_noun(int n);

/* exported by advtrm.c */
void trm_init(int rows, int cols, char *name);
void trm_done();
char *trm_get(char *line);
void trm_str(char *str);
void trm_xstr(char *str);
void trm_chr(int ch);
void trm_word();
void trm_eol();
void trm_wait();
char *trm_line(char *line);




















