//
//  unzip_in_mem.h
//  scott
//
//  Created by Administrator on 2026-02-18.
//

// Extracts one file from a zip archive held in memory. It is used for
// the Return to Pirate's Isle cartridge images, and reads no more of the
// format than they need: files that are stored or deflated, in an
// archive without the Zip64 extensions.

#ifndef unzip_in_mem_h
#define unzip_in_mem_h

#include <stddef.h>
#include <stdint.h>

uint8_t *extract_file_from_zip_data(const uint8_t *zipdata, size_t zipdatasize, const char *filename_to_extract, size_t *unzipped_size);

#endif /* unzip_in_mem_h */
