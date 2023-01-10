#include "attribute.h"

#include "syserr.h"
#include "current.h"
#include "lists.h"


/*----------------------------------------------------------------------*/
static AttributeEntry *lookupAttribute(AttributeEntry *attributeTable, int attributeCode)
{
    AttributeEntry *attribute = attributeTable;
    while (attribute->code != attributeCode) {
        attribute++;
        if (isEndOfArray(attribute))
            return NULL;
    }
    return attribute;
}


/*======================================================================*/
bool attributeExists(AttributeEntry *attributeTable, int attributeCode)
{
    AttributeEntry *attribute = lookupAttribute(attributeTable, attributeCode);

    return attribute != NULL;
}


/*======================================================================*/
Aword getAttribute(AttributeEntry *attributeTable, int attributeCode)
{
    AttributeEntry *attribute = lookupAttribute(attributeTable, attributeCode);

    if (attribute == NULL)
        syserr("Attribute not found.");

    return attribute->value;
}


/*======================================================================*/
void setAttribute(AttributeEntry *attributeTable, int attributeCode, Aptr newValue)
{
    AttributeEntry *attribute = lookupAttribute(attributeTable, attributeCode);

    if (attribute == NULL)
        syserr("Attribute not found.");

    attribute->value = newValue;
    gameStateChanged = true;
}
