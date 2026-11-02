//
//  graphics.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "animations.h"
#include "apple2draw.h"
#include "sagadraw_host.h"
#include "common.h"
#include "graphics.h"
#include "loaddatabase.h"
#include "read_le16.h"
#include "c64a8draw.h"
#include "pcdraw.h"
#include "sagadraw_glue.h"
#include "stdraw.h"

ImgType LastImgType;
int LastImgIndex;

int upside_down = 0;

int pixel_size;
int x_offset, y_offset;

glui32 pal[16];

imgrec *Images;

void SetSystem(SystemType sys)
{
    CurrentSys = sys;
    switch (sys) {
    case SYS_C64:
    case SYS_ATARI8:
        ImageWidth = 280;
        ImageHeight = 160;
        break;
    case SYS_ST:
        ImageWidth = 319;
        ImageHeight = 162;
        break;
    case SYS_APPLE2:
        ImageWidth = 560;
        ImageHeight = 320;
        break;
    default:
        /* MS-DOS pictures carry their own size */
        break;
    }
}

void AdoptDataFile(uint8_t **sf, size_t *extent, uint8_t *data, size_t len, SystemType sys)
{
    free(*sf);
    *sf = data;
    *extent = len;
    SetSystem(sys);
}

imgrec *NewImageList(size_t count)
{
    imgrec *list = MemAlloc((count + 1) * sizeof(imgrec));
    memset(list, 0, (count + 1) * sizeof(imgrec));
    return list;
}

int issagaimg(const char *name)
{
    if (name == NULL)
        return 0;
    size_t len = strlen(name);
    if (len < 4)
        return 0;
    char c = name[0];
    if (c == 'R' || c == 'B' || c == 'S') {
        for (int i = 1; i < 4; i++)
            if (!isdigit((unsigned char)name[i]))
                return 0;
        return 1;
    }
    return 0;
}

/* The graphics window that is known to be blank: cleared and not drawn
   to since. NULL when it is not known. */
static winid_t blank_window = NULL;

/* What DrawCurrentRoom() last drew, and where. While this is valid the
   graphics window still shows exactly that, so drawing the same thing
   again can be skipped. Games ask for a redraw far more often than the
   picture changes. */
#define MAX_SCENE_OBJECTS 32

typedef struct {
    winid_t window;
    glui32 win_width, win_height;
    int pixel_size, x_offset, y_offset;
    int room_image;
    int upside_down;
    int num_objects;
    int objects[MAX_SCENE_OBJECTS];
} Scene;

static Scene shown_scene;
static int shown_scene_valid = 0;
/* The values the draw of shown_scene left in these globals */
static ImgType shown_last_type;
static int shown_last_index;
static int shown_image_width, shown_image_height;

/* Tell that something was drawn to the graphics window by other means
   than the functions in this file. */
void GraphicsWindowChanged(void)
{
    blank_window = NULL;
    shown_scene_valid = 0;
}

/* Tell that pixels of the picture on screen were recoloured, which
   leaves the window showing the same scene, but not blank. */
void GraphicsWindowRecoloured(void)
{
    blank_window = NULL;
}

/* Clear the graphics window, unless it is known to be blank already. */
void ClearGraphicsWindow(void)
{
    shown_scene_valid = 0;
    if (Graphics == NULL || blank_window == Graphics)
        return;
    glk_window_clear(Graphics);
    blank_window = Graphics;
}

void DrawBlack(void)
{
    GraphicsWindowChanged();
    glk_window_fill_rect(Graphics, 0, x_offset, y_offset, (glui32)(ImageWidth * pixel_size),
        (glui32)(ImageHeight * pixel_size));
    LastImgType = NO_IMG;
}

/* Write the short image name for an image type ('R' = room,
   'B' = item/object, 'S' = special) and number, e.g. "R003", to buf,
   which must hold IMAGE_NAME_SIZE bytes. Returns 0 on failure. */
static int ImageNameFromType(char *buf, char type, int index)
{
    return snprintf(buf, IMAGE_NAME_SIZE, "%c0%02d", type, index) >= 0;
}

char *ShortNameFromType(char type, int index)
{
    char buf[IMAGE_NAME_SIZE];
    if (!ImageNameFromType(buf, type, index))
        return NULL;
    size_t len = strlen(buf) + 1;
    char *result = MemAlloc(len);
    memcpy(result, buf, len);
    return result;
}

/* The kind of picture an image name is, from its first letter, e.g.
   IMG_ROOM for "R003". NO_IMG if it is none of the three. */
ImgType ImageTypeFromName(const char *name)
{
    switch (name[0]) {
    case 'B':
        return IMG_OBJECT;
    case 'R':
        return IMG_ROOM;
    case 'S':
        return IMG_SPECIAL;
    default:
        return NO_IMG;
    }
}

/* The image number in a short image name, e.g. 3 for "R003". */
int ImageNumberFromName(const char *name)
{
    return (name[2] - '0') * 10 + name[3] - '0';
}

/* Pictures are drawn a pixel at a time, mostly left to right in runs
   of one colour. Each fill_rect is a round trip to the window server,
   so a run is collected here and sent as a single rectangle. */
static struct {
    glui32 color;
    glsi32 x, y;
    glui32 width, height;
} pending_run;

/* Send the run collected so far to the graphics window. Must be called
   before anything else draws to or clears the window, and when a
   picture is finished. */
void FlushPixels(void)
{
    if (pending_run.width == 0)
        return;
    if (Graphics != NULL) {
        blank_window = NULL;
        glk_window_fill_rect(Graphics, pending_run.color, pending_run.x,
            pending_run.y, pending_run.width, pending_run.height);
    }
    pending_run.width = 0;
}

static void FillRun(glui32 color, glsi32 x, glsi32 y, glui32 width, glui32 height)
{
    if (width == 0 || height == 0)
        return;

    glsi32 right = x + (glsi32)width;
    glsi32 pending_right = pending_run.x + (glsi32)pending_run.width;

    /* A rectangle of the same colour on the same row that touches or
       overlaps the collected run covers the same pixels when joined to
       it. Anything else is drawn after the run, keeping the order. */
    if (pending_run.width != 0 && color == pending_run.color
        && y == pending_run.y && height == pending_run.height
        && x <= pending_right && right >= pending_run.x) {
        if (x < pending_run.x)
            pending_run.x = x;
        if (right > pending_right)
            pending_right = right;
        pending_run.width = (glui32)(pending_right - pending_run.x);
        return;
    }

    FlushPixels();
    pending_run.color = color;
    pending_run.x = x;
    pending_run.y = y;
    pending_run.width = width;
    pending_run.height = height;
}

/* Used by IBM PC graphics in "striped" mode where every other pixel is skipped
   to make halftones */
void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    PutPixelWithWidth(xpos, ypos, color, 1);
}

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color) {
    PutPixelWithWidth(xpos, ypos, color, 2);
}


void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixelwidth)
{
    xpos = xpos * pixel_size;

    if (upside_down) {
        xpos = ImageWidth * pixel_size - xpos - 1;
    }
    xpos += x_offset;

    /* Some pictures draw black columns outside their frame */
    if (xpos < x_offset || xpos >= x_offset + ImageWidth * pixel_size) {
        return;
    }

    ypos = ypos * pixel_size;
    if (upside_down) {
        ypos = (ImageHeight - 1) * pixel_size - ypos;
        if (CurrentSys == SYS_ST)
            ypos -= 2 * pixel_size;
        else if (CurrentSys == SYS_ATARI8)
            ypos -= ImageHeight & 1;
    }
    ypos += y_offset;

    FillRun(pal[color], xpos, ypos, pixel_size * pixelwidth, pixel_size);
}

void SetColor(int32_t index, glui32 color)
{
    pal[index] = color;
}

/* In Spider-Man, pictures are drawn upside down while the player is in
   the same room as item 0 (walking on the ceiling). */
static int IsUpsideDown(void)
{
    return (CurrentGame == SPIDERMAN && Items[0].Location == MyLoc);
}

/* Apple II pictures are drawn into a copy of the video memory. This
   shows the result in the graphics window. Does nothing on other
   systems. */
void ShowApple2Screen(void)
{
    if (CurrentSys == SYS_APPLE2) {
        GraphicsWindowChanged();
        DrawApple2ImageFromVideoMemWithFlip(upside_down);
    }
}

int DrawImageWithName(const char *filename)
{
    debug_print("DrawImageWithName %s\n", filename);

    shown_scene_valid = 0;

    int i;
    for (i = 0; Images[i].Filename != NULL; i++)
        if (strcmp(filename, Images[i].Filename) == 0)
            break;

    if (Images[i].Filename == NULL) {
        if (CurrentSys == SYS_MSDOS) {
            /* The image was not on disk when the list was built. Look
               for it again, growing the list (which may be full) to
               make room for the new entry and the terminator. */
            Images = MemRealloc(Images, (i + 2) * sizeof(imgrec));
            memset(&Images[i], 0, 2 * sizeof(imgrec));
            if (!FindAndAddImageFile(filename, &Images[i])) {
                Images[i].Filename = NULL;
                return 0;
            }
        } else {
            return 0;
        }
    }

    if (!IsSet(GRAPHICSBIT))
        return 0;

    ImgType type = ImageTypeFromName(Images[i].Filename);
    if (type == NO_IMG)
        debug_print("DrawImageWithName: Unknown image type!\n");
    else
        LastImgType = type;

    LastImgIndex = ImageNumberFromName(Images[i].Filename);

    int result;
    switch (CurrentSys) {
    case SYS_C64:
    case SYS_ATARI8:
        result = DrawC64A8ImageFromData(Images[i].Data, Images[i].Size, C64A8AdjustPlus, CurrentSys == SYS_C64 ? C64A8_C64 : C64A8_ATARI8);
        break;
    case SYS_ST:
        result = DrawSTImageFromData(Images[i].Data, Images[i].Size);
        break;
    case SYS_APPLE2:
        /* After the size word. Size is the whole record: the word of an
           object picture does not count itself (AddImage). */
        result = DrawApple2ImageFromData(Images[i].Data + 2, Images[i].Size - 2, APPLE2_STANDARD, Apple2AdjustPlus);
        break;
    default:
        result = DrawDOSImageFromData(Images[i].Data, Images[i].Size);
        break;
    }

    FlushPixels();
    return result;
}

int DrawImageWithTypeAndNumber(char type, int index)
{
    char buf[IMAGE_NAME_SIZE];
    if (!ImageNameFromType(buf, type, index))
        return 0;
    return DrawImageWithName(buf);
}

void DrawItemImage(int item)
{
    LastImgType = IMG_OBJECT;
    LastImgIndex = item;
    char buf[IMAGE_NAME_SIZE];

    if (!ImageNameFromType(buf, 'B', item))
        return;

    upside_down = IsUpsideDown();

    DrawImageWithName(buf);
}

int DrawCloseup(int img)
{
    LastImgType = IMG_SPECIAL;
    LastImgIndex = img;
    char buf[IMAGE_NAME_SIZE];

    if (!ImageNameFromType(buf, 'S', img))
        return 0;

    upside_down = 0;

    if (CurrentSys == SYS_APPLE2)
        ClearApple2ScreenMem();

    int result = DrawImageWithName(buf);

    if (result && CurrentSys == SYS_APPLE2) {
        ClearGraphicsWindow();
        ShowApple2Screen();
    }

    return result;
}

static int STRoomImageClearsWindow(const char *name)
{
    if (!IsSet(GRAPHICSBIT))
        return 0;
    for (int i = 0; Images[i].Filename != NULL; i++)
        if (strcmp(name, Images[i].Filename) == 0)
            return STImageClearsWindow(Images[i].Data, Images[i].Size);
    return 0;
}

int DrawRoomImage(int roomimg)
{
    LastImgType = IMG_ROOM;
    LastImgIndex = roomimg;

    char buf[IMAGE_NAME_SIZE];
    if (!ImageNameFromType(buf, 'R', roomimg))
        return 0;

    ClearGraphicsWindow();

    /* On the Atari ST, small room pictures are drawn on top of the
       backdrop S999. One that clears the window would only erase it. */
    if (CurrentSys == SYS_ST && CurrentGame != CLAYMORGUE) {
        if (!STRoomImageClearsWindow(buf))
            DrawImageWithName("S999");
    } else if (CurrentSys == SYS_APPLE2)
        ClearApple2ScreenMem();

    upside_down = IsUpsideDown();

    return DrawImageWithName(buf);
}

/* Redraw an item or closeup image that was showing on top of the room
   picture, given the type and index it was drawn with. Room images are
   left alone. */
void DrawOverlayImage(ImgType type, int index)
{
    if (type == IMG_SPECIAL)
        DrawCloseup(index);
    else if (type == IMG_OBJECT)
        DrawItemImage(index);
}

/* Draw the item images that ObjectImages assigns to room, for every
   such item at location. */
void DrawObjectImages(int room, int location)
{
    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].Room == room && Items[ObjectImages[ct].Object].Location == location) {
            DrawItemImage(ObjectImages[ct].Image);
        }
}

/* Describe the picture of the current room: its image, the item images
   on top of it, and where in the graphics window they go. Returns 0 if
   there are more item images than a Scene has room for. */
static int DescribeScene(Scene *scene)
{
    memset(scene, 0, sizeof *scene);
    scene->window = Graphics;
    glk_window_get_size(Graphics, &scene->win_width, &scene->win_height);
    scene->pixel_size = pixel_size;
    scene->x_offset = x_offset;
    scene->y_offset = y_offset;
    scene->room_image = PlayerRoom()->Exits[EXIT_IMAGE];
    scene->upside_down = IsUpsideDown();
    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].Room == MyLoc && Items[ObjectImages[ct].Object].Location == MyLoc) {
            if (scene->num_objects == MAX_SCENE_OBJECTS)
                return 0;
            scene->objects[scene->num_objects++] = ObjectImages[ct].Image;
        }
    return 1;
}

static int SameScene(const Scene *a, const Scene *b)
{
    if (a->window != b->window || a->win_width != b->win_width
        || a->win_height != b->win_height || a->pixel_size != b->pixel_size
        || a->x_offset != b->x_offset || a->y_offset != b->y_offset
        || a->room_image != b->room_image
        || a->upside_down != b->upside_down || a->num_objects != b->num_objects)
        return 0;
    for (int i = 0; i < a->num_objects; i++)
        if (a->objects[i] != b->objects[i])
            return 0;
    return 1;
}

void DrawCurrentRoom(void)
{
    OpenGraphicsWindow();

    showing_inventory = (CurrentGame == CLAYMORGUE && MyLoc == 33);

    if (!IsSet(GRAPHICSBIT)) {
        CloseGraphicsWindow();
        return;
    }

    int dark = IsDark();

    if (PlayerRoom()->Exits[EXIT_IMAGE] == NO_ROOM_IMAGE || Images[0].Filename == NULL) {
        CloseGraphicsWindow();
        return;
    }

    if (dark && Graphics != NULL) {
        if (CurrentGame != CLAYMORGUE) {
            if (CurrentSys == SYS_ST && !AnimationRunning)
                DrawImageWithName("S999");
            else
                DrawBlack();
        } else {
            DrawImageWithName("R000");
        }
        return;
    }

    if (Graphics == NULL || AnimationRoom == MyLoc)
        return;

    Scene scene;
    int describable = DescribeScene(&scene);
    if (describable && shown_scene_valid && SameScene(&scene, &shown_scene)
        && ImageWidth == shown_image_width && ImageHeight == shown_image_height) {
        debug_print("DrawCurrentRoom: picture unchanged, not redrawn\n");
        LastImgType = shown_last_type;
        LastImgIndex = shown_last_index;
        return;
    }

    if (!DrawRoomImage(PlayerRoom()->Exits[EXIT_IMAGE])) {
        CloseGraphicsWindow();
        return;
    }

    DrawObjectImages(MyLoc, MyLoc);

    ShowApple2Screen();

    /* Drawing may have resized the window, so describe it again */
    if (describable && Graphics != NULL && DescribeScene(&scene)) {
        shown_scene = scene;
        shown_last_type = LastImgType;
        shown_last_index = LastImgIndex;
        shown_image_width = ImageWidth;
        shown_image_height = ImageHeight;
        shown_scene_valid = 1;
    }
}
