/* Cruel Crunch/Oneway and variants */
#include "../unp64.h"
void Scn_Cruel(unpstr *Unp) {
  unsigned char *mem;
  int q, p, strtmp = 0;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    if (mem[0x810] == 0xb9 &&
        ((*(unsigned int *)(mem + 0x813) & 0xfffffeff) == 0xC800FA99) &&
        (*(unsigned short int *)(mem + 0x818) == 0x4CF7)) {
      if (mem[0x814] == 0xFA) {
        p = mem[0x811] | mem[0x812] << 8;
        if (*(unsigned int *)(mem + p + 9) == 0xC8071C99) {
          Unp->EndAdr = mem[p + 2] | mem[p + 3] << 8;
          Unp->DepAdr = 0x100;
          if (Unp->info->run == -1)
            Unp->Forced = 0x80b;
          Unp->fStrAf = 0xfc;
          q = mem[p + 7] | mem[p + 8] << 8;
          if ((mem[q + 0x8e] == 0xc6) && (mem[q + 0x8f] == 0x01) &&
              (mem[q + 0x93] == 0xe6) && (mem[q + 0x94] == 0x01)) {
            mem[q + 0x90] = 0x2c;
          }
          /* retadr is not always at the same addr, but at least can't
             be anything < $07e8
          */
          // Unp->RetAdr=0x7e8;
          q = mem[p + 7] | mem[p + 8] << 8;
          if (mem[q + 0x3c] == 0x4c) {
            /* v2.2/dynamix, v2.5/cross, v2.5/crest */
            strtmp = *(unsigned short int *)(mem + q + 0x3d);
            PrintInfo(Unp, _I_CRUEL22);
          } else if (mem[q + 0x4a] == 0x4c) {
            strtmp = *(unsigned short int *)(mem + q + 0x4b);
            PrintInfo(Unp, _I_CRUELFAST2);
          } else if (mem[q + 0x3f] == 0x4c) {
            /* v2.2/oneway+scs, also hacked as cruel 2mhz 1.0 */
            strtmp = *(unsigned short int *)(mem + q + 0x40);
            PrintInfo(Unp, _I_CRUEL2MHZ);
          } else {
            /* todo: determine real retadr, for now a default seems ok */
            strtmp = 0;
            PrintInfo(Unp, _I_CRUEL_X);
          }
          if (strtmp) {
            if (strtmp >= Unp->RetAdr) {
              Unp->RetAdr = strtmp;
            } else { /* now search it... variable code here */
              strtmp += p - *(unsigned short int *)(mem + 0x814);
              for (q = strtmp; q < Unp->info->end; q++) {
                if ((mem[q] == 0xa9) || (mem[q] == 0x85)) {
                  q++;
                  continue;
                }
                if (mem[q] == 0x8d) {
                  q += 2;
                  continue;
                }
                if (mem[q] == 0x4c) {
                  Unp->RetAdr = *(unsigned short int *)(mem + q + 1);
                  ;
                  break;
                }
              }
            }
          }
        }
      } else if (mem[0x814] == 0xFB) {
        /* not Cruel2 but MSCRUNCH by Marco/Taboo
           v1.0 works only with some old AR cart (unless patched ;)
           v1.5 is infact more common
        */
        p = mem[0x811] | mem[0x812] << 8;
        if (*(unsigned int *)(mem + p + 7) == 0xC8071C99) {
          Unp->EndAdr = mem[p + 3] | mem[p + 4] << 8;
          Unp->DepAdr = 0x100;
          Unp->Forced = 0x80b;
          Unp->fStrAf = 0xfe;
          if ((mem[p + 0x93] == 0x4c) && (mem[p + 0xa1] == 0x4c)) {
            Unp->RetAdr = mem[p + 0xa2] | mem[p + 0xa3] << 8;
            PrintInfo(Unp, _I_CRUELMS15);
          } else if ((mem[p + 0x8c] == 0x4c) && (mem[p + 0x94] == 0x4c)) {
            Unp->RetAdr = mem[p + 0x95] | mem[p + 0x96] << 8;
            PrintInfo(Unp, _I_CRUELMS10);
          } else if ((mem[p + 0x20] == 0x4c) && (mem[p + 0x28] == 0x4c)) {
            Unp->RetAdr = mem[p + 0x29] | mem[p + 0x2a] << 8;
            PrintInfo(Unp, _I_CRUELMS1X);
          } else {
            PrintInfo(Unp, _I_CRUELMS_U);
          }
        }
      }
    }
    if (Unp->DepAdr) {
      Unp->IdFlag = 1;
      return;
    }
  }
  /* MSCRUNCH 1.5 hack by Anubis */
  if (Unp->DepAdr == 0) {
    if (mem[0x819] == 0x4c) {
      p = mem[0x81a] | mem[0x81b] << 8;
      if ((mem[p] == 0xa9) && (mem[p + 0x0f] == 0x30) &&
          (*(unsigned int *)(mem + p + 0x13) == 0xCA04009D) &&
          (*(unsigned int *)(mem + p + 0x38) == 0x01084C01)) {
        q = mem[p + 0x1f] | mem[p + 0x20] << 8;
        if (*(unsigned int *)(mem + q + 7) == 0xC8071C99) {
          Unp->EndAdr = mem[q + 3] | mem[q + 4] << 8;
          Unp->DepAdr = 0x100;
          if (Unp->info->run == -1)
            Unp->Forced = 0x819;
          Unp->fStrAf = 0xfe;
          Unp->RetAdr = mem[q + 0xa2] | mem[q + 0xa3] << 8;
          PrintInfo(Unp, _I_CRUMSABS);
        }
      }
    }
  }
  /* fast cruel 4.x */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x80b) == 0xE67800A0) &&
        (*(unsigned int *)(mem + 0x813) == 0xC8034099) &&
        ((*(unsigned int *)(mem + 0x818) == 0x03404cF7) ||
         (*(unsigned int *)(mem + 0x818) == 0x03b34cF7) ||
         (*(unsigned int *)(mem + 0x818) == 0x03db4cF7))) {
      p = mem[0x811] | mem[0x812] << 8;
      if (*(unsigned int *)(mem + p) == 0xa75801c6) {
        p += 0x45;
        q = mem[p] | mem[p + 1] << 8;
        Unp->EndAdr = mem[q + 2] | mem[q + 3] << 8;
        Unp->DepAdr = 0x340;
        Unp->Forced = 0x80b;
        Unp->fStrAf = 0xfc;
        PrintInfo(Unp, _I_CRUELFAST4);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  /* Cruel 2.0 / (BB) packer header */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x837) == 0x9D0845BD) &&
        (*(unsigned int *)(mem + 0x84f) == 0xE808039D) &&
        (*(unsigned int *)(mem + 0x83b) == 0xC9E803B7)) {
      Unp->DepAdr = mem[0x843] | mem[0x844] << 8;
      if (Unp->info->run == -1)
        Unp->Forced = 0x80d;
      Unp->RetAdr = mem[0x868] | mem[0x869] << 8;
      Unp->EndAdr = Unp->info->end - 0x90;
      Unp->StrMem = 0x801;
      PrintInfo(Unp, _I_CRUEL_HDR);
      if (Unp->DebugP)
        printmsg(mem, 0x86b, 40);
      Unp->IdFlag = 1;
      return;
    }
    if ((*(unsigned int *)(mem + 0x845) == 0x03E04CF2) &&
        (*(unsigned int *)(mem + 0x852) == 0x9D0893BD) &&
        (*(unsigned int *)(mem + 0x856) == 0xD0E80803)) {
      Unp->DepAdr = mem[0x847] | mem[0x848] << 8;
      if (Unp->info->run == -1)
        Unp->Forced = 0x80d;
      Unp->RetAdr = mem[0x869] | mem[0x86a] << 8;
      Unp->EndAdr = Unp->info->end - 0x90;
      Unp->StrMem = 0x801;
      PrintInfo(Unp, _I_CRUEL_H22);
      if (Unp->DebugP)
        printmsg(mem, 0x86b, 40);
      Unp->IdFlag = 1;
      return;
    }
    if ((*(unsigned int *)(mem + 0x841) == 0x03B74CF5) &&
        (*(unsigned int *)(mem + 0x84c) == 0x9D089BBD) &&
        (*(unsigned int *)(mem + 0x850) == 0xD0E8080B)) {
      Unp->DepAdr = mem[0x843] | mem[0x844] << 8;
      if (Unp->info->run == -1) {
        Unp->Forced = 0x811;
      } else {
        mem[0x808] = '5'; /* just to be safe, change sys for next layer */
        mem[0x809] = '9'; /* this hdr leaves it as sys2065 */
      }
      Unp->RetAdr = mem[0x868] | mem[0x869] << 8; /* fixed $080b */
      Unp->EndAdr = Unp->info->end - 0x90;
      Unp->StrMem = 0x801;
      PrintInfo(Unp, _I_CRUEL_H20);
      if (Unp->DebugP)
        printmsg(mem, 0x86b, 40);
      Unp->IdFlag = 1;
      return;
    }
    /* this is a totally useless header, cheers TCOM! */
    if ((*(unsigned int *)(mem + 0x80b) == 0x1BB900A0) &&
        (*(unsigned int *)(mem + 0x80f) == 0x03B79908) &&
        (*(unsigned int *)(mem + 0x823) == 0x039D0840)) {
      Unp->DepAdr = mem[0x819] | mem[0x81a] << 8;
      if (Unp->info->run == -1)
        Unp->Forced = 0x80b;
      Unp->RetAdr = mem[0x83e] | mem[0x83f] << 8;
      Unp->EndAdr = Unp->info->end - 0x3d;
      Unp->StrMem = 0x801;
      PrintInfo(Unp, _I_CRUEL_HTC);
      Unp->IdFlag = 1;
      return;
    }
  }
  /* Cruel 2.0 / (BB) packer sysless */
  if (Unp->DepAdr == 0) {
    if ((((*(unsigned int *)(mem + 0x80b)) & 0x0000ffff) == 0x000000A0) &&
        (*(unsigned int *)(mem + 0x817) == 0xC800CB99) &&
        (*(unsigned int *)(mem + 0x81b) == 0x004CF7D0) && mem[0x81f] == 1) {
      p = mem[0x815] | mem[0x816] << 8;
      p += 0x31;
      if ((mem[p + 4] == 0xb9) &&
          (*(unsigned int *)(mem + p + 7) == 0xC8072099)) {
        Unp->Forced = 0x80b;
        Unp->DepAdr = 0x100;
        Unp->EndAdr = mem[p] | mem[p + 1] << 8;
        Unp->fStrAf = 0xfc;
        /* patch: some version contain a zp cleaner sub at $01a2 */
        if ((*(unsigned int *)(mem + p + 0xa6) == 0x00A9CBA2) &&
            (*(unsigned int *)(mem + p + 0xaa) == 0xD0E80095)) {
          mem[p + 0xa6] = 0x60;
        }
        /* patch: some version expects $01==#$34 already set from the header */
        if (*(unsigned int *)(mem + 0x811) == 0xb9eaeaea) {
          mem[0x811] = 0xe6;
          mem[0x812] = 0x01;
        }
        q = mem[p + 5] | mem[p + 6] << 8;
        Unp->RetAdr = 0x7e8;
        if (mem[q + 0x6c] == 0x4c)
          Unp->RetAdr = mem[q + 0x6d] | mem[q + 0x6e] << 8;
        PrintInfo(Unp, _I_CRUEL20);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  /* Cruel 2.1 / STA */
  if (Unp->DepAdr == 0) {
    if (mem[0x80b] == 0xa0 && (*(unsigned int *)(mem + 0x817) == 0xC800CB99) &&
        (*(unsigned int *)(mem + 0x81b) == 0x004CF7D0) && mem[0x81f] == 1) {
      p = mem[0x815] | mem[0x816] << 8;
      p += 0x31;
      if ((mem[p + 6] == 0xb9) &&
          (*(unsigned int *)(mem + p + 9) == 0xC8072099)) {
        Unp->Forced = 0x80b;
        Unp->DepAdr = 0x100;
        Unp->EndAdr = mem[p] | mem[p + 1] << 8;
        Unp->fStrAf = 0xfc;
        q = mem[p + 7] | mem[p + 8] << 8;
        Unp->RetAdr = 0x7e8;
        if (mem[q + 0x6c] == 0x4c)
          Unp->RetAdr = mem[q + 0x6d] | mem[q + 0x6e] << 8;
        PrintInfo(Unp, _I_CRUEL21);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  /* unknown cruel, jmp $00e9, found in Illusion/Random warez */
  if (Unp->DepAdr == 0) {
    if (mem[0x810] == 0xb9 && (*(unsigned int *)(mem + 0x813) == 0xC800e999) &&
        (*(unsigned int *)(mem + 0x818) == 0x00e94CF7)) {
      p = mem[0x811] | mem[0x812] << 8;
      q = p - 0xed;
      if ((*(unsigned int *)(mem + p) == 0x13F01284) &&
          (*(unsigned int *)(mem + q) == 0xA9C8C8C8)) {
        Unp->DepAdr = 0xe9;
        Unp->EndAdr = mem[p + 0x13] | mem[p + 0x14] << 8;
        Unp->RetAdr = mem[q + 0x38] | mem[q + 0x39] << 8;
        if (Unp->info->run == -1)
          Unp->Forced = 0x80b;
        Unp->fStrAf = 0xfc;
        PrintInfo(Unp, _I_CRUEL_ILS);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  if (Unp->DepAdr == 0) {
    if (mem[0x810] == 0xb9 && (*(unsigned int *)(mem + 0x813) == 0xC800ed99) &&
        (*(unsigned int *)(mem + 0x818) == 0x01004CF7)) {
      p = mem[0x811] | mem[0x812] << 8;
      q = p - 0xed;
      if ((*(unsigned int *)(mem + p) == 0x01C60888) &&
          (*(unsigned int *)(mem + q) == 0xA9C8C8C8)) {
        Unp->DepAdr = 0x100;
        Unp->EndAdr = mem[p + 0x0f] | mem[p + 0x10] << 8;
        Unp->RetAdr = mem[q + 0x38] | mem[q + 0x39] << 8;
        if (Unp->info->run == -1)
          Unp->Forced = 0x80b;
        Unp->fStrAf = 0xfc;
        PrintInfo(Unp, _I_CRUEL_RND);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  /* cruel 1.2 / unknown 2059 */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x80b) == 0xE67800A0) &&
        (*(unsigned int *)(mem + 0x80f) == 0x0803B901) &&
        (*(unsigned int *)(mem + 0x813) == 0xC800E399) &&
        (*(unsigned int *)(mem + 0x817) == 0x004CF7D0) &&
        (*(unsigned int *)(mem + 0x90b) == 0xC068FEC6)) {
      Unp->DepAdr = 0x100;
      Unp->Forced = 0x80b;
      Unp->RetAdr = mem[0x91c] | mem[0x91d] << 8;
      Unp->EndAdr = 0x2d;
      Unp->fStrAf = 0xfc;
      PrintInfo(Unp, _I_CRUEL12);
      Unp->IdFlag = 1;
      return;
    }
    /* this was found in Agile and S451 cracks, Galleon's "Cruel+Search"
       it's actually the real v1.0
    */
    if ((*(unsigned int *)(mem + 0x80b) == 0xE67800A0) &&
        (*(unsigned int *)(mem + 0x80f) == 0x0803B901) &&
        (*(unsigned int *)(mem + 0x813) == 0xC800E399) &&
        (*(unsigned int *)(mem + 0x8c5) == 0x011D4C04) &&
        (*(unsigned int *)(mem + 0x90b) == 0xB1486018)) {
      Unp->DepAdr = 0x100;
      Unp->Forced = 0x80b;
      Unp->RetAdr = mem[0x92d] | mem[0x92e] << 8;
      Unp->EndAdr = 0x2d;
      Unp->fStrAf = 0xfc;
      PrintInfo(Unp, _I_CRUEL10);
      Unp->IdFlag = 1;
      return;
    }
    if ((*(unsigned int *)(mem + 0x80b) == 0xE67800A0) &&
        (*(unsigned int *)(mem + 0x80f) == 0x0803B901) &&
        (*(unsigned int *)(mem + 0x813) == 0xC800E399) &&
        (*(unsigned int *)(mem + 0x8b7) == 0x011D4C04) &&
        (*(unsigned int *)(mem + 0x8fc) == 0xB1486018)) {
      Unp->DepAdr = 0x100;
      Unp->Forced = 0x80b;
      Unp->RetAdr = mem[0x91e] | mem[0x91f] << 8;
      Unp->EndAdr = 0x2d;
      Unp->fStrAf = 0xfc;
      PrintInfo(Unp, _I_CRUEL14);
      Unp->IdFlag = 1;
      return;
    }
  }

  /* TKC "proggy crueler 2.3" (and 2.5) */
  if (Unp->DepAdr == 0) {
    if ((mem[0x810] == 0xb9) && (mem[0x819] == 0xa9) &&
        (*(unsigned int *)(mem + 0x813) == 0xC800fa99) &&
        (*(unsigned int *)(mem + 0x822) == 0x4CAF86AE)) {
      p = mem[0x811] | mem[0x812] << 8;
      q = p - 0x100;

      if ((*(unsigned int *)(mem + p + 0x0c) == 0x20F7D0C8) &&
          (*(unsigned int *)(mem + q) == 0xA9C8C8C8)) {
        Unp->DepAdr = 0x100;
        Unp->EndAdr = mem[p + 0x02] | mem[p + 0x03] << 8;
        Unp->RetAdr = mem[q + 0x3f] | mem[q + 0x40] << 8;
        if (Unp->info->run == -1)
          Unp->Forced = 0x80b;
        Unp->fStrAf = 0xfc;
        PrintInfo(Unp, _I_CRUEL_TKC);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
}
