/* tapetzx.c: gets the data blocks out of .tzx and .tap tape images of the ZX
   Spectrum.

   Part of the decompressz80 library. The tape is only walked: every block is
   stepped over by its length and the data blocks are handed out, which is
   enough for getting at what a tape would have loaded and does not fail on
   the whole tape for one block that is out of the ordinary.

   Written with libspectrum's tzx_read.c and tap.c at hand,
   Copyright (c) 2001-2007 Philip Kendall, Darren Salt and others, GPL v2 or
   later (see z80snap.c for the notice).
*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"
#include "decompressz80_internal.h"

/* A .tzx file begins with "ZXTape!", 0x1A and two bytes of version number */
static const uint8_t tzx_signature[8] = { 'Z', 'X', 'T', 'a', 'p', 'e', '!', 0x1a };
#define TZX_HEADER_SIZE 10

#define TZX_ID_STANDARD_DATA 0x10
#define TZX_ID_TURBO_DATA 0x11

/* What follows the ID byte of each kind of block in TZX 1.20: 'fixed' bytes,
   among them a number of 'length_bytes' bytes at 'length_at', and then that
   number of items of 'item_size' bytes each. 0x16, 0x17, 0x34 and 0x40 are
   the blocks that version 1.20 deprecated. */
typedef struct {
    uint8_t id;
    uint8_t fixed;
    uint8_t length_at;
    uint8_t length_bytes;
    uint8_t item_size;
} TZXLayout;

static const TZXLayout tzx_layouts[] = {
    { TZX_ID_STANDARD_DATA, 4, 2, 2, 1 },
    { TZX_ID_TURBO_DATA, 18, 15, 3, 1 },
    { 0x12, 4, 0, 0, 0 },   /* pure tone */
    { 0x13, 1, 0, 1, 2 },   /* pulse sequence */
    { 0x14, 10, 7, 3, 1 },  /* pure data */
    { 0x15, 8, 5, 3, 1 },   /* direct recording */
    { 0x16, 4, 0, 4, 1 },   /* C64 ROM type data */
    { 0x17, 4, 0, 4, 1 },   /* C64 turbo tape data */
    { 0x18, 4, 0, 4, 1 },   /* CSW recording */
    { 0x19, 4, 0, 4, 1 },   /* generalized data */
    { 0x20, 2, 0, 0, 0 },   /* pause, or stop the tape */
    { 0x21, 1, 0, 1, 1 },   /* group start */
    { 0x22, 0, 0, 0, 0 },   /* group end */
    { 0x23, 2, 0, 0, 0 },   /* jump */
    { 0x24, 2, 0, 0, 0 },   /* loop start */
    { 0x25, 0, 0, 0, 0 },   /* loop end */
    { 0x26, 2, 0, 2, 2 },   /* call sequence */
    { 0x27, 0, 0, 0, 0 },   /* return from sequence */
    { 0x28, 2, 0, 2, 1 },   /* select */
    { 0x2a, 4, 0, 4, 1 },   /* stop the tape if in 48K mode */
    { 0x2b, 4, 0, 4, 1 },   /* set signal level */
    { 0x30, 1, 0, 1, 1 },   /* text description */
    { 0x31, 2, 1, 1, 1 },   /* message */
    { 0x32, 2, 0, 2, 1 },   /* archive info */
    { 0x33, 1, 0, 1, 3 },   /* hardware type */
    { 0x34, 8, 0, 0, 0 },   /* emulation info */
    { 0x35, 20, 16, 4, 1 }, /* custom info */
    { 0x40, 4, 1, 3, 1 },   /* snapshot */
    { 0x5a, 9, 0, 0, 0 },   /* glue */
};

/* One block of a tape. 'data' is what the ROM loader would have read from a
   standard or turbo speed block: the flag byte, the data and the checksum.
   It is NULL for any other kind of block. */
typedef struct {
    const uint8_t *data;
    size_t length;
} TapeBlock;

static int IsTZX(const uint8_t *tape, size_t size)
{
    return tape != NULL && size >= TZX_HEADER_SIZE
        && memcmp(tape, tzx_signature, sizeof tzx_signature) == 0;
}

/* Reads the block of a .tzx at *pos and moves *pos past it. Returns 0 at the
   end of the tape, at a block of an unknown kind and at one that does not fit
   in what is left of the file. */
static int NextTZXBlock(const uint8_t *tape, size_t size, size_t *pos, TapeBlock *block)
{
    if (*pos >= size)
        return 0;

    uint8_t id = tape[*pos];
    size_t left = size - *pos - 1;
    const uint8_t *header = tape + *pos + 1;

    const TZXLayout *layout = NULL;
    for (size_t i = 0; i < sizeof tzx_layouts / sizeof tzx_layouts[0]; i++)
        if (tzx_layouts[i].id == id)
            layout = &tzx_layouts[i];
    if (layout == NULL) {
        ZX_LOG("unknown TZX block type 0x%02x", id);
        return 0;
    }

    if (left < layout->fixed) {
        ZX_LOG("TZX block 0x%02x cut short", id);
        return 0;
    }
    uint64_t length = (uint64_t)ReadLE(header + layout->length_at, layout->length_bytes)
        * layout->item_size;
    if (length > left - layout->fixed) {
        ZX_LOG("TZX block 0x%02x runs past the end of the file", id);
        return 0;
    }

    int has_data = (id == TZX_ID_STANDARD_DATA || id == TZX_ID_TURBO_DATA);
    block->data = has_data ? header + layout->fixed : NULL;
    block->length = has_data ? (size_t)length : 0;
    *pos += 1 + layout->fixed + (size_t)length;
    return 1;
}

/* The same for a .tap, which is nothing but data blocks, each after its
   two-byte length. A block of no length is taken for the end of the tape,
   as what follows the last block of a padded file looks like one. */
static int NextTAPBlock(const uint8_t *tape, size_t size, size_t *pos, TapeBlock *block)
{
    if (tape == NULL || *pos >= size || size - *pos < 2)
        return 0;

    size_t length = ReadLE(tape + *pos, 2);
    if (length == 0 || length > size - *pos - 2)
        return 0;

    block->data = tape + *pos + 2;
    block->length = length;
    *pos += 2 + length;
    return 1;
}

static uint8_t *CopyOf(const uint8_t *data, size_t length)
{
    /* malloc(0) may return NULL, which here means failure */
    uint8_t *copy = malloc(length ? length : 1);
    if (copy != NULL)
        memcpy(copy, data, length);
    return copy;
}

uint8_t *GetTZXBlock(int blockno, const uint8_t *tzx, size_t tzx_length,
    size_t *block_length)
{
    if (block_length == NULL || !IsTZX(tzx, tzx_length))
        return NULL;

    TapeBlock block;
    size_t pos = TZX_HEADER_SIZE;
    int found = 0;
    while (NextTZXBlock(tzx, tzx_length, &pos, &block)) {
        if (block.data == NULL || found++ != blockno)
            continue;
        uint8_t *copy = CopyOf(block.data, block.length);
        if (copy != NULL)
            *block_length = block.length;
        return copy;
    }

    ZX_LOG("no TZX data block %d", blockno);
    return NULL;
}

uint8_t *GetTAPBlock(int blockno, const uint8_t *tap, size_t tap_length,
    size_t *block_length)
{
    if (block_length == NULL)
        return NULL;

    TapeBlock block;
    size_t pos = 0;
    for (int i = 0; NextTAPBlock(tap, tap_length, &pos, &block); i++) {
        if (i != blockno)
            continue;
        uint8_t *copy = CopyOf(block.data, block.length);
        if (copy != NULL)
            *block_length = block.length;
        return copy;
    }

    ZX_LOG("no TAP block %d", blockno);
    return NULL;
}

/* More data blocks than any tape we look at has. Only this many are searched
   for a loading screen: one further in is not found. */
#define MAX_SCREEN_SEARCH_BLOCKS 64

uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx)
{
    size_t block_offset[MAX_SCREEN_SEARCH_BLOCKS], block_len[MAX_SCREEN_SEARCH_BLOCKS];
    int num_blocks = 0;

    uint8_t *payloads = ExtractTapePayloads(raw, raw_len, is_tzx, NULL,
        block_offset, block_len, &num_blocks, MAX_SCREEN_SEARCH_BLOCKS);
    if (payloads == NULL)
        return NULL;

    uint8_t *screen = NULL;
    for (int i = 0; i < num_blocks; i++) {
        if (block_len[i] == ZX_SCREEN_SIZE) {
            screen = CopyOf(payloads + block_offset[i], ZX_SCREEN_SIZE);
            break;
        }
    }
    free(payloads);
    return screen;
}

/* A block that begins with this flag byte is data; 0x00 is for a header */
#define TAP_DATA_FLAG 0xff

uint8_t *ExtractTapePayloads(const uint8_t *raw, size_t raw_len, int is_tzx,
    size_t *out_len, size_t *block_offsets, size_t *block_lengths,
    int *num_blocks, int max_blocks)
{
    if (out_len)
        *out_len = 0;
    if (num_blocks)
        *num_blocks = 0;
    if (raw == NULL || raw_len == 0 || (is_tzx && !IsTZX(raw, raw_len)))
        return NULL;

    /* The payloads are parts of the tape, so all of them fit in its length */
    uint8_t *payloads = malloc(raw_len);
    if (payloads == NULL)
        return NULL;

    size_t payloads_len = 0, pos = is_tzx ? TZX_HEADER_SIZE : 0;
    int block_count = 0;
    TapeBlock block;

    while (is_tzx ? NextTZXBlock(raw, raw_len, &pos, &block)
                  : NextTAPBlock(raw, raw_len, &pos, &block)) {
        if (block.data == NULL || block.length <= ZX_TAPE_FRAME_BYTES
            || block.data[0] != TAP_DATA_FLAG)
            continue;

        size_t payload_len = block.length - ZX_TAPE_FRAME_BYTES;
        if (block_count < max_blocks) {
            if (block_offsets)
                block_offsets[block_count] = payloads_len;
            if (block_lengths)
                block_lengths[block_count] = payload_len;
            block_count++;
        }
        memcpy(payloads + payloads_len, block.data + 1, payload_len);
        payloads_len += payload_len;
    }

    if (payloads_len == 0) {
        free(payloads);
        return NULL;
    }

    if (num_blocks)
        *num_blocks = block_count;
    if (out_len)
        *out_len = payloads_len;
    return payloads;
}
