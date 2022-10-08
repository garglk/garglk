//
//  loaddatabase.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef loaddatabase_h
#define loaddatabase_h

#include <stdio.h>

#include "definitions.h"

int LoadDatabasePlaintext(FILE *f, int loud);
int LoadDatabaseBinary(void);
void PrintDictWord(int idx, DictWord *dict);
int FindAndAddImageFile(char *shortname, struct imgrec *rec);

#endif /* loaddatabase_h */
