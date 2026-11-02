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

void scnActionPacker(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x811, 0x018538A9) &&
			u32eq(mem + 0x81d, 0xCEF7D0E8) &&
			u32eq(mem + 0x82d, 0x0F9D0837) &&
			u32eq(mem + 0x84b, 0x03D00120)) {
			unp->_depAdr = 0x110;
			unp->_forced = 0x811;
			unp->_strMem = READ_LE_UINT16(&mem[0x848]);
			unp->_fEndAf = 0x120;
			unp->_retAdr = READ_LE_UINT16(&mem[0x863]);
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
