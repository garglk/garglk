/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i errmesg.c' */
#ifndef CFH_ERRMESG_H
#define CFH_ERRMESG_H

/* From `errmesg.c': */
typedef enum { E_INSTR, E_OBJECT, E_STACK,  E_MEMORY,  E_MATH,    E_STRING,
	       E_OUTPUT, E_SOUND, E_SYSTEM, E_VERSION, E_CORRUPT, E_SAVE,
	       E_DEBUG }errortypes;
void n_show_warn (errortypes type , const char *message , offset number );
void n_show_port (errortypes type , const char *message , offset number );
void n_show_error (errortypes type , const char *message , offset number );
void n_show_fatal (errortypes type , const char *message , offset number );
void n_show_debug (errortypes type , const char *message , offset number );
zword z_range_error (offset p );

#endif /* CFH_ERRMESG_H */
