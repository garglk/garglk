// header for gargoyle automated testing code
// (only in debug build)
#include <unistd.h> // TODO better x-platform way to get sleep function
struct garglktstctx{
	int active, sleep, eofexit;
	char const *inpfname, *outfname;
	FILE *inpf, *outf;
	};
extern struct garglktstctx garglktstctx;
