/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i oplist.c' */
#ifndef CFH_OPLIST_H
#define CFH_OPLIST_H

/* From `oplist.c': */
#define OFFSET_0OP 0x00
#define OFFSET_1OP 0x10
#define OFFSET_2OP 0x20
#define OFFSET_VAR 0x40
#define OFFSET_EXT 0x60
#define OFFSET_END 0x80
typedef void (*op_func)(void);
extern op_func opcodetable[];

#ifdef DEBUGGING
extern opcodeinfo opcodelist[];

#endif

#endif /* CFH_OPLIST_H */
