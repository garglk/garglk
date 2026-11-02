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

/*
 MrZ/Triad packer, does cover also some triad intros.
 found also as "SYS2069 PSHYCO!" in some Censor warez.
 */
void scnMrZ(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p = 0;
	if (unp->_depAdr == 0) {
		for (q = 0x810; q < 0x880; q++) {
			if (u32eq(mem + q, 0x8D00A978) &&
				u32eq(mem + q + 4, 0x0BA9D020) &&
				u32eq(mem + q + 8, 0xE6D0118D) &&
				u16eq(mem + q + 0xc, (unsigned short int)0x4c01)) {
				p = READ_LE_UINT16(&mem[q + 0xe]);
				unp->_forced = q;
				break;
			}
			if (u32eq(mem + q, 0x208D00A9) &&
				u32eq(mem + q + 4, 0x8D0BA9D0) &&
				u32eq(mem + q + 8, 0xE678D011) &&
				u16eq(mem + q + 0xc, (unsigned short int)0x4c01)) {
				p = READ_LE_UINT16(&mem[q + 0xe]);
				unp->_forced = q;
				break;
			}
			if (u32eq(mem + q, 0xD9409901) &&
				u32eq(mem + q + 4, 0xD0FCC6C8) &&
				u32eq(mem + q + 8, 0xDFD0E8ED) &&
				u32eq(mem + q + 0xc, 0x4C01E678)) {
				p = READ_LE_UINT16(&mem[q + 0x10]);
				break;
			}
			/* mrz crunch, identical apart preamble? %) */
			if (u32eq(mem + q + 0x00, 0x6FD0116F) &&
				u32eq(mem + q + 0x0a, 0xFACA0820) &&
				u32eq(mem + q + 0x1b, 0x3737261F) &&
				u32eq(mem + q + 0x42, 0x4C01E678)) {
				p = READ_LE_UINT16(&mem[q + 0x46]);
				break;
			}
		}
		if (p) {
			if (u32eq(mem + p, 0xA29AFAA2) &&
				u32eq(mem + p + 0x08, 0x10CA3995) &&
				u32eq(mem + p + 0x2b, 0xB901004C)) {
				unp->_depAdr = 0x100;
				unp->_endAdr = READ_LE_UINT16(&mem[p + 0x36]);
				if (unp->_endAdr == 0)
					unp->_endAdr = 0x10000;
				unp->_fStrAf = 0x41; /*str_add=EA_USE_Y; Y is stored at $6d*/
				for (q = p + 0x8c; q < unp->_info->_end; q++) {
					if ((mem[q] == 0xa9) || (mem[q] == 0xc6)) {
						q++;
						continue;
					}
					if (mem[q] == 0x8d) {
						q += 2;
						continue;
					}
					if (mem[q] == 0x20 && (u16eq(mem + q + 1, 0xe3bf) ||
											 u16eq(mem + q + 1, 0xfda3) ||
											 u16eq(mem + q + 1, 0xa660) ||
											 u16eq(mem + q + 1, 0xa68e) ||
											 u16eq(mem + q + 1, 0xa659))) {
						mem[q] = 0x2c;
						q += 2;
						continue;
					}
					if (mem[q] == 0x4c) {
						unp->_retAdr = READ_LE_UINT16(&mem[q + 1]);
						p = 2;
						mem[p++] = 0xA5; /* fix for start address */
						mem[p++] = 0x41; /*     *=$02             */
						mem[p++] = 0x18; /*     lda $41           */
						mem[p++] = 0x65; /*     clc               */
						mem[p++] = 0x6D; /*     adc $6d           */
						mem[p++] = 0x85; /*     sta $41           */
						mem[p++] = 0x41; /*     bcc noh           */
						mem[p++] = 0x90; /*     inc $42           */
						mem[p++] = 0x02; /* noh rts               */
						mem[p++] = 0xE6; /*                       */
						mem[p++] = 0x42; /*                       */
						mem[p++] = 0x4c; /*     jmp realstart     */
						mem[p++] = mem[q + 1];
						mem[p++] = mem[q + 2];

						mem[q + 0] = 0x4c;
						mem[q + 1] = 0x02;
						mem[q + 2] = 0x00;

						break;
					}
				}
				unp->_idFlag = 1;
				return;
			}
		}
	}
}

} // End of namespace Unp64
