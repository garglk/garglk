/* Part of a cut-down version of UNP64, the generic Commodore 64 prg
 * unpacker, with only what is needed to decompress a number of Commodore 64
 * adventure games.
 *
 * (C) 2008-2022 iAN CooG/HVSC Crew^C64Intros
 * Copyright (c) 2002 - 2023 Magnus Lind
 * C++ version based on code adapted to ScummVM by Avijeet Maurya
 *
 * Distributed under the zlib License by kind permission of the original
 * authors Magnus Lind and iAN CooG. The notice is in the file LICENSE in
 * the directory above (terps/unp64), which has to go wherever this file goes.
 */

#include "scanners.h"

namespace Unp64 {

void scnTCScrunch(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p;
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x819, 0x018536A9) && mem[0x81d] == 0x4c) {
			p = READ_LE_UINT16(&mem[0x81e]);
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
			p = READ_LE_UINT16(&mem[0x81e]);
			if (mem[p] == 0xa2 && mem[p + 2] == 0xbd &&
				u32eq(mem + p + 0x05, 0xE801109D) &&
				u32eq(mem + p + 0x38, 0x01304CFB)) {
				unp->_depAdr = 0x334;
				unp->_forced = 0x818;
				if (mem[unp->_forced] != 0x78)
					unp->_forced++;
				unp->_endAdr = 0x2d;
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0xd9]);
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
