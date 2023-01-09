/* MC and clones
   Timecruncher 1.x/2.x are the same
   Which is the original?
*/
#include "../unp64.h"
void Scn_MasterCompressor(unpstr *Unp) {
  unsigned char *mem;
  int p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    for (p = 0x80d; p < 0x880; p++) {
      if (((*(unsigned int *)(mem + p + 0x005) & 0x00ffffff) == 0x00BDD2A2) &&
          (*(unsigned int *)(mem + p + 0x00a) == 0xE000F99D) &&
          (*(unsigned int *)(mem + p + 0x017) == 0xCAEDD0CA) &&
          (*(unsigned int *)(mem + p + 0x031) == 0x84C82E86) &&
          ((*(unsigned int *)(mem + p + 0x035) & 0x0000ffff) == 0x00004C2D) &&
          (*(unsigned int *)(mem + p + 0x134) == 0xDBD0FFE6)) {
        if (/*mem[p]==0x78&&*/ mem[p + 1] == 0xa9 &&
            (*(unsigned int *)(mem + p + 0x003) == 0xD2A20185)) {
          Unp->DepAdr = mem[p + 0x37] | mem[p + 0x38] << 8;
          Unp->Forced = p + 1;
          if (mem[p + 0x12b] == 0x020) // jsr $0400, unuseful fx
            mem[p + 0x12b] = 0x2c;
        } else if (*(unsigned int *)(mem + p) == 0xD024E0E8) {
          /* HTL version */
          Unp->DepAdr = mem[p + 0x37] | mem[p + 0x38] << 8;
          Unp->Forced = 0x840;
        }
        if (Unp->DepAdr) {
          Unp->RetAdr = mem[p + 0x13e] | mem[p + 0x13f] << 8;
          Unp->EndAdr = 0x2d;
          Unp->fStrBf = Unp->EndAdr;
          PrintInfo(Unp, _I_MASTCOMP);
          Unp->IdFlag = 1;
          return;
        }
      }
    }
  }
  if (Unp->DepAdr == 0) {
    for (p = 0x80d; p < 0x880; p++) {
      if (((*(unsigned int *)(mem + p + 0x005) & 0x00ffffff) == 0x00BDD2A2) &&
          (*(unsigned int *)(mem + p + 0x00a) == 0xE000F99D) &&
          (*(unsigned int *)(mem + p + 0x017) == 0xCAEDD0CA) &&
          (*(unsigned int *)(mem + p + 0x031) == 0x84C82E86) &&
          ((*(unsigned int *)(mem + p + 0x035) & 0x0000ffff) == 0x00004C2D) &&
          (*(unsigned int *)(mem + p + 0x12d) == 0xe2D0FFE6)) {
        if (mem[p + 1] == 0xa9 &&
            (*(unsigned int *)(mem + p + 0x003) == 0xD2A20185)) {
          Unp->DepAdr = mem[p + 0x37] | mem[p + 0x38] << 8;
          Unp->Forced = p + 1;
        }
        if (Unp->DepAdr) {
          if (mem[p + 0x136] == 0x4c)
            Unp->RetAdr = mem[p + 0x137] | mem[p + 0x138] << 8;
          else if (mem[p + 0x13d] == 0x4c)
            Unp->RetAdr = mem[p + 0x13e] | mem[p + 0x13f] << 8;
          Unp->EndAdr = 0x2d;
          Unp->fStrBf = Unp->EndAdr;
          PrintInfo(Unp, _I_MASTCRLX);
          Unp->IdFlag = 1;
          return;
        }
      }
    }
  }
  if (Unp->DepAdr == 0) {
    p = 0x812;
    if ((*(unsigned int *)(mem + p + 0x000) == 0xE67800A0) &&
        (*(unsigned int *)(mem + p + 0x004) == 0x0841B901) &&
        (*(unsigned int *)(mem + p + 0x008) == 0xB900FA99) &&
        (*(unsigned int *)(mem + p + 0x00c) == 0x34990910)) {
      Unp->DepAdr = 0x100;
      Unp->Forced = p;
      Unp->RetAdr = mem[0x943] | mem[0x944] << 8;
      Unp->EndAdr = 0x2d;
      Unp->fStrBf = Unp->EndAdr;
      PrintInfo(Unp, _I_MASTCAGL);
      Unp->IdFlag = 1;
      return;
    }
  }
  /* Fred/Channel4 hack */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x811) == 0xA9A98078) &&
        (*(unsigned int *)(mem + 0x815) == 0x85EE8034) &&
        (*(unsigned int *)(mem + 0x819) == 0x802DA201) &&
        (*(unsigned int *)(mem + 0x882) == 0x01004C2D)) {
      Unp->DepAdr = 0x100;
      Unp->Forced = 0x811;
      Unp->RetAdr = mem[0x98b] | mem[0x98c] << 8;
      if (Unp->RetAdr < 0x800)
        Unp->RtAFrc = 1;
      Unp->EndAdr = 0x2d;
      PrintInfo(Unp, _I_MASTCHF);
      Unp->IdFlag = 1;
      return;
    }
  }
}
