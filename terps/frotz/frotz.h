/*
 * frotz.h
 *
 * Global declarations and definitions
 *
 */

typedef int bool;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include <stdlib.h>
#include <string.h>

typedef unsigned char zbyte;
typedef unsigned short zword;

/*** Glk needs a 32-bit integer type for Unicode characters ***/
#include <limits.h>
#if (USHORT_MAX == 4294967295)
typedef unsigned short zchar;
#elif (UINT_MAX   == 4294967295)
typedef unsigned int zchar;
#elif (ULONG_MAX == 4294967295)
typedef unsigned long zchar;
#else
#error No 32-bit integer type found.
#endif

enum story
{
    BEYOND_ZORK,
    SHERLOCK,
    ZORK_ZERO,
    SHOGUN,
    ARTHUR,
    JOURNEY,
    LURKING_HORROR,
    UNKNOWN
};

/*** Constants that may be set at compile time ***/

#ifndef MAX_UNDO_SLOTS
#define MAX_UNDO_SLOTS 500
#endif
#ifndef MAX_FILE_NAME
#define MAX_FILE_NAME 256
#endif
#ifndef TEXT_BUFFER_SIZE
#define TEXT_BUFFER_SIZE 200
#endif
#ifndef INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE 200
#endif
#ifndef STACK_SIZE
#define STACK_SIZE 61440
#endif

#ifndef DEFAULT_SAVE_NAME
#define DEFAULT_SAVE_NAME "story.sav"
#endif
#ifndef DEFAULT_SCRIPT_NAME
#define DEFAULT_SCRIPT_NAME "story.scr"
#endif
#ifndef DEFAULT_COMMAND_NAME
#define DEFAULT_COMMAND_NAME "story.rec"
#endif
#ifndef DEFAULT_AUXILARY_NAME
#define DEFAULT_AUXILARY_NAME "story.aux"
#endif
#ifndef DEFAULT_SAVE_DIR	/* DG */
#define DEFAULT_SAVE_DIR ".frotz-saves"
#endif

/*** Story file header format ***/

#define H_VERSION 0
#define H_CONFIG 1
#define H_RELEASE 2
#define H_RESIDENT_SIZE 4
#define H_START_PC 6
#define H_DICTIONARY 8
#define H_OBJECTS 10
#define H_GLOBALS 12
#define H_DYNAMIC_SIZE 14
#define H_FLAGS 16
#define H_SERIAL 18
#define H_ABBREVIATIONS 24
#define H_FILE_SIZE 26
#define H_CHECKSUM 28
#define H_INTERPRETER_NUMBER 30
#define H_INTERPRETER_VERSION 31
#define H_SCREEN_ROWS 32
#define H_SCREEN_COLS 33
#define H_SCREEN_WIDTH 34
#define H_SCREEN_HEIGHT 36
#define H_FONT_HEIGHT 38 /* this is the font width in V5 */
#define H_FONT_WIDTH 39 /* this is the font height in V5 */
#define H_FUNCTIONS_OFFSET 40
#define H_STRINGS_OFFSET 42
#define H_DEFAULT_BACKGROUND 44
#define H_DEFAULT_FOREGROUND 45
#define H_TERMINATING_KEYS 46
#define H_LINE_WIDTH 48
#define H_STANDARD_HIGH 50
#define H_STANDARD_LOW 51
#define H_ALPHABET 52
#define H_EXTENSION_TABLE 54
#define H_USER_NAME 56

#define HX_TABLE_SIZE 0
#define HX_MOUSE_X 1
#define HX_MOUSE_Y 2
#define HX_UNICODE_TABLE 3
#define HX_FLAGS 4
#define HX_FORE_COLOUR 5
#define HX_BACK_COLOUR 6

/*** Various Z-machine constants ***/

#define V1 1
#define V2 2
#define V3 3
#define V4 4
#define V5 5
#define V6 6
#define V7 7
#define V8 8
#define V9 9

#define CONFIG_BYTE_SWAPPED 0x01 /* Story file is byte swapped         - V3  */
#define CONFIG_TIME         0x02 /* Status line displays time          - V3  */
#define CONFIG_TWODISKS     0x04 /* Story file occupied two disks      - V3  */
#define CONFIG_TANDY        0x08 /* Tandy licensed game                - V3  */
#define CONFIG_NOSTATUSLINE 0x10 /* Interpr can't support status lines - V3  */
#define CONFIG_SPLITSCREEN  0x20 /* Interpr supports split screen mode - V3  */
#define CONFIG_PROPORTIONAL 0x40 /* Interpr uses proportional font     - V3  */

#define CONFIG_COLOUR       0x01 /* Interpr supports colour            - V5+ */
#define CONFIG_PICTURES	    0x02 /* Interpr supports pictures	       - V6  */
#define CONFIG_BOLDFACE     0x04 /* Interpr supports boldface style    - V4+ */
#define CONFIG_EMPHASIS     0x08 /* Interpr supports emphasis style    - V4+ */
#define CONFIG_FIXED        0x10 /* Interpr supports fixed width style - V4+ */
#define CONFIG_SOUND	    0x20 /* Interpr supports sound             - V6  */
#define CONFIG_TIMEDINPUT   0x80 /* Interpr supports timed input       - V4+ */

#define SCRIPTING_FLAG	  0x0001 /* Outputting to transscription file  - V1+ */
#define FIXED_FONT_FLAG   0x0002 /* Use fixed width font               - V3+ */
#define REFRESH_FLAG 	  0x0004 /* Refresh the screen                 - V6  */
#define GRAPHICS_FLAG	  0x0008 /* Game wants to use graphics         - V5+ */
#define OLD_SOUND_FLAG	  0x0010 /* Game wants to use sound effects    - V3  */
#define UNDO_FLAG	  0x0010 /* Game wants to use UNDO feature     - V5+ */
#define MOUSE_FLAG	  0x0020 /* Game wants to use a mouse          - V5+ */
#define COLOUR_FLAG	  0x0040 /* Game wants to use colours          - V5+ */
#define SOUND_FLAG	  0x0080 /* Game wants to use sound effects    - V5+ */
#define MENU_FLAG	  0x0100 /* Game wants to use menus            - V6  */

#define TRANSPARENT_FLAG  0x0001 /* Game wants to use transparency     - V6  */

#define INTERP_DEFAULT 0
#define INTERP_DEC_20 1
#define INTERP_APPLE_IIE 2
#define INTERP_MACINTOSH 3
#define INTERP_AMIGA 4
#define INTERP_ATARI_ST 5
#define INTERP_MSDOS 6
#define INTERP_CBM_128 7
#define INTERP_CBM_64 8
#define INTERP_APPLE_IIC 9
#define INTERP_APPLE_IIGS 10
#define INTERP_TANDY 11

#define BLACK_COLOUR 2
#define RED_COLOUR 3
#define GREEN_COLOUR 4
#define YELLOW_COLOUR 5
#define BLUE_COLOUR 6
#define MAGENTA_COLOUR 7
#define CYAN_COLOUR 8
#define WHITE_COLOUR 9
#define GREY_COLOUR 10		/* INTERP_MSDOS only */
#define LIGHTGREY_COLOUR 10 	/* INTERP_AMIGA only */
#define MEDIUMGREY_COLOUR 11 	/* INTERP_AMIGA only */
#define DARKGREY_COLOUR 12 	/* INTERP_AMIGA only */
#define TRANSPARENT_COLOUR 15 /* ZSpec 1.1 */

#define REVERSE_STYLE 1
#define BOLDFACE_STYLE 2
#define EMPHASIS_STYLE 4
#define FIXED_WIDTH_STYLE 8

#define TEXT_FONT 1
#define PICTURE_FONT 2
#define GRAPHICS_FONT 3
#define FIXED_WIDTH_FONT 4

/*** Constants for os_beep */

#define BEEP_HIGH       1
#define BEEP_LOW        2

/*** Constants for os_restart_game */

#define RESTART_BEGIN 0
#define RESTART_WPROP_SET 1
#define RESTART_END 2

/*** Constants for os_menu */

#define MENU_NEW 0
#define MENU_ADD 1
#define MENU_REMOVE 2

/*** Character codes ***/

#define ZC_TIME_OUT 0x00
#define ZC_NEW_STYLE 0x01
#define ZC_NEW_FONT 0x02
#define ZC_BACKSPACE 0x08
#define ZC_INDENT 0x09
#define ZC_GAP 0x0b
#define ZC_RETURN 0x0d
#define ZC_HKEY_MIN 0x0e
#define ZC_HKEY_RECORD 0x0e
#define ZC_HKEY_PLAYBACK 0x0f
#define ZC_HKEY_SEED 0x10
#define ZC_HKEY_UNDO 0x11
#define ZC_HKEY_RESTART 0x12
#define ZC_HKEY_QUIT 0x13
#define ZC_HKEY_DEBUG 0x14
#define ZC_HKEY_HELP 0x15
#define ZC_HKEY_MAX 0x15
#define ZC_ESCAPE 0x1b
#define ZC_ASCII_MIN 0x20
#define ZC_ASCII_MAX 0x7e
#define ZC_BAD 0x7f
#define ZC_ARROW_MIN 0x81
#define ZC_ARROW_UP 0x81
#define ZC_ARROW_DOWN 0x82
#define ZC_ARROW_LEFT 0x83
#define ZC_ARROW_RIGHT 0x84
#define ZC_ARROW_MAX 0x84
#define ZC_FKEY_MIN 0x85
#define ZC_FKEY_MAX 0x90
#define ZC_NUMPAD_MIN 0x91
#define ZC_NUMPAD_MAX 0x9a
#define ZC_SINGLE_CLICK 0x9b
#define ZC_DOUBLE_CLICK 0x9c
#define ZC_MENU_CLICK 0x9d
#define ZC_LATIN1_MIN 0xa0
#define ZC_LATIN1_MAX 0xff

/*** File types ***/

#define FILE_RESTORE 0
#define FILE_SAVE 1
#define FILE_SCRIPT 2
#define FILE_PLAYBACK 3
#define FILE_RECORD 4
#define FILE_LOAD_AUX 5
#define FILE_SAVE_AUX 6

/*** Data access macros ***/

#define SET_BYTE(addr,v)  { zmp[addr] = v; }
#define LOW_BYTE(addr,v)  { v = zmp[addr]; }
#define CODE_BYTE(v)	  { v = *pcp++;    }

#if defined (AMIGA)

extern zbyte *pcp;
extern zbyte *zmp;

#define lo(v)		((zbyte *)&v)[1]
#define hi(v)		((zbyte *)&v)[0]

#define SET_WORD(addr,v)  { zmp[addr] = hi(v); zmp[addr+1] = lo(v); }
#define LOW_WORD(addr,v)  { hi(v) = zmp[addr]; lo(v) = zmp[addr+1]; }
#define HIGH_WORD(addr,v) { hi(v) = zmp[addr]; lo(v) = zmp[addr+1]; }
#define CODE_WORD(v)      { hi(v) = *pcp++; lo(v) = *pcp++; }
#define GET_PC(v)         { v = pcp - zmp; }
#define SET_PC(v)         { pcp = zmp + v; }

#endif

/* A bunch of x86 assembly code previously appeared here. */

#if !defined (AMIGA) && !defined (MSDOS_16BIT)

extern zbyte *pcp;
extern zbyte *zmp;

#define lo(v)	(v & 0xff)
#define hi(v)	(v >> 8)

#define SET_WORD(addr,v)  { zmp[addr] = hi(v); zmp[addr+1] = lo(v); }
#define LOW_WORD(addr,v)  { v = ((zword) zmp[addr] << 8) | zmp[addr+1]; }
#define HIGH_WORD(addr,v) { v = ((zword) zmp[addr] << 8) | zmp[addr+1]; }
#define HIGH_LONG(addr,v) { v = ((zword) zmp[addr]   << 24) | \
                                ((zword) zmp[addr+1] << 16) | \
                                ((zword) zmp[addr+2] <<  8) | zmp[addr+3]; }
#define CODE_WORD(v)      { v = ((zword) pcp[0] << 8) | pcp[1]; pcp += 2; }
#define CODE_IDX_WORD(v,i){ v = ((zword) pcp[i] << 8) | pcp[i+1]; }
#define GET_PC(v)         { v = pcp - zmp; }
#define SET_PC(v)         { pcp = zmp + v; }

#endif


/*** Story file header data ***/

extern zbyte h_version;
extern zbyte h_config;
extern zword h_release;
extern zword h_resident_size;
extern zword h_start_pc;
extern zword h_dictionary;
extern zword h_objects;
extern zword h_globals;
extern zword h_dynamic_size;
extern zword h_flags;
extern zbyte h_serial[6];
extern zword h_abbreviations;
extern zword h_file_size;
extern zword h_checksum;
extern zbyte h_interpreter_number;
extern zbyte h_interpreter_version;
extern zbyte h_screen_rows;
extern zbyte h_screen_cols;
extern zword h_screen_width;
extern zword h_screen_height;
extern zbyte h_font_height;
extern zbyte h_font_width;
extern zword h_functions_offset;
extern zword h_strings_offset;
extern zbyte h_default_background;
extern zbyte h_default_foreground;
extern zword h_terminating_keys;
extern zword h_line_width;
extern zbyte h_standard_high;
extern zbyte h_standard_low;
extern zword h_alphabet;
extern zword h_extension_table;
extern zbyte h_user_name[8];

extern zword hx_table_size;
extern zword hx_mouse_x;
extern zword hx_mouse_y;
extern zword hx_unicode_table;
extern zword hx_flags;
extern zword hx_fore_colour;
extern zword hx_back_colour;

/*** Various data ***/

extern char *story_name;

extern enum story story_id;
extern long story_size;

extern zword stack[STACK_SIZE];
extern zword *sp;
extern zword *fp;
extern zword frame_count;

extern zword zargs[8];
extern int zargc;

extern bool ostream_screen;
extern bool ostream_script;
extern bool ostream_memory;
extern bool ostream_record;
extern bool istream_replay;
extern bool message;

extern int cwin;
extern int mwin;

extern int mouse_x;
extern int mouse_y;
extern int menu_selected;

extern bool enable_wrapping;
extern bool enable_scripting;
extern bool enable_scrolling;
extern bool enable_buffering;

extern int option_attribute_assignment;
extern int option_attribute_testing;
extern int option_object_locating;
extern int option_object_movement;
extern int option_context_lines;
extern int option_left_margin;
extern int option_right_margin;
extern int option_ignore_errors;
extern int option_piracy;
extern int option_undo_slots;
extern int option_expand_abbreviations;
extern int option_script_cols;
extern int option_save_quetzal;
extern int option_sound;	/* dg */
extern int option_err_report_mode;
extern char *option_zcode_path;	/* dg */

extern long reserve_mem;

/*** Z-machine opcodes ***/

void 	z_add (void);
void 	z_and (void);
void 	z_art_shift (void);
void 	z_buffer_mode (void);
void 	z_buffer_screen (void);
void 	z_call_n (void);
void 	z_call_s (void);
void 	z_catch (void);
void 	z_check_arg_count (void);
void	z_check_unicode (void);
void 	z_clear_attr (void);
void 	z_copy_table (void);
void 	z_dec (void);
void 	z_dec_chk (void);
void 	z_div (void);
void 	z_draw_picture (void);
void 	z_encode_text (void);
void 	z_erase_line (void);
void 	z_erase_picture (void);
void 	z_erase_window (void);
void 	z_get_child (void);
void 	z_get_cursor (void);
void 	z_get_next_prop (void);
void 	z_get_parent (void);
void 	z_get_prop (void);
void 	z_get_prop_addr (void);
void 	z_get_prop_len (void);
void 	z_get_sibling (void);
void 	z_get_wind_prop (void);
void 	z_inc (void);
void 	z_inc_chk (void);
void 	z_input_stream (void);
void 	z_insert_obj (void);
void 	z_je (void);
void 	z_jg (void);
void 	z_jin (void);
void 	z_jl (void);
void 	z_jump (void);
void 	z_jz (void);
void 	z_load (void);
void 	z_loadb (void);
void 	z_loadw (void);
void 	z_log_shift (void);
void 	z_make_menu (void);
void 	z_mod (void);
void 	z_mouse_window (void);
void 	z_move_window (void);
void 	z_mul (void);
void 	z_new_line (void);
void 	z_nop (void);
void 	z_not (void);
void 	z_or (void);
void 	z_output_stream (void);
void 	z_picture_data (void);
void 	z_picture_table (void);
void 	z_piracy (void);
void 	z_pop (void);
void 	z_pop_stack (void);
void 	z_print (void);
void 	z_print_addr (void);
void 	z_print_char (void);
void 	z_print_form (void);
void 	z_print_num (void);
void 	z_print_obj (void);
void 	z_print_paddr (void);
void 	z_print_ret (void);
void 	z_print_table (void);
void	z_print_unicode (void);
void 	z_pull (void);
void 	z_push (void);
void 	z_push_stack (void);
void 	z_put_prop (void);
void 	z_put_wind_prop (void);
void 	z_quit (void);
void 	z_random (void);
void 	z_read (void);
void 	z_read_char (void);
void 	z_read_mouse (void);
void 	z_remove_obj (void);
void 	z_restart (void);
void 	z_restore (void);
void 	z_restore_undo (void);
void 	z_ret (void);
void 	z_ret_popped (void);
void 	z_rfalse (void);
void 	z_rtrue (void);
void 	z_save (void);
void 	z_save_undo (void);
void 	z_scan_table (void);
void 	z_scroll_window (void);
void 	z_set_attr (void);
void 	z_set_font (void);
void 	z_set_colour (void);
void 	z_set_cursor (void);
void 	z_set_margins (void);
void 	z_set_window (void);
void 	z_set_text_style (void);
void 	z_set_true_colour (void);
void 	z_show_status (void);
void 	z_sound_effect (void);
void 	z_split_window (void);
void 	z_store (void);
void 	z_storeb (void);
void 	z_storew (void);
void 	z_sub (void);
void 	z_test (void);
void 	z_test_attr (void);
void 	z_throw (void);
void 	z_tokenise (void);
void 	z_verify (void);
void 	z_window_size (void);
void 	z_window_style (void);

/* Definitions for error handling functions and error codes. */

/* extern int err_report_mode; */

void	init_err (void);
void	runtime_error (int);
 
/* Error codes */
#define ERR_TEXT_BUF_OVF 1	/* Text buffer overflow */
#define ERR_STORE_RANGE 2	/* Store out of dynamic memory */
#define ERR_DIV_ZERO 3		/* Division by zero */
#define ERR_ILL_OBJ 4		/* Illegal object */
#define ERR_ILL_ATTR 5		/* Illegal attribute */
#define ERR_NO_PROP 6		/* No such property */
#define ERR_STK_OVF 7		/* Stack overflow */
#define ERR_ILL_CALL_ADDR 8	/* Call to illegal address */
#define ERR_CALL_NON_RTN 9	/* Call to non-routine */
#define ERR_STK_UNDF 10		/* Stack underflow */
#define ERR_ILL_OPCODE 11	/* Illegal opcode */
#define ERR_BAD_FRAME 12	/* Bad stack frame */
#define ERR_ILL_JUMP_ADDR 13	/* Jump to illegal address */
#define ERR_SAVE_IN_INTER 14	/* Can't save while in interrupt */
#define ERR_STR3_NESTING 15	/* Nesting stream #3 too deep */
#define ERR_ILL_WIN 16		/* Illegal window */
#define ERR_ILL_WIN_PROP 17	/* Illegal window property */
#define ERR_ILL_PRINT_ADDR 18	/* Print at illegal address */
#define ERR_DICT_LEN 19	/* Illegal dictionary word length */
#define ERR_MAX_FATAL 19

/* Less serious errors */
#define ERR_JIN_0 20		/* @jin called with object 0 */
#define ERR_GET_CHILD_0 21	/* @get_child called with object 0 */
#define ERR_GET_PARENT_0 22	/* @get_parent called with object 0 */
#define ERR_GET_SIBLING_0 23	/* @get_sibling called with object 0 */
#define ERR_GET_PROP_ADDR_0 24	/* @get_prop_addr called with object 0 */
#define ERR_GET_PROP_0 25	/* @get_prop called with object 0 */
#define ERR_PUT_PROP_0 26	/* @put_prop called with object 0 */
#define ERR_CLEAR_ATTR_0 27	/* @clear_attr called with object 0 */
#define ERR_SET_ATTR_0 28	/* @set_attr called with object 0 */
#define ERR_TEST_ATTR_0 29	/* @test_attr called with object 0 */
#define ERR_MOVE_OBJECT_0 30	/* @move_object called moving object 0 */
#define ERR_MOVE_OBJECT_TO_0 31	/* @move_object called moving into object 0 */
#define ERR_REMOVE_OBJECT_0 32	/* @remove_object called with object 0 */
#define ERR_GET_NEXT_PROP_0 33	/* @get_next_prop called with object 0 */
#define ERR_NUM_ERRORS (33)
 
/* There are four error reporting modes: never report errors;
  report only the first time a given error type occurs; report
  every time an error occurs; or treat all errors as fatal
  errors, killing the interpreter. I strongly recommend
  "report once" as the default. But you can compile in a
  different default by changing the definition of
  ERR_DEFAULT_REPORT_MODE. In any case, the player can
  specify a report mode on the command line by typing "-Z 0"
  through "-Z 3". */

#define ERR_REPORT_NEVER (0)
#define ERR_REPORT_ONCE (1)
#define ERR_REPORT_ALWAYS (2)
#define ERR_REPORT_FATAL (3)

#define ERR_DEFAULT_REPORT_MODE ERR_REPORT_NEVER


/*** Various global functions ***/

zchar	translate_from_zscii (zbyte);
zbyte	translate_to_zscii (zchar);

void 	flush_buffer (void);
void	new_line (void);
void	print_char (zchar);
void	print_num (zword);
void	print_object (zword);
void 	print_string (const char *);

void 	stream_mssg_on (void);
void 	stream_mssg_off (void);

void	ret (zword);
void 	store (zword);
void 	branch (bool);

void	storeb (zword, zbyte);
void	storew (zword, zword);

/*** Interface functions ***/

void 	os_beep (int);
int 	os_buffer_screen (int);
int  	os_char_width (zchar);
int  	os_check_unicode (int, zchar);
void 	os_display_char (zchar);
void 	os_display_string (const zchar *);
void 	os_display_cstring (const char *);
void 	os_draw_picture (int, int, int);
void 	os_erase_area (int, int, int, int, int);
void 	os_fatal (const char *);
void 	os_finish_with_sample (int);
int  	os_font_data (int, int *, int *);
int 	os_from_true_colour (zword);
void 	os_init_screen (void);
void	os_init_setup (void);
void 	os_menu(int, int, const zword *);
void 	os_more_prompt (void);
int  	os_peek_colour (void);
int  	os_picture_data (int, int *, int *);
void 	os_prepare_sample (int);
void 	os_process_arguments (int, char *[]);
int	os_random_seed (void);
int  	os_read_file_name (char *, const char *, int);
zchar	os_read_key (int, int);
zchar	os_read_line (int, zchar *, int, int, int);
zword	os_read_mouse (void);
void 	os_reset_screen (void);
void 	os_restart_game (int);
void 	os_scroll_area (int, int, int, int, int);
void 	os_scrollback_char (zchar);
void 	os_scrollback_erase (int);
void 	os_set_colour (int, int);
void 	os_set_cursor (int, int);
void 	os_set_font (int);
void 	os_set_text_style (int);
void 	os_start_sample (int, int, int, zword);
void 	os_stop_sample (int);
int  	os_string_width (const zchar *);
int  	os_string_length (zchar *);
void 	os_tick (void);
zword 	os_to_true_colour (int);
int	os_wrap_window (int);
void	os_window_height (int, int);

void seed_random (int);
void restart_screen (void);
void refresh_text_style (void);
void call (zword, int, zword *, int);
void split_window (zword);
void script_open (void);
void script_close (void);
void init_buffer (void);

//FILE *os_path_open (const char *, const char *, long *);

//zword save_quetzal (FILE *, zbyte *);
//zword restore_quetzal (FILE *, zbyte *);

void erase_window (zword);

extern void (*op0_opcodes[]) (void);
extern void (*op1_opcodes[]) (void);
extern void (*op2_opcodes[]) (void);
extern void (*var_opcodes[]) (void);

extern zchar* decoded;
extern zchar* encoded;
