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

void scnXTC(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q = 0, p;
	if (unp->_depAdr == 0) {
		if (u16eq(mem + 0x80d, 0xE678) &&
			u32eq(mem + 0x811, 0x1BCE0818) &&
			u32eq(mem + 0x819, 0xC8000099) &&
			u32eq(mem + 0x82c, 0x4CF7D0CA) &&
			mem[0x85c] == 0x99) {
			unp->_retAdr = READ_LE_UINT16(&mem[0x872]);
			unp->_depAdr = 0x100;
			unp->_forced = 0x80d; /* the ldy #$00 can be missing, skipped */
			unp->_fEndAf = 0x121;
			unp->_endAdC = 0xffff | EA_USE_Y;
			unp->_strMem = READ_LE_UINT16(&mem[0x85d]);
			unp->_idFlag = 1;
			return;
		}
	}
	/* XTC packer 1.0 & 2.2/2.4 */
	if (unp->_depAdr == 0) {
		for (p = 0x801; p < 0x80c; p += 0x0a) {
			if (u16eq(mem + p + 0x02, 0xE678) &&
				u32eq(mem + p + 0x07, (unsigned int)(0xce08 | ((p + 0x10) << 16))) &&
				u32eq(mem + p + 0x0e, 0xC8000099) &&
				u32eq(mem + p + 0x23, 0x4CF7D0CA)) {
				/* has variable codebytes so addresses varies */
				for (q = p + 0x37; q < p + 0x60; q += 4) {
					if (mem[q] == 0xc9)
						continue;
					if (mem[q] == 0x99) {
						unp->_depAdr = 0x100;
						break;
					}
					break; /* unexpected byte, get out */
				}
				break;
			}
		}
		if (unp->_depAdr) {
			unp->_retAdr = READ_LE_UINT16(&mem[q + 0x16]);
			if (!u16eq(mem + p, 0x00a0))
				unp->_forced = p + 2; /* the ldy #$00 can be missing, skipped */
			else
				unp->_forced = p;

			unp->_fEndAf = READ_LE_UINT16(&mem[q + 0x7]);
			unp->_fEndAf--;
			unp->_endAdC = 0xffff | EA_USE_Y;
			unp->_strMem = READ_LE_UINT16(&mem[q + 1]);
			if (u32eq(mem + q + 0x1f, 0xDDD00285)) {
			} else if (u32eq(mem + q + 0x1f, 0xF620DFD0)) {
				/* rockstar's 2.2+ & shade/light's 2.4 are all the same */
			} else {                                           /* actually found to be Visiomizer 6.2/Zagon */
				unp->_depAdr = READ_LE_UINT16(&mem[p + 0x27]);
			}
			unp->_idFlag = 1;
			return;
		}
	}
	/* XTC 2.3 / 6codezipper */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x803, 0xB9018478) &&
			u32eq(mem + 0x80b, 0xF7D0C8FF) &&
			u32eq(mem + 0x81b, 0x00FC9D08) &&
			u32eq(mem + 0x85b, 0xD0D0FFE4)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x823]);
			unp->_forced = 0x803;
			unp->_retAdr = READ_LE_UINT16(&mem[0x865]);
			unp->_strMem = READ_LE_UINT16(&mem[0x850]);
			unp->_endAdC = 0xffff | EA_USE_Y;
			unp->_fEndAf = 0x128;
			unp->_idFlag = 1;
			return;
		}
	}
	/* XTC 2.3 / G*P, probably by Rockstar */
	if (unp->_depAdr == 0) {
		if ((u32eq(mem + 0x803, 0xB901e678) ||
			 u32eq(mem + 0x803, 0xB9018478)) &&
			u32eq(mem + 0x80b, 0xF7D0C8FF) &&
			u32eq(mem + 0x81b, 0x00F59D08) &&
			u32eq(mem + 0x85b, 0xD0D0F8E4)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x823]);
			unp->_forced = 0x803;
			unp->_retAdr = READ_LE_UINT16(&mem[0x865]);
			unp->_strMem = READ_LE_UINT16(&mem[0x850]);
			unp->_endAdC = 0xffff | EA_USE_Y;
			unp->_fEndAf = 0x121;
			unp->_idFlag = 1;
			return;
		}
	}
	/* XTC packer 2.x? found in G*P/NEI/Armageddon warez
	just some different byte on copy loop, else is equal to 2.3
	*/
	if (unp->_depAdr == 0) {
		for (p = 0x801; p < 0x80c; p += 0x0a) {
			if (u32eqmasked(mem + p + 0x00, 0xffff0000, 0xE6780000) &&
				u32eqmasked(mem + p + 0x05, 0xffff00ff, 0xB90800CE) &&
				u32eq(mem + p + 0x0b, 0xC8000099) &&
				u32eq(mem + p + 0x1e, 0x4CF7D0CA)) {
				/* has variable codebytes so addresses varies */
				for (q = p + 0x36; q < p + 0x60; q += 4) {
					if (mem[q] == 0xc9)
						continue;
					if (mem[q] == 0x99) {
						unp->_depAdr = 0x100;
						break;
					}
					break; /* unexpected byte, get out */
				}
				break;
			}
		}
		if (unp->_depAdr) {
			unp->_retAdr = READ_LE_UINT16(&mem[q + 0x16]);
			unp->_forced = p + 2;
			unp->_fEndAf = READ_LE_UINT16(&mem[q + 0x7]);
			unp->_fEndAf--;
			unp->_endAdC = 0xffff | EA_USE_Y;
			unp->_strMem = READ_LE_UINT16(&mem[q + 1]);
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
