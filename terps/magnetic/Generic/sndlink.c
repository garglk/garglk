
/*
    SndLink.c
    Extracter for Wonderland music scores
    Amiga, Atari ST and PC versions.
    Written by Stefan Meier.

	v1.1 added option for tempo patching
	v1.2 fixed tempo patching
	v1.3 remove garbage bytes at end of original data
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 64UL*1024UL

struct OutputSndFile
{
	char Name[8];
	unsigned char tempo[2];
	unsigned char Offset[4];
	unsigned char Length[4];
};

struct SndFile
{
	char Name[8];
	unsigned long Offset;
	unsigned long Length;
};

struct SndFile* SndFiles = NULL;
int SndFileCount = 0;
unsigned char* Buffer1 = NULL;
unsigned char* Buffer2 = NULL;
FILE* InputFile = NULL;
FILE* OutputFile = NULL;
unsigned long OutOffset = 0;
int tempopatch=0;
int garbagefix=0;

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

void WriteShort2(unsigned char* p, unsigned short v)
{
	p[0] = (unsigned char)(v & 0xFF);
	v >>= 8;
	p[1] = (unsigned char)v;
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

unsigned short ReadShort2(unsigned char *ptr)
{
	return (unsigned short)(ptr[0]<<8 | ptr[1]);
}

void CleanUp(void)
{
	if (InputFile)
		fclose(InputFile);
	if (OutputFile)
		fclose(OutputFile);
	if (SndFiles)
		free(SndFiles);
	if (Buffer1)
		free(Buffer1);
}

void Error(const unsigned char *pError)
{
	fprintf(stderr,"Fatal Error: %s\n",pError);
	CleanUp();
	exit(1);
}

unsigned long ReadFile(unsigned char* buf, unsigned long* offset, unsigned long sz)
{
	fseek(InputFile,*offset,SEEK_SET);
	*offset += sz;
	return fread(buf,1,sz,InputFile);
}

unsigned long WriteFile(unsigned char* buf, unsigned long sz)
{
	return fwrite(buf,1,sz,OutputFile);
}

void OpenFile(const char* pszName)
{
	unsigned char buffer[4];
	unsigned long offset = 0;
	unsigned long id = 0;

	if ((InputFile = fopen(pszName,"rb")) == NULL)
		Error("Cannot open resource file");

	ReadFile(buffer,&offset,4);
}

void FindResourceNames(void)
{
	unsigned char buffer[32];
	unsigned long offset = 0;
	int count, i = 0;

	ReadFile(buffer,&offset,4);
	offset = ((long)buffer[0] <<  0) |
	         ((long)buffer[1] <<  8) |
	         ((long)buffer[2] << 16) |
	         ((long)buffer[3] << 24);
	ReadFile(buffer,&offset,2);
	count = ((int)buffer[0] << 0) |
	        ((int)buffer[1] << 8);

	SndFiles = calloc(count,sizeof(struct SndFile));

	for (i = 0; i < count; i++)
	{
		char name[8];
		unsigned long o, l;
		unsigned int n;
		int j;

		ReadFile(buffer,&offset,18);
		o = ((long)buffer[2] <<  0) |
		    ((long)buffer[3] <<  8) |
		    ((long)buffer[4] << 16) |
		    ((long)buffer[5] << 24);
		l = ((long)buffer[6] <<  0) |
		    ((long)buffer[7] <<  8) |
		    ((long)buffer[8] << 16) |
		    ((long)buffer[9] << 24);
		n = ((unsigned)buffer[16] << 0) |
		    ((unsigned)buffer[17] << 8);

		if (n == 4)
		{
			for (j = 0; (j < 6) && (buffer[10+j] != 0); j++)
				name[j] = buffer[10+j];
			name[j] = 0;

			if (name[0]=='t' && name[1]=='-')
			{
			   strcpy(SndFiles[SndFileCount].Name,name);
			   SndFiles[SndFileCount].Offset = o;
			   SndFiles[SndFileCount].Length = l;

			   SndFileCount++;
			}
		}
	}
}

void WriteSndHeader1(void)
{
	struct OutputSndFile out;
	unsigned char zeros[2] = { 0,0 };
	int i;

		if ((OutputFile = fopen("wonder.snd","wb")) == NULL)
			Error("Cannot open output file");

		WriteFile("MaSd",4);
		OutOffset += 4;

		WriteFile(zeros,2);
		OutOffset += 2;

		memset(&out,0,sizeof(struct OutputSndFile));
		for (i = 0; i < SndFileCount; i++)
			WriteFile((unsigned char*)&out,sizeof(struct OutputSndFile));
		OutOffset += (SndFileCount * sizeof(struct OutputSndFile));
}

void WriteSndFile(int Index)
{
	unsigned long offset, length, j = 0;
	unsigned short tsize;
	int game = -1;

			offset = SndFiles[Index].Offset;
			length = SndFiles[Index].Length;
			if (length > BUFFER_SIZE)
				length = BUFFER_SIZE;
			ReadFile(Buffer1,&offset,length);

			// Remove garbage bytes if present
			if (garbagefix)
			{
				while ((j <= length-1) && !(Buffer1[j]==0xFF && Buffer1[j+1]==0x2F && Buffer1[j+2]==0x00))
				{
					j++;
				}
				if (j+3 != length)
				{
					SndFiles[Index].Length = j+3;
					length=j+3;
				}
			}


			// Tempo patching
			if (tempopatch)
			{
				memset(Buffer2,0x00, BUFFER_SIZE);
				SndFiles[Index].Length=SndFiles[Index].Length+7;
                memcpy(Buffer2,Buffer1,0x19);
				Buffer2[0x1A]=0x00;Buffer2[0x1B]=0xFF;Buffer2[0x1C]=0x51;Buffer2[0x1D]=0x03;
				memcpy(Buffer2+0x19+7,Buffer1+0x19,length-0x19);
				// Calculate new track size
				tsize = ReadShort2(Buffer2+0x14);
				WriteShort(Buffer2+0x14,tsize+7);
				if (strncmp(SndFiles[Index].Name,"t-mus",5)==0)
				{ // Music
					// Tempo event 05 16 15 = 180
					Buffer2[0x1E]=0x05;Buffer2[0x1F]=0x16;Buffer2[0x20]=0x15;
				}
				else if (strncmp(SndFiles[Index].Name,"t-cat",5)==0)
				{ // Catter
					// Tempo event 0A 2C 2A = 90
					Buffer2[0x1E]=0x0A;Buffer2[0x1F]=0x2C;Buffer2[0x20]=0x2A;
				}
				else if (strncmp(SndFiles[Index].Name,"t-croq",6)==0)
				{ // Croque
					// Tempo event 07 A1 20 = 120
					Buffer2[0x1E]=0x07;Buffer2[0x1F]=0xA1;Buffer2[0x20]=0x20;
				}
				else if (strncmp(SndFiles[Index].Name,"t-pal",5)==0)
				{ // enthal
					// Tempo event 07 A1 20 = 120
					Buffer2[0x1E]=0x07;Buffer2[0x1F]=0xA1;Buffer2[0x20]=0x20;
				}
				else if (strncmp(SndFiles[Index].Name,"t-madt",6)==0)
				{ // dormou
					// Tempo event 07 A1 20 = 120
					Buffer2[0x1E]=0x07;Buffer2[0x1F]=0xA1;Buffer2[0x20]=0x20;
				}
				else if (strncmp(SndFiles[Index].Name,"t-crt",5)==0)
				{ // court
					// Tempo event 09 27 C0 = 100
					Buffer2[0x1E]=0x09;Buffer2[0x1F]=0x27;Buffer2[0x20]=0xC0;
				}
				WriteFile(Buffer2,length+7);
			}
			else
			{
				WriteFile(Buffer1,length);
			}

}

void WriteSndFiles(void)
{
	int i;

	Buffer1 = malloc(BUFFER_SIZE);
	if (Buffer1 == NULL)
		Error("Not enough memory");
	if (tempopatch)
	{
		Buffer2 = malloc(BUFFER_SIZE);
		if (Buffer2 == NULL)
			Error("Not enough memory");
	}

	for (i = 0; i < SndFileCount; i++)
		WriteSndFile(i);
}

void WriteSndHeader2(void)
{
	struct OutputSndFile out;
	unsigned char header_size[2];
	int i, pos, game = -1;
	int table = 0;

	fseek(OutputFile,4,SEEK_SET);
	WriteShort(header_size,(unsigned short)(SndFileCount * sizeof(struct OutputSndFile)));
	WriteFile(header_size,2);

	for (i = 0; i < SndFileCount; i++)
	{
			memset(&out,0,sizeof(struct OutputSndFile));
			strcpy(out.Name,SndFiles[i].Name);
			if (strcmp(SndFiles[i].Name,"t-mus")==0)
			{
			   //strcpy(out.Name,"music");
			   WriteShort(out.tempo,180);
			}
			else if (strcmp(SndFiles[i].Name,"t-cat")==0)
			{
			   //strcpy(out.Name,"catter");
			   WriteShort(out.tempo,95);
			}
			else if (strcmp(SndFiles[i].Name,"t-croq")==0)
			{
			   //strcpy(out.Name,"croque");
			   WriteShort(out.tempo,120);
			}
			else if (strcmp(SndFiles[i].Name,"t-crt")==0)
			{
			   //strcpy(out.Name,"court");
			   WriteShort(out.tempo,100);
			}
			else if (strcmp(SndFiles[i].Name,"t-pal")==0)
			{
			   //strcpy(out.Name,"enthal");
			   WriteShort(out.tempo,120);
			}
			else if (strcmp(SndFiles[i].Name,"t-madt")==0)
			{
			   //strcpy(out.Name,"dormou");
			   WriteShort(out.tempo,120);
			}
			WriteLong(out.Offset,OutOffset);
			WriteLong(out.Length,SndFiles[i].Length);

			pos = 6 + (table * sizeof(struct OutputSndFile));
			fseek(OutputFile,pos,SEEK_SET);
			WriteFile((unsigned char*)&out,sizeof(struct OutputSndFile));

			OutOffset += SndFiles[i].Length;
			table++;
	}
}

int main(int argc, char** argv)
{
	if (argc == 2 || 
		((argc == 3 && ((strcmp(argv[1],"-p")==0) || strcmp(argv[1],"-r")==0))) ||
		((argc == 4 && ((strcmp(argv[1],"-p")==0) && strcmp(argv[2],"-r")==0))))
	{
		if (argc==2)
		   OpenFile(argv[1]);
		else if (argc==2)
		{
			if (strcmp(argv[1],"-p")==0)
				tempopatch = 1;
			if (strcmp(argv[1],"-r")==0)
				garbagefix = 1;
			OpenFile(argv[2]);
		}
		else
		{
			tempopatch = 1;
			garbagefix = 1;
			OpenFile(argv[3]);
		}
		FindResourceNames();
		WriteSndHeader1();
		WriteSndFiles();
		WriteSndHeader2();
		CleanUp();
		printf("Music extracted successfully");
	}
	else
	{
		printf("SndLink v1.3 by Stefan Meier.\n\n"
		       "Extractor for the music scores in Magnetic Scrolls' Wonderland\n"
		       "Amiga, Atari ST, PC versions.\n\n"
			   "Usage: SndLink [-p] [-r] all.rsc\n\n"
		       "\"all.rsc\" is taken from an installed game. Depending on your\n"
			   "game version, the resource file might be split into several files\n"
			   "named e.g. all.1, all.2,... or TWO,THREE,FOUR...\n"
		       "Before running the extractor, you need to merge these parts into\n"
			   "one file, e.g. with the DOS command copy /B ONE+TWO+THREE+... all.rsc\n"
		       "If the extraction is successfull, the file wonder.snd is created\n\n"
			   "The optional -p switch adds tempo data to the music score\n"
			   "The optional -r switch removes garbage bytes from the PC versions");
	}
	return 0;
}

