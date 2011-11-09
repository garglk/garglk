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
extern bool isObject(int instance);
extern bool isContainer(int instance);
extern bool isActor(int instance);
extern bool isALocation(int instance);
extern bool isLiteral(int instance);
extern bool isNumeric(int instance);
extern bool isString(int instance);

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

extern int where(int instance, bool directly);
extern int locationOf(int instance);

extern bool at(int instance, int other,bool directly);
extern bool in(int instance, int theContainer, bool directly);
extern bool isHere(int instance, bool directly);
extern bool isNearby(int instance, bool directly);
extern bool isNear(int instance, int other, bool directly);

extern void locate(int instance, int whr);

#endif /* INSTANCES_H_ */
