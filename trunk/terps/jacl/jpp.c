/* jpp.c --- The JACL Preprocessor
   (C) 2001 Andreas Matthias

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include <string.h>

extern char			text_buffer[];
extern char			temp_buffer[];
extern char			*word[];
extern short int	quoted[];
extern short int	punctuated[];
extern int			wp;

extern char			user_id[];
extern char			prefix[];
extern char			game_path[];
extern char			game_file[];
extern char			processed_file[];

extern char			include_directory[];
extern char			temp_directory[];

extern char			error_buffer[];

int					lines_written;

FILE 	 	        *outputFile = NULL;
FILE   	    		*inputFile = NULL;

/* INDICATES THAT THE CURRENT '.j2' FILE BEING WORKED 
 * WITH IS ENCRYPTED */
short int			encrypted = FALSE;

/* INDICATES THAT THE CURRENT '.j2' FILE BEING WORKED 
 * WITH BEING PREPARED FOR RELEASE (DON'T INCLUDE DEBUG LIBARIES) */
short int			release = FALSE;

/* INDICATES THAT THE CURRENT '.j2' FILE BEING WORKED 
 * SHOULD BE ENCRYPTED */
short int			encrypt = TRUE;

/* INDICATES THAT THE CURRENT '.processed' FILE BRING WRITTEN SHOULD NOW
 * HAVE EACH LINE ENCRYPTED AS THE FIRST NONE COMMENT LINE HAS BEEN HIT */
short int			encrypting = FALSE;

int
jacl_whitespace(character)
	int character;
{
	/* CHECK IF A CHARACTER IS CONSIDERED WHITE SPACE IN THE JACL LANGUAGE */
	switch (character) {
		case ':':
		case '\t':
		case ' ':
			return(TRUE);
		default:
			return(FALSE);
	}
}

void
stripwhite (string)
     char *string;
{
	register int i = 0;

	/* STRIP WHITESPACE FROM THE START AND END OF STRING. */
	while (jacl_whitespace (string[i])) i++;

	if (i) strcpy (string, string + i);

	i = strlen (string) - 1;

	while (i >= 0 && (jacl_whitespace (string[i]) || string[i] == '\n' || string[i] == '\r')) i--;

#ifdef WIN32
	string[++i] = '\r';
#endif
	string[++i] = '\n';
	string[++i] = '\0';
}

int
jpp()
{
	int				game_version;

	lines_written = 0;

	/* CHECK IF GAME FILE IS ALREADY A PROCESSED FILE BY LOOKING FOR THE
	 * STRING "#encrypted" OR "#processed" WITHIN THE FIRST FIVE LINES OF 
	 * THE GAME FILE IF SO, RETURN THE GAME FILE AS THE PROCESSED FILE */
	if ((inputFile = fopen(game_file, "r")) != NULL) {
		int index = 0;
		char *result = NULL;

		result = fgets(text_buffer, 1024, inputFile);
		if (!result && !feof(inputFile)) return (FALSE);

		while (!feof(inputFile) && index < 10) {
			if (strstr(text_buffer, "#processed")) {
				/* THE GAME FILE IS ALREADY A PROCESSED FILE, JUST USE IT
				 * DIRECTLY */
				if (sscanf(text_buffer, "#processed:%d", &game_version)) {
					if (INTERPRETER_VERSION < game_version) {
						sprintf (error_buffer, OLD_INTERPRETER);
						return (FALSE);
					}
				}
				strcpy(processed_file, game_file);
				
				return (TRUE);	
			}					
			result = fgets(text_buffer, 1024, inputFile);
			if (!result && !feof(inputFile)) return (FALSE);
			index++;
		}

		fclose(inputFile);
	} else {
		sprintf (error_buffer, NOT_FOUND);
		return (FALSE);
	}

	/* SAVE A TEMPORARY FILENAME INTO PROCESSED_FILE */
	sprintf(processed_file, "%s%s.j2", temp_directory, prefix);

	/* ATTEMPT TO OPEN THE PROCESSED FILE IN THE TEMP DIRECTORY */
	if ((outputFile = fopen(processed_file, "w")) == NULL) {
		/* NO LUCK, TRY OPEN THE PROCESSED FILE IN THE CURRENT DIRECTORY */
		sprintf(processed_file, "%s.j2", prefix);
		if ((outputFile = fopen(processed_file, "w")) == NULL) {
			/* NO LUCK, CAN'T CONTINUE */
			sprintf(error_buffer, CANT_OPEN_PROCESSED, processed_file);
			return (FALSE);
		}
	}

	if (process_file(game_file, (char *) NULL) == FALSE) {
		return (FALSE);
	}

	fclose(outputFile);

	/* ALL OKAY, RETURN TRUE */
	return (TRUE);
}

int
process_file(sourceFile1, sourceFile2)
	 char           *sourceFile1;
	 char           *sourceFile2;
{
	char            temp_buffer1[1025];
	char            temp_buffer2[1025];
	FILE           *inputFile = NULL;
	char           *includeFile = NULL;
	char           *result = NULL;

	/* THIS FUNCTION WILL CREATE A PROCESSED FILE THAT HAS HAD ALL
	 * LEADING AND TRAILING WHITE SPACE REMOVED AND ALL INCLUDED
	 * FILES INSERTED */
	if ((inputFile = fopen(sourceFile1, "r")) == NULL) {
		if (sourceFile2 != NULL) {
			if ((inputFile = fopen(sourceFile2, "r")) == NULL) {
				sprintf(error_buffer, CANT_OPEN_OR, sourceFile1, sourceFile2);
				return (FALSE);
			}

		} else {
			sprintf(error_buffer, CANT_OPEN_SOURCE, sourceFile1);
			return (FALSE);
		}
	}

	*text_buffer = 0;
	result = fgets(text_buffer, 1024, inputFile);
	if (!result && !feof(inputFile)) return (FALSE);

	while (!feof(inputFile) || *text_buffer != 0) {
		if (!strncmp(text_buffer, "#include", 8) ||
		   (!strncmp(text_buffer, "#debug", 6) & !release)) {
			includeFile = strrchr(text_buffer, '"');

			if (includeFile != NULL)
				*includeFile = 0;

			includeFile = strchr(text_buffer, '"');

			if (includeFile != NULL) {
				strcpy(temp_buffer1, game_path);
				strcat(temp_buffer1, includeFile + 1);
				strcpy(temp_buffer2, include_directory);
				strcat(temp_buffer2, includeFile + 1);
				if (process_file(temp_buffer1, temp_buffer2) == FALSE) {
					return (FALSE);
				}
			} else {
				sprintf (error_buffer, BAD_INCLUDE);
				return (FALSE);
			}
		} else {
			/* STRIP WHITESPACE FROM LINE BEFORE WRITING TO OUTPUTFILE. */
			stripwhite(text_buffer);

			if (!encrypting && text_buffer[0] != '#' && text_buffer[0] != '\0' && encrypt & release) {
				/* START ENCRYPTING FROM THE FIRST NON-COMMENT LINE IN
				 * THE SOURCE FILE */
#ifdef WIN32
				fputs("#encrypted\r\n", outputFile);
#else
				fputs("#encrypted\n", outputFile);
#endif
				encrypting = TRUE;
			} 

			/* ENCRYPT PROCESSED FILE IF REQUIRED */
			if (encrypting) {
				jacl_encrypt(text_buffer);
			}

			fputs(text_buffer, outputFile);

			lines_written++;
			if (lines_written == 1) {
#ifdef WIN32
				sprintf(temp_buffer, "#processed:%d\r\n", INTERPRETER_VERSION);
#else
				sprintf(temp_buffer, "#processed:%d\n", INTERPRETER_VERSION);
#endif
				fputs(temp_buffer, outputFile);
			}
		}

		*text_buffer = 0;
		result = fgets(text_buffer, 1024, inputFile);
		if (!result && !feof(inputFile)) return (FALSE);
	}

	fclose(inputFile);

	/* ALL OKAY, RETURN TRUE */
	return (TRUE);
}

void
jacl_encrypt(string)
  char *string;
{
	int index, length;

	length = strlen(string);
	
	for (index = 0; index < length; index++) {
		if (string[index] == '\n' || string[index] == '\r') {
			return;
		}
		string[index] = string[index] ^ 255;
	}
}

void
jacl_decrypt(string)
  char *string;
{
	int index, length;

	length = strlen(string);
	
	for (index = 0; index < length; index++) {
		if (string[index] == '\n' || string[index] == '\r') {
			return;
		}
		string[index] = string[index] ^ 255;
	}
}

