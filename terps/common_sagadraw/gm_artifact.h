//
//  gm_artifact.h
//
//  Shared Apple II hi-res -> colour kernel: the NTSC artifact-colour algorithm
//  (from the MAME Apple II driver) plus the 16-entry palette and the 7-bit
//  doubling. Used by both the Scott-family raster renderer
//  (common_sagadraw/apple2draw.c, which plots to Glk) and the Comprehend
//  renderer (graphics_magician.cpp, which fills an RGBA buffer). Only the
//  output step differs between the two; this colour kernel is identical, so it
//  lives here once.
//
//  Pure C, no Glk / allocation dependencies, so it links into every consumer.
//

#ifndef gm_artifact_h
#define gm_artifact_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 16-entry Apple II hi-res palette (0x00RRGGBB), indexed by the 4-bit artifact
// colour produced by gm_render_line_colors().
extern const uint32_t gm_apple2_palette[16];

// Double a 7-bit hi-res byte into its 14-bit doubled form (each set low bit
// becomes two output bits). Returns 0 for inputs > 127.
uint16_t gm_double7bits(uint8_t value);

// Compute the 40 doubled 14-bit "words" for one hi-res scanline
// (vram_row[0..39]), applying the high-bit (palette/phase) carry between
// adjacent bytes. words[] must hold 40 entries.
void gm_compute_row_words(const uint8_t *vram_row, uint16_t *words);

// Expand one scanline's 40 words into 560 4-bit NTSC-artifact colour indices.
// colors560[] must hold 560 entries.
void gm_render_line_colors(const uint16_t *words, uint8_t *colors560);

// --- Double hi-res (DHGR) variants -------------------------------------------
// DHGR already supplies 560 real pixels per row (no 7->14 doubling): each of the
// 40 columns carries 14 bits = the aux byte's low 7 pixels (leftmost) followed
// by the main byte's low 7 pixels. Build those words from a scanline's aux/main
// bytes (aux_row[0..39], main_row[0..39]). words[] must hold 40 entries.
void gm_compute_dhgr_row_words(const uint8_t *aux_row, const uint8_t *main_row,
    uint16_t *words);

// Expand one DHGR scanline's 40 words into 560 4-bit NTSC-artifact colour
// indices. Identical pixel-run colour kernel as gm_render_line_colors(), but at
// the 80-column chroma phase (MAME's is_80_column rotation offset). colors560[]
// must hold 560 entries.
void gm_render_dhgr_line_colors(const uint16_t *words, uint8_t *colors560);

#ifdef __cplusplus
}
#endif

#endif /* gm_artifact_h */
