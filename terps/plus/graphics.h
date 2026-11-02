//
//  graphics.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef graphics_h
#define graphics_h

#include "glk.h"

#include "definitions.h"

/* Bytes needed for a short image name such as "R003", with terminator */
#define IMAGE_NAME_SIZE 5

extern winid_t Graphics;

extern int pixel_size;
extern int ImageWidth, ImageHeight;
extern int x_offset, y_offset;
extern int upside_down;

/* Set CurrentSys and the native picture size of that system */
void SetSystem(SystemType sys);
/* Replace the game file buffer *sf with data, which was found on a disk
   image of system sys */
void AdoptDataFile(uint8_t **sf, size_t *extent, uint8_t *data, size_t len, SystemType sys);
/* Allocate an empty, terminated image list with room for count images */
imgrec *NewImageList(size_t count);

int DrawCloseup(int item);
void DrawCurrentRoom(void);
int DrawRoomImage(int room);
void DrawItemImage(int item);
int DrawImageWithName(const char *filename);
void FlushPixels(void);
void ClearGraphicsWindow(void);
void GraphicsWindowChanged(void);
void GraphicsWindowRecoloured(void);
int DrawImageWithTypeAndNumber(char type, int index);
void DrawOverlayImage(ImgType type, int index);
void DrawObjectImages(int room, int location);
char *ShortNameFromType(char type, int index);
ImgType ImageTypeFromName(const char *name);
int ImageNumberFromName(const char *name);
void DrawBlack(void);
void ShowApple2Screen(void);
/* Is this the name of an image file, such as "R003" or "B012"? */
int issagaimg(const char *name);
void SetColor(int32_t index, glui32 color);
// Only IBM PC graphics in "striped" mode use PutPixel()
void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color);
void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);
void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixelwidth);

#endif /* graphics_h */
