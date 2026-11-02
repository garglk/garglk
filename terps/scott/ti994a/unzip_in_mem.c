//
//  unzip_in_mem.c
//  scott
//
//  Created by Administrator on 2026-02-18.
//
//  Extracts one file from a zip archive held in memory. It is used for
//  the Return to Pirate's Isle cartridge images, and reads no more of
//  the format than they need: files that are stored or deflated, in an
//  archive without the Zip64 extensions.
//

#include <stdlib.h>
#include <string.h>
#include <strings.h>
/* Makes z_stream.next_in a pointer to const */
#define ZLIB_CONST
#include <zlib.h>

#include "memory_allocation.h"
#include "read_le16.h"
#include "unzip_in_mem.h"

/* The record at the end of the archive that says where the directory is */
#define END_RECORD_SIZE       22
#define END_RECORD_ENTRIES    10
#define END_RECORD_DIR_OFFSET 16
/* It can be followed by a comment of up to 65535 bytes */
#define END_RECORD_MAX_COMMENT 0xffff

/* A directory entry: fixed fields, then the name, extra field and comment */
#define DIR_ENTRY_SIZE         46
#define DIR_ENTRY_METHOD       10
#define DIR_ENTRY_CRC          16
#define DIR_ENTRY_PACKED_SIZE  20
#define DIR_ENTRY_SIZE_FIELD   24
#define DIR_ENTRY_NAME_LENGTH  28
#define DIR_ENTRY_EXTRA_LENGTH 30
#define DIR_ENTRY_NOTE_LENGTH  32
#define DIR_ENTRY_FILE_OFFSET  42

/* The header in front of a file's data: fixed fields, name, extra field */
#define FILE_HEADER_SIZE         30
#define FILE_HEADER_NAME_LENGTH  26
#define FILE_HEADER_EXTRA_LENGTH 28

#define METHOD_STORED   0
#define METHOD_DEFLATED 8

/* More than any cartridge holds; keeps a corrupt size field from
   asking for gigabytes. */
#define MAX_UNZIPPED_SIZE 0x1000000

/* Find the end record, which is the last thing in the archive but
   for its comment. Returns NULL if there is none. */
static const uint8_t *FindEndRecord(const uint8_t *zip, size_t size) {
    if (size < END_RECORD_SIZE)
        return NULL;
    size_t pos = size - END_RECORD_SIZE;
    size_t lowest = pos > END_RECORD_MAX_COMMENT ? pos - END_RECORD_MAX_COMMENT : 0;
    for (;; pos--) {
        if (memcmp(zip + pos, "PK\x05\x06", 4) == 0)
            return zip + pos;
        if (pos == lowest)
            return NULL;
    }
}

/* Find the directory entry of the file with that name, compared
   without regard to case. Returns NULL if there is none. */
static const uint8_t *FindDirEntry(const uint8_t *zip, size_t size, const char *name) {
    const uint8_t *end = FindEndRecord(zip, size);
    if (end == NULL)
        return NULL;

    size_t entries = READ_LE_UINT16(end + END_RECORD_ENTRIES);
    size_t pos = READ_LE_UINT32(end + END_RECORD_DIR_OFFSET);
    size_t name_length = strlen(name);

    for (size_t i = 0; i < entries; i++) {
        if (pos > size || size - pos < DIR_ENTRY_SIZE || memcmp(zip + pos, "PK\x01\x02", 4) != 0)
            return NULL;
        const uint8_t *entry = zip + pos;
        size_t length = READ_LE_UINT16(entry + DIR_ENTRY_NAME_LENGTH);
        size_t next = pos + DIR_ENTRY_SIZE + length
            + READ_LE_UINT16(entry + DIR_ENTRY_EXTRA_LENGTH)
            + READ_LE_UINT16(entry + DIR_ENTRY_NOTE_LENGTH);
        if (next > size)
            return NULL;
        if (length == name_length && strncasecmp((const char *)entry + DIR_ENTRY_SIZE, name, length) == 0)
            return entry;
        pos = next;
    }
    return NULL;
}

/* Inflate packed_size bytes of raw deflate data into exactly
   unpacked_size bytes. Returns 0 if it does not come out that way. */
static int Inflate(const uint8_t *packed, size_t packed_size, uint8_t *unpacked, size_t unpacked_size) {
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    /* Negative window bits: no zlib header or checksum around the data */
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return 0;
    stream.next_in = packed;
    stream.avail_in = (uInt)packed_size;
    stream.next_out = unpacked;
    stream.avail_out = (uInt)unpacked_size;
    int result = inflate(&stream, Z_FINISH);
    int ok = (result == Z_STREAM_END && stream.avail_out == 0);
    inflateEnd(&stream);
    return ok;
}

/* Extract the file named filename_to_extract from the zip archive at
   zipdata. Returns a newly allocated buffer with its contents, which
   the caller frees, and sets *unzipped_size to its length. Returns
   NULL, with *unzipped_size 0, if the archive has no such file, the
   file is empty, or it cannot be unpacked intact. */
uint8_t *extract_file_from_zip_data(const uint8_t *zipdata, size_t zipdatasize, const char *filename_to_extract, size_t *unzipped_size) {
    *unzipped_size = 0;

    const uint8_t *entry = FindDirEntry(zipdata, zipdatasize, filename_to_extract);
    if (entry == NULL)
        return NULL;

    /* The sizes and checksum are taken from the directory; the copies
       in the file's own header can be left blank by the archiver. */
    int method = READ_LE_UINT16(entry + DIR_ENTRY_METHOD);
    uint32_t crc = READ_LE_UINT32(entry + DIR_ENTRY_CRC);
    size_t packed_size = READ_LE_UINT32(entry + DIR_ENTRY_PACKED_SIZE);
    size_t size = READ_LE_UINT32(entry + DIR_ENTRY_SIZE_FIELD);
    size_t header = READ_LE_UINT32(entry + DIR_ENTRY_FILE_OFFSET);

    if (size == 0 || size > MAX_UNZIPPED_SIZE)
        return NULL;
    if (header > zipdatasize || zipdatasize - header < FILE_HEADER_SIZE || memcmp(zipdata + header, "PK\x03\x04", 4) != 0)
        return NULL;

    size_t data = header + FILE_HEADER_SIZE
        + READ_LE_UINT16(zipdata + header + FILE_HEADER_NAME_LENGTH)
        + READ_LE_UINT16(zipdata + header + FILE_HEADER_EXTRA_LENGTH);
    if (data > zipdatasize || zipdatasize - data < packed_size)
        return NULL;

    uint8_t *result = MemAlloc(size);
    int ok;
    if (method == METHOD_STORED) {
        ok = (packed_size == size);
        if (ok)
            memcpy(result, zipdata + data, size);
    } else if (method == METHOD_DEFLATED) {
        ok = Inflate(zipdata + data, packed_size, result, size);
    } else {
        ok = 0;
    }

    if (!ok || crc32(crc32(0, NULL, 0), result, (uInt)size) != crc) {
        free(result);
        return NULL;
    }

    *unzipped_size = size;
    return result;
}
