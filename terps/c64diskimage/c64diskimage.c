/*
 https://paradroid.automac.se/diskimage/

 Copyright (c) 2003-2006, Per Olofsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "c64diskimage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct errormessage {
    signed int number;
    const char *string;
} ErrorMessage;

ErrorMessage error_msg[] = {
    /* non-errors */
    { 0, "ok" },
    { 1, "files scratched" },
    { 2, "partition selected" },
    /* errors */
    { 20, "read error (block header not found)" },
    { 21, "read error (drive not ready)" },
    { 22, "read error (data block not found)" },
    { 23, "read error (crc error in data block)" },
    { 24, "read error (byte sector header)" },
    { 25, "write error (write-verify error)" },
    { 26, "write protect on" },
    { 27, "read error (crc error in header)" },
    { 30, "syntax error (general syntax)" },
    { 31, "syntax error (invalid command)" },
    { 32, "syntax error (long line)" },
    { 33, "syntax error (invalid file name)" },
    { 34, "syntax error (no file given)" },
    { 39, "syntax error (invalid command)" },
    { 50, "record not present" },
    { 51, "overflow in record" },
    { 52, "file too large" },
    { 60, "write file open" },
    { 61, "file not open" },
    { 62, "file not found" },
    { 63, "file exists" },
    { 64, "file type mismatch" },
    { 65, "no block" },
    { 66, "illegal track or sector" },
    { 67, "illegal system t or s" },
    { 70, "no channel" },
    { 71, "directory error" },
    { 72, "disk full" },
    { 73, "dos mismatch" },
    { 74, "drive not ready" },
    { 75, "format error" },
    { 76, "controller error" },
    { 77, "selected partition illegal" },
    { -1, NULL }
};

/* dos errors as used by the DOS internally (and as saves in the error info) */
typedef struct doserror {
    signed int number; /* dos error number */
    signed int errnum; /* reported error number */
    char *string; /* description */
} DosError;

DosError dos_error[] = {
    /* non-errors */
    { 0x01, 0, "ok" },
    /* errors */
    { 0x02, 20, "Header descriptor byte not found (Seek)" },
    /*  { 0x02, 20, "Header block not found (Seek)" }, */
    { 0x03, 21, "No SYNC sequence found (Seek)" },
    { 0x04, 22, "Data descriptor byte not found (Read)" },
    { 0x05, 23, "Checksum error in data block (Read)" },
    { 0x06, 24, "Write verify (Write)" },
    { 0x07, 25, "Write verify error (Write)" },
    { 0x08, 26, "Write protect on (Write)" },
    { 0x09, 27, "Checksum error in header block (Seek)" },
    { 0x0A, 28, "Write error (Write)" },
    { 0x0B, 29, "Disk sector ID mismatch (Seek)" },
    { 0x0F, 74, "Drive Not Ready (Read)" },
    { -1, -1, NULL }
};

/* convert to rawname */
int di_rawname_from_name(unsigned char *rawname, const char *name)
{
    int i;

    memset(rawname, 0xa0, 16);
    for (i = 0; i < 16 && name[i]; ++i)
        rawname[i] = name[i];
    return i;
}

/* convert from rawname */
int di_name_from_rawname(char *name, unsigned char *rawname)
{
    int i;

    for (i = 0; i < 16 && rawname[i] != 0xa0; ++i)
        name[i] = rawname[i];
    name[i] = 0;
    return i;
}

static int set_status(DiskImage *di, int status, int track, int sector)
{
    di->status = status;
    di->statusts.track = track;
    di->statusts.sector = sector;
    return status;
}

/* return write interleave */
static int interleave(ImageType type)
{
    switch (type) {
    case D64:
        return 10;
        break;
    case D71:
        return 6;
        break;
    default:
        return 1;
        break;
    }
}

/* return number of tracks for image type */
static int di_tracks(ImageType type)
{
    switch (type) {
    case D64:
        return 35;
        break;
    case D71:
        return 70;
        break;
    case D81:
        return 80;
        break;
    }
    return 0;
}

/* return disk geometry for track */
static int di_sectors_per_track(ImageType type, int track)
{
    switch (type) {
    case D71:
        if (track > 35) {
            track -= 35;
        }
        /* fall through */
    case D64:
        if (track < 18) {
            return 21;
        } else if (track < 25) {
            return 19;
        } else if (track < 31) {
            return 18;
        } else {
            return 17;
        }
        break;
    case D81:
        return 40;
        break;
    }
    return 0;
}

/* check if given track/sector is within valid range */
static int di_ts_is_valid(ImageType type, TrackSector ts)
{
    if ((ts.track < 1) || (ts.track > di_tracks(type))) {
        return 0; /* track out of range */
    }
    if (ts.sector > (di_sectors_per_track(type, ts.track) - 1)) {
        return 0; /* sector out of range */
    }
    return 1;
}

/* convert track, sector to blocknum */
static int di_get_block_num(ImageType type, TrackSector ts)
{
    int block;

    /* assertion, should never happen (indicates bad error handling elsewhere) */
    if (!di_ts_is_valid(type, ts)) {
        fprintf(stderr, "%s:%s:%d internal error, track/sector out of range\n",
            __FILE__, __FUNCTION__, __LINE__);
        exit(-1);
    }

    switch (type) {
    case D64:
        if (ts.track < 18) {
            block = (ts.track - 1) * 21;
        } else if (ts.track < 25) {
            block = (ts.track - 18) * 19 + 17 * 21;
        } else if (ts.track < 31) {
            block = (ts.track - 25) * 18 + 17 * 21 + 7 * 19;
        } else {
            block = (ts.track - 31) * 17 + 17 * 21 + 7 * 19 + 6 * 18;
        }
        return block + ts.sector;
        break;
    case D71:
        if (ts.track > 35) {
            block = 683;
            ts.track -= 35;
        } else {
            block = 0;
        }
        if (ts.track < 18) {
            block += (ts.track - 1) * 21;
        } else if (ts.track < 25) {
            block += (ts.track - 18) * 19 + 17 * 21;
        } else if (ts.track < 31) {
            block += (ts.track - 25) * 18 + 17 * 21 + 7 * 19;
        } else {
            block += (ts.track - 31) * 17 + 17 * 21 + 7 * 19 + 6 * 18;
        }
        return block + ts.sector;
        break;
    case D81:
        return (ts.track - 1) * 40 + ts.sector;
        break;
    }
    return 0;
}

/* get a pointer to block data */
static unsigned char *di_get_ts_addr(DiskImage *di, TrackSector ts)
{
    return di->image + di_get_block_num(di->type, ts) * 256;
}

/* get error info for a sector */
static int get_ts_doserr(DiskImage *di, TrackSector ts)
{
    if (di->errinfo == NULL) {
        return 1; /* return OK if image has no error info */
    }

    return di->errinfo[di_get_block_num(di->type, ts)];
}

/* get status info for a sector */
static int di_get_ts_err(DiskImage *di, TrackSector ts)
{
    int errnum;
    DosError *err = dos_error;

    errnum = get_ts_doserr(di, ts);
    while (err->number >= 0) {
        if (errnum == err->number) {
            return err->errnum;
        }
        ++err;
    }
    return -1; /* unknown error */
}

/* return a pointer to the next block in the chain */
static TrackSector next_ts_in_chain(DiskImage *di, TrackSector ts)
{
    unsigned char *p;
    TrackSector newts;

    p = di_get_ts_addr(di, ts);
    newts.track = p[0];
    newts.sector = p[1];

    return newts;
}

/* return t/s of first directory sector */
static TrackSector di_get_dir_ts(DiskImage *di)
{
    TrackSector newts;
    unsigned char *p;

    p = di_get_ts_addr(di, di->dir);
    if ((di->type == D64) || (di->type == D71)) {
        newts.track = 18; /* 1541/71 ignores bam t/s link */
        newts.sector = 1;
    } else {
        newts.track = p[0];
        newts.sector = p[1];
    }

    return newts;
}

/* return number of free blocks in track */
static int di_track_blocks_free(DiskImage *di, int track)
{
    unsigned char *bam;

    switch (di->type) {
    default:
    case D64:
        bam = di_get_ts_addr(di, di->bam);
        break;
    case D71:
        bam = di_get_ts_addr(di, di->bam);
        if (track >= 36) {
            return bam[track + 185];
        }
        break;
    case D81:
        if (track <= 40) {
            bam = di_get_ts_addr(di, di->bam);
        } else {
            bam = di_get_ts_addr(di, di->bam2);
            track -= 40;
        }
        return bam[track * 6 + 10];
        break;
    }
    return bam[track * 4];
}

/* count number of free blocks */
static int blocks_free(DiskImage *di)
{
    int track;
    int blocks = 0;

    for (track = 1; track <= di_tracks(di->type); ++track) {
        if (track != di->dir.track) {
            blocks += di_track_blocks_free(di, track);
        }
    }
    return blocks;
}

/* check if track, sector is free in BAM */
static int di_is_ts_free(DiskImage *di, TrackSector ts)
{
    unsigned char mask;
    unsigned char *bam;

    switch (di->type) {
    case D64:
        bam = di_get_ts_addr(di, di->bam);
        if (bam[ts.track * 4]) {
            mask = 1 << (ts.sector & 7);
            return bam[ts.track * 4 + ts.sector / 8 + 1] & mask ? 1 : 0;
        } else {
            return 0;
        }
        break;
    case D71:
        mask = 1 << (ts.sector & 7);
        if (ts.track < 36) {
            bam = di_get_ts_addr(di, di->bam);
            return bam[ts.track * 4 + ts.sector / 8 + 1] & mask ? 1 : 0;
        } else {
            bam = di_get_ts_addr(di, di->bam2);
            return bam[(ts.track - 35) * 3 + ts.sector / 8 - 3] & mask ? 1 : 0;
        }
        break;
    case D81:
        mask = 1 << (ts.sector & 7);
        if (ts.track < 41) {
            bam = di_get_ts_addr(di, di->bam);
        } else {
            bam = di_get_ts_addr(di, di->bam2);
            ts.track -= 40;
        }
        return bam[ts.track * 6 + ts.sector / 8 + 11] & mask ? 1 : 0;
        break;
    }
    return 0;
}

/* allocate track, sector in BAM */
static void di_alloc_ts(DiskImage *di, TrackSector ts)
{
    unsigned char mask;
    unsigned char *bam;

    di->modified = 1;
    switch (di->type) {
    case D64:
        bam = di_get_ts_addr(di, di->bam);
        bam[ts.track * 4] -= 1;
        mask = 1 << (ts.sector & 7);
        bam[ts.track * 4 + ts.sector / 8 + 1] &= ~mask;
        break;
    case D71:
        mask = 1 << (ts.sector & 7);
        if (ts.track < 36) {
            bam = di_get_ts_addr(di, di->bam);
            bam[ts.track * 4] -= 1;
            bam[ts.track * 4 + ts.sector / 8 + 1] &= ~mask;
        } else {
            bam = di_get_ts_addr(di, di->bam);
            bam[ts.track + 185] -= 1;
            bam = di_get_ts_addr(di, di->bam2);
            bam[(ts.track - 35) * 3 + ts.sector / 8 - 3] &= ~mask;
        }
        break;
    case D81:
        if (ts.track < 41) {
            bam = di_get_ts_addr(di, di->bam);
        } else {
            bam = di_get_ts_addr(di, di->bam2);
            ts.track -= 40;
        }
        bam[ts.track * 6 + 10] -= 1;
        mask = 1 << (ts.sector & 7);
        bam[ts.track * 6 + ts.sector / 8 + 11] &= ~mask;
        break;
    }
}

/* allocate next available directory block */
static TrackSector alloc_next_dir_ts(DiskImage *di)
{
    unsigned char *p;
    int spt;
    TrackSector ts, lastts;

    if (di_track_blocks_free(di, di->bam.track)) {
        lastts.track = di->bam.track;
        lastts.sector = 0;
        if ((di->type == D64) || (di->type == D71)) {
            ts.track = 18;
            ts.sector = 1;
        } else {
            ts = next_ts_in_chain(di, lastts);
        }
        while (ts.track) {
            lastts = ts;
            ts = next_ts_in_chain(di, ts);
        }
        ts.track = lastts.track;
        ts.sector = lastts.sector + 3;
        spt = di_sectors_per_track(di->type, ts.track);
        for (;; ts.sector = (ts.sector + 1) % spt) {
            if (di_is_ts_free(di, ts)) {
                di_alloc_ts(di, ts);
                p = di_get_ts_addr(di, lastts);
                p[0] = ts.track;
                p[1] = ts.sector;
                p = di_get_ts_addr(di, ts);
                memset(p, 0, 256);
                p[1] = 0xff;
                di->modified = 1;
                return ts;
            }
        }
    } else {
        ts.track = 0;
        ts.sector = 0;
        return ts;
    }
}

DiskImage *di_create_from_data(uint8_t *data, int length)
{
    DiskImage *di;

    if (data == NULL)
        return NULL;

    if ((di = malloc(sizeof(*di))) == NULL) {
        return NULL;
    }

    di->size = length;
    di->image = data;

    di->errinfo = NULL;

    /* check image type */
    switch (length) {
    case D64ERRSIZE: /* D64 with error info */
        // di->errinfo = &(di->error_);
        // fallthrough
    case D64SIZE: /* standard D64 */
        di->type = D64;
        di->bam.track = 18;
        di->bam.sector = 0;
        di->dir = di->bam;
        break;

    case D71ERRSIZE: /* D71 with error info */
        di->errinfo = &(di->image[D71SIZE]);
        // fallthrough
    case D71SIZE:
        di->type = D71;
        di->bam.track = 18;
        di->bam.sector = 0;
        di->bam2.track = 53;
        di->bam2.sector = 0;
        di->dir = di->bam;
        break;

    case D81ERRSIZE: /* D81 with error info */
        di->errinfo = &(di->image[D81SIZE]);
        // fallthrough
    case D81SIZE:
        di->type = D81;
        di->bam.track = 40;
        di->bam.sector = 1;
        di->bam2.track = 40;
        di->bam2.sector = 2;
        di->dir.track = 40;
        di->dir.sector = 0;
        break;

    default:
        free(di);
        return NULL;
    }

    di->filename = NULL;
    di->openfiles = 0;
    di->blocksfree = blocks_free(di);
    di->modified = 0;
    di->interleave = interleave(di->type);
    set_status(di, 254, 0, 0);
    return di;
}

static int match_pattern(unsigned char *rawpattern, unsigned char *rawname)
{
    int i;

    for (i = 0; i < 16; ++i) {
        if (rawpattern[i] == '*') {
            return 1;
        }
        if (rawname[i] == 0xa0) {
            if (rawpattern[i] == 0xa0) {
                return 1;
            } else {
                return 0;
            }
        } else {
            if (rawpattern[i] == '?' || rawpattern[i] == rawname[i]) {
            } else {
                return 0;
            }
        }
    }
    return 1;
}

RawDirEntry *find_largest_file_entry(DiskImage *di)
{
    unsigned char *buffer;
    TrackSector ts;
    RawDirEntry *rde, *largest = NULL;
    int offset, largestsize = 0;

    ts = di_get_dir_ts(di);

    while (ts.track) {
        buffer = di_get_ts_addr(di, ts);
        for (offset = 0; offset < 256; offset += 32) {
            rde = (RawDirEntry *)(buffer + offset);
            int size = rde->sizelo + rde->sizehi * 0x100;
            if (size > largestsize) {
                largest = rde;
                largestsize = size;
            }
        }
        /* todo: add sanity checking */
        ts = next_ts_in_chain(di, ts);
    }
    return largest;
}

char **get_all_file_names(DiskImage *di, int *numfiles)
{
    unsigned char *buffer;
    TrackSector ts;
    RawDirEntry *rde;
    int offset;

    char temp_filenames[128][64];
    int filename_index = 0;

    ts = di_get_dir_ts(di);

    while (ts.track) {
        buffer = di_get_ts_addr(di, ts);
        for (offset = 0; offset < 256; offset += 32) {
            rde = (RawDirEntry *)(buffer + offset);
            di_name_from_rawname(temp_filenames[filename_index++], rde->rawname);
        }
        /* todo: add sanity checking */
        ts = next_ts_in_chain(di, ts);
    }
    if (filename_index == 0)
        return NULL;
    char **filenames = malloc((unsigned long)(filename_index + 1) * sizeof(char *));
    for (int i = 0; i < filename_index; i++) {
        size_t len = strlen(temp_filenames[i]);
        if (len == 0) {
            filenames[i] = NULL;
            continue;
        }
        len++;
        filenames[i] = malloc(len);
        memcpy(filenames[i], temp_filenames[i], len);
    }
    filenames[filename_index] = NULL;
    *numfiles = filename_index;
    return filenames;
}

static RawDirEntry *find_file_entry(DiskImage *di, unsigned char *rawpattern)
{
    unsigned char *buffer;
    TrackSector ts;
    RawDirEntry *rde;
    int offset;

    ts = di_get_dir_ts(di);

    while (ts.track) {
        buffer = di_get_ts_addr(di, ts);
        for (offset = 0; offset < 256; offset += 32) {
            rde = (RawDirEntry *)(buffer + offset);
            if (match_pattern(rawpattern, rde->rawname)) {
                return rde;
            }
        }
        /* todo: add sanity checking */
        ts = next_ts_in_chain(di, ts);
    }
    return NULL;
}

static RawDirEntry *alloc_file_entry(DiskImage *di, unsigned char *rawname,
    FileType type)
{
    unsigned char *buffer;
    TrackSector ts;
    RawDirEntry *rde;
    int offset;

    /* check if file already exists */
    ts = next_ts_in_chain(di, di->bam);
    while (ts.track) {
        buffer = di_get_ts_addr(di, ts);
        for (offset = 0; offset < 256; offset += 32) {
            rde = (RawDirEntry *)(buffer + offset);
            if (rde->type) {
                if (strncmp((char *)rawname, (char *)rde->rawname, 16) == 0) {
                    set_status(di, 63, 0, 0);
                    return NULL;
                }
            }
        }
        ts = next_ts_in_chain(di, ts);
    }

    /* allocate empty slot */
    ts = next_ts_in_chain(di, di->bam);
    while (ts.track) {
        buffer = di_get_ts_addr(di, ts);
        for (offset = 0; offset < 256; offset += 32) {
            rde = (RawDirEntry *)(buffer + offset);
            if (rde->type == 0) {
                memset((unsigned char *)rde + 2, 0, 30);
                memcpy(rde->rawname, rawname, 16);
                rde->type = type;
                di->modified = 1;
                return rde;
            }
        }
        ts = next_ts_in_chain(di, ts);
    }

    /* allocate new dir block */
    ts = alloc_next_dir_ts(di);
    if (ts.track) {
        rde = (RawDirEntry *)di_get_ts_addr(di, ts);
        memset((unsigned char *)rde + 2, 0, 30);
        memcpy(rde->rawname, rawname, 16);
        rde->type = type;
        di->modified = 1;
        return rde;
    } else {
        set_status(di, 72, 0, 0);

        return NULL;
    }
}

/* create a hacked fake file */
ImageFile *di_create_file_from_ts(DiskImage *di, int track, int sector)
{
    ImageFile *imgfile;
    unsigned char *p;

    if ((imgfile = malloc(sizeof(*imgfile))) == NULL) {
        return NULL;
    }

    memset(imgfile->visited, 0, sizeof(imgfile->visited));

    imgfile->mode = 'r';

    imgfile->ts.track = track;
    imgfile->ts.sector = sector;

    p = di_get_ts_addr(di, imgfile->ts);
    imgfile->buffer = p + 2;

    imgfile->nextts = next_ts_in_chain(di, imgfile->ts);

    imgfile->buflen = 254;

    if (!di_ts_is_valid(di->type, imgfile->nextts)) {
        set_status(di, 66, imgfile->nextts.track, imgfile->nextts.sector);
        free(imgfile);
        return NULL;
    }

    imgfile->diskimage = di;
    imgfile->rawdirentry = NULL;
    imgfile->position = 0;
    imgfile->bufptr = 0;

    ++(di->openfiles);
    set_status(di, 0, 0, 0);
    return imgfile;
}


/* open a file */
ImageFile *di_open(DiskImage *di, unsigned char *rawname, FileType type,
    char *mode)
{
    ImageFile *imgfile;
    RawDirEntry *rde;
    unsigned char *p;

    set_status(di, 255, 0, 0);

    if (strcmp("rb", mode) == 0) {

        if ((imgfile = malloc(sizeof(*imgfile))) == NULL) {
            return NULL;
        }

        memset(imgfile->visited, 0, sizeof(imgfile->visited));

        if (strcmp("$", (char *)rawname) == 0) {
            imgfile->mode = 'r';

            imgfile->ts = di->dir;

            p = di_get_ts_addr(di, di->dir);
            imgfile->buffer = p + 2;

            imgfile->nextts = di_get_dir_ts(di);

            imgfile->buflen = 254;

            if (!di_ts_is_valid(di->type, imgfile->nextts)) {
                set_status(di, 66, imgfile->nextts.track, imgfile->nextts.sector);
                free(imgfile);
                return NULL;
            }
            rde = NULL;

        } else {
            if ((rde = find_file_entry(di, rawname)) == NULL) {
                set_status(di, 62, 0, 0);
                free(imgfile);
                return NULL;
            }
            imgfile->mode = 'r';
            imgfile->ts = rde->startts;

            if (!di_ts_is_valid(di->type, imgfile->ts)) {
                set_status(di, 66, imgfile->ts.track, imgfile->ts.sector);
                free(imgfile);
                return NULL;
            }

            p = di_get_ts_addr(di, rde->startts);
            imgfile->buffer = p + 2;
            imgfile->nextts.track = p[0];
            imgfile->nextts.sector = p[1];

            if (imgfile->nextts.track == 0) {
                if (imgfile->nextts.sector != 0) {
                    imgfile->buflen = imgfile->nextts.sector - 1;
                } else {
                    imgfile->buflen = 254;
                }
            } else {
                if (!di_ts_is_valid(di->type, imgfile->nextts)) {
                    set_status(di, 66, imgfile->nextts.track, imgfile->nextts.sector);
                    free(imgfile);
                    return NULL;
                }
                imgfile->buflen = 254;
            }
        }

    } else if (strcmp("wb", mode) == 0) {

        if ((rde = alloc_file_entry(di, rawname, type)) == NULL) {
            return NULL;
        }
        if ((imgfile = malloc(sizeof(*imgfile))) == NULL) {
            return NULL;
        }
        imgfile->mode = 'w';
        imgfile->ts.track = 0;
        imgfile->ts.sector = 0;
        if ((imgfile->buffer = malloc(254)) == NULL) {
            free(imgfile);
            return NULL;
        }
        imgfile->buflen = 254;
        di->modified = 1;

    } else {
        return NULL;
    }

    imgfile->diskimage = di;
    imgfile->rawdirentry = rde;
    imgfile->position = 0;
    imgfile->bufptr = 0;

    ++(di->openfiles);
    set_status(di, 0, 0, 0);
    return imgfile;
}

int di_read(ImageFile *imgfile, unsigned char *buffer, int len)
{
    unsigned char *p;
    int bytesleft;
    int counter = 0;
    int err;

    while (len) {
        bytesleft = imgfile->buflen - imgfile->bufptr;

        err = di_get_ts_err(imgfile->diskimage, imgfile->ts);
        if (err) {
            set_status(imgfile->diskimage, err, imgfile->ts.track,
                imgfile->ts.sector);
            return counter;
        }

        if (bytesleft == 0) {
            if (imgfile->nextts.track == 0) {
                return counter;
            }
            if (((imgfile->diskimage->type == D64) || (imgfile->diskimage->type == D71)) && imgfile->ts.track == 18 && imgfile->ts.sector == 0) {
                imgfile->ts.track = 18;
                imgfile->ts.sector = 1;
            } else {
                imgfile->ts = next_ts_in_chain(imgfile->diskimage, imgfile->ts);
            }
            if (imgfile->ts.track == 0) {
                return counter;
            }

            /* check for cyclic files */
            if (imgfile->visited[imgfile->ts.track][imgfile->ts.sector]) {
                /* return 52, file too long error */
                set_status(imgfile->diskimage, 52, imgfile->ts.track,
                    imgfile->ts.sector);
            } else {
                imgfile->visited[imgfile->ts.track][imgfile->ts.sector] = 1;
            }

            err = di_get_ts_err(imgfile->diskimage, imgfile->ts);
            if (err) {
                set_status(imgfile->diskimage, err, imgfile->ts.track,
                    imgfile->ts.sector);
                return counter;
            }

            p = di_get_ts_addr(imgfile->diskimage, imgfile->ts);
            imgfile->buffer = p + 2;
            imgfile->nextts.track = p[0];
            imgfile->nextts.sector = p[1];

            if (imgfile->nextts.track == 0) {

                if (imgfile->nextts.sector == 0) {
                    /* fixme, something is wrong if this happens, should be a proper error
           */
                    imgfile->buflen = 0;
                    set_status(imgfile->diskimage, -1, imgfile->ts.track,
                        imgfile->ts.sector);
                } else {
                    imgfile->buflen = imgfile->nextts.sector - 1;
                }

            } else {

                if (!di_ts_is_valid(imgfile->diskimage->type, imgfile->nextts)) {
                    set_status(imgfile->diskimage, 66, imgfile->nextts.track,
                        imgfile->nextts.sector);
                    return counter;
                }

                imgfile->buflen = 254;
            }
            imgfile->bufptr = 0;
        } else {
            if (len >= bytesleft) {
                while (bytesleft) {
                    *buffer++ = imgfile->buffer[imgfile->bufptr++];
                    --len;
                    --bytesleft;
                    ++counter;
                    ++(imgfile->position);
                }
            } else {
                while (len) {
                    *buffer++ = imgfile->buffer[imgfile->bufptr++];
                    --len;
                    --bytesleft;
                    ++counter;
                    ++(imgfile->position);
                }
            }
        }
    }
    return counter;
}
