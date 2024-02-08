
/*
    XtractPC.c
    Extractor for the original MS-DOS versions of the
    Magnetic Scrolls games.

    Written by Niclas Karlsson and David Kinder.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MSDOS__) || defined (_WIN32)
#define SEP "\\"
#else
#define SEP "/"
#endif

#ifdef __MSDOS__
typedef unsigned char huge* HugePtr;
#include <alloc.h>
#define malloc farmalloc
#define free farfree
#else
typedef unsigned char* HugePtr;
#endif

struct GameInfo
{
  const char* Name;

  const char* HeaderFileIndex;
  unsigned char HeaderBytes[8];

  int Version;
  unsigned long UndoSize;
  unsigned long UndoPC;
  unsigned long CopyAddr1;
  unsigned char CopyByte1;
  unsigned long CopyAddr2;
  unsigned char CopyByte2;

  unsigned long PatchAddr;
  unsigned char PatchByte;
  const char* PatchMsg;
};

struct GameInfo Games[7] =
{
  "Corruption 1.11",
  "0",{ 0x7A,0x02,0x01,0x05,0x03,0x04,0x06,0x08 },
  3,0x2100,0x43A0,0x0000,0x00,0x0000,0x00,
  0x0000,0x00,0,

  "Fish 1.02",
  "1",{ 0x7E,0x02,0x01,0x80,0x03,0x04,0x05,0x06 },
  3,0x2300,0x3FA0,0x3A8E,0x4E,0x3A8F,0x71,
  0x0000,0x00,0,

  "Guild 1.1",
  "1",{ 0x7E,0x02,0x01,0x05,0x03,0x04,0x80,0x06 },
  1,0x5000,0x6D5C,0x6D02,0x4E,0x6D03,0x71,
  0x0000,0x00,0,

  "Jinxter 1.05",
  "0",{ 0x7A,0x02,0x01,0x05,0x03,0x04,0x06,0x07 },
  2,0x2C00,0x4A08,0x4484,0x4E,0x4485,0x71,
  0xD29E,0x58,
  "Jinxter fix:\n"
  "  This game has bug that makes the interpreter crash when\n"
  "  objects are thrown at a certain window. This is being patched...\n",

  "Jinxter 1.10",
  "0",{ 0x7A,0x02,0x01,0x05,0x03,0x04,0x06,0x09 },
  2,0x2C00,0x4A56,0x44CC,0x4E,0x44CD,0x71,
  0xD28A,0x58,
  "Jinxter fix:\n"
  "  This game has bug that makes the interpreter crash when\n"
  "  objects are thrown at a certain window. This is being patched...\n",

  "Myth 1.0",
  "0",{ 0x78,0x02,0x01,0x05,0x03,0x04,0x06,0x07 },
  3,0x1500,0x3A0A,0x314A,0x60,0x0000,0x00,
  0x0000,0x00,0,

  "Pawn 2.3",
  "1",{ 0x7E,0x02,0x01,0x04,0x03,0x80,0x05,0x07 },
  0,0x3600,0x4420,0x40F6,0x4E,0x40F7,0x71,
  0x0000,0x00,0,
};

#define NUMBER_GAMES 7

char Name[256];
char* NameNumber = 0;
FILE* InputFile = 0;
FILE* OutputFile = 0;
int GameIndex = -1;

HugePtr Dictionary, Code, String1, String2;
unsigned long DictionarySize = 0, CodeSize = 0;
unsigned long String1Size = 0, String2Size = 0;
unsigned char DecodeTable[256];

void Error(const char* error)
{
  if (InputFile)
    fclose(InputFile);
  if (OutputFile)
    fclose(OutputFile);
  if (Dictionary)
    free(Dictionary);
  if (Code)
    free(Code);
  if (String1)
    free(String1);
  if (String2)
    free(String2);

  fprintf(stderr,"Fatal Error: %s\n",error);
  exit(1);
}

unsigned long ReadFile(HugePtr buf, unsigned long sz, FILE *fp)
{
#ifdef __MSDOS__
  unsigned long n = 0xF000, i, j = 0;

  for (i = 0; i < sz; i += n)
  {
    if (sz-i < 0xF000)
      n = sz - i;
    j += fread(buf+i,1,n,fp);
  }
  return j;
#else
  return fread(buf,1,sz,fp);
#endif
}

unsigned long WriteFile(HugePtr buf, unsigned long sz, FILE *fp)
{
#ifdef __MSDOS__
  unsigned long n = 0xF000, i, j = 0;

  for (i = 0; i < sz; i += n)
  {
    if (sz-i < 0xF000)
      n = sz - i;
    j += fwrite(buf+i,1,n,fp);
  }
  return j;
#else
  return fwrite(buf,1,sz,fp);
#endif
}

void RecogniseGame(void)
{
  int i = 0;

  while ((GameIndex == -1) && (i < NUMBER_GAMES))
  {
    strcpy(NameNumber,Games[i].HeaderFileIndex);

    if (InputFile = fopen(Name,"rb"))
    {
      unsigned char header[8];
      int j = 0;
      int fail = 0;

      fread(header,1,8,InputFile);
      while (j < 8)
      {
        if (header[j] != Games[i].HeaderBytes[j])
          fail = 1;
        j++;
      }

      if (fail == 0)
      {
        GameIndex = i;
        printf("Found %s\n",Games[GameIndex].Name);
      }

      fclose(InputFile);
      InputFile = 0;
    }
    i++;
  }

  if (GameIndex == -1)
    Error("Game not recognised");
}

unsigned char GetBits(int init)
{
  static unsigned char mask, c;
  unsigned char b;

  if (init)
  {
    mask = 0;
    return 0;
  }
  for (b = 0; b < 0x80;)
  {
    b <<= 1;
    mask >>= 1;
    if (!mask)
    {
      mask = 0x80;
      c = fgetc(InputFile);
    }
    if (!(c & mask))
      b++;
    b = DecodeTable[b];
  }
  return (unsigned char)(b & 0x3F);
}

unsigned long ReadAndUnpack(HugePtr* ptr, int unpack)
{
  unsigned char data[4];
  HugePtr ptr2;
  unsigned long size, loop = 0;
  int table_size, i;

  if ((InputFile = fopen(Name,"rb")) == 0)
    Error("Couldn't open input file");

  if (unpack)
  {
    GetBits(1);
    table_size = fgetc(InputFile);
    for (i = 0; i < table_size; i++)
      DecodeTable[i] = fgetc(InputFile);

    loop = (fgetc(InputFile) << 8) | fgetc(InputFile);
    if ((ptr2 = (HugePtr)malloc(loop*3)) == 0)
      Error("Not enough memory");

    size = 0;
    *ptr = ptr2;
    while (loop--)
    {
      for (i = 0; i < 4; i++)
        data[i] = GetBits(0);
      for (i = 0; i < 3; i++)
        ptr2[size++] = data[i] | ((data[3] << (2*i+2)) & 0xC0);
    }
  }
  else
  {
    if (fseek(InputFile,0,SEEK_END) < 0)
      Error("File error");
    if ((size = ftell(InputFile)) < 0)
      Error("File error");
    rewind(InputFile);
    ptr2 = malloc(size);
    *ptr = ptr2;
    if (ReadFile(ptr2,size,InputFile) != size)
      Error("File error");
  }

  fclose(InputFile);
  InputFile = 0;
  return size;
}

void ExtractData(void)
{
  if (Games[GameIndex].Version > 1)
  {
    strcpy(NameNumber,"0");
    DictionarySize = ReadAndUnpack(&Dictionary,1);
  }
  strcpy(NameNumber,"1");
  CodeSize = ReadAndUnpack(&Code,1);
  strcpy(NameNumber,"2");
  String2Size = ReadAndUnpack(&String2,0);
  strcpy(NameNumber,"3");
  String1Size = ReadAndUnpack(&String1,0);
}

void DoPatches(void)
{
  if (Games[GameIndex].CopyAddr1 != 0x0000)
  {
    char c;

    printf("Should the password protection be removed (y/n)? ");
    c = getchar();
    if (tolower(c) == 'y')
    {
      Code[Games[GameIndex].CopyAddr1 - 0x2A] = Games[GameIndex].CopyByte1;
      if (Games[GameIndex].CopyAddr2 != 0x0000)
        Code[Games[GameIndex].CopyAddr2 - 0x2A] = Games[GameIndex].CopyByte2;
    }
  }

  if (Games[GameIndex].PatchAddr != 0x0000)
  {
    Code[Games[GameIndex].PatchAddr - 0x2A] = Games[GameIndex].PatchByte;
    if (Games[GameIndex].PatchMsg != 0)
      printf(Games[GameIndex].PatchMsg);
  }
}

void WriteLong(HugePtr ptr, unsigned long value)
{
  ptr[3] = (unsigned char)(value & 0xFF);
  value >>= 8;
  ptr[2] = (unsigned char)(value & 0xFF);
  value >>= 8;
  ptr[1] = (unsigned char)(value & 0xFF);
  value >>= 8;
  ptr[0] = (unsigned char)value;
}

void WriteOutputFile(const char* output)
{
  unsigned char header[42];

  WriteLong(header,0x4D615363);
  WriteLong(header+4,
    CodeSize + DictionarySize + String1Size + String2Size + 42);
  WriteLong(header+8,42);
  header[12] = 0;
  header[13] = Games[GameIndex].Version;
  WriteLong(header+14,CodeSize);
  WriteLong(header+18,(String1Size < 0x10000) ? String1Size : 0x10000);
  WriteLong(header+22,(String1Size < 0x10000) ?
    String2Size : String1Size - 0x10000 + String2Size);
  WriteLong(header+26,DictionarySize);
  WriteLong(header+30,String1Size);
  WriteLong(header+34,Games[GameIndex].UndoSize);
  WriteLong(header+38,Games[GameIndex].UndoPC);

  if ((OutputFile = fopen(output,"wb")) == 0)
    Error("Couldn't open output file");

  if (WriteFile(header,42,OutputFile) != 42)
    Error("File error");
  if (WriteFile(Code,CodeSize,OutputFile) != CodeSize)
    Error("File error");
  if (WriteFile(String1,String1Size,OutputFile) != String1Size)
    Error("File error");
  if (WriteFile(String2,String2Size,OutputFile) != String2Size)
    Error("File error");
  if (Games[GameIndex].Version > 1)
  {
    if (WriteFile(Dictionary,DictionarySize,OutputFile) != DictionarySize)
      Error("File error");
  }
  fclose(OutputFile);
  OutputFile = 0;
}

int main(int argc, char** argv)
{
  if (argc == 3)
  {
    strcpy(Name,argv[1]);
    NameNumber = Name + strlen(Name);

    RecogniseGame();
    ExtractData();
    DoPatches();
    WriteOutputFile(argv[2]);
    printf("Game extracted successfully");
  }
  else
  {
    printf("XtractPC v1.0 by Niclas Karlsson and David Kinder.\n"
           "Extractor for the MS-DOS versions of Magnetic Scrolls games.\n\n"
           "Usage: XtractPC game game.mag\n\n"
           "where \"game\" is the full path to the data files, minus any trailing\n"
           "numbers. For example, the data files for \"The Pawn\" are called \"pawn1\"\n"
           "to \"pawn6\". If these files are in a directory \"Magnetic"SEP"ThePawn\", the\n"
           "following use would be valid:\n\n"
           "       XtractPC Magnetic"SEP"ThePawn"SEP"pawn Pawn.mag\n");
  }
  return 0;
}
