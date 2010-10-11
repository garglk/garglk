/*----------------------------------------------------------------------*\

	class

\*----------------------------------------------------------------------*/
#include "class.h"

/* IMPORTS */
#include "types.h"
#include <memory.h>


/* CONSTANTS */


/* PUBLIC DATA */
ClassEntry *classes; /* Class table pointer */


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
char *idOfClass(int theClass) {
    return (char *)pointerTo(classes[theClass].id);
}


