/*
 *  loaddatabase.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-06-04.
 */

#ifndef loaddatabase_h
#define loaddatabase_h

#include <stdio.h>

#include "definitions.h"

extern Synonym *Substitutions;

int LoadDatabasePlaintext(FILE *f);
int LoadDatabaseBinary(void);
int LooksLikePlusDatabase(uint8_t *data, size_t size);
int FindAndAddImageFile(const char *shortname, imgrec *rec);

#endif /* loaddatabase_h */
