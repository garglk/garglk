/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i struct.c' */
#ifndef CFH_STRUCT_H
#define CFH_STRUCT_H

/* From `struct.c': */
glui32 fillstruct (strid_t stream , const unsigned *info , glui32 *dest , glui32 ( *special ) ( strid_t ) );
glui32 emptystruct (strid_t stream , const unsigned *info , const glui32 *src );

#endif /* CFH_STRUCT_H */
