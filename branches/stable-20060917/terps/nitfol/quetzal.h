/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i quetzal.c' */
#ifndef CFH_QUETZAL_H
#define CFH_QUETZAL_H

/* From `quetzal.c': */
BOOL quetzal_diff (const zbyte *a , const zbyte *b , glui32 length , zbyte **diff , glui32 *diff_length , BOOL do_utf8 );
BOOL quetzal_undiff (zbyte *dest , glui32 length , const zbyte *diff , glui32 diff_length , BOOL do_utf8 );
BOOL savequetzal (strid_t stream );
BOOL restorequetzal (strid_t stream );
strid_t quetzal_findgamefile (strid_t stream );

#endif /* CFH_QUETZAL_H */
