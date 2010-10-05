/* logging.c --- Logging functions
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "types.h"
#include "prototypes.h"
#include "language.h"

extern char						user_id[];
extern char						prefix[];

#ifdef __NDS__
void
log_error(message, console)
	char        *message;
	int			console;
{
	char 			consoleMessage[256];

	sprintf(consoleMessage, "ERROR: %s^", message);

	write_text(consoleMessage);
}
#endif
#ifdef GLK
void
log_error(message, console)
	char        *message;
	int			console;
{
	/* LOG A MESSAGE TO THE CONSOLE */

	char 			consoleMessage[256];
    event_t			event;

	// BUILD A STRING SUITABLE FOR DISPLAY ON THE CONSOLE.
	sprintf(consoleMessage, "ERROR: %s^", message);

	glk_set_style(style_Alert);
	write_text(consoleMessage);
	glk_set_style(style_Normal);

	// FLUSH THE GLK WINDOW SO THE ERROR GETS DISPLAYED IMMEDIATELY.
    glk_select_poll(&event);
}
#endif
#ifndef GLK
#ifndef __NDS__
extern char						error_log[];
extern char						access_log[];

void
log_access(message)
	 char           *message;
{
	/* LOG A MESSAGE TO THE ACCESS LOG */

	FILE           *accessLog = fopen(access_log, "a");

	if (accessLog != NULL) {
		fputs(message, accessLog);
		fflush(accessLog);
		fclose(accessLog);
	}
}

void
log_error(message, console)
	char        *message;
	int			console;
{
	FILE           *errorLog = fopen(error_log, "a");
	time_t          tnow;
	char 			consoleMessage[256];
	char 			temp_buffer[256];

	time(&tnow);
	/* MAKE THE LOG MESSAGE WITH TIME, USER_ID AND GAME PREFIX */
	sprintf(temp_buffer, "%s - %s - %s - %s\n", strip_return(ctime(&tnow)), user_id, prefix, message);

	/* MAKE THE MESSAGE FOR STDERR AND STDOUT WITH ERROR PREFIX */
	sprintf(consoleMessage, "%s: %s", prefix, message);

	if (console < ONLY_STDERR) {
		if (errorLog != NULL) {
			fputs(temp_buffer, errorLog);
			fflush(errorLog);
			fclose(errorLog);
		} 
	}

	/* SEND THE MESSAGE TO STANDARD ERROR OR STANDARD OUT AS REQUIRED */
	if (console == PLUS_STDOUT || console == ONLY_STDOUT) {
		write_text(consoleMessage);
		write_text("^");
	} else if (console == PLUS_STDERR || console == ONLY_STDERR) {
		fprintf(stderr, "%s", consoleMessage);
		fprintf(stderr, "%s", "\n");
		fflush(stderr);
	}
}

void
log_debug(message, console)
{
	log_message(message, console);
}

void
log_message(message, console)
	char        *message;
	int			console;
{
	FILE           *errorLog = fopen(error_log, "a");
	time_t          tnow;
	char 			consoleMessage[256];
	char 			temp_buffer[256];

	/* MAKE THE LOG MESSAGE WITH GAME PREFIX */
	sprintf(temp_buffer, "%s - %s - %s\n", strip_return(ctime(&tnow)), prefix, message);

	/* MAKE THE MESSAGE FOR STDERR AND STDOUT WITH ERROR PREFIX */
	sprintf(consoleMessage, "%s: %s\n", prefix, message);

	if (errorLog != NULL) {
		fputs(temp_buffer, errorLog);
		fflush(errorLog);
		fclose(errorLog);
	} 

	/* SEND THE MESSAGE TO STANDARD ERROR OR STANDARD OUT AS REQUIRED */
	if (console == 1) {
		write_text(message);
	} else if (console == 2) {
		fprintf(stderr, "%s", message);
		fflush(stderr);
	}
}
#endif
#endif
