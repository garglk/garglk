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

#include <cstring>

#include "exo_util.h"
#include "types.h"
#include "unp64.h"

namespace Unp64 {

void scnIntros(UnpStr *unp) {
	unsigned char *mem;
	int q = 0, p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	/* HIV virus */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x80b) == 0x10DD002C) &&
			(*(unsigned int *)(mem + 0x84f) == 0x34A9EDDD) &&
			(*(unsigned int *)(mem + 0x8a1) == 0xA94D2D57) &&
			(*(unsigned int *)(mem + 0x9bc) == 0xF004B120)) {
			unp->_forced = 0x859;
			unp->_depAdr = 0x100;
			unp->_retAdr = 0xa7ae;
			unp->_strMem = 0x801;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
	/* BHP virus */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x821) == 0xADA707F0) &&
			(*(unsigned int *)(mem + 0x825) == 0x32A55DA6) &&
			(*(unsigned int *)(mem + 0x920) == 0xD13D20D1) &&
			(*(unsigned int *)(mem + 0xef5) == 0x02CD4C20)) {
			unp->_forced = 0x831;
			unp->_depAdr = 0x2ab;
			unp->_retAdr = 0xa7ae;
			unp->_rtAFrc = 1;
			unp->_strMem = 0x801;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Bula virus */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x810) == 0x78084220) &&
			(*(unsigned int *)(mem + 0x853) == 0x4DA908F6) &&
			(*(unsigned int *)(mem + 0xa00) == 0xF00348AD) &&
			(*(unsigned int *)(mem + 0xac4) == 0x04B14C0D)) {
			unp->_forced = 0x810;
			unp->_depAdr = 0x340;
			unp->_retAdr = 0xa7ae;
			unp->_rtAFrc = 1;
			unp->_strMem = 0x801;
			unp->_endAdr = unp->_info->_end - 0x2fa;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Coder virus */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x81f) == 0xD3018D08) &&
			(*(unsigned int *)(mem + 0x823) == 0xA9082D8D) &&
			(*(unsigned int *)(mem + 0x89F) == 0x78802C20) &&
			(*(unsigned int *)(mem + 0x8AF) == 0x6CE3974C)) {
			unp->_forced = 0x812;
			unp->_depAdr = 0x2b3;
			unp->_retAdr = 0xa7ae;
			unp->_rtAFrc = 1;
			unp->_strMem = 0x801;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Boa virus */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x80b) == 0x10A900A0) &&
			(*(unsigned int *)(mem + 0x81b) == 0xEDDD2008) &&
			(*(unsigned int *)(mem + 0x85F) == 0x0401004C) &&
			(*(unsigned int *)(mem + 0x9F6) == 0x04EE4C00)) {
			unp->_forced = 0x80b;
			unp->_depAdr = 0x100;
			unp->_retAdr = 0xa7ae;
			unp->_rtAFrc = 1;
			unp->_strMem = 0x801;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Relax intros */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0xB50) == 0x0C632078) &&
			(*(unsigned int *)(mem + 0xb54) == 0xA90C5A20) &&
			(*(unsigned int *)(mem + 0xc30) == 0x4C0D6420) &&
			(*(unsigned int *)(mem + 0xc34) == 0x00A2EA31)) {
			unp->_forced = 2070;
			unp->_depAdr = 0x100;
			unp->_retAdr = READ_LE_UINT16(&mem[0xbea]);
			unp->_rtAFrc = 1;
			unp->_strMem = 0x0801;
			unp->_endAdr = 0x2d;
			mem[0xbae] = 0x24; /* lda $c5 is not simulated in UNP64 */
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x816) == 0x2ABD00A2) &&
			(*(unsigned int *)(mem + 0x81a) == 0xCE009D08) &&
			(*(unsigned int *)(mem + 0x826) == 0xCE004CF1) &&
			(*(unsigned int *)(mem + 0x9F9) == 0x5A5A5A5A)) {
			unp->_forced = 0x816;
			unp->_depAdr = 0xcf7e; /* highmem unpacker always need to be managed */
			unp->_retAdr = READ_LE_UINT16(&mem[0x9a9]);
			unp->_rtAFrc = 1;
			unp->_strMem = 0x0801;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return;
		}
	}
	/* F4CG */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x812) == 0x20FDA320) &&
			(*(unsigned int *)(mem + 0x890) == 0x06309DD8) &&
			(*(unsigned int *)(mem + 0x9BF) == 0xA2A9D023) &&
			(*(unsigned int *)(mem + 0xa29) == 0xFFFF2CFF)) {
			unp->_forced = 0x812;
			unp->_depAdr = 0x110;
			p = mem[0x8ee];
			unp->_retAdr = (mem[0xe44] ^ p) | (mem[0xe45] ^ p) << 8;
			unp->_strMem = 0x0801;
			unp->_endAdr = mem[0x8f8] | mem[0x8fc] << 8;
			mem[0xa2d] = 0x4c; /* indirect jmp $0110 using basic rom */
			mem[0xa2e] = 0x10;
			mem[0xa2f] = 0x01;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Triad */
	if (unp->_depAdr == 0) {
		for (p = 0x80d; p < 0x828; p++) {
			if ((*(unsigned int *)(mem + p + 0x000) == 0xA9D0228D) &&
				(*(unsigned int *)(mem + p + 0x00c) == 0x06979D0A) &&
				(*(unsigned int *)(mem + p + 0x0a0) == 0x03489D03) &&
				(*(unsigned int *)(mem + p + 0x0a4) == 0x9D0371BD)) {

				for (q = 0; q < 5; q += 4) {
					if ((*(unsigned int *)(mem + p + 0x1ec + q) == 0x04409D04) &&
						(*(unsigned int *)(mem + p + 0x210 + q) == 0x607EFF60)) {
						unp->_depAdr = 0x100;
						break;
					}
				}
				break;
			}
		}
		if (unp->_depAdr) {
			unp->_forced = p;
			unp->_retAdr = READ_LE_UINT16(&mem[p + q + 0x28a]);
			unp->_endAdr = mem[p + q + 0x19e] | mem[p + q + 0x1a4] << 8;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x81f) == 0xA2D0228D) &&
			(*(unsigned int *)(mem + 0x823) == 0x0A5CBD28) &&
			(*(unsigned int *)(mem + 0x8d7) == 0x8509754C) &&
			(*(unsigned int *)(mem + 0x975) == 0x07A053A2)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x81f;
			unp->_retAdr = READ_LE_UINT16(&mem[0xac3]);
			unp->_endAdr = mem[0x9cd] | mem[0x9d3] << 8;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
	}
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x821) == 0xA9D0228D) &&
			(*(unsigned int *)(mem + 0x82a) == 0x0A3aBD28) &&
			(*(unsigned int *)(mem + 0x8a7) == 0x85093c4C) &&
			(*(unsigned int *)(mem + 0x93c) == 0x17108EC6)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x821;
			unp->_retAdr = READ_LE_UINT16(&mem[0xa99]);
			unp->_endAdr = mem[0x9bc] | mem[0x9c2] << 8;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Snacky/G*P */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x818) == 0x018533A9) &&
			(*(unsigned int *)(mem + 0x888) == 0x08684C03) &&
			(*(unsigned int *)(mem + 0x898) == 0x40A9FAD0) &&
			(*(unsigned int *)(mem + 0xa2d) == 0x0A2E4C58)) {
			for (q = 0xc3c; q < 0xc4f; q++) {
				if ((mem[q] == 0xa9) &&
					(mem[q + 2] == 0x85) &&
					(mem[q + 3] == 0x01) &&
					(mem[q + 4] == 0x4c)) {
					unp->_forced = q;
					unp->_endAdr = READ_LE_UINT16(&mem[q + 5]);
					break;
				}
				if ((mem[q] == 0x84) &&
					(mem[q + 1] == 0x01) &&
					(mem[q + 2] == 0x4c)) {
					unp->_forced = q;
					unp->_endAdr = READ_LE_UINT16(&mem[q + 3]);
					break;
				}
			}
			unp->_depAdr = 0x340;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
	}
	/* 711 introdes 3 */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x1118) == 0x8D01A978) &&
			(*(unsigned int *)(mem + 0x111c) == 0x1A8DDC0D) &&
			(*(unsigned int *)(mem + 0x1174) == 0x11534C2A) &&
			(*(unsigned int *)(mem + 0x2B30) == 0x786001F0)) {
			unp->_forced = 0x2b33;
			/* it restores $3fff by saving it to $02 =) */
			mem[2] = mem[0x3fff];
			if (*(unsigned int *)(mem + 0x2B4F) == 0xE803409D) {
				unp->_depAdr = 0x340;
				unp->_strMem = mem[0x2b65] | mem[0x2b69] << 8;
				unp->_endAdr = unp->_info->_end - (mem[0x2b5d] | mem[0x2b61] << 8) + unp->_strMem;
				p = 0x2b7f;
				if (*(unsigned int *)(mem + p - 1) == 0x4CA65920) {
					mem[p - 1] = 0x2c;
					p += 3;
				}
				unp->_retAdr = READ_LE_UINT16(&mem[p]);
			}
			unp->_idFlag = 1;
			return;
		}
	}
	/* BN 1872 intromaker(?), magic disk and so on */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x4800) == 0x48F02078) &&
			(*(unsigned int *)(mem + 0x4E67) == 0x48004CFC) &&
			(*(unsigned int *)(mem + 0x4851) == 0x4E00BD78) &&
			(*(unsigned int *)(mem + 0x4855) == 0xE800FA9D)) {
			unp->_forced = 0x4851;
			unp->_depAdr = 0xfa;
			unp->_endAdr = 0x2d;
			unp->_strMem = 0x800;
			unp->_idFlag = 1;
			return;
		}
	}
	/* generic excell/ikari reloc routine */
	if (unp->_depAdr == 0) {
		for (p = 0xe00; p < 0x3400; p++) {
			if ((*(unsigned int *)(mem + p + 0x00) == 0x01A90385) &&
				(*(unsigned int *)(mem + p + 0x04) == 0x08A90485) &&
				(*(unsigned int *)(mem + p + 0x10) == 0xE6F9D0C8)) {
				for (q = p + 0x4e; q < p + 0x70; q++) {
					if ((*(unsigned int *)(mem + q) & 0xffffff00) == 0x00206C00) {
						q = 1;
						break;
					}
				}
				if (q != 1)
					break;

				if ((mem[p - 0x100 + 0xec] == 0xa2) &&
					(mem[p - 0x100 + 0xee] == 0xbd) &&
					(*(unsigned int *)(mem + p - 0x100 + 0xe8) == 0x018534A9) &&
					(*(unsigned int *)(mem + p - 0x100 + 0xf1) == 0xE804009D) &&
					(*(unsigned int *)(mem + p - 0x100 + 0xf5) == 0x004CF7D0)) {
					unp->_forced = p - 0x100 + 0xe8;
					unp->_depAdr = 0x400;
					break;
				}
				if ((mem[p - 0x100 + 0xc8] == 0xa2) &&
					(mem[p - 0x100 + 0xca] == 0xbd) &&
					(*(unsigned int *)(mem + (READ_LE_UINT16(&mem[p - 0x100 + 0xcb])) + 0x0f) == 0x018534A9) &&
					(*(unsigned int *)(mem + p - 0x100 + 0xcd) == 0xA904009D) &&
					(*(unsigned int *)(mem + p - 0x100 + 0xe3) == 0x040F4CF8)) {
					unp->_forced = p - 0x100 + 0xc8;
					unp->_depAdr = 0x40f;
					break;
				}
			}
		}

		if (unp->_depAdr) {
			unp->_endAdr = mem[p + 0x22] | mem[p + 0x24] << 8;
			unp->_strMem = 0x801;
			for (q = p + 0x29; q < p + 0x39; q++) {
				if ((mem[q] == 0xa9) || (mem[q] == 0xa2) || (mem[q] == 0xa0) ||
					(mem[q] == 0x85) || (mem[q] == 0x86)) {
					q++;
					continue;
				}
				if ((mem[q] == 0x8d) || (mem[q] == 0x8e)) {
					q += 2;
					continue;
				}
				if ((mem[q] == 0x20) || (mem[q] == 0x4c)) {
					unp->_retAdr = READ_LE_UINT16(&mem[q + 1]);
					if ((unp->_retAdr == 0xa659) && (mem[q] == 0x20)) {
						mem[q] = 0x2c;
						unp->_retAdr = 0;
						continue;
					}
					break;
				}
			}
			unp->_idFlag = 1;
			return;
		}
	}
	/* ikari-06 by tridos/ikari */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0xC00) == 0xD0CAA5A2) &&
			(*(unsigned int *)(mem + 0xC10) == 0xE807A00C) &&
			(*(unsigned int *)(mem + 0xC20) == 0x201CFB20) &&
			(*(unsigned int *)(mem + 0xfe8) == 0x60BD80A2) &&
			(*(unsigned int *)(mem + 0xd60) == 0x8534A978)) {
			unp->_forced = 0xfe8;
			unp->_depAdr = 0x334;
			unp->_endAdr = 0x2d;
			unp->_strMem = 0x801;
			unp->_retAdr = READ_LE_UINT16(&mem[0xda0]);
			unp->_idFlag = 1;
			return;
		}
	}
	/* flt-01 version at $0801 */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x998) == 0x78FF5B20) &&
			(*(unsigned int *)(mem + 0x9a8) == 0xBE8DFAB1) &&
			(*(unsigned int *)(mem + 0xd3b) == 0xFD152078) &&
			(*(unsigned int *)(mem + 0xd4b) == 0xD0CA033B) &&
			(*(unsigned int *)(mem + 0xd5b) == 0xA90DFD4C)) {
			unp->_forced = 0xd3b;
			unp->_depAdr = 0x33c;
			unp->_endAdr = mem[0xb5c] | mem[0xb62] << 8;
			unp->_strMem = 0x801;
			unp->_retAdr = READ_LE_UINT16(&mem[0xb6b]);
			unp->_idFlag = 1;
			return;
		}
	}
	/* s451-09 at $0801 */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x801) == 0xE67800A0) &&
			(*(unsigned int *)(mem + 0x805) == 0xA62DA501) &&
			(*(unsigned int *)(mem + 0x82a) == 0xFD91FBB1) &&
			(*(unsigned int *)(mem + 0x831) == 0xC6E6D00F) &&
			(*(unsigned int *)(mem + 0x841) == 0x61BE0004)) {
			/* first moves $0839-(eof) to $1000 */
			p = mem[0x837];
			if ((p == 0x3e) || (p == 0x48)) {
				unp->_endAdr = mem[0x80f] | mem[0x811] << 8;
				memmove(mem + 0x1000, mem + 0x839, (size_t)(unp->_endAdr - 0x1000));
				if (p == 0x3e) {
					unp->_strMem = READ_LE_UINT16(&mem[0x113c]);
				} else /* if(p==0x48) */
				{
					unp->_strMem = READ_LE_UINT16(&mem[0x114c]);
				}
				unp->_depAdr = unp->_strMem;
				unp->_retAdr = unp->_strMem;
				unp->_forced = unp->_strMem;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* Super_Titlemaker/DSCompware */
	if (unp->_depAdr == 0) {
		if ((*(unsigned int *)(mem + 0x820) == 0x8536A978) &&
			(*(unsigned int *)(mem + 0x830) == 0x8570A9F9) &&
			(*(unsigned int *)(mem + 0x860) == 0xB100A0B5) &&
			(*(unsigned int *)(mem + 0x864) == 0xE60285B2)) {
			unp->_endAdr = mem[0x854] | mem[0x852] << 8;
			//unp->_endAdC=1;
			unp->_depAdr = 0x851;
			unp->_retAdr = READ_LE_UINT16(&mem[0x88a]);
			unp->_strMem = unp->_retAdr;
			unp->_forced = 0x820;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
