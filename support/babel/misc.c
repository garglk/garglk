/* misc.h : miscellany for babel
 * This file is public domain
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

