/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i op_table.c' */
#ifndef CFH_OP_TABLE_H
#define CFH_OP_TABLE_H

/* From `op_table.c': */
void op_copy_table (void);
void op_scan_table (void);
void op_loadb (void);
void op_loadw (void);
void op_storeb (void);
void op_storew (void);
void header_extension_write (zword w , zword val );
zword header_extension_read (unsigned w );

#endif /* CFH_OP_TABLE_H */
