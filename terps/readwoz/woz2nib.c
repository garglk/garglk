/*
 * WOZ to NIB converter.
 *
 * Converts WOZ 1.x and 2.x disk image files into raw nibble (.nib) format.
 * WOZ is a modern Apple II disk image format that stores raw flux/bit data;
 * NIB is the simpler 6656-bytes-per-track nibble format used by emulators.
 *
 * The conversion process:
 *   1. Parse the WOZ container (INFO, TMAP, TRKS chunks)
 *   2. For each track, locate sync byte sequences to find sector alignment
 *   3. Extract nibble bytes from the raw bitstream
 *   4. Pad or trim to exactly 6656 nibbles per track
 *
 * A file that is not a WOZ of a single-sided 5.25" disk is refused. Within
 * one that is, a track that cannot be used -- unmapped, mapped to an entry
 * that does not exist, too short to hold a sector, or lying past the end of
 * the file -- is an empty track of zeros, and the rest of the disk converts.
 *
 * Created by Petter Sjölund on 2022-09-09.
 * Based on woz2dsk.pl by LEE Sau Dan <leesaudan@gmail.com>
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "debugprint.h"
#include "read_le16.h"

#include "woz2nib.h"

/* WOZ container layout */
#define WOZ_HEADER_SIZE 12          /* "WOZ1" or "WOZ2", ff 0a 0d 0a, CRC-32 of the rest */
#define WOZ_CRC_OFFSET 8
#define WOZ_CHUNK_HEADER_SIZE 8     /* four characters of ID, 32 bits of size */
#define WOZ_BLOCK_SIZE 512

/* WOZ quarter-track limits */
#define MAX_QUARTER_TRACKS 160
#define TMAP_EMPTY_TRACK 0xFF

/* WOZ 1.x track record layout */
#define WOZ1_TRACK_DATA_SIZE 6646   /* bitstream data portion */
#define WOZ1_TRACK_RECORD_SIZE 6656 /* data + 10-byte trailer */
#define WOZ1_BIT_COUNT_OFFSET (WOZ1_TRACK_DATA_SIZE + 2)

/* WOZ 2.x TRKS entry layout: starting block, block count, bit count */
#define WOZ2_TRKS_ENTRY_SIZE 8
#define WOZ2_BIT_COUNT_OFFSET 4

/* INFO chunk layout */
#define INFO_CHUNK_SIZE 60
#define INFO_VERSION_OFFSET 0
#define INFO_DISK_TYPE_OFFSET 1
#define INFO_DISK_TYPE_525 1
#define INFO_CREATOR_OFFSET 5
#define INFO_CREATOR_LENGTH 32
#define INFO_DISK_SIDES_OFFSET 37   /* INFO version 2 and later */

/* Where a track's bitstream is in the caller's buffer. */
typedef struct {
    size_t data_offset;         // byte offset into woz_data where the bitstream begins
    uint32_t bit_count;         // total number of valid bits in the track
} WozTrack;

typedef struct {
    int has_info, has_tmap, has_trks;
    uint8_t tmap[WOZ_NIB_TRACKS];       // index into trks of each whole track
    WozTrack trks[MAX_QUARTER_TRACKS];
} WozImage;

int is_woz_image(const uint8_t *data, size_t size)
{
    return size >= 4 && memcmp(data, "WOZ", 3) == 0 && (data[3] == '1' || data[3] == '2');
}

/* The CRC-32 of zlib and everything else: polynomial 0xedb88320, reflected.
 *
 * Eight bytes at a time ("slicing by 8"): table[k][b] is the CRC of byte b
 * followed by k zero bytes, so the eight lookups of a step do not wait for
 * one another. One byte at a time, each lookup waits for the last, and the
 * checksum took as long as the whole of the conversion that follows it. */
static uint32_t woz_crc32(const uint8_t *buf, size_t size)
{
    static uint32_t table[8][256];
    if (table[0][255] == 0) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int bit = 0; bit < 8; bit++)
                c = (c & 1) ? (c >> 1) ^ 0xedb88320 : c >> 1;
            table[0][i] = c;
        }
        for (int k = 1; k < 8; k++)
            for (uint32_t i = 0; i < 256; i++)
                table[k][i] = table[0][table[k - 1][i] & 0xff] ^ (table[k - 1][i] >> 8);
    }

    uint32_t crc = 0xffffffff;
    for (; size >= 8; size -= 8, buf += 8) {
        uint32_t lo = READ_LE_UINT32(buf) ^ crc;
        uint32_t hi = READ_LE_UINT32(buf + 4);
        crc = table[7][lo & 0xff] ^ table[6][(lo >> 8) & 0xff] ^
              table[5][(lo >> 16) & 0xff] ^ table[4][lo >> 24] ^
              table[3][hi & 0xff] ^ table[2][(hi >> 8) & 0xff] ^
              table[1][(hi >> 16) & 0xff] ^ table[0][hi >> 24];
    }
    while (size--)
        crc = table[0][(crc ^ *buf++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

static int woz_check_header(const uint8_t *woz_data, size_t woz_size)
{
    if (woz_size < WOZ_HEADER_SIZE || !is_woz_image(woz_data, woz_size)) {
        debug_print("WOZ header not found\n");
        return 0;
    }

    if (memcmp(woz_data + 4, "\xff\n\r\n", 4) != 0) {
        debug_print("bad header\n");
        return 0;
    }

    /* A stored CRC of zero means that none was calculated. */
    uint32_t stored_crc = READ_LE_UINT32(woz_data + WOZ_CRC_OFFSET);
    if (stored_crc != 0) {
        uint32_t computed_crc = woz_crc32(woz_data + WOZ_HEADER_SIZE, woz_size - WOZ_HEADER_SIZE);
        if (computed_crc != stored_crc) {
            debug_print("CRC mismatch (stored=%08x; computed=%08x)\n", stored_crc, computed_crc);
            return 0;
        }
    }
    return 1;
}

static int woz_parse_info(const uint8_t *chunk, uint32_t chunk_size)
{
    if (chunk_size != INFO_CHUNK_SIZE) {
        debug_print("INFO chunk size is not %d but %u bytes\n", INFO_CHUNK_SIZE, chunk_size);
        return 0;
    }
    if (chunk[INFO_DISK_TYPE_OFFSET] != INFO_DISK_TYPE_525) {
        debug_print("Only 5.25\" disks are supported\n");
        return 0;
    }
    if (chunk[INFO_VERSION_OFFSET] >= 2 && chunk[INFO_DISK_SIDES_OFFSET] != 1) {
        debug_print("Only 1-sided images are supported\n");
        return 0;
    }
    debug_print("INFO: version %d, creator: %.*s\n", chunk[INFO_VERSION_OFFSET],
                INFO_CREATOR_LENGTH, (const char *)chunk + INFO_CREATOR_OFFSET);
    return 1;
}

/* Only whole tracks are of interest: every fourth quarter track. */
static int woz_parse_tmap(const uint8_t *chunk, uint32_t chunk_size, WozImage *image)
{
    if (chunk_size != MAX_QUARTER_TRACKS) {
        debug_print("TMAP chunk size is not %d but %u bytes\n", MAX_QUARTER_TRACKS, chunk_size);
        return 0;
    }
    for (int i = 0; i < WOZ_NIB_TRACKS; i++)
        image->tmap[i] = chunk[i * 4];
    return 1;
}

/* WOZ 1.x: fixed-size track records laid out back-to-back in the chunk; the
 * bitstream sits at the start of each record, with the bit count in the
 * trailer. A chunk may hold fewer records than there are quarter tracks. */
static int woz_parse_trks1(const uint8_t *chunk, uint32_t chunk_size, size_t chunk_offset, WozImage *image)
{
    memset(image->trks, 0, sizeof image->trks);
    size_t records = chunk_size / WOZ1_TRACK_RECORD_SIZE;
    for (size_t i = 0; i < records && i < MAX_QUARTER_TRACKS; i++) {
        size_t rec_offset = i * WOZ1_TRACK_RECORD_SIZE;
        uint32_t bit_count = READ_LE_UINT16(chunk + rec_offset + WOZ1_BIT_COUNT_OFFSET);
        /* a count that runs into the trailer and the next record is not to be believed */
        if (bit_count > WOZ1_TRACK_DATA_SIZE * 8)
            bit_count = WOZ1_TRACK_DATA_SIZE * 8;
        image->trks[i].data_offset = chunk_offset + rec_offset;
        image->trks[i].bit_count = bit_count;
    }
    return 1;
}

/* WOZ 2.x: an array of 8-byte entries; each gives the track's starting block
 * (absolute, from the start of the file) and bit count. */
static int woz_parse_trks2(const uint8_t *chunk, uint32_t chunk_size, WozImage *image)
{
    if (chunk_size < MAX_QUARTER_TRACKS * WOZ2_TRKS_ENTRY_SIZE) {
        debug_print("TRKS chunk of %u bytes is too small\n", chunk_size);
        return 0;
    }
    for (int i = 0; i < MAX_QUARTER_TRACKS; i++) {
        const uint8_t *entry = chunk + i * WOZ2_TRKS_ENTRY_SIZE;
        image->trks[i].data_offset = (size_t)READ_LE_UINT16(entry) * WOZ_BLOCK_SIZE;
        image->trks[i].bit_count = READ_LE_UINT32(entry + WOZ2_BIT_COUNT_OFFSET);
    }
    return 1;
}

/* Walk the chunks, which are read in place. The layout of TRKS goes by the
 * version in the magic number. Of two chunks of a kind the later one counts;
 * WRIT, META and chunks not known are skipped, as are bytes after the last
 * chunk that are too few to be another. */
static int woz_parse_chunks(const uint8_t *woz_data, size_t woz_size, WozImage *image)
{
    int is_woz2 = (woz_data[3] == '2');
    size_t read_pos = WOZ_HEADER_SIZE;

    memset(image, 0, sizeof *image);

    while (woz_size - read_pos >= WOZ_CHUNK_HEADER_SIZE) {
        const uint8_t *id = woz_data + read_pos;
        uint32_t chunk_size = READ_LE_UINT32(id + 4);
        read_pos += WOZ_CHUNK_HEADER_SIZE;
        if (chunk_size > woz_size - read_pos) {
            debug_print("chunk %.4s size %u extends beyond end of file\n", (const char *)id, chunk_size);
            return 0;
        }
        const uint8_t *chunk = woz_data + read_pos;
        int ok = 1;

        if (memcmp(id, "INFO", 4) == 0) {
            if (image->has_info)
                debug_print("more than one INFO chunk\n");
            image->has_info = 1;
            ok = woz_parse_info(chunk, chunk_size);
        } else if (memcmp(id, "TMAP", 4) == 0) {
            if (image->has_tmap)
                debug_print("more than one TMAP chunk\n");
            image->has_tmap = 1;
            ok = woz_parse_tmap(chunk, chunk_size, image);
        } else if (memcmp(id, "TRKS", 4) == 0) {
            if (image->has_trks)
                debug_print("more than one TRKS chunk\n");
            image->has_trks = 1;
            ok = is_woz2 ? woz_parse_trks2(chunk, chunk_size, image)
                         : woz_parse_trks1(chunk, chunk_size, read_pos, image);
        } else if (memcmp(id, "META", 4) == 0) {
            debug_print("META: %u bytes\n%.*s\n", chunk_size, (int)(chunk_size & 0xffff), (const char *)chunk);
        } else {
            debug_print("ignoring chunk %.4s of %u bytes\n", (const char *)id, chunk_size);
        }
        if (!ok)
            return 0;
        read_pos += chunk_size;
    }

    if (!image->has_info || !image->has_tmap || !image->has_trks) {
        debug_print("missing required chunk (INFO=%d TMAP=%d TRKS=%d)\n", image->has_info, image->has_tmap, image->has_trks);
        return 0;
    }
    return 1;
}

/*
 * Sync byte patterns that precede sector address fields.
 * These are sequences of self-sync bytes (10-bit patterns that always
 * produce valid nibble bytes regardless of bit alignment). The 16-sector
 * (DOS 3.3) pattern ends two bits into its seventh byte; the 13-sector
 * (DOS 3.2) pattern is nine whole bytes. Their second bytes differ, so no
 * run of bits matches both.
 */

// clang-format off
static const uint8_t sync_pattern_16[6] = {
    0xff, //    11111111
    0x3f, //    00111111
    0xcf, //    11001111
    0xf3, //    11110011
    0xfc, //    11111100
    0xff, //    11111111
};        //    00           the top two bits of the byte that follows
// clang-format on

#define SYNC16_TAIL_MASK 0xc0
#define SYNC16_TAIL_BITS 2

static const uint8_t sync_pattern_13[9] = {
    0xff, //    11111111
    0x7f, //    01111111
    0xbf, //    10111111
    0xdf, //    11011111
    0xef, //    11101111
    0xf7, //    11110111
    0xfb, //    11111011
    0xfd, //    11111101
    0xfe, //    11111110
};

/* Pattern lengths in bits, to step past the pattern */
#define SYNC16_BIT_LENGTH (8 * (uint32_t)sizeof sync_pattern_16 + SYNC16_TAIL_BITS)
#define SYNC13_BIT_LENGTH (8 * (uint32_t)sizeof sync_pattern_13)

/* Bytes that a match looks at, the longer of the two patterns */
#define SYNC_WINDOW sizeof sync_pattern_13

/* The length in bits of the sync pattern that the SYNC_WINDOW bytes at p
 * begin with, or 0 if they begin with neither. */
static uint32_t match_sync_pattern(const uint8_t *p)
{
    if (memcmp(p, sync_pattern_13, sizeof sync_pattern_13) == 0)
        return SYNC13_BIT_LENGTH;
    if (memcmp(p, sync_pattern_16, sizeof sync_pattern_16) == 0 &&
        (p[sizeof sync_pattern_16] & SYNC16_TAIL_MASK) == 0)
        return SYNC16_BIT_LENGTH;
    return 0;
}

/*
 * Search the track bitstream for a sync byte pattern and return the position
 * of the bit that follows it, or 0 if there is none.
 *
 * First tries byte-aligned matches (fast path), then falls back to
 * checking every possible bit offset (slow path). The search stops short of
 * the end of the track: a pattern that straddles the splice is not found,
 * which leaves the decoder to start at bit 0 and sync itself.
 */
static uint32_t find_sync_end(const uint8_t *bitstream, uint32_t bit_count)
{
    size_t total_bytes = bit_count / 8;
    if (total_bytes <= SYNC_WINDOW)
        return 0;
    /* the deepest byte read below is bitstream[i + SYNC_WINDOW] */
    size_t search_limit = total_bytes - SYNC_WINDOW;
    uint32_t length;

    // Both sync byte sequences begin with 0xff, so first we do
    // a simple loop looking for that, without shifting any bits.
    // This is enough to find what we're looking for
    // in the majority of cases.
    for (size_t i = 0; i < search_limit; i++) {
        if (bitstream[i] == 0xff && (length = match_sync_pattern(&bitstream[i])) != 0)
            return (uint32_t)((i * 8 + length) % bit_count);
    }

    uint8_t shifted_bytes[SYNC_WINDOW];
    // Otherwise we will have to look harder, for byte pairs that can be left shifted to 0xff
    for (size_t i = 0; i < search_limit; i++) {
        // Skip any bytes where the rightmost bit is unset
        // or the next byte has the leftmost bit unset
        if ((bitstream[i] & 0x01) == 0 || (bitstream[i + 1] & 0x80) == 0)
            continue;
        for (unsigned shift = 1; shift < 8; shift++) {
            // Check if this byte left-shifted (together with the following byte)
            // shift places becomes 0xff
            if ((uint8_t)((bitstream[i] << shift) | (bitstream[i + 1] >> (8 - shift))) != 0xff)
                continue;
            // If so, copy the bytes from here to a buffer left-shifted by shift
            // positions, and compare them to the sync-byte sequences.
            for (size_t k = 0; k < SYNC_WINDOW; k++)
                shifted_bytes[k] = (uint8_t)((bitstream[i + k] << shift) | (bitstream[i + k + 1] >> (8 - shift)));
            if ((length = match_sync_pattern(shifted_bytes)) != 0)
                return (uint32_t)((i * 8 + shift + length) % bit_count);
        }
    }
    return 0;
}

/* Number of leading zero bits in v, which must not be zero.
 *
 * Deliberately a loop and not __builtin_clzll: it runs only for the ~6% of
 * nibbles preceded by a gap, nearly always for one or two turns, and measured
 * marginally faster than the builtin (see commit 8a2d1dbbf). */
static int woz_clz64(uint64_t v)
{
    int n = 0;
    while ((v & 0x8000000000000000ULL) == 0) {
        v <<= 1;
        n++;
    }
    return n;
}

/* The fast path of the decoder takes the bitstream 64 bits at a time. Shifted
 * to start at any bit of its first byte, such a window has 57 good bits: a
 * nibble preceded by up to 48 zero bits is wholly inside it, and if 49 or
 * more are zero, 49 are known to be and can be stepped over. */
#define WOZ_WINDOW_BITS 64
#define WOZ_LATCH_GAP_BITS 49

/* The decoder gives up on a track after reading this many times around it. */
#define WOZ_GUARD_REVOLUTIONS 3

/*
 * Decode up to WOZ_NIB_TRACK_LEN nibbles from the bit_count bits at src into
 * out, beginning at start_bit, and return how many there were.
 *
 * Decode with a soft data latch (Apple II controller model): shift bits
 * into an 8-bit register, emitting a nibble when the high bit is set.
 * The bitstream is read as a true circle starting at the sync gap, so
 * the final sector is never split or zero-padded at the track boundary
 * -- which is what ailed tightly-formatted disks such as Talisman. When
 * no sync is found, decoding from bit 0 still recovers the track because
 * the latch self-syncs at the first gap it meets. The emitted run is
 * continuous, so any nibbles past one physical revolution simply repeat
 * the start; FindRevolutionLength() in ciderpress.c recovers the true
 * revolution length from that duplication so a sector whose data field
 * wraps the splice rejoins.
 *
 * Fewer than WOZ_NIB_TRACK_LEN nibbles come out only of a track with next to
 * nothing on it, when the guard stops the reading.
 */
static size_t woz_decode_track(const uint8_t *src, uint32_t bit_count, uint32_t start_bit, uint8_t *out)
{
    size_t nibble_count = 0;
    int reg = 0;
    uint32_t p = start_bit;
    uint64_t guard = 0, guard_limit = (uint64_t)bit_count * WOZ_GUARD_REVOLUTIONS;

    while (nibble_count < WOZ_NIB_TRACK_LEN && guard < guard_limit) {
        /* Fast path for the common case: with the latch empty, the next
         * nibble is simply the eight bits starting at the next set bit, so
         * a whole gap-plus-nibble fits in one 64-bit window. Taking it in
         * one go beats shifting bit by bit -- that inner branch fires at
         * data-dependent positions and mispredicts constantly, and it
         * dominated the conversion time.
         * Requires the window to lie wholly inside the track, which keeps
         * the load itself within the bitstream. Everything else -- a partly
         * filled latch, and the wrap at the end of the circular read --
         * falls through to the bit-at-a-time loop below.
         * A step here ends inside the window, so it can at most leave p at
         * bit_count, and p is not wrapped here: with the test for the wrap
         * on the way from one nibble to the next, the decode took twice as
         * long. The loop below wraps p before it reads. */
        if (reg == 0 && bit_count - p >= WOZ_WINDOW_BITS) {
            /* Gather the window MSB-first, which is the order the bits come
             * off the disk. Spelling it out keeps this independent of host
             * byte order; clang makes one load and a byte swap of it. */
            const uint8_t *q = src + (p >> 3);
            uint64_t window = ((uint64_t)q[0] << 56) | ((uint64_t)q[1] << 48) |
                              ((uint64_t)q[2] << 40) | ((uint64_t)q[3] << 32) |
                              ((uint64_t)q[4] << 24) | ((uint64_t)q[5] << 16) |
                              ((uint64_t)q[6] << 8) | (uint64_t)q[7];
            window <<= (p & 7);
            /* A data field runs its nibbles back to back, so the next bit
             * is usually already the top of the next nibble: measured over
             * a 130-image corpus, skip is 0 for 93.7% of nibbles. Testing
             * for that one case first keeps the overwhelmingly common path
             * off the count-leading-zeros dependency chain, which runs from
             * the window load all the way to the store. Peeling off the
             * next-commonest widths (1 and 2 bits, another 6%) as well was
             * tried and measured slower -- the extra tests cost more than
             * the count they replace. */
            uint32_t skip = 0;
            if ((window >> 63) == 0) {
                if (window == 0 || (skip = (uint32_t)woz_clz64(window)) >= WOZ_LATCH_GAP_BITS) {
                    /* A long gap: its zero bits leave the latch empty, so
                     * step over the ones we know are zero and look again
                     * from there. */
                    p += WOZ_LATCH_GAP_BITS;
                    guard += WOZ_LATCH_GAP_BITS;
                    continue;
                }
            }
            out[nibble_count++] = (uint8_t)(window >> (56 - skip));
            p += skip + 8;
            guard += skip + 8;
            continue;
        }
        if (p >= bit_count)
            p -= bit_count;
        int bit = (src[p >> 3] >> (7 - (p & 7))) & 1;
        reg = ((reg << 1) | bit) & 0xff;
        if (reg & 0x80) {
            out[nibble_count++] = (uint8_t)reg;
            reg = 0;
        }
        p++;
        guard++;
    }
    return nibble_count;
}

/* Convert one whole track into the WOZ_NIB_TRACK_LEN zero bytes at out, which
 * stay as they are if the track is one that cannot be used. */
static void woz_convert_track(const uint8_t *woz_data, size_t woz_size, const WozImage *image, int track, uint8_t *out)
{
    int trks_index = image->tmap[track];
    if (trks_index == TMAP_EMPTY_TRACK) {
        debug_print("T[%02x]: empty track\n", track);
        return;
    }
    if (trks_index >= MAX_QUARTER_TRACKS) {
        debug_print("T[%02x]: bad TMAP value %d\n", track, trks_index);
        return;
    }

    const WozTrack *trk = &image->trks[trks_index];
    uint32_t bit_count = trk->bit_count;
    size_t stream_bytes = (size_t)bit_count / 8 + (bit_count % 8 != 0);  // bytes spanned by the valid bits

    /* A track too short to hold even the sync pattern can't yield sectors:
     * this is also every TRKS entry that nothing was written to. */
    if (bit_count <= SYNC13_BIT_LENGTH) {
        debug_print("T[%02x]: track %d too short (%u bits)\n", track, trks_index, bit_count);
        return;
    }
    if (trk->data_offset > woz_size || stream_bytes > woz_size - trk->data_offset) {
        debug_print("T[%02x]: bitstream (offset %zu, %zu bytes) extends beyond end of file (%zu)\n",
                    track, trk->data_offset, stream_bytes, woz_size);
        return;
    }

    /* Begin after a sync gap, at a sector boundary, which keeps marginal
     * first sectors readable. */
    const uint8_t *src = woz_data + trk->data_offset;
    size_t nibble_count = woz_decode_track(src, bit_count, find_sync_end(src, bit_count), out);
    if (nibble_count < WOZ_NIB_TRACK_LEN)
        debug_print("T[%02x]: only %zu nibbles decoded\n", track, nibble_count);
}

uint8_t *woz2nib(const uint8_t *woz_data, size_t *data_size)
{
    size_t woz_size = *data_size;
    WozImage image;

    if (!woz_check_header(woz_data, woz_size) || !woz_parse_chunks(woz_data, woz_size, &image))
        return NULL;

    uint8_t *nib = calloc(WOZ_NIB_IMAGE_SIZE, 1);
    if (nib == NULL)
        return NULL;

    for (int track = 0; track < WOZ_NIB_TRACKS; track++)
        woz_convert_track(woz_data, woz_size, &image, track, nib + (size_t)track * WOZ_NIB_TRACK_LEN);

    *data_size = WOZ_NIB_IMAGE_SIZE;
    return nib;
}
