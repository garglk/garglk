#ifndef MSG_H_
#define MSG_H_
/*----------------------------------------------------------------------*\

	msg

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "acode.h"
#include "types.h"
#include "params.h"

/* CONSTANTS */


/* TYPES */
typedef struct MessageEntry {   /* MESSAGE TABLE */
  Aaddr stms;           /* Address to statements*/
} MessageEntry;


/* DATA */
extern MessageEntry *msgs;  /* Message table pointer */


/* FUNCTIONS */
extern void setErrorHandler(void (*handler)(MsgKind));
extern void abortPlayerCommand(void);
extern void error(MsgKind msg);
extern bool confirm(MsgKind msgno);
extern void printMessage(MsgKind msg);
extern void printMessageWithParameters(MsgKind msg, Parameter *messageParameters);
extern void printMessageWithInstanceParameter(MsgKind message, int i);
extern void printMessageUsing2InstanceParameters(MsgKind message, int instance1, int instance2);

#endif /* MSG_H_ */
