//
//  gm_hires.h
//
//  Apple II hi-res page geometry, shared by the SAGA bitmap and vector
//  renderers (apple2draw.c, gm_vector.c) and the Comprehend renderers.
//

#ifndef gm_hires_h
#define gm_hires_h

#ifdef __cplusplus
extern "C" {
#endif

#define GM_HIRES_PAGE_SIZE 0x2000
#define GM_MAX_SCREEN_ADDR (GM_HIRES_PAGE_SIZE - 1)
#define GM_HIRES_ROWS      192
#define GM_HIRES_COLS      40 // bytes per scanline
#define GM_PIXELS_PER_BYTE 7  // bit 7 of each byte selects the palette

// Offset into the hi-res page of the first byte of scanline y
static inline int gm_row_address(int y)
{
    return (((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10);
}

// Inverse of gm_row_address: the scanline that page offset `offset` lies on.
// Only meaningful for offsets inside a scanline (offset & 0x7f < 120); the
// 8 "screen hole" bytes at the end of each 128-byte block belong to no row.
static inline int gm_row_from_offset(unsigned offset)
{
    return ((offset & 0x7f) / 40) * 64 + ((offset >> 7) & 7) * 8 + ((offset >> 10) & 7);
}

#ifdef __cplusplus
}
#endif

#endif /* gm_hires_h */
