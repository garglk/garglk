// vim: set ft=c:

#ifndef ZTERP_IO_H
#define ZTERP_IO_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct zterp_io zterp_io;

enum zterp_io_mode {
    ZTERP_IO_RDONLY,
    ZTERP_IO_WRONLY,
    ZTERP_IO_APPEND,
};

enum zterp_io_purpose {
    ZTERP_IO_DATA,
    ZTERP_IO_SAVE,
    ZTERP_IO_TRANS,
    ZTERP_IO_INPUT,
};

zterp_io *zterp_io_open(const char *filename, enum zterp_io_mode mode, enum zterp_io_purpose purpose);
zterp_io *zterp_io_open_memory(const void *buf, size_t n);
zterp_io *zterp_io_stdin(void);
zterp_io *zterp_io_stdout(void);
void zterp_io_close(zterp_io *io);
bool zterp_io_close_memory(zterp_io *io, uint8_t **buf, long *n);
bool zterp_io_try(zterp_io *io);
bool zterp_io_seek(zterp_io *io, long offset, int whence);
long zterp_io_tell(zterp_io *io);
size_t zterp_io_read(zterp_io *io, void *buf, size_t n);
bool zterp_io_read_exact(zterp_io *io, void *buf, size_t n);
size_t zterp_io_write(zterp_io *io, const void *buf, size_t n);
bool zterp_io_write_exact(zterp_io *io, const void *buf, size_t n);
bool zterp_io_read8(zterp_io *io, uint8_t *v);
bool zterp_io_read16(zterp_io *io, uint16_t *v);
bool zterp_io_read32(zterp_io *io, uint32_t *v);
bool zterp_io_write8(zterp_io *io, uint8_t v);
bool zterp_io_write16(zterp_io *io, uint16_t v);
bool zterp_io_write32(zterp_io *io, uint32_t v);
long zterp_io_getc(zterp_io *io);
void zterp_io_putc(zterp_io *io , uint16_t c);
long zterp_io_readline(zterp_io *io, uint16_t *buf, size_t len);
long zterp_io_filesize(zterp_io *io);
void zterp_io_flush(zterp_io *io);

#endif
