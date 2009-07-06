#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

/* storage for hyperlink and selection coordinates */
static mask_t *gli_mask;

/* for copy selection */
int gli_copyselect = FALSE;
int gli_drawselect = FALSE;

void gli_resize_mask(int x, int y)
{
    int i;
    int oldsize;
    int rx, ry;

    if (!gli_mask) {
        gli_mask = (mask_t*) calloc(sizeof(mask_t), 1);
        if (!gli_mask) {
            gli_strict_warning("resize_mask: out of memory");
        }
    }

    rx = x + 1;
    ry = y + 1;

    /* preserve contents of hyperlink storage */
    if (rx < gli_mask->hor) {
        for (i = rx; i < gli_mask->hor; i++) {
            if (gli_mask->links[i]) {
                free(gli_mask->links[i]);
            }
        }
    }

    oldsize = gli_mask->hor < rx ? gli_mask->hor : rx;
    gli_mask->hor = rx;
    gli_mask->ver = ry;

    /* resize hyperlinks storage */
    gli_mask->links = (glui32**)realloc(gli_mask->links, gli_mask->hor * sizeof(glui32*));
    if (!gli_mask->links) {
        gli_strict_warning("resize_mask: out of memory");
        free(gli_mask->links);
        gli_mask->hor = 0;
        gli_mask->ver = 0;
        return;
    }

    for (i = 0; i < oldsize; i++) {
        gli_mask->links[i] = (glui32*) realloc(gli_mask->links[i], gli_mask->ver * sizeof(glui32));
        if (!gli_mask->links[i]) {
            gli_strict_warning("resize_mask: could not reallocate old memory");
            return;
        }
    }

    for (i = oldsize; i < gli_mask->hor; i++) {
        gli_mask->links[i] = (glui32*) calloc(sizeof(glui32), gli_mask->ver);
        if (!gli_mask->links[i]) {
            gli_strict_warning("resize_mask: could not allocate new memory");
            return;
        }
    }

    gli_mask->select.x0 = 0;
    gli_mask->select.y0 = 0;
    gli_mask->select.x1 = 0;
    gli_mask->select.y1 = 0;

    return;
}

void gli_put_hyperlink(glui32 linkval, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1)
{
    int i, k;
    int tx0 = x0 < x1 ? x0 : x1;
    int tx1 = x0 < x1 ? x1 : x0;
    int ty0 = y0 < y1 ? y0 : y1;
    int ty1 = y0 < y1 ? y1 : y0;
    glui32* m;

    if (!gli_mask || !gli_mask->hor || !gli_mask->ver) {
        gli_strict_warning("set_hyperlink: struct not initialized");
        return;
    }

    if (tx0 > gli_mask->hor
            || tx1 > gli_mask->hor
            || ty0 > gli_mask->ver   || ty1 > gli_mask->ver
            || !gli_mask->links[tx0] || !gli_mask->links[tx1]) {
        gli_strict_warning("set_hyperlink: invalid range given");
        return;
    }

    for (i = tx0; i < tx1; i++) {
        for (k = ty0; k < ty1; k++) {
            gli_mask->links[i][k] = linkval;
        }
    }

    return;
}

glui32 gli_get_hyperlink(unsigned int x, unsigned int y)
{
    if (!gli_mask || !gli_mask->hor || !gli_mask->ver) {
        gli_strict_warning("get_hyperlink: struct not initialized");
        return;
    }

    if (x > gli_mask->hor
            || y > gli_mask->ver
            || !gli_mask->links[x]) {
        //gli_strict_warning("get_hyperlink: invalid range given");
        return;
    }

    return gli_mask->links[x][y];
}

void gli_start_selection(int x, int y)
{
    int tx, ty;

    if (!gli_mask || !gli_mask->hor || !gli_mask->ver) {
        gli_strict_warning("start_selection: mask not initialized");
        return;
    }

    tx = x < gli_mask->hor ? x : gli_mask->hor;
    ty = y < gli_mask->ver ? y : gli_mask->ver;

    gli_mask->select.x0 = tx;
    gli_mask->select.y0 = ty;
    gli_mask->select.x1 = 0;
    gli_mask->select.y1 = 0;

    gli_force_redraw = 1;
    gli_windows_redraw();
    return;
}

void gli_move_selection(int x, int y)
{
    int tx, ty;

    if (!gli_mask || !gli_mask->hor || !gli_mask->ver) {
        gli_strict_warning("move_selection: mask not initialized");
        return;
    }

    tx = x < gli_mask->hor ? x : gli_mask->hor;
    ty = y < gli_mask->ver ? y : gli_mask->ver;

    gli_mask->select.x1 = tx;
    gli_mask->select.y1 = ty;

    gli_windows_redraw();
    return;
}

void gli_clear_selection(void)
{
    if (!gli_mask) {
        gli_strict_warning("clear_selection: mask not initialized");
        return;
    }

    gli_mask->select.x0 = 0;
    gli_mask->select.y0 = 0;
    gli_mask->select.x1 = 0;
    gli_mask->select.y1 = 0;

    return;
}

int gli_check_selection(unsigned int x0, unsigned int y0,
        unsigned int x1, unsigned int y1)
{
    int cx0, cx1, cy0, cy1;

    cx0 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x0
            : gli_mask->select.x1;

    cx1 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x1
            : gli_mask->select.x0;

    cy0 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y0
            : gli_mask->select.y1;

    cy1 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y1
            : gli_mask->select.y0;

    if (!cx0 || !cx1 || !cy0 || !cy1)
        return FALSE;

    if (cx0 >= x0 && cx0 <= x1
            && cy0 >= y0 && cy0 <= y1)
        return TRUE;

    if (cx0 >= x0 && cx0 <= x1
            && cy1 >= y0 && cy1 <= y1)
        return TRUE;

    if (cx1 >= x0 && cx1 <= x1
            && cy0 >= y0 && cy0 <= y1)
        return TRUE;

    if (cx1 >= x0 && cx1 <= x1
            && cy1 >= y0 && cy1 <= y1)
        return TRUE;

    return FALSE;
}

int gli_get_selection(unsigned int x0, unsigned int y0,
        unsigned int x1, unsigned int y1,
        unsigned int *rx0, unsigned int *rx1)
{
    unsigned int row, above, below;
    int found_left, found_right;
    int cx0, cx1, cy0, cy1;
    int i;

    row = (y0 + y1)/2;
    above = row - gli_leading;
    below = row + gli_leading;

    found_left = FALSE;
    found_right = FALSE;

    cx0 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x0
            : gli_mask->select.x1;

    cx1 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x1
            : gli_mask->select.x0;

    cy0 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y0
            : gli_mask->select.y1;

    cy1 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y1
            : gli_mask->select.y0;

    *rx0 = 0;
    *rx1 = 0;

    if (!(row >= cy0 && row <= cy1))
        return FALSE;

    if (above >= cy0 && above <= cy1) {
        *rx0 = x0;
        found_left = TRUE;
    }

    if (below >= cy0 && below <= cy1) {
        *rx1 = x1;
        found_right = TRUE;
    }

    if (found_left && found_right)
        return TRUE;

    for (i = x0; i <= x1; i++) {
        if (i >= cx0 && i <= cx1) {
            if (!found_left) {
                *rx0 = i;
                found_left = TRUE;
                if (found_right)
                    return TRUE;
            } else {
                if (!found_right)
                    *rx1 = i;
            }
        }
    }

    if (rx0 && !rx1)
        *rx1 = x1;

    return (rx0 && rx1);
}

void gli_clipboard_copy(glui32 *buf, int len)
{
    winclipstore(buf, len);
    return;
}
