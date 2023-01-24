/* utils.c --- General utility functions
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */


#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include <string.h>


static int jacl_whitespace(int character);

void
eachturn()
{
	/* INCREMENT THE TOTAL NUMBER OF MOVES MADE AND CALL THE 'EACHTURN'
	 * FUNCTION FOR THE CURRENT LOCATION AND THE GLOBAL 'EACHTURN'
	 * FUNCTION. THESE FUNCTIONS CONTAIN ANY CODE THAT SIMULATED EVENTS
	 * OCCURING DUE TO THE PASSING OF TIME */
	TOTAL_MOVES->value++;
	execute("+eachturn");
	strcpy(function_name, "eachturn_");
	strcat(function_name, object[HERE]->label);
	execute(function_name);
	execute("+system_eachturn");
	
	/* SET TIME TO FALSE SO THAT NO MORE eachturn FUNCTIONS ARE EXECUTED
	 * UNTIL EITHER THE COMMAND PROMPT IS RETURNED TO (AND TIME IS SET
	 * TO TRUE AGAIN, OR IT IS MANUALLY SET TO TRUE BY A VERB THAT CALLS
	 * MORE THAN ONE proxy COMMAND. THIS IS BECAUSE OTHERWISE A VERB THAT 
	 * USES A proxy COMMAND TO TRANSLATE THE VERB IS RESULT IN TWO MOVES
	 * (OR MORE) HAVING PASSED FOR THE ONE ACTION. */
	TIME->value = FALSE;
}

int
get_here()
{
	/* THIS FUNCTION RETURNS THE VALUE OF 'here' IN A SAFE, ERROR CHECKED
	 * WAY */
	if (player < 1 || player > objects) {
		badplrrun(player);
		terminate(44);
	} else if (object[player]->PARENT < 1 || object[player]->PARENT> objects || object[player]->PARENT== player) {
		badparrun();
		terminate(44);
	} else {
		return (object[player]->PARENT);
	}

	/* SHOULDN'T GET HERE, JUST TRYING TO KEEP VisualC++ HAPPY */
	return 1;
}

char *
strip_return (char *string)
{
	/* STRIP ANY TRAILING RETURN OR NEWLINE OFF THE END OF A STRING */
	int index;
	int length = strlen(string);

	for (index = 0; index < length; index++) {
		switch (string[index]) {
		case '\r':
		case '\n':
			string[index] = 0;
			break;
		}
	}

	return string;
}

int
random_number()
{
	/* GENERATE A RANDOM NUMBER BETWEEN 0 AND THE CURRENT VALUE OF
	 * THE JACL VARIABLE MAX_RAND */

	rand();
	return (1 + (int) ((float) MAX_RAND->value * rand() / (RAND_MAX + 1.0)));
}

void
create_paths(char *full_path)
{
	int				index;
	char           *last_slash;

	/* SAVE A COPY OF THE SUPPLIED GAMEFILE NAME */
	strcpy(game_file, full_path);

	/* FIND THE LAST SLASH IN THE SPECIFIED GAME PATH AND REMOVE THE GAME
	 * FILE SUFFIX IF ANY EXISTS */
	last_slash = (char *) NULL;
	
	/* GET A POINTER TO THE LAST SLASH IN THE FULL PATH */
	last_slash = strrchr(full_path, DIR_SEPARATOR);

	for (index = strlen(full_path); index >= 0; index--) {
		if (full_path[index] == DIR_SEPARATOR){
			/* NO '.' WAS FOUND BEFORE THE LAST SLASH WAS REACHED,
			 * THERE IS NO FILE EXTENSION */
			break;
		} else if (full_path[index] == '.') {
			full_path[index] = 0;
			break;
		}
	}

	/* STORE THE GAME PATH AND THE GAME FILENAME PARTS SEPARATELY */
	if (last_slash == (char *) NULL) {
		/* GAME MUST BE IN CURRENT DIRECTORY SO THERE WILL BE NO GAME PATH */
		strcpy(prefix, full_path);
		game_path[0] = 0;

		/* THIS ADDITION OF ./ TO THE FRONT OF THE GAMEFILE IF IT IS IN THE
		 * CURRENT DIRECTORY IS REQUIRED TO KEEP Gargoyle HAPPY. */
#ifdef __NDS__
		sprintf (temp_buffer, "%c%s", DIR_SEPARATOR, game_file);
#else
		sprintf (temp_buffer, ".%c%s", DIR_SEPARATOR, game_file);
#endif
		strcpy (game_file, temp_buffer);
	} else {
		/* STORE THE DIRECTORY THE GAME FILE IS IN WITH THE TRAILING
		 * SLASH IF THERE IS ONE */
		last_slash++;
		strcpy(prefix, last_slash);
		*last_slash = '\0';
		strcpy(game_path, full_path);
	}

#ifdef GLK
	/* SET DEFAULT WALKTHRU FILE NAME */
	sprintf(walkthru, "%s.walkthru", prefix);

	/* SET DEFAULT SAVED GAME FILE NAME */
	sprintf(bookmark, "%s.bookmark", prefix);

	/* SET DEFAULT BLORB FILE NAME */
#ifdef GARGLK
	// Gargoyle uses glkunix_stream_open_pathname to open this file, but
	// it's not (necessarily) going to be in the current working directory,
	// so provide the full path.
	sprintf(blorb, "%s/%s.blorb", game_path, prefix);
#else
	sprintf(blorb, "%s.blorb", prefix);
#endif
#endif

	/* SET DEFAULT FILE LOCATIONS IF NOT SET BY THE USER IN CONFIG */
	if (include_directory[0] == 0) {
		strcpy(include_directory, game_path);
		strcat(include_directory, INCLUDE_DIR);
	}

	if (temp_directory[0] == 0) {
		strcpy(temp_directory, game_path);
		strcat(temp_directory, TEMP_DIR);
	}

	if (data_directory[0] == 0) {
		strcpy(data_directory, game_path);
		strcat(data_directory, DATA_DIR);
	}
}

int
jacl_whitespace(int character)
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

char *
stripwhite (char *string)
{
    int i;

	/* STRIP WHITESPACE FROM THE START AND END OF STRING. */
	while (jacl_whitespace (*string)) string++;

	i = strlen (string) - 1;

	while (i >= 0 && ((jacl_whitespace (*(string+ i))) || *(string + i) == '\n' || *(string + i) == '\r')) i--;

#ifdef WIN32
    i++;
	*(string + i) = '\r';
#endif
    i++;
	*(string + i) = '\n';
    i++;
	*(string + i) = '\0';

    return string;
}

void
jacl_encrypt(char *string)
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
jacl_decrypt(char *string)
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

