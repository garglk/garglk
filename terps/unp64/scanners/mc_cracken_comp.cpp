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
/* MCCrackenCompressor */
#include "../unp64.h"
void scnMCCrackenComp(UnpStr *unp) {
    unsigned char *mem;
    int p, q;
    if (unp->_idFlag)
        return;
    mem = unp->_mem;
    if (unp->_depAdr == 0) {
        for (p = 0x810; p < 0x840; p++) {
            if (mem[p] == 0xa9 && mem[p + 2] == 0x85 &&
                (*(unsigned int *)(mem + p + 0x04) == 0xAFC600A0) &&
                (*(unsigned int *)(mem + p + 0x2a) == 0xFFA2F8D0) &&
                (*(unsigned int *)(mem + p + 0x2e) == 0x01004C9A) &&
                (*(unsigned int *)(mem + p + 0x3f) == 0x2a2a2a2a)) {
                unp->_depAdr = 0x100;
                unp->_endAdr = 0xae;
                unp->_strMem = READ_LE_UINT16(&mem[p + 0x32]);
                if (mem[p + 0x61] == 0x4c) {
                    unp->_retAdr = READ_LE_UINT16(&mem[p + 0x62]);
                    if ((unp->_retAdr == 0x1b7) &&
                        (*(unsigned int *)(mem + p + 0xed) == 0x4CA65920)) {
                        unp->_retAdr = 0xa7ae;
                        mem[p + 0x62] = 0xae;
                        mem[p + 0x63] = 0xa7;
                    }
                    if (unp->_info->_run == -1) {
                        unp->_forced = p;
                        while (--p >= 0x810) {
                            if (mem[p] == 0x78) {
                                unp->_forced = p;
                            }
                        }
                    }
                } else if (mem[p + 0x60] == 0x4c /* && mem[p+0x108]==0x4c */) {
                    unp->_forced = 0x828;
                    if (mem[p + 0x108] == 0x4c) {
                        unp->_retAdr = READ_LE_UINT16(&mem[p + 0x109]);
                    } else if ((mem[p + 0xff] == 0x4c) && (mem[p + 0x100] == 0xab)) {
                        unp->_retAdr = 0x5ab;
                    }
                }
                break;
            }
        }
    }
    if (unp->_depAdr == 0) {
        if ((*(unsigned int *)(mem + 0x828) == 0x8534A978) &&
            (*(unsigned int *)(mem + 0x82c) == 0xC600A001) &&
            (*(unsigned int *)(mem + 0x830) == 0x0838CE2E) &&
            (*(unsigned int *)(mem + 0x834) == 0x00992DB1) &&
            (*(unsigned int *)(mem + 0x908) == 0x2001364C)) {
            unp->_depAdr = 0x100;
            unp->_endAdr = 0xae;
            unp->_retAdr = READ_LE_UINT16(&mem[0x88b]);
            unp->_strMem = READ_LE_UINT16(&mem[0x85b]);
            unp->_forced = 0x828;
            unp->_idFlag = 1;
            return;
        }
    }
    if (unp->_depAdr == 0) {
        if ((*(unsigned int *)(mem + 0x828) == 0x8534A978) &&
            (*(unsigned int *)(mem + 0x82c) == 0xC600A001) &&
            (*(unsigned int *)(mem + 0x830) == 0x0838CEaf) &&
            (*(unsigned int *)(mem + 0x834) == 0x0099aeB1) &&
            (*(unsigned int *)(mem + 0x908) == 0x2001364C)) {
            unp->_depAdr = 0x100;
            unp->_endAdr = 0xae;
            unp->_retAdr = READ_LE_UINT16(&mem[0x88b]);
            unp->_strMem = READ_LE_UINT16(&mem[0x85b]);
            if (unp->_info->_run == -1)
                unp->_forced = 0x813;
            unp->_idFlag = 1;
            return;
        }
    }
    if (unp->_depAdr == 0) {
        if ((*(unsigned int *)(mem + 0x812) == 0xA201E678) &&
            (*(unsigned int *)(mem + 0x816) == 0x0840BD06) &&
            (*(unsigned int *)(mem + 0x81a) == 0x10CAAB95) &&
            (*(unsigned int *)(mem + 0x81e) == 0x00A09AF8) &&
            (*(unsigned int *)(mem + 0x8f0) == 0x2001364C)) {
            unp->_depAdr = 0x100;
            unp->_endAdr = 0xae;
            unp->_retAdr = READ_LE_UINT16(&mem[0x873]);
            unp->_strMem = READ_LE_UINT16(&mem[0x843]);
            if (unp->_info->_run == -1)
                unp->_forced = 0x812;
            unp->_idFlag = 1;
            return;
        }
    }
    /* substantially a normal MCC + crypted data from $916 */
    if (unp->_depAdr == 0) {
        if (((*(unsigned int *)(mem + 0x80e) & 0xffff00ff) == 0xAE8500A9) &&
            (*(unsigned int *)(mem + 0x81a) == 0x9816A001) &&
            (*(unsigned int *)(mem + 0x81e) == 0x99090059) &&
            (*(unsigned int *)(mem + 0x86a) == 0x07292A2A) &&
            (*(unsigned int *)(mem + 0x908) == 0x2001364C)) {
            /* patch deprypter and already xor data upto eof
             so unused memory won't be filled with a sequential pattern
             */
            mem[0x81c] = 0x00;
            mem[0x81e] = 0x4c;
            mem[0x81f] = 0x2f;
            mem[0x820] = 0x08;
            p = mem[0x80f] | mem[0x813] << 8;
            q = 0x916;
            for (q = 0x916; q < p; q++)
                mem[q] ^= (q & 0xff);

            unp->_depAdr = 0x100;
            unp->_endAdr = 0xae;
            unp->_retAdr = READ_LE_UINT16(&mem[0x88b]);
            unp->_strMem = READ_LE_UINT16(&mem[0x85b]);
            unp->_forced = 0x80e;
            unp->_idFlag = 1;
            return;
        }
    }
    if (unp->_depAdr) {
        unp->_idFlag = 1;
        return;
    }
}

} // End of namespace Unp64
