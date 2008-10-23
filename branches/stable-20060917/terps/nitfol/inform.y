%{
/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at ecr+@andrew.cmu.edu
*/

#include "nitfol.h"
#include <ctype.h>

/* bison uses str* functions; make it use n_str* instead... */
#ifndef n_strcat
#define strcat(d, s) n_strcat(d, s)
#endif
#ifndef n_strlen
#define strlen(s) n_strlen(s)
#endif
#ifndef n_strcpy
#define strcpy(d, s) n_strcpy(d, s)
#endif
  
  
#ifdef DEBUGGING
  
  typedef struct zword_list zword_list;
  struct zword_list {
    zword_list *next;
    zword item;
  };

  typedef struct cond_list cond_list;
  struct cond_list {
    cond_list *next;
    zword val;
    BOOL (*condfunc)(zword a, zword b);
    BOOL opposite;
  };

  cond_list *condlist;
  
  static z_typed z_t(z_typed a, z_typed b, zword v);
  
  static const char *lex_expression;
  static int lex_offset;

  static const char *lex_tail(void) {
    const char *t = lex_expression + lex_offset;
    while(*t == ' ')
      t++;
    lex_offset = n_strlen(lex_expression);
    return t;
  }
  
  static z_typed inform_result;
  
  static int yylex(void);
  static void yyerror(const char *s);
  static void inform_help(void);
  
  int ignoreeffects;

#define YYERROR_VERBOSE
  
/*
#define YYDEBUG 1
*/

%}

%union {
  glui32 pcoffset;
  infix_file *filenum;
  z_typed val;
  
  zword_list *zlist;

  struct {
    BOOL (*condfunc)(zword a, zword b);
    BOOL opposite;
  } cond;

  BOOL flag;
}

%token	<val>		NUM
%token	<filenum>	DFILE
%token	<cond>		CONDITION
%type	<val>		commaexp
%type	<val>		condexp
%type	<val>		exp
%type	<pcoffset>	linespec
%type	<zlist>		arglist
%type	<flag>		orlist



%token ALIAS RALIAS UNALIAS DUMPMEM AUTOMAP HELP UNDO REDO LANGUAGE INFOSOURCE INFOSOURCES COPYING WARRANTY PRINT SET MOVE TO GIVE REMOVE JUMP CONT STEP NEXT UNTIL STEPI NEXTI FINISH BREAK DELETE IF COND IGNORE BREAKPOINTS RESTORE RESTART QUIT RECORDON RECORDOFF REPLAY REPLAYOFF SYMBOL_FILE FRAME SELECT_FRAME BACKTRACE UP_FRAME DOWN_FRAME UP_SILENTLY DOWN_SILENTLY DISPLAY UNDISPLAY DISABLE_DISPLAY ENABLE_DISPLAY DISABLE_BREAK ENABLE_BREAK OBJECT_TREE FIND LIST_GLOBALS BTRUE BFALSE NOTHING PARENT CHILD SIBLING CHILDREN RANDOM

%left ','
%right '='
%left ANDAND OROR NOTNOT
%nonassoc CONDITION
%left OR
%left '+' '-'
%left '*' '/' '%' '&' '|' '~'
%left BYTEARRAY WORDARRAY
%nonassoc precNEG
%nonassoc NUMBER OBJECT ROUTINE STRING GLOBAL LOCAL
%nonassoc INCREMENT DECREMENT
%left PROPADDR PROPLENGTH
%left '('   /* function calls */
%left '.'
%left SUPERCLASS

%%
input:	  /* empty string */
/* :: Enter a comment */
	| '#' /* comment */	{ lex_offset = n_strlen(lex_expression); }
/* :: Dump memory to a file */
	| DUMPMEM /*FILE*/	{
		strid_t f;
		f = n_file_name_or_prompt(fileusage_Data|fileusage_BinaryMode,
					  filemode_Write, lex_tail());
		w_glk_put_buffer_stream(f, (char *) z_memory, total_size);
		glk_stream_close(f, NULL);
	}
/* :: Add an alias */
	| ALIAS /*@var{name} @var{value}*/ { parse_new_alias(lex_tail(), FALSE); }
/* :: Add a recursive alias */
	| RALIAS /*@var{name} @var{value}*/ { parse_new_alias(lex_tail(), TRUE); }
/* :: Remove an alias */
	| UNALIAS /*@var{name}*/{ remove_alias(lex_tail()); }
/* :: Start automapping */
	| AUTOMAP /*commaexp*/	{ automap_init(object_count, lex_tail()); }
/* :: Print list of commands. */
	| HELP			{ inform_help(); }
/* :: Restart the game. */
	| RESTART		{ op_restart(); exit_debugger = TRUE; read_abort = TRUE;  }
/* :: Restore a saved game. */
	| RESTORE		{
		if(restoregame()) {
		  exit_debugger = TRUE; read_abort = TRUE;
		  if(zversion <= 3)
		    mop_take_branch();
		  else
		    mop_store_result(2);
		} else {
		  infix_print_string("Restore failed.\n");
		} }
/* :: Start recording a script. */
	| RECORDON		{ zword oldop0 = operand[0]; operand[0] = 4; op_output_stream(); operand[0] = oldop0; }
/* :: Stop recording a script. */
	| RECORDOFF		{ zword oldop0 = operand[0]; operand[0] = neg(4); op_output_stream(); operand[0] = oldop0; }
/* :: Replay a recorded script. */
	| REPLAY		{ zword oldop0 = operand[0]; operand[0] = 1; op_input_stream(); operand[0] = oldop0; exit_debugger = TRUE; }
/* :: Halt replay. */
	| REPLAYOFF		{ zword oldop0 = operand[0]; operand[0] = 0; op_input_stream(); operand[0] = oldop0; }
/* :: Exit nitfol. */
	| QUIT			{ z_close(); glk_exit();	}
/* :: Undo last move (not last debugger command). */
	| UNDO			{
		if(restoreundo()) {
		  read_abort = TRUE; exit_debugger = TRUE;
		} else {
		  infix_print_string("No undo slots.\n");
		} }
/* :: Redo undid move.  Only works immediately after an @code{undo}. */
	| REDO			{
		if(restoreredo()) {
		  read_abort = TRUE; exit_debugger = TRUE;
		} else {
		  infix_print_string("No redo slots.\n");
		} }
/* :: Load debugging info from a file (usually @file{gameinfo.dbg}). */
	| SYMBOL_FILE /*FILE*/	{
		strid_t f;
		f = n_file_name_or_prompt(fileusage_Data|fileusage_BinaryMode,
					  filemode_Read, lex_tail());
		if(f) {
		  kill_infix();
		  init_infix(f);
		} }
/* :: Evaluates an expression and prints the result.\nThis can include function calls. */
	| PRINT commaexp	{ infix_display($2);		}
/* :: Evaluate an expression without printing its value. */
	| SET commaexp		{ inform_result = $2;		}
/* :: Print value of an expression each time the program stops. */
	| DISPLAY /*commaexp*/	{ infix_auto_display(lex_tail()); }
/* :: Stop automatically displaying an expression. */
	| UNDISPLAY NUM		{ infix_auto_undisplay($2.v);	}
/* :: Temporarily disable an automatic display. */
	| DISABLE_DISPLAY NUM	{ infix_set_display_enabled($2.v, FALSE); }
/* :: Re-enable an automatic display. */
	| ENABLE_DISPLAY NUM	{ infix_set_display_enabled($2.v, TRUE); }
/* :: Move an object around the object tree. */
	| MOVE commaexp TO commaexp { infix_move($4.v, $2.v); 	}
/* :: Display the object tree. */
	| OBJECT_TREE		{ infix_object_tree(0);		}
/* :: An argument says which object to use as the root of the tree. */
	| OBJECT_TREE commaexp	{ infix_object_tree($2.v);	}
/* :: Find objects whose shortnames contain a string. */
	| FIND                  {
		if(lex_expression[lex_offset])
		  infix_object_find(lex_tail());
	}
/* :: List all global variables and their values. */
	| LIST_GLOBALS		{
		z_typed v; v.t = Z_GLOBAL;
		for(v.o = 0; v.o <= 245; v.o++) {
		  const char *name = infix_get_name(v);
		  if(v.o) infix_print_string("; ");
		  if(name) {
		    infix_print_string(name);
		  } else {
		    infix_print_char('G');
		    infix_print_number(v.o);
		  }
		  infix_print_char('=');
		  infix_get_val(&v);
		  infix_print_number(v.v);
		}
		infix_print_char(10);
	}
/* :: With an argument, list all only those with a specific value. */
	| LIST_GLOBALS commaexp	{
		z_typed v; v.t = Z_GLOBAL;
		for(v.o = 0; v.o <= 245; v.o++) {
		  infix_get_val(&v);
		  if(v.v == $2.v) {
		    const char *name = infix_get_name(v);
		    if(name) {
		      infix_print_string(name);
		    } else {
		      infix_print_char('G');
		      infix_print_number(v.o);
		    }
		    infix_print_char(10);
		  }
		} }
/* :: Give an object an attribute. */
	| GIVE commaexp NUM	{ infix_set_attrib($2.v, $3.v);	}
/* :: With a tilde clears the attribute instead of setting it. */
	| GIVE commaexp '~' NUM { infix_clear_attrib($2.v, $4.v); }
/* :: Remove an object from the object tree. */
	| REMOVE commaexp	{ infix_remove($2.v);		}
/* :: Continue execution at a new location. */
	| JUMP linespec		{ PC=$2; exit_debugger = TRUE;	}
/* :: Continue execution. */
	| CONT			{ set_step(CONT_GO, 1); }
/* :: An argument sets the ignore count of the current breakpoint. */
	| CONT NUM		{ set_step(CONT_GO, 1); infix_set_ignore(cur_break, $2.v); }
/* :: Step through program to a different source line. */
	| STEP			{ set_step(CONT_STEP, 1); }
/* :: An argument specifies a repeat count. */
	| STEP NUM		{ set_step(CONT_STEP, $2.v); }
/* :: Step through program, stepping over subroutine calls. */
	| NEXT			{ set_step(CONT_NEXT, 1); }
/* :: An argument specifies a repeat count. */
	| NEXT NUM		{ set_step(CONT_NEXT, $2.v); }
/* :: Resume execution until the program reaches a line number greater than the current line. */
	| UNTIL			{ set_step(CONT_UNTIL, 1); }
/* :: Step exactly one instruction. */
	| STEPI			{ set_step(CONT_STEPI, 1); }
/* :: An argument specifies a repeat count. */
	| STEPI NUM		{ set_step(CONT_STEPI, $2.v); }
/* :: Step one instruction, stepping over subroutine calls. */
	| NEXTI			{ set_step(CONT_NEXTI, 1); }
/* :: Step a specified number of instructions, stepping over subroutine calls. */
	| NEXTI NUM		{ set_step(CONT_NEXTI, $2.v); }
/* :: An argument specifies a repeat count. */
	| FINISH		{ set_step(CONT_FINISH, 1); }
/* :: Set a breakpoint. */
	| BREAK linespec	{ infix_set_break($2);	}
/* :: An @code{if} clause specifies a condition. */
	| BREAK linespec IF /*commaexp*/ { int n = infix_set_break($2); infix_set_cond(n, lex_tail()); }
/* :: Set a condition for an existing breakpoint. */
	| COND NUM /*commaexp*/	{ infix_set_cond($2.v, lex_tail()); }
/* :: Set the ignore count for a breakpoint. */
	| IGNORE NUM NUM        { infix_set_ignore($2.v, $3.v);	}
/* :: Delete a breakpoint. */
	| DELETE NUM		{ infix_delete_breakpoint($2.v); }
/* :: List breakpoints. */
	| BREAKPOINTS		{ infix_show_all_breakpoints(); }
/* :: An argument specifies a specific breakpoint to list. */
	| BREAKPOINTS NUM	{ infix_show_breakpoint($2.v);	}
/* :: Temporarily disable a breakpoint. */
	| DISABLE_BREAK NUM	{ infix_set_break_enabled($2.v, FALSE); }
/* :: Re-enabled a breakpoint. */
	| ENABLE_BREAK NUM	{ infix_set_break_enabled($2.v, TRUE); }
/* :: Show the current source language. */
	| LANGUAGE		{ infix_print_string("The current source language is \"inform\".\n"); }
/* :: Get information on the current source file. */
	| INFOSOURCE		{ infix_print_string("Current source file is "); infix_print_string(cur_file?cur_file->filename:"unknown"); infix_print_string("\nContains "); infix_print_number(cur_file?cur_file->num_lines:0); infix_print_string(" lines.\nSource language is inform.\n"); }
/* :: List source files. */
	| INFOSOURCES		{ infix_print_string("Source files for which symbols have been read in:\n\n"); infix_list_files(); infix_print_char('\n'); }
/* :: Show licensing information. */
	| COPYING		{ show_copying(); }
/* :: Show warranty information. */
	| WARRANTY		{ show_warranty(); }
/* :: Show the selected stack frame. */
	| FRAME			{ infix_show_frame(infix_selected_frame); }
/* :: An argument specifies a stack frame to show. */
	| FRAME	NUM		{ infix_select_frame($2.v); infix_show_frame($2.v); }
/* :: Select a specific stack frame. */
	| SELECT_FRAME NUM	{ infix_select_frame($2.v); }
/* :: Select the parent of the selected frame. */
	| UP_FRAME		{ infix_select_frame(infix_selected_frame - 1); infix_show_frame(infix_selected_frame); }
/* :: An argument specifies how many frames up to go. */
	| UP_FRAME NUM		{ infix_select_frame(infix_selected_frame - $2.v); infix_show_frame(infix_selected_frame); }
/* :: Select the parent of the selected frame silently. */
	| UP_SILENTLY		{ infix_select_frame(infix_selected_frame - 1); }
/* :: An argument specifies how many frames up to go. */
	| UP_SILENTLY NUM	{ infix_select_frame(infix_selected_frame - $2.v); }
/* :: Select the child of the selected frame. */
	| DOWN_FRAME		{ infix_select_frame(infix_selected_frame + 1); infix_show_frame(infix_selected_frame); }
/* :: An argument specifies how many frames down to go. */
	| DOWN_FRAME NUM	{ infix_select_frame(infix_selected_frame + $2.v); infix_show_frame(infix_selected_frame); }
/* :: Silently select the child of the selected frame. */
	| DOWN_SILENTLY		{ infix_select_frame(infix_selected_frame + 1); }
/* :: An argument specifies how many frames down to go. */
	| DOWN_SILENTLY NUM	{ infix_select_frame(infix_selected_frame + $2.v); }	
/* :: Display the parent functions of the current frame. */
	| BACKTRACE		{ infix_backtrace(0, stack_get_depth()); }
/* :: An argument specifies how many frames back to show. */
	| BACKTRACE NUM		{ infix_backtrace(stack_get_depth() - $2.v, $2.v); }
/* :: If the argument is negative, start from the first frame instead of the current. */
	| BACKTRACE '-' NUM	{ infix_backtrace(0, $3.v); }
/*
	| LIST			{ infix_print_more(); }
	| LIST '-'		{ infix_print_before(); }
	| LIST NUM		{ if($1.t == Z_ROUTINE) { infix_location loc; infix_decode_; infix_file_print_around(...); }; else infix_file_print_around(cur_location.file, $2.v); }
*/
;

linespec: NUM			{ if($1.t == Z_ROUTINE) $$ = infix_get_routine_PC($1.v); else { infix_location l; infix_decode_fileloc(&l, cur_file?cur_file->filename:"", $1.v); $$ = l.thisPC; } }
	| '+' NUM		{ infix_location l; infix_decode_fileloc(&l, cur_file?cur_file->filename:"", cur_line + $2.v); $$ = l.thisPC; }
	| '-' NUM		{ infix_location l; infix_decode_fileloc(&l, cur_file?cur_file->filename:"", cur_line - $2.v); $$ = l.thisPC; }
	| DFILE ':' NUM		{ if($3.t == Z_ROUTINE) $$ = UNPACKR($3.v); else { infix_location l; infix_decode_fileloc(&l, $1->filename, $3.v); $$ = l.thisPC; } }
	| '*' NUM		{ $$ = $2.v;			}
;


orlist:	  exp			{
		if(condlist->condfunc(condlist->val, $1.v) ^ condlist->opposite) {
		   $$ = TRUE;
		   ignoreeffects++;
		} else
		   $$ = FALSE;
	    }
	| orlist OR exp		{
		if($1)
		  $$ = TRUE;
		else {
		  if(condlist->condfunc(condlist->val, $3.v) ^ condlist->opposite) {
		    $$ = TRUE;
		    ignoreeffects++;
		  }
		  else $$ = FALSE;
		} }
;


arglist:  /* empty string */	{ $$ = NULL; }
	| exp ',' arglist	{ zword_list g; $$ = $3; g.item = $1.v; LEaddm($$, g, n_rmmalloc); }
;

/* Expressions with commas */
commaexp: exp
	| condexp
	| commaexp ',' exp	{ $$ = $3;			}
	| commaexp ',' condexp	{ $$ = $3;			}
;

/* Expressions with conditions */
condexp:
	exp CONDITION { cond_list newcond; newcond.val = $1.v; newcond.condfunc = $2.condfunc; newcond.opposite = $2.opposite; LEaddm(condlist, newcond, n_rmmalloc); } orlist { if($4) ignoreeffects--; $$.v = $4; $$.t = Z_BOOLEAN; LEremovem(condlist, n_rmfreeone); }
;

/* Expressions without commas */
exp:	  NUM
		{ $$ = $1;				}
	| BFALSE
		{ $$.v = 0; $$.t = Z_BOOLEAN;		}
	| BTRUE
		{ $$.v = 1; $$.t = Z_BOOLEAN;		}
	| NOTHING
		{ $$.v = 0; $$.t = Z_OBJECT;		}

	| exp '=' exp
		{ $$ = $3; infix_assign(&$1, $3.v);	}

	| PARENT '(' commaexp ')'
		{ $$.v = infix_parent($3.v); $$.t = Z_OBJECT; }
	| CHILD '(' commaexp ')'
		{ $$.v = infix_child($3.v); $$.t = Z_OBJECT; }
	| SIBLING '(' commaexp ')'
		{ $$.v = infix_sibling($3.v); $$.t = Z_OBJECT; }
	| CHILDREN '(' commaexp ')'
		{ int n = 0; zword o = infix_child($3.v); while(o) { n++; o = infix_sibling(o); } $$.v = n; $$.t = Z_NUMBER; }

	| RANDOM '(' commaexp ')'
		{
		  if(!ignoreeffects) {
		    $$.v = z_random($3.v);
		    $$.t = Z_NUMBER;
		  } else {
		    $$.v = 0;
		    $$.t = Z_UNKNOWN;
		  }
		}
	| exp '(' arglist ')'
	      {
		zword locals[16];
		int i = 0;
		zword_list *p;
		if(!ignoreeffects) {
		  for(p = $3; p && i < 16; p=p->next) {
		    locals[i++] = p->item;
		  }
		  mop_call($1.v, i, locals, -2);
		  decode();
		  exit_decoder = FALSE;
		  $$.v = time_ret; $$.t = Z_UNKNOWN;
		} else {
		  $$.v = 0; $$.t = Z_UNKNOWN;
		}
	      }

	| exp ANDAND { if($1.v == 0) ignoreeffects++; } exp
		{ if($1.v == 0) ignoreeffects--; $$ = z_t($1, $4, $1.v && $4.v);	}
	| exp OROR { if($1.v != 0) ignoreeffects++; } exp
		{ if($1.v != 0) ignoreeffects--; $$ = z_t($1, $4, $1.v || $4.v);	}
	| NOTNOT exp
		{ $$.v = !($2.v); $$.t = Z_NUMBER;	}

	| exp '+' exp
		{ $$ = z_t($1, $3, $1.v + $3.v);	}
	| exp '-' exp
		{ $$ = z_t($1, $3, $1.v + neg($3.v));	}
	| exp '*' exp
		{ $$ = z_t($1, $3, z_mult($1.v, $3.v));	}
	| exp '/' exp
		{ $$ = z_t($1, $3, z_div($1.v, $3.v));	}
	| exp '%' exp
		{ $$ = z_t($1, $3, z_mod($1.v, $3.v));	}
	| exp '&' exp
		{ $$ = z_t($1, $3, $1.v & $3.v);	}
	| exp '|' exp
		{ $$ = z_t($1, $3, $1.v | $3.v);	}
	| '~' exp
		{ $$ = z_t($2, $2, ~$2.v);		}

	| exp BYTEARRAY exp
		{ $$.t = Z_BYTEARRAY; $$.o = $1.v; $$.p = $3.v; infix_get_val(&$$); }
	| exp WORDARRAY exp
		{ $$.t = Z_WORDARRAY; $$.o = $1.v; $$.p = $3.v; infix_get_val(&$$);	}

	| '-' exp		%prec precNEG
		{ $$ = z_t($2, $2, neg($2.v));		}

	| INCREMENT exp
		{ if(!ignoreeffects) infix_assign(&$2, ARITHMASK($2.v + 1)); $$ = $2; }
	| exp INCREMENT
		{ $$ = $1; if(!ignoreeffects) infix_assign(&$1, ARITHMASK($1.v + 1)); }
	| DECREMENT exp
		{ if(!ignoreeffects) infix_assign(&$2, ARITHMASK($2.v + neg(1))); $$ = $2; }
	| exp DECREMENT
		{ $$ = $1; if(!ignoreeffects) infix_assign(&$1, ARITHMASK($1.v + neg(1))); }

	| exp PROPADDR exp
		{ zword len; $$.v = infix_get_proptable($1.v, $3.v, &len); $$.t = Z_NUMBER; }
	| exp PROPLENGTH exp
		{ infix_get_proptable($1.v, $3.v, &$$.v); $$.t = Z_NUMBER; }

	| exp '.' exp
		{ $$.t = Z_OBJPROP; $$.o = $1.v; $$.p = $3.v; infix_get_val(&$$); }

/*
	| exp SUPERCLASS exp
		{ $$ = infix_superclass($1, $3); 	}
*/

	| NUMBER exp
		{ $$.v = $2.v; $$.t = Z_NUMBER;		}
	| OBJECT exp
		{ $$.v = $2.v; $$.t = Z_OBJECT;		}
	| ROUTINE exp
		{ $$.v = $2.v; $$.t = Z_ROUTINE;	}
	| STRING exp
		{ $$.v = $2.v; $$.t = Z_STRING;		}
	| GLOBAL exp
		{ $$.t = Z_WORDARRAY; $$.o = z_globaltable; $$.p = $2.v; infix_get_val(&$$); }
	| LOCAL exp
		{ $$.t = Z_LOCAL; $$.o = infix_selected_frame; $$.p = $2.v; infix_get_val(&$$); }
	| '(' commaexp ')'
		{ $$ = $2;				}
;


%%

#if 0
{ /* fanagling to get emacs indentation sane */
int foo;
#endif

static z_typed z_t(z_typed a, z_typed b, zword v)
{
  z_typed r;
  r.v = ARITHMASK(v);
  if(a.t == Z_NUMBER && b.t == Z_NUMBER)
    r.t = Z_NUMBER;
  else
    r.t = Z_UNKNOWN;
  return r;
}



typedef struct {
  int token;
  const char *name;
} name_token;

static name_token infix_operators[] = {
  { ANDAND,     "&&" },
  { OROR,       "||" },
  { NOTNOT,     "~~" },
  { BYTEARRAY,  "->" },
  { WORDARRAY,  "-->" },
  { NUMBER,     "(number)" },
  { OBJECT,     "(object)" },
  { ROUTINE,    "(routine)" },
  { STRING,     "(string)" },
  { GLOBAL,     "(global)" },
  { LOCAL,      "(local)" },
  { INCREMENT,  "++" },
  { DECREMENT,  "--" },
  { SUPERCLASS, "::" }
};


static name_token infix_keywords[] = {
  { TO,         "to" },
  { IF,         "if" },
  { OR,         "or" },
  { BTRUE,      "true" },
  { BFALSE,     "false" },
  { NOTHING,    "nothing" },
  { PARENT,     "parent" },
  { CHILD,      "child" },
  { SIBLING,    "sibling" },
  { RANDOM,     "random" },
  { CHILDREN,   "children" }
};


/* These are only valid as the first token in an expression.  A single space
   matches at least one typed whitespace character */
static name_token infix_commands[] = {
  { '#',          "#" },
  { HELP,         "help" },
  { ALIAS,        "alias" },
  { RALIAS,       "ralias" },
  { UNALIAS,      "unalias" },
  { DUMPMEM,      "dumpmem" },
  { AUTOMAP,      "automap" },
  { UNDO,         "undo" },
  { REDO,         "redo" },
  { QUIT,         "quit" },
  { RESTORE,      "restore" },
  { RESTART,      "restart" },
  { RESTART,      "run" },
  { RECORDON,	  "recording on" },
  { RECORDOFF,    "recording off" },
  { REPLAY,       "replay" },
  { REPLAYOFF,    "replay off" },
  { SYMBOL_FILE,  "symbol-file" },
  { PRINT,        "print" },
  { PRINT,        "p" },
  { PRINT,        "call" },  /* No void functions in inform */
  { SET,          "set" },
  { MOVE,         "move" },
  { OBJECT_TREE,  "object-tree" },
  { OBJECT_TREE,  "tree" },
  { FIND,         "find" },
  { REMOVE,       "remove" },
  { GIVE,         "give" },
  { LIST_GLOBALS, "globals" },
  { JUMP,         "jump" },
  { CONT,         "continue" },
  { CONT,         "c" },
  { CONT,         "fg" },
  { STEP,         "step" },
  { STEP,         "s" },
  { NEXT,         "next" },
  { NEXT,         "n" },
  { STEPI,        "stepi" },
  { STEPI,        "si" },
  { NEXTI,        "nexti" },
  { NEXTI,        "ni" },
  { UNTIL,        "until" },
  { UNTIL,        "u" },
  { FINISH,       "finish" },
  { BREAK,        "break" },
  { DELETE,       "delete" },
  { DELETE,       "d" },
  { DELETE,       "delete breakpoints" },
  { COND,         "condition" },
  { IGNORE,       "ignore" },
  { FRAME,        "frame" },
  { FRAME,        "f" },
  { SELECT_FRAME, "select-frame" },
  { UP_FRAME,     "up" },
  { DOWN_FRAME,   "down" },
  { DOWN_FRAME,   "do" },
  { UP_SILENTLY,  "up-silently" },
  { DOWN_SILENTLY,"down-silently" },
  { BREAKPOINTS,  "info breakpoints" },
  { BREAKPOINTS,  "info watchpoints" },
  { BREAKPOINTS,  "info break" },
  { DISABLE_BREAK,"disable" },
  { DISABLE_BREAK,"disable breakpoints" },
  { DISABLE_BREAK,"dis" },
  { DISABLE_BREAK,"dis breakpoints" },
  { ENABLE_BREAK, "enable" },
  { ENABLE_BREAK, "enable breakpoints" },
  { LANGUAGE,     "show language" },
  { INFOSOURCE,   "info source" },
  { INFOSOURCES,  "info sources" },
  { COPYING,      "show copying" },
  { WARRANTY,     "show warranty" },
  { BACKTRACE,    "backtrace" },
  { BACKTRACE,    "bt" },
  { BACKTRACE,    "where" },
  { BACKTRACE,    "info stack" },
  { BACKTRACE,    "info s" },
  { DISPLAY,      "display" },
  { UNDISPLAY,    "undisplay" },
  { UNDISPLAY,    "delete display" },
  { DISABLE_DISPLAY,"disable display" },
  { DISABLE_DISPLAY,"dis display" },
  { ENABLE_DISPLAY,"enable display" }
};

#include "dbg_help.h"

static BOOL z_isequal(zword a, zword b)
{
  return (a == b);
}

static BOOL z_isgreat(zword a, zword b)
{
  return is_greaterthan(a, b);
}

static BOOL z_isless(zword a, zword b)
{
  return is_lessthan(a, b);
}

static BOOL infix_provides(zword o, zword p)
{
  zword len;
  return (infix_get_proptable(o, p, &len) != 0);
}

static BOOL infix_in(zword a, zword b)
{
  return infix_parent(a) == b;
}

typedef struct {
  const char *name;
  BOOL (*condfunc)(zword a, zword b);
  BOOL opposite;
} condition;

condition conditionlist[] = {
  { "==",      z_isequal,         FALSE },
  { "~=",      z_isequal,         TRUE },
  { ">",       z_isgreat,         FALSE },
  { "<",       z_isless,          FALSE },
  { "<=",      z_isgreat,         TRUE },
  { ">=",      z_isless,          TRUE },
  { "has",     infix_test_attrib, FALSE },
  { "hasnt",   infix_test_attrib, TRUE },
  { "in",      infix_in,          FALSE },
  { "notin",   infix_in,          TRUE },
/*{ "ofclass", infix_ofclass,     FALSE },*/
  { "provides",infix_provides,    FALSE }
};


static BOOL is_command_identifier(char c)
{
  return isalpha(c) || (c == '-');
}

static BOOL is_identifier(char c)
{
  return isalpha(c) || isdigit(c) || (c == '_');
}

static BOOL is_longer_identifier(char c)
{
  return isalpha(c) || isdigit(c) || (c == '_') || (c == '.') || (c == ':');
}

static int grab_number(z_typed *val)
{
  int len = 0;
  char *endptr;
  char c = lex_expression[lex_offset + len];
  int base = 10;
  long int num;

  /* Don't handle negativity here */
  if(c == '-' || c == '+')
    return 0;
  
  if(c == '$') {
    len++;
    base = 16;
    c = lex_expression[lex_offset + len];
    if(c == '$') {
      len++;
      base = 2;
      c = lex_expression[lex_offset + len];
    }
  }
  
  num = n_strtol(lex_expression + lex_offset + len, &endptr, base);

  if(endptr != lex_expression + lex_offset) {
    len += endptr - lex_expression - lex_offset;
    val->v = num;
    val->t = Z_NUMBER;
    return len;
  }
  return 0;
}


typedef enum { match_None, match_Partial, match_Complete } match_type;

static match_type command_matches(const char *command, const char *expression,
				  unsigned *matchedlen)
{
  unsigned c, e;
  e = 0;

  for(c = 0; command[c]; c++) {
    if(command[c] != expression[e]) {
      if(!is_command_identifier(expression[e])) {
	*matchedlen = e;
	return match_Partial;
      }
      return match_None;
    }

    e++;
    
    if(command[c] == ' ') {
      while(expression[e] == ' ')
	e++;
    }
  }

  if(!is_command_identifier(expression[e])) {
    *matchedlen = e;
    return match_Complete; 
  }

  return match_None;
}


static int grab_command(void)
{
  unsigned i;
  unsigned len;

  unsigned best;
  match_type best_match = match_None;
  unsigned best_len = 0;
  BOOL found = FALSE;
  BOOL ambig = FALSE;

  while(isspace(lex_expression[lex_offset]))
    lex_offset++;

  for(i = 0; i < sizeof(infix_commands) / sizeof(*infix_commands); i++) {
    switch(command_matches(infix_commands[i].name, lex_expression + lex_offset, &len)) {
    case match_Complete:
      if(len > best_len || best_match != match_Complete) {
	best = i;
	best_match = match_Complete;
	best_len = len;
	found = TRUE;
      }
      break;

    case match_Partial:
      if(best_match != match_Complete) {
	if(found)
	  ambig = TRUE;
	best = i;
	best_match = match_Partial;
	best_len = len;
	found = TRUE;
      }

    case match_None:
      ;
    }
  }

  if(ambig && best_match != match_Complete) {
    infix_print_string("Ambiguous command.\n");
    return 0;
  }

  if(found) {
    lex_offset += best_len;
    return infix_commands[best].token;
  }

  infix_print_string("Undefined command.\n");
  return 0;
}


static void inform_help(void)
{
  int command;
  unsigned i;
  BOOL is_command = FALSE;
  
  for(i = lex_offset; lex_expression[i]; i++)
    if(!isspace(lex_expression[i]))
      is_command = TRUE;

  if(!is_command) {
    infix_print_string("Help is available on the following commands:\n");
    for(i = 0; i < sizeof(command_help) / sizeof(*command_help); i++) {
      unsigned j;
      for(j = 0; j < sizeof(infix_commands) / sizeof(*infix_commands); j++)
	if(command_help[i].token == infix_commands[j].token) {
	  infix_print_char('\'');
	  infix_print_string(infix_commands[j].name);
	  infix_print_char('\'');
	  break;
	}
      infix_print_char(' ');
    }
    infix_print_string("\n");
    return;
  }
  
  command = grab_command();
  if(command) {
    for(i = 0; i < sizeof(command_help) / sizeof(*command_help); i++) {
      if(command_help[i].token == command) {
	infix_print_string(command_help[i].name);
	infix_print_char(10);
	return;
      }
    }
    infix_print_string("No help available for that command.\n");
  }
}


void process_debug_command(const char *buffer)
{
#ifdef YYDEBUG
  yydebug = 1;
#endif
  lex_expression = buffer;
  lex_offset = 0;
  ignoreeffects = 0;
  yyparse();
  n_rmfree();
}

BOOL exp_has_locals(const char *exp)
{
  return FALSE;
}

z_typed evaluate_expression(const char *exp, unsigned frame)
{
  unsigned old_frame = infix_selected_frame;
  char *new_exp = (char *) n_malloc(n_strlen(exp) + 5);
  n_strcpy(new_exp, "set ");
  n_strcat(new_exp, exp);

  infix_selected_frame = frame;
  process_debug_command(new_exp);
  infix_selected_frame = old_frame;

  n_free(new_exp);

  return inform_result;
}

static void yyerror(const char *s)
{
  infix_print_string(s);
  infix_print_char(10);
}

static int yylex(void)
{
  unsigned i, len, longer;
  BOOL check_command = FALSE;

  if(lex_offset == 0)
    check_command = TRUE;

  while(isspace(lex_expression[lex_offset]))
    lex_offset++;

  if(check_command) {
    return grab_command();
  }

  if((len = grab_number(&yylval.val)) != 0) {
    lex_offset += len;
    return NUM;
  }

  for(i = 0; i < sizeof(infix_operators) / sizeof(*infix_operators); i++) {
    if(n_strncmp(infix_operators[i].name, lex_expression + lex_offset,
	       n_strlen(infix_operators[i].name)) == 0) {
      lex_offset += n_strlen(infix_operators[i].name);
      return infix_operators[i].token;
    }
  }

  for(i = 0; i < sizeof(conditionlist) / sizeof(*conditionlist); i++) {
    len = n_strlen(conditionlist[i].name);
    if(len
       && n_strncmp(conditionlist[i].name,
		   lex_expression + lex_offset, len) == 0
       && !(is_identifier(conditionlist[i].name[len-1])
	    && is_identifier(lex_expression[lex_offset + len]))) {

      lex_offset += len;
      yylval.cond.condfunc = conditionlist[i].condfunc;
      yylval.cond.opposite = conditionlist[i].opposite;
      return CONDITION;
    }
  }

  if((len = infix_find_file(&yylval.filenum, lex_expression + lex_offset)) != 0) {
    lex_offset += len;
    return DFILE;
  }


  for(len = 0; is_identifier(lex_expression[lex_offset + len]); len++)
    ;

  if(!len)
    return lex_expression[lex_offset++];

  for(i = 0; i < sizeof(infix_keywords) / sizeof(*infix_keywords); i++) {
    if(n_strmatch(infix_keywords[i].name, lex_expression + lex_offset, len)) {
      lex_offset += len;
      return infix_keywords[i].token;
    }
  }

  for(longer = len; is_longer_identifier(lex_expression[lex_offset + longer]); longer++)
    ;

  if(infix_find_symbol(&yylval.val, lex_expression + lex_offset, longer)) {
    lex_offset += longer;
    return NUM;
  }

  if(infix_find_symbol(&yylval.val, lex_expression + lex_offset, len)) {
    lex_offset += len;
    return NUM;
  }

  infix_print_string("Unknown identifier \"");
  for(i = 0; i < len; i++)
    infix_print_char(lex_expression[lex_offset + i]);
  infix_print_string("\"\n");

  return 0;
}

#endif /* DEBUGGING */
