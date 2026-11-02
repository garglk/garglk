//
//  rtpi_graphics.h
//  scott
//
//  Created by Administrator on 2026-02-12.
//

#ifndef rtpi_graphics_h
#define rtpi_graphics_h

#include <stddef.h>
#include <stdint.h>

#include "sagagraphics.h"
#include "scott_defines.h"

int DrawRTPIImage(USImage *image);
int DrawFuzzyRTPIImage(USImage *image);
void DrawRTPIFromMem(void);
void UpdateRTPISystemMessages(void);
void ClearVDP(void);
GameIDType DetectRTPI(uint8_t *data, size_t datalength);

#endif /* rtpi_graphics_h */
