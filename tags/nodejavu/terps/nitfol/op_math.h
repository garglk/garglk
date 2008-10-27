/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i op_math.c' */
#ifndef CFH_OP_MATH_H
#define CFH_OP_MATH_H

/* From `op_math.c': */
void op_load (void);
void op_store (void);
void op_add (void);
void op_sub (void);
void op_and (void);
void op_or (void);
void op_not (void);
void op_art_shift (void);
void op_log_shift (void);
void op_dec (void);
void op_dec_chk (void);
void op_inc (void);
void op_inc_chk (void);
zword z_mult (zword a , zword b );
zword z_div (zword a , zword b );
zword z_mod (zword a , zword b );
void op_div (void);
void op_mod (void);
void op_mul (void);
zword z_random (zword num );
void op_random (void);

#endif /* CFH_OP_MATH_H */
