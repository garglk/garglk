/*
	HCFILE.C

	contains file-management and i/o routines:

		CleanUpFiles
		GetLine
		GetWords
		OpenFiles
		ReadWord
		TrytoOpen
		WriteCode
		WriteText
		WriteWord

	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"

long codeptr = 0;               /* position in object code */
long textptr = 0;               /* position in text file */
char buffer[MAXBUFFER];         /* the input line */
int totalfiles = 0;

FILE *sourcefile; char sourcefilename[MAXPATH]; /* original source         */
FILE *objectfile; char objectfilename[MAXPATH]; /* .HEX file               */
FILE *textfile; char textfilename[MAXPATH];     /* text file               */
FILE *allfile; char allfilename[MAXPATH];       /* source w/included files */
FILE *listfile; char listfilename[MAXPATH];     /* compilation record      */
FILE *linkfile; char linkfilename[MAXPATH];     /* .HLB file               */


/* CLEANUPFILES */

void CleanUpFiles(void)
{
	hugo_closefiles();

#if !defined (USE_TEMPFILES)
	remove(textfilename);
	remove(allfilename);
#else
	rmtmp();
#endif

	/* Remove the objectfile if we had any errors (and aren't forcing
	   it to be output anyway */
	if (er && !writeanyway)
		remove(objectfilename);
}


/* GETLINE

	Gets an unprocessed line from sourcefile and separates the words.
	<offset> is set during subsequent calls by SeparateWords if a line
	ends inside a string constant.
*/

/* hugo_fgets() is for reading (in binary mode) text files with
   Unix, DOS, and Mac line-endings
*/
char *hugo_fgets(char *string, int count, FILE *file)
{
	char *pointer = string;
	char *retval = string;
	int ch;
	long pos;

	if (count <= 0) return(NULL);

	while (--count)
	{
		if ((ch = fgetc(file))==EOF)
		{
			if (pointer==string)
				return NULL;

			break;
		}

		/* Line-ending translation goodness */
		if (ch=='\r')
		{
			pos = ftell(file);
			if (fgetc(file)!='\n')
				fseek(file, pos, SEEK_SET);
			ch = '\n';
		}

		*pointer++ = ch;

		if (ch=='\n')
		{
			break;
		}
	}

	*pointer = '\0';

        return retval;
}

void GetLine(int offset)
{
	char *a, b[MAXBUFFER*2];
	int len = 0;
	int cnest;                    	/* count nested comments */

	buffer[offset] = '\0';
	a = b;
	*a = '\0';

	while (!feof(sourcefile) && (*a=='\0' || *(a+len-1)=='\\'))
	{
		do
		{
GetaNewLine:
			tlines++;	/* total lines compiled */
			totallines++;

			a = b;
			if (!hugo_fgets(a, MAXBUFFER*2, sourcefile))
			{
				if (!feof(sourcefile)) FatalError(READ_E, sourcefilename);
				if (offset) FatalError(EOF_TEXT_E, sourcefilename);
			}
			len = strlen(a);
			if ((len) && a[len-1]=='\n') *(a+--len) = '\0';

			/* Trim leading spaces/tabs */
			while (*a==' ' || *a=='\t')
			{
				a++;
				len--;
			}

			if (*a=='!' && *(a+1)=='\\' && !offset)
			{
				cnest = 1;
				do
				{
					a = b;
					if (!hugo_fgets(a, MAXBUFFER*2, sourcefile))
					{
						if (!feof(sourcefile))
							FatalError(READ_E, sourcefilename);
						FatalError(EOF_COMMENT_E, sourcefilename);
					}
					len = strlen(a);
					*(a+--len) = '\0';

					/* Trim trailing spaces/tabs */
					while ((len>0) && (*(a+len-1)==' ' || *(a+len-1)=='\t'))
						*(a+--len) = '\0';

					/* Trim leading spaces/tabs */
					while (*a==' ' || *a=='\t')
					{
						a++;
						len--;
					}

					tlines++;
					totallines++;

					if (*a=='!' && *(a+1)=='\\')
						cnest++;
					if ((len>=2) && (*(a+len-2)=='\\' && *(a+len-1)=='!'))
						cnest--;
				} while (cnest);

				goto GetaNewLine;
			}

		} while (*a=='!' && !feof(sourcefile));

		/* Trim trailing spaces/tabs */
		while ((len>0) && (*(a+len-1)==' ' || *(a+len-1)=='\t'))
			*(a+--len) = '\0';

		if (offset + strlen(buffer+offset) + strlen(a) > MAXBUFFER)
		{
			Printout("");
			Printout(buffer+offset);
			FatalError(OVERFLOW_E, sourcefilename);
		}
		else
			strcat(buffer+offset, a);

		if (buffer[0]!='\0' && buffer[offset+strlen(buffer+offset)-1]=='\\')
			buffer[offset+strlen(buffer+offset)-1] = '\0';
	}

	if (!offset) SeparateWords();

}


/* GETWORDS

	Gets the next line of already-separated words from allfile.
*/

void GetWords(void)
{
	int bloc = 0;

	strcpy(buffer, "");
	word[1] = buffer;

	do
	{
GetNextWords:
		words = 1;
		if (!fgets(word[words], MAXBUFFER+1, allfile) && ferror(allfile))
			FatalError(READ_E, allfilename);

		word[words][strlen(word[words])-1] = '\0';

		/* Check if word is a new filename.  The '!' is used because
		   comments would never be written to the allfile.
		*/
		if (word[words][0]=='!')
		{
			strcpy(errfile, word[words]+1);
			goto GetNextWords;
		}

		while (word[words][0]!=':' && !feof(allfile))
		{
			bloc = bloc + strlen(word[words]) + 1;
			if (bloc > MAXBUFFER)
				{Printout("");
				PrintWords(1);
				FatalError(OVERFLOW_E, allfilename);}
			words++;
			word[words] = buffer + bloc;
			if (!fgets(word[words], MAXBUFFER+1, allfile)) FatalError(READ_E, allfilename);

			word[words][strlen(word[words])-1] = '\0';
		}
		words--;

	} while (words==0 && !feof(allfile));

	/* Get line number for error reporting--this is so we can locate
	   the error even when the lines are being read back from the big,
	   contiguous allfile.
	*/
	if (word[words+1][1]=='!') errline = ReadWord(allfile);

	word[words+1] = "";

/* Uncomment the following for debugging: */
/*
	strcpy(line, "");
	for (bloc=1; bloc<=words; bloc++)
	{
		strcat(line, word[bloc]);
		strcat(line, " ");
	}
	Printout(line);
*/
}


/* OPENFILES

	Does all the file-opening and calls array-mallocing (in SetMem)
	before beginning compilation.
*/

void OpenFiles(void)
{
	int i;

	SetMem();

	totalfiles = 0;

	if (!(sourcefile = TrytoOpen(sourcefilename, "rb", "source")))
		FatalError(OPEN_E, sourcefilename);
	if (!(objectfile = TrytoOpen(objectfilename, "w+b", "object")))
		FatalError(OPEN_E, objectfilename);

	/* Write blank header */
	for (i=0; i<(int)HEADER_LENGTH; i++)
		if (fputc(0, objectfile) == EOF) FatalError(WRITE_E, objectfilename);
	codeptr = HEADER_LENGTH;

#if !defined (USE_TEMPFILES)
	strcpy(textfilename, TEXTTEMPNAME);
	strcpy(allfilename, ALLTEMPNAME);

	if (!(allfile = TrytoOpen(allfilename, "w+b", "temp")))
		FatalError(OPEN_E, allfilename);
	if (!(textfile = TrytoOpen(textfilename, "w+b", "temp")))
		FatalError(OPEN_E, textfilename);
#else
	strcpy(allfilename, "work file");
	strcpy(textfilename, "work file");
	if (!(allfile = tmpfile())) FatalError(OPEN_E, allfilename);
	if (!(textfile = tmpfile())) FatalError(OPEN_E, textfilename);
#endif  /* !defined (USE_TEMPFILES) */

	if (listing==2)
		{if (!(listfile = TrytoOpen(listfilename, "wt", "list")))
			FatalError(OPEN_E, listfilename);
		listing = true;}
}


/* READWORD

	Reads a nicely compact 16-bit word from the specified workfile.
*/

unsigned int ReadWord(FILE *f)
{
	int v1, v2 = 0;

        if (((v1=fgetc(f))==EOF || (v2=fgetc(f))==EOF) && ferror(f))
		FatalError(READ_E, "work file");

	return (unsigned int)(v1 + v2*256);
}


/* TRYTOOPEN

	Tries to open a particular filename (based on a given environment
	variable or command-line directory).
*/

FILE *TrytoOpen(char *f, char *p, char *d)
{
	int i;
	char drive[MAXDRIVE], dir[MAXDIR], fname[MAXFILENAME], ext[MAXEXT];
	char envvar[32];
	FILE *tempfile; char temppath[MAXPATH];

	hugo_splitpath(f, drive, dir, fname, ext);	/* file to open */

	/* If the given filename doesn't already specify where to find it */
        if (!strcmp(drive, "") && !strcmp(dir, ""))
	{
		/* Check first for a directory in the command line 
		   or source
		*/
		for (i=0; i<directoryctr; i++)
		{
			if (!STRICMP(Mid(directory[i], 2, strlen(d)), d))
			{
				hugo_makepath(temppath, "", Right(directory[i], strlen(directory[i])-strlen(d)-2), fname, ext);

				if ((tempfile = HUGO_FOPEN(temppath, p)))
				{
					strcpy(f, temppath);  /* the new pathname */
					return tempfile;
				}
			}
		}

		/* Then check environment variables */
		strcpy(envvar, "hugo_");  /* the actual var. name */
		strcat(envvar, d);

		if (getenv(strupr(envvar)))
		{
			hugo_makepath(temppath, "", getenv(strupr(envvar)), fname, ext);

			if ((tempfile = HUGO_FOPEN(temppath, p)))
			{
				strcpy(f, temppath);  /* the new pathname */
				return tempfile;
			}
		}
	}

	/* Try to open the given, vanilla filename */
	if ((tempfile = HUGO_FOPEN(f, p)))
		return tempfile;

	return NULL;		/* return NULL if not openable */
}


/* WRITECODE

	Actually writes the code to objectfile.
*/

void WriteCode(unsigned int a, int b)
{
	int hbyte, lbyte;
	static long currentpos;

	if (currentpos != codeptr)
		{if (fseek(objectfile, codeptr, SEEK_SET)) FatalError(WRITE_E, objectfilename);
		currentpos = codeptr;}

	if (b==1)                          /* 1 byte */
	{
		if (fputc((int)a, objectfile)==EOF) FatalError(WRITE_E, objectfilename);

		codeptr++;
		currentpos++;
	}
	else                               /* 2-byte word */
	{

		hbyte = a/256;
		lbyte = a%256;

		fputc(lbyte, objectfile);
		fputc(hbyte, objectfile);
		if (ferror(objectfile)) FatalError(WRITE_E, objectfilename);

		codeptr += 2;
		currentpos += 2;
	}

}


/* WRITETEXT

	Writes string <a> to textfile and returns its position.
*/

long WriteText(char *a)
{
	int l, hbyte, lbyte;
	int i;
	long tempptr;

	l = strlen(a);
	hbyte = l/256, lbyte = l - hbyte * 256;

	if (fputc(lbyte, textfile)==EOF || fputc(hbyte, textfile)==EOF) FatalError(WRITE_E, textfilename);

	for (i=1; i<=l; i++)
	{
		if (fputc(a[i-1] + CHAR_TRANSLATION, textfile)==EOF) FatalError(WRITE_E, textfilename);
	}

	tempptr = textptr;
	textptr = textptr + 2 + l;
	textcount++;

	if (spellcheck) Printout(a);

	return tempptr;
}


/* WRITEWORD

	Writes a nicely compact 16-bit word to the specified workfile.
*/

void WriteWord(unsigned int val, FILE *f)
{
	if ((fputc(val%256, f)==EOF || fputc(val/256, f)==EOF) && ferror(f))
		FatalError(WRITE_E, "work file");
}
