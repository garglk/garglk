//
//  ti99_disk.c
//  scott
//
//  Reads TI-99/4A disk images: the directory, the sectors of a file,
//  and the object files written by the TI Editor/Assembler.
//

#include <stdlib.h>
#include <string.h>

#include "read_le16.h"
#include "memory_allocation.h"

#include "ti99_disk.h"

/* TI-99/4A disk layout. Sector 0 is the volume header, sector 1 is
   the file index, which holds up to 127 two-byte sector numbers, and
   each of those sectors is a File Descriptor Record (FDR) describing
   one file. */
#define SECTOR_SIZE         0x100
#define FDIR_SECTOR         1
#define FDIR_MAX_ENTRIES    127
#define FDIR_ENTRY_SIZE     2
#define FDR_NAME_LENGTH     10
#define FDR_FLAGS           0x0C
#define FDR_NUM_SECTORS     0x0E
#define FDR_RECORD_LENGTH   0x11
#define FDR_NUM_RECORDS     0x12
#define FDR_DATA_CHAIN      0x1C
#define FDR_CHAIN_ENTRY     3

/* FDR_FLAGS bits: a program (memory image) file, and a file of
   variable-length records. Object files are neither. */
#define FDR_FLAG_PROGRAM    0x01
#define FDR_FLAG_VARIABLE   0x80

/* List the files of a disk image. The list ends with an entry whose
   filename is NULL. Returns NULL if there is no directory. */
ti99_disk_file *TI99ReadDirectory(const uint8_t *dsk, size_t length) {
    if (length < SECTOR_SIZE * 2)
        return NULL;

    const uint8_t *fdir = dsk + FDIR_SECTOR * SECTOR_SIZE;

    int count = 0;
    for (int i = 0; i < FDIR_MAX_ENTRIES; i++) {
        uint16_t fdr_sector = READ_BE_UINT16(fdir + i * FDIR_ENTRY_SIZE);
        if (fdr_sector == 0)
            break;
        count++;
    }

    if (count == 0)
        return NULL;

    ti99_disk_file *files = MemAlloc((count + 1) * sizeof(ti99_disk_file));

    int found = 0;
    for (int i = 0; i < count; i++) {
        uint16_t fdr_sector = READ_BE_UINT16(fdir + i * FDIR_ENTRY_SIZE);
        size_t fdr_offset = (size_t)fdr_sector * SECTOR_SIZE;
        if (fdr_offset + SECTOR_SIZE > length)
            continue;

        char *name = MemAlloc(FDR_NAME_LENGTH + 1);
        memcpy(name, dsk + fdr_offset, FDR_NAME_LENGTH);

        /* Trim trailing spaces */
        int end = FDR_NAME_LENGTH;
        while (end > 0 && name[end - 1] == ' ')
            end--;
        name[end] = '\0';

        files[found].filename = name;
        files[found].fdr_offset = fdr_offset;
        found++;
    }

    /* NULL-terminate by zeroing the sentinel entry */
    memset(&files[found], 0, sizeof(ti99_disk_file));
    return files;
}

void TI99FreeDirectory(ti99_disk_file *files) {
    if (files == NULL)
        return;
    for (int i = 0; files[i].filename != NULL; i++)
        free(files[i].filename);
    free(files);
}

static const ti99_disk_file *FindFile(const ti99_disk_file *files, const char *name) {
    for (int i = 0; files[i].filename != NULL; i++) {
        if (strcmp(files[i].filename, name) == 0)
            return &files[i];
    }
    return NULL;
}

/* Collect the sectors of a file, in order, into one buffer. The FDR
   lists them as runs: each three-byte entry packs the 12-bit number of
   the run's first sector and the 12-bit position within the file of
   its last one. Returns NULL if the file has no sectors inside the
   image; *size is a whole number of sectors. */
static uint8_t *ReadFileSectors(const uint8_t *dsk, size_t length, const ti99_disk_file *file, size_t *size) {
    const uint8_t *fdr = dsk + file->fdr_offset;
    size_t total = READ_BE_UINT16(fdr + FDR_NUM_SECTORS);
    *size = 0;
    if (total == 0)
        return NULL;

    uint8_t *data = MemAlloc(total * SECTOR_SIZE);
    size_t done = 0;
    for (int k = FDR_DATA_CHAIN; k + FDR_CHAIN_ENTRY <= SECTOR_SIZE && done < total; k += FDR_CHAIN_ENTRY) {
        size_t sector = fdr[k] | ((fdr[k + 1] & 0x0f) << 8);
        size_t last = (fdr[k + 1] >> 4) | (fdr[k + 2] << 4);
        if (sector == 0)
            break;
        for (; done <= last && done < total; done++, sector++) {
            if ((sector + 1) * SECTOR_SIZE > length)
                goto out;
            memcpy(data + done * SECTOR_SIZE, dsk + sector * SECTOR_SIZE, SECTOR_SIZE);
        }
    }

out:
    if (done == 0) {
        free(data);
        return NULL;
    }
    *size = done * SECTOR_SIZE;
    return data;
}

/* The pictures, and the program that holds the title text and the
   tile font, are stored on the disk as object files, as written by
   the TI Editor/Assembler, in files of fixed 80-byte records.

   A record is a row of fields, each a one-character tag followed by a
   16-bit value and, for some tags, a name:

     0       module header: length of the relocatable code, 8-character name
     1, 2    entry address, absolute and relocatable
     3, 4    external reference, relocatable and absolute, 6-character name
     5, 6    external definition, relocatable and absolute, 6-character name
     7       checksum of the record so far
     8       checksum to be ignored
     9, A    load address for what follows, absolute and relocatable
     B, C    one word of data, absolute and relocatable
     F       end of record; the rest of it is padding
     :       last record of the file

   The values are four hexadecimal digits, or, in the "compressed"
   form the assembler writes when given the C option, two raw bytes.
   The header tag tells which: it is the character '0' in the plain
   form and the byte 0x01 in the compressed one. */
#define TAG_HEADER_COMPRESSED 0x01
#define TAG_NAME_HEADER       8
#define TAG_NAME_SYMBOL       6

/* The address space of the machine, which is the most a file can fill. */
#define TI99_MEMORY_SIZE 0x10000

/* Read the 16-bit value of a field. Returns 0 if it does not fit in
   the record or is not a number. */
static int ReadTaggedValue(const uint8_t *record, size_t length, size_t *pos, int compressed, uint16_t *value) {
    if (compressed) {
        if (*pos + 2 > length)
            return 0;
        *value = READ_BE_UINT16(record + *pos);
        *pos += 2;
        return 1;
    }

    if (*pos + 4 > length)
        return 0;
    uint16_t result = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t c = record[*pos + i];
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return 0;
        result = (uint16_t)((result << 4) | digit);
    }
    *value = result;
    *pos += 4;
    return 1;
}

/* Load one record into obj. Returns 0 at the record that ends the
   file, 1 otherwise. *address is the load address, carried from one
   record to the next; it is TI99_MEMORY_SIZE until the file sets one. */
static int LoadTaggedRecord(const uint8_t *record, size_t length, int compressed, size_t *address, ti99_object *obj) {
    size_t pos = 0;
    while (pos < length) {
        uint8_t tag = record[pos++];
        size_t name = 0;
        int known = 1;

        switch (tag) {
        case 'F':
            return 1;
        case ':':
            return 0;
        case TAG_HEADER_COMPRESSED:
        case '0':
            /* Only the header tag that matches the file's form counts */
            if ((tag == TAG_HEADER_COMPRESSED) != (compressed != 0))
                known = 0;
            name = TAG_NAME_HEADER;
            break;
        case '3': case '4': case '5': case '6':
            name = TAG_NAME_SYMBOL;
            break;
        case '1': case '2': case '7': case '8':
        case '9': case 'A': case 'B': case 'C':
            break;
        default:
            known = 0;
            break;
        }

        uint16_t value;
        if (!known || !ReadTaggedValue(record, length, &pos, compressed, &value) || pos + name > length) {
            /* Not a field. What is left of the record cannot be
               trusted; the next one says where it loads, so only
               this record's data is lost. */
            obj->damaged++;
            return 1;
        }
        pos += name;

        if (tag == '9' || tag == 'A') {
            *address = value;
        } else if (tag == 'B' || tag == 'C') {
            if (*address + 2 > TI99_MEMORY_SIZE) {
                /* Data before any load address, or off the end of memory */
                obj->damaged++;
                return 1;
            }
            obj->memory[*address] = value >> 8;
            obj->memory[*address + 1] = value & 0xff;
            if (*address < obj->low)
                obj->low = *address;
            *address += 2;
            if (*address > obj->high)
                obj->high = *address;
        }
    }
    return 1;
}

/* Load an object file. Returns 0 if the file is not one or holds no
   data, leaving nothing allocated. */
static int LoadTaggedObject(const uint8_t *dsk, size_t length, const ti99_disk_file *file, ti99_object *obj) {
    memset(obj, 0, sizeof(*obj));

    const uint8_t *fdr = dsk + file->fdr_offset;
    size_t record_length = fdr[FDR_RECORD_LENGTH];
    if ((fdr[FDR_FLAGS] & (FDR_FLAG_PROGRAM | FDR_FLAG_VARIABLE)) || record_length == 0)
        return 0;

    size_t size;
    uint8_t *data = ReadFileSectors(dsk, length, file, &size);
    if (data == NULL)
        return 0;

    int compressed = (data[0] == TAG_HEADER_COMPRESSED);
    if (!compressed && data[0] != '0') {
        free(data);
        return 0;
    }

    obj->memory = MemCalloc(TI99_MEMORY_SIZE);
    obj->low = TI99_MEMORY_SIZE;

    /* Records do not cross sectors: each sector holds as many as fit,
       and the rest of it is unused. Sectors past the file's last record
       can hold stale records, so the ':' record has to stop the reading;
       the record count of the FDR is there in case it is missing. */
    size_t records = READ_LE_UINT16(fdr + FDR_NUM_RECORDS);
    size_t address = TI99_MEMORY_SIZE;
    int more = 1;
    for (size_t sector = 0; more && sector < size; sector += SECTOR_SIZE) {
        for (size_t pos = 0; more && pos + record_length <= SECTOR_SIZE; pos += record_length) {
            if (records-- == 0)
                more = 0;
            else
                more = LoadTaggedRecord(data + sector + pos, record_length, compressed, &address, obj);
        }
    }
    free(data);

    if (obj->high <= obj->low) {
        free(obj->memory);
        memset(obj, 0, sizeof(*obj));
        return 0;
    }
    return 1;
}

/* Load the object file of that name. Returns 0 if it is missing or
   is not an object file. On success the caller calls TI99FreeObject(). */
int TI99LoadObject(const uint8_t *dsk, size_t length, const ti99_disk_file *files, const char *name, ti99_object *obj) {
    const ti99_disk_file *file = FindFile(files, name);
    return file != NULL && LoadTaggedObject(dsk, length, file, obj);
}

/* Copy length bytes of what an object file loads from address on.
   Bytes past the last one it loads are returned as zeros. Returns
   NULL if it loads nothing there. */
uint8_t *TI99CopyObjectBytes(const ti99_object *obj, size_t address, size_t length) {
    if (address < obj->low || address >= obj->high)
        return NULL;
    uint8_t *result = MemCalloc(length);
    size_t available = obj->high - address;
    memcpy(result, obj->memory + address, length < available ? length : available);
    return result;
}

void TI99FreeObject(ti99_object *obj) {
    free(obj->memory);
    memset(obj, 0, sizeof(*obj));
}
