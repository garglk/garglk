// This is a cut-down version of UNP64 with only the bare minimum
// needed to decompress a number of Scott Adams Commodore 64 games
// for the ScottFree interpreter.

/*
UNP64 - generic Commodore 64 prg unpacker
(C) 2008-2019 iAN CooG/HVSC Crew^C64Intros
original source and idea: testrun.c, taken from exo20b7

Follows original disclaimer
*/

/*
 * Copyright (c) 2002 - 2008 Magnus Lind.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software, alter it and re-
 * distribute it freely for any non-commercial, non-profit purpose subject to
 * the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software in a
 *   product, an acknowledgment in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not
 *   be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any distribution.
 *
 *   4. The names of this software and/or it's copyright holders may not be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 */

#include "unp64.h"
//#include "log.h"
#include "exo_util.h"
#include "6502emu.h"

#include <stdlib.h>
#include <string.h>

/*****************************************************************************/

char appstr[80];
int iter = 0, parsepar = 1, debugprint = 0;

unpstr Unp;

/*****************************************************************************/
int desledge(unsigned int p, unsigned int prle, unsigned int startm,
    unsigned char *mem)
{
    if (mem[p] == mem[prle]) // starts with a rle?
    {
        p++;
        if (mem[p] == 0) // 16bit length?
        {
            if (mem[p + 3] == 0) {
                // {RLEbyte} 00 {hi} {lo} {byte}
                // hibyte is +1
                startm += ((mem[p + 1] - 1) << 8) | mem[p + 2];
            }
        } else {
            if (mem[p + 1] == 0) {
                // {RLEbyte} {lo} {byte}
                startm += mem[p];
            }
        }
        if (startm > 0x800) { // keep lower startm, if higher it's ok for me to keep
            // mem from $0800
            startm = 0x800;
        }
    }
    return startm;
}
/*****************************************************************************/
void printmsg(unsigned char *mem, int start, int num)
{
    int q, p;
    for (q = 0, p = start; q < num; q++, p++) {
        if (mem[p] == 0)
            break;
        appstr[q] = mem[p] & 0x7f;
        if (appstr[q] < 0x20)
            appstr[q] |= 0x40;
    }
    appstr[q] = 0;
    fprintf(stderr, "\"%s\"\n", appstr);
}

/* the Isepic fill pattern
        *=$200
        sei
        LDA #$34
        LDY #$00
        STA $01

        STY $FE
        LDA #$08
        STA $FF
loop
        TYA
        EOR $FF
        STA ($FE),Y
        TYA
        EOR #$01
        STA $0100,Y
        LDA #$00
        STA $FF00,Y
        INY
        BNE loop

        INC $FF
        BNE loop

        lda #$37
        sta $01
        jmp reset
*/
/*****************************************************************************/
void IseFill(unsigned char *oldb)
{
    unsigned short int wptr;
    unsigned char a, y;
    y = 0;
    a = 0x8;
    wptr = y | (a) << 8;

    while (wptr >> 8) {
        do {
            a = y;
            a ^= (wptr >> 8);
            oldb[wptr + y] = a;
            a = y;
            a ^= 1;
            oldb[0x100] = a;
            oldb[0xff00] = 0;
            y++;
        } while (y);
        wptr += 0x100;
    }
}
/*****************************************************************************/
void reinitUnp(void)
{
    Unp.IdFlag = 0;
    Unp.Forced = 0;
    Unp.StrMem = 0x800;
    Unp.RetAdr = 0x800;
    Unp.DepAdr = 0;
    Unp.EndAdr = 0x10000;
    Unp.RtAFrc = 0;
    Unp.WrMemF = 0;
    Unp.LfMemF = 0;
    Unp.ExoFnd = 0;
    Unp.ECAFlg = 0;
    Unp.fEndBf = 0;
    Unp.fEndAf = 0;
    Unp.fStrAf = 0;
    Unp.fStrBf = 0;
    Unp.Mon1st = 0;
}
/*****************************************************************************/
int xxOpenFile(FILE **hh, unpstr *Unpstring, int p)
{
    char *pp;
    char *nfmask = "%s not found\n";
    int retval = 0;
    FILE *h;

    h = fopen(appstr, "rb");
    if (h == NULL) {
        fprintf(stderr, nfmask, appstr);
    retry:
        strcat(appstr, ".prg");
        h = fopen(appstr, "rb");
    }
    if (h == NULL) {
        fprintf(stderr, nfmask, appstr);
        appstr[p] = 0;
        pp = strrchr(appstr, '.');
        if (pp)
            *pp = 0;
        h = fopen(appstr, "rb");
    }
    if (h == NULL) {
        if (strlen(appstr) == 17) {
            fprintf(stderr, nfmask, appstr);
            p = 16;
            appstr[p] = 0;
            goto retry;
        }
    }
    if (h == NULL) {
        fprintf(stderr, nfmask, appstr);
        fprintf(stderr, "Can't proceed onefiling\n");
        retval = 2;
    } else {
        fprintf(stderr, "Onefiling with %s\n", appstr);
    }
    *hh = h;
    return retval;
}
/*****************************************************************************/
int IsBasicRun1(int pc)
{
    if( pc==0xa7ae
      ||pc==0xa7ea
      ||pc==0xa7b1
      ||pc==0xa474
      ||pc==0xa533
      ||pc==0xa871
      ||pc==0xa888
      ||pc==0xa8bc
      )
        return 1;
    else
        return 0;
}
/*****************************************************************************/
int IsBasicRun2(int pc)
{
    if(IsBasicRun1(pc)
      ||((pc>=0xA57C)&&(pc<=0xA659)) /* Tokenise Input Buffer, happens we get here in some freezed programs, freezed after giving the sys from immediate mode :D */
      ||pc==0xa660
      ||pc==0xa68e
      )
        return 1;
    else
        return 0;
}
/*****************************************************************************/
char *normalizechar(char *app, unsigned char *m, int *ps)
{
    int i, s = *ps;
    memset(app, 0, 0x20);
    for(i=0;m[s]&&i<16;i++)
    {
        if( (m[s]=='<') ||
            (m[s]=='|') ||
            (m[s]=='>') ||
            (m[s]==' ') ||
            (m[s]=='?') ||
            (m[s]=='\\')||
            (m[s]=='/') ||
            (m[s]=='*') )
        {
            m[s] = '_';
        }
        app[i] = m[s++] & 0x7f;
    }
    *ps = s;
    strcat(app, ".prg");
    return app;
}
/*****************************************************************************/
// int main(int argc, char *argv[])
int unp64(uint8_t *compressed, size_t length, uint8_t *destination_buffer,
    size_t *final_length, char *settings[], int numsettings)
{
    struct cpu_ctx r[1];
    struct load_info info[1];
    char name[260] = { 0 }, forcedname[260] = { 0 };
    unsigned char mem[65536] = { 0 }, oldmem[65536] = { 0 },
    vector[0x20]=
    {0x31,0xEA,0x66,0xFE,0x47,0xFE,0x4A,0xF3,0x91,0xF2,0x0E,0xF2,0x50,0xF2,0x33,0xF3
    ,0x57,0xF1,0xCA,0xF1,0xED,0xF6,0x3E,0xF1,0x2F,0xF3,0x66,0xFE,0xA5,0xF4,0xED,0xF5
    },
    stack[0x100]=
    {0x33,0x38,0x39,0x31,0x31,0x00,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    ,0xFF,0xFF,0xFF,0xFF,0xFF,0x7D,0xEA,0x00,0x00,0x82,0x22,0x0E,0xBC,0x81,0x64,0xB8
    ,0x0C,0xBD,0xBA,0xB7,0xBC,0x03,0x00,0x46,0xE1,0xE9,0xA7,0xA7,0x79,0xA6,0x9C,0xE3
                  };
    int iter_max = ITERMAX;
    int q, p;

    memset(&Unp, 0, sizeof(Unp));
    reinitUnp();
    Unp.FStack = 1;
    Unp.mem = mem;
    Unp.r = r;
    Unp.name = name;
    Unp.info = info;

    p = 0;
    //    if(numsettings == 0)
    //    {
    // usage:
    //        strcpy(name,__DATE__);
    //        fprintf(stderr, "\nUNP64 v%s - Generic C64 prg unpacker - (C)2008-%s
    //        iAN CooG [%s]\n",VERSION,name+7,name); fprintf(stderr, "Based on
    //        Exomizer 2.0b7 sources by Magnus Lind.\n"
    //               "usage: UNP64 <packed.prg>{[,|@]Addr} [-parameters]\n\n"
    //               "-a       Save all mem instead of $0800-$ffff.\n"
    //               "-t$Addr  Truncate mem at Addr before saving. ZP Addr used as
    //               lo/hi pointer.\n"
    //               "-e$Addr  Specify entry point address in case SYS detection
    //               fails.\n"
    //               "-d$Addr  Unpacker address = Addr; default: first address <
    //               return address.\n"
    //               "-r$Addr  Return address from unpacker must be >= Addr.
    //               (default:$0800)\n"
    //               "-R$Addr  Return address from unpacker must be == Addr.\n"
    //               "-u       Clean unchanged memory.\n"
    //               "-l       Clean leftover packed data copied at end of
    //               memory.\n"
    //               "-s       Use a zero-filled stack on loading + SP=$FF, else
    //               like on C64.\n"
    //               "-fXX     Fill memory with byte XX instead of 0, before
    //               loading.\n"
    //               "-i       Identify only.       -c      Continue unpacking
    //               until error.\n"
    //               "-v       Verbose output.      -x      Trace for Debug.\n"
    //               "-B       Patch original Basic ROM at $a000, if prg is
    //               shorter.\n"
    //               "-K       Patch original Kernal ROM at $e000, if prg is
    //               shorter.\n"
    //               "-o Name  Force output filename as \"Name\"\n"
    //               "Default output filename is packed.prg.XXXX where XXXX is the
    //               hex JMP address.\n" "Use packed.prg,Addr to relocate loading
    //               at Addr instead of .prg loadaddress.\n" "Use packed.bin@Addr
    //               to load at Addr raw binaries without loadaddress.\n"
    //               );
    //        return 1;
    //    }

    if (numsettings != 0) {

        if (settings[0][0] == '-' && parsepar && settings[0][1] == 'f') {
            str_to_int(settings[p] + 2, (int *)&Unp.Filler);
            if (Unp.Filler) {
                memset(mem + (Unp.Filler >> 16), Unp.Filler & 0xff,
                    0x10000 - (Unp.Filler >> 16));
            }
            p++;
        }

        //        p=0;
        //        while((Unp.Recurs==0) && (p<numsettings))
        //        {
        //            if (settings[p][0]=='-'&&parsepar)
        //            {
        //                switch(settings[p][1])
        //                {
        //                    case '-':
        //                        parsepar=0;
        //                        break;
        //                    case 'i':
        //                        Unp.IdOnly=1;
        //                        break;
        //                    case 'f':
        //                        str_to_int(settings[p]+2, (int *)&Unp.Filler);
        //                        if(Unp.Filler)
        //                        {
        //                            memset(mem+(Unp.Filler>>16),Unp.Filler&0xff,0x10000-(Unp.Filler>>16));
        //                        }
        //                        break;
        //                    case 'v':
        //                        Unp.DebugP=1;
        //                        debugprint=Unp.DebugP;
        //                        break;
        //                    case 'o':
        //                        if (settings[p][2])
        //                            strncpy(forcedname,settings[p]+2,sizeof(forcedname));
        //                        else if (settings[p+1]&&settings[p+1][0])
        //                        {
        //                            p++;
        //                            strncpy(forcedname,settings[p],sizeof(forcedname));
        //                        }
        //                }
        //            }
        //        }
    }
////                    else
////                        goto usage;
//
//                    if(strlen(forcedname)>248)
//                        forcedname[248]=0;
//
//                }
//            }
//            else
//            {
//                if(!*name)
//                    strcpy((char*)name,settings[p]);
//            }
//            p++;
//        }
//    }
//    if(!*name)
//       goto usage;
looprecurse:
    /* init logging */
    //    LOG_INIT_CONSOLE(LOG_BRIEF);

    //    /*appl =*/ fixup_appl(argv[0]);

    info->basic_txt_start = 0x801;
    load_data(compressed, length, mem, info);

    //    if(Unp.IdOnly)
    //    {
    //        fprintf(stderr, "%s : ",name);
    //    }
    //    else
    //        fprintf(stderr, "\n");

    /* no start address from load */
    if (info->run == -1) {
        /* look for sys line */
        info->run = find_sys(mem + info->basic_txt_start, 0x9e);
    }

    Scanners(&Unp);
    if (Unp.IdFlag == 2)
        return 0;

    if ((Unp.Recurs == 0) && (numsettings > 0)) {
        //        p=0;
        while (p < numsettings) {
            if (settings && settings[p][0] == '-') {
                switch (settings[p][1]) {
                case '-':
                    p = numsettings;
                    break;
                case 'e':
                    str_to_int(settings[p] + 2, &Unp.Forced);
                    Unp.Forced &= 0xffff;
                    if (Unp.Forced < 0x1)
                        Unp.Forced = 0;
                    fprintf(stderr, "entry point forced to $%04x\n", Unp.Forced);
                    break;
                case 'a':
                    Unp.StrMem = 2;
                    Unp.EndAdr = 0x10001;
                    Unp.fEndAf = 0;
                    Unp.fStrAf = 0;
                    Unp.StrAdC = 0;
                    Unp.EndAdC = 0;
                    Unp.MonEnd = 0;
                    Unp.MonStr = 0;
                    fprintf(stderr, "save all mem from $0002 to $ffff\n");
                    break;
                case 'r':
                    str_to_int(settings[p] + 2, &Unp.RetAdr);
                    Unp.RetAdr &= 0xffff;
                    fprintf(stderr, "return >= $%04x\n", Unp.RetAdr);
                    break;
                case 'R':
                    str_to_int(settings[p] + 2, &Unp.RetAdr);
                    Unp.RetAdr &= 0xffff;
                    fprintf(stderr, "return == $%04x\n", Unp.RetAdr);
                    Unp.RtAFrc = 1;
                    break;
                case 'd':
                    str_to_int(settings[p] + 2, &Unp.DepAdr);
                    Unp.DepAdr &= 0xffff;
                    fprintf(stderr, "unpacker = $%04x\n", Unp.DepAdr);
                    break;
                case 't':
                    str_to_int(settings[p] + 2, &Unp.EndAdr);
                    Unp.EndAdr &= 0xffff;
                    if (Unp.EndAdr >= 0x100)
                        Unp.EndAdr++;
                    fprintf(stderr, "End Addr = $%04x\n", Unp.EndAdr);
                    break;
                case 'u':
                    Unp.WrMemF = 1;
                    fprintf(stderr, "Clean unwritten memory\n");
                    break;
                case 'l':
                    Unp.LfMemF = info->end;
                    fprintf(stderr, "Clean memory-end leftovers\n");
                    break;
                case 's':
                    Unp.FStack = 0;
                    break;
                case 'x':
                    //                    LOG_INIT_CONSOLE(LOG_DUMP);
                    break;
                case 'B':
                    break;
                case 'K':
                    break;
                case 'c':
                    Unp.Recurs++;
                    break;
                case 'm': // keep undocumented for now
                    str_to_int(settings[p] + 2, &iter_max);
                    fprintf(stderr, "new itermax=0x%x\r\n", iter_max);
                }
            }
            p++;
        }
    }

    if (Unp.IdOnly) {
        if (Unp.DepAdr == 0)
            fprintf(stderr, " (Unknown)\n");
        return 0;
    }
    if (Unp.WrMemF | Unp.LfMemF) {
        memcpy(oldmem, mem, sizeof(oldmem));
    }
    if (Unp.Forced)
        info->run = Unp.Forced;
    if (info->run == -1) {
        fprintf(stderr, "Error, can't find entry point.\n");
        return 0;
    }
    if (Unp.StrMem > Unp.RetAdr)
        Unp.StrMem = Unp.RetAdr;

    fprintf(stderr, "Entry point: $%04x\n", info->run);
    mem[0] = 0x60;
    r->cycles = 0;
    mem[1] = 0x37;
    if (((Unp.Forced >= 0xa000) && (Unp.Forced < 0xc000)) || (Unp.Forced >= 0xd000))
        mem[1] = 0x38;
    /* some packers rely on basic pointers already set */
    mem[0x2b] = info->basic_txt_start & 0xff;
    mem[0x2c] = info->basic_txt_start >> 8;
    if (info->basic_var_start == -1) {
        mem[0x2d] = info->end & 0xff;
        mem[0x2e] = info->end >> 8;
    } else {
        mem[0x2d] = info->basic_var_start & 0xff;
        mem[0x2e] = info->basic_var_start >> 8;
    }
    mem[0x2f] = mem[0x2d];
    mem[0x30] = mem[0x2e];
    mem[0x31] = mem[0x2d];
    mem[0x32] = mem[0x2e];
    mem[0xae] = info->end & 0xff;
    mem[0xaf] = info->end >> 8;
    /* CCS unpacker requires $39/$3a (current basic line number) set */
    mem[0x39] = mem[0x803];
    mem[0x3a] = mem[0x804];
    mem[0x52] = 0;
    mem[0x53] = 3;

    if (Unp.FStack) {
        memcpy(mem + 0x100, stack,
            sizeof(stack)); /* stack as found on clean start */
        r->sp = 0xf6; /* sys from immediate mode leaves $f6 in stackptr */
    } else {
        r->sp = 0xff;
    }

    if (info->start > (0x314 + sizeof(vector))) {
        /* some packers use values in irq pointers to decrypt themselves */
        memcpy(mem + 0x314, vector, sizeof(vector));
    }
    mem[0x200] = 0x8a;
    r->mem = mem;
    r->pc = info->run;

    r->flags = 0x20;
    r->a = 0;
    r->y = 0;
    if (info->run > 0x351) /* temporary for XIP */
    {
        r->x = 0;
    }

    fprintf(stderr, "pass1, find unpacker: ");
    iter = 0;
    while( (Unp.DepAdr?r->pc!=Unp.DepAdr:r->pc>=Unp.RetAdr) )
    {
        if( (((mem[1]&0x7) >= 6) && (r->pc >= 0xe000)) ||
            ((r->pc >= 0xa000) && (r->pc <= 0xbfff) && ((mem[1]&0x7) > 6) )
           )
        {
            /* some packer relies on regs set at return from CLRSCR */
            if((r->pc==0xe536)||
               (r->pc==0xe544)||
               (r->pc==0xff5b)||
              ((r->pc==0xffd2)&&(r->a==0x93))
              )
            {
                if(r->pc!=0xffd2)
                {
                    r->x=0x01;r->y=0x84;
                    if (r->pc == 0xff5b)
                        r->a = 0x97; /* actually depends on $d012 */
                    else
                        r->a = 0xd8;
                    r->flags &= ~(128 | 2);
                    r->flags |= (r->a == 0 ? 2 : 0) | (r->a & 128);
                }
                memset(mem + 0x400, 0x20, 1000);
            }
            /* intros */
            if ((r->pc == 0xffe4) || (r->pc == 0xf13e)) {
                static int flipspe4 = -1;
                static unsigned char fpressedchars[] = {
                    0x20, 0, 0x4e, 0, 3, 0, 0x5f, 0, 0x11, 00, 0x0d, 0, 0x31, 0
                };
                flipspe4++;
                if (flipspe4 > (sizeof(fpressedchars) / sizeof(*fpressedchars)))
                    flipspe4 = 0;
                r->a = fpressedchars[flipspe4];
                if (Unp.DebugP) {
                    fprintf(stderr, "\nJSR $FFE4-> char $%02x %s\n", r->a,
                        (r->a ? "pressed" : "released"));
                    fprintf(stderr, "pass1, find unpacker: ");
                }
                r->flags &= ~(128 | 2);
                r->flags |= (r->a == 0 ? 2 : 0) | (r->a & 128);
            }
            if (r->pc == 0xfd15) {
                r->a = 0x31;
                r->x = 0x30;
                r->y = 0xff;
            }
            if (r->pc == 0xfda3) {
                mem[0x01] = 0xe7;
                r->a = 0xd7;
                r->x = 0xff;
            }
            if (r->pc == 0xffbd) {
                mem[0xB7] = r->a;
                mem[0xBB] = r->x;
                mem[0xBC] = r->y;
            }
            if ((r->pc == 0xffd5) || (r->pc == 0xf4a2)) {
                if (Unp.DebugP) {
                    fprintf(stderr, "\nJSR $FFD5->loads: ");
                    q = mem[0xbb] | mem[0xbc] << 8;
                    for (p = 0; p < mem[0xb7]; p++)
                        fprintf(stderr, "%c", mem[q + p]);
                    fprintf(stderr, "\n");
                }
                break;
            }
            if (IsBasicRun1(r->pc)) {
                if (Unp.DebugP) {
                    fprintf(stderr, "\n$%04x -> forced %s\n", r->pc, "RUN");
                }
                info->run = find_sys(mem + info->basic_txt_start, 0x9e);
                if (info->run > 0) {
                    r->sp = 0xf6;
                    r->pc = info->run;
                } else {
                    mem[0] = 0x60;
                    r->pc = 0; /* force a RTS instead of executing ROM code */
                }

                // continue;
            } else {
                mem[0] = 0x60;
                r->pc = 0; /* force a RTS instead of executing ROM code */
            }
        }
        if (next_inst(r) == 1)
            return 0;
        iter++;
        if (iter == iter_max) {
            fprintf(stderr, "\nMax Iterations 0x%08x Reached, quitting...\n",
                iter_max);
            return 0;
        }

        if (Unp.ExoFnd && (Unp.EndAdr == 0x10000) && (r->pc >= 0x100) && (r->pc <= 0x200) && (Unp.StrMem != 2)) {
            Unp.EndAdr = r->mem[0xfe] + (r->mem[0xff] << 8);
            if ((Unp.ExoFnd & 0xff) == 0x30) { /* low byte of EndAdr, it's a lda $ff00,y */
                Unp.EndAdr = (Unp.ExoFnd >> 8) + (r->mem[0xff] << 8);
            } else if ((Unp.ExoFnd & 0xff) == 0x32) { /* add 1 */
                Unp.EndAdr = 1 + ((Unp.ExoFnd >> 8) + (r->mem[0xff] << 8));
            }

            if (Unp.EndAdr == 0)
                Unp.EndAdr = 0x10001;
            if (Unp.DebugP)
                fprintf(stderr, "\nexo endaddr=$%04x\n", Unp.EndAdr - 1);
        }
        if (Unp.fEndBf && (Unp.EndAdr == 0x10000) && (r->pc == Unp.DepAdr)) {
            Unp.EndAdr = r->mem[Unp.fEndBf] | r->mem[Unp.fEndBf + 1] << 8;
            Unp.EndAdr++;
            if (Unp.EndAdr == 0)
                Unp.EndAdr = 0x10001;
            Unp.fEndBf = 0;
        }
        if (Unp.fStrBf && (Unp.StrMem != 0x2) && (r->pc == Unp.DepAdr)) {
            Unp.StrMem = r->mem[Unp.fStrBf] | r->mem[Unp.fStrBf + 1] << 8;
            Unp.fStrBf = 0;
        }

        if (Unp.DebugP) {
            for (p = 0; p < 0x20; p += 2) {
                if (*(unsigned short int *)(mem + 0x314 + p) != *(unsigned short int *)(vector + p)) {
                    fprintf(stderr,
                        "Warning! Vector $%04x-$%04x changed from $%04x to $%04x\n",
                        0x314 + p, 0x314 + p + 1, *(unsigned short int *)(vector + p),
                        *(unsigned short int *)(mem + 0x314 + p));
                    fprintf(stderr, "pass1, find unpacker: ");
                    *(unsigned short int *)(vector + p) = *(unsigned short int *)(mem + 0x314 + p);
                }
            }
        }
    }
    fprintf(stderr, "$%04x\n", r->pc);
    if (Unp.DebugP)
        fprintf(stderr, "Iterations %d cycles %d\n", iter, r->cycles);

    fprintf(stderr, "pass2, return to mem: ");
    iter = 0;
    while (Unp.RtAFrc ? r->pc != Unp.RetAdr : r->pc < Unp.RetAdr) {
        if (Unp.MonEnd && r->pc == Unp.DepAdr) {
            if (Unp.DebugP)
                if (Unp.EndAdr == 0x10000)
                    fprintf(stderr, "\n");
            p = r->mem[Unp.MonEnd >> 16] | r->mem[Unp.MonEnd & 0xffff] << 8;
            if (p > (Unp.EndAdr & 0xffff)) {
                Unp.EndAdr = p;
                if (Unp.DebugP) {
                    if (Unp.fEndAf)
                        fprintf(stderr, "\r");
                    else
                        fprintf(stderr, "\n");
                    fprintf(stderr, "Monitored %s pointer $%04x+$%04x -> $%04x", "end",
                        Unp.MonEnd >> 16, Unp.MonEnd & 0xffff, Unp.EndAdr);
                }
            }
        }
        if (Unp.MonStr && r->pc == Unp.DepAdr) {
            p = r->mem[Unp.MonStr >> 16] | r->mem[Unp.MonStr & 0xffff] << 8;
            if (p > 0) {
                if (Unp.Mon1st == 0) {
                    Unp.StrMem = p;
                }
                Unp.Mon1st = Unp.StrMem;
                Unp.StrMem = (p < Unp.StrMem ? p : Unp.StrMem);
                if (Unp.DebugP) {
                    if (Unp.Mon1st != Unp.StrMem) {
                        fprintf(stderr, "\n");
                        fprintf(stderr, "Monitored %s pointer $%04x+$%04x -> $%04x",
                            "start", Unp.MonStr >> 16, Unp.MonStr & 0xffff, Unp.StrMem);
                    }
                }
            }
        }

        if (r->pc >= 0xe000) {
            if (((mem[1] & 0x7) >= 6) && ((mem[1] & 0x7) <= 7)) {

                if (Unp.DebugP) {
                    fprintf(stderr, "\n$%04x -> forced %s\n", r->pc, "RTS");
                }
                mem[0] = 0x60;
                r->pc = 0;
            }
        }
        if (next_inst(r) == 1)
            return 0;
        if ((mem[r->pc] == 0x40) && (Unp.RTIFrc == 1)) {
            if (next_inst(r) == 1)
                return 0;
            Unp.RetAdr = r->pc;
            Unp.RtAFrc = 1;
            if (Unp.RetAdr < Unp.StrMem)
                Unp.StrMem = 2;
            break;
        }
        iter++;
        if (iter == iter_max) {
            fprintf(stderr, "\nMax Iterations 0x%08x Reached, quitting...\n",
                iter_max);
            return 0;
        }
        if ((r->pc >= 0xa000) && (r->pc <= 0xbfff) && ((mem[1] & 0x7) == 7)) {
            if (IsBasicRun2(r->pc)) {
                if (Unp.DebugP) {
                    fprintf(stderr, "\n$%04x -> forced %s\n", r->pc, "RUN");
                }
                r->pc = 0xa7ae;
                break;
            } else {
                if (Unp.DebugP) {
                    fprintf(stderr, "\n$%04x -> forced %s\n", r->pc, "RTS");
                }
                mem[0] = 0x60;
                r->pc = 0;
            }
        }
        if (r->pc >= 0xe000) {
            if (((mem[1] & 0x7) >= 6) && ((mem[1] & 0x7) <= 7)) {
                if (r->pc == 0xffbd) {
                    mem[0xB7] = r->a;
                    mem[0xBB] = r->x;
                    mem[0xBC] = r->y;
                }
                if (r->pc == 0xffd5) {
                    if (Unp.DebugP) {
                        fprintf(stderr, "\nJSR $FFD5->loads: ");
                        q = mem[0xbb] | mem[0xbc] << 8;
                        for (p = 0; p < mem[0xb7]; p++)
                            fprintf(stderr, "%c", mem[q + p]);
                        fprintf(stderr, "\n");
                    }
                }

                /* return into IRQ handler, better stop here */
                if (((r->pc >= 0xea31) && (r->pc <= 0xeb76)) || (r->pc == 0xffd5) || (r->pc == 0xfce2)) {
                    if (Unp.DebugP) {
                        fprintf(stderr, "\n$%04x -> forced %s\n", r->pc, "exit");
                    }
                    break;
                }
                if (r->pc == 0xfda3) {
                    mem[0x01] = 0xe7;
                    r->a = 0xd7;
                    r->x = 0xff;
                }

                if (Unp.DebugP) {
                    fprintf(stderr, "\n$%04x -> forced %s\n", r->pc, "RTS");
                }
                mem[0] = 0x60;
                r->pc = 0;
            }
        }
    }
    if ((Unp.MonEnd | Unp.MonStr) && Unp.DebugP)
        fprintf(stderr, "\n");
    if (Unp.fEndAf && Unp.MonEnd) {
        Unp.EndAdC = mem[Unp.fEndAf] | mem[Unp.fEndAf + 1] << 8;
        if ((int)Unp.EndAdC > Unp.EndAdr)
            Unp.EndAdr = Unp.EndAdC;
        Unp.EndAdC = 0;
        Unp.fEndAf = 0;
    }
    if (Unp.fEndAf && (Unp.EndAdr == 0x10000)) {
        Unp.EndAdr = r->mem[Unp.fEndAf] | r->mem[Unp.fEndAf + 1] << 8;
        if (Unp.EndAdr == 0)
            Unp.EndAdr = 0x10000;
        else
            Unp.EndAdr++;
        Unp.fEndAf = 0;
    }
    if (Unp.fStrAf /*&&(Unp.StrMem==0x800)*/) {
        Unp.StrMem = r->mem[Unp.fStrAf] | r->mem[Unp.fStrAf + 1] << 8;
        Unp.StrMem++;
        Unp.fStrAf = 0;
    }

    if (Unp.ExoFnd && (Unp.StrMem != 2)) {
        Unp.StrMem = r->mem[0xfe] + (r->mem[0xff] << 8);
        if ((Unp.ExoFnd & 0xff) == 0x30) {
            Unp.StrMem += r->y;
        } else if ((Unp.ExoFnd & 0xff) == 0x32) {
            Unp.StrMem += r->y + 1;
        }

        if (Unp.DebugP)
            fprintf(stderr, "\nexo startaddr=$%04x\n", Unp.StrMem);
    }
    if (r->pc == 0xfce2) {
        if ((*(unsigned int *)(mem + 0x8004) == 0x38cdc2c3) && (mem[0x8008] == 0x30)) {
            fprintf(stderr, "CBM80($%04x)->", r->pc);
            r->pc = r->mem[0x8000] + (r->mem[0x8001] << 8);
        }
    } else if (r->pc == 0xa7ae) {
        fprintf(stderr, "($%04x)->", r->pc);
        info->basic_txt_start = mem[0x2b] | mem[0x2c] << 8;
        if (info->basic_txt_start != 0x801)
            fprintf(stderr, "(new basic start $%04x)->", info->basic_txt_start);
        else {
            info->run = find_sys(mem + info->basic_txt_start, 0x9e);
            if (info->run > 0)
                r->pc = info->run;
        }
    }
    if (r->pc == 0xa7ae)
        fprintf(stderr, "RUN\n");
    else
        fprintf(stderr, "$%04x\n", r->pc);
    if (Unp.DebugP)
        fprintf(stderr, "Iterations %d cycles %d\n", iter, r->cycles);

    if (Unp.WrMemF) {
        Unp.WrMemF = 0;
        for (p = 0x800; p < 0x10000; p += 4) {
            if (*(unsigned int *)(oldmem + p) == *(unsigned int *)(mem + p)) {
                *(unsigned int *)(mem + p) = 0;
                Unp.WrMemF = 1;
            }
        }
        /* clean also the $fd30 table copy in RAM */
        if (memcmp(mem + 0xfd30, vector, sizeof(vector)) == 0) {
            memset(mem + 0xfd30, 0, sizeof(vector));
        }
    }
    if (Unp.LfMemF) {
        for (p = 0xffff; p > 0x0800; p--) {
            if (oldmem[--Unp.LfMemF] == mem[p])
                mem[p] = 0x0;
            else {
                if (p < 0xffff)
                    fprintf(stderr, "cleaned from $%04x to $ffff\n", p + 1);
                else
                    Unp.LfMemF = 0 | Unp.ECAFlg;
                break;
            }
        }
    }
    if (*forcedname) {
        strcpy(name, forcedname);
    } else {
        if (strlen(name) > 248) /* dirty hack in case name is REALLY long */
            name[248] = 0;
        sprintf(name + strlen(name), ".%04x%s", r->pc,
            (Unp.WrMemF | Unp.LfMemF ? ".clean" : ""));
    }
    //    h=fopen(name,"wb");
    /*  endadr is set to a ZP location? then use it as a pointer
      todo: use fEndAf instead, it can be used for any location, not only ZP. */
    if (Unp.EndAdr && (Unp.EndAdr < 0x100)) {
        p = (mem[Unp.EndAdr] | mem[Unp.EndAdr + 1] << 8) & 0xffff;
        Unp.EndAdr = p;
    }
    if (Unp.ECAFlg && (Unp.StrMem != 2)) /* checkme */
    {
        if (Unp.EndAdr >= ((Unp.ECAFlg >> 16) & 0xffff)) {
            /* most of the times transfers $2000 byte from $d000-efff to $e000-ffff
         but there are exceptions */
            if (Unp.DebugP) {
                fprintf(stderr, "ECA: endmem adjusted from $%04x to $%04x\n",
                    Unp.EndAdr, (Unp.EndAdr + 0x1000));
                if (Unp.LfMemF) {
                    fprintf(stderr, "mem $%04x-$%04x cleaned\n",
                        ((Unp.ECAFlg >> 16) & 0xffff),
                        ((Unp.ECAFlg >> 16) & 0xffff) + 0x0fff);
                }
            }
            if (Unp.LfMemF)
                memset(mem + ((Unp.ECAFlg >> 16) & 0xffff), 0, 0x1000);
            Unp.EndAdr += 0x1000;
        }
    }
    if (Unp.EndAdr <= 0)
        Unp.EndAdr = 0x10000;
    if (Unp.EndAdr > 0x10000)
        Unp.EndAdr = 0x10000;
    if (Unp.EndAdr < Unp.StrMem)
        Unp.EndAdr = 0x10000;

    if (Unp.EndAdC & 0xffff) {
        Unp.EndAdr += (Unp.EndAdC & 0xffff);
        Unp.EndAdr &= 0xffff;
    }
    if (Unp.EndAdC & EA_USE_A) {
        Unp.EndAdr += r->a;
        Unp.EndAdr &= 0xffff;
    }
    if (Unp.EndAdC & EA_USE_X) {
        Unp.EndAdr += r->x;
        Unp.EndAdr &= 0xffff;
    }
    if (Unp.EndAdC & EA_USE_Y) {
        Unp.EndAdr += r->y;
        Unp.EndAdr &= 0xffff;
    }
    if (Unp.StrAdC & 0xffff) {
        Unp.StrMem += (Unp.StrAdC & 0xffff);
        Unp.StrMem &= 0xffff;
        /* only if ea_addff, no reg involved */
        if (((Unp.StrAdC & 0xffff0000) == EA_ADDFF) && ((Unp.StrMem & 0xff) == 0)) {
            Unp.StrMem += 0x100;
            Unp.StrMem &= 0xffff;
            if (Unp.DebugP) {
                fprintf(stderr, STRMEMAJ, Unp.StrMem, "(no reg)");
            }
        }
    }
    if (Unp.StrAdC & EA_USE_A) {
        Unp.StrMem += r->a;
        Unp.StrMem &= 0xffff;
        if (Unp.StrAdC & EA_ADDFF) {
            if ((Unp.StrMem & 0xff) == 0xff)
                Unp.StrMem++;
            if (r->a == 0) {
                Unp.StrMem += 0x100;
                Unp.StrMem &= 0xffff;
                //                if(Unp.DebugP)
                //                {
                //                    fprintf(stderr,STRMEMAJ,Unp.StrMem,"(+A)");
                //                }
            }
        }
    }
    if (Unp.StrAdC & EA_USE_X) {
        Unp.StrMem += r->x;
        Unp.StrMem &= 0xffff;
        if (Unp.StrAdC & EA_ADDFF) {
            if ((Unp.StrMem & 0xff) == 0xff)
                Unp.StrMem++;
            if (r->x == 0) {
                Unp.StrMem += 0x100;
                Unp.StrMem &= 0xffff;
                //                if(Unp.DebugP)
                //                {
                //                    fprintf(stderr,STRMEMAJ,Unp.StrMem,"(+X)");
                //                }
            }
        }
    }
    if (Unp.StrAdC & EA_USE_Y) {
        Unp.StrMem += r->y;
        Unp.StrMem &= 0xffff;
        if (Unp.StrAdC & EA_ADDFF) {
            if ((Unp.StrMem & 0xff) == 0xff)
                Unp.StrMem++;
            if (r->y == 0) {
                Unp.StrMem += 0x100;
                Unp.StrMem &= 0xffff;
                //                if(Unp.DebugP)
                //                {
                //                    fprintf(stderr,STRMEMAJ,Unp.StrMem,"(+Y)");
                //                }
            }
        }
    }
    if (Unp.EndAdr <= 0)
        Unp.EndAdr = 0x10000;
    if (Unp.EndAdr > 0x10000)
        Unp.EndAdr = 0x10000;
    if (Unp.EndAdr < Unp.StrMem)
        Unp.EndAdr = 0x10000;

    mem[Unp.StrMem - 2] = Unp.StrMem & 0xff;
    mem[Unp.StrMem - 1] = Unp.StrMem >> 8;
    //    fwrite(mem+(Unp.StrMem-2),Unp.EndAdr-Unp.StrMem+2,1,h);
    memcpy(destination_buffer, mem + (Unp.StrMem - 2),
        Unp.EndAdr - Unp.StrMem + 2);
    *final_length = Unp.EndAdr - Unp.StrMem + 2;
    fprintf(stderr, "saved $%04x-$%04x as %s\n", Unp.StrMem,
        (Unp.EndAdr - 1) & 0xffff, name);
    //    fclose(h);
    if (Unp.Recurs) {
        if (++Unp.Recurs > RECUMAX)
            return 1;
        reinitUnp();
        goto looprecurse;
    }
    return 1;
}
