//
//  ciderpress.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//  This is based on parts of the Ciderpress source.
//  See https://github.com/fadden/ciderpress for the full version.
//
//  Includes fragments from a2tools by Terry Kyriacopoulos and Paul Schlyter
//

#ifndef ciderpress_h
#define ciderpress_h

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * There is one current disk image. It is the caller's buffer, which is only
 * read and has to stay alive until FreeDiskImage() or the next image.
 *
 * InitNibImage() and InitDskImage() say what format the buffer is in: a .nib
 * of 6656 bytes for each of 35 tracks, or the 143360 bytes of a .dsk in DOS
 * 3.3 sector order. The functions below that take an image can be called
 * without one of these first, and then assume the format given with each;
 * called with the image that was announced, they go by what was announced.
 *
 * FreeDiskImage() forgets the image and frees what was allocated for it.
 * It does not free the image, or anything that was returned to the caller.
 */
void InitNibImage(uint8_t *data, size_t extent);
void InitDskImage(uint8_t *data, size_t extent);
void FreeDiskImage(void);

typedef struct {
    char *filename;
    uint8_t *data;
    size_t datasize;
    size_t index;
} A2FileRec;

/*
 * Read size bytes at offset of the DOS 3.3 ordered disk in a .nib. Returns a
 * buffer that the caller must free, or NULL if a sector could not be read.
 */
uint8_t *ReadImageFromNib(size_t offset, size_t size, uint8_t *data, size_t datasize);

/*
 * Decode a .nib into a .dsk: 143360 bytes in DOS 3.3 sector order, which the
 * caller must free. Sectors that could not be read are zero-filled. The .nib
 * is the current image afterwards.
 */
uint8_t *ReadFullImageFromNib(uint8_t *data, size_t datasize);

/*
 * Read a Scott Adams SAGA game from a DOS 3.3 disk (a .dsk unless announced
 * as a .nib). On entry *len is the size of the image. Returns the database
 * file and sets *len to its size, or returns NULL and leaves *len alone.
 *
 * Also looks for the inventory image in PAK.INVEN or PAC.INVEN (*invimg, of
 * *invimglen bytes: the file's sectors without its four bytes of address and
 * length, so longer than the picture), the 0x182 bytes of image unscrambling
 * tables in M2 (*m2) and the Graphics Magician drawing data in
 * M3 (*m3, *m3len), and the kA2CloseupTableSize bytes of the table of close-up
 * pictures (*closeups): the verb, then sixteen items, sixteen pictures and
 * sixteen nouns. Each is only written to if it is found, so that a disk
 * without one keeps what its companion disk had: set them to NULL beforehand.
 *
 * The caller must free everything that is returned.
 */
enum { kA2CloseupTableSize = 1 + 3 * 16 };
uint8_t *ReadApple2DOSFile(uint8_t *data, size_t *len, uint8_t **invimg, size_t *invimglen, uint8_t **m2, uint8_t **m3, size_t *m3len, uint8_t **closeups);

/*
 * Extract every file of a DOS 3.3 disk (a .dsk unless announced as a .nib),
 * except for those of no sectors. A file's data is all of its sectors: a
 * binary file starts with its four bytes of address and length.
 *
 * Returns an array of *number_of_files records, or NULL if there are none.
 * The caller must free each record's filename and data, then the array.
 */
A2FileRec *GetAllApple2DOSFiles(uint8_t *data, size_t len, size_t *number_of_files);

/*
 * Extract every ProDOS BIN file from a .dsk (DOS sector order), used by
 * the Apple II Comprehend games whose game-file directory is unlinked from the
 * volume root. Same ownership contract as GetAllApple2DOSFiles. The image
 * is the current image afterwards.
 */
A2FileRec *GetAllProDOSFiles(uint8_t *data, size_t len, size_t *number_of_files);

/*
 * Read the game file, such as ZORK0.D1, from an Infocom V6 ProDOS disk: a
 * .nib if *len is the size of one or it was announced as one, and otherwise
 * a sector image in DOS or ProDOS order. The title and the number of the disk
 * come from the name of the volume.
 *
 * Returns a buffer of *len bytes that the caller must free and sets *game
 * (0 to 3 for Zork Zero, Shogun, Journey, Arthur) and *diskindex (1 to 5),
 * either of which may be NULL. Returns NULL and sets *len to 0 on failure.
 * There is no current image afterwards.
 */
uint8_t *ReadInfocomV6File(uint8_t *data, size_t *len, int *game, int *diskindex);

#ifdef __cplusplus
}
#endif

#endif /* ciderpress_h */
