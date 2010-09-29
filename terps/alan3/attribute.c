#include "attribute.h"

#include "syserr.h"
#include "current.h"
#include "lists.h"


/*======================================================================*/
AttributeEntry *findAttribute(AttributeEntry *attributeTable, int attributeCode)
{
  AttributeEntry *attribute = attributeTable;
  while (attribute->code != attributeCode) {
    attribute++;
    if (isEndOfArray(attribute))
      syserr("Attribute not found.");
  }
  return attribute;
}


/*======================================================================*/
Aptr getAttribute(AttributeEntry *attributeTable, int attributeCode)
{
  AttributeEntry *attribute = findAttribute(attributeTable, attributeCode);

  return attribute->value;
}


/*======================================================================*/
void setAttribute(AttributeEntry *attributeTable, int attributeCode, Aptr newValue)
{
  AttributeEntry *attribute = findAttribute(attributeTable, attributeCode);

  attribute->value = newValue;
  gameStateChanged = TRUE;
}
