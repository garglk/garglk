/* Code from Exomizer.
 *
 * Copyright (c) 2002 - 2023 Magnus Lind
 * C++ version based on code adapted to ScummVM by Avijeet Maurya
 *
 * Distributed under the zlib License by kind permission of the original
 * author Magnus Lind. The notice is in the file LICENSE in this directory
 * (terps/unp64), which has to go wherever this file goes.
 */

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include "exo_util.h"

namespace Unp64 {

/* BASIC token codes used by findSys. */
static const uint8_t TOK_SYS   = 0x9E;
static const uint8_t TOK_PLUS  = 0xAA;
static const uint8_t TOK_MINUS = 0xAB;
static const uint8_t TOK_MUL   = 0xAC;
static const uint8_t TOK_DIV   = 0xAD;
static const uint8_t TOK_POW   = 0xAE;
/* PETSCII digit-character mask: ('0'..'9') & 0x30 == 0x30. */
static const uint8_t ASCII_DIGIT_MASK = 0x30;
/* Defensive cap on how far past the line start we scan. */
static const int FIND_SYS_SCAN_LIMIT = 1000;

/* Walk a tokenized BASIC line and extract the integer argument of a
 * `SYS <addr>` statement.
 *
 * The buffer is expected to point at a BASIC line in C64 memory layout:
 *   [next-line-link 2B] [line-number 2B] [tokens ...] [null terminator]
 *
 * Returns the parsed address, or -1 if no SYS line was found. The parser
 * understands the BASIC arithmetic tokens that some packers concatenate
 * to obfuscate the entry point ($AA + / $AB - / $AC * / $AD / / $AE ^),
 * and also "E"-style scientific notation (`SYS 5E3` etc.).
 *
 * `bufsize` bounds the scan so a malformed PRG without a line terminator
 * can't walk off the end of the buffer. */
int findSys(const uint8_t *buf, size_t bufsize) {
	if (buf == nullptr || bufsize < 4)
		return -1;

	/* Skip link and line number. */
	buf += 4;
	bufsize -= 4;

	int i = 0;

	/* Workaround for hidden SYS line (1001 cruncher, CFB, etc.):
	 * some crunchers stash the real SYS token a few bytes into a line
	 * that starts with a NUL, so scan forward for the SYS token followed
	 * by an ASCII-digit byte.
	 *
	 * The scan starts at 5 because that is where the text of the next
	 * line would be: the NUL at 0 ends this line, 1-4 are the next one's
	 * link and number. It gives up after 32 bytes, and the ordinary scan
	 * below then carries on from there. */
	if (bufsize > 0 && buf[0] == 0) {
		for (i = 5; i < 32 && (size_t)(i + 2) < bufsize; i++) {
			if (buf[i] == TOK_SYS &&
				(((buf[i + 1] & ASCII_DIGIT_MASK) == ASCII_DIGIT_MASK) ||
				 ((buf[i + 2] & ASCII_DIGIT_MASK) == ASCII_DIGIT_MASK))) {
				break;
			}
		}
	}

	/* Scan for the SYS token. FIND_SYS_SCAN_LIMIT is tested here and once
	 * more after the loop: here it ends a line that has no terminator, there
	 * it tells that ending from having found the token. */
	while (i < FIND_SYS_SCAN_LIMIT && (size_t)i < bufsize && buf[i] != '\0') {
		if (buf[i] == TOK_SYS) {
			++i;
			break;
		}
		++i;
	}
	if (i >= FIND_SYS_SCAN_LIMIT || (size_t)i >= bufsize || buf[i] == '\0')
		return -1;

	/* Skip leading spaces and an optional opening parenthesis. */
	while ((size_t)i < bufsize && buf[i] != '\0' && (buf[i] == ' ' || buf[i] == '(')) {
		++i;
	}
	if ((size_t)i >= bufsize || buf[i] == '\0')
		return -1;

	/* Parse the address literal. */
	char *parseEnd;
	long outstart = strtol((const char *)(buf + i), &parseEnd, 10);
	if (parseEnd == (const char *)(buf + i))
		return -1; /* no digits */

	/* If the next byte is a BASIC arithmetic token, consume its operand
	 * and apply it: e.g. `SYS 2048+15`. */
	uint8_t op = (uint8_t)*parseEnd;
	if (op >= TOK_PLUS && op <= TOK_POW) {
		long operand = strtol(parseEnd + 1, &parseEnd, 10);
		switch (op) {
		case TOK_PLUS:  outstart += operand; break;
		case TOK_MINUS: outstart -= operand; break;
		case TOK_MUL:   outstart *= operand; break;
		case TOK_DIV:   if (operand > 0) outstart /= operand; break;
		case TOK_POW: {
			long base = outstart;
			while (--operand > 0)
				outstart *= base;
			break;
		}
		}
	} else if (op == 'E') {
		/* Scientific notation: `5E3` → 5 * 10^3. */
		long exp = strtol(parseEnd + 1, &parseEnd, 10);
		while (exp-- > 0)
			outstart *= 10;
	}

	return (int)outstart;
}

/* Copy a PRG payload (no header) into the emulated 64 KiB address
 * space at `info->_start`, clamping to the bounds. Initialises
 * `_end`, `_run`, and the BASIC-variable-start hint that BASIC sets
 * after a LOAD when the program lands at TXTTAB. */
static void loadPrgData(uint8_t *mem, const uint8_t *data, size_t dataLength, LoadInfo *info) {
	int len = std::min(65536 - info->_start, static_cast<int>(dataLength));
	memcpy(mem + info->_start, data, (size_t)len);

	info->_end = info->_start + len;
	info->_basicVarStart = -1;
	info->_run = -1;
	if (info->_basicTxtStart >= info->_start && info->_basicTxtStart < info->_end) {
		info->_basicVarStart = info->_end;
	}
}

/* Load a full PRG file (including the two-byte little-endian load
 * address header) into the emulated address space. Returns false, and
 * leaves `mem` and `info` alone, if the file is too short to have one. */
bool loadData(const uint8_t *data, size_t dataLength, uint8_t *mem, LoadInfo *info) {
	if (data == nullptr || dataLength < 2)
		return false; /* no room for the load address */

	info->_start = READ_LE_UINT16(data);
	loadPrgData(mem, data + 2, dataLength - 2, info);
	return true;
}

/* Parse a numeric string into *value. Decimal by default; either a `$`
 * prefix or a `0x`/`0X` prefix selects base 16 (both forms can appear in
 * the UNP64 switches — e.g. "-e0x2ec2", "-d$0820").
 *
 * Returns true on success. Fails on:
 *   - NULL or empty input
 *   - a lone prefix with no digits after it
 *   - trailing garbage after the number
 *   - overflow of `long` (ERANGE) or out-of-`int`-range result
 *
 * Base 10 is fixed (rather than strtol's base-0 auto-detect) for the
 * no-prefix path so a leading zero like `-e040` parses as 40, not 32. */
bool strToInt(const char *str, int *value) {
	if (str == nullptr || *str == '\0')
		return false;

	int base = 10;
	if (*str == '$') {
		++str;
		base = 16;
	} else if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
		base = 16;
	}
	if (*str == '\0')
		return false; /* lone prefix — no digits */

	errno = 0;
	char *strEnd;
	long lval = strtol(str, &strEnd, base);

	if (strEnd == str || *strEnd != '\0')
		return false; /* no digits parsed, or trailing garbage */
	if (errno == ERANGE || lval > INT_MAX || lval < INT_MIN)
		return false; /* value won't fit in int */

	if (value != nullptr)
		*value = static_cast<int>(lval);
	return true;
}

} // End of namespace Unp64
