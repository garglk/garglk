//
//  woz2nib.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-09.
//  Based on woz2dsk.pl by LEE Sau Dan <leesaudan@gmail.com>
//

#ifndef woz2nib_h
#define woz2nib_h

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The .nib that woz2nib() makes, which is also the one ciderpress.c reads */
#define WOZ_NIB_TRACKS 35
#define WOZ_NIB_TRACK_LEN 6656
#define WOZ_NIB_IMAGE_SIZE (WOZ_NIB_TRACKS * WOZ_NIB_TRACK_LEN)

/* Whether the size bytes at data begin as a WOZ 1.x or 2.x file does. */
int is_woz_image(const uint8_t *data, size_t size);

/*
 * Convert a WOZ disk image (1.x or 2.x) to raw nibble format.
 *
 * On entry *data_size is the size of the file at woz_data, which is only
 * read. Returns a buffer of WOZ_NIB_IMAGE_SIZE bytes that the caller must
 * free, and sets *data_size to that; or returns NULL and leaves *data_size
 * alone if this is not a WOZ of a single-sided 5.25" disk, if its checksum
 * is wrong or if there is no memory.
 *
 * Only the 35 whole tracks are converted; quarter tracks and tracks past the
 * 35th are ignored. A track that is missing or unusable is all zeros. A track
 * with fewer than WOZ_NIB_TRACK_LEN nibbles on it is read around again to
 * fill the space, so its end repeats its beginning.
 */
uint8_t *woz2nib(const uint8_t *woz_data, size_t *data_size);

#ifdef __cplusplus
}
#endif

#endif /* woz2nib_h */
