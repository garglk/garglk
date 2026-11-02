//
//  title_image.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-01.
//

#ifndef title_image_h
#define title_image_h

#include <stdint.h>
#include <stddef.h>

#include "decompressz80.h"  /* ZX_SCREEN_SIZE and the SCREEN$ decode helpers */

void DrawTitleImageScott(void);
void PrintTitleScreenBuffer(void);
void PrintTitleScreenGrid(void);

/* Show the captured ZXLoadingScreen as a title image, then restore the
   normal game windows. Frees ZXLoadingScreen and sets it to NULL. */
void DrawZXTitleImage(void);

#endif /* title_image_h */
