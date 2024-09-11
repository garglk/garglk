/***********************************************************************\
*
* Level 9 interpreter
* Version 5.1
* Copyright (c) 1996-2011 Glen Summers and contributors.
* Contributions from David Kinder, Alan Staniforth, Simon Baldwin,
* Dieter Baron and Andreas Scherrer.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
*
\***********************************************************************/

typedef unsigned char L9BYTE;
typedef unsigned short L9UINT16;
typedef unsigned long L9UINT32;
typedef int L9BOOL;

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define LISTAREASIZE 0x800
#define STACKSIZE 1024
#define V1FILESIZE 0x600

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

typedef struct
{
	L9UINT32 Id;
	L9UINT16 codeptr,stackptr,listsize,stacksize,filenamesize,checksum;
	L9UINT16 vartable[256];
	L9BYTE listarea[LISTAREASIZE];
	L9UINT16 stack[STACKSIZE];
	char filename[MAX_PATH];
} GameState;

typedef enum
{
	NO_BITMAPS,
	AMIGA_BITMAPS,
	PC1_BITMAPS,
	PC2_BITMAPS,
	C64_BITMAPS,
	BBC_BITMAPS,
	CPC_BITMAPS,
	MAC_BITMAPS,
	ST1_BITMAPS,
	ST2_BITMAPS,
} BitmapType;

typedef struct
{
	L9BYTE red, green, blue;
} Colour;

typedef struct
{
	L9UINT16 width, height;
	L9BYTE* bitmap;
	Colour palette[32];
	L9UINT16 npalette;
} Bitmap;

#define MAX_BITMAP_WIDTH 512
#define MAX_BITMAP_HEIGHT 218

#if defined(_Windows) || defined(__MSDOS__) || defined (_WIN32) || defined (__WIN32__)
	#define L9WORD(x) (*(L9UINT16*)(x))
	#define L9SETWORD(x,val) (*(L9UINT16*)(x)=(L9UINT16)val)
	#define L9SETDWORD(x,val) (*(L9UINT32*)(x)=val)
#else
	#define L9WORD(x) (*(x) + ((*(x+1))<<8))
	#define L9SETWORD(x,val) *(x)=(L9BYTE) val; *(x+1)=(L9BYTE)(val>>8);
	#define L9SETDWORD(x,val) *(x)=(L9BYTE)val; *(x+1)=(L9BYTE)(val>>8); *(x+2)=(L9BYTE)(val>>16); *(x+3)=(L9BYTE)(val>>24);
#endif

#if defined(_Windows) && !defined(__WIN32__)
#include <alloc.h>
#define malloc farmalloc
#define calloc farcalloc
#define free farfree
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* routines provided by os dependent code */
void os_printchar(char c);
L9BOOL os_input(char* ibuff, int size);
char os_readchar(int millis);
L9BOOL os_stoplist(void);
void os_flush(void);
L9BOOL os_save_file(L9BYTE* Ptr, int Bytes);
L9BOOL os_load_file(L9BYTE* Ptr, int* Bytes, int Max);
L9BOOL os_get_game_file(char* NewName, int Size);
void os_set_filenumber(char* NewName, int Size, int n);
void os_graphics(int mode);
void os_cleargraphics(void);
void os_setcolour(int colour, int index);
void os_drawline(int x1, int y1, int x2, int y2, int colour1, int colour2);
void os_fill(int x, int y, int colour1, int colour2);
void os_show_bitmap(int pic, int x, int y);
FILE* os_open_script_file(void);

/* routines provided by level9 interpreter */
L9BOOL LoadGame(char* filename, char* picname);
L9BOOL RunGame(void);
void StopGame(void);
void RestoreGame(char* filename);
void FreeMemory(void);
void GetPictureSize(int* width, int* height);
L9BOOL RunGraphics(void);

/* bitmap routines provided by level9 interpreter */
BitmapType DetectBitmaps(char* dir);
Bitmap* DecodeBitmap(char* dir, BitmapType type, int num, int x, int y);

#ifdef NEED_STRICMP_PROTOTYPE
int stricmp(const char* str1, const char* str2);
int strnicmp(const char* str1, const char* str2, size_t n);
#endif

#ifdef __cplusplus
}
#endif
