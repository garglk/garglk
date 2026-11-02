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

void scnMasterCompressor(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int p;
	if (unp->_depAdr == 0) {
		for (p = 0x80d; p < 0x880; p++) {
			if (u32eqmasked(mem + p + 0x005, 0x00ffffff, 0x00BDD2A2) &&
				u32eq(mem + p + 0x00a, 0xE000F99D) &&
				u32eq(mem + p + 0x017, 0xCAEDD0CA) &&
				u32eq(mem + p + 0x031, 0x84C82E86) &&
				u32eqmasked(mem + p + 0x035, 0x0000ffff, 0x00004C2D) &&
				u32eq(mem + p + 0x134, 0xDBD0FFE6)) {
				if (/*mem[p]==0x78&&*/ mem[p + 1] == 0xa9 &&
					u32eq(mem + p + 0x003, 0xD2A20185)) {
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x37]);
					unp->_forced = p + 1;
					if (mem[p + 0x12b] == 0x020) // jsr $0400, unuseful fx
						mem[p + 0x12b] = 0x2c;
				} else if (u32eq(mem + p, 0xD024E0E8)) {
					/* HTL version */
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x37]);
					unp->_forced = 0x840;
				}
				if (unp->_depAdr) {
					unp->_retAdr = READ_LE_UINT16(&mem[p + 0x13e]);
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
			if (u32eqmasked(mem + p + 0x005, 0x00ffffff, 0x00BDD2A2) &&
				u32eq(mem + p + 0x00a, 0xE000F99D) &&
				u32eq(mem + p + 0x017, 0xCAEDD0CA) &&
				u32eq(mem + p + 0x031, 0x84C82E86) &&
				u32eqmasked(mem + p + 0x035, 0x0000ffff, 0x00004C2D) &&
				u32eq(mem + p + 0x12d, 0xe2D0FFE6)) {
				if (mem[p + 1] == 0xa9 &&
					u32eq(mem + p + 0x003, 0xD2A20185)) {
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x37]);
					unp->_forced = p + 1;
				}
				if (unp->_depAdr) {
					if (mem[p + 0x136] == 0x4c)
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0x137]);
					else if (mem[p + 0x13d] == 0x4c)
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0x13e]);
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
		if (u32eq(mem + p + 0x000, 0xE67800A0) &&
			u32eq(mem + p + 0x004, 0x0841B901) &&
			u32eq(mem + p + 0x008, 0xB900FA99) &&
			u32eq(mem + p + 0x00c, 0x34990910)) {
			unp->_depAdr = 0x100;
			unp->_forced = p;
			unp->_retAdr = READ_LE_UINT16(&mem[0x943]);
			unp->_endAdr = 0x2d;
			unp->_fStrBf = unp->_endAdr;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Fred/Channel4 hack */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x811, 0xA9A98078) &&
			u32eq(mem + 0x815, 0x85EE8034) &&
			u32eq(mem + 0x819, 0x802DA201) &&
			u32eq(mem + 0x882, 0x01004C2D)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x811;
			unp->_retAdr = READ_LE_UINT16(&mem[0x98b]);
			if (unp->_retAdr < 0x800)
				unp->_rtAFrc = 1;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
