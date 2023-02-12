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

    /* TimeCruncher and clones */
    void scnTimeCruncher(UnpStr *unp) {
        unsigned char *mem;
        int q, p;
        if (unp->_idFlag)
            return;
        mem = unp->_mem;
        if (unp->_depAdr == 0) { /*TimeCruncher 4.0 */
            if ((*(unsigned int *)(mem + 0x811) == 0x00A201E6) &&
                (*(unsigned int *)(mem + 0x82e) == 0xAD4C2E85) &&
                (*(unsigned int *)(mem + 0x83d) == 0x0377204C)) {
                unp->_depAdr = 0x3ad;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x811;
                unp->_retAdr = READ_LE_UINT16(&mem[0x966]);
                unp->_endAdr = mem[0x829] | mem[0x82d] << 8;
                unp->_fStrAf = 0xfc; /* to test */
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 4.2 pre-header*/
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x811 + 0x84) == 0x31BD0186) &&
                (*(unsigned int *)(mem + 0x82c + 0x84) == 0xAD4C2E85) &&
                (*(unsigned int *)(mem + 0x83b + 0x84) == 0x0377204C) &&
                (*(unsigned int *)(mem + 0x842) == 0x9D088FBD) &&
                (*(unsigned int *)(mem + 0x846) == 0xD0E8080B)) {
                unp->_depAdr = READ_LE_UINT16(&mem[0x839]);
                if (unp->_info->_run == -1)
                    unp->_forced = 0x80d;
                unp->_retAdr = READ_LE_UINT16(&mem[0x864]);
                unp->_endAdr = unp->_info->_end - 0x84;
                unp->_strMem = 0x801;
                if ((*(unsigned int *)(mem + 0x85c) == 0xFDA32058) &&
                    (*(unsigned int *)(mem + 0x860) == 0x4CFD1520)) {
                    mem[0x85d] = 0x2c;
                    mem[0x860] = 0x2c;
                }
                mem[0x808] = 0x35;
                mem[0x809] = 0x39;
                mem[0x80a] = 0x00;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 4.2 pre-header / ILS */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x811 + 0x9d) == 0x31BD0186) &&
                (*(unsigned int *)(mem + 0x82c + 0x9d) == 0xAD4C2E85) &&
                (*(unsigned int *)(mem + 0x83b + 0x9d) == 0x0377204C) &&
                (*(unsigned int *)(mem + 0x84d) == 0x9D08a8BD) &&
                (*(unsigned int *)(mem + 0x851) == 0xD0E8080B)) {
                unp->_depAdr = READ_LE_UINT16(&mem[0x844]);
                if (unp->_info->_run == -1)
                    unp->_forced = 0x810;
                unp->_retAdr = READ_LE_UINT16(&mem[0x86f]);
                unp->_endAdr = unp->_info->_end - 0x9d;
                unp->_strMem = 0x801;
                if ((*(unsigned int *)(mem + 0x867) == 0xFDA32058) &&
                    (*(unsigned int *)(mem + 0x86b) == 0x4CFD1520)) {
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
                return;
            }
        }
        /*TimeCruncher pre-header by Vikings */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x81a) == 0x018535a9) &&
                (*(unsigned int *)(mem + 0x81e) == 0x2CBD00A2) &&
                (*(unsigned int *)(mem + 0x822) == 0x04009D08) &&
                (*(unsigned int *)(mem + 0x833) == 0xEE08019D) &&
                (*(unsigned int *)(mem + 0x846) == 0xD0C90404)) {
                unp->_depAdr = 0x400;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x81a;
                unp->_retAdr = READ_LE_UINT16(&mem[0x861]);
                unp->_endAdr = unp->_info->_end - 0x62;
                unp->_strMem = 0x801;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 4.2 */
        if (unp->_depAdr == 0) {
            if (/*((*(unsigned int*)(mem+0x80b)&0x00ffffff)==0x007800A2) &&*/
                (*(unsigned int *)(mem + 0x811) == 0x31BD0186) &&
                ((*(unsigned int *)(mem + 0x82c) == 0xAD4C2E85) || (*(unsigned int *)(mem + 0x82c) == 0xAD4Caf85)) &&
                ((*(unsigned int *)(mem + 0x83b) & 0xf0ffffff) == 0x0077204C) &&
                ((*(unsigned int *)(mem + 0x951) & 0xf0ffffff) == 0x003C4C68)) {
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
                    return;
                }
        }
        /*TimeCruncher 4.3? */
        if (unp->_depAdr == 0) {
            if (((*(unsigned int *)(mem + 0x82b) & 0x00ffffff) == 0x0003AA4C) &&
                (*(unsigned int *)(mem + 0x80a) == 0x7800A200) &&
                (*(unsigned int *)(mem + 0x834) == 0x90037420) &&
                (*(unsigned int *)(mem + 0x954) == 0xF2D007C0)) {
                unp->_depAdr = 0x3aa;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x80b;
                unp->_retAdr = READ_LE_UINT16(&mem[0x95e]);
                unp->_endAdr = READ_LE_UINT16(&mem[0x830]);
                unp->_strMem = 0x801;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 4.4 */
        if (unp->_depAdr == 0) {
            if (((*(unsigned short int *)(mem + 0x80b) & 0xfff0) == 0x00A0) &&
                (*(unsigned int *)(mem + 0x811) == 0xFA9D082E) &&
                (*(unsigned int *)(mem + 0x82a) == 0x03AE4C00) &&
                (*(unsigned int *)(mem + 0x83a) == 0x04B00374)) {
                unp->_depAdr = 0x3ae;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x80b;
                unp->_retAdr = READ_LE_UINT16(&mem[0x95e]);
                unp->_endAdr = READ_LE_UINT16(&mem[0x830]);
                unp->_fStrAf = 0xfc;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 5 */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x813) == 0xBD9AFFA2) &&
                (*(unsigned int *)(mem + 0x817) == 0xF89D0837) &&
                (*(unsigned int *)(mem + 0x81b) == 0xF7D0CA00) &&
                (*(unsigned int *)(mem + 0x82b) == 0x2E842D85)) {
                unp->_idFlag = 1;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x813;
                unp->_endAdr = mem[0x828] | mem[0x82a] << 8;
                unp->_depAdr = 0x100;
                /* same as Stoat&Tim Timecrunch 2mhz v5 */
            } else if ((*(unsigned int *)(mem + 0x80f) == 0x9AFFA278) &&
                       (*(unsigned int *)(mem + 0x813) == 0x9D0837BD) &&
                       (*(unsigned int *)(mem + 0x817) == 0xD0CA00F8) &&
                       (*(unsigned int *)(mem + 0x82b) == 0x2E842D85)) {
                unp->_idFlag = 1;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x812;
                unp->_depAdr = 0x100;
                unp->_endAdr = mem[0x828] | mem[0x82a] << 8;
            } else if ((*(unsigned int *)(mem + 0x819) == 0x9AFFA278) &&
                       (*(unsigned int *)(mem + 0x81d) == 0x9D0837BD) &&
                       (*(unsigned int *)(mem + 0x821) == 0xD0CA00F8) &&
                       (*(unsigned int *)(mem + 0x82f) == 0x2E842D85)) {
                unp->_idFlag = 1;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x819;
                unp->_depAdr = 0x100;
                unp->_endAdr = mem[0x82c] | mem[0x82e] << 8;
            } else if ((*(unsigned int *)(mem + 0x810) == 0x9AFFA278) &&
                       (*(unsigned int *)(mem + 0x814) == 0x9D0837BD) &&
                       (*(unsigned int *)(mem + 0x818) == 0xD0CA00F8) &&
                       (*(unsigned int *)(mem + 0x82b) == 0x2E842D85)) {
                unp->_idFlag = 1;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x810;
                unp->_depAdr = 0x100;
                unp->_endAdr = mem[0x828] | mem[0x82a] << 8; /* also at $83d/e (add 1) */
                /* hack at 2064 found in Mentallic/PD */
            } else /* 5.0 gen hack */
                if ((*(unsigned int *)(mem + 0x841) == 0x07E89D09) &&
                    (*(unsigned int *)(mem + 0x851) == 0x9D2002A2) &&
                    (*(unsigned int *)(mem + 0x861) == 0x90066901) &&
                    (*(unsigned int *)(mem + 0x901) == 0xC0FCC6FD)) {
                    unp->_idFlag = 1;
                    for (q = 0x801; q < 0x830; q++) {
                        if (mem[q] == 0x78) {
                            if (unp->_info->_run == -1) {
                                unp->_forced = q;
                                unp->_info->_run = q;
                            }
                        }
                        if ((unp->_endAdr == 0x10000) &&
                            (mem[q] == 0xa9) &&
                            (mem[q + 2] == 0xa0) &&
                            (*(unsigned int *)(mem + q + 4) == 0x2E842D85)) {
                            unp->_endAdr = mem[q + 1] | mem[q + 3] << 8;
                        }
                    }
                    unp->_depAdr = 0x100;
                }
            if (unp->_idFlag) {
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
                        if ((*(unsigned short int *)(mem + q + 1) == 0xa659) ||
                            (*(unsigned short int *)(mem + q + 1) == 0xff81) ||
                            (*(unsigned short int *)(mem + q + 1) == 0xe3bf) ||
                            (*(unsigned short int *)(mem + q + 1) == 0xe544) ||
                            (*(unsigned short int *)(mem + q + 1) == 0xe5a0) ||
                            (*(unsigned short int *)(mem + q + 1) == 0xe518)) {
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
                return;
            }
        }
        /*TimeCruncher 3.0? */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x818) == 0x8534A978) &&
                (*(unsigned int *)(mem + 0x81c) == 0xBDD2A201) &&
                (*(unsigned int *)(mem + 0x820) == 0xF99D0850) &&
                (*(unsigned int *)(mem + 0x824) == 0xB079E000) &&
                (*(unsigned int *)(mem + 0x8c1) == 0xa0033420)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x818;
                unp->_retAdr = READ_LE_UINT16(&mem[0x95c]);
                unp->_endAdr = 0x2d;
                unp->_strMem = 0x0801;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 3.1/BYG */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x847) == 0xC807E899) &&
                (*(unsigned int *)(mem + 0x84b) == 0x02EEF7D0) &&
                (*(unsigned int *)(mem + 0x84f) == 0x0105EE01) &&
                (*(unsigned int *)(mem + 0x816) == 0x3CB9C4A0) &&
                (*(unsigned int *)(mem + 0x967) == 0x4C01E678)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x967;
                unp->_retAdr = READ_LE_UINT16(&mem[0x934]);
                unp->_endAdr = mem[0x833] | mem[0x837] << 8;
                unp->_fStrAf = 0xfc;

                if (unp->_retAdr == 0x277) { /* this version only issues RUN and has no text */
                    unp->_retAdr = 0xa7ae;
                }
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 3.3/2066 */
        if (unp->_depAdr == 0) {
            if ((mem[0x812] == 0x78) &&
                ((*(unsigned int *)(mem + 0x818) & 0xfffff0ff) == 0x018530a9) &&
                (*(unsigned int *)(mem + 0x820) == 0xF899083C) &&
                (*(unsigned int *)(mem + 0x82b) == 0x03339908) &&
                (*(unsigned int *)(mem + 0x838) == 0x004C2E85)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x812;
                unp->_endAdr = mem[0x833] | mem[0x837] << 8;
                for (p = 0x91a; p < 0x931; p++) {
                    if ((*(unsigned int *)(mem + p) & 0xff00ffff) == 0x4c000185) {
                        unp->_retAdr = READ_LE_UINT16(&mem[p + 4]);
                        unp->_fStrAf = 0xfc;
                    }
                }
                unp->_idFlag = 1;
                return;
            }
        }

        /* Stoat&Tim Timecrunch 2mhz v1 */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x813) == 0xBDBFA278) &&
                (*(unsigned int *)(mem + 0x817) == 0xF99D0841) &&
                (*(unsigned int *)(mem + 0x81b) == 0x0900BD00) &&
                (*(unsigned int *)(mem + 0x833) == 0xAE852D85) &&
                (*(unsigned int *)(mem + 0x930) == 0xA9D6D007)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x813;
                unp->_retAdr = READ_LE_UINT16(&mem[0x941]);
                unp->_endAdr = mem[0x832] | mem[0x838] << 8;
                unp->_fStrAf = 0xfc;
                unp->_idFlag = 1;
                return;
            }
        }
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x813) == 0xB9BFA078) &&
                (*(unsigned int *)(mem + 0x817) == 0xF9990840) &&
                (*(unsigned int *)(mem + 0x81b) == 0x08ffB900) &&
                (*(unsigned int *)(mem + 0x832) == 0xAE852D85) &&
                (*(unsigned int *)(mem + 0x92f) == 0xA9D6D007)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x813;
                unp->_retAdr = READ_LE_UINT16(&mem[0x940]);
                unp->_endAdr = mem[0x831] | mem[0x837] << 8;
                unp->_fStrAf = 0xfc;
                unp->_idFlag = 1;
                return;
            }
        }

        /*TimeCruncher 3.1+/2061 */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x80d) == 0x8534A978) ||
                (((mem[0x80d] == 0x20) || (mem[0x080d] == 0x4c)) && (mem[0x810] == 0x85))) {
                if ((*(unsigned int *)(mem + 0x811) == 0xB9C4A001) &&
                    (*(unsigned int *)(mem + 0x815) == 0xF899083C) &&
                    (*(unsigned int *)(mem + 0x838) == 0x004C2E85)) {
                    unp->_depAdr = 0x100;
                    if (unp->_info->_run == -1)
                        unp->_forced = 0x80d;
                    unp->_retAdr = READ_LE_UINT16(&mem[0x934]);
                    unp->_endAdr = mem[0x833] | mem[0x837] << 8;
                    unp->_fStrAf = 0xfc;
                    unp->_idFlag = 1;
                    return;
                }
                if ((*(unsigned int *)(mem + 0x811) == 0xB9C1A001) &&
                    (*(unsigned int *)(mem + 0x815) == 0xF8990831) &&
                    (*(unsigned int *)(mem + 0x82d) == 0x004Caf85)) {
                    unp->_depAdr = 0x100;
                    if (unp->_info->_run == -1)
                        unp->_forced = 0x80d;
                    unp->_retAdr = READ_LE_UINT16(&mem[0x929]);
                    unp->_endAdr = mem[0x824] | mem[0x82a] << 8;
                    unp->_fStrAf = 0xfc;
                    unp->_idFlag = 1;
                    return;
                }
            }
        }
        /*TimeCruncher 3.x/2072 */
        if (unp->_depAdr == 0) {
            if (((((*(unsigned int *)(mem + 0x818)) & 0xfff0ffff) == 0x8530A978) ||
                 (*(unsigned int *)(mem + 0x818) == 0x8578d020)) &&
                (*(unsigned int *)(mem + 0x81c) == 0xB9C4A001) &&
                (*(unsigned int *)(mem + 0x820) == 0xF899083C) &&
                ((*(unsigned int *)(mem + 0x838) == 0x004C2E85) ||
                 (*(unsigned int *)(mem + 0x838) == 0x004Caf85)) &&
                mem[0x83c] == 0x01) {
                unp->_depAdr = 0x100;
                if (mem[0x081a] == 0x78) {
                    if (unp->_info->_run == -1)
                        unp->_forced = 0x81a;
                    /*MATCHAM SPEED-PACKER V1.1*/
                } else {
                    if (unp->_info->_run == -1)
                        unp->_forced = 0x818;
                }
                unp->_endAdr = mem[0x833] | mem[0x837] << 8;
                // Henk/HTL $91a, Matcham/NET $930
                for (p = 0x91a; p < 0x931; p++) {
                    if ((*(unsigned int *)(mem + p) & 0xff00ffff) == 0x4c000185) {
                        unp->_retAdr = READ_LE_UINT16(&mem[p + 4]);
                        unp->_fStrAf = 0xfc;
                        unp->_strAdC = 0xffff;
                    }
                }
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher /Radical? */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x80b) == 0x3CB900A0) &&
                (*(unsigned int *)(mem + 0x80f) == 0x00F89908) &&
                (*(unsigned int *)(mem + 0x820) == 0xF899083C) &&
                (*(unsigned int *)(mem + 0x839) == 0x04004C2E)) {
                unp->_depAdr = 0x400;
                unp->_forced = 0x80b;
                unp->_endAdr = mem[0x833] | mem[0x837] << 8;
                unp->_retAdr = READ_LE_UINT16(&mem[0x91e]);
                unp->_fStrAf = 0xfc;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher /2074 */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x81a) == 0xC4A00185) &&
                (*(unsigned int *)(mem + 0x81e) == 0x99083DB9) &&
                (*(unsigned int *)(mem + 0x822) == 0xFDB900F9) &&
                (*(unsigned int *)(mem + 0x836) == 0xA2AF852E)) {
                unp->_depAdr = 0x101;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x81a;
                unp->_endAdr = mem[0x82e] | mem[0x834] << 8;
                for (p = 0x935; p < 0x93a; p++) {
                    if (*(mem + p) == 0x4c) {
                        unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
                        unp->_fStrAf = 0xfc;
                    }
                }
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher /Fast pack'em */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x84f) == 0x4909C8BD) &&
                (*(unsigned int *)(mem + 0x857) == 0xA0F510CA) &&
                (*(unsigned int *)(mem + 0x85b) == 0x087DB9C1) &&
                (*(unsigned int *)(mem + 0x885) == 0x9909EEB9)) {
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
                unp->_endAdr = mem[0x86c] | mem[0x872] << 8;
                unp->_fStrAf = 0xfc;
                if (unp->_debugP) {
                    p = READ_LE_UINT16(&mem[0x850]);
                    if (mem[0x852] == 0x49) {
                        for (q = 0; q < 40 && mem[p + q]; q++) {
                            mem[p + q] ^= mem[0x853];
                        }
                    }
                }
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher /FBI */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x888) == 0xC807E899) &&
                (*(unsigned int *)(mem + 0x88c) == 0x02EEF7D0) &&
                (*(unsigned int *)(mem + 0x890) == 0x0105EE01) &&
                (*(unsigned int *)(mem + 0x80d) == 0xFFA2D878) &&
                (*(unsigned int *)(mem + 0x971) == 0x4C580185)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x80d;
                unp->_retAdr = READ_LE_UINT16(&mem[0x975]);
                unp->_endAdr = mem[0x86c] | mem[0x872] << 8;
                unp->_fStrAf = 0xfc;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher /Triumwyrat aka Final Cut Compressor */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x822) == 0x04289D09) &&
                (*(unsigned int *)(mem + 0x85b) == 0x087DB9C1) &&
                (*(unsigned int *)(mem + 0x87a) == 0x01004c01)) {
                if (unp->_info->_run == -1)
                    unp->_forced = 0x818;
                unp->_depAdr = 0x100;
                unp->_endAdr = mem[0x86c] | mem[0x872] << 8;
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
                return;
            }
        }
        /*TimeCruncher generic hacks*/
        if (unp->_depAdr == 0) {
            if (((*(unsigned int *)(mem + 0x839) & 0xffffff00) == 0x01004C00) &&
                ((*(unsigned int *)(mem + 0x847) & 0xfffff0ff) == 0xC807E099) &&
                (*(unsigned int *)(mem + 0x84b) == 0x02EEF7D0) &&
                (*(unsigned int *)(mem + 0x84f) == 0x0105EE01) &&
                (*(unsigned int *)(mem + 0x91f) == 0xC6FFC602)) {
                if (unp->_info->_run == -1) {
                    for (p = 0x80b; p < 0x820; p++) {
                        if (mem[p] == 0x78) {
                            unp->_forced = p;
                            break;
                        }
                    }
                }
                unp->_depAdr = 0x100;
                unp->_endAdr = mem[0x833] | mem[0x837] << 8;
                unp->_retAdr = READ_LE_UINT16(&mem[0x934]);
                unp->_fStrAf = 0xfc;
                /*HTL hack (lax $dd07)*/
                if (*(unsigned int *)(mem + 0x822) == 0xE0DD07AF) {
                    mem[0x0822] = 0xa9;
                    mem[0x0823] = 0xff;
                    mem[0x0824] = 0xaa;
                }
                unp->_idFlag = 1;
                return;
            }
        }
        /* File Press Expert/SIR crunch 1 */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x824) == 0x018534A9) &&
                (*(unsigned int *)(mem + 0x82a) == 0x99083CB9) &&
                (*(unsigned int *)(mem + 0x82e) == 0xB9D800F8) &&
                (*(unsigned int *)(mem + 0x892) == 0xD0033420) &&
                (*(unsigned int *)(mem + 0x940) == 0x01E6EC10)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x81e;
                unp->_endAdr = READ_LE_UINT16(&mem[0x840]);
                unp->_strMem = 0x801;
                p = 0x96f;
                if (mem[p] == 0x4c) {
                    unp->_retAdr = READ_LE_UINT16(&mem[p + 1]);
                } else if (mem[p + 1] == 0x4c) {
                    unp->_retAdr = READ_LE_UINT16(&mem[p + 2]);
                }

                unp->_idFlag = 1;
                return;
            }
        }
        /* SIR crunch 4 */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x81b) == 0x018534A9) &&
                (*(unsigned int *)(mem + 0x81f) == 0x3BB9C4A0) &&
                (*(unsigned int *)(mem + 0x82b) == 0x88033399) &&
                (*(unsigned int *)(mem + 0x95F) == 0xA2F0FEC6)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x81b;
                unp->_retAdr = READ_LE_UINT16(&mem[0x970]);
                unp->_endAdr = READ_LE_UINT16(&mem[0x83f]);
                unp->_strMem = 0x801;
                unp->_idFlag = 1;
                return;
            }
        }
        /* SIR crunch 3 */
        if (unp->_depAdr == 0) {
            if ((mem[0x81e] == 0x78) &&
                (*(unsigned int *)(mem + 0x824) == 0x018534A9) &&
                (*(unsigned int *)(mem + 0x828) == 0x48B9C4A0) &&
                (*(unsigned int *)(mem + 0x834) == 0x88033399) &&
                (*(unsigned int *)(mem + 0x96c) == 0xA2F0FEC6)) {
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
                return;
            }
        }
        /* SIR crunch 2.x */
        if (unp->_depAdr == 0) {
            if ((mem[0x81e] == 0x78) &&
                (*(unsigned int *)(mem + 0x824) == 0x018534A9) &&
                (*(unsigned int *)(mem + 0x828) == 0x4aB9C4A0) &&
                (*(unsigned int *)(mem + 0x834) == 0x88033399) &&
                (*(unsigned int *)(mem + 0x96e) == 0xA2F0FEC6)) {
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
                return;
            }
        }
        /* Entropy hack? similar to SIR crunch 3, at $80d */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x80e) == 0x23B9C4A0) &&
                (*(unsigned int *)(mem + 0x816) == 0x9908E4B9) &&
                (*(unsigned int *)(mem + 0x81a) == 0xD0880333) &&
                (*(unsigned int *)(mem + 0x947) == 0xA2F0FEC6)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x80e;
                unp->_retAdr = READ_LE_UINT16(&mem[0x94e]);
                unp->_strMem = 0x801;
                unp->_endAdr = READ_LE_UINT16(&mem[0x827]);
                unp->_idFlag = 1;
                return;
            }
        }
        /* SupraCompactor 2.0/TKC - TC 5 clone + unpacktext */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x817) == 0xBD9ACA78) &&
                (*(unsigned int *)(mem + 0x81b) == 0xF89D0842) &&
                (*(unsigned int *)(mem + 0x84b) == 0xE89D0935) &&
                (*(unsigned int *)(mem + 0x90f) == 0xDCD0E7C0)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x817;
                if (*(unsigned int *)(mem + 0x91f) == 0xA6592008) {
                    mem[0x920] = 0x2c;
                    unp->_retAdr = READ_LE_UINT16(&mem[0x925]);
                } else {
                    unp->_retAdr = READ_LE_UINT16(&mem[0x921]);
                }
                unp->_endAdr = READ_LE_UINT16(&mem[0x848]);
                unp->_endAdr++;
                unp->_fStrAf = 0xfe;
                unp->_idFlag = 1;
                return;
            }
        }
        /* Relax Timer, TimeCruncher 3.0 hack? very similar to mastercompr/relax */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x816) == 0x8534A978) &&
                (*(unsigned int *)(mem + 0x81a) == 0xBDD2A201) &&
                (*(unsigned int *)(mem + 0x81e) == 0xF99D084e) &&
                (*(unsigned int *)(mem + 0x822) == 0xB073E000) &&
                (*(unsigned int *)(mem + 0x8bf) == 0xa0033420)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x816;
                unp->_retAdr = READ_LE_UINT16(&mem[0x94d]);
                unp->_endAdr = 0x2d;
                unp->_strMem = 0x0801;
                unp->_idFlag = 1;
                return;
            }
        }
        /* TimeCruncher 3/Triad, same as Relax but at $812 */
        if (unp->_depAdr == 0) {
            if ((mem[0x812] == 0x78) && (mem[0x813] == 0xe6) &&
                (*(unsigned int *)(mem + 0x814) == 0xBDD2A201) &&
                (*(unsigned int *)(mem + 0x818) == 0xF99D0848) &&
                (*(unsigned int *)(mem + 0x81c) == 0xB073E000) &&
                (*(unsigned int *)(mem + 0x8b9) == 0xa0033420)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x812;
                unp->_retAdr = READ_LE_UINT16(&mem[0x947]);
                unp->_endAdr = 0x2d;
                unp->_strMem = 0x0801;
                unp->_idFlag = 1;
                return;
            }
        }
        /* TimeCruncher 3/PC */
        if (unp->_depAdr == 0) {
            if ((mem[0x810] == 0x78) && (mem[0x813] == 0x85) &&
                (*(unsigned int *)(mem + 0x814) == 0xBD00A201) &&
                (*(unsigned int *)(mem + 0x818) == 0x349D0920) &&
                (*(unsigned int *)(mem + 0x81c) == 0x73E0E803) &&
                (*(unsigned int *)(mem + 0x8bf) == 0xa0033420)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x810;
                unp->_retAdr = READ_LE_UINT16(&mem[0x94c]);
                unp->_endAdr = 0x2d;
                unp->_strMem = 0x0801;
                unp->_idFlag = 1;
                return;
            }
        }
        /* TimeCruncher 4.2/Triad */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x81a) == 0x00A201E6) &&
                (*(unsigned int *)(mem + 0x81e) == 0x9D083CBD) &&
                (*(unsigned int *)(mem + 0x837) == 0xAD4C2E85) &&
                (*(unsigned int *)(mem + 0x846) == 0x0377204C) &&
                (*(unsigned int *)(mem + 0x95c) == 0x033C4C68)) {
                unp->_depAdr = 0x3ad;
                if (unp->_info->_run == -1)
                    unp->_forced = 0x81a;
                unp->_retAdr = READ_LE_UINT16(&mem[0x96f]);
                unp->_endAdr = mem[0x832] | mem[0x836] << 8;
                unp->_strMem = 0x801;
                unp->_idFlag = 1;
                return;
            }
        }
        /*TimeCruncher 4.2 generic hack */
        if (unp->_depAdr == 0) {
            if ((*(unsigned short int *)(mem + 0x82e) == 0xAD4C) &&
                (*(unsigned int *)(mem + 0x875) == 0xF785FEA5) &&
                (*(unsigned int *)(mem + 0x8c3) == 0x2001004C) &&
                (*(unsigned int *)(mem + 0x9B4) == 0x01004C02)) {
                unp->_depAdr = READ_LE_UINT16(&mem[0x82f]);
                if (unp->_info->_run == -1) {
                    for (q = 0x810; q < 0x820; q++) {
                        if (*(unsigned int *)(mem + q) == 0x8600A278) {
                            unp->_forced = q;
                            break;
                        }
                        if (*(unsigned int *)(mem + q) == 0x31BD0186) {
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
                return;
            }
        }
        /*TimeCruncher 6/different hacks
         almost identical to TimeCruncher 6 henk/htl but different preamble
         */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x844) == 0x990958B9) &&
                (*(unsigned int *)(mem + 0x848) == 0xD0C807E8) &&
                (*(unsigned int *)(mem + 0x84c) == 0x0102EEF7) &&
                (*(unsigned int *)(mem + 0x8c1) == 0x033420E8)) {
                unp->_depAdr = 0x100;
                unp->_fStrAf = 0xfc;
                unp->_strAdC = 0xffff;
                unp->_retAdr = READ_LE_UINT16(&mem[0x91e]);
                unp->_endAdr = READ_LE_UINT16(&mem[0x840]);
                if ((mem[0x813] == 0x78) && (mem[0x91d] == 0xee) &&
                    (*(unsigned int *)(mem + 0x819) == 0xF899083C)) {
                    unp->_forced = 0x813;
                    unp->_retAdr = READ_LE_UINT16(&mem[0x953]);
                    if (unp->_retAdr == 0xa659)
                        unp->_retAdr = READ_LE_UINT16(&mem[0x956]);
                    unp->_idFlag = 1;
                    return;
                } else if ((mem[0x810] == 0x78) &&
                           (*(unsigned int *)(mem + 0x820) == 0xF899083C)) {
                    unp->_forced = 0x810;
                    unp->_idFlag = 1;
                    return;
                } else if ((mem[0x812] == 0x78) &&
                           (*(unsigned int *)(mem + 0x824) == 0xF899083C)) {
                    unp->_forced = 0x812;
                    unp->_idFlag = 1;
                    return;
                } else if ((mem[0x80e] == 0xe6) &&
                           (*(unsigned int *)(mem + 0x820) == 0xF899083C)) {
                    unp->_forced = 0x80e;
                    unp->_idFlag = 1;
                    return;
                } else if ((mem[0x819] == 0xe6) &&
                           (*(unsigned int *)(mem + 0x824) == 0xF899083C)) {
                    unp->_forced = 0x819;
                    unp->_idFlag = 1;
                    return;
                } else if ((mem[0x80d] == 0x78) &&
                           (*(unsigned int *)(mem + 0x811) == 0xF899083C)) {
                    unp->_forced = 0x80d;
                    unp->_idFlag = 1;
                    return;
                } else {
                    if (unp->_retAdr < unp->_depAdr) { // dirty workaround, found ONE single case like this
                        // retaddr=$00f0 and it contains just JSR $A660+JMP $A7AE
                        unp->_retAdr = 0xa7ae;
                    }
                    unp->_idFlag = 1;
                    return;
                }
            }
        }
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x844) == 0x990976B9) &&
                (*(unsigned int *)(mem + 0x848) == 0xD0C807E8) &&
                (*(unsigned int *)(mem + 0x831) == 0x01004CED) &&
                (*(unsigned int *)(mem + 0x8c1) == 0x08F00334)) {
                unp->_depAdr = 0x100;
                if (unp->_info->_run == -1) {
                    if (mem[0x818] == 0x78)
                        unp->_forced = 0x818;
                }
                unp->_retAdr = READ_LE_UINT16(&mem[0x974]);
                unp->_endAdr = READ_LE_UINT16(&mem[0x840]);
                unp->_idFlag = 1;
                return;
            }
        }
        /* time 3.3 2mhz/galleon */
        if (unp->_depAdr == 0) {
            if ((mem[0x80b] == 0xa0) && (mem[0x80c] == 0x00) &&
                (*(unsigned int *)(mem + 0x810) == 0x990829B9) &&
                (*(unsigned int *)(mem + 0x817) == 0x339908EA) &&
                (*(unsigned int *)(mem + 0x825) == 0x004C2E85) &&
                (*(unsigned int *)(mem + 0x8ae) == 0x08F00334)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x80b;
                unp->_retAdr = READ_LE_UINT16(&mem[0x921]);
                if (unp->_retAdr < 0x800)
                    unp->_rtAFrc = 1;
                unp->_endAdr = mem[0x820] | mem[0x824] << 8;
                unp->_idFlag = 1;
                return;
            }
        }
        /* science451, variable entry and depacker */
        if (unp->_depAdr == 0) {
            if ((mem[0x81e] == 0x78) && (mem[0x820] == 0x01) &&
                (*(unsigned int *)(mem + 0x821) == 0x9D084EBD) &&
                (*(unsigned int *)(mem + 0x82d) == 0xCAF1D0CA) &&
                (*(unsigned int *)(mem + 0x831) == 0xC600A09A) &&
                (*(unsigned int *)(mem + 0x835) == 0x083DCEFD)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x81e;
                unp->_retAdr = READ_LE_UINT16(&mem[0x94d]);
                if (unp->_retAdr < 0x800)
                    unp->_rtAFrc = 1;
                unp->_endAdr = 0x2d;
                unp->_idFlag = 1;
                return;
            }
        }
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x80b) == 0xE67800A0) &&
                (*(unsigned int *)(mem + 0x813) == 0x9D083CBD) &&
                (*(unsigned int *)(mem + 0x817) == 0xFBBD00F9) &&
                (*(unsigned int *)(mem + 0x81b) == 0x04329D08) &&
                (*(unsigned int *)(mem + 0x8d1) == 0xD0066918)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x80b;
                unp->_retAdr = READ_LE_UINT16(&mem[0x929]);
                if (unp->_retAdr < 0x800)
                    unp->_rtAFrc = 1;
                unp->_endAdr = 0x2d;
                unp->_idFlag = 1;
                return;
            }
        }
        /* Ikari, hack of s451 variant */
        if (unp->_depAdr == 0) {
            if ((*(unsigned int *)(mem + 0x817) == 0xE678FFA2) &&
                (*(unsigned int *)(mem + 0x81c) == 0x9D084EBD) &&
                (*(unsigned int *)(mem + 0x82d) == 0xCAecD0CA) &&
                (*(unsigned int *)(mem + 0x831) == 0xC600A09A) &&
                (*(unsigned int *)(mem + 0x835) == 0x083DCEFD)) {
                unp->_depAdr = 0x100;
                unp->_forced = 0x817;
                unp->_retAdr = READ_LE_UINT16(&mem[0x94d]);
                if (unp->_retAdr < 0x800)
                    unp->_rtAFrc = 1;
                unp->_endAdr = 0x2d;
                unp->_idFlag = 1;
                return;
            }
        }
    }

} // End of namespace Unp64
