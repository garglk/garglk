//
//  ui.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2026-05-18.
//

#ifndef ui_h
#define ui_h

#include <stdarg.h>

#include "glk.h"

void Display(winid_t w, const char *fmt, ...);
void PrintToStreamV(strid_t stream, const char *fmt, va_list ap);
void PrintCharacter(unsigned char c);
unsigned char WaitCharacter(void);
void TopWindow(void);
void BottomWindow(void);
void Updates(event_t ev);
void CloseGraphicsWindow(void);
void OpenGraphicsWindow(void);
void OpenTopWindow(void);
void OpenBottomWindow(void);
void DrawBlack(void);
void DrawRoomImage(void);
void DisplayInit(void);

extern winid_t Bottom, Top, Graphics, CurrentWindow;
extern int Resizing;
extern int Options;
extern int LineEvent;

#endif /* ui_h */
