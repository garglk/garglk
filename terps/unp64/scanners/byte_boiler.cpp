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

void scnByteBoiler(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p;
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x813, 0xE800F09D) &&
			u32eq(mem + 0x818, 0x014E4CF7)) {
			p = READ_LE_UINT16(&mem[0x811]);
			if (u32eq(mem + p + 1, 0x02D0FAA5)) {
				unp->_depAdr = 0x14e;
				unp->_forced = 0x80b;
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0x5c]);
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x0e]);
				unp->_endAdr++;
				unp->_fStrAf = 0xfe;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* CPX hack */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80b, 0xA97800A2) &&
			u32eq(mem + 0x815, 0x4C01E6D0)) {
			q = READ_LE_UINT16(&mem[0x819]);
			if (u32eq(mem + q + 3, 0xE800F09D) &&
				u32eq(mem + q + 8, 0x014E4CF7)) {
				p = READ_LE_UINT16(&mem[q + 1]);
				if (u32eq(mem + p + 1, 0x02D0FAA5)) {
					unp->_depAdr = 0x14e;
					unp->_forced = 0x80b;
					unp->_retAdr = READ_LE_UINT16(&mem[p + 0x5c]);
					unp->_endAdr = READ_LE_UINT16(&mem[p + 0x0e]);
					unp->_endAdr++;
					unp->_fStrAf = 0xfe;
					unp->_idFlag = 1;
					return;
				}
			}
		}
	}
	/* SCS hack */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x813, 0xE800F09D) &&
			u32eq(mem + 0x818, 0x01bf4CF7)) {
			p = READ_LE_UINT16(&mem[0x811]);
			if (u32eq(mem + p + 1, 0x02D0FAA5) &&
				u32eq(mem + p + 0xdd, 0x014e4c01)) {
				unp->_depAdr = 0x14e;
				unp->_forced = 0x80b;
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0x5c]);
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x0e]);
				unp->_endAdr++;
				unp->_fStrAf = 0xfe;
				unp->_idFlag = 1;
				return;
			}
		}
	}
}

} // End of namespace Unp64
