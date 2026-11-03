//
//  robin_of_sherwood.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>

#include "decompress_text.h"
#include "sagagraphics.h"
#include "game_specific.h"
#include "irmak.h"
#include "robin_of_sherwood.h"
#include "scott.h"
#include "detect_game.h"
#include "scott_meta.h"
#include "scott_display.h"

/* Sub-image table for the 63 procedurally assembled forest locations
   (rooms 11–73). Each forest room is described by a variable-length
   sequence of image indices; see draw_sherwood(). */
static uint8_t *forest_images = NULL;

#define WATERFALL_ANIMATION_RATE 15
/* Rooms 11-73 are the procedurally assembled forest; they have no stored
   images. */
#define NUM_FOREST_ROOMS 63

enum {
    ROOM_BEDROOM = 7, /* The one with the treasure chest */
    FIRST_FOREST_ROOM = 11,
    LAST_FOREST_ROOM = FIRST_FOREST_ROOM + NUM_FOREST_ROOMS - 1,
    ROOM_OUTLAWS_CAMP = LAST_FOREST_ROOM,
    ROOM_HERNES_CAVE = 79,
    ROOM_DOWN_FROM_TREE = 82, /* Exit target of "up a tree" */
    ROOM_STONE_CIRCLE = 84,
    ROOM_WATERFALL = 86,
    ROOM_UP_A_TREE = 93,
};

enum {
    ITEM_GREGORY_AND_CART = 36, /* Gregory the tax collector */
    ITEM_HERNE = 39,
    ITEM_SERF_AND_CART = 60,
    ITEM_OPEN_CHEST = 62,
    ITEM_HORSE_AND_CART = 77,
};

static void animate_lightning(int stage);

/* Handle game-specific scripted actions triggered by the action table.
   0 = lightning flash at the stone circle (palette-swap animation loop),
   1 = Herne the Hunter apparition (show close-up, move Herne to room 79),
   2 = climb a tree in the forest (save current location, move to room 93). */
void SherwoodAction(int p)
{
    event_t ev;

    switch (p) {
    case 0:
        /* Lightning flash: run 11 frames of palette-swap animation,
           blocking until the sequence completes */
        AnimationFlag = 1;
        glk_request_timer_events(15);

        while (AnimationFlag < 11) {
            glk_select(&ev);
            if (ev.type == evtype_Timer) {
                AnimationFlag++;
                animate_lightning(AnimationFlag);
            }
        }
        break;
    case 1:
        /* Herne the Hunter close-up, then place him at the waterfall cave */
        DrawImage(0);
        Display(Bottom, "\n%s\n", sys[HIT_ENTER]);
        showing_closeup = 1;
        HitEnter();
        Items[ITEM_HERNE].Location = ROOM_HERNES_CAVE;
        Look();
        break;
    case 2:
        /* Climb a tree: remember current forest location so we can
           return when the player climbs back down */
        SavedRoom = MyLoc;
        MyLoc = ROOM_UP_A_TREE;
        Look();
        break;
    default:
        fprintf(stderr, "Unhandled SherwoodAction %d!\n", p);
        break;
    }
}

static int IsForestLocation(void) { return (MyLoc >= FIRST_FOREST_ROOM && MyLoc <= LAST_FOREST_ROOM); }

/* Forest scene composition types */
#define TREES 0  /* 10–11 sub-images: 8 tree columns + undergrowth + optional paths */
#define BUSHES 1 /* 5 sub-images placed side by side */

/* Assemble and draw a procedural forest image for the given location.
   The forest_images table stores a variable-length record per forest room.
   Bit 7 of the first byte distinguishes BUSHES (set) from TREES (clear).
   TREES records have 10 or 11 sub-image indices (trees only vs. trees with
   a path); BUSHES records always have 5. Sub-images are positioned on a
   fixed grid for TREES, or packed left-to-right for BUSHES. */
static void draw_sherwood(int loc)
{
    if (Graphics)
        glk_window_clear(Graphics);
    int subimage_index = 0;

    /* Skip past preceding rooms' records to reach the one for loc */
    for (int i = 0; i < loc - FIRST_FOREST_ROOM; i++) {
        int skip = 5; /* BUSHES: 5 sub-images */
        if (forest_images[subimage_index] < 128)
            skip = 11; /* TREES: 10 or 11 (skip the maximum) */
        subimage_index += skip;
    }

    int forest_type = TREES;
    int subimages;

    if (forest_images[subimage_index] >= 128) {
        forest_type = BUSHES;
        subimages = 5;
    } else if (forest_images[subimage_index + 10] == 0xff) {
        subimages = 10; /* Trees only, no path */
    } else {
        subimages = 11; /* Trees with a forward path */
    }

    int xpos = 0, ypos = 0, image_number;

    for (int i = 0; i < subimages; i++) {
        if (forest_type == TREES) {
            if (i >= 8) {
                if (i == 8) {        /* Undergrowth strip */
                    ypos = 7;
                    xpos = 0;
                } else if (i == 9) { /* Bottom path */
                    ypos = 10;
                    xpos = 0;
                } else {             /* Forward path overlay */
                    ypos = 7;
                    xpos = 12;
                }
            } else {
                /* 8 tree columns, each 4 characters wide */
                ypos = 0;
                xpos = i * 4;
            }
        }

        image_number = forest_images[subimage_index++] & 127;

        DrawPictureAtPos(image_number, xpos, ypos, 0);

        if (forest_type == BUSHES) {
            /* BUSHES sub-images are variable-width, packed left to right */
            xpos += images[image_number].width;
        }
    }
}

/* Animate a waterfall by scrolling its water pixels downward by 'stage'
   pixels, wrapping around within the 64 pixel high area. The waterfall
   is the 8 character lines starting at first_line, cols characters wide
   from first_col. Reads pixel data from the layout buffer (not
   imagebuffer) so the source pattern stays constant across frames.
   In room 86 it is blue water on white; seen from inside the cave
   (room 79) a single column at the right edge, white on blue. */
static void animate_waterfall(int first_col, int cols, int first_line,
    int stage, int water_colour, int background_colour)
{
    int top = first_line * 8;
    RectFill(first_col * 8, top, cols * 8, 64, background_colour);
    for (int line = first_line; line < first_line + 8; line++) {
        for (int col = first_col; col < first_col + cols; col++) {
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    if (PixelSet(layout[col + line * IRMAK_IMGWIDTH][i], j)) {
                        int ypos = line * 8 + i + stage;
                        if (ypos > top + 63)
                            ypos = ypos - 64;
                        PutPixel(col * 8 + j, ypos, water_colour);
                    }
                }
            }
        }
    }
}

static void swap_palette_entries(int a, int b)
{
    glui32 temp = pal[a];
    pal[a] = pal[b];
    pal[b] = temp;
}

/* Draw one frame of the flash animation at the stone circle. Each
   call swaps blue/yellow palette entries (platform-dependent colours)
   and redraws image 77, creating a strobe effect. Stage 3 holds a long
   pause (700 ms) for dramatic effect; stage 11 ends the sequence. */
static void animate_lightning(int stage)
{
    if (palchosen == C64B) {
        swap_palette_entries(6, 7);
    } else {
        swap_palette_entries(1, 14);
        swap_palette_entries(9, 6);
    }
    DrawPictureNumber(77, 0);
    last_image_index = 77;
    if (stage == 11) {
        glk_request_timer_events(0);
    } else if (stage == 3) {
        glk_request_timer_events(700);
    } else {
        glk_request_timer_events(40);
    }
}

/* Draw the graphics for the current room. Non-forest rooms use their
   stored image index plus item overlays. Forest rooms (11–73) are
   assembled procedurally via draw_sherwood, with additional overlays
   for NPCs and the campfire. Also handles the tree-climbing rooms
   (82/93) and starts waterfall animation timers for rooms 86 and 79. */
void RobinOfSherwoodLook(void)
{
    if (!IsForestLocation()) {
        if (Rooms[MyLoc].Image == 255) {
            CloseGraphicsWindow();
        } else {
            DrawImage(Rooms[MyLoc].Image);
            /* Draw images for items present in this room */
            DrawRoomItemImages();
        }
    }

    /* Room 82 is a dummy exit target from "up a tree" — redirect to
       the forest room the player climbed from */
    if (MyLoc == ROOM_DOWN_FROM_TREE)
        MyLoc = SavedRoom;
    /* Room 93 is "up a tree" — move any items here to the actual
       forest location so they appear when the player climbs down */
    if (MyLoc == ROOM_UP_A_TREE)
        for (int i = 0; i < GameHeader.NumItems; i++)
            if (Items[i].Location == ROOM_UP_A_TREE)
                Items[i].Location = SavedRoom;
    /* Open treasure chest overlay in the left bedroom */
    if (MyLoc == ROOM_BEDROOM && Items[ITEM_OPEN_CHEST].Location == ROOM_BEDROOM)
        DrawImage(70);
    if (IsForestLocation()) {
        OpenGraphicsWindow();
        draw_sherwood(MyLoc);

        if (Items[ITEM_GREGORY_AND_CART].Location == MyLoc) {
            DrawImage(15);
            DrawImage(3);
        }
        if (Items[ITEM_SERF_AND_CART].Location == MyLoc || Items[ITEM_HORSE_AND_CART].Location == MyLoc) {
            DrawImage(15);
            DrawImage(12);
        }
        if (MyLoc == ROOM_OUTLAWS_CAMP) {
            DrawImage(36); /* Campfire */
        }
    }

    /* Start waterfall animation at the waterfall or inside the cave */
    if (MyLoc == ROOM_WATERFALL || MyLoc == ROOM_HERNES_CAVE) {
        glk_request_timer_events(WATERFALL_ANIMATION_RATE);
    }
}

/* Timer callback for Robin of Sherwood animations. AnimationFlag cycles
   0–63 as the vertical scroll offset for waterfalls. Room 84 (stone
   circle) keeps the timer alive without drawing — the lightning animation
   uses the same timer and would be cancelled if we stopped it here. */
void UpdateRobinOfSherwoodAnimations(void)
{
    AnimationFlag++;
    if (AnimationFlag > 63)
        AnimationFlag = 0;
    if (MyLoc == ROOM_WATERFALL || MyLoc == ROOM_HERNES_CAVE || MyLoc == ROOM_STONE_CIRCLE) {
        if (MyLoc == ROOM_WATERFALL) {
            animate_waterfall(11, 6, 2, AnimationFlag, BlueColour(), WhiteColour());
        } else if (MyLoc == ROOM_HERNES_CAVE) {
            animate_waterfall(31, 1, 3, AnimationFlag, WhiteColour(), BlueColour());
        }
    } else {
        glk_request_timer_events(0);
    }
}

/* Room image indices: one byte per room. Forest rooms (11–73) are
   skipped because they use the procedural draw_sherwood() instead of
   a single image. */
static int LoadSherwoodRoomImages(int c64)
{
    int offset = file_baseline_offset + ((c64 == 1) ? 0x1ffd : 0x3d99);

    uint8_t *ptr= SeekToPos(offset);

    if (ptr == NULL)
        return 0;

    int ct;
    Room *rp = Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        rp++;
        if (ct == FIRST_FOREST_ROOM - 1) {
            /* Skip the 63 forest rooms (11–73) — no stored images */
            for (int i = 0; i < NUM_FOREST_ROOMS; i++) {
                rp++;
                ct++;
            }
        }
    }
    return 1;
}

/* Room descriptions (5-bit compressed text). Forest rooms all share
   the same hardcoded description. The first letter of each
   decompressed description is lowercased because the engine prepends
   "You are ". */
static int LoadSherwoodRoomDescs(int c64)
{
    int ct = 0;
    Room *rp = Rooms;

    int offset = file_baseline_offset + ((c64 == 1) ? 0x402e : 0x5b7e);

    uint8_t *ptr = SeekToPos(offset);
    if (ptr == NULL)
        return 0;

    do {
        rp->Text = DecompressText(ptr, ct);
        *(rp->Text) = tolower(*(rp->Text));
        ct++;
        if (ct == 11) {
            /* Rooms 11-71 share one description; the stored texts go
               on with room 72 */
            for (int i = 0; i < 61; i++) {
                rp++;
                rp->Text = "in Sherwood Forest";
            }
        }
        rp++;
    } while (ct < 33);
    return 1;
}

/* C64 and Spectrum versions store system messages in different orders;
   map each platform's message array to the canonical SysMessageType keys */
static void MapSherwoodSysMessages(int c64)
{
    if (c64 == 1) {
        SysMessageType messagekey[] = {
            NORTH,
            SOUTH,
            EAST,
            WEST,
            UP,
            DOWN,
            EXITS,
            YOU_SEE,
            YOU_ARE,
            HIT_ENTER,
            YOU_CANT_GO_THAT_WAY,
            OK,
            WHAT_NOW,
            HUH,
            YOU_HAVE_IT,
            TAKEN,
            DROPPED,
            YOU_HAVENT_GOT_IT,
            INVENTORY,
            YOU_DONT_SEE_IT,
            THATS_BEYOND_MY_POWER,
            DIRECTION,
            YOURE_CARRYING_TOO_MUCH,
            PLAY_AGAIN,
            RESUME_A_SAVED_GAME,
            YOU_CANT_DO_THAT_YET,
            I_DONT_UNDERSTAND,
            NOTHING
        };

        MapSysMessages(messagekey, 28, 0);

        /* Deliberately replaces the HIT_ENTER mapped by the key list above */
        sys[HIT_ENTER] = system_messages[30];
        sys[WHAT] = system_messages[13];

    } else {
        /* Several keys appear twice (OK, YOU_SEE, WHAT_NOW, HIT_ENTER, ...):
           the Spectrum version has variant texts for them, and the last
           mapping wins. */
        SysMessageType messagekey[] = {
            BAD_DATA,
            OK,
            OK,
            PLAY_AGAIN,
            I_DONT_UNDERSTAND,
            YOU_CANT_DO_THAT_YET,
            HUH,
            DIRECTION,
            YOU_HAVENT_GOT_IT,
            YOU_HAVE_IT,
            YOU_DONT_SEE_IT,
            THATS_BEYOND_MY_POWER,
            YOU_CANT_GO_THAT_WAY,
            YOU_ARE,
            YOU_SEE,
            YOU_SEE,
            EXITS,
            INVENTORY,
            NOTHING,
            WHAT_NOW,
            HIT_ENTER,
            YOURE_CARRYING_TOO_MUCH,
            RESUME_A_SAVED_GAME
        };

        MAP_SYS_MESSAGES(messagekey, 0);

        sys[WHAT] = sys[HUH];
    }
    sys[EXITS_DELIMITER] = " ";
    sys[MESSAGE_DELIMITER] = ". ";
}

/* Forest sub-image table: 555 bytes of packed sub-image indices
   describing the composition of each forest room (rooms 11–73).
   See draw_sherwood(). */
static void LoadForestImages(int c64)
{
    int offset = file_baseline_offset + ((c64 == 1) ? 0x2300 : 0x3b6e);

    uint8_t *ptr = SeekToPos(offset);
    if (ptr == NULL)
        return;

    int cells = 555;
    forest_images = MemAlloc(cells);

    for (int i = 0; i < cells; i++)
        forest_images[i] = *(ptr++);
}

/* Load extra data that Robin of Sherwood stores outside the standard
   Scott Adams database: per-room image indices, compressed room
   descriptions, system message mappings, and the forest sub-image
   table. All offsets are platform-dependent (C64 vs Spectrum). */
void LoadExtraSherwoodData(int c64)
{
    if (!LoadSherwoodRoomImages(c64) || !LoadSherwoodRoomDescs(c64))
        return;
    MapSherwoodSysMessages(c64);
    LoadForestImages(c64);
}
