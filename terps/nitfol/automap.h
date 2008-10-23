/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i automap.c' */
#ifndef CFH_AUTOMAP_H
#define CFH_AUTOMAP_H

/* From `automap.c': */

#ifdef DEBUGGING
extern char * roomsymbol;
void automap_kill (void);
BOOL automap_init (int numobj , const char *location_exp );
void mymap_init (int width , int height );
int automap_get_height (void);
void mymap_reinit (void);
void mymap_kill (void);
glui32 automap_draw_callback (winid_t win , glui32 width , glui32 height );
BOOL automap_mouse_callback (BOOL is_char_event , winid_t win , glui32 x , glui32 y );
void make_untouched (const char *unused_key , void *r );
void automap_set_locations (int center );
extern zword automap_location;
const char * automap_explore (void);
BOOL automap_unexplore (void);

#else
extern char * roomsymbol;
BOOL automap_unexplore (void);

#endif

#endif /* CFH_AUTOMAP_H */
