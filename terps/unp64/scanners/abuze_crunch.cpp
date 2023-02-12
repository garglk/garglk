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

/* AbuzeCrunch */
void scnAbuzeCrunch(UnpStr *unp) {
	unsigned char *mem;
	int q, p = 0;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		for (q = 0x80b; q < 0x820; q++) {
			if ((*(unsigned int *)(mem + q) == 0x1BA200A0) &&
				(*(unsigned int *)(mem + q + 0x13) == 0x10CAA095) &&
				(*(unsigned short int *)(mem + q + 0x20) == 0x4CF7)) {
				p = READ_LE_UINT16(&mem[q + 0xc]);
				if ((*(unsigned int *)(mem + p + 0x25) == 0xFFA9FE91) ||
					(*(unsigned int *)(mem + p + 0x25) == 0xFeA5FE91)) {
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
			if ((*(unsigned int *)(mem + q) == 0xA27800A0) &&
				(*(unsigned int *)(mem + q + 4) == 0xBAA59AFF) &&
				(*(unsigned short int *)(mem + q + 0x2c) == 0x4CF7)) {
				p = READ_LE_UINT16(&mem[q + 0x12]);
				if (*(unsigned int *)(mem + p + 0x25) == 0xFeA5FE91) {
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
		if ((*(unsigned int *)(mem + 0x80b) == 0x1BA200A0) &&
			(*(unsigned int *)(mem + 0x813) == 0x10CAA095) &&
			(*(unsigned int *)(mem + 0x822) == 0x011F4C01)) {
			p = READ_LE_UINT16(&mem[0x819]);
			if (*(unsigned int *)(mem + p + 0x06) == 0x9101E520) {
				unp->_depAdr = 0x11f;
				if (unp->_info->_run == -1)
					unp->_forced = 0x80b;
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
