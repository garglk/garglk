//
//  hulk.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-18.
//

#include <string.h>

#include "apple2draw.h"
#include "detect_game.h"
#include "game_specific.h"
#include "hulk.h"
#include "saga.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scott_display.h"

/* The UK releases have the table of close-ups of the US ones in a shorter
   form: seven items followed by their nouns. The interpreter only looks at
   the six that have a picture (the seventh is the gas outlet), and compares
   the verb with a number in its code. Their pictures are 28 onwards.

   The table is not in the same place in every C64 file, so it is found by
   the 24 bytes in front of it, which are the same in all releases. */
void HulkReadCloseups(void)
{
    enum { ITEMS = 7, CLOSEUPS = 6, FIRST_PICTURE = 28, LOOK = 39 };
    static const uint8_t before[] = {
        0x14, 0x13, 0x0f, 0x00, 0x0e, 0x0e, 0x00, 0x00, 0x05, 0x00, 0x03, 0x08,
        0x09, 0x00, 0x09, 0x04, 0x04, 0x09, 0x0a, 0x01, 0x09, 0x00, 0x04, 0x04
    };

    if (entire_file == NULL)
        return;
    for (size_t i = 0; i + sizeof before + 2 * ITEMS <= file_length; i++) {
        if (memcmp(entire_file + i, before, sizeof before) != 0)
            continue;
        const uint8_t *items = entire_file + i + sizeof before;
        SetCloseupVerb(LOOK, 0);
        for (int j = 0; j < CLOSEUPS; j++)
            AddCloseup(items[j], items[ITEMS + j], FIRST_PICTURE + j);
        return;
    }
}

enum {
    ROOM_UNDERGROUND = 15, /* Small underground room */
    ITEM_BIO_GEM = 18,
    ITEM_DR_STRANGE = 26, /* Astral projection of Dr. Strange */
    ITEM_PLUGGED_OUTLET = 28,
};

void HulkLook(void)
{
    DrawImage(Rooms[MyLoc].Image);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        int image = Items[ct].Image;
        if (Items[ct].Location == MyLoc && image != 255) {
            /* Don't draw bio gem in fuzzy area */
            if ((ct == ITEM_BIO_GEM && MyLoc != ROOM_UNDERGROUND) ||
                /* Don't draw Dr. Strange until outlet is plugged */
                (ct == ITEM_DR_STRANGE && Items[ITEM_PLUGGED_OUTLET].Location != MyLoc))
                continue;
            DrawImage(image);
        }
    }
}

void DrawHulkImage(int p)
{
    int image = 0;
    switch (p) {
    case 85:
        /* Hulk lifts dome */
        image = 34;
        break;
    case 86:
        /* Gas turns Hulk into Banner */
        image = 35;
        break;
    case 83:
        /* Chair breaks */
        image = 36;
        break;
    case 84:
        /* Banner becomes Hulk */
        image = 37;
        break;
    case 87:
        /* Victory: Chief Examiner and gems */
        image = 38;
        break;
    case 88:
        /* Hulk thinks about Nightmare */
        image = 39;
        break;
    case 89:
        /* Hulk digs */
        image = 40;
        break;
    case 82:
        /* Banner looks into mirror */
        image = 41;
        break;
    case 81:
        /* Hulk looks into mirror */
        image = 42;
        break;
    default:
        fprintf(stderr, "Unhandled image number %d!\n", p);
        break;
    }

    if (image != 0)
        ShowCloseup(image);
}
