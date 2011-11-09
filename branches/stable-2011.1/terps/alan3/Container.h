#ifndef _CONTAINER_H_
#define _CONTAINER_H_
/*----------------------------------------------------------------------*\

  Containers

\*----------------------------------------------------------------------*/

#include "types.h"
#include "acode.h"


/* CONSTANTS */


/* TYPES */


/* DATA */
extern ContainerEntry *containers; /* Container table pointer */


/* FUNCTIONS */
extern int containerSize(int container, bool directly);
extern bool passesContainerLimits(Aint container, Aint addedInstance);
extern void describeContainer(int container);
extern void list(int cnt);


#endif
