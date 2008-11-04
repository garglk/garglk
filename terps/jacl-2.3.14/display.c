/* display.c --- Functions for language output.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include <string.h>

extern char						temp_buffer[];
extern char						function_name[];

extern struct object_type		*object[];
extern struct variable_type		*variable[];

extern char						*word[];

extern int						wp;
extern int						objects;
extern int						custom_error;

extern short int				spaced;

int
check_light(where)
	 int             where;
{
	int             index;

	if ((object[where]->attributes & DARK) == FALSE)
		return (TRUE);
	else {
		for (index = 1; index <= objects; index++) {
			if ((object[index]->attributes & LUMINOUS)
				&& scope(index, "*present"))
				return (TRUE);
		}
	}
	return (FALSE);
}

char *
sentence_output(index, capital)
	 int             index;
	 int             capital;
{
	if (!strcmp(object[index]->article, "name")) {
		strcpy(temp_buffer, object[index]->inventory);
	} else {
		strcpy(temp_buffer, object[index]->definite);
		strcat(temp_buffer, " ");
		strcat(temp_buffer, object[index]->inventory);
	}

	if (capital)
		temp_buffer[0] = toupper(temp_buffer[0]);

	return ((char *) &temp_buffer);
}

void
isnt_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		strcpy(temp_buffer, ARENT);
	else
		strcpy(temp_buffer, ISNT);
}

void
is_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		strcpy(temp_buffer, ARE);
	else
		strcpy(temp_buffer, IS);
}

void
it_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, THEM_WORD);
	} else if (object[index]->attributes & ANIMATE) {
		if (object[index]->attributes && FEMALE) {
			strcpy(temp_buffer, HER_WORD);
		} else {
			strcpy(temp_buffer, HIM_WORD);
		}
	} else {
		strcpy(temp_buffer, IT_WORD);
	}
}

void
that_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, THEY_WORD);
	} else if (object[index]->attributes & ANIMATE) {
		if (object[index]->attributes && FEMALE) {
			strcpy(temp_buffer, SHE_WORD);
		} else {
			strcpy(temp_buffer, HE_WORD);
		}
	} else {
		strcpy(temp_buffer, THAT_WORD);
	}
}

void
doesnt_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		strcpy(temp_buffer, DONT);
	else
		strcpy(temp_buffer, DOESNT);
}

void
does_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		strcpy(temp_buffer, DO);
	else
		strcpy(temp_buffer, DOES);
}

void
list_output(index, capital)
	 int             index;
	 int             capital;
{
	if (!strcmp(object[index]->article, "name")) {
		strcpy(temp_buffer, object[index]->inventory);
	} else {
		strcpy(temp_buffer, object[index]->article);
		strcat(temp_buffer, " ");
		strcat(temp_buffer, object[index]->inventory);
	}

	if (capital)
		temp_buffer[0] = toupper(temp_buffer[0]);
}

void
long_output(index)
	 int             index;
{
	if (!strcmp(object[index]->described, "function")) {
		strcpy(function_name, "long_");
		strcat(function_name, object[index]->label);
		if (execute(function_name) == FALSE) {
			unkfunrun(function_name);
		}
		temp_buffer[0] = 0;
	} else {
		strcpy(temp_buffer, object[index]->described);
	}
}

void
no_it()
{
	write_text(NO_IT);
	write_text(word[wp]);
	write_text(NO_IT_END);
	custom_error = TRUE;
}
