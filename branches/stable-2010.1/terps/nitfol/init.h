/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i init.c' */
#ifndef CFH_INIT_H
#define CFH_INIT_H

/* From `init.c': */
void set_header (glui32 width , glui32 height );
BOOL load_header (strid_t zfile , offset filesize , BOOL report_errors );
void z_init (strid_t zfile );
void z_close (void);

#endif /* CFH_INIT_H */
