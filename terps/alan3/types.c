/*
  Acode functions
*/

#include "acode.h"
#include "lists.h"

/*======================================================================*/
Aaddr addressAfterTable(Aaddr adr, int size) {
    while (!isEndOfArray(&memory[adr])) {
        adr += size/sizeof(Aword);
    }
    return adr+1;
}
