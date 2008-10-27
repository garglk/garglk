/* $Header: d:/cvsroot/tads/tads3/TCT3UNAS.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3unas.h - TADS 3 Compiler - T3 Unassembler
Function
  Takes a T3 byte-code stream and disassembles it to a printable display
Notes
  
Modified
  05/10/99 MJRoberts  - Creation
*/

#ifndef TCT3UNAS_H
#define TCT3UNAS_H

#include "tcunas.h"

/* ------------------------------------------------------------------------ */
/* 
 *   operand types, for instruction descriptions 
 */
enum t3_oper_type_t
{
    /* 8-bit signed integer */
    T3OP_TYPE_8S,

    /* 8-bit unsigned integer */
    T3OP_TYPE_8U,

    /* 16-bit signed integer */
    T3OP_TYPE_16S,

    /* 16-bit unsigned integer */
    T3OP_TYPE_16U,

    /* 32-bit signed integer */
    T3OP_TYPE_32S,

    /* 32-bit unsigned integer */
    T3OP_TYPE_32U,

    /* string offset */
    T3OP_TYPE_STR,

    /* list offset */
    T3OP_TYPE_LIST,

    /* absolute (32-bit) code address */
    T3OP_TYPE_CODE_ABS,

    /* relative (16-bit) code address */
    T3OP_TYPE_CODE_REL,

    /* object ID */
    T3OP_TYPE_OBJ,

    /* property ID */
    T3OP_TYPE_PROP,

    /* enum ID */
    T3OP_TYPE_ENUM,

    /* context element type */
    T3OP_TYPE_CTX_ELE,

    /* in-line string */
    T3OP_TYPE_INLINE_STR
};


/* ------------------------------------------------------------------------ */
/* 
 *   Instruction information - this structure describes one T3 byte-code
 *   instruction.  
 */
struct t3_instr_info_t
{
    /* name of the instruction */
    const char *nm;

    /* number of operands */
    int op_cnt;

    /* 
     *   operand types (allow for a fixed upper limit to the number of
     *   operands) 
     */
    t3_oper_type_t op_type[3];
};


/* ------------------------------------------------------------------------ */
/*
 *   T3 Disassembler 
 */
class CTcT3Unasm
{
public:
    /* disassemble a byte stream */
    static void disasm(class CTcUnasSrc *src, class CTcUnasOut *out);

    /* show an exception table */
    static void show_exc_table(class CTcUnasSrc *src, class CTcUnasOut *out,
                               unsigned long base_ofs);

    /* get the instruction information array (read-only) */
    static const t3_instr_info_t *get_instr_info() { return instr_info; }

protected:
    /* disassemble an instruction */
    static void disasm_instr(class CTcUnasSrc *src, class CTcUnasOut *out,
                             char ch);

    /* instruction information array */
    static t3_instr_info_t instr_info[];
};

#endif /* TCT3UNAS_H */

