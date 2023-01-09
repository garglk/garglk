/* TBC Multicompactor */
#include "../unp64.h"
void Scn_TBCMultiComp(unpstr *Unp) {
  unsigned char *mem;
  int p = 0, q = 0, strtmp;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    if (((*(unsigned int *)(mem + 0x82c) & 0xfffffffd) == 0x9ACA0184) &&
        (*(unsigned int *)(mem + 0x830) == 0xA001004C) &&
        (*(unsigned int *)(mem + 0x834) == 0x84FD8400) &&
        (*(unsigned int *)(mem + 0x8a2) == 0x01494C01)) {
      /*normal 2080*/
      if (mem[0x84a] == 0x81) {
        if (*(unsigned int *)(mem + 0x820) == 0x32BDE9A2) {
          Unp->Forced = 0x820;
          Unp->RetAdr = mem[0x8b2] | mem[0x8b3] << 8;
          if (Unp->RetAdr == 0x1e1) {
            if (*(unsigned int *)(mem + 0x916) == 0x4CA87120) {
              p = *(unsigned short int *)(mem + 0x91a);
              if (p == 0xa7ae) {
                Unp->RetAdr = p;
                mem[0x8b2] = 0xae;
                mem[0x8b3] = 0xa7;
              } else {
                mem[0x916] = 0x2c;
                Unp->RetAdr = p;
              }
            } else if ((mem[0x916] == 0x4C) || (mem[0x916] == 0x20)) {
              Unp->RetAdr = mem[0x917] | mem[0x918] << 8;
            } else if (mem[0x919] == 0x4c) {
              Unp->RetAdr = mem[0x91a] | mem[0x91b] << 8;
            }
          }
          if ((Unp->RetAdr == 0) && (mem[0x8b1] == 0)) {
            Unp->RetAdr = 0xa7ae;
            mem[0x8b1] = 0x4c;
            mem[0x8b2] = 0xae;
            mem[0x8b3] = 0xa7;
          }
          p = 0x8eb;
        }
      }
      /*firelord 2076*/
      else if (mem[0x84a] == 0x7b) {
        if (*(unsigned int *)(mem + 0x81d) == 0x32BDE9A2) {
          Unp->Forced = 0x81d;
          Unp->RetAdr = mem[0x8ac] | mem[0x8ad] << 8;
          p = 0x8eb;
        }
      }
      if (Unp->Forced) {
        Unp->DepAdr = 0x100;
        Unp->StrMem = mem[p + 1] | mem[p + 2] << 8;
        q = p;
        q += mem[p];
        Unp->EndAdr = 0;
        for (; q > p; q -= 4) {
          strtmp = (mem[q - 1] | mem[q] << 8);
          if (strtmp == 0)
            strtmp = 0x10000;
          if (strtmp > Unp->EndAdr)
            Unp->EndAdr = strtmp;
        }
        PrintInfo(Unp, _I_TBCMULTI);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  /* TBC Multicompactor ?  very similar but larger code */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x822) == 0x9D083DBD) &&
        (*(unsigned int *)(mem + 0x826) == 0xD0CA0333) &&
        (*(unsigned int *)(mem + 0x832) == 0xF7D0CA00) &&
        (*(unsigned int *)(mem + 0x836) == 0xCA018678) &&
        (*(unsigned int *)(mem + 0x946) == 0xADC5AFA5)) {
      if (Unp->info->run == -1) {
        for (p = 0x81e; p < 0x821; p++) {
          if (mem[p] == 0xa2) {
            Unp->Forced = p;
            break;
          }
        }
      }
      Unp->DepAdr = 0x334;
      Unp->RetAdr = mem[0x92a] | mem[0x92b] << 8;
      p = 0x94d;
      Unp->StrMem = mem[p + 1] | mem[p + 2] << 8;
      q = p;
      q += mem[p];
      Unp->EndAdr = 0;
      for (; q > p; q -= 4) {
        strtmp = (mem[q - 1] | mem[q] << 8);
        if (strtmp == 0)
          strtmp = 0x10000;
        if (strtmp > Unp->EndAdr)
          Unp->EndAdr = strtmp;
      }
      PrintInfo(Unp, _I_TBCMULT2);
      Unp->IdFlag = 1;
      return;
    }
  }
  /*"AUTOMATIC BREAK SYSTEM" found in Manowar Cracks*/
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x835) == 0x9D0845BD) &&
        (*(unsigned int *)(mem + 0x839) == 0xD0CA00ff) &&
        (*(unsigned int *)(mem + 0x83e) == 0xCA018678) &&
        (*(unsigned int *)(mem + 0x8e1) == 0xADC5AFA5)) {
      if (Unp->info->run == -1) {
        for (p = 0x830; p < 0x834; p++) {
          if (mem[p] == 0xa2) {
            Unp->Forced = p;
            break;
          }
        }
      }
      Unp->DepAdr = 0x100;
      Unp->RetAdr = mem[0x8c5] | mem[0x8c6] << 8;
      p = 0x8fe;
      Unp->StrMem = mem[p + 1] | mem[p + 2] << 8;
      q = p;
      q += mem[p];
      Unp->EndAdr = 0;
      for (; q > p; q -= 4) {
        strtmp = (mem[q - 1] | mem[q] << 8);
        if (strtmp == 0)
          strtmp = 0x10000;
        if (strtmp > Unp->EndAdr)
          Unp->EndAdr = strtmp;
      }
      PrintInfo(Unp, _I_TBCMULTMOW);
      Unp->IdFlag = 1;
      return;
    }
  }
}
