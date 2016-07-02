/* jacl.c --- A GLK port of the console JACL Interpreter
   (C) 1992-2008 Stuart Allen

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

#include "csv.h"

#ifdef WIN32
#ifndef GARGLK
#include <windows.h>
#include "glkterm/glk.h"
#include "WinGlk.h"
#endif
#endif

#include "jacl.h"
#include "types.h"
#include "language.h"
#include <string.h>
#include "prototypes.h"

#ifndef GARGLK
#include "glkterm/gi_blorb.h"
#include "glkterm/glk.h"
#include "Gargoyle/garglk.h"
#endif

int convert_to_utf32 (unsigned char *text);

glui32 				status_width, status_height;

schanid_t 			sound_channel[8] = { NULL, NULL, NULL, NULL, 
										 NULL, NULL, NULL, NULL };

event_t				*cancelled_event;

extern struct csv_parser parser_csv;

extern char			text_buffer[];
extern char			*word[];
extern short int	quoted[];
extern short int	punctuated[];
extern int			wp;

extern int			custom_error;
extern int			interrupted;

extern int			jpp_error;

extern int			it;
extern int			them[];
extern int			her;
extern int			him;

extern int			oops_word;

#ifdef WINGLK
struct	string_type	*resolved_string;
#endif

char            include_directory[81] = "\0";
char            temp_directory[81] = "\0";
char            data_directory[81] = "\0";
char            special_prompt[81] = "\n: \0";
char            file_prompt[5] = ": \0";
char            bookmark[81] = "\0";
char            walkthru[81] = "\0";

char            function_name[81];

char            default_function[81];
char            override[81];

char            temp_buffer[1024];
char            error_buffer[1024];
unsigned char   chunk_buffer[4096];
#ifndef NOUNICODE
glui32          chunk_buffer_uni[4096];
#endif
char            proxy_buffer[1024];

char			oops_buffer[1024];
char			oopsed_current[1024];
char            last_command[1024];
char			*blank_command = "blankjacl\0";
char            *current_command = (char *) NULL;
char			command_buffer[1024];
#ifndef NOUNICODE
glui32			command_buffer_uni[1024];
#endif
char			players_command[1024];

int				walkthru_running = FALSE;

int				start_of_last_command;
int				start_of_this_command;

int             objects, integers, functions, strings;

/* A STREAM FOR THE GAME FILE, WHEN IT'S OPEN. */
strid_t         game_stream = NULL;

/* THE STREAM FOR OPENING UP THE ARCHIVE CONTAINING GRAPHICS AND SOUND */
strid_t				blorb_stream;

/* A FILE REFERENCE FOR THE TRANSCRIPT FILE. */
static frefid_t script_fref = NULL;
/* A STREAM FOR THE TRANSCRIPT FILE, WHEN IT'S OPEN. */
static strid_t script_stream = NULL;

int             noun[4];
int             player = 0;

int             noun3_backup;
int             player_backup = 0;

int             variable_contents;
int             oec;
int            *object_element_address,
			   *object_backup_address;

short int       spaced = TRUE;

int       		delay = 0;

/* START OF GLK STUFF */

/* POINTERS TO THE GLK WINDOWS */
winid_t mainwin = NULL;
winid_t statuswin = NULL;
winid_t promptwin = NULL;
winid_t inputwin = NULL;
winid_t current_window = NULL;

/* POINTERS TO THE WINDOWS STREAMS */
strid_t mainstr = NULL;
strid_t statusstr = NULL;
strid_t promptstr = NULL;
strid_t inputstr = NULL;

/* END OF GLK STUFF */

char            user_id[] = "local";
char            prefix[81] = "\0";
char            blorb[81] = "\0";
char            game_path[256] = "\0";
char            game_file[256] = "\0";
char            processed_file[256] = "\0";

struct object_type *object[MAX_OBJECTS];
struct integer_type *integer_table = NULL;
struct cinteger_type *cinteger_table = NULL;
struct window_type *window_table = NULL;
struct attribute_type *attribute_table = NULL;
struct string_type *string_table = NULL;
struct string_type *cstring_table = NULL;
struct function_type *function_table = NULL;
struct function_type *executing_function = NULL;
struct command_type *completion_list = NULL;
struct word_type *grammar_table = NULL;
struct synonym_type *synonym_table = NULL;
struct filter_type *filter_table = NULL;

void
glk_main(void)
{
	int             index;

	frefid_t 		blorb_file;

	srand((int) time(NULL));

	override[0] = 0;

	/* ALLOC AN EVENT TO STORE A CANCELLED EVENT IN */
 	if ((cancelled_event = (event_t *) malloc(sizeof(event_t))) == NULL)
        outofmem();

	/* CREATE style_User1 FOR USE IN THE STATUS LINE */
	glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
	glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_ReverseColor, 1);

    /* OPEN THE MAIN WINDOW THE GLK WINDOWS */
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);

    if (!mainwin) {
        /* IT'S POSSIBLE THAT THE MAIN WINDOW FAILED TO OPEN. THERE's
         * NOTHING WE CAN DO WITHOUT IT, SO EXIT. */
        return;
    } else {
		/* GET A REFERENCE TO mainwin's STREAM */
		mainstr = glk_window_get_stream(mainwin);
	}

    /* SET THE CURRENT OUTPUT STREAM TO PRINT TO IT. */
    jacl_set_window(mainwin);

    /* OPEN A THIRD WINDOW: A TEXT GRID, BELOW THE MAIN WINDOW, ONE LINE
     * HIGH. THIS IS THE WINDOW TO DISPLAY THE COMMAND PROMPT IN */
    //promptwin = glk_window_open(mainwin, winmethod_Below | winmethod_Fixed,
    //    3, wintype_TextBuffer, 0);

	/* SET THIS TO DETERMINE THE SYTEM OF INPUT TO USE */
	//inputwin = promptwin;
	inputwin = mainwin;

	if (jpp_error) {
		/* THERE WAS AN ERROR DURING PREPROCESSING. NOW THAT THERE IS AN
		 * OPEN GLK WINDOW, OUTPUT THE ERROR MESSAGE AND EXIT */
		log_error(error_buffer, FALSE);
		terminate(200);
	}

	/* OPEN THE BLORB FILE IF ONE EXISTS */
#ifndef WINGLK
	blorb_file = glk_fileref_create_by_name(fileusage_BinaryMode, blorb, 0);
#else
	strcpy(temp_buffer, game_path);
	strcat(temp_buffer, blorb);
	strcpy(blorb, temp_buffer);
	blorb_file = winglk_fileref_create_by_name(fileusage_BinaryMode, blorb, 0, 0);
#endif

	if (blorb_file != NULL && glk_fileref_does_file_exist(blorb_file)) {
		blorb_stream = glk_stream_open_file(blorb_file, filemode_Read, 0);

		if (blorb_stream != NULL) {
			/* IF THE FILE EXISTS, SET THE RESOURCE MAP */
			giblorb_set_resource_map(blorb_stream);
		}
	}

	// INTIALISE THE CSV PARSER
	csv_init(&parser_csv, CSV_APPEND_NULL);
  
	/* NO PREPROCESSOR ERRORS, LOAD THE GAME FILE */
	read_gamefile();

	execute ("+bootstrap");

    // OPEN A SECOND WINDOW: A TEXT GRID, ABOVE THE MAIN WINDOW, ONE LINE
    // HIGH. IT IS POSSIBLE THAT THIS WILL FAIL ALSO, BUT WE ACCEPT THAT.
  	statuswin = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed,
		0, wintype_TextGrid, 0);

	// GET A REFERENCE TO statuswin's STREAM
	if (statuswin != NULL) {
		statusstr = glk_window_get_stream(statuswin);
	}

#ifdef WINGLK
    if ((resolved_string = cstring_resolve("game_title")) != NULL) {
		winglk_window_set_title(resolved_string->value);
	} else {
		sprintf(temp_buffer, "JACL v%d.%d.%d ", J_VERSION, J_RELEASE, J_BUILD);
		winglk_window_set_title(temp_buffer);
	}
#endif

	if (SOUND_SUPPORTED->value) {
		/* CREATE THE EIGHT SOUND CHANNELS */
		for (index = 0; index < 8; index++) {
			sound_channel[index] = glk_schannel_create(0);
		}
	}

	jacl_set_window(mainwin);

	execute("+intro");

	if (object[2] == NULL) {
		log_error (CANT_RUN, PLUS_STDERR);
		terminate(43);
	}

    /* DUMMY RETRIEVE OF 'HERE' FOR TESTING OF GAME STATE */
    get_here();

	eachturn();

	/* TOP OF COMMAND LOOP */
	do {
		int gotline;
		event_t ev;

		custom_error = FALSE;

		jacl_set_window(mainwin);

		execute("+bottom");

		status_line();

		if (current_command != NULL) {
			strcpy(last_command, current_command);
		}

		if (inputwin == promptwin) {
			glk_window_clear(promptwin);
			jacl_set_window(inputwin);
		}

		/* OUTPUT THE CUSTOM COMMAND PROMPT */
		write_text(string_resolve("command_prompt")->value);

#ifdef NOUNICODE
		glk_request_line_event(inputwin, command_buffer, 255, 0);
#else
		glk_request_line_event_uni(inputwin, command_buffer_uni, 255, 0);
#endif

		jacl_set_window(inputwin);

		gotline = FALSE;

		while (!gotline) {
  			/* GRAB AN EVENT. */
            glk_select(&ev);

            switch (ev.type) {

                case evtype_LineInput:
                    if (ev.win == inputwin) {
                        gotline = TRUE;
						jacl_set_window(mainwin);
                        /* REALLY THE EVENT CAN *ONLY* BE FROM MAINWIN,
                         * BECAUSE WE NEVER REQUEST LINE INPUT FROM THE
                         * STATUS WINDOW. BUT WE DO A PARANOIA TEST,
                         * BECAUSE COMMANDBUF IS ONLY FILLED IF THE LINE
                         * EVENT COMES FROM THE MAINWIN REQUEST. IF THE
                         * LINE EVENT COMES FROM ANYWHERE ELSE, WE IGNORE
                         * IT. */
                    }
                    break;

				case evtype_SoundNotify:
					/* A SOUND HAS FINISHED PLAYING CALL +sound_finished
					 * WITH THE RESOUCE NUMBER AS THE FIRST ARGUMENT
					 * AND THE CHANNEL NUMBER AS THE SECOND ARGUMENT */
					sprintf(temp_buffer, "+sound_finished<%d<%d", (int) ev.val1, (int) ev.val2 - 1);
					execute(temp_buffer);
                    break;

				case evtype_Timer:
					/* A TIMER EVENT IS TRIGGERED PERIODICALLY IF THE GAME
					 * REQUESTS THEM. THIS SIMPLY EXECUTES THE FUNCTION
					 * +timer WHICH IS LIKE +eachturn EXCEPT IT DOESN'T
					 * WAIT FOR THE PLAYER TO TYPE A COMMAND */

					jacl_set_window(mainwin);
					execute("+timer");
                    break;

                case evtype_Arrange:
                    /* WINDOWS HAVE CHANGED SIZE, SO WE HAVE TO REDRAW THE
                     * STATUS WINDOW. */
                    status_line();
                    break;
            }
		}

		// THE PLAYER'S INPUT WILL BE UTF-32. CONVERT IT TO UTF-8 AND NULL TERMINATE IT
#ifndef NOUNICODE
		convert_to_utf8(command_buffer_uni, ev.val1);
#endif

		current_command = command_buffer;

		/* SET ALL THE OUTPUT TO GO TO mainwin NOW THE COMMAND HAS BEEN READ */
		if (inputwin == promptwin) {
			jacl_set_window(mainwin);
			write_text(string_resolve("command_prompt")->value);
			glk_set_style(style_Input);
			write_text(current_command);
			glk_set_style(style_Normal);
			write_text("^");
		}

		execute("+top");

		index = 0;

		if (*current_command) {
			while (*(current_command + index) && index < 1024) {
				if (*(current_command + index) == '\r' || *(current_command + index) == '\n') {
					break;
				} else {
					text_buffer[index] = *(current_command + index);
					index++;
				}
			}
		}

		text_buffer[index] = 0;

		if (text_buffer[0] == 0) {
			/* NO COMMAND WAS SPECIFIED, FILL THE COMMAND IN AS 'blankjacl'
			 * FOR THE GAME TO PROCESS AS DESIRED */
			strcpy(text_buffer, "blankjacl");
			current_command = blank_command;
		}

		command_encapsulate();
		jacl_truncate();

		index = 0;

		/* SET THE INTEGER INTERRUPTED TO FALSE. IF THIS IS SET TO
		 * TRUE BY ANY COMMAND, FURTHER PROCESSING WILL STOP */
		INTERRUPTED->value = FALSE;

		interrupted = FALSE;

		if (word[0] != NULL) {
			if (strcmp(word[0], "undo")) {
				/* COMMAND DOES NOT EQUAL undo */
				save_game_state();
			}

			if (word[0][0] == '*') {
				if (script_stream) {
					write_text(cstring_resolve("COMMENT_RECORDED")->value);
				} else {
					write_text(cstring_resolve("COMMENT_IGNORED")->value);
				}
			} else {
				/* COMMAND IS NOT NULL, START PROCESSING IT */
				preparse();
			}
		} else {
			/* NO COMMAND WAS SPECIFIED, FILL THE COMMAND IN AS 'blankjacl'
			 * FOR THE GAME TO PROCESS AS DESIRED */
			strcpy(text_buffer, "blankjacl");
			command_encapsulate();
			preparse();
		}

	} while (TRUE);
}

void
preparse()
{
	int position;

	// THE INTERRUPTED VARIABLE IS USED TO STOP LATER ACTIONS IN A COMMAND 
    // IF ANY ONE
	while (word[wp] != NULL && INTERRUPTED->value == FALSE) {
		//printf("--- preparse %s\n", word[wp]);
		// PROCESS THE CURRENT COMMAND
		// CREATE THE command STRINGS FROM THIS POINT ONWARDS SO THE VERB OF
		// THE CURRENT COMMAND IS ALWAYS command[0]. 

		clear_cstring("command");

		position = wp;

		while (word[position] != NULL && strcmp(word[position], cstring_resolve("THEN_WORD")->value)) {
			add_cstring ("command", word[position]);
			position++;
		};

		// PROCESS THE COMMAND
		word_check();

		/* THE PREVIOUS COMMAND HAS FINISHED, LOOK FOR ANOTHER COMMAND */
		while (word[wp] != NULL) {
			if (word[wp] != NULL && !strcmp(word[wp], cstring_resolve("THEN_WORD")->value)) {
				wp++;
				break;
			}
			wp++;
		}
	}
}

void
word_check()
{
	int index;

	/* REMEMBER THE START OF THIS COMMAND TO SUPPORT 'oops' AND 'again' */
	start_of_this_command = wp;
	//printf("--- command starts at %d\n", start_of_this_command);

	/* START CHECKING THE PLAYER'S COMMAND FOR SYSTEM COMMANDS */
	if (!strcmp(word[wp], cstring_resolve("QUIT_WORD")->value) || !strcmp(word[wp], "q")) {
		if (execute("+quit_game") == FALSE) {
			TIME->value = FALSE;
			write_text(cstring_resolve("SURE_QUIT")->value);
			if (get_yes_or_no()) {
				newline();
				execute("+score");
				terminate(0);
			} else {
				write_text(cstring_resolve("RETURN_GAME")->value);
			}
		}
	} else if (!strcmp(word[wp], cstring_resolve("RESTART_WORD")->value)) {
		if (execute("+restart_game") == FALSE) {
			TIME->value = FALSE;
			write_text(cstring_resolve("SURE_RESTART")->value);
			if (get_yes_or_no()) {
				write_text(cstring_resolve("RESTARTING")->value);
				restart_game();
				glk_window_clear(current_window);
				execute ("+intro");
				eachturn();
			} else {
				write_text(cstring_resolve("RETURN_GAME")->value);
			}
		}
	} else if (!strcmp(word[wp], cstring_resolve("UNDO_WORD")->value)) {
		if (execute("+undo_move") == FALSE) {
			undoing();
		}
	} else if (!strcmp(word[wp], cstring_resolve("OOPS_WORD")->value) || !strcmp(word[wp], "o")) {
		//printf("--- oops word is %d\n", oops_word);
		if (word[++wp] != NULL) {
			if (oops_word == -1) {
				if (TOTAL_MOVES->value == 0) {
					write_text(cstring_resolve("NO_MOVES")->value);
					TIME->value = FALSE;
				} else {
					write_text(cstring_resolve("CANT_CORRECT")->value);
					TIME->value = FALSE;
				}
			} else {
				strcpy(oops_buffer, word[wp]);
				strcpy(text_buffer, last_command);
				command_encapsulate();
				//printf("--- trying to replace %s with %s\n", word[oops_word], oops_buffer);
				jacl_truncate();
				word[oops_word] = (char *) &oops_buffer;

				/* BUILD A PLAIN STRING REPRESENTING THE NEW COMMAND */
				oopsed_current[0] = 0;
				index = 0;

				while (word[index] != NULL) {
					if (oopsed_current[0] != 0) {
						strcat(oopsed_current, " ");
					}

					strcat(oopsed_current, word[index]);

					index++;
				}

				current_command = oopsed_current;
				//printf("--- current command is: %s\n", current_command);

				/* PROCESS THE FIXED COMMAND ONLY */
				wp = start_of_last_command;
				word_check();
			}
		} else {
			write_text(cstring_resolve("BAD_OOPS")->value);
			TIME->value = FALSE;
		}
	} else if (!strcmp(word[wp], cstring_resolve("AGAIN_WORD")->value) || !strcmp(word[wp], "g")) {
		if (TOTAL_MOVES->value == 0) {
			write_text(cstring_resolve("NO_MOVES")->value);
			TIME->value = FALSE;
		} else if (last_command[0] == 0) {
			write_text(cstring_resolve("NOT_CLEVER")->value);
			TIME->value = FALSE;
		} else {
			strcpy(text_buffer, last_command);
			current_command = last_command;
			command_encapsulate();
			jacl_truncate();
			//printf("--- command started at %d\n", start_of_last_command);
			wp = start_of_last_command;
			word_check();
		}
	} else if (!strcmp(word[wp], cstring_resolve("SCRIPT_WORD")->value) || !strcmp(word[wp], "transcript")) {
		scripting();
	} else if (!strcmp(word[wp], cstring_resolve("UNSCRIPT_WORD")->value)) {
    	if (!script_stream) {
       		write_text(cstring_resolve("SCRIPTING_ALREADY_OFF")->value);
    	} else {
    		/* Close the file. */
    		glk_put_string_stream(script_stream, "\nEND OF A TRANSCRIPT\n");
    		glk_stream_close(script_stream, NULL);
    		write_text(cstring_resolve("SCRIPTING_OFF")->value);
    		script_stream = NULL;
		}
	} else if (!strcmp(word[wp], cstring_resolve("WALKTHRU_WORD")->value)) {
		walking_thru();
	} else if (!strcmp(word[wp], cstring_resolve("INFO_WORD")->value) || !strcmp(word[wp], "version")) {
		version_info();
		write_text("you can redistribute it and/or modify it under the ");
		write_text("terms of the GNU General Public License as published by ");
		write_text("the Free Software Foundation; either version 2 of the ");
		write_text("License, or any later version.^^");
		write_text("This program is distributed in the hope that it will be ");
		write_text("useful, but WITHOUT ANY WARRANTY; without even the ");
		write_text("implied warranty of MERCHANTABILITY or FITNESS FOR A ");
		write_text("PARTICULAR PURPOSE. See the GNU General Public License ");
		write_text("for more details.^^");
		write_text("You should have received a copy of the GNU General ");
		write_text("Public License along with this program; if not, write ");
		write_text("to the Free Software Foundation, Inc., 675 Mass Ave, ");
		write_text("Cambridge, MA 02139, USA.^^");
		sprintf(temp_buffer, "OBJECTS DEFINED:   %d^", objects);
		write_text(temp_buffer);
		TIME->value = FALSE;
	} else {
		/* NO WORD HAS BEEN MARKED AS AN ERROR YET*/
		oops_word = -1;

		/* THIS IS NOT A SYSTEM COMMAND, CALL parser TO PROCESS THE COMMAND */
		parser();
	}

	start_of_last_command = start_of_this_command;
}

void
version_info()
{
	char            buffer[80];

	sprintf(buffer, "JACL Interpreter v%d.%d.%d ", J_VERSION, J_RELEASE,
			J_BUILD);
	write_text(buffer);
	sprintf(buffer, "/ %d object.^", MAX_OBJECTS);
	write_text(buffer);
	write_text("Copyright (c) 1992-2010 Stuart Allen.^^");
}

void
save_game_state()
{
	/* THIS FUNCTION MAKES AN IN-MEMORY COPY OF THE GAME STATE AFTER EACH
	 * OF THE PLAYER'S COMMANDS SO THE 'undo' COMMAND CAN BE USED */
	int             index,
	                counter;

	struct integer_type *current_integer = integer_table;
	struct function_type *current_function = function_table;

	do {
		current_function->call_count_backup = current_function->call_count;
		current_function = current_function->next_function;
	}
	while (current_function != NULL);

	do {
		current_integer->value_backup = current_integer->value;
		current_integer = current_integer->next_integer;
	}
	while (current_integer != NULL);

	for (index = 1; index <= objects; index++) {
		if (object[index]->nosave)
			continue;

		for (counter = 0; counter < 16; counter++) {
			object[index]->integer_backup[counter] =
				object[index]->integer[counter];
		}

		object[index]->attributes_backup = object[index]->attributes;
		object[index]->user_attributes_backup = object[index]->user_attributes;
	}

	player_backup = player;
	noun3_backup = noun[3];
}

int
save_interaction(filename)
	char 		*filename;
{
	frefid_t saveref;

	jacl_set_window(inputwin);

	if (inputwin == promptwin) {
		glk_window_clear(promptwin);
		newline();
	}

	if (filename == NULL) {
		saveref = glk_fileref_create_by_prompt(fileusage_SavedGame | fileusage_BinaryMode, filemode_Write, 0);
	} else {
		saveref = glk_fileref_create_by_name(fileusage_SavedGame | fileusage_BinaryMode, filename, 0);

	}

	jacl_set_window(mainwin);

	if (!saveref) {
		write_text(cstring_resolve("CANT_SAVE")->value);
        return (FALSE);
	} else {
		if (save_game(saveref)) {
			return (TRUE);
		} else {
			write_text(cstring_resolve("CANT_SAVE")->value);
           return (FALSE);
		}
	}
}

void
restore_game_state()
{
	/* THIS FUNCTION IS CALLED AS A RESULT OF THE PLAYER USING THE 'undo'
	 * COMMAND */
	int             index,
	                counter;

	struct integer_type *current_integer = integer_table;
	struct function_type *current_function = function_table;

	do {
		current_function->call_count = current_function->call_count_backup;
		current_function = current_function->next_function;
	}
	while (current_function != NULL);


	do {
		current_integer->value = current_integer->value_backup;
		current_integer = current_integer->next_integer;
	}
	while (current_integer != NULL);

	for (index = 1; index <= objects; index++) {
		if (object[index]->nosave)
			continue;

		for (counter = 0; counter < 16; counter++)
			object[index]->integer[counter] =
				object[index]->integer_backup[counter];

		object[index]->attributes = object[index]->attributes_backup;
		object[index]->user_attributes = object[index]->user_attributes_backup;
	}

	player = player_backup;
	noun[3] = noun3_backup;

	write_text(cstring_resolve("MOVE_UNDONE")->value);
	object[HERE]->attributes &= ~1;
	execute("+top");
	execute ("+look_around");
	execute("+bottom");
	TIME->value = FALSE;
}

void
write_text(string_buffer)
	 char            *string_buffer;
{
	int             index,
					length;

	if (!strcmp(string_buffer, "tilde")) {
		glk_put_string("~");
		return;
	} else if (!strcmp(string_buffer, "caret")) {
		glk_put_string("^");
		return;
	}

	length = strlen(string_buffer);

	for (index = 0; index < length; index++) {
		if (*(string_buffer + index) == '^') {
			chunk_buffer[index] = '\n';
		} else if (*(string_buffer + index) == '~') {
			chunk_buffer[index] = '\"';
		} else {
			chunk_buffer[index] = *(string_buffer + index);
		}
	}

	chunk_buffer[index] = 0;

	/* PRINT THE CONTENTS OF string_buffer */
#ifdef NOUNICODE
	glk_put_string(chunk_buffer);
#else
	chunk_buffer_uni[(glui32) convert_to_utf32(chunk_buffer)] = 0;
	glk_put_string_uni(chunk_buffer_uni);
#endif
}

void
jacl_sleep(unsigned int mseconds)
{
	int multiplier = CLOCKS_PER_SEC / 1000;

	/* WAIT FOR A GIVEN NUMBER OF MILLISECONDS */
    clock_t goal = (mseconds * multiplier) + clock();
    while (goal > clock());
}

void
status_line()
{
	int cursor, index;
	winid_t pair_window;

	if (!statuswin) {
        return;
	} else {
		// THERE IS AN EXISTING STATUS WINDOW, MAKE SURE A NEW SIZE HASN'T BEEN
        // REQUESTED
   		glk_window_get_size(statuswin, &status_width, &status_height);
		if (status_height != integer_resolve("status_window")->value) {
			// HEIGHT HAS CHANGED, UPDATE THE WINDOW
			pair_window = glk_window_get_parent(statuswin);
			glk_window_set_arrangement(pair_window, winmethod_Above | winmethod_Fixed, integer_resolve("status_window")->value, statuswin);
   			glk_window_get_size(statuswin, &status_width, &status_height);
		}
	}

	if (status_height == 0) {
		// THE STATUS WINDOW CAN'T BE CLOSED, ONLY SET TO HAVE A HEIGHT OF ZERO
		return;
	}

	jacl_set_window(statuswin);
	glk_window_clear(statuswin);

	if (execute("+update_status_window") == FALSE) {
		glk_set_style(style_User1);

		/* DISPLAY THE INVERSE STATUS LINE AT THE TOP OF THE SCREEN */
		for (index = 0; index < status_width; index++) {
			temp_buffer[index] = ' ';
		}
		temp_buffer[index] = 0;
		write_text(temp_buffer);

    	/* PRINT THE LOCATION'S TITLE ON THE LEFT. */
		glk_window_move_cursor(statuswin, 1, 0);
		write_text(sentence_output(HERE, TRUE));

		/* BUILD THE SCORE/ MOVES STRING */
		temp_buffer[0] = 0;
		sprintf (temp_buffer, "Score: %d  Moves: %d", SCORE->value, TOTAL_MOVES->value);

		cursor = status_width - strlen(temp_buffer);
		cursor--;
		glk_window_move_cursor(statuswin, cursor, 0);
		write_text(temp_buffer);
	}

    jacl_set_window(mainwin);

}

void
newline()
{
	/* START A NEW LINE ON THE SCREEN */
	write_text("\n");
}

void
more(message)
	char* message;
{
	int character;

	jacl_set_window(inputwin);

	if (inputwin == promptwin) {
		glk_window_clear(promptwin);
		newline();
	}

	glk_set_style(style_Emphasized);
	write_text(message);
	glk_set_style(style_Normal);

	character = get_key();

	if (inputwin == mainwin) newline();
}

int
get_key()
{
	event_t ev;

	glk_request_char_event (inputwin);

	while(1) {

		glk_select(&ev);

		switch (ev.type) {
			case evtype_CharInput:
				if (ev.win == inputwin) {
					return (ev.val1);
				}
				break;
		}
	}

}

int
get_number(insist, low, high)
	int		insist;
	int		low;
	int		high;
{
    char *cx;
	char commandbuf[256];
	int response;
    int gotline;
    event_t ev;

    status_line();

	sprintf(temp_buffer, cstring_resolve("TYPE_NUMBER")->value, low, high);

    /* THIS LOOP IS IDENTICAL TO THE MAIN COMMAND LOOP IN glk_main(). */

    while (1) {
		if (inputwin == promptwin) {
			glk_window_clear(promptwin);
			jacl_set_window(inputwin);
		}

   		write_text(temp_buffer);
		jacl_set_window(mainwin);

        glk_request_line_event(inputwin, commandbuf, 255, 0);

        gotline = FALSE;
        while (!gotline) {

            glk_select(&ev);

            switch (ev.type) {
                case evtype_LineInput:
                    if (ev.win == inputwin) {
                        gotline = TRUE;
                    }
                    break;

                case evtype_Arrange:
                    status_line();
                    break;
            }
        }

        commandbuf[ev.val1] = '\0';
        for (cx = commandbuf; *cx == ' '; cx++) { };

		if (validate(cx)) {
			response = atoi(cx);
			if (response >= low && response <= high) {
				return (response);
			}
		}

		if (!insist) {
			return (-1);
		} else {
			write_text(cstring_resolve("INVALID_SELECTION")->value);
		}
    }
}

void
get_string(char *string_buffer)
{
    char *cx;
	char commandbuf[256];
    int gotline;
    event_t ev;

    status_line();

    /* THIS LOOP IS IDENTICAL TO THE MAIN COMMAND LOOP IN glk_main(). */

	if (inputwin == promptwin) {
		glk_window_clear(promptwin);
		jacl_set_window(inputwin);
	}

	jacl_set_window(mainwin);

	glk_request_line_event(inputwin, commandbuf, 255, 0);

	gotline = FALSE;
	while (!gotline) {

   		glk_select(&ev);

		switch (ev.type) {
       		case evtype_LineInput:
                if (ev.win == inputwin) {
           	         gotline = TRUE;
				}
                break;

            case evtype_Arrange:
                status_line();
                break;
        }
    }

    commandbuf[ev.val1] = '\0';
    for (cx = commandbuf; *cx == ' '; cx++) { };

	// COPY UP TO 255 BYTES OF THE ENTERED TEXT INTO THE SUPPLIED STRING
	strncpy (string_buffer, cx, 255);
}

int
get_yes_or_no(void)
{
    char *cx;
	char commandbuf[256];
    int gotline;
    event_t ev;

    status_line();

    /* THIS LOOP IS IDENTICAL TO THE MAIN COMMAND LOOP IN glk_main(). */

    while (1) {
		if (inputwin == promptwin) {
			glk_window_clear(promptwin);
			jacl_set_window(inputwin);
		}

		write_text(cstring_resolve("YES_OR_NO")->value);
		jacl_set_window(mainwin);

        glk_request_line_event(inputwin, commandbuf, 255, 0);

        gotline = FALSE;
        while (!gotline) {

            glk_select(&ev);

            switch (ev.type) {
                case evtype_LineInput:
                    if (ev.win == inputwin) {
                        gotline = TRUE;
                    }
                    break;

                case evtype_Arrange:
                    status_line();
                    break;
            }
        }

        commandbuf[ev.val1] = '\0';
        for (cx = commandbuf; *cx == ' '; cx++) { };

		// PUSH THE FIRST NON-SPACE CHARACTER TO LOWER FOR COMPARISON
		// WITH CONSTANT
		*cx = tolower(*cx);

        if (*cx == cstring_resolve("YES_WORD")->value[0]) {
            return TRUE;
		} else if (*cx == cstring_resolve("NO_WORD")->value[0]) {
            return FALSE;
		}

    }
}

char
get_character(message)
    char			*message;
{
    char *cx;
	char commandbuf[256];
    int gotline;
    event_t ev;

    status_line();

    /* THIS LOOP IS IDENTICAL TO THE MAIN COMMAND LOOP IN glk_main(). */

    while (1) {
		if (inputwin == promptwin) {
			glk_window_clear(promptwin);
			jacl_set_window(inputwin);
		}

		write_text(message);
        glk_request_line_event(inputwin, commandbuf, 255, 0);
		jacl_set_window(mainwin);

        gotline = FALSE;
        while (!gotline) {

            glk_select(&ev);

            switch (ev.type) {
                case evtype_LineInput:
                    if (ev.win == inputwin) {
                        gotline = TRUE;
                    }
                    break;

                case evtype_Arrange:
                    status_line();
                    break;
            }
        }

        commandbuf[ev.val1] = '\0';
        for (cx = commandbuf; *cx == ' '; cx++) { };

		return (*cx);
    }
}

strid_t
open_glk_file (usage, mode, filename)
	glui32 			usage;
	glui32 			mode;
	char *			filename;
{

	frefid_t	file_reference;
	strid_t		stream_reference;

   	file_reference = glk_fileref_create_by_name(usage, filename, 0);

	if (file_reference) {
    	stream_reference = glk_stream_open_file(file_reference, mode, 0);

		if (stream_reference) {
    		/* WE'RE DONE WITH THE FILE REFERENCE NOW THAT THE STREAM
     	 	 * HAS BEEN SUCCESSFULLY OPENED */
    		glk_fileref_destroy (file_reference);

			return (stream_reference);
		}
	}

	return (strid_t) NULL;
}

void
scripting()
{
	if (script_stream) {
   		write_text(cstring_resolve("SCRIPTING_ALREADY_ON")->value);
		return;
   	}

   	/* IF WE'VE TURNED ON SCRIPTING BEFORE, USE THE SAME FILE REFERENCE;
     * OTHERWISE, PROMPT THE PLAYER FOR A FILE. */
   	if (!script_fref) {
   		script_fref = glk_fileref_create_by_prompt(
        fileusage_Transcript | fileusage_TextMode,
        filemode_WriteAppend, 0);
      	if (!script_fref) {
			write_text(cstring_resolve("CANT_WRITE_SCRIPT")->value);
           	return;
       	}
   	}

  	/* OPEN THE TRANSCRIPT FILE */
   	script_stream = glk_stream_open_file(script_fref, filemode_WriteAppend, 0);

   	if (!script_stream) {
   		write_text(cstring_resolve("CANT_WRITE_SCRIPT")->value);
       	return;
   	}
   	write_text(cstring_resolve("SCRIPTING_ON")->value);
   	glk_window_set_echo_stream(mainwin, script_stream);
   	glk_put_string_stream(script_stream, "TRANSCRIPT OF: ");
   	glk_put_string_stream(script_stream, cstring_resolve("game_title")->value);
   	glk_put_string_stream(script_stream, "\n");
}

void
undoing()
{
	if (TOTAL_MOVES->value && strcmp(last_command, cstring_resolve("UNDO_WORD")->value)) {
		restore_game_state();
	} else {
		write_text(cstring_resolve("NO_UNDO")->value);
		TIME->value = FALSE;
	}
}

void
walking_thru()
{
	int result, index;

	int length;
	char script_line[81];

	/* A FILE REFERENCE FOR THE WALKTHRU FILE. */
	frefid_t walkthru_fref = NULL;

	/* A STREAM FOR THE WALKTHRU FILE, WHEN IT'S OPEN. */
	strid_t walkthru_stream = NULL;

	walkthru_fref = glk_fileref_create_by_prompt(fileusage_Data | fileusage_TextMode, filemode_Read, 0);

	if (!walkthru_fref) {
		write_text (cstring_resolve("ERROR_READING_WALKTHRU")->value);
		return;
	}

  	/* OPEN THE WALKTHRU FILE */
   	walkthru_stream = glk_stream_open_file(walkthru_fref, filemode_Read, 0);

   	if (!walkthru_stream) {
   		write_text(cstring_resolve("ERROR_READING_WALKTHRU")->value);
       	return;
   	}

	walkthru_running = TRUE;

	/* ISSUE ALL THE COMMANDS STORE IN THE WALKTHRU FILE */

    /* WE'RE DONE WITH THE FILE REFERENCE NOW THAT THE STREAM
     * HAS BEEN SUCCESSFULLY OPENED */
    glk_fileref_destroy (walkthru_fref);

	result = glk_get_line_stream(walkthru_stream, text_buffer, (glui32) 80);

	/* SET TO LOWER CASE AND STRIP NEWLINES */
	length = strlen(text_buffer);
	for (index = 0; index < length; index++) {
		if (text_buffer[index] == '\r' ||
			text_buffer[index] == '\n') {
			text_buffer[index] = 0;
			break;
		}
	}

	strcpy(script_line, text_buffer);

	while (result && INTERRUPTED->value == FALSE) {
		/* THERE COULD BE A LOT OF PROCESSING GOING ON HERE BEFORE GETTING
		 * TO THE NEXT EVENT LOOP SO CALL glk_tick AFTER EACH LINE READ */
		glk_tick();
		command_encapsulate();
		jacl_truncate();
		if (word[0] != NULL) {
			custom_error = FALSE;

			execute("+bottom");

			write_text(string_resolve("command_prompt")->value);
			glk_set_style(style_Input);
			write_text(script_line);
			newline();
			glk_set_style(style_Normal);

			execute("+top");

			preparse();
		}

		result = glk_get_line_stream(walkthru_stream, text_buffer, (glui32) 80);

		/* SET TO LOWER CASE AND STRIP NEWLINES */
		length = strlen(text_buffer);
		for (index = 0; index < length; index++) {
			if (text_buffer[index] == '\r' ||
				text_buffer[index] == '\n') {
				text_buffer[index] = 0;
				break;
			}
		}

		strcpy(script_line, text_buffer);
	}

    /* CLOSE THE STREAM */
    glk_stream_close(walkthru_stream, NULL);

	/* FINISH UP */
	walkthru_running = FALSE;
}

int
restore_interaction(filename)
	char 		*filename;
{
	frefid_t saveref;

	jacl_set_window(inputwin);

	if (inputwin == promptwin) {
		glk_window_clear(promptwin);
		newline();
	}

	if (filename == NULL) {
		saveref = glk_fileref_create_by_prompt(fileusage_SavedGame | fileusage_BinaryMode, filemode_Read, 0);
	} else {
		saveref = glk_fileref_create_by_name(fileusage_SavedGame | fileusage_BinaryMode, filename, 0);
	}

	jacl_set_window(mainwin);

	if (!saveref) {
		write_text(cstring_resolve("CANT_RESTORE")->value);
		return (FALSE);
	}

	if (restore_game(saveref, TRUE) == FALSE) {
		write_text(cstring_resolve("CANT_RESTORE")->value);
		return (FALSE);
	} else {
		return (TRUE);
	}
}

glui32
glk_get_bin_line_stream(file_stream, buffer, max_length)
	strid_t         file_stream;
	char *			buffer;
	glui32			max_length;
{
	glsi32 character = 0;

	register int index = 0;

	character = glk_get_char_stream(file_stream);
	while (character != -1 && index < (int) max_length) {
		*(buffer + index) = (char) character;
		index++;
		if (character == (glsi32) '\n' ||
            character == (glsi32) '\r') {
			break;
		}
		character = glk_get_char_stream(file_stream);
	};

	*(buffer + index) = 0;

	return ((glui32) index);
}

void
jacl_set_window(new_window)
	winid_t	new_window;
{
	current_window = new_window;
	glk_set_window(new_window);
}

#ifdef READLINE
char **
command_completion(text, start, end)
char* text;
int start;
int end;
{
    /* READLINE TAB COMPLETION CODE */
    char **options;

    options = (char **) NULL;

    if (start == 0)
        options = completion_matches (text, verb_generator);
    else
        options = completion_matches (text, object_generator);

    return (options);
}
#endif

char *
object_generator(text, state)
char* text;
int state;
{
    static int len;
    static struct command_type* now;
    struct command_type* to_send;
    struct name_type *current_name = (struct name_type *) NULL;

    /* IF THIS IS A NEW WORD TO COMPLETE, INITIALIZE NOW. THIS INCLUDES
    SAVING THE LENGTH OF TEXT FOR EFFICIENCY, AND INITIALIZING THE INDEX
    VARIABLE TO 0. */

    if (!state) {
        /* BUILD THE LIST */
        int index;
        completion_list = NULL;

        /* LOOP THROUGH ALL THE OBJECTS AND SEE IF THEY ARE IN
           THE CURRENT LOCATION */
        for (index = 1; index <= objects; index++) {
            if (parent_of (HERE, index, UNRESTRICT) && !(object[index]->attributes & NO_TAB)) {
                /* LOOP THROUGH ALL THE OBJECTS NAMES AND
                   THEM TO THE COMPLETION LIST */
                current_name = object[index]->first_name;
                while (current_name) {
                    add_word (current_name);
                    current_name = current_name->next_name;
                }
            }
        }
        now = completion_list;
        len = strlen (text);
    }

    while (now != NULL) {
        if (!strncmp(text, now->word, len)) {
            to_send = now;
            now = now->next;
            return ((char*) to_send->word);
        }
        now = now->next;
    }

    return (char *) NULL;
}

char *
verb_generator(text, state)
char* text;
int state;
{
    static int len;
    static struct command_type* now;
    struct command_type* to_send;
    struct word_type *pointer;

    /* IF THIS IS A NEW WORD TO COMPLETE, INITIALIZE NOW. THIS INCLUDES
    SAVING THE LENGTH OF TEXT FOR EFFICIENCY, AND INITIALIZING THE INDEX
    VARIABLE TO 0. */

    if (!state) {
        /* BUILD THE LIST */
        completion_list = NULL;

        pointer = grammar_table;
        while(pointer != NULL) {
            add_word(pointer->word);
            pointer = pointer->next_sibling;
        }

        add_word("walkthru");

        now = completion_list;
        len = strlen (text);
    }

    while (now != NULL) {
        if (!strncmp(text, now->word, len)) {
            to_send = now;
            now = now->next;

            /* MALLOC A COPY AND RETURN A POINTER TO THE COPY */
            return ((char*) to_send->word);
        }
        now = now->next;
    }

    return (char *) NULL;
}

/* ADD A COPY OF STRING TO A LIST OF STRINGS IF IT IS NOT
   ALREADY IN THE LIST. THIS IS FOR THE USE OF READLINE */
void
add_word(word)
char * word;
{
    static struct command_type *current_word = NULL;
    struct command_type *previous_word = NULL;

    /* DON'T ADD WORDS SUCH AS *present TO THE LIST*/
    if (*word == '*')
        return;

    if (current_word != NULL)
        previous_word = current_word;

    current_word = (struct command_type *) malloc(sizeof(struct command_type));

    if (current_word != NULL) {
        if (completion_list == NULL) {
            completion_list = current_word;
        }

        strncpy(current_word->word, word, 40);
        current_word->word[40] = 0;
        current_word->next = NULL;

        if (previous_word != NULL) {
            previous_word->next = current_word;
		}
    }
}

void 
convert_to_utf8(glui32 *text, int len) {
	int i, k;

	i = 0;
	k = 0;

	/*convert UTF-32 to UTF-8 */
	while (i < len) {
		if (text[i] < 0x80) {
			command_buffer[k] = text[i];
			k++;
		}
		else if (text[i] < 0x800) {
			command_buffer[k  ] = (0xC0 | ((text[i] & 0x7C0) >> 6));
			command_buffer[k+1] = (0x80 |  (text[i] & 0x03F)      );
			k = k + 2;
		}
		else if (text[i] < 0x10000) {
			command_buffer[k  ] = (0xE0 | ((text[i] & 0xF000) >> 12));
			command_buffer[k+1] = (0x80 | ((text[i] & 0x0FC0) >>  6));
			command_buffer[k+2] = (0x80 |  (text[i] & 0x003F)       );
			k = k + 3;
		}
		else if (text[i] < 0x200000) {
			command_buffer[k  ] = (0xF0 | ((text[i] & 0x1C0000) >> 18));
			command_buffer[k+1] = (0x80 | ((text[i] & 0x03F000) >> 12));
			command_buffer[k+2] = (0x80 | ((text[i] & 0x000FC0) >>  6));
			command_buffer[k+3] = (0x80 |  (text[i] & 0x00003F)       );
			k = k + 4;
		}
		else {
			command_buffer[k] = '?';
			k++;
		}
		i++;
	}

	/* null-terminated string */
	command_buffer[k] = '\0';
}

#ifndef NOUNICODE
int
convert_to_utf32 (unsigned char *text)
{
	int text_len;
	int rlen;

	if (!text) {
	    return 0;
	}

	text_len = strlen(text);

	if (!text_len) {
	    return 0;
	}

	rlen = (int) parse_utf8(text, text_len, chunk_buffer_uni, text_len);

	return (rlen);
}

glui32 
parse_utf8(unsigned char *buf, glui32 buflen,
    glui32 *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;
    glui32 val0, val1, val2, val3;

    while (outpos < outlen) {
        if (pos >= buflen)
            break;

        val0 = buf[pos++];

        if (val0 < 0x80) {
            res = val0;
            out[outpos++] = res;
            continue;
        }

        if ((val0 & 0xe0) == 0xc0) {
            if (pos+1 > buflen) {
                gli_strict_warning("incomplete two-byte character");
                break;
            }
            val1 = buf[pos++];
            if ((val1 & 0xc0) != 0x80) {
                gli_strict_warning("malformed two-byte character");
                break;
            }
            res = (val0 & 0x1f) << 6;
            res |= (val1 & 0x3f);
            out[outpos++] = res;
            continue;
        }

        if ((val0 & 0xf0) == 0xe0) {
            if (pos+2 > buflen) {
                gli_strict_warning("incomplete three-byte character");
                break;
            }
            val1 = buf[pos++];
            val2 = buf[pos++];
            if ((val1 & 0xc0) != 0x80) {
                gli_strict_warning("malformed three-byte character");
                break;
            }
            if ((val2 & 0xc0) != 0x80) {
                gli_strict_warning("malformed three-byte character");
                break;
            }
            res = (((val0 & 0xf)<<12)  & 0x0000f000);
            res |= (((val1 & 0x3f)<<6) & 0x00000fc0);
            res |= (((val2 & 0x3f))    & 0x0000003f);
            out[outpos++] = res;
            continue;
        }

        if ((val0 & 0xf0) == 0xf0) {
            if ((val0 & 0xf8) != 0xf0) {
                gli_strict_warning("malformed four-byte character");
                break;        
            }
            if (pos+3 > buflen) {
                gli_strict_warning("incomplete four-byte character");
                break;
            }
            val1 = buf[pos++];
            val2 = buf[pos++];
            val3 = buf[pos++];
            if ((val1 & 0xc0) != 0x80) {
                gli_strict_warning("malformed four-byte character");
                break;
            }
            if ((val2 & 0xc0) != 0x80) {
                gli_strict_warning("malformed four-byte character");
                break;
            }
            if ((val3 & 0xc0) != 0x80) {
                gli_strict_warning("malformed four-byte character");
                break;
            }
            res = (((val0 & 0x7)<<18)   & 0x1c0000);
            res |= (((val1 & 0x3f)<<12) & 0x03f000);
            res |= (((val2 & 0x3f)<<6)  & 0x000fc0);
            res |= (((val3 & 0x3f))     & 0x00003f);
            out[outpos++] = res;
            continue;
        }

        gli_strict_warning("malformed character");
    }

    return outpos;
}
#endif
