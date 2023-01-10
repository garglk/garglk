#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "types.h"

extern bool verboseOption;
extern bool ignoreErrorOption;
extern bool debugOption;
extern bool traceSectionOption;
extern bool tracePushOption;
extern bool traceStackOption;
extern bool traceSourceOption;
extern bool traceInstructionOption;
extern bool transcriptOption;
extern bool commandLogOption;
extern bool statusLineOption;
extern bool regressionTestOption;
extern bool nopagingOption;

#define ENCODING_ISO 0
#define ENCODING_UTF 1
extern int encodingOption;         /* 0 = ISO, 1 = UTF-8 */


/* FUNCTIONS: */
extern void usage(char *programName);

#endif
