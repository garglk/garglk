// vim: set ft=c:

#ifndef ZTERP_IO_H
#define ZTERP_IO_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct zterp_io zterp_io;

enum zterp_io_mode
{
  ZTERP_IO_RDONLY,
  ZTERP_IO_WRONLY,
  ZTERP_IO_APPEND,
};

enum zterp_io_purpose
{
  ZTERP_IO_DATA,
  ZTERP_IO_SAVE,
  ZTERP_IO_TRANS,
  ZTERP_IO_INPUT,
};

zterp_io *zterp_io_open(const char *, enum zterp_io_mode, enum zterp_io_purpose);
zterp_io *zterp_io_open_memory(const void *, size_t);
zterp_io *zterp_io_stdin(void);
zterp_io *zterp_io_stdout(void);
void zterp_io_close(zterp_io *);
bool zterp_io_close_memory(zterp_io *, uint8_t **, long *);
bool zterp_io_try(zterp_io *);
bool zterp_io_seek(zterp_io *, long, int);
long zterp_io_tell(zterp_io *);
size_t zterp_io_read(zterp_io *, void *, size_t);
bool zterp_io_read_exact(zterp_io *, void *, size_t);
size_t zterp_io_write(zterp_io *, const void *, size_t);
bool zterp_io_write_exact(zterp_io *, const void *, size_t);
bool zterp_io_read8(zterp_io *, uint8_t *);
bool zterp_io_read16(zterp_io *, uint16_t *);
bool zterp_io_read32(zterp_io *, uint32_t *);
bool zterp_io_write8(zterp_io *, uint8_t);
bool zterp_io_write16(zterp_io *, uint16_t);
bool zterp_io_write32(zterp_io *, uint32_t);
long zterp_io_getc(zterp_io *);
void zterp_io_putc(zterp_io *, uint16_t);
long zterp_io_readline(zterp_io *, uint16_t *, size_t);
long zterp_io_filesize(zterp_io *);
void zterp_io_flush(zterp_io *);

#endif
