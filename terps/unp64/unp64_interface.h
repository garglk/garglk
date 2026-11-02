/* Unp64 ScummVM version C interface written by Petter Sjölund */

#ifndef UNP64_INTERFACE_H
#define UNP64_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* How much unp64() may write to destinationBuffer. The interface has no
 * capacity parameter: the buffer has to be at least this big. */
#define UNP64_OUTPUT_CAPACITY 0x10000

/* Unpack a crunched C64 program.
 *
 *   compressed         the program file: two bytes of load address, then data
 *   length             its size in bytes
 *   destinationBuffer  receives the unpacked program, in the same format;
 *                      at least UNP64_OUTPUT_CAPACITY bytes
 *   finalLength        out: how many bytes were written
 *   settings           UNP64 command line switches, separated by spaces, or
 *                      NULL: "-e<addr>" entry point, "-d<addr>" address at
 *                      which unpacking is done, "-r<addr>" return address,
 *                      "-t<addr>" end address, "-f<fill>" ... Numbers are
 *                      decimal, or hex with a $ or 0x prefix. At most four
 *                      switches of less than 64 characters each.
 *
 * Returns nonzero on success. Returns 0, with nothing useful in the buffer,
 * if the program is not packed with anything that is recognised, if the
 * emulation of the unpacker runs off the rails, or if the settings are
 * malformed (too many, too long, or not a number where one is expected).
 *
 * The biggest possible output is all of memory from $0002 up (the -a
 * switch, "dump everything") with its load address in front, which is
 * exactly UNP64_OUTPUT_CAPACITY bytes.
 *
 * Reentrant: there is no state outside the call. */
int unp64(const uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t *finalLength, const char *settings);

#ifdef __cplusplus
}
#endif

#endif
