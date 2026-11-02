//
//  layouttext.c
//  Part of TaylorMade, ScottFree and Plus,
//  interpreters of related text adventure formats.
//
//  Created by Petter Sjölund on 2022-01-11.
//

#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "memory_allocation.h"

#include "layouttext.h"

/* A bottom window shorter than this is not made any shorter */
#define MIN_BOTTOM_ROWS 3

static int IsLineBreak(char c)
{
    return c == '\n' || c == '\r';
}

char *LineBreakText(const char *source, int columns, int *rows)
{
    *rows = 0;
    if (source == NULL)
        source = "";

    while (*source == ' ' || *source == '.')
        source++;

    /* Wrap at columns - 1: a line that fills the last column of a text grid
       moves the cursor to the next row, so the newline after it would leave
       a blank row. */
    size_t width = columns > 2 ? (size_t)columns - 1 : 1;
    size_t len = strlen(source);
    /* Every break replaces a space or adds a newline after at least one
       character, so the result is at most twice as long. */
    char *result = MemAlloc(2 * len + 1);
    size_t out = 0;

    const char *line = source;
    while (*line != '\0') {
        size_t linelen = 0;
        while (line[linelen] != '\0' && !IsLineBreak(line[linelen]))
            linelen++;
        const char *next = line + linelen;
        if (next[0] == '\r' && next[1] == '\n')
            next += 2;
        else if (*next != '\0')
            next++;
        /* Spaces at the end of a line would only push it over the edge */
        while (linelen > 0 && line[linelen - 1] == ' ')
            linelen--;

        while (linelen > width) {
            /* Break at the last space that leaves at most width characters
               on this line, or in the middle of a word with no such space */
            size_t brk = width;
            while (brk > 0 && line[brk] != ' ')
                brk--;
            size_t skip = brk;
            if (brk == 0) {
                brk = skip = width;
            } else {
                while (brk > 0 && line[brk - 1] == ' ')
                    brk--;
                while (line[skip] == ' ')
                    skip++;
                if (brk == 0)
                    brk = skip = width;
            }
            memcpy(result + out, line, brk);
            out += brk;
            result[out++] = '\n';
            line += skip;
            linelen -= skip;
        }
        memcpy(result + out, line, linelen);
        out += linelen;
        if (*next != '\0' || IsLineBreak(next[-1]))
            result[out++] = '\n';
        line = next;
    }

    while (out > 0 && (result[out - 1] == '\n' || result[out - 1] == ' '))
        out--;
    result[out] = '\0';

    if (out > 0) {
        *rows = 1;
        for (size_t i = 0; i < out; i++)
            if (result[i] == '\n')
                (*rows)++;
    }
    return result;
}

int ShowTextInTopWindow(winid_t top, winid_t bottom, const char *text,
    void (*print)(winid_t win, const char *text))
{
    glui32 width, height, bottomheight;
    glk_window_clear(top);
    glk_window_get_size(top, &width, &height);
    glk_window_get_size(bottom, NULL, &bottomheight);

    int rows;
    char *wrapped = LineBreakText(text, (int)width, &rows);
    glui32 wanted = (glui32)rows + 1;

    int resized = 0;
    if (bottomheight >= MIN_BOTTOM_ROWS || height >= wanted) {
        glk_window_set_arrangement(glk_window_get_parent(top),
            winmethod_Above | winmethod_Fixed, wanted, top);
        resized = 1;
    }

    /* The lines are printed one at a time, as print may have a buffer
       shorter than the whole text. */
    glk_window_move_cursor(top, 0, 0);
    char *line = wrapped;
    while (*line != '\0') {
        char *end = strchr(line, '\n');
        if (end == NULL) {
            print(top, line);
            break;
        }
        char after = end[1];
        end[1] = '\0';
        print(top, line);
        end[1] = after;
        line = end + 1;
    }

    free(wrapped);
    return resized;
}
