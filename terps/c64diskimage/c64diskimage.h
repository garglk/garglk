/*
 https://paradroid.automac.se/diskimage/

 Copyright (c) 2003-2006, Per Olofsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 Cut down to a reader of whole files for Spatterlight: the write path, the
 file streams and the error info of the original are gone, and every track
 and sector link is checked before it is followed.
 */

#ifndef c64diskimage_h
#define c64diskimage_h

#include <stddef.h>
#include <stdint.h>

/* A D64, D71 or D81 disk image. D64 images may have 40 tracks, and all three
   may be followed by error info, which is ignored. */
typedef struct diskimage DiskImage;

/* Returns NULL if length is not the size of a disk image. The DiskImage
   does not copy data and does not own it: data must outlive it. Release it
   with di_free(), which leaves data alone. */
DiskImage *di_create_from_data(const uint8_t *data, size_t length);
void di_free(DiskImage *di);

/* Returns the names of the files of the disk as a NULL-terminated array of
   *numfiles strings, in directory order. The caller frees the strings and
   the array. Scratched entries and entries without a name are left out.
   Returns NULL and sets *numfiles to 0 if there are none. */
char **di_get_all_file_names(const DiskImage *di, int *numfiles);

/* Each of the functions below returns the contents of a file in a new buffer
   that the caller frees, and sets *length to its size. An empty file gives a
   buffer too. They return NULL and set *length to 0 if there is no such
   file, if its first sector is off the disk or links off the disk, or if
   memory runs out. A file whose chain of sectors goes wrong further on (a
   link off the disk, or back to a sector already read) ends there. */

/* The first file whose name matches name, in which ? matches any character
   and * the rest of the name. No file matches a name of more than 16
   characters. */
uint8_t *di_read_file_named(const DiskImage *di, const char *name,
    size_t *length);

/* The same for a disk image in memory. These return NULL also if data is not
   a disk image. */
uint8_t *di_get_file_named(const uint8_t *data, size_t length,
    size_t *newlength, const char *name);

/* The file with the most blocks according to the directory; the first of
   them if several have. */
uint8_t *di_get_largest_file(const uint8_t *data, size_t length,
    size_t *newlength);

/* The chain of sectors starting at track and sector, for a file that the
   directory does not list. */
uint8_t *di_get_file_at_ts(const uint8_t *data, size_t length,
    size_t *newlength, int track, int sector);

#endif /* c64diskimage_h */
