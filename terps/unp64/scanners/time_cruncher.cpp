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

/* TimeCruncher 4.0 to 4.4 and the headers put in front of 4.2 */
static bool tryTimeCruncher4(UnpStr *unp, uint8_t *mem) {
	int q;
	if (unp->_depAdr == 0) { /*TimeCruncher 4.0 */
		if (u32eq(mem + 0x811, 0x00A201E6) &&
			u32eq(mem + 0x82e, 0xAD4C2E85) &&
			u32eq(mem + 0x83d, 0x0377204C)) {
			unp->_depAdr = 0x3ad;
			forceEntryIfNoSys(unp, 0x811);
			unp->_retAdr = READ_LE_UINT16(&mem[0x966]);
			unp->_endAdr = readSplit(mem, 0x829, 0x82d);
			unp->_fStrAf = 0xfc; /* to test */
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 4.2 pre-header*/
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x811 + 0x84, 0x31BD0186) &&
			u32eq(mem + 0x82c + 0x84, 0xAD4C2E85) &&
			u32eq(mem + 0x83b + 0x84, 0x0377204C) &&
			u32eq(mem + 0x842, 0x9D088FBD) &&
			u32eq(mem + 0x846, 0xD0E8080B)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x839]);
			forceEntryIfNoSys(unp, 0x80d);
			unp->_retAdr = READ_LE_UINT16(&mem[0x864]);
			unp->_endAdr = unp->_info->_end - 0x84;
			unp->_strMem = 0x801;
			if (u32eq(mem + 0x85c, 0xFDA32058) &&
				u32eq(mem + 0x860, 0x4CFD1520)) {
				mem[0x85d] = 0x2c;
				mem[0x860] = 0x2c;
			}
			mem[0x808] = 0x35;
			mem[0x809] = 0x39;
			mem[0x80a] = 0x00;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 4.2 pre-header / ILS */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x811 + 0x9d, 0x31BD0186) &&
			u32eq(mem + 0x82c + 0x9d, 0xAD4C2E85) &&
			u32eq(mem + 0x83b + 0x9d, 0x0377204C) &&
			u32eq(mem + 0x84d, 0x9D08a8BD) &&
			u32eq(mem + 0x851, 0xD0E8080B)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x844]);
			forceEntryIfNoSys(unp, 0x810);
			unp->_retAdr = READ_LE_UINT16(&mem[0x86f]);
			unp->_endAdr = unp->_info->_end - 0x9d;
			unp->_strMem = 0x801;
			if (u32eq(mem + 0x867, 0xFDA32058) &&
				u32eq(mem + 0x86b, 0x4CFD1520)) {
				mem[0x868] = 0x2c;
				mem[0x86b] = 0x2c;
			}
			if ((mem[0x961] == 0xf2) &&
				(mem[0x962] == 0x03)) {
				mem[0x961] = 0x0;
				mem[0x962] = 0x1;
			}
			mem[0x808] = 0x35;
			mem[0x809] = 0x39;
			mem[0x80a] = 0x00;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher pre-header by Vikings */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x81a, 0x018535a9) &&
			u32eq(mem + 0x81e, 0x2CBD00A2) &&
			u32eq(mem + 0x822, 0x04009D08) &&
			u32eq(mem + 0x833, 0xEE08019D) &&
			u32eq(mem + 0x846, 0xD0C90404)) {
			unp->_depAdr = 0x400;
			forceEntryIfNoSys(unp, 0x81a);
			unp->_retAdr = READ_LE_UINT16(&mem[0x861]);
			unp->_endAdr = unp->_info->_end - 0x62;
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 4.2 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x811, 0x31BD0186) &&
			(u32eq(mem + 0x82c, 0xAD4C2E85) || u32eq(mem + 0x82c, 0xAD4Caf85)) &&
			u32eqmasked(mem + 0x83b, 0xf0ffffff, 0x0077204C) &&
			u32eqmasked(mem + 0x951, 0xf0ffffff, 0x003C4C68)) {
				unp->_depAdr = READ_LE_UINT16(&mem[0x82f]);
				//if( unp->_info->_run == -1 )
				unp->_forced = 0x811;
				for (q = 0x95f; q < 0x964; q++) {
					if (mem[q] == 0x4c) {
						unp->_retAdr = READ_LE_UINT16(&mem[q + 1]);
						break;
					}
				}
				unp->_endAdr = READ_LE_UINT16(&mem[0x833]);
				unp->_strMem = 0x801; /* tested crunching a prg starting at $1000, it's decrunched at $801 anyway! */
				/* ILS hack, has border flash at $3f2 left there by the preheader */
				if ((mem[0x8c4] == 0xf2) &&
					(mem[0x8c5] == 0x03)) {
					mem[0x8c4] = 0x0;
					mem[0x8c5] = 0x1;
				}
				unp->_idFlag = 1;
				return true;
			}
	}
	/*TimeCruncher 4.3? */
	if (unp->_depAdr == 0) {
		if (u32eqmasked(mem + 0x82b, 0x00ffffff, 0x0003AA4C) &&
			u32eq(mem + 0x80a, 0x7800A200) &&
			u32eq(mem + 0x834, 0x90037420) &&
			u32eq(mem + 0x954, 0xF2D007C0)) {
			unp->_depAdr = 0x3aa;
			forceEntryIfNoSys(unp, 0x80b);
			unp->_retAdr = READ_LE_UINT16(&mem[0x95e]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x830]);
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 4.4 */
	if (unp->_depAdr == 0) {
		if (u16eqmasked(mem + 0x80b, 0xfff0, 0x00A0) &&
			u32eq(mem + 0x811, 0xFA9D082E) &&
			u32eq(mem + 0x82a, 0x03AE4C00) &&
			u32eq(mem + 0x83a, 0x04B00374)) {
			unp->_depAdr = 0x3ae;
			forceEntryIfNoSys(unp, 0x80b);
			unp->_retAdr = READ_LE_UINT16(&mem[0x95e]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x830]);
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}
	return false;
}

/* TimeCruncher 5 and its hacks */
static bool tryTimeCruncher5(UnpStr *unp, uint8_t *mem) {
	int q;
	/*TimeCruncher 5 */
	if (unp->_depAdr == 0) {
		bool found = false;
		if (u32eq(mem + 0x813, 0xBD9AFFA2) &&
			u32eq(mem + 0x817, 0xF89D0837) &&
			u32eq(mem + 0x81b, 0xF7D0CA00) &&
			u32eq(mem + 0x82b, 0x2E842D85)) {
			found = true;
			forceEntryIfNoSys(unp, 0x813);
			unp->_endAdr = readSplit(mem, 0x828, 0x82a);
			unp->_depAdr = 0x100;
			/* same as Stoat&Tim Timecrunch 2mhz v5 */
		} else if (u32eq(mem + 0x80f, 0x9AFFA278) &&
				u32eq(mem + 0x813, 0x9D0837BD) &&
				u32eq(mem + 0x817, 0xD0CA00F8) &&
				u32eq(mem + 0x82b, 0x2E842D85)) {
			found = true;
			forceEntryIfNoSys(unp, 0x812);
			unp->_depAdr = 0x100;
			unp->_endAdr = readSplit(mem, 0x828, 0x82a);
		} else if (u32eq(mem + 0x819, 0x9AFFA278) &&
				u32eq(mem + 0x81d, 0x9D0837BD) &&
				u32eq(mem + 0x821, 0xD0CA00F8) &&
				u32eq(mem + 0x82f, 0x2E842D85)) {
			found = true;
			forceEntryIfNoSys(unp, 0x819);
			unp->_depAdr = 0x100;
			unp->_endAdr = readSplit(mem, 0x82c, 0x82e);
		} else if (u32eq(mem + 0x810, 0x9AFFA278) &&
				u32eq(mem + 0x814, 0x9D0837BD) &&
				u32eq(mem + 0x818, 0xD0CA00F8) &&
				u32eq(mem + 0x82b, 0x2E842D85)) {
			found = true;
			forceEntryIfNoSys(unp, 0x810);
			unp->_depAdr = 0x100;
			unp->_endAdr = readSplit(mem, 0x828, 0x82a); /* also at $83d/e (add 1) */
			/* hack at 2064 found in Mentallic/PD */
		} else /* 5.0 gen hack */
			if (u32eq(mem + 0x841, 0x07E89D09) &&
				u32eq(mem + 0x851, 0x9D2002A2) &&
				u32eq(mem + 0x861, 0x90066901) &&
				u32eq(mem + 0x901, 0xC0FCC6FD)) {
				found = true;
				for (q = 0x801; q < 0x830; q++) {
					if (mem[q] == 0x78) {
						if (unp->_info->_run == -1) {
							unp->_forced = q;
							unp->_info->_run = q;
						}
					}
					if (unp->_endAdr == 0x10000 &&
						mem[q] == 0xa9 &&
						mem[q + 2] == 0xa0 &&
						u32eq(mem + q + 4, 0x2E842D85)) {
						unp->_endAdr = readSplit(mem, q + 1, q + 3);
					}
				}
				unp->_depAdr = 0x100;
			}
		if (found) {
			unp->_idFlag = 1;
			for (q = 0x912; q < 0x927; q++) {
				if (mem[q] == 0xa9) {
					q++;
					continue;
				}
				if ((mem[q] == 0x8d) ||
					(mem[q] == 0xce) ||
					(mem[q] == 0x2c)) {
					q += 2;
					continue;
				}
				if (mem[q] == 0x20) {
					if (u16eq(mem + q + 1, 0xa659) ||
						u16eq(mem + q + 1, 0xff81) ||
						u16eq(mem + q + 1, 0xe3bf) ||
						u16eq(mem + q + 1, 0xe544) ||
						u16eq(mem + q + 1, 0xe5a0) ||
						u16eq(mem + q + 1, 0xe518)) {
						mem[q] = 0x2c;
						q += 2;
						continue;
					} else {
						unp->_retAdr = READ_LE_UINT16(&mem[q + 1]);
						break;
					}
				}
				if (mem[q] == 0x4c) {
					unp->_retAdr = READ_LE_UINT16(&mem[q + 1]);
					break;
				}
			}
			unp->_fStrAf = 0xfe;
			return true;
		}
	}
	return false;
}

/* TimeCruncher 3.x and what was made from it */
static bool tryTimeCruncher3(UnpStr *unp, uint8_t *mem) {
	int p;
	/*TimeCruncher 3.0? */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x818, 0x8534A978) &&
			u32eq(mem + 0x81c, 0xBDD2A201) &&
			u32eq(mem + 0x820, 0xF99D0850) &&
			u32eq(mem + 0x824, 0xB079E000) &&
			u32eq(mem + 0x8c1, 0xa0033420)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x818);
			unp->_retAdr = READ_LE_UINT16(&mem[0x95c]);
			unp->_endAdr = 0x2d;
			unp->_strMem = 0x0801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 3.1/BYG */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x847, 0xC807E899) &&
			u32eq(mem + 0x84b, 0x02EEF7D0) &&
			u32eq(mem + 0x84f, 0x0105EE01) &&
			u32eq(mem + 0x816, 0x3CB9C4A0) &&
			u32eq(mem + 0x967, 0x4C01E678)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x967);
			unp->_retAdr = READ_LE_UINT16(&mem[0x934]);
			unp->_endAdr = readSplit(mem, 0x833, 0x837);
			unp->_fStrAf = 0xfc;

			if (unp->_retAdr == 0x277) { /* this version only issues RUN and has no text */
				unp->_retAdr = kBasicRun;
			}
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 3.3/2066 */
	if (unp->_depAdr == 0) {
		if (mem[0x812] == 0x78 &&
			u32eqmasked(mem + 0x818, 0xfffff0ff, 0x018530a9) &&
			u32eq(mem + 0x820, 0xF899083C) &&
			u32eq(mem + 0x82b, 0x03339908) &&
			u32eq(mem + 0x838, 0x004C2E85)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x812);
			unp->_endAdr = readSplit(mem, 0x833, 0x837);
			for (p = 0x91a; p < 0x931; p++) {
				if (u32eqmasked(mem + p, 0xff00ffff, 0x4c000185)) {
					unp->_retAdr = READ_LE_UINT16(&mem[p + 4]);
					unp->_fStrAf = 0xfc;
				}
			}
			unp->_idFlag = 1;
			return true;
		}
	}

	/* Stoat&Tim Timecrunch 2mhz v1 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x813, 0xBDBFA278) &&
			u32eq(mem + 0x817, 0xF99D0841) &&
			u32eq(mem + 0x81b, 0x0900BD00) &&
			u32eq(mem + 0x833, 0xAE852D85) &&
			u32eq(mem + 0x930, 0xA9D6D007)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x813);
			unp->_retAdr = READ_LE_UINT16(&mem[0x941]);
			unp->_endAdr = readSplit(mem, 0x832, 0x838);
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x813, 0xB9BFA078) &&
			u32eq(mem + 0x817, 0xF9990840) &&
			u32eq(mem + 0x81b, 0x08ffB900) &&
			u32eq(mem + 0x832, 0xAE852D85) &&
			u32eq(mem + 0x92f, 0xA9D6D007)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x813;
			unp->_retAdr = READ_LE_UINT16(&mem[0x940]);
			unp->_endAdr = readSplit(mem, 0x831, 0x837);
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}

	/*TimeCruncher 3.1+/2061 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80d, 0x8534A978) ||
			((mem[0x80d] == 0x20 || mem[0x080d] == 0x4c) && mem[0x810] == 0x85)) {
			if (u32eq(mem + 0x811, 0xB9C4A001) &&
				u32eq(mem + 0x815, 0xF899083C) &&
				u32eq(mem + 0x838, 0x004C2E85)) {
				unp->_depAdr = 0x100;
				forceEntryIfNoSys(unp, 0x80d);
				unp->_retAdr = READ_LE_UINT16(&mem[0x934]);
				unp->_endAdr = readSplit(mem, 0x833, 0x837);
				unp->_fStrAf = 0xfc;
				unp->_idFlag = 1;
				return true;
			}
			if (u32eq(mem + 0x811, 0xB9C1A001) &&
				u32eq(mem + 0x815, 0xF8990831) &&
				u32eq(mem + 0x82d, 0x004Caf85)) {
				unp->_depAdr = 0x100;
				forceEntryIfNoSys(unp, 0x80d);
				unp->_retAdr = READ_LE_UINT16(&mem[0x929]);
				unp->_endAdr = readSplit(mem, 0x824, 0x82a);
				unp->_fStrAf = 0xfc;
				unp->_idFlag = 1;
				return true;
			}
		}
	}
	/*TimeCruncher 3.x/2072 */
	if (unp->_depAdr == 0) {
		if ((u32eqmasked(mem + 0x818, 0xfff0ffff, 0x8530A978) ||
			u32eq(mem + 0x818, 0x8578d020)) &&
			u32eq(mem + 0x81c, 0xB9C4A001) &&
			u32eq(mem + 0x820, 0xF899083C) &&
			(u32eq(mem + 0x838, 0x004C2E85) ||
			u32eq(mem + 0x838, 0x004Caf85)) &&
			mem[0x83c] == 0x01) {
			unp->_depAdr = 0x100;
			if (mem[0x081a] == 0x78) {
				forceEntryIfNoSys(unp, 0x81a);
				/*MATCHAM SPEED-PACKER V1.1*/
			} else {
				forceEntryIfNoSys(unp, 0x818);
			}
			unp->_endAdr = readSplit(mem, 0x833, 0x837);
			// Henk/HTL $91a, Matcham/NET $930
			for (p = 0x91a; p < 0x931; p++) {
				if (u32eqmasked(mem + p, 0xff00ffff, 0x4c000185)) {
					unp->_retAdr = READ_LE_UINT16(&mem[p + 4]);
					unp->_fStrAf = 0xfc;
					unp->_strAdC = 0xffff;
				}
			}
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher /Radical? */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80b, 0x3CB900A0) &&
			u32eq(mem + 0x80f, 0x00F89908) &&
			u32eq(mem + 0x820, 0xF899083C) &&
			u32eq(mem + 0x839, 0x04004C2E)) {
			unp->_depAdr = 0x400;
			unp->_forced = 0x80b;
			unp->_endAdr = readSplit(mem, 0x833, 0x837);
			unp->_retAdr = READ_LE_UINT16(&mem[0x91e]);
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher /2074 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x81a, 0xC4A00185) &&
			u32eq(mem + 0x81e, 0x99083DB9) &&
			u32eq(mem + 0x822, 0xFDB900F9) &&
			u32eq(mem + 0x836, 0xA2AF852E)) {
			unp->_depAdr = 0x101;
			forceEntryIfNoSys(unp, 0x81a);
			unp->_endAdr = readSplit(mem, 0x82e, 0x834);
			for (p = 0x935; p < 0x93a; p++) {
				if (mem[p] == 0x4c) {
					unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
					unp->_fStrAf = 0xfc;
				}
			}
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher /Fast pack'em */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x84f, 0x4909C8BD) &&
			u32eq(mem + 0x857, 0xA0F510CA) &&
			u32eq(mem + 0x85b, 0x087DB9C1) &&
			u32eq(mem + 0x885, 0x9909EEB9)) {
			unp->_depAdr = 0x100;
			if (unp->_info->_run == -1) {
				p = 0x830;
				if (mem[p] == 0x78)
					unp->_forced = p;
				else {
					p = 0x834;
					if (mem[p] == 0x78)
						unp->_forced = p;
				}
			}
			unp->_retAdr = READ_LE_UINT16(&mem[0x975]);
			unp->_endAdr = readSplit(mem, 0x86c, 0x872);
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher /FBI */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x888, 0xC807E899) &&
			u32eq(mem + 0x88c, 0x02EEF7D0) &&
			u32eq(mem + 0x890, 0x0105EE01) &&
			u32eq(mem + 0x80d, 0xFFA2D878) &&
			u32eq(mem + 0x971, 0x4C580185)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80d;
			unp->_retAdr = READ_LE_UINT16(&mem[0x975]);
			unp->_endAdr = readSplit(mem, 0x86c, 0x872);
			unp->_fStrAf = 0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher /Triumwyrat aka Final Cut Compressor */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x822, 0x04289D09) &&
			u32eq(mem + 0x85b, 0x087DB9C1) &&
			u32eq(mem + 0x87a, 0x01004c01)) {
			forceEntryIfNoSys(unp, 0x818);
			unp->_depAdr = 0x100;
			unp->_endAdr = readSplit(mem, 0x86c, 0x872);
			unp->_fStrAf = 0xfc;
			unp->_retAdr = READ_LE_UINT16(&mem[0x975]);
			if (unp->_retAdr == 0x3cd) {
				unp->_retAdr = READ_LE_UINT16(&mem[0x9e3]);
				if (unp->_retAdr == 0xa659) {
					mem[0x9e2] = 0x2c;
					unp->_retAdr = READ_LE_UINT16(&mem[0x9e6]);
				}
			}
			unp->_idFlag = 1;
			return true;
		}
	}
	return false;
}

/* Hacks of unknown version, the SIR crunch family, and clones that other
 * groups put their name on */
static bool tryTimeCruncherClones(UnpStr *unp, uint8_t *mem) {
	int q, p;
	/*TimeCruncher generic hacks*/
	if (unp->_depAdr == 0) {
		if (u32eqmasked(mem + 0x839, 0xffffff00, 0x01004C00) &&
			u32eqmasked(mem + 0x847, 0xfffff0ff, 0xC807E099) &&
			u32eq(mem + 0x84b, 0x02EEF7D0) &&
			u32eq(mem + 0x84f, 0x0105EE01) &&
			u32eq(mem + 0x91f, 0xC6FFC602)) {
			if (unp->_info->_run == -1) {
				for (p = 0x80b; p < 0x820; p++) {
					if (mem[p] == 0x78) {
						unp->_forced = p;
						break;
					}
				}
			}
			unp->_depAdr = 0x100;
			unp->_endAdr = readSplit(mem, 0x833, 0x837);
			unp->_retAdr = READ_LE_UINT16(&mem[0x934]);
			unp->_fStrAf = 0xfc;
			/*HTL hack (lax $dd07)*/
			if (u32eq(mem + 0x822, 0xE0DD07AF)) {
				mem[0x0822] = 0xa9;
				mem[0x0823] = 0xff;
				mem[0x0824] = 0xaa;
			}
			unp->_idFlag = 1;
			return true;
		}
	}
	/* File Press Expert/SIR crunch 1 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x824, 0x018534A9) &&
			u32eq(mem + 0x82a, 0x99083CB9) &&
			u32eq(mem + 0x82e, 0xB9D800F8) &&
			u32eq(mem + 0x892, 0xD0033420) &&
			u32eq(mem + 0x940, 0x01E6EC10)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x81e);
			unp->_endAdr = READ_LE_UINT16(&mem[0x840]);
			unp->_strMem = 0x801;
			p = 0x96f;
			if (mem[p] == 0x4c) {
				unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
			} else if (mem[p + 1] == 0x4c) {
				unp->_retAdr = READ_LE_UINT16(&mem[p + 2]);
			}

			unp->_idFlag = 1;
			return true;
		}
	}
	/* SIR crunch 4 */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x81b, 0x018534A9) &&
			u32eq(mem + 0x81f, 0x3BB9C4A0) &&
			u32eq(mem + 0x82b, 0x88033399) &&
			u32eq(mem + 0x95F, 0xA2F0FEC6)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x81b);
			unp->_retAdr = READ_LE_UINT16(&mem[0x970]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x83f]);
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* SIR crunch 3 */
	if (unp->_depAdr == 0) {
		if (mem[0x81e] == 0x78 &&
			u32eq(mem + 0x824, 0x018534A9) &&
			u32eq(mem + 0x828, 0x48B9C4A0) &&
			u32eq(mem + 0x834, 0x88033399) &&
			u32eq(mem + 0x96c, 0xA2F0FEC6)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x824;
			p = READ_LE_UINT16(&mem[0x980]);
			if ((mem[0x97c] == 0x20) || (mem[0x97c] == 0x4c)) {
				q = READ_LE_UINT16(&mem[0x97d]);
				if (q != 0xa659)
					p = q;
			}
			unp->_retAdr = p;
			unp->_endAdr = READ_LE_UINT16(&mem[0x84c]);
			//unp->_fStrAf=0xfc;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* SIR crunch 2.x */
	if (unp->_depAdr == 0) {
		if (mem[0x81e] == 0x78 &&
			u32eq(mem + 0x824, 0x018534A9) &&
			u32eq(mem + 0x828, 0x4aB9C4A0) &&
			u32eq(mem + 0x834, 0x88033399) &&
			u32eq(mem + 0x96e, 0xA2F0FEC6)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x824;
			p = READ_LE_UINT16(&mem[0x982]);
			if ((mem[0x97e] == 0x20) || (mem[0x97e] == 0x4c)) {
				q = READ_LE_UINT16(&mem[0x97f]);
				if (q != 0xa659)
					p = q;
			}
			unp->_retAdr = p;
			unp->_endAdr = READ_LE_UINT16(&mem[0x84e]);
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* Entropy hack? similar to SIR crunch 3, at $80d */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80e, 0x23B9C4A0) &&
			u32eq(mem + 0x816, 0x9908E4B9) &&
			u32eq(mem + 0x81a, 0xD0880333) &&
			u32eq(mem + 0x947, 0xA2F0FEC6)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80e;
			unp->_retAdr = READ_LE_UINT16(&mem[0x94e]);
			unp->_strMem = 0x801;
			unp->_endAdr = READ_LE_UINT16(&mem[0x827]);
			unp->_idFlag = 1;
			return true;
		}
	}
	/* SupraCompactor 2.0/TKC - TC 5 clone + unpacktext */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x817, 0xBD9ACA78) &&
			u32eq(mem + 0x81b, 0xF89D0842) &&
			u32eq(mem + 0x84b, 0xE89D0935) &&
			u32eq(mem + 0x90f, 0xDCD0E7C0)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x817;
			if (u32eq(mem + 0x91f, 0xA6592008)) {
				mem[0x920] = 0x2c;
				unp->_retAdr = READ_LE_UINT16(&mem[0x925]);
			} else {
				unp->_retAdr = READ_LE_UINT16(&mem[0x921]);
			}
			unp->_endAdr = READ_LE_UINT16(&mem[0x848]);
			unp->_endAdr++;
			unp->_fStrAf = 0xfe;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* Relax Timer, TimeCruncher 3.0 hack? very similar to mastercompr/relax */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x816, 0x8534A978) &&
			u32eq(mem + 0x81a, 0xBDD2A201) &&
			u32eq(mem + 0x81e, 0xF99D084e) &&
			u32eq(mem + 0x822, 0xB073E000) &&
			u32eq(mem + 0x8bf, 0xa0033420)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x816;
			unp->_retAdr = READ_LE_UINT16(&mem[0x94d]);
			unp->_endAdr = 0x2d;
			unp->_strMem = 0x0801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* TimeCruncher 3/Triad, same as Relax but at $812 */
	if (unp->_depAdr == 0) {
		if (mem[0x812] == 0x78 && mem[0x813] == 0xe6 &&
			u32eq(mem + 0x814, 0xBDD2A201) &&
			u32eq(mem + 0x818, 0xF99D0848) &&
			u32eq(mem + 0x81c, 0xB073E000) &&
			u32eq(mem + 0x8b9, 0xa0033420)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x812);
			unp->_retAdr = READ_LE_UINT16(&mem[0x947]);
			unp->_endAdr = 0x2d;
			unp->_strMem = 0x0801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* TimeCruncher 3/PC */
	if (unp->_depAdr == 0) {
		if (mem[0x810] == 0x78 && mem[0x813] == 0x85 &&
			u32eq(mem + 0x814, 0xBD00A201) &&
			u32eq(mem + 0x818, 0x349D0920) &&
			u32eq(mem + 0x81c, 0x73E0E803) &&
			u32eq(mem + 0x8bf, 0xa0033420)) {
			unp->_depAdr = 0x100;
			forceEntryIfNoSys(unp, 0x810);
			unp->_retAdr = READ_LE_UINT16(&mem[0x94c]);
			unp->_endAdr = 0x2d;
			unp->_strMem = 0x0801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* TimeCruncher 4.2/Triad */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x81a, 0x00A201E6) &&
			u32eq(mem + 0x81e, 0x9D083CBD) &&
			u32eq(mem + 0x837, 0xAD4C2E85) &&
			u32eq(mem + 0x846, 0x0377204C) &&
			u32eq(mem + 0x95c, 0x033C4C68)) {
			unp->_depAdr = 0x3ad;
			forceEntryIfNoSys(unp, 0x81a);
			unp->_retAdr = READ_LE_UINT16(&mem[0x96f]);
			unp->_endAdr = readSplit(mem, 0x832, 0x836);
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 4.2 generic hack */
	if (unp->_depAdr == 0) {
		if (u16eq(mem + 0x82e, 0xAD4C) &&
			u32eq(mem + 0x875, 0xF785FEA5) &&
			u32eq(mem + 0x8c3, 0x2001004C) &&
			u32eq(mem + 0x9B4, 0x01004C02)) {
			unp->_depAdr = READ_LE_UINT16(&mem[0x82f]);
			if (unp->_info->_run == -1) {
				for (q = 0x810; q < 0x820; q++) {
					if (u32eq(mem + q, 0x8600A278)) {
						unp->_forced = q;
						break;
					}
					if (u32eq(mem + q, 0x31BD0186)) {
						unp->_forced = q;
						break;
					}
				}
			}
			for (q = 0x95f; q < 0x964; q++) {
				if (mem[q] == 0x4c) {
					unp->_retAdr = READ_LE_UINT16(&mem[q + 1]);
					break;
				}
			}
			unp->_endAdr = READ_LE_UINT16(&mem[0x833]);
			unp->_strMem = 0x801;
			unp->_idFlag = 1;
			return true;
		}
	}
	/*TimeCruncher 6/different hacks
	almost identical to TimeCruncher 6 henk/htl but different preamble
	*/
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x844, 0x990958B9) &&
			u32eq(mem + 0x848, 0xD0C807E8) &&
			u32eq(mem + 0x84c, 0x0102EEF7) &&
			u32eq(mem + 0x8c1, 0x033420E8)) {
			unp->_depAdr = 0x100;
			unp->_fStrAf = 0xfc;
			unp->_strAdC = 0xffff;
			unp->_retAdr = READ_LE_UINT16(&mem[0x91e]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x840]);
			if (mem[0x813] == 0x78 && mem[0x91d] == 0xee &&
				u32eq(mem + 0x819, 0xF899083C)) {
				unp->_forced = 0x813;
				unp->_retAdr = READ_LE_UINT16(&mem[0x953]);
				if (unp->_retAdr == 0xa659)
					unp->_retAdr = READ_LE_UINT16(&mem[0x956]);
				unp->_idFlag = 1;
				return true;
			} else if (mem[0x810] == 0x78 &&
					u32eq(mem + 0x820, 0xF899083C)) {
				unp->_forced = 0x810;
				unp->_idFlag = 1;
				return true;
			} else if (mem[0x812] == 0x78 &&
					u32eq(mem + 0x824, 0xF899083C)) {
				unp->_forced = 0x812;
				unp->_idFlag = 1;
				return true;
			} else if (mem[0x80e] == 0xe6 &&
					u32eq(mem + 0x820, 0xF899083C)) {
				unp->_forced = 0x80e;
				unp->_idFlag = 1;
				return true;
			} else if (mem[0x819] == 0xe6 &&
					u32eq(mem + 0x824, 0xF899083C)) {
				unp->_forced = 0x819;
				unp->_idFlag = 1;
				return true;
			} else if (mem[0x80d] == 0x78 &&
					u32eq(mem + 0x811, 0xF899083C)) {
				unp->_forced = 0x80d;
				unp->_idFlag = 1;
				return true;
			} else {
				if (unp->_retAdr < unp->_depAdr) { // dirty workaround, found ONE single case like this
					// retaddr=$00f0 and it contains just JSR $A660+JMP $A7AE
					unp->_retAdr = kBasicRun;
				}
				unp->_idFlag = 1;
				return true;
			}
		}
	}
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x844, 0x990976B9) &&
			u32eq(mem + 0x848, 0xD0C807E8) &&
			u32eq(mem + 0x831, 0x01004CED) &&
			u32eq(mem + 0x8c1, 0x08F00334)) {
			unp->_depAdr = 0x100;
			if (unp->_info->_run == -1) {
				if (mem[0x818] == 0x78)
					unp->_forced = 0x818;
			}
			unp->_retAdr = READ_LE_UINT16(&mem[0x974]);
			unp->_endAdr = READ_LE_UINT16(&mem[0x840]);
			unp->_idFlag = 1;
			return true;
		}
	}
	/* time 3.3 2mhz/galleon */
	if (unp->_depAdr == 0) {
		if (mem[0x80b] == 0xa0 && mem[0x80c] == 0x00 &&
			u32eq(mem + 0x810, 0x990829B9) &&
			u32eq(mem + 0x817, 0x339908EA) &&
			u32eq(mem + 0x825, 0x004C2E85) &&
			u32eq(mem + 0x8ae, 0x08F00334)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80b;
			unp->_retAdr = READ_LE_UINT16(&mem[0x921]);
			if (unp->_retAdr < 0x800)
				unp->_rtAFrc = 1;
			unp->_endAdr = readSplit(mem, 0x820, 0x824);
			unp->_idFlag = 1;
			return true;
		}
	}
	/* science451, variable entry and depacker */
	if (unp->_depAdr == 0) {
		if (mem[0x81e] == 0x78 && mem[0x820] == 0x01 &&
			u32eq(mem + 0x821, 0x9D084EBD) &&
			u32eq(mem + 0x82d, 0xCAF1D0CA) &&
			u32eq(mem + 0x831, 0xC600A09A) &&
			u32eq(mem + 0x835, 0x083DCEFD)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x81e;
			unp->_retAdr = READ_LE_UINT16(&mem[0x94d]);
			if (unp->_retAdr < 0x800)
				unp->_rtAFrc = 1;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return true;
		}
	}
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80b, 0xE67800A0) &&
			u32eq(mem + 0x813, 0x9D083CBD) &&
			u32eq(mem + 0x817, 0xFBBD00F9) &&
			u32eq(mem + 0x81b, 0x04329D08) &&
			u32eq(mem + 0x8d1, 0xD0066918)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x80b;
			unp->_retAdr = READ_LE_UINT16(&mem[0x929]);
			if (unp->_retAdr < 0x800)
				unp->_rtAFrc = 1;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return true;
		}
	}
	/* Ikari, hack of s451 variant */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x817, 0xE678FFA2) &&
			u32eq(mem + 0x81c, 0x9D084EBD) &&
			u32eq(mem + 0x82d, 0xCAecD0CA) &&
			u32eq(mem + 0x831, 0xC600A09A) &&
			u32eq(mem + 0x835, 0x083DCEFD)) {
			unp->_depAdr = 0x100;
			unp->_forced = 0x817;
			unp->_retAdr = READ_LE_UINT16(&mem[0x94d]);
			if (unp->_retAdr < 0x800)
				unp->_rtAFrc = 1;
			unp->_endAdr = 0x2d;
			unp->_idFlag = 1;
			return true;
		}
	}
	return false;
}

/* TimeCruncher and clones */
void scnTimeCruncher(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	if (tryTimeCruncher4(unp, mem) || tryTimeCruncher5(unp, mem) || tryTimeCruncher3(unp, mem))
		return;
	tryTimeCruncherClones(unp, mem);
}

} // End of namespace Unp64
