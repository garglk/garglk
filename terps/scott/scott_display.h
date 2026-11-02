/*
 *  scott_display.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Declarations for room description display, image rendering,
 *  inventory listing, and related UI functions.
 */

#ifndef scott_display_h
#define scott_display_h

#include "glk.h"

void UpdateUSInventory(void);
void DrawRoomImage(void);
int ItIsDark(void);
void DrawBlack(void);
void OpenBottomWindow(void);
void OpenTopWindow(void);
void CloseTopWindow(void);
glui32 OptimalPictureSize(glui32 graphwidth, glui32 graphheight, glui32 *outwidth, glui32 *outheight);
void FitPictureToGraphicsWindow(glui32 *graphheight, glui32 *optimal_height);
int GraphicsEnabled(void);
void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);
void DrawImage(int image);
void DrawRoomItemImages(void);
void Look(void);
void ListInventory(int upper);
void LookWithPause(void);
void RequestRedraw(void);

extern int print_look_to_transcript;

#endif /* scott_display_h */
