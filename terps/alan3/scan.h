#ifndef _SCAN_H_
#define _SCAN_H_
/*----------------------------------------------------------------------*\

  SCAN.H

  Player input scanner for ALAN interpreter module.

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "types.h"

/* TYPES */


/* DATA */
extern bool continued;


/* FUNCTIONS */

extern void forceNewPlayerInput(void);
extern void scan(void);

#endif
