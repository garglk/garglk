/*
 *  graphics.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-06-04.
 */

#ifndef graphics_h
#define graphics_h

#include "definitions.h"
#include "sagadraw_host.h"

/* Bytes needed for a short image name such as "R003", with terminator */
#define IMAGE_NAME_SIZE 5

extern int ImageWidth;
extern int upside_down;

extern imgrec *Images;

extern ImgType LastImgType;
extern int LastImgIndex;

/* Set CurrentSys and the native picture size of that system */
void SetSystem(SystemType sys);
/* Replace the game file buffer *sf with data, which was found on a disk
   image of system sys */
void AdoptDataFile(uint8_t **sf, size_t *extent, uint8_t *data, size_t len, SystemType sys);
/* Allocate an empty, terminated image list with room for count images */
imgrec *NewImageList(size_t count);

/* Draw special picture number item. Returns 0 on failure. */
int DrawCloseup(int item);
/* Draw the player's room with the items in it, unless that picture is what
   is already shown or an animation that began in this room is running. A
   dark room is drawn as black, or as S999 on the Atari ST and R000 in
   Claymorgue. With graphics off or no room picture, closes the window. */
void DrawCurrentRoom(void);
/* Clear the window and draw a room picture without its items. Returns 0 on
   failure. */
int DrawRoomImage(int room);
void DrawItemImage(int item);
/* Draw a picture from Images with the decoder of the current system. On
   MS-DOS one that is not on the list yet is looked for on disk. Returns 0 on
   failure. */
int DrawImageWithName(const char *filename);
void ClearGraphicsWindow(void);
void GraphicsWindowChanged(void);
void GraphicsWindowRecoloured(void);
int DrawImageWithTypeAndNumber(char type, int index);
void DrawOverlayImage(ImgType type, int index);
void DrawObjectImages(int room, int location);
/* A new string such as "R003" for type 'R' (room), 'B' (item) or 'S'
   (special) and a number. The caller frees it. */
char *ShortNameFromType(char type, int index);
int ImageNumberFromName(const char *name);
void DrawBlack(void);
void ShowApple2Screen(void);
/* Is this the name of an image file, such as "R003" or "B012"? */
int issagaimg(const char *name);

#endif /* graphics_h */
