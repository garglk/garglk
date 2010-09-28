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
extern Bool isA(int instance, int class);
extern Bool isObject(int instance);
extern Bool isContainer(int instance);
extern Bool isActor(int instance);
extern Bool isALocation(int instance);
extern Bool isLiteral(int instance);
extern Bool isNumeric(int instance);
extern Bool isString(int instance);

extern Aword getInstanceAttribute(int instance, int attribute);
extern char *getInstanceStringAttribute(int instane, int attribute);
extern Set *getInstanceSetAttribute(int instance, int attribute);

extern void setInstanceAttribute(int instance, int atr, Aword value);
extern void setInstanceStringAttribute(int instance, int attribute, char *string);
extern void setInstanceSetAttribute(int instance, int atr, Aword set);

extern void say(int instance);
extern void sayForm(int instance, SayForm form);
extern void sayInstance(int instance);

extern Bool hasDescription(int instance);
extern Bool isDescribable(int instance);
extern void describeAnything(int instance);
extern void describeInstances(void);
extern Bool describe(int instance);

extern int where(int instance, Bool directly);
extern int locationOf(int instance);

extern Bool at(int instance, int other,Bool directly);
extern Bool in(int instance, int theContainer, Bool directly);
extern Bool isHere(int instance, Bool directly);
extern Bool isNearby(int instance, Bool directly);
extern Bool isNear(int instance, int other, Bool directly);

extern void locate(int instance, int whr);

#endif /* INSTANCES_H_ */
