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

void
log_error(message, console)
	char        *message;
	int			console;
{
	/* LOG A MESSAGE TO THE CONSOLE */

	char 			consoleMessage[256];

	sprintf(consoleMessage, "ERROR: %s^", message);

	glk_set_style(style_Alert);
	write_text(consoleMessage);
	glk_set_style(style_Normal);
}
