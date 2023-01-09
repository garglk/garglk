/* Megabyte Cruncher 64 / ABC cruncher */
#include "../unp64.h"
void Scn_Megabyte(unpstr *Unp) {
  unsigned char *mem;
  int p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    p = 0;
    if (mem[0x816] == 0x4c)
      p = mem[0x817] | mem[0x818] << 8;
    else if ((Unp->info->run == 0x810) && (mem[0x814] == 0x4c) &&
             ((*(unsigned int *)(mem + 0x810) & 0xffff00ff) == 0x018500A9))
      p = mem[0x815] | mem[0x816] << 8;
    if (p) {
      if ((mem[p + 0] == 0x78) && (mem[p + 1] == 0xa2) &&
          (mem[p + 3] == 0xa0) &&
          (*(unsigned int *)(mem + p + 0x05) == 0x15841486) &&
          (*(unsigned int *)(mem + p + 0x1d) == 0x03804CF7)) {
        Unp->DepAdr = 0x380;
        Unp->EndAdr = mem[p + 0x55] | mem[p + 0x56] << 8;
        Unp->EndAdr++;
        Unp->StrMem = 0x801;
        Unp->RetAdr = 0x801; /* ususally it just runs */
        PrintInfo(Unp, _I_MBCR1);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  if (Unp->DepAdr == 0) {
    p = 0;
    if ((mem[0x81a] == 0x4c) &&
        ((*(unsigned int *)(mem + 0x816) & 0xffff00ff) == 0x018500A9))
      p = mem[0x81b] | mem[0x81c] << 8;
    if (p) {
      if ((mem[p + 0] == 0x78) && (mem[p + 1] == 0xa2) &&
          (mem[p + 3] == 0xa0) &&
          (*(unsigned int *)(mem + p + 0x05) == 0x15841486) &&
          (*(unsigned int *)(mem + p + 0x1d) == 0x03844CF7)) {
        Unp->DepAdr = 0x384;
        Unp->Forced = 0x816;
        Unp->EndAdr = mem[p + 0x59] | mem[p + 0x5a] << 8;
        Unp->EndAdr++;
        Unp->StrMem = 0x801;
        Unp->RetAdr = 0x801; /* ususally it just runs */
        PrintInfo(Unp, _I_MBCR2);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
}
