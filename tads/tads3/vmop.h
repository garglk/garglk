/* $Header: d:/cvsroot/tads/tads3/vmop.h,v 1.4 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmop.h - T3 VM Opcodes
Function
  
Notes
  
Modified
  11/14/98 MJRoberts  - Creation
*/

#ifndef VMOP_H
#define VMOP_H

/*
 *   T3 VM opcode definitions 
 */
class CVmOpcodes
{
public:
    /*
     *   Opcode size table.  Index by opcode; each entry gives the size in
     *   bytes of the instruction.  A value of 0 is special - it means that
     *   the instruction is varying-length.
     */
    static const uchar op_siz[];

    /* 
     *   Get the size in bytes of an opcode.  This computes the actual size
     *   of varying-length instructions. 
     */
    static size_t get_op_size(const uchar *op);
};


#define OPC_PUSH_0       0x01                    /* push constant integer 0 */
#define OPC_PUSH_1       0x02                    /* push constant integer 1 */
#define OPC_PUSHINT8     0x03              /* push SBYTE operand as integer */
#define OPC_PUSHINT      0x04               /* push INT4 operand as integer */
#define OPC_PUSHSTR      0x05      /* push UINT4 operand as string constant */
#define OPC_PUSHLST      0x06        /* push UINT4 operand as list constant */
#define OPC_PUSHOBJ      0x07            /* push UINT4 operand as object ID */
#define OPC_PUSHNIL      0x08                                   /* push nil */
#define OPC_PUSHTRUE     0x09                                  /* push true */
#define OPC_PUSHPROPID   0x0A          /* push UINT2 operand as property ID */
#define OPC_PUSHFNPTR    0x0B                     /* push UINT4 code offset */
#define OPC_PUSHSTRI     0x0C                /* push inline string constant */
#define OPC_PUSHPARLST   0x0D                /* push varargs parameter list */
#define OPC_MAKELSTPAR   0x0E           /* push varargs parameter from list */
#define OPC_PUSHENUM     0x0F                         /* push an enum value */
#define OPC_PUSHBIFPTR   0x10      /* push a pointer to a built-in function */

#define OPC_NEG          0x20                                     /* negate */
#define OPC_BNOT         0x21                                /* bitwise NOT */
#define OPC_ADD          0x22                                        /* add */
#define OPC_SUB          0x23                                   /* subtract */
#define OPC_MUL          0x24                                   /* multiply */
#define OPC_BAND         0x25                                /* bitwise AND */
#define OPC_BOR          0x26                                 /* bitwise OR */
#define OPC_SHL          0x27                                 /* shift left */
#define OPC_ASHR          0x28                    /* arithmetic shift right */
#define OPC_XOR          0x29                        /* bitwise/logical XOR */
#define OPC_DIV          0x2A                                     /* divide */
#define OPC_MOD          0x2B                            /* MOD (remainder) */
#define OPC_NOT          0x2C                                /* logical NOT */
#define OPC_BOOLIZE      0x2D           /* convert top of stack to true/nil */
#define OPC_INC          0x2E            /* increment value at top of stack */
#define OPC_DEC          0x2F            /* decrement value at top of stack */
#define OPC_LSHR         0x30                        /* logical shift right */

#define OPC_EQ           0x40                                     /* equals */
#define OPC_NE           0x41                                 /* not equals */
#define OPC_LT           0x42                                  /* less than */
#define OPC_LE           0x43                      /* less than or equal to */
#define OPC_GT           0x44                               /* greater than */
#define OPC_GE           0x45                   /* greater than or equal to */

#define OPC_RETVAL       0x50          /* return with value at top of stack */
#define OPC_RETNIL       0x51                                 /* return nil */
#define OPC_RETTRUE      0x52                                /* return true */
#define OPC_RET          0x54                       /* return with no value */

#define OPC_NAMEDARGPTR  0x56            /* pointer to named argument table */
#define OPC_NAMEDARGTAB  0x57                       /* named argument table */

#define OPC_CALL         0x58                              /* function call */
#define OPC_PTRCALL      0x59              /* function call through pointer */

#define OPC_GETPROP      0x60                               /* get property */
#define OPC_CALLPROP     0x61               /* call property with arguments */
#define OPC_PTRCALLPROP  0x62    /* call property through pointer with args */
#define OPC_GETPROPSELF  0x63                     /* get property of 'self' */
#define OPC_CALLPROPSELF 0x64                      /* call method of 'self' */
#define OPC_PTRCALLPROPSELF 0x65   /* call method of 'self' through pointer */
#define OPC_OBJGETPROP   0x66            /* get property of specific object */
#define OPC_OBJCALLPROP  0x67             /* call method of specific object */
#define OPC_GETPROPDATA  0x68     /* get property, disallowing side effects */
#define OPC_PTRGETPROPDATA 0x69      /* get prop through pointer, data only */
#define OPC_GETPROPLCL1  0x6A             /* get property of local variable */
#define OPC_CALLPROPLCL1 0x6B            /* call property of local variable */
#define OPC_GETPROPR0    0x6C                         /* get property of R0 */
#define OPC_CALLPROPR0   0x6D                        /* call property of R0 */

#define OPC_INHERIT      0x72                    /* inherit from superclass */
#define OPC_PTRINHERIT   0x73           /* inherit through property pointer */
#define OPC_EXPINHERIT   0x74        /* inherit from an explicit superclass */
#define OPC_PTREXPINHERIT 0x75 /* inherit from explicit sc through prop ptr */
#define OPC_VARARGC      0x76       /* modifier: next call is var arg count */
#define OPC_DELEGATE     0x77                /* delegate to object on stack */
#define OPC_PTRDELEGATE  0x78          /* delegate through property pointer */

#define OPC_SWAP2        0x7A        /* swap top two elements with next two */
#define OPC_SWAPN        0x7B           /* swap elements at operand indices */

#define OPC_GETARGN0     0x7C                            /* get argument #0 */
#define OPC_GETARGN1     0x7D                            /* get argument #1 */
#define OPC_GETARGN2     0x7E                            /* get argument #2 */
#define OPC_GETARGN3     0x7F                            /* get argument #3 */

#define OPC_GETLCL1      0x80                      /* push a local variable */
#define OPC_GETLCL2      0x81                /* push a local (2-byte index) */
#define OPC_GETARG1      0x82                           /* push an argument */
#define OPC_GETARG2      0x83            /* push an argument (2-byte index) */
#define OPC_PUSHSELF     0x84                                /* push 'self' */
#define OPC_GETDBLCL     0x85                     /* push debug frame local */
#define OPC_GETDBARG     0x86                  /* push debug frame argument */
#define OPC_GETARGC      0x87                 /* get current argument count */
#define OPC_DUP          0x88                     /* duplicate top of stack */
#define OPC_DISC         0x89                       /* discard top of stack */
#define OPC_DISC1        0x8A                 /* discard n items from stack */
#define OPC_GETR0        0x8B        /* push the R0 register onto the stack */
#define OPC_GETDBARGC    0x8C            /* push debug frame argument count */
#define OPC_SWAP         0x8D                /* swap top two stack elements */

#define OPC_PUSHCTXELE   0x8E                /* push a method context value */
#define PUSHCTXELE_TARGPROP 0x01                    /* push target property */
#define PUSHCTXELE_TARGOBJ  0x02                      /* push target object */
#define PUSHCTXELE_DEFOBJ   0x03                    /* push defining object */
#define PUSHCTXELE_INVOKEE  0x04                        /* push the invokee */

#define OPC_DUP2         0x8F       /* duplicate the top two stack elements */

#define OPC_SWITCH       0x90                    /* jump through case table */
#define OPC_JMP          0x91                       /* unconditional branch */
#define OPC_JT           0x92                               /* jump if true */
#define OPC_JF           0x93                              /* jump if false */
#define OPC_JE           0x94                              /* jump if equal */
#define OPC_JNE          0x95                          /* jump if not equal */
#define OPC_JGT          0x96                       /* jump if greater than */
#define OPC_JGE          0x97                   /* jump if greater or equal */
#define OPC_JLT          0x98                          /* jump if less than */
#define OPC_JLE          0x99                 /* jump if less than or equal */
#define OPC_JST          0x9A                      /* jump and save if true */
#define OPC_JSF          0x9B                     /* jump and save if false */
#define OPC_LJSR         0x9C                   /* local jump to subroutine */
#define OPC_LRET         0x9D               /* local return from subroutine */
#define OPC_JNIL         0x9E                                /* jump if nil */
#define OPC_JNOTNIL      0x9F                            /* jump if not nil */
#define OPC_JR0T         0xA0                         /* jump if R0 is true */
#define OPC_JR0F         0xA1                        /* jump if R0 is false */
#define OPC_ITERNEXT     0xA2                              /* iterator next */

#define OPC_GETSETLCL1R0 0xA3 /* set local from R0 and leave value on stack */
#define OPC_GETSETLCL1   0xA4         /* set local and leave value on stack */
#define OPC_DUPR0        0xA5                              /* push R0 twice */
#define OPC_GETSPN       0xA6           /* get stack element at given index */

#define OPC_GETLCLN0     0xAA                               /* get local #0 */
#define OPC_GETLCLN1     0xAB                               /* get local #1 */
#define OPC_GETLCLN2     0xAC                               /* get local #2 */
#define OPC_GETLCLN3     0xAD                               /* get local #3 */
#define OPC_GETLCLN4     0xAE                               /* get local #4 */
#define OPC_GETLCLN5     0xAF                               /* get local #5 */

#define OPC_SAY          0xB0                  /* display a constant string */
#define OPC_BUILTIN_A    0xB1              /* call built-in func from set 0 */
#define OPC_BUILTIN_B    0xB2                   /* call built-in from set 1 */
#define OPC_BUILTIN_C    0xB3                   /* call built-in from set 2 */
#define OPC_BUILTIN_D    0xB4                   /* call built-in from set 3 */
#define OPC_BUILTIN1     0xB5    /* call built-in from any set, 8-bit index */
#define OPC_BUILTIN2     0xB6   /* call built-in from any set, 16-bit index */
#define OPC_CALLEXT      0xB7                     /* call external function */
#define OPC_THROW        0xB8                         /* throw an exception */
#define OPC_SAYVAL       0xB9          /* display the value at top of stack */

#define OPC_INDEX        0xBA                               /* index a list */
#define OPC_IDXLCL1INT8  0xBB    /* index a local variable by an int8 value */
#define OPC_IDXINT8      0xBC                     /* index by an int8 value */

#define OPC_NEW1         0xC0                 /* create new object instance */
#define OPC_NEW2         0xC1        /* create new object (2-byte operands) */
#define OPC_TRNEW1       0xC2              /* create new transient instance */
#define OPC_TRNEW2       0xC3  /* create transient object (2-byte operands) */

#define OPC_INCLCL       0xD0              /* increment local variable by 1 */
#define OPC_DECLCL       0xD1              /* decrement local variable by 1 */
#define OPC_ADDILCL1     0xD2          /* add immediate 1-byte int to local */
#define OPC_ADDILCL4     0xD3          /* add immediate 4-byte int to local */
#define OPC_ADDTOLCL     0xD4                /* add value to local variable */
#define OPC_SUBFROMLCL   0xD5         /* subtract value from local variable */
#define OPC_ZEROLCL1     0xD6                          /* set local to zero */
#define OPC_ZEROLCL2     0xD7                          /* set local to zero */
#define OPC_NILLCL1      0xD8                           /* set local to nil */
#define OPC_NILLCL2      0xD9                           /* set local to nil */
#define OPC_ONELCL1      0xDA               /* set local to numeric value 1 */
#define OPC_ONELCL2      0xDB               /* set local to numeric value 1 */

#define OPC_SETLCL1      0xE0            /* set local (1-byte local number) */
#define OPC_SETLCL2      0xE1            /* set local (2-byte local number) */
#define OPC_SETARG1      0xE2        /* set parameter (1-byte param number) */
#define OPC_SETARG2      0xE3        /* set parameter (2-byte param number) */
#define OPC_SETIND       0xE4                         /* set value at index */
#define OPC_SETPROP      0xE5                     /* set property in object */
#define OPC_PTRSETPROP   0xE6          /* set property through prop pointer */
#define OPC_SETPROPSELF  0xE7                       /* set property in self */
#define OPC_OBJSETPROP   0xE8           /* set property in immediate object */
#define OPC_SETDBLCL     0xE9                /* set debugger local variable */
#define OPC_SETDBARG     0xEA            /* set debugger parameter variable */

#define OPC_SETSELF      0xEB                                 /* set 'self' */
#define OPC_LOADCTX      0xEC             /* load method context from stack */
#define OPC_STORECTX     0xED     /* store method context and push on stack */
#define OPC_SETLCL1R0    0xEE    /* set local (1-byte local number) from R0 */
#define OPC_SETINDLCL1I8 0xEF                          /* set indexed local */

#define OPC_BP           0xF1                        /* debugger breakpoint */
#define OPC_NOP          0xF2                               /* no operation */


#endif /* VMOP_H */

