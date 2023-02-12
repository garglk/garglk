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

void scnSection8(UnpStr *unp) {
	uint8_t *mem;
	int p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		for (p = 0x810; p <= 0x828; p++) {
			if ((*(unsigned int *)(mem + p) == (unsigned int)(0x00BD00A2 + (((p & 0xff) + 0x11) << 24))) &&
				(*(unsigned int *)(mem + p + 0x04) == 0x01009D08) &&
				(*(unsigned int *)(mem + p + 0x10) == 0x34A97801) &&
				(*(unsigned int *)(mem + p + 0x6a) == 0xB1017820) &&
				(*(unsigned int *)(mem + p + 0x78) == 0x017F20AE)) {
				unp->_depAdr = 0x100;
				break;
			}
		}
		if (unp->_depAdr) {
			if (unp->_info->_run == -1)
				unp->_forced = p;
			unp->_strMem = mem[p + 0x47] | mem[p + 0x4b] << 8;
			unp->_retAdr = READ_LE_UINT16(&mem[p + 0x87]); // mem[p + 0x87] | mem[p + 0x88] << 8;
			if (unp->_retAdr == 0xf7) {
				unp->_retAdr = 0xa7ae;
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
		if ((*(unsigned int *)(mem + 0x827) == 0x38BD00A2) &&
			(*(unsigned int *)(mem + 0x82b) == 0x01009D08) &&
			(*(unsigned int *)(mem + 0x837) == 0x34A97801) &&
			(*(unsigned int *)(mem + 0x891) == 0xB1018420) &&
			(*(unsigned int *)(mem + 0x89f) == 0x018b20AE)) {
			unp->_depAdr = 0x100;
			if (unp->_info->_run == -1)
				unp->_forced = 0x827;
			unp->_strMem = mem[0x86e] | mem[0x872] << 8;
			if (*(unsigned short int *)(mem + 0x8b7) == 0xff5b) {
				mem[0x8b6] = 0x2c;
				unp->_retAdr = READ_LE_UINT16(&mem[0x8ba]); // mem[0x8ba] | mem[0x8bb] << 8;
			} else {
				unp->_retAdr = READ_LE_UINT16(&mem[0x8b7]); // mem[0x8b7] | mem[0x8b8] << 8;
			}
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
	/* PET||SLAN variant? */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x812) == 0x20BD00A2) &&
			(*(unsigned int *)(mem + 0x816) == 0x033c9D08) &&
			(*(unsigned int *)(mem + 0x863) == 0xB103B420) &&
			(*(unsigned int *)(mem + 0x86c) == 0x03BB20AE)) {
			unp->_depAdr = 0x33c;
			if (unp->_info->_run == -1)
				unp->_forced = 0x812;
			unp->_strMem = mem[0x856] | mem[0x85a] << 8;
			unp->_retAdr = READ_LE_UINT16(&mem[0x896]); // mem[0x896] | mem[0x897] << 8;
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
