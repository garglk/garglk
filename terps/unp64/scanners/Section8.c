/* Section8 packer?
   found mainly at $0827, also at $0828, some at $0810 and even $081b
*/
#include "../unp64.h"
void Scn_Section8(unpstr *Unp) {
  unsigned char *mem;
  int p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    for (p = 0x810; p <= 0x828; p++) {
      if ((*(unsigned int *)(mem + p) ==
          (unsigned int)(0x00BD00A2 + (((p & 0xff) + 0x11) << 24))) &&
          (*(unsigned int *)(mem + p + 0x04) == 0x01009D08) &&
          (*(unsigned int *)(mem + p + 0x10) == 0x34A97801) &&
          (*(unsigned int *)(mem + p + 0x6a) == 0xB1017820) &&
          (*(unsigned int *)(mem + p + 0x78) == 0x017F20AE)) {
        Unp->DepAdr = 0x100;
        break;
      }
    }
    if (Unp->DepAdr) {
      if (Unp->info->run == -1)
        Unp->Forced = p;
      Unp->StrMem = mem[p + 0x47] | mem[p + 0x4b] << 8;
      Unp->RetAdr = mem[p + 0x87] | mem[p + 0x88] << 8;
      if (Unp->RetAdr == 0xf7) {
        Unp->RetAdr = 0xa7ae;
        mem[p + 0x87] = 0xae;
        mem[p + 0x88] = 0xa7;
      }
      Unp->EndAdr = 0xae;
      PrintInfo(Unp, _I_S8PACK);
      Unp->IdFlag = 1;
      return;
    }
  }
  /* Crackman variant? */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x827) == 0x38BD00A2) &&
        (*(unsigned int *)(mem + 0x82b) == 0x01009D08) &&
        (*(unsigned int *)(mem + 0x837) == 0x34A97801) &&
        (*(unsigned int *)(mem + 0x891) == 0xB1018420) &&
        (*(unsigned int *)(mem + 0x89f) == 0x018b20AE)) {
      Unp->DepAdr = 0x100;
      if (Unp->info->run == -1)
        Unp->Forced = 0x827;
      Unp->StrMem = mem[0x86e] | mem[0x872] << 8;
      if (*(unsigned short int *)(mem + 0x8b7) == 0xff5b) {
        mem[0x8b6] = 0x2c;
        Unp->RetAdr = mem[0x8ba] | mem[0x8bb] << 8;
      } else {
        Unp->RetAdr = mem[0x8b7] | mem[0x8b8] << 8;
      }
      Unp->EndAdr = 0xae;
      PrintInfo(Unp, _I_CRMPACK);
      Unp->IdFlag = 1;
      return;
    }
  }
  /* PET||SLAN variant? */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x812) == 0x20BD00A2) &&
        (*(unsigned int *)(mem + 0x816) == 0x033c9D08) &&
        (*(unsigned int *)(mem + 0x863) == 0xB103B420) &&
        (*(unsigned int *)(mem + 0x86c) == 0x03BB20AE)) {
      Unp->DepAdr = 0x33c;
      if (Unp->info->run == -1)
        Unp->Forced = 0x812;
      Unp->StrMem = mem[0x856] | mem[0x85a] << 8;
      Unp->RetAdr = mem[0x896] | mem[0x897] << 8;
      Unp->EndAdr = 0xae;
      PrintInfo(Unp, _I_PETPACK);
      Unp->IdFlag = 1;
      return;
    }
  }
}
