/*----------------------------------------------------------------------*\

	class

\*----------------------------------------------------------------------*/
#include "class.h"

/* IMPORTS */
#include "types.h"


/* CONSTANTS */


/* PUBLIC DATA */
ClassEntry *classes; /* Class table pointer */


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
char *idOfClass(int theClass) {
    return (char *)pointerTo(classes[theClass].id);
}


