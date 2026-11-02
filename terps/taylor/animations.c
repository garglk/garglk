//
//  animations.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-24.
//

// Frames drawn into imagebuffer[] go to the screen through
// DrawBufferedPicture(), which paints only the cells that changed. The
// star field, force field, Queen computer and click shelves instead
// paint pixels straight into the window with PutPixel() and RectFill(),
// so each of their frames repaints its whole area.

#include "graphics.h"
#include "taylordraw.h"
#include "irmak.h"
#include "utility.h"

#include "animations.h"

/* Rebel Planet rooms and the objects whose presence starts each
   animation there */
#define REBEL_ROOM_SHIP 1
#define REBEL_OBJ_UNFOLDING_SPACE 50 /* The star view */
#define REBEL_ROOM_FORCE_FIELDS 86
#define REBEL_OBJ_FORCE_FIELDS 42
#define REBEL_ROOM_QUEEN 88
#define REBEL_OBJ_QUEEN 107 /* "A holograph head" */
#define REBEL_FIRST_SEWER 29
#define REBEL_LAST_SEWER 33
#define REBEL_OBJ_SERPENT 92 /* The Sycane Serpent */
#define REBEL_ROOM_MUSEUM_PASSAGE 50
#define REBEL_OBJ_ROBOT 58 /* The killer security robot */
#define REBEL_ROOM_CAVE 71
#define REBEL_OBJ_SNAPPER 36 /* The crag snapper */

/* Kayleth rooms with a timer rate of their own, and the objects that
   set the rate where they are */
#define KAYLETH_ROOM_CONVEYOR_BELT 1 /* "lying spreadeagled on a conveyor belt" */
#define KAYLETH_ROOM_PRODUCTION_UNIT 2 /* "an android production unit" */
#define KAYLETH_ROOM_CLICK_SHELVES 3
#define KAYLETH_ROOM_VIDEODROME 32
#define KAYLETH_ROOM_GUARD_DOME 36
#define KAYLETH_ROOM_TWIN_PERIL_FOREST 52
#define KAYLETH_ROOM_TWIN_PERIL_FOREST_2 53
#define KAYLETH_ROOM_ZENRON_CITADEL 56 /* Outside it */
#define KAYLETH_OBJ_AZAP_CHAMBER 0 /* "You are in an Azap Chamber." */
#define KAYLETH_OBJ_MOKKI_RAY 30 /* "a huge two headed Mokki Ray!" */
/* The object an animation that always plays requires (13 of the 22
   animated rooms in every release). It is the Azap chamber's number, so
   UpdateKaylethAnimationFrames moves that object into the room while the
   frames run. */
#define KAYLETH_ANIM_ALWAYS KAYLETH_OBJ_AZAP_CHAMBER

/* Kayleth's flag for the destroyer droid: above 1 when it pursues us */
#define KaylethDroidPursuing (Flag[10] > 1)

/* Timer intervals (ms) for each Rebel Planet animation */
#define STARS_ANIMATION_RATE 15
#define STARS_ANIMATION_RATE_64 40
#define FIELD_ANIMATION_RATE 10
#define SERPENT_ANIMATION_RATE 100
#define SERPENT_WITHDRAW_RATE 50
#define ROBOT_ANIMATION_RATE 400
#define SNAPPER_ANIMATION_RATE 400
#define QUEEN_ANIMATION_RATE 120

/* Timer intervals (ms) for Kayleth: one for any room with frames, and the
   rooms and objects that override it */
#define KAYLETH_FRAMES_RATE 20
#define KAYLETH_AZAP_RATE_64 30
#define KAYLETH_CONVEYOR_BELT_RATE 14
#define KAYLETH_PRODUCTION_UNIT_RATE 10
#define KAYLETH_CLICK_SHELVES_RATE 40
#define KAYLETH_CLICK_SHELVES_RATE_64 80
#define KAYLETH_VIDEODROME_RATE 50
#define KAYLETH_GUARD_DOME_RATE 12
#define KAYLETH_MOKKI_RAY_RATE 150
#define KAYLETH_ZENRON_CITADEL_RATE 40

/* The click shelves scroll through 81 positions, moving on seven ticks out
   of every nine: the jerky cadence of the Spectrum original */
#define SHELF_TICKS_PER_CYCLE 9
#define SHELF_MOVING_TICKS 7
#define SHELF_LAST_POSITION 80

int AnimationRunning = 0;              /* Current timer rate, or 0 if idle */
static int KaylethAnimationIndex = 0;  /* Room index of the last Kayleth animation */
/* Each animation keeps its own frame counter, so that a restore or undo
   that jumps from one animated room straight to another cannot start the
   new animation at the old one's frame. */
static int QueenStage = 0;             /* Queen computer head blink, 0–15 (0–127 on the C64) */
static int SerpentStage = 0;           /* How far the Sycane Serpent has emerged, 0–5 */
static int TwoFrameStage = 0;          /* Security robot and crag snapper, 0 or 1 */
static int ShelfScrollStage = 0;       /* Kayleth click-shelf scroll position, 0–80 */
static int ClickShelfStage = 0;        /* Sub-counter for Kayleth click-shelf jerkiness */

/* Set the Glk timer to the given interval (ms) unless it is already
   running at that rate. */
static void SetAnimationRate(int rate)
{
    if (AnimationRunning != rate) {
        glk_request_timer_events(rate);
        AnimationRunning = rate;
    }
}

static void StopAnimations(void)
{
    glk_request_timer_events(0);
    AnimationRunning = 0;
}

/* Paint the set pixels of one pixel row of a character cell in the given
   ink. */
static void DrawPixelRow(int col, int line, int pixrow, glui32 ink)
{
    uint8_t bits = imagebuffer[col + line * IRMAK_IMGWIDTH][pixrow];
    for (int bit = 0; bit < 8; bit++) {
        if (PixelSet(bits, bit))
            PutPixel(col * 8 + bit, line * 8 + pixrow, ink);
    }
}

/* Rotate one pixel row of the cells from column first to column last one
   pixel to the left (or right) as a single long word: the pixel pushed out
   at one end comes back in at the other. */
static void RotatePixelRow(int line, int pixrow, int first, int last, int to_left)
{
    uint8_t *row = &imagebuffer[line * IRMAK_IMGWIDTH][pixrow];
    int carry = 0;
    if (to_left) {
        for (int col = last; col >= first; col--) {
            uint8_t *b = row + col * IRMAK_CELL_BYTES;
            int out = *b >> 7;
            *b = (uint8_t)((*b << 1) | carry);
            carry = out;
        }
        row[last * IRMAK_CELL_BYTES] |= (uint8_t)carry;
    } else {
        for (int col = first; col <= last; col++) {
            uint8_t *b = row + col * IRMAK_CELL_BYTES;
            int out = *b & 1;
            *b = (uint8_t)((*b >> 1) | (carry << 7));
            carry = out;
        }
        row[first * IRMAK_CELL_BYTES] |= (uint8_t)(carry << 7);
    }
}

/* Animate the star field in Rebel Planet room 1 (view through a window).
   The left half of the image scrolls left and the right half scrolls right,
   creating a parallax / hyperspace effect. Works by rotating bits in the
   ZX Spectrum-format imagebuffer (8 pixel-rows per character cell, 1 bit
   per pixel) and redrawing the affected region. The original rotates pixel
   lines 24–48, but in every release the stars are on lines 24–47 (character
   rows 3–5), so only those are touched. */
static void AnimateStars(void)
{
    /* First fill area with black, erasing all stars */
    RectFill(48, 24, 160, 24, 0);
    /* We go line by line and pixel row by pixel row */
    for (int line = 3; line < 6; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            for (int col = 6; col < 26; col++) {
                uint8_t attribute = imagebuffer[col + line * IRMAK_IMGWIDTH][IRMAK_CELL_ATTR];
                DrawPixelRow(col, line, pixrow, Remap(AttributeInk(attribute)));
            }
            /* The left half drifts one pixel to the left, the right half
               one pixel to the right, each wrapping around on itself */
            RotatePixelRow(line, pixrow, 6, 15, 1);
            RotatePixelRow(line, pixrow, 16, 25, 0);
        }
    }
}

/* Animate the force field in Rebel Planet room 86. The entire field
   region is rotated one pixel to the right each frame, with the carry
   bit wrapping from the rightmost column back to the leftmost. */
static void AnimateForcefield(void)
{
    /* First fill door area with black, erasing field */
    RectFill(104, 16, 48, 39, 0);

    /* Use the colour attribute from the top-left cell for the whole field */
    uint8_t colour = imagebuffer[2 * IRMAK_IMGWIDTH + 13][IRMAK_CELL_ATTR];
    glui32 ink = Remap(colour & INK_MASK);

    for (int line = 2; line < 7; line++) {
        for (int pixrow = 0; pixrow < 8; pixrow++) {
            for (int col = 13; col < 19; col++)
                DrawPixelRow(col, line, pixrow, ink);
            /* The force field is rotated one pixel to the right */
            RotatePixelRow(line, pixrow, 13, 18, 0);
        }
    }
}

/* Repaint a character cell with the given ink and paper, both palette
   indices. Used by AnimateQueenComputer to cycle the colours of its
   lights. */
static void PaintCell(int cell, glui32 ink, glui32 background)
{
    int startx = (cell % IRMAK_IMGWIDTH) * 8;
    int starty = (cell / IRMAK_IMGWIDTH) * 8;
    for (int pixrow = 0; pixrow < 8; pixrow++) {
        for (int pix = 0; pix < 8; pix++) {
            int set = PixelSet(imagebuffer[cell][pixrow], pix);
            PutPixel(startx + pix, starty + pixrow, set ? ink : background);
        }
    }
}

/* The cells of the Queen's computer lights, in the order the original
   colours them: the attribute offsets at 7260–726E of the Spectrum
   release, read from the end. Rows 6, 7 and 3, columns 3, 4, 10, 11,
   18–20, 27, 28. */
static const int queen_light_cells[15] = {
    6 * IRMAK_IMGWIDTH + 28, 6 * IRMAK_IMGWIDTH + 27,
    6 * IRMAK_IMGWIDTH + 11, 6 * IRMAK_IMGWIDTH + 10,
    6 * IRMAK_IMGWIDTH + 4, 6 * IRMAK_IMGWIDTH + 3,
    7 * IRMAK_IMGWIDTH + 20, 7 * IRMAK_IMGWIDTH + 19, 7 * IRMAK_IMGWIDTH + 18,
    3 * IRMAK_IMGWIDTH + 28, 3 * IRMAK_IMGWIDTH + 27,
    3 * IRMAK_IMGWIDTH + 11, 3 * IRMAK_IMGWIDTH + 10,
    3 * IRMAK_IMGWIDTH + 4, 3 * IRMAK_IMGWIDTH + 3
};

/* The paper colour of the light last coloured. Like the originals'
   counters (at 726F on the Spectrum, CE98 on the C64) it runs on from
   frame to frame and is never reset. The Spectrum release starts it at
   7 and counts up through the eight bright colours; the C64 release
   starts at 15 and counts down through all sixteen C64 colours. */
static int QueenLightPaper = 7;
static int QueenLightPaper64 = 15;

/* C64 light grey, the ink of every light in the C64 release */
#define C64_LIGHT_GREY 15

/* Whether pal[] holds the C64 colours, in the C64's own order */
static int PaletteIsC64(void)
{
    return palchosen != ZX && palchosen != ZXOPT && palchosen != VGA;
}

/* Animate the Queen's computer in Rebel Planet room 88. Each frame gives
   every light the next paper colour in turn, so the colours appear to run
   around the lights. The Arcadian head image is toggled on and off every
   8 frames on the Spectrum. The C64 release only toggles it every 1024
   calls of its animation routine, about every 8 seconds; at our rate that
   is every 64 frames. */
static void AnimateQueenComputer(void)
{
    int c64 = (CurrentGame == REBEL_PLANET_64);
    int head_period = c64 ? 64 : 8;

    for (int i = 0; i < 15; i++) {
        int cell = queen_light_cells[i];
        if (PaletteIsC64()) {
            /* The C64 writes the counter as the cell's 0-bit colour, then
               counts it down */
            PaintCell(cell, C64_LIGHT_GREY, QueenLightPaper64);
            QueenLightPaper64 = (QueenLightPaper64 - 1) & 15;
        } else {
            /* The Spectrum steps the paper first and keeps the cell's ink,
               making both bright */
            QueenLightPaper = (QueenLightPaper + 1) & 7;
            PaintCell(cell, Remap((imagebuffer[cell][IRMAK_CELL_ATTR] & INK_MASK) + 8),
                Remap(QueenLightPaper + 8));
        }
    }
    if (QueenStage == 0) {
        /* Draw the Arcadian head overlay (image block 18) */
        DrawPictureAtPos(18, 18, 1, 0);
    }

    if (QueenStage == head_period) {
        /* Erase the Arcadian head area, creating a blink effect */
        RectFill(144, 8, 24, 40, 0);
    }

    QueenStage++;
    if (QueenStage >= 2 * head_period)
        QueenStage = 0;
}

/* Animate the click shelves in Kayleth room 3. The shelves scroll
   vertically with the stage parameter controlling the offset. The left
   half (cols 12–15) scrolls downward using vertically mirrored data,
   while the right half (cols 16–19) scrolls upward. Both wrap around
   at 80 pixels to create an endless loop. */
static void AnimateKaylethClickShelves(int stage)
{
    RectFill(100, 0, 56, 81, 0);
    for (int line = 0; line < 10; line++) {
        for (int col = 12; col < 20; col++) {
            for (int i = 0; i < 8; i++) {
                /* Compute wrapped vertical position */
                int ypos = line * 8 + i + stage;
                if (ypos > 79)
                    ypos = ypos - 80;
                uint8_t bits;
                int y;
                if (col > 15) {
                    /* Right half: draw from source row, scrolling up */
                    bits = imagebuffer[col + line * IRMAK_IMGWIDTH][i];
                    y = ypos;
                } else {
                    /* Left half: draw from mirrored source row, scrolling down */
                    bits = imagebuffer[col + (9 - line) * IRMAK_IMGWIDTH][7 - i];
                    y = 79 - ypos;
                }
                /* In the colours of the row drawn to */
                uint8_t attribute = imagebuffer[col + (y / 8) * IRMAK_IMGWIDTH][IRMAK_CELL_ATTR];
                glui32 ink = Remap(AttributeInk(attribute));
                for (int j = 0; j < 8; j++) {
                    if (PixelSet(bits, j))
                        PutPixel(col * 8 + j, y, ink);
                }
            }
        }
    }
}

/* A single frame in a Kayleth animation sequence. The counter increments
   each timer tick; when it reaches counter_to_draw_at, the frame's image
   is drawn and the animation advances. */
typedef struct {
    int counter_to_draw_at; /* Number of ticks before this frame is drawn */
    int counter;            /* Current tick count (reset after drawing) */
    int image;              /* Image index to draw for this frame */
} KaylethAnimationFrame;

/* A complete animation sequence for one Kayleth room. Animations only
   play when required_object is present in the current room. After the
   last frame, the sequence loops back to the frame at loop_to. */
typedef struct {
    int loop_to;            /* Frame index to restart from after the last frame */
    int required_object;    /* Object that must be present to trigger this animation */
    int number_of_frames;
    int current_frame;
    KaylethAnimationFrame *frames;
} KaylethAnimation;

/* Per-room animation table, indexed by room number (NULL = no animation) */
static KaylethAnimation **KaylethAnimations = NULL;

/* Total number of rooms in Kayleth; NULL entries for rooms without animation */
#define NUMBER_OF_KAYLETH_ANIMATIONS 92

/* First image of the Azap chamber animation, whose timing is patched */
#define AZAP_FIRST_IMAGE 118

/* Not a real object (Kayleth has 120): a slot that the picture
   instructions test to draw the destroyer droid in the current room. */
#define KAYLETH_DROID_SLOT 122

/* Parse the Kayleth animation data from the game file image into the
   KaylethAnimations table. The data format is a sequence of room entries:
   - 0xFF bytes mark rooms with no animation (one per room).
   - Otherwise: loop_to (byte, in units of 3), required_object (byte),
     skip byte, then 3-byte frame records (delay, skip, image) terminated
     by 0xFF.
   After parsing, some timing values are patched to better match the
   original ZX Spectrum playback speed. */
void LoadKaylethAnimationData(void)
{
    KaylethAnimations = MemCalloc(sizeof(KaylethAnimation *) * NUMBER_OF_KAYLETH_ANIMATIONS);
    /* No animation data was found in this file: no room is animated */
    if (AnimationData == 0 || AnimationData >= FileImageLen)
        return;
    uint8_t *ptr = &FileImage[AnimationData];
    uint8_t *end = FileImage + FileImageLen;
    int counter = 0;
    while (counter < NUMBER_OF_KAYLETH_ANIMATIONS && ptr < end) {
        while (counter < NUMBER_OF_KAYLETH_ANIMATIONS && ptr < end && *ptr == 0xff) {
            counter++;
            ptr++;
        }
        /* An entry is a 3-byte header and at least a terminator */
        if (counter < NUMBER_OF_KAYLETH_ANIMATIONS && end - ptr > 3) {
            KaylethAnimation *anim = MemAlloc(sizeof(KaylethAnimation));
            anim->loop_to = *ptr / 3;
            ptr++;
            anim->current_frame = 0;
            anim->required_object = *ptr;
            ptr += 2; /* Skip the runtime counter byte (always zero in file) */
            /* Count frames by scanning ahead for the 0xFF terminator */
            anim->number_of_frames = 0;
            for (int i = 0; ptr + i < end && ptr[i] != 0xff; i += 3) {
                anim->number_of_frames++;
            }
            /* Truncated data: the last frame record is incomplete */
            if (ptr + anim->number_of_frames * 3 > end)
                anim->number_of_frames = (int)((end - ptr) / 3);
            anim->frames = MemAlloc(anim->number_of_frames * sizeof(KaylethAnimationFrame));
            for (int i = 0; i < anim->number_of_frames; i++) {
                anim->frames[i].counter_to_draw_at = *ptr;
                anim->frames[i].counter = 0;
                ptr += 2; /* Skip the runtime counter byte */
                anim->frames[i].image = *ptr++;
            }

            /* Patch Azap chamber timing to better match original speed */
            if (anim->number_of_frames > 4 && anim->frames[0].image == AZAP_FIRST_IMAGE) {
                anim->frames[1].counter_to_draw_at = 5;
                anim->frames[3].counter_to_draw_at = 5;
                anim->frames[4].counter_to_draw_at = 8;
            }

            if (anim->loop_to >= anim->number_of_frames)
                anim->loop_to = 0;

            KaylethAnimations[counter++] = anim;

            ptr++; /* Skip the 0xFF terminator */
        }
    }

    /* Patch assembly line (room 2) timing to match original speed */
    KaylethAnimation *belt = KaylethAnimations[KAYLETH_ROOM_PRODUCTION_UNIT];
    if (belt != NULL && belt->number_of_frames > 2) {
        belt->frames[0].counter_to_draw_at = 10;
        belt->frames[1].counter_to_draw_at = 20;
        belt->frames[2].counter_to_draw_at = 20;
    }
}

/* Advance the Kayleth animation for the current room by one tick.
   Returns 1 if the current room has an animation, 0 otherwise.
   Object 0 (KAYLETH_ANIM_ALWAYS) is placed in the room for the duration,
   so the animations that require it always play. If the destroyer droid
   is pursuing, the droid slot is placed here too, for the animations
   (rooms 4-8) that require it. */
static int UpdateKaylethAnimationFrames(void)
{
    if (KaylethAnimations == NULL || MyLoc >= NUMBER_OF_KAYLETH_ANIMATIONS
        || KaylethAnimations[MyLoc] == NULL) {
        return 0;
    }

    int always_loc = ObjectLoc[KAYLETH_ANIM_ALWAYS];
    ObjectLoc[KAYLETH_ANIM_ALWAYS] = MyLoc;
    if (KaylethDroidPursuing)
        ObjectLoc[KAYLETH_DROID_SLOT] = MyLoc;

    KaylethAnimation *anim = KaylethAnimations[MyLoc];

    /* Reset animation sequence when entering a new room */
    if (KaylethAnimationIndex != MyLoc) {
        KaylethAnimationIndex = MyLoc;
        anim->current_frame = 0;
    }

    if (anim->current_frame < anim->number_of_frames
        && ObjectLoc[anim->required_object] == MyLoc) {

        KaylethAnimationFrame *frame = &anim->frames[anim->current_frame];

        if (frame->counter >= frame->counter_to_draw_at) {
            DrawTaylor(frame->image, MyLoc);
            DrawBufferedPicture();
            anim->current_frame++;
            frame->counter = 0;
            if (anim->current_frame >= anim->number_of_frames) {
                anim->current_frame = anim->loop_to;
            }
        } else {
            frame->counter++;
        }
    }

    ObjectLoc[KAYLETH_ANIM_ALWAYS] = always_loc;
    ObjectLoc[KAYLETH_DROID_SLOT] = Destroyed();
    return 1;
}

/* Timer callback for all Kayleth animations. Drives the per-room frame
   animation and, in room 3, the click-shelf scrolling effect. The shelf
   animation only advances on 7 out of every 9 ticks, reproducing the
   jerky cadence of the original ZX Spectrum version. */
void UpdateKaylethAnimations(void)
{
    ClickShelfStage++;
    if (ClickShelfStage == SHELF_TICKS_PER_CYCLE)
        ClickShelfStage = 0;
    UpdateKaylethAnimationFrames();
    if (MyLoc == KAYLETH_ROOM_CLICK_SHELVES) {
        AnimateKaylethClickShelves(ShelfScrollStage);
        if (ClickShelfStage < SHELF_MOVING_TICKS)
            ShelfScrollStage++;
        if (ShelfScrollStage > SHELF_LAST_POSITION)
            ShelfScrollStage = 0;
    }
}

typedef enum {
    REBEL_NO_ANIMATION,
    REBEL_STARS,
    REBEL_FORCE_FIELD,
    REBEL_QUEEN_COMPUTER,
    REBEL_SERPENT,
    REBEL_ROBOT,
    REBEL_SNAPPER,
} RebelAnimationType;

static int IsInSewers(void)
{
    return MyLoc >= REBEL_FIRST_SEWER && MyLoc <= REBEL_LAST_SEWER;
}

static int ObjectInRoom(int obj, int room)
{
    return MyLoc == room && ObjectLoc[obj] == room;
}

/* Which Rebel Planet animation belongs in the current room. The serpent
   animates in the sewers whether or not it is there, as it withdraws. */
static RebelAnimationType RebelAnimationHere(void)
{
    if (ObjectInRoom(REBEL_OBJ_UNFOLDING_SPACE, REBEL_ROOM_SHIP))
        return REBEL_STARS;
    if (ObjectInRoom(REBEL_OBJ_FORCE_FIELDS, REBEL_ROOM_FORCE_FIELDS))
        return REBEL_FORCE_FIELD;
    if (ObjectInRoom(REBEL_OBJ_QUEEN, REBEL_ROOM_QUEEN))
        return REBEL_QUEEN_COMPUTER;
    if (IsInSewers())
        return REBEL_SERPENT;
    if (ObjectInRoom(REBEL_OBJ_ROBOT, REBEL_ROOM_MUSEUM_PASSAGE))
        return REBEL_ROBOT;
    if (ObjectInRoom(REBEL_OBJ_SNAPPER, REBEL_ROOM_CAVE))
        return REBEL_SNAPPER;
    return REBEL_NO_ANIMATION;
}

/* Timer callback for all Rebel Planet animations. Dispatches to the
   appropriate animation based on the current room and object presence.
   Stops the timer when the player leaves an animated room. */
void UpdateRebelAnimations(void)
{
    switch (RebelAnimationHere()) {
    case REBEL_STARS:
        AnimateStars();
        break;
    case REBEL_FORCE_FIELD:
        AnimateForcefield();
        break;
    case REBEL_QUEEN_COMPUTER:
        AnimateQueenComputer();
        break;
    case REBEL_SERPENT:
        /* Sycane Serpent in sewers. SerpentStage tracks
           how far the serpent has emerged (0–5). When the serpent object
           is present it grows; when absent it retreats, speeding up as
           it nears full retraction. */
        if (ObjectLoc[REBEL_OBJ_SERPENT] == MyLoc) {
            SerpentStage++;
            if (SerpentStage > 5) {
                SerpentStage = 5;
                StopAnimations();
            }
        } else {
            SerpentStage--;
            if (SerpentStage < 0) {
                SerpentStage = 0;
                StopAnimations();
            } else if (SerpentStage == 4) {
                SetAnimationRate(SERPENT_WITHDRAW_RATE);
            }
            DrawTaylor(MyLoc, MyLoc);
        }
        if (SerpentStage) {
            /* Draw serpent overlay; higher stages use earlier image blocks
               and move higher up the screen */
            DrawPictureAtPos(62 + SerpentStage, 14, 10 - SerpentStage - (SerpentStage > 2), 1);
        }
        DrawBufferedPicture();
        break;
    case REBEL_ROBOT:
        /* Killer security robot: alternates between two frames */
        DrawPictureAtPos(138 + TwoFrameStage, 13, 2, 0);
        TwoFrameStage = (TwoFrameStage == 0);
        break;
    case REBEL_SNAPPER:
        /* Crag snapper: alternates between two poses at different positions */
        if (!TwoFrameStage)
            DrawPictureAtPos(133, 14, 4, 0);
        else
            DrawPictureAtPos(142, 17, 6, 0);
        TwoFrameStage = (TwoFrameStage == 0);
        break;
    case REBEL_NO_ANIMATION:
        /* No animation for this room — stop the timer */
        StopAnimations();
        QueenStage = SerpentStage = TwoFrameStage = 0;
        break;
    }
}

/* Start or adjust the Glk timer for the animation in the current room.
   Called after drawing the room image. Sets the timer interval appropriate
   for the room's animation and, for some Rebel Planet rooms, kicks off
   the first animation update immediately. For Kayleth, also handles
   per-room speed overrides for specific locations. */
void StartAnimations(void)
{
    if (BaseGame == REBEL_PLANET) {
        /* The serpent's timer stops once it is fully out, so nothing else
           would reset it: without this it would be seen withdrawing on the
           next visit to the sewers. */
        if (!IsInSewers())
            SerpentStage = 0;
        switch (RebelAnimationHere()) {
        case REBEL_STARS:
            SetAnimationRate(CurrentGame == REBEL_PLANET_64 ? STARS_ANIMATION_RATE_64 : STARS_ANIMATION_RATE);
            break;
        case REBEL_FORCE_FIELD:
            SetAnimationRate(FIELD_ANIMATION_RATE);
            break;
        case REBEL_QUEEN_COMPUTER:
            SetAnimationRate(QUEEN_ANIMATION_RATE);
            break;
        case REBEL_SERPENT:
            SetAnimationRate(SERPENT_ANIMATION_RATE);
            UpdateRebelAnimations();
            break;
        case REBEL_ROBOT:
            SetAnimationRate(ROBOT_ANIMATION_RATE);
            break;
        case REBEL_SNAPPER:
            SetAnimationRate(SNAPPER_ANIMATION_RATE);
            break;
        case REBEL_NO_ANIMATION:
            break;
        }
    } else if (BaseGame == KAYLETH) {
        int speed = 0;
        if (UpdateKaylethAnimationFrames()) {
            speed = KAYLETH_FRAMES_RATE;
            KaylethAnimationIndex = 0;
        }
        if (ObjectLoc[KAYLETH_OBJ_AZAP_CHAMBER] == MyLoc && CurrentGame == KAYLETH_64) {
            speed = KAYLETH_AZAP_RATE_64;
        }
        switch (MyLoc) {
        case KAYLETH_ROOM_CONVEYOR_BELT:
            speed = KAYLETH_CONVEYOR_BELT_RATE;
            break;
        case KAYLETH_ROOM_PRODUCTION_UNIT:
            speed = KAYLETH_PRODUCTION_UNIT_RATE;
            break;
        case KAYLETH_ROOM_CLICK_SHELVES:
            if (CurrentGame == KAYLETH_64)
                speed = KAYLETH_CLICK_SHELVES_RATE_64;
            else
                speed = KAYLETH_CLICK_SHELVES_RATE;
            break;
        case KAYLETH_ROOM_VIDEODROME:
            speed = KAYLETH_VIDEODROME_RATE;
            break;
        case KAYLETH_ROOM_GUARD_DOME:
            speed = KAYLETH_GUARD_DOME_RATE;
            break;
        case KAYLETH_ROOM_TWIN_PERIL_FOREST:
        case KAYLETH_ROOM_TWIN_PERIL_FOREST_2:
            if (ObjectLoc[KAYLETH_OBJ_MOKKI_RAY] == MyLoc) {
                speed = KAYLETH_MOKKI_RAY_RATE;
                UpdateKaylethAnimationFrames();
            }
            break;
        case KAYLETH_ROOM_ZENRON_CITADEL:
            speed = KAYLETH_ZENRON_CITADEL_RATE;
            break;
        default:
            break;
        }
        SetAnimationRate(speed);
    }
}
