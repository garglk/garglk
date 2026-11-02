//
//  stdetect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-02.
//

#define MAX_ST_LENGTH 368640
#define MIN_ST_LENGTH 213610

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "definitions.h"
#include "graphics.h"
#include "read_le16.h"

#include "stdetect.h"

// clang-format off
typedef struct {
    uint16_t    sector_size;    /* bytes per logical sector */
    uint8_t     sec_per_clus;   /* sectors/cluster */
    uint16_t    reserved;       /* reserved sectors */
    uint8_t     fats;           /* number of FATs */
    uint16_t    dir_entries;    /* root directory entries */
    uint16_t    sectors;        /* number of sectors */
    uint8_t     media;          /* media code */
    uint16_t    fat_length;     /* sectors/FAT */
    uint16_t    secs_track;     /* sectors per track */
    uint32_t    heads;          /* number of heads */
    uint32_t    total_sect;     /* number of sectors (if sectors == 0) */

    uint32_t    fat32_length;   /* sectors for FAT32 */
    uint16_t    flags;          /* bit 8: fat mirroring, low 4: active fat */
    uint8_t     boot_signature; /* Boot signature */
    uint32_t    volume_id;      /* Volume id */
    uint8_t     vol_label[12];  /* Volume label */
    uint8_t     system_type;    /* filesystem type */
    uint16_t    backup_boot;    /* backup boot sector */
} fat_boot_sector;

#define MSDOS_NAME 11

typedef struct {
    char      name[MSDOS_NAME];/* name and extension */
    uint8_t   attr;            /* attribute bits */
    uint8_t   lcase;           /* Case for base and extension */
    uint8_t   ctime_cs;        /* Creation time, centiseconds (0-199) */
    uint16_t  ctime;           /* Creation time */
    uint16_t  cdate;           /* Creation date */
    uint16_t  adate;           /* Last access date */
    uint16_t  starthi;         /* High 16 bits of cluster in FAT32 */
    uint16_t  time,date,start; /* time, date and first cluster */
    uint32_t  size;            /* file size (in bytes) */
} msdos_dir_entry;
// clang-format on

static fat_boot_sector boot;

typedef struct {
    int sectorsize;
    int starttrack;
    int endtrack;
    int sectors_per_track;
    int numheads;
    int numtracks;
    int totalsectors;
} MsaImageInfo;

/* Read the header of an MSA image. Returns 0 if it does not describe a
   floppy we could expect. */
static int ReadMsaImageInfo(MsaImageInfo *msa_image_info, uint8_t *msa_image)
{
    msa_image += 2;
    msa_image_info->sectorsize = 512;
    msa_image_info->sectors_per_track = READ_BE_UINT16_AND_ADVANCE(&msa_image);
    msa_image_info->numheads = READ_BE_UINT16_AND_ADVANCE(&msa_image) + 1;
    msa_image_info->starttrack = READ_BE_UINT16_AND_ADVANCE(&msa_image);
    msa_image_info->endtrack = READ_BE_UINT16_AND_ADVANCE(&msa_image);
    msa_image_info->numtracks = msa_image_info->endtrack + 1;
    if (msa_image_info->sectors_per_track < 1 || msa_image_info->sectors_per_track > 64 ||
        msa_image_info->numheads > 2 || msa_image_info->starttrack > msa_image_info->endtrack ||
        msa_image_info->numtracks > 100)
        return 0;
    msa_image_info->totalsectors = msa_image_info->numtracks * msa_image_info->sectors_per_track * msa_image_info->numheads;
    return 1;
}

/* Unpack an MSA image of size bytes into a raw disk image of *newsize
   bytes. Each track is either stored as it is or run-length encoded with
   0xe5 as the escape byte. Returns NULL if the data does not fit its own
   header. */
static uint8_t *DecodeMsaImageToRawImage(uint8_t *msa_image, size_t size, size_t *newsize)
{
    MsaImageInfo msa_image_info;

    if (size < 10 || !ReadMsaImageInfo(&msa_image_info, msa_image))
        return NULL;
    size_t tracksize = (size_t)msa_image_info.sectors_per_track * msa_image_info.sectorsize;
    *newsize = msa_image_info.totalsectors * msa_image_info.sectorsize;
    uint8_t *raw_image = MemCalloc(*newsize);

    uint8_t *msa_pointer = msa_image + 10;
    uint8_t *msa_end = msa_image + size;

    for (int trackindex = msa_image_info.starttrack; trackindex <= msa_image_info.endtrack; trackindex++) {
        for (int headindex = 0; headindex < msa_image_info.numheads; headindex++) {
            uint8_t *raw_pointer = raw_image + ((size_t)trackindex * msa_image_info.numheads + headindex) * tracksize;
            uint8_t *raw_end = raw_pointer + tracksize;

            if (msa_end - msa_pointer < 2)
                goto bad;
            size_t numbytes = READ_BE_UINT16_AND_ADVANCE(&msa_pointer);
            if (numbytes > (size_t)(msa_end - msa_pointer))
                goto bad;
            uint8_t *end_pointer = msa_pointer + numbytes;

            if (numbytes < tracksize) {
                while (msa_pointer < end_pointer) {
                    uint8_t msa_data = *msa_pointer++;
                    size_t rle_count = 1;

                    if (msa_data == 0xe5) {
                        if (end_pointer - msa_pointer < 3)
                            goto bad;
                        msa_data = *msa_pointer++;
                        rle_count = READ_BE_UINT16_AND_ADVANCE(&msa_pointer);
                    }
                    if (rle_count > (size_t)(raw_end - raw_pointer))
                        goto bad;
                    memset(raw_pointer, msa_data, rle_count);
                    raw_pointer += rle_count;
                }
            } else {
                memcpy(raw_pointer, msa_pointer, tracksize);
                msa_pointer = end_pointer;
            }
        }
    }

    return raw_image;

bad:
    debug_print("DecodeMsaImageToRawImage: damaged MSA image\n");
    free(raw_image);
    return NULL;
}

/* Size of a FAT directory entry on disk */
#define DIR_ENTRY_SIZE 32

/* Byte offset of the root directory, after the FATs */
static size_t RootDirOffset(void)
{
    return (size_t)(boot.reserved + boot.fats * boot.fat_length) * boot.sector_size;
}

/* Read the BIOS parameter block of the boot sector into boot. Returns 0
   if it cannot be that of a FAT12 floppy of the given size. */
static int ReadFAT12BootSector(uint8_t *disk, size_t disksize)
{
    if (disksize < 512)
        return 0;
    uint8_t *ptr = &disk[11];
    boot.sector_size = READ_LE_UINT16_AND_ADVANCE(&ptr);
    boot.sec_per_clus = *ptr++;
    boot.reserved = READ_LE_UINT16_AND_ADVANCE(&ptr);
    boot.fats = *ptr++;
    boot.dir_entries = READ_LE_UINT16_AND_ADVANCE(&ptr);
    boot.sectors = READ_LE_UINT16_AND_ADVANCE(&ptr);
    boot.media = *ptr++;
    boot.fat_length = READ_LE_UINT16_AND_ADVANCE(&ptr);
    boot.secs_track = READ_LE_UINT16_AND_ADVANCE(&ptr);
    boot.heads = READ_LE_UINT16_AND_ADVANCE(&ptr);
    ptr += 10;
    boot.boot_signature = *ptr++;
    ptr += 4;
    for (int i = 0; i < 11; i++) {
        boot.vol_label[i] = *ptr++;
    }
    boot.vol_label[11] = 0;

    if (boot.sector_size == 0 || boot.sec_per_clus == 0)
        return 0;
    if (RootDirOffset() + (size_t)boot.dir_entries * DIR_ENTRY_SIZE > disksize)
        return 0;
    return 1;
}

static msdos_dir_entry *ReadFAT12DirEntry(uint8_t **pointer)
{
    uint8_t *ptr = *pointer;
    msdos_dir_entry *dir = MemAlloc(sizeof(msdos_dir_entry));
    memcpy(dir->name, ptr, MSDOS_NAME);
    ptr += MSDOS_NAME;
    dir->attr = *ptr;
    ptr += 3;
    dir->ctime = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->cdate = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->adate = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->starthi = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->time = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->date = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->start = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->size = READ_LE_UINT16_AND_ADVANCE(&ptr);
    dir->size += ((uint32_t)READ_LE_UINT16_AND_ADVANCE(&ptr) << 16);
    *pointer = ptr;
    return dir;
}

/*
 * Return the next cluster in the chain of the provided cluster from the FAT.
 * If the cluster you find is out of bounds, return 0
 *
 * For FAT12, two entries are stored in 3 bytes. if the bytes are uv, wx, yz then the entries are xuv and yzw
 */
static uint32_t get_next_cluster12(const uint8_t *sf, size_t disksize, uint32_t cluster)
{
    uint8_t fat_hi, fat_lo;
    uint32_t new_clust;

    size_t fat_offset = (size_t)boot.reserved * boot.sector_size;
    size_t entry = fat_offset + ((cluster / 2) * 3);
    if (entry + 3 > disksize)
        return 0;
    // If the cluster number is odd, we're getting the last half of the three bytes.
    if (cluster & 1) {
        fat_lo = sf[entry + 1];
        fat_hi = sf[entry + 2];
        new_clust = (fat_hi << 4) | (fat_lo >> 4);
    } else { // If it's even we're getting the first half.
        fat_lo = sf[entry];
        fat_hi = sf[entry + 1] & 0x0f;
        new_clust = (fat_hi << 8) | fat_lo;
    }
    // for FAT12, the valid entries are between 0x2 and 0xfef.
    if ((new_clust >= 0x02) && (new_clust <= 0xfef)) {
        return new_clust;
    }
    return 0;
}


/* The most pictures we take from one disk */
#define MAX_ST_IMAGES 100

/* The deepest we follow subdirectories */
#define MAX_DIR_DEPTH 8

/* What a walk through the directories of a disk has found so far */
typedef struct {
    uint8_t *disk;
    size_t disksize;
    imgrec imgs[MAX_ST_IMAGES];
    int numimgs;
    uint8_t *database; /* The contents of the file DATABASE, or NULL */
    size_t databasesize;
    size_t entries_left; /* Directory entries we may still read */
} STScan;

/* The length bytes of a cluster in the disk image, or NULL if they are not
   all on the disk. Cluster numbering starts at 2, at the beginning of the
   data section after the FATs and the root directory. */
static uint8_t *ClusterData(const STScan *scan, uint32_t cluster, size_t length)
{
    if (cluster < 2)
        return NULL;
    size_t datasection = boot.reserved + boot.fats * boot.fat_length + (boot.dir_entries * DIR_ENTRY_SIZE / boot.sector_size);
    size_t offset = (datasection + (size_t)(cluster - 2) * boot.sec_per_clus) * boot.sector_size;
    if (offset > scan->disksize || length > scan->disksize - offset)
        return NULL;
    return scan->disk + offset;
}

/* Read a file into a new buffer of dir->size bytes, by following its
   cluster chain. What is not on the disk is left as zeros. */
static uint8_t *GetFile(const STScan *scan, const msdos_dir_entry *dir)
{
    size_t bytespercluster = (size_t)boot.sector_size * boot.sec_per_clus;
    uint8_t *result = MemCalloc(dir->size ? dir->size : 1);
    size_t offset = 0;
    uint32_t cluster = dir->start;
    while (offset < dir->size && cluster) {
        size_t bytestoread = MIN(bytespercluster, dir->size - offset);
        uint8_t *data = ClusterData(scan, cluster, bytestoread);
        if (data == NULL)
            break;
        memcpy(result + offset, data, bytestoread);
        offset += bytestoread;
        cluster = get_next_cluster12(scan->disk, scan->disksize, cluster);
    }

    return result;
}

/* Read the directory entry at ptr and take note of it in scan if it is
   the game database or a picture. A subdirectory is walked through in
   turn. Returns a pointer to the next entry, or NULL if this was the
   end of the directory. No more entries are read than the disk can hold,
   which ends the walk on a damaged disk whose cluster chains or
   directories go round in circles. */
static uint8_t *ReadDirEntryRecursive(uint8_t *ptr, STScan *scan, int depth)
{
    if (ptr == NULL || (size_t)(ptr - scan->disk) + DIR_ENTRY_SIZE > scan->disksize || scan->entries_left == 0)
        return NULL;
    scan->entries_left--;
    msdos_dir_entry *dir = ReadFAT12DirEntry(&ptr);
    if (dir->name[0] == 0) {
        free(dir);
        return NULL;
    }
    dir->name[8] = 0;
    if (dir->attr & 0x10 && dir->name[0] != '.') {
        uint32_t cluster = dir->start;
        int lastfound = (depth >= MAX_DIR_DEPTH);
        size_t entries_per_cluster = (size_t)boot.sector_size * boot.sec_per_clus / DIR_ENTRY_SIZE;
        while (cluster && !lastfound) {
            uint8_t *subdirentry = ClusterData(scan, cluster, entries_per_cluster * DIR_ENTRY_SIZE);
            if (subdirentry == NULL)
                break;
            for (size_t i = 0; i < entries_per_cluster; i++) {
                subdirentry = ReadDirEntryRecursive(subdirentry, scan, depth + 1);
                if (subdirentry == NULL) {
                    lastfound = 1;
                    break;
                }
            }
            cluster = get_next_cluster12(scan->disk, scan->disksize, cluster);
        }
    } else if (strncmp(dir->name, "DATABASE", 8) == 0) {
        uint8_t *file = GetFile(scan, dir);
        /* The two bytes at the head match the other systems' files */
        free(scan->database);
        scan->database = MemCalloc(dir->size + 2);
        memcpy(scan->database + 2, file, dir->size);
        scan->databasesize = dir->size + 2;
        free(file);
    } else if (issagaimg(dir->name) && scan->numimgs < MAX_ST_IMAGES) {
        imgrec *img = &scan->imgs[scan->numimgs++];
        img->Data = GetFile(scan, dir);
        img->Filename = MemAlloc(IMAGE_NAME_SIZE);
        memcpy(img->Filename, dir->name, IMAGE_NAME_SIZE - 1);
        img->Filename[IMAGE_NAME_SIZE - 1] = 0;
        img->Size = dir->size;
    }
    free(dir);
    return ptr;
}

int DetectST(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_ST_LENGTH || *extent < MIN_ST_LENGTH)
        return 0;

    if ((*sf)[0] == 0x0e && (*sf)[1] == 0x0f) {
        size_t rawsize;
        uint8_t *raw = DecodeMsaImageToRawImage(*sf, *extent, &rawsize);
        if (raw == NULL)
            return 0;
        free(*sf);
        *sf = raw;
        *extent = rawsize;
    }

    if (!ReadFAT12BootSector(*sf, *extent))
        return 0;

    /* Too big for the stack */
    STScan *scan = MemCalloc(sizeof(STScan));
    scan->disk = *sf;
    scan->disksize = *extent;
    scan->entries_left = *extent / DIR_ENTRY_SIZE;

    uint8_t *ptr = *sf + RootDirOffset();
    for (int i = 0; i < boot.dir_entries; i++) {
        ptr = ReadDirEntryRecursive(ptr, scan, 0);
        if (ptr == NULL)
            break;
    }

    int found = (scan->database != NULL);
    if (found) {
        Images = NewImageList(scan->numimgs);
        memcpy(Images, scan->imgs, scan->numimgs * sizeof(imgrec));
        AdoptDataFile(sf, extent, scan->database, scan->databasesize, SYS_ST);
    } else {
        for (int i = 0; i < scan->numimgs; i++) {
            free(scan->imgs[i].Data);
            free(scan->imgs[i].Filename);
        }
    }
    free(scan);
    return found;
}
