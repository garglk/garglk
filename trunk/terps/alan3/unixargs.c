/*----------------------------------------------------------------------*\

  unixargs - Argument handling for arun on unix and linux

  This file is included in args.c when compiling for unix and linux

  Handles the various startup methods.

  Main function args() will set up global variables adventureName,
  adventureFileName and the flags, the terminal will also be set up
  and connected if necessary.

  \*----------------------------------------------------------------------*/

/*======================================================================*/
void args(int argc, char * argv[])
{
    char *prgnam;

    if ((prgnam = strrchr(argv[0], '/')) == NULL)
	prgnam = strdup(argv[0]);
    else
	prgnam = strdup(&prgnam[1]);
    if (strrchr(prgnam, ';') != NULL)
	*strrchr(prgnam, ';') = '\0';

    /* Now look at the switches and arguments */
    switches(argc, argv);

    // TODO This is the same as in winargs.c!!
    if (adventureFileName == NULL)
	/* No game given, try program name to see if there is a game with that name */
	if (strcmp(prgnam, PROGNAME) != 0
	    && strstr(prgnam, PROGNAME) == 0) {
	    adventureFileName = strdup(argv[0]);
            adventureName = strdup(argv[0]);
            strcat(adventureFileName, ".a3c");
        }
}
