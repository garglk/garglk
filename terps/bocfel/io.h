// vim: set ft=c:

#ifndef ZTERP_IO_H
#define ZTERP_IO_H

#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct zterp_io zterp_io;

enum zterp_io_mode {
    ZTERP_IO_MODE_RDONLY,
    ZTERP_IO_MODE_WRONLY,
    ZTERP_IO_MODE_APPEND,
};

enum zterp_io_purpose {
    ZTERP_IO_PURPOSE_DATA,
    ZTERP_IO_PURPOSE_SAVE,
    ZTERP_IO_PURPOSE_TRANS,
    ZTERP_IO_PURPOSE_INPUT,
};

void zterp_io_set_exception_mode(zterp_io *io, bool mode);
jmp_buf *zterp_io_get_exception(zterp_io *io);

// Turn on “exception handling” for this I/O object. After calling this
// macro, all future I/O calls which return a bool will now instead
// cause this macro to set “ok” to false in case of error. Simple usage:
//
// bool ok;
// zterp_io_try(io, ok);
// if (!ok) {
//     return ERROR;
// }
// // call I/O functions here
//
// This *only* affects boolean functions. Those which return non-boolean
// values, such as zterp_io_write() and zterp_io_getc() are unaffected.
// This may be inconsistent, at least for zterp_io_getc(), which can
// return a failure condition, but non-boolean functions are not
// currently used with exception handling, so it’s not relevant.
#define zterp_io_try(io, ok) do { \
    zterp_io_set_exception_mode(io, true); \
    if (setjmp(*zterp_io_get_exception(io)) != 0) { \
        ok = false; \
    } else { \
        ok = true; \
    } \
} while (false)

zterp_io *zterp_io_open(const char *filename, enum zterp_io_mode mode, enum zterp_io_purpose purpose);
zterp_io *zterp_io_open_memory(const void *buf, size_t n);
zterp_io *zterp_io_stdin(void);
zterp_io *zterp_io_stdout(void);
void zterp_io_close(zterp_io *io);
bool zterp_io_close_memory(zterp_io *io, uint8_t **buf, long *n);
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
bool zterp_io_putc(zterp_io *io , uint16_t c);
long zterp_io_readline(zterp_io *io, uint16_t *buf, size_t len);
long zterp_io_filesize(zterp_io *io);
void zterp_io_flush(zterp_io *io);

#endif
