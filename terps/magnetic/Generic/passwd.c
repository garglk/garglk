#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef unsigned char  type8;
typedef unsigned long   type32;

type8 obfuscate(type8 c) {
	static type8 state;
	type8	       i;

	if (!c) state=0;
	else {
		state^=c;
		for (i=0;i<13;i++) {
			if ((state & 1) ^ ((state>>1) & 1)) state|=0x80;
			else state&=0x7f;
			state>>=1;
		}
	}
	return state;
}

int main(int argc, char **argv) {

	type8 tmp,name[128],result[128],pad[]="MAGNETICSCR";
	type32 i,j;

	if (argc!=2) {
		printf("Usage: %s string\n",argv[0]);
		exit(1);
	}

	for (i=j=0;i<strlen(argv[1]);i++) {
		if (argv[1][i]!=0x20) name[j++]=toupper(argv[1][i]);
	}
	name[j]=0;

	tmp=name[strlen(name)-1];
	if ((tmp=='#') || (tmp==']')) name[strlen(name)-1]=0;

	if (strlen(name)<12) {
		for (i=strlen(name),j=0;i<12;i++,j++) name[i]=pad[j];
		name[i]=0;
	}

	obfuscate(0);
	i=j=0;
	while (name[i]) {
		tmp=obfuscate(name[i++]);
		if (name[i]) tmp+=obfuscate(name[i++]);
		tmp&=0x1f;
		if (tmp<26) tmp+='A';
		else tmp+=0x16;
		result[j++]=tmp;
	}
	result[j]=0;
	printf("The password for \"%s\" is: %s\n",argv[1],result);
	return 0;
}
