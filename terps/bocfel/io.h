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
const zterp_io *zterp_io_stdin(void);
const zterp_io *zterp_io_stdout(void);
void zterp_io_close(zterp_io *);
bool zterp_io_seek(const zterp_io *, long, int);
long zterp_io_tell(const zterp_io *);
size_t zterp_io_read(const zterp_io *, void *, size_t);
size_t zterp_io_write(const zterp_io *, const void *, size_t);
bool zterp_io_read16(const zterp_io *, uint16_t *);
bool zterp_io_read32(const zterp_io *, uint32_t *);
long zterp_io_getc(const zterp_io *);
void zterp_io_putc(const zterp_io *, uint16_t);
long zterp_io_readline(const zterp_io *, uint16_t *, size_t);
long zterp_io_filesize(const zterp_io *);
void zterp_io_flush(const zterp_io *);

#endif
