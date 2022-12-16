//
//  layouttext.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-11.
//

#ifndef layouttext_h
#define layouttext_h

#include <stdio.h>

/* Breaks a null-terminated string up by inserting newlines,*/
/* moving words down to the next line when reaching the end of the line */
char *LineBreakText(char *source, int columns, int *rows, int *length);

#endif /* layouttext_h */
