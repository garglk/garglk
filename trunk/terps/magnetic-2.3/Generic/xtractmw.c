
/*
    XtractMW.c
    Extracter for Magnetic Scrolls games (Wonderland
    and the MS Collection Volume 1), MS-DOS versions.

    Written by David Kinder, based on code
    by Stefan Jokisch and Niclas Karlsson.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 42
#define MAX_FILES 10
#define BUF_SIZE 1024
#define NUMBER_GAMES 4

FILE* FileHandles[MAX_FILES];
FILE* OutputFile = NULL;
long FileLengths[MAX_FILES];
int FileCount = 0;
unsigned char Buffer[BUF_SIZE];
int Game = -1;
int NextExtract = 0;
int RemoveProtection = 0;

struct GameInfo
{
  int TotalFiles;
  const char* pDisplayName;
  long Protection;

  const char* pCodeName;
  long CodeSize;
  const char* pTextName;
  long TextSize;
  const char* pIndexName;
  long IndexSize;
  const char* pDictName;
  long DictSize;

  long StringSplit;
  long UndoSize;
  long UndoPC;
};

const struct GameInfo MSGames[4] =
{
  { 1031,"Wonderland 1.21 (MS-DOS)",0x06778,
    "code",0x1523C,"text",0x13C2C,"index",0x103A,"wtab",0x2FA0,
    0x10000,0x3900,0x75F2 },
  { 383,"Corruption 1.12 (MS-DOS)",0xFFFFF,
    "ccode",0x12560,"ctext",0x161C3,"cindex",0xF4C,"cwtab",0x1D88,
    0x10000,0x2500,0x6624 },
  { 466,"Guild of Thieves 1.3 (MS-DOS)",0xFFFFF,
    "gcode",0x107F8,"gtext",0xF394,"gindex",0xDA0,"gwtab",0x2070,
    0xE000,0x3400,0x6528 },
  { 339,"Fish! 1.10 (MS-DOS)",0xFFFFF,
    "fcode",0x124C4,"ftext",0x14E17,"findex",0xE48,"fwtab",0x2098,
    0x10000,0x2A00,0x583A }
};

void writeLong(unsigned char* p, unsigned long v)
{
  p[3] = (unsigned char)(v & 0xFF);
  v >>= 8;
  p[2] = (unsigned char)(v & 0xFF);
  v >>= 8;
  p[1] = (unsigned char)(v & 0xFF);
  v >>= 8;
  p[0] = (unsigned char)v;
}

void writeWord(unsigned char* p, unsigned long v)
{
  p[1] = (unsigned char)(v & 0xFF);
  v >>= 8;
  p[0] = (unsigned char)v;
}

void Error(const unsigned char *pError)
{
   fprintf(stderr,"Fatal Error: %s\n",pError);
   exit(1);
}

void OpenFiles(const unsigned char* pRdfName)
{
  FILE *fp;
  int result;

  if ((fp = fopen(pRdfName,"rt")) == NULL)
    Error("Cannot open RDF file");

  while ((result = fscanf(fp," %256s %ld\n",Buffer,FileLengths+FileCount)) == 2)
  {
    if (FileCount < MAX_FILES)
    {
      if ((FileHandles[FileCount++] = fopen(Buffer,"rb")) == NULL)
        Error("Cannot open resource file");
    }
  }

  fclose(fp);
  if (result != EOF)
    Error("Bad format RDF file");
}

void OpenOutputFile(const unsigned char* pMagName)
{
  unsigned char header[HEADER_SIZE];
  long code, text1, text2, index, dict, decode, total;
  long undo_size, undo_pc;

  if ((OutputFile = fopen(pMagName,"wb")) == NULL)
    Error("Cannot open output file");

  /* Get all the buffer sizes. */

  code = MSGames[Game].CodeSize;
  decode = MSGames[Game].TextSize;
  index = MSGames[Game].IndexSize;
  dict = MSGames[Game].DictSize;
  undo_size = MSGames[Game].UndoSize;
  undo_pc = MSGames[Game].UndoPC;

  text1 = MSGames[Game].StringSplit;
  text2 = decode + index - text1;
  total = HEADER_SIZE + code + decode + index + dict;

  /* Set up the header. */

  writeLong(header+ 0,0x4D615363);   /* Magic word */
  writeLong(header+ 4,total);        /* Size of this file */
  writeLong(header+ 8,42);           /* Size of this header */
  writeWord(header+12,4);            /* Game version */
  writeLong(header+14,code);         /* Size of code section */
  writeLong(header+18,text1);        /* Size of first string section */
  writeLong(header+22,text2);        /* Size of second string section */
  writeLong(header+26,dict);         /* Size of dictionary */
  writeLong(header+30,decode);       /* Offset to string decoding table */
  writeLong(header+34,undo_size);    /* Undo size */
  writeLong(header+38,undo_pc);      /* Undo offset */

  /* Write out the header. */
  fwrite(header,1,HEADER_SIZE,OutputFile);
}

void ReadFile(long *offset, unsigned int size)
{
  long l = 0;
  unsigned int read = 0;
  int i;

  for (i = 0; (i < FileCount) && (read < size); i++)
  {
    if (l + FileLengths[i] > (long)(*offset + read))
    {
      unsigned int s = size - read;

      fseek(FileHandles[i],*offset + read - l,SEEK_SET);
      if ((long)(*offset + size) > l + FileLengths[i])
        s = FileLengths[i] - ftell(FileHandles[i]);
      fread(Buffer + read,s,1,FileHandles[i]);
      read += s;
    }
    l += FileLengths[i];
  }

  if (read != size)
    Error("Bad offset");
  *offset += size;
}

void ExtractFile(long offset, long size, int unprotect)
{
  long read = 0;
  long prot = MSGames[Game].Protection;

  while (read < size)
  {
    long s = size - read;

    if (s > BUF_SIZE)
      s = BUF_SIZE;
    ReadFile(&offset,(unsigned int)s);

    /* Remove copy protection */
    
    if ((unprotect != 0) && (RemoveProtection != 0))
    {
      if ((prot >= read) && (prot < read + s))
      {
        Buffer[prot - read] = 0x4E;
        printf("Removing copy protection...\n");
      }
      if ((prot + 1 >= read) && (prot + 1 < read + s))
        Buffer[prot + 1- read] = 0x75;
    }

    fwrite(Buffer,s,1,OutputFile);
    read += s;
  }
}

void ExtractFiles(const char* pMagName)
{
  long offset = 0;
  int count, i = 0;
  long dict_l = 0, dict_o = 0;

  /* Work out how many files are present. */

  ReadFile(&offset,4);
  offset = ((long)Buffer[0] <<  0) |
           ((long)Buffer[1] <<  8) |
           ((long)Buffer[2] << 16) |
           ((long)Buffer[3] << 24);

  ReadFile(&offset,2);
  count = ((int)Buffer[0] << 0) |
          ((int)Buffer[1] << 8);

  /* Identify the game. */

  while ((Game < 0) && (i < NUMBER_GAMES))
  {
    if (count == MSGames[i].TotalFiles)
    {
      Game = i;
      break;
    }
    i++;
  }

  if (Game >= 0)
  {
    printf("Found %s\n",MSGames[Game].pDisplayName);
    if (MSGames[Game].Protection != 0xFFFF)
    {
      RemoveProtection = 1;
/*
      char c;
      printf("Remove the password protection (y/n)? ");
      c = getchar();
      if (c=='y' || c=='Y')
        RemoveProtection = 1;
*/
    }
    OpenOutputFile(pMagName);
  }
  else
    Error("Game not recognised");

  /* Loop through each file. */

  for (i = 0; i < count; i++)
  {
    char name[13];
    long o, l;
    unsigned int n;
    int j;

    ReadFile(&offset,18);
    o = ((long)Buffer[2] <<  0) |
        ((long)Buffer[3] <<  8) |
        ((long)Buffer[4] << 16) |
        ((long)Buffer[5] << 24);
    l = ((long)Buffer[6] <<  0) |
        ((long)Buffer[7] <<  8) |
        ((long)Buffer[8] << 16) |
        ((long)Buffer[9] << 24);
    n = ((unsigned)Buffer[16] << 0) |
        ((unsigned)Buffer[17] << 8);

    if (n == 4)
    {
      for (j = 0; (j < 6) && (Buffer[10+j] != 0); j++)
        name[j] = Buffer[10+j];
      name[j] = 0;

      switch (NextExtract)
      {
      case 0:
        if (strcmp(name,MSGames[Game].pCodeName) == 0)
        {
          printf("Extracting code...\n");
          ExtractFile(o,l,1);
          NextExtract++;
        }
        else if (strcmp(name,MSGames[Game].pDictName) == 0)
        {
          dict_o = o;
          dict_l = l;
        }
        break;
      case 1:
        if (strcmp(name,MSGames[Game].pTextName) == 0)
        {
          printf("Extracting text...\n");
          ExtractFile(o,l,0);
          NextExtract++;
        }
        break;
      case 2:
        if (strcmp(name,MSGames[Game].pIndexName) == 0)
        {
          printf("Extracting index...\n");
          ExtractFile(o,l,0);
          NextExtract++;
        }
        break;
      case 3:
        if (strcmp(name,MSGames[Game].pDictName) == 0)
        {
          printf("Extracting dictionary...\n");
          ExtractFile(o,l,0);
          NextExtract++;
        }
        break;
      }
    }
  }

  /* Check if the dictionary occured out of sequence. */
  if (NextExtract == 3)
  {
    if (dict_l > 0)
    {
      printf("Extracting dictionary...\n");
      ExtractFile(dict_o,dict_l,0);
      NextExtract++;
    }
  }
}

void CloseFiles(void)
{
  int i;
  for (i = 0; i < FileCount; i++)
    fclose(FileHandles[i]);
  fclose(OutputFile);
}

int main(int argc, char** argv)
{
  if (argc == 3)
  {
    OpenFiles(argv[1]);
    ExtractFiles(argv[2]);
    CloseFiles();

    /* Was all the data extracted? */
    if (NextExtract != 4)
      Error("Not enough game data found");
    else
      printf("Game extracted successfully");
  }
  else
  {
    printf("XtractMW v1.0 by David Kinder, Stefan Jokisch and Niclas Karlsson.\n\n"
           "Extractor for the Magnetic Windows versions of Magnetic Scrolls\n"
           "games (Wonderland and the MS Collection Volume 1), MS-DOS versions.\n\n"
           "Usage: XtractMW game.rdf game.mag\n");
  }
  return 0;
}
