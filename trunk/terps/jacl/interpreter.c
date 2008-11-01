/* interpreter.c --- Main functions for executing JACL code.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include <string.h>

char *location_attributes[] = {
 "VISITED ", "DARK ", "ON_WATER ", "UNDER_WATER ", "WITHOUT_AIR ", "OUTDOORS ",
 "MID_AIR ", "TIGHT_ROPE ", "POLLUTED ", "SOLVED ", "MID_WATER ", "DARKNESS ",
 "MAPPED ", "KNOWN ",
 NULL };

char *object_attributes[] = {
 "CLOSED ", "LOCKED ", "DEAD ", "IGNITABLE ", "WORN ", "CONCEALING ",
 "LUMINOUS ", "WEARABLE ", "CLOSABLE ", "LOCKABLE ", "ANIMATE ", "LIQUID ",
 "CONTAINER ", "SURFACE ", "PLURAL ", "FLAMMABLE ", "BURNING ", "LOCATION ",
 "ON ", "DAMAGED ", "FEMALE ", "POSSESSIVE ", "OUT_OF_REACH ", "TOUCHED ",
 "SCORED ", "SITTING ", "FIRST ", "DONE ", "GASS ", "NO_TAB ",
 "NOT_IMPORTANT ", NULL };

char *object_elements[] = {
 "parent", "capacity", "mass", "bearing", "velocity", "next", "previous",
 "child", "index", "status", "state", "counter", "points", "class", "x", "y", 
 NULL };

char *location_elements[] = {
 "north", "south", "east", "west", "northeast", "northwest", "southeast",
 "southwest", "up", "down", "in", "out", "points", "class", "x", "y", 
 NULL };

struct stack_type				backup[STACK_SIZE];

short int						debug = FALSE;
short int						notify = TRUE;

extern short int				encrypt;
extern short int				encrypted;

extern schanid_t				sound_channel[];

int								resolved_attribute;

int             				stack = 0;
int             				current_level;
int             				execution_level;
int             				*loop_integer;
int             				*ask_integer;
int								new_x;
int								new_y;

int								interrupted = FALSE;


extern strid_t					game_stream;
extern winid_t					mainwin;
extern winid_t 					statuswin;
extern winid_t 					current_window;

extern strid_t 					mainstr;
extern strid_t 					statusstr;
extern strid_t 					quotestr;
extern strid_t 					inputstr;

extern char						user_id[];
extern char						prefix[];
extern char						text_buffer[];
extern char						chunk_buffer[];
extern char						*word[];

extern char						bookmark[];
extern char						file_prompt[];

/* CONTAINED IN PARSER.C */
extern int						object_list[4][MAX_WORDS];
extern int						list_size[];
extern int						max_size[];

/* CONTAINED IN ENCAPSULATE.C */
extern short int				quoted[];

extern struct object_type		*object[];
extern struct integer_type		*integer_table;
extern struct integer_type		*integer[];
extern struct cinteger_type		*cinteger_table;
extern struct attribute_type	*attribute_table;
extern struct string_type		*string_table;
extern struct string_type		*cstring_table;
extern struct function_type		*function_table;
extern struct function_type		*executing_function;
extern struct command_type		*completion_list;
extern struct word_type			*grammar_table;
extern struct synonym_type		*synonym_table;
extern struct filter_type		*filter_table;

extern char						function_name[];
extern char						temp_buffer[];
extern char						error_buffer[];
extern char						proxy_buffer[];

extern char						default_function[];
extern char						override[];

extern int						noun[];
extern int						wp;
extern int						buffer_index;
extern int						objects;
extern int						integers;
extern int						player;
extern int						oec;
extern int						*object_element_address;
extern int						*object_backup_address;
extern int						save_toggle;
extern int						walkthru_running;

extern FILE           			*transcript;
extern short int				noansi;
extern char						margin_string[];

char  					        integer_buffer[16];
char							string_buffer[2048];

void
terminate(code)
	 int             code;
{
	int index;

	/* CLOSES ALL FILES THEN EXIT THE INTERPRETER */

	/* CLOSE THE SOUND CHANNELS */
	for (index = 0; index < 4; index++) {
		if (sound_channel[index] != NULL) {
			glk_schannel_destroy(sound_channel[index]);
		}
	}	

    /* CLOSE THE STREAM */
	if (game_stream != NULL) {
    	glk_stream_close(game_stream, NULL);
	}
	
    glk_exit();
}

void
build_proxy()
{
	char            integer_buffer[16];
	struct integer_type *resolved_integer;
	struct cinteger_type *resolved_cinteger;
	int             index,
	                counter;

	proxy_buffer[0] = 0;

	/* LOOP THROUGH ALL THE PARAMETERS OF THE PROXY COMMAND
	   AND BUILD THE MOVE TO BE ISSUED ON THE PLAYER'S BEHALF */
	for (counter = 1; word[counter] != NULL; counter++) {
		if (quoted[counter] == 1) {
			/* THE STRING IS ENCLOSED IN QUOTES, SO CONSIDER IT
			 * AS LITERAL TEXT */
			strcat(proxy_buffer, word[counter]);
		} else {
			strcat(proxy_buffer, text_of(word[counter]));
		}
	}

	for (index = 0; index < strlen(proxy_buffer); index++) {
		if (proxy_buffer[index] == '~') {
			proxy_buffer[index] = ('\"');
		} 
	}

	//printf("--- proxy buffer = %s\n", proxy_buffer);
}

int
execute(funcname)
	 char           *funcname;
{
    char			called_name[1024];

	int             index,
					result,
	                to,
	                from;
	int             counter;
	int            *container;
	int             contents;

	int             object_1,
	                object_2;

	short int       skip_to_next;

	long            go_pointer;
	long            no_pointer;

	long            bit_mask;

	struct integer_type *resolved_integer = NULL;
	struct cinteger_type *resolved_cinteger = NULL;
	struct function_type *resolved_function = NULL;
	struct string_type *resolved_string = NULL;

	char            integer_buffer[16];

	glsi32			before_command = 0;

    /* THESE VARIABLE KEEP TRACK OF if AND endif COMMANDS TO DECIDE WHETHER
     *THE CURRENT LINE OF CODE SHOULD BE EXECUTED OR NOT */
    int             current_level = 0;
    int             execution_level = 0;

    /* THESE ARE USED AS FILE POINTER OFFSETS TO RETURN TO FIXED
     * POINTS IN THE GAME FILE */
	glsi32            				top_of_loop = 0;
	glsi32            				top_of_while = 0;
	glsi32            				top_of_do_loop = 0;

    /* THE ITERATION VARIABLE USED FOR LOOPS */
    int                             *loop_integer = NULL;

	strcpy (called_name, funcname);

	/* GET THE FUNCTION OBJECT BY THE FUNCTION NAME */
	resolved_function = function_resolve(called_name);

	if (resolved_function == NULL) {
		//printf("--- failed to find %s\n", called_name);
		return (FALSE);
	}

	push_stack(glk_stream_get_position(game_stream));

	/* CREATE ALL THE PASSED ARGUMENTS AS JACL INTEGER CONSTANTS */
	set_arguments(called_name);

	executing_function = resolved_function;
	executing_function->call_count++;

	/* SET function_name TO THE CORE NAME STORED IN THE FUNCTION OBJECT */
	/* LEAVING called_name TO CONTAIN THE FULL ARGUMENT LIST */
	strcpy (function_name, executing_function->name);

	if (debug) {
		/* OUTPUT THE ENTRY INTO THIS FUNCTION */
		sprintf(error_buffer, EXECUTING, executing_function->name);
		log_debug(error_buffer, PLUS_STDOUT);
	}

	/* JUMP TO THE POINT IN THE PROCESSED GAME 
	   FILE WHERE THIS FUNCTION STARTS */
	glk_stream_set_position(game_stream, executing_function->position, seekmode_Start);
    before_command = executing_function->position;
	result = glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);

	if (encrypted) jacl_decrypt(text_buffer);

	while (text_buffer[0] != 125 && !interrupted) {
		encapsulate();
		if (word[0] == NULL);
		else if (!strcmp(word[0], "endwhile")) {
			if (debug) log_debug(text_buffer);
			current_level--;
			if (current_level < execution_level) {
				/* THIS ENDWHILE COMMAND WAS BEING EXECUTED, 
				   NOT JUST COUNTED. */
				if (top_of_while == FALSE) {
					sprintf(error_buffer, NO_WHILE, executing_function->name);
					log_error(error_buffer, PLUS_STDOUT);
				} else {
					glk_stream_set_position(game_stream, top_of_while, seekmode_Start);
					execution_level = current_level;
				}
			}
		} else if (!strcmp(word[0], "endif")) {
			if (debug) log_debug(text_buffer);
			current_level--;
			if (current_level < execution_level) {
				/* THIS SHOULD NEVER HAPPEN */	
				execution_level = current_level;
			}
		} else if (!strcmp(word[0], "endall")) {
			if (debug) log_debug(text_buffer);
			current_level = 0;
			execution_level = 0;
		} else if (!strcmp(word[0], "else")) {
			if (debug) log_debug(text_buffer);
			if (current_level == execution_level)
				execution_level--;
			else if (current_level == execution_level + 1)
				execution_level++;
		} else if (current_level == execution_level) {
			if (debug) log_debug(text_buffer);
			if (!strcmp(word[0], "look")) {
				object[HERE]->attributes &= ~1L;
				display();
			} else if (!strcmp(word[0], "repeat")) {
				top_of_do_loop = glk_stream_get_position(game_stream);
			} else if (!strcmp(word[0], "until")) {
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				} else {
					if (top_of_do_loop == FALSE) {
						sprintf(error_buffer, NO_REPEAT, executing_function->name);
						log_error(error_buffer, PLUS_STDOUT);
					} else if (!condition()) {
						glk_stream_set_position(game_stream, top_of_do_loop, seekmode_Start);
					}
				}
			} else if (!strcmp(word[0], "while")) {
				current_level++;
				/* THIS LOOP COMES BACK TO THE START OF THE LINE CURRENTLY
				   EXECUTING, NOT THE LINE AFTER */
				top_of_while = before_command;
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				} else {
					if (condition()) {
						execution_level++;
					}
				}
			} else if (!strcmp(word[0], "loop")) {
				/* THE LOOP COMMAND LOOPS ONCE FOR EACH DEFINED 
				 * OBJECT (FOREACH) */
				top_of_loop = glk_stream_get_position(game_stream);
				if (word[1] == NULL) {
					loop_integer = &noun[2];
				} else {
					loop_integer = container_resolve(word[1]);
					if (loop_integer == NULL)
						loop_integer = &noun[2];
				}
				*loop_integer = 1;
			} else if (!strcmp(word[0], "endloop")) {
				if (top_of_loop == FALSE) {
					sprintf(error_buffer, NO_LOOP, executing_function->name);
					log_error(error_buffer, PLUS_STDOUT);
				} else {
					*loop_integer += 1;
					if (*loop_integer > objects) {
						top_of_loop = FALSE;
						*loop_integer = 0;
					} else {
						glk_stream_set_position(game_stream, top_of_loop, seekmode_Start);
					}
				}
			} else if (!strcmp(word[0], "stop")) {
				int channel;

				if (SOUND_SUPPORTED->value) {
					/* SET THE CHANNEL TO STOP, IF SUPPLIED */
					if (word[1] == NULL) {
						channel = 0;
					} else {
						channel = value_of(word[1], TRUE);

						/* SANITY CHECK THE CHANNEL SELECTED */
						if (channel < 0 || channel > 3) {
							channel = 0;
						}
					}
					glk_schannel_stop(sound_channel[channel]);
				}
			} else if (!strcmp(word[0], "volume")) {
				int channel, volume;

				if (SOUND_SUPPORTED->value) {
					/* SET THE CHANNEL TO STOP, IF SUPPLIED */
					if (word[2] == NULL) {
						channel = 0;
					} else {
						channel = value_of(word[2], TRUE);

						/* SANITY CHECK THE CHANNEL SELECTED */
						if (channel < 0 || channel > 3) {
							channel = 0;
						}
					}

					if (word[1] == NULL) {
						/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
						noproprun();
						return(exit_function (TRUE));
					} else {
						volume = value_of(word[1], TRUE);

						/* SANITY CHECK THE CHANNEL SELECTED */
						if (volume < 0) {
							volume = 0;
						}

						if (volume > 100) {
							volume = 100;
						}

						/* STORE A COPY OF THE CURRENT VOLUME FOR ACCESS
						 * FROM JACL CODE */
						sprintf(temp_buffer, "volume[%d]", channel);
						cinteger_resolve(temp_buffer)->value = volume;

						/* NOW SCALE THE 0-100 VOLUME TO THE 0-65536 EXPECTED
						 * BY Glk */
						volume = volume * 655;

						/* SET THE VOLUME */
						glk_schannel_set_volume(sound_channel[channel], volume);
						
					}
				}
			} else if (!strcmp(word[0], "timer")) {
				if (TIMER_SUPPORTED->value) {
					if (word[1] == NULL) {
						/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
						noproprun();
						return(exit_function (TRUE));
					} else {
						index = value_of(word[1], TRUE);
						if (index < 0) index = 0;
						glk_request_timer_events((glui32) index);
					}
				}
			} else if (!strcmp(word[0], "sound")) {
				int channel;
				glui32 repeats;

				if (SOUND_SUPPORTED->value) {
					/* SET THE CHANNEL TO USE, IF SUPPLIED */
					if (word[2] == NULL) {
						channel = 0;
					} else {
						channel = value_of(word[2], TRUE);

						/* SANITY CHECK THE CHANNEL SELECTED */
						if (channel < 0 || channel > 3) {
							channel = 0;
						}
					}

					/* SET THE NUMBER OF REPEATS, IF SUPPLIED */
					if (word[3] == NULL) {
						repeats = 1;
					} else {
						repeats = value_of(word[3], TRUE);
					}

					if (word[1] == NULL) {
						/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
						noproprun();
						return(exit_function (TRUE));
					} else {
						if (glk_schannel_play_ext(sound_channel[channel], (glui32) value_of(word[1], channel + 1), repeats, TRUE) == 0) {
							/* THE CHANNEL NUMBER IS PASSED SO THAT THE SOUND
							 * NOTIFICATION EVENT CAN USE THE INFORMATION
							 * IT HAS 1 ADDED TO IT SO THAT IT IS A NON-ZERO
							 * NUMBER AND THE EVENT IS ACTIVATED */
							sprintf(error_buffer, "Unable to play sound: %d", value_of(word[1]), FALSE);
							log_error(error_buffer, PLUS_STDERR);
						}
					}
				}
			} else if (!strcmp(word[0], "image")) {
				if (GRAPHICS_SUPPORTED->value) {
					if (word[1] == NULL) {
						/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
						noproprun();
						return(exit_function (TRUE));
					} else {
						if (glk_image_draw(mainwin, (glui32) value_of(word[1], TRUE), imagealign_InlineDown, 0) == 0) {
							sprintf(error_buffer, "Unable to draw image: %d", value_of(word[1], FALSE));
							log_error(error_buffer, PLUS_STDERR);
						}
					}
				}
			} else if (!strcmp(word[0], "endgame")) {
				char key_hit = 0;

				while (TRUE) {
					key_hit = get_character("^Please type S to start again, R to restore, U to undo or Q to quit: ");

					switch (key_hit) {
						case 'Q':
						case 'q':
							terminate(0);
						case 'S':
						case 's':
							newline();
							write_text("Restarting...^");
							restart_game();
							INTERRUPTED->value = FALSE;
							interrupted = TRUE;
							return(exit_function (TRUE));
						case 'U':
						case 'u':
							restore_game_state();
							interrupted = TRUE;
							return(exit_function (TRUE));
						case 'R':
						case 'r':
							if (restore_interaction()) {
								interrupted = TRUE;
								return(exit_function (TRUE));
							}
					}
				};
			} else if (!strcmp(word[0], "asknumber") || !strcmp(word[0], "getnumber")) {
				int low, high;

				int insist = FALSE;

				/* THE ONLY DIFFERENCE WITH THE getnumber COMMAND IS THAT
				 * IT INSISTS THE PLAYER GIVES A LEGAL RESPONSE */
				if (!strcmp(word[0], "getnumber")) {
					insist = TRUE;
				}

				if (word[3] != NULL) {
					ask_integer = container_resolve(word[1]);
					if (ask_integer == NULL) {
						unkvarrun(word[1]);
						return(exit_function (TRUE));
					}

					low = value_of(word[2], TRUE);
					high = value_of(word[3], TRUE);
				
					if (high == -1 || low == -1) {
						return(exit_function (TRUE));
					}

					*ask_integer = get_number(insist, low, high);
				} else {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				}
			} else if (!strcmp(word[0], "getyesorno")) {
				if (word[1] != NULL) {
					ask_integer = container_resolve(word[1]);
					if (ask_integer == NULL) {
						unkvarrun(word[1]);
						return(exit_function (TRUE));
					}

					*ask_integer = get_yes_or_no();
				} else {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				}
			} else if (!strcmp(word[0], "clear")) {
				if (!walkthru_running) {
					glk_window_clear(current_window);
				}
			} else if (!strcmp(word[0], "more")) {
				if (word[1] == NULL) {
					more ("[MORE]");
				} else {
					more (word[1]);
				}
			} else if (!strcmp(word[0], "terminate")) {
				terminate(0);
			} else if (!strcmp(word[0], "style")) {
				/* THIS COMMAND IS USED TO OUTPUT ANSI CODES OR SET GLK 
				 * STREAM STYLES */
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				} else {
					if (!strcmp(word[1], "bold") 
								|| !strcmp(word[1], "emphasised")) {
						glk_set_style(style_Emphasized);
					} else if (!strcmp(word[1], "note")) {
						glk_set_style(style_Note);
					} else if (!strcmp(word[1], "input")) {
						glk_set_style(style_Input);
					} else if (!strcmp(word[1], "header")) {
						glk_set_style(style_Header);
					} else if (!strcmp(word[1], "subheader")) {
						glk_set_style(style_Subheader);
					} else if (!strcmp(word[1], "reverse")
								|| !strcmp(word[1], "inverse")) {
						if (current_window == mainwin) {
							glk_set_style(style_User2);
						} else {
							glk_set_style(style_User1);
						}
					} else if (!strcmp(word[1], "pre") 
								|| !strcmp(word[1], "preformatted")) {
						glk_set_style(style_Preformatted);
					} else if (!strcmp(word[1], "normal")) {
						glk_set_style(style_Normal);
					} 
				}
			} else if (!strcmp(word[0], "proxy")) {
				/* THE PROXY COMMAND ISSUES A MOVE ON THE PLAYER'S BEHALF
				 * ALL STATE MUST BE SAVED SO THE CURRENT MOVE CAN CONTINUE
				 * ONCE THE PROXIED MOVE IS COMPLETE */
				push_stack(glk_stream_get_position(game_stream));

				build_proxy();

				/* TEXT BUFFER IS THE NORMAL ARRAY FOR HOLDING THE PLAYERS
				 * MOVE FOR PROCESSING */
				strcpy(text_buffer, proxy_buffer);

				command_encapsulate();
				jacl_truncate();

				preparse();
				
				pop_stack();
			} else if (!strcmp(word[0], "override")) {
				/* TELLS THE INTERPRETER TO LOOK FOR AN _override FUNCTION 
				 * TO EXECUTE IN PLACE OF ANY CODE THAT FOLLOWS THIS LINE. 
				 * THIS COMMAND IS USED EXCLUSIVELY IN GLOBAL FUNCTIONS 
				 * ASSOCIATED WITH GRAMMAR LINES */
				if (execute(override) == TRUE) {
					return(exit_function (TRUE));
				} else {
					if (execute(default_function) == TRUE) {
						return(exit_function (TRUE));
					}
				}
			} else if (!strcmp(word[0], "execute")) {
				/* CALLS ANOTHER JACL FUNCTION */
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				} else {
					if (function_resolve(word[1]) == NULL) {
						char * argstart;

						/* REMOVE ANY PARAMETERS FROM FUNCTION NAME
						   BEFORE DISPLAYING ERROR MESSAGE */
						strcpy(temp_buffer, word[1]);
						argstart = strchr(temp_buffer, '<');
						if (argstart != NULL)
							*argstart = 0;
						
						sprintf(error_buffer, UNDEFINED_FUNCTION, executing_function->name, temp_buffer);
						log_error(error_buffer, PLUS_STDOUT);
					} else {
						execute(word[1]);
					}
				}
			} else if (!strcmp(word[0], "points")) {
				/* INCREASE THE PLAYER'S SCORE AND POTENTIALLY INFORM THEM OF THE INCREASE */
				if (word[1] != NULL) {
					SCORE->value = SCORE->value + value_of(word[1], TRUE);
					if (notify) {
						glk_set_style(style_Note);
						write_text(SCORE_UP);
						sprintf(temp_buffer, "%d", value_of(word[1]), TRUE);
						write_text(temp_buffer);
						if (value_of(word[1], TRUE) == 1) {
							write_text(POINT);
						} else {
							write_text(POINTS);
						}
						glk_set_style(style_Normal);
					}
				} else {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return(exit_function (TRUE));
				}
			} else if (!strcmp(word[0], "print")) {
				/* DISPLAYS A BLOCK OF PLAIN TEXT UNTIL IT FINDS A 
				 * LINE THAT STARTS WITH A '.' OR A '}' */
				glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);

				if (encrypted) jacl_decrypt(text_buffer);

				while (text_buffer[0] != '.' && text_buffer[0] != '}') {
					index = 0;

					/* REMOVE ANY NEWLINE CHARACTERS */
					while (text_buffer[index] != 0) {
						if (text_buffer[index] == '|') {
							/* THE BAR CHARACTER IS CHANGED TO A SPACE TO 
							 * ALLOW INDENTING OF NEW PARAGRAPHS ETC */
							text_buffer[index] = ' ';
						} else if (text_buffer[index] == '\r') {
							text_buffer[index] = 0;
							break;
						} else if (text_buffer[index] == '\n') {
							text_buffer[index] = 0;
							break;
						}

						index++;
					}

					if (text_buffer[0] != 0) {
						/* CHECK IF THERE IS THE NEED TO ADD AN 
						 * IMPLICIT SPACE */
						index = strlen(text_buffer);
	
						if (text_buffer[index - 1] == '\\') {	
							/* A BACKSLASH IS USED TO INDICATE AN IMPLICIT 
							 * SPACE SHOULD NOT BE PRINTED */
							text_buffer[index - 1] = 0;
						} else if (text_buffer[index - 1] != '^') {
							strcat(text_buffer, " ");
						}
	
						/* OUTPUT THE LINE READ AS PLAIN TEXT */
						write_text(text_buffer);
					}

					/* GET THE NEXT LINE */
					glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
					if (encrypted) jacl_decrypt(text_buffer);

				}
			} else if (!strcmp(word[0], "write")) {
				for (counter = 1; word[counter] != NULL && counter < MAX_WORDS; counter++) {
					if (quoted[counter] == 1) {
						/* THE STRING IS ENCLOSED IN QUOTES, SO CONSIDER IT
			 		 	 * AS LITERAL TEXT */
						write_text(word[counter]);
					} else {
						/* THE STRING IS NO ENCLOSED IN QUOTES, SO TRY TO
						 * RESOLVE ITS TRUE VALUE */
						write_text(text_of(word[counter]));
					}
				}
			} else if (!strcmp(word[0], "cursor")) {
				if (word[2] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return(exit_function (TRUE));
				} else {
					if (current_window == statuswin) {
						glk_window_move_cursor(statuswin, value_of(word[1], TRUE), value_of(word[2], TRUE));
					} else {
						log_error(BAD_CURSOR, PLUS_STDOUT);
					}
				}
			} else if (!strcmp(word[0], "length")) {
				string_buffer[0] = 0;

				if (word[2] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return(exit_function (TRUE));
				} else {
					if ((container = container_resolve(word[1])) == NULL) {
						unkvarrun(word[1]);
						return(exit_function (TRUE));
					}
					
					*container = strlen(text_of(word[2]));	
				}
			} else if (!strcmp(word[0], "savegame")) {
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					if ((container = container_resolve(word[1])) == NULL) {
						unkvarrun(word[1]);
						return (exit_function(TRUE));
					} else {
						*container = save_interaction();
					}
				} 
			} else if (!strcmp(word[0], "restoregame")) {
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					if ((container = container_resolve(word[1])) == NULL) {
						unkvarrun(word[1]);
						return (exit_function(TRUE));
					} else {
						*container = restore_interaction();
					}
				} 
			} else if (!strcmp(word[0], "restartgame")) {
				restart_game();
			} else if (!strcmp(word[0], "undomove")) {
				undoing();
			} else if (!strcmp(word[0], "updatestatus")) {
				status_line();
			} else if (!strcmp(word[0], "setstring") ||
						!strcmp(word[0], "addstring")) {
				string_buffer[0] = 0;

				if (word[2] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return (exit_function(TRUE));
				} else {
					/* GET A POINTER TO THE STRING BEING MODIFIED */
					if ((resolved_string = string_resolve(word[1])) == NULL) {
						unkstrrun(word[1]);
						return (exit_function(TRUE));
					}

					/* RESOLVE ALL THE TEXT AND STORE IT IN A TEMPORARY BUFFER*/
					for (counter = 2; word[counter] != NULL && counter < MAX_WORDS; counter++) {
						if (quoted[counter] == 1) {
							/* THE STRING IS ENCLOSED IN QUOTES, SO CONSIDER
			 		 	 	 * IT AS LITERAL TEXT */
							strcat(string_buffer, word[counter]);
						} else {
							/* THE STRING IS NO ENCLOSED IN QUOTES, SO TRY TO
						 	* RESOLVE ITS TRUE VALUE */
							strcat(string_buffer, text_of(word[counter]));
						}
					}

					/* string_buffer IS NOW FILLED, COPY THE UP TO 256 BYTES OF
					 * IT INTO THE STRING */
					if (!strcmp(word[0], "setstring")) {
						strncpy (resolved_string->value, string_buffer, 255);
					} else {
						/* CALCULATE HOW MUCH SPACE IS LEFT IN THE STRING */
						counter = 255 - strlen(resolved_string->value);
						/* THIS IS A addstring COMMAND, SO USE STRNCAT INSTEAD */
						strncat (resolved_string->value, string_buffer, counter);
					}
				}
			} else if (!strcmp(word[0], "padstring")) {
				string_buffer[0] = 0;

				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return (exit_function(TRUE));
				} else {
					/* GET A POINTER TO THE STRING BEING MODIFIED */
					if ((resolved_string = string_resolve(word[1])) == NULL) {
						unkstrrun(word[1]);
						return (exit_function(TRUE));
					}

					index = value_of(word[3], TRUE);	

					if (quoted[2] == 1) {
						/* THE STRING IS ENCLOSED IN QUOTES, SO CONSIDER IT
						 * AS LITERAL TEXT */
						for (counter = 0; counter < index; counter++) {
							strcat(string_buffer, word[2]);
						}
					} else {
						/* THE STRING IS NO ENCLOSED IN QUOTES, SO TRY TO
						 * RESOLVE ITS TRUE VALUE */
						for (counter = 0; counter < index; counter++) {
							strcat(string_buffer, text_of(word[2]));
						}
					}

					/* string_buffer IS NOW FILLED, COPY THE UP TO 256 BYTES OF
					 * IT INTO THE STRING */
					strncpy (resolved_string->value, string_buffer, 256);
				}
			} else if (!strcmp(word[0], "return")) {
				/* RETURN FROM THIS FUNCTION, POSSIBLY RETURNING AN INTEGER VALUE */
				if (word[1] == NULL) {
					return (exit_function(TRUE));
				} else {
					index = value_of(word[1], TRUE);
					return (exit_function(index));
				}
			} else if (!strcmp(word[0], "position")) {
				/* MOVE AN OBJECT TO ITS NEW X,Y COORDINATES BASED ON ITS CURRENT VALUES
				 * FOR x, y, bearing, velocity */
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					object_1 = value_of(word[1], TRUE);

					if (object_1 < 1 || object_1 > objects) {
						badptrrun(word[1], object_1);
						return (exit_function(TRUE));
					} else {
						new_position((double) object[object_1]->X,
									 (double) object[object_1]->Y,
									 (double) object[object_1]->BEARING,
									 (double) object[object_1]->VELOCITY);

						object[object_1]->X = new_x;
						object[object_1]->Y = new_y;
					}
				}
			} else if (!strcmp(word[0], "bearing")) {
				/* CALCULATE THE BEARING BETWEEN TWO OBJECTS */
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					if ((container = container_resolve(word[1])) == NULL) {
						unkvarrun(word[1]);
						return (exit_function(TRUE));
					}

					object_1 = value_of(word[2], TRUE);

					if (object_1 < 1 || object_1 > objects) {
						badptrrun(word[2], object_1);
						return (exit_function(TRUE));
					} else {
						object_2 = value_of(word[3], TRUE);

						if (object_2 < 1 || object_2 > objects) {
							badptrrun(word[3], object_2);
							return (exit_function(TRUE));
						} else {
							if (container != NULL
								&& object_1 != FALSE
								&& object_2 != FALSE) {
								*container = bearing((double) object[object_1]->X,
													 (double) object[object_1]->Y,
													 (double) object[object_2]->X,
													 (double) object[object_2]->Y);
							}
						}
					}
				}
			} else if (!strcmp(word[0], "distance")) {
				/* CALCULATE THE DISTANCE BETWEEN TWO OBJECTS */
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					container = container_resolve(word[1]);

					object_1 = value_of(word[2], TRUE);

					if (object_1 < 1 || object_1 > objects) {
						badptrrun(word[2], object_1);
						return (exit_function(TRUE));
					} else {
						object_2 = value_of(word[3], TRUE);

						if (object_2 < 1 || object_2 > objects) {
							badptrrun(word[3], object_2);
							return (exit_function(TRUE));
						} else {
							if (container != NULL
								&& object_1 != FALSE
								&& object_2 != FALSE) {
								*container = distance((double)
													  object[object_1]->X,
													  (double)
													  object[object_1]->Y,
													  (double)
													  object[object_2]->X,
													  (double)
													  object[object_2]->Y);
							}
						}
					}
				}
			} else if (!strcmp(word[0], "dir_to") ||
					   !strcmp(word[0], "npc_to")) {
				/* CALCULATE THE DISTANCE BETWEEN TWO OBJECTS */
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					container = container_resolve(word[1]);

					object_1 = value_of(word[2], TRUE);

					if (object_1 < 1 || object_1 > objects) {
						badptrrun(word[2], object_1);
						return (exit_function(TRUE));
					} else {
						object_2 = value_of(word[3], TRUE);

						if (object_2 < 1 || object_2 > objects) {
							badptrrun(word[3], object_2);
							return (exit_function(TRUE));
						} else {
							if (container != NULL
								&& object_1 != FALSE
								&& object_2 != FALSE) {
								if (!strcmp(word[0], "dir_to")) {
									*container = find_route(object_1, object_2, TRUE);
								} else {
									*container = find_route(object_1, object_2, FALSE);
								}
							}
						}
					}
				}
			} else if (!strcmp(word[0], "set")) {
				/* SET THE VALUE OF AN ELEMENT TO A SUPPLIED INTEGER */
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					container = container_resolve(word[1]);

					if (container == NULL) {
						unkvarrun(word[1]);
						return (exit_function(TRUE));
					} else {
						contents = value_of(word[3], TRUE);
						if (word[2][0] == '+')
							*container += contents;
						else if (word[2][0] == '-')
							*container -= contents;
						else if (word[2][0] == '*')
							*container = *container * contents;
						else if (word[2][0] == '%')
							*container = *container % contents;
						else if (word[2][0] == '/') {
							if (contents == 0) {
								sprintf(error_buffer, DIVIDE_BY_ZERO,
										executing_function->name);
								log_error(error_buffer, PLUS_STDOUT);
							} else
								*container = *container / contents;
						} else if (word[2][0] == '=')
							*container = contents;
						else {
							sprintf(error_buffer, ILLEGAL_OPERATOR,
									executing_function->name,
									word[2]);
							log_error(error_buffer, PLUS_STDOUT);
						}
					}
				}
			} else if (!strcmp(word[0], "ensure")) {
				/* USED TO GIVE OR TAKE AN ATTRIBUTE TO OR FROM AND OBJECT */
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					if (bit_mask = attribute_resolve(word[3])) {
						index = value_of(word[1], TRUE);
						if (index < 1 || index > objects) {
							badptrrun(word[1], index);
							return (exit_function(TRUE));
						} else {
							if (!strcmp(word[2], "has")) {
								object[index]->attributes =
									object[index]->attributes | bit_mask;
							} else if (!strcmp(word[2], "hasnt")) {
								bit_mask = ~bit_mask;
								object[index]->attributes =
									object[index]->attributes & bit_mask;
							}
						}
					} else if (bit_mask = user_attribute_resolve(word[3])) {
						index = value_of(word[1], TRUE);
						if (index < 1 || index > objects) {
							badptrrun(word[1], index);
							return (exit_function(TRUE));
						} else {
							if (!strcmp(word[2], "has")) {
								object[index]->user_attributes =
									object[index]->user_attributes | bit_mask;
							} else if (!strcmp(word[2], "hasnt")) {
								bit_mask = ~bit_mask;
								object[index]->user_attributes =
									object[index]->user_attributes & bit_mask;
							}
						}
					} else {
						unkattrun(3);
						return (exit_function(TRUE));
					}
				}
			} else if (!strcmp(word[0], "travel")) {
				/* THE TRAVEL COMMAND IS USED TO MOVE THE PLAYER AROUND.
				 * IT IS RARELY USED OUTSIDE THE LIBRARY'S DIRECTION COMMANDS */
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					if ((resolved_cinteger = cinteger_resolve(word[1])) != NULL) {
						index = resolved_cinteger->value;

						if (index < 0 || index > 11) {
							index = -1;
						}
					} else {
						index = -1;
					}

					if (index != -1) {
						DESTINATION->value = object[HERE]->integer[index];
						COMPASS->value = index;
						move_player();
					} else {
						unkdirrun(1);
						return (exit_function(TRUE));
					}
				}
			} else if (!strcmp(word[0], "inspect")) {
				if (word[1] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				} else {
					inspect(value_of(word[1], TRUE));
				}
			} else if (!strcmp(word[0], "move")) {
				/* THIS COMMAND IS USED TO MOVE AN OBJECT TO HAVE ANOTHER PARENT
				 * INCLUDING MODIFYING ALL QUANTITY VALUES BASED ON THE OBJECTS MASS */
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun();
					return (exit_function(TRUE));
				}

				index = value_of(word[1], TRUE);
				if (index < 1 || index > objects) {
					badptrrun(word[1], index);
					return (exit_function(TRUE));
				} else {
					from = object[index]->PARENT;
					if (from && !(object[from]->attributes & LOCATION)) {
						object[from]->QUANTITY += object[index]->MASS;
					}
					to = value_of(word[3], TRUE);
					if (to < 1 || to > objects) {
						badptrrun(word[1], to);
						return (exit_function(TRUE));
					} else {
						object[index]->PARENT = to;
						if (!(object[to]->attributes & LOCATION))
							object[to]->QUANTITY -= object[index]->MASS;
					} 
				}
			} else if (!strcmp(word[0], "ifstring")) {
				/* CHECK IF A STRING EQUALS OR CONTAINS ANOTHER STRING */
				current_level++;
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return (exit_function(TRUE));
				} else if (strcondition()) {
					execution_level++;
				}
			} else if (!strcmp(word[0], "if")) {
				current_level++;
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return (exit_function(TRUE));
				} else if (condition()) {
					execution_level++;
				}
			} else if (!strcmp(word[0], "ifall")) {
				current_level++;
				if (word[3] == NULL) {
					/* NOT ENOUGH PARAMETERS SUPPLIED FOR THIS COMMAND */
					noproprun(0);
					return (exit_function(TRUE));
				} else if (and_condition()) {
					execution_level++;
				}
			} else {
				sprintf(error_buffer, UNKNOWN_COMMAND,
						executing_function->name, word[0]);
				log_error(error_buffer, PLUS_STDOUT);
			}
		} else if (!strcmp(word[wp], "if") 
				|| !strcmp(word[wp], "ifall")
				|| !strcmp(word[wp], "ifstring")
				|| !strcmp(word[wp], "while")) {
			current_level++;
		}

		before_command = glk_stream_get_position(game_stream);
		glk_get_bin_line_stream(game_stream, text_buffer, (glui32) 1024);
		if (encrypted) jacl_decrypt(text_buffer);
	};

	return (exit_function(TRUE));
}

int
exit_function(return_code)
	int			return_code;
{
	/* POP THE STACK REGARDLESS OF THE RETURN CODE */
	pop_stack();

	return (return_code);
}

char           *
object_names(object_index)
	 int             object_index;
{
	/* THIS FUNCTION CREATES A LIST OF ALL AN OBJECT'S NAMES.
	   THE escape ARGUMENT INDICATES WHETHER A + SIGN SHOULD BE
	   USED IN PLACE OF A SPACE BETWEEN EACH OF THE NAMES */
	struct name_type *current_name = object[object_index]->first_name;
	temp_buffer[0] = 0;

	while (current_name != NULL) {
		strcat(temp_buffer, " ");
		strcat(temp_buffer, current_name->name);
		current_name = current_name->next_name;
	}

	return (temp_buffer);
}

int
distance(x1, y1, x2, y2)
	 double          x1,
	                 y1,
	                 x2,
	                 y2;
{
	/* THIS FUNCTION CALCULATES THE DISTANCE BETWEEN TWO POINTS IN A 
	   TWO-DIMENSIONAL PLANE */
	double          delta_x,
	                delta_y;
	double          distance,
	                total;

	/*
	 * Object two in which quadrant compared to object one? 0 x = opp, y = 
	 * ajd + 0 degrees 1 x = adj, y = opp + 90 degrees 2 x = opp, y = ajd
	 * + 180 degrees 3 x = adj, y = opp + 270 degrees 
	 */

	/*
	 * DETERMINE WHICH QUADRANT OBJECT TWO IS IN 
	 */

	if (x2 > x1) {
		/*
		 * OBJECT TWO IS IN 1 OR 2 
		 */
		delta_x = x2 - x1;
		if (y2 > y1) {
			delta_y = y2 - y1;
		} else {
			delta_y = y1 - y2;
		}
	} else {
		/*
		 * OBJECT TWO IS IN 3 OR 4 
		 */
		delta_x = x1 - x2;
		if (y2 > y1) {
			delta_y = y2 - y1;
		} else {
			delta_y = y1 - y2;
		}
	}

	delta_y = delta_y * delta_y;
	delta_x = delta_x * delta_x;

	total = delta_y + delta_x;

	distance = sqrt(total);

	return ((int) distance);
}

void
new_position(x1, y1, bearing, velocity)
	 double          x1,
	                 y1,
	                 bearing,
	                 velocity;
{
	double          delta_x,
	                delta_y;
	double          distance,
	                radians;

	/*
	 * Object two in which quadrant compared to object one? 0 x = opp, y = 
	 * ajd + 0 degrees 1 x = adj, y = opp + 90 degrees 2 x = opp, y = ajd
	 * + 180 degrees 3 x = adj, y = opp + 270 degrees 
	 */

	/*
	 * sin finds opp, cos finds adj 
	 */

	if (bearing < 91) {
		radians = bearing * 2.0 * M_PI / 360.;
		delta_x = velocity * sin(radians);
		delta_y = velocity * cos(radians);
		new_x = x1 + delta_x;
		new_y = y1 + delta_y;
	} else if (bearing < 181) {
		bearing -= 90;
		radians = bearing * 2.0 * M_PI / 360.;
		delta_y = velocity * sin(radians);
		delta_x = velocity * cos(radians);
		new_x = x1 + delta_x;
		new_y = y1 - delta_y;
	} else if (bearing < 271) {
		bearing -= 180;
		radians = bearing * 2.0 * M_PI / 360.;
		delta_x = velocity * sin(radians);
		delta_y = velocity * cos(radians);
		new_x = x1 - delta_x;
		new_y = y1 - delta_y;
	} else {
		bearing -= 270;
		radians = bearing * 2.0 * M_PI / 360.;
		delta_y = velocity * sin(radians);
		delta_x = velocity * cos(radians);
		new_x = x1 - delta_x;
		new_y = y1 + delta_y;
	}
}

int
bearing(x1, y1, x2, y2)
	 double          x1,
	                 y1,
	                 x2,
	                 y2;
{
	int             quadrant;
	double          delta_x,
	                delta_y;
	double          oppoadj;
	double          bearing;

	/*
	 * Object two in which quadrant compared to object one? 0 x = opp, y = 
	 * ajd + 0 degrees 1 x = adj, y = opp + 90 degrees 2 x = opp, y = ajd
	 * + 180 degrees 3 x = adj, y = opp + 270 degrees 
	 */

	if (x2 > x1) {
		delta_x = x2 - x1;
		if (y2 > y1) {
			quadrant = 0;
			delta_y = y2 - y1;
			oppoadj = delta_x / delta_y;
		} else {
			quadrant = 1;
			delta_y = y1 - y2;
			oppoadj = delta_y / delta_x;
		}
	} else {
		delta_x = x1 - x2;
		if (y2 > y1) {
			quadrant = 3;
			delta_y = y2 - y1;
			oppoadj = delta_y / delta_x;
		} else {
			quadrant = 2;
			delta_y = y1 - y2;
			oppoadj = delta_x / delta_y;
		}
	}

	bearing = atan(oppoadj);
	bearing = bearing / (2.0 * M_PI) * 360.;
	bearing = bearing + (90 * quadrant);

	return ((int) bearing);
}

void
set_arguments(function_call)
  char		function_call[];
{
	/* THIS FUNCTION CREATES AN ARRAY OF JACL INTEGER CONSTANTS TO
	   REPRESENT THE ARGUMENTS PASSED TO A JACL FUNCTION */
	int             index,
					counter,
	                length;
	int             position = 0; /* STORE THE INDEX OF THE WORD */
					/* SETTING new_word TO FALSE SKIPS THE FIRST */
					/* WORD WHICH IS THE FUNCTION NAME */
	int             new_word = FALSE;

	char 			*arg_ptr[MAX_WORDS];
	int 			arg_value[MAX_WORDS];

	struct integer_type *resolved_integer;
	struct cinteger_type *resolved_cinteger;

	/* SPLIT UP THE FUNCTION CALL STRING AND EXTRACT THE ARGUMENTS */
	length = strlen(function_call);

	for (index = 0; index < length; index++) {
		if (function_call[index] == '<') {
			function_call[index] = 0;
			new_word = TRUE;
		} else {
			if (new_word) {
				arg_ptr[position] = &function_call[index];
				new_word = FALSE;
				if (position < MAX_WORDS)
					position++;
			}
		}
	}

	/* CLEAR THE NEXT ARGUMENT POINTER */
	arg_ptr[position] = NULL;


	/* STORE THE VALUE OF EACH ARGUMENT PASSED*/
	index = 0;
	while (arg_ptr[index] != NULL) {
		//arg_value[index] = value_of(arg_ptr[index], TRUE);

		if ((resolved_integer = integer_resolve(arg_ptr[index])) != NULL) {
			arg_value[index] = resolved_integer->value;
		} else if ((resolved_cinteger = cinteger_resolve(arg_ptr[index])) != NULL) {
			arg_value[index] = resolved_cinteger->value;
		} else if (object_element_resolve(arg_ptr[index])) {
			arg_value[index] = oec;
		} else if ((counter = object_resolve(arg_ptr[index])) != -1) {
			if (counter < 1 || counter > objects) {
				badptrrun(arg_ptr[index], counter);
				pop_stack();
				return;
			} else {
				arg_value[index] = counter;
			}
		} else if (validate(arg_ptr[index])) {
			arg_value[index] = atoi(arg_ptr[index]);
		} else {
			arg_value[index] = -1;
		}

        index++;
	}

	/* THE CURRENT ARGUMENTS HAVE ALREADY BEEN PUSHED ONTO THE STACK */
	/* AND STORED IF PASSED AS AN ARGUMENT TO THIS FUNCTION SO IT IS 
	/* OKAY TO CLEAR THEM AND SET THE NEW VALUES */
	clear_cinteger("arg");
	clear_cstring("string_arg");

	/* CREATE A CONSTANT FOR EACH ARGUMENT AFTER THE CORE FUNCTION NAME */
	index = 0;
	while (arg_ptr[index] != NULL) {
		if (index == 0) noun[3] = arg_value[index];
		add_cinteger ("arg", arg_value[index]);
		add_cstring ("string_arg", arg_text_of(arg_ptr[index]));
        index++;
	}
}

void
pop_stack()
{
	int index, counter;

	stack--;

	clear_cinteger ("arg");
	clear_cinteger ("integer");
	clear_cstring ("$string");
	clear_cstring ("string_arg");
	clear_cstring ("$word");


	/* RECREATE THE arg ARRAY FOR THIS STACK FRAME */
	for (index = 0; index < backup[stack].argcount; index++) {
		if (index == 0) noun[3] = backup[stack].arguments[0];
		add_cinteger ("arg", backup[stack].arguments[index]);
	}

    /* RECREATE THE string_arg ARRAY FOR THIS STACK FRAME */
    for (index = 0; index < backup[stack].argcount; index++) {
        add_cstring ("string_arg", backup[stack].str_arguments[index]);
    }

	/* RECREATE THE integer ARRAY FOR THIS STACK FRAME */
	for (index = 0; index < backup[stack].integercount; index++) {
		add_cinteger ("integer", backup[stack].integer[index]);
	}

	/* RECREATE THE text ARRAY FOR THIS STACK FRAME */
	for (index = 0; index < backup[stack].textcount; index++) {
		add_cstring ("$string", backup[stack].text[index]);
	}

    /* RECREATE THE $word ARRAY FOR THIS STACK FRAME */
	for (index = 0; index < backup[stack].commandcount; index++) {
        add_cstring ("$word", backup[stack].command[index]);
    }

	/* RESTORE THE CONTENTS OF text_buffer */
	for (counter = 0; counter < 256; counter++)
		text_buffer[counter] = backup[stack].text_buffer[counter];

	/* RESTORE THE STORED FUNCTION NAMES THAT ARE USED WHEN AN
	 * 'override' COMMAND IS ENCOUNTERED IN THE CURRENT FUNCTION */
	strcpy (override, backup[stack].override);
	strcpy (default_function, backup[stack].default_function);

	/* RESTORE ALL THE WORD POINTERS */
	for (counter = 0; counter < MAX_WORDS; counter++)
		word[counter] = backup[stack].word[counter];

	/* RESTORE ALL THE NOUN POINTERS */
	for (counter = 0; counter < 4; counter++)
		noun[counter] = backup[stack].object_pointers[counter];

	/* PUSH ALL THE RESOLVED OBJECTS ONTO THE STACK */
	for (index = 0; index < 4; index++) {
		list_size[index] = backup[stack].list_size[index];
		max_size[index] = backup[stack].max_size[index];
		for (counter = 0; counter < max_size[index]; counter++) {
			object_list[index][counter] = 
				backup[stack].object_list[index][counter];
		}
	}

	executing_function = backup[stack].function;

	if (executing_function != NULL)
		strcpy (function_name, executing_function->name);

	wp = backup[stack].wp;

	glk_stream_set_position(game_stream, backup[stack].address, seekmode_Start);

	if (debug) {
		sprintf (error_buffer, "Returning to function \"%s\".\n", executing_function->name);	
		log_debug(error_buffer, PLUS_STDOUT);
	}
}

void
push_stack(file_pointer)
	 glsi32          file_pointer;
{
	/* COPY ALL THE CURRENT SYSTEM DATA ONTO THE STACK */
	int index;
	int	counter = 0;
	int	command = 0;
	int text = 0;
	int sarg = 0;
	int integers = 0;
    int arguments = 0;

	struct cinteger_type *cinteger_pointer = cinteger_table;
	struct string_type *cstring_pointer = cstring_table;

	if (stack == STACK_SIZE) {
		log_error("Stack overflow.", PLUS_STDERR);
		terminate(45);
	} else {
		backup[stack].function = executing_function;
		backup[stack].address = file_pointer;
		backup[stack].wp = wp;
		
		/* MAKE A COPY OF THE CURRENT CONTENTS OF text_buffer */
		for (counter = 0; counter < 256; counter++)
			backup[stack].text_buffer[counter] = text_buffer[counter];

		/* COPY THE STORED FUNCTION NAMES THAT ARE USED WHEN AN
		 * 'override' COMMAND IS ENCOUNTERED IN THE CURRENT FUNCTION */
		strcpy (backup[stack].override, override);
		strcpy (backup[stack].default_function, default_function);

		/* PUSH ALL THE WORD POINTERS ONTO THE STACK */
		for (counter = 0; counter < MAX_WORDS; counter++)
			backup[stack].word[counter] = word[counter];

		/* PUSH ALL THE OBJECT POINTERS ONTO THE STACK */
		for (counter = 0; counter < 4; counter++)
			backup[stack].object_pointers[counter] = noun[counter];

		/* PUSH ALL THE RESOLVED OBJECTS ONTO THE STACK */
		for (index = 0; index < 4; index++) {
			for (counter = 0; counter < max_size[index]; counter++) {
				backup[stack].object_list[index][counter] 
					=	object_list[index][counter];
			}
			backup[stack].list_size[index] = list_size[index];
			backup[stack].max_size[index] = max_size[index];
		}

		/* PUSH ALL THE CURRENT ARGUMENTS AND COMMAND INTEGERS ONTO THE STACK */
		integers = 0;
		arguments = 0;


        if (cinteger_pointer != NULL) {
            do {
                if (!strcmp(cinteger_pointer->name, "arg")) {
                    backup[stack].arguments[arguments++] = cinteger_pointer->value;
                } else if (!strcmp(cinteger_pointer->name, "integer")) {
                    backup[stack].integer[integers++] = cinteger_pointer->value;                }
                cinteger_pointer = cinteger_pointer->next_cinteger;
            }
            while (cinteger_pointer != NULL);
        }

		backup[stack].argcount = arguments;
		backup[stack].integercount = integers;

        /* PUSH ALL THE TEXT STRING SUPPLIED BY THE CURRENT COMMAND
           ONTO THE STACK */
        text = 0;
        sarg = 0;
		counter = 0;
		command = 0;

        if (cstring_pointer != NULL) {
            do {
                if (!strcmp(cstring_pointer->name, "$string")) {
                    strncpy((char *) &backup[stack].text[text++], cstring_pointer->value, 255);
                    backup[stack].text[counter++][255] = 0;
                } else if (!strcmp(cstring_pointer->name, "string_arg")) {
                    strncpy((char *) &backup[stack].str_arguments[sarg++], cstring_pointer->value, 255);
                } else if (!strcmp(cstring_pointer->name, "$word")) {
                    strncpy((char *) &backup[stack].command[command++], cstring_pointer->value, 255);
                }

                cstring_pointer = cstring_pointer->next_string;
            }
            while (cstring_pointer != NULL);
        }

		backup[stack].textcount = counter;
		backup[stack].commandcount = command;

	}
	stack++;
}

int
condition()
{
	/* COMPARE GROUPS OF TWO ELEMENTS. RETURN TRUE IF ANY ONE GROUP OF 
	 * ELEMENTS COMPARE 'TRUE' */
	int             first;

	first = 1;

	while (word[first + 2] != NULL && ((first +2) < MAX_WORDS)) {
		if (logic_test(first))
			return (TRUE);
		else
			first = first + 3;
	}
	return (FALSE);
}

int
and_condition()
{
	/* COMPARE GROUPS OF TWO ELEMENTS. RETURN FALSE IF ANY ONE GROUP OF 
	 * ELEMENTS COMPARE 'FALSE' */
	int             first;

	first = 1;

	while (word[first + 2] != NULL && ((first +2) < MAX_WORDS)) {
		if (logic_test(first) == FALSE)
			return (FALSE);
		else
			first = first + 3;
	}
	return (TRUE);
}

int
logic_test(first)
	 int             first;
{
	long            index,
	                compare;

	struct integer_type *resolved_integer;

	resolved_attribute = FALSE;

	index = value_of(word[first], TRUE);
	compare = value_of(word[first + 2], TRUE);

	if (!strcmp(word[first + 1], "=") || !strcmp(word[first + 1], "==")) {
		if (index == compare)
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], ">")) {
		if (index > compare)
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], "<")) {
		if (index < compare)
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], "is")) {
		if (index < 1 || index > objects) {
			unkobjrun(first);
			return (FALSE);
		} else
			return (scope(index, word[first + 2]));
	} else if (!strcmp(word[first + 1], "isnt")) {
		if (index < 1 || index > objects) {
			unkobjrun(first);
			return (FALSE);
		} else
			return (!scope(index, word[first + 2]));
	} else if (!strcmp(word[first + 1], "has"))
		if (index < 1 || index > objects) {
			unkobjrun(first);
			return (FALSE);
		} else {
			if (resolved_attribute == SYSTEM_ATTRIBUTE) {
				return (object[index]->attributes & compare);
			} else {
				return (object[index]->user_attributes & compare);
			}
		}
	else if (!strcmp(word[first + 1], "hasnt"))
		if (index < 1 || index > objects) {
			unkobjrun(first);
			return (FALSE);
		} else {
			if (resolved_attribute == SYSTEM_ATTRIBUTE) {
				return (!(object[index]->attributes & compare));
			} else {
				return (!(object[index]->user_attributes & compare));
			}
		}
	else if (!strcmp(word[first + 1], "!=")
			 || !strcmp(word[first + 1], "<>")) {
		if (index != compare)
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], ">=")
			   || !strcmp(word[first + 1], "=>")) {
		if (index >= compare)
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], "<=")
			   || !strcmp(word[first + 1], "=<")) {
		if (index <= compare)
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], "grandof")) {
		/* GRANDOF SAYS THAT AN OBJECT IS THE EVENTUAL PARENT OF ANOTHER OBJECT, NOT
		 * NECESSARILY IMMEDIATE */
		if (index < 1 || index > objects) {
			unkobjrun(first);
			return (FALSE);
		} else {
			if (compare < 1 || compare > objects) {
				unkobjrun(first + 2);
				return (FALSE);
			} else {
				if (parent_of(index, compare, UNRESTRICT))
					return (TRUE);
				else
					return (FALSE);
			}
		}
	} else if (!strcmp(word[first + 1], "!grandof")) {
		if (index < 1 || index > objects) {
			unkobjrun(first);
			return (FALSE);
		} else {
			if (compare < 1 || compare > objects) {
				unkobjrun(first + 2);
				return (FALSE);
			} else {
				if (parent_of(index, compare, UNRESTRICT))
					return (FALSE);
				else
					return (TRUE);
			}
		}
	} else {
		sprintf(error_buffer,
				"ERROR: In function \"%s\", illegal operator \"%s\".^",
				executing_function->name, word[2]);
		write_text(error_buffer);
		return (FALSE);
	}
}

int
strcondition()
{
	int             first;

	first = 1;

	while (word[first + 2] != NULL && ((first +2) < MAX_WORDS)) {
		if (str_test(first))
			return (TRUE);
		else
			first = first + 3;
	}
	return (FALSE);
}

int
str_test(first)
	 int             first;
{
	char           *index,
	               *compare;

	index = (char *) text_of(word[first]);
	compare = (char *) text_of(word[first + 2]);

	if (!strcmp(word[first + 1], "==") || !strcmp(word[first + 1], "=")) {
		if (!strcmp(index, compare))
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], "!contains")) {
		if (strstr(index, compare))
			return (FALSE);
		else
			return (TRUE);
	} else if (!strcmp(word[first + 1], "contains")) {
		if (strstr(index, compare))
			return (TRUE);
		else
			return (FALSE);
	} else if (!strcmp(word[first + 1], "<>")
			   || !strcmp(word[first + 1], "!=")) {
		if (strcmp(index, compare))
			return (TRUE);
		else
			return (FALSE);
	} else {
		sprintf(error_buffer,
				"ERROR: In function \"%s\", illegal operator \"%s\".^",
				executing_function->name, word[2]);
		write_text(error_buffer);
		return (FALSE);
	}
}

void
add_cinteger(name, value)
  char *name;
  int   value;
{
	/* THIS FUNCTION ADDS A NEW JACL CONSTANT TO THE LIST */

	struct cinteger_type *current_cinteger;
	struct cinteger_type *new_cinteger;

	if ((new_cinteger = (struct cinteger_type *)
		 malloc(sizeof(struct cinteger_type))) == NULL)
		outofmem();
	else {
		if (cinteger_table == NULL) {
			cinteger_table = new_cinteger;
		} else {
			/* FIND LAST CONSTANT IN LIST */	
			current_cinteger = cinteger_table;
			while (current_cinteger->next_cinteger != NULL) {
				current_cinteger = current_cinteger->next_cinteger;
			}
			current_cinteger->next_cinteger = new_cinteger;
		}
		strncpy(new_cinteger->name, name, 40);
		new_cinteger->name[40] = 0;
		new_cinteger->value = value;
		new_cinteger->next_cinteger = NULL;
	}
}

void
clear_cinteger(name)
  char *name;
{
    /* FREE CONSTANTS THAT HAVE SUPPLIED NAME*/

	struct cinteger_type *current_cinteger;
	struct cinteger_type *previous_cinteger;

	if (cinteger_table != NULL) {
		current_cinteger = cinteger_table;
		previous_cinteger = cinteger_table;
		while (current_cinteger != NULL) {
			if (!strcmp(current_cinteger->name, name)) {
				/* FREE THIS CONSTANT */
				if (previous_cinteger == current_cinteger) {
					cinteger_table = current_cinteger->next_cinteger;
					previous_cinteger = current_cinteger->next_cinteger;
					free(current_cinteger);
					current_cinteger = previous_cinteger;
				} else {
					previous_cinteger->next_cinteger = current_cinteger->next_cinteger;
					free(current_cinteger);
					current_cinteger = previous_cinteger->next_cinteger;
				}
			} else {
				previous_cinteger = current_cinteger;
				current_cinteger = current_cinteger->next_cinteger;
			}
		}
	}
}

void
add_cstring(name, value)
  char *name;
  char *value;
{
	/* ADD A STRING CONSTANT WITH THE SUPPLIED NAME AND VALUE */
	struct string_type *current_string;
	struct string_type *new_string;

	if ((new_string = (struct string_type *)
		 malloc(sizeof(struct string_type))) == NULL)
		outofmem();
	else {
		if (cstring_table == NULL) {
			cstring_table = new_string;
		} else {
			/* FIND LAST STRING IN LIST */	
			current_string = cstring_table;
			while (current_string->next_string != NULL) {
				current_string = current_string->next_string;
			}
			current_string->next_string = new_string;
		}
		strncpy(new_string->name, name, 40);
		new_string->name[40] = 0;
		strncpy(new_string->value, value, 255);
		new_string->value[255] = 0;
		new_string->next_string = NULL;
	}
}

void
clear_cstring(name)
  char *name;
{
	struct string_type *current_string;
	struct string_type *previous_string;

    /* FREE CONSTANTS THAT HAVE SUPPLIED NAME*/
	if (cstring_table != NULL) {
		current_string = cstring_table;
		previous_string = cstring_table;
		while (current_string != NULL) {
			if (!strcmp(current_string->name, name)) {
				/* FREE THIS STRING */
				if (previous_string == current_string) {
					cstring_table = current_string->next_string;
					previous_string = current_string->next_string;
					free(current_string);
					current_string = previous_string;
				} else {
					previous_string->next_string = current_string->next_string;
					free(current_string);
					current_string = previous_string->next_string;
				}
			} else {
				previous_string = current_string;
				current_string = current_string->next_string;
			}
		}
	}
}

void
inspect (object_num) 
	int		object_num;
{
	/* THIS FUNCTION DISPLAYS THE STATE OF A JACL OBJECT FOR DEBUGGING */

	int index, attribute_value;

	struct attribute_type *pointer = attribute_table;

	if (object_num < 1 || object_num > objects) {
		badptrrun(word[1], object_num);
		return;
	}

	write_text("label: ");
	write_text(object[object_num]->label);

	if (object[object_num]->attributes & LOCATION) {
		/* OUTPUT ALL THE ATTRIBUTES WITH LOCATION ATTRIBUTE TEXT */
		write_text("^has location attributes: ");
		index = 0;
		attribute_value = 1;
		while (location_attributes[index] != NULL) {
			if (object[object_num]->attributes & attribute_value) {
				write_text(location_attributes[index]);
			}
			index++;
			attribute_value *= 2;
		}
	} else {
		/* OUTPUT ALL THE ATTRIBUTES WITH OBJECT ATTRIBUTE TEXT */
		write_text("^has object attributes: ");
		index = 0;
		attribute_value = 1;
		while (object_attributes[index] != NULL) {
			if (object[object_num]->attributes & attribute_value) {
				write_text(object_attributes[index]);
			}
			index++;
			attribute_value *= 2;
		}

		write_text("^has user attributes: ");
		attribute_value = 1;
	}

	if (pointer != NULL) {
		/* THERE ARE USER ATTRIBUTES, SO CHECK IF THIS OBJECT OR LOCATION 
		 * HAS ANY OF THEM */
		do {
			if (object[object_num]->user_attributes & pointer->value) { 
				write_text(pointer->name);
				write_text(" ");
			}

			pointer = pointer->next_attribute;
		} while (pointer != NULL);
	}

	write_text("^");

	index = 0;
	if (object[object_num]->attributes & LOCATION) {
		while (location_elements[index] != NULL) {
			if (index < 12) {
				if (object[object_num]->integer[index] < 1 || object[object_num]->integer[index] > objects) {
					sprintf(temp_buffer, "%s: nowhere (%d)^", location_elements[index], object[object_num]->integer[index]);
				} else {
					sprintf(temp_buffer, "%s: %s (%d)^", location_elements[index], object[object[object_num]->integer[index]]->label, object[object_num]->integer[index]);
				}
			} else {
				sprintf(temp_buffer, "%s: %d^", location_elements[index], object[object_num]->integer[index]);
			}
			write_text(temp_buffer);
			index++;
		}
	} else {
		while (object_elements[index] != NULL) {
			if (index == 0) {
				sprintf(temp_buffer, "%s: %s (%d)^", object_elements[index], object[object[object_num]->integer[index]]->label, object[object_num]->integer[index]);
			} else {
				sprintf(temp_buffer, "%s: %d^", object_elements[index], object[object_num]->integer[index]);
			}
			write_text(temp_buffer);
			index++;
		}
	}
}

