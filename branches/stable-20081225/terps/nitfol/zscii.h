/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i zscii.c' */
#ifndef CFH_ZSCII_H
#define CFH_ZSCII_H

/* From `zscii.c': */
int getstring (zword packedaddress );
int decodezscii (offset zscii , void ( *putcharfunc ) ( int ) );
int encodezscii (zbyte *dest , int mindestlen , int maxdestlen , const char *source , int sourcelen );
void op_encode_text (void);

#endif /* CFH_ZSCII_H */
