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

/* FinalSuperCompressor/flexible */
void scnFinalSuperComp(UnpStr *unp) {
	unsigned char *mem;
	int q, p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
        if (((*(unsigned int *)(mem + 0x810) & 0xff00ff00) == 0x9A00A200) &&
			u32eq(mem + 0x832, 0x0DF008C9) &&
			u32eq(mem + 0x836, 0xB1083DCE) &&
			u32eq(mem + 0x8af, 0x4C00FFBD) &&
			(mem[0x88e] == 0x4c || mem[0x889] == 0x4c)) {
			mem[0x812] = 0xff; /* fixed, can't be otherwise */
			unp->_depAdr = READ_LE_UINT16(&mem[0x856]);
			if (unp->_info->_run == -1)
				unp->_forced = 0x811;
			for (p = 0x889; p < 0x88f; p++) {
				if (mem[p] == 0x4c) {
					unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
					break;
				}
			}
			unp->_strMem = READ_LE_UINT16(&mem[0x872]);
			unp->_endAdr = mem[0x84e] | mem[0x852] << 8;
			unp->_endAdr++;
			unp->_idFlag = 1;
			return;
		}
	}
	/* SC/EqualSequences, FinalPack4/UA and TLC packer (sys2061/2064/2065/2066) */
	if (unp->_depAdr == 0) {
		for (p = 0x80d; p < 0x830; p++) {
			if (mem[p + 2] == 0xb9) {
				if (mem[p + 5] == 0x99 && mem[p + 0x15] == 0x4c &&
					mem[p + 0x10] == mem[p + 0x81] &&
					u32eq(mem + p + 0x78, 0x18F7D0FC) &&
					u32eq(mem + p + 0x8c, 0xD0FCC4C8)) {
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x16]);
					for (q = 0x80b; q < 0x813; q++) {
						if (mem[q] == 0x78) {
							unp->_forced = q;
							break;
						}
					}
					if (q == 0x813) {
						if (mem[p] == 0xa0)
							unp->_forced = p;
					}
					unp->_endAdr = mem[p + 0x10];                  /* either 0x2d or 0xae */
					unp->_strMem = READ_LE_UINT16(&mem[p + 0x2c]); /* $0800 fixed? */
					p += 0x40;
					while ((mem[p] != 0x4c) && (p < 0x860))
						p++;
					unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
					break;
				}
			}
		}
		if (unp->_depAdr) {
			unp->_idFlag = 1;
			return;
		}
	}
	/* SC/Equal chars */
	if (unp->_depAdr == 0) {
        if (((*(unsigned int *)(mem + 0x810) & 0xf0ffffff) == 0xA0A93878) &&
			(u32eq(mem + 0x814, 0xFC852DE5) ||
			 u32eq(mem + 0x814, 0xFC85aeE5)) &&
			(u32eq(mem + 0x818, 0xafE508A9) ||
			 u32eq(mem + 0x818, 0x2EE508A9)) &&
            ((*(unsigned int *)(mem + 0x844) & 0x00ffffff) == 0x00004C9A)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x846]);
			unp->_forced = 0x810;
			unp->_retAdr = READ_LE_UINT16(&mem[0x872]);
			unp->_endAdr = mem[0x866]; /* either 0x2d or 0xae */
			unp->_strMem = READ_LE_UINT16(&mem[0x857]);
			unp->_strMem += mem[0x849]; /* ldx #$?? -> sta $????,x; not always 0! */
			unp->_idFlag = 1;
			return;
		}
	}
	/* FinalSuperCompressor/flexible hacked? */
	if (unp->_depAdr == 0) {
        if (u32eqmasked(mem + 0x80d, 0xff00ffff, 0x9A00A278) &&
			u32eq(mem + 0x814, 0x9D0847BD) &&
			u32eq(mem + 0x818, 0x21BD0334) &&
			u32eq(mem + 0x81c, 0x040E9D09) &&
			mem[0x878] == 0x4c) {
			mem[0x80f] = 0xff; /* fixed, can't be otherwise */
			unp->_depAdr = 0x334;
			if (unp->_info->_run == -1)
				unp->_forced = 0x80d;
			unp->_retAdr = READ_LE_UINT16(&mem[0x879]);
			/* patch */
			mem[0x2a7] = 0x85;
			mem[0x2a8] = 0x01;
			mem[0x2a9] = 0x4c;
			mem[0x2aa] = mem[0x879];
			mem[0x2ab] = mem[0x87a];
			unp->_strMem = 0x2a7;
			unp->_fEndAf = 0x34e;
			unp->_endAdC = 0xffff;
			unp->_recurs = RECUMAX - 2;
			unp->_idFlag = 1;
			return;
		}
	}
	/* FinalSuperCompressor/flexible hacked? 2nd layer */
	if (unp->_depAdr == 0) {
		if (unp->_info->_start == 0x2a7) {
			if (mem[0x2a7] == 0x85 &&
				mem[0x2a8] == 0x01 &&
				mem[0x2a9] == 0x4c) {
				p = READ_LE_UINT16(&mem[0x2aa]);
				if ((*(unsigned int *)(mem + p) == (((unsigned int)((mem[0x2aa] + 0x15) & 0xff) << 24) | 0x00B99EA0U)) &&
                    u32eq(mem + p + 0x96, 0x033B4CFE)) {
					unp->_forced = 0x2a7;
					unp->_depAdr = 0x334;
					unp->_retAdr = READ_LE_UINT16(&mem[p + 0x2c]);
					//unp->_strMem=mem[p+0x17]|mem[p+0x18];
					unp->_strMem = 0x2ac;
					mem[0x2ac] = 0x85;
					mem[0x2ad] = 0x01;
					mem[0x2ae] = 0x4c;
					mem[0x2af] = mem[p + 0x2c];
					mem[0x2b0] = mem[p + 0x2d];
					unp->_fEndAf = 0x335;
					unp->_endAdC = EA_USE_Y | 0xffff;
					unp->_idFlag = 1;
					return;
				}
			}
		}
	}
	/* FinalSuperCompressor/flexible hacked? 3rd layer */
	if (unp->_depAdr == 0) {
		if (unp->_info->_start == 0x2ac) {
			if (mem[0x2ac] == 0x85 &&
				mem[0x2ad] == 0x01 &&
				mem[0x2ae] == 0x4c) {
				p = READ_LE_UINT16(&mem[0x2af]);
				if (mem[p] == 0xa9 &&
					u32eq(mem + p + 0x0a, 0xAF852E85) &&
					u32eq(mem + p + 0x1f, 0x8D034B4C)) {
					unp->_forced = 0x2ac;
					unp->_depAdr = 0x34b;
					unp->_fEndAf = 0x2d;
					unp->_idFlag = 1;
					return;
				}
			}
		}
	}
	/* another special hack */
	if (unp->_depAdr == 0) {
		if (u32eqmasked(mem + 0x80d, 0xff00ffff, 0x9a00a278) &&
			u32eq(mem + 0x811, 0x018534a9) &&
			u32eq(mem + 0x818, 0x34990846) &&
			u32eq(mem + 0x81c, 0x0927b903) &&
			mem[0x87e] == 0x4c) {
			mem[0x80f] = 0xff; /* fixed, can't be otherwise */
			unp->_depAdr = 0x334;
			if (unp->_info->_run == -1)
				unp->_forced = 0x80d;
			unp->_retAdr = 0x7f8;
			unp->_strMem = unp->_retAdr;
			unp->_fEndAf = 0x34e;
			p = 0x7f8;
			mem[p++] = 0x85;
			mem[p++] = 0x01;
			mem[p++] = 0x4c;
			mem[p++] = mem[0x87f];
			mem[p++] = mem[0x880];
			unp->_idFlag = 1;
			return;
		}
	}
	/* Flexible / Generic Hack */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x8b0, 0x4D4C00FF) &&
			u32eq(mem + 0x918, 0x4D4C0187) &&
			u32eq(mem + 0x818, 0x57BDCCA2) &&
			u32eqmasked(mem + 0x81c, 0xf0ffffff, 0x00339d08) &&
			mem[0x88e] == 0x4c) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x856]);
			for (p = 0x80b; p < 0x818; p++) {
				if (mem[p] == 0x78) {
					unp->_forced = p;
					break;
				}
			}
			unp->_retAdr = READ_LE_UINT16(&mem[0x88f]);
			unp->_strMem = READ_LE_UINT16(&mem[0x872]);
			unp->_endAdr = mem[0x84e] | mem[0x852] << 8;
			unp->_endAdr++;
			unp->_idFlag = 1;
			return;
		}
	}
	/* eq seq + indirect jmp ($0348) -> expects $00b9 containing $60 (rts) */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x810, 0x8534A978) &&
			u32eq(mem + 0x834, 0x03354C48) &&
			u32eq(mem + 0x850, 0xF0C81BF0) &&
			u32eq(mem + 0x865, 0xEE03486C)) {
			unp->_depAdr = 0x335;
			unp->_forced = 0x810;
			unp->_retAdr = mem[0x82f] | mem[0x80e] << 8;
			unp->_retAdr++;
			mem[0xb9] = 0x60;
			mem[0x865] = 0x4c;
			mem[0x866] = unp->_retAdr & 0xff;
			mem[0x867] = unp->_retAdr >> 8;
			unp->_endAdr = mem[0x825]; /* either 0x2d or 0xae */
			unp->_strMem = READ_LE_UINT16(&mem[0x84c]);
			unp->_idFlag = 1;
			return;
		}
	}
	/* eq char hack, usually 2nd layer of previous eq seq hack */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x810, 0x1EB97FA0) &&
			u32eq(mem + 0x814, 0x033B9908) &&
			u32eq(mem + 0x838, 0xFE912DB1) &&
			u32eq(mem + 0x88a, 0xFE850369)) {
			unp->_depAdr = 0x33b;
			unp->_forced = 0x810;
			unp->_retAdr = READ_LE_UINT16(&mem[0x89a]);
			unp->_fEndAf = 0x2d;
			unp->_strMem = 0x800; /* fixed */
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
