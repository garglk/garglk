/*
	HERES.C

	contains resource-file management routines:

		DisplayPicture
		PlayMusic
		PlaySample
		PlayVideo

		FindResource
		GetResourceParameters

	for the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman

	NOTE:  The normal course of action for loading any resource is
	first to try and find it in the specified resourcefile.  If that
	fails, an attempt is made to load the resource as an independent
	file (see FindResource()).
*/


#include "heheader.h"


/* Function prototypes: */
long FindResource(char *filename, char *resname);
int GetResourceParameters(char *filename, char *resname, int restype);

/* from hejpeg.c */
int hugo_displaypicture(HUGO_FILE infile, long len);

/* from hesound.c */
int hugo_playmusic(HUGO_FILE infile, long reslength, char loop_flag);
int hugo_playsample(HUGO_FILE infile, long reslength, char loop_flag);
void hugo_musicvolume(int vol);
void hugo_stopmusic(void);
void hugo_samplevolume(int vol);
void hugo_stopsample(void);

/* from hevideo.c */
#if !defined (COMPILE_V25)
int hugo_hasvideo(void);
int hugo_playvideo(HUGO_FILE infile, long reslength,
	char loop_flag, char background, int volume);
void hugo_stopvideo(void);
#endif


#ifndef MAX_RES_PATH
#define MAX_RES_PATH 255
#endif

HUGO_FILE resource_file;
int extra_param;
char loaded_filename[MAX_RES_PATH];
char loaded_resname[MAX_RES_PATH];
char resource_type = 0;


/* For system_status: */
#define STAT_UNAVAILABLE	((short)-1)
#define STAT_NOFILE 		101
#define STAT_NORESOURCE		102
#define STAT_LOADERROR		103


/* DISPLAYPICTURE */

void DisplayPicture(void)
{
	char filename[MAX_RES_PATH], resname[MAX_RES_PATH];
	long reslength;

	GetResourceParameters(filename, resname, PICTURE_T);
	
	if (!hugo_hasgraphics())
	{
		var[system_status] = STAT_UNAVAILABLE;
		return;
	}

	/* If filename is empty, no resource file was specified and
	   the line[] array simply holds the path of the file to be loaded
	   as a resource
	*/
	if (!(reslength = FindResource(filename, resname)))
		return;

	/* Find out what type of image resource this is */
	resource_type = (char)((fgetc(resource_file)==0xff)?JPEG_R:UNKNOWN_R);
	fseek(resource_file, -1, SEEK_CUR);

	/* If FindResource() is successful, the resource file is already
	   open and positioned; hugo_displaypicture() is responsible for
	   closing resource_file before returning regardless of success
	   or failure
	*/
	if (!hugo_displaypicture(resource_file, reslength))
		var[system_status] = STAT_LOADERROR;
}


/* PLAYMUSIC */

void PlayMusic(void)
{
	char filename[MAX_RES_PATH], resname[MAX_RES_PATH];
	char loop_flag = 0;
	long resstart, reslength;

	if (MEM(codeptr+1)==REPEAT_T) loop_flag = true, codeptr++;

	hugo_stopmusic();

	/* If a 0 parameter is passed, i.e. "music 0" */
	if (!GetResourceParameters(filename, resname, MUSIC_T))
	{
		return;
	}

	if (extra_param>=0)
	{
		if (extra_param > 100) extra_param = 100;
		hugo_musicvolume(extra_param);
	}

	if (!(reslength = FindResource(filename, resname)))
		return;

	/* Find out what type of music resource this is */
	resstart = ftell(resource_file);

	/* Check for MIDI */
	fseek(resource_file, resstart, SEEK_SET);
	fread(resname, 4, 1, resource_file);
	if (!memcmp(resname, "MThd", 4))
	{
		resource_type = MIDI_R;
		goto Identified;
	}

	/* Check for XM */
	fseek(resource_file, resstart, SEEK_SET);
	fread(resname, 17, 1, resource_file);
	if (!memcmp(resname, "Extended Module: ", 17))
	{
		resource_type = XM_R;
		goto Identified;
	}

	/* Check for S3M */
	fseek(resource_file, resstart+0x2c, SEEK_SET);
	fread(resname, 4, 1, resource_file);
	if (!memcmp(resname, "SCRM", 4))
	{
		resource_type = S3M_R;
		goto Identified;
	}

	/* Check for MOD */
	fseek(resource_file, resstart+1080, SEEK_SET);
	fread(resname, 4, 1, resource_file);
	resname[4] = '\0';
	/* There are a whole bunch of different MOD identifiers: */
	if (!strcmp(resname+1, "CHN") ||	/* 4CHN, 6CHN, 8CHN */
	    !strcmp(resname+2, "CN") ||		/* 16CN, 32CN */
	    !strcmp(resname, "M.K.") || !strcmp(resname, "M!K!") ||
	    !strcmp(resname, "FLT4") || !strcmp(resname, "CD81") ||
	    !strcmp(resname, "OKTA") || !strcmp(resname, "    "))
	{
		resource_type = MOD_R;
		goto Identified;
	}

	/* Check for MP3 */
/* Assume for now that otherwise unidentified is MP3 */
	else
	{
		resource_type = MP3_R;
		goto Identified;
	}

	/* No file type identified */
	resource_type = UNKNOWN_R;

Identified:
	fseek(resource_file, resstart, SEEK_SET);

	if (!hugo_playmusic(resource_file, reslength, loop_flag))
		var[system_status] = STAT_LOADERROR;
}


/* PLAYSAMPLE */

void PlaySample(void)
{
	char filename[MAX_RES_PATH], resname[MAX_RES_PATH];
	char loop_flag = 0;
	long reslength;

	if (MEM(codeptr+1)==REPEAT_T) loop_flag = true, codeptr++;

	hugo_stopsample();

	/* If a 0 parameter is passed, i.e. "sound 0" */
	if (!GetResourceParameters(filename, resname, SOUND_T))
	{
		return;
	}

	if (extra_param>=0)
	{
		if (extra_param > 100) extra_param = 100;
		hugo_samplevolume(extra_param);
	}

	if (!(reslength = FindResource(filename, resname)))
		return;

	/* Find out what kind of audio sample this is */
	fread(resname, 4, 1, resource_file);
	if (!memcmp(resname, "WAVE", 4))
		resource_type = WAVE_R;
	else
		resource_type = UNKNOWN_R;

	fseek(resource_file, -4, SEEK_CUR);

	if (!hugo_playsample(resource_file, reslength, loop_flag))
		var[system_status] = STAT_LOADERROR;
}


/* PLAYVIDEO */

void PlayVideo(void)
{
	char filename[MAX_RES_PATH], resname[MAX_RES_PATH];
	char loop_flag = 0, background = 0;
	int volume = 100;
	long resstart, reslength;

#if defined (COMPILE_V25)
	var[system_status] = STAT_UNAVAILABLE;
#endif
	if (MEM(codeptr+1)==REPEAT_T) loop_flag = true, codeptr++;

#if !defined (COMPILE_V25)
	hugo_stopvideo();
#endif

	/* If a 0 parameter is passed, i.e. "video 0" */
	if (!GetResourceParameters(filename, resname, VIDEO_T))
	{
		return;
	}

	if (MEM(codeptr-1)==COMMA_T)
	{
		background = (char)GetValue();
		codeptr++;	/* eol */
	}

	if (extra_param>=0)
	{
		if (extra_param > 100) extra_param = 100;
		volume = extra_param;
	}

	if (!(reslength = FindResource(filename, resname)))
		return;

	/* Find out what type of video resource this is */
	resstart = ftell(resource_file);

	/* Check for MPEG */
	fseek(resource_file, resstart, SEEK_SET);
	fread(resname, 4, 1, resource_file);
	if (resname[2]==0x01 && (unsigned char)resname[3]==0xba)
	{
		resource_type = MPEG_R;
		goto Identified;
	}

	/* Check for AVI */
	fseek(resource_file, resstart+8, SEEK_SET);
	fread(resname, 4, 1, resource_file);
	if (!memcmp(resname, "AVI ", 4))
	{
		resource_type = AVI_R;
		goto Identified;
	}

	/* No file type identified */
	resource_type = UNKNOWN_R;

Identified:
	fseek(resource_file, resstart, SEEK_SET);

#if !defined (COMPILE_V25)
	if (!hugo_playvideo(resource_file, reslength, loop_flag, background, volume))
		var[system_status] = STAT_LOADERROR;
#else
	fclose(resource_file);
	resource_file = NULL;
#endif
}


/* FINDRESOURCE

	Assumes that filename/resname contain a resourcefile name and
	a resource name.  If resname is "", filename contains the path
	of the resource on disk.  Returns the length of the resource if
	if the named resource is found.

	If FindResource() returns non-zero, the file is hot, i.e., it is
	open and positioned to the start of the resource.

	Note that resourcefiles are expected to be in (if not the current
	directory) "object" or "games", and on-disk resources in (if not the
	given directory) "source" or "resource" (where these are the 
	environment variables "HUGO_...", not actual on-disk directories).
*/

long FindResource(char *filename, char *resname)
{
	char resource_in_file[MAX_RES_PATH];
	int i, len;
	int resfileversion, rescount;
	unsigned int startofdata;
	long resposition, reslength;
#if defined (GLK)
	frefid_t fref;
#endif
/* Previously, resource positions were written as 24 bits, which meant that
   a given resource couldn't start after 16,777,216 bytes or be more than
   that length.  The new resource file format (designated by 'r') corrects this. */
	int res_32bits = true;

	resource_file = NULL;

	strcpy(loaded_filename, filename);
	strcpy(loaded_resname, resname);
	if (!strcmp(filename, "")) strcpy(loaded_filename, resname);

	/* See if the file is supposed to be in a resourcefile to
	   begin with
	*/
	if (!strcmp(filename, ""))
		goto NotinResourceFile;


	/* Open the resourcefile */
	strupr(filename);

#if !defined (GLK)
	/* stdio implementation */
	if (!(resource_file = TrytoOpen(filename, "rb", "games")))
		if (!(resource_file = TrytoOpen(filename, "rb", "object")))
		{
			var[system_status] = STAT_NOFILE;
			return 0;
		}
#else
	/* Glk implementation */
	fref = glk_fileref_create_by_name(fileusage_Data | fileusage_BinaryMode,
		filename, 0);
	resource_file = glk_stream_open_file(fref, filemode_Read, 0);
	glk_fileref_destroy(fref);
	if (!resource_file)
	{
		var[system_status] = STAT_NOFILE;
		return 0;
	}
#endif

	/* Read the resourcefile header */
	/* if (fgetc(resource_file)!='R') goto ResfileError; */
	i = fgetc(resource_file);
	if (i=='r')
		res_32bits = true;
	else if (i=='R')
		res_32bits = false;
	else
		goto ResfileError;
	resfileversion = fgetc(resource_file);
	rescount = fgetc(resource_file);
	rescount += fgetc(resource_file)*256;
	startofdata = fgetc(resource_file);
	startofdata += (unsigned int)fgetc(resource_file)*256;
	if (ferror(resource_file))
		goto ResfileError;


	/* Now skim through the list of resources in the resourcefile to
	   see if we have a match
	*/
	for (i=1; i<=rescount; i++)
	{
		len = fgetc(resource_file);
		if (ferror(resource_file))
			goto ResfileError;

		if (!(fgets(resource_in_file, len+1, resource_file)))
			goto ResfileError;

		resposition = (long)fgetc(resource_file);
		resposition += (long)fgetc(resource_file)*256L;
		resposition += (long)fgetc(resource_file)*65536L;
		if (res_32bits)
		{
			resposition += (long)fgetc(resource_file)*16777216L;
		}

		reslength = (long)fgetc(resource_file);
		reslength += (long)fgetc(resource_file)*256L;
		reslength += (long)fgetc(resource_file)*65536L;
		if (res_32bits)
		{
			reslength += (long)fgetc(resource_file)*16777216L;
		}
		if (ferror(resource_file)) goto ResfileError;

		if (!strcmp(resname, resource_in_file))
		{
			if (fseek(resource_file, (long)startofdata+resposition, SEEK_SET))
				goto ResfileError;
			return reslength;
		}
	}

ResfileError:

	var[system_status] = STAT_NORESOURCE;

#if defined (DEBUGGER)
	SwitchtoDebugger();
	sprintf(debug_line, "Unable to find \"%s\" in \"%s\"", resname, filename);
	DebugMessageBox("Resource Error", debug_line);
	SwitchtoGame();
#endif
	fclose(resource_file);
	resource_file = NULL;


	/* If we get here, we've either been unable to find the named
	   resource in the given resourcefile, or no resourcefile was
	   given
	*/
NotinResourceFile:

#if !defined (GLK)
	/* stdio implementation */
	if (!(resource_file = TrytoOpen(resname, "rb", "resource")))
		if (!(resource_file = TrytoOpen(resname, "rb", "source")))
		{
			if (!strcmp(filename, ""))
				var[system_status] = STAT_NOFILE;
			else
				var[system_status] = STAT_NORESOURCE;
			return 0;
		}
#else
	/* Glk implementation */
	fref = glk_fileref_create_by_name(fileusage_Data | fileusage_BinaryMode,
		resname, 0);
	resource_file = glk_stream_open_file(fref, filemode_Read, 0);
	glk_fileref_destroy(fref);
	if (!resource_file)
	{
		if (!strcmp(filename, ""))
			var[system_status] = STAT_NOFILE;
		else
			var[system_status] = STAT_NORESOURCE;
		return 0;
	}
#endif

	/* resource_file here refers to a resource in an individual
	   on-disk file, not a consolidated resource file
	*/
	fseek(resource_file, 0, SEEK_END);
	reslength = ftell(resource_file);
	fseek(resource_file, 0, SEEK_SET);
	if (ferror(resource_file))
	{
		fclose(resource_file);
		resource_file = NULL;
		return false;
	}

	return reslength;
}


/* GETRESOURCEPARAMETERS

	Processes resourcefile/filename (and resource, if applicable).
	Returns 0 if a valid 0 parameter is passed as in "music 0" or
	"sound 0".
*/

int GetResourceParameters(char *filename, char *resname, int restype)
{
	int f;
	
	var[system_status] = 0;

	extra_param = -1;

	codeptr++;		/* token--i.e., 'picture', etc. */

	f = GetValue();

	/* If a 0 parameter is passed for "music 0", etc. */
	if (!f && MEM(codeptr)!=COMMA_T)
	{
		++codeptr;
		return 0;
	}

	strcpy(filename, GetWord((unsigned int)f));

	if (MEM(codeptr++)!=EOL_T)	/* two or more parameters */
	{
		strupr(filename);
		strcpy(resname, GetWord(GetValue()));
		if (MEM(codeptr++)==COMMA_T)
		{
			extra_param = GetValue();
			codeptr++;
		}
	}
	else				/* only one parameter */
	{
		strcpy(resname, filename);
		strcpy(filename, "");
	}

	return true;
}
