/*----------------------------------------------------------------------*\

    The Amachine memory

\*----------------------------------------------------------------------*/
#include "memory.h"

/* Imports */
#include "types.h"
#include "syserr.h"


/* PUBLIC DATA */

Aword *memory = NULL;
static ACodeHeader dummyHeader; /* Dummy to use until memory allocated */
ACodeHeader *header = &dummyHeader;
int memTop = 0;                 /* Top of load memory */

/* Private data */
typedef struct {
    Aptr aptr;
    void *voidp;
} PointerMapEntry;

/* (64-bit) pointers mapped to 32-bit Awords through mapping table:
   Aptrs are numbered incrementally for each address that needs to be mapped.
   A slot in the table is empty if its voidp field == NULL.
   That slot can then be reused, so index in the table bears no meaning.
*/

static PointerMapEntry *pointerMap = NULL;
static int pointerMapSize = 0;
static int nextAptr = 1;


/*======================================================================*/
void resetPointerMap(void) {
    if (pointerMap != NULL) free(pointerMap);
    pointerMap = NULL;
    pointerMapSize = 0;
}

/*======================================================================*/
void *fromAptr(Aptr aptr) {
    int index;

    for (index=0; index < pointerMapSize && pointerMap[index].aptr != aptr; index++)
        ;

    if (index == pointerMapSize)
        syserr("No pointerMap entry for Aptr");

    return pointerMap[index].voidp;
}


/*======================================================================*/
Aptr toAptr(void *ptr) {
    int index;

    if (pointerMap == NULL) {
        pointerMap = (PointerMapEntry *)allocate(sizeof(PointerMapEntry));
        pointerMapSize = 1;
    }

    /* Search for ptr */
    for (index=0; index < pointerMapSize; index++)
        if (pointerMap[index].voidp == ptr)
            return pointerMap[index].aptr;

    /* So find a free slot */
    for (index=0; index < pointerMapSize && pointerMap[index].voidp != NULL; index++)
        ;

    /* Need to extend pointerMap */
    if (index == pointerMapSize) {
        pointerMap = realloc(pointerMap, (index+1)*sizeof(PointerMapEntry));
        pointerMapSize++;
    }

    pointerMap[index].voidp = ptr;
    pointerMap[index].aptr = nextAptr++;
    return pointerMap[index].aptr;
}

/*-------------------------------------------------------------------------------*/
static void forgetAptr(void *ptr) {
    for (int index=0; index < pointerMapSize; index++)
        if (pointerMap[index].voidp == ptr) {
            pointerMap[index].aptr = 0;
            pointerMap[index].voidp = NULL;
            return;
        }
}

/* Allocation/Deallocation: */
/*======================================================================*/
void *allocate(unsigned long lengthInBytes)
{
    void *p = (void *)calloc((size_t)lengthInBytes, 1);

    if (p == NULL)
        syserr("Out of memory.");

    return p;
}


/*======================================================================*/
void deallocate(void *memory)
{
    forgetAptr(memory);
    free(memory);
}


/*======================================================================*/
void *duplicate(void *original, unsigned long len)
{
  void *p = allocate(len+1);

  memcpy(p, original, len);
  return p;
}
