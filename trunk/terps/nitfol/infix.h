/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i infix.c' */
#ifndef CFH_INFIX_H
#define CFH_INFIX_H

/* From `infix.c': */


#ifdef DEBUGGING
typedef enum { Z_UNKNOWN, Z_BOOLEAN, Z_NUMBER, Z_OBJECT, Z_ROUTINE, Z_STRING, Z_GLOBAL, Z_LOCAL, Z_BYTEARRAY, Z_WORDARRAY, Z_OBJPROP, Z_ATTR, Z_PROP, Z_ARRAY }z_type;
typedef struct z_typed z_typed;
struct z_typed {
  zword v;      
  z_type t;

  zword o, p;   
} 
;
typedef struct {
  const char *filename;
  strid_t stream;
  int num_lines;
  glui32 *line_locations;
} 
infix_file;
typedef struct {
  infix_file *file;
  int line_num;
  int line_x;
  const char *func_name;
  unsigned func_num;
  offset thisPC;
} 
infix_location;
offset infix_get_routine_PC (zword routine );
void infix_file_print_line (infix_file *f , int line );
BOOL init_infix (strid_t infix );
void kill_infix (void);
void infix_print_znumber (zword blah );
void infix_print_offset (zword blah );
void infix_print_number (zword blah );
void infix_print_char (int blah );
void infix_print_fixed_char (int blah );
void infix_print_string (const char *blah );
void infix_print_fixed_string (const char *blah );
void infix_get_string (char *dest , int maxlen );
void infix_get_val (z_typed *val );
void infix_assign (z_typed *dest , zword val );
void infix_display (z_typed val );
int infix_find_file (infix_file **dest , const char *name );
BOOL infix_find_symbol (z_typed *val , const char *name , int len );
const char * infix_get_name (z_typed val );
BOOL infix_decode_PC (infix_location *dest , offset thisPC );
BOOL infix_decode_fileloc (infix_location *dest , const char *filename , unsigned line_num );
BOOL infix_decode_func_name (infix_location *dest , const char *file_name , const char *func_name );
void infix_gprint_loc (int frame , offset thisPC );
void infix_list_files (void);

#else
BOOL init_infix (strid_t unused );

#endif

#endif /* CFH_INFIX_H */
