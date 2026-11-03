//
//  ti99_disk.h
//  scott
//
//  Reads TI-99/4A disk images: the directory, the sectors of a file,
//  and the object files written by the TI Editor/Assembler.
//

#ifndef ti99_disk_h
#define ti99_disk_h

#include <stddef.h>
#include <stdint.h>

typedef struct {
    /* File name, with the trailing spaces removed */
    char *filename;
    /* Byte offset in the disk image of the file's FDR */
    size_t fdr_offset;
} ti99_disk_file;

typedef struct {
    /* What the file loads, at the addresses it asks for. Relocatable
       code is placed as if loaded at address 0, and its relocatable
       words are left unadjusted. */
    uint8_t *memory;
    /* The lowest address loaded, and one past the highest */
    size_t low;
    size_t high;
    /* The number of records cut short by something that is no tag */
    int damaged;
} ti99_object;

/* List the files of a disk image. The list ends with an entry whose
   filename is NULL. Returns NULL if there is no directory. */
ti99_disk_file *TI99ReadDirectory(const uint8_t *dsk, size_t length);
void TI99FreeDirectory(ti99_disk_file *files);

/* Load the object file of that name. Returns 0 if it is missing or
   is not an object file. On success the caller calls TI99FreeObject(). */
int TI99LoadObject(const uint8_t *dsk, size_t length, const ti99_disk_file *files, const char *name, ti99_object *obj);
void TI99FreeObject(ti99_object *obj);

/* Copy length bytes of what an object file loads from address on.
   Bytes past the last one it loads are returned as zeros. Returns
   NULL if it loads nothing there. */
uint8_t *TI99CopyObjectBytes(const ti99_object *obj, size_t address, size_t length);

#endif /* ti99_disk_h */
