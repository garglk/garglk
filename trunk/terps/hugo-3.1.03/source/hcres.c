/*
	HCRES.C

	contains resource-file writing routines:

		CreateResourceFile

	for the Hugo Compiler

	Copyright (c) 1995-2006 by Kent Tessman
*/


#include "hcheader.h"

#define MAX_RESOURCES_PER_FILE  255
#define TRANSFER_BLOCK_SIZE 1024


/* CREATERESOURCEFILE

	Called from Pass1().  Builds a resource file from:

		resource "resourcefile"
		{
			"resource1"
			"resource2"
			...
		}
*/

void CreateResourceFile(void)
{
	char error_in_filename = 0;
	char resfilename[MAXPATH];
	char filename[MAXFILENAME], drive[MAXDRIVE], dir[MAXDIR], ext[MAXEXT];
	char temppath[MAXPATH];
	int i, j;
	int rescount = 0, resourceswritten = 0;
	char **resourcepath, **resourcename;
	FILE *resfile, *resource;
	long reslength, resposition = 0, endofindex;
	
	/* Since we're now being called from Pass2(), need to set object_id
	   in case of an Error() call */
	strcpy(object_id, "resource block");

	/*
	 * Verify first of all that this is a valid header for a resource
	 * file definition block, and that the name given is a valid
	 * resourcefile name
	 */
	if (words!=2) Error("Illegal resource file definition");

	if (word[2][0]=='\"')
	{
		StripQuotes(word[2]);
		strcpy(resfilename, word[2]);
	}
	else
	{
		IDWord(2);
		for (i=0; i<dictcount; i++)
		{
			if (lexaddr[i]==(unsigned int)token_val)
			{
				strcpy(resfilename, lexentry[i]);
				break;
			}
		}
		if (i==dictcount)
			Error("Non-dictionary resourcefile name");
	}

	if (strlen(resfilename)>8) error_in_filename = true;

	strupr(resfilename);
	for (i=0; i<(int)strlen(resfilename); i++)
	{
		if ((resfilename[i]<'0' || resfilename[i]>'9') &&
			(resfilename[i]<'A' || resfilename[i]>'Z') &&
			resfilename[i]!='_')
		{
			error_in_filename = true;
		}
		resfilename[i] = (char)toupper(resfilename[i]);
	}

	if (error_in_filename)
		Error("Resourcefile name must be 8 or fewer alphanumeric characters");
	else
		sprintf(object_id, "resource file \"%s\"", resfilename);

	if ((resfile = TrytoOpen(resfilename, "wb", "object"))==NULL)
		FatalError(OPEN_E, resfilename);

	/*
	 * Write the resource-file header, followed by a to-be-filled-in
	 * number of resources and an index length (6 bytes in total)
	 */

/* Previously, resource positions were written as 24 bits, which meant that
   a given resource couldn't start after 16,777,216 bytes or be more than
   that length.  The new resource file format (designated by 'r') corrects this.
	fputc('R', resfile);
*/
	fputc('r', resfile);
	fputc(HCVERSION*10+HCREVISION, resfile);
	for (i=1; i<=4; i++) fputc(0, resfile);

	if (ferror(resfile)) FatalError(WRITE_E, resfilename);


	/*
	 * Allocate memory for the resourcepath and resourcename
	 * arrays
	 */

	if (!(resourcepath = malloc(MAX_RESOURCES_PER_FILE*sizeof(char *))))
		FatalError(MEMORY_E, "");
	if (!(resourcename = malloc(MAX_RESOURCES_PER_FILE*sizeof(char *))))
		FatalError(MEMORY_E, "");

	GetWords();
	if (word[1][0]!='{')
	{
		Error("Expecting opening brace for resource file list");
		return;
	}


	/*
	 * First pass through the files, trying to find each, getting the
	 * length, then writing the name and location (as an offset from
	 * the end of the resource index)
	 */
	while (word[words][0]!='}')
	{
		GetWords();
		
		for (j=1; j<=words; j++)
		{
			if (rescount==MAX_RESOURCES_PER_FILE)
			{
				sprintf(line, "Maximum of %d resources per resource file exceeded", MAX_RESOURCES_PER_FILE);
				Error(line);
				break;
			}
	
			if (word[words][0]=='}')
				break;
			if (word[j][0]==',')
				continue;

			StripQuotes(word[j]);

			if (word[j][0]=='\0')	/* Skip blanks, should there be any */
				continue;

			strcpy(temppath, word[j]);
	
			if ((resource = TrytoOpen(temppath, "rb", "resource"))==NULL)
				resource = TrytoOpen(temppath, "rb", "source");
	
			resourcepath[rescount] = MakeString(temppath);
	
			if (resource==NULL)
			{
				sprintf(line, "Unable to find resource \"%s\"", word[j]);
				Error(line);
	
				/* At any point if there's an error initially finding
				   or reading the resource, remove it from the list
				*/
SkipthisResource:
				if (resource!=NULL) fclose(resource);
	
				strcpy(resourcepath[rescount], "");
				resourcename[rescount] = MakeString("");
				goto NextResource;
			}
	
			/* Find out how long the resource is, then finish with it
			   (for now)
			*/
			if (fseek(resource, 0, SEEK_END)) goto SkipthisResource;
			if ((reslength = ftell(resource))==-1L) goto SkipthisResource;
			fclose(resource);
	
			/* Once the pathname has been used to open the resource, 
			   truncate it to something potentially more manageable, 
			   without any drive/directory/extension dressing.
	
			   (The reason the extension is discarded as well is because
			   some non-filename.ext naming systems--like the Acorn--switch
			   elements of the path around in hugo_splitpath(), etc.)
			*/
			hugo_splitpath(word[j], drive, dir, filename, ext);
	
			for (i=0; i<rescount; i++)
			{
				if (!strcmp(resourcename[i], filename))
				{
					sprintf(line, "Duplicate resource name \"%s\" in \"%s\"", filename, resfilename);
					Error(line);
				}
			}
	
			resourcename[rescount] = MakeString(filename);
	
			/* Write the resource filename, length first */
			fputc((int)strlen(filename), resfile);
			for (i=0; i<(int)strlen(filename); i++)
				fputc(filename[i], resfile);
			if (ferror(resfile)) FatalError(WRITE_E, resfilename);
	
			/* Now write its (eventual) position in the file, offset
			   from the end of the index...
			*/
			/* The old 24-bit way:
			fputc((int)((resposition%65536L)%256L), resfile);
			fputc((int)((resposition%65536L)/256L), resfile);
			fputc((int)(resposition/65536L), resfile);
			*/
			/* vs. the new 32-bit way: */
			fputc((int)((resposition) & 0xff), resfile);
			fputc((int)((resposition >> 8) & 0xff), resfile);
			fputc((int)((resposition >> 16) & 0xff), resfile);
			fputc((int)((resposition >> 24) & 0xff), resfile);
	
			/* ...followed by its length */
			/* The old 24-bit way:
			fputc((int)((reslength%65536L)%256L), resfile);
			fputc((int)((reslength%65536L)/256L), resfile);
			fputc((int)(reslength/65536L), resfile);
			*/
			/* vs. the new 32-bit way: */
			fputc((int)((reslength) & 0xff), resfile);
			fputc((int)((reslength >> 8) & 0xff), resfile);
			fputc((int)((reslength >> 16) & 0xff), resfile);
			fputc((int)((reslength >> 24) & 0xff), resfile);
	
			if (ferror(resfile)) FatalError(WRITE_E, resfilename);
	
			resposition+=reslength;
	
NextResource:
			if (++rescount>MAX_RESOURCES_PER_FILE) rescount = MAX_RESOURCES_PER_FILE;
		}
	}

	/*
	 * Having gotten here, we've built a list of all the valid resources
	 * into resourcefilename[] (with invalid ones as simply ""), and
	 * resfile's index has been built with names and offsets.
	 */
	if ((endofindex = ftell(resfile))==-1L) FatalError(WRITE_E, resfilename);

	/*
	 * So append each of the resources in order onto the resource file
	 */
	for (i=0; i<rescount; i++)
	{
		/* If it's valid, i.e., found during the initial search */
		if (strcmp(resourcepath[i], ""))
		{
			char buf[TRANSFER_BLOCK_SIZE];
			unsigned int read = TRANSFER_BLOCK_SIZE;
			
			if ((resource = TrytoOpen(resourcepath[i], "rb", "resource"))==NULL)
				if ((resource = TrytoOpen(resourcepath[i], "rb", "source"))==NULL)
					FatalError(READ_E, resourcepath[i]);
/*
			while (!feof(resource))
			{
				j = fgetc(resource);
				if (feof(resource)) break;
				fputc(j, resfile);
				if (ferror(resfile))
					FatalError(WRITE_E, resfilename);
				if (ferror(resource))
					FatalError(READ_E, resourcepath[i]);
			}
*/
			while (read==TRANSFER_BLOCK_SIZE)
			{
				read = fread(buf, 1, TRANSFER_BLOCK_SIZE, resource);
				if (read < 0)
					FatalError(READ_E, resourcepath[i]);
				if (fwrite(buf, 1, read, resfile) < read)
					FatalError(WRITE_E, resfilename);
			}

			resourceswritten++;

			if (fclose(resource)) FatalError(READ_E, resourcepath[i]);
		}
	}

	/*
	 * All done copying the resources, so go back and fill in the
	 * missing data in the header
	 */
	if (fseek(resfile, 2, SEEK_SET)) FatalError(WRITE_E, resfilename);

	fputc(resourceswritten%256, resfile);
	fputc(resourceswritten/256, resfile);
	fputc((int)(endofindex%256), resfile);
	fputc((int)(endofindex/256), resfile);


	/*
	 * Clean up the files and arrays before leaving
	 */
	if (fclose(resfile)) FatalError(READ_E, resfilename);

	for (i=0; i<rescount; i++)
	{
		free(resourcepath[i]);
		free(resourcename[i]);
	}
	free(resourcepath);
	free(resourcename);
}

