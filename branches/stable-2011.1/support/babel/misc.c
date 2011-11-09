/* misc.h : miscellany for babel
 * This file has been released into the public domain by its author.
* The author waives all of his rights to the work
* worldwide under copyright law to the maximum extent allowed by law
* 
 * 2006 by L. Ross Raszewski
 */

#include <stdlib.h>
#include <stdio.h>

void *my_malloc(unsigned int size, char *rs)
{
 void *buf=calloc(size,1);
 if (size && !buf)
  {
        fprintf(stderr,"Error: Memory exceeded (%d for %s)!\n",size,rs);
        exit(2);
   }
   return buf;
}

