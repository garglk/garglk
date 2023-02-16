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

void scnExomizer(UnpStr *unp) {
	uint8_t *mem;
	int q, p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	/* exomizer 3.x */
	if (unp->_depAdr == 0) {
		for (p = unp->_info->_end - 4; p > unp->_info->_start; p--) {
			if (u32eq(mem + p, 0x100A8069) &&
				u32eq(mem + p + 4, 0xD0FD060F) &&
				mem[p - 6] == 0x4c && mem[p - 4] == 0x01) {
				p -= 5;
				q = 2;
				if (mem[p - q] == 0x8a)
					q++;

				/* low byte of EndAdr, it's a lda $ff00,y */

				if ((mem[p - q - 1] == mem[p - q - 3]) &&
					(mem[p - q - 2] == mem[p - q])) { /* a0 xx a0 xx -> exomizer 3.0/3.01 */
					unp->_exoFnd = 0x30;
				} else { /* d0 c1 a0 xx -> exomizer 3.0.2, force +1 in start/end */
					unp->_exoFnd = 0x32;
				}
				unp->_exoFnd |= (mem[p - q] << 8);
				break;
			}
		}
		if (unp->_exoFnd) {
			unp->_depAdr = 0x100 | mem[p];
			for (; p < unp->_info->_end; p++) {
				if (u32eq(mem + p, 0x7d010020))
					break;
			}
			for (; p < unp->_info->_end; p++) {
				if (mem[p] == 0x4c) {
					unp->_retAdr = 0;
					if ((unp->_retAdr = READ_LE_UINT16(&mem[p + 1])) >= 0x200) {
						break;
					} else { /* it's a jmp $01xx, goto next */
						p++;
						p++;
					}
				}
			}
			if (unp->_info->_run == -1) {
				p = unp->_info->_start;
				q = p + 0x10;
				for (; p < q; p++) {
					if ((mem[p] == 0xba) && (mem[p + 1] == 0xbd)) {
						unp->_forced = p;
						break;
					}
				}
				for (q = p - 1; q >= unp->_info->_start; q--) {
					if (mem[q] == 0xe6)
						unp->_forced = q;
					if (mem[q] == 0xa0)
						unp->_forced = q;
					if (mem[q] == 0x78)
						unp->_forced = q;
				}
			}
		}
	}
	/* exomizer 1.x/2.x */
	if (unp->_depAdr == 0) {
		for (p = unp->_info->_end - 4; p > unp->_info->_start; p--) {
			if (((u32eq(mem + p, 0x4CF7D088) &&
				  u32eq(mem + p - 0x0d, 0xD034C0C8)) ||
				 (u32eq(mem + p, 0x4CA7A438) &&
				  u32eq(mem + p - 0x0c, 0x799FA5AE)) ||
				 (u32eq(mem + p, 0x4CECD08A) &&
				  u32eq(mem + p - 0x13, 0xCECA0EB0)) ||
				 (u32eq(mem + p, 0x4C00A0D3) &&
				  u32eq(mem + p - 0x04, 0xD034C0C8)) ||
				 (u32eq(mem + p, 0x4C00A0D2) &&
				  u32eq(mem + p - 0x04, 0xD034C0C8))) &&
				mem[p + 5] == 1) {
				p += 4;
				unp->_exoFnd = 1;
				break;
			} else if (((u32eq(mem + p, 0x8C00A0d2) &&
						 u32eq(mem + p - 0x04, 0xD034C0C8)) ||
						(u32eq(mem + p, 0x8C00A0d3) &&
						 u32eq(mem + p - 0x04, 0xD034C0C8)) ||
						(u32eq(mem + p, 0x8C00A0cf) &&
						 u32eq(mem + p - 0x04, 0xD034C0C8))) &&
					   mem[p + 6] == 0x4c && mem[p + 8] == 1) {
				p += 7;
				unp->_exoFnd = 1;
				break;
			}
		}
		if (unp->_exoFnd) {
			unp->_depAdr = 0x100 | mem[p];
			if (unp->_depAdr >= 0x134 && unp->_depAdr <= 0x14a /*0x13e*/) {
				for (p = unp->_info->_end - 4; p > unp->_info->_start;
					 p--) { /* 02 04 04 30 20 10 80 00 */
					if (u32eq(mem + p, 0x30040402))
						break;
				}
			} else {
				// exception for exo v1.x, otherwise add 8 to the counter and
				// scan backward from here
				if (unp->_depAdr != 0x143)
					p += 0x08;
				else
					p -= 0xb8;
			}
			for (; p > unp->_info->_start; p--) {
				// incredibly there can be a program starting at $4c00 :P
				if ((mem[p] == 0x4c) && (mem[p - 1] != 0x4c) && (mem[p - 2] != 0x4c)) {
					unp->_retAdr = 0;
					if ((unp->_retAdr = READ_LE_UINT16(&mem[p + 1])) >= 0x200) {
						break;
					}
				}
			}
			if (unp->_info->_run == -1) {
				p = unp->_info->_start;
				q = p + 0x10;
				for (; p < q; p++) {
					if ((mem[p] == 0xba) && (mem[p + 1] == 0xbd)) {
						unp->_forced = p;
						break;
					}
				}
				for (q = p - 1; q >= unp->_info->_start; q--) {
					if (mem[q] == 0xe6)
						unp->_forced = q;
					if (mem[q] == 0xa0)
						unp->_forced = q;
					if (mem[q] == 0x78)
						unp->_forced = q;
				}
			}
		}
	}
	if (unp->_depAdr != 0) {
		unp->_idFlag = 1;
		return;
	}
}

} // End of namespace Unp64
