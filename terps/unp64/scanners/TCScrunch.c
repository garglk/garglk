/* TCS Crunch */
#include "../unp64.h"
void Scn_TCScrunch(unpstr *Unp) {
  unsigned char *mem;
  int q, p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x819) == 0x018536A9) && mem[0x81d] == 0x4c) {
      p = mem[0x81e] | mem[0x81f] << 8;
      if (mem[p] == 0xa2 && mem[p + 2] == 0xbd &&
          (*(unsigned int *)(mem + p + 0x05) == 0xE801109D) &&
          ((*(unsigned int *)(mem + p + 0x38) == 0x01524CFB) ||
           ((*(unsigned int *)(mem + p + 0x38) == 0x8DE1A9FB) &&
            (*(unsigned int *)(mem + p + 0x3c) == 0x524C0328)))) {
        Unp->DepAdr = 0x334;
        Unp->Forced = 0x819;
        Unp->EndAdr = 0x2d;
        PrintInfo(Unp, _I_TCSCR2);
      }
    } else if ((*(unsigned int *)(mem + 0x819) == 0x018534A9) &&
               mem[0x81d] == 0x4c) {
      p = mem[0x81e] | mem[0x81f] << 8;
      if (mem[p] == 0xa2 && mem[p + 2] == 0xbd &&
          (*(unsigned int *)(mem + p + 0x05) == 0xE801109D) &&
          (*(unsigned int *)(mem + p + 0x38) == 0x01304CFB)) {
        Unp->DepAdr = 0x334;
        Unp->Forced = 0x818;
        if (mem[Unp->Forced] != 0x78)
          Unp->Forced++;
        Unp->EndAdr = 0x2d;
        Unp->RetAdr = mem[p + 0xd9] | mem[p + 0xda] << 8;
        p += 0xc8;
        q = p + 6;
        for (; p < q; p += 3) {
          if ((mem[p] == 0x20) &&
              (*(unsigned short int *)(mem + p + 1) >= 0xa000) &&
              (*(unsigned short int *)(mem + p + 1) <= 0xbfff)) {
            mem[p] = 0x2c;
          }
        }
        PrintInfo(Unp, _I_TCSCR3);
      }
    }
    if (Unp->DepAdr) {
      Unp->IdFlag = 1;
      return;
    }
  }
}
