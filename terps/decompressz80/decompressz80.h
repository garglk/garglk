//
//  decompressz80.h
//
//  A small library for getting at ZX Spectrum games, shared by the ScottFree
//  (scott), TaylorMade (taylor), Level 9 and UnQuill interpreters:
//
//    z80snap.c   reads .z80 snapshots
//    tapetzx.c   gets the data blocks out of .tzx and .tap tapes
//    alkatraz.c  undoes the Alkatraz copy protection
//    zxscreen.c  the layout and colours of the screen memory
//
//  Nothing here prints anything: a NULL or a 0 is all there is to say that a
//  file was not what it was taken for. Build the library with
//  -DDECOMPRESSZ80_VERBOSE to have the reason written to stderr.
//
//  Created by Petter Sjölund on 2022-01-24.
//

#ifndef decompressz80_h
#define decompressz80_h

#include <stdint.h>
#include <stdlib.h> /* what these functions return is for the caller to free */

/* --- The memory of the ZX Spectrum ---
   The ROM is at 0x0000-0x3FFF. The RAM begins with the screen: the bitmap at
   0x4000 and the attributes at 0x5800. A 128K machine has eight RAM pages of
   16K: page 5 is at 0x4000, page 2 at 0x8000, and any one of them is banked
   in at 0xC000. */
#define ZX_MEMORY_SIZE  0x10000 /* the whole address space */
#define ZX_PAGE_SIZE    0x4000
#define ZX_RAM_BASE     0x4000
#define ZX_BITMAP_BASE  0x4000
#define ZX_ATTR_BASE    0x5800

/* --- .z80 snapshots --- */

#define Z80_IMAGE_SIZE_48K  0xc000
#define Z80_IMAGE_SIZE_128K 0x20000

/* Reads the version 1, 2 or 3 .z80 snapshot in the *length bytes at raw_data
   and returns its RAM as one malloc'd image, with *length set to the size of
   it. Returns NULL, leaving *length alone, if the data is not a .z80
   snapshot. There is no signature to tell one by, so anything may be handed
   in to find out.

   The image of a 48K machine is the Z80_IMAGE_SIZE_48K bytes from address
   0x4000 to 0xFFFF. A 16K machine and the Timex ones are read as 48K.

   The image of a 128K machine (which includes the +2, +2A, +3, Pentagon and
   Scorpion) is Z80_IMAGE_SIZE_128K bytes, each of the eight pages once. It
   starts as the 48K one does, with what was at 0x4000 to 0xFFFF: page 5,
   page 2 and the page that was banked in at 0xC000, which is the low three
   bits of the value last written to port 0x7FFD. The other pages follow in
   page order. That is the order of a 128K .sna file, without its header and
   the four bytes it has after the first three pages. If the page banked in
   is 5 or 2 it is not put in twice: all six others follow in page order.
   Pages that the file does not have are zeroes. Use DecompressZ80Pages for
   the pages by number. */
uint8_t *DecompressZ80(const uint8_t *raw_data, size_t *length);

/* Reads a .z80 snapshot as DecompressZ80 does and returns the eight RAM pages
   in page order as one malloc'd buffer of 8 * ZX_PAGE_SIZE bytes, or NULL.
   Page 5 is 0x4000-0x7FFF and page 2 is 0x8000-0xBFFF. A 48K snapshot has
   0xC000-0xFFFF in page 0 and zeroes in the other five. Pages that the file
   does not have are zeroes.

   If is_128k is not NULL it is set to 1 for a 128K machine and 0 for 48K.
   If banked_page is not NULL it is set to the number of the page at 0xC000,
   the low three bits of the last value written to port 0x7FFD. That is 0 for
   a 48K snapshot, which has its memory in that page. */
uint8_t *DecompressZ80Pages(const uint8_t *raw_data, size_t length,
    int *is_128k, int *banked_page);

/* --- .tzx and .tap tapes ---
   A data block is the bytes that one pilot tone on the tape is followed by.
   The ROM loader has a flag byte first (0x00 for a header, 0xFF for data) and
   a checksum byte last. A loader of its own, such as Alkatraz, has neither:
   every byte of its blocks is data. So GetTZXBlock and GetTAPBlock hand out
   the whole block, and it is for the caller to know what its first and last
   bytes are. ExtractTapePayloads is for what the ROM loader would load. */

/* The flag byte and the checksum byte of a block that the ROM loads: its
   data is ZX_TAPE_FRAME_BYTES shorter and begins ZX_TAPE_FLAG_BYTES in. */
#define ZX_TAPE_FLAG_BYTES 1
#define ZX_TAPE_FRAME_BYTES 2

/* Returns a malloc'd copy of data block number blockno, counting from 0, of
   the tzx_length bytes of .tzx at tzx, and sets *block_length to its length.
   Only standard and turbo speed data blocks count, the ROM's headers among
   them. Text, pauses, group marks and the like do not, so that the number
   stays the same when a tape image has more or fewer of those, and is that
   of the same block in a .tap of the tape. Returns NULL, leaving
   *block_length alone, if the file is not a .tzx or has no such block. */
uint8_t *GetTZXBlock(int blockno, const uint8_t *tzx, size_t tzx_length,
    size_t *block_length);

/* The same for the tap_length bytes of .tap at tap, in which every block is
   a data block. A .tap has no signature: any file is one until a block
   length runs past its end. */
uint8_t *GetTAPBlock(int blockno, const uint8_t *tap, size_t tap_length,
    size_t *block_length);

/* Joins the data of every data block (flag byte 0xFF, standard or turbo
   speed) of a .tzx (is_tzx != 0) or a .tap (is_tzx == 0) into one malloc'd
   buffer, each without its flag byte and checksum, and sets *out_len to the
   length of it. Headers and blocks with other flag bytes are passed over.
   A block that cannot be read ends the tape there, and what was found before
   it is returned. Returns NULL if there is nothing: no data blocks, or not a
   .tzx when one was asked for.

   block_offsets and block_lengths, if not NULL, get where in the result each
   block is and how long, for the first max_blocks of them, and *num_blocks
   how many that was. Any of the four output parameters may be NULL. */
uint8_t *ExtractTapePayloads(const uint8_t *raw, size_t raw_len, int is_tzx,
    size_t *out_len, size_t *block_offsets, size_t *block_lengths,
    int *num_blocks, int max_blocks);

/* The loading screen of a .tzx (is_tzx != 0) or a .tap (is_tzx == 0): in
   practice always the first data block that is exactly one SCREEN$
   (ZX_SCREEN_SIZE bytes) long, flag byte and checksum left out. Only the
   first 64 data blocks are searched. Returns a malloc'd copy of it for the
   caller to free, or NULL if there is none. A screen that a loader of its
   own draws, as Alkatraz does, is not found. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx);

/* --- Alkatraz copy protection ---
   An Alkatraz loader decrypts the game with a rolling key as it loads it and
   then moves blocks of it about in memory. These do the same to an image of
   the Spectrum's whole memory, ZX_MEMORY_SIZE bytes indexed by address. */

/* A zeroed ZX_MEMORY_SIZE image for the caller to free, or NULL */
uint8_t *ZXNewMemory(void);

/* The Z80 block copy instructions LDIR (upwards from the given addresses) and
   LDDR (downwards from them), a byte at a time, so that overlapping blocks
   come out as they do on the machine. */
void ZXLdir(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size);
void ZXLddr(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size);

/* How a loader moves its key on after every byte. They all do it the same
   way but each with numbers of its own. */
typedef struct {
    uint8_t adjust; /* added every other sixteen bytes */
    uint8_t step;   /* added after every byte */
    /* The loader of Temple of Terror decrypts over its own code, and gets new
       values for the two numbers above when it reaches the addresses of the
       instructions that hold them. 0 for a loader that does not. */
    uint16_t adjust_address;
    uint16_t step_address;
} AlkatrazKey;

/* Decrypts the count bytes at src + src_offset to address target of mem.
   *key is the key to begin with and is left as the key to go on with, for
   the next call to take up where this one ended. The numbers in schedule are
   never changed: what a loader does to its own is forgotten at the end of
   each call. Returns 1, or 0 with nothing done if the src_length bytes at
   src are too few. */
int DeAlkatraz(const uint8_t *src, size_t src_length, size_t src_offset,
    uint8_t *mem, uint16_t target, uint16_t count, uint8_t *key,
    const AlkatrazKey *schedule);

/* Puts back the gaps that the game was saved without. There is a table of
   'repeats' entries of five bytes: the address of a gap, its length less one
   and the byte to fill it with. ix is the address of the last byte of the
   table, which is gone through backwards. 'store' is the address of the last
   byte of the game as it is before the gaps are put in. */
void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t ix, uint16_t store);

/* --- The screen ---
   A SCREEN$ is the 6912 bytes of the Spectrum's screen memory: 6144 bytes of
   bitmap, which is not in order from the top down, and 768 bytes of
   attributes, one for the colours of each 8 by 8 cell. These work out what
   is where; the drawing is for the interpreters to do. */
#define ZX_SCREEN_SIZE   6912   /* bitmap and attributes                     */
#define ZX_BITMAP_SIZE   6144   /* the bitmap alone (192 * 256 / 8)         */
#define ZX_SCREEN_WIDTH  256
#define ZX_SCREEN_HEIGHT 192
#define ZX_SCREEN_COLS   32     /* character cells across                   */

/* The colours as 0x00RRGGBB: the eight normal ones and then the eight bright
   ones, in the order of the numbers that ZXDecodeAttr gives. */
extern const uint32_t ZXPalette[16];

/* The pixel row (0 to 191) of the bitmap byte at bitmap_offset (0 to 6143) */
int ZXBitmapRow(int bitmap_offset);

/* The other way: the offset into the bitmap of the byte at pixel row y (0 to
   191) of character column col (0 to 31) */
int ZXBitmapOffset(int y, int col);

/* The ink and paper colours (0 to 15) of an attribute byte, bright bit and
   all. The flash bit is left out. Either pointer may be NULL. */
void ZXDecodeAttr(uint8_t attr, int *ink, int *paper);

/* 1 if every attribute is black ink on white paper, bright or not: a screen
   with nothing but text on it, or nothing at all. NULL counts as that too. */
int ZXScreenIsBlackOnWhite(const uint8_t *screen);

#endif /* decompressz80_h */
