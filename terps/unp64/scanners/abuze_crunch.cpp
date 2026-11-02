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

/* AbuzeCrunch */
void scnAbuzeCrunch(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p = 0;
	if (unp->_depAdr == 0) {
		for (q = 0x80b; q < 0x820; q++) {
			if (u32eq(mem + q, 0x1BA200A0) &&
				u32eq(mem + q + 0x13, 0x10CAA095) &&
				u16eq(mem + q + 0x20, 0x4CF7)) {
				p = READ_LE_UINT16(&mem[q + 0xc]);
				if (u32eq(mem + p + 0x25, 0xFFA9FE91) ||
					u32eq(mem + p + 0x25, 0xFeA5FE91)) {
					unp->_depAdr = READ_LE_UINT16(&mem[q + 0x22]);
					break;
				}
			}
		}
		if (unp->_depAdr) {
			unp->_endAdr = READ_LE_UINT16(&mem[p + 0x4]);
			unp->_endAdr++;
			unp->_forced = q;
			if (mem[p + 0x2f] == 0xa5)
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0x3f]);
			if (mem[p + 0x2f] == 0xa9)
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0x3d]);
			unp->_fStrAf = 0xfe;
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		for (q = 0x80b; q < 0x820; q++) {
			if (u32eq(mem + q, 0xA27800A0) &&
				u32eq(mem + q + 4, 0xBAA59AFF) &&
				u16eq(mem + q + 0x2c, 0x4CF7)) {
				p = READ_LE_UINT16(&mem[q + 0x12]);
				if (u32eq(mem + p + 0x25, 0xFeA5FE91)) {
					unp->_depAdr = READ_LE_UINT16(&mem[q + 0x2e]);
					break;
				}
			}
		}
		if (unp->_depAdr) {
			unp->_endAdr = READ_LE_UINT16(&mem[p + 0x4]);
			unp->_endAdr++;
			unp->_forced = q;
			unp->_retAdr = READ_LE_UINT16(&mem[p + 0x42]);
			unp->_fStrAf = 0xfe;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Abuze 5.0/FLT */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80b, 0x1BA200A0) &&
			u32eq(mem + 0x813, 0x10CAA095) &&
			u32eq(mem + 0x822, 0x011F4C01)) {
			p = READ_LE_UINT16(&mem[0x819]);
			if (u32eq(mem + p + 0x06, 0x9101E520)) {
				unp->_depAdr = 0x11f;
				forceEntryIfNoSys(unp, 0x80b);
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0x23]);
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x4]);
				unp->_endAdr++;
				unp->_fStrAf = 0xfe;
				unp->_idFlag = 1;
				return;
			}
		}
	}
}

} // End of namespace Unp64
