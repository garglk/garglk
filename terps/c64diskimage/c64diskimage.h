/*
https://paradroid.automac.se/diskimage/

Copyright (c) 2003-2006, Per Olofsson
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef diskimage_h
#define diskimage_h

#include <stdlib.h>
#include <stdint.h>

/* constants for the supported disk formats */
#define MAXTRACKS 80
#define MAXSECTORS 40

#define D64SIZE 174848
#define D64ERRSIZE 175531
#define D71SIZE 349696
#define D71ERRSIZE 351062
#define D81SIZE 819200
#define D81ERRSIZE 822400

typedef enum imagetype { D64 = 1, D71, D81 } ImageType;

typedef enum filetype {
  T_DEL = 0,
  T_SEQ,
  T_PRG = 0xc2,
  T_USR,
  T_REL,
  T_CBM,
  T_DIR
} FileType;

typedef struct ts {
  unsigned char track;
  unsigned char sector;
} TrackSector;

typedef struct diskimage {
  char *filename;
  int size;
  ImageType type;
  unsigned char *image;
  unsigned char *errinfo;
  TrackSector bam;
  TrackSector bam2;
  TrackSector dir;
  int openfiles;
  int blocksfree;
  int modified;
  int status;
  int interleave;
  TrackSector statusts;
} DiskImage;

typedef struct rawdirentry {
  TrackSector nextts;
  unsigned char type;
  TrackSector startts;
  unsigned char rawname[16];
  TrackSector relsidets;
  unsigned char relrecsize;
  unsigned char unused[4];
  TrackSector replacetemp;
  unsigned char sizelo;
  unsigned char sizehi;
} RawDirEntry;

typedef struct imagefile {
  DiskImage *diskimage;
  RawDirEntry *rawdirentry;
  char mode;
  int position;
  TrackSector ts;
  TrackSector nextts;
  unsigned char *buffer;
  int bufptr;
  int buflen;
  unsigned char visited[MAXTRACKS][MAXSECTORS];
} ImageFile;

ImageFile *di_open(DiskImage *di, unsigned char *rawname, FileType type,
                   char *mode);
ImageFile *di_create_file_from_ts(DiskImage *di, int track, int sector);
int di_read(ImageFile *imgfile, unsigned char *buffer, int len);
int di_rawname_from_name(unsigned char *rawname, const char *name);

RawDirEntry *find_largest_file_entry(DiskImage *di);
DiskImage *di_create_from_data(uint8_t *data, int length);

/* returns an array of strings containing all file names */
char **get_all_file_names(DiskImage *di, int *numfiles);

#endif /* diskimage_h */
