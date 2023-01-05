//
//  animations.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-03-24.
//

#include <stdlib.h>
#include <string.h>
#include "definitions.h"
#include "glk.h"
#include "common.h"
#include "graphics.h"
#include "animations.h"

#define ANIMATION_RATE 200
#define CANNON_RATE 300
#define MAX_ANIM_FRAMES 32

int AnimationRunning = 0;
int AnimationBackground = 0;
int LastAnimationBackground = 0;

int AnimationRoom = -1;
glui32 AnimTimerRate = 0;

static int AnimationStage = 0;
static int StopNext = 0;
static int ImgTail = 0;
static int FramesTail = 0;


static char *AnimationFilenames[MAX_ANIM_FRAMES];
static int AnimationFrames[MAX_ANIM_FRAMES];

/*
 The "fire cannon at blob" animation in Fantastic Four is actually two
 animations sequences, one after another, which conflicts with our
 animation system, so we use a hack to stitch them together, basically
 adding the new frames to the end of the buffer rather than clearing it
 and starting over.
 */

static int PostCannonAnimationSeam = 0;
static int CannonAnimationPause = 0;

/*
 The Atari ST version of Spider-Man only uses a single picture with color cycling for the "shooting web" animation, so we use a hack to ignore attempts to draw the other frames and instead wait exactly one color cycle.
 */
int STWebAnimation = 0;
int STWebAnimationFinished = 1;

void AddImageToBuffer(char *basename) {
    if (ImgTail >= MAX_ANIM_FRAMES - 1)
        return;
    size_t len = strlen(basename) + 1;
    if (AnimationFilenames[ImgTail] != NULL)
        free(AnimationFilenames[ImgTail]);
    AnimationFilenames[ImgTail] = MemAlloc(len);
    debug_print("AddImageToBuffer: Setting AnimationFilenames[%d] to %s\n", ImgTail, basename);
    memcpy(AnimationFilenames[ImgTail++], basename, len);
    AnimationFilenames[ImgTail] = NULL;
}

void AddFrameToBuffer(int frameidx) {
    if (FramesTail >= MAX_ANIM_FRAMES - 1)
        return;
    debug_print("AddFrameToBuffer: Setting AnimationFrames[%d] to %d\n", FramesTail, frameidx);
    AnimationFrames[FramesTail++] = frameidx;
    AnimationFrames[FramesTail] = -1;
}

void AddRoomImage(int image) {
    char *shortname = ShortNameFromType('R', image);
    AddImageToBuffer(shortname);
    free(shortname);
    STWebAnimation = 0;
}

void AddItemImage(int image) {
    char *shortname = ShortNameFromType('B', image);
    AddImageToBuffer(shortname);
    free(shortname);
    STWebAnimation = 0;
}

void AddSpecialImage(int image) {

    if (CurrentSys == SYS_ST && CurrentGame == SPIDERMAN) {
        if (image == 14) {
            image = 18;
            STWebAnimation = 1;
            STWebAnimationFinished = 1;
        } else if (image > 14 && image < 19) {
            return;
        } else {
            STWebAnimation = 0;
        }
    }

    char *shortname = ShortNameFromType('S', image);
    AddImageToBuffer(shortname);
    free(shortname);
}

void SetAnimationTimer(glui32 milliseconds) {
    debug_print("SetAnimationTimer %d\n", milliseconds);
    AnimTimerRate = milliseconds;
    if (milliseconds == 0 && ColorCyclingRunning)
        return;
    if (TimerRate == 0 || TimerRate > milliseconds) {
        debug_print("Actually setting animation timer\n");
        SetTimer(milliseconds);
    }
}

void Animate(int frame) {

    int cannonanimation = (CurrentGame == FANTASTIC4 && MyLoc == 5);

    if (PostCannonAnimationSeam) {
        AddFrameToBuffer(10 + frame);
        return;
    }

    if (STWebAnimation && frame > 0)
        return;

    AddFrameToBuffer(frame);
    if (!AnimationRunning) {

        if (!AnimationBackground)
            LastAnimationBackground = 0;

        if (cannonanimation)
            SetAnimationTimer(CANNON_RATE);
        else
            SetAnimationTimer(ANIMATION_RATE);

        AnimationRunning = 1;
        AnimationStage = 0;
        AnimationRoom = MyLoc;
    }

    if (cannonanimation && frame == 9)
        PostCannonAnimationSeam = 1;
}

void ClearAnimationBuffer(void) {

    if (PostCannonAnimationSeam)
        return;

    for (int i = 0; i < MAX_ANIM_FRAMES && AnimationFilenames[i] != NULL; i++) {
        if (AnimationFilenames[i] != NULL)
            free(AnimationFilenames[i]);
        AnimationFilenames[i] = NULL;
    }
    ImgTail = 0;
    FramesTail = 0;
}

void ClearFrames(void) {
    for (int i = 0; i < FramesTail; i++) {
        AnimationFrames[i] = -1;
    }
    FramesTail = 0;
}

void InitAnimationBuffer(void) {
    for (int i = 0; i < MAX_ANIM_FRAMES; i++) {
        AnimationFilenames[i] = NULL;
        AnimationFrames[i] = -1;
    }
}

void StopAnimation(void) {
    SetAnimationTimer(0);
    debug_print("StopAnimation: stopped timer\n");
    AnimationStage = 0;
    AnimationRunning = 0;
    PostCannonAnimationSeam = 0;
    AnimationRoom = -1;
    StopNext = 0;
}

void DrawApple2ImageFromVideoMem(void);
void DrawBlack(void);

void UpdateAnimation(void) // Draw animation frame
{
    if (CannonAnimationPause) {
        SetAnimationTimer(CANNON_RATE);
        CannonAnimationPause = 0;
    }

    if (STWebAnimation) {
        if (!STWebAnimationFinished) {
            if (AnimTimerRate != 25)
                SetAnimationTimer(25);
            return;
        } else {
            STWebAnimationFinished = 0;
        }
    }

    if (StopNext) {
        StopNext = 0;
        if (AnimationFrames[AnimationStage] == -1)
            ClearFrames();
        StopAnimation();

        if (CurrentGame == BANZAI && MyLoc == 29 && IsSet(6)) {
            DrawBlack();
        } else if (!showing_inventory) {
            glk_window_clear(Graphics);
            DrawCurrentRoom();
            if (CurrentGame == SPIDERMAN && MyLoc == 5) {
                DrawItemImage(23);
                if (CurrentSys == SYS_APPLE2)
                    DrawApple2ImageFromVideoMem();
            }
        }

        return;
    }

    if (AnimationStage >= MAX_ANIM_FRAMES - 1 || AnimationRoom != MyLoc || !IsSet(GRAPHICSBIT)) {
        StopNext = 1;
        return;
    }

    if (AnimationBackground) {
        char buf[5];
        snprintf(buf, 5, "S0%02d", AnimationBackground);
        LastAnimationBackground = AnimationBackground;
        AnimationBackground = 0;
        DrawImageWithName(buf);
        return;
    }

    if (AnimationFrames[AnimationStage] != -1) {
        debug_print("UpdateAnimation: Drawing AnimationFrames[%d] (%d)\n", AnimationStage, AnimationFrames[AnimationStage]);
        if (!DrawImageWithName(AnimationFilenames[AnimationFrames[AnimationStage++]]))
            StopNext = 1;
        else if (CurrentSys == SYS_APPLE2)
            DrawApple2ImageFromVideoMem();

        if (PostCannonAnimationSeam && AnimationStage == 10) {
            CannonAnimationPause = 1;
            SetAnimationTimer(2000);
        }

        if (STWebAnimation) {
            StopNext = 1;
        }

    } else {
        debug_print("StopNext\n");
        SetAnimationTimer(2000);
        StopNext = 1;
    }
}
