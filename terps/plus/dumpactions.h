//
//  dumpactions.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//

#ifndef dumpactions_h
#define dumpactions_h

#include "definitions.h"

void PrintDictWord(int idx, const DictWord *dict);
void PrintHeaderInfo(const Header h);
/* comments is the NULL-terminated list of action comments of a plain text
   database, or NULL */
void DumpActions(char **comments);

#endif /* dumpactions_h */
