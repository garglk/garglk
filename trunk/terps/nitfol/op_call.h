/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i op_call.c' */
#ifndef CFH_OP_CALL_H
#define CFH_OP_CALL_H

/* From `op_call.c': */
void mop_call (zword dest , unsigned num_args , zword *args , int result_var );
void op_call_n (void);
void op_call_s (void);
void op_ret (void);
void op_rfalse (void);
void op_rtrue (void);

#endif /* CFH_OP_CALL_H */
