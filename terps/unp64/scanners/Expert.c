/* check for Trilogic Expert */
#include "../unp64.h"
void Scn_Expert(unpstr *Unp) {
  unsigned char *mem;
  int q, p;
  if (Unp->IdFlag)
    return;
  mem = Unp->mem;
  if (Unp->DepAdr == 0) {
    for (q = 0x81b; q < 0x81d; q++) {
      if ((*(unsigned int *)(mem + q + 0x00) == 0x852FA978) &&
          (*(unsigned int *)(mem + q + 0x04) == 0x8534A900) &&
          (*(unsigned int *)(mem + q + 0x14) == 0x03860286)) {
        for (p = 0x900; p < 0xfff0; p++) {
          if ((*(unsigned int *)(mem + p + 1) == 0x00084C9A) &&
              (*(unsigned int *)(mem + p - 4) == 0xA2058604)) {
            if (Unp->info->run == -1) {
              Unp->Forced = q;
              Unp->info->run = q;
            }
            q = 0x100 + mem[p] + 1;
            if (q != 0x100) {
              Unp->DepAdr = q;
            }
          }
        }
        break;
      }
    }
    if (Unp->DepAdr) {
      Unp->RTIFrc = 1;
      if (*(unsigned int *)(mem + 0x835) == 0x6E8D48A9) {
        p = 0;
        if (*(unsigned int *)(mem + 0x92c) == 0x4902B100) {
          PrintInfo(Unp, _I_EXPERT27);
          if (!Unp->IdOnly) {
            p = 0x876;
            if (Unp->DebugP)
              fprintf(stderr, "\nantihack patched @ $%04x\n", p);
            mem[p] = 0x00; /* 1st anti hack */
            p = mem[0x930];
          }
        } else if (*(unsigned int *)(mem + 0x92f) == 0x4902B100) {
          PrintInfo(Unp, _I_EXPERT29);
          if (!Unp->IdOnly) {
            p = 0x873;
            if (Unp->DebugP)
              fprintf(stderr, "\nantihack patched @ $%04x\n", p);
            mem[p] = 0xa9; /* 1st anti hack */
            mem[p + 1] = 0x02;
            p = mem[0x933];
          }
        } else {
          PrintInfo(Unp, _I_EXPERT2X);
        }
        if (p && !Unp->IdOnly) {
          p |= (p << 24) | (p << 16) | (p << 8);
          for (q = 0x980; q < 0xfff0; q++) {
            if (((mem[q] ^ (p & 0xff)) == 0xac) &&
                ((mem[q + 3] ^ (p & 0xff)) == 0xc0) &&
                (((*(unsigned int *)(mem + q + 7)) ^ p) == 0xC001F2AC)) {
              if (Unp->DebugP)
                fprintf(stderr, "\nantihack patched @ $%04x\n", q);
              mem[q + 0x06] = (p & 0xff); /* 2nd anti hack */
              mem[q + 0x0d] = (p & 0xff);
              break;
            }
          }
        }
      } else {
        PrintInfo(Unp, _I_EXPERT20);
      }
    }
  }
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x81b) == 0x2FA9D878) &&
        (*(unsigned int *)(mem + 0x82d) == 0x0873BDB0)) {
      for (p = 0x900; p < 0xfff0; p++) {
        if ((*(unsigned int *)(mem + p) == 0xA2F3D0CA) &&
            (mem[p + 0x05] == 0x4c)) {
          q = mem[p + 0x06] | mem[p + 0x07] << 8;
          if (q != 0x100) {
            Unp->DepAdr = q;
            break;
          }
        }
      }
      if (Unp->DepAdr) {
        Unp->RTIFrc = 1;
        Unp->Forced = 0x81b;
        PrintInfo(Unp, _I_EXPERT21);
      }
    }
  }
  /* 2.9 Expert User Club version, found in
     BloodMoney/HTL & SWIV/Inceria
  */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x81b) == 0x8C00A078) &&
        (*(unsigned int *)(mem + 0x831) == 0x05860485) &&
        (*(unsigned int *)(mem + 0x998) == 0x00084C9A)) {
      p = mem[0x919];
      q = p << 24 | p << 16 | p << 8 | p;
      for (p = 0x900; p < 0xfff0; p++) {
        if (((*(unsigned int *)(mem + p) ^ q) == 0xA2F3D0CA) &&
            ((mem[p + 0x05] ^ (q & 0xff)) == 0x4c)) {
          q = (mem[p + 0x06] ^ (q & 0xff)) | (mem[p + 0x07] ^ (q & 0xff)) << 8;
          if (q != 0x100) {
            Unp->DepAdr = q;
            break;
          }
        }
      }
      if (Unp->DepAdr) {
        Unp->RTIFrc = 1;
        Unp->Forced = 0x81b;
        PrintInfo(Unp, _I_EXPERT29EUC);
      }
    }
  }
  /* sys2070 A.S.S. */
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x817) == 0x00852FA9) &&
        (*(unsigned int *)(mem + 0x823) == 0x05860485) &&
        (*(unsigned int *)(mem + 0x9a0) == 0x00084C9A)) {
      p = mem[0x923];
      q = p << 24 | p << 16 | p << 8 | p;
      for (p = 0x900; p < 0xfff0; p++) {
        if (((*(unsigned int *)(mem + p) ^ q) == 0xA2F3D0CA) &&
            ((mem[p + 0x05] ^ (q & 0xff)) == 0x4c)) {
          q = (mem[p + 0x06] ^ (q & 0xff)) | (mem[p + 0x07] ^ (q & 0xff)) << 8;
          if (q != 0x100) {
            Unp->DepAdr = q;
            break;
          }
        }
      }
      if (Unp->DepAdr) {
        Unp->RTIFrc = 1;
        Unp->Forced = 0x81b;
        PrintInfo(Unp, _I_EXPERTASS);
      }
    }
  }
  if (Unp->DepAdr == 0) {
    if ((*(unsigned int *)(mem + 0x81b) == 0x7FA978D8) ||
        (*(unsigned int *)(mem + 0x81b) == 0x7FA9D878) ||
        (*(unsigned int *)(mem + 0x816) == 0x7FA978D8)) {
      for (p = 0x900; p < 0xfff0; p++) {
        if ((*(unsigned int *)(mem + p) == 0xA2F3D0CA) &&
            (mem[p + 0x05] == 0x4c)) {
          q = mem[p + 0x06] | mem[p + 0x07] << 8;
          if (q != 0x100) {
            Unp->DepAdr = q;
            break;
          }
        }
      }
      if (Unp->DepAdr) {
        Unp->RTIFrc = 1;
        if (*(unsigned int *)(mem + 0x816) == 0x7FA978D8) {
          q = 0x816;
          PrintInfo(Unp, _I_EXPERT4X);
          if (!Unp->IdOnly) {
            for (p = 0x900; p < 0xfff0; p++) {
              if ((*(unsigned int *)(mem + p) == 0xE0A9F0A2) &&
                  (*(unsigned int *)(mem + p + 4) == 0xE807135D) &&
                  (mem[p + 0x8] == 0xd0)) {
                if (Unp->DebugP)
                  fprintf(stderr, "\nantihack patched @ $%04x\n", p);
                mem[p + 0x1] = 0x00;
                mem[p + 0x3] = 0x98;
                memset(mem + p + 4, 0xea, 6);
                break;
              }
            }
          }
        } else {
          q = 0x81b;
          PrintInfo(Unp, _I_EXPERT3X);
          if (!Unp->IdOnly) {
            for (p = 0x900; p < 0xfff0; p++) {
              if ((*(unsigned int *)(mem + p) == 0xCA08015D) &&
                  (*(unsigned int *)(mem + p + 4) == 0xF8D003E0) &&
                  (mem[p + 0xa] == 0xd0)) {
                p += 0xa;
                if (Unp->DebugP)
                  fprintf(stderr, "\nantihack patched @ $%04x\n", p);
                mem[p] = 0x24;
                break;
              }
            }
          }
        }
        if (Unp->info->run == -1) {
          Unp->Forced = q;
          Unp->info->run = q;
        }
      }
    }
  }
  if (Unp->DepAdr == 0) {
    q = 0x81b;
    if ((*(unsigned int *)(mem + q + 0x00) == 0x852FA978) &&
        (*(unsigned int *)(mem + q + 0x04) == 0x8534A900) &&
        (*(unsigned int *)(mem + q + 0x14) == 0x03860286) &&
        (*(unsigned int *)(mem + q + 0x4f) == 0xA200594C) &&
        (*(unsigned int *)(mem + q + 0xad) == 0x2000124C)) {
      Unp->Forced = q;
      Unp->info->run = q;
      Unp->DepAdr = 0x12;
      Unp->RTIFrc = 1;
      PrintInfo(Unp, _I_EXPERT1X);
    }
  }
  /* expert 2.11 (sys2074) & unknown sys2061 */
  if (Unp->DepAdr == 0) {
    for (q = 0x80d; q < 0x820; q++) {
      if ((*(unsigned int *)(mem + q + 0x00) == 0x852FA978) &&
          (*(unsigned int *)(mem + q + 0x04) == 0x8534A900) &&
          (*(unsigned int *)(mem + q + 0x13) == 0x03840284) &&
          (*(unsigned int *)(mem + q + 0x4f) == 0x084C003A) &&
          (*(unsigned int *)(mem + q + 0xad) == 0x00AA2048)) {
        Unp->Forced = q;
        Unp->info->run = q;
        Unp->DepAdr = 0x100 + mem[q + 0x17a] + 1;
        break;
      }
    }
    if (Unp->DepAdr != 0) {
      Unp->RTIFrc = 1;
      PrintInfo(Unp, _I_EXPERT211);
    }
  }

  if (Unp->DepAdr != 0) {
    Unp->IdFlag = 1;
    return;
  }
}
