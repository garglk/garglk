//
//  robinofsherwood.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <string.h>

#include "robinofsherwood.h"

#include "decompresstext.h"
#include "sagadraw.h"
#include "sagagraphics.h"
#include "scott.h"

extern Image *images;
extern uint8_t screenchars[768][8];

static uint8_t *forest_images = NULL;

#define WATERFALL_ANIMATION_RATE 15

void animate_lightning(int stage);

void SherwoodAction(int p)
{
    event_t ev;

    switch (p) {
    case 0:
        // Flash animation
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
        DrawImage(0); /* Herne */
        Display(Bottom, "\n%s\n", sys[HIT_ENTER]);
        showing_closeup = 1;
        HitEnter();
        Items[39].Location = 79;
        Look();
        break;
    case 2:
        // Climbing tree in forest
        SavedRoom = MyLoc;
        MyLoc = 93;
        Look();
        break;
    default:
        fprintf(stderr, "Unhandled SherwoodAction %d!\n", p);
        break;
    }
}

int IsForestLocation(void) { return (MyLoc >= 11 && MyLoc <= 73); }

#define TREES 0
#define BUSHES 1

void draw_sherwood(int loc)
{
    glk_window_clear(Graphics);
    int subimage_index = 0;

    for (int i = 0; i < loc - 11; i++) {
        // BUSHES type images are made up of 5 smaller images
        int skip = 5;
        if (forest_images[subimage_index] < 128)
            // Those with bit 7 unset have 10 (trees only) or 11 (trees with path)
            skip = 11;
        subimage_index += skip;
    }

    int forest_type = TREES;
    int subimages;

    // if bit 7 of the image index is set then this image is BUSHES
    if (forest_images[subimage_index] >= 128) {
        forest_type = BUSHES;
        subimages = 5;
        // if the last subimage value is 255, there is no path
    } else if (forest_images[subimage_index + 10] == 0xff) {
        // Trees only
        subimages = 10;
    } else {
        // Trees with path
        subimages = 11;
    }

    int xpos = 0, ypos = 0, image_number;

    for (int i = 0; i < subimages; i++) {
        if (forest_type == TREES) {
            if (i >= 8) {
                if (i == 8) { // Undergrowth
                    ypos = 7;
                    xpos = 0;
                } else if (i == 9) { // Bottom path
                    ypos = 10;
                    xpos = 0;
                } else { // Forward path
                    ypos = 7;
                    xpos = 12;
                }
            } else { // Trees (every tree image is 4 characters wide)
                ypos = 0;
                xpos = i * 4;
            }
        }

        image_number = forest_images[subimage_index++] & 127;

        DrawSagaPictureAtPos(image_number, xpos, ypos);

        if (forest_type == BUSHES) {
            xpos += images[image_number].width;
        }
    }
}

void animate_waterfall(int stage)
{
    RectFill(88, 16, 48, 64, white_colour);
    for (int line = 2; line < 10; line++) {
        for (int col = 11; col < 17; col++) {
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < 8; j++)
                    if ((screenchars[col + line * 32][i] & (1 << j)) != 0) {
                        int ypos = line * 8 + i + stage;
                        if (ypos > 79)
                            ypos = ypos - 64;
                        PutPixel(col * 8 + j, ypos, blue_colour);
                    }
        }
    }
}

void animate_waterfall_cave(int stage)
{
    RectFill(248, 24, 8, 64, white_colour);
    for (int line = 3; line < 11; line++) {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                if ((screenchars[31 + line * 32][i] & (1 << j)) != 0) {
                    int ypos = line * 8 + i + stage;
                    if (ypos > 87)
                        ypos = ypos - 64;
                    PutPixel(248 + j, ypos, blue_colour);
                }
    }
}

void animate_lightning(int stage)
{
    // swich blue and bright yellow
    if (palchosen == C64B)
        SwitchPalettes(6, 7);
    else {
        SwitchPalettes(1, 14);
        SwitchPalettes(9, 6);
    }
    DrawSagaPictureNumber(77);
    if (stage == 11) {
        glk_request_timer_events(0);
    } else if (stage == 3) {
        glk_request_timer_events(700);
    } else {
        glk_request_timer_events(40);
    }
}

void RobinOfSherwoodLook(void)
{
    if (!IsForestLocation()) {
        if (Rooms[MyLoc].Image == 255) {
            CloseGraphicsWindow();
        } else {
            DrawImage(Rooms[MyLoc].Image);
            for (int ct = 0; ct <= GameHeader.NumItems; ct++)
                if (Items[ct].Image) {
                    if ((Items[ct].Flag & 127) == MyLoc && Items[ct].Location == MyLoc) {
                        DrawImage(Items[ct].Image);
                    }
                }
        }
    }

    if (MyLoc == 82) // Dummy room where exit from Up a tree goes
        MyLoc = SavedRoom;
    if (MyLoc == 93) // Up a tree
        for (int i = 0; i < GameHeader.NumItems; i++)
            if (Items[i].Location == 93)
                Items[i].Location = SavedRoom;
    if (MyLoc == 7 && Items[62].Location == 7) // Left bedroom, open treasure chest
        DrawImage(70);
    if (IsForestLocation()) {
        OpenGraphicsWindow();
        draw_sherwood(MyLoc);

        if (Items[36].Location == MyLoc) {
            //"Gregory the tax collector with his horse and cart"
            DrawImage(15); // Horse and cart
            DrawImage(3); // Sacks of grain
        }
        if (Items[60].Location == MyLoc || Items[77].Location == MyLoc) {
            // "A serf driving a horse and cart"
            DrawImage(15); // Horse and cart
            DrawImage(12); // Hay
        }
        if (MyLoc == 73) {
            // Outlaws camp
            DrawImage(36); // Campfire
        }
    }

    if (MyLoc == 86 || MyLoc == 79) {
        glk_request_timer_events(WATERFALL_ANIMATION_RATE);
    }
}

void UpdateRobinOfSherwoodAnimations(void)
{
    AnimationFlag++;
    if (AnimationFlag > 63)
        AnimationFlag = 0;
    if (MyLoc == 86 || MyLoc == 79 || MyLoc == 84) {
        /* If we're in room 84, the stone circle, we just */
        /* want the timer to not switch off */
        if (MyLoc == 86) {
            animate_waterfall(AnimationFlag);
        } else if (MyLoc == 79) {
            animate_waterfall_cave(AnimationFlag);
        }
    } else {
        glk_request_timer_events(0);
    }
}

GameIDType LoadExtraSherwoodData(void)
{

// room images

    int offset = 0x3d99 + file_baseline_offset;
    uint8_t *ptr;
    /* Load the room images */

    ptr = SeekToPos(entire_file, offset);
    if (ptr == 0)
        return 0;

    int ct;
    Room *rp = Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        rp++;
        if (ct == 10) {
            for (int i = 0; i < 63; i++) {
                rp++;
                ct++;
            }
        }
    }

// rooms

    ct = 0;
    rp = Rooms;

    int actual_room_number = 0;

    ptr = SeekToPos(entire_file, 0x5b7e + file_baseline_offset);
    if (ptr == 0)
        return 0;

    do {
        rp->Text = DecompressText(ptr, ct);
        *(rp->Text) = tolower(*(rp->Text));
        ct++;
        actual_room_number++;
        if (ct == 11) {
            for (int i = 0; i < 61; i++) {
                rp++;
                rp->Text = "in Sherwood Forest";
                actual_room_number++;
            }
        }
        rp++;
    } while (ct < 33);

    for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
        sys[i] = system_messages[4 - I_DONT_UNDERSTAND + i];

    for (int i = YOU_SEE; i <= HIT_ENTER; i++)
        sys[i] = system_messages[15 - YOU_SEE + i];

    sys[OK] = system_messages[2];
    sys[TAKEN] = system_messages[2];
    sys[DROPPED] = system_messages[2];
    sys[PLAY_AGAIN] = system_messages[3];
    sys[YOURE_CARRYING_TOO_MUCH] = system_messages[21];
    sys[YOU_CANT_GO_THAT_WAY] = system_messages[12];
    sys[YOU_ARE] = system_messages[13];
    sys[EXITS_DELIMITER] = " ";
    sys[MESSAGE_DELIMITER] = ". ";

    ptr = SeekToPos(entire_file, 0x3b6e + file_baseline_offset);
    if (ptr == 0)
        return 0;

    int cells = 555;
    forest_images = MemAlloc(cells);

    for (int i = 0; i < cells; i++)
        forest_images[i] = *(ptr++);

    return ROBIN_OF_SHERWOOD;
}

GameIDType LoadExtraSherwoodData64(void)
{

// room images

    int offset = 0x1ffd + file_baseline_offset;
    uint8_t *ptr;
    /* Load the room images */

    ptr = SeekToPos(entire_file, offset);
    if (ptr == 0)
        return 0;

    int ct;
    Room *rp = Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        rp++;

        if (ct == 10) {
            for (int i = 0; i < 63; i++) {
                rp++;
                ct++;
            }
        }
    }

// rooms

    ct = 0;
    rp = Rooms;

    int actual_room_number = 0;

    offset = 0x402e + file_baseline_offset;

    ptr = SeekToPos(entire_file, offset);
    if (ptr == 0)
        return 0;

    do {
        rp->Text = DecompressText(ptr, ct);
        *(rp->Text) = tolower(*(rp->Text));
        ct++;
        actual_room_number++;
        if (ct == 11) {
            for (int i = 0; i < 61; i++) {
                rp++;
                rp->Text = "in Sherwood Forest";
                actual_room_number++;
            }
        }
        rp++;
    } while (ct < 33);

    SysMessageType messagekey[] = { NORTH,
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
        NOTHING };

    for (int i = 0; i < 26; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    sys[HIT_ENTER] = system_messages[30];
    sys[WHAT] = system_messages[13];

    sys[EXITS_DELIMITER] = " ";
    sys[MESSAGE_DELIMITER] = ". ";

    offset = 0x2300 + file_baseline_offset;

    ptr = SeekToPos(entire_file, offset);
    if (ptr == 0)
        return 0;

    int cells = 555;
    forest_images = MemAlloc(cells);

    for (int i = 0; i < cells; i++) {
        forest_images[i] = *(ptr++);
    }

    return ROBIN_OF_SHERWOOD_C64;
}
