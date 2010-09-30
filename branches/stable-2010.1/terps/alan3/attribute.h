#ifndef ATTRIBUTE_H_
#define ATTRIBUTE_H_

#include "acode.h"

/* CONSTANTS */

/* TYPES */

/* DATA */

/* FUNCTIONS */
extern AttributeEntry *findAttribute(AttributeEntry *attributeTable, int attributeCode);
extern Aptr getAttribute(AttributeEntry *attributeTable, int attributeCode);
extern void setAttribute(AttributeEntry *attributeTable, int attributeCode, Aptr newValue);

#endif /* ATTRIBUTE_H_ */
