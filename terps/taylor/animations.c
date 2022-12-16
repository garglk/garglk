//
//  animations.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-24.
//

// This is currently inefficient in two ways: 1) we redraw the entire
// image when only parts have changed, and 2) we sometimes draw the
// entire image, and then draw a second animation on top of this.
//
// There should be a second buffer array copy that represents what is
// currently on-screen, and every pixel should be compared against this
// to see if it is the same before being redrawn.
//
// The click shelf animation should be done by setting the bitmap in a
// buffer array first, before drawing the image. This will require a
// second copy of the image buffer.

#include "sagadraw.h"
#include "animations.h"
#include "utility.h"

#define UNFOLDING_SPACE 50
#define STARS_ANIMATION_RATE 15
#define STARS_ANIMATION_RATE_64 40
#define FIELD_ANIMATION_RATE 10
#define SERPENT_ANIMATION_RATE 100
#define ROBOT_ANIMATION_RATE 400
#define QUEEN_ANIMATION_RATE 120

int AnimationRunning = 0;
static int KaylethAnimationIndex = 0;
static int AnimationStage = 0;
static int ClickShelfStage = 0;

extern uint8_t buffer[384][9];
extern Image *images;
extern int draw_to_buffer;

static void AnimateStars(void)
{
    int carry;
    /* First fill area with black, erasing all stars */
    RectFill(48, 16, 160, 32, 0, 0);
    /* We go line by line and pixel row by pixel row */
    for (int line = 3; line < 7; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            carry = 0;
            /* The left half is rotated one pixel to the left, */
            /* byte by byte, but we actually rotate to the right */
            /* because the bytes are flipped in our implementation */
            /* for some reason */
            for (int col = 15; col > 5; col--) {
                uint8_t attribute = buffer[col + line * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                ink = Remap(ink);
                for (int bit = 0; bit < 8; bit++) {
                    if ((buffer[col + line * 32][pixrow] & (1 << bit)) != 0) {
                        PutPixel(col * 8 + bit, line * 8 + pixrow, ink);
                    }
                }
                carry = rotate_right_with_carry(&(buffer[col + line * 32][pixrow]), carry);
            }
            if (carry) {
                buffer[line * 32 + 15][pixrow] = buffer[line * 32 + 15][pixrow] | 128;
            }
            carry = 0;
            /* Then the right half */
            for (int col = 16; col < 26; col++) {
                uint8_t attribute = buffer[col + line * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                ink = Remap(ink);
                for (int pix = 0; pix < 8; pix++) {
                    if ((buffer[col + line * 32][pixrow] & (1 << pix)) != 0) {
                        PutPixel(col * 8 + pix, line * 8 + pixrow, ink);
                    }
                }
                carry = rotate_left_with_carry(&(buffer[col + line * 32][pixrow]), carry);
            }
            buffer[line * 32 + 16][pixrow] = buffer[line * 32 + 16][pixrow] | carry;
        }
    }
}

static void AnimateForcefield(void)
{
    int carry;
    /* First fill door area with black, erasing field */
    RectFill(104, 16, 48, 39, 0, 0);
    /* We go line by line and pixel row by pixel row */

    uint8_t colour = buffer[2 * 32 + 13][8];
    glui32 ink = Remap(colour & 0x7);

    for (int line = 2; line < 7; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            carry = 0;
            for (int col = 13; col < 19; col++) {
                for (int pix = 0; pix < 8; pix++) {
                    if ((buffer[col + line * 32][pixrow] & (1 << pix)) != 0) {
                        PutPixel(col * 8 + pix, line * 8 + pixrow, ink);
                    }
                }
                /* The force field is rotated one pixel to the right, */
                /* byte by byte, but we actually rotate to the left */
                /* because the bytes are flipped in our implementation */
                /* for some reason */
                carry = rotate_left_with_carry(&(buffer[col + line * 32][pixrow]), carry);
            }
            buffer[line * 32 + 13][pixrow] = buffer[line * 32 + 13][pixrow] | carry;
        }
    }
}

static void FillCell(int cell, glui32 ink) {
    int startx = (cell % 32) * 8;
    int starty = (cell / 32) * 8;
    for (int pixrow = 0; pixrow < 8; pixrow++) {
        for (int pix = 0; pix < 8; pix++) {
            if ((buffer[cell][pixrow] & (1 << pix)) == 0) {
                PutPixel(startx + pix, starty + pixrow, ink);
            }
        }
    }
}

static void AnimateQueenComputer(void)
{
    int offset = 3;
    int rotatingink = 7 - (AnimationStage - 7 * (AnimationStage > 7));
    /* First fill areas with black */
    for (int i = 0; i < 3; i++) {
        for (int line = 3; line <= 6; line += 3) {
            for (int cell = 0; cell < 2; cell++) {
                FillCell(line * 32 + offset + cell, 8 + rotatingink);
                rotatingink--;
                if (rotatingink < 0)
                    rotatingink = 7;
            }
        }
        if (offset == 3)
            offset = 10;
        else if (offset == 10)
            offset = 27;
    }
    for (int cell = 0; cell < 3; cell++) {
        FillCell(7 * 32 + 18 + cell, 8 + rotatingink--);
        if (rotatingink < 0)
            rotatingink = 7;
    }
    if (AnimationStage == 0) {
        draw_to_buffer = 0;
        /* Image block 18: Arcadian head */
        DrawSagaPictureAtPos(18, 18, 1);
        draw_to_buffer = 1;
    }

    if (AnimationStage == 7) {
        RectFill(144, 8, 24, 40, 0, 0);
    }

    AnimationStage++;
    if (AnimationStage > 15)
        AnimationStage = 0;
}

static void AnimateKaylethClickShelves(int stage)
{
    RectFill(100, 0, 56, 81, 0, 0);
    for (int line = 0; line < 10; line++) {
        for (int col = 12; col < 20; col++) {
            for (int i = 0; i < 8; i++) {
                int ypos = line * 8 + i + stage;
                if (ypos > 79)
                    ypos = ypos - 80;
                uint8_t attribute = buffer[col + (ypos / 8) * 32][8];
                glui32 ink = attribute & 7;
                ink += 8 * ((attribute & 64) == 64);
                attribute = buffer[col + ((79 - ypos) / 8) * 32][8];
                glui32 ink2 = attribute & 7;
                ink2 += 8 * ((attribute & 64) == 64);
                ink = Remap(ink);
                ink2 = Remap(ink2);
                for (int j = 0; j < 8; j++)
                    if (col > 15) {
                        if ((buffer[col + line * 32][i] & (1 << j)) != 0) {
                            PutPixel(col * 8 + j, ypos, ink);
                        }
                    } else {
                        if ((buffer[col + (9 - line) * 32][7 - i] & (1 << j)) != 0) {
                            PutPixel(col * 8 + j, 79 - ypos, ink2);
                        }
                    }
            }
        }
    }
}

static int UpdateKaylethAnimationFrames(void) // Draw animation frame
{
    if (MyLoc == 0) {
        return 0;
    }

    uint8_t *ptr = &FileImage[AnimationData];
    int counter = 0;
    // Jump to animation index equal to location index
    // Animations are delimited by 0xff
    do {
        if (*ptr == 0xff)
            counter++;
        ptr++;
    } while(counter < MyLoc);

    while(1) {
        if (*ptr == 0xff) {
            return 0;
        }
        int Stage = *ptr;
        ptr++;
        uint8_t *LastInstruction = ptr;
        if (ObjectLoc[*ptr] != MyLoc && *ptr != 0 && *ptr != 122) {
            //Returning because location of object *ptr is not current location
            return 0;
        }
        ptr++;
        // Reset animation if we are in a new room
        if (KaylethAnimationIndex != MyLoc) {
            KaylethAnimationIndex = MyLoc;
            *ptr = 0;
        }

        ptr += *ptr + 1;
        int AnimationRate = *ptr;

        // This is needed to make conveyor belt animation 2 smooth
        // (the one you see after getting up)
        // No idea why, this code is still largely a mystery
        if (KaylethAnimationIndex == 2 && AnimationRate == 50)
            AnimationRate = 10;

        if (AnimationRate != 0xff) {
            ptr++;
            (*ptr)++;
            if (AnimationRate == *ptr) {
                *ptr = 0;
                // Draw "room image" *(ptr + 1) (Actually an animation frame)
                int result = *(ptr + 1);
                DrawTaylor(result);
                DrawSagaPictureFromBuffer();
                ptr = LastInstruction + 1;
                (*ptr) += 3;
                return result;
            }
            return 1;
        } else {
            ptr = LastInstruction + 1;
            *ptr = Stage;
            ptr -= 2;
        }
    }
}

void UpdateKaylethAnimations(void) {
    // This is an attempt to make the animation jerky like the original.
    ClickShelfStage++;
    if (ClickShelfStage == 9)
        ClickShelfStage = 0;
    UpdateKaylethAnimationFrames();
    if (MyLoc == 3) {
        AnimateKaylethClickShelves(AnimationStage);
        if (ClickShelfStage < 7)
            AnimationStage++;
        if (AnimationStage > 80)
            AnimationStage = 0;
    }
}

void UpdateRebelAnimations(void)
{
    if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
        AnimateStars();
    } else if (MyLoc == 86 && ObjectLoc[42] == 86) {
        AnimateForcefield();
    } else if (MyLoc == 88 && ObjectLoc[107] == 88) {
        AnimateQueenComputer();
    } else if (MyLoc > 28 && MyLoc < 34) {
        if (ObjectLoc[92] == MyLoc) {
            AnimationStage++;
            if (AnimationStage > 5) {
                AnimationStage = 5;
                glk_request_timer_events(0);
                AnimationRunning = 0;
            }
        } else {
            AnimationStage--;
            if (AnimationStage < 0) {
                AnimationStage = 0;
                glk_request_timer_events(0);
                AnimationRunning = 0;
            } else if (AnimationStage == 4) {
                glk_request_timer_events(50);
            }
            DrawTaylor(MyLoc);
        }
        if (AnimationStage) {
            DrawSagaPictureAtPos(62 + AnimationStage, 14, 10 - AnimationStage - (AnimationStage > 2));
        }
        DrawSagaPictureFromBuffer();
    } else if (MyLoc == 50 && ObjectLoc[58] == 50) {
        draw_to_buffer = 0;
        DrawSagaPictureAtPos(138 + AnimationStage, 13, 2);
        AnimationStage = (AnimationStage == 0);
        draw_to_buffer = 1;
    } else if (MyLoc == 71 && ObjectLoc[36] == 71) {
        draw_to_buffer = 0;
        if (!AnimationStage)
            DrawSagaPictureAtPos(133, 14, 4);
        else
            DrawSagaPictureAtPos(142, 17, 6);
        AnimationStage = (AnimationStage == 0);
        draw_to_buffer = 1;
    } else {
        glk_request_timer_events(0);
        AnimationRunning = 0;
        AnimationStage = 0;
    }
}

void StartAnimations(void) {
    if (BaseGame == REBEL_PLANET) {
        if (MyLoc == 1 && ObjectLoc[UNFOLDING_SPACE] == 1) {
            int rate = STARS_ANIMATION_RATE;
            if (CurrentGame == REBEL_PLANET_64)
                rate = STARS_ANIMATION_RATE_64;
            if (AnimationRunning != rate) {
                glk_request_timer_events(rate);
                AnimationRunning = rate;
            }
        } else if (MyLoc == 86 && ObjectLoc[42] == 86) {
            if (AnimationRunning != FIELD_ANIMATION_RATE) {
                glk_request_timer_events(FIELD_ANIMATION_RATE);
                AnimationRunning = FIELD_ANIMATION_RATE;
            }
        } else if (MyLoc == 88 && ObjectLoc[107] == 88) {
            if (AnimationRunning != QUEEN_ANIMATION_RATE) {
                glk_request_timer_events(QUEEN_ANIMATION_RATE);
                AnimationRunning = QUEEN_ANIMATION_RATE;
            }
        } else if (MyLoc > 28 && MyLoc < 34) {
            glk_request_timer_events(SERPENT_ANIMATION_RATE);
            UpdateRebelAnimations();
        } else if (MyLoc == 50 && ObjectLoc[58] == 50) {
            glk_request_timer_events(ROBOT_ANIMATION_RATE);
            UpdateRebelAnimations();
        } else if (MyLoc == 71 && ObjectLoc[36] == 71) {
            glk_request_timer_events(ROBOT_ANIMATION_RATE);
            UpdateRebelAnimations();
        }
    } else if (BaseGame == KAYLETH) {
        int speed = 0;
        if (UpdateKaylethAnimationFrames()) {
            speed = 20;
            KaylethAnimationIndex = 0;
        }
        if (ObjectLoc[0] == MyLoc) { // Azap chamber
            if (CurrentGame == KAYLETH_64)
                speed = 30;
            else
                speed = 13;
        }
        switch(MyLoc) {
            case 1:
                speed = 14;
                break;
            case 2: // Conveyor belt
                speed = 10;
                break;
            case 3: // Click shelves
                if (CurrentGame == KAYLETH_64)
                    speed = 80;
                else
                    speed = 40;
                break;
            case 53: // Twin peril forest
                speed = 350;
                break;
            case 56: // Citadel of Zenron
                speed = 40;
                break;
            case 36: // Guard dome
                speed = 12;
                break;
            default:
                break;
        }
        if (AnimationRunning != speed) {
            glk_request_timer_events(speed);
            AnimationRunning = speed;
        }
    }
}

