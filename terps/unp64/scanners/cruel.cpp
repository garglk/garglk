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

void scnCruel(UnpStr *unp) {
	uint8_t *mem;
	int q, p, strtmp = 0;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		if (mem[0x810] == 0xb9 &&
			((*(unsigned int *)(mem + 0x813) & 0xfffffeff) == 0xC800FA99) &&
			u16eq(mem + 0x818, 0x4CF7)) {
			if (mem[0x814] == 0xFA) {
				p = READ_LE_UINT16(&mem[0x811]); // mem[0x811] | mem[0x812] << 8;
				if (u32eq(mem + p + 9, 0xC8071C99)) {
					unp->_endAdr = READ_LE_UINT16(&mem[p + 2]); // mem[p + 2] | mem[p + 3] << 8;
					unp->_depAdr = 0x100;
					if (unp->_info->_run == -1)
						unp->_forced = 0x80b;
					unp->_fStrAf = 0xfc;
					q = READ_LE_UINT16(&mem[p + 7]); // mem[p + 7] | mem[p + 8] << 8;
					if ((mem[q + 0x8e] == 0xc6) && (mem[q + 0x8f] == 0x01) &&
						(mem[q + 0x93] == 0xe6) && (mem[q + 0x94] == 0x01)) {
						mem[q + 0x90] = 0x2c;
					}
					/* retadr is not always at the same addr, but at least can't
					   be anything < $07e8
					*/
					// unp->_retAdr=0x7e8;
					q = READ_LE_UINT16(&mem[p + 7]); // mem[p + 7] | mem[p + 8] << 8;
					if (mem[q + 0x3c] == 0x4c) {
						/* v2.2/dynamix, v2.5/cross, v2.5/crest */
						strtmp = *(unsigned short int *)(mem + q + 0x3d);
					} else if (mem[q + 0x4a] == 0x4c) {
						strtmp = *(unsigned short int *)(mem + q + 0x4b);
					} else if (mem[q + 0x3f] == 0x4c) {
						/* v2.2/oneway+scs, also hacked as cruel 2mhz 1.0 */
						strtmp = *(unsigned short int *)(mem + q + 0x40);
					} else {
						/* todo: determine real retadr, for now a default seems ok */
						strtmp = 0;
					}
					if (strtmp) {
						if (strtmp >= unp->_retAdr) {
							unp->_retAdr = strtmp;
						} else { /* now search it... variable code here */
							strtmp += p - *(unsigned short int *)(mem + 0x814);
							for (q = strtmp; q < unp->_info->_end; q++) {
								if ((mem[q] == 0xa9) || (mem[q] == 0x85)) {
									q++;
									continue;
								}
								if (mem[q] == 0x8d) {
									q += 2;
									continue;
								}
								if (mem[q] == 0x4c) {
									unp->_retAdr = *(unsigned short int *)(mem + q + 1);
									;
									break;
								}
							}
						}
					}
				}
			} else if (mem[0x814] == 0xFB) {
				/* not Cruel2 but MSCRUNCH by Marco/Taboo
				   v1.0 works only with some old AR cart (unless patched ;)
				   v1.5 is infact more common
				*/
				p = READ_LE_UINT16(&mem[0x811]); // mem[0x811] | mem[0x812] << 8;
				if (u32eq(mem + p + 7, 0xC8071C99)) {
					unp->_endAdr = READ_LE_UINT16(&mem[p + 3]); // mem[p + 3] | mem[p + 4] << 8;
					unp->_depAdr = 0x100;
					unp->_forced = 0x80b;
					unp->_fStrAf = 0xfe;
					if ((mem[p + 0x93] == 0x4c) && (mem[p + 0xa1] == 0x4c)) {
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0xa2]); // mem[p + 0xa2] | mem[p + 0xa3] << 8;
					} else if ((mem[p + 0x8c] == 0x4c) && (mem[p + 0x94] == 0x4c)) {
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0x95]); // mem[p + 0x95] | mem[p + 0x96] << 8;
					} else if ((mem[p + 0x20] == 0x4c) && (mem[p + 0x28] == 0x4c)) {
						unp->_retAdr = READ_LE_UINT16(&mem[p + 0x29]); // mem[p + 0x29] | mem[p + 0x2a] << 8;
					}
				}
			}
		}
		if (unp->_depAdr) {
			unp->_idFlag = 1;
			return;
		}
	}
	/* MSCRUNCH 1.5 hack by Anubis */
	if (unp->_depAdr == 0) {
		if (mem[0x819] == 0x4c) {
			p = READ_LE_UINT16(&mem[0x81a]); //mem[0x81a] | mem[0x81b] << 8;
			if ((mem[p] == 0xa9) && (mem[p + 0x0f] == 0x30) &&
				u32eq(mem + p + 0x13, 0xCA04009D) &&
				u32eq(mem + p + 0x38, 0x01084C01)) {
				q = READ_LE_UINT16(&mem[p + 0x1f]); // mem[p + 0x1f] | mem[p + 0x20] << 8;
				if (u32eq(mem + q + 7, 0xC8071C99)) {
					unp->_endAdr = READ_LE_UINT16(&mem[q + 3]); // mem[q + 3] | mem[q + 4] << 8;
					unp->_depAdr = 0x100;
					if (unp->_info->_run == -1)
						unp->_forced = 0x819;
					unp->_fStrAf = 0xfe;
					unp->_retAdr = READ_LE_UINT16(&mem[q + 0xa2]); // mem[q + 0xa2] | mem[q + 0xa3] << 8;
				}
			}
		}
	}
	/* fast cruel 4.x */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80b, 0xE67800A0) &&
			u32eq(mem + 0x813, 0xC8034099) &&
			(u32eq(mem + 0x818, 0x03404cF7) ||
			 u32eq(mem + 0x818, 0x03b34cF7) ||
			 u32eq(mem + 0x818, 0x03db4cF7))) {
			p = READ_LE_UINT16(&mem[0x811]); // mem[0x811] | mem[0x812] << 8;
			if (u32eq(mem + p, 0xa75801c6)) {
				p += 0x45;
				q = READ_LE_UINT16(&mem[p]);                // mem[p] | mem[p + 1] << 8;
				unp->_endAdr = READ_LE_UINT16(&mem[q + 2]); // mem[q + 2] | mem[q + 3] << 8;
				unp->_depAdr = 0x340;
				unp->_forced = 0x80b;
				unp->_fStrAf = 0xfc;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* Cruel 2.0 / (BB) packer header */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x837, 0x9D0845BD) &&
			u32eq(mem + 0x84f, 0xE808039D) &&
			u32eq(mem + 0x83b, 0xC9E803B7)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x843]); // mem[0x843] | mem[0x844] << 8;
			if (unp->_info->_run == -1)
				unp->_forced = 0x80d;
			unp->_retAdr = READ_LE_UINT16(&mem[0x868]); // mem[0x868] | mem[0x869] << 8;
			unp->_endAdr = unp->_info->_end - 0x90;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
		if (u32eq(mem + 0x845, 0x03E04CF2) &&
			u32eq(mem + 0x852, 0x9D0893BD) &&
			u32eq(mem + 0x856, 0xD0E80803)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x847]); // mem[0x847] | mem[0x848] << 8;
			if (unp->_info->_run == -1)
				unp->_forced = 0x80d;
			unp->_retAdr = READ_LE_UINT16(&mem[0x869]); // mem[0x869] | mem[0x86a] << 8;
			unp->_endAdr = unp->_info->_end - 0x90;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
		if (u32eq(mem + 0x841, 0x03B74CF5) &&
			u32eq(mem + 0x84c, 0x9D089BBD) &&
			u32eq(mem + 0x850, 0xD0E8080B)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x843]); // mem[0x843] | mem[0x844] << 8;
			if (unp->_info->_run == -1) {
				unp->_forced = 0x811;
			} else {
				mem[0x808] = '5'; /* just to be safe, change sys for next layer */
				mem[0x809] = '9'; /* this hdr leaves it as sys2065 */
			}
			unp->_retAdr = READ_LE_UINT16(&mem[0x868]); // mem[0x868] | mem[0x869] << 8; /* fixed $080b */
			unp->_endAdr = unp->_info->_end - 0x90;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
		/* this is a totally useless header, cheers TCOM! */
		if (u32eq(mem + 0x80b, 0x1BB900A0) &&
			u32eq(mem + 0x80f, 0x03B79908) &&
			u32eq(mem + 0x823, 0x039D0840)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x819]); // mem[0x819] | mem[0x81a] << 8;
			if (unp->_info->_run == -1)
				unp->_forced = 0x80b;
			unp->_retAdr = READ_LE_UINT16(&mem[0x83e]); // mem[0x83e] | mem[0x83f] << 8;
			unp->_endAdr = unp->_info->_end - 0x3d;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return;
		}
	}
	/* Cruel 2.0 / (BB) packer sysless */
	if (unp->_depAdr == 0) {
		if (u32eqmasked(mem + 0x80b, 0x0000ffff, 0x000000A0) &&
			u32eq(mem + 0x817, 0xC800CB99) &&
			u32eq(mem + 0x81b, 0x004CF7D0) && mem[0x81f] == 1) {
			p = READ_LE_UINT16(&mem[0x815]); // mem[0x815] | mem[0x816] << 8;
			p += 0x31;
			if ((mem[p + 4] == 0xb9) &&
				u32eq(mem + p + 7, 0xC8072099)) {
				unp->_forced = 0x80b;
				unp->_depAdr = 0x100;
				unp->_endAdr = READ_LE_UINT16(&mem[p]); // mem[p] | mem[p + 1] << 8;
				unp->_fStrAf = 0xfc;
				/* patch: some version contain a zp cleaner sub at $01a2 */
				if (u32eq(mem + p + 0xa6, 0x00A9CBA2) &&
					u32eq(mem + p + 0xaa, 0xD0E80095)) {
					mem[p + 0xa6] = 0x60;
				}
				/* patch: some version expects $01==#$34 already set from the header */
				if (u32eq(mem + 0x811, 0xb9eaeaea)) {
					mem[0x811] = 0xe6;
					mem[0x812] = 0x01;
				}
				q = READ_LE_UINT16(&mem[p + 5]); // mem[p + 5] | mem[p + 6] << 8;
				unp->_retAdr = 0x7e8;
				if (mem[q + 0x6c] == 0x4c)
					unp->_retAdr = READ_LE_UINT16(&mem[q + 0x6d]); // mem[q + 0x6d] | mem[q + 0x6e] << 8;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* Cruel 2.1 / STA */
	if (unp->_depAdr == 0) {
		if (mem[0x80b] == 0xa0 && u32eq(mem + 0x817, 0xC800CB99) &&
			u32eq(mem + 0x81b, 0x004CF7D0) && mem[0x81f] == 1) {
			p = READ_LE_UINT16(&mem[0x815]); // mem[0x815] | mem[0x816] << 8;
			p += 0x31;
			if (mem[p + 6] == 0xb9 &&
				u32eq(mem + p + 9, 0xC8072099)) {
				unp->_forced = 0x80b;
				unp->_depAdr = 0x100;
				unp->_endAdr = READ_LE_UINT16(&mem[p]); // mem[p] | mem[p + 1] << 8;
				unp->_fStrAf = 0xfc;
				q = READ_LE_UINT16(&mem[p + 7]); // mem[p + 7] | mem[p + 8] << 8;
				unp->_retAdr = 0x7e8;
				if (mem[q + 0x6c] == 0x4c)
					unp->_retAdr = READ_LE_UINT16(&mem[q + 0x6d]); // mem[q + 0x6d] | mem[q + 0x6e] << 8;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* unknown cruel, jmp $00e9, found in Illusion/Random warez */
	if (unp->_depAdr == 0) {
		if (mem[0x810] == 0xb9 && u32eq(mem + 0x813, 0xC800e999) &&
			u32eq(mem + 0x818, 0x00e94CF7)) {
			p = READ_LE_UINT16(&mem[0x811]); // mem[0x811] | mem[0x812] << 8;
			q = p - 0xed;
			if (u32eq(mem + p, 0x13F01284) &&
				u32eq(mem + q, 0xA9C8C8C8)) {
				unp->_depAdr = 0xe9;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x13]); // mem[p + 0x13] | mem[p + 0x14] << 8;
				unp->_retAdr = READ_LE_UINT16(&mem[q + 0x38]); // mem[q + 0x38] | mem[q + 0x39] << 8;
				if (unp->_info->_run == -1)
					unp->_forced = 0x80b;
				unp->_fStrAf = 0xfc;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	if (unp->_depAdr == 0) {
		if (mem[0x810] == 0xb9 && u32eq(mem + 0x813, 0xC800ed99) &&
			u32eq(mem + 0x818, 0x01004CF7)) {
			p = READ_LE_UINT16(&mem[0x811]); // mem[0x811] | mem[0x812] << 8;
			q = p - 0xed;
			if (u32eq(mem + p, 0x01C60888) &&
				u32eq(mem + q, 0xA9C8C8C8)) {
				unp->_depAdr = 0x100;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x0f]); // mem[p + 0x0f] | mem[p + 0x10] << 8;
				unp->_retAdr = READ_LE_UINT16(&mem[q + 0x38]); // mem[q + 0x38] | mem[q + 0x39] << 8;
				if (unp->_info->_run == -1)
					unp->_forced = 0x80b;
				unp->_fStrAf = 0xfc;
				unp->_idFlag = 1;
				return;
			}
		}
	}
	/* cruel 1.2 / unknown 2059 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80b, 0xE67800A0) &&
			u32eq(mem + 0x80f, 0x0803B901) &&
			u32eq(mem + 0x813, 0xC800E399) &&
			u32eq(mem + 0x817, 0x004CF7D0) &&
			u32eq(mem + 0x90b, 0xC068FEC6)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80b;
			unp->_retAdr = READ_LE_UINT16(&mem[0x91c]); // mem[0x91c] | mem[0x91d] << 8;
			unp->_endAdr = 0x2d;
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return;
		}
		/* this was found in Agile and S451 cracks, Galleon's "Cruel+Search"
		   it's actually the real v1.0
		*/
		if (u32eq(mem + 0x80b, 0xE67800A0) &&
			u32eq(mem + 0x80f, 0x0803B901) &&
			u32eq(mem + 0x813, 0xC800E399) &&
			u32eq(mem + 0x8c5, 0x011D4C04) &&
			u32eq(mem + 0x90b, 0xB1486018)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80b;
			unp->_retAdr = READ_LE_UINT16(&mem[0x92d]); // mem[0x92d] | mem[0x92e] << 8;
			unp->_endAdr = 0x2d;
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return;
		}
		if (u32eq(mem + 0x80b, 0xE67800A0) &&
			u32eq(mem + 0x80f, 0x0803B901) &&
			u32eq(mem + 0x813, 0xC800E399) &&
			u32eq(mem + 0x8b7, 0x011D4C04) &&
			u32eq(mem + 0x8fc, 0xB1486018)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80b;
			unp->_retAdr = READ_LE_UINT16(&mem[0x91e]); // mem[0x91e] | mem[0x91f] << 8;
			unp->_endAdr = 0x2d;
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return;
		}
	}

	/* TKC "proggy crueler 2.3" (and 2.5) */
	if (unp->_depAdr == 0) {
		if (mem[0x810] == 0xb9 && mem[0x819] == 0xa9 &&
			u32eq(mem + 0x813, 0xC800fa99) &&
			u32eq(mem + 0x822, 0x4CAF86AE)) {
			p = READ_LE_UINT16(&mem[0x811]); // mem[0x811] | mem[0x812] << 8;
			q = p - 0x100;

			if (u32eq(mem + p + 0x0c, 0x20F7D0C8) &&
				u32eq(mem + q, 0xA9C8C8C8)) {
				unp->_depAdr = 0x100;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x02]); // mem[p + 0x02] | mem[p + 0x03] << 8;
				unp->_retAdr = READ_LE_UINT16(&mem[q + 0x3f]); // mem[q + 0x3f] | mem[q + 0x40] << 8;
				if (unp->_info->_run == -1)
					unp->_forced = 0x80b;
				unp->_fStrAf = 0xfc;
				unp->_idFlag = 1;
				return;
			}
		}
	}
}

} // End of namespace Unp64
