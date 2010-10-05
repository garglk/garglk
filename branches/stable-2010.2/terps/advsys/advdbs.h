/* advdbs.h - adventure database definitions */
/*
	Copyright (c) 1986, by David Michael Betz
	All rights reserved
*/

/* useful constants */
#define T	-1
#define NIL	0
#define WRDSIZE	6

/* data structure version number */
#define VERSION		102

/* file header offsets */
#define HDR_LENGTH	0	/* length of header in bytes */
#define HDR_MAGIC	2	/* magic information (6 bytes) */
#define HDR_VERSION	8	/* data structure version number */	
#define HDR_ANAME	10	/* adventure name (18 bytes) */
#define HDR_AVERSION	28	/* adventure version number */
#define HDR_WTABLE	30	/* offset to word table */
#define HDR_WTYPES	32	/* offset to word type table */
#define HDR_OTABLE	34	/* offset to object table */
#define HDR_ATABLE	36	/* offset to action table */
#define HDR_VTABLE	38	/* offset to variable table */
#define HDR_DBASE	40	/* offset to base of data space */
#define HDR_CBASE	42	/* offset to base of code space */
#define HDR_DATBLK	44	/* first data block */
#define HDR_MSGBLK	46	/* first message text block */
#define HDR_INIT	48	/* initialization code */
#define HDR_UPDATE	50	/* update code */
#define HDR_BEFORE	52	/* code to execute before verb handler */
#define HDR_AFTER	54	/* code to execute after verb handler */
#define HDR_ERROR	56	/* error handler code */
#define HDR_SAVE	58	/* save area offset */
#define HDR_SLEN	60	/* save area length */
#define HDR_SIZE	62	/* size of header */

/* word types */
#define WT_UNKNOWN	0
#define WT_VERB		1
#define WT_NOUN		2
#define WT_ADJECTIVE	3
#define WT_PREPOSITION	4
#define WT_CONJUNCTION	5
#define WT_ARTICLE	6

/* object fields */
#define O_CLASS		0
#define O_NOUNS		2
#define O_ADJECTIVES	4
#define O_NPROPERTIES	6
#define O_PROPERTIES	8
#define O_SIZE		8

/* action fields */
#define A_VERBS		0
#define A_PREPOSITIONS	2
#define A_FLAG		4
#define A_MASK		5
#define A_CODE		6
#define A_SIZE		8

/* link fields */
#define L_DATA		0
#define L_NEXT		2
#define L_SIZE		4

/* property flags */
#define P_CLASS		0x8000	/* class property */

/* action flags */
#define A_ACTOR		0x01	/* actor */
#define A_DOBJECT	0x02	/* direct object */
#define A_IOBJECT	0x04	/* indirect object */

/* opcodes */
#define OP_BRT		0x01	/* branch on true */
#define OP_BRF		0x02	/* branch on false */
#define OP_BR		0x03	/* branch unconditionally */
#define OP_T		0x04	/* load top of stack with t */
#define OP_NIL		0x05	/* load top of stack with nil */
#define OP_PUSH		0x06	/* push nil onto stack */
#define OP_NOT		0x07	/* logical negate top of stack */
#define OP_ADD		0x08	/* add two numeric expressions */
#define OP_SUB		0x09	/* subtract two numeric expressions */
#define OP_MUL		0x0A	/* multiply two numeric expressions */
#define OP_DIV		0x0B	/* divide two numeric expressions */
#define OP_REM		0x0C	/* remainder of two numeric expressions */
#define OP_BAND		0x0D	/* bitwise and of two numeric expressions */
#define OP_BOR		0x0E	/* bitwise or of two numeric expressions */
#define OP_BNOT		0x0F	/* bitwise not of two numeric expressions */
#define OP_LT		0x10	/* less than */
#define OP_EQ		0x11	/* equal to */
#define OP_GT		0x12	/* greater than */
#define OP_LIT		0x13	/* load literal */
#define OP_VAR		0x14	/* load a variable value */
#define OP_GETP		0x15	/* get the value of an object property */
#define OP_SETP		0x16	/* set the value of an object property */
#define OP_SET		0x17	/* set the value of a variable */
#define OP_PRINT	0x18	/* print messages */
#define OP_TERPRI	0x19	/* terminate the print line */
#define OP_PNUMBER	0x1A	/* print a number */
#define OP_FINISH	0x1B	/* finish handling this command */
#define OP_CHAIN	0x1C	/* chain to the next handler */
#define OP_ABORT	0x1D	/* abort this command */
#define OP_EXIT		0x1E	/* exit the program */
#define OP_RETURN	0x1F	/* return from interpreter */
#define OP_CALL		0x20	/* call a function */
#define OP_SVAR		0x21	/* short load a variable */
#define OP_SSET		0x22	/* short set a variable */
#define OP_SPLIT	0x23	/* short load a positive literal */
#define OP_SNLIT	0x24	/* short load a negative literal */
#define OP_YORN		0x25	/* yes-or-no predicate */
#define OP_SAVE		0x26	/* save data structures */
#define OP_RESTORE	0x27	/* restore data structures */
#define OP_ARG		0x28	/* load an argument value */
#define OP_ASET		0x29	/* set an argument value */
#define OP_TMP		0x2A	/* load a temporary variable value */
#define OP_TSET		0x2B	/* set a temporary variable */
#define OP_TSPACE	0x2C	/* allocate temporary variable space */
#define OP_CLASS	0x2D	/* get the class of an object */
#define OP_MATCH	0x2E	/* match a noun phrase with an object */
#define OP_PNOUN	0x2F	/* print a noun phrase */
#define OP_RESTART	0x30	/* restart the current game */
#define OP_RAND		0x31	/* generate a random number */
#define OP_RNDMIZE	0x32	/* seed the random number generator */
#define OP_SEND		0x33	/* send a message to an object */
#define OP_VOWEL	0x34	/* check for vowel beginning string */

#define OP_XVAR		0x40	/* extra short load a variable */
#define OP_XSET		0x60	/* extra short set a variable */
#define OP_XPLIT	0x80	/* extra short load a positive literal */
#define OP_XNLIT	0xC0	/* extra short load a negative literal */

/* builtin variables */
#define V_ACTOR		1	/* actor noun phrase number */
#define V_ACTION	2	/* action from parse */
#define V_DOBJECT	3	/* first direct object noun phrase number */
#define V_NDOBJECTS	4	/* number of direct object noun phrases */
#define V_IOBJECT	5	/* indirect object noun phrase number */
#define V_OCOUNT	6	/* total object count */
