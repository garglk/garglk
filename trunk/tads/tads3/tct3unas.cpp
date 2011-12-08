#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCT3UNAS.CPP,v 1.3 1999/07/11 00:46:57 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3unas.cpp - TADS 3 Compiler - T3 Unassembler
Function
  
Notes
  
Modified
  05/10/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmop.h"
#include "tcunas.h"
#include "tct3unas.h"
#include "vmtype.h"


/* ------------------------------------------------------------------------ */
/*
 *   disassemble a code stream 
 */
void CTcT3Unasm::disasm(CTcUnasSrc *src, CTcUnasOut *out)
{
    /* keep going until we run out of source material */
    for (;;)
    {
        char ch;
        
        /* get the next byte; stop if we've reached the end of the source */
        if (src->next_byte(&ch))
            break;

        /* disassemble this instruction */
        disasm_instr(src, out, ch);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   T3 Instruction Information Array 
 */
t3_instr_info_t CTcT3Unasm::instr_info[] =
{
    /* 0x00 */ { "db 0",  0,},
    /* 0x01 */ { "push_0",     0 },
    /* 0x02 */ { "push_1",     0 },
    /* 0x03 */ { "pushint8",   1, { T3OP_TYPE_8S }},
    /* 0x04 */ { "pushint",    1, { T3OP_TYPE_32S }},
    /* 0x05 */ { "pushstr",    1, { T3OP_TYPE_STR }},
    /* 0x06 */ { "pushlst",    1, { T3OP_TYPE_LIST }},
    /* 0x07 */ { "pushobj",    1, { T3OP_TYPE_OBJ }},
    /* 0x08 */ { "pushnil",    0 },
    /* 0x09 */ { "pushtrue",   0 },
    /* 0x0a */ { "pushpropid", 1, { T3OP_TYPE_PROP }},
    /* 0x0b */ { "pushfnptr",  1, { T3OP_TYPE_CODE_ABS }},
    /* 0x0c */ { "pushstri",   1, { T3OP_TYPE_INLINE_STR }},
    /* 0x0d */ { "pushparlst", 1, { T3OP_TYPE_8U }},
    /* 0x0e */ { "makelstpar", 0 },
    /* 0x0f */ { "pushenum",   1, { T3OP_TYPE_ENUM }},
    /* 0x10 */ { "pushbifptr", 2, { T3OP_TYPE_16U, T3OP_TYPE_16U }},
    /* 0x11 */ { "db 0x11", 0 },
    /* 0x12 */ { "db 0x12", 0 },
    /* 0x13 */ { "db 0x13", 0 },
    /* 0x14 */ { "db 0x14", 0 },
    /* 0x15 */ { "db 0x15", 0 },
    /* 0x16 */ { "db 0x16", 0 },
    /* 0x17 */ { "db 0x17", 0 },
    /* 0x18 */ { "db 0x18", 0 },
    /* 0x19 */ { "db 0x19", 0 },
    /* 0x1a */ { "db 0x1a", 0 },
    /* 0x1b */ { "db 0x1b", 0 },
    /* 0x1c */ { "db 0x1c", 0 },
    /* 0x1d */ { "db 0x1d", 0 },
    /* 0x1e */ { "db 0x1e", 0 },
    /* 0x1f */ { "db 0x1f", 0 },
    /* 0x20 */ { "neg", 0 },
    /* 0x21 */ { "bnot", 0 },
    /* 0x22 */ { "add", 0 },
    /* 0x23 */ { "sub", 0 },
    /* 0x24 */ { "mul", 0 },
    /* 0x25 */ { "band", 0 },
    /* 0x26 */ { "bor", 0 },
    /* 0x27 */ { "shl", 0 },
    /* 0x28 */ { "shr", 0 },
    /* 0x29 */ { "xor", 0 },
    /* 0x2a */ { "div", 0 },
    /* 0x2b */ { "mod", 0 },
    /* 0x2c */ { "not", 0 },
    /* 0x2d */ { "boolize", 0 },
    /* 0x2e */ { "inc", 0 },
    /* 0x2f */ { "dec", 0 },
    /* 0x30 */ { "db 0x30", 0 },
    /* 0x31 */ { "db 0x31", 0 },
    /* 0x32 */ { "db 0x32", 0 },
    /* 0x33 */ { "db 0x33", 0 },
    /* 0x34 */ { "db 0x34", 0 },
    /* 0x35 */ { "db 0x35", 0 },
    /* 0x36 */ { "db 0x36", 0 },
    /* 0x37 */ { "db 0x37", 0 },
    /* 0x38 */ { "db 0x38", 0 },
    /* 0x39 */ { "db 0x39", 0 },
    /* 0x3a */ { "db 0x3a", 0 },
    /* 0x3b */ { "db 0x3b", 0 },
    /* 0x3c */ { "db 0x3c", 0 },
    /* 0x3d */ { "db 0x3d", 0 },
    /* 0x3e */ { "db 0x3e", 0 },
    /* 0x3f */ { "db 0x3f", 0 },
    /* 0x40 */ { "eq", 0 },
    /* 0x41 */ { "ne", 0 },
    /* 0x42 */ { "lt", 0 },
    /* 0x43 */ { "le", 0 },
    /* 0x44 */ { "gt", 0 },
    /* 0x45 */ { "ge", 0 },
    /* 0x46 */ { "db 0x46", 0 },
    /* 0x47 */ { "db 0x47", 0 },
    /* 0x48 */ { "db 0x48", 0 },
    /* 0x49 */ { "db 0x49", 0 },
    /* 0x4a */ { "db 0x4a", 0 },
    /* 0x4b */ { "db 0x4b", 0 },
    /* 0x4c */ { "db 0x4c", 0 },
    /* 0x4d */ { "db 0x4d", 0 },
    /* 0x4e */ { "db 0x4e", 0 },
    /* 0x4f */ { "db 0x4f", 0 },
    /* 0x50 */ { "retval", 0 },
    /* 0x51 */ { "retnil", 0 },
    /* 0x52 */ { "rettrue", 0 },
    /* 0x53 */ { "db 0x53", 0 },
    /* 0x54 */ { "ret", 0 },
    /* 0x55 */ { "db 0x55", 0 },
    /* 0x56 */ { "namedargptr", 2, { T3OP_TYPE_8U, T3OP_TYPE_16U }},
    /* 0x57 */ { "namedargtab", 0 },             /* varying-length operands */
    /* 0x58 */ { "call", 2, { T3OP_TYPE_8U, T3OP_TYPE_CODE_ABS }},
    /* 0x59 */ { "ptrcall", 1, { T3OP_TYPE_8U }},
    /* 0x5a */ { "db 0x5a", 0 },
    /* 0x5b */ { "db 0x5b", 0 },
    /* 0x5c */ { "db 0x5c", 0 },
    /* 0x5d */ { "db 0x5d", 0 },
    /* 0x5e */ { "db 0x5e", 0 },
    /* 0x5f */ { "db 0x5f", 0 },
    /* 0x60 */ { "getprop", 1, { T3OP_TYPE_PROP }},
    /* 0x61 */ { "callprop", 2, { T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x62 */ { "ptrcallprop", 1, { T3OP_TYPE_8U }},
    /* 0x63 */ { "getpropself", 1, { T3OP_TYPE_PROP }},
    /* 0x64 */ { "callpropself", 2, { T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x65 */ { "ptrcallpropself", 1, { T3OP_TYPE_8U }},
    /* 0x66 */ { "objgetprop", 2, { T3OP_TYPE_OBJ, T3OP_TYPE_PROP }},
    /* 0x67 */ { "objcallprop", 3,
                   { T3OP_TYPE_8U, T3OP_TYPE_OBJ, T3OP_TYPE_PROP }},
    /* 0x68 */ { "getpropdata", 1, { T3OP_TYPE_PROP }},
    /* 0x69 */ { "ptrgetpropdata", 0 },
    /* 0x6a */ { "getproplcl1", 2, { T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x6b */ { "callproplcl1", 3,
                   { T3OP_TYPE_8U, T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x6c */ { "getpropr0", 1, { T3OP_TYPE_PROP }},
    /* 0x6d */ { "callpropr0", 2, { T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x6e */ { "db 0x6e", 0 },
    /* 0x6f */ { "db 0x6f", 0 },
    /* 0x70 */ { "db 0x70", 0 },
    /* 0x71 */ { "db 0x71", 0 },
    /* 0x72 */ { "inherit", 2, { T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x73 */ { "ptrinherit", 1, { T3OP_TYPE_8U }},
    /* 0x74 */ { "expinherit", 3,
                   { T3OP_TYPE_8U, T3OP_TYPE_PROP, T3OP_TYPE_OBJ }},
    /* 0x75 */ { "ptrexpinherit", 2, { T3OP_TYPE_8U, T3OP_TYPE_OBJ }},
    /* 0x76 */ { "varargc", 0 },
    /* 0x77 */ { "delegate",   2, { T3OP_TYPE_8U, T3OP_TYPE_PROP }},
    /* 0x78 */ { "ptrdelegate", 1, { T3OP_TYPE_PROP }},
    /* 0x79 */ { "db 0x79", 0 },
    /* 0x7a */ { "swap2", 0 },
    /* 0x7b */ { "swapn", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0x7c */ { "getargn0", 0 },
    /* 0x7d */ { "argargn1", 0 },
    /* 0x7e */ { "getargn2", 0 },
    /* 0x7f */ { "getargn3", 0 },
    /* 0x80 */ { "getlcl1", 1, { T3OP_TYPE_8U }},
    /* 0x81 */ { "getlcl2", 1, { T3OP_TYPE_16U }},
    /* 0x82 */ { "getarg1", 1, { T3OP_TYPE_8U }},
    /* 0x83 */ { "getarg2", 1, { T3OP_TYPE_16U }},
    /* 0x84 */ { "pushself", 0 },
    /* 0x85 */ { "getdblcl", 2, { T3OP_TYPE_16U, T3OP_TYPE_16U }},
    /* 0x86 */ { "getdbarg", 2, { T3OP_TYPE_16U, T3OP_TYPE_16U }},
    /* 0x87 */ { "getargc", 0 },
    /* 0x88 */ { "dup", 0 },
    /* 0x89 */ { "disc", 0 },
    /* 0x8a */ { "disc1", 1, { T3OP_TYPE_8U }},
    /* 0x8b */ { "getr0", 0 },
    /* 0x8c */ { "getdbargc", 1, { T3OP_TYPE_16U }},
    /* 0x8d */ { "swap", 0 },
    /* 0x8e */ { "pushctxele", 1, { T3OP_TYPE_CTX_ELE }},
    /* 0x8f */ { "dup2", 0 },
    /* 0x90 */ { "switch", 0 },                  /* varying-length operands */
    /* 0x91 */ { "jmp", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x92 */ { "jt", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x93 */ { "jf", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x94 */ { "je", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x95 */ { "jne", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x96 */ { "jgt", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x97 */ { "jge", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x98 */ { "jlt", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x99 */ { "jle", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x9a */ { "jst", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x9b */ { "jsf", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x9c */ { "ljsr", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x9d */ { "lret", 1, { T3OP_TYPE_16U }},
    /* 0x9e */ { "jnil", 1, { T3OP_TYPE_CODE_REL }},
    /* 0x9f */ { "jnotnil", 1, { T3OP_TYPE_CODE_REL }},
    /* 0xa0 */ { "jr0t", 1, { T3OP_TYPE_CODE_REL }},
    /* 0xa1 */ { "jr0f", 1, { T3OP_TYPE_CODE_REL }},
    /* 0xa2 */ { "iternext", 2, { T3OP_TYPE_16U, T3OP_TYPE_CODE_REL }},
    /* 0xa3 */ { "db 0xa3", 0 },
    /* 0xa4 */ { "db 0xa4", 0 },
    /* 0xa5 */ { "db 0xa5", 0 },
    /* 0xa6 */ { "getspn", 1, { T3OP_TYPE_8U }},
    /* 0xa7 */ { "db 0xa7", 0 },
    /* 0xa8 */ { "db 0xa8", 0 },
    /* 0xa9 */ { "db 0xa9", 0 },
    /* 0xaa */ { "getlcln1", 0 },
    /* 0xab */ { "getlcln1", 0 },
    /* 0xac */ { "getlcln2", 0 },
    /* 0xad */ { "getlcln3", 0 },
    /* 0xae */ { "getlcln4", 0 },
    /* 0xaf */ { "getlcln5", 0 },
    /* 0xb0 */ { "say", 1, { T3OP_TYPE_STR }},
    /* 0xb1 */ { "builtin_a", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xb2 */ { "builtin_b", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xb3 */ { "builtin_c", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xb4 */ { "builtin_d", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xb5 */ { "builtin1", 3, { T3OP_TYPE_8U, T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xb6 */ { "builtin2", 0, { T3OP_TYPE_16U, T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xb7 */ { "callext", 0 },
    /* 0xb8 */ { "throw", 0 },
    /* 0xb9 */ { "sayval", 0 },
    /* 0xba */ { "index", 0 },
    /* 0xbb */ { "idxlcl1int8", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xbc */ { "idxint8", 1, { T3OP_TYPE_8U }},
    /* 0xbd */ { "db 0xbd", 0 },
    /* 0xbe */ { "db 0xbe", 0 },
    /* 0xbf */ { "db 0xbf", 0 },
    /* 0xc0 */ { "new1", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xc1 */ { "new2", 2, { T3OP_TYPE_16U, T3OP_TYPE_8U }},
    /* 0xc2 */ { "trnew1", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xc3 */ { "trnew2", 2, { T3OP_TYPE_16U, T3OP_TYPE_8U }},
    /* 0xc4 */ { "db 0xc4", 0 },
    /* 0xc5 */ { "db 0xc5", 0 },
    /* 0xc6 */ { "db 0xc6", 0 },
    /* 0xc7 */ { "db 0xc7", 0 },
    /* 0xc8 */ { "db 0xc8", 0 },
    /* 0xc9 */ { "db 0xc9", 0 },
    /* 0xca */ { "db 0xca", 0 },
    /* 0xcb */ { "db 0xcb", 0 },
    /* 0xcc */ { "db 0xcc", 0 },
    /* 0xcd */ { "db 0xcd", 0 },
    /* 0xce */ { "db 0xce", 0 },
    /* 0xcf */ { "db 0xcf", 0 },
    /* 0xd0 */ { "inclcl", 1, { T3OP_TYPE_16U }},
    /* 0xd1 */ { "declcl", 1, { T3OP_TYPE_16U }},
    /* 0xd2 */ { "addilcl1", 2, { T3OP_TYPE_8U, T3OP_TYPE_8S }},
    /* 0xd3 */ { "addilcl4", 2, { T3OP_TYPE_16U, T3OP_TYPE_32S }},
    /* 0xd4 */ { "addtolcl", 1, { T3OP_TYPE_16U }},
    /* 0xd5 */ { "subfromlcl", 1, { T3OP_TYPE_16U }},
    /* 0xd6 */ { "zerolcl1", 1, { T3OP_TYPE_8U }},
    /* 0xd7 */ { "zerolcl2", 1, { T3OP_TYPE_16U }},
    /* 0xd8 */ { "nillcl1", 1, { T3OP_TYPE_8U }},
    /* 0xd9 */ { "nillcl2", 1, { T3OP_TYPE_16U }},
    /* 0xda */ { "onelcl1", 1, { T3OP_TYPE_8U }},
    /* 0xdb */ { "onelcl2", 1, { T3OP_TYPE_16U }},
    /* 0xdc */ { "db 0xdc", 0 },
    /* 0xdd */ { "db 0xdd", 0 },
    /* 0xde */ { "db 0xde", 0 },
    /* 0xdf */ { "db 0xdf", 0 },
    /* 0xe0 */ { "setlcl1", 1, { T3OP_TYPE_8U }},
    /* 0xe1 */ { "setlcl2", 1, { T3OP_TYPE_16U }},
    /* 0xe2 */ { "setarg1", 1, { T3OP_TYPE_8U }},
    /* 0xe3 */ { "setarg2", 1, { T3OP_TYPE_16U }},
    /* 0xe4 */ { "setind", 0 },
    /* 0xe5 */ { "setprop", 1, { T3OP_TYPE_PROP }},
    /* 0xe6 */ { "ptrsetprop", 0 },
    /* 0xe7 */ { "setpropself", 1, { T3OP_TYPE_PROP }},
    /* 0xe8 */ { "objsetprop", 2, { T3OP_TYPE_OBJ, T3OP_TYPE_PROP }},
    /* 0xe9 */ { "setdblcl", 2, { T3OP_TYPE_16U, T3OP_TYPE_16U }},
    /* 0xea */ { "setdbarg", 2, { T3OP_TYPE_16U, T3OP_TYPE_16U }},
    /* 0xeb */ { "setself", 0 },
    /* 0xec */ { "loadctx", 0 },
    /* 0xed */ { "storectx", 0 },
    /* 0xee */ { "setlcl1r0", 1, { T3OP_TYPE_8U }},
    /* 0xef */ { "setindlcl1i8", 2, { T3OP_TYPE_8U, T3OP_TYPE_8U }},
    /* 0xf0 */ { "db 0xf0", 0 },
    /* 0xf1 */ { "bp", 0 },
    /* 0xf2 */ { "nop", 0 },
    /* 0xf3 */ { "db 0xf3", 0 },
    /* 0xf4 */ { "db 0xf4", 0 },
    /* 0xf5 */ { "db 0xf5", 0 },
    /* 0xf6 */ { "db 0xf6", 0 },
    /* 0xf7 */ { "db 0xf7", 0 },
    /* 0xf8 */ { "db 0xf8", 0 },
    /* 0xf9 */ { "db 0xf9", 0 },
    /* 0xfa */ { "db 0xfa", 0 },
    /* 0xfb */ { "db 0xfb", 0 },
    /* 0xfc */ { "db 0xfc", 0 },
    /* 0xfd */ { "db 0xfd", 0 },
    /* 0xfe */ { "db 0xfe", 0 },
    /* 0xff */ { "db 0xff", 0 }
};

/* ------------------------------------------------------------------------ */
/*
 *   disassemble an instruction
 */
void CTcT3Unasm::disasm_instr(CTcUnasSrc *src, CTcUnasOut *out, char ch_op)
{
    t3_instr_info_t *info;
    int i;
    ulong acc;
    char ch[10];
    ulong prv_ofs = src->get_ofs();
    
    /* get the information on this instruction */
    info = &instr_info[(int)(uchar)ch_op];
    out->print("%8lx  %-14.14s ", src->get_ofs() - 1, info->nm);

    /* check the instruction type */
    switch((uchar)ch_op)
    {
    case OPC_SWITCH:
        /* 
         *   It's a switch instruction - special handling is required,
         *   since this instruction doesn't fit any of the normal
         *   patterns.  First, get the number of elements in the case
         *   table - this is a UINT2 value at the start of the table.  
         */
        src->next_byte(ch);
        src->next_byte(ch+1);
        acc = osrp2(ch);

        /* display the count */
        out->print("0x%x\n", (uint)acc);

        /* display the table */
        for (i = 0 ; i < (int)acc ; ++i)
        {
            const char *dt;
            char valbuf[128];
            const char *val = valbuf;

            /* note the current offset */
            prv_ofs = src->get_ofs();
            
            /* read the DATAHOLDER value */
            src->next_byte(ch);
            src->next_byte(ch+1);
            src->next_byte(ch+2);
            src->next_byte(ch+3);
            src->next_byte(ch+4);

            /* read the jump offset */
            src->next_byte(ch+5);
            src->next_byte(ch+6);

            /* 
             *   stop looping if the offset hasn't changed - this probably
             *   means we're stuck trying to interpret as a "switch" some
             *   data at the end of the stream that happens to look like a
             *   switch but really isn't (such as an exception table, or
             *   debug records) 
             */
            if (src->get_ofs() == prv_ofs)
                break;

            /* show the value */
            switch(ch[0])
            {
            case VM_NIL:
                dt = "nil";
                val = "";
                break;
                
            case VM_TRUE:
                dt = "true";
                val = "";
                break;
                
            case VM_OBJ:
                dt = "object";
                sprintf(valbuf, "0x%08lx", t3rp4u(ch+1));
                break;
                
            case VM_PROP:
                dt = "prop";
                sprintf(valbuf, "0x%04x", osrp2(ch+1));
                break;
                
            case VM_INT:
                dt = "int";
                sprintf(valbuf, "0x%08lx", t3rp4u(ch+1));
                break;

            case VM_ENUM:
                dt = "enum";
                sprintf(valbuf, "0x%08lx", t3rp4u(ch+1));
                break;
                
            case VM_SSTRING:
                dt = "sstring";
                sprintf(valbuf, "0x%08lx", t3rp4u(ch+1));
                break;
                
            case VM_LIST:
                dt = "list";
                sprintf(valbuf, "0x%08lx", t3rp4u(ch+1));
                break;
                
            case VM_FUNCPTR:
                dt = "funcptr";
                sprintf(valbuf, "0x%08lx", t3rp4u(ch+1));
                break;

            default:
                dt = "???";
                val = "???";
                break;
            }

            /* show the slot data */
            out->print("          0x%08lx      %-8.8s(%-10.10s) "
                       "-> +0x%04lx (0x%08lx)\n",
                       src->get_ofs() - 7, dt, val, osrp2(ch+5),
                       src->get_ofs() - 2 + osrp2s(ch+5));
        }

        /* read and show the 'default' jump offset */
        src->next_byte(ch);
        src->next_byte(ch+1);
        out->print("          0x%08lx      default              "
                   "-> +0x%04lx (0x%08lx)\n",
                   src->get_ofs() - 2, osrp2(ch),
                   src->get_ofs() - 2 + osrp2s(ch));

        /* done */
        break;

    case OPC_NAMEDARGTAB:
        /* named argument table */
        src->next_byte(ch);
        src->next_byte(ch+1);
        src->next_byte(ch+2);
        acc = osrp2(ch);
        out->print("len=0x%x, cnt=0x%x [", (uint)acc, (uint)(uchar)*ch);
        acc = (uchar)*ch;

        /* show the names */
        for (i = 0 ; i < (int)acc ; ++i)
        {
            /* get the length */
            src->next_byte(ch);
            src->next_byte(ch);
            uint len = osrp2(ch);

            /* get the name */
            char name[128];
            uint j;
            for (j = 0 ; j < len && j < sizeof(name) ; ++j)
                src->next_byte(name + j);

            /* show the name */
            out->print("%s%.*s", i > 0 ? ", " : ":", (int)j, name);

            /* skip any excess portion of the name */
            for ( ; j < len ; ++j)
                src->next_byte(ch);
        }

        /* end the list, and we're done */
        out->print("]");
        break;

    default:
        /* show all parameters */
        for (i = 0 ; i < info->op_cnt ; ++i)
        {
            /* add a separator if this isn't the first one */
            if (i != 0)
                out->print(", ");
            
            /* display the operand */
            switch(info->op_type[i])
            {
            case T3OP_TYPE_8S:
                /* 8-bit signed integer */
                src->next_byte(ch);
                out->print("0x%x", (int)ch[0]);
                break;

            case T3OP_TYPE_8U:
                /* 8-bit unsigned integer */
                src->next_byte(ch);
                out->print("0x%x", (uint)(uchar)ch[0]);
                break;

            case T3OP_TYPE_16S:
                /* 16-bit signed integer */
                src->next_byte(ch);
                src->next_byte(ch+1);
                acc = osrp2s(ch);
                out->print("0x%x", (int)acc);
                break;

            case T3OP_TYPE_16U:
                /* 16-bit unsigned integer */
                src->next_byte(ch);
                src->next_byte(ch+1);
                acc = osrp2(ch);
                out->print("0x%x", (uint)acc);
                break;

            case T3OP_TYPE_32S:
                /* 32-bit signed integer */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("0x%lx", acc);
                break;

            case T3OP_TYPE_32U:
                /* 32-bit unsigned integer */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("0x%lx", acc);
                break;

            case T3OP_TYPE_STR:
                /* string offset */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("string(0x%lx)", acc);
                break;

            case T3OP_TYPE_LIST:
                /* list offset */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("list(0x%lx)", acc);
                break;

            case T3OP_TYPE_CODE_ABS:
                /* 32-bit absolute code address */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("code(0x%08lx)", acc);
                break;

            case T3OP_TYPE_CODE_REL:
                /* 16-bit relative code address */
                src->next_byte(ch);
                src->next_byte(ch+1);
                acc = osrp2s(ch);
                if ((long)acc < 0)
                    out->print("-0x%04x (0x%08lx)",
                               -(int)acc, src->get_ofs() - 2 + acc);
                else
                    out->print("+0x%04x (0x%08lx)",
                               (int)acc, src->get_ofs() - 2 + acc);
                break;

            case T3OP_TYPE_OBJ:
                /* object ID */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("object(0x%lx)", acc);
                break;

            case T3OP_TYPE_PROP:
                /* property ID */
                src->next_byte(ch);
                src->next_byte(ch+1);
                acc = osrp2(ch);
                out->print("property(0x%x)", (uint)acc);
                break;

            case T3OP_TYPE_ENUM:
                /* enum */
                src->next_byte(ch);
                src->next_byte(ch+1);
                src->next_byte(ch+2);
                src->next_byte(ch+3);
                acc = t3rp4u(ch);
                out->print("enum(0x%lx)", acc);
                break;

            case T3OP_TYPE_CTX_ELE:
                /* context element identifier */
                src->next_byte(ch);
                switch(ch[0])
                {
                case PUSHCTXELE_TARGPROP:
                    out->print("targetprop");
                    break;

                case PUSHCTXELE_TARGOBJ:
                    out->print("targetobj");
                    break;

                case PUSHCTXELE_DEFOBJ:
                    out->print("definingobj");
                    break;

                default:
                    out->print("0x%x", (int)ch[0]);
                    break;
                }
                break;

            case T3OP_TYPE_INLINE_STR:
                /* get the string length */
                src->next_byte(ch);
                src->next_byte(ch+1);

                /* show it */
                out->print("string(inline)");

                /* skip the string */
                for (acc = osrp2(ch) ; acc != 0 ; --acc)
                    src->next_byte(ch);

                /* done */
                break;

            default:
                out->print("...unknown type...");
                break;
            }
        }

        /* end the line */
        out->print("\n");
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Show an exception table 
 */
void CTcT3Unasm::show_exc_table(class CTcUnasSrc *src, class CTcUnasOut *out,
                                unsigned long base_ofs)
{
    unsigned entries;
    char ch[10];
    
    /* read the number of entries */
    src->next_byte(ch);
    src->next_byte(ch+1);

    /* show the entries */
    for (entries = osrp2(ch) ; entries != 0 ; --entries)
    {
        unsigned long start_ofs;
        unsigned long end_ofs;
        unsigned exc_obj_id;
        unsigned long catch_ofs;
        
        /* read the code start offset */
        src->next_byte(ch);
        src->next_byte(ch+1);
        start_ofs = base_ofs + osrp2(ch);

        /* read the code end offset */
        src->next_byte(ch);
        src->next_byte(ch+1);
        end_ofs = base_ofs + osrp2(ch);

        /* read the object ID */
        src->next_byte(ch);
        src->next_byte(ch+1);
        src->next_byte(ch+2);
        src->next_byte(ch+3);
        exc_obj_id = base_ofs + t3rp4u(ch);

        /* read the catch offset */
        src->next_byte(ch);
        src->next_byte(ch+1);
        catch_ofs = base_ofs + osrp2(ch);

        /* show it */
        out->print("    from %lx to %lx object %x catch %lx\n",
                   start_ofs, end_ofs, exc_obj_id, catch_ofs);
    }
}

