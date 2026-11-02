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

void scnPuCrunch(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p;
	if (unp->_depAdr == 0) {
		if (mem[0x80d] == 0x78 &&
			u32eq(mem + 0x813, 0x34A20185) &&
			u32eq(mem + 0x817, 0x9D0842BD) &&
			u32eq(mem + 0x81b, 0xD0CA01FF) &&
			u32eq(mem + 0x83d, 0x4CEDD088)) {
			for (p = 0x912; p < 0x938; p++) {
				if (u32eq(mem + p, 0x2D85FAA5) &&
					u32eq(mem + p + 4, 0x2E85FBA5)) {
					unp->_endAdr = 0xfa;
					unp->_strMem = READ_LE_UINT16(&mem[0x879]);
					unp->_depAdr = READ_LE_UINT16(&mem[0x841]);
					unp->_retAdr = READ_LE_UINT16(&mem[p + 0xa]);
					unp->_forced = 0x80d;
					break;
				}
			}
		} else if (mem[0x80d] == 0x78 &&
				   u32eq(mem + 0x81a, 0x10CA4B95) &&
				   u32eq(mem + 0x81e, 0xBD3BA2F8) &&
				   u32eq(mem + 0x847, 0x4CEDD088)) {
			for (p = 0x912; p < 0x938; p++) {
				if (u32eq(mem + p, 0x2D85FAA5) &&
					u32eq(mem + p + 4, 0x2E85FBA5)) {
					unp->_endAdr = 0xfa;
					unp->_strMem = READ_LE_UINT16(&mem[p + 0x88a]);
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x84b]);
					unp->_retAdr = READ_LE_UINT16(&mem[p + 0xa]);
					unp->_forced = 0x80d;
					break;
				}
			}
		} else if (mem[0x80d] == 0x78 &&
				   u32eq(mem + 0x811, 0x85AAA901) &&
				   u32eq(mem + 0x81d, 0xF69D083C) &&
				   u32eq(mem + 0x861, 0xC501C320) &&
				   u32eq(mem + 0x839, 0x01164CED)) {
			unp->_endAdr = 0xfa;
			unp->_strMem = READ_LE_UINT16(&mem[0x840]);
			unp->_depAdr = 0x116;
			unp->_retAdr = READ_LE_UINT16(&mem[0x8df]);
			unp->_forced = 0x80d;
		} else if (mem[0x80d] == 0x78 &&
				   u32eq(mem + 0x811, 0x85AAA901) &&
				   u32eq(mem + 0x81d, 0xF69D083C) &&
				   u32eq(mem + 0x861, 0xC501C820) &&
				   u32eq(mem + 0x839, 0x01164CED)) {
			unp->_endAdr = 0xfa;
			unp->_strMem = READ_LE_UINT16(&mem[0x840]);
			unp->_depAdr = 0x116;
			if (mem[0x8de] == 0xa9) {
				unp->_retAdr = READ_LE_UINT16(&mem[0x8e1]);
				if ((unp->_retAdr == 0xa871) && (mem[0x8e0] == 0x20) &&
					(mem[0x8e3] == 0x4c)) {
					mem[0x8e0] = 0x2c;
					unp->_retAdr = READ_LE_UINT16(&mem[0x8e4]);
				}
			} else {
				unp->_retAdr = READ_LE_UINT16(&mem[0x8df]);
			}
			unp->_forced = 0x80d;
		} else {
			/* unknown old/hacked pucrunch ? */
			for (p = 0x80d; p < 0x820; p++) {
				if (mem[p] == 0x78) {
					q = p;
					for (; p < 0x824; p++) {
						if (u32eqmasked(mem + p, 0xf0ffffff, 0xF0BD53A2) &&
							u32eq(mem + p + 4, 0x01FF9D08) &&
							u32eq(mem + p + 8, 0xA2F7D0CA)) {
							unp->_forced = q;
							q = mem[p + 3] & 0xf;              /* can be $f0 or $f2, q&0x0f as offset */
							p = READ_LE_UINT16(&mem[p + 0xe]);
							if (mem[p - 2] == 0x4c && mem[p + 0xa0 + q] == 0x85) {
								unp->_depAdr = READ_LE_UINT16(&mem[p - 1]);
								unp->_strMem = READ_LE_UINT16(&mem[p + 4]);
								unp->_endAdr = 0xfa;
								p += 0xa2;
								q = p + 8;
								for (; p < q; p++) {
									if (u32eq(mem + p, 0x2D85FAA5) &&
										mem[p + 9] == 0x4c) {
										unp->_retAdr = READ_LE_UINT16(&mem[p + 0xa]);
										break;
									}
								}
							}
						}
					}
				}
			}
		}

		/* various old/hacked pucrunch */
		/* common pattern, variable pos from 0x79 to 0xd1
		90 ?? C8 20 ?? 0? 85 ?? C9 ?0 90 0B A2 0? 20 ?? 0? 85 ?? 20 ?? 0? A8 20 ??
		0? AA BD ?? 0? E0 20 90 0? 8A (A2 03) not always 20 ?? 02 A6
		?? E8 20 F9
		*/
		if (unp->_depAdr == 0) {
			bool found = false;
			for (q = 0x70; q < 0xff; q++) {
				if (u32eqmasked(mem + 0x801 + q, 0xFFFF00FF, 0x20C80090) &&
					u32eqmasked(mem + 0x801 + q + 8, 0xFFFF0FFF,
					 0x0B9000C9) &&
					u32eqmasked(mem + 0x801 + q + 12, 0x00FFF0FF,
					 0x002000A2) &&
					u32eqmasked(mem + 0x801 + q + 30, 0xF0FFFFFf,
					 0x009020E0)) {
					found = true;
					break;
				}
			}
			if (found) {
				for (p = 0x801 + q + 34; p < 0x9ff; p++) {
					if (u32eq(mem + p, 0x00F920E8)) {
						for (; p < 0x9ff; p++) {
							if (mem[p] == 0x4c) {
								unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
								if (unp->_retAdr > 0x257)
									break;
							}
						}
						break;
					}
				}
				for (p = 0; p < 0x40; p++) {
					if (unp->_info->_run == -1)
						if (unp->_forced == 0) {
							if (mem[0x801 + p] == 0x78) {
								unp->_forced = 0x801 + p;
								unp->_info->_run = unp->_forced;
							}
						}
					if (u32eq(mem + 0x801 + p, 0xCA00F69D) &&
						mem[0x801 + p + 0x1b] == 0x4c) {
						q = 0x801 + p + 0x1c;
						unp->_depAdr = READ_LE_UINT16(&mem[q]);
						q = 0x801 + p - 2;
						p = READ_LE_UINT16(&mem[q]);
						if ((mem[p + 3] == 0x8d) && (mem[p + 6] == 0xe6)) {
							unp->_strMem = READ_LE_UINT16(&mem[p + 4]);
						}
						break;
					}
				}
				unp->_endAdr = 0xfa; // some hacks DON'T xfer fa/b to 2d/e
				unp->_idFlag = 1;
			}
		}
		if (unp->_depAdr) {
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
