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

namespace Unp64 {

void scnMasterCompressor(UnpStr *unp) {
	uint8_t *mem;
	int p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		for (p = 0x80d; p < 0x880; p++) {
			if (((*(unsigned int *)(mem + p + 0x005) & 0x00ffffff) == 0x00BDD2A2) &&
				(*(unsigned int *)(mem + p + 0x00a) == 0xE000F99D) &&
				(*(unsigned int *)(mem + p + 0x017) == 0xCAEDD0CA) &&
				(*(unsigned int *)(mem + p + 0x031) == 0x84C82E86) &&
				((*(unsigned int *)(mem + p + 0x035) & 0x0000ffff) == 0x00004C2D) &&
				(*(unsigned int *)(mem + p + 0x134) == 0xDBD0FFE6)) {
				if (/*mem[p]==0x78&&*/ mem[p + 1] == 0xa9 &&
					(*(unsigned int *)(mem + p + 0x003) == 0xD2A20185)) {
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x37]); // mem[p + 0x37] | mem[p + 0x38] << 8;
					unp->_forced = p + 1;
					if (mem[p + 0x12b] == 0x020) // jsr $0400, unuseful fx
						mem[p + 0x12b] = 0x2c;
				} else if (*(unsigned int *)(mem + p) == 0xD024E0E8) {
					/* HTL version */
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x37]); // mem[p + 0x37] | mem[p + 0x38] << 8;
					unp->_forced = 0x840;
				}
				if (unp->_depAdr) {
					unp->_retAdr = READ_LE_UINT16(&mem[p + 0x13e]); // mem[p + 0x13e] | mem[p + 0x13f] << 8;
					unp->_endAdr = 0x2d;
					unp->_fStrBf = unp->_endAdr;
					unp->_idFlag = 1;
					return;
				}
			}
		}
	}
	if (unp->_depAdr == 0) {
		for (p = 0x80d; p < 0x880; p++) {
			if (((*(unsigned int *)(mem + p + 0x005) & 0x00ffffff) == 0x00BDD2A2) &&
				(*(unsigned int *)(mem + p + 0x00a) == 0xE000F99D) &&
				(*(unsigned int *)(mem + p + 0x017) == 0xCAEDD0CA) &&
				(*(unsigned int *)(mem + p + 0x031) == 0x84C82E86) &&
				((*(unsigned int *)(mem + p + 0x035) & 0x0000ffff) == 0x00004C2D) &&
				(*(unsigned int *)(mem + p + 0x12d) == 0xe2D0FFE6)) {
				if (mem[p + 1] == 0xa9 &&
					(*(unsigned int *)(mem + p + 0x003) == 0xD2A20185)) {
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x37]); // mem[p + 0x37] | mem[p + 0x38] << 8;
					unp->_forced = p + 1;
				}
				if (unp->_depAdr) {
					if (mem[p + 0x136] == 0x4c)
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0x137]); // mem[p + 0x137] | mem[p + 0x138] << 8;
					else if (mem[p + 0x13d] == 0x4c)
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0x13e]); // mem[p + 0x13e] | mem[p + 0x13f] << 8;
					unp->_endAdr = 0x2d;
					unp->_fStrBf = unp->_endAdr;
					unp->_idFlag = 1;
					return;
				}
			}
		}
	}
	if (unp->_depAdr == 0) {
		p = 0x812;
		if ((*(unsigned int *)(mem + p + 0x000) == 0xE67800A0) &&
			(*(unsigned int *)(mem + p + 0x004) == 0x0841B901) &&
			(*(unsigned int *)(mem + p + 0x008) == 0xB900FA99) &&
			(*(unsigned int *)(mem + p + 0x00c) == 0x34990910)) {
			unp->_depAdr = 0x100;
			unp->_forced = p;
			unp->_retAdr = READ_LE_UINT16(&mem[0x943]); // mem[0x943] | mem[0x944] << 8;
			unp->_endAdr = 0x2d;
			unp->_fStrBf = unp->_endAdr;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Fred/Channel4 hack */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x811) == 0xA9A98078) &&
			(*(unsigned int *)(mem + 0x815) == 0x85EE8034) &&
			(*(unsigned int *)(mem + 0x819) == 0x802DA201) &&
			(*(unsigned int *)(mem + 0x882) == 0x01004C2D)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x811;
			unp->_retAdr = READ_LE_UINT16(&mem[0x98b]); // mem[0x98b] | mem[0x98c] << 8;
			if (unp->_retAdr < 0x800)
				unp->_rtAFrc = 1;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
