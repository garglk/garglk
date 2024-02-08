
/*
    GfxLink2.c
    Extracter for Magnetic Scrolls pictures (Wonderland
    and the MS Collection Volume 1), Amiga versions.
    Written by David Kinder.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MSDOS__) && defined(__BORLANDC__)
extern unsigned _stklen = 0x1000;
#include <alloc.h>
#define malloc farmalloc
#define free farfree
#endif

#define BUFFER_SIZE 32UL*1024UL

#define MS_WONDERLAND 1
#define MS_COLLECTION 2

struct OutputGfxFile
{
	char Name[8];
	unsigned char Offset[4];
	unsigned char Length[4];
};

struct GfxFile
{
	char Name[8];
	unsigned long Offset;
	unsigned long Length;
};

int Game = 0;
struct GfxFile* GfxFiles = NULL;
int GfxFileCount = 0;
int GfxSubCount[3] = { 0, 0, 0 };
unsigned char* Buffer1 = NULL;
FILE* InputFile = NULL;
FILE* OutputFile[3] = { NULL, NULL, NULL };
unsigned long OutOffset[3] = { 0, 0, 0 };

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
	int i;

	if (InputFile)
		fclose(InputFile);
	for (i = 0; i < 3; i++)
	{
		if (OutputFile[i])
			fclose(OutputFile[i]);
	}
	if (GfxFiles)
		free(GfxFiles);
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
#if defined(__MSDOS__) && defined(__BORLANDC__)
	unsigned long n = 0xF000, i, j = 0;

	fseek(InputFile,*offset,SEEK_SET);
	for (i = 0; i < sz; i += n)
	{
		if (sz-i < 0xF000)
			n = sz - i;
		j += fread(buf+i,1,n,InputFile);
	}
	*offset += sz;
	return j;
#else
	fseek(InputFile,*offset,SEEK_SET);
	*offset += sz;
	return fread(buf,1,sz,InputFile);
#endif
}

unsigned long WriteFile(unsigned char* buf, unsigned long sz, int file)
{
#if defined(__MSDOS__) && defined(__BORLANDC__)
	unsigned long n = 0xF000, i, j = 0;

	for (i = 0; i < sz; i += n)
	{
		if (sz-i < 0xF000)
			n = sz - i;
		j += fwrite(buf+i,1,n,OutputFile[file]);
	}
	return j;
#else
	return fwrite(buf,1,sz,OutputFile[file]);
#endif
}

void OpenFile(const char* pszName)
{
	unsigned char buffer[4];
	unsigned long offset = 0;
	unsigned long id = 0;

	if ((InputFile = fopen(pszName,"rb")) == NULL)
		Error("Cannot open resource file");

	ReadFile(buffer,&offset,4);
	id = ReadLong(buffer);
	switch (id)
	{
	case 0xA34F0026:
		Game = MS_WONDERLAND;
		printf("Wonderland detected\n");
		break;
	case 0x7D140021:
		Game = MS_COLLECTION;
		printf("Collection Volume 1 detected\n");
		break;
	default:
		Error("Game not recognised");
		break;
	}
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

	GfxFiles = calloc(count,sizeof(struct GfxFile));

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

		if (n == 11)
		{
			for (j = 0; (j < 6) && (buffer[10+j] != 0); j++)
				name[j] = buffer[10+j];
			name[j] = 0;

			strcpy(GfxFiles[GfxFileCount].Name,name);
			GfxFiles[GfxFileCount].Offset = o;
			GfxFiles[GfxFileCount].Length = l;

			/* Corrections for specific pictures */
			if (Game == MS_WONDERLAND)
			{
				if (strcmp(name,"frog") == 0)
					GfxFiles[GfxFileCount].Length += 128UL*1024UL;
				else if (strcmp(name,"croque") == 0)
					GfxFiles[GfxFileCount].Length += 128UL*1024UL;
			}

			GfxFileCount++;
		}
	}
}

void WriteGfxHeader1(void)
{
	struct OutputGfxFile out;
	unsigned char zeros[2] = { 0,0 };
	int i;

	switch (Game)
	{
	case MS_WONDERLAND:
		if ((OutputFile[0] = fopen("wonder.gfx","wb")) == NULL)
			Error("Cannot open output file");

		WriteFile("MaP2",4,0);
		OutOffset[0] += 4;

		WriteFile(zeros,2,0);
		OutOffset[0] += 2;

		memset(&out,0,sizeof(struct OutputGfxFile));
		for (i = 0; i < GfxFileCount; i++)
			WriteFile((unsigned char*)&out,sizeof(struct OutputGfxFile),0);
		OutOffset[0] += (GfxFileCount * sizeof(struct OutputGfxFile));
		break;
	case MS_COLLECTION:
		if ((OutputFile[0] = fopen("corrupt.gfx","wb")) == NULL)
			Error("Cannot open output file");
		if ((OutputFile[1] = fopen("fish.gfx","wb")) == NULL)
			Error("Cannot open output file");
		if ((OutputFile[2] = fopen("guild.gfx","wb")) == NULL)
			Error("Cannot open output file");

		for (i = 0; i < 3; i++)
		{
			WriteFile("MaP2",4,i);
			OutOffset[i] += 4;

			WriteFile(zeros,2,i);
			OutOffset[i] += 2;
		}

		memset(&out,0,sizeof(struct OutputGfxFile));
		for (i = 0; i < GfxFileCount; i++)
		{
			int game = -1;

			if (tolower(GfxFiles[i].Name[0]) == 'c')
				game = 0;
			else if (tolower(GfxFiles[i].Name[0]) == 'f')
				game = 1;
			else if (tolower(GfxFiles[i].Name[0]) == 'g')
				game = 2;

			if (game >= 0)
			{
				WriteFile((unsigned char*)&out,sizeof(struct OutputGfxFile),game);
				OutOffset[game] += sizeof(struct OutputGfxFile);
				GfxSubCount[game]++;
			}
		}
		break;
	}
}

void WriteGfxFile(int Index)
{
	unsigned long position = 0;
	unsigned long offset, length, patch_offset[2];
	unsigned char patch_bytes[2];
	int game = -1, patch_count = 0, i;

	switch (Game)
	{
	case MS_WONDERLAND:
		game = 0;
		if (strcmp(GfxFiles[Index].Name,"rablan") == 0)
		{
			patch_offset[0] = 0x1ED0;
			patch_bytes[0] = 0xD0;
			patch_offset[1] = 0x1ED1;
			patch_bytes[1] = 0x5E;
			patch_count = 2;
		}
		else if (strcmp(GfxFiles[Index].Name,"rbrige") == 0)
		{
			patch_offset[0] = 0x47E8;
			patch_bytes[0] = 0x00;
			patch_count = 1;
		}
		break;
	case MS_COLLECTION:
		if (tolower(GfxFiles[Index].Name[0]) == 'c')
			game = 0;
		else if (tolower(GfxFiles[Index].Name[0]) == 'f')
			game = 1;
		else if (tolower(GfxFiles[Index].Name[0]) == 'g')
			game = 2;
		break;
	}	

	if (game >= 0)
	{
		while (position < GfxFiles[Index].Length)
		{
			offset = GfxFiles[Index].Offset + position;
			length = GfxFiles[Index].Length - position;
			if (length > BUFFER_SIZE)
				length = BUFFER_SIZE;
			ReadFile(Buffer1,&offset,length);

			for (i = 0; i < patch_count; i++)
			{
				if ((position <= patch_offset[i]) && (position + length > patch_offset[i]))
					Buffer1[patch_offset[i] - position] = patch_bytes[i];
			}

			WriteFile(Buffer1,length,game);
			position += length;
		}
	}
}

void WriteGfxFiles(void)
{
	int i;

	Buffer1 = malloc(BUFFER_SIZE);
	if (Buffer1 == NULL)
		Error("Not enough memory");

	for (i = 0; i < GfxFileCount; i++)
		WriteGfxFile(i);
}

void WriteGfxHeader2(void)
{
	struct OutputGfxFile out;
	unsigned char header_size[2];
	int i, pos, game = -1;
	int table[3] = { 0, 0, 0 };

	switch (Game)
	{
	case MS_WONDERLAND:
		fseek(OutputFile[0],4,SEEK_SET);
		WriteShort(header_size,(unsigned short)(GfxFileCount * sizeof(struct OutputGfxFile)));
		WriteFile(header_size,2,0);
		break;
	case MS_COLLECTION:
		for (i = 0; i < 3; i++)
		{
			fseek(OutputFile[i],4,SEEK_SET);
			WriteShort(header_size,(unsigned short)(GfxSubCount[i] * sizeof(struct OutputGfxFile)));
			WriteFile(header_size,2,i);
		}
		break;
	}

	for (i = 0; i < GfxFileCount; i++)
	{
		switch (Game)
		{
		case MS_WONDERLAND:
			game = 0;
			break;
		case MS_COLLECTION:
			if (tolower(GfxFiles[i].Name[0]) == 'c')
				game = 0;
			else if (tolower(GfxFiles[i].Name[0]) == 'f')
				game = 1;
			else if (tolower(GfxFiles[i].Name[0]) == 'g')
				game = 2;
			break;
		}	

		if (game >= 0)
		{
			memset(&out,0,sizeof(struct OutputGfxFile));
			strcpy(out.Name,GfxFiles[i].Name);
			WriteLong(out.Offset,OutOffset[game]);
			WriteLong(out.Length,GfxFiles[i].Length);

			pos = 6 + (table[game] * sizeof(struct OutputGfxFile));
			fseek(OutputFile[game],pos,SEEK_SET);
			WriteFile((unsigned char*)&out,sizeof(struct OutputGfxFile),game);

			OutOffset[game] += GfxFiles[i].Length;
			table[game]++;
		}
	}
}

int main(int argc, char** argv)
{
	if (argc == 2)
	{
		OpenFile(argv[1]);
		FindResourceNames();
		WriteGfxHeader1();
		WriteGfxFiles();
		WriteGfxHeader2();
		CleanUp();
		printf("Graphics extracted successfully");
	}
	else
	{
		printf("GfxLink2 v1.0 by David Kinder.\n\n"
		       "Extractor for the pictures in the Magnetic Windows versions of\n"
		       "Magnetic Scrolls games (Wonderland and the MS Collection Volume 1),\n"
		       "Amiga versions.\n\n"
		       "Usage: GfxLink2 user.rsc\n\n"
		       "\"user.rsc\" is taken from an Amiga Magnetic Scrolls installation\n"
		       "in which the option to expand all the graphics files has been\n"
		       "selected. For Wonderland there is only one output file\n"
		       "(\"wonder.gfx\"), for the Collection there are three (\"corrupt.gfx\",\n"
		       "\"fish.gfx\" and \"guild.gfx\").\n");
	}
	return 0;
}

