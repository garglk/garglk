//
//  layouttext.h
//  Part of TaylorMade, ScottFree and Plus,
//  interpreters of related text adventure formats.
//
//  Created by Petter Sjölund on 2022-01-11.
//

#ifndef layouttext_h
#define layouttext_h

#include "glk.h"

/* Word-wraps a null-terminated string to lines of at most columns - 1
   characters, which leaves the last column of a text grid free. Leading
   spaces and full stops and trailing blank lines are dropped. Returns a
   newly allocated string with the lines separated by newlines, and the
   number of lines in *rows (0 for an empty string). */
char *LineBreakText(const char *source, int columns, int *rows);

/* Shows text in top, a text grid window split off above bottom with a
   fixed size: wraps it to the width of top, makes top one row taller than
   the text, and prints the text with print, a line at a time. The extra
   row is left blank for the caller to draw a delimiter in, so empty text
   leaves top one row tall. If bottom is shorter than three rows and top
   too short for the text, top is left as it is rather than squeeze bottom
   further. Returns 1 if top was resized, so its last row is free, else 0. */
int ShowTextInTopWindow(winid_t top, winid_t bottom, const char *text,
    void (*print)(winid_t win, const char *text));

#endif /* layouttext_h */
