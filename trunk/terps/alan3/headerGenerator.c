#include <stdio.h>
#include <stdlib.h>
#include "acode.h"

/*----------------------------------------------------------------------*/
static int littleEndian() {
  int x = 1;
  return (*(char *)&x == 1);
}


/*----------------------------------------------------------------------*/
static Aword reverse(Aword w) {
  Aword s;                      /* The swapped ACODE word */
  char *wp, *sp;
  int i;

  if (littleEndian()) {
    wp = (char *) &w;
    sp = (char *) &s;
    for (i = 0; i < sizeof(Aword); i++)
      sp[sizeof(Aword)-1 - i] = wp[i];
    return s;
  } else
    return w;
}


/*======================================================================*/
int main(int argc, char **argv) {
  ACodeHeader *header = calloc(sizeof(ACodeHeader), 1);
  FILE *acodeFile;
  int writeSize;

  if (argc != 6)
    printf("Usage: %s <file> <version> <revision> <correction> <state>\n", argv[0]);
  else {
    header->tag[0] = 'A';
    header->tag[1] = 'L';
    header->tag[2] = 'A';
    header->tag[3] = 'N';
    header->version[0] = strtol(argv[2], NULL, 0);
    header->version[1] = strtol(argv[3], NULL, 0);
    header->version[2] = strtol(argv[4], NULL, 0);
    header->version[3] = argv[5][0];
    header->size = reverse(sizeof(ACodeHeader)/sizeof(Aword));
    if ((acodeFile = fopen(argv[1], "wb")) == NULL) {
      fprintf(stderr, "ERROR - Could not open file for writing.\n");
      exit(-1);
    }
    writeSize = fwrite(header, 1, sizeof(ACodeHeader), acodeFile);
    if (writeSize != sizeof(ACodeHeader)) {
      fprintf(stderr, "ERROR - Could only write %d bytes of header to file.\n", writeSize);
      exit(-1);
    }
    fclose(acodeFile);
  }
  exit(0);
}
