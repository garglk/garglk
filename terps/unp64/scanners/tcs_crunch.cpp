/* This is a cut-down version of UNP64 with only the bare minimum
 * needed to decompress a number of Commodore 64 adventure games.
 * It is distributed under the zlib License by kind permission of
 * the original authors Magnus Lind and iAN CooG.
 */

/*
UNP64 - generic Commodore 64 prg unpacker
(C) 2008-2022 iAN CooG/HVSC Crew^C64Intros
original source and idea: testrun.c, taken from exo20b7

Follows original disclaimer
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

#include "types.h"
#include "unp64.h"
#include "exo_util.h"

namespace Unp64 {

void scnTCScrunch(UnpStr *unp) {
	uint8_t *mem;
	int q, p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x819, 0x018536A9) && mem[0x81d] == 0x4c) {
			p = READ_LE_UINT16(&mem[0x81e]); // mem[0x81e] | mem[0x81f] << 8;
			if (mem[p] == 0xa2 && mem[p + 2] == 0xbd &&
				u32eq(mem + p + 0x05, 0xE801109D) &&
				(u32eq(mem + p + 0x38, 0x01524CFB) ||
				 (u32eq(mem + p + 0x38, 0x8DE1A9FB) &&
				  u32eq(mem + p + 0x3c, 0x524C0328)))) {
				unp->_depAdr = 0x334;
				unp->_forced = 0x819;
				unp->_endAdr = 0x2d;
			}
		} else if (u32eq(mem + 0x819, 0x018534A9) && mem[0x81d] == 0x4c) {
			p = READ_LE_UINT16(&mem[0x81e]); // mem[0x81e] | mem[0x81f] << 8;
			if (mem[p] == 0xa2 && mem[p + 2] == 0xbd &&
				u32eq(mem + p + 0x05, 0xE801109D) &&
				u32eq(mem + p + 0x38, 0x01304CFB)) {
				unp->_depAdr = 0x334;
				unp->_forced = 0x818;
				if (mem[unp->_forced] != 0x78)
					unp->_forced++;
				unp->_endAdr = 0x2d;
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0xd9]); // mem[p + 0xd9] | mem[p + 0xda] << 8;
				p += 0xc8;
				q = p + 6;
				for (; p < q; p += 3) {
					if (mem[p] == 0x20 &&
						u16gteq(mem + p + 1, 0xa000) &&
						u16lteq(mem + p + 1, 0xbfff)) {
						mem[p] = 0x2c;
					}
				}
			}
		}
		if (unp->_depAdr) {
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
