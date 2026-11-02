/*
 * Apple II disk image reader.
 *
 * Reads DOS 3.3 and ProDOS disk images in .dsk (sector) and .nib (nibble)
 * formats. Provides file-level access to extract individual files from the
 * disk image's filesystem.
 *
 * Based on parts of the CiderPress source by Andy McFadden.
 * See https://github.com/fadden/ciderpress for the full version.
 *
 * Includes fragments from a2tools by Terry Kyriacopoulos and Paul Schlyter.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "debugprint.h"
#include "ciderpress.h"
#include "woz2nib.h"

/*
 * Only one geometry is supported: a 5.25" disk of 35 tracks x 16 sectors x
 * 256 bytes = 143360 bytes, or 280 ProDOS blocks.
 */
enum {
    kNumTracks = WOZ_NIB_TRACKS,
    kNumSectPerTrack = 16,
    kSectorSize = 256,
    kBlkSize = 512,
    kNumBlocks = kNumTracks * kNumSectPerTrack / 2,
    kDiskImageSize = kNumTracks * kNumSectPerTrack * kSectorSize,

    kTrackLenNib525 = WOZ_NIB_TRACK_LEN,    // bytes per track in a ".nib"
    kNibImageSize = kNumTracks * kTrackLenNib525,
    kChunkSize62 = 86,          // 6&2 encoding: 86 "twos" bytes per sector

    kMaxFileName = 30,          // DOS 3.3 file name length
    kMaxProDOSName = 15,
    kFssep = ':',
};

/* DOS 3.3 */
enum {
    kVTOCTrack = 17,
    kVTOCSector = 0,
    kCatalogEntryOffset = 0x0b,     // first entry in cat sect starts here
    kCatalogEntrySize = 0x23,       // length in bytes of catalog entries
    kCatalogEntriesPerSect = 7,     // #of entries per catalog sector
    kEntryDeleted = 0xff,           // this is used for track# of deleted files
    kEntryUnused = 0x00,            // this is track# in never-used entries
    kMaxTSPairs = 0x7a,             // 122 entries for 256-byte sectors
    kTSOffset = 0x0c,               // first T/S entry in a T/S list
    kMaxTSIterations = 32,
    kMaxCatalogSectors = 64,        // two tracks on a 32-sector disk
};

/* ProDOS */
enum {
    kVolHeaderBlock = 2,            // block where Volume Header resides
    kMaxCatalogIterations = 1024,   // theoretical max is 32768?
    kMaxDirectoryDepth = 64,        // not sure what ProDOS limit is
    kMaxBlocksPerIndex = 256,
    kDirEntryLen = 0x27,            // standard length of a directory entry
    kDirEntriesPerBlock = 13,
    kFileTypeBIN = 0x06,
};

typedef enum {
    kStorageDeleted         = 0,      /* indicates deleted file */
    kStorageSeedling        = 1,      /* <= 512 bytes */
    kStorageSapling         = 2,      /* < 128KB */
    kStorageTree            = 3,      /* < 16MB */
    kStorageDirectory       = 13,
    kStorageVolumeDirHeader = 15,
} StorageType;

typedef enum DIError {
    kDIErrNone = 0,
    kDIErrDataUnderrun,         // tried to read off end of the image
    kDIErrInvalidTrack,         // request for invalid track number
    kDIErrInvalidSector,        // request for invalid sector number
    kDIErrInvalidBlock,         // request for invalid block number
    kDIErrDirectoryLoop,        // directory chain points into itself
    kDIErrFileLoop,             // file sector or block alloc loops
    kDIErrBadDiskImage,         // the FS on the disk image is damaged
    kDIErrBadFile,              // bad file on disk image
    kDIErrBadDirectory,         // bad dir on disk image
    kDIErrSectorUnreadable,     // requested sector not readable
    kDIErrInvalidDiskByte,      // invalid byte for encoding type
    kDIErrNotSupported,         // feature not currently supported
} DIError;

typedef enum { // format of the image data stream
    kPhysicalFormatSectors = 0,     // sequential 256-byte sectors
    kPhysicalFormatNib525_6656,     // 5.25" disk ".nib" (6656 bytes/track)
} PhysicalFormat;

typedef enum { // sector ordering of a "sector" format image
    kSectorOrderDOS = 0,        // written as series of DOS sectors (.dsk, .do)
    kSectorOrderProDOS,         // written as series of ProDOS blocks (.po)
} SectorOrder;

/* a T/S pair */
typedef struct TrackSector {
    uint8_t track;
    uint8_t sector;
} TrackSector;

/* The parts of a ProDOS directory header that we use. */
typedef struct DirHeader {
    uint8_t     entryLength;
    uint8_t     entriesPerBlock;
} DirHeader;

/* The parts of a ProDOS directory entry that we use. */
typedef struct DirEntry {
    int             storageType;
    char            fileName[kMaxProDOSName + 1];   // shows lower case
    uint16_t        keyPointer;
    uint16_t        blocksUsed;
    uint32_t        eof;
} DirEntry;

/* A file in the catalog (DOS 3.3) or directory tree (ProDOS). */
typedef struct A2File {
    struct A2File *next;
    char *pathName;         // DOS: file name. ProDOS: "DIR:SUBDIR:NAME"

    /* DOS 3.3 */
    int fTSListTrack;
    int fTSListSector;
    int fLengthInSectors;

    /* ProDOS */
    DirEntry fDirEntry;
} A2File;

/*
 * Everything we know about the current disk image. FreeDiskImage() returns
 * all of it to this initial state.
 */
typedef struct {
    const uint8_t *raw;         // the loaded disk image; not ours
    size_t rawLen;
    PhysicalFormat physical;
    SectorOrder order;          // of a sector image

    /* The nibble track most recently read, as a ring of "len" disk bytes. */
    struct {
        uint8_t buf[kTrackLenNib525];
        int track;              // which track is in buf (-1 = none)
        int len;
        /* Per-track sector lookup, built once per track and reused for all 16
         * sector reads. Without it, ReadFullImageFromNib scans the whole track
         * from the top once per sector -- an O(sectors * trackLen) hot spot. */
        int sectorIndexTrack;                   // track dataIdx is valid for (-1 = none)
        int dataIdx[kNumSectPerTrack];          // ring index of each sector's data field, -1 = absent
    } nib;

    /* Linked list of files found on the disk image */
    A2File *firstfile;
    A2File *lastfile;

    char volumeName[kMaxProDOSName + 1];        // ProDOS
} DiskState;

static DiskState g = { .nib = { .track = -1, .sectorIndexTrack = -1 } };

static void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return (t);
}

/* Zero-filled, and never a zero-sized request. */
static void *MemCalloc(size_t size)
{
    void *t = MemAlloc(size ? size : 1);
    memset(t, 0, size);
    return t;
}

static void *MemRealloc(void *ptr, size_t size)
{
    void *t = (void *)realloc(ptr, size);
    if (t == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return (t);
}

static char *MemStrdup(const char *s)
{
    size_t size = strlen(s) + 1;
    return memcpy(MemAlloc(size), s, size);
}

/*
 * Get values from a memory buffer.
 */
static uint16_t GetShortLE(const uint8_t* ptr)
{
    return *ptr | (uint16_t) *(ptr+1) << 8;
}

static uint32_t GetLongLE(const uint8_t* ptr)
{
    return *ptr |
    (uint32_t) *(ptr+1) << 8 |
    (uint32_t) *(ptr+2) << 16 |
    (uint32_t) *(ptr+3) << 24;
}

/*
 * ==========================================================================
 *      Nibble images
 * ==========================================================================
 */

/*
 * DOS 3.3 standard sector format. Checksums of the data field are never
 * verified and those of the address field only as described in
 * BuildSectorIndex(): the copy-protected disks we read depend on that.
 */
static const uint8_t kAddrProlog[3] = { 0xd5, 0xaa, 0x96 };
static const uint8_t kAddrEpilog[2] = { 0xde, 0xaa };   // of de aa eb; we check two
static const uint8_t kDataProlog[3] = { 0xd5, 0xaa, 0xad };

/* 6&2 encoding table: maps 6-bit values (0-63) to valid disk bytes (>= 0x96) */
static const uint8_t kDiskBytes62[64] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#define kInvInvalidValue 0xff

/* Inverse of kDiskBytes62: maps disk bytes back to 6-bit values */
static const uint8_t *InvDiskBytes62(void)
{
    static uint8_t table[256];
    static int built = 0;

    if (!built) {
        memset(table, kInvInvalidValue, sizeof(table));
        for (unsigned int i = 0; i < sizeof(kDiskBytes62); i++)
            table[kDiskBytes62[i]] = i;
        built = 1;
    }
    return table;
}

/*
 * Read a byte from the current track, which is circular: a sector may span
 * the end/start boundary of the buffer. "index" must not be negative.
 */
static inline uint8_t Ring(int index)
{
    while (index >= g.nib.len)
        index -= g.nib.len;
    return g.nib.buf[index];
}

/* Decode a 4-and-4 encoded byte pair (used in address fields) */
static int ConvFrom44(uint8_t val1, uint8_t val2)
{
    return ((val1 << 1) | 0x01) & val2;
}

/*
 * Scan the whole track once and record, for every sector, the ring index of
 * its data field (or -1 if absent) in g.nib.dataIdx. The first qualifying
 * occurrence of each sector wins.
 */
static void BuildSectorIndex(int track)
{
    const int trackLen = g.nib.len;

    for (int s = 0; s < kNumSectPerTrack; s++)
        g.nib.dataIdx[s] = -1;

    for (int i = 0; i < trackLen; i++) {
        /* Some Penguin/Polarware "Comprehend" disks (e.g. the Apple II
         * Talisman) write a D4 address prolog on odd tracks instead of the
         * standard D5; accept either. */
        uint8_t b0 = Ring(i);
        if (!((b0 == kAddrProlog[0] || b0 == 0xd4) && Ring(i + 1) == kAddrProlog[1] && Ring(i + 2) == kAddrProlog[2]))
            continue;

        /* found the address header, decode the address: volume, track,
         * sector, checksum. The track number is not checked. */
        i += 3;
        int hdrVol = ConvFrom44(Ring(i), Ring(i + 1));
        int hdrTrack = ConvFrom44(Ring(i + 2), Ring(i + 3));
        int hdrSector = ConvFrom44(Ring(i + 4), Ring(i + 5));
        int hdrChksum = ConvFrom44(Ring(i + 6), Ring(i + 7));

        /* DOS 3.3 address checksum: chk == vol ^ track ^ sector. */
        int checksumOk = ((hdrVol ^ hdrTrack ^ hdrSector ^ hdrChksum) == 0);
        int epilogOk = (Ring(i + 8) == kAddrEpilog[0] && Ring(i + 9) == kAddrEpilog[1]);

        /* Standard disks pass the epilog check. Some Penguin/Polarware
         * "Comprehend" disks (Apple II Talisman) use a non-standard address
         * epilog but keep a valid checksum, so accept the field if either
         * the epilog matches or the checksum is good. */
        if (!epilogOk && !checksumOk)
            continue;

        if (hdrSector >= kNumSectPerTrack || g.nib.dataIdx[hdrSector] >= 0)
            continue;   // out of range, or an earlier occurrence already won

        /*
         * Scan forward and look for data prolog. (CiderPress limits the reach
         * of this search so as not to blunder into the data field of the next
         * sector; we look all the way round.)
         */
        for (int j = 0; j < trackLen; j++) {
            if (Ring(i + j) == kDataProlog[0] && Ring(i + j + 1) == kDataProlog[1] && Ring(i + j + 2) == kDataProlog[2]) {
                g.nib.dataIdx[hdrSector] = (i + j + 3) % trackLen;
                break;
            }
        }
    }

    g.nib.sectorIndexTrack = track;
}

/*
 * Decode the 6&2 encoded data field that starts at ring index "idx".
 */
static DIError DecodeNibbleData(int idx, uint8_t *sctBuf)
{
    const uint8_t *inv = InvDiskBytes62();
    uint8_t twos[kChunkSize62 * 3]; // 258
    int chksum = 0;
    uint8_t decodedVal;
    int i;

    /*
     * Pull the 342 bytes out, convert them from disk bytes to 6-bit
     * values, and arrange them into a DOS-like pair of buffers.
     */
    for (i = 0; i < kChunkSize62; i++) {
        decodedVal = inv[Ring(idx++)];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;

        chksum ^= decodedVal;
        twos[i] = ((chksum & 0x01) << 1) | ((chksum & 0x02) >> 1);
        twos[i + kChunkSize62] = ((chksum & 0x04) >> 1) | ((chksum & 0x08) >> 3);
        twos[i + kChunkSize62 * 2] = ((chksum & 0x10) >> 3) | ((chksum & 0x20) >> 5);
    }

    for (i = 0; i < kSectorSize; i++) {
        decodedVal = inv[Ring(idx++)];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;

        chksum ^= decodedVal;
        sctBuf[i] = (chksum << 2) | twos[i];
    }

    /*
     * The 343rd byte is the checksum. It has to be a valid disk byte, but
     * we deliberately do not require it to match.
     */
    if (inv[Ring(idx)] == kInvInvalidValue)
        return kDIErrInvalidDiskByte;

    return kDIErrNone;
}

/*
 * Copy a chunk of bytes out of the disk image.
 *
 * (This is the lowest-level read routine.)
 */
static DIError CopyBytesOut(void *buf, size_t offset, size_t size)
{
    if (g.raw == NULL || offset > g.rawLen || size > g.rawLen - offset)
        return kDIErrDataUnderrun;
    memcpy(buf, g.raw + offset, size);
    return kDIErrNone;
}

/*
 * woz2nib decodes the bitstream continuously, so when a physical track holds
 * fewer than kTrackLenNib525 nibbles the buffer's tail simply repeats its head
 * (the decode wrapped into a second revolution; see woz_decode_track() in
 * woz2nib.c, which reads the track as a circle for exactly that). Reading the track as a
 * fixed-length ring then mis-joins the wrap, corrupting whichever sector's
 * data field straddles the kTrackLenNib525 boundary -- e.g. the Talisman
 * Empire disk's T3 S15, which backs the bazaar message-board picture (RA #6).
 *
 * Recover the true revolution length from that duplication: find the longest
 * tail that equals the head (the repeated region of length L) and use
 * fullLen - L as the ring length. Then the buffer holds exactly one continuous
 * revolution and the wrap is a faithful circle, so every sector -- including
 * one straddling the splice -- decodes. Require a substantial overlap so
 * random data never triggers a false shortening; if none is found the length
 * stays fullLen.
 */
static int FindRevolutionLength(const uint8_t *track, int fullLen)
{
    const int kMinOverlap = 64;          // long enough to rule out chance
    const int kMaxOverlap = 1024;        // > slack for a 16-sector revolution
    int limit = kMaxOverlap < fullLen / 2 ? kMaxOverlap : fullLen / 2;

    for (int L = limit; L >= kMinOverlap; L--) {
        if (memcmp(track, track + fullLen - L, (size_t)L) == 0)
            return fullLen - L;
    }
    return fullLen;
}

/*
 * Load a nibble track into our track buffer.
 */
static DIError LoadNibbleTrack(int track)
{
    if (track == g.nib.track)
        return kDIErrNone;

    debug_print("  DI loading track %d\n", track);

    /* invalidate in case we fail */
    g.nib.track = -1;
    g.nib.sectorIndexTrack = -1;

    DIError dierr = CopyBytesOut(g.nib.buf, (size_t)track * kTrackLenNib525, kTrackLenNib525);
    if (dierr != kDIErrNone)
        return dierr;

    g.nib.len = FindRevolutionLength(g.nib.buf, kTrackLenNib525);
    g.nib.track = track;
    return kDIErrNone;
}

/*
 * Read a sector, given by its physical sector number, from a nibble image.
 */
static DIError ReadNibbleSector(int track, int sector, uint8_t *buf)
{
    DIError dierr = LoadNibbleTrack(track);
    if (dierr != kDIErrNone) {
        debug_print("   DI ReadNibbleSector: LoadNibbleTrack %d failed\n", track);
        return dierr;
    }

    if (g.nib.sectorIndexTrack != track)
        BuildSectorIndex(track);

    if (g.nib.dataIdx[sector] < 0)
        return kDIErrSectorUnreadable;

    return DecodeNibbleData(g.nib.dataIdx[sector], buf);
}

/*
 * ==========================================================================
 *      Block/track/sector I/O
 * ==========================================================================
 */

/*
 * Sector order conversions, by way of the physical ("raw") sector number.
 */
static const uint8_t dos2raw[16] = {
    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};
static const uint8_t raw2dos[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};
static const uint8_t prodos2raw[16] = {
    0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
};
static const uint8_t raw2prodos[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

/*
 * Read the sector with the given physical sector number, wherever the image
 * keeps it.
 *
 * Copies 256 bytes into "*buf".
 */
static DIError ReadRawSector(int track, int rawSector, uint8_t *buf)
{
    if (track < 0 || track >= kNumTracks) {
        debug_print(" DI read invalid track %d\n", track);
        return kDIErrInvalidTrack;
    }
    if (rawSector < 0 || rawSector >= kNumSectPerTrack) {
        debug_print(" DI read invalid sector %d\n", rawSector);
        return kDIErrInvalidSector;
    }

    if (g.physical == kPhysicalFormatNib525_6656)
        return ReadNibbleSector(track, rawSector, buf);

    int sector = (g.order == kSectorOrderProDOS) ? raw2prodos[rawSector] : raw2dos[rawSector];
    return CopyBytesOut(buf, (size_t)(track * kNumSectPerTrack + sector) * kSectorSize, kSectorSize);
}

/* Read a sector given by its DOS 3.3 (logical) sector number. */
static DIError ReadDOSSector(int track, int sector, uint8_t *buf)
{
    if (sector < 0 || sector >= kNumSectPerTrack)
        return kDIErrInvalidSector;
    return ReadRawSector(track, dos2raw[sector], buf);
}

/*
 * Read a 512-byte ProDOS block.
 *
 * Copies 512 bytes into "*buf".
 */
static DIError ReadBlock(long block, uint8_t *buf)
{
    if (block < 0 || block >= kNumBlocks)
        return kDIErrInvalidBlock;

    int track = (int)(block / (kNumSectPerTrack / 2));
    int sector = (int)(block % (kNumSectPerTrack / 2)) * 2;

    DIError dierr = ReadRawSector(track, prodos2raw[sector], buf);
    if (dierr != kDIErrNone)
        return dierr;
    return ReadRawSector(track, prodos2raw[sector + 1], buf + kSectorSize);
}

/*
 * ==========================================================================
 *      Images and the file list
 * ==========================================================================
 */

static void FreeFileList(void)
{
    while (g.firstfile != NULL) {
        A2File *nextfile = g.firstfile->next;
        free(g.firstfile->pathName);
        free(g.firstfile);
        g.firstfile = nextfile;
    }
    g.lastfile = NULL;
}

static void InitImage(uint8_t *data, size_t datasize, PhysicalFormat physical)
{
    FreeDiskImage();
    g.raw = data;
    g.rawLen = datasize;
    g.physical = physical;
}

void InitNibImage(uint8_t *data, size_t datasize)
{
    InitImage(data, datasize, kPhysicalFormatNib525_6656);
}

void InitDskImage(uint8_t *data, size_t datasize)
{
    InitImage(data, datasize, kPhysicalFormatSectors);
}

void FreeDiskImage(void)
{
    FreeFileList();
    memset(&g, 0, sizeof(g));
    g.nib.track = -1;
    g.nib.sectorIndexTrack = -1;
}

/*
 * The entry points that take an image go through here. If it is the image
 * that an Init*Image() call announced, that call's format stands (and the
 * cached nibble track is still good); if it is a new one, it is taken to be
 * of the format "assumed".
 */
static void UseImage(uint8_t *data, size_t datasize, PhysicalFormat assumed)
{
    if (g.raw != data || g.rawLen != datasize)
        InitImage(data, datasize, assumed);
    FreeFileList();
}

/* Allocate a file entry, zeroed, and append it to the list of discovered files */
static A2File *NewA2File(void)
{
    A2File *file = MemCalloc(sizeof(A2File));

    if (g.lastfile == NULL)
        g.firstfile = file;
    else
        g.lastfile->next = file;
    g.lastfile = file;
    return file;
}

/* Find a file by name, compared with "cmp" (strcmp or strcasecmp). */
static A2File *FindFile(const char *name, int (*cmp)(const char *, const char *))
{
    for (A2File *file = g.firstfile; file != NULL; file = file->next) {
        if (cmp(file->pathName, name) == 0)
            return file;
    }
    return NULL;
}

/*
 * Read a contiguous range of bytes from a DOS-ordered nibble disk image.
 *
 * Decodes sectors on-the-fly starting from the given byte offset.
 * The caller must free the returned buffer.
 */
uint8_t *ReadImageFromNib(size_t offset, size_t size, uint8_t *data, size_t datasize)
{
    UseImage(data, datasize, kPhysicalFormatNib525_6656);

    uint8_t *result = MemCalloc(size);
    uint8_t buf[kSectorSize];
    size_t sector = offset / kSectorSize;

    for (size_t done = 0; done < size; done += kSectorSize, sector++) {
        size_t track = sector / kNumSectPerTrack;
        if (track >= kNumTracks || ReadDOSSector((int)track, sector % kNumSectPerTrack, buf) != kDIErrNone) {
            fprintf(stderr, "Error while reading nibble sector!\n");
            free(result);
            return NULL;
        }
        size_t count = size - done;
        memcpy(result + done, buf, count < kSectorSize ? count : kSectorSize);
    }

    return result;
}

/*
 * Decode an entire nibble disk image into a standard 143360-byte
 * DOS 3.3 ordered .dsk image (35 tracks * 16 sectors * 256 bytes).
 *
 * Unlike ReadImageFromNib(), this is tolerant of unreadable sectors:
 * sectors that fail to decode (e.g. unformatted or copy-protected tracks)
 * are left zero-filled instead of aborting the whole conversion.  This is
 * what we want when reconstructing a raw disk image from a WOZ dump, where
 * spare/unused tracks need not decode cleanly.
 *
 * The caller must free the returned buffer.
 */
uint8_t *ReadFullImageFromNib(uint8_t *data, size_t datasize)
{
    InitNibImage(data, datasize);

    uint8_t *result = MemCalloc(kDiskImageSize);
    uint8_t buf[kSectorSize];

    for (int track = 0; track < kNumTracks; track++) {
        for (int sector = 0; sector < kNumSectPerTrack; sector++) {
            if (ReadDOSSector(track, sector, buf) == kDIErrNone)
                memcpy(result + (track * kNumSectPerTrack + sector) * kSectorSize,
                    buf, kSectorSize);
        }
    }

    return result;
}

/*
 * ===========================================================================
 *      DOS 3.3
 * ===========================================================================
 */

/*
 * "Fix" a DOS3.3 filename.  Convert DOS-ASCII to normal ASCII, and strip
 * trailing spaces.
 *
 * Some people put inverse and flashing text into filenames, not to mention
 * control characters, so we have to cope with those too.
 */
static void FixFilename(uint8_t filename[kMaxFileName + 1])
{
    int len;

    for (len = 0; len < kMaxFileName; len++) {
        uint8_t *buf = &filename[len];
        if (*buf & 0x80) {
            if (*buf >= 0xa0)
                *buf &= 0x7f;
            else
                *buf = (*buf & 0x7f) + 0x20;
        } else
            *buf = ((*buf & 0x3f) ^ 0x20) + 0x20;
    }

    while (len > 0 && filename[len - 1] == ' ')
        len--;
    filename[len] = '\0';
}

/*
 * Extract the track/sector pairs from the TS list in "sctBuf".  The entries
 * are copied to "tsList", which is assumed to have enough space to hold
 * at least kMaxTSPairs entries.
 *
 * The last non-zero entry will be identified and stored in "*pLastNonZero".
 * If all entries are zero, it will be set to -1.
 *
 * Sometimes files will have junk at the tail end of an otherwise valid
 * T/S list.  We can't just stop when we hit the first (0,0) entry because
 * that'll screw up random-access text file handling.  If the junk follows a
 * (0,0) entry, the list looks partially valid: only the valid part is copied
 * out, with the rest zeroed, and that is not an error.  Any other invalid
 * entry is.
 */
static DIError ExtractTSPairs(const uint8_t *sctBuf, TrackSector *tsList,
    int *pLastNonZero)
{
    const uint8_t *ptr = &sctBuf[kTSOffset]; // offset of first T/S entry (0x0c)

    *pLastNonZero = -1;
    memset(tsList, 0, sizeof(TrackSector) * kMaxTSPairs);

    for (int i = 0; i < kMaxTSPairs; i++) {
        int track = *ptr++;
        int sector = *ptr++;

        if (track >= kNumTracks || sector >= kNumSectPerTrack || (track == 0 && sector != 0)) {
            debug_print(" DOS33 invalid T/S %d,%d\n", track, sector);

            if (i > 0 && tsList[i - 1].track == 0 && tsList[i - 1].sector == 0) {
                debug_print("  T/S list looks partially valid\n");
                return kDIErrNone;
            }
            return kDIErrBadFile;
        }

        if (track != 0 || sector != 0)
            *pLastNonZero = i;

        tsList[i].track = track;
        tsList[i].sector = sector;
    }

    return kDIErrNone;
}

/*
 * Load the T/S list for this file. The caller must free "*pList".
 *
 * A single T/S sector holds 122 entries, enough to store a 30.5K file.
 * It's very unlikely that a file will need more than two, although it's
 * possible for a random-access text file to have a very large number of
 * entries.
 *
 * It's entirely possible to get a large T/S list back that is filled
 * entirely with zeroes.  This can happen if we have a large set of T/S
 * index sectors that are all zero.
 */
static DIError LoadTSList(const A2File *DOSFile, TrackSector **pList, int *pCount)
{
    DIError dierr = kDIErrNone;
    TrackSector *tsList = NULL;
    int tsCount = 0;
    uint8_t sctBuf[kSectorSize];
    int iterations = 0;

    debug_print("--- DOS loading T/S list for '%s'\n", DOSFile->pathName);

    /* get the first T/S sector for this file */
    int track = DOSFile->fTSListTrack;
    int sector = DOSFile->fTSListSector;

    /*
     * Run through the set of t/s pairs.
     */
    do {
        int lastNonZero;

        if (iterations == kMaxTSIterations) {
            dierr = kDIErrFileLoop;
            goto bail;
        }

        /* A bogus link to a further T/S list sector (seen on real disks:
         * "R0106" on Adventureland links to 32,32) ends the list instead. */
        dierr = ReadDOSSector(track, sector, sctBuf);
        if (dierr != kDIErrNone) {
            if (iterations > 0)
                break;
            goto bail;
        }

        /* grab next track/sector */
        track = sctBuf[0x01];
        sector = sctBuf[0x02];

        /*
         * Add the entries.  If there's another T/S list linked, we just
         * grab the entire sector.  If not, we grab every entry until the
         * last 0,0.  (Can't stop at the first (0,0), or we'll drop a
         * piece of a random access text file.)
         */
        tsList = MemRealloc(tsList, (tsCount + kMaxTSPairs) * sizeof(TrackSector));
        dierr = ExtractTSPairs(sctBuf, &tsList[tsCount], &lastNonZero);
        if (dierr != kDIErrNone) {
            if (iterations > 0)
                break;
            goto bail;
        }

        /* As in DOS, a link to track 0 ends the chain. */
        if (track != 0)
            tsCount += kMaxTSPairs;
        else
            tsCount += lastNonZero + 1;

        iterations++; // watch for infinite loops
    } while (track != 0);

    *pList = tsList;
    *pCount = tsCount;
    return kDIErrNone;

bail:
    debug_print("DOS33 unable to load TS for '%s'\n", DOSFile->pathName);
    free(tsList);
    return dierr;
}

/*
 * Read a whole file, as its sectors: a binary file starts with its four
 * bytes of address and length, and the result is not trimmed to that length.
 *
 * The length comes from the catalog entry, which counts the T/S list sector
 * too -- one of them; a file with more reads up to the end of its T/S list.
 * Sectors that the file does not have (a sparse text file) or that cannot be
 * read are zero-filled.
 *
 * Returns a buffer of "*pLen" bytes that the caller must free, or NULL if the
 * catalog says that the file is empty.
 */
static uint8_t *ReadDOSFileData(const A2File *file, size_t *pLen)
{
    TrackSector *tsList = NULL;
    int tsCount = 0;

    *pLen = 0;
    if (file->fLengthInSectors <= 0)
        return NULL;

    LoadTSList(file, &tsList, &tsCount);

    int sectors = file->fLengthInSectors - 1;
    if (sectors > tsCount)
        sectors = tsCount;
    if (sectors > kNumTracks * kNumSectPerTrack)
        sectors = kNumTracks * kNumSectPerTrack;

    uint8_t *buf = MemCalloc((size_t)sectors * kSectorSize);

    for (int i = 0; i < sectors; i++) {
        if (tsList[i].track == 0 && tsList[i].sector == 0)
            continue;
        if (ReadDOSSector(tsList[i].track, tsList[i].sector, buf + i * kSectorSize) != kDIErrNone) {
            debug_print(" DOS error reading file '%s'\n", file->pathName);
            memset(buf + i * kSectorSize, 0, kSectorSize);
        }
    }

    free(tsList);
    *pLen = (size_t)sectors * kSectorSize;
    return buf;
}

/*
 * Process the list of files in one sector of the catalog.
 */
static void ProcessCatalogSector(const uint8_t *sctBuf)
{
    const uint8_t *pEntry = &sctBuf[kCatalogEntryOffset];

    for (int i = 0; i < kCatalogEntriesPerSect; i++, pEntry += kCatalogEntrySize) {
        if (pEntry[0x00] == kEntryUnused || pEntry[0x00] == kEntryDeleted)
            continue;

        uint8_t name[kMaxFileName + 1];
        memcpy(name, &pEntry[0x03], kMaxFileName);
        FixFilename(name);
        debug_print("File name: %s\n", name);

        A2File *pFile = NewA2File();
        pFile->pathName = MemStrdup((char *)name);
        pFile->fTSListTrack = pEntry[0x00];
        pFile->fTSListSector = pEntry[0x01];
        pFile->fLengthInSectors = GetShortLE(&pEntry[0x21]);
    }
}

/*
 * Read the disk's catalog, by way of the Volume Table of Contents, into the
 * file list.
 *
 * NOTE: supposedly DOS stops reading the catalog track when it finds the
 * first entry with a 00 byte, which is why deleted files use ff.  If so,
 * it *might* make sense to mimic this behavior, though on a healthy disk
 * we shouldn't be finding garbage anyway.
 *
 * A catalog that goes bad part of the way keeps the files found so far, and
 * returns the error: the callers report it and go on with those files.
 */
static DIError ReadCatalog(void)
{
    uint8_t sctBuf[kSectorSize];

    DIError dierr = ReadDOSSector(kVTOCTrack, kVTOCSector, sctBuf);
    if (dierr != kDIErrNone)
        return dierr;

    int catTrack = sctBuf[0x01];
    int catSect = sctBuf[0x02];
    if (catTrack >= kNumTracks || catSect >= kNumSectPerTrack)
        return kDIErrBadDiskImage;

    for (int iterations = 0; catTrack != 0 && catSect != 0; iterations++) {
        if (iterations == kMaxCatalogSectors)
            return kDIErrDirectoryLoop;

        dierr = ReadDOSSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            return dierr;

        /*
         * Watch for flaws that the DOS detector allows.
         */
        if (catTrack == sctBuf[0x01] && catSect == sctBuf[0x02]) {
            debug_print(" DOS detected self-reference on cat (%d,%d)\n",
                catTrack, catSect);
            break;
        }

        /*
         * Check the next track/sector in the chain.  If the pointer is
         * broken, there's a very good chance that this isn't really a
         * catalog sector, so we want to bail out now.
         */
        if (sctBuf[0x01] >= kNumTracks || sctBuf[0x02] >= kNumSectPerTrack) {
            debug_print(" DOS bailing out early on catalog read due to funky T/S\n");
            break;
        }

        ProcessCatalogSector(sctBuf);

        catTrack = sctBuf[0x01];
        catSect = sctBuf[0x02];
    }

    return kDIErrNone;
}

/*
 * The main SAGA adventure game data file goes by several names:
 * "DATABASE", specific game titles, or "A<n>.DAT" (adventure number).
 */
static int IsSagaDatabaseName(const char *name)
{
    return strcmp("DATABASE", name) == 0 ||
        strcmp("SORCEROR OF CLAYMORGUE CASTLE", name) == 0 ||
        strcmp("THE INCREDIBLE HULK", name) == 0 ||
        (name[0] == 'A' && name[1] != '\0' && strncmp(name + 2, ".DAT", 4) == 0);
}

/* M2 holds the image unscrambling tables (Norm Sailer lookup data). */
static const char kM2Copyright[] = "COPYRIGHT 1983 NORMAN L. SAILER";
enum {
    kM2CopyrightOffset = 0x172c,
    kM2TableOffset = 0x174b,
    kM2TableSize = 0x182,
};

/* The graphics module keeps a table of the pictures to show when something
 * is looked at: a JMP past it, the verb, then sixteen items, sixteen pictures
 * and sixteen nouns, and the prompt to print under the picture. This is M2,
 * or part of the database file on a disk that has everything in one file. */
static const char kCloseupPrompt[] = "HIT RETURN PLEASE. ";
enum {
    kCloseupJmpSize = 3,
    kOpcodeJmp = 0x4c,
};

static uint8_t *CopyCloseupTable(const uint8_t *buf, size_t len)
{
    const size_t promptlen = sizeof(kCloseupPrompt) - 1;
    const size_t before = kCloseupJmpSize + kA2CloseupTableSize;
    for (size_t i = before; i + promptlen <= len; i++) {
        if (buf[i - before] == kOpcodeJmp && memcmp(buf + i, kCloseupPrompt, promptlen) == 0) {
            uint8_t *table = MemAlloc(kA2CloseupTableSize);
            memcpy(table, buf + i - kA2CloseupTableSize, kA2CloseupTableSize);
            return table;
        }
    }
    return NULL;
}

uint8_t *ReadApple2DOSFile(uint8_t *data, size_t *len, uint8_t **invimg, size_t *invimglen, uint8_t **m2, uint8_t **m3, size_t *m3len, uint8_t **closeups)
{
    UseImage(data, *len, kPhysicalFormatSectors);
    DIError dierr = ReadCatalog();
    if (dierr != kDIErrNone) {
        debug_print("%s: reading the DOS catalog failed with error %d\n", __func__, dierr);
        if (g.firstfile == NULL)
            return NULL;
    }

    uint8_t *temp;
    size_t templen;

    uint8_t *database = NULL;
    for (A2File *file = g.firstfile; file != NULL && database == NULL; file = file->next) {
        if (IsSagaDatabaseName(file->pathName) && (database = ReadDOSFileData(file, &templen)) != NULL)
            *len = templen;
    }

    uint8_t *table = NULL;

    /* The inventory image is often on the boot disk (other images on side B) */
    A2File *file = FindFile("PAK.INVEN", strcmp);
    if (!file)
        file = FindFile("PAC.INVEN", strcmp);
    if (file && (temp = ReadDOSFileData(file, &templen)) != NULL) {
        /* Drop the four bytes of address and length */
        if (templen > 4 && templen <= 100000) {
            *invimglen = templen - 4;
            *invimg = MemAlloc(*invimglen);
            memcpy(*invimg, temp + 4, *invimglen);
        }
        free(temp);
    }

    file = FindFile("M2", strcmp);
    if (file && (temp = ReadDOSFileData(file, &templen)) != NULL) {
        if (templen >= kM2TableOffset + kM2TableSize &&
            memcmp(temp + kM2CopyrightOffset, kM2Copyright, sizeof(kM2Copyright) - 1) == 0) {
            *m2 = MemAlloc(kM2TableSize);
            memcpy(*m2, temp + kM2TableOffset, kM2TableSize);
        }
        table = CopyCloseupTable(temp, templen);
        free(temp);
    }
    if (table == NULL && database != NULL)
        table = CopyCloseupTable(database, *len);
    if (table != NULL)
        *closeups = table;

    /* M3 contains Graphics Magician drawing data (brush bitmaps, patterns, etc.) */
    file = FindFile("M3", strcmp);
    if (file && (temp = ReadDOSFileData(file, &templen)) != NULL) {
        if (templen > 0) {
            *m3 = temp;
            *m3len = templen;
        } else {
            free(temp);
        }
    }

    return database;
}

A2FileRec *GetAllApple2DOSFiles(uint8_t *data, size_t len, size_t *number_of_files)
{
    UseImage(data, len, kPhysicalFormatSectors);
    DIError dierr = ReadCatalog();
    if (dierr != kDIErrNone) {
        debug_print("%s: reading the DOS catalog failed with error %d\n", __func__, dierr);
        if (g.firstfile == NULL) {
            *number_of_files = 0;
            return NULL;
        }
    }

    size_t filecount = 0;
    for (A2File *file = g.firstfile; file != NULL; file = file->next)
        filecount += (file->fLengthInSectors != 0);

    *number_of_files = filecount;
    if (filecount == 0)
        return NULL;

    A2FileRec *recs = MemAlloc(sizeof(A2FileRec) * filecount);
    A2FileRec *rec = recs;
    for (A2File *file = g.firstfile; file != NULL; file = file->next) {
        if (file->fLengthInSectors == 0)
            continue;
        rec->index = (size_t)(rec - recs);
        rec->filename = MemStrdup(file->pathName);
        rec->data = ReadDOSFileData(file, &rec->datasize);
        debug_print("GetAllApple2DOSFiles: Extracted Apple 2 file named \"%s\"\n", rec->filename);
        rec++;
    }

    return recs;
}

/*
 * ===========================================================================
 *      ProDOS
 * ===========================================================================
 */

/*
 * Copy the entries from the index block in "block" to "list", copying
 * at most "maxCount" entries.
 */
static DIError LoadIndexBlock(uint16_t block, uint16_t* list, long maxCount)
{
    uint8_t blkBuf[kBlkSize];

    if (maxCount > kMaxBlocksPerIndex)
        maxCount = kMaxBlocksPerIndex;

    DIError dierr = ReadBlock(block, blkBuf);
    if (dierr != kDIErrNone)
        return dierr;

    for (int i = 0; i < maxCount; i++)
        *list++ = blkBuf[i] | (uint16_t) blkBuf[i+256] << 8;

    return kDIErrNone;
}

/*
 * Gather a linear, non-sparse list of file blocks into an array.
 *
 * Pass in the storage type and top-level key block.
 *
 * There are two kinds of sparse: sparse *inside* data, and sparse
 * *past* data.  The latter is interesting, because there is no need
 * to create space in index blocks to hold it.  Thus, a sapling could
 * hold a file with an EOF of 16MB.
 *
 * The caller must free "*pBlockList".
 */
static DIError LoadBlockList(int storageType, uint16_t keyBlock, long eof,
                             long* pBlockCount, uint16_t** pBlockList)
{
    if (storageType != kStorageSeedling &&
        storageType != kStorageSapling &&
        storageType != kStorageTree)
    {
        debug_print(" ProDOS can't open storage type %d\n", storageType);
        return kDIErrNotSupported;
    }

    DIError dierr = kDIErrNone;
    long count = (eof + kBlkSize - 1) / kBlkSize;
    if (count == 0)
        count = 1;

    /* zeroed: this should take care of trailing sparse entries */
    uint16_t *list = MemCalloc(sizeof(uint16_t) * count);

    if (storageType == kStorageSeedling) {
        list[0] = keyBlock;
    } else if (storageType == kStorageSapling) {
        dierr = LoadIndexBlock(keyBlock, list, count);
    } else {
        uint8_t blkBuf[kBlkSize];
        uint16_t* listPtr = list;
        long countDown = count;

        dierr = ReadBlock(keyBlock, blkBuf);

        for (int idx = 0; dierr == kDIErrNone && countDown > 0; idx++) {
            long blockCount = countDown;
            if (blockCount > kMaxBlocksPerIndex)
                blockCount = kMaxBlocksPerIndex;

            /* past the master index block, or a fully sparse index block: leave zeroes */
            uint16_t idxBlock = idx < kMaxBlocksPerIndex ? blkBuf[idx] | (uint16_t) blkBuf[idx+256] << 8 : 0;
            if (idxBlock != 0)
                dierr = LoadIndexBlock(idxBlock, listPtr, blockCount);

            listPtr += blockCount;
            countDown -= blockCount;
        }
    }

    if (dierr != kDIErrNone) {
        free(list);
        return dierr;
    }

    *pBlockCount = count;
    *pBlockList = list;
    return kDIErrNone;
}

/*
 * Read the first "len" bytes of a file, or all of it if it is shorter.
 * Blocks that cannot be read are zero-filled.
 *
 * Returns a buffer of "*pLen" bytes (and one more) that the caller must free,
 * or NULL if the file's list of blocks could not be loaded.
 */
static uint8_t *ReadProDOSFileData(const DirEntry *entry, size_t len, size_t *pLen)
{
    long blockCount = 0;
    uint16_t *blockList = NULL;
    uint8_t blkBuf[kBlkSize];

    *pLen = 0;
    if (LoadBlockList(entry->storageType, entry->keyPointer, entry->eof,
            &blockCount, &blockList) != kDIErrNone) {
        debug_print(" ProDOS open failed\n");
        return NULL;
    }

    if (len > entry->eof)
        len = entry->eof;

    uint8_t *buf = MemCalloc(len + 1);

    for (size_t done = 0; done < len; done += kBlkSize) {
        uint16_t block = blockList[done / kBlkSize];
        if (ReadBlock(block, blkBuf) != kDIErrNone) {
            debug_print(" ProDOS error reading block %d of '%s'\n", block, entry->fileName);
            continue;
        }
        size_t count = len - done;
        memcpy(buf + done, blkBuf, count < kBlkSize ? count : kBlkSize);
    }

    free(blockList);
    *pLen = len;
    return buf;
}

/*
 * Apply GS/OS tech note #8 lower-case flags to a ProDOS filename.
 * Each bit in lcFlags indicates whether the corresponding character
 * should be lowercased, and a '.' made a space.
 */
static void LowerCaseName(char *name, uint16_t lcFlags)
{
    /* Should be zero or 0x8000 plus other bits; shouldn't be
     * bunch of bits without 0x8000 or 0x8000 by itself.  Not
     * really a problem, just unexpected.
     */
    if (lcFlags != 0 && !(lcFlags & 0x8000))
        return;

    for (; *name != '\0'; name++) {
        lcFlags <<= 1;
        if ((lcFlags & 0x8000) != 0)
            *name = (*name == '.') ? ' ' : (char)tolower((unsigned char)*name);
    }
}

/* Parse a ProDOS directory entry from raw block data */
static void InitDirEntry(DirEntry* pEntry, const uint8_t* entryBuf)
{
    int nameLen = entryBuf[0x00] & 0x0f;

    pEntry->storageType = (entryBuf[0x00] & 0xf0) >> 4;
    memcpy(pEntry->fileName, &entryBuf[0x01], nameLen);
    pEntry->fileName[nameLen] = '\0';
    pEntry->keyPointer = GetShortLE(&entryBuf[0x11]);
    pEntry->blocksUsed = GetShortLE(&entryBuf[0x13]);
    pEntry->eof = GetLongLE(&entryBuf[0x15]) & 0x00ffffff;

    /* the version and min version bytes double as the lower-case flags */
    if (entryBuf[0x1d] & 0x80)
        LowerCaseName(pEntry->fileName, GetShortLE(&entryBuf[0x1c]));
}

static DIError RecursiveDirAdd(uint16_t dirBlock, const char* basePath, int depth);

/*
 * Slurp the entries out of a single ProDOS directory block.
 *
 * Recursively calls RecursiveDirAdd for directories.
 */
static DIError SlurpEntries(const DirHeader* pHeader, const uint8_t* blkBuf,
                            int skipFirst, const char* basePath, int depth)
{
    int entriesThisBlock = pHeader->entriesPerBlock;
    const uint8_t* entryBuf = &blkBuf[0x04];

    if (skipFirst) {
        entriesThisBlock--;
        entryBuf += pHeader->entryLength;
    }

    for ( ; entriesThisBlock > 0 ; entriesThisBlock--, entryBuf += pHeader->entryLength) {
        if (entryBuf + kDirEntryLen > blkBuf + kBlkSize) {
            debug_print("  ProDOS whoops, just walked out of dirent buffer\n");
            return kDIErrBadDirectory;
        }

        if ((entryBuf[0x00] & 0xf0) == kStorageDeleted) {
            /* skip deleted entries */
            continue;
        }

        A2File *pFile = NewA2File();
        DirEntry *pEntry = &pFile->fDirEntry;
        InitDirEntry(pEntry, entryBuf);

        /* the full pathname: in the volume directory, the base path is "" */
        size_t pathSize = strlen(basePath) + 1 + strlen(pEntry->fileName) + 1;
        pFile->pathName = MemAlloc(pathSize);
        if (basePath[0] != '\0')
            snprintf(pFile->pathName, pathSize, "%s%c%s", basePath, kFssep, pEntry->fileName);
        else
            snprintf(pFile->pathName, pathSize, "%s", pEntry->fileName);

        /* a subdirectory that can't be read doesn't spoil the rest */
        if (pEntry->storageType == kStorageDirectory)
            RecursiveDirAdd(pEntry->keyPointer, pFile->pathName, depth+1);
    }

    return kDIErrNone;
}

/*
 * Pass in the number of the first block of the directory.
 */
static DIError RecursiveDirAdd(uint16_t dirBlock, const char* basePath, int depth)
{
    DIError dierr;
    DirHeader header;
    uint8_t blkBuf[kBlkSize];

    /* if we get too deep, assume it's a loop */
    if (depth > kMaxDirectoryDepth)
        return kDIErrDirectoryLoop;

    for (int iterations = 0; dirBlock != 0; iterations++) {
        if (iterations == kMaxCatalogIterations) {
            debug_print(" ProDOS subdir iteration count exceeded\n");
            return kDIErrDirectoryLoop;
        }
        if (dirBlock < kVolHeaderBlock || dirBlock >= kNumBlocks) {
            debug_print(" ProDOS ERROR: directory block %u out of range\n", dirBlock);
            return kDIErrInvalidBlock;
        }

        dierr = ReadBlock(dirBlock, blkBuf);
        if (dierr != kDIErrNone)
            return dierr;

        if (iterations == 0) {
            /* the first entry of the first block is the directory header */
            header.entryLength = blkBuf[0x23];
            header.entriesPerBlock = blkBuf[0x24];
            if (header.entryLength * header.entriesPerBlock > kBlkSize ||
                header.entryLength * header.entriesPerBlock == 0)
            {
                debug_print(" ProDOS invalid subdir header: entryLen=%d, entriesPerBlock=%d\n",
                     header.entryLength, header.entriesPerBlock);
                return kDIErrBadDirectory;
            }
        }

        dierr = SlurpEntries(&header, blkBuf, iterations == 0, basePath, depth);
        if (dierr != kDIErrNone)
            return dierr;

        dirBlock = GetShortLE(&blkBuf[0x02]);
    }

    return kDIErrNone;
}

/*
 * Read the volume name from the volume header and all the files, in all
 * directories, into the file list.
 */
static DIError InitializeProDOS(void)
{
    uint8_t blkBuf[kBlkSize];

    DIError dierr = ReadBlock(kVolHeaderBlock, blkBuf);
    if (dierr != kDIErrNone)
        return dierr;

    if ((blkBuf[0x04] & 0xf0) >> 4 != kStorageVolumeDirHeader)
        return kDIErrBadDiskImage;

    int nameLen = blkBuf[0x04] & 0x0f;
    memcpy(g.volumeName, &blkBuf[0x05], nameLen);
    g.volumeName[nameLen] = '\0';

    /*
     * Handle lower-case conversion; see GS/OS tech note #8.  Technote 8
     * doesn't actually talk about volume names.  By experimentation the
     * field was discovered at offset 0x1a from the start of the block,
     * which is marked as "reserved" in Beneath Apple ProDOS.
     */
    if (blkBuf[0x1b] & 0x80)
        LowerCaseName(g.volumeName, GetShortLE(&blkBuf[0x1a]));

    return RecursiveDirAdd(kVolHeaderBlock, "", 0);
}

/*
 * Does this look like an entry for a Comprehend game file: a ProDOS BIN file
 * with a printable name, of a plausible size, that starts on the disk?
 */
static int LooksLikeBinEntry(const uint8_t *e)
{
    int storageType = e[0x00] >> 4;
    int nameLen = e[0x00] & 0x0f;

    if (storageType < kStorageSeedling || storageType > kStorageTree || nameLen < 1)
        return 0;
    if (e[0x10] != kFileTypeBIN)
        return 0;

    for (int c = 0; c < nameLen; c++) {
        if (e[0x01 + c] < 33 || e[0x01 + c] >= 127)
            return 0;
    }

    uint32_t eof = GetLongLE(&e[0x15]) & 0x00ffffff;
    uint16_t key = GetShortLE(&e[0x11]);
    return eof != 0 && eof <= 150000 && key >= kVolHeaderBlock && key < kNumBlocks;
}

/*
 * The Apple II Comprehend games (Penguin/Polarware "RUNCOMP" loader) store
 * their data in a ProDOS volume whose game-file directory is deliberately
 * unlinked from the volume root, so a normal directory walk misses it. We
 * instead scan every block of the disk for what looks like directory entries
 * and read each file that we have not seen yet.
 */
A2FileRec *GetAllProDOSFiles(uint8_t *data, size_t len, size_t *number_of_files)
{
    InitDskImage(data, len);

    A2FileRec *recs = NULL;
    size_t filecount = 0;
    uint8_t blkBuf[kBlkSize];

    for (long blk = kVolHeaderBlock; blk < kNumBlocks; blk++) {
        if (ReadBlock(blk, blkBuf) != kDIErrNone)
            continue;

        for (int i = 0; i < kDirEntriesPerBlock; i++) {
            const uint8_t *e = &blkBuf[0x04 + i * kDirEntryLen];
            if (!LooksLikeBinEntry(e))
                continue;

            /* The names are upper case: no lower-case flags are applied here. */
            DirEntry entry;
            int nameLen = e[0x00] & 0x0f;
            entry.storageType = e[0x00] >> 4;
            memcpy(entry.fileName, &e[0x01], nameLen);
            entry.fileName[nameLen] = '\0';
            entry.keyPointer = GetShortLE(&e[0x11]);
            entry.eof = GetLongLE(&e[0x15]) & 0x00ffffff;

            int dup = 0;
            for (size_t s = 0; s < filecount && !dup; s++)
                dup = (strcmp(recs[s].filename, entry.fileName) == 0);
            if (dup)
                continue;

            size_t actual;
            uint8_t *buf = ReadProDOSFileData(&entry, entry.eof, &actual);
            if (buf == NULL)
                continue;

            recs = MemRealloc(recs, sizeof(A2FileRec) * (filecount + 1));
            recs[filecount].index = filecount;
            recs[filecount].filename = MemStrdup(entry.fileName);
            recs[filecount].data = buf;
            recs[filecount].datasize = actual;
            filecount++;
        }
    }

    *number_of_files = filecount;
    return recs;
}

/* Known Infocom V6 game titles on Apple II ProDOS disks */
static const char *const titles[] = { "ZORK0", "SHOGUN", "JOURNEY", "ARTHUR", NULL };

/*
 * Reads the directory of an Infocom V6 disk. The volume name is the title,
 * a separator and the number of the disk, 1 to 5.
 */
static int LoadInfocomV6Volume(int *game, int *diskindex)
{
    FreeFileList();
    if (InitializeProDOS() != kDIErrNone)
        return 0;

    for (int i = 0; titles[i]; i++) {
        size_t length = strlen(titles[i]);
        if (strncmp(g.volumeName, titles[i], length) == 0 && g.volumeName[length] != '\0') {
            *game = i;
            *diskindex = g.volumeName[length + 1] - '0';
            return *diskindex >= 1 && *diskindex <= 5;
        }
    }
    return 0;
}

uint8_t *ReadInfocomV6File(uint8_t *data, size_t *len, int *game, int *diskindex)
{
    uint8_t *buf = NULL;
    int title, index;

    UseImage(data, *len, *len == kNibImageSize ? kPhysicalFormatNib525_6656 : kPhysicalFormatSectors);
    *len = 0;

    /* A sector image may be in either order (.dsk or .po). */
    int found = LoadInfocomV6Volume(&title, &index);
    if (!found && g.physical == kPhysicalFormatSectors) {
        g.order = kSectorOrderProDOS;
        found = LoadInfocomV6Volume(&title, &index);
    }

    if (!found) {
        fprintf(stderr, "Not a recognized Apple 2 Infocom V6 game!\n");
    } else {
        if (game)
            *game = title;
        if (diskindex)
            *diskindex = index;

        char filename[64];
        snprintf(filename, sizeof(filename), "%s.D%d", titles[title], index);

        A2File *file = FindFile(filename, strcasecmp);
        if (file) {
            size_t blockbytes = (size_t)file->fDirEntry.blocksUsed * kBlkSize;
            buf = ReadProDOSFileData(&file->fDirEntry, blockbytes ? blockbytes - 1 : 0, len);
            if (buf == NULL)
                fprintf(stderr, "Could not read \"%s\"\n", filename);
        }
    }

    FreeDiskImage();
    return buf;
}
