/* Action(?) packer */
#include "../unp64.h"
void Scn_ActionPacker(unpstr *Unp) {
  unsigned char *mem;

  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x811) == 0x018538A9) &&
        (*(unsigned int *)(mem + 0x81d) == 0xCEF7D0E8) &&
        (*(unsigned int *)(mem + 0x82d) == 0x0F9D0837) &&
        (*(unsigned int *)(mem + 0x84b) == 0x03D00120)) {
      Unp->DepAdr = 0x110;
      Unp->Forced = 0x811;
      Unp->StrMem = mem[0x848] | mem[0x849] << 8;
      Unp->fEndAf = 0x120;
      Unp->RetAdr = mem[0x863] | mem[0x864] << 8;
      PrintInfo(Unp, _I_ACTIONP);
      Unp->IdFlag = 1;
      return;
    }
  }
}
