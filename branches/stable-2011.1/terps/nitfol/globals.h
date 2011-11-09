/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i globals.c' */
#ifndef CFH_GLOBALS_H
#define CFH_GLOBALS_H

/* From `globals.c': */
extern strid_t current_zfile;
extern glui32 zfile_offset;
extern strid_t input_stream1;
extern strid_t blorb_file;
extern glui32 imagecount;
extern int using_infix;
extern int zversion;
extern int granularity;
extern offset rstart;
extern offset sstart;
extern const char * username;
extern BOOL aye_matey;
extern BOOL do_tandy;
extern BOOL do_spell_correct;
extern BOOL do_expand;
extern BOOL do_automap;
extern BOOL fullname;
extern BOOL quiet;
extern BOOL ignore_errors;
extern BOOL auto_save_undo;
extern BOOL auto_save_undo_char;
extern int interp_num;
extern char interp_ver;
extern zbyte * z_memory;
extern offset PC;
extern offset oldPC;
extern int numoperands;
extern zword operand[];
extern zword maxobjs;
extern zword object_count;
extern zword obj_first_prop_addr;
extern zword obj_last_prop_addr;
extern zword prop_table_start;
extern zword prop_table_end;
extern offset total_size;
extern offset dynamic_size;
extern offset high_mem_mark;
extern offset game_size;
extern zword z_checksum;
extern zword z_globaltable;
extern zword z_objecttable;
extern zword z_propdefaults;
extern zword z_synonymtable;
extern zword z_dictionary;
extern zword z_terminators;
extern zword z_headerext;
extern int faked_random_seed;
extern BOOL in_timer;
extern BOOL exit_decoder;
extern zword time_ret;
extern BOOL smart_timed;
extern BOOL lower_block_quotes;
extern BOOL read_abort;
extern BOOL has_done_save_undo;
extern BOOL allow_saveundo;
extern BOOL allow_output;
extern BOOL testing_string;
extern BOOL string_bad;
extern BOOL do_check_watches;
extern BOOL false_undo;
extern char * db_prompt;
extern char * search_path;
extern int automap_size;
extern glui32 automap_split;
extern int stacklimit;
extern BOOL enablefont3;

#endif /* CFH_GLOBALS_H */
