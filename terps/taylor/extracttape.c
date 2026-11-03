//
//  extracttape.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Handles extraction and decryption of game data from ZX Spectrum tape images
//  (TZX and TAP formats) and Z80 memory snapshots. Most Adventure Soft UK games
//  used the "Alkatraz" copy protection scheme (by Appleby Associates), which
//  encrypts game data using XOR with a self-modifying key stream, then shuffles
//  memory blocks to obscure the final layout. This file provides the tools to
//  reverse that process.
//
//  Created by Petter Sjölund on 2022-04-11.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alkatraz_screen.h"
#include "decompressz80.h"
#include "decrypttotloader.h"
#include "loading_screen.h"
#include "kayleth_loadscreen_data.h"
#include "templea_loadscreen_data.h"
#include "terraquake_loadscreen_data.h"
#include "utility.h"

#include "extracttape.h"

// Start of the ZX Spectrum printer buffer area, used as a convenient
// staging address for decrypted data before it is relocated.
#define SPECTRUM_PRINTER_BUFFER 0x5b00

// The Alkatraz block-copy (ZXLdir/ZXLddr), decryption (DeAlkatraz) and block
// unshuffle (DeshuffleAlkatraz) primitives are in the decompressz80 library,
// which the scott interpreter shares.

// The part of the Z80 address space (ZX_MEMORY_SIZE) that makes up a 48K
// .SNA snapshot: a 27-byte header followed by the RAM from 0x4000.
#define SNA_SIZE 0xc01b
#define SNA_START_IN_MEMORY 0x3fe5

// A 128K .SNA file: the header, pages 5, 2 and the one banked in, four bytes
// (PC, the paging port, TR-DOS) and the five other pages in page order.
// Without the four bytes, what follows the header is the image that
// DecompressZ80 makes of a 128K snapshot.
#define SNA_128K_SIZE 0x2001f
#define SNA_128K_EXTRA_OFFSET (SNA_SIZE)
#define SNA_128K_EXTRA_SIZE 4

// Where the decrypted Temple of Terror Side B image keeps its loader's
// screen window descriptors (see alkatraz_screen.h), and their count.
#define TOT_SIDE_B_SCREEN_WINDOWS 0xeea9
#define TOT_SIDE_B_SCREEN_DESCRIPTORS 0xeeab

// Length of a Kayleth Z80 snapshot that was saved while still unpacking.
#define BROKEN_KAYLETH_Z80_SIZE 0xb4bb

// Extracts the game-relevant portion from a full 64 KB memory image.
// Copies 0xC01B bytes starting at offset 0x3FE5 (just below the 0x4000
// screen boundary), producing a new buffer in .SNA snapshot format that the
// interpreter can load directly. The caller still owns memory.
static uint8_t *ShrinkToSnaSize(const uint8_t *memory, size_t *length)
{
    uint8_t *sna = MemAlloc(SNA_SIZE);
    memcpy(sna, memory + SNA_START_IN_MEMORY, SNA_SIZE);
    *length = SNA_SIZE;
    return sna;
}

// Extracts a single data block from a tape image (TAP or TZX format) and
// copies its data, without the flag byte before it and the checksum after
// it, into outbuf at the given offset. Returns outbuf on success, or NULL if
// block extraction fails or the block does not fit in the outsize bytes of
// outbuf.
static uint8_t *add_block(const uint8_t *image, uint8_t *outbuf, size_t outsize, size_t origlen, int blocknum, size_t offset, int is_tzx)
{
    size_t len = 0;
    uint8_t *block = is_tzx ? GetTZXBlock(blocknum, image, origlen, &len)
                            : GetTAPBlock(blocknum, image, origlen, &len);
    if (!block) return NULL;
    len = len >= ZX_TAPE_FRAME_BYTES ? len - ZX_TAPE_FRAME_BYTES : 0;
    if (offset > outsize || len > outsize - offset) {
        free(block);
        return NULL;
    }
    memcpy(outbuf + offset, block + ZX_TAPE_FLAG_BYTES, len);
    free(block);
    return outbuf;
}

// Keeps a copy of a 6912-byte SCREEN$ to show as the title picture.
static void CaptureLoadingScreen(const uint8_t *screen)
{
    ZXLoadingScreen = MemAlloc(ZX_SCREEN_SIZE);
    memcpy(ZXLoadingScreen, screen, ZX_SCREEN_SIZE);
}

// Parameters for the common TZX extraction pipeline used by several games.
// Each game that uses Alkatraz protection follows the same sequence:
//   1. Extract a TZX block
//   2. Decrypt via DeAlkatraz into the Spectrum printer buffer area
//   3. Relocate data upward via LDDR
//   4. Unshuffle memory blocks via DeshuffleAlkatraz
//   5. Trim to SNA-sized output
// This struct captures the game-specific magic numbers for each step.
typedef struct {
    int block_index;           // Which TZX data block to extract
    uint8_t loacon_init;       // Initial rolling key for DeAlkatraz
    size_t source_offset;      // Starting offset within the extracted block
    uint16_t size;             // Number of bytes to decrypt
    uint8_t add1;              // First key evolution constant
    uint8_t add2;              // Second key evolution constant
    uint16_t lddr_target;      // LDDR destination address
    uint16_t lddr_source;      // LDDR source address
    uint16_t lddr_size;        // LDDR byte count
    uint8_t deshuffle_repeats; // Number of deshuffle table entries
    uint16_t deshuffle_IX;     // Address of the deshuffle table (end)
    uint16_t deshuffle_store;  // Base store address for deshuffle
} ExtractSpec;

// Executes the standard Alkatraz extraction pipeline for a given game's
// parameters. Returns the SNA-sized game data, or the original image on failure.
static uint8_t *process_tzx_extract(uint8_t *image, size_t *length, const ExtractSpec *spec)
{
    size_t block_length = 0;
    uint8_t *block = GetTZXBlock(spec->block_index, image, *length, &block_length);
    if (!block) {
        fprintf(stderr, "TZX extract: Could not extract block %d\n", spec->block_index);
        return image;
    }

    uint8_t loacon = spec->loacon_init;
    const AlkatrazKey schedule = { spec->add1, spec->add2, 0, 0 };
    uint8_t *uncompressed = ZXNewMemory();
    if (!uncompressed || !DeAlkatraz(block, block_length, spec->source_offset, uncompressed, SPECTRUM_PRINTER_BUFFER, spec->size, &loacon, &schedule)) {
        free(uncompressed);
        free(block);
        fprintf(stderr, "DeAlkatraz failed for block %d\n", spec->block_index);
        return image;
    }

    ZXLddr(uncompressed, spec->lddr_target, spec->lddr_source, spec->lddr_size);
    DeshuffleAlkatraz(uncompressed, spec->deshuffle_repeats, spec->deshuffle_IX, spec->deshuffle_store);

    free(block);
    uint8_t *sna = ShrinkToSnaSize(uncompressed, length);
    free(uncompressed);
    free(image);
    return sna;
}

// The Alkatraz decryption parameters of each game.
static const ExtractSpec templeA = {
    .block_index = 3,
    .loacon_init = 0x9a,
    .source_offset = 0x1b0c,
    .size = 0x9f64,
    .add1 = 0xcf,
    .add2 = 0xcd,
    .lddr_target = 0xffff,
    .lddr_source = 0xfc85,
    .lddr_size = 0x9f8e,
    .deshuffle_repeats = 5,
    .deshuffle_IX = 0x5b8b,
    .deshuffle_store = 0xfdd6
};
static const ExtractSpec kayleth = {
    .block_index = 3,
    .loacon_init = 0xce,
    .source_offset = 0x172e,
    .size = 0xa004,
    .add1 = 0xeb,
    .add2 = 0x8f,
    .lddr_target = 0xfd93,
    .lddr_source = 0xfb02,
    .lddr_size = 0x9e38,
    .deshuffle_repeats = 0x17,
    .deshuffle_IX = 0x5bf2,
    .deshuffle_store = 0xfd90
};
static const ExtractSpec terraquake = {
    .block_index = 3,
    .loacon_init = 0xe7,
    .source_offset = 0x17eb,
    .size = 0x9f64,
    .add1 = 0x75,
    .add2 = 0x55,
    .lddr_target = 0xfc80,
    .lddr_source = 0xfa32,
    .lddr_size = 0x9d38,
    .deshuffle_repeats = 0x03,
    .deshuffle_IX = 0x5b90,
    .deshuffle_store = 0xfc80
};

// The Alkatraz-protected tapes, identified by file size: how to decrypt the
// game, and how to rebuild the loading screen. The screen is drawn by the
// Alkatraz loader, not stored as a plain block, so it has its own window
// descriptors and stream parameters (see the *_loadscreen_data.h files).
typedef struct {
    size_t file_size;
    const ExtractSpec *spec;
    const AlkatrazScreen *screen;
} AlkatrazTape;

static const AlkatrazTape alkatraz_tapes[] = {
    { 0xcadc, &kayleth, &kayleth_screen },       // Kayleth (TZX)
    { 0xccca, &templeA, &templea_screen },       // Temple of Terror, Side A (TZX)
    { 0xcd15, &terraquake, &terraquake_screen }, // Terraquake (TZX, variant 1)
    { 0xcd17, &terraquake, &terraquake_screen }, // Terraquake (TZX, variant 2)
};

// Decrypts an Alkatraz tape. The loading screen is rebuilt first, before
// extraction consumes the raw image.
static uint8_t *ProcessAlkatrazTape(uint8_t *image, size_t *length, const AlkatrazTape *tape)
{
    if (ZXLoadingScreen == NULL)
        ZXLoadingScreen = DecodeAlkatrazLoadingScreen(image, *length, tape->screen);
    return process_tzx_extract(image, length, tape->spec);
}

// Special-case handler for a broken Kayleth Z80 snapshot that was captured
// mid-decompression. The snapshot contains partially-relocated data that
// needs the remaining LDDR and DeshuffleAlkatraz steps to be completed
// before the game data is usable. image is the decompressed 48K snapshot.
static uint8_t *FixBrokenKayleth(uint8_t *image, size_t *length)
{
    uint8_t *mem = MemAlloc(ZX_MEMORY_SIZE);
    memcpy(mem + ZX_RAM_BASE, image, ZX_MEMORY_SIZE - ZX_RAM_BASE);
    // The rest of the loader's relocation: move 0x1ed5 bytes up by 8 ...
    ZXLddr(mem, 0xf906, 0xf8fe, 0x1ed5);
    // ... zero the 8 bytes at 0xda2a ...
    mem[0xda2a] = 0;
    ZXLdir(mem, 0xda2b, 0xda2a, 7);
    // ... and put back the 0x13 gaps from the table that ends at 0x5bde.
    DeshuffleAlkatraz(mem, 0x13, 0x5bde, 0xfdcf);
    free(image);
    *length = ZX_MEMORY_SIZE;
    return mem;
}

// Top-level entry point for processing a raw game file. Detects the file
// format and applies the appropriate extraction/decryption pipeline.
//
// The file may be a Z80 snapshot (handled by DecompressZ80), a TZX tape
// image, or a TAP tape image. TZX/TAP files are identified by their size,
// which is unique to each known game release. Returns a buffer containing
// the processed game data ready for the interpreter.
uint8_t *ProcessFile(uint8_t *image, size_t *length)
{
    size_t origlen = *length;

    // Capture a ZX loading screen to show as a title image. For .tap/.tzx
    // tapes the SCREEN$ is a data block in the raw image, so grab it now,
    // before the snapshot/tape extraction below transforms the buffer.
    int is_tzx = (origlen > 8 && memcmp(image, "ZXTape!\x1a", 8) == 0);
    int looks_like_tap = (!is_tzx && origlen > 21 &&
        image[0] == 0x13 && image[1] == 0x00 && image[2] == 0x00);
    if (is_tzx || looks_like_tap)
        ZXLoadingScreen = FindTapeLoadingScreen(image, origlen, is_tzx);

    // First, try to decompress as a Z80 snapshot format
    uint8_t *uncompressed = DecompressZ80(image, length);
    if (uncompressed != NULL) {
        free(image);
        image = uncompressed;
        // In a decompressed 48K snapshot, offset 0 maps to Spectrum address
        // 0x4000 (the display file): the first 6912 bytes are the screen
        // visible when the snapshot was saved.
        if (ZXLoadingScreen == NULL && *length >= ZX_SCREEN_SIZE) {
            CaptureLoadingScreen(image);
        }
    } else {
        // Not a Z80 snapshot — try TZX/TAP extraction, identified by file size.
        uint8_t *out = NULL;
        int isTZX = 0;

        // A 128K .SNA is given the layout of a decompressed 128K .z80, so
        // that the pages past the first three are in the same place in both.
        if (origlen == SNA_128K_SIZE && !is_tzx && !looks_like_tap) {
            size_t rest = SNA_128K_EXTRA_OFFSET + SNA_128K_EXTRA_SIZE;
            memmove(image + SNA_128K_EXTRA_OFFSET, image + rest, origlen - rest);
            *length = origlen - SNA_128K_EXTRA_SIZE;
        }

        // Dispatch by file size to identify which game/format we have
        switch (origlen) {
            case 0xa000: { // Temple of Terror, Side B (text-only, TZX)
                uint8_t *memory = DecryptToTSideB(image, length);
                if (memory == NULL) {
                    // Not the tape we thought it was: keep the raw image,
                    // like the other tape paths do.
                    break;
                }
                // DecryptToTSideB draws the loading screen into the display
                // file at 0x4000 (and uses it to decrypt the game); capture it
                // as a title image before the buffer is shrunk.
                if (ZXLoadingScreen == NULL) {
                    CaptureLoadingScreen(memory + ZX_RAM_BASE);
                    // The loader's window descriptors survive in the decrypted
                    // image; regenerate the draw order so the title reveals in
                    // the same sequence the loader used.
                    int windows = READ_LE_UINT16(memory + TOT_SIDE_B_SCREEN_WINDOWS);
                    if (windows <= (ZX_MEMORY_SIZE - TOT_SIDE_B_SCREEN_DESCRIPTORS) / 4) {
                        uint16_t order[ZX_SCREEN_SIZE];
                        int n = AlkatrazScreenOrder(memory + TOT_SIDE_B_SCREEN_DESCRIPTORS,
                            windows, order, ZX_SCREEN_SIZE);
                        ZXSetDrawOrder(order, n);
                    }
                }
                free(image);
                image = ShrinkToSnaSize(memory, length);
                free(memory);
                break;
            }
            case 0x10428: // Blizzard Pass (TZX)
                isTZX = 1;
                // Fallthrough
            case 0x104c4: { // Blizzard Pass (TAP)
                // Blizzard Pass isn't Alkatraz-encrypted — just put its 5
                // data blocks where they are in the image that DecompressZ80
                // makes of a 128K snapshot of the game, which has page 0
                // banked in: pages 5, 2, 0, 1, 3, 4, 6, 7. The first block
                // is page 6. The others are loaded at 0xC000 (page 0),
                // 0x8000, 0x5ECB and 0xFFDC. The block numbers are those of
                // the TZX: the TAP has four blocks more before the first.
                static const struct {
                    int tzx_block;
                    size_t offset;
                } BlizzardPassBlocks[] = {
                    { 7, 6 * ZX_PAGE_SIZE },
                    { 11, 0xc000 - ZX_RAM_BASE },
                    { 13, 0x8000 - ZX_RAM_BASE },
                    { 15, 0x5ecb - ZX_RAM_BASE },
                    { 17, 0xffdc - ZX_RAM_BASE }
                };
                int extra_tap_blocks = isTZX ? 0 : 4;
                out = MemAlloc(Z80_IMAGE_SIZE_128K);
                memset(out, 0, Z80_IMAGE_SIZE_128K);
                for (size_t i = 0; i < sizeof BlizzardPassBlocks / sizeof BlizzardPassBlocks[0]; i++) {
                    if (!add_block(image, out, Z80_IMAGE_SIZE_128K, origlen,
                            BlizzardPassBlocks[i].tzx_block + extra_tap_blocks,
                            BlizzardPassBlocks[i].offset, isTZX)) {
                        free(out);
                        return image;
                    }
                }
                *length = Z80_IMAGE_SIZE_128K;
                free(image);
                return out;
            }
            default:
                for (size_t i = 0; i < sizeof alkatraz_tapes / sizeof alkatraz_tapes[0]; i++) {
                    if (alkatraz_tapes[i].file_size == origlen) {
                        image = ProcessAlkatrazTape(image, length, &alkatraz_tapes[i]);
                        break;
                    }
                }
                break;
        }
    }

    // Post-processing: one known Kayleth Z80 snapshot (0xB4BB bytes) was
    // captured mid-decompression and needs its relocation completed.
    if (origlen == BROKEN_KAYLETH_Z80_SIZE && uncompressed != NULL) {
        image = FixBrokenKayleth(image, length);
    }

    // Snapshots saved at a text prompt (e.g. "Resume a saved game?") have
    // no picture, just black text on white. Don't show those as a title.
    if (ZXLoadingScreen != NULL && ZXScreenIsBlackOnWhite(ZXLoadingScreen)) {
        free(ZXLoadingScreen);
        ZXLoadingScreen = NULL;
    }

    return image;
}
