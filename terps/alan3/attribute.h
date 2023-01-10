#ifndef ATTRIBUTE_H_
#define ATTRIBUTE_H_

#include "acode.h"
#include <stdbool.h>


/* CONSTANTS */

/* TYPES */

/* DATA */

/* FUNCTIONS */
extern bool attributeExists(AttributeEntry *attributeTable, int attributeCode);
extern Aword getAttribute(AttributeEntry *attributeTable, int attributeCode);
extern void setAttribute(AttributeEntry *attributeTable, int attributeCode, Aptr newValue);

#endif /* ATTRIBUTE_H_ */
