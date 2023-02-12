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

void scnCCS(UnpStr *unp) {
	uint8_t *mem;
	int p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x817) == 0xB901E678) &&
			(*(unsigned int *)(mem + 0x81b) == 0xFD990831) &&
			(*(unsigned int *)(mem + 0x8ff) == 0xFEE60290) &&
			(*(unsigned int *)(mem + 0x90f) == 0x02903985)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x817;
			unp->_depAdr = 0x0ff;
			unp->_fEndAf = 0x2d;
			unp->_endAdC = 0xffff;
			unp->_retAdr = READ_LE_UINT16(&mem[0x8ed]);
			if (unp->_retAdr == 0xa659) {
				mem[0x8ec] = 0x2c;
				unp->_retAdr = READ_LE_UINT16(&mem[0x8f0]);
			}
			unp->_idFlag = 1;
			return;
		}
	}
	/* derived from supercomp/eqseq */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x80b) == 0x8C7800A0) &&
			(*(unsigned int *)(mem + 0x812) == 0x0099082F) &&
			(*(unsigned int *)(mem + 0x846) == 0x0DADF2D0) &&
			(*(unsigned int *)(mem + 0x8c0) == 0xF001124C)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x80b;
			unp->_depAdr = 0x100;
			unp->_endAdr = 0xae;
			unp->_retAdr = READ_LE_UINT16(&mem[0x8f1]);
			if (unp->_retAdr == 0xa659) {
				mem[0x8f0] = 0x2c;
				unp->_retAdr = READ_LE_UINT16(&mem[0x8f4]);
			}
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x814) == 0xB901E678) &&
			(*(unsigned int *)(mem + 0x818) == 0xFD990829) &&
			(*(unsigned int *)(mem + 0x8a1) == 0xFDA6FDB1) &&
			(*(unsigned int *)(mem + 0x8a5) == 0xFEC602D0)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x814;
			unp->_depAdr = 0x0ff;
			unp->_fEndBf = 0x39;
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x818) == 0x2CB901E6) &&
			(*(unsigned int *)(mem + 0x81c) == 0x00FB9908) &&
			(*(unsigned int *)(mem + 0x850) == 0xFBB1C84A) &&
			(*(unsigned int *)(mem + 0x854) == 0xB1C81185)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x812;
			unp->_depAdr = 0x0ff;
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x818) == 0x2CB901E6) &&
			(*(unsigned int *)(mem + 0x81c) == 0x00FB9908) &&
			(*(unsigned int *)(mem + 0x851) == 0xFBB1C812) &&
			(*(unsigned int *)(mem + 0x855) == 0xB1C81185)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x812;
			unp->_depAdr = 0x0ff;
			unp->_endAdr = 0xae;
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x82c) == 0x018538A9) &&
			(*(unsigned int *)(mem + 0x831) == 0xFD990842) &&
			(*(unsigned int *)(mem + 0x83e) == 0x00FF4CF1) &&
			(*(unsigned int *)(mem + 0x8a5) == 0x50C651C6)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x822;
			unp->_depAdr = 0x0ff;
			unp->_fEndBf = 0x39;
			unp->_retAdr = READ_LE_UINT16(&mem[0x8ea]);
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned short int *)(mem + 0x81a) == 0x00A0) &&
			((*(unsigned int *)(mem + 0x820) == 0xFB990837) ||
			 (*(unsigned int *)(mem + 0x824) == 0xFB990837)) &&
			(*(unsigned int *)(mem + 0x83b) == 0xFD91FBB1) &&
			(*(unsigned int *)(mem + 0x8bc) == 0xEE00FC99)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x81a;
			unp->_depAdr = 0x0ff;
			unp->_fEndAf = 0x39;
			unp->_endAdC = 0xffff;
			unp->_retAdr = READ_LE_UINT16(&mem[0x8b3]);
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x812) == 0xE67800A0) &&
			(*(unsigned int *)(mem + 0x816) == 0x0823B901) &&
			(*(unsigned int *)(mem + 0x81a) == 0xC800FD99) &&
			(*(unsigned int *)(mem + 0x81e) == 0xFF4CF7D0) &&
			(*(unsigned int *)(mem + 0x885) == 0xFDA6FDB1)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x812;
			unp->_depAdr = 0x0ff;
			// $2d is unreliable, Executer uses line number at $0803/4,
			// which is read at $0039/3a by basic, as end address,
			// then can set arbitrarily $2d/$ae pointers after unpack.
			// unp->_fEndAf=0x2d;
			unp->_endAdr = READ_LE_UINT16(&mem[0x803]);
			unp->_endAdr++;
			if (*(unsigned int *)(mem + 0x87f) == 0x4CA65920)
				mem[0x87f] = 0x2c;
			unp->_retAdr = READ_LE_UINT16(&mem[0x883]);
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x812) == 0xE67800A0) &&
			(*(unsigned int *)(mem + 0x816) == 0x084CB901) &&
			(*(unsigned int *)(mem + 0x81a) == 0xA900FB99) &&
			(*(unsigned int *)(mem + 0x848) == 0x00FF4CE2)) {
			if (unp->_info->_run == -1)
				unp->_forced = 0x812;
			unp->_depAdr = 0x0ff;
			unp->_fEndAf = 0x2d;
			unp->_endAdC = 0xffff;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Triad Hack */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x838) == 0xB9080099) &&
			(*(unsigned int *)(mem + 0x83f) == 0xD0880816) &&
			(*(unsigned int *)(mem + 0x8ff) == 0xFEE60290) &&
			(*(unsigned int *)(mem + 0x90f) == 0x02903985)) {
			if (unp->_info->_run == -1) {
				for (p = 0x80b; p < 0x820; p++) {
					if ((mem[p] & 0xa0) == 0xa0) {
						unp->_forced = p;
						break;
					}
				}
			}
			unp->_depAdr = 0x0ff;
			unp->_fEndAf = 0x2d;
			unp->_endAdC = 0xffff;
			unp->_retAdr = READ_LE_UINT16(&mem[0x8ed]);
			if (unp->_retAdr == 0xa659) {
				mem[0x8ec] = 0x2c;
				unp->_retAdr = READ_LE_UINT16(&mem[0x8f0]);
			}
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
