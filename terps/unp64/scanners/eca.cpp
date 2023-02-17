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
#include "exo_util.h"

namespace Unp64 {

void scnECA(UnpStr *unp) {
	uint8_t *mem;
	int q, p;
	if (unp->_idFlag)
		return;
	mem = unp->_mem;
	if (unp->_depAdr == 0) {
		// for(p=0x810;p<0x830;p+=0x4)
		for (p = 0x80d; p < 0x830; p += 0x1) {
			if (u32eq(mem + p + 0x08, (unsigned int)(0x2D9D0032 + p)) &&
				u32eq(mem + p + 0x3a, 0x2a2a2a2a) &&
				u32eq(mem + p + 0x0c, 0xF710CA00)) {
				if (((*(unsigned int *)(mem + p + 0x00) & 0xf4fff000) == 0x8434A000) &&
					u32eq(mem + p + 0x04, 0xBD05A201)) {
					unp->_forced = p + 1;
				} else if (((*(unsigned int *)(mem + p + 0x00) & 0xffffff00) == 0x04A27800) &&
						   u32eq(mem + p + 0x04, 0xBDE80186)) {
					unp->_forced = p + 1;
				} else if (((*(unsigned int *)(mem + p - 0x03) & 0xffffff00) == 0x04A27800) &&
                        u32eq(mem + p + 0x04, 0xBDE80186)) {
					unp->_forced = p - 2;
				} else if (u32eq(mem + p - 0x03, 0x8D00a978)) {
					unp->_forced = p - 2;
				}
			}
			if (!unp->_forced) {
				if (u32eq(mem + p + 0x3a, 0x2a2a2a2a) &&
					u32eq(mem + p + 0x02, 0x8534A978) &&
					mem[p - 3] == 0xa0) {
					unp->_forced = p - 3;
					if (mem[p + 0x0d6] == 0x20 && mem[p + 0x0d7] == 0xe0 &&
						mem[p + 0x0d8] == 0x03 && mem[p + 0x1da] == 0x5b &&
						mem[p + 0x1e7] == 0x59) {
						/* antiprotection :D */
						mem[p + 0x0d6] = 0x4c;
						mem[p + 0x0d7] = 0xae;
						mem[p + 0x0d8] = 0xa7;
					}
				}
			}
			if (!unp->_forced) { /* FDT */
				if (u32eq(mem + p + 0x3a, 0x2a2a2a2a) &&
					u32eq(mem + p + 0x03, 0x8604A278) &&
					u32eq(mem + p + 0x0a, 0x2D950842)) {
					unp->_forced = p + 3;
				}
			}
			if (!unp->_forced) {
				/* decibel hacks */
				if (u32eq(mem + p + 0x3a, 0x2a2a2a2a) &&
					u32eq(mem + p + 0x00, 0x9D085EBD) &&
					u32eq(mem + p - 0x06, 0x018534A9)) {
					unp->_forced = p - 0x6;
				}
			}
			if (unp->_forced) {
				for (q = 0xd6; q < 0xde; q++) {
					if (mem[p + q] == 0x20) {
						if (u16eq(mem + p + q + 1, 0xa659) ||
							u16eq(mem + p + q + 1, 0xff81) ||
							u16eq(mem + p + q + 1, 0xe3bf) ||
							u16eq(mem + p + q + 1, 0xe5a0) ||
							u16eq(mem + p + q + 1, 0xe518)) {
							mem[p + q] = 0x2c;
							q += 2;
							continue;
						} else {
							unp->_retAdr = READ_LE_UINT16(&mem[p + q + 1]); // mem[p + q + 1] | mem[p + q + 2] << 8;
							break;
						}
					}
					if (mem[p + q] == 0x4c) {
						unp->_retAdr = READ_LE_UINT16(&mem[p + q + 1]); // mem[p + q + 1] | mem[p + q + 2] << 8;
						break;
					}
				}
				unp->_depAdr = READ_LE_UINT16(&mem[p + 0x30]); // mem[p + 0x30] | mem[p + 0x31] << 8;
				// some use $2d, some $ae
				for (q = 0xed; q < 0x108; q++) {
					if (u32eq(mem + p + q, 0xA518F7D0)) {
						unp->_endAdr = mem[p + q + 4];
						// if(unp->_DebugP)
						// printf("EndAdr from $%02x\n",unp->_endAdr);
						break;
					}
				}
				/*
				if anything it's unpacked to $d000-efff, it will be copied
				to $e000-ffff as last action in unpacker before starting.
				0196  20 DA 01  JSR $01DA ; some have this jsr nopped, reloc doesn't
				happen 0199  A9 37     LDA #$37 019b  85 01     STA $01 019d  58 CLI
				019e  20 00 0D  JSR $0D00 ; retaddr can be either here or following
				01a1  4C AE A7  JMP $A7AE
				01da  B9 00 EF  LDA $EF00,Y
				01dd  99 00 FF  STA $FF00,Y
				01e0  C8        INY
				01e1  D0 F7     BNE $01DA
				01e3  CE DC 01  DEC $01DC
				01e6  CE DF 01  DEC $01DF
				01e9  AD DF 01  LDA $01DF
				01ec  C9 DF     CMP #$DF   ;<< not fixed, found as lower as $44 for
				example 01ee  D0 EA     BNE $01DA 01f0  60        RTS Because of this,
				$d000-dfff will be a copy of $e000-efff. So if $2d points to >= $d000,
				SOMETIMES it's better save upto $ffff or: mem[$2d]|(mem[$2e]+$10)<<8
				Still it's not a rule and I don't know exactly when.
				17/06/09: Implemented but still experimental, so better check
				extensively. use -v to know when it does the adjustments. 28/10/09:
				whoops, was clearing ONLY $d000-dfff =)
				*/
				unp->_strMem = READ_LE_UINT16(&mem[p + 0x32]); // mem[p + 0x32] | mem[p + 0x33] << 8;
				for (q = 0xcd; q < 0xd0; q++) {
					if (u32eqmasked(mem + p + q, 0xffff00ff, 0xa9010020)) {
						unp->_ecaFlg = READ_LE_UINT16(&mem[p + q + 1]); // mem[p + q + 1] | mem[p + q + 2] << 8;
						for (q = 0x110; q < 0x11f; q++) {
							if (u32eq(mem + p + q, 0x99EF00B9) &&
								mem[p + q + 0x12] == 0xc9) {
								unp->_ecaFlg |= (mem[p + q + 0x13] - 0xf) << 24;
								break;
							}
						}
						break;
					}
				}
				/* radwar hack has a BRK here, fffe/f used as IRQ/BRK vector */
				if (mem[0x8e1] == 0) {
					mem[0x8e1] = 0x6c;
					mem[0x8e2] = 0xfe;
					mem[0x8e3] = 0xff;
				}
				break;
			}
		}
		if (unp->_depAdr) {
			unp->_idFlag = 1;
			return;
		}
	}
	/* old packer, many old 1985 warez used this */
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x81b, 0x018534A9) &&
			u32eq(mem + 0x822, 0xAFC600A0) &&
			u32eq(mem + 0x826, 0xB1082DCE) &&
			u32eq(mem + 0x85b, 0x2A2A2A2A)) {
			unp->_forced = 0x81b;
			unp->_depAdr = 0x100;
			unp->_strMem = READ_LE_UINT16(&mem[0x853]); // mem[0x853] | mem[0x854] << 8;
			unp->_endAdr = mem[0x895];
			unp->_retAdr = READ_LE_UINT16(&mem[0x885]); // mem[0x885] | mem[0x886] << 8;
			unp->_idFlag = 1;
			return;
		}
	}
}

} // End of namespace Unp64
