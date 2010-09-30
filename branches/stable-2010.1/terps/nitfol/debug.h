/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i debug.c' */
#ifndef CFH_DEBUG_H
#define CFH_DEBUG_H

/* From `debug.c': */

#ifndef DEBUGGING
#define debug_object(o, t)
#define debug_attrib(a, o)

#endif
typedef enum { CONT_GO, CONT_STEP, CONT_NEXT, CONT_FINISH, CONT_UNTIL, CONT_STEPI, CONT_NEXTI }Cont_type;
extern BOOL enter_debugger;

#ifdef DEBUGGING
extern BOOL exit_debugger;
extern infix_file * cur_file;
extern int cur_line;
extern int cur_break;
extern int cur_stack_depth;
extern int infix_selected_frame;
void set_step (Cont_type t , int count );
int infix_set_break (offset location );
void infix_delete_breakpoint (int breaknum );
void infix_set_cond (int breaknum , const char *condition );
void infix_set_ignore (int breaknum , int count );
void infix_set_break_enabled (int breaknum , BOOL enabled );
void infix_show_all_breakpoints (void);
void infix_show_breakpoint (int breaknum );
int infix_auto_display (const char *expression );
void perform_displays (void);
void infix_auto_undisplay (int displaynum );
void infix_set_display_enabled (int displaynum , BOOL enabled );
const char * debug_decode_number (unsigned number );
extern unsigned opcode_counters[];
void check_watches (void);
void debug_prompt (void);
void infix_select_frame (int num );
void infix_show_frame (int frame );
void infix_backtrace (int start , int length );
extern const char * watchnames[];
void debug_object (zword objectnum , watchinfo type );
void debug_attrib (zword attribnum , zword objectnum );

#endif

#endif /* CFH_DEBUG_H */
