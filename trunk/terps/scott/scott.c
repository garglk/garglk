/*
 *	ScottFree Revision 1.14
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *
 *	You must have an ANSI C compiler to build this program.
 */

/*
 * Part of this source file (mainly the GLK parts) are copyright 2011
 * Chris Spiegel.
 *
 * Some notes about the GLK version:
 *
 * o Room descriptions, exits, and visible items can, as in the
 *   original, be placed in a window at the top of the screen, or can be
 *   inline with user input in the main window.  The former is default,
 *   and the latter can be selected with the -w flag.
 *
 * o Game saving and loading uses GLK, which means that providing a
 *   save game file on the command-line will no longer work.  Instead,
 *   the verb "restore" has been special-cased to call GameLoad(), which
 *   now prompts for a filename via GLK.
 */
static int split_screen = 1;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#include "glk.h"
#include "glkstart.h"

#include "scott.h"

#ifdef __GNUC__
__attribute__((__format__(__printf__, 2, 3)))
#endif
static void wprintw(winid_t w, const char *fmt, ...)
{
	va_list ap;
	char msg[2048];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);

	glk_put_string_stream(glk_window_get_stream(w), msg);
}

/*
 *	Configuration Twiddles
 */

/*
 * The following applies to xstrcasecmp() and xstrncasecmp(), both taken
 * from FreeBSD.
 *
 * Copyright (c) 1987, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
static int xstrcasecmp(const char *s1, const char *s2)
{
	const unsigned char
		*us1 = (const unsigned char *)s1,
		*us2 = (const unsigned char *)s2;

	while (tolower(*us1) == tolower(*us2++))
		if (*us1++ == '\0')
			return (0);
	return (tolower(*us1) - tolower(*--us2));
}

static int xstrncasecmp(const char *s1, const char *s2, size_t n)
{
	if (n != 0) {
		const unsigned char
			*us1 = (const unsigned char *)s1,
			*us2 = (const unsigned char *)s2;

		do {
			if (tolower(*us1) != tolower(*us2++))
				return (tolower(*us1) - tolower(*--us2));
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}

static void delay(int seconds)
{
	int done = 0;
	event_t ev;

	if(!glk_gestalt(gestalt_Timer, 0))
		return;

	glk_request_timer_events(1000 * seconds);

	while(!done)
	{
		glk_select(&ev);

		if(ev.type == evtype_Timer)
			done = 1;
	}

	glk_request_timer_events(0);
}

static Header GameHeader;
static Item *Items;
static Room *Rooms;
static char **Verbs;
static char **Nouns;
static char **Messages;
static Action *Actions;
static int LightRefill;
static char NounText[16];
static int Counters[16];	/* Range unknown */
static int CurrentCounter;
static int SavedRoom;
static int RoomSaved[16];	/* Range unknown */
static int Options;		/* Option flags set */
static int Width;		/* Terminal width */
static int TopHeight;		/* Height of top window */

static winid_t Bottom, Top;

#define TRS80_LINE	"\n<------------------------------------------------------------>\n"

#define MyLoc	(GameHeader.PlayerRoom)

static long BitFlags=0;	/* Might be >32 flags - I haven't seen >32 yet */

static void Fatal(char *x)
{
	wprintw(Bottom, "%s", x);
	glk_exit();
}

static void ClearScreen(void)
{
	glk_window_clear(Bottom);
}

static void *MemAlloc(int size)
{
	void *t=(void *)malloc(size);
	if(t==NULL)
		Fatal("Out of memory");
	return(t);
}

static int RandomPercent(int n)
{
	unsigned int rv=rand()<<6;
	rv%=100;
	if(rv<n)
		return(1);
	return(0);
}

static int CountCarried(void)
{
	int ct=0;
	int n=0;
	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].Location==CARRIED)
			n++;
		ct++;
	}
	return(n);
}

static char *MapSynonym(char *word)
{
	int n=1;
	char *tp;
	static char lastword[16];	/* Last non synonym */
	while(n<=GameHeader.NumWords)
	{
		tp=Nouns[n];
		if(*tp=='*')
			tp++;
		else
			strcpy(lastword,tp);
		if(xstrncasecmp(word,tp,GameHeader.WordLength)==0)
			return(lastword);
		n++;
	}
	return(NULL);
}

static int MatchUpItem(char *text, int loc)
{
	char *word=MapSynonym(text);
	int ct=0;

	if(word==NULL)
		word=text;

	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].AutoGet && Items[ct].Location==loc &&
			xstrncasecmp(Items[ct].AutoGet,word,GameHeader.WordLength)==0)
			return(ct);
		ct++;
	}
	return(-1);
}

static char *ReadString(FILE *f)
{
	char tmp[1024];
	char *t;
	int c,nc;
	int ct=0;
	do
	{
		c=fgetc(f);
	}
	while(c!=EOF && isspace(c));
	if(c!='"')
	{
		Fatal("Initial quote expected");
	}
	do
	{
		c=fgetc(f);
		if(c==EOF)
			Fatal("EOF in string");
		if(c=='"')
		{
			nc=fgetc(f);
			if(nc!='"')
			{
				ungetc(nc,f);
				break;
			}
		}
		if(c==0x60)
			c='"'; /* pdd */
		tmp[ct++]=c;
	}
	while(1);
	tmp[ct]=0;
	t=MemAlloc(ct+1);
	memcpy(t,tmp,ct+1);
	return(t);
}

static void LoadDatabase(FILE *f, int loud)
{
	int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
	int ct;
	short lo;
	Action *ap;
	Room *rp;
	Item *ip;
/* Load the header */

	/* As this was written, there were too many arguments for the
	 * format string.  The ct that would be read from here, if there
	 * were a conversion specifier for it, is never used; so
	 * presumably just removing &ct is fine.
	 * --Chris
	 */
#if 0
	if(fscanf(f,"%*d %d %d %d %d %d %d %d %d %d %d %d",
		&ni,&na,&nw,&nr,&mc,&pr,&tr,&wl,&lt,&mn,&trm,&ct)<10)
#endif
	if(fscanf(f,"%*d %d %d %d %d %d %d %d %d %d %d %d",
		&ni,&na,&nw,&nr,&mc,&pr,&tr,&wl,&lt,&mn,&trm)<10)
		Fatal("Invalid database(bad header)");
	GameHeader.NumItems=ni;
	Items=(Item *)MemAlloc(sizeof(Item)*(ni+1));
	GameHeader.NumActions=na;
	Actions=(Action *)MemAlloc(sizeof(Action)*(na+1));
	GameHeader.NumWords=nw;
	GameHeader.WordLength=wl;
	Verbs=(char **)MemAlloc(sizeof(char *)*(nw+1));
	Nouns=(char **)MemAlloc(sizeof(char *)*(nw+1));
	GameHeader.NumRooms=nr;
	Rooms=(Room *)MemAlloc(sizeof(Room)*(nr+1));
	GameHeader.MaxCarry=mc;
	GameHeader.PlayerRoom=pr;
	GameHeader.Treasures=tr;
	GameHeader.LightTime=lt;
	LightRefill=lt;
	GameHeader.NumMessages=mn;
	Messages=(char **)MemAlloc(sizeof(char *)*(mn+1));
	GameHeader.TreasureRoom=trm;

/* Load the actions */

	ct=0;
	ap=Actions;
	if(loud)
		printf("Reading %d actions.\n",na);
	while(ct<na+1)
	{
		if(fscanf(f,"%hu %hu %hu %hu %hu %hu %hu %hu",
			&ap->Vocab,
			&ap->Condition[0],
			&ap->Condition[1],
			&ap->Condition[2],
			&ap->Condition[3],
			&ap->Condition[4],
			&ap->Action[0],
			&ap->Action[1])!=8)
		{
			printf("Bad action line (%d)\n",ct);
			exit(1);
		}
		ap++;
		ct++;
	}
	ct=0;
	if(loud)
		printf("Reading %d word pairs.\n",nw);
	while(ct<nw+1)
	{
		Verbs[ct]=ReadString(f);
		Nouns[ct]=ReadString(f);
		ct++;
	}
	ct=0;
	rp=Rooms;
	if(loud)
		printf("Reading %d rooms.\n",nr);
	while(ct<nr+1)
	{
		fscanf(f,"%hd %hd %hd %hd %hd %hd",
			&rp->Exits[0],&rp->Exits[1],&rp->Exits[2],
			&rp->Exits[3],&rp->Exits[4],&rp->Exits[5]);
		rp->Text=ReadString(f);
		ct++;
		rp++;
	}
	ct=0;
	if(loud)
		printf("Reading %d messages.\n",mn);
	while(ct<mn+1)
	{
		Messages[ct]=ReadString(f);
		ct++;
	}
	ct=0;
	if(loud)
		printf("Reading %d items.\n",ni);
	ip=Items;
	while(ct<ni+1)
	{
		ip->Text=ReadString(f);
		ip->AutoGet=strchr(ip->Text,'/');
		/* Some games use // to mean no auto get/drop word! */
		if(ip->AutoGet && strcmp(ip->AutoGet,"//") && strcmp(ip->AutoGet,"/*"))
		{
			char *t;
			*ip->AutoGet++=0;
			t=strchr(ip->AutoGet,'/');
			if(t!=NULL)
				*t=0;
		}
		fscanf(f,"%hd",&lo);
		ip->Location=(unsigned char)lo;
		ip->InitialLoc=ip->Location;
		ip++;
		ct++;
	}
	ct=0;
	/* Discard Comment Strings */
	while(ct<na+1)
	{
		free(ReadString(f));
		ct++;
	}
	fscanf(f,"%d",&ct);
	if(loud)
		printf("Version %d.%02d of Adventure ",
		ct/100,ct%100);
	fscanf(f,"%d",&ct);
	if(loud)
		printf("%d.\nLoad Complete.\n\n",ct);
}

static void OutBuf(char *buffer)
{
	wprintw(Bottom, "%s", buffer);
}

static void Output(char *a)
{
	char block[512];
	strcpy(block,a);
	OutBuf(block);
}

static void OutputNumber(int a)
{
	char buf[16];
	sprintf(buf,"%d ",a);
	OutBuf(buf);
}

static void Look(void)
{
	static char *ExitNames[6]=
	{
		"North","South","East","West","Up","Down"
	};
	Room *r;
	int ct,f;
	int pos;

	if(split_screen)
		glk_window_clear(Top);

	if((BitFlags&(1<<DARKBIT)) && Items[LIGHT_SOURCE].Location!= CARRIED
	            && Items[LIGHT_SOURCE].Location!= MyLoc)
	{
		if(Options&YOUARE)
			wprintw(Top,"You can't see. It is too dark!\n");
		else
			wprintw(Top,"I can't see. It is too dark!\n");
		if (Options & TRS80_STYLE)
			wprintw(Top,TRS80_LINE);
		return;
	}
	r=&Rooms[MyLoc];
	if(*r->Text=='*')
		wprintw(Top,"%s\n",r->Text+1);
	else
	{
		if(Options&YOUARE)
			wprintw(Top,"You are in a %s\n",r->Text);
		else
			wprintw(Top,"I'm in a %s\n",r->Text);
	}
	ct=0;
	f=0;
	wprintw(Top,"\nObvious exits: ");
	while(ct<6)
	{
		if(r->Exits[ct]!=0)
		{
			if(f==0)
				f=1;
			else
				wprintw(Top,", ");
			wprintw(Top,"%s",ExitNames[ct]);
		}
		ct++;
	}
	if(f==0)
		wprintw(Top,"none");
	wprintw(Top,".\n");
	ct=0;
	f=0;
	pos=0;
	while(ct<=GameHeader.NumItems)
	{
		if(Items[ct].Location==MyLoc)
		{
			if(f==0)
			{
				if(Options&YOUARE)
					wprintw(Top,"\nYou can also see: ");
				else
					wprintw(Top,"\nI can also see: ");
				pos=16;
				f++;
			}
			else if (!(Options & TRS80_STYLE))
			{
				wprintw(Top," - ");
				pos+=3;
			}
			if(pos+strlen(Items[ct].Text)>(Width-10))
			{
				pos=0;
				wprintw(Top,"\n");
			}
			wprintw(Top,"%s",Items[ct].Text);
			pos += strlen(Items[ct].Text);
			if (Options & TRS80_STYLE)
			{
				wprintw(Top,". ");
				pos+=2;
			}
		}
		ct++;
	}
	wprintw(Top,"\n");
	if (Options & TRS80_STYLE)
		wprintw(Top,TRS80_LINE);
}

static int WhichWord(char *word, char **list)
{
	int n=1;
	int ne=1;
	char *tp;
	while(ne<=GameHeader.NumWords)
	{
		tp=list[ne];
		if(*tp=='*')
			tp++;
		else
			n=ne;
		if(xstrncasecmp(word,tp,GameHeader.WordLength)==0)
			return(n);
		ne++;
	}
	return(-1);
}

static void LineInput(char *buf, size_t n)
{
	event_t ev;
	int done = 0;

	glk_request_line_event(Bottom, buf, n - 1, 0);

	while(!done)
	{
		glk_select(&ev);

		if(ev.type == evtype_LineInput)
			done = 1;
		else if(ev.type == evtype_Arrange && split_screen)
			Look();
	}

	buf[ev.val1] = 0;
}

static void SaveGame(void)
{
	strid_t file;
	frefid_t ref;
	int ct;
	char buf[128];

	ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame, filemode_Write, 0);
	if(ref == NULL) return;

	file = glk_stream_open_file(ref, filemode_Write, 0);
	glk_fileref_destroy(ref);
	if(file == NULL) return;

	for(ct=0;ct<16;ct++)
	{
		snprintf(buf, sizeof buf, "%d %d\n",Counters[ct],RoomSaved[ct]);
		glk_put_string_stream(file, buf);
	}
	snprintf(buf, sizeof buf, "%ld %d %hd %d %d %hd\n",BitFlags, (BitFlags&(1<<DARKBIT))?1:0,
		MyLoc,CurrentCounter,SavedRoom,GameHeader.LightTime);
	glk_put_string_stream(file, buf);
	for(ct=0;ct<=GameHeader.NumItems;ct++)
	{
		snprintf(buf, sizeof buf, "%hd\n",(short)Items[ct].Location);
		glk_put_string_stream(file, buf);
	}

	glk_stream_close(file, NULL);
	Output("Saved.\n");
}

static void LoadGame(void)
{
	strid_t file;
	frefid_t ref;
	char buf[128];
	int ct=0;
	short lo;
	short DarkFlag;

	ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame, filemode_Read, 0);
	if(ref == NULL) return;

	file = glk_stream_open_file(ref, filemode_Read, 0);
	glk_fileref_destroy(ref);
	if(file == NULL) return;

	for(ct=0;ct<16;ct++)
	{
		glk_get_line_stream(file, buf, sizeof buf);
		sscanf(buf, "%d %d",&Counters[ct],&RoomSaved[ct]);
	}
	glk_get_line_stream(file, buf, sizeof buf);
	sscanf(buf, "%ld %hd %hd %d %d %hd\n",
		&BitFlags,&DarkFlag,&MyLoc,&CurrentCounter,&SavedRoom,
		&GameHeader.LightTime);
	/* Backward compatibility */
	if(DarkFlag)
		BitFlags|=(1<<15);
	for(ct=0;ct<=GameHeader.NumItems;ct++)
	{
		glk_get_line_stream(file, buf, sizeof buf);
		sscanf(buf, "%hd\n",&lo);
		Items[ct].Location=(unsigned char)lo;
	}
}

static int GetInput(int *vb, int *no)
{
	char buf[256];
	char verb[10],noun[10];
	int vc,nc;
	int num;

	do
	{
		do
		{
			Output("\nTell me what to do ? ");
			LineInput(buf, sizeof buf);
			num=sscanf(buf,"%9s %9s",verb,noun);
		}
		while(num==0||*buf=='\n');
		if(xstrcasecmp(verb, "restore") == 0)
		{
			LoadGame();
			return -1;
		}
		if(num==1)
			*noun=0;
		if(*noun==0 && strlen(verb)==1)
		{
			switch(isupper(*verb)?tolower(*verb):*verb)
			{
				case 'n':strcpy(verb,"NORTH");break;
				case 'e':strcpy(verb,"EAST");break;
				case 's':strcpy(verb,"SOUTH");break;
				case 'w':strcpy(verb,"WEST");break;
				case 'u':strcpy(verb,"UP");break;
				case 'd':strcpy(verb,"DOWN");break;
				/* Brian Howarth interpreter also supports this */
				case 'i':strcpy(verb,"INVENTORY");break;
			}
		}
		nc=WhichWord(verb,Nouns);
		/* The Scott Adams system has a hack to avoid typing 'go' */
		if(nc>=1 && nc <=6)
		{
			vc=1;
		}
		else
		{
			vc=WhichWord(verb,Verbs);
			nc=WhichWord(noun,Nouns);
		}
		*vb=vc;
		*no=nc;
		if(vc==-1)
		{
			Output("You use word(s) I don't know! ");
		}
	}
	while(vc==-1);
	strcpy(NounText,noun);	/* Needed by GET/DROP hack */

	return 0;
}

static int PerformLine(int ct)
{
	int continuation=0;
	int param[5],pptr=0;
	int act[4];
	int cc=0;
	while(cc<5)
	{
		int cv,dv;
		cv=Actions[ct].Condition[cc];
		dv=cv/20;
		cv%=20;
		switch(cv)
		{
			case 0:
				param[pptr++]=dv;
				break;
			case 1:
				if(Items[dv].Location!=CARRIED)
					return(0);
				break;
			case 2:
				if(Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 3:
				if(Items[dv].Location!=CARRIED&&
					Items[dv].Location!=MyLoc)
					return(0);
				break;
			case 4:
				if(MyLoc!=dv)
					return(0);
				break;
			case 5:
				if(Items[dv].Location==MyLoc)
					return(0);
				break;
			case 6:
				if(Items[dv].Location==CARRIED)
					return(0);
				break;
			case 7:
				if(MyLoc==dv)
					return(0);
				break;
			case 8:
				if((BitFlags&(1<<dv))==0)
					return(0);
				break;
			case 9:
				if(BitFlags&(1<<dv))
					return(0);
				break;
			case 10:
				if(CountCarried()==0)
					return(0);
				break;
			case 11:
				if(CountCarried())
					return(0);
				break;
			case 12:
				if(Items[dv].Location==CARRIED||Items[dv].Location==MyLoc)
					return(0);
				break;
			case 13:
				if(Items[dv].Location==0)
					return(0);
				break;
			case 14:
				if(Items[dv].Location)
					return(0);
				break;
			case 15:
				if(CurrentCounter>dv)
					return(0);
				break;
			case 16:
				if(CurrentCounter<=dv)
					return(0);
				break;
			case 17:
				if(Items[dv].Location!=Items[dv].InitialLoc)
					return(0);
				break;
			case 18:
				if(Items[dv].Location==Items[dv].InitialLoc)
					return(0);
				break;
			case 19:/* Only seen in Brian Howarth games so far */
				if(CurrentCounter!=dv)
					return(0);
				break;
		}
		cc++;
	}
	/* Actions */
	act[0]=Actions[ct].Action[0];
	act[2]=Actions[ct].Action[1];
	act[1]=act[0]%150;
	act[3]=act[2]%150;
	act[0]/=150;
	act[2]/=150;
	cc=0;
	pptr=0;
	while(cc<4)
	{
		if(act[cc]>=1 && act[cc]<52)
		{
			Output(Messages[act[cc]]);
			Output("\n");
		}
		else if(act[cc]>101)
		{
			Output(Messages[act[cc]-50]);
			Output("\n");
		}
		else switch(act[cc])
		{
			case 0:/* NOP */
				break;
			case 52:
				if(CountCarried()==GameHeader.MaxCarry)
				{
					if(Options&YOUARE)
						Output("You are carrying too much. ");
					else
						Output("I've too much to carry! ");
					break;
				}
				Items[param[pptr++]].Location= CARRIED;
				break;
			case 53:
				Items[param[pptr++]].Location=MyLoc;
				break;
			case 54:
				MyLoc=param[pptr++];
				break;
			case 55:
				Items[param[pptr++]].Location=0;
				break;
			case 56:
				BitFlags|=1<<DARKBIT;
				break;
			case 57:
				BitFlags&=~(1<<DARKBIT);
				break;
			case 58:
				BitFlags|=(1<<param[pptr++]);
				break;
			case 59:
				Items[param[pptr++]].Location=0;
				break;
			case 60:
				BitFlags&=~(1<<param[pptr++]);
				break;
			case 61:
				if(Options&YOUARE)
					Output("You are dead.\n");
				else
					Output("I am dead.\n");
				BitFlags&=~(1<<DARKBIT);
				MyLoc=GameHeader.NumRooms;/* It seems to be what the code says! */
				break;
			case 62:
			{
				/* Bug fix for some systems - before it could get parameters wrong */
				int i=param[pptr++];
				Items[i].Location=param[pptr++];
				break;
			}
			case 63:
doneit:				Output("The game is now over.\n");
				glk_exit();
			case 64:
				break;
			case 65:
			{
				int ct=0;
				int n=0;
				while(ct<=GameHeader.NumItems)
				{
					if(Items[ct].Location==GameHeader.TreasureRoom &&
					  *Items[ct].Text=='*')
					  	n++;
					ct++;
				}
				if(Options&YOUARE)
					Output("You have stored ");
				else
					Output("I've stored ");
				OutputNumber(n);
				Output(" treasures.  On a scale of 0 to 100, that rates ");
				OutputNumber((n*100)/GameHeader.Treasures);
				Output(".\n");
				if(n==GameHeader.Treasures)
				{
					Output("Well done.\n");
					goto doneit;
				}
				break;
			}
			case 66:
			{
				int ct=0;
				int f=0;
				if(Options&YOUARE)
					Output("You are carrying:\n");
				else
					Output("I'm carrying:\n");
				while(ct<=GameHeader.NumItems)
				{
					if(Items[ct].Location==CARRIED)
					{
						if(f==1)
						{
							if (Options & TRS80_STYLE)
								Output(". ");
							else
								Output(" - ");
						}
						f=1;
						Output(Items[ct].Text);
					}
					ct++;
				}
				if(f==0)
					Output("Nothing");
				Output(".\n");
				break;
			}
			case 67:
				BitFlags|=(1<<0);
				break;
			case 68:
				BitFlags&=~(1<<0);
				break;
			case 69:
				GameHeader.LightTime=LightRefill;
				Items[LIGHT_SOURCE].Location=CARRIED;
				BitFlags&=~(1<<LIGHTOUTBIT);
				break;
			case 70:
				ClearScreen(); /* pdd. */
				break;
			case 71:
				SaveGame();
				break;
			case 72:
			{
				int i1=param[pptr++];
				int i2=param[pptr++];
				int t=Items[i1].Location;
				Items[i1].Location=Items[i2].Location;
				Items[i2].Location=t;
				break;
			}
			case 73:
				continuation=1;
				break;
			case 74:
				Items[param[pptr++]].Location= CARRIED;
				break;
			case 75:
			{
				int i1,i2;
				i1=param[pptr++];
				i2=param[pptr++];
				Items[i1].Location=Items[i2].Location;
				break;
			}
			case 76:	/* Looking at adventure .. */
				break;
			case 77:
				if(CurrentCounter>=0)
					CurrentCounter--;
				break;
			case 78:
				OutputNumber(CurrentCounter);
				break;
			case 79:
				CurrentCounter=param[pptr++];
				break;
			case 80:
			{
				int t=MyLoc;
				MyLoc=SavedRoom;
				SavedRoom=t;
				break;
			}
			case 81:
			{
				/* This is somewhat guessed. Claymorgue always
				   seems to do select counter n, thing, select counter n,
				   but uses one value that always seems to exist. Trying
				   a few options I found this gave sane results on ageing */
				int t=param[pptr++];
				int c1=CurrentCounter;
				CurrentCounter=Counters[t];
				Counters[t]=c1;
				break;
			}
			case 82:
				CurrentCounter+=param[pptr++];
				break;
			case 83:
				CurrentCounter-=param[pptr++];
				if(CurrentCounter< -1)
					CurrentCounter= -1;
				/* Note: This seems to be needed. I don't yet
				   know if there is a maximum value to limit too */
				break;
			case 84:
				Output(NounText);
				break;
			case 85:
				Output(NounText);
				Output("\n");
				break;
			case 86:
				Output("\n");
				break;
			case 87:
			{
				/* Changed this to swap location<->roomflag[x]
				   not roomflag 0 and x */
				int p=param[pptr++];
				int sr=MyLoc;
				MyLoc=RoomSaved[p];
				RoomSaved[p]=sr;
				break;
			}
			case 88:
				delay(2);
				break;
			case 89:
				pptr++;
				/* SAGA draw picture n */
				/* Spectrum Seas of Blood - start combat ? */
				/* Poking this into older spectrum games causes a crash */
				break;
			default:
				fprintf(stderr,"Unknown action %d [Param begins %d %d]\n",
					act[cc],param[pptr],param[pptr+1]);
				break;
		}
		cc++;
	}
	return(1+continuation);
}


static int PerformActions(int vb,int no)
{
	static int disable_sysfunc=0;	/* Recursion lock */
	int d=BitFlags&(1<<DARKBIT);

	int ct=0;
	int fl;
	int doagain=0;
	if(vb==1 && no == -1 )
	{
		Output("Give me a direction too.");
		return(0);
	}
	if(vb==1 && no>=1 && no<=6)
	{
		int nl;
		if(Items[LIGHT_SOURCE].Location==MyLoc ||
		   Items[LIGHT_SOURCE].Location==CARRIED)
		   	d=0;
		if(d)
			Output("Dangerous to move in the dark! ");
		nl=Rooms[MyLoc].Exits[no-1];
		if(nl!=0)
		{
			MyLoc=nl;
			return(0);
		}
		if(d)
		{
			if(Options&YOUARE)
				Output("You fell down and broke your neck. ");
			else
				Output("I fell down and broke my neck. ");
			glk_exit();
		}
		if(Options&YOUARE)
			Output("You can't go in that direction. ");
		else
			Output("I can't go in that direction. ");
		return(0);
	}
	fl= -1;
	while(ct<=GameHeader.NumActions)
	{
		int vv,nv;
		vv=Actions[ct].Vocab;
		/* Think this is now right. If a line we run has an action73
		   run all following lines with vocab of 0,0 */
		if(vb!=0 && (doagain&&vv!=0))
			break;
		/* Oops.. added this minor cockup fix 1.11 */
		if(vb!=0 && !doagain && fl== 0)
			break;
		nv=vv%150;
		vv/=150;
		if((vv==vb)||(doagain&&Actions[ct].Vocab==0))
		{
			if((vv==0 && RandomPercent(nv))||doagain||
				(vv!=0 && (nv==no||nv==0)))
			{
				int f2;
				if(fl== -1)
					fl= -2;
				if((f2=PerformLine(ct))>0)
				{
					/* ahah finally figured it out ! */
					fl=0;
					if(f2==2)
						doagain=1;
					if(vb!=0 && doagain==0)
						return(0);
				}
			}
		}
		ct++;

		/* Previously this did not check ct against
		 * GameHeader.NumActions and would read past the end of
		 * Actions.  I don't know what should happen on the last
		 * action, but doing nothing is better than reading one
		 * past the end.
		 * --Chris
		 */
		if(ct <= GameHeader.NumActions && Actions[ct].Vocab!=0)
			doagain=0;
	}
	if(fl!=0 && disable_sysfunc==0)
	{
		int i;
		if(Items[LIGHT_SOURCE].Location==MyLoc ||
		   Items[LIGHT_SOURCE].Location==CARRIED)
		   	d=0;
		if(vb==10 || vb==18)
		{
			/* Yes they really _are_ hardcoded values */
			if(vb==10)
			{
				if(xstrcasecmp(NounText,"ALL")==0)
				{
					int ct=0;
					int f=0;

					if(d)
					{
						Output("It is dark.\n");
						return 0;
					}
					while(ct<=GameHeader.NumItems)
					{
						if(Items[ct].Location==MyLoc && Items[ct].AutoGet!=NULL && Items[ct].AutoGet[0]!='*')
						{
							no=WhichWord(Items[ct].AutoGet,Nouns);
							disable_sysfunc=1;	/* Don't recurse into auto get ! */
							PerformActions(vb,no);	/* Recursively check each items table code */
							disable_sysfunc=0;
							if(CountCarried()==GameHeader.MaxCarry)
							{
								if(Options&YOUARE)
									Output("You are carrying too much. ");
								else
									Output("I've too much to carry. ");
								return(0);
							}
						 	Items[ct].Location= CARRIED;
						 	OutBuf(Items[ct].Text);
						 	Output(": O.K.\n");
						 	f=1;
						 }
						 ct++;
					}
					if(f==0)
						Output("Nothing taken.");
					return(0);
				}
				if(no==-1)
				{
					Output("What ? ");
					return(0);
				}
				if(CountCarried()==GameHeader.MaxCarry)
				{
					if(Options&YOUARE)
						Output("You are carrying too much. ");
					else
						Output("I've too much to carry. ");
					return(0);
				}
				i=MatchUpItem(NounText,MyLoc);
				if(i==-1)
				{
					if(Options&YOUARE)
						Output("It is beyond your power to do that. ");
					else
						Output("It's beyond my power to do that. ");
					return(0);
				}
				Items[i].Location= CARRIED;
				Output("O.K. ");
				return(0);
			}
			if(vb==18)
			{
				if(xstrcasecmp(NounText,"ALL")==0)
				{
					int ct=0;
					int f=0;
					while(ct<=GameHeader.NumItems)
					{
						if(Items[ct].Location==CARRIED && Items[ct].AutoGet && Items[ct].AutoGet[0]!='*')
						{
							no=WhichWord(Items[ct].AutoGet,Nouns);
							disable_sysfunc=1;
							PerformActions(vb,no);
							disable_sysfunc=0;
							Items[ct].Location=MyLoc;
							OutBuf(Items[ct].Text);
							Output(": O.K.\n");
							f=1;
						}
						ct++;
					}
					if(f==0)
						Output("Nothing dropped.\n");
					return(0);
				}
				if(no==-1)
				{
					Output("What ? ");
					return(0);
				}
				i=MatchUpItem(NounText,CARRIED);
				if(i==-1)
				{
					if(Options&YOUARE)
						Output("It's beyond your power to do that.\n");
					else
						Output("It's beyond my power to do that.\n");
					return(0);
				}
				Items[i].Location=MyLoc;
				Output("O.K. ");
				return(0);
			}
		}
	}
	return(fl);
}

glkunix_argumentlist_t glkunix_arguments[] =
{
	{ "-y",		glkunix_arg_NoValue,		"-y		Generate 'You are', 'You are carrying' type messages for games that use these instead (eg Robin Of Sherwood)" },
	{ "-i",		glkunix_arg_NoValue,		"-i		Generate 'I am' type messages (default)" },
	{ "-d",		glkunix_arg_NoValue,		"-d		Debugging info on load " },
	{ "-s",		glkunix_arg_NoValue,		"-s		Generate authentic Scott Adams driver light messages rather than other driver style ones (Light goes out in n turns..)" },
	{ "-t",		glkunix_arg_NoValue,		"-t		Generate TRS80 style display (terminal width is 64 characters; a line <-----------------> is displayed after the top stuff; objects have periods after them instead of hyphens" },
	{ "-p",		glkunix_arg_NoValue,		"-p		Use for prehistoric databases which don't use bit 16" },
	{ "-w",		glkunix_arg_NoValue,		"-w		Disable upper window" },
	{ "",		glkunix_arg_ValueFollows,	"filename	file to load" },

	{ NULL, glkunix_arg_End, NULL }
};

static char *game_file;

int glkunix_startup_code(glkunix_startup_t *data)
{
	int argc = data->argc;
	char **argv = data->argv;

	if(argc < 1)
		return 0;

	while(argv[1])
	{
		if(*argv[1]!='-')
			break;
		switch(argv[1][1])
		{
			case 'y':
				Options|=YOUARE;
				break;
			case 'i':
				Options&=~YOUARE;
				break;
			case 'd':
				Options|=DEBUGGING;
				break;
			case 's':
				Options|=SCOTTLIGHT;
				break;
			case 't':
				Options|=TRS80_STYLE;
				break;
			case 'p':
				Options|=PREHISTORIC_LAMP;
				break;
			case 'w':
				split_screen = 0;
				break;
		}
		argv++;
		argc--;
	}

#ifdef GARGLK
	garglk_set_program_name("ScottFree 1.14");
	garglk_set_program_info(
		"ScottFree 1.14 by Alan Cox\n"
		"Glk port by Chris Spiegel\n");
#endif

	if(argc==2)
	{
		game_file = argv[1];
#ifdef GARGLK
		char *s;
		s = strrchr(game_file, '\\');
		if (s) garglk_set_story_name(s+1);
		s = strrchr(game_file, '/');
		if (s) garglk_set_story_name(s+1);
#endif
	}

	return 1;
}

void glk_main(void)
{
	FILE *f;
	int vb,no;

	Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
	if(Bottom == NULL)
		glk_exit();
	glk_set_window(Bottom);

	if(game_file == NULL)
		Fatal("No game provided");

	f = fopen(game_file, "r");
	if(f==NULL)
		Fatal("Cannot open game");

	if (Options & TRS80_STYLE)
	{
		Width = 64;
		TopHeight = 11;
	}
	else
	{
		Width = 80;
		TopHeight = 10;
	}

	if(split_screen)
	{
		Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight, wintype_TextGrid, 0);
		if(Top == NULL)
		{
			split_screen = 0;
			Top = Bottom;
		}
	}
	else
	{
		Top = Bottom;
	}

	OutBuf("\
Scott Free, A Scott Adams game driver in C.\n\
Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
Distributed under the GNU software license\n\n");
	LoadDatabase(f,(Options&DEBUGGING)?1:0);
	fclose(f);
	srand(time(NULL));
	while(1)
	{
		glk_tick();

		PerformActions(0,0);

		Look();

		if(GetInput(&vb,&no) == -1)
			continue;
		switch(PerformActions(vb,no))
		{
			case -1:Output("I don't understand your command. ");
				break;
			case -2:Output("I can't do that yet. ");
				break;
		}
		/* Brian Howarth games seem to use -1 for forever */
		if(Items[LIGHT_SOURCE].Location/*==-1*/!=DESTROYED && GameHeader.LightTime!= -1)
		{
			GameHeader.LightTime--;
			if(GameHeader.LightTime<1)
			{
				BitFlags|=(1<<LIGHTOUTBIT);
				if(Items[LIGHT_SOURCE].Location==CARRIED ||
					Items[LIGHT_SOURCE].Location==MyLoc)
				{
					if(Options&SCOTTLIGHT)
						Output("Light has run out! ");
					else
						Output("Your light has run out. ");
				}
				if(Options&PREHISTORIC_LAMP)
					Items[LIGHT_SOURCE].Location=DESTROYED;
			}
			else if(GameHeader.LightTime<25)
			{
				if(Items[LIGHT_SOURCE].Location==CARRIED ||
					Items[LIGHT_SOURCE].Location==MyLoc)
				{

					if(Options&SCOTTLIGHT)
					{
						Output("Light runs out in ");
						OutputNumber(GameHeader.LightTime);
						Output(" turns. ");
					}
					else
					{
						if(GameHeader.LightTime%5==0)
							Output("Your light is growing dim. ");
					}
				}
			}
		}
	}
}
