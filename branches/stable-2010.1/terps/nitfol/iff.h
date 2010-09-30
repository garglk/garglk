/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i iff.c' */
#ifndef CFH_IFF_H
#define CFH_IFF_H

/* From `iff.c': */
BOOL iffgetchunk (strid_t stream , char *desttype , glui32 *ulength , glui32 file_size );
BOOL ifffindchunk (strid_t stream , const char *type , glui32 *length , glui32 loc );
void iffputchunk (strid_t stream , const char *type , glui32 ulength );
void iffpadend (strid_t stream );

#endif /* CFH_IFF_H */
