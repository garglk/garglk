#ifndef _SYSERR_H_
#define _SYSERR_H_
/*----------------------------------------------------------------------*\

  SYSERR.H

  Header file for syserr unit of ARUN Alan System interpreter

\*----------------------------------------------------------------------*/

/* Data: */

/* Functions: */
extern void syserr(char *msg);
extern void apperr(char *msg);
extern void playererr(char *msg);
extern void setSyserrHandler(void (*handler)(char *));

#endif
