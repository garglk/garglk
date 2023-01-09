/* ECA compacker */
#include "../unp64.h"
void Scn_ECA(unpstr *Unp) {
  unsigned char *mem;
  int q, p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    // for(p=0x810;p<0x830;p+=0x4)
    for (p = 0x80d; p < 0x830; p += 0x1) {
      if ((*(unsigned int *)(mem + p + 0x08) == (unsigned int)0x2D9D0032 + p) &&
          (*(unsigned int *)(mem + p + 0x3a) == 0x2a2a2a2a) &&
          (*(unsigned int *)(mem + p + 0x0c) == 0xF710CA00)) {
        if (((*(unsigned int *)(mem + p + 0x00) & 0xf4fff000) == 0x8434A000) &&
            (*(unsigned int *)(mem + p + 0x04) == 0xBD05A201)) {
          Unp->Forced = p + 1;
        } else if (((*(unsigned int *)(mem + p + 0x00) & 0xffffff00) ==
                    0x04A27800) &&
                   (*(unsigned int *)(mem + p + 0x04) == 0xBDE80186)) {
          Unp->Forced = p + 1;
        } else if (((*(unsigned int *)(mem + p - 0x03) & 0xffffff00) ==
                    0x04A27800) &&
                   (*(unsigned int *)(mem + p + 0x04) == 0xBDE80186)) {
          Unp->Forced = p - 2;
        } else if (*(unsigned int *)(mem + p - 0x03) == 0x8D00a978) {
          Unp->Forced = p - 2;
        }
      }
      if (!Unp->Forced) {
        if ((*(unsigned int *)(mem + p + 0x3a) == 0x2a2a2a2a) &&
            (*(unsigned int *)(mem + p + 0x02) == 0x8534A978) &&
            (mem[p - 3] == 0xa0)) {
          Unp->Forced = p - 3;
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
      if (!Unp->Forced) { /* FDT */
        if ((*(unsigned int *)(mem + p + 0x3a) == 0x2a2a2a2a) &&
            (*(unsigned int *)(mem + p + 0x03) == 0x8604A278) &&
            (*(unsigned int *)(mem + p + 0x0a) == 0x2D950842)) {
          Unp->Forced = p + 3;
        }
      }
      if (!Unp->Forced) {
        /* decibel hacks */
        if ((*(unsigned int *)(mem + p + 0x3a) == 0x2a2a2a2a) &&
            (*(unsigned int *)(mem + p + 0x00) == 0x9D085EBD) &&
            (*(unsigned int *)(mem + p - 0x06) == 0x018534A9)) {
          Unp->Forced = p - 0x6;
        }
      }
      if (Unp->Forced) {
        for (q = 0xd6; q < 0xde; q++) {
          if (mem[p + q] == 0x20) {
            if ((*(unsigned short int *)(mem + p + q + 1) == 0xa659) ||
                (*(unsigned short int *)(mem + p + q + 1) == 0xff81) ||
                (*(unsigned short int *)(mem + p + q + 1) == 0xe3bf) ||
                (*(unsigned short int *)(mem + p + q + 1) == 0xe5a0) ||
                (*(unsigned short int *)(mem + p + q + 1) == 0xe518)) {
              mem[p + q] = 0x2c;
              q += 2;
              continue;
            } else {
              Unp->RetAdr = mem[p + q + 1] | mem[p + q + 2] << 8;
              break;
            }
          }
          if (mem[p + q] == 0x4c) {
            Unp->RetAdr = mem[p + q + 1] | mem[p + q + 2] << 8;
            break;
          }
        }
        Unp->DepAdr = mem[p + 0x30] | mem[p + 0x31] << 8;
        // some use $2d, some $ae
        for (q = 0xed; q < 0x108; q++) {
          if (*(unsigned int *)(mem + p + q) == 0xA518F7D0) {
            Unp->EndAdr = mem[p + q + 4];
            // if(Unp->DebugP)
            // printf("EndAdr from $%02x\n",Unp->EndAdr);
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
        Unp->StrMem = mem[p + 0x32] | mem[p + 0x33] << 8;
        PrintInfo(Unp, _I_ECA);
        for (q = 0xcd; q < 0xd0; q++) {
          if ((*(unsigned int *)(mem + p + q) & 0xffff00ff) == 0xa9010020) {
            Unp->ECAFlg = mem[p + q + 1] | mem[p + q + 2] << 8;
            for (q = 0x110; q < 0x11f; q++) {
              if ((*(unsigned int *)(mem + p + q) == 0x99EF00B9) &&
                  (mem[p + q + 0x12] == 0xc9)) {
                Unp->ECAFlg |= (mem[p + q + 0x13] - 0xf) << 24;
                break;
              }
            }
            if (Unp->DebugP)
              fprintf(stderr,
                      "ECA reloc active at $%04x, from $%04x-$efff to "
                      "$%04x-$ffff (use -l)\n",
                      Unp->ECAFlg & 0xffff, (Unp->ECAFlg >> 16) & 0xffff,
                      0x1000 + ((Unp->ECAFlg >> 16) & 0xffff));
            break;
          }
        }
        /* radwar hack has a BRK here, fffe/f used as IRQ/BRK vector */
        if (mem[0x8e1] == 0) {
          if (Unp->DebugP)
            fprintf(stderr, "RWE BRK Hack patched\n");
          mem[0x8e1] = 0x6c;
          mem[0x8e2] = 0xfe;
          mem[0x8e3] = 0xff;
        }
        break;
      }
    }
    if (Unp->DepAdr) {
      Unp->IdFlag = 1;
      return;
    }
  }
  /* old packer, many old 1985 warez used this */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x81b) == 0x018534A9) &&
        (*(unsigned int *)(mem + 0x822) == 0xAFC600A0) &&
        (*(unsigned int *)(mem + 0x826) == 0xB1082DCE) &&
        (*(unsigned int *)(mem + 0x85b) == 0x2A2A2A2A)) {
      Unp->Forced = 0x81b;
      Unp->DepAdr = 0x100;
      Unp->StrMem = mem[0x853] | mem[0x854] << 8;
      Unp->EndAdr = mem[0x895];
      Unp->RetAdr = mem[0x885] | mem[0x886] << 8;
      PrintInfo(Unp, _I_ECAOLD);
      Unp->IdFlag = 1;
      return;
    }
  }
}
