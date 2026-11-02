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

void scnSection8(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int p;
	if (unp->_depAdr == 0) {
		for (p = 0x810; p <= 0x828; p++) {
			if (u32eq(mem + p, (unsigned int)(0x00BD00A2 + (((p & 0xff) + 0x11) << 24))) &&
				u32eq(mem + p + 0x04, 0x01009D08) &&
				u32eq(mem + p + 0x10, 0x34A97801) &&
				u32eq(mem + p + 0x6a, 0xB1017820) &&
				u32eq(mem + p + 0x78, 0x017F20AE)) {
				unp->_depAdr = 0x100;
				break;
			}
		}
		if (unp->_depAdr) {
			forceEntryIfNoSys(unp, p);
			unp->_strMem = readSplit(mem, p + 0x47, p + 0x4b);
			unp->_retAdr = READ_LE_UINT16(&mem[p + 0x87]);
			if (unp->_retAdr == 0xf7) {
				unp->_retAdr = kBasicRun;
				mem[p + 0x87] = 0xae;
				mem[p + 0x88] = 0xa7;
			}
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Crackman variant? */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x827, 0x38BD00A2) &&
			u32eq(mem + 0x82b, 0x01009D08) &&
			u32eq(mem + 0x837, 0x34A97801) &&
			u32eq(mem + 0x891, 0xB1018420) &&
			u32eq(mem + 0x89f, 0x018b20AE)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x827);
			unp->_strMem = readSplit(mem, 0x86e, 0x872);
			if (u16eq(mem + 0x8b7, 0xff5b)) {
				mem[0x8b6] = 0x2c;
				unp->_retAdr = READ_LE_UINT16(&mem[0x8ba]);
			} else {
				unp->_retAdr = READ_LE_UINT16(&mem[0x8b7]);
			}
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
	/* PET||SLAN variant? */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x812, 0x20BD00A2) &&
			u32eq(mem + 0x816, 0x033c9D08) &&
			u32eq(mem + 0x863, 0xB103B420) &&
			u32eq(mem + 0x86c, 0x03BB20AE)) {
			unp->_depAdr = 0x33c;
			forceEntryIfNoSys(unp, 0x812);
			unp->_strMem = readSplit(mem, 0x856, 0x85a);
			unp->_retAdr = READ_LE_UINT16(&mem[0x896]);
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
