#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define size_d64 ((type32)(174848))
#define NL ((type32) -1)

typedef unsigned char type8;
typedef unsigned long type32;
#ifdef __MSDOS__
typedef unsigned char huge * type8ptr;
#include <alloc.h>
#define malloc farmalloc
#define free farfree
#define memcpy _fmemcpy
#else
typedef unsigned char * type8ptr;
#endif

FILE *fpout=0, *fp[3];			/* Globals for easy cleanup */
type8 dir[256], block[256], game1, game2;
type8ptr out, tmpbuf;

void cleanup(char *errormsg, type32 status) {
	fprintf(stderr,errormsg);
	if (fpout) fclose(fpout);
	if (fp[1]) fclose(fp[1]);
	if (fp[2]) fclose(fp[2]);
	if (out) free(out);
	if (tmpbuf) free(tmpbuf);
	exit(status);
}

void write_l(type8ptr ptr, type32 val) {
	ptr[3]=val & 0xff; val>>=8;
	ptr[2]=val & 0xff; val>>=8;
	ptr[1]=val & 0xff; val>>=8;
	ptr[0]=val;
}

void write_w(type8ptr ptr, type32 val) {
	ptr[1]=val & 0xff; val>>=8;
	ptr[0]=val;
}

FILE *get_file(char *name, type8ptr game) {
	FILE *fp;
	type8 buf[512];
	type32 id;

	if (!(fp=fopen(name,"rb"))) cleanup("Couldn't open input file\n",1);
	if (fread(buf,1,512,fp)!=512) cleanup("Wrong size or read error\n",1);

	if	(buf[0]!='M' || buf[1]!='S')
		cleanup("This isn't a disk image of a Magnetic Scrolls game.\n",1);

	id=((type32)buf[491]<<24|(type32)buf[492]<<16|(type32)buf[493]<<8|(type32)buf[494]);
	if (id==0x5041574e) {						/* PAWN */
		printf("'The Pawn' detected\n");
		*game=0;
	} else if (id==0x53574147) {				/* SWAG */
		printf("'Guild of Thieves' detected\n");
		*game=1;
	} else if (id==0x41525345) {				/* ARSE */
		printf("'Jinxter' detected\n");
		*game=2;
	} else if (id==0x474c5547) {				/* GLUG */
		printf("'Fish' detected\n");
		*game=3;
	} else if (id==0x434f4b45) {				/* COKE */
		printf("'Corruption' detected\n");
		*game=4;
	} else if (id==0x474f4453) {				/* GODS */
		printf("'Myth' detected\n");
		*game=5;
	} else cleanup("Nothing I can identify, yet.\n",1);
	return fp;
}

void ungarble(type8ptr block, type32 init) {
	type8 d;
	static signed long number;
	signed long i,j;

	if (init) {
		number=0;
		return;
	}
	d=(number & 0x07) ^ 0xff;
	if (d<0xff) {
		i=d; j=d+1;
		while (j<0x100) block[j++]^=block[i];
	}
	i=0xff; j=d-1;
	while (j>=0) block[j--]^=block[i--];
	for (i=0;i<128;i++) {
		d=block[i];
		block[i]=block[255-i];
		block[255-i]=d;
	}
	number++;
}

type8ptr getblock(type32 file, type32 sector, type32 garble) {
	type32 track;
	type8 side;

	static type32 pertrack[]={	00,21,21,21,21,21,21,21,21,21,
					21,21,21,21,21,21,21,21,00,19,
					19,19,19,19,19,18,18,18,18,18,
					18,17,17,17,17,17 };

	side=(type8)dir[4*file+3];
	if (game1!=0 && game1!=5 && side!=0)
		side=3-side;
	if (sector>=(type32)dir[4*file+2]) return 0;

	track=(type32)dir[4*file];
	sector+=(type32)dir[4*file+1];
	while (sector>=pertrack[track]) sector-=pertrack[track++];
	while (--track>0) {
		if (track==18) sector+=19;
		else sector+=pertrack[track];
	}
	fseek(fp[side],sector*256,SEEK_SET);
	if (fread(block,1,256,fp[side])!=256) cleanup("Wrong file size or read error",1);
	if (garble) ungarble(block,0);
	return block;
}

type32 runlength(type8ptr ptr) {
	type32	end,i,j,k;

	end=ptr[0]<<8 | ptr[1];
	for (i=2,j=0;i<end;i++) {
		if (ptr[i]) tmpbuf[j++]=ptr[i];
		else {
			i++;
			for (k=0;k<ptr[i];k++) tmpbuf[j++]=0;
		}
	}
	memcpy(ptr,tmpbuf,j);
	return (j+255) & 0xffff00;
}

type32 dump_file(type32 file, type32 currpos, type32 garble, type32 rle) {
	type8ptr ptr;
	type8 side;
	type32 i, len, outlen=0;

	if (file==NL)
		return 0;
	side=(type8)dir[4*file+3];
	if (game1!=0 && game1!=5 && side!=0)
		side=3-side;
	printf("Dumping file %2d from side %d (0=any)\n", (int) file, (int) side);
	len=(type32)dir[4*file+2];
	if (garble==1) ungarble(0,1);						/* initialize ungarbling */

	for (i=0;i<len;i++) {
		ptr=getblock(file,i,garble);
		if (ptr) memcpy(&out[currpos+outlen],ptr,256);
		outlen+=256;
	}
	if (rle) outlen=runlength(out+currpos);
	return outlen;
}

type32 write_file(type8ptr buffer, type32 sz, FILE *fp) {
#ifdef __MSDOS__
	type32 n=0xf000,i,j=0;

	for (i=0; i<sz; i+=n) {
		if (sz-i < 0xf000)
			n = sz-i;
		j += fwrite(buffer+i,1,n,fp);
	}
	return j;
#else
	return fwrite(buffer,1,sz,fp);
#endif
}

main(int argc, char **argv) {
	type32 m1,m2,s1,s2,dc,sz,i,sum;
	type32 info[6][9]={
		{ 0, 35, 1, 2, 3, NL, 0x0b400, 0x3fb0, 0x09528f1 }, /* The Pawn */
		{ 1, 34, 1, 2, 3, NL, 0x0f100, 0x6cac, 0x0c423d4 },	/* Guild of Thieves */
		{ 2, 33, 1, 2, 3, 34, 0x13100, 0x488c, 0x0efc6bf },	/* Jinxter */
		{ 3, 32, 1, 2, 3, 33, 0x14e00, 0x3f72, 0x0fd6a3d },	/* Fish */
		{ 3, 31, 1, 2, 3, 32, 0x16100, 0x4336, 0x10405a7 },	/* Corruption */
		{ 3,  4, 1, 2, 3,  5, 0x08b00, 0x3940, 0x08f0e4b }	/* Myth */
	};
	/* version, mem1, mem2, str1, str2, dict, decode_offset, undo_pc, checksum */

	assert(sizeof(type8)==1);
	assert(sizeof(type32)==4);

	if (argc!=4) cleanup(
		"Xtract64 v1.0 by Niclas Karlsson\n"
		"\n"
		"This is an utility to extract story files from C64 disk images of Magnetic\n"
		"Scrolls games. The resulting story files can be run with with the Magnetic\n"
		"interpreter by Niclas Karlsson.\n"
		"\n"
		"Usage: Xtract64 disk1.d64 disk2.d64 story.mag\n"
		"\n"
		"(disk2 can be anything when extracting Myth)\n",1);

	if (!(out=(type8ptr)malloc(0x29800))) cleanup("Not enough memory\n",1);
	if (!(tmpbuf=(type8ptr)malloc(0x8000))) cleanup("Not enough memory\n",1);

	fp[1]=get_file(argv[1],&game1);
	if (game1!=5)
		fp[2]=get_file(argv[2],&game2);
	fp[0]=fp[1];
	printf ("\n");

	if (fread(dir,1,256,fp[1])!=256) cleanup("Wrong file size or read error\n",1);

	sz=42;
	sz+=(m1=dump_file(info[game1][1],sz,1,info[game1][0]!=0));	/* main memory 1 */
	sz+=(m2=dump_file(info[game1][2],sz,2,0));						/* main memory 2 */
	sz+=(s1=dump_file(info[game1][3],sz,0,0));						/* strings 1 */
	sz+=(s2=dump_file(info[game1][4],sz,0,0));						/* strings 2 */
	sz+=(dc=dump_file(info[game1][5],sz,1,0));						/* dictionary */
	printf("\n");

	write_l(out+ 0,0x4D615363);			/* magic number MaSc */
	write_l(out+ 4,sz);						/* file size */
	write_l(out+ 8,42);						/* header size */
	write_w(out+12,info[game1][0]);		/* version */
	write_l(out+14,m1+m2);					/* main memory size */
	write_l(out+18,s1);						/* string1 size */
	write_l(out+22,s2);						/* string2 size */
	write_l(out+26,dc);						/* dict size */
	write_l(out+30,info[game1][6]);		/* decoding offset */
	write_l(out+34,m1);						/* undo size */
	write_l(out+38,info[game1][7]);		/* undo-pc */

	if (game1==0 && out[0x17928]==0x3f) {
		printf(
			"The Pawn fix:\n"
			"  C64 disks of this game contained five corrupt bytes that\n"
			"  caused a misprint of Kronos' note. I'm correcting these bytes...\n"
			"\n");
		out[0x17928]=0x68;
		out[0x17929]=0x4b;
		out[0x1792a]=0x8b;
		out[0x1792b]=0xe0;
		out[0x1792c]=0x1e;
	}
	if (game1==0 && out[0x3628]==0xff) {
		printf(
			"The Pawn fix:\n"
			"  Your disk image was probably taken from the Internet and\n"
			"  contains four corrupt bytes. I'm fixing this...\n"
			"\n");
		out[0x3628]=0x00;
		out[0x3728]=0x00;
		out[0x3828]=0x00;
		out[0xf9e0]=0x86;
	}
	if (game1==1 && out[0xc042]==0x29) {
		printf(
			"Guild of Thieves fix:\n"
			"  Your disk image could be taken from the CD96. This version\n"
			"  contains eight modified bytes. I'm restoring original values...\n"
			"\n");
		out[0xc042]=0x4b;
		out[0xc043]=0xfe;
		out[0xda08]=0x3f;
		out[0xda09]=0x06;
		out[0xda0a]=0x4e;
		out[0xda0b]=0x71;
		out[0xdb7c]=0x4b;
		out[0xdb7d]=0xfe;
	}
	if (game1==2 && out[0xd212]==0x50) {
		printf(
			"Jinxter fix:\n"
			"  This game has bug that makes the interpreter crash when\n"
			"  objects are thrown at a certain window. I'm patching one byte...\n"
			"\n");
		out[0xd212]=0x58;
	}

	for (i=0,sum=0;i<sz;i++) sum+=out[i];
	printf(
		"Checksum is %s (actual %lx, should be %lx)\n\n",
		(sum==info[game1][8])?"OK":"bad",
		(long)sum,
		(long)info[game1][8]);

	if (game1!=4) {
		char c;
		printf("Shall I remove the password protection (y/n)? ");
		c = getchar();
		if (c=='y' || c=='Y') {
			if (game1==0) {
				out[0x3de6]=0x4e; out[0x3de7]=0x71; }
			if (game1==1) {
				out[0x6c52]=0x4e; out[0x6c53]=0x71; }
			if (game1==2) {
				out[0x4310]=0x4e; out[0x4311]=0x71; }
			if (game1==3) {
				out[0x3a6c]=0x4e; out[0x3a6d]=0x71; }
			if (game1==5)
				out[0x3080]=0x60;
		}
	} else printf("This game has no password protection.\n");
	printf("\n");

	if (!(fpout=fopen(argv[3],"wb"))) cleanup("Couldn't open output file\n",1);
	if (write_file(out,sz,fpout)!=sz) cleanup("Write error\n",1);

	cleanup("Operation successful\n",0);

	return 0;
}
