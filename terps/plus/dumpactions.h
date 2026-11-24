/*
 *  dumpactions.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2026-10-12.
 */

#ifndef dumpactions_h
#define dumpactions_h

#include "definitions.h"

void PrintDictWord(int idx, const DictWord *dict);
void PrintHeaderInfo(const Header h);
/* comments is the NULL-terminated list of action comments of a plain text
   database, or NULL */
void DumpActions(char **comments);

#endif /* dumpactions_h */
