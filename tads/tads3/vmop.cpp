#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2012 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmop.cpp - TADS 3 VM opcode definitions
Function
  
Notes
  
Modified
  03/14/12 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmop.h"
#include "vmtype.h"


/*
 *   Get the size of an opcode 
 */
size_t CVmOpcodes::get_op_size(const uchar *op)
{
    switch (*op)
    {
    case OPC_PUSHSTRI:
        return 3 + osrp2(op+1);
        
    case OPC_SWITCH:
        return 1 + 2 + (VMB_DATAHOLDER + 2)*osrp2(op+1) + 2;
        
    case OPC_NAMEDARGTAB:
        return 1 + 2 + osrp2(op+1);

    default:
        /* all others have fixed sizes */
        return op_siz[*op];
    }
}

/*
 *   Table of T3 VM opcode sizes
 */
const uchar CVmOpcodes::op_siz[] =
{
    0,                                                     /* 0x00 - unused */

    1,                                                 /* 0x01 - OPC_PUSH_0 */
    1,                                                 /* 0x02 - OPC_PUSH_1 */
    2,                                               /* 0x03 - OPC_PUSHINT8 */
    5,                                                /* 0x04 - OPC_PUSHINT */
    5,                                                /* 0x05 - OPC_PUSHSTR */
    5,                                                /* 0x06 - OPC_PUSHLST */
    5,                                                /* 0x07 - OPC_PUSHOBJ */
    1,                                                /* 0x08 - OPC_PUSHNIL */
    1,                                               /* 0x09 - OPC_PUSHTRUE */
    3,                                             /* 0x0A - OPC_PUSHPROPID */
    5,                                              /* 0x0B - OPC_PUSHFNPTR */
    0,                   /* 0x0C - OPC_PUSHSTRI - variable-size instruction */
    2,                                             /* 0x0D - OPC_PUSHPARLST */
    1,                                             /* 0x0E - OPC_MAKELSTPAR */
    5,                                               /* 0x0F - OPC_PUSHENUM */
    5,                                             /* 0x10 - OPC_PUSHBIFPTR */
    
    1,                                                     /* 0x11 - unused */
    1,                                                     /* 0x12 - unused */
    1,                                                     /* 0x13 - unused */
    1,                                                     /* 0x14 - unused */
    1,                                                     /* 0x15 - unused */
    1,                                                     /* 0x16 - unused */
    1,                                                     /* 0x17 - unused */
    1,                                                     /* 0x18 - unused */
    1,                                                     /* 0x19 - unused */
    1,                                                     /* 0x1A - unused */
    1,                                                     /* 0x1B - unused */
    1,                                                     /* 0x1C - unused */
    1,                                                     /* 0x1D - unused */
    1,                                                     /* 0x1E - unused */
    1,                                                     /* 0x1F - unused */
    
    1,                                                    /* 0x20 - OPC_NEG */
    1,                                                   /* 0x21 - OPC_BNOT */
    1,                                                    /* 0x22 - OPC_ADD */
    1,                                                    /* 0x23 - OPC_SUB */
    1,                                                    /* 0x24 - OPC_MUL */
    1,                                                   /* 0x25 - OPC_BAND */
    1,                                                    /* 0x26 - OPC_BOR */
    1,                                                    /* 0x27 - OPC_SHL */
    1,                                                   /* 0x28 - OPC_ASHR */
    1,                                                    /* 0x29 - OPC_XOR */
    1,                                                    /* 0x2A - OPC_DIV */
    1,                                                    /* 0x2B - OPC_MOD */
    1,                                                    /* 0x2C - OPC_NOT */
    1,                                                /* 0x2D - OPC_BOOLIZE */
    1,                                                    /* 0x2E - OPC_INC */
    1,                                                    /* 0x2F - OPC_DEC */
    1,                                                   /* 0x31 - OPC_LSHR */
    
    1,                                                     /* 0x31 - unused */
    1,                                                     /* 0x32 - unused */
    1,                                                     /* 0x33 - unused */
    1,                                                     /* 0x34 - unused */
    1,                                                     /* 0x35 - unused */
    1,                                                     /* 0x36 - unused */
    1,                                                     /* 0x37 - unused */
    1,                                                     /* 0x38 - unused */
    1,                                                     /* 0x39 - unused */
    1,                                                     /* 0x3A - unused */
    1,                                                     /* 0x3B - unused */
    1,                                                     /* 0x3C - unused */
    1,                                                     /* 0x3D - unused */
    1,                                                     /* 0x3E - unused */
    1,                                                     /* 0x3F - unused */
    
    1,                                                     /* 0x40 - OPC_EQ */
    1,                                                     /* 0x41 - OPC_NE */
    1,                                                     /* 0x42 - OPC_LT */
    1,                                                     /* 0x43 - OPC_LE */
    1,                                                     /* 0x44 - OPC_GT */
    1,                                                     /* 0x45 - OPC_GE */
    
    1,                                                     /* 0x46 - unused */
    1,                                                     /* 0x47 - unused */
    1,                                                     /* 0x48 - unused */
    1,                                                     /* 0x49 - unused */
    1,                                                     /* 0x4A - unused */
    1,                                                     /* 0x4B - unused */
    1,                                                     /* 0x4C - unused */
    1,                                                     /* 0x4D - unused */
    1,                                                     /* 0x4E - unused */
    1,                                                     /* 0x4F - unused */
    
    1,                                                 /* 0x50 - OPC_RETVAL */
    1,                                                 /* 0x51 - OPC_RETNIL */
    1,                                                /* 0x52 - OPC_RETTRUE */
    1,                                                     /* 0x53 - unused */
    1,                                                    /* 0x54 - OPC_RET */
    
    1,                                                     /* 0x55 - unused */
    
    4,                                            /* 0x56 - OPC_NAMEDARGPTR */
    0,                   /* 0x57 - OPC_NAMEDARGTAB - variable-size operands */
    
    6,                                                   /* 0x58 - OPC_CALL */
    2,                                                /* 0x59 - OPC_PTRCALL */
    
    1,                                                     /* 0x5A - unused */
    1,                                                     /* 0x5B - unused */
    1,                                                     /* 0x5C - unused */
    1,                                                     /* 0x5D - unused */
    1,                                                     /* 0x5E - unused */
    1,                                                     /* 0x5F - unused */
    
    3,                                                /* 0x60 - OPC_GETPROP */
    4,                                               /* 0x61 - OPC_CALLPROP */
    2,                                            /* 0x62 - OPC_PTRCALLPROP */
    3,                                            /* 0x63 - OPC_GETPROPSELF */
    4,                                           /* 0x64 - OPC_CALLPROPSELF */
    2,                                        /* 0x65 - OPC_PTRCALLPROPSELF */
    7,                                             /* 0x66 - OPC_OBJGETPROP */
    8,                                            /* 0x67 - OPC_OBJCALLPROP */
    3,                                            /* 0x68 - OPC_GETPROPDATA */
    1,                                         /* 0x69 - OPC_PTRGETPROPDATA */
    4,                                            /* 0x6A - OPC_GETPROPLCL1 */
    5,                                           /* 0x6B - OPC_CALLPROPLCL1 */
    3,                                              /* 0x6C - OPC_GETPROPR0 */
    4,                                             /* 0x6D - OPC_CALLPROPR0 */
    
    1,                                                     /* 0x6E - unused */
    1,                                                     /* 0x6F - unused */
    1,                                                     /* 0x70 - unused */
    1,                                                     /* 0x71 - unused */
    
    4,                                                /* 0x72 - OPC_INHERIT */
    2,                                             /* 0x73 - OPC_PTRINHERIT */
    8,                                             /* 0x74 - OPC_EXPINHERIT */
    6,                                          /* 0x75 - OPC_PTREXPINHERIT */
    1,                                                /* 0x76 - OPC_VARARGC */
    4,                                               /* 0x77 - OPC_DELEGATE */
    2,                                            /* 0x78 - OPC_PTRDELEGATE */
    
    1,                                                     /* 0x79 - unused */
    1,                                                      /* 0x7A - SWAP2 */
    3,                                                      /* 0x7B - SWAPN */
    
    1,                                               /* 0x7C - OPC_GETARGN0 */
    1,                                               /* 0x7D - OPC_GETARGN1 */
    1,                                               /* 0x7E - OPC_GETARGN2 */
    1,                                               /* 0x7F - OPC_GETARGN3 */
    2,                                                /* 0x80 - OPC_GETLCL1 */
    3,                                                /* 0x81 - OPC_GETLCL2 */
    2,                                                /* 0x82 - OPC_GETARG1 */
    3,                                                /* 0x83 - OPC_GETARG2 */
    1,                                               /* 0x84 - OPC_PUSHSELF */
    5,                                               /* 0x85 - OPC_GETDBLCL */
    5,                                               /* 0x86 - OPC_GETDBARG */
    1,                                                /* 0x87 - OPC_GETARGC */
    1,                                                    /* 0x88 - OPC_DUP */
    1,                                                   /* 0x89 - OPC_DISC */
    2,                                                  /* 0x8A - OPC_DISC1 */
    1,                                                  /* 0x8B - OPC_GETR0 */
    3,                                              /* 0x8C - OPC_GETDBARGC */
    1,                                                   /* 0x8D - OPC_SWAP */
    
    2,                                             /* 0x8E - OPC_PUSHCTXELE */
    
    1,                                                       /* 0x8F - DUP2 */
    
    0,                     /* 0x90 - OPC_SWITCH - variable-size instruction */
    3,                                                    /* 0x91 - OPC_JMP */
    3,                                                     /* 0x92 - OPC_JT */
    3,                                                     /* 0x93 - OPC_JF */
    3,                                                     /* 0x94 - OPC_JE */
    3,                                                    /* 0x95 - OPC_JNE */
    3,                                                    /* 0x96 - OPC_JGT */
    3,                                                    /* 0x97 - OPC_JGE */
    3,                                                    /* 0x98 - OPC_JLT */
    3,                                                    /* 0x99 - OPC_JLE */
    3,                                                    /* 0x9A - OPC_JST */
    3,                                                    /* 0x9B - OPC_JSF */
    3,                                                   /* 0x9C - OPC_LJSR */
    3,                                                   /* 0x9D - OPC_LRET */
    3,                                                   /* 0x9E - OPC_JNIL */
    3,                                                /* 0x9F - OPC_JNOTNIL */
    3,                                                   /* 0xA0 - OPC_JR0T */
    3,                                                   /* 0xA1 - OPC_JR0F */
    5,                                               /* 0xA2 - OPC_ITERNEXT */
    
    2,                                               /* 0xA3 - GETSETLCL1R0 */
    2,                                                 /* 0xA4 - GETSETLCL1 */
    1,                                                      /* 0xA5 - DUPR0 */
    2,                                                     /* 0xA6 - GETSPN */
    1,                                                     /* 0xA7 - unused */
    1,                                                     /* 0xA8 - unused */
    1,                                                     /* 0xA9 - unused */
    
    1,                                                   /* 0xAA - GETLCLN0 */
    1,                                                   /* 0xAB - GETLCLN1 */
    1,                                                   /* 0xAC - GETLCLN2 */
    1,                                                   /* 0xAD - GETLCLN3 */
    1,                                                   /* 0xAE - GETLCLN4 */
    1,                                                   /* 0xAF - GETLCLN5 */
    
    5,                                                    /* 0xB0 - OPC_SAY */
    3,                                              /* 0xB1 - OPC_BUILTIN_A */
    3,                                              /* 0xB2 - OPC_BUILTIN_B */
    3,                                              /* 0xB3 - OPC_BUILTIN_C */
    3,                                              /* 0xB4 - OPC_BUILTIN_D */
    3,                                               /* 0xB5 - OPC_BUILTIN1 */
    4,                                               /* 0xB6 - OPC_BUILTIN2 */
    0,          /* 0xB7 - OPC_CALLEXT (reserved; not currently implemented) */
    1,                                                  /* 0xB8 - OPC_THROW */
    1,                                                 /* 0xB9 - OPC_SAYVAL */
    
    1,                                                  /* 0xBA - OPC_INDEX */
    3,                                            /* 0xBB - OPC_IDXLCL1INT8 */
    2,                                               /* 0xBC - OPC_IDXLINT8 */
    1,                                                     /* 0xBD - unused */
    
    1,                                                     /* 0xBE - unused */
    1,                                                     /* 0xBF - unused */
    
    3,                                                   /* 0xC0 - OPC_NEW1 */
    5,                                                   /* 0xC1 - OPC_NEW2 */
    3,                                                 /* 0xC2 - OPC_TRNEW1 */
    5,                                                 /* 0xC3 - OPC_TRNEW2 */
    
    1,                                                     /* 0xC4 - unused */
    1,                                                     /* 0xC5 - unused */
    1,                                                     /* 0xC6 - unused */
    1,                                                     /* 0xC7 - unused */
    1,                                                     /* 0xC8 - unused */
    1,                                                     /* 0xC9 - unused */
    1,                                                     /* 0xCA - unused */
    1,                                                     /* 0xCB - unused */
    1,                                                     /* 0xCC - unused */
    1,                                                     /* 0xCD - unused */
    1,                                                     /* 0xCE - unused */
    1,                                                     /* 0xCF - unused */
    
    3,                                                 /* 0xD0 - OPC_INCLCL */
    3,                                                 /* 0xD1 - OPC_DECLCL */
    3,                                               /* 0xD2 - OPC_ADDILCL1 */
    7,                                               /* 0xD3 - OPC_ADDILCL4 */
    3,                                               /* 0xD4 - OPC_ADDTOLCL */
    3,                                             /* 0xD5 - OPC_SUBFROMLCL */
    2,                                               /* 0xD6 - OPC_ZEROLCL1 */
    3,                                               /* 0xD7 - OPC_ZEROLCL2 */
    2,                                                /* 0xD8 - OPC_NILLCL1 */
    3,                                                /* 0xD9 - OPC_NILLCL2 */
    2,                                                /* 0xDA - OPC_ONELCL1 */
    3,                                                /* 0xDB - OPC_ONELCL2 */
    
    1,                                                     /* 0xDC - unused */
    1,                                                     /* 0xDD - unused */
    1,                                                     /* 0xDE - unused */
    1,                                                     /* 0xDF - unused */
    
    2,                                                /* 0xE0 - OPC_SETLCL1 */
    3,                                                /* 0xE1 - OPC_SETLCL2 */
    2,                                                /* 0xE2 - OPC_SETARG1 */
    3,                                                /* 0xE3 - OPC_SETARG2 */
    1,                                                 /* 0xE4 - OPC_SETIND */
    3,                                                /* 0xE5 - OPC_SETPROP */
    1,                                             /* 0xE6 - OPC_PTRSETPROP */
    3,                                            /* 0xE7 - OPC_SETPROPSELF */
    7,                                             /* 0xE8 - OPC_OBJSETPROP */
    5,                                               /* 0xE9 - OPC_SETDBLCL */
    5,                                               /* 0xEA - OPC_SETDBARG */
    
    1,                                                /* 0xEB - OPC_SETSELF */
    1,                                                /* 0xEC - OPC_LOADCTX */
    1,                                               /* 0xED - OPC_STORECTX */
    2,                                              /* 0xEE - OPC_SETLCL1R0 */
    3,                                           /* 0xEF - OPC_SETINDLCL1I8 */
    
    1,                                                     /* 0xF0 - unused */
    
    1,                                                     /* 0xF1 - OPC_BP */
    1,                                                    /* 0xF2 - OPC_NOP */
    
    1,                                                     /* 0xF3 - unused */
    1,                                                     /* 0xF4 - unused */
    1,                                                     /* 0xF5 - unused */
    1,                                                     /* 0xF6 - unused */
    1,                                                     /* 0xF7 - unused */
    1,                                                     /* 0xF8 - unused */
    1,                                                     /* 0xF9 - unused */
    1,                                                     /* 0xFA - unused */
    1,                                                     /* 0xFB - unused */
    1,                                                     /* 0xFC - unused */
    1,                                                     /* 0xFD - unused */
    1,                                                     /* 0xFE - unused */
    255                                                    /* 0xFF - unused */
};
