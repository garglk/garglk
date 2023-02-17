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

#include "exo_util.h"
#include "types.h"
#include "unp64.h"

namespace Unp64 {

void scnMegabyte(UnpStr *unp) {
	uint8_t *mem;
	int p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		p = 0;
		if (mem[0x816] == 0x4c)
			p = READ_LE_UINT16(&mem[0x817]); // mem[0x817] | mem[0x818] << 8;
		else if (unp->_info->_run == 0x810 && mem[0x814] == 0x4c &&
				 u32eqmasked(mem + 0x810, 0xffff00ff, 0x018500A9))
			p = READ_LE_UINT16(&mem[0x815]); // mem[0x815] | mem[0x816] << 8;
		if (p) {
			if (mem[p + 0] == 0x78 && mem[p + 1] == 0xa2 &&
				mem[p + 3] == 0xa0 &&
				u32eq(mem + p + 0x05, 0x15841486) &&
				u32eq(mem + p + 0x1d, 0x03804CF7)) {
				unp->_depAdr = 0x380;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x55]); // mem[p + 0x55] | mem[p + 0x56] << 8;
				unp->_endAdr++;
				unp->_strMem = 0x801;
				unp->_retAdr = 0x801; /* ususally it just runs */
				unp->_idFlag = 1;
				return;
			}
		}
	}
	if (unp->_depAdr == 0) {
		p = 0;
		if (mem[0x81a] == 0x4c &&
			u32eqmasked(mem + 0x816, 0xffff00ff, 0x018500A9))
			p = READ_LE_UINT16(&mem[0x81b]); // mem[0x81b] | mem[0x81c] << 8;
		if (p) {
			if (mem[p + 0] == 0x78 && mem[p + 1] == 0xa2 &&
				mem[p + 3] == 0xa0 &&
				u32eq(mem + p + 0x05, 0x15841486) &&
				u32eq(mem + p + 0x1d, 0x03844CF7)) {
				unp->_depAdr = 0x384;
				unp->_forced = 0x816;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x59]); // mem[p + 0x59] | mem[p + 0x5a] << 8;
				unp->_endAdr++;
				unp->_strMem = 0x801;
				unp->_retAdr = 0x801; /* ususally it just runs */
				unp->_idFlag = 1;
				return;
			}
		}
	}
}

} // End of namespace Unp64
