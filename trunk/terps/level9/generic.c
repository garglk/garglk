/***********************************************************************\
*
* Level 9 interpreter
* Version 4.1
* Copyright (c) 1996 Glen Summers
* Copyright (c) 2002,2003,2005,2007 Glen Summers and David Kinder
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

#include <stdio.h>
#include <string.h>
#include "level9.h"

#define TEXTBUFFER_SIZE 10240
char TextBuffer[TEXTBUFFER_SIZE+1];
int TextBufferPtr = 0;

int Column = 0;
#define SCREENWIDTH 76

void os_printchar(char c)
{
	if (c == '\r')
	{
		os_flush();
		putchar('\n');
		Column = 0;
		return;
	}
	if (isprint(c) == 0) return;
	if (TextBufferPtr >= TEXTBUFFER_SIZE) os_flush();
	*(TextBuffer + (TextBufferPtr++)) = c;
}

L9BOOL os_input(char *ibuff, int size)
{
char *nl;

	os_flush();
	fgets(ibuff, size, stdin);
	nl = strchr(ibuff, '\n');
	if (nl) *nl = 0;
}

char os_readchar(int millis)
{
	static int count = 0;
	char c;

	os_flush();
	if (millis == 0) return 0;

	if (++count < 1024)
		return 0;/* fake no keypress on occasions */
	count = 0;

	c = getc(stdin); /* will require enter key as well */
	if (c != '\n')
	{
		while (getc(stdin) != '\n')
			;/* unbuffer input until enter key */
	}

	return c;
}

L9BOOL os_stoplist(void)
{
	return FALSE;
}

void os_flush(void)
{
static int semaphore = 0;
int ptr, space, lastspace, searching;

	if (TextBufferPtr < 1) return;
	if (semaphore) return;
	semaphore = 1;

	*(TextBuffer+TextBufferPtr) = ' ';
	ptr = 0;
	while (TextBufferPtr + Column > SCREENWIDTH)
	{
		space = ptr;
		lastspace = space;
		searching = 1;
		while (searching)
		{
			while (TextBuffer[space] != ' ') space++;
			if (space - ptr + Column > SCREENWIDTH)
			{
				space = lastspace;
				printf("%.*s\n", space - ptr, TextBuffer + ptr);
				Column = 0;
				space++;
				if (TextBuffer[space] == ' ') space++;
				TextBufferPtr -= (space - ptr);
				ptr = space;
				searching = 0;
			}
			else lastspace = space;
			space++;
		}
	}
	printf("%.*s", TextBufferPtr, TextBuffer + ptr);
	Column += TextBufferPtr;
	TextBufferPtr = 0;

	semaphore = 0;
}

L9BOOL os_save_file(L9BYTE * Ptr, int Bytes)
{
char name[256];
char *nl;
FILE *f;

	os_flush();
	printf("Save file: ");
	fgets(name, 256, stdin);
	nl = strchr(name, '\n');
	if (nl) *nl = 0;
	f = fopen(name, "wb");
	if (!f) return FALSE;
	fwrite(Ptr, 1, Bytes, f);
	fclose(f);
	return TRUE;
}

L9BOOL os_load_file(L9BYTE *Ptr,int *Bytes,int Max)
{
char name[256];
char *nl;
FILE *f;

	os_flush();
	printf("Load file: ");
	fgets(name, 256, stdin);
	nl = strchr(name, '\n');
	if (nl) *nl = 0;
	f = fopen(name, "rb");
	if (!f) return FALSE;
	*Bytes = fread(Ptr, 1, Max, f);
	fclose(f);
	return TRUE;
}

L9BOOL os_get_game_file(char *NewName,int Size)
{
char *nl;

	os_flush();
	printf("Load next game: ");
	fgets(NewName, Size, stdin);
	nl = strchr(NewName, '\n');
	if (nl) *nl = 0;
	return TRUE;
}

void os_set_filenumber(char *NewName,int Size,int n)
{
char *p;
int i;

#if defined(_Windows) || defined(__MSDOS__) || defined (_WIN32) || defined (__WIN32__)
	p = strrchr(NewName,'\\');
#else
	p = strrchr(NewName,'/');
#endif
	if (p == NULL) p = NewName;
	for (i = strlen(p)-1; i >= 0; i--)
	{
		if (isdigit(p[i]))
		{
			p[i] = '0'+n;
			return;
		}
	}
}

void os_graphics(int mode)
{
}

void os_cleargraphics(void)
{
}

void os_setcolour(int colour, int index)
{
}

void os_drawline(int x1, int y1, int x2, int y2, int colour1, int colour2)
{
}

void os_fill(int x, int y, int colour1, int colour2)
{
}

void os_show_bitmap(int pic, int x, int y)
{
}

int main(int argc, char **argv)
{
	printf("Level 9 Interpreter\n\n");
	if (argc != 2)
	{
		printf("Syntax: %s <gamefile>\n",argv[0]);
		return 0;
	}
	if (!LoadGame(argv[1],NULL))
	{
		printf("Error: Unable to open game file\n");
		return 0;
	}
	while (RunGame());
	StopGame();
	FreeMemory();
	return 0;
}

