/****************************************************************************\
*
* Magnetic - Magnetic Scrolls Interpreter.
*
* Written by Niclas Karlsson <nkarlsso@abo.fi>,
*            David Kinder <davidk@davidkinder.co.uk>,
*            Stefan Meier <Stefan.Meier@if-legends.org> and
*            Paul David Doherty <pdd@if-legends.org>
*
* Copyright (C) 1997-2008  Niclas Karlsson
*
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
*
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
*
*     Simple ANSI interface main.c
*
\****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"

#define WIDTH 78

type8 buffer[80], xpos = 0, bufpos = 0, log_on = 0, ms_gfx_enabled, filename[256];
FILE *logfile1 = 0, *logfile2 = 0;

type8 ms_load_file(type8s *name, type8 *ptr, type16 size)
{
	FILE *fh;
	type8s *realname;

	if (name)
		realname = name;
	else
	{
		do
		{
			printf("Filename: ");
		}
		while (!gets(filename));
		realname = filename;
	}
	if (!(fh=fopen(realname,"rb")))
		return 1;
	if (fread(ptr,1,size,fh) != size)
		return 1;
	fclose(fh);
	return 0;
}

type8 ms_save_file(type8s *name, type8 *ptr, type16 size)
{
	FILE *fh;
	type8s *realname;

	if (name)
		realname = name;
	else
	{
		do
		{
			printf("Filename: ");
		}
		while (!gets(filename));
		realname = filename;
	}
	if (!(fh = fopen(realname,"wb")))
		return 1;
	if (fwrite(ptr,1,size,fh) != size)
		return 1;
	fclose(fh);
	return 0;
}

void script_write(type8 c)
{
	if (log_on == 2 && fputc(c,logfile1) == EOF)
	{
		printf("[Problem with script file - closing]\n");
		fclose(logfile1);
		log_on = 0;
	}
}

void transcript_write(type8 c)
{
	if (logfile2 && c == 0x08 && ftell(logfile2) > 0)
		fseek(logfile2,-1,SEEK_CUR);
	else if (logfile2 && fputc(c,logfile2) == EOF)
	{
		printf("[Problem with transcript file - closing]\n");
		fclose(logfile2);
		logfile2 = 0;
	}
}

void ms_statuschar(type8 c)
{
	static type8 x=0;

	if (c == 0x09)
	{
		while (x + 11 < WIDTH)
		{
			putchar(0x20);
			x++;
		}
		return;
	}
	if (c == 0x0a)
	{
		x = 0;
		putchar(0x0a);
		return;
	}
	printf("\x1b[32m%c\x1b[31m",c);
	x++;
}

void ms_flush(void)
{
	type8 j;

	if (!bufpos)
		return;
	if (xpos + bufpos > WIDTH)
	{
		putchar(0x0a);
		transcript_write(0x0a);
		xpos = 0;
	}
	for (j = 0; j < bufpos; j++)
	{
		if (buffer[j] == 0x0a)
			xpos = 0;
		if (buffer[j] == 0x08)
			xpos -= 2;
		putchar(buffer[j]);
		transcript_write(buffer[j]);
		xpos++;
	}
	bufpos = 0;
}

void ms_putchar(type8 c)
{
/*
	if (c == 0x08)
	{
		if (bufpos > 0)
			bufpos--;
		return;
	}
*/
	buffer[bufpos++] = c;
	if ((c == 0x20) || (c == 0x0a) || (bufpos >= 80))
		ms_flush();
}

type8 ms_getchar(type8 trans)
{
	static type8 buf[256];
	static type16 pos=0;
	int c;
	type8 i;

	if (!pos)
	{
		/* Read new line? */
		i = 0;
		while (1)
		{
			if (log_on == 1)
			{
				/* Reading from logfile */
				if ((c = fgetc(logfile1)) == EOF)
				{
					/* End of log? - turn off */
					log_on = 0;
					fclose(logfile1);
					c = getchar();
				}
				else printf("%c",c); /* print the char as well */
			}
			else
			{
				c = getchar();
				if (c == '#' && !i && trans)
				{
					/* Interpreter command? */
					while ((c = getchar()) != '\n' && c != EOF && i < 255)
						buf[i++] = c;
					buf[i] = 0;
					c = '\n'; /* => Prints new prompt */
					i = 0;
					if (!strcmp(buf,"logoff") && log_on == 2)
					{
						printf("[Closing script file]\n");
						log_on = 0;
						fclose(logfile1);
					}
					else if (!strcmp(buf,"undo"))
						c = 0;
					else
						printf("[Nothing done]\n");
				}
			}
			script_write((type8)c);
			if (c != '\n')
				transcript_write((type8)c);
			if (c == '\n' || c == EOF || i == 255)
				break;
			buf[i++] = c;
			if (!c)
				break;
		}
		buf[i] = '\n';
	}
	if ((c = buf[pos++]) == '\n' || !c)
		pos = 0;
	return (type8)c;
}

void ms_showpic(type32 c,type8 mode)
{
/* Insert your favourite picture viewing code here
   mode: 0 gfx off, 1 gfx on (thumbnails), 2 gfx on (normal) */

/*
	printf("Display picture [%d]\n",c);
*/

/* Small bitmap retrieving example */

/*
	{
		type16 w, h, pal[16];
		type8 *raw = 0, i;

		raw = ms_extract(c,&w,&h,pal,0);
		printf("\n\nExtract: [%d] %dx%d",c,w,h);
		for (i = 0; i < 16; i++)
			printf(", %3.3x",pal[i]);
		printf("\n");
		printf("Bitmap at: %8.8x\n",raw);
	}
*/
}

void ms_fatal(type8s *txt)
{
	fputs("\nFatal error: ",stderr);
	fputs(txt,stderr);
	fputs("\n",stderr);
	ms_status();
	exit(1);
}

type8 ms_showhints(struct ms_hint * hints)
{
	return 0;
}

void ms_playmusic(type8 * midi_data, type32 length, type16 tempo)
{
}

main(int argc, char **argv)
{
	type8 running, i, *gamename = 0, *gfxname = 0, *hintname = 0;
	type32 dlimit, slimit;

	if (sizeof(type8) != 1 || sizeof(type16) != 2 || sizeof(type32) != 4)
	{
		fprintf(stderr,
			"You have incorrect typesizes, please edit the typedefs and recompile\n"
			"or proceed on your own risk...\n");
		exit(1);
	}
	dlimit = slimit = 0xffffffff;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			switch (tolower(argv[i][1]))
			{
			case 'd':
				if (strlen(argv[i]) > 2)
					dlimit = atoi(&argv[i][2]);
				else
					dlimit = 0;
				break;
			case 's':
				if (strlen(argv[i])>2)
					slimit = atoi(&argv[i][2]);
				else
					slimit = 655360;
				break;
			case 't':
				if (!(logfile2 = fopen(&argv[i][2],"w")))
					printf("Failed to open \"%s\" for writing.\n",&argv[i][2]);
				break; 
			case 'r':
				if (logfile1 = fopen(&argv[i][2],"r"))
					log_on = 1;
				else
					printf("Failed to open \"%s\" for reading.\n",&argv[i][2]);
				break;
			case 'w':
				if (logfile1 = fopen(&argv[i][2],"w"))
					log_on = 2;
				else
					printf("Failed to open \"%s\" for writing.\n",&argv[i][2]);
				break;
			default:
				printf("Unknown option -%c, ignoring.\n",argv[i][1]);
				break;
			}
		}
		else if (!gamename)
			gamename = argv[i];
		else if (!gfxname)
			gfxname = argv[i];
		else if (!hintname)
			hintname = argv[i];
	}
	if (!gamename)
	{
		printf("Magnetic 2.3 - a Magnetic Scrolls interpreter\n\n");
		printf("Usage: %s [options] game [gfxfile] [hintfile]\n\n"
			"Where the options are:\n"
			" -dn    activate register dump (after n instructions)\n"
			" -rname read script file\n"
			" -sn    safety mode, exits automatically (after n instructions)\n"
			" -tname write transcript file\n"
			" -wname write script file\n\n"
			"The interpreter commands are:\n"
			" #undo   undo - don't use it near are_you_sure prompts\n"
			" #logoff turn off script writing\n\n",argv[0]);
		exit(1);
	}

	if (!(ms_gfx_enabled = ms_init(gamename,gfxname,hintname,0)))
	{
		printf("Couldn't start up game \"%s\".\n",gamename);
		exit(1);
	}
	ms_gfx_enabled--;
	running = 1;
	while ((ms_count() < slimit) && running)
	{
		if (ms_count() >= dlimit)
			ms_status();
		running = ms_rungame();
	}
	if (ms_count() == slimit)
	{
		printf("\n\nSafety limit (%d) reached.\n",slimit);
		ms_status();
	}
	ms_freemem();
	if (log_on)
		fclose(logfile1);
	if (logfile2)
		fclose(logfile2);
	printf("\nExiting.\n");
	return 0;
}
