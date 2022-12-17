/*  exec.h-- Header file for verb execution routines      */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                        */
/* This is part of the source for AGiliTy: the (Mostly) Universal  */
/*       AGT Interpreter                                           */


#ifndef global    /* Don't touch this */
#define global extern
#define global_defined_exec
#endif



/* This contains the decoding of the current instruction */
typedef struct {
  integer op;
  int arg1;
  int arg2;
  int optype;
  int argcnt;  /* Actual number of argument words present */
  const opdef *opdata;
  const char *errmsg;
  rbool disambig; /* Trigger disambiguation? */
  rbool negate;  /* NOT? (cond token only) */
  rbool failmsg;  /* Run only on failure? */
  rbool endor;  /* End any OR blocks?  (action tokens, mainly) */
} op_rec;



/* The following determines if we are doing disambiguation
   or actually executing a verb */
global uchar do_disambig;  /* 0= execution
		       	      1= disambiguating noun
		              2= disambiguating object */


/* Flags used during turn execution */
global rbool beforecmd;     /* Only used by 1.8x games */
global rbool supress_debug; /* Causes debugging info to _not_ be printed
			      even if debugging is on; used by disambiguator
			      and to supress ANY commands */
global rbool was_metaverb; /* Was the verb that just executed a metaverb? */
          /* Metaverbs are commands that should not take game time 
	     to execute: SAVE, RESTORE, RESTART, QUIT, SCRIPT, UNSCRIPT,
	     NOTIFY, SCORE, etc. */
global integer oldloc;  /* Save old location for NO_BLOCK_HOSTILE purposes */

/* This is a hack to pass the subroutine number from exec_token
   back to scan_metacommand when a DoSubroutine is done */
global integer subcall_arg; 

/* This fixes a bug in the original AGT spec, causing "actor, verb ..."
   commands to misfire if there is more than one creature of the same
   name. */
global integer *creat_fix;


/* --------------------------------------------------------------------	*/
/* Defined in EXEC.C 							*/
/* -------------------------------------------------------------------- */
void raw_lineout(const char *s, rbool do_repl, 
		 int context, const char *pword);
void msgout(int msgnum,rbool add_nl);
void sysmsg(int msgid,char *s);
void alt_sysmsg(int msgid,char *s,
		parse_rec *new_dobjrec,parse_rec *new_iobjrec);
void sysmsgd(int msgid,char *s,parse_rec *new_dobj_rec);

rbool ask_question(int qnum);
void increment_turn(void);

/* Warning: the following function rfrees <ans> */
rbool match_answer(char *ans, int anum);

void look_room(void);
void runptr(int i, descr_ptr dp[], char *msg, int msgid, 
	    parse_rec *nounrec,parse_rec *objrec);

int normalize_time(int tnum); /* Convert hhmm so mm<60 */
void add_time(int dt);


/* --------------------------------------------------------------------	*/
/* Defined in OBJECT.C 							*/
/* --------------------------------------------------------------------	*/
parse_rec *make_parserec(int obj,parse_rec *rec);
parse_rec *copy_parserec(parse_rec *rec);
void free_all_parserec(void); /* Freeds doj_rec, iobj_rec, and actor_rec */

rbool in_scope(int item);
rbool islit(void);
rbool it_possess(int item);
rbool it_proper(int item);
rbool it_isweapon(int item);
rbool it_door(int obj,word noun); /* Is obj a door? */
rbool is_within(integer obj1, integer obj2, rbool stop_if_closed);

integer it_room(int item); /* Returns the room that the item is in */

int lightcheck(int parent,int roomlight,rbool active);
/* If active is false, we don't care if the light is actually working. */

#define it_move(a,b) it_reposition(a,b,0)
#define it_destroy(item) it_move(item,0)
#define get_obj(dobj) it_move(dobj,1)
#define drop_obj(dobj) it_move(dobj,loc+first_room)

void it_reposition(int item,int newloc,rbool save_pos);
void goto_room(int newroom);

void it_describe(int dobj);
int print_contents(int obj,int ind_lev);

void recompute_score(void);

int check_fit(int obj1, int obj2);

/* And its possible return values: */

#define FIT_OK 0     /* Fits */
#define FIT_WEIGHT 1   /* Too heavy [*]  */
#define FIT_NETWEIGHT 2  /* With other stuff is too heavy [*] */
#define FIT_SIZE 3    /* Too big */
#define FIT_NETSIZE 4   /* With other stuff is too big */
/* [*]-- These can only occur if obj2==1 or for ME/1.5-1.7 */


long getprop(int obj, int prop);
void setprop(int obj, int prop, long val);
rbool getattr(int obj, int prop);
void setattr(int obj, int prop, rbool val);

rbool matchclass(int obj, int oclass);

/* ---------------------------------------------------------------------- */
/* Define in RUNVERB.C                                                    */
/* ---------------------------------------------------------------------- */

/* Verbs actually used elsewhere in th interpreter */
void v_inventory(void);
void v_look(void);
void v_listexit(void);

/* The routine that actually runs the current player command */
void exec_verb(void); 


/* ---------------------------------------------------------------------- */
/* In METACOMMAND.C 							  */
/* ---------------------------------------------------------------------- */
/* The main routine to search the metacommand list and run the appropriate
   meta-commands */
int scan_metacommand(integer m_actor,int vcode,
		     integer m_dobj, word m_prep, integer m_iobj,
		     int *redir_flag);

/* The type checking routine */
rbool argvalid(int argtype,int arg);

/* ---------------------------------------------------------------------- */
/* In TOKEN.C 								  */
/* ---------------------------------------------------------------------- */
int exec_instr(op_rec *oprec); /* Execute instruction */
long pop_expr_stack(void);  /* Wrapper around routine to access TOS */

/* ---------------------------------------------------------------------- */ 
/* Defined in DEBUGCMD.C 						  */
/* ---------------------------------------------------------------------- */
void get_debugcmd(void);  /* Get and execute debugging commands */


/* ------------------------------------------------------------------- 	*/
/* Macros for getting information about items 				*/
/* (mainly used to blackbox the difference between nouns and creatures) */
/* --------------------------------------------------------------------	*/

/* A note on object codes:
       <0                 obj is a 'virtual' object, existing only as the word
                           dict[-obj], e.g. DOOR, flag nouns, global nouns
       0                  No object (or any object) 
       1                  Self(i.e. the player)
   first_room..last_room  Rooms
   first_noun..last_noun  Nouns
   first_creat..last_creat Creatures
      1000                Being worn by the player			*/


/* The following macro loops over the contents of an object */
#define contloop(i,obj)   for(i=it_contents(obj);i!=0;i=it_next(i))
#define safecontloop(i,j,obj) for(i=it_contents(obj),j=it_next(i); \
				  i!=0;i=j,j=it_next(i))

#define cnt_val(c) ((c)==-1 ? 0 : (c))


/* --------------------------------------------------------------------	*/
/* These are the macros that should usually be used to determine 	*/
/*  information about the objects in the game, unless the object type   */
/*  is definitely known  						*/
/* ------------------------------------------------------------------- 	*/

#define it_on(item) nounattr(item,on)
#define it_group(item) creatattr(item,groupmemb)
#define it_adj(item) objattr(item,adj)
#define it_pushable(item) nounattr(item,pushable)
#define it_pullable(item) nounattr(item,pullable)
#define it_turnable(item) nounattr(item,turnable)
#define it_playable(item) nounattr(item,playable) 
#define it_plur(item) nounattr(item,plural)
#define it_gender(item) creatattr(item,gender)

#define it_pict(item) objattr(item,pict)
#define it_class(item) anyattr(item,oclass)
#define it_next(item) objattr(item,next)
#define it_isglobal(item) objattr(item,isglobal)
#define it_flagnum(item) objattr(item,flagnum)
#define it_seen(item) anyattr(item,seen)


#define it_name(item) objattr2(item,name,(item<0) ? -item : 0)
#define it_open(item) nounattr2(item,open, tcreat(item) || \
				(tdoor(item) && !room[loc].locked_door))

     /* This checks to make sure the object isn't unmovable. */
     /* (As such, all non-nouns automatically pass) */
#define it_canmove(item) (!tnoun(item) || noun[(item)-first_noun].movable)


#ifdef IT_MACRO
#define it_contents(item) objattr2(item,contents,\
				   roomattr2(item,contents,\
					     (item==1) ? player_contents : \
					     (item==1000) ? player_worn : 0))
#define it_lockable(item)  nounattr2(item,lockable, (tdoor(item) ? 1 : 0) )
#define it_locked(item,name) nounattr2(item,locked,\
				  (tdoor(item) && room[loc].locked_door ? \
				   1 : 0))
#else
int it_contents(integer obj);
rbool it_lockable(integer obj,word noun);
rbool it_locked(integer obj, word noun);
#endif


#ifdef global_defined_exec
#undef global
#undef global_defined_exec
#endif


