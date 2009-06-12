/* errors.c --- Error messages.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"

extern struct function_type		*executing_function;
extern char           			*word[];

extern char						error_buffer[];

void
badparrun()
{
	sprintf(error_buffer, BAD_PARENT, executing_function->name);

	log_error(error_buffer, PLUS_STDERR);
}

void
notintrun()
{
	sprintf(error_buffer, NOT_INTEGER, executing_function->name, word[0]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkfunrun(name)
	 char           *name;
{
	sprintf(error_buffer, UNKNOWN_FUNCTION_RUN, name);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkkeyerr(line, wordno)
	 int             line;
	 int             wordno;
{
	sprintf(error_buffer, UNKNOWN_KEYWORD_ERR, line, word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkatterr(line, wordno)
	 int             line;
	 int             wordno;
{
	sprintf(error_buffer, UNKNOWN_ATTRIBUTE_ERR, line,
			word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkvalerr(line, wordno)
	 int             line;
	 int             wordno;
{
	sprintf(error_buffer, UNKNOWN_VALUE_ERR, line,
			word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
noproprun()
{
	sprintf(error_buffer, INSUFFICIENT_PARAMETERS_RUN, executing_function->name, word[0]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
noobjerr(line)
	 int             line;
{
	sprintf(error_buffer, NO_OBJECT_ERR,
			line, word[0]);
	log_error(error_buffer, PLUS_STDERR);
}

void
noproperr(line)
	 int             line;
{
	sprintf(error_buffer, INSUFFICIENT_PARAMETERS_ERR,
			line, word[0]);
	log_error(error_buffer, PLUS_STDERR);
}

void
nongloberr(line)
	 int             line;
{
	sprintf(error_buffer, NON_GLOBAL_FIRST, line);
	log_error(error_buffer, PLUS_STDERR);
}

void
nofnamerr(line)
	 int             line;
{
	sprintf(error_buffer, NO_NAME_FUNCTION, line);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkobjerr(line, wordno)
	 int             line;
	 int             wordno;
{
	sprintf(error_buffer, UNDEFINED_ITEM_ERR, line, word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
maxatterr(line, wordno)
	 int             line;
	 int             wordno;
{
	sprintf(error_buffer,
			MAXIMUM_ATTRIBUTES_ERR, line, word[wordno]);
	log_error(error_buffer, PLUS_STDERR);
}

void
unkobjrun(wordno)
	 int             wordno;
{
	sprintf(error_buffer, UNDEFINED_ITEM_RUN, executing_function->name, word[wordno]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkattrun(wordno)
	 int             wordno;
{
	sprintf(error_buffer, UNKNOWN_ATTRIBUTE_RUN, executing_function->name, word[wordno]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkdirrun(wordno)
	 int             wordno;
{
	sprintf(error_buffer, UNDEFINED_DIRECTION_RUN,
			executing_function->name, word[wordno]);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badparun()
{
	sprintf(error_buffer, BAD_PARENT, executing_function->name);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badplrrun(value)
	int			value;
{
	sprintf(error_buffer, BAD_PLAYER, executing_function->name, value);
	log_error(error_buffer, PLUS_STDOUT);
}

void
badptrrun(name, value)
	char       *name;
	int			value;
{
	sprintf(error_buffer, BAD_POINTER, executing_function->name, name, value);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkvarrun(variable)
	 char           *variable;
{
	sprintf(error_buffer, UNDEFINED_CONTAINER_RUN, executing_function->name, arg_text_of(variable));
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkstrrun(variable)
	 char           *variable;
{
	sprintf(error_buffer, UNDEFINED_STRING_RUN, executing_function->name, variable);
	log_error(error_buffer, PLUS_STDOUT);
}

void
unkscorun(scope)
	 char           *scope;
{
	sprintf(error_buffer, UNKNOWN_SCOPE_RUN, executing_function->name, scope);
	log_error(error_buffer, PLUS_STDOUT);
}

void
totalerrs(errors)
	int				errors;
{
	if (errors == 1)
		sprintf(error_buffer, ERROR_DETECTED);
	else {
		sprintf(error_buffer, ERRORS_DETECTED, errors);
	}

	log_error(error_buffer, PLUS_STDERR);
}

void
outofmem()
{
	log_error(OUT_OF_MEMORY, PLUS_STDERR);
	terminate(49);
}

