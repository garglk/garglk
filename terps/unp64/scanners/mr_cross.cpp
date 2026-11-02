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

/* Mr.Cross linker */
void scnMrCross(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p;
	if (unp->_depAdr == 0) {
		/*
         there are sysless versions and also can be at
         different address than $0801
         */
		p = unp->_info->_run;
		if (p == -1)
			p = unp->_info->_start;
		for (q = p + 0x80; p < q; p++) {
			if (u32eq(mem + p + 0x00, 0x8538A978) &&
				u32eq(mem + p + 0x04, 0x9AF9A201) &&
				u32eq(mem + p + 0x0b, 0xCA01069D) &&
				u32eq(mem + p + 0x36, 0x60D8d000U + (unsigned int)(p >> 8))
			) {
				unp->_depAdr = READ_LE_UINT16(&mem[p + 0x96]);
				unp->_depAdr += 0x1f;
				unp->_forced = p;
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0x9a]);
				unp->_strMem = READ_LE_UINT16(&mem[p + 0x4d]);
				unp->_monEnd = (unsigned int)((mem[p + 0x87]) << 24 |
							   (mem[p + 0x86]) << 16 |
							   (mem[p + 0x8d]) << 8 |
							   (mem[p + 0x8c]));
				break;
			}
			if (u32eq(mem + p + 0x00, 0x8538A978) &&
				u32eq(mem + p + 0x04, 0x9AF9A201) &&
				u32eq(mem + p + 0x0b, 0xCA01089D) &&
				u32eq(mem + p + 0x36, 0xe0D8d000U + (unsigned int)(p >> 8))
			) {
				unp->_depAdr = 0x1f6;
				unp->_forced = p;
				unp->_retAdr = READ_LE_UINT16(&mem[p + 0xaa]);
				unp->_strMem = READ_LE_UINT16(&mem[p + 0x53]);
				unp->_fEndAf = 0x1a7;
				unp->_endAdC = 0xffff;
				break;
			}
		}
		if (unp->_depAdr) {
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
