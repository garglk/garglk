/* AGT2AGX, AGT to AGX file converter                        */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                           */
/* This is part of the source for AGiliTy, the (Mostly) Universal */
/*       AGT Interpreter                                          */

#include <ctype.h>

#define global
#include "agility.h"



static long cntvar, cntflag, cntcnt, cntquest, cntstr, cntsub;
/* We can't count messages because they can be referenced by variable */

#define m(cnt) do {if (arg>cnt) cnt=arg; return;} while(0) 

void check_arg(integer argtype, integer arg)
{
  if (argtype & AGT_VAR) argtype=AGT_VAR;
  switch(argtype) 
    {
    case AGT_VAR:m(cntvar);
    case AGT_FLAG:m(cntflag);
    case AGT_CNT:m(cntcnt);
    case AGT_QUEST:m(cntquest);
    case AGT_STR:m(cntstr);
    case AGT_SUB:m(cntsub);
#if 0  /* These are already taken care of AFAIK */
    case AGT_PIC:m(cntpic);
    case AGT_PIX:m(cntpix);
    case AGT_FONT:m(cntfont);
    case AGT_SONG:m(cntsong);
    case AGT_ROOMFLAG:m(cntroomflag);
#endif
    default:break;
    }
}

#undef m

/* AGT stores a lot of junk in the command blocks. This shortens the 
   command blocks to their real length (Determined by scanning for
   end markers). */
/* This also analyzes commands to determine the maximum
   variable,flag,counter,question,and userstr used. */
void truncate_cmd(void)
{
  int i, j;
  const opdef *opdata;

  cntvar=cntflag=cntcnt=cntquest=cntstr=cntsub=0;

  for(i=0;i<last_cmd;i++) {
    j=0;
    while(j<command[i].cmdsize) {
      if (command[i].data[j]>=END_ACT) break;
      opdata=get_opdef(command[i].data[j++]);
      if (opdata==&illegal_def) 
	agtwarn("Illegal opcode found.",1);

      if (j+opdata->argnum>command[i].cmdsize || 
	  opdata==&illegal_def) {
	j=command[i].cmdsize; /* To prevent cmdsize from being changed */
	if (!(opdata==&illegal_def)) 
	  agtwarn("Command block overrun",0);
	break;
      }
      /* Note: j now points to the token *after* the opcode */
      if (opdata->argnum>=1) {
	check_arg(opdata->arg1,command[i].data[j++]);
	if (opdata->argnum>=2) 
	  check_arg(opdata->arg2,command[i].data[j++]);
      }
    }
    if (j<command[i].cmdsize)
      command[i].cmdsize=j+1;
  }
  if (VAR_NUM>cntvar) VAR_NUM=cntvar;
  if (FLAG_NUM>cntflag) FLAG_NUM=cntflag; 
  if (CNT_NUM>cntcnt) CNT_NUM=cntcnt;
  if (MAX_USTR>cntstr) MAX_USTR=cntstr;
  if (MAX_SUB>cntsub) MAX_SUB=cntsub;
  if (MaxQuestion>cntquest) MaxQuestion=cntquest;
}


/* This transfers <num> descriptions from an AGT D$$ file to our AGX file */
/* Note that this overwrites the descr_ptr it is sent */
void copydesc(descr_ptr *descp,int num)
{
  int i;
  descr_line *txt;

  if (num<=0 || descp==NULL) return;
  for(i=0;i<num;i++) {
    txt=read_descr(descp[i].start,descp[i].size);
    write_descr(&descp[i],txt);
    free_descr(txt);
  }
}

/* Convert a tline array into an array of description pointers,
   writing the line out to a file */
descr_ptr *convert_qa(tline *qa)
{
  int i;
  descr_ptr *qa_ptr;
  descr_line txt[2];

  qa_ptr=rmalloc(sizeof(descr_ptr)*MaxQuestion);
  txt[1]=NULL;
  if (qa!=NULL) 
    for(i=0;i<MaxQuestion;i++) {
      txt[0]=qa[i];
      write_descr(&qa_ptr[i],txt);
    }
  else 
    for(i=0;i<MaxQuestion;i++) {
      qa_ptr[i].start=0;
      qa_ptr[i].size=0;
    }
  return qa_ptr;
}

/* This converts titles and instructions to descriptions and
   writes them to the AGX file */
static void copy_misc(fc_type fc)
{
  descr_line *txt;
  
  txt=read_ttl(fc);
  write_descr(&title_ptr,txt);
  free_descr(txt);

  txt=read_ins(fc);
  write_descr(&ins_ptr,txt);
  free_ins(txt);
}


/* Routine to write descriptions to AGX file */
void descr_out(fc_type fc)
{
  open_descr(fc);
  copydesc(&intro_ptr,1);
  copydesc(err_ptr,NUM_ERR);
  copydesc(msg_ptr,last_message);
  copydesc(help_ptr,maxroom-first_room+1);
  copydesc(room_ptr,maxroom-first_room+1);
  copydesc(special_ptr,maxroom-first_room+1);
  copydesc(noun_ptr,maxnoun-first_noun+1);
  copydesc(text_ptr,maxnoun-first_noun+1);
  copydesc(turn_ptr,maxnoun-first_noun+1);
  copydesc(push_ptr,maxnoun-first_noun+1);
  copydesc(pull_ptr,maxnoun-first_noun+1);
  copydesc(play_ptr,maxnoun-first_noun+1);
  copydesc(talk_ptr,maxcreat-first_creat+1);
  copydesc(ask_ptr,maxcreat-first_creat+1);
  copydesc(creat_ptr,maxcreat-first_creat+1);

  if (quest_ptr!=NULL) copydesc(quest_ptr,MaxQuestion);
  else quest_ptr=convert_qa(question);

  if (ans_ptr!=NULL) copydesc(ans_ptr,MaxQuestion);
  else ans_ptr=convert_qa(answer);

  close_descr(); 

  copy_misc(fc);
}



rbool end_cmd_options;

void helpmsg(void)
{
  printf("Syntax: agt2agx <options> <game name>\n");
  printf("Options:\n");
  printf("(all of these can be turned off by putting a - after the option)\n");
  printf("-h  Print this help message.\n");
  printf("-o <filename>  Specify output file root.\n");
#ifdef OPEN_AS_TEXT
  printf(" -b Open data files as binary files.\n");
#endif
}

char *outname; /* Name of output file */

int parse_options(char *opt,char *next)
{
  if (opt[0]=='-' && opt[1]==0)   /* -- */
    {end_cmd_options=1;return 0;}
  for(;*opt!=0;opt++)
    switch(tolower(*opt))
      {
      case '?': case 'h':
	helpmsg();
	exit(EXIT_SUCCESS);
      case 'i': 
	if (opt[1]=='-') {DIAG=0;opt++;}
	else if (opt[1]=='+') {DIAG=1;opt++;}
	else DIAG=1;
      break;
#ifdef OPEN_FILE_AS_TEXT
      case 'b': 
	if (opt[1]=='-') {open_as_binary=0;opt++;}
	else if (opt[1]=='+') {open_as_binary=1;opt++;}
	else open_as_binary=1;
	break;
#endif
      case 'o':
	if (opt[1]!=0 || next!=NULL) {
	  if (opt[1]!=0) outname=opt+1;
	  else outname=next;
	  return (opt[1]==0); /* Skip next argument if opt[1]==0 */
	} else {
	  printf("-o requires a file name\n");
	  exit(EXIT_FAILURE);
	} 
      default:printf("Do not recognize option %c\n",*opt);
	helpmsg();
	exit(EXIT_FAILURE);
      }
  return 0;
}


int main(int argc,char *argv[])
{
  int i;
  char *gamefile;
  fc_type fc, fc_out;

  end_cmd_options=0;

  init_flags();

  no_auxsyn=1;
  fix_ascii_flag=0;  /* We don't want to translate the character
			set when we're converting files */

  /* Mark all of the purity settings as "undecided" */
  PURE_ANSWER=PURE_TIME=PURE_ROOMTITLE=2;
  PURE_AND=PURE_METAVERB=PURE_SYN=PURE_NOUN=PURE_ADJ=2;
  PURE_DUMMY=PURE_SUBNAME=PURE_PROSUB=PURE_HOSTILE=2;
  PURE_GETHOSTILE=PURE_DISAMBIG=PURE_ALL=2;
  irun_mode=verboseflag=2;

  build_trans_ascii();

  printf("agt2agx: Convert AGT game files into AGX files\n");
  printf("%s\n",version_str);
  printf("  Copyright (C) 1996-1999,2001 Robert Masenten\n");
  printf("[%s]\n\n",portstr);

  outname=gamefile=NULL;
  for(i=1;i<argc;i++)
    if (argv[i][0]=='-' && !end_cmd_options)
      i+=parse_options(argv[i]+1,argv[i+1]);
    else if (gamefile==NULL)
      gamefile=argv[i];
    else {helpmsg();exit(EXIT_FAILURE);}
  if (gamefile==NULL)
    {helpmsg();exit(EXIT_FAILURE);}
  if (outname==NULL)
    outname=gamefile; /* By default use gamefile name for output name */

  fc=init_file_context(gamefile,fDA1);
  fc_out=init_file_context(outname,fAGX);
  text_file=1;
  read_config(openfile(fc,fCFG,NULL,0),0);
  text_file=0;
  printf("Reading AGT file...\n");
  if (!readagt(fc,0))
    fatal("Unable to open info (DA1) or AGX file.");
  text_file=1;
  read_config(openfile(fc,fCFG,NULL,0),1);
  text_file=0;

  sort_cmd();
  truncate_cmd();

  printf("Opening AGX file and copying description text...\n");
  agx_create(fc_out);
  descr_out(fc);
  printf("Writing AGX file...\n");
  agx_write();
  agx_wclose();

  free_all_agtread();

  return 0;
}
