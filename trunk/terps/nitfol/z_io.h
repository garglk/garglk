/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i z_io.c' */
#ifndef CFH_Z_IO_H
#define CFH_Z_IO_H

/* From `z_io.c': */
BOOL is_transcripting (void);
void set_transcript (strid_t stream );
void init_windows (BOOL dofixed , glui32 maxwidth , glui32 maxheight );
glui32 draw_upper_callback (winid_t win , glui32 width , glui32 height );
void output_string (const char *s );
void output_char (int c );
void n_print_number (unsigned n );
void g_print_number (unsigned n );
void g_print_snumber (int n );
void g_print_znumber (zword n );
void n_print_znumber (zword n );
void stream4number (unsigned c );
void op_buffer_mode (void);
void op_check_unicode (void);
void op_erase_line (void);
void op_erase_window (void);
void op_get_cursor (void);
void op_new_line (void);
void op_output_stream (void);
void op_print (void);
void op_print_ret (void);
void op_print_addr (void);
void op_print_paddr (void);
void op_print_char (void);
void op_print_num (void);
void op_print_table (void);
void op_set_colour (void);
void op_set_cursor (void);
void op_set_text_style (void);
void op_set_window (void);
void op_split_window (void);
BOOL upper_mouse_callback (BOOL is_char_event , winid_t win , glui32 x , glui32 y );
void parse_new_alias (const char *aliascommand , BOOL is_recursive );
void add_alias (const char *from , const char *to , BOOL is_recursive );
BOOL remove_alias (const char *from );
int search_for_aliases (char *text , int length , int maxlen );
int n_read (zword dest , unsigned maxlen , zword parse , unsigned initlen , zword timer , zword routine , unsigned char *terminator );
void stream4line (const char *buffer , int length , char terminator );
void op_sread (void);
void op_aread (void);
void op_read_char (void);
void op_show_status (void);
unsigned char transcript_getchar (unsigned *num );
unsigned char transcript_getline (char *dest , glui32 *length );
void op_input_stream (void);
void op_set_font (void);
void op_print_unicode (void);

#endif /* CFH_Z_IO_H */
