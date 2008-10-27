/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i tokenise.c' */
#ifndef CFH_TOKENISE_H
#define CFH_TOKENISE_H

/* From `tokenise.c': */

#ifdef SMART_TOKENISER
extern struct Typocorrection * recent_corrections;
void forget_corrections (void);

#endif
void z_tokenise (const char *text , int length , zword parse_dest , zword dictionarytable , BOOL write_unrecognized );
void op_tokenise (void);

#endif /* CFH_TOKENISE_H */
