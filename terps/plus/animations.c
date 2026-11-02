//
//  animations.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-03-24.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "definitions.h"
#include "glk.h"
#include "graphics.h"
#include "apple2draw.h"

#include "animations.h"
#include "stdraw.h"

/* Default timer intervals (ms) */
#define ANIMATION_RATE 200
#define CANNON_RATE 300
#define MAX_ANIM_FRAMES 32

int AnimationRunning = 0;          /* Non-zero while an animation sequence is playing */
int AnimationBackground = 0;       /* Special-image index to draw as background before next frame */
int LastAnimationBackground = 0;   /* Previous AnimationBackground, for redraw after animation ends */

int AnimationRoom = -1;            /* Room where the current animation started */
glui32 AnimTimerRate = 0;          /* Current animation timer interval (ms) */

static int AnimationStage = 0;     /* Index into AnimationFrames for the next frame to draw */
static int StopNext = 0;           /* Flag: stop and clean up on the next timer tick */
static int ImgTail = 0;            /* Next free slot in AnimationFilenames */
static int FramesTail = 0;         /* Next free slot in AnimationFrames */

/* Image filename pool: each entry is a short filename like "S014" or "R003".
   AnimationFrames indices refer into this array. */
static char *AnimationFilenames[MAX_ANIM_FRAMES];
/* Frame sequence: each entry is an index into AnimationFilenames, or -1
   to mark the end of the sequence. */
static int AnimationFrames[MAX_ANIM_FRAMES];

/*
 The "fire cannon at Blob" animation in Fantastic Four is actually two
 animations sequences, one after another, which conflicts with our
 animation system, so we use a hack to stitch them together, basically
 adding the new frames to the end of the buffer rather than clearing it
 and starting over.
 */

static int PostCannonAnimationSeam = 0;  /* Set after frame 9 of the cannon animation */
static int CannonAnimationPause = 0;    /* Triggers a 2-second pause between the two halves */

/* The Atari ST Spider-Man "shooting web" animation uses a single picture
   with colour cycling instead of the multiple frames of other version.
   STWebAnimation suppresses extra frame draws; STWebAnimationFinished
   gates the colour-cycle wait. */
int STWebAnimation = 0;
int STWebAnimationFinished = 1;

/* Append a filename to the image pool. The filename is copied so the
   caller can free the original. A NULL sentinel is always written after
   the last entry. */
void AddImageToBuffer(char *basename)
{
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

/* Append a frame to the playback sequence. frameidx is an index into
   AnimationFilenames. A -1 sentinel follows the last entry. */
void AddFrameToBuffer(int frameidx)
{
    if (FramesTail >= MAX_ANIM_FRAMES - 1)
        return;
    debug_print("AddFrameToBuffer: Setting AnimationFrames[%d] to %d\n", FramesTail, frameidx);
    AnimationFrames[FramesTail++] = frameidx;
    AnimationFrames[FramesTail] = -1;
}

/* Build a short filename from an image type prefix ('R' = room,
   'B' = item/object, 'S' = special) and index, then add it to the
   animation image pool. */
static void AddImageWithType(char type, int image)
{
    char *shortname = ShortNameFromType(type, image);
    AddImageToBuffer(shortname);
    free(shortname);
}

void AddRoomImage(int image)
{
    AddImageWithType('R', image);
    STWebAnimation = 0;
}

void AddItemImage(int image)
{
    AddImageWithType('B', image);
    STWebAnimation = 0;
}

/* Add a special image, with Atari ST Spider-Man web-animation workaround:
   replace images 14–18 with a single image 18 that uses colour cycling. */
void AddSpecialImage(int image)
{

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

    AddImageWithType('S', image);
}

/* Request an animation timer rate. Avoids overriding a faster existing
   timer (e.g. colour cycling), and never stops the timer while colour
   cycling is active. */
void SetAnimationTimer(glui32 milliseconds)
{
    debug_print("SetAnimationTimer %d\n", milliseconds);
    AnimTimerRate = milliseconds;
    if (milliseconds == 0 && ColorCyclingRunning)
        return;
    if (TimerRate == 0 || TimerRate > milliseconds) {
        debug_print("Actually setting animation timer\n");
        SetTimer(milliseconds);
    }
}

/* Queue a frame for playback. Starts the animation timer on the first
   call. For the Fantastic Four cannon animation, frames 0–9 play first;
   when frame 9 arrives PostCannonAnimationSeam is set so subsequent
   frames are appended with an offset of 10, stitching the two halves
   together into a single sequence. */
void Animate(int frame)
{
    if (Images[0].Filename == NULL)
        return;

    int cannonanimation = (CurrentGame == FANTASTIC4 && MyLoc == 5);

    if (PostCannonAnimationSeam) {
        /* Second half of cannon animation: offset frame indices */
        AddFrameToBuffer(10 + frame);
        return;
    }

    /* ST Spider-Man web animation: only the first frame matters */
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

/* Free all filenames in the image pool and reset both buffers.
   No-op during the cannon animation seam to preserve the first half's
   images while the second half is being built. */
void ClearAnimationBuffer(void)
{

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

/* Reset the frame sequence without touching the image pool. */
void ClearFrames(void)
{
    for (int i = 0; i < FramesTail; i++) {
        AnimationFrames[i] = -1;
    }
    FramesTail = 0;
}

/* One-time initialization of both buffers at startup. */
void InitAnimationBuffer(void)
{
    for (int i = 0; i < MAX_ANIM_FRAMES; i++) {
        AnimationFilenames[i] = NULL;
        AnimationFrames[i] = -1;
    }
}

/* Stop the animation immediately and reset all state. */
void StopAnimation(void)
{
    SetAnimationTimer(0);
    debug_print("StopAnimation: stopped timer\n");
    AnimationStage = 0;
    AnimationRunning = 0;
    PostCannonAnimationSeam = 0;
    AnimationRoom = -1;
    StopNext = 0;
}

/* Timer callback: advance the animation by one frame.
   The two-phase approach (set StopNext, then clean up on the next tick)
   ensures the final frame is visible for one timer interval before the
   room image is restored. Special cases:
   - Fantastic 4 Cannon animation: inserts a 2-second pause between the two halves.
   - ST web animation: waits for one colour-cycle completion, then stops.
   - Buckaroo Banzai room 29: draws a black screen instead of restoring the room.
   - Spider-Man room 5: redraws the web item overlay after the animation. */
void UpdateAnimation(void)
{
    /* Resume normal cannon rate after the mid-animation pause */
    if (CannonAnimationPause) {
        SetAnimationTimer(CANNON_RATE);
        CannonAnimationPause = 0;
    }

    /* ST web animation: wait for the colour cycle to finish */
    if (STWebAnimation) {
        if (!STWebAnimationFinished) {
            if (AnimTimerRate != 25)
                SetAnimationTimer(25);
            return;
        } else {
            STWebAnimationFinished = 0;
        }
    }

    /* Second tick after the sequence ended: stop and restore the room */
    if (StopNext) {
        StopNext = 0;
        if (AnimationFrames[AnimationStage] == -1)
            ClearFrames();
        StopAnimation();

        if (CurrentGame == BANZAI && MyLoc == 29 && IsSet(BANZAI_BLACKOUT_BIT)) {
            DrawBlack();
        } else if (!showing_inventory) {
            ClearGraphicsWindow();
            DrawCurrentRoom();
            if (CurrentGame == SPIDERMAN && MyLoc == 5) {
                DrawItemImage(23);
                ShowApple2Screen();
            }
        }

        return;
    }

    /* Safety: stop if we've overrun, left the room, or graphics are off */
    if (AnimationStage >= MAX_ANIM_FRAMES - 1 || AnimationRoom != MyLoc || !IsSet(GRAPHICSBIT)) {
        StopNext = 1;
        return;
    }

    /* Draw a background image before the next animation frame if one
       was queued (e.g. a room transition overlay) */
    if (AnimationBackground) {
        LastAnimationBackground = AnimationBackground;
        AnimationBackground = 0;
        DrawImageWithTypeAndNumber('S', LastAnimationBackground);
        return;
    }

    if (AnimationFrames[AnimationStage] != -1) {
        debug_print("UpdateAnimation: Drawing AnimationFrames[%d] (%d)\n", AnimationStage, AnimationFrames[AnimationStage]);
        if (!DrawImageWithName(AnimationFilenames[AnimationFrames[AnimationStage++]]))
            StopNext = 1;
        else
            ShowApple2Screen();

        /* Cannon animation: pause for 2 seconds at the seam between halves */
        if (PostCannonAnimationSeam && AnimationStage == 10) {
            CannonAnimationPause = 1;
            SetAnimationTimer(2000);
        }

        if (STWebAnimation) {
            StopNext = 1;
        }

    } else {
        /* No more frames — hold the last frame for 2 seconds then stop */
        debug_print("StopNext\n");
        SetAnimationTimer(2000);
        StopNext = 1;
    }
}
