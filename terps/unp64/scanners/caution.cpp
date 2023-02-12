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

#include "types.h"
#include "unp64.h"

namespace Unp64 {

void scnCaution(UnpStr *unp) {
	uint8_t *mem;

	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	/* quickpacker 1.0 sysless */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x801) == 0xE67800A2) &&
			(*(unsigned int *)(mem + 0x805) == 0x07EDBD01) &&
			(*(unsigned int *)(mem + 0x80d) == 0x00284CF8) &&
			(*(unsigned int *)(mem + 0x844) == 0xAC00334C)) {
			unp->_forced = 0x801;
			unp->_depAdr = 0x28;
			unp->_retAdr = READ_LE_UINT16(&mem[0x86b]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x85a]);
			unp->_fStrAf = mem[0x863];
			unp->_strAdC = EA_ADDFF | 0xffff;
			unp->_idFlag = 1;
			return;
		}
	}
	/* quickpacker 2.x + sys */
	if (unp->_depAdr == 0) {
		if (((*(unsigned int *)(mem + 0x80b) & 0xf0ffffff) == 0x60A200A0) &&
			(*(unsigned int *)(mem + 0x80f) == 0x0801BD78) &&
			(*(unsigned int *)(mem + 0x813) == 0xD0CA0095) &&
			(*(unsigned int *)(mem + 0x81e) == 0xD0C80291) &&
			(*(unsigned int *)(mem + 0x817) == 0x001A4CF8)) {
			unp->_forced = 0x80b;
			unp->_depAdr = 0x01a;
			if (mem[0x80e] == 0x69) {
				unp->_retAdr = READ_LE_UINT16(&mem[0x842]);
				unp->_endAdr = READ_LE_UINT16(&mem[0x850]);
				unp->_endAdr += 0x100;
				unp->_fStrAf = 0x4f;
				unp->_strAdC = 0xffff | EA_USE_Y;
				unp->_idFlag = 1;
				return;
			} else if (mem[0x80e] == 0x6c) {
				unp->_retAdr = READ_LE_UINT16(&mem[0x844]);
				unp->_endAdr = READ_LE_UINT16(&mem[0x84e]);
				unp->_endAdr++;
				unp->_fStrAf = 0x4d;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* strangely enough, sysless v2.0 depacker is at $0002 */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x83d) == 0xAA004A20) &&
			(*(unsigned int *)(mem + 0x801) == 0xA27800A0) &&
			(*(unsigned int *)(mem + 0x805) == 0x080FBD55) &&
			(*(unsigned int *)(mem + 0x809) == 0xD0CA0095) &&
			(*(unsigned int *)(mem + 0x80d) == 0x00024CF8)) {
			unp->_forced = 0x801;
			unp->_depAdr = 0x2;
			unp->_retAdr = READ_LE_UINT16(&mem[0x83b]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x845]);
			unp->_endAdr++;
			unp->_fStrAf = mem[0x849];
			// unp->_StrAdC=0xffff;
			unp->_idFlag = 1;
			return;
		}
	}
	/* same goes for v2.5 sysless, seems almost another packer */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x83b) == 0xAA005520) &&
			(*(unsigned int *)(mem + 0x801) == 0x60A200A0) &&
			(*(unsigned int *)(mem + 0x805) == 0x0801BD78) &&
			(*(unsigned int *)(mem + 0x809) == 0xD0CA0095) &&
			(*(unsigned int *)(mem + 0x80d) == 0x00104CF8)) {
			unp->_forced = 0x801;
			unp->_depAdr = 0x10;
			unp->_retAdr = READ_LE_UINT16(&mem[0x839]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x847]);
			unp->_endAdr += 0x100;
			unp->_fStrAf = 0x46;
			unp->_strAdC = 0xffff | EA_USE_Y;
			unp->_idFlag = 1;
			return;
		}
	}
	/* hardpacker */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x80d) == 0x8534A978) &&
			(*(unsigned int *)(mem + 0x811) == 0xB9B3A001) &&
			(*(unsigned int *)(mem + 0x815) == 0x4C99081F) &&
			(*(unsigned int *)(mem + 0x819) == 0xF7D08803) &&
			(*(unsigned int *)(mem + 0x81d) == 0xB9034D4C)) {
			unp->_forced = 0x80d;
			unp->_depAdr = 0x34d;
			unp->_retAdr = READ_LE_UINT16(&mem[0x87f]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x88d]);
			unp->_fStrAf = 0x3ba;
			unp->_strAdC = EA_ADDFF | 0xffff;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
