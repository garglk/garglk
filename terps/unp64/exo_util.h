/* Code from Exomizer.
 *
 * Copyright (c) 2002 - 2023 Magnus Lind
 * C++ version based on code adapted to ScummVM by Avijeet Maurya
 *
 * Distributed under the zlib License by kind permission of the original
 * author Magnus Lind. The notice is in the file LICENSE in this directory
 * (terps/unp64), which has to go wherever this file goes.
 */

/* The name is historical: what is left in here of Exomizer's exo_util.c is
 * the PRG loader and the BASIC SYS line parser. The rest is UNP64's: the
 * switch number parser and the little-endian readers and signature
 * comparisons that the scanners are written in. */

#ifndef UNP64_EXO_UTIL_H
#define UNP64_EXO_UTIL_H

#include <cstddef>
#include <cstdint>

namespace Unp64 {

struct LoadInfo {
	int _basicTxtStart; /* in */
	int _basicVarStart; /* out */
	int _run;           /* out */
	int _start;         /* out */
	int _end;           /* out */
};

int findSys(const uint8_t *buf, size_t bufsize);

/* `mem` is the emulated 64 KiB address space (at least 65536 bytes). */
bool loadData(const uint8_t *data, size_t dataLength, uint8_t *mem, LoadInfo *info);

bool strToInt(const char *str, int *value);

inline uint16_t READ_LE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[1] << 8) | b[0];
}

/* A 16-bit value whose two halves are not adjacent in memory. Packers load
 * an address as "LDA #lo ... LDY #hi" or store it into the operands of two
 * separate instructions, so the low and the high byte are the immediates of
 * two instructions a few bytes apart. */
inline int readSplit(const uint8_t *mem, int loAddr, int hiAddr) {
	return mem[loAddr] | mem[hiAddr] << 8;
}

inline uint32_t READ_LE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return ((uint32_t)b[3] << 24) | (b[2] << 16) | (b[1] << 8) | (b[0]);
}

/* The u32eq / u16eq family compare the four (or two) bytes at `addr`
 * against `val` interpreted little-endian — the byte order in which
 * the 6502 stores multi-byte values. The masked / xored variants exist
 * so scanners can match opcode patterns where some bytes are operand
 * placeholders (mask out the operand) or where a single byte has been
 * EOR-encrypted by the packer (apply xormask before comparing).
 *
 * Inline, because a scan is some thousands of these. */

inline bool u32eq(const unsigned char *addr, uint32_t val) {
	return READ_LE_UINT32(addr) == val;
}

inline bool u32eqmasked(const unsigned char *addr, uint32_t mask, uint32_t val) {
	return (READ_LE_UINT32(addr) & mask) == val;
}

inline bool u32eqxored(const unsigned char *addr, uint32_t xormask, uint32_t val) {
	return (READ_LE_UINT32(addr) ^ xormask) == val;
}

inline bool u16eq(const unsigned char *addr, uint16_t val) {
	return READ_LE_UINT16(addr) == val;
}

inline bool u16eqmasked(const unsigned char *addr, uint16_t mask, uint16_t val) {
	return (READ_LE_UINT16(addr) & mask) == val;
}

/* Interpret two bytes at `addr` as a little-endian 16-bit address and
 * compare to `val` — used by scanners that test "is the JMP/JSR target
 * within / outside this range?". */
inline bool u16gteq(const unsigned char *addr, uint16_t val) {
	return READ_LE_UINT16(addr) >= val;
}

inline bool u16lteq(const unsigned char *addr, uint16_t val) {
	return READ_LE_UINT16(addr) <= val;
}

} // End of namespace Unp64

#endif
