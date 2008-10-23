/*  agtout.c-- Program to test agtread.c */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                                */
/* This is part of the source for AGiliTy, the (Mostly) Universal */
/*       AGT Interpreter                                          */

/* This program was originally written to test agtread.c. */
/* It dumps all of the file information to stdout in a human readable */
/*  format and so will also be of use to game authors who want to know */
/*  how the AGT compiler translated their work, and of course to game */
/*  players who have gotten stuck... :-) */
/* It's not quite as polished as some of the other modules; in particular, */
/*  if your system doesn't support I/O redirection, you'll need to redirect */
/*  stdin yourself in main() (using freopen, say) or else replace all the */
/*  printf calls by  fprintf calls. */ 

#include <ctype.h>
#define global
#include "agility.h"

#define AGX_DESC 1

/* These are for the sake of the disassembler */
integer dobj=0, iobj=0, actor=0;



/* DICTFIX */

#define verbose_dump 1

rbool printmsg, printdict, sortflag;
global rbool dbg_msgrange; /* Print the range of messages for
			     RandomMessage (only checked if
			     dbg_nomsg isn't set) */
global rbool quick_rndmsg; /* Don't give RandomMessage special treatment. */
global rbool no_badobj; /* Filter out objects with garbage names. */

long msgcnt(int ver,int aver)
{
  switch(ver) {
    case 1:return 250;
    case 2:if (aver==AGTCOS) return 700; /* Cosmosrv */
      else if (aver<=AGT12) return 500;
      else return 600;  /* 500? */
    case 3:return 700;
    case 4:return 900;
  }
  printf("INTERNAL ERROR: Invalid VER\n");
  return 0;
}

long cmdcnt(int ver,int aver)
{
  switch(ver) {
    case 1:return 400;
    case 2:  if (aver==AGTCOS) return 1100; 
      else return 900; /* 700? */
    case 3:return 900;
    case 4:return 1500;
  }
  printf("INTERNAL ERROR: Invalid VER\n");
  return 0;


}

long cmdsize(void)
{
  int i;
  long size;

  size=0;
  for(i=0;i<last_cmd;i++)
    size+=command[i].cmdsize;
  return size*sizeof(integer);
}


extern long dictstrptr, dictstrsize;


void printopt(char *s, char opt)
{
  static int x;
  if (s==NULL) {x=0;return;}
  if (opt==2) return;
  if (opt==1) printf("%s  ",s);
  if (opt==0) {printf("NO_%s  ",s);x+=3;}
  x+=strlen(s);
  if (x>60) {
    printf("\n");
    x=0;
  }
}


void nprintopt(char *s, char opt)
{
  if (opt==1) opt=0; 
  else if (opt==0) opt=1;
  printopt(s,opt);
}


void info_out(void)
{
  integer creatcnt;
  long tmp;

  printf("\nGeneral Information:(ver=%d, aver=%d)\n",ver,aver);
  printf("Signature: %ld\n",game_sig);
  printf("AGT Version %s,  (Size:%s)\n",averstr[aver],verstr[ver]);
  printf("\tStart room: %d    Treas Room: %d    Resurrect Room: %d\n",
	 start_room,treas_room,resurrect_room);
  printf("\tLives: %d    Max Score: %ld\n",max_lives,max_score);
  printf("\t");
  if (debug_mode) printf("<DEBUG>\t");
  if (have_meta) printf("<META>\t");
  if (freeze_mode) printf("<FREEZE>\t");
  if (mars_fix) printf("<MARS>\t");
  printf("Status Mode %d    Score mode %d",statusmode,score_mode);
  printf("   Time %d+%d\n",start_time,delta_time);
  if (milltime_mode) printf("\t<MILITARY>");
  printf("\n");
  printf("Ranges--  Room: %d..%d   Noun: %d..%d   Creature: %d..%d\n",
	 first_room,maxroom,first_noun,maxnoun,first_creat,maxcreat);
  printf("     ExitMsg Base:%d\n",exitmsg_base);
  printf("Max--\tVars: %d    Counters: %d    Flags: %d   User Strings: %d\n",
	 VAR_NUM, CNT_NUM, FLAG_NUM, MAX_USTR);
  printf("\tMetacommands: %ld     Subroutines: %d    Error Messages: %d\n",
	 last_cmd, MAX_SUB, NUM_ERR);
  if (aver>=AGTME10)
    printf("\tGlobal Nouns: %ld  Picts: %ld   RoomPIXs: %ld  Fonts: %ld   "
	   "Songs: %ld\n",
	   numglobal,maxpict,maxpix,maxfont,maxsong);
  if (aver>=AGX00) {
    printf("\tObject flags: %d/%d/%d \tObject Props: %d/%d/%d\n",
	   8*num_rflags,8*num_nflags,8*num_cflags,
	   num_rprops,num_nprops,num_cprops);
    printf("\t(Total ObjFlags: %d \tTotal ObjProps: %d )\n",
	   oflag_cnt,oprop_cnt);
  }
  printf("\tVocabulary: %ld\n",dp);
  printf("\n");
  printf("\nStatistics:\n");
  printf("  Static String Size: %6ld    Syntbl Size: %6ld\n",
	 ss_end,((long)synptr)*sizeof(word));
  if (maxcreat>=first_creat) creatcnt=maxcreat-first_creat+1;
     else creatcnt=0;
  printf("  Data size-- Room: %5ld   Noun: %5ld    Creat: %5ld\n",
       sizeof(room_rec)*(long)(maxroom-first_room+1), 
	 sizeof(noun_rec)*(long)(maxnoun-first_noun+1),
	 sizeof(creat_rec)*(long)creatcnt);
  tmp=cmdsize();
  printf("      Commands: Headers: %5ld   Text: %5ld     Total: %5ld\n",
	 sizeof(cmd_rec)*last_cmd,tmp,sizeof(cmd_rec)*last_cmd+tmp);
  printf("      Dictionary:  Dict: %5ld       Text: %5ld(%5ld)    Tot: %5ld\n",
	 ((long)dp)*sizeof(char*),
	 dictstrptr,dictstrsize,dp*sizeof(char*)+dictstrptr);
  printf("\n");
  printf("  Num of Objs-- Room: %3d/%3d   Noun: %3d/%3d  Creat: %3d/%3d\n",
	 maxroom-first_room+1, first_noun-first_room,
	 maxnoun-first_noun+1, first_creat-first_noun, 
	 creatcnt, last_obj-first_creat+1);
  printf("      Message: %3ld/%3ld    Cmd: %4ld/%4ld\n",
	 last_message,msgcnt(ver,aver), last_cmd,cmdcnt(ver,aver));  
  printf("\n");
  printf("Settings:\n"); 
  printopt(NULL,0);
  printopt("INTRO_FIRST",intro_first);
  printopt("SLASH_BOLD",bold_mode);
  printopt("PURE_ANSWER",PURE_ANSWER);
  nprintopt("CONST_TIME",  PURE_TIME);
  nprintopt("ROOMTITLE",  PURE_ROOMTITLE);
  nprintopt("FIX_MULTINOUN",PURE_AND);
  nprintopt("FIX_METAVERB",PURE_METAVERB);
  nprintopt("CHECK_GRAMMAR",PURE_GRAMMAR);
  /*
    printopt( PURE_SYN);
    printopt( PURE_NOUN);
    printopt( PURE_ADJ);
    */
  printopt("PURE_DUMMY",PURE_DUMMY);
  printopt("PURE_SUBNAME",PURE_SUBNAME);
  nprintopt("PRONOUN_SUBS",PURE_PROSUB);
  printopt("BLOCK_HOSTILE",PURE_HOSTILE);
  printopt("GET_HOSTILE",PURE_GETHOSTILE);
  nprintopt("SMART_DISAMBIG",PURE_DISAMBIG);
  nprintopt("EXPAND_ALL",PURE_ALL);
  printopt("IRUN",irun_mode);
  printopt("VERBOSE",verboseflag);
  printf("\n");
}



/* Doprint=0 ==>don't print, = 1: do print unless size 0,
   =2: print regardless */
void print_descr(char *name,descr_ptr dp, int doprint)
{
  int j;
  descr_line *txt;

  if (doprint==0) return;
  if (doprint==1 && dp.size==0) return;
  
  printf("%s (%ld,%ld)\n",name,dp.start,dp.size);
  if (agx_file && !AGX_DESC) return;
  txt=read_descr(dp.start,dp.size);
  if (txt!=NULL)
    for(j=0;txt[j]!=NULL;j++)
      printf("%s\n",txt[j]);
  free_descr(txt);
}


void dump_attr(int t, int objnum)
     /* t=0 for room, 1 for noun, 2 for creature */
     /* objnum is the object index */
{
  int i, line;
  char ofs;

  if (num_oattrs(t,0)>0) { /* List flags */
    printf("  FLAGS:\n");
    line=0;
    for(i=0;i<oflag_cnt;i++) 
      if (lookup_objflag(i,t,&ofs)!=-1) {
	if (line==0) printf("    ");
	printf("Flag %2d: %s     ",i,op_objflag(2,objnum,i)?"Yes":"No ");
	if (++line==4) {
	  printf("\n");
	  line=0;
	}
      }
    if (line!=0) printf("\n");
  }
  if (num_oattrs(t,1)>0) { /* List properties */
    printf("  PROPS:\n");
    line=0;
    for(i=0;i<oprop_cnt;i++)
      if (lookup_objprop(i,t)!=-1) {
	if (line==0) printf("    ");
	printf("Prop %2d=%6ld       ",i,op_objprop(2,objnum,i,0));
	if (++line==2) {
	  printf("\n");
	  line=0;
	}
      }
    if (line!=0) printf("\n");
  }
}
  


void print_slist(slist sp)
{
  while(syntbl[sp]!=0) {
    printf("%s",dict[syntbl[sp++]]);
    if (syntbl[sp]!=0) printf(", ");
  }
}

rbool badname(uchar *s)
{
  if (aver>=AGTME10 || !no_badobj) 
    return 0; /* ME games shouldn't have gaps. */
  for(;*s!=0;s++)
    if (*s<32) return 1;
  return 0;
}

 
void rooms_out(void)
{
  long i,j;

  printf("\nRoom List:");
  for(i=0;i<maxroom-first_room+1;i++)
    {
      /* if (badname((uchar*)room[i].name)) continue;*/
      if (room[i].unused) continue;
      printf("\nROOM %ld:%-15s",i+first_room,room[i].name);
      if (room[i].oclass!=0) 
	printf("  (Class:%d)",room[i].oclass);
      printf("\n  ATTR:");
      if (room[i].seen)
	printf(" seen ");
      if (room[i].locked_door)
	printf(" lck door ");
      if (room[i].win) printf(" WIN ");
      if (room[i].end) printf(" END ");
      if (room[i].killplayer) printf(" DIE ");
      printf("\n");
      printf("  Key=%d   Light=%d   Points=%d",
	     room[i].key,room[i].light,room[i].points);
      if (room_inside!=NULL)
	printf("   #Nouns=%d\n",room_inside[i]);
      else printf("\n");
      for(j=0;j<12;j++)
	printf("  %s:%d",exitname[j],room[i].path[j]);
      printf("\n  SPECIAL:%d\n",room[i].path[12]);
      printf("  SYN %s=",dict[room[i].replace_word]);
      print_slist(room[i].replacing_word);
      printf("\n");
      if (ver==3) {
	printf("  Initial=%d   Picture=%d    Flags=%08lx  PIX=%08lx\n",
	       room[i].initdesc,room[i].pict,room[i].flag_noun_bits,
	       room[i].PIX_bits);
	printf("  Autoexec=%s\n",dict[room[i].autoverb]);
      }
      dump_attr(0,i+first_room);
      if (verbose_dump) {
	print_descr("DESCR:",room_ptr[i],2);
	print_descr("HELP:",help_ptr[i],1);
	print_descr("SPECIAL:",special_ptr[i],1);
      }
    }
}

void nouns_out(void)
{
  long i;

  printf("\n\nNoun List:");
  for(i=0;i<maxnoun-first_noun+1;i++)
    {
      /*      if (badname((uchar*)noun[i].shortdesc)) continue; */
      if (noun[i].unused) continue;
      printf("\n\nNOUN %ld:%s %s",i+first_noun,dict[noun[i].adj],
	     dict[noun[i].name]);
      if (noun[i].oclass!=0) 
	printf("   (Class:%d)",noun[i].oclass);
      if (noun[i].flagnum!=0) 
	printf("  [flag %d]",noun[i].flagnum);
      printf("\n%s\n",noun[i].shortdesc);
      printf("  Pos:%s\n",(noun[i].position==NULL)?"<none>":noun[i].position);
      printf("  Location:%d (behind %d)   Points:%d",
	     noun[i].location,noun[i].nearby_noun,noun[i].points);
      if (noun_inside!=NULL)
	printf("   #Nouns=%d\n",noun_inside[i]);
      else printf("\n");
      printf("  Wt=%d  Size=%d  Key:%d   #Shots=%d\n",
	     noun[i].weight,noun[i].size,noun[i].key,noun[i].num_shots);
      printf("  ATTR:");
      if (noun[i].plural) printf(" pl ");
	else printf(" sing ");
      if (noun[i].something_pos_near_noun) printf(" in_front ");
      if (noun[i].win) printf(" WIN ");
      if (noun[i].has_syns) printf(" syns ");
      printf("\n  CAN:");
      if (noun[i].pushable) printf(" push ");
      if (noun[i].pullable) printf(" pull ");
      if (noun[i].turnable) printf(" turn ");
      if (noun[i].playable) printf(" play ");
      if (noun[i].readable) printf(" read ");
      if (noun[i].closable) printf(" close ");
      if (noun[i].lockable) printf(" lock ");
      if (noun[i].edible) printf(" eat ");
      if (noun[i].wearable) printf(" wear ");
      if (noun[i].drinkable) printf(" drink ");
      if (noun[i].movable) printf(" move ");
      printf("\n  ATT2:");
      if (noun[i].on) printf(" on ");
      if (noun[i].open) printf(" open ");
      if (noun[i].locked) printf(" locked ");
      if (noun[i].poisonous) printf(" poison ");
      if (noun[i].light) printf(" light ");
      if (noun[i].shootable) printf(" shoot ");
      if (noun[i].isglobal) printf(" global ");
      printf("\n");
      if (noun[i].has_syns) {
	  printf("  Syns:");
	  print_slist(noun[i].syns);printf("\n");
	}
      if (ver==3)
	printf("  Initial=%d    Picture=%d    Related->%d\n",
	       noun[i].initdesc,noun[i].pict,noun[i].related_name);
      dump_attr(1,i+first_noun);
      if (verbose_dump) {
	print_descr("DESCR:",noun_ptr[i],2);
	print_descr("TEXT:",text_ptr[i],noun[i].readable);
	print_descr("PUSH:",push_ptr[i],noun[i].pushable);
	print_descr("PULL:",pull_ptr[i],noun[i].pullable);
	print_descr("TURN:",turn_ptr[i],noun[i].turnable);
	print_descr("PLAY:",play_ptr[i],noun[i].playable);
      }
    }
}

void creat_out(void)
{
  int i;
  printf("\n\nCreature List:");
  for(i=0;i<maxcreat-first_creat+1;i++)
    {
      /*      if (badname((uchar*)creature[i].shortdesc)) continue;*/
      if (creature[i].unused) continue;
      printf("\nCREATURE: %d:%s %s",i+first_creat,
	     dict[creature[i].adj],dict[creature[i].name]);
      if (creature[i].oclass!=0) 
	printf("  (Class:%d)",creature[i].oclass);
      if (creature[i].flagnum!=0) 
	printf("  [flag %d]",creature[i].flagnum);
      printf("\n%s\n",creature[i].shortdesc);
      printf("  ATTR:");
      if (creature[i].groupmemb) printf(" grp ");
      if (creature[i].hostile) printf(" hostile ");
      if (creature[i].gender==0) printf(" thing ");
      else if (creature[i].gender==1) printf(" woman ");
      else printf(" man ");
      if (creature[i].has_syns) printf(" syns ");
      if (creature[i].isglobal) printf(" global ");
      printf("\n");
      printf("  Loc=%d   Weap=%d   Points=%d",
	     creature[i].location,creature[i].weapon,creature[i].points);
      if (creat_inside!=NULL)
	printf("   #Nouns=%d\n",creat_inside[i]);
      else printf("\n");
      printf("  Thres=%d(%d)  TimeThres=%d(%d)\n",
	     creature[i].threshold,creature[i].counter,creature[i].timethresh,
	     creature[i].timecounter);
      if (creature[i].has_syns) {
	printf("  Syns:");
	print_slist(creature[i].syns);printf("\n");
      }
      if (ver==3) printf("  Initial=%d    Pict=%d\n",
			 creature[i].initdesc,creature[i].pict);
      dump_attr(2,i+first_creat);
      if (verbose_dump) {
	print_descr("DESCR:",creat_ptr[i],2);
	print_descr("TALK:",talk_ptr[i],1);
	print_descr("ASK:",ask_ptr[i],1);
      }
    }
}

static void fake_noun_out(void) 
{
  int i;

  if (globalnoun!=NULL && numglobal>0) {
    printf("\n\nGLOBAL NOUNS:\n");
    for(i=0;i<numglobal;i++)
      printf("  %s\n",dict[globalnoun[i]]);
  }
  for(i=0;i<MAX_FLAG_NOUN && flag_noun[i]==0;i++);
  if (i<MAX_FLAG_NOUN) {
    printf("\n\nFLAG NOUNS:\n");
    for(;i<MAX_FLAG_NOUN;i++)
      if (flag_noun[i]!=0) 
	printf("  %2d: %-25s(%08lx)\n",i,dict[flag_noun[i]],1L<<i);
  }
}


static void print_message_range(int msg1,int msg2)
{
  int i;
  if (msg1<1) return;
  if (msg1>last_message) return;
  for(i=msg1;i<=msg2;i++) {
    printf("\n\t");
    argout(AGT_MSG,i,0);
  }
}

void cmd_codes_out(integer *clist,int cnt)
{
  int ip,j;
  const opdef *opdata;
  char endflag;
  char save_dbg_nomsg;
  int op, optype;

  ip=0;endflag=0;
  save_dbg_nomsg=0;  /* Just to silence compiler warnings */
  while(ip<cnt && !endflag) {
    printf("  %2d:",ip);
    op=clist[ip]%2048;
    optype=clist[ip]/2048;
    opdata=get_opdef(op);
    ip++;
    if (opdata==&illegal_def) 
      printf("ILLEGAL %d\n",op);
    else {
      if (op>=END_ACT) { /* End codes */
	 /* Magx games support Goto commands, so we can't stop just
	    because we hit an end code-- it might be possible to jump
	    beyond it. */
	 if (aver<AGX00) endflag=1;  
	 printf("!");
      }
      else if (op<=MAX_COND) printf("?");
      printf("%s",opdata->opcode);
      if (op==1063 && !quick_rndmsg) {
	save_dbg_nomsg=dbg_nomsg;
	dbg_nomsg=1;
      }
      for(j=0;j<opdata->argnum;j++) {
	printf("\t");
	if (ip<cnt || (optype&4))
	    ip+=argout(j==0 ? opdata->arg1 : opdata->arg2 , 
		       ip<cnt ? clist[ip] : 0, optype>>2);
	optype=optype<<2;
      }
      if (op==1063 && !quick_rndmsg) { 
	dbg_nomsg=save_dbg_nomsg;
	if (optype==0 && !dbg_nomsg && dbg_msgrange && ip>=2)
	  print_message_range(clist[ip-2],clist[ip-1]);
      }
      printf("\n");
    }
  }
}



void raw_cmd_codes_out(integer *clist,int cnt)
{
  int j,col;

  printf("    "); col=5;
  for(j=0;j<cnt;j++)
    {
      if (col>70) {
	if (j>200) break;
	printf("\n    ");
	col=5;
      }
      printf("  %04d",clist[j]);col+=6;
    }
  printf("\n");
  if (j!=cnt)
    printf("  .... <Continues for %d more tokens> ....\n",cnt-j);
}

/* This is called from the disassembler */
void print_tos(void)
{
  printf("TOS"); /* Reference to the Top of Stack */
}

void commands_out(void)
{
  int i, v, a;
  word w;
  rbool noncanon; /* Header verb-code != verb-word */

  printf("\nCommands:\n");
  if (last_cmd==0) {
    printf("<None>\n");
    return;
  }
  for(i=0;i<last_cmd;i++)
    {
  /* Decode dummy verbs */
      v=verb_code(command[i].verbcmd);
      w=command[i].verbcmd;
      noncanon=(w!=syntbl[auxsyn[v]]);
      if (noncanon && v!=0) {
	printf("NONCANONICAL HEADER:");
	printf("  %d:%s instead of %d:%s\n",w,dict[w],syntbl[auxsyn[v]],
	       dict[syntbl[auxsyn[v]]]);
      }
      if (v>=BASE_VERB && v<BASE_VERB+DUMB_VERB && syntbl[synlist[v]]!=0)
	w=syntbl[synlist[v]];  /* Replace dummy verbs with their first 
				  synonyms */

      if (command[i].actor>0) {
	printf("CMD %d: ",i);
	a=command[i].actor;
      } else {
	printf("REDIR: ");
	a=-command[i].actor;
      }
      if (a==2) 
	printf("anybody, ");
      else if (a>2) {
	char *name;
	name=objname(a);
	name[0]=toupper(name[0]);
	printf("%s, ",name);
	rfree(name);
      }
      
      /* Now to print out the actually command to be matched. */
      printf("%s",dict[w]);
      if (noncanon) printf("[%d]",v);
      printf(" ");
      if (command[i].noun_adj!=0)
	printf("%s ",gdict(command[i].noun_adj) );
      printf("%s",gdict(command[i].nouncmd));
      if (command[i].noun_obj!=0) printf("[%d]",command[i].noun_obj);
      printf(" %s ",(ver==3) ? gdict(command[i].prep) : "->");
      if (command[i].obj_adj!=0)
	printf("%s ",gdict(command[i].obj_adj) );
      printf("%s",gdict(command[i].objcmd) );
      if (command[i].obj_obj!=0) printf("[%d]",command[i].obj_obj);

      if (ver==3 ||agx_file) printf(" ===> %lx (%lx) [%d]\n",cmd_ptr[i],
			 cmd_ptr[i]*2-2,command[i].cmdsize);
      else
	printf("\n");
      if (!RAW_CMD_OUT)
	cmd_codes_out(command[i].data,command[i].cmdsize);
      else
	raw_cmd_codes_out(command[i].data,command[i].cmdsize);
    }
}


void strings_out(void)
{
  int i;
  printf("\n\nStrings:\n");
  if (userstr!=NULL)
    for(i=0;i<MAX_USTR;i++)
      printf("%d:%s\n",i+1,userstr[i]);
  else printf(" <Not defined in this version of AGT>\n");
  printf("\n");
}

void msg_out(void)
{
  int i;
  char s[50];

  printf("\n\nMessages:\n");
  for(i=0;i<last_message;i++) {
    sprintf(s,"MSG %d",i+1);
    print_descr(s,msg_ptr[i],2);
  }
}


void stdmsg_out(void)
{
  int i;
  char s[50];

  printf("\n\nStandard Error Messages:\n");
  if (err_ptr==NULL) {
    printf("<Not defined in this version of AGT>\n");
    return;
  }
  for(i=0;i<NUM_ERR;i++) {
    sprintf(s,"STD MSG %d",i+1);
    print_descr(s,err_ptr[i],1);
  }
}

void syn_out(void)
{
  int i;
  int num_verb;

  num_verb=BASE_VERB+DVERB;
  if (aver>=AGTME10) num_verb+=MAX_SUB;

  build_verblist();

  printf("\nSynonyms:\n");
  for(i=0;i<num_verb;i++) {
    printf("%s--",verblist[i]);
    print_slist(synlist[i]);
    printf("\n");
  }
}

void prep_out(void)
{
  int i;
  if (num_prep==0) return;
  printf("\nPrepositions:\n");
  for(i=0;i<num_prep;i++) {
    printf("%s ( ",dict[syntbl[userprep[i]]]);
    print_slist(userprep[i]+1);
    printf(" )\n");
  }
}

void dict_out(void)
{
  int i;

  printf("\n\nDICTIONARY:\n");
  for(i=0;i<dp;i++)
    printf("%d: %s\n",i,dict[i]);
}


void propstr_out(long ptr, long cnt)
{
  int i;
  printf("%ld[%ld]",ptr,cnt);
  if (cnt==0) return;
  printf(": ");
  for(i=0;i<cnt;i++)
    printf("\"%s\"  ",propstr[ptr+i]);
}


void propetc_out(void)
{
  int i;
  if (proptable) {
    printf("\n\nObjProps:\n");
    for(i=0;i<oprop_cnt;i++) {
      printf("%2d:  ",i);
      propstr_out(proptable[i].str_list,proptable[i].str_cnt);
      printf("\n");
    }
  }
  if (attrtable) {
    printf("\n\nObjFlags:\n");
    for(i=0;i<oflag_cnt;i++) {
      printf("%2d:   ",i);
      printf("\"%s\" / \"%s\"",attrtable[i].ystr,attrtable[i].nstr);
      printf("\n");
    }
  }
  if (vartable) {
    printf("\n\nItemized Variables:\n");
    for(i=0;i<=VAR_NUM;i++) {
      printf("%3d: ",i);
      if (vartable[i].str_cnt>0) {
	propstr_out(vartable[i].str_list,vartable[i].str_cnt);
	printf("\n");
      }
    }
  }
  if (flagtable) {
    printf("\n\nItemized Flags:\n");
    for(i=0;i<=FLAG_NUM;i++) {
      printf("%3d: ",i);
      if (flagtable[i].ystr!=NULL || flagtable[i].nstr!=NULL) {
	printf("\"%s\" / \"%s\"",flagtable[i].ystr,flagtable[i].nstr);
	printf("\n");
      }
    }
  }
}




rbool end_cmd_options;

void helpmsg(void)
{
  printf("Syntax: agtout <options> <game name>\n");
  printf("Options:\n");
  printf("(all of these can be turned off by putting a - after the option)\n");
  printf("-v Verbose: equivalent to -dmia\n");
  printf("-i Print basic diagnostic information\n");
  printf("-d Print out dictionary\n");
  printf("-m Dump messages\n");
  printf("-o Try to avoid printing 'undefined' objects.\n");
  printf("-s Show all possible messages printed out by RandomMessage.\n");
  printf("-q Quick RandomMessage printout (only show actual arguments).\n");
  printf("-u Leave commands unsorted.\n");
  printf("-x Examine .DA1 file as it is read (implies -i).\n");
  printf("-a Interpret arguments to metacommands (implies -r-).\n");
  printf("-r Raw command out: don't disassemble metacommands (implies -a-)\n");
}

static rbool setarg(char **optptr)
{
  if ( (*optptr)[1]=='+') {(*optptr)++;return 1;}
  if ( (*optptr)[1]=='-') {(*optptr)++;return 0;}
  return 1;
}

void parse_options(char *opt,char *next)
{
  if (opt[0]=='-' && opt[1]==0)   /* -- */
    {end_cmd_options=1;return;}
  for(;*opt!=0;opt++)
    switch(tolower(*opt))
      {
      case 'v': 
	printdict=1; printmsg=1; DIAG=1;
	interp_arg=1;
	break;
      case '?': case 'h':
	  helpmsg();
	  exit(EXIT_SUCCESS);
      case 'd': printdict=setarg(&opt);break;
      case 's': dbg_msgrange=setarg(&opt);break;
      case 'q': quick_rndmsg=setarg(&opt);break;
      case 'm': printmsg=setarg(&opt);break;
      case 'i': DIAG=setarg(&opt);break;
      case 'o': no_badobj=setarg(&opt); break;
      case 'a': interp_arg=setarg(&opt);
	  if (interp_arg)  RAW_CMD_OUT=0;
	  break;
      case 'x': debug_da1=setarg(&opt);
	  if (debug_da1) DIAG=1;
	  break;
	case 'r': RAW_CMD_OUT=setarg(&opt);
	  if (RAW_CMD_OUT) interp_arg=0;
	  break;
	case 'u': sortflag=!setarg(&opt);break;
#ifdef OPEN_FILE_AS_TEXT
      case 'b': open_as_binary=setarg(&opt);break;
#endif
	default:printf("Do not recognize option %c\n",*opt);
	  helpmsg();
	  exit(EXIT_FAILURE);
      }
}


int main(int argc,char *argv[])
{
  int i;
  char *gamefile;
  fc_type fc;

  end_cmd_options=0;

  init_flags();
  debug_mode=1;  /* Unless something turns it off explictly */
  build_trans_ascii();

  printmsg=printdict=0;
  sortflag=1;
  dbg_msgrange=0; /* Don't print out message range for RandomMessage */
  quick_rndmsg=0; /* Don't print out arguments to RandomMessage 
		   (except perhaps the range above) */

  printf("AGTOut: Prints contents of AGT files in human readable form\n");
  printf("%s\n",version_str);
  printf("  Copyright (C) 1996-1999,2001 Robert Masenten\n");
  printf("[%s]\n\n",portstr);

  gamefile=NULL;
  for(i=1;i<argc;i++)
    if (argv[i][0]=='-' && !end_cmd_options)
      parse_options(argv[i]+1,argv[i+1]);
    else if (gamefile==NULL)
      gamefile=argv[i];
    else {helpmsg();exit(EXIT_FAILURE);}
  if (gamefile==NULL)
    {helpmsg();exit(EXIT_FAILURE);}

  fc=init_file_context(gamefile,fDA1);
  if (!read_agx(fc,1) && !readagt(fc,1))
    fatal("Unable to open game file.");
#ifndef DEVEL_TEST
  if (!debug_mode) 
    fatal("Disassembly blocked by game author.");
#endif

  if (sortflag && !agx_file) sort_cmd();
  if (!agx_file)
    open_descr(fc);
  info_out();
  rooms_out();
  nouns_out();
  creat_out();
  fake_noun_out(); /* Global and flag nouns */
  printf("\n\n");

  print_descr("INTRO:",intro_ptr,2);
  commands_out();
  if (printmsg) msg_out();
  syn_out();
  prep_out();
  propetc_out();
  strings_out();
  if (printmsg) stdmsg_out();

  if (agx_file)
    agx_close_descr();
  else
    close_descr();

  if (printdict) dict_out();
  free_all_agtread();
  return 0;
}

