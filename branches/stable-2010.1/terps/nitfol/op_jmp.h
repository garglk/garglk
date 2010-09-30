/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i op_jmp.c' */
#ifndef CFH_OP_JMP_H
#define CFH_OP_JMP_H

/* From `op_jmp.c': */
void mop_skip_branch (void);
void mop_take_branch (void);
void mop_cond_branch (BOOL cond );
void op_je (void);
void op_jg (void);
void op_jl (void);
void op_jump (void);
void op_jz (void);
void op_test (void);
void op_verify (void);
void op_piracy (void);

#endif /* CFH_OP_JMP_H */
