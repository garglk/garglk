#ifndef ZTERP_IO_H
#define ZTERP_IO_H

#include <stdio.h>
#include <stdint.h>

typedef struct zterp_io zterp_io;

#define ZTERP_IO_DATA		0x00
#define ZTERP_IO_SAVE		0x01
#define ZTERP_IO_TRANS		0x02
#define ZTERP_IO_INPUT		0x04
#define ZTERP_IO_RDONLY		0x08
#define ZTERP_IO_WRONLY		0x10
#define ZTERP_IO_APPEND		0x20

/* This variable controls whether the IO system writes UTF-8 or
 * Latin-1; it is distinct from Glk’s Unicode setting.
 * If this is set, transcripts will be written in UTF-8, and if
 * Glk is not being used, screen output will be written in UTF-8.
 */
extern int use_utf8_io;

zterp_io *zterp_io_open(const char *, int);
const zterp_io *zterp_io_stdin(void);
const zterp_io *zterp_io_stdout(void);
void zterp_io_close(zterp_io *);
int zterp_io_seek(const zterp_io *, long, int);
long zterp_io_tell(const zterp_io *);
size_t zterp_io_read(const zterp_io *, void *, size_t);
size_t zterp_io_write(const zterp_io *, const void *, size_t);
int zterp_io_read16(const zterp_io *, uint16_t *);
int zterp_io_read32(const zterp_io *, uint32_t *);
long zterp_io_getc(const zterp_io *);
void zterp_io_putc(const zterp_io *, uint16_t);
long zterp_io_readline(const zterp_io *, uint16_t *, size_t);
long zterp_io_filesize(const zterp_io *);
void zterp_io_flush(const zterp_io *);

#endif
