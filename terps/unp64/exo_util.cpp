/* Code from Exomizer distributed under the zlib License
 * by kind permission of the original author
 * Magnus Lind.
 */

/*
 * Copyright (c) 2002 - 2023 Magnus Lind.
 *
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

/*
 C++ version based on code adapted to ScummVM by Avijeet Maurya
 */

#include <cstring>
#include <cstdlib>
#include "exo_util.h"

namespace Unp64 {

int findSys(const uint8_t *buf, int target) {
	int outstart = -1;
	int state = 1;
	int i = 0;
	/* skip link and line number */
	buf += 4;
	/* iAN: workaround for hidden sysline (1001 cruncher, CFB, etc)*/
	if (buf[0] == 0) {
		for (i = 5; i < 32; i++)
			if (buf[i] == 0x9e && (((buf[i + 1] & 0x30) == 0x30) || ((buf[i + 2] & 0x30) == 0x30)))
				break;
	}
	/* exit loop at line end */
	while (i < 1000 && buf[i] != '\0') {
		uint8_t *sysEnd;
		int c = buf[i];
		switch (state) {
			/* look for and consume sys token */
		case 1:
			if ((target == -1 && (c == 0x9e)) || c == target) {
				state = 2;
			}
			break;
			/* skip spaces and left parenthesis, if any */
		case 2:
			if (strchr(" (", c) != nullptr)
				break;
			// fallthrough
			/* convert string number to int */
		case 3:
			outstart = (int)strtol((const char *)(buf + i), (char **)&sysEnd, 10);
			if ((buf + i) == sysEnd) {
				/* we got nothing */
				outstart = -1;
			} else {
				c = *sysEnd;
				if ((c >= 0xaa) && (c <= 0xae)) {
					i = (int)strtol((char *)(sysEnd + 1), (char **)&sysEnd, 10);
					switch (c) {
					case 0xaa:
						outstart += i;
						break;
					case 0xab:
						outstart -= i;
						break;
					case 0xac:
						outstart *= i;
						break;
					case 0xad:
						if (i > 0)
							outstart /= i;
						break;
					case 0xae:
						c = outstart;
						while (--i)
							outstart *= c;
						break;
					}
				} else if (c == 'E') {
					i = (int)strtol((char *)(sysEnd + 1), (char **)&sysEnd, 10);
					i++;
					while (--i)
						outstart *= 10;
				}
			}
			state = 4;
			break;
		case 4:
			break;
		}
		++i;
	}

	return outstart;
}

static void loadPrgData(uint8_t mem[65536], uint8_t *data, size_t dataLength, LoadInfo *info) {
	int len = MIN(65536 - info->_start, static_cast<int>(dataLength));
	memcpy(mem + info->_start, data, (size_t)len);

	info->_end = info->_start + len;
	info->_basicVarStart = -1;
	info->_run = -1;
	if (info->_basicTxtStart >= info->_start && info->_basicTxtStart < info->_end) {
		info->_basicVarStart = info->_end;
	}
}

void loadData(uint8_t *data, size_t dataLength, uint8_t mem[65536], LoadInfo *info) {
	int load = data[0] + data[1] * 0x100;

	info->_start = load;
	loadPrgData(mem, data + 2, dataLength - 2, info);
}

int strToInt(const char *str, int *value) {
	int status = 0;
	do {
		char *strEnd;
		long lval;

		/* base 0 is auto detect */
		int base = 0;

		if (*str == '\0') {
			/* no string to parse */
			status = 1;
			break;
		}

		if (*str == '$') {
			/* a $ prefix specifies base 16 */
			++str;
			base = 16;
		}

		lval = strtol(str, &strEnd, base);

		if (*strEnd != '\0') {
			/* there is garbage in the string */
			status = 1;
			break;
		}

		if (value != nullptr) {
			/* all is well, set the out parameter */
			*value = static_cast<int>(lval);
		}
	} while (0);

	return status;
}

bool u32eq(const unsigned char *addr, uint32_t val)
{
    return addr[3] == (val >> 24) &&
    addr[2] == ((val >> 16) & 0xff) &&
    addr[1] == ((val >>  8) & 0xff) &&
    addr[0] == (val & 0xff);
}

bool u32eqmasked(const unsigned char *addr, uint32_t mask, uint32_t val)
{
    uint32_t val1 = addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
    return (val1 & mask) == val;
}

bool u32eqxored(const unsigned char *addr, uint32_t xormask, uint32_t val)
{
    uint32_t val1 = addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
    return (val1 ^ xormask) == val;
}

bool u16eqmasked(const unsigned char *addr, uint16_t mask, uint16_t val)
{
    uint16_t val1 = addr[0] | (addr[1] << 8);
    return (val1 & mask) == val;
}

bool u16eq(const unsigned char *addr, uint16_t val)
{
    return addr[1] == (val >> 8) &&
    addr[0] == (val & 0xff);
}

bool u16gteq(const unsigned char *addr, uint16_t val)
{
    uint16_t val2 = addr[0] | (addr[1] << 8);
    return val2 >= val;
}

bool u16lteq(const unsigned char *addr, uint16_t val)
{
    uint16_t val2 = addr[0] | (addr[1] << 8);
    return val2 <= val;
}

} // End of namespace Unp64
