#ifndef OUTPUT_H_
#define OUTPUT_H_

/* IMPORTS */
#include "types.h"


/* CONSTANTS */


/* TYPES */


/* DATA */
extern int col, lin; // TODO Move to current.column & current.line?
extern int pageLength, pageWidth;

extern bool anyOutput;
extern bool needSpace;
extern bool capitalize;

/* Log files */
#ifdef HAVE_GLK
#include "glk.h"
extern strid_t transcriptFile;
extern strid_t commandLogFile;
#else
extern FILE *transcriptFile;
extern FILE *commandLogFile;
#endif


/* FUNCTIONS */
extern void setSubHeaderStyle(void);
extern void setNormalStyle(void);
extern void newline(void);
extern void para(void);
extern void clear(void);
extern void printAndLog(char string[]);
extern void output(char string[]);

#endif /* OUTPUT_H_ */
