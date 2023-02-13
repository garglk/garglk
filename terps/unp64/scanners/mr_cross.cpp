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

/* Mr.Cross linker */
void scnMrCross(UnpStr *unp) {
	unsigned char *mem;
	int q, p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		/*
         there are sysless versions and also can be at
         different address than $0801
         */
		p = unp->_info->_run;
		if (p == -1)
			p = unp->_info->_start;
		for (q = p + 0x80; p < q; p++) {
			if ((*(unsigned int *)(mem + p + 0x00) == 0x8538A978) &&
				(*(unsigned int *)(mem + p + 0x04) == 0x9AF9A201) &&
				(*(unsigned int *)(mem + p + 0x0b) == 0xCA01069D) &&
				(*(unsigned int *)(mem + p + 0x36) == 0x60D8d000U + (unsigned int)(p >> 8))
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
			if ((*(unsigned int *)(mem + p + 0x00) == 0x8538A978) &&
				(*(unsigned int *)(mem + p + 0x04) == 0x9AF9A201) &&
				(*(unsigned int *)(mem + p + 0x0b) == 0xCA01089D) &&
                (*(unsigned int *)(mem + p + 0x36) == 0xe0D8d000U + (unsigned int)(p >> 8))
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
