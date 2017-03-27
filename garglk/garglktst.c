// gargoyle automated testing code
// (only in debug build)
#include "garglktst.h"
struct garglktstctx garglktstctx={0};
static FILE* garglktst_checked_fopen(char const *fname, const char *mode){
	FILE *f;
	if(fname){
		f=fopen(fname, mode);
		if(NULL==f){
			fprintf(stderr, "cannot open file (mode=%s): %s\n", mode, fname);
			exit(1);
			}
		return f;
		}
	return NULL;
	}
static int garglktst_init_and_stripargv(int ac, char ***pav){
	int n,i;
	char ch;
	char **av=*pav;
	for(n=1; 0==strncmp("-grg", av[n], sizeof("-grg")-1); n++){
		switch((ch=av[n][4])){
			default:
				fprintf(stderr, "unknown -grg option -grg[%c]\n", ch);
				exit(1);
			case 'i':
				garglktstctx.inpfname=av[++n];
				break;
			case 'o':
				garglktstctx.outfname=av[++n];
				break;
			case 's':
				garglktstctx.sleep=strtoul(av[++n], NULL, 10);
				break;
			case 'x':
				garglktstctx.eofexit=1;
				break;
			}
		}
	if(n>1){
		fprintf(stderr, "gargoyle testing context settings (skip to arg[%d] '%s'):\n", n, av[n]);
		garglktstctx.active=1;
		if(garglktstctx.inpfname){
			garglktstctx.inpf=garglktst_checked_fopen(garglktstctx.inpfname, "r");
			fprintf(stderr, "\tinput file: %s\n", garglktstctx.inpfname);
			fprintf(stderr, "\t\ton command eof: %s\n", garglktstctx.eofexit?"exit":"keep playing");
			}
		if(garglktstctx.outfname){
			garglktstctx.outf=garglktst_checked_fopen(garglktstctx.outfname, "w");
			fprintf(stderr, "\toutput file: %s\n", garglktstctx.outfname);
			}
		if(garglktstctx.sleep>0) fprintf(stderr, "\tpause between commands: %us\n", garglktstctx.sleep);
		n--;
		av[n]=av[0];
		*pav=&av[n];
		ac-=n;
		fprintf(stderr, "\tremaining command line args being passed on: (argc=%d)\n", ac);
		for(i=0;i<ac;i++){
			fprintf(stderr, "\t\targv[%3d]='%s'\n", i, (*pav)[i]);
			}
		}
	return ac;
	}
