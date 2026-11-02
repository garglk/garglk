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

void scnMegabyte(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int p;
	if (unp->_depAdr == 0) {
		p = 0;
		if (mem[0x816] == 0x4c)
			p = READ_LE_UINT16(&mem[0x817]);
		else if (unp->_info->_run == 0x810 && mem[0x814] == 0x4c &&
				 u32eqmasked(mem + 0x810, 0xffff00ff, 0x018500A9))
			p = READ_LE_UINT16(&mem[0x815]);
		if (p) {
			if (mem[p + 0] == 0x78 && mem[p + 1] == 0xa2 &&
				mem[p + 3] == 0xa0 &&
				u32eq(mem + p + 0x05, 0x15841486) &&
				u32eq(mem + p + 0x1d, 0x03804CF7)) {
				unp->_depAdr = 0x380;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x55]);
				unp->_endAdr++;
				unp->_strMem = 0x801;
				unp->_retAdr = 0x801; /* usually it just runs */
				unp->_idFlag = 1;
				return;
			}
		}
	}
	if (unp->_depAdr == 0) {
		p = 0;
		if (mem[0x81a] == 0x4c &&
			u32eqmasked(mem + 0x816, 0xffff00ff, 0x018500A9))
			p = READ_LE_UINT16(&mem[0x81b]);
		if (p) {
			if (mem[p + 0] == 0x78 && mem[p + 1] == 0xa2 &&
				mem[p + 3] == 0xa0 &&
				u32eq(mem + p + 0x05, 0x15841486) &&
				u32eq(mem + p + 0x1d, 0x03844CF7)) {
				unp->_depAdr = 0x384;
				unp->_forced = 0x816;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x59]);
				unp->_endAdr++;
				unp->_strMem = 0x801;
				unp->_retAdr = 0x801; /* usually it just runs */
				unp->_idFlag = 1;
				return;
			}
		}
	}
}

} // End of namespace Unp64
