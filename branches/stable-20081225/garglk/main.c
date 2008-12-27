#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "glk.h"
#include "glkstart.h"
#include "garglk.h"

int main(int argc, char *argv[])
{
	glkunix_startup_t startdata;
	startdata.argc = argc;
	startdata.argv = malloc(argc * sizeof(char*));
	memcpy(startdata.argv, argv, argc * sizeof(char*));

	gli_startup(argc, argv);

	if (!glkunix_startup_code(&startdata))
		glk_exit();

	glk_main();
	glk_exit();

	return 0;
}

