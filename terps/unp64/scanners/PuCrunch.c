/* PuCrunch */
#include "../unp64.h"
void Scn_PuCrunch(unpstr *Unp) {
  unsigned char *mem;
  int q, p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    if ((mem[0x80d] == 0x78) &&
        (*(unsigned int *)(mem + 0x813) == 0x34A20185) &&
        (*(unsigned int *)(mem + 0x817) == 0x9D0842BD) &&
        (*(unsigned int *)(mem + 0x81b) == 0xD0CA01FF) &&
        (*(unsigned int *)(mem + 0x83d) == 0x4CEDD088)) {
      for (p = 0x912; p < 0x938; p++) {
        if ((*(unsigned int *)(mem + p) == 0x2D85FAA5) &&
            (*(unsigned int *)(mem + p + 4) == 0x2E85FBA5)) {
          Unp->EndAdr = 0xfa;
          Unp->StrMem = mem[0x879] | mem[0x87a] << 8;
          Unp->DepAdr = mem[0x841] | mem[0x842] << 8;
          Unp->RetAdr = mem[p + 0xa] | mem[p + 0xb] << 8;
          Unp->Forced = 0x80d;
          PrintInfo(Unp, _I_PUCRUNCH);
          break;
        }
      }
    } else if ((mem[0x80d] == 0x78) &&
               (*(unsigned int *)(mem + 0x81a) == 0x10CA4B95) &&
               (*(unsigned int *)(mem + 0x81e) == 0xBD3BA2F8) &&
               (*(unsigned int *)(mem + 0x847) == 0x4CEDD088)) {
      for (p = 0x912; p < 0x938; p++) {
        if ((*(unsigned int *)(mem + p) == 0x2D85FAA5) &&
            (*(unsigned int *)(mem + p + 4) == 0x2E85FBA5)) {
          Unp->EndAdr = 0xfa;
          Unp->StrMem = mem[0x88a] | mem[0x88b] << 8;
          Unp->DepAdr = mem[0x84b] | mem[0x84c] << 8;
          Unp->RetAdr = mem[p + 0xa] | mem[p + 0xb] << 8;
          Unp->Forced = 0x80d;
          PrintInfo(Unp, _I_PUCRUNCW);
          break;
        }
      }
    } else if ((mem[0x80d] == 0x78) &&
               (*(unsigned int *)(mem + 0x811) == 0x85AAA901) &&
               (*(unsigned int *)(mem + 0x81d) == 0xF69D083C) &&
               (*(unsigned int *)(mem + 0x861) == 0xC501C320) &&
               (*(unsigned int *)(mem + 0x839) == 0x01164CED)) {
      Unp->EndAdr = 0xfa;
      Unp->StrMem = mem[0x840] | mem[0x841] << 8;
      Unp->DepAdr = 0x116;
      Unp->RetAdr = mem[0x8df] | mem[0x8e0] << 8;
      Unp->Forced = 0x80d;
      PrintInfo(Unp, _I_PUCRUNCS);
    } else if ((mem[0x80d] == 0x78) &&
               (*(unsigned int *)(mem + 0x811) == 0x85AAA901) &&
               (*(unsigned int *)(mem + 0x81d) == 0xF69D083C) &&
               (*(unsigned int *)(mem + 0x861) == 0xC501C820) &&
               (*(unsigned int *)(mem + 0x839) == 0x01164CED)) {
      Unp->EndAdr = 0xfa;
      Unp->StrMem = mem[0x840] | mem[0x841] << 8;
      Unp->DepAdr = 0x116;
      if (mem[0x8de] == 0xa9) {
        Unp->RetAdr = mem[0x8e1] | mem[0x8e2] << 8;
        if ((Unp->RetAdr == 0xa871) && (mem[0x8e0] == 0x20) &&
            (mem[0x8e3] == 0x4c)) {
          mem[0x8e0] = 0x2c;
          Unp->RetAdr = mem[0x8e4] | mem[0x8e5] << 8;
        }
      } else {
        Unp->RetAdr = mem[0x8df] | mem[0x8e0] << 8;
      }
      Unp->Forced = 0x80d;
      PrintInfo(Unp, _I_PUCRUNCS);
    } else {
      /* unknown old/hacked pucrunch ? */
      for (p = 0x80d; p < 0x820; p++) {
        if (mem[p] == 0x78) {
          q = p;
          for (; p < 0x824; p++) {
            if (((*(unsigned int *)(mem + p) & 0xf0ffffff) == 0xF0BD53A2) &&
                (*(unsigned int *)(mem + p + 4) == 0x01FF9D08) &&
                (*(unsigned int *)(mem + p + 8) == 0xA2F7D0CA)) {
              Unp->Forced = q;
              q = mem[p + 3] & 0xf; /* can be $f0 or $f2, q&0x0f as offset */
              p = mem[p + 0xe] | mem[p + 0xf] << 8;
              if (mem[p - 2] == 0x4c && mem[p + 0xa0 + q] == 0x85) {
                Unp->DepAdr = mem[p - 1] | mem[p] << 8;
                Unp->StrMem = mem[p + 4] | mem[p + 5] << 8;
                Unp->EndAdr = 0xfa;
                p += 0xa2;
                q = p + 8;
                for (; p < q; p++) {
                  if ((*(unsigned int *)(mem + p) == 0x2D85FAA5) &&
                      (mem[p + 9] == 0x4c)) {
                    Unp->RetAdr = mem[p + 0xa] | mem[p + 0xb] << 8;
                    break;
                  }
                }
                PrintInfo(Unp, _I_PUCRUNCO);
              }
            }
          }
        }
      }
    }

    /* various old/hacked pucrunch */
    /* common pattern, variable pos from 0x79 to 0xd1
    90 ?? C8 20 ?? 0? 85 ?? C9 ?0 90 0B A2 0? 20 ?? 0? 85 ?? 20 ?? 0? A8 20 ??
    0? AA BD ?? 0? E0 20 90 0? 8A (A2 03) not always 20 ?? 02 A6
    ?? E8 20 F9
    */
    if (Unp->DepAdr == 0) {
      Unp->IdFlag = 0;
      for (q = 0x70; q < 0xff; q++) {
        if (((*(unsigned int *)(mem + 0x801 + q) & 0xFFFF00FF) == 0x20C80090) &&
            ((*(unsigned int *)(mem + 0x801 + q + 8) & 0xFFFF0FFF) ==
             0x0B9000C9) &&
            ((*(unsigned int *)(mem + 0x801 + q + 12) & 0x00FFF0FF) ==
             0x002000A2) &&
            ((*(unsigned int *)(mem + 0x801 + q + 30) & 0xF0FFFFFf) ==
             0x009020E0)) {
          Unp->IdFlag = _I_PUCRUNCG;
          break;
        }
      }
      if (Unp->IdFlag) {
        for (p = 0x801 + q + 34; p < 0x9ff; p++) {
          if (*(unsigned int *)(mem + p) == 0x00F920E8) {
            for (; p < 0x9ff; p++) {
              if (mem[p] == 0x4c) {
                Unp->RetAdr = mem[p + 1] | mem[p + 2] << 8;
                if (Unp->RetAdr > 0x257)
                  break;
              }
            }
            break;
          }
        }
        for (p = 0; p < 0x40; p++) {
          if (Unp->info->run == -1)
            if (Unp->Forced == 0) {
              if (mem[0x801 + p] == 0x78) {
                Unp->Forced = 0x801 + p;
                Unp->info->run = Unp->Forced;
              }
            }
          if ((*(unsigned int *)(mem + 0x801 + p) == 0xCA00F69D) &&
              (mem[0x801 + p + 0x1b] == 0x4c)) {
            q = 0x801 + p + 0x1c;
            Unp->DepAdr = mem[q] | mem[q + 1] << 8;
            q = 0x801 + p - 2;
            p = mem[q] | mem[q + 1] << 8;
            if ((mem[p + 3] == 0x8d) && (mem[p + 6] == 0xe6)) {
              Unp->StrMem = mem[p + 4] | mem[p + 5] << 8;
            }
            break;
          }
        }
        Unp->EndAdr = 0xfa; // some hacks DON'T xfer fa/b to 2d/e
        Unp->IdFlag = 1;
        PrintInfo(Unp, _I_PUCRUNCG);
      }
    }
    if (Unp->DepAdr) {
      Unp->IdFlag = 1;
      return;
    }
  }
}
