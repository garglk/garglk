/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i op_v6.c' */
#ifndef CFH_OP_V6_H
#define CFH_OP_V6_H

/* From `op_v6.c': */
int is_in_bounds (glui32 x1 , glui32 y1 , glui32 width1 , glui32 height1 , glui32 x2 , glui32 y2 , glui32 width2 , glui32 height2 );
void v6_main_window_is (zwinid win );
void op_set_window6 (void);
void op_set_margins (void);
void op_move_window (void);
void op_window_size (void);
void op_window_style (void);
void op_get_wind_prop (void);
void op_put_wind_prop (void);
void op_scroll_window (void);
void op_read_mouse (void);
void op_mouse_window (void);
void op_print_form (void);
void op_make_menu (void);
void op_picture_table (void);
void op_draw_picture (void);
void op_picture_data (void);
void op_erase_picture (void);

#endif /* CFH_OP_V6_H */
