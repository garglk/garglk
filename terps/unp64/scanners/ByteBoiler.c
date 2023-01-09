/* Byteboiler/OneWay */
#include "../unp64.h"
void Scn_ByteBoiler(unpstr *Unp) {
  unsigned char *mem;
  int q, p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x813) == 0xE800F09D) &&
        (*(unsigned int *)(mem + 0x818) == 0x014E4CF7)) {
      p = mem[0x811] | mem[0x812] << 8;
      if (*(unsigned int *)(mem + p + 1) == 0x02D0FAA5) {
        Unp->DepAdr = 0x14e;
        Unp->Forced = 0x80b;
        Unp->RetAdr = mem[p + 0x5c] | mem[p + 0x5d] << 8;
        Unp->EndAdr = mem[p + 0x0e] | mem[p + 0x0f] << 8;
        Unp->EndAdr++;
        Unp->fStrAf = 0xfe;
        PrintInfo(Unp, _I_BYTEBOILER);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
  /* CPX hack */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x80b) == 0xA97800A2) &&
        (*(unsigned int *)(mem + 0x815) == 0x4C01E6D0)) {
      q = mem[0x819] | mem[0x81a] << 8;
      if ((*(unsigned int *)(mem + q + 3) == 0xE800F09D) &&
          (*(unsigned int *)(mem + q + 8) == 0x014E4CF7)) {
        p = mem[q + 1] | mem[q + 2] << 8;
        if (*(unsigned int *)(mem + p + 1) == 0x02D0FAA5) {
          Unp->DepAdr = 0x14e;
          Unp->Forced = 0x80b;
          Unp->RetAdr = mem[p + 0x5c] | mem[p + 0x5d] << 8;
          Unp->EndAdr = mem[p + 0x0e] | mem[p + 0x0f] << 8;
          Unp->EndAdr++;
          Unp->fStrAf = 0xfe;
          PrintInfo(Unp, _I_BYTEBOICPX);
          Unp->IdFlag = 1;
          return;
        }
      }
    }
  }
  /* SCS hack */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x813) == 0xE800F09D) &&
        (*(unsigned int *)(mem + 0x818) == 0x01bf4CF7)) {
      p = mem[0x811] | mem[0x812] << 8;
      if ((*(unsigned int *)(mem + p + 1) == 0x02D0FAA5) &&
          (*(unsigned int *)(mem + p + 0xdd) == 0x014e4c01)) {
        Unp->DepAdr = 0x14e;
        Unp->Forced = 0x80b;
        Unp->RetAdr = mem[p + 0x5c] | mem[p + 0x5d] << 8;
        Unp->EndAdr = mem[p + 0x0e] | mem[p + 0x0f] << 8;
        Unp->EndAdr++;
        Unp->fStrAf = 0xfe;
        PrintInfo(Unp, _I_BYTEBOISCS);
        Unp->IdFlag = 1;
        return;
      }
    }
  }
}
