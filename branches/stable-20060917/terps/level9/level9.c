/***********************************************************************\
*
* Level 9 interpreter
* Version 4.0
* Copyright (c) 1996 Glen Summers
* Copyright (c) 2002,2003 Glen Summers and David Kinder
* Copyright (c) 2005 Glen Summers, David Kinder, Alan Staniforth,
* Simon Baldwin and Dieter Baron
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
* The input routine will repond to the following 'hash' commands
*  #save         saves position file directly (bypasses any
*                disk change prompts)
*  #restore      restores position file directly (bypasses any
*                protection code)
*  #quit         terminates current game, RunGame() will return FALSE
*  #cheat        tries to bypass restore protection on v3,4 games
*                (can be slow)
*  #dictionary   lists game dictionary (press a key to interrupt)
*  #picture <n>  show picture <n>
*
\***********************************************************************/

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "level9.h"

/* #define L9DEBUG */
/* #define CODEFOLLOW */
/* #define FULLSCAN */

/* "L901" */
#define L9_ID 0x4c393031

#define IBUFFSIZE 500
#define RAMSAVESLOTS 10
#define GFXSTACKSIZE 100


/* Typedefs */
typedef struct
{
	L9UINT16 vartable[256];
	L9BYTE listarea[LISTAREASIZE];
} SaveStruct;

typedef struct
{
	L9BYTE *a5;
	int scale;
} GfxState;


/* Enumerations */
enum L9GameTypes {L9_V1,L9_V2,L9_V3,L9_V4};
enum V2MsgTypes {V2M_NORMAL,V2M_ERIK};


/* Global Variables */
L9BYTE* startfile=NULL,*pictureaddress=NULL,*picturedata=NULL;
L9BYTE* startdata;
L9UINT32 FileSize,picturesize;

L9BYTE *L9Pointers[12];
L9BYTE *absdatablock,*list2ptr,*list3ptr,*list9startptr,*acodeptr;
L9BYTE *startmd,*endmd,*endwdp5,*wordtable,*dictdata,*defdict;
L9UINT16 dictdatalen;
L9BYTE *startmdV2;

int wordcase;
int unpackcount;
char unpackbuf[8];
L9BYTE* dictptr;
char threechars[34];
int L9GameType;
int V2MsgType;
char LastGame[MAX_PATH];


#if defined(AMIGA) && defined(_DCC)
__far SaveStruct ramsavearea[RAMSAVESLOTS];
#else
SaveStruct ramsavearea[RAMSAVESLOTS];
#endif

GameState workspace;

L9UINT16 randomseed;
L9BOOL Running;

char ibuff[IBUFFSIZE];
L9BYTE* ibuffptr;
char obuff[34];

L9BOOL Cheating=FALSE;
int CheatWord;
GameState CheatWorkspace;

int reflectflag,scale,gintcolour,option;
int l9textmode=0,drawx=0,drawy=0,screencalled=0,showtitle=1;
L9BYTE *gfxa5=NULL;
L9BOOL scalegfx=TRUE;
Bitmap* bitmap=NULL;

GfxState GfxStack[GFXSTACKSIZE];
int GfxStackPos=0;

char lastchar='.';
char lastactualchar=0;
int d5;

L9BYTE* codeptr;	/* instruction codes */
L9BYTE code;		/* instruction codes */

L9BYTE* list9ptr;

int unpackd3;

L9BYTE exitreversaltable[16]= {0x00,0x04,0x06,0x07,0x01,0x08,0x02,0x03,0x05,0x0a,0x09,0x0c,0x0b,0xff,0xff,0x0f};

L9UINT16 gnostack[128];
L9BYTE gnoscratch[32];
int object,gnosp,numobjectfound,searchdepth,inithisearchpos;


/* Prototypes */
L9BOOL LoadGame2(char *filename,char *picname);
L9BOOL checksubs(void);
int getlongcode(void);
L9BOOL GetWordV2(char *buff,int Word);
L9BOOL GetWordV3(char *buff,int Word);
void show_picture(int pic);


#ifdef CODEFOLLOW
#define CODEFOLLOWFILE "c:\\temp\\output"
FILE *f;
L9UINT16 *cfvar,*cfvar2;
char *codes[]=
{
"Goto",
"intgosub",
"intreturn",
"printnumber",
"messagev",
"messagec",
"function",
"input",
"varcon",
"varvar",
"_add",
"_sub",
"ilins",
"ilins",
"jump",
"Exit",
"ifeqvt",
"ifnevt",
"ifltvt",
"ifgtvt",
"screen",
"cleartg",
"picture",
"getnextobject",
"ifeqct",
"ifnect",
"ifltct",
"ifgtct",
"printinput",
"ilins",
"ilins",
"ilins",
};
char *functions[]=
{
	"calldriver",
	"L9Random",
	"save",
	"restore",
	"clearworkspace",
	"clearstack"
};
char *drivercalls[]=
{
"init",
"drivercalcchecksum",
"driveroswrch",
"driverosrdch",
"driverinputline",
"driversavefile",
"driverloadfile",
"settext",
"resettask",
"returntogem",
"10 *",
"loadgamedatafile",
"randomnumber",
"13 *",
"driver14",
"15 *",
"driverclg",
"line",
"fill",
"driverchgcol",
"20 *",
"21 *",
"ramsave",
"ramload",
"24 *",
"lensdisplay",
"26 *",
"27 *",
"28 *",
"29 *",
"allocspace",
"31 *",
"showbitmap",
"33 *",
"checkfordisc"
};
#endif

void initdict(L9BYTE *ptr)
{
	dictptr=ptr;
	unpackcount=8;
}

char getdictionarycode(void)
{
	if (unpackcount!=8) return unpackbuf[unpackcount++];
	else
	{
		/* unpackbytes */
		L9BYTE d1=*dictptr++,d2;
		unpackbuf[0]=d1>>3;
		d2=*dictptr++;
		unpackbuf[1]=((d2>>6) + (d1<<2)) & 0x1f;
		d1=*dictptr++;
		unpackbuf[2]=(d2>>1) & 0x1f;
		unpackbuf[3]=((d1>>4) + (d2<<4)) & 0x1f;
		d2=*dictptr++;
		unpackbuf[4]=((d1<<1) + (d2>>7)) & 0x1f;
		d1=*dictptr++;
		unpackbuf[5]=(d2>>2) & 0x1f;
		unpackbuf[6]=((d2<<3) + (d1>>5)) & 0x1f;
		unpackbuf[7]=d1 & 0x1f;
		unpackcount=1;
		return unpackbuf[0];
	}
}

int getdictionary(int d0)
{
	if (d0>=0x1a) return getlongcode();
	else return d0+0x61;
}

int getlongcode(void)
{
	int d0,d1;
	d0=getdictionarycode();
	if (d0==0x10)
	{
		wordcase=1;
		d0=getdictionarycode();
		return getdictionary(d0); /* reentrant? */
	}
	d1=getdictionarycode();
	return 0x80 | ((d0<<5) & 0xe0) | (d1 & 0x1f);
}

void printchar(char c)
{
	if (Cheating) return;

	if (c&128) lastchar=(c&=0x7f);
	else if (c!=0x20 && c!=0x0d && (c<'\"' || c>='.'))
	{
		if (lastchar=='!' || lastchar=='?' || lastchar=='.') c=toupper(c);
		lastchar=c;
	}
	/* eat multiple CRs */
	if (c!=0x0d || lastactualchar!=0x0d) os_printchar(c);
	lastactualchar=c;
}

void printstring(char*buf)
{
	int i;
	for (i=0;i< (int) strlen(buf);i++) printchar(buf[i]);
}

void printdecimald0(int d0)
{
	char temp[12];
	sprintf(temp,"%d",d0);
	printstring(temp);
}

void error(char *fmt,...)
{
	char buf[256];
	int i;
	va_list ap;
	va_start(ap,fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);
	for (i=0;i< (int) strlen(buf);i++) os_printchar(buf[i]);
}

void printautocase(int d0)
{
	if (d0 & 128) printchar((char) d0);
	else
	{
		if (wordcase) printchar((char) toupper(d0));
		else if (d5<6) printchar((char) d0);
		else
		{
			wordcase=0;
			printchar((char) toupper(d0));
		}
	}
}

void displaywordref(L9UINT16 Off)
{
	static int mdtmode=0;

	wordcase=0;
	d5=(Off>>12)&7;
	Off&=0xfff;
	if (Off<0xf80)
	{
	/* dwr01 */
		L9BYTE *a0,*oPtr,*a3;
		int d0,d2,i;

		if (mdtmode==1) printchar(0x20);
		mdtmode=1;

		/* setindex */
		a0=dictdata;
		d2=dictdatalen;

	/* dwr02 */
		oPtr=a0;
		while (d2 && Off >= L9WORD(a0+2))
		{
			a0+=4;
			d2--;
		}
	/* dwr04 */
		if (a0==oPtr)
		{
			a0=defdict;
		}
		else
		{
			a0-=4;
			Off-=L9WORD(a0+2);
			a0=startdata+L9WORD(a0);
		}
	/* dwr04b */
		Off++;
		initdict(a0);
		a3=(L9BYTE*) threechars; /* a3 not set in original, prevent possible spam */

		/* dwr05 */
		while (TRUE)
		{
			d0=getdictionarycode();
			if (d0<0x1c)
			{
				/* dwr06 */
				if (d0>=0x1a) d0=getlongcode();
				else d0+=0x61;
				*a3++=d0;
			}
			else
			{
				d0&=3;
				a3=(L9BYTE*) threechars+d0;
				if (--Off==0) break;
			}
		}
		for (i=0;i<d0;i++) printautocase(threechars[i]);

		/* dwr10 */
		while (TRUE)
		{
			d0=getdictionarycode();
			if (d0>=0x1b) return;
			printautocase(getdictionary(d0));
		}
	}

	else
	{
		if (d5&2) printchar(0x20); /* prespace */
		mdtmode=2;
		Off&=0x7f;
		if (Off!=0x7e) printchar((char)Off);
		if (d5&1) printchar(0x20); /* postspace */
	}
}

int getmdlength(L9BYTE **Ptr)
{
	int tot=0,len;
	do
	{
		len=(*(*Ptr)++ -1) & 0x3f;
		tot+=len;
	} while (len==0x3f);
	return tot;
}

void printmessage(int Msg)
{
	L9BYTE* Msgptr=startmd;
	L9BYTE Data;

	int len;
	L9UINT16 Off;

	while (Msg>0 && Msgptr-endmd<=0)
	{
		Data=*Msgptr;
		if (Data&128)
		{
			Msgptr++;
			Msg-=Data&0x7f;
		}
		else Msgptr+=getmdlength(&Msgptr);
		Msg--;
	}
	if (Msg<0 || *Msgptr & 128) return;

	len=getmdlength(&Msgptr);
	if (len==0) return;

	while (len)
	{
		Data=*Msgptr++;
		len--;
		if (Data&128)
		{
		/* long form (reverse word) */
			Off=(Data<<8) + *Msgptr++;
			len--;
		}
		else
		{
			Off=(wordtable[Data*2]<<8) + wordtable[Data*2+1];
		}
		if (Off==0x8f80) break;
		displaywordref(Off);
	}
}

/* v2 message stuff */

int msglenV2(L9BYTE **ptr)
{
	int i=0;
	L9BYTE a;

	/* catch berzerking code */
	if (*ptr >= startdata+FileSize) return 0;

	while ((a=**ptr)==0)
	{
	 (*ptr)++;
	 
	 if (*ptr >= startdata+FileSize) return 0;

	 i+=255;
	}
	i+=a;
	return i;
}

void printcharV2(char c)
{
	if (c==0x25) c=0xd;
	else if (c==0x5f) c=0x20;
	printautocase(c);
}

void displaywordV2(L9BYTE *ptr,int msg)
{
	int n;
	L9BYTE a;
	if (msg==0) return;
	while (--msg)
	{
		ptr+=msglenV2(&ptr);
	}
	n=msglenV2(&ptr);

	while (--n>0)
	{
		a=*++ptr;
		if (a<3) return;

		if (a>=0x5e) displaywordV2(startmdV2-1,a-0x5d);
		else printcharV2((char)(a+0x1d));
	}
}

int msglenV25(L9BYTE **ptr)
{
	L9BYTE *ptr2=*ptr;
	while (ptr2<startdata+FileSize && *ptr2++!=1) ;
	return ptr2-*ptr;
}

void displaywordV25(L9BYTE *ptr,int msg)
{
	int n;
	L9BYTE a;
	while (msg--)
	{
		ptr+=msglenV25(&ptr);
	}
	n=msglenV25(&ptr);

	while (--n>0)
	{
		a=*ptr++;
		if (a<3) return;

		if (a>=0x5e) displaywordV25(startmdV2,a-0x5e);
		else printcharV2((char)(a+0x1d));
	}
}

L9BOOL amessageV2(L9BYTE *ptr,int msg,long *w,long *c)
{
	int n;
	L9BYTE a;
	static int depth=0;
	if (msg==0) return FALSE;
	while (--msg)
	{
		ptr+=msglenV2(&ptr);
	}
	if (ptr >= startdata+FileSize) return FALSE;
	n=msglenV2(&ptr);

	while (--n>0)
	{
		a=*++ptr;
		if (a<3) return TRUE;

		if (a>=0x5e)
		{
			if (++depth>10 || !amessageV2(startmdV2-1,a-0x5d,w,c))
			{
				depth--;
				return FALSE;
			}
			depth--;
		}
		else
		{
			char ch=a+0x1d;
			if (ch==0x5f || ch==' ') (*w)++;
			else (*c)++;
		}
	}
	return TRUE;
}

L9BOOL amessageV25(L9BYTE *ptr,int msg,long *w,long *c)
{
	int n;
	L9BYTE a;
	static int depth=0;

	while (msg--)
	{
		ptr+=msglenV25(&ptr);
	}
	if (ptr >= startdata+FileSize) return FALSE;
	n=msglenV25(&ptr);

	while (--n>0)
	{
		a=*ptr++;
		if (a<3) return TRUE;

		if (a>=0x5e)
		{
			if (++depth>10 || !amessageV25(startmdV2,a-0x5e,w,c))
			{
				depth--;
				return FALSE;
			}
			depth--;
		}
		else
		{
			char ch=a+0x1d;
			if (ch==0x5f || ch==' ') (*w)++;
			else (*c)++;
		}
	}
	return TRUE;
}

L9BOOL analyseV2(double *wl)
{
	long words=0,chars=0;
	int i;
	for (i=1;i<256;i++)
	{
		long w=0,c=0;
		if (amessageV2(startmd,i,&w,&c))
		{
			words+=w;
			chars+=c;
		}
		else return FALSE;
	}
	*wl=words ? (double) chars/words : 0.0;
	return TRUE;
}

L9BOOL analyseV25(double *wl)
{
	long words=0,chars=0;
	int i;
	for (i=0;i<256;i++)
	{
		long w=0,c=0;
		if (amessageV25(startmd,i,&w,&c))
		{
			words+=w;
			chars+=c;
		}
		else return FALSE;
	}

	*wl=words ? (double) chars/words : 0.0;
	return TRUE;
}

void printmessageV2(int Msg)
{
	if (V2MsgType==V2M_NORMAL) displaywordV2(startmd,Msg);
	else displaywordV25(startmd,Msg);
}

L9UINT32 filelength(FILE *f)
{
	L9UINT32 pos,FileSize;

	pos=ftell(f);
	fseek(f,0,SEEK_END);
	FileSize=ftell(f);
	fseek(f,pos,SEEK_SET);
	return FileSize;
}

void L9Allocate(L9BYTE **ptr,L9UINT32 Size)
{
	if (*ptr) free(*ptr);
	*ptr=malloc(Size);
	if (*ptr==NULL) 
	{
		fprintf(stderr,"Unable to allocate memory for the game! Exiting...\n");
		exit(0);
	}
}

void FreeMemory(void)
{
	if (startfile)
	{
		free(startfile);
		startfile=NULL;
	}
	if (pictureaddress)
	{
		free(pictureaddress);
		pictureaddress=NULL;
	}
	if (bitmap)
	{
		free(bitmap);
		bitmap=NULL;
	}
	picturedata=NULL;
	picturesize=0;
	gfxa5=NULL;
}

L9BOOL load(char *filename)
{
	FILE *f=fopen(filename,"rb");
	if (!f) return FALSE;
	FileSize=filelength(f);
	L9Allocate(&startfile,FileSize);

	if (fread(startfile,1,FileSize,f)!=FileSize)
	{
		fclose(f);
		return FALSE;
	}
 	fclose(f);
	return TRUE;
}

L9UINT16 scanmovewa5d0(L9BYTE* Base,L9UINT32 *Pos)
{
	L9UINT16 ret=L9WORD(Base+*Pos);
	(*Pos)+=2;
	return ret;
}

L9UINT32 scangetaddr(int Code,L9BYTE *Base,L9UINT32 *Pos,L9UINT32 acode,int *Mask)
{
	(*Mask)|=0x20;
	if (Code&0x20)
	{
		/* getaddrshort */
		signed char diff=Base[*Pos];
		(*Pos)++;
		return (*Pos)+diff-1;
	}
	else
	{
		return acode+scanmovewa5d0(Base,Pos);
	}
}

void scangetcon(int Code,L9UINT32 *Pos,int *Mask)
{
	(*Pos)++;
	if (!(Code & 64)) (*Pos)++;
	(*Mask)|=0x40;
}

L9BOOL CheckCallDriverV4(L9BYTE* Base,L9UINT32 Pos)
{
	int i,j;

	/* Look back for an assignment from a variable
	 * to list9[0], which is used to specify the
	 * driver call.
	 */
	for (i = 0; i < 2; i++)
	{
		int x = Pos - ((i+1)*3);
		if ((Base[x] == 0x89) && (Base[x+1] == 0x00))
		{
			/* Get the variable being copied to list9[0] */
			int var = Base[x+2];

			/* Look back for an assignment to the variable. */
			for (j = 0; j < 2; j++)
			{
				int y = x - ((j+1)*3);
				if ((Base[y] == 0x48) && (Base[y+2] == var))
				{
					/* If this a V4 driver call? */
					switch (Base[y+1])
					{
					case 0x0E:
					case 0x20:
					case 0x22:
						return TRUE;
					}
					return FALSE;
				}
			}
		}
	}
	return FALSE;
}

L9BOOL ValidateSequence(L9BYTE* Base,L9BYTE* Image,L9UINT32 iPos,L9UINT32 acode,L9UINT32 *Size,L9UINT32 FileSize,L9UINT32 *Min,L9UINT32 *Max,L9BOOL Rts,L9BOOL *JumpKill, L9BOOL *DriverV4)
{
	L9UINT32 Pos;
	L9BOOL Finished=FALSE,Valid;
	L9UINT32 Strange=0;
	int ScanCodeMask;
	int Code;
	*JumpKill=FALSE;

	if (iPos>=FileSize)
		return FALSE;
	Pos=iPos;
	if (Pos<*Min) *Min=Pos;

	if (Image[Pos]) return TRUE; /* hit valid code */

	do
	{
		Code=Base[Pos];
		Valid=TRUE;
		if (Image[Pos]) break; /* converged to found code */
		Image[Pos++]=2;
		if (Pos>*Max) *Max=Pos;

		ScanCodeMask=0x9f;
		if (Code&0x80)
		{
			ScanCodeMask=0xff;
			if ((Code&0x1f)>0xa)
				Valid=FALSE;
			Pos+=2;
		}
		else switch (Code & 0x1f)
		{
			case 0: /* goto */
			{
				L9UINT32 Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,TRUE/*Rts*/,JumpKill,DriverV4);
				Finished=TRUE;
				break;
			}
			case 1: /* intgosub */
			{
				L9UINT32 Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,TRUE,JumpKill,DriverV4);
				break;
			}
			case 2: /* intreturn */
				Valid=Rts;
				Finished=TRUE;
				break;
			case 3: /* printnumber */
				Pos++;
				break;
			case 4: /* messagev */
				Pos++;
				break;
			case 5: /* messagec */
				scangetcon(Code,&Pos,&ScanCodeMask);
				break;
			case 6: /* function */
				switch (Base[Pos++])
				{
					case 2:/* random */
						Pos++;
						break;
					case 1:/* calldriver */
						if (DriverV4)
						{
							if (CheckCallDriverV4(Base,Pos-2))
								*DriverV4 = TRUE;
						}
						break;
					case 3:/* save */
					case 4:/* restore */
					case 5:/* clearworkspace */
					case 6:/* clear stack */
						break;
					case 250: /* printstr */
						while (Base[Pos++]);
						break;

					default:
#ifdef L9DEBUG
						/* printf("scan: illegal function call: %d",Base[Pos-1]); */
#endif
						Valid=FALSE;
						break;
				}
				break;
			case 7: /* input */
				Pos+=4;
				break;
			case 8: /* varcon */
				scangetcon(Code,&Pos,&ScanCodeMask);
				Pos++;
				break;
			case 9: /* varvar */
				Pos+=2;
				break;
			case 10: /* _add */
				Pos+=2;
				break;
			case 11: /* _sub */
				Pos+=2;
				break;
			case 14: /* jump */
#ifdef L9DEBUG
				/* printf("jmp at codestart: %ld",acode); */
#endif
				*JumpKill=TRUE;
				Finished=TRUE;
				break;
			case 15: /* exit */
				Pos+=4;
				break;
			case 16: /* ifeqvt */
			case 17: /* ifnevt */
			case 18: /* ifltvt */
			case 19: /* ifgtvt */
			{
				L9UINT32 Val;
				Pos+=2;
				Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,Rts,JumpKill,DriverV4);
				break;
			}
			case 20: /* screen */
				if (Base[Pos++]) Pos++;
				break;
			case 21: /* cleartg */
				Pos++;
				break;
			case 22: /* picture */
				Pos++;
				break;
			case 23: /* getnextobject */
				Pos+=6;
				break;
			case 24: /* ifeqct */
			case 25: /* ifnect */
			case 26: /* ifltct */
			case 27: /* ifgtct */
			{
				L9UINT32 Val;
				Pos++;
				scangetcon(Code,&Pos,&ScanCodeMask);
				Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,Rts,JumpKill,DriverV4);
				break;
			}
			case 28: /* printinput */
				break;
			case 12: /* ilins */
			case 13: /* ilins */
			case 29: /* ilins */
			case 30: /* ilins */
			case 31: /* ilins */
#ifdef L9DEBUG 
				/* printf("scan: illegal instruction"); */
#endif
				Valid=FALSE;
				break;
		}
	if (Valid && (Code & ~ScanCodeMask))
		Strange++;
	} while (Valid && !Finished && Pos<FileSize); /* && Strange==0); */
	(*Size)+=Pos-iPos;
	return Valid; /* && Strange==0; */
}

L9BYTE calcchecksum(L9BYTE* ptr,L9UINT32 num)
{
	L9BYTE d1=0;
	while (num--!=0) d1+=*ptr++;
	return d1;
}

/*
L9BOOL Check(L9BYTE* StartFile,L9UINT32 FileSize,L9UINT32 Offset)
{
	L9UINT16 d0,num;
	int i;
	L9BYTE* Image;
	L9UINT32 Size=0,Min,Max;
	L9BOOL ret,JumpKill;

	for (i=0;i<12;i++)
	{
		d0=L9WORD (StartFile+Offset+0x12 + i*2);
		if (d0>=0x8000+LISTAREASIZE) return FALSE;
	}

	num=L9WORD(StartFile+Offset)+1;
	if (Offset+num>FileSize) return FALSE;
	if (calcchecksum(StartFile+Offset,num)) return FALSE; 

	Image=calloc(FileSize,1);

	Min=Max=Offset+d0;
	ret=ValidateSequence(StartFile,Image,Offset+d0,Offset+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL);
	free(Image);
	return ret;
}
*/

long Scan(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Chk=malloc(FileSize+1);
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,num,Size,MaxSize=0;
	int j;
	L9UINT16 d0=0,l9,md,ml,dd,dl;
	L9UINT32 Min,Max;
	long Offset=-1;
	L9BOOL JumpKill, DriverV4;

	if ((Chk==NULL)||(Image==NULL))
	{
		fprintf(stderr,"Unable to allocate memory for game scan! Exiting...\n");
		exit(0);
	}

	Chk[0]=0;
	for (i=1;i<=FileSize;i++)
		Chk[i]=Chk[i-1]+StartFile[i-1];

	for (i=0;i<FileSize-33;i++)
	{
		num=L9WORD(StartFile+i)+1;
/*
		Chk[i] = 0 +...+ i-1
		Chk[i+n] = 0 +...+ i+n-1
		Chk[i+n] - Chk[i] = i + ... + i+n
*/
		if (num>0x2000 && i+num<=FileSize && Chk[i+num]==Chk[i])
		{
			md=L9WORD(StartFile+i+0x2);
			ml=L9WORD(StartFile+i+0x4);
			dd=L9WORD(StartFile+i+0xa);
			dl=L9WORD(StartFile+i+0xc);

			if (ml>0 && md>0 && i+md+ml<=FileSize && dd>0 && dl>0 && i+dd+dl*4<=FileSize)
			{
				/* v4 files may have acodeptr in 8000-9000, need to fix */
				for (j=0;j<12;j++)
				{
					d0=L9WORD (StartFile+i+0x12 + j*2);
					if (j!=11 && d0>=0x8000 && d0<0x9000)
					{
						if (d0>=0x8000+LISTAREASIZE) break;
					}
					else if (i+d0>FileSize) break;
				}
				/* list9 ptr must be in listarea, acode ptr in data */
				if (j<12 /*|| (d0>=0x8000 && d0<0x9000)*/) continue;

				l9=L9WORD(StartFile+i+0x12 + 10*2);
				if (l9<0x8000 || l9>=0x8000+LISTAREASIZE) continue;

				Size=0;
				Min=Max=i+d0;
				DriverV4=0;
				if (ValidateSequence(StartFile,Image,i+d0,i+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,&DriverV4))
				{
#ifdef L9DEBUG
					printf("Found valid header at %ld, code size %ld",i,Size);
#endif
					if (Size>MaxSize)
					{
						Offset=i;
						MaxSize=Size;
						L9GameType=DriverV4?L9_V4:L9_V3;
					}
				}
			}
		}
	}
	free(Chk);
	free(Image);
	return Offset;
}

long ScanV2(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Chk=malloc(FileSize+1);
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size,MaxSize=0,num;
	int j;
	L9UINT16 d0=0,l9;
	L9UINT32 Min,Max;
	long Offset=-1;
	L9BOOL JumpKill;

	if ((Chk==NULL)||(Image==NULL))
	{
		fprintf(stderr,"Unable to allocate memory for game scan! Exiting...\n");
		exit(0);
	}

	Chk[0]=0;
	for (i=1;i<=FileSize;i++)
		Chk[i]=Chk[i-1]+StartFile[i-1];

	for (i=0;i<FileSize-28;i++)
	{
		num=L9WORD(StartFile+i+28)+1;
		if (i+num<=FileSize && ((Chk[i+num]-Chk[i+32])&0xff)==StartFile[i+0x1e])
		{
			for (j=0;j<14;j++)
			{
				 d0=L9WORD (StartFile+i+ j*2);
				 if (j!=13 && d0>=0x8000 && d0<0x9000)
				 {
					if (d0>=0x8000+LISTAREASIZE) break;
				 }
				 else if (i+d0>FileSize) break;
			}
			/* list9 ptr must be in listarea, acode ptr in data */
			if (j<14 /*|| (d0>=0x8000 && d0<0x9000)*/) continue;

			l9=L9WORD(StartFile+i+6 + 9*2);
			if (l9<0x8000 || l9>=0x8000+LISTAREASIZE) continue;

			Size=0;
			Min=Max=i+d0;
			if (ValidateSequence(StartFile,Image,i+d0,i+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL))
			{
#ifdef L9DEBUG 
				printf("Found valid V2 header at %ld, code size %ld",i,Size);
#endif
				if (Size>MaxSize)
				{
					Offset=i;
					MaxSize=Size;
				}
			}
		}
	}
	free(Chk);
	free(Image);
	return Offset;
}

long ScanV1(L9BYTE* StartFile,L9UINT32 FileSize)
{
	return -1;
/*
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size;
	int Replace;
	L9BYTE* ImagePtr;
	long MaxPos=-1;
	L9UINT32 MaxCount=0;
	L9UINT32 Min,Max,MaxMin,MaxMax;
	L9BOOL JumpKill,MaxJK;

	L9BYTE c;
	int maxdict,maxdictlen;
	L9BYTE *ptr,*start;

	for (i=0;i<FileSize;i++)
	{
		Size=0;
		Min=Max=i;
		Replace=0;
		if (ValidateSequence(StartFile,Image,i,i,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL))
		{
			if (Size>MaxCount)
			{
				MaxCount=Size;
				MaxMin=Min;
				MaxMax=Max;

				MaxPos=i;
				MaxJK=JumpKill;
			}
			Replace=0;
		}
		for (ImagePtr=Image+Min;ImagePtr<=Image+Max;ImagePtr++) if (*ImagePtr==2) *ImagePtr=Replace;
	}
#ifdef L9DEBUG
	printf("V1scan found code at %ld size %ld",MaxPos,MaxCount);
#endif

	ptr=StartFile;
	maxdictlen=0;
	do
	{
		start=ptr;
		do
		{
			do
			{
				c=*ptr++;
			} while (((c>='A' && c<='Z') || c=='-') && ptr<StartFile+FileSize);
			if (c<0x7f || (((c&0x7f)<'A' || (c&0x7f)>'Z') && (c&0x7f)!='/')) break;
			ptr++;
		} while (TRUE);
		if (ptr-start-1>maxdictlen)
		{
			maxdict=start-StartFile;
			maxdictlen=ptr-start-1;
		}
	} while (ptr<StartFile+FileSize);
#ifdef L9DEBUG
	if (maxdictlen>0) printf("V1scan found dictionary at %ld size %ld",maxdict,maxdictlen);
#endif

	MaxPos=-1;

	free(Image);
	return MaxPos;
*/
}

#ifdef FULLSCAN
void FullScan(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size;
	int Replace;
	L9BYTE* ImagePtr;
	L9UINT32 MaxPos=0;
	L9UINT32 MaxCount=0;
	L9UINT32 Min,Max,MaxMin,MaxMax;
	int Offset;
	L9BOOL JumpKill,MaxJK;
	for (i=0;i<FileSize;i++)
	{
		Size=0;
		Min=Max=i;
		Replace=0;
		if (ValidateSequence(StartFile,Image,i,i,&Size,FileSize,&Min,&Max,FALSE,&JumpKill,NULL))
		{
			if (Size>MaxCount)
			{
				MaxCount=Size;
				MaxMin=Min;
				MaxMax=Max;

				MaxPos=i;
				MaxJK=JumpKill;
			}
			Replace=0;
		}
		for (ImagePtr=Image+Min;ImagePtr<=Image+Max;ImagePtr++) if (*ImagePtr==2) *ImagePtr=Replace;
	}
	printf("%ld %ld %ld %ld %s",MaxPos,MaxCount,MaxMin,MaxMax,MaxJK ? "jmp killed" : "");
	/* search for reference to MaxPos */
	Offset=0x12 + 11*2;
	for (i=0;i<FileSize-Offset-1;i++)
	{
		if ((L9WORD(StartFile+i+Offset)) +i==MaxPos)
		{
			printf("possible v3,4 Code reference at : %ld",i);
			/* startdata=StartFile+i; */
		}
	}
	Offset=13*2;
	for (i=0;i<FileSize-Offset-1;i++)
	{
		if ((L9WORD(StartFile+i+Offset)) +i==MaxPos)
			printf("possible v2 Code reference at : %ld",i);
	}
	free(Image);
}
#endif

L9BOOL intinitialise(char*filename,char*picname)
{
/* init */
/* driverclg */

	int i;
	int hdoffset;
	long Offset;
	FILE *f;

	if (pictureaddress)
	{
		free(pictureaddress);
		pictureaddress=NULL;
	}
	picturedata=NULL;
	picturesize=0;
	gfxa5=NULL;

	if (!load(filename))
	{
		error("\rUnable to load: %s\r",filename);
		return FALSE;
	}

	/* try to load graphics */
	if (picname)
	{
		f=fopen(picname,"rb");
		if (f)
		{
			picturesize=filelength(f);
			L9Allocate(&pictureaddress,picturesize);
			if (fread(pictureaddress,1,picturesize,f)!=picturesize)
			{
				free(pictureaddress);
				pictureaddress=NULL;
				picturesize=0;
			}
			picturedata=pictureaddress;
			fclose(f);
		}
	}
	screencalled=0;
	l9textmode=0;

#ifdef FULLSCAN
	FullScan(startfile,FileSize);
#endif

	Offset=Scan(startfile,FileSize);
	if (Offset<0)
	{
		Offset=ScanV2(startfile,FileSize);
		L9GameType=L9_V2;
		if (Offset<0)
		{
			Offset=ScanV1(startfile,FileSize);
			L9GameType=L9_V1;
			if (Offset<0)
			{
				error("\rUnable to locate valid header in file: %s\r",filename);
			 	return FALSE;
			}
		}
	}

	startdata=startfile+Offset;
	FileSize-=Offset;

/* setup pointers */
	if (L9GameType!=L9_V1)
	{
		/* V2,V3,V4 */

		hdoffset=L9GameType==L9_V2 ? 4 : 0x12;

		for (i=0;i<12;i++)
		{
			L9UINT16 d0=L9WORD(startdata+hdoffset+i*2);
			L9Pointers[i]= (i!=11 && d0>=0x8000 && d0<=0x9000) ? workspace.listarea+d0-0x8000 : startdata+d0;
		}
		absdatablock=L9Pointers[0];
		list2ptr=L9Pointers[3];
		list3ptr=L9Pointers[4];
		/*list9startptr */
/*
		if ((((L9UINT32) L9Pointers[10])&1)==0) L9Pointers[10]++; amiga word access hack
*/
		list9startptr=L9Pointers[10];
		acodeptr=L9Pointers[11];
	}

	switch (L9GameType)
	{
		case L9_V1:
			break;
		case L9_V2:
		{
			double a2,a25;
			startmd=startdata + L9WORD(startdata+0x0);
			startmdV2=startdata + L9WORD(startdata+0x2);

			/* determine message type */
			if (analyseV2(&a2) && a2>2 && a2<10)
			{
				V2MsgType=V2M_NORMAL;
				#ifdef L9DEBUG
				printf("V2 msg table: normal, wordlen=%.2lf",a2);
				#endif
			}
			else if (analyseV25(&a25) && a25>2 && a25<10)
			{
				V2MsgType=V2M_ERIK;
				#ifdef L9DEBUG
				printf("V2 msg table: Erik, wordlen=%.2lf",a25);
				#endif
			}
			else
			{
				error("\rUnable to identify V2 message table in file: %s\r",filename);
				return FALSE;
			}
			break;
		}
		case L9_V3:
		case L9_V4:
			startmd=startdata + L9WORD(startdata+0x2);
			endmd=startmd + L9WORD(startdata+0x4);
			defdict=startdata+L9WORD(startdata+6);
			endwdp5=defdict + 5 + L9WORD(startdata+0x8);
			dictdata=startdata+L9WORD(startdata+0x0a);
			dictdatalen=L9WORD(startdata+0x0c);
			wordtable=startdata + L9WORD(startdata+0xe);
			break;
	}

#ifndef NO_SCAN_GRAPHICS
	/* If there was no graphics file, look in the game data */
	if (picturedata==NULL)
	{
		int sz=FileSize-(acodeptr-startdata);
		int i=0;
		while ((i<sz-0x1000)&&(picturedata==NULL))
		{
			picturedata=acodeptr+i;
			picturesize=sz-i;
			if (!checksubs())
			{
				picturedata=NULL;
				picturesize=0;
			}
			i++;
		}
	}
#endif
	return TRUE;
}

L9BOOL checksumgamedata(void)
{
	return calcchecksum(startdata,L9WORD(startdata)+1)==0;
}

L9UINT16 movewa5d0(void)
{
	L9UINT16 ret=L9WORD(codeptr);
	codeptr+=2;
	return ret;
}

L9UINT16 getcon(void)
{
	if (code & 64)
	{
		/* getconsmall */
		return *codeptr++;
	}
	else return movewa5d0();
}

L9BYTE* getaddr(void)
{
	if (code&0x20)
	{
		/* getaddrshort */
		signed char diff=*codeptr++;
		return codeptr+ diff-1;
	}
	else
	{
		return acodeptr+movewa5d0();
	}
}

L9UINT16 *getvar(void)
{
#ifndef CODEFOLLOW
	return workspace.vartable + *codeptr++;
#else
	cfvar2=cfvar;
	return cfvar=workspace.vartable + *codeptr++;
#endif
}

void Goto(void)
{
	codeptr=getaddr();
}

void intgosub(void)
{
	L9BYTE* newcodeptr=getaddr();
	if (workspace.stackptr==STACKSIZE)
	{
		error("\rStack overflow error\r");
		Running=FALSE;
		return;
	}
	workspace.stack[workspace.stackptr++]=(L9UINT16) (codeptr-acodeptr);
	codeptr=newcodeptr;
}

void intreturn(void)
{
	if (workspace.stackptr==0)
	{
		error("\rStack underflow error\r");
		Running=FALSE;
		return;
	}
	codeptr=acodeptr+workspace.stack[--workspace.stackptr];
}

void printnumber(void)
{
	printdecimald0(*getvar());
}

void messagec(void)
{
	if (L9GameType==L9_V2)
		printmessageV2(getcon());
	else
		printmessage(getcon());
}

void messagev(void)
{
	if (L9GameType==L9_V2)
		printmessageV2(*getvar());
	else
		printmessage(*getvar());
}

void init(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - init");
#endif
}

void randomnumber(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - randomnumber");
#endif
	L9SETWORD(a6,rand());
}

void driverclg(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverclg");
#endif
}

void _line(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - line");
#endif
}

void fill(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - fill");
#endif
}

void driverchgcol(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverchgcol");
#endif
}

void drivercalcchecksum(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - calcchecksum");
#endif
}

void driveroswrch(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driveroswrch");
#endif
}

void driverosrdch(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverosrdch");
#endif

	os_flush();
	if (Cheating) {
		*a6 = '\r';
	} else {
		/* max delay of 1/50 sec */
		*a6=os_readchar(20);
	}
}

void driversavefile(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driversavefile");
#endif
}

void driverloadfile(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverloadfile");
#endif
}

void settext(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - settext");
#endif
}

void resettask(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - resettask");
#endif
}

void driverinputline(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - driverinputline");
#endif
}

void returntogem(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - returntogem");
#endif
}

void lensdisplay(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - lensdisplay");
#endif

	printstring("\rLenslok code is ");
	printchar(*a6);
	printchar(*(a6+1));
	printchar('\r');
}

void allocspace(L9BYTE* a6)
{
#ifdef L9DEBUG
	printf("driver - allocspace");
#endif
}

void driver14(L9BYTE *a6)
{
#ifdef L9DEBUG
	printf("driver - call 14");
#endif

	*a6 = 0;
}

void showbitmap(L9BYTE *a6)
{
#ifdef L9DEBUG
	printf("driver - showbitmap");
#endif

	os_show_bitmap(a6[1],a6[3],a6[5]);
}

void checkfordisc(L9BYTE *a6)
{
#ifdef L9DEBUG
	printf("driver - checkfordisc");
#endif

	*a6 = 0;
	list9startptr[2] = 0;
}

void driver(int d0,L9BYTE* a6)
{
	switch (d0)
	{
		case 0: init(a6); break;
		case 0x0c: randomnumber(a6); break;
		case 0x10: driverclg(a6); break;
		case 0x11: _line(a6); break;
		case 0x12: fill(a6); break;
		case 0x13: driverchgcol(a6); break;
		case 0x01: drivercalcchecksum(a6); break;
		case 0x02: driveroswrch(a6); break;
		case 0x03: driverosrdch(a6); break;
		case 0x05: driversavefile(a6); break;
		case 0x06: driverloadfile(a6); break;
		case 0x07: settext(a6); break;
		case 0x08: resettask(a6); break;
		case 0x04: driverinputline(a6); break;
		case 0x09: returntogem(a6); break;
/*
		case 0x16: ramsave(a6); break;
		case 0x17: ramload(a6); break;
*/
		case 0x19: lensdisplay(a6); break;
		case 0x1e: allocspace(a6); break;
/* v4 */
		case 0x0e: driver14(a6); break;
		case 0x20: showbitmap(a6); break;
		case 0x22: checkfordisc(a6); break;
	}
}

void ramsave(int i)
{
#ifdef L9DEBUG
	printf("driver - ramsave %d",i);
#endif

	memmove(ramsavearea+i,workspace.vartable,sizeof(SaveStruct));
}

void ramload(int i)
{
#ifdef L9DEBUG
	printf("driver - ramload %d",i);
#endif

	memmove(workspace.vartable,ramsavearea+i,sizeof(SaveStruct));
}

void calldriver(void)
{
	L9BYTE* a6=list9startptr;
	int d0=*a6++;
#ifdef CODEFOLLOW
	fprintf(f," %s",drivercalls[d0]);
#endif

	if (d0==0x16 || d0==0x17)
	{
		int d1=*a6;
		if (d1>0xfa) *a6=1;
		else if (d1+1>=RAMSAVESLOTS) *a6=0xff;
		else
		{
			*a6=0;
			if (d0==0x16) ramsave(d1+1); else ramload(d1+1);
		}
		*list9startptr=*a6;
	}
	else if (d0==0x0b)
	{
		char NewName[MAX_PATH];
		strcpy(NewName,LastGame);
		if (*a6==0)
		{
			printstring("\rSearching for next sub-game file.\r");
			if (!os_get_game_file(NewName,MAX_PATH))
			{
				printstring("\rFailed to load game.\r");
				return;
			}
		}
		else
		{
			os_set_filenumber(NewName,MAX_PATH,*a6);
		}
		LoadGame2(NewName,NULL);
	}
	else driver(d0,a6);
}

void L9Random(void)
{
	randomseed=(((randomseed<<8) + 0x0a - randomseed) <<2) + randomseed + 1;
	*getvar()=randomseed & 0xff;
}

void save(void)
{
	L9UINT16 checksum;
	int i;
#ifdef L9DEBUG
	printf("function - save");
#endif
/* does a full save, workpace, stack, codeptr, stackptr, game name, checksum */

	workspace.Id=L9_ID;
	workspace.codeptr=codeptr-acodeptr;
	workspace.listsize=LISTAREASIZE;
	workspace.stacksize=STACKSIZE;
	workspace.filenamesize=MAX_PATH;
	workspace.checksum=0;
	strcpy(workspace.filename,LastGame);

	checksum=0;
	for (i=0;i<sizeof(GameState);i++) checksum+=((L9BYTE*) &workspace)[i];
	workspace.checksum=checksum;

	if (os_save_file((L9BYTE*) &workspace,sizeof(workspace))) printstring("\rGame saved.\r");
	else printstring("\rUnable to save game.\r");
}

L9BOOL CheckFile(GameState *gs)
{
	L9UINT16 checksum;
	int i;
	char c = 'Y';

	if (gs->Id!=L9_ID) return FALSE;
	checksum=gs->checksum;
	gs->checksum=0;
	for (i=0;i<sizeof(GameState);i++) checksum-=*((L9BYTE*) gs+i);
	if (checksum) return FALSE;
	if (stricmp(gs->filename,LastGame))
	{
		printstring("\rWarning: game path name does not match, you may be about to load this position file into the wrong story file.\r");
		printstring("Are you sure you want to restore? (Y/N)");
		os_flush();

		c = '\0';
		while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N')) 
			c = os_readchar(20);
	}
	if ((c == 'y') || (c == 'Y'))
		return TRUE;
	return FALSE;
}

void NormalRestore(void)
{
	GameState temp;
	int Bytes;
#ifdef L9DEBUG
	printf("function - restore");
#endif
	if (Cheating)
	{
		/* not really an error */
		Cheating=FALSE;
		error("\rWord is: %s\r",ibuff);
	}

	if (os_load_file((L9BYTE*) &temp,&Bytes,sizeof(GameState)))
	{
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memmove(workspace.vartable,temp.vartable,sizeof(SaveStruct));
		}
		else
		{
			printstring("\rSorry, unrecognised format. Unable to restore\r");
		}
	}
	else printstring("\rUnable to restore game.\r");
}

void restore(void)
{
	int Bytes;
	GameState temp;
	if (os_load_file((L9BYTE*) &temp,&Bytes,sizeof(GameState)))
	{
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* full restore */
			memmove(&workspace,&temp,sizeof(GameState));
			codeptr=acodeptr+workspace.codeptr;
		}
		else
		{
			printstring("\rSorry, unrecognised format. Unable to restore\r");
		}
	}
	else printstring("\rUnable to restore game.\r");
}

void clearworkspace(void)
{
	memset(workspace.vartable,0,sizeof(workspace.vartable));
}

void ilins(int d0)
{
	error("\rIllegal instruction: %d\r",d0);
	Running=FALSE;
}

void function(void)
{
	int d0=*codeptr++;
#ifdef CODEFOLLOW
	fprintf(f," %s",d0==250 ? "printstr" : functions[d0-1]);
#endif

	switch (d0)
	{
		case 1: calldriver(); break;
		case 2: L9Random(); break;
		case 3: save(); break;
		case 4: NormalRestore(); break;
		case 5: clearworkspace(); break;
		case 6: workspace.stackptr=0; break;
		case 250:
			printstring((char*) codeptr);
			while (*codeptr++);
			break;

		default: ilins(d0);
	}
}

void findmsgequiv(int d7)
{
	int d4=-1,d0;
	L9BYTE* a2=startmd;

	do
	{
		d4++;
		if (a2>endmd) return;
		d0=*a2;
		if (d0&0x80)
		{
			a2++;
			d4+=d0&0x7f;
		}
		else if (d0&0x40)
		{
			int d6=getmdlength(&a2);
			do
			{
				int d1;
				if (d6==0) break;

				d1=*a2++;
				d6--;
				if (d1 & 0x80)
				{
					if (d1<0x90)
					{
						a2++;
						d6--;
					}
					else
					{
						d0=(d1<<8) + *a2++;
						d6--;
						if (d7==(d0 & 0xfff))
						{
							d0=((d0<<1)&0xe000) | d4;
							list9ptr[1]=d0;
							list9ptr[0]=d0>>8;
							list9ptr+=2;
							if (list9ptr>=list9startptr+0x20) return;
						}
					}
				}
			} while (TRUE);
		}
		else a2+=getmdlength(&a2);
	} while (TRUE);
}

L9BOOL unpackword(void)
{
	L9BYTE *a3;

	if (unpackd3==0x1b) return TRUE;

	a3=(L9BYTE*) threechars + (unpackd3&3);

/*uw01 */
	while (TRUE)
	{
		L9BYTE d0=getdictionarycode();
		if (dictptr>=endwdp5) return TRUE;
		if (d0>=0x1b)
		{
			*a3=0;
			unpackd3=d0;
			return FALSE;
		}
		*a3++=getdictionary(d0);
	}
}

L9BOOL initunpack(L9BYTE* ptr)
{
	initdict(ptr);
	unpackd3=0x1c;
	return unpackword();
}

int partword(char c)
{
	c=tolower(c);

	if (c==0x27 || c==0x2d) return 0;
	if (c<0x30) return 1;
	if (c<0x3a) return 0;
	if (c<0x61) return 1;
	if (c<0x7b) return 0;
	return 1;
}

L9UINT32 readdecimal(char *buff)
{
	return atol(buff);
}

void checknumber(void)
{
	if (*obuff>=0x30 && *obuff<0x3a)
	{
		if (L9GameType==L9_V4)
		{
			*list9ptr=1;
			L9SETWORD(list9ptr+1,readdecimal(obuff));
			L9SETWORD(list9ptr+3,0);
		}
		else
		{
			L9SETDWORD(list9ptr,readdecimal(obuff));
			L9SETWORD(list9ptr+4,0);
		}
	}
	else
	{
		L9SETWORD(list9ptr,0x8000);
		L9SETWORD(list9ptr+2,0);
	}
}

void NextCheat(void)
{
	/* restore game status */
	memmove(&workspace,&CheatWorkspace,sizeof(GameState));
	codeptr=acodeptr+workspace.codeptr;

	if (!((L9GameType==L9_V2) ? GetWordV2(ibuff,CheatWord++) : GetWordV3(ibuff,CheatWord++)))
	{
		Cheating=FALSE;
		printstring("\rCheat failed.\r");
		*ibuff=0;
	}
}

void StartCheat(void)
{
	Cheating=TRUE;
	CheatWord=0;

	/* save current game status */
	memmove(&CheatWorkspace,&workspace,sizeof(GameState));
	CheatWorkspace.codeptr=codeptr-acodeptr;

	NextCheat();
}

/* v3,4 input routine */

L9BOOL GetWordV3(char *buff,int Word)
{
	int i;
	int subdict=0;
	/* 26*4-1=103 */

	initunpack(startdata+L9WORD(dictdata));
	unpackword();

	while (Word--)
	{
		if (unpackword())
		{
			if (++subdict==dictdatalen) return FALSE;
			initunpack(startdata+L9WORD(dictdata+(subdict<<2)));
			Word++; /* force unpack again */
		}
	}
	strcpy(buff,threechars);
	for (i=0;i<(int)strlen(buff);i++) buff[i]&=0x7f;
	return TRUE;
}

L9BOOL CheckHash(void)
{
	if (stricmp(ibuff,"#cheat")==0) StartCheat();
	else if (stricmp(ibuff,"#save")==0)
	{
		save();
		return TRUE;
	}
	else if (stricmp(ibuff,"#restore")==0)
	{
		restore();
		return TRUE;
	}
	else if (stricmp(ibuff,"#quit")==0)
	{
		StopGame();
		printstring("\rGame Terminated\r");
		return TRUE;
	}
	else if (stricmp(ibuff,"#dictionary")==0)
	{
		CheatWord=0;
		printstring("\r");
		while ((L9GameType==L9_V2) ? GetWordV2(ibuff,CheatWord++) : GetWordV3(ibuff,CheatWord++))
		{
			error("%s ",ibuff);
			if (os_stoplist() || !Running) break;
		}
		printstring("\r");
		return TRUE;
	}
	else if (strnicmp(ibuff,"#picture ",9)==0)
	{
		int pic = 0;
		if (sscanf(ibuff+9,"%d",&pic) == 1)
		{
			if (L9GameType==L9_V4)
				os_show_bitmap(pic,0,0);
			else
				show_picture(pic);
		}

		lastactualchar = 0;
		printchar('\r');
		return TRUE;
	}
	return FALSE;
}

L9BOOL corruptinginput(void)
{
	L9BYTE *a0,*a2,*a6;
	int d0,d1,d2,keywordnumber,abrevword;
	char *iptr;

	list9ptr=list9startptr;

	if (ibuffptr==NULL)
	{
		if (Cheating) NextCheat();
		else
		{
			/* flush */
			os_flush();
			lastchar='.';
			/* get input */
			if (!os_input(ibuff,IBUFFSIZE)) return FALSE; /* fall through */
			if (CheckHash()) return FALSE;

			/* check for invalid chars */
			for (iptr=ibuff;*iptr!=0;iptr++)
			{
				if (!isalnum(*iptr))
					*iptr=' ';
			}

			/* force CR but prevent others */
			os_printchar(lastactualchar='\r');
		}
		ibuffptr=(L9BYTE*) ibuff;
	}

	a2=(L9BYTE*) obuff;
	a6=ibuffptr;

/*ip05 */
	while (TRUE)
	{
		d0=*a6++;
		if (d0==0)
		{
			ibuffptr=NULL;
			L9SETWORD(list9ptr,0);
			return TRUE;
		}
		if (partword((char)d0)==0) break;
		if (d0!=0x20)
		{
			ibuffptr=a6;
			L9SETWORD(list9ptr,d0);
			L9SETWORD(list9ptr+2,0);
			*a2=0x20;
			keywordnumber=-1;
			return TRUE;
		}
	}

	a6--;
/*ip06loop */
	do
	{
		d0=*a6++;
		if (partword((char)d0)==1) break;
		d0=tolower(d0);
		*a2++=d0;
	} while (a2<(L9BYTE*) obuff+0x1f);
/*ip06a */
	*a2=0x20;
	a6--;
	ibuffptr=a6;
	abrevword=-1;
	keywordnumber=-1;
	list9ptr=list9startptr;
/* setindex */
	a0=dictdata;
	d2=dictdatalen;
	d0=*obuff-0x61;
	if (d0<0)
	{
		a6=defdict;
		d1=0;
	}
	else
	{
	/*ip10 */
		d1=0x67;
		if (d0<0x1a)
		{
			d1=d0<<2;
			d0=obuff[1];
			if (d0!=0x20) d1+=((d0-0x61)>>3)&3;
		}
	/*ip13 */
		if (d1>=d2)
		{
			checknumber();
			return TRUE;
		}
		a0+=d1<<2;
		a6=startdata+L9WORD(a0);
		d1=L9WORD(a0+2);
	}
/*ip13gotwordnumber */

	initunpack(a6);
/*ip14 */
	d1--;
	do
	{
		d1++;
		if (unpackword())
		{/* ip21b */
			if (abrevword==-1) break; /* goto ip22 */
			else d0=abrevword; /* goto ip18b */
		}
		else
		{
			L9BYTE* a1=(L9BYTE*) threechars;
			int d6=-1;

			a0=(L9BYTE*) obuff;
		/*ip15 */
			do
			{
				d6++;
				d0=tolower(*a1++ & 0x7f);
				d2=*a0++;
			} while (d0==d2);

			if (d2!=0x20)
			{/* ip17 */
				if (abrevword==-1) continue;
				else d0=-1;
			}
			else if (d0==0) d0=d1;
			else if (abrevword!=-1) break;
			else if (d6>=4) d0=d1;
			else
			{
				abrevword=d1;
				continue;
			}
		}
		/*ip18b */
		findmsgequiv(d1);

		abrevword=-1;
		if (list9ptr!=list9startptr)
		{
			L9SETWORD(list9ptr,0);
			return TRUE;
		}
	} while (TRUE);
/* ip22 */
	checknumber();
	return TRUE;
}

/* version 2 stuff hacked from bbc v2 files */

L9BOOL GetWordV2(char *buff,int Word)
{
	L9BYTE *ptr=L9Pointers[1],x;

	while (Word--)
	{
		do
		{
			x=*ptr++;
		} while (x>0 && x<0x7f);
		if (x==0) return FALSE; /* no more words */
		ptr++;
	}
	do
	{
		x=*ptr++;
		*buff++=x&0x7f;
	} while (x>0 && x<0x7f);
	*buff=0;
	return TRUE;
}

L9BOOL inputV2(int *wordcount)
{
	L9BYTE a,x;
	L9BYTE *ibuffptr,*obuffptr,*ptr,*list0ptr;
	char *iptr;

	if (Cheating) NextCheat();
	else
	{
		os_flush();
		lastchar='.';
		/* get input */
		if (!os_input(ibuff,IBUFFSIZE)) return FALSE; /* fall through */
		if (CheckHash()) return FALSE;

		/* check for invalid chars */
		for (iptr=ibuff;*iptr!=0;iptr++)
		{
			if (!isalnum(*iptr))
				*iptr=' ';
		}

		/* force CR but prevent others */
		os_printchar(lastactualchar='\r');
	}
	/* add space onto end */
	ibuffptr=(L9BYTE*) strchr(ibuff,0);
	*ibuffptr++=32;
	*ibuffptr=0;

	*wordcount=0;
	ibuffptr=(L9BYTE*) ibuff;
	obuffptr=(L9BYTE*) obuff;
	/* ibuffptr=76,77 */
	/* obuffptr=84,85 */
	/* list0ptr=7c,7d */
	list0ptr=L9Pointers[1];

	while (*ibuffptr==32) ++ibuffptr;

	ptr=ibuffptr;
	do
	{
		while (*ptr==32) ++ptr;
		if (*ptr==0) break;
		(*wordcount)++;
		do
		{
			a=*++ptr;
		} while (a!=32 && a!=0);
	} while (*ptr>0);

	while (TRUE)
	{
		ptr=ibuffptr; /* 7a,7b */
		while (*ibuffptr==32) ++ibuffptr;

		while (TRUE)
		{
			a=*ibuffptr;
			x=*list0ptr++;

			if (a==32) break;
			if (a==0)
			{
				*obuffptr++=0;
				return TRUE;
			}

			++ibuffptr;
			if (tolower(x&0x7f) != tolower(a))
			{
				while (x>0 && x<0x7f) x=*list0ptr++;
				if (x==0)
				{
					do
					{
						a=*ibuffptr++;
						if (a==0)
						{
							*obuffptr=0;
							return TRUE;
						}
					} while (a!=32);
					while (*ibuffptr==32) ++ibuffptr;
					list0ptr=L9Pointers[1];
					ptr=ibuffptr;
				}
				else
				{
					list0ptr++;
					ibuffptr=ptr;
				}
			}
			else if (x>=0x7f) break;
		}

		a=*ibuffptr;
		if (a!=32)
		{
			ibuffptr=ptr;
			list0ptr+=2;
			continue;
		}
		--list0ptr;
		while (*list0ptr++<0x7e);
		*obuffptr++=*list0ptr;
		while (*ibuffptr==32) ++ibuffptr;
		list0ptr=L9Pointers[1];
	}
}

void input(void)
{
	/* if corruptinginput() returns false then, input will be called again
	   next time around instructionloop, this is used when save() and restore()
	   are called out of line */

	codeptr--;
	if (L9GameType==L9_V2)
	{
		int wordcount;
		if (inputV2(&wordcount))
		{
			L9BYTE *obuffptr=(L9BYTE*) obuff;
			codeptr++;
			*getvar()=*obuffptr++;
			*getvar()=*obuffptr++;
			*getvar()=*obuffptr;
			*getvar()=wordcount;
		}
	}
	else
		if (corruptinginput()) codeptr+=5;
}

void varcon(void)
{
	L9UINT16 d6=getcon();
	*getvar()=d6;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=%d)",cfvar-workspace.vartable,*cfvar);
#endif
}

void varvar(void)
{
	L9UINT16 d6=*getvar();
	*getvar()=d6;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=Var[%d] (=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d6);
#endif
}

void _add(void)
{
	L9UINT16 d0=*getvar();
	*getvar()+=d0;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]+=Var[%d] (+=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d0);
#endif
}

void _sub(void)
{
	L9UINT16 d0=*getvar();
	*getvar()-=d0;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]-=Var[%d] (-=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d0);
#endif
}

void jump(void)
{
	L9UINT16 d0=L9WORD(codeptr);
	L9BYTE* a0;
	codeptr+=2;

	a0=acodeptr+((d0+((*getvar())<<1))&0xffff);
	codeptr=acodeptr+L9WORD(a0);
}

/* bug */
void exit1(L9BYTE *d4,L9BYTE *d5,L9BYTE d6,L9BYTE d7)
{
	L9BYTE* a0=absdatablock;
	L9BYTE d1=d7,d0;
	if (--d1)
	{
		do
		{
			d0=*a0;
			a0+=2;
		}
		while ((d0&0x80)==0 || --d1);
	}

	do
	{
		*d4=*a0++;
		if (((*d4)&0xf)==d6)
		{
			*d5=*a0;
			return;
		}
		a0++;
	}
	while (((*d4)&0x80)==0);

	/* notfn4 */
	d6=exitreversaltable[d6];
	a0=absdatablock;
	*d5=1;

	do
	{
		*d4=*a0++;
		if (((*d4)&0x10)==0 || ((*d4)&0xf)!=d6) a0++;
		else if (*a0++==d7) return;
		/* exit6noinc */
		if ((*d4)&0x80) (*d5)++;
	} while (*d4);
	*d5=0;
}

void Exit(void)
{
	L9BYTE d4,d5;
	L9BYTE d7=(L9BYTE) *getvar();
	L9BYTE d6=(L9BYTE) *getvar();
	exit1(&d4,&d5,d6,d7);

	*getvar()=(d4&0x70)>>4;
	*getvar()=d5;
}

void ifeqvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0==d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]=Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0==d1 ? "Yes":"No");
#endif
}

void ifnevt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0!=d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]!=Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0!=d1 ? "Yes":"No");
#endif
}

void ifltvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0<d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]<Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0<d1 ? "Yes":"No");
#endif
}

void ifgtvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0>d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]>Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0>d1 ? "Yes":"No");
#endif
}

int scalex(int x)
{
	if (scalegfx)
		return (L9GameType == L9_V2) ? x>>6 : x>>5;
	return x;
}

int scaley(int y)
{
	if (scalegfx)
		return 96 - (((y>>5)+(y>>6))>>3);
	return (96<<3) - ((y>>5)+(y>>6));
}

void _screen(void)
{
	int mode = 0;

	l9textmode = *codeptr++;
	if (l9textmode)
	{
		if (L9GameType==L9_V4)
			mode = 2;
		else if (picturedata)
			mode = 1;
	}
	os_graphics(mode);

	screencalled = 1;

#ifdef L9DEBUG
	printf("screen %s",l9textmode ? "graphics" : "text");
#endif

	if (l9textmode)
	{
		codeptr++;
/* clearg */
/* gintclearg */
		os_cleargraphics();

		/* title pic */
		if (showtitle==1 && mode==2)
		{
			showtitle = 0;
			os_show_bitmap(0,0,0);
		}
	}
/* screent */
}

void cleartg(void)
{
	int d0 = *codeptr++;
#ifdef L9DEBUG
	printf("cleartg %s",d0 ? "graphics" : "text");
#endif

	if (d0)
	{
/* clearg */
		if (l9textmode)
/* gintclearg */
			os_cleargraphics();
	}
/* cleart */
/* oswrch(0x0c) */
}

L9BOOL validgfxptr(L9BYTE* a5)
{
	return ((a5 >= picturedata) && (a5 < picturedata+picturesize));
}

L9BOOL findsub(int d0,L9BYTE** a5)
{
	int d1,d2,d3,d4;

	d1=d0 << 4;
	d2=d1 >> 8;
	*a5=picturedata;
/* findsubloop */
	while (TRUE)
	{
		d3=*(*a5)++;
		if (!validgfxptr(*a5))
			return FALSE;
		if (d3&0x80) 
			return FALSE;
		if (d2==d3)
		{
			if ((d1&0xff)==(*(*a5) & 0xf0))
			{
				(*a5)+=2;
				return TRUE;
			}
		}

		d3=*(*a5)++ & 0x0f;
		if (!validgfxptr(*a5))
			return FALSE;

		d4=**a5;
		if ((d3|d4)==0)
			return FALSE;

		(*a5)+=(d3<<8) + d4 - 2;
		if (!validgfxptr(*a5))
			return FALSE;
	}
}

L9BOOL checksubs(void)
{
	L9BYTE* a5;
	int i,cnt=0;

	if (picturedata[0]!=0 || picturedata[1]!=0)
		return FALSE;

	for (i = 1; i < 50; i++)
	{
		if (findsub(i,&a5))
			cnt++;
	}
	return (cnt > 30);
}

void gosubd0(int d0, L9BYTE** a5)
{
	if (GfxStackPos < GFXSTACKSIZE)
	{
		GfxStack[GfxStackPos].a5 = *a5;
		GfxStack[GfxStackPos].scale = scale;
		GfxStackPos++;

		if (findsub(d0,a5) == FALSE)
		{
			GfxStackPos--;
			*a5 = GfxStack[GfxStackPos].a5;
			scale = GfxStack[GfxStackPos].scale;
		}
	}
}

void newxy(int x, int y)
{
	drawx += (x*scale)&~7;
	drawy += (y*scale)&~7;
}

/* sdraw instruction plus arguments are stored in an 8 bit word.
       76543210
       iixxxyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void sdraw(int d7)
{
	int x,y,x1,y1;

/* getxy1 */
	x = (d7&0x18)>>3;
	if (d7&0x20)
		x = (x|0xfc) - 0x100;
	y = (d7&0x3)<<2;
	if (d7&0x4)
		y = (y|0xf0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;

/* gintline */
	x1 = drawx;
	y1 = drawy;
	newxy(x,y);

#ifdef L9DEBUG
	printf("gfx - sdraw (%d,%d) (%d,%d) colours %d,%d",
		x1,y1,drawx,drawy,gintcolour&3,option&3);
#endif

	os_drawline(scalex(x1),scaley(y1),scalex(drawx),scaley(drawy),
		gintcolour&3,option&3);
}

/* smove instruction plus arguments are stored in an 8 bit word.
       76543210
       iixxxyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void smove(int d7)
{
	int x,y;

/* getxy1 */
	x = (d7&0x18)>>3;
	if (d7&0x20)
		x = (x|0xfc) - 0x100;
	y = (d7&0x3)<<2;
	if (d7&0x4)
		y = (y|0xf0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;
	newxy(x,y);
}

void sgosub(int d7, L9BYTE** a5)
{
	int d0 = d7&0x3f;
#ifdef L9DEBUG
	printf("gfx - sgosub 0x%.2x",d0);
#endif
	gosubd0(d0,a5);
}

/* draw instruction plus arguments are stored in a 16 bit word.
       FEDCBA9876543210
       iiiiixxxxxxyyyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void draw(int d7, L9BYTE** a5)
{
	int xy,x,y,x1,y1;

/* getxy2 */
	xy = (d7<<8)+(*(*a5)++);
	x = (xy&0x3e0)>>5;
	if (xy&0x400)
		x = (x|0xe0) - 0x100;
	y = (xy&0xf)<<2;
	if (xy&0x10)
		y = (y|0xc0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;

/* gintline */
	x1 = drawx;
	y1 = drawy;
	newxy(x,y);

#ifdef L9DEBUG
	printf("gfx - draw (%d,%d) (%d,%d) colours %d,%d",
		x1,y1,drawx,drawy,gintcolour&3,option&3);
#endif

	os_drawline(scalex(x1),scaley(y1),scalex(drawx),scaley(drawy),
		gintcolour&3,option&3);
}

/* move instruction plus arguments are stored in a 16 bit word.
       FEDCBA9876543210
       iiiiixxxxxxyyyyy
   where i is instruction code
         x is x argument, high bit is sign
         y is y argument, high bit is sign
*/
void _move(int d7, L9BYTE** a5)
{
	int xy,x,y;

/* getxy2 */
	xy = (d7<<8)+(*(*a5)++);
	x = (xy&0x3e0)>>5;
	if (xy&0x400)
		x = (x|0xe0) - 0x100;
	y = (xy&0xf)<<2;
	if (xy&0x10)
		y = (y|0xc0) - 0x100;

	if (reflectflag&2)
		x = -x;
	if (reflectflag&1)
		y = -y;
	newxy(x,y);
}

void icolour(int d7)
{
	gintcolour = d7&3;
#ifdef L9DEBUG
	printf("gfx - icolour 0x%.2x",gintcolour);
#endif
}

void size(int d7)
{
	static int sizetable[7] = { 0x02,0x04,0x06,0x07,0x09,0x0c,0x10 };

	d7 &= 7;
	if (d7)
	{
		int d0 = (scale*sizetable[d7-1])>>3;
		scale = (d0 < 0x100) ? d0 : 0xff;
	}
	else
/* sizereset */
		scale = 0x80;

#ifdef L9DEBUG
	printf("gfx - size 0x%.2x",scale);
#endif
}

void gintfill(int d7)
{
	if ((d7&7) == 0)
/* filla */
		d7 = gintcolour;
	else
		d7 &= 3;
/* fillb */

#ifdef L9DEBUG
	printf("gfx - gintfill (%d,%d) colours %d,%d",drawx,drawy,d7&3,option&3);
#endif

	os_fill(scalex(drawx),scaley(drawy),d7&3,option&3);
}

void gosub(int d7, L9BYTE** a5)
{
	int d0 = ((d7&7)<<8)+(*(*a5)++);
#ifdef L9DEBUG
	printf("gfx - gosub 0x%.2x",d0);
#endif
	gosubd0(d0,a5);
}

void reflect(int d7)
{
#ifdef L9DEBUG
	printf("gfx - reflect 0x%.2x",d7);
#endif

	if (d7&4)
	{
		d7 &= 3;
		d7 ^= reflectflag;
	}
/* reflect1 */
	reflectflag = d7;
}

void notimp(void)
{
#ifdef L9DEBUG
	printf("gfx - notimp");
#endif
}

void gintchgcol(L9BYTE** a5)
{
	int d0 = *(*a5)++;

#ifdef L9DEBUG
	printf("gfx - gintchgcol %d %d",(d0>>3)&3,d0&7);
#endif

	os_setcolour((d0>>3)&3,d0&7);
}

void amove(L9BYTE** a5)
{
	drawx = 0x40*(*(*a5)++);
	drawy = 0x40*(*(*a5)++);
#ifdef L9DEBUG
	printf("gfx - amove (%d,%d)",drawx,drawy);
#endif
}

void opt(L9BYTE** a5)
{
	int d0 = *(*a5)++;
#ifdef L9DEBUG
	printf("gfx - opt 0x%.2x",d0);
#endif

	if (d0)
		d0 = (d0&3)|0x80;
/* optend */
	option = d0;
}

void restorescale(void)
{
#ifdef L9DEBUG
	printf("gfx - restorescale");
#endif
	if (GfxStackPos > 0)
		scale = GfxStack[GfxStackPos-1].scale;
}

L9BOOL rts(L9BYTE** a5)
{
	if (GfxStackPos > 0)
	{
		GfxStackPos--;
		*a5 = GfxStack[GfxStackPos].a5;
		scale = GfxStack[GfxStackPos].scale;
		return TRUE;
	}
	return FALSE;
}

L9BOOL getinstruction(L9BYTE** a5)
{
	int d7 = *(*a5)++;
	if ((d7&0xc0) != 0xc0)
	{
		switch ((d7>>6)&3)
		{
		case 0: sdraw(d7); break;
		case 1: smove(d7); break;
		case 2: sgosub(d7,a5); break;
		}
	}
	else if ((d7&0x38) != 0x38)
	{
		switch ((d7>>3)&7)
		{
		case 0: draw(d7,a5); break;
		case 1: _move(d7,a5); break;
		case 2: icolour(d7); break;
		case 3: size(d7); break;
		case 4: gintfill(d7); break;
		case 5: gosub(d7,a5); break;
		case 6: reflect(d7); break;
		}
	}
	else
	{
		switch (d7&7)
		{
		case 0: notimp(); break;
		case 1: gintchgcol(a5); break;
		case 2: notimp(); break;
		case 3: amove(a5); break;
		case 4: opt(a5); break;
		case 5: restorescale(); break;
		case 6: notimp(); break;
		case 7: return rts(a5);
		}
	}
	return TRUE;
}

void absrunsub(int d0)
{
	L9BYTE* a5;
	if (!findsub(d0,&a5))
		return;
	while (getinstruction(&a5));
}

void show_picture(int pic)
{
	if (picturedata)
	{
		/* Some games don't call the screen() opcode before drawing
		   graphics, so here graphics are enabled if necessary. */
		if ((screencalled == 0) && (l9textmode == 0))
		{
			l9textmode = 1;
			os_graphics(1);
		}

#ifdef L9DEBUG
		printf("picture %d",pic);
#endif

		os_cleargraphics();
/* gintinit */
		gintcolour = 3;
		option = 0x80;
		reflectflag = 0;
		drawx = 0x1400;
		drawy = 0x1400;
/* sizereset */
		scale = 0x80;

		GfxStackPos=0;
		absrunsub(0);
		if (!findsub(pic,&gfxa5))
			gfxa5 = NULL;
	}
}

void picture(void)
{
	show_picture(*getvar());
}

void GetPictureSize(int* width, int* height)
{
	if (L9GameType == L9_V4)
	{
		if (width != NULL)
			*width = 0;
		if (height != NULL)
			*height = 0;
	}
	else if (scalegfx)
	{
		if (width != NULL)
			*width = (L9GameType == L9_V2) ? 160 : 320;
		if (height != NULL)
			*height = 96;
	}
	else
	{
		if (width != NULL)
			*width = 320<<6;
		if (height != NULL)
			*height = 96<<3;
	}
}

L9BOOL RunGraphics(void)
{
	if (gfxa5)
	{
		if (!getinstruction(&gfxa5))
			gfxa5 = NULL;
		return TRUE;
	}
	return FALSE;
}

void SetScaleGraphics(L9BOOL scale)
{
	scalegfx = scale;
}

void initgetobj(void)
{
	int i;
	numobjectfound=0;
	object=0;
	for (i=0;i<32;i++) gnoscratch[i]=0;
}

void getnextobject(void)
{
	int d2,d3,d4;
	L9UINT16 *hisearchposvar,*searchposvar;

#ifdef L9DEBUG
	printf("getnextobject");
#endif

	d2=*getvar();
	hisearchposvar=getvar();
	searchposvar=getvar();
	d3=*hisearchposvar;
	d4=*searchposvar;

/* gnoabs */
	do
	{
		if ((d3 | d4)==0)
		{
			/* initgetobjsp */
			gnosp=128;
			searchdepth=0;
			initgetobj();
			break;
		}

		if (numobjectfound==0) inithisearchpos=d3;

	/* gnonext */
		do
		{
			if (d4==list2ptr[++object])
			{
				/* gnomaybefound */
				int d6=list3ptr[object]&0x1f;
				if (d6!=d3)
				{
					if (d6==0 || d3==0) continue;
					if (d3!=0x1f)
					{
						gnoscratch[d6]=d6;
						continue;
					}
					d3=d6;
				}
				/* gnofound */
				numobjectfound++;
				gnostack[--gnosp]=object;
				gnostack[--gnosp]=0x1f;

				*hisearchposvar=d3;
				*searchposvar=d4;
				*getvar()=object;
				*getvar()=numobjectfound;
				*getvar()=searchdepth;
				return;
			}
		} while (object<=d2);

		if (inithisearchpos==0x1f)
		{
			gnoscratch[d3]=0;
			d3=0;

		/* gnoloop */
			do
			{
				if (gnoscratch[d3])
				{
					gnostack[--gnosp]=d4;
					gnostack[--gnosp]=d3;
				}
			} while (++d3<0x1f);
		}
	/* gnonewlevel */
		if (gnosp!=128)
		{
			d3=gnostack[gnosp++];
			d4=gnostack[gnosp++];
		}
		else d3=d4=0;

		numobjectfound=0;
		if (d3==0x1f) searchdepth++;

		initgetobj();
	} while (d4);

/* gnofinish */
/* gnoreturnargs */
	*hisearchposvar=0;
	*searchposvar=0;
	*getvar()=object=0;
	*getvar()=numobjectfound;
	*getvar()=searchdepth;
}

void ifeqct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0==d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]=%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0==d1 ? "Yes":"No");
#endif
}

void ifnect(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0!=d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]!=%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0!=d1 ? "Yes":"No");
#endif
}

void ifltct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0<d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]<%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0<d1 ? "Yes":"No");
#endif
}

void ifgtct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
	if (d0>d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]>%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0>d1 ? "Yes":"No");
#endif
}

void printinput(void)
{
	L9BYTE* ptr=(L9BYTE*) obuff;
	char c;
	while ((c=*ptr++)!=' ') printchar(c);

#ifdef L9DEBUG
	printf("printinput");
#endif
}

void listhandler(void)
{
	L9BYTE *a4,*MinAccess,*MaxAccess;
	L9UINT16 val;
	L9UINT16 *var;
#ifdef CODEFOLLOW
	int offset; 
#endif

	if ((code&0x1f)>0xa)
	{
		error("\rillegal list access %d\r",code&0x1f);
		Running=FALSE;
		return;
	}
	a4=L9Pointers[1+code&0x1f];

	if (a4>=workspace.listarea && a4<workspace.listarea+LISTAREASIZE)
	{
		MinAccess=workspace.listarea;
		MaxAccess=workspace.listarea+LISTAREASIZE;
	}
	else
	{
		MinAccess=startdata;
		MaxAccess=startdata+FileSize;
	}

	if (code>=0xe0)
	{
		/* listvv */
#ifndef CODEFOLLOW
		a4+=*getvar();
		val=*getvar();
#else
		offset=*getvar();
		a4+=offset;
		var=getvar();
		val=*var;
		fprintf(f," list %d [%d]=Var[%d] (=%d)",code&0x1f,offset,var-workspace.vartable,val);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *a4=(L9BYTE) val;
		#ifdef L9DEBUG
		else printf("Out of range list access");
		#endif
	}
	else if (code>=0xc0)
	{
		/* listv1c */
#ifndef CODEFOLLOW
		a4+=*codeptr++;
		var=getvar();
#else
		offset=*codeptr++;
		a4+=offset;
		var=getvar();
		fprintf(f," Var[%d]= list %d [%d])",var-workspace.vartable,code&0x1f,offset);
		if (a4>=MinAccess && a4<MaxAccess) fprintf(f," (=%d)",*a4);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *var=*a4;
		else
		{
			*var=0;
			#ifdef L9DEBUG
			printf("Out of range list access");
			#endif
		}
	}
	else if (code>=0xa0)
	{
		/* listv1v */
#ifndef CODEFOLLOW
		a4+=*getvar();
		var=getvar();
#else
		offset=*getvar();
		a4+=offset;
		var=getvar();

		fprintf(f," Var[%d] =list %d [%d]",var-workspace.vartable,code&0x1f,offset);
		if (a4>=MinAccess && a4<MaxAccess) fprintf(f," (=%d)",*a4);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *var=*a4;
		else
		{
			*var=0;
			#ifdef L9DEBUG
			printf("Out of range list access");
			#endif
		}
	}
	else
	{
#ifndef CODEFOLLOW
		a4+=*codeptr++;
		val=*getvar();
#else
		offset=*codeptr++;
		a4+=offset;
		var=getvar();
		val=*var;
		fprintf(f," list %d [%d]=Var[%d] (=%d)",code&0x1f,offset,var-workspace.vartable,val);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *a4=(L9BYTE) val;
		#ifdef L9DEBUG
		else printf("Out of range list access");
		#endif
	}
}

void executeinstruction(void)
{
#ifdef CODEFOLLOW
	f=fopen(CODEFOLLOWFILE,"a");
	fprintf(f,"%ld (s:%d) %x",(L9UINT32) (codeptr-acodeptr)-1,workspace.stackptr,code);
	if (!(code&0x80))
		fprintf(f," = %s",codes[code&0x1f]);
#endif

	if (code & 0x80) listhandler();
	else switch (code & 0x1f)
	{
		case 0:		Goto();break;
		case 1: 	intgosub();break;
		case 2:		intreturn();break;
		case 3:		printnumber();break;
		case 4:		messagev();break;
		case 5:		messagec();break;
		case 6:		function();break;
		case 7:		input();break;
		case 8:		varcon();break;
		case 9:		varvar();break;
		case 10:	_add();break;
		case 11:	_sub();break;
		case 12:	ilins(code & 0x1f);break;
		case 13:	ilins(code & 0x1f);break;
		case 14:	jump();break;
		case 15:	Exit();break;
		case 16:	ifeqvt();break;
		case 17:	ifnevt();break;
		case 18:	ifltvt();break;
		case 19:	ifgtvt();break;
		case 20:	_screen();break;
		case 21:	cleartg();break;
		case 22:	picture();break;
		case 23:	getnextobject();break;
		case 24:	ifeqct();break;
		case 25:	ifnect();break;
		case 26:	ifltct();break;
		case 27:	ifgtct();break;
		case 28:	printinput();break;
		case 29:	ilins(code & 0x1f);break;
		case 30:	ilins(code & 0x1f);break;
		case 31:	ilins(code & 0x1f);break;
	}
#ifdef CODEFOLLOW
	fprintf(f,"\n");
	fclose(f);
#endif
}

L9BOOL LoadGame2(char *filename,char *picname)
{
#ifdef CODEFOLLOW
	f=fopen(CODEFOLLOWFILE,"w");
	fprintf(f,"Code follow file...\n");
	fclose(f);
#endif

	/* may be already running a game, maybe in input routine */
	Running=FALSE;
	ibuffptr=NULL;

/* intstart */
	if (!intinitialise(filename,picname)) return FALSE;
/*	if (!checksumgamedata()) return FALSE; */

	codeptr=acodeptr;
	randomseed=(L9UINT16)time(NULL);
	strcpy(LastGame,filename);
	return Running=TRUE;
}

L9BOOL LoadGame(char *filename,char *picname)
{
	L9BOOL ret=LoadGame2(filename,picname);
	showtitle=1;
	clearworkspace();
	workspace.stackptr=0;
	/* need to clear listarea as well */
	memset((L9BYTE*) workspace.listarea,0,LISTAREASIZE);
	return ret;
}

/* can be called from input to cause fall through for exit */
void StopGame(void)
{
	Running=FALSE;
}

L9BOOL RunGame(void)
{
	code=*codeptr++;
/*	printf("%d",code); */
	executeinstruction();
	return Running;
}

void RestoreGame(char* filename)
{
	int Bytes;
	GameState temp;
	FILE* f = NULL;

	if ((f = fopen(filename, "rb")) != NULL)
	{
		Bytes = fread(&temp, 1, sizeof(GameState), f);
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* full restore */
			memmove(&workspace,&temp,sizeof(GameState));
			codeptr=acodeptr+workspace.codeptr;
		}
		else
			printstring("\rSorry, unrecognised format. Unable to restore\r");
	}
	else
		printstring("\rUnable to restore game.\r");
}

#ifdef BITMAP_DECODER

L9BOOL bitmap_exists(char* file)
{
	FILE* f = fopen(file,"rb");
	if (f != NULL)
	{
		fclose(f);
		return TRUE;
	}
	return FALSE;
}

L9BYTE* bitmap_load(char* file, L9UINT32* size)
{
	L9BYTE* data = NULL;
	FILE* f = fopen(file,"rb");
	if (f)
	{
		*size = filelength(f);
		L9Allocate(&data,*size);
		if (fread(data,1,*size,f) != *size)
		{
			free(data);
			data = NULL;
		}
		fclose(f);
	}
	return data;
}

Bitmap* bitmap_alloc(int x, int y)
{
	Bitmap* bitmap = NULL;
	L9Allocate((L9BYTE**)&bitmap,sizeof(Bitmap)+(x*y));

	bitmap->width = x;
	bitmap->height = y;
	bitmap->bitmap = ((L9BYTE*)bitmap)+sizeof(Bitmap);
	bitmap->npalette = 0;
	return bitmap;
}

/*
	A PC or ST palette colour is a sixteen bit value in which the low three nybbles
	hold the rgb colour values. The lowest nybble holds the blue value, the 
	second nybble the blue value and the third nybble the red value. (The high
	nybble is ignored). Within each nybble, only the low three bits are used
	IE the value can only be 0-7 not the full possible 0-15 and so the MSbit in
	each nybble is always 0.
*/
Colour bitmap_pcst_colour(int big, int small)
{
	Colour col;
	L9UINT32 r = big & 0xF;
	L9UINT32 g = (small >> 4) & 0xF;
	L9UINT32 b = small & 0xF;

	r *= 0x49; r >>= 1;
	g *= 0x49; g >>= 1;
	b *= 0x49; b >>= 1;

	col.red = (L9BYTE)(r&0xFF);
	col.green = (L9BYTE)(g&0xFF);
	col.blue = (L9BYTE)(b&0xFF);
	return col;
}

/*
	ST Bitmaps

	On the ST different graphics file formats were used for the early V4
	games (Knight Orc, Gnome Ranger) and the later V4 games (Lancelot,
	Ingrid's Back, Time & Magik and Scapeghost).
*/

/*
	Extracts the number of pixels requested from an eight-byte data block (4 bit-
	planes) passed to it.

	Note:	On entry each one of four pointers is set to point to the start of each
			bit-plane in the block. The function then indexes through each byte in
			each bit plane. and uses shift and mask operations to extract each four
			bit pixel into an L9PIXEL.

			The bit belonging to the pixel in the current byte of the current bit-
			plane is moved to its position in an eight-bit pixel. The byte is then
			masked by a value to select only that bit and added to the final pixel
			value.
*/
L9UINT32 bitmap_st1_decode_pixels(L9BYTE* bitmap, L9BYTE* data, L9UINT32 count, L9UINT32 pixels)
{
	L9UINT32 bitplane_length = count / 4; /* length of each bitplane */
	L9BYTE *bitplane0 = data; /* address of bit0 bitplane */
	L9BYTE *bitplane1 = data + (bitplane_length); /* address of bit1 bitplane */
	L9BYTE *bitplane2 = data + (bitplane_length * 2); /* address of bit2 bitplane */
	L9BYTE *bitplane3 = data + (bitplane_length * 3); /* address of bit3 bitplane */
	L9UINT32 bitplane_index, pixel_index = 0; /* index variables */

	for (bitplane_index = 0; bitplane_index < bitplane_length; bitplane_index++)
	{
		/* build the eight pixels from the current bitplane bytes, high bit to low */

		/* bit7 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] >> 4) & 0x08) 
			+ ((bitplane2[bitplane_index] >> 5) & 0x04)
			+ ((bitplane1[bitplane_index] >> 6) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 7) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit6 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] >> 3) & 0x08) 
			+ ((bitplane2[bitplane_index] >> 4) & 0x04)
			+ ((bitplane1[bitplane_index] >> 5) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 6) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit5 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] >> 2) & 0x08) 
			+ ((bitplane2[bitplane_index] >> 3) & 0x04)
			+ ((bitplane1[bitplane_index] >> 4) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 5) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit4 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] >> 1) & 0x08) 
			+ ((bitplane2[bitplane_index] >> 2) & 0x04)
			+ ((bitplane1[bitplane_index] >> 3) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 4) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit3 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index]) & 0x08) 
			+ ((bitplane2[bitplane_index] >> 1) & 0x04)
			+ ((bitplane1[bitplane_index] >> 2) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 3) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit2 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] << 1) & 0x08) 
			+ ((bitplane2[bitplane_index]) & 0x04)
			+ ((bitplane1[bitplane_index] >> 1) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 2) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit1 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] << 2) & 0x08) 
			+ ((bitplane2[bitplane_index] << 1) & 0x04)
			+ ((bitplane1[bitplane_index]) & 0x02) 
			+ ((bitplane0[bitplane_index] >> 1) & 0x01);
		if (pixels == ++pixel_index)
			break;

		/* bit0 byte */
		bitmap[pixel_index] = ((bitplane3[bitplane_index] << 3) & 0x08) 
			+ ((bitplane2[bitplane_index] << 2) & 0x04)
			+ ((bitplane1[bitplane_index] << 1) & 0x02) 
			+ ((bitplane0[bitplane_index]) & 0x01);
		if (pixels == ++pixel_index)
			break;
	}

	return pixel_index;
}

/*
	The ST image file has the following format. It consists of a 44 byte header
	followed by the image data. 

	The header has the following format:
	Bytes 0-31:		sixteen entry ST palette
	Bytes 32-33:	padding
	Bytes 34-35:	big-endian word holding number of bitplanes needed to make
				up a row of pixels*
	Bytes 36-37:	padding
	Bytes 38-39:	big-endian word holding number of rows in the image*
	Bytes 40-41:	padding**
	Bytes 42-43:	mask for pixels to show in last 16 pixel block. Again, this 
				is big endian

	[*]		these are probably big-endian unsigned longs but I have designated
			the upper two bytes as padding because (a) Level 9 does not need
			them as longs and (b) using unsigned shorts reduces byte sex induced
			byte order juggling.
	[**]	not certain what this is for but I suspect that, like bytes 42-43
			it is a mask to indicate which pixels to show, in this case in the 
			first 16 pixel block 

	The image data is essentially a memory dump of the video RAM representing
	the image in lo-res mode. In lo-res mode each row is 320 pixels wide
	and each pixel can be any one of sixteen colours - needs 4 bits to store. 

	In the ST video memory (in lo-res mode which we are dealing with here)
	is organised as follows. The lowest point in memory in the frame buffer 
	represents the top-left of the screen, the highest the bottom-right. 
	Each row of pixels is stored in sequence. 

	Within each pixel row the pixels are stored as follows. Each row is
	divided into groups of 16 pixels. Each sixteen pixel group is stored
	in 8 bytes, logically four groups of two. Each two byte pair 
	is a bit-plane for that sixteen pixel group - that is it stores the
	same bit of each pixel in that group. The first two bytes store the 
	lowest bit, the second pair the second bit &c. 

	The word at bytes 34-35 of the header stores the number of bitplanes
	that make up each pixel row in the image. Multplying this number by
	four gives the number of pixels in the row***. For title and frame 
	images that will be 320, for sub-images it will be less. 

	[***]	Not always exactly. For GnomeRanger sub-images this value is 60 
			- implying there are 240 pixels per row. In fact there are only 
			225 pixels in each row. To identify this situation look at the 
			big-endian word in bytes 42-43 of the header. This is a mask 
			telling you the pixels to use. Each bit represents one pixel in 
			the block, with the MSBit representing the first pixel and the 
			LSbit the last.

			In this situation, the file does contain the entire sixteen
			pixel block (it has to with the bitplane arrangement) but
			the pixels which are not part of the image are just noise. When
			decoding the image, the L9BITMAP produced has the actual pixel 
			dimensions - the surplus pixels are discarded.

			I suspect, though I have not found an instance, that in theory
			the same situation could apply at the start of a pixel row and that
			in this case the big-endian word at bytes 40-41 is the mask.

	Having obtained the pixel dimensions of the image the function uses
	them to allocate memory for the bitmap and then extracts the pixel
	information from the bitmap row by row. For each row eight byte blocks
	are read from the image data and passed to UnpackSTv1Pixels along with
	the number of pixels to extract (usually 16, possibly less for the last
	block in a row.)
*/
L9BOOL bitmap_st1_decode(char* file, int x, int y)
{
	L9BYTE* data = NULL;
	int i, xi, yi, max_x, max_y, last_block;
	int bitplanes_row, bitmaps_row, pixel_count, get_pixels;

	L9UINT32 size;
	data = bitmap_load(file,&size);
	if (data == NULL)
		return FALSE;

	bitplanes_row = data[35]+data[34]*256;
	bitmaps_row = bitplanes_row/4;
	max_x = bitplanes_row*4;
	max_y = data[39]+data[38]*256;
	last_block = data[43]+data[42]*256;

	/* Check if sub-image with rows shorter than max_x */
	if (last_block != 0xFFFF)
	{
		/* use last_block to adjust max_x */
		int i = 0;
		while ((0x0001 & last_block) == 0) /* test for ls bit set */
		{
			last_block >>= 1; /* if not, shift right one bit */
			i++;
		}
		max_x = max_x - i;
	}

	if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
	{
		free(data);
		return FALSE;
	}

	if ((x == 0) && (y == 0))
	{
		if (bitmap)
			free(bitmap);
		bitmap = bitmap_alloc(max_x,max_y);
	}
	if (bitmap == NULL)
	{
		free(data);
		return FALSE;
	}

	if (x+max_x > bitmap->width)
		max_x = bitmap->width-x;
	if (y+max_y > bitmap->height)
		max_y = bitmap->height-y;

	for (yi = 0; yi < max_y; yi++)
	{
		pixel_count = 0;
		for (xi = 0; xi < bitmaps_row; xi++)
		{
			if ((max_x - pixel_count) < 16)
				get_pixels = max_x - pixel_count;
			else
				get_pixels = 16;

			pixel_count += bitmap_st1_decode_pixels(
				bitmap->bitmap+((y+yi)*bitmap->width)+x+(xi*16),
				data+44+(yi*bitplanes_row*2)+(xi*8),8,get_pixels);
		}
	}

	bitmap->npalette = 16;
	for (i = 0; i < 16; i++)
	{
		L9UINT16 colour = data[1+(i*2)]+data[i*2]*256;
		bitmap->palette[i] = bitmap_pcst_colour(data[(i*2)],data[1+(i*2)]);
	}

	free(data);
	return TRUE;
}

void bitmap_st2_name(int num, char* dir, char* out)
{
	/* title picture is #30 */
	if (num == 0)
		num = 30;
	sprintf(out,"%s%d.squ",dir,num);
}

/*
	PC Bitmaps

	On the PC different graphics file formats were used for the early V4
	games (Knight Orc, Gnome Ranger) and the later V4 games (Lancelot,
	Ingrid's Back, Time & Magik and Scapeghost).

	The ST and the PC both use the same image file format for the later 
	V4 games (Lancelot, Ingrid's Back, Time & Magik and Scapeghost.)
*/

void bitmap_pc_name(int num, char* dir, char* out)
{
	/* title picture is #30 */
	if (num == 0)
		num = 30;
	sprintf(out,"%s%d.pic",dir,num);
}

/*
	The EGA standard for the IBM PCs and compatibles defines 64 colors, any
	16 of which can be mapped to the usable palette at any given time. If
	you display these 64 colors in numerical order, 16 at a time, you get a
	hodgepodge of colors in no logical order. The 64 EGA color numbers are
	assigned in a way that the numbers can easily be converted to a relative
	intensity of each of the three phosphor colors R,G,B. If the number is
	converted to six bit binary, the most significant three bits represent
	the 25% level of R,G,B in that order and the least significant three
	bits represent the 75% level of R,G,B in that order. Take EGA color 53
	for example. In binary, 53 is 110101. Since both R bits are on, R = 1.0.
	Of the G bits only the 25% bit is on so G = 0.25. Of the B bits only the
	75% bit is on so B = 0.75.
*/
Colour bitmap_pc1_colour(int i)
{
	Colour col;
	col.red = (((i&4)>>1) | ((i&0x20)>>5)) * 0x55;
	col.green = ((i&2) | ((i&0x10)>>4)) * 0x55;
	col.blue = (((i&1)<<1) | ((i&8)>>3)) * 0x55;
	return col;
}

/*
	The PC (v1) image file has the following format. It consists of a 22
	byte header organised like this: 

	Byte 0:     probably a file type flag
	Byte 1:     the MSB of the file's length as a word
	Bytes 2-3:  little-endian word with picture width in pixels
	Bytes 4-5:  little-endian word with picture height in pixel rows
	Bytes 6-21: the image colour table. One EGA colour in each byte

	The image data is extremely simple. The entire block is packed array
	of 4-bit pixels - IE each byte holds two pixels - the first in the high
	nybble, the second in the low. The pixel value is an index into the 
	image colour table. The pixels are organised with the top left first and 
	bottom left last, each row in turn.
*/
L9BOOL bitmap_pc1_decode(char* file, int x, int y)
{
	L9BYTE* data = NULL;
	int i, xi, yi, max_x, max_y;

	L9UINT32 size;
	data = bitmap_load(file,&size);
	if (data == NULL)
		return FALSE;

	max_x = data[2]+data[3]*256;
	max_y = data[4]+data[5]*256;
	if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
	{
		free(data);
		return FALSE;
	}

	if ((x == 0) && (y == 0))
	{
		if (bitmap)
			free(bitmap);
		bitmap = bitmap_alloc(max_x,max_y);
	}
	if (bitmap == NULL)
	{
		free(data);
		return FALSE;
	}

	if (x+max_x > bitmap->width)
		max_x = bitmap->width-x;
	if (y+max_y > bitmap->height)
		max_y = bitmap->height-y;

	for (yi = 0; yi < max_y; yi++)
	{
		for (xi = 0; xi < max_x; xi++)
		{
			bitmap->bitmap[(bitmap->width*(y+yi))+(x+xi)] =
				(data[23+((yi*max_x)/2)+(xi/2)]>>((1-(xi&1))*4)) & 0x0f;
		}
	}

	bitmap->npalette = 16;
	for (i = 0; i < 16; i++)
		bitmap->palette[i] = bitmap_pc1_colour(data[6+i]);

	free(data);
	return TRUE;
}

/*
	The PC (v2) image file has the following format. It consists of a 44
	byte header followed by the image data. 

	The header has the following format:
	Bytes 0-1: "datalen": 	length of file -1 as a big-endian word*
	Bytes 2-3: "flagbyte1 & flagbyte2": unknown, possibly type identifiers. 
		Usually 0xFF or 0xFE followed by 0x84, 0x72, 0xFF, 0xFE or
		some other (of a fairly small range of possibles) byte.
	Bytes 4-35: "colour_index[]": sixteen entry palette. Basically an ST 
		palette (even if in a PC image file. Each entry is a sixteen
		bit value in which the low three nybbles hold the rgb colour
		values. The lowest nybble holds the blue value, the second
		nybble the blue value and the third nybble the red value. (The
		high nybble is ignored). Within each nybble, only the low
		three bits are used IE the value can only be 0-7 not the full
		possible 0-15 and so the MSbit in each nybble is always 0.**,
	Bytes 36-37: "width": image width in pixels as a big-endian word
	Bytes 38-39: "numrows": image height in pixel rows as a big-endian word
	Byte 40: "seedByte": seed byte to start picture decoding.
	Byte 41: "padByte": unknown. Possibly padding to word align the next 
		element?
	Bytes 42-297: "pixelTable": an array of 0x100 bytes used as a lookup table 
		for pixel values
	Bytes 298-313: "bitStripTable": an array of 0x10 bytes used as a lookup table 
		for the number of bytes to strip from the bit stream for the pixel being
		decoded
	Bytes 314-569:	"indexByteTable": an array of 0x100 bytes used as a lookup 
		table to index into bitStripTable and pixelTable****

	The encoded image data then follows ending in a 0x00 at the file length stored
	in the first two bytes of the file. there is then one extra byte holding a 
	checksum produced by the addition of all the bytes in the file (except the first 
	two and itself)*

	[*] in some PC games the file is padded out beyond this length to the
	nearest 0x80/0x00 boundary with the byte 0x1A. The valid data in the 
	file still finishes where this word says with the checkbyte following it.
	[**] I imagine when a game was running on a PC this standard palette
	was algorithimcally changed to suit the graphics mode being used
	(Hercules, MDA, CGA, EGA, MCGA, VGA &c.) 
	[***]	Note also, in image 1 of PC Time & Magik I think one palette entry
	is bad as what should be white in the image is actually set to
	a very pale yellow. This is corrected with the display of the next 
	sub-picture and I am pretty sure it is note a decoding problem
	here as when run on the PC the same image has the same pale yellow
	cast.
	[****] for detail of how all this works see below

	As this file format is intended for two very different platforms the decoded
	imaged data is in a neutral, intermediate form. Each pixel is extracted as a
	byte with only the low four bits significant. The pixel value is an index into
	the sixteen entry palette.

	The pixel data is compressed, presumably to allow a greater number of images
	to be distributed on the (rather small) default ST & PC floppy disks (in both
	cases about 370 Kbytes.)*****

	Here's how to decode the data. The image data is actually a contiguous bit
	stream with the byte structure on disk having almost no relevance to the
	encoding. We access the bit stream via a two-byte buffer arranged as a word.

	Preparation:

	Initially, move the first byte from the image data into the low byte of
	theBitStreamBuffer and move the second byte of the image data into the
	high byte of theBitStreamBuffer.

	Set a counter (theBufferBitCounter) to 8 which you will use to keep track
	of when it is necesary to refill the buffer.

	Set a L9BYTE variable (theNewPixel) to byte 40 (seedByte) of the header.
	We need to do this because as part of identifying the pixel being
	extracted we need to know the value of the previous pixel extracted. Since
	none exists at this point we must prime this variable with the correct
	value.

	Extraction:

	Set up a loop which you will execute once for each pixel to be extracted
	and within that loop do as follows.

	Copy the low byte of theBitStreamBuffer to an L9BYTE
	(theNewPixelIndexSelector). Examine theNewPixelIndexSelector. If this
	is 0xFF this flags that the index to the new pixel is present as a
	literal in the bit stream; if it is NOT 0xFF then the new pixel index
	value has to be decoded.

	If theNewPixelIndexSelector is NOT 0xFF do as follows:

	Set the variable theNewPixelIndex to the byte in the
	indexByteTable array of the header indexed by
	theNewPixelIndexSelector.

	Set the variable theBufferBitStripCount to the value in the
	bitStripTable array of the header indexed by theNewPixelIndex.

	One-by-one use right bit shift (>>) to remove
	theBufferBitStripCount bits from theBitStreamBuffer. After each
	shift decrement theBufferBitCounter and check whether it has
	reached 0. If it has, get the next byte from the image data and
	insert it in the high byte of theBitStreamBuffer and reset
	theBufferBitCounter to 8. What is happening here is as we remove
	each bit from the bottom of the bit stream buffer we check to see
	if there are any bits left in the high byte of the buffer. As soon
	as we know there are none, we refill it with the next eight bits
	from the image data.

	When this 'bit-stripping' is finished, other than actually identifying
	the new pixel we are nearly done. I will leave that for the moment and
	look at what happens if the low byte of theBitStreamBuffer we put in
	theNewPixelIndexSelector was actually 0xFF:

	In this case, instead of the above routine we begin by removing
	the low eight bits from the theBitStreamBuffer. We use the same
	ono-by-one bit shift right process described above to do this,
	again checking after each shift if it is necesary to refill the
	buffer's high byte.

	When the eight bits have been removed we set theNewPixelIndex to
	the value of the low four bits of theBitStreamBuffer. Having done
	that we again one-by-one strip off those low four bits from the
	theBitStreamBuffer, again checking if we need to refill the buffer
	high byte.

	Irrespective of whether we initially had 0xFF in
	theNewPixelIndexSelector we now have a new value in theNewPixelIndex.
	This value is used as follows to obtain the new pixel value.

	The variable theNewPixel contains either the seedByte or the value of
	the previously extracted pixel. In either case this is a 4-bit value
	in the lower 4 bits. Use the left bit shift operator (or multiply by
	16) to shift those four bits into the high four bits of theNewPixel.

	Add the value in theNewPixelIndex (it is a 4-bit value) to
	theNewPixel. The resulting value is used as an index into the
	pixelTable array of the header to get the actual new pixel value so
	theNewPixel = header.pixelTable[theNewPixel] gets us our new pixel and
	primes theNewPixel for the same process next time around the loop.

	Having got our new pixel it is stored in the next empty space in the
	bitmap and we loop back and start again.

	[*****]	I am not sure how the compression was done - someone with a better
	understanding of this area may be able to work out the method from the above.
	I worked out how to decode it by spending many, many hours tracing through the
	code in a debugger - thanks to the now defunct HiSoft for their DevPac ST and
	Gerin Philippe for NoSTalgia <http://users.skynet.be/sky39147/>.
*/
L9BOOL bitmap_pc2_decode(char* file, int x, int y)
{
	L9BYTE* data = NULL;
	int i, xi, yi, max_x, max_y;

	L9BYTE theNewPixel, theNewPixelIndex;
	L9BYTE theBufferBitCounter, theNewPixelIndexSelector, theBufferBitStripCount;
	L9UINT16 theBitStreamBuffer, theImageDataIndex;
	L9BYTE* theImageFileData;

	L9UINT32 size;
	data = bitmap_load(file,&size);
	if (data == NULL)
		return FALSE;

	max_x = data[37]+data[36]*256;
	max_y = data[39]+data[38]*256;
	if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
	{
		free(data);
		return FALSE;
	}

	if ((x == 0) && (y == 0))
	{
		if (bitmap)
			free(bitmap);
		bitmap = bitmap_alloc(max_x,max_y);
	}
	if (bitmap == NULL)
	{
		free(data);
		return FALSE;
	}

	if (x+max_x > bitmap->width)
		max_x = bitmap->width-x;
	if (y+max_y > bitmap->height)
		max_y = bitmap->height-y;

	/* prime the new pixel variable with the seed byte */
	theNewPixel = data[40];
	/* initialise the index to the image data */
	theImageDataIndex = 0;
	/* prime the bit stream buffer */
	theImageFileData = data+570;
	theBitStreamBuffer = theImageFileData[theImageDataIndex++];
	theBitStreamBuffer = theBitStreamBuffer + 
		(0x100 * theImageFileData[theImageDataIndex++]);
	/* initialise the bit stream buffer bit counter */
	theBufferBitCounter = 8;

	for (yi = 0; yi < max_y; yi++)
	{
		for (xi = 0; xi < max_x; xi++)
		{
			theNewPixelIndexSelector = (theBitStreamBuffer & 0x00FF);
			if (theNewPixelIndexSelector != 0xFF)
			{
				/* get index for new pixel and bit strip count */
				theNewPixelIndex = (data+314)[theNewPixelIndexSelector];
				/* get the bit strip count */
				theBufferBitStripCount = (data+298)[theNewPixelIndex];
				/* strip theBufferBitStripCount bits from theBitStreamBuffer */
				while (theBufferBitStripCount > 0)
				{
					theBitStreamBuffer = theBitStreamBuffer >> 1;
					theBufferBitStripCount--;
					theBufferBitCounter--;
					if (theBufferBitCounter == 0)
					{
						/* need to refill the theBitStreamBuffer high byte */
						theBitStreamBuffer = theBitStreamBuffer + 
							(0x100 * theImageFileData[theImageDataIndex++]);
						/* re-initialise the bit stream buffer bit counter */
						theBufferBitCounter = 8;
					}
				}
			}
			else
			{
				/* strip the 8 bits holding 0xFF from theBitStreamBuffer */
				theBufferBitStripCount = 8;
				while (theBufferBitStripCount > 0)
				{
					theBitStreamBuffer = theBitStreamBuffer >> 1;
					theBufferBitStripCount--;
					theBufferBitCounter--;
					if (theBufferBitCounter == 0)
					{
						/* need to refill the theBitStreamBuffer high byte */
						theBitStreamBuffer = theBitStreamBuffer + 
							(0x100 * theImageFileData[theImageDataIndex++]);
						/* re-initialise the bit stream buffer bit counter */
						theBufferBitCounter = 8;
					}
				}
				/* get the literal pixel index value from the bit stream */
				theNewPixelIndex = (0x000F & theBitStreamBuffer);
				theBufferBitStripCount = 4;
				/* strip 4 bits from theBitStreamBuffer */
				while (theBufferBitStripCount > 0)
				{
					theBitStreamBuffer = theBitStreamBuffer >> 1;
					theBufferBitStripCount--;
					theBufferBitCounter--;
					if (theBufferBitCounter == 0)
					{
						/* need to refill the theBitStreamBuffer high byte */
						theBitStreamBuffer = theBitStreamBuffer + 
							(0x100 * theImageFileData[theImageDataIndex++]);
						/* re-initialise the bit stream buffer bit counter */
						theBufferBitCounter = 8;
					}
				}
			}

			/* shift the previous pixel into the high four bits of theNewPixel */
			theNewPixel = (0xF0 & (theNewPixel << 4));
			/* add the index to the new pixel to theNewPixel */
			theNewPixel = theNewPixel + theNewPixelIndex;
			/* extract the nex pixel from the table */
			theNewPixel = (data+42)[theNewPixel];
			/* store new pixel in the bitmap */
			bitmap->bitmap[(bitmap->width*(y+yi))+(x+xi)] = theNewPixel;
		}
	}

	bitmap->npalette = 16;
	for (i = 0; i < 16; i++)
		bitmap->palette[i] = bitmap_pcst_colour(data[4+(i*2)],data[5+(i*2)]);

	free(data);
	return TRUE;
}

BitmapType bitmap_pc_type(char* file)
{
	BitmapType type = PC2_BITMAPS;

	FILE* f = fopen(file,"rb");
	if (f != NULL)
	{
		L9BYTE data[6];
		int x, y;

		fread(data,1,sizeof data,f);
		fclose(f);

		x = data[2]+data[3]*256;
		y = data[4]+data[5]*256;

		if ((x == 0x0140) && (y == 0x0087))
			type = PC1_BITMAPS;
		if ((x == 0x00E0) && (y == 0x0074))
			type = PC1_BITMAPS;
		if ((x == 0x0140) && (y == 0x0087))
			type = PC1_BITMAPS;
		if ((x == 0x00E1) && (y == 0x0076))
			type = PC1_BITMAPS;
	}

	return type;
}

/*
	Amiga Bitmaps
*/

void bitmap_noext_name(int num, char* dir, char* out)
{
	if (num == 0)
	{
		FILE* f;

		sprintf(out,"%stitle",dir);
		f = fopen(out,"rb");
		if (f != NULL)
		{
			fclose(f);
			return;
		}
		else
			num = 30;
	}

	sprintf(out,"%s%d",dir,num);
}

int bitmap_amiga_intensity(int col)
{
	return (int)(pow((double)col/15,1.0/0.8) * 0xff);
}

/*
	Amiga palette colours are word length structures with the red, green and blue
	values stored in the second, third and lowest nybles respectively. The high
	nybble is always zero.
*/
Colour bitmap_amiga_colour(int i1, int i2)
{
	Colour col;
	col.red = bitmap_amiga_intensity(i1&0xf);
	col.green = bitmap_amiga_intensity(i2>>4);
	col.blue = bitmap_amiga_intensity(i2&0xf);
	return col;
}

/*
	The Amiga image file has the following format. It consists of a 44 byte
	header followed by the image data. 

	The header has the following format:
	Bytes 0-63:  thirty-two entry Amiga palette
	Bytes 64-65: padding
	Bytes 66-67: big-endian word holding picture width in pixels*
	Bytes 68-69: padding
	Bytes 70-71: big-endian word holding number of pixel rows in the image*

	[*] these are probably big-endian unsigned longs but I have designated
	the upper two bytes as padding because (a) Level 9 does not need
	them as longs and (b) using unsigned shorts reduces byte sex induced
	byte order juggling.

	The images are designed for an Amiga low-res mode screen - that is they 
	assume a 320*256 (or 320 * 200 if NSTC display) screen with a palette of 
	32 colours from the possible 4096.

	The image data is organised the same way that Amiga video memory is. The 
	entire data block is divided into five equal length bit planes with the 
	first bit plane holding the low bit of each 5-bit pixel, the second bitplane
	the second bit of the pixel and so on up to the fifth bit plane holding the 
	high bit of the f5-bit pixel.
*/
L9BOOL bitmap_amiga_decode(char* file, int x, int y)
{
	L9BYTE* data = NULL;
	int i, xi, yi, max_x, max_y, p, b;

	L9UINT32 size;
	data = bitmap_load(file,&size);
	if (data == NULL)
		return FALSE;

	max_x = (((((data[64]<<8)|data[65])<<8)|data[66])<<8)|data[67];
	max_y = (((((data[68]<<8)|data[69])<<8)|data[70])<<8)|data[71];
	if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
	{
		free(data);
		return FALSE;
	}

	if ((x == 0) && (y == 0))
	{
		if (bitmap)
			free(bitmap);
		bitmap = bitmap_alloc(max_x,max_y);
	}
	if (bitmap == NULL)
	{
		free(data);
		return FALSE;
	}

	if (x+max_x > bitmap->width)
		max_x = bitmap->width-x;
	if (y+max_y > bitmap->height)
		max_y = bitmap->height-y;

	for (yi = 0; yi < max_y; yi++)
	{
		for (xi = 0; xi < max_x; xi++)
		{
			p = 0;
			for (b = 0; b < 5; b++)
				p |= ((data[72+(max_x/8)*(max_y*b+yi)+xi/8]>>(7-(xi%8)))&1)<<b;
			bitmap->bitmap[(bitmap->width*(y+yi))+(x+xi)] = p;
		}
	}

	bitmap->npalette = 32;
	for (i = 0; i < 32; i++)
		bitmap->palette[i] = bitmap_amiga_colour(data[i*2],data[i*2+1]);

	free(data);
	return TRUE;
}

BitmapType bitmap_noext_type(char* file)
{
	FILE* f = fopen(file,"rb");
	if (f != NULL)
	{
		L9BYTE data[72];
		int x, y;

		fread(data,1,sizeof data,f);
		fclose(f);

		x = data[67]+data[66]*256;
		y = data[71]+data[70]*256;

		if ((x == 0x0140) && (y == 0x0088))
			return AMIGA_BITMAPS;
		if ((x == 0x0140) && (y == 0x0087))
			return AMIGA_BITMAPS;
		if ((x == 0x00E0) && (y == 0x0075))
			return AMIGA_BITMAPS;
		if ((x == 0x00E4) && (y == 0x0075))
			return AMIGA_BITMAPS;
		if ((x == 0x00E0) && (y == 0x0076))
			return AMIGA_BITMAPS;
		if ((x == 0x00DB) && (y == 0x0076))
			return AMIGA_BITMAPS;

		x = data[3]+data[2]*256;
		y = data[7]+data[6]*256;

		if ((x == 0x0200) && (y == 0x00D8))
			return MAC_BITMAPS;
		if ((x == 0x0168) && (y == 0x00BA))
			return MAC_BITMAPS;
		if ((x == 0x0168) && (y == 0x00BC))
			return MAC_BITMAPS;

		x = data[35]+data[34]*256;
		y = data[39]+data[38]*256;

		if ((x == 0x0050) && (y == 0x0087))
			return ST1_BITMAPS;
		if ((x == 0x0038) && (y == 0x0074))
			return ST1_BITMAPS;
	}

	return NO_BITMAPS;
}

/*
	Macintosh Bitmaps
*/

/*
	The Mac image file format is very simple. The header is ten bytes
	with the width of the image in pixels in the first long and the 
	height (in pixel rows) in the second long - both are big-endian. 
	(In both cases I treat these as unsigned shorts to minimise byte 
	twiddling when working around byte sex issues). There follow two
	unidentified bytes - possibly image type identifiers or maybe 
	valid pixel masks for the beginning and end of pixel rows in
	sub-images.

	The image data is extremely simple. The entire block is a packed array
	of 1-bit pixels - I.E. each byte holds eight pixels - with 1 representing
	white and 0 representing black. The pixels are organised with the top
	left first and bottom left last, each row in turn.

	The image sizes are 512 * 216 pixels for main images and 360 * 186 pixels
	for sub-images.
*/
L9BOOL bitmap_mac_decode(char* file, int x, int y)
{
	L9BYTE* data = NULL;
	int xi, yi, max_x, max_y;

	L9UINT32 size;
	data = bitmap_load(file,&size);
	if (data == NULL)
		return FALSE;

	max_x = data[3]+data[2]*256;
	max_y = data[7]+data[6]*256;
	if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
	{
		free(data);
		return FALSE;
	}

	if (x > 0)	/* Mac bug, apparently */
		x = 78;

	if ((x == 0) && (y == 0))
	{
		if (bitmap)
			free(bitmap);
		bitmap = bitmap_alloc(max_x,max_y);
	}
	if (bitmap == NULL)
	{
		free(data);
		return FALSE;
	}

	if (x+max_x > bitmap->width)
		max_x = bitmap->width-x;
	if (y+max_y > bitmap->height)
		max_y = bitmap->height-y;

	for (yi = 0; yi < max_y; yi++)
	{
		for (xi = 0; xi < max_x; xi++)
		{
			bitmap->bitmap[(bitmap->width*(y+yi))+(x+xi)] = 
				(data[10+(max_x/8)*yi+xi/8]>>(7-(xi%8)))&1;
		}
	}

	bitmap->npalette = 2;
	bitmap->palette[0].red = 0;
	bitmap->palette[0].green = 0;
	bitmap->palette[0].blue = 0;
	bitmap->palette[1].red = 0xff;
	bitmap->palette[1].green = 0xff;
	bitmap->palette[1].blue = 0xff;

	free(data);
	return TRUE;
}

/*
	C64 Bitmaps, also related formats (BBC B, Amstrad CPC and Spectrum +3)
*/

/* Commodore 64 palette from Vice */
const Colour bitmap_c64_colours[] = {
	{0x00, 0x00, 0x00 },
	{0xff, 0xff, 0xff },
	{0x89, 0x40, 0x36 },
	{0x7a, 0xbf, 0xc7 },
	{0x8a, 0x46, 0xae },
	{0x68, 0xa9, 0x41 },
	{0x3e, 0x31, 0xa2 },
	{0xd0, 0xdc, 0x71 },
	{0x90, 0x5f, 0x25 },
	{0x5c, 0x47, 0x00 },
	{0xbb, 0x77, 0x6d },
	{0x55, 0x55, 0x55 },
	{0x80, 0x80, 0x80 },
	{0xac, 0xea, 0x88 },
	{0x7c, 0x70, 0xda },
	{0xab, 0xab, 0xab }};

void bitmap_c64_name(int num, char* dir, char* out)
{
	if (num == 0)
	{
		FILE* f;

		sprintf(out,"%stitle mpic",dir);
		f = fopen(out,"rb");
		if (f != NULL)
		{
			fclose(f);
			return;
		}

		sprintf(out,"%stitle",dir);
	}
	else
		sprintf(out,"%spic%d",dir,num);
}

void bitmap_bbc_name(int num, char* dir, char* out)
{
	if (num == 0)
		sprintf(out,"%sP.Title",dir);
	else
		sprintf(out,"%sP.Pic%d",dir,num);
}

void bitmap_cpc_name(int num, char* dir, char* out)
{
	if (num == 0)
		sprintf(out,"%stitle.pic",dir);
	else if (num == 1)
		sprintf(out,"%s1.pic",dir);
	else
		sprintf(out,"%sallpics.pic",dir);
}

/*
	The C64 graphics file format is (loosely) based on the layout of
	C64 graphics memory. There are in fact two formats (i) the
	standard game images and (ii) title pictures. For both formats
	the file begins within the 2-byte pair 0x00 and 0x20.

	The images are "multi-color bitmap mode" images which means they
	have rows of 160 double width pixels and can be up to 200 rows
	long. (The title images are 200 lines long, the game images are
	136 lines long.) Unlike Amiga, Mac, ST and PC graphics there are
	no "main" and "sub" images. All game graphics have the same
	dimensions and each completely replaces its predecessor.


	The graphics files used by the BBC B platform are virtually identical
	to C64 graphics files. I assume that (as with the CPC and
	Spectrum+3) this choice was made because the BBC mode 2 screen,
	although more capable than the c64, was nearly the same size
	(160*256) and displayed roughly the same number of colours. In
	addition (a) the artwork already existed so no extra expense would
	be incurred and (b) by accepting the C64's limitation of only four
	colours in each 4*8 pixel block (but still with sixteen colours on
	screen) they got a compressed file format allowing more pictures
	on each disk.

	The file organisation is very close to the C64. The naming system
	can be the same eg "PIC12", but another form is also used :
	"P.Pic12". Unlike the C64 the BBC has well defined title images,
	called "TITLE" or P.Title. All pictures are in separate files.

	The only difference seems to be:

	*	There is either *no* header before the image data or a simple
	10 byte header which I think *may* be a file system header
	left in place by the extractor system.

	*	There is an extra 32 bytes following the data at the end of
	each file. I am almost certain this is palette information for
	the BBC - probably a C64-BBC colour conversion table because
	although the Beeb has 16 colours in mode 2, 8 of those are
	flashing alternates of the other eight.

	
	The graphics files used on the Amstrad CPC and Spectrum +3 are also
	virtually identical to C64 graphics files. This choice was presumably
	made because although the CPC screen was more capable than the c64 it
	was (in low resolution) the same size (160*200) and presumably
	algorothmic conversion conversion of the colours was trivial for
	the interpreter. In addition (a) the artwork already existed so no
	extra expense would be incurred and (b) by accepting the C64's
	limitation of only four colours in each 4*8 pixel block (but still
	with sixteen colours on screen) they got a compressed file format
	allowing more pictures on each disk.

	The file organisation is rather different though. Only picture 
	one and the title picture are separate files. All the other 
	pictures (2-29) are stored in one large file "allpics.pic".

	On these platforms the picture 1 file and title picture file have
	an AMSDOS header (a 128 byte block of metadata) which contains a
	checksum of the first 66 bytes of the header in a little-endian
	word at bytes 67 & 68. On the original C64 platform there was a
	simple two byte header. Following the header the data is organised
	exactly as in the C64 game and title image files. The
	'allpics.pic" file has no header and consists of 0x139E blocks
	each forming a picture, in the C64 game file format (minus the two
	byte header).
*/
L9BOOL bitmap_c64_decode(char* file, BitmapType type, int num)
{
	L9BYTE* data = NULL;
	int i, xi, yi, max_x, max_y, cx, cy, px, py, p;
	int off, off_scr, off_col, off_bg, col_comp;

	L9UINT32 size;
	data = bitmap_load(file,&size);
	if (data == NULL)
		return FALSE;

	if (type == C64_BITMAPS)
	{
		if (size == 10018) /* C64 title picture */
		{
			max_x = 320;
			max_y = 200;
			off = 2;
			off_scr = 8002;
			off_bg = 9003;
			off_col = 9018;
			col_comp = 0;
		}
		else if (size == 6464) /* C64 picture */
		{
			max_x = 320;
			max_y = 136;
			off = 2;
			off_scr = 5442;
			off_col = 6122;
			off_bg = 6463;
			col_comp = 1;
		}
		else if (size == 10048) /* BBC title picture */
		{
			max_x = 320;
			max_y = 200;
			off = 0;
			off_scr = 8000;
			off_bg = 9001;
			off_col = 9016;
			col_comp = 0;
		}
		else if (size == 6494) /* BBC picture */
		{
			max_x = 320;
			max_y = 136;
			off = 0;
			off_scr = 5440;
			off_col = 6120;
			off_bg = 6461;
			col_comp = 1;
		}
		else
			return FALSE;
	}
	else if (type == BBC_BITMAPS)
	{
		if (size == 10058) /* BBC title picture */
		{
			max_x = 320;
			max_y = 200;
			off = 10;
			off_scr = 8010;
			off_bg = 9011;
			off_col = 9026;
			col_comp = 0;
		}
		else if (size == 6504) /* BBC picture */
		{
			max_x = 320;
			max_y = 136;
			off = 10;
			off_scr = 5450;
			off_col = 6130;
			off_bg = 6471;
			col_comp = 1;
		}
		else
			return FALSE;
	}
	else if (type == CPC_BITMAPS)
	{
		if (num == 0) /* CPC/+3 title picture */
		{
			max_x = 320;
			max_y = 200;
			off = 128;
			off_scr = 8128;
			off_bg = 9128;
			off_col = 9144;
			col_comp = 0;
		}
		else if (num == 1) /* First CPC/+3 picture */
		{
			max_x = 320;
			max_y = 136;
			off = 128;
			off_scr = 5568;
			off_col = 6248;
			off_bg = 6588;
			col_comp = 1;
		}
		else if (num >= 2 && num <= 29) /* Subsequent CPC/+3 pictures */
		{
			max_x = 320;
			max_y = 136;
			off = ((num-2)*6462);
			off_scr = 5440+((num-2)*6462);
			off_col = 6120+((num-2)*6462);
			off_bg = 6460+((num-2)*6462);
			col_comp = 1;
		}
		else
			return FALSE;
	}

	if (bitmap)
		free(bitmap);
	bitmap = bitmap_alloc(max_x,max_y);
	if (bitmap == NULL)
	{
		free(data);
		return FALSE;
	}

	for (yi = 0; yi < max_y; yi++)
	{
		for (xi = 0; xi < max_x/2; xi++)
		{
			cx = xi/4;
			px = xi%4;
			cy = yi/8;
			py = yi%8;

			p = data[off+(cy*40+cx)*8+py];
			p = (p>>((3-px)*2))&3;

			switch (p)
			{
			case 0:
				i = data[off_bg] & 0x0f;
				break;
			case 1:
				i = data[off_scr+cy*40+cx] >> 4;
				break;
			case 2:
				i = data[off_scr+cy*40+cx] & 0x0f;
				break;
			case 3:
				if (col_comp)
					i = (data[off_col+(cy*40+cx)/2]>>((1-(cx%2))*4)) & 0x0f;
				else
					i = data[off_col+(cy*40+cx)] & 0x0f;
				break;
			}

			bitmap->bitmap[(bitmap->width*yi)+(xi*2)] = i;
			bitmap->bitmap[(bitmap->width*yi)+(xi*2)+1] = i;
		}
	}

	bitmap->npalette = 16;
	for (i = 0; i < 16; i++)
		bitmap->palette[i] = bitmap_c64_colours[i];

	free(data);
	return TRUE;
}

BitmapType DetectBitmaps(char* dir)
{
	char file[MAX_PATH];

	bitmap_noext_name(2,dir,file);
	if (bitmap_exists(file))
		return bitmap_noext_type(file);

	bitmap_pc_name(2,dir,file);
	if (bitmap_exists(file))
		return bitmap_pc_type(file);

	bitmap_c64_name(2,dir,file);
	if (bitmap_exists(file))
		return C64_BITMAPS;

	bitmap_bbc_name(2,dir,file);
	if (bitmap_exists(file))
		return BBC_BITMAPS;

	bitmap_cpc_name(2,dir,file);
	if (bitmap_exists(file))
		return CPC_BITMAPS;

	bitmap_st2_name(2,dir,file);
	if (bitmap_exists(file))
		return ST2_BITMAPS;

	return NO_BITMAPS;
}

Bitmap* DecodeBitmap(char* dir, BitmapType type, int num, int x, int y)
{
	char file[MAX_PATH];

	switch (type)
	{
	case PC1_BITMAPS:
		bitmap_pc_name(num,dir,file);
		if (bitmap_pc1_decode(file,x,y))
			return bitmap;
		break;

	case PC2_BITMAPS:
		bitmap_pc_name(num,dir,file);
		if (bitmap_pc2_decode(file,x,y))
			return bitmap;
		break;

	case AMIGA_BITMAPS:
		bitmap_noext_name(num,dir,file);
		if (bitmap_amiga_decode(file,x,y))
			return bitmap;
		break;

	case C64_BITMAPS:
		bitmap_c64_name(num,dir,file);
		if (bitmap_c64_decode(file,type,num))
			return bitmap;
		break;

	case BBC_BITMAPS:
		bitmap_bbc_name(num,dir,file);
		if (bitmap_c64_decode(file,type,num)) /* Nearly identical to C64 */
			return bitmap;
		break;

	case CPC_BITMAPS:
		bitmap_cpc_name(num,dir,file);
		if (bitmap_c64_decode(file,type,num)) /* Nearly identical to C64 */
			return bitmap;
		break;

	case MAC_BITMAPS:
		bitmap_noext_name(num,dir,file);
		if (bitmap_mac_decode(file,x,y))
			return bitmap;
		break;

	case ST1_BITMAPS:
		bitmap_noext_name(num,dir,file);
		if (bitmap_st1_decode(file,x,y))
			return bitmap;
		break;

	case ST2_BITMAPS:
		bitmap_st2_name(num,dir,file);
		if (bitmap_pc2_decode(file,x,y))
			return bitmap;
		break;
	}

	return NULL;
}

#endif /* BITMAP_DECODER */
