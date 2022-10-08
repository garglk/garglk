//
//  layouttext.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-11.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"
#include "layouttext.h"

static int FindBreak(const char *buf, int pos, int columns)
{
    int diff = 0;

    if (isspace((unsigned char)buf[pos]))
        return 0;

    while (diff < columns && !isspace((unsigned char)buf[pos])) {
        pos--;
        diff++;
    }

    if (diff >= columns || diff < 1) /* Found no space */ {
        return -1;
    }

    return diff;
}

/* Breaks a null-terminated string up by inserting newlines, moving words
 down to the next line when reaching the end of the line */
char *LineBreakText(char *source, int columns, int *rows, int *length)
{
    columns -= 1;

    char *result = NULL;
    char buf[768];
    int col = 0;
    int row = 0;
    int sourcepos = 0;
    int destpos = 0;
    int diff = 0;
    *rows = 0;
    if (!source)
        return NULL;
    while (source[sourcepos] != '\0' && (source[sourcepos] == ' ' || source[sourcepos] == '.'))
        sourcepos++;
    while (source[sourcepos] != '\0') {
        while (col < columns && source[sourcepos] != '\0') {
            if (source[sourcepos] == 10 || source[sourcepos] == 13) {
                /* Found a line break. */
                /* Any spaces before a line break may cause trouble, */
                /* so we delete them */
                while (destpos && buf[destpos - 1] == ' ') {
                    destpos--;
                }
                col = 0;
                row++;
            } else {
                col++;
            }

            buf[destpos++] = source[sourcepos++];

            if (source[sourcepos] == 10 || source[sourcepos] == 13)
                col--;
        }

        /* We have reached the end of a line */
        row++;
        col = 0;

        if (source[sourcepos] == '\0') {
            break;
        }

        diff = FindBreak(source, sourcepos, columns);
        if (diff > -1) { /* We found a suitable break */
            sourcepos = sourcepos - diff;
            destpos = destpos - diff;
            buf[destpos++] = '\n';

            if (isspace((unsigned char)source[sourcepos])) {
                sourcepos++;
            }
        }
    }
    *rows = row;
    *length = 0;
    result = MemAlloc(destpos + 1);
    memcpy(result, buf, destpos);
    result[destpos] = '\0';
    *length = destpos;
    return result;
}
