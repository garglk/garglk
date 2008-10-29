/*
$Header: d:/cvsroot/tads/TADS2/CMD.H,v 1.2 1999/05/17 02:52:11 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  cmd.h - interface to command line option service routines
Function
  defines command line option service routine interfaces
Notes
  none
Modified
  04/04/92 MJRoberts     - creation
*/

#ifndef CMD_INCLUDED
#define CMD_INCLUDED

/*
 *   Get argument to an option.  Option can be rammed up against option
 *   letter(s) with no space, or can be separated by a space.  argp is a
 *   pointer to the pointer to the current position in the argv[] array;
 *   ip is a pointer to the index in the argv[] array.  Both *argpp and
 *   *ip are incremented if the next word must be read.  argc is the total
 *   number of arguments.  ofs gives the number of characters (NOT
 *   including the '-') in this option flag; most options will have ofs==1
 *   since they are of the form '-x'.  usagefn is a function to call if
 *   the parsing fails; it is not expected to return, but should signal an
 *   error instead.  
 */
char *cmdarg(struct errcxdef *ec, char ***argpp, int *ip, int argc,
             int ofs, void (*usagefn)(struct errcxdef*));


/*
 *   Read a toggle argument.  prv is the previous value (prior to this
 *   switch) of the parameter (TRUE or FALSE).  argp is a pointer to the
 *   current argument word.  ofs is the length of this option flag, NOT
 *   including the '-'; most options have ofs==1 since they are of the
 *   form '-x'.  If the option is followed by '+', the value returned is
 *   TRUE; if it's followed by '-', the value is FALSE; if followed by
 *   nothing, the option is the logical inverse of the previous value.  If
 *   it's followed by any other character, we call the usage callback,
 *   which is not expected to return, but should signal an error. 
 */
int cmdtog(struct errcxdef *ec, int prv, char *argp, int ofs,
           void (*usagefn)(struct errcxdef*));

#endif /* CMD_INCLUDED */
