#ifndef INSTANCES_H_
#define INSTANCES_H_
/*----------------------------------------------------------------------*\

  Instance

\*----------------------------------------------------------------------*/

/* Imports: */
#include "acode.h"
#include "types.h"
#include "set.h"

/* Constants: */


/* Types: */
typedef struct AdminEntry { /* Administrative data about instances */
  Aint location;
  AttributeEntry *attributes;
  Abool alreadyDescribed;
  Aint visitsCount;
  Aint script;
  Aint step;
  Aint waitCount;
} AdminEntry;


/* Data: */
extern InstanceEntry *instances; /* Instance table pointer */

extern AdminEntry *admin;   /* Administrative data about instances */
extern AttributeEntry *attributes; /* Dynamic attribute values */


/* Functions: */
extern bool isA(int instance, int class);
extern bool isAObject(int instance);
extern bool isAContainer(int instance);
extern bool isAActor(int instance);
extern bool isALocation(int instance);
extern bool isLiteral(int instance);
extern bool isANumeric(int instance);
extern bool isAString(int instance);

extern Aptr getInstanceAttribute(int instance, int attribute);
extern char *getInstanceStringAttribute(int instane, int attribute);
extern Set *getInstanceSetAttribute(int instance, int attribute);

extern void setInstanceAttribute(int instance, int atr, Aptr value);
extern void setInstanceStringAttribute(int instance, int attribute, char *string);
extern void setInstanceSetAttribute(int instance, int atr, Aptr set);

extern void say(int instance);
extern void sayForm(int instance, SayForm form);
extern void sayInstance(int instance);

extern bool hasDescription(int instance);
extern bool isDescribable(int instance);
extern void describeAnything(int instance);
extern void describeInstances(void);
extern bool describe(int instance);

extern int where(int instance, ATrans trans);
extern int positionOf(int instance);
extern int locationOf(int instance);

extern bool isAt(int instance, int other, ATrans trans);
extern bool isIn(int instance, int theContainer, ATrans trans);
extern bool isHere(int instance, ATrans trans);
extern bool isNearby(int instance, ATrans trans);
extern bool isNear(int instance, int other, ATrans trans);

extern bool isOpaque(int container);

extern void locate(int instance, int whr);

#endif /* INSTANCES_H_ */
