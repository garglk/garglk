
/*
    hintlink.c
    Extracter for Magnetic Scrolls hints (Wonderland
    and the MS Collection Volume 1), Amiga versions.
    Written by Stefan Meier.
*/

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#if defined(__MSDOS__) && defined(__BORLANDC__)
extern unsigned _stklen = 0x1000;
#include <alloc.h>
#define malloc farmalloc
#define free farfree
#endif

#define TXTBUFFER_SIZE 32UL*1024UL
#define INDBUFFER_SIZE 8UL*1024UL

int Game = 0;
FILE* txtFile;
FILE* indexFile;
unsigned char* BufferTxt = NULL;
unsigned char* BufferIndex = NULL;
FILE* OutputFile = NULL;
unsigned long OutOffset = 0;

void WriteLong(unsigned char* p, unsigned long v)
{
	p[3] = (unsigned char)(v & 0xFF);
	v >>= 8;
	p[2] = (unsigned char)(v & 0xFF);
	v >>= 8;
	p[1] = (unsigned char)(v & 0xFF);
	v >>= 8;
	p[0] = (unsigned char)v;
}

void WriteShort(unsigned char* p, unsigned short v)
{
	p[1] = (unsigned char)(v & 0xFF);
	v >>= 8;
	p[0] = (unsigned char)v;
}

unsigned long ReadLong(unsigned char *ptr)
{
	return ((unsigned long)ptr[1]) << 24 |
	       ((unsigned long)ptr[0]) << 16 |
	       ((unsigned long)ptr[3]) <<  8 |
	       ((unsigned long)ptr[2]);
}

unsigned short ReadShort(unsigned char *ptr)
{
	return (unsigned short)(ptr[1]<<8 | ptr[0]);
}

void CleanUp(void)
{
	if (txtFile)
		fclose(txtFile);
	if (indexFile)
		fclose(indexFile);
	if (OutputFile)
		fclose(OutputFile);
	if (BufferTxt)
		free(BufferTxt);
	if (BufferIndex)
		free(BufferIndex);
}

void Error(const unsigned char *pError)
{
	fprintf(stderr,"Fatal Error: %s\n",pError);
	CleanUp();
	exit(1);
}

unsigned long ReadFile(FILE* target, unsigned char* buf,unsigned long sz)
{
#if defined(__MSDOS__) && defined(__BORLANDC__)
	unsigned long n = 0xF000, i, j = 0;

	fseek(target,0,SEEK_SET);
	for (i = 0; i < sz; i += n)
	{
		if (sz-i < 0xF000)
			n = sz - i;
		j += fread(buf+i,1,n,InputFile);
	}
	return j;
#else
	fseek(target,0,SEEK_SET);
	return (unsigned long)fread(buf,sz,1,target);
#endif
}

unsigned long WriteFile(unsigned char* buf, unsigned long sz)
{
#if defined(__MSDOS__) && defined(__BORLANDC__)
	unsigned long n = 0xF000, i, j = 0;

	for (i = 0; i < sz; i += n)
	{
		if (sz-i < 0xF000)
			n = sz - i;
		j += fwrite(buf+i,1,n,OutputFile);
	}
	return j;
#else
	return (unsigned long)fwrite(buf,1,sz,OutputFile);
#endif
}


void WriteHintHeader(char * filename)
{
//	struct OutputHintFile out;

	if ((OutputFile = fopen(filename,"wb")) == NULL)
		Error("Cannot open output file");

	WriteFile("MaHt",4);
	OutOffset += 4;
}

void BuildAndWriteHints(void)
{
   char * iWalker;
   unsigned int blkcount,elcnt,ityp,i,j,offset,tlength;
   iWalker = BufferIndex;
   blkcount = ReadShort(iWalker);
   WriteFile(iWalker,2);
   iWalker += 2;
   for (i=0; i < blkcount; i++) {

	  // number of elements in this block
	  elcnt = ReadShort(iWalker);
      WriteFile(iWalker,2);
      iWalker += 2;
      ityp = ReadShort(iWalker);
      WriteFile(iWalker,2);
      iWalker += 2;
      for (j=0; j < elcnt; j++) {
         offset = ReadShort(iWalker);
		 iWalker += 2;
   	     tlength = ReadShort( BufferTxt+offset );
         WriteFile(BufferTxt+offset,2);
		 WriteFile(BufferTxt+offset+2,tlength);
   	  }
      if (	ityp == 1 ) {
		  WriteFile(iWalker, elcnt*2);
		  iWalker += elcnt*2;
   	  }
	  WriteFile(iWalker, 2);
	  iWalker += 2;
   }
   
}

void WriteHintFile(void)
{
	BufferTxt = malloc(TXTBUFFER_SIZE);
	if (BufferTxt == NULL)
		Error("Not enough memory");

    // load Text file
	ReadFile(txtFile,BufferTxt,TXTBUFFER_SIZE);

	BufferIndex = malloc(INDBUFFER_SIZE);
	if (BufferIndex == NULL)
		Error("Not enough memory");

    // load index file
	ReadFile(indexFile,BufferIndex,INDBUFFER_SIZE);

	BuildAndWriteHints();
}

void OpenFiles( char * iFile, char * tFile) {

	if ((indexFile = fopen(iFile,"rb")) == NULL)
		Error("Cannot open index file");

	if ((txtFile = fopen(tFile,"rb")) == NULL)
		Error("Cannot open txt file");
}


int main(int argc, char** argv)
{
	if (argc == 4)
	{
		OpenFiles(argv[1],argv[2]);
		WriteHintHeader(argv[3]);
		WriteHintFile();
		CleanUp();
		printf("Hints extracted successfully");
	}
	else
	{
		printf("HintLink v1.0 by Stefan Meier.\n\n"
		       "Extractor for the hints in the Magnetic Windows versions of\n"
		       "Magnetic Scrolls games (Wonderland and the MS Collection Volume 1).\n"
		       "Usage: HintLink index text result\n\n");
	}
	return 0;
}

