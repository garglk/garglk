/* vi: set ts=8:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * Written from a reading of the ADRIFT 5 Runner source as released in
 * FrankenDrift, to reproduce its behaviour; no code from it is used.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * ADRIFT 5 support for Scarier -- de-obfuscation + zlib inflate.
 *
 * An ADRIFT 5 game payload (the body of the Blorb 'ADRI' Exec chunk, or a bare
 * .taf) is a zlib stream that has been XOR'd against a fixed 1024-byte key,
 * repeating.  Undo that, inflate, and you get UTF-8 XML.
 *
 * Borrowed concept: the Adrift 5 runner FileIO.ObfuscateByteArray / Decompress.
 */

#ifndef SCARIER_A5DEOBF_H
#define SCARIER_A5DEOBF_H

#include <stddef.h>
#include <stdint.h>

/*
 * XOR a region of data in place against the ADRIFT 5 1024-byte key.  The key
 * index is reset at the start of the region, i.e. key[(i - offset) % 1024].
 */
extern void a5_deobfuscate (uint8_t *data, uint32_t offset, uint32_t length);

/*
 * Inflate a zlib stream into a freshly malloc'd buffer (caller frees with
 * free()).  Returns the buffer and writes its length to *out_size, or NULL on
 * error.  The result is NUL-terminated for convenience (the NUL is not counted
 * in *out_size).
 */
extern uint8_t *a5_inflate (const uint8_t *data, uint32_t length,
                            uint32_t *out_size);

/*
 * Deflate a buffer into a freshly malloc'd RFC-1950 zlib stream (0x78 header +
 * DEFLATE + Adler-32 trailer), the framing the Adrift 5 runner wraps its saved games in
 * (FileIO.SaveState -> ZLibStream(CompressionLevel.Optimal)).  Returns the buffer
 * and writes its length to *out_size, or NULL on error.  Caller frees with
 * free().  Uses the best compression level so the output matches the runner's Optimal.
 */
extern uint8_t *a5_deflate (const uint8_t *data, uint32_t length,
                            uint32_t *out_size);

#endif
