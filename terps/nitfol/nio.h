/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i io.c' */
#ifndef CFH_IO_H
#define CFH_IO_H

/* From `io.c': */
typedef struct z_window * zwinid;
extern BOOL is_fixed;
extern glsi32 bgcolortable[];
extern glsi32 fgcolortable[];
void set_glk_stream_current (void);
void draw_intext_picture (zwinid window , glui32 picture , glui32 alignment );
void draw_picture (zwinid window , glui32 picture , glui32 x , glui32 y );
void showstuff (const char *title , const char *type , const char *message , offset number );
void init_lower (zwinid *lower );
void init_upper (zwinid *upper );
void z_init_windows (BOOL dofixed , glui32 ( *draw_callback ) ( winid_t , glui32 , glui32 ) , BOOL ( *mouse_callback ) ( BOOL , winid_t , glui32 , glui32 ) , glui32 maxwidth , glui32 maxheight , zwinid *upper , zwinid *lower );
zwinid z_split_screen (glui32 wintype , glui32 method , glui32 ( *draw_callback ) ( winid_t , glui32 , glui32 ) , BOOL ( *mouse_callback ) ( BOOL , winid_t , glui32 , glui32 ) );
void z_kill_window (zwinid win );
void kill_windows (void);
void free_windows (void);
zwinid z_find_win (winid_t win );
void z_pause_timed_input (zwinid window );
void z_flush_all_windows (void);
void z_draw_all_windows (void);
void z_flush_fixed (zwinid window );
void z_flush_text (zwinid window );
void z_flush_graphics (zwinid window );
void z_print_number (zwinid window , int number );
void z_put_char (zwinid window , unsigned c );
void z_setxy (zwinid window , zword x , zword y );
void z_getxy (zwinid window , zword *x , zword *y );
void z_getsize (zwinid window , unsigned *width , unsigned *height );
void z_find_size (glui32 *wid , glui32 *hei );
void z_set_height (zwinid window , unsigned height );
void z_set_color (zwinid window , unsigned fore , unsigned back );
void z_set_style (zwinid window , int style );
void set_fixed (BOOL p );
void z_set_transcript (zwinid window , strid_t stream );
void z_clear_window (zwinid window );
void z_erase_line (zwinid window );
void z_wait_for_key (zwinid window );
zwinid check_valid_for_input (zwinid window );
int z_read (zwinid window , char *dest , unsigned maxlen , unsigned initlen , zword timer , BOOL ( *timer_callback ) ( zword ) , zword timer_arg , unsigned char *terminator );
zword z_read_char (zwinid window , zword timer , BOOL ( *timer_callback ) ( zword ) , zword timer_arg );

#endif /* CFH_IO_H */
