/* display.c --- Functions for language output.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"

extern char						temp_buffer[];
extern char						function_name[];

extern struct object_type		*object[];
extern struct variable_type		*variable[];

extern char						*word[];

extern int						player;
extern int						wp;
extern int						objects;
extern int						custom_error;

extern int						spaced;

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

	return (temp_buffer);
}

char *
isnt_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		return (cstring_resolve("ARENT")->value);
	else
		return (cstring_resolve("ISNT")->value);
}

char *
is_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		return (cstring_resolve("ARE")->value);
	else
		return (cstring_resolve("IS")->value);
}

char *
sub_output(index, capital)
	 int             index;
	 int             capital;
{
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, cstring_resolve("THEY_WORD")->value);
	} else {
		if (index == player) {
			strcpy(temp_buffer, cstring_resolve("YOU_WORD")->value);
		} else if (object[index]->attributes & ANIMATE) {
			if (object[index]->attributes & FEMALE) {
				strcpy(temp_buffer, cstring_resolve("SHE_WORD")->value);
			} else {
				strcpy(temp_buffer, cstring_resolve("HE_WORD")->value);
			}
		} else {
			strcpy(temp_buffer, cstring_resolve("IT_WORD")->value);
		}
	}

	if (capital)
		temp_buffer[0] = toupper(temp_buffer[0]);

	return temp_buffer;
}

char *
obj_output(index, capital)
	 int             index;
	 int             capital;
{
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, cstring_resolve("THEM_WORD")->value);
	} else {
		if (index == player) {
			strcpy(temp_buffer, cstring_resolve("YOURSELF_WORD")->value);
		} else if (object[index]->attributes & ANIMATE) {
			if (object[index]->attributes & FEMALE) {
				strcpy(temp_buffer, cstring_resolve("HER_WORD")->value);
			} else {
				strcpy(temp_buffer, cstring_resolve("HIM_WORD")->value);
			}
		} else {
			strcpy(temp_buffer, cstring_resolve("IT_WORD")->value);
		}
	}

	if (capital)
		temp_buffer[0] = toupper(temp_buffer[0]);

	return temp_buffer;
}

char *
it_output(index)
	 int             index;
{
	if (object[index]->attributes & ANIMATE) {
		return sentence_output(index, FALSE);
	} else {
		if (object[index]->attributes & PLURAL) {
			return (cstring_resolve("THEM_WORD")->value);
		} else {
			return (cstring_resolve("IT_WORD")->value);
		}
	}
}

char *
that_output(index, capital)
	 int             index;
	 int             capital;
{
	if (object[index]->attributes & PLURAL) {
		strcpy(temp_buffer, cstring_resolve("THOSE_WORD")->value);
	} else {
		strcpy(temp_buffer, cstring_resolve("THAT_WORD")->value);
	}

	if (capital)
		temp_buffer[0] = toupper(temp_buffer[0]);

	return temp_buffer;
}

char *
doesnt_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		return (cstring_resolve("DONT")->value);
	else
		return (cstring_resolve("DOESNT")->value);
}

char *
does_output(index)
	 int             index;
{
	if (object[index]->attributes & PLURAL)
		return (cstring_resolve("DO")->value);
	else
		return (cstring_resolve("DOES")->value);
}

char *
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

	return (temp_buffer);
}

char *
plain_output(index, capital)
	 int             index;
	 int             capital;
{
	strcpy(temp_buffer, object[index]->inventory);

	if (capital)
		temp_buffer[0] = toupper(temp_buffer[0]);

	return (temp_buffer);
}

char *
long_output(index)
	 int             index;
{
	if (!strcmp(object[index]->described, "function")) {
		strcpy(function_name, "long_");
		strcat(function_name, object[index]->label);
		if (execute(function_name) == FALSE) {
			unkfunrun(function_name);
		}

		// THE BUFFER IS RETURNED EMPTY AS THE TEXT IS OUTPUT BY
		// WRITE STATEMENTS IN THE FUNCTION CALLED
		temp_buffer[0] = 0;
		return (temp_buffer);
	} else {
		return (object[index]->described);
	}
}

void
no_it()
{
	write_text(cstring_resolve("NO_IT")->value);
	write_text(word[wp]);
	write_text(cstring_resolve("NO_IT_END")->value);
	custom_error = TRUE;
}

void
look_around()
{
	/* THIS FUNCTION DISPLAYS THE DESCRIPTION OF THE CURRENT LOCATION ALONG
	 * WITH ANY OBJECTS CURRENTLY IN IT */

	if (!check_light(HERE)) {
		/* THE CURRENT LOCATION HAS 'DARK' AND NO SOURCE OF LIGHT IS
		 * CURRENTLY PRESENT */
		execute("+dark_description");
		return;
	}

	if (execute("+before_look") != FALSE)
		return;

	execute("+title");

	if (DISPLAY_MODE->value) {
		/* THE INTERPRETER IS IN VERBOSE MODE SO TEMPORARILY TAKE AWAYS THE
		 * 'VISITED' ATTRIBUTE */
		object[HERE]->attributes &= ~1L;
	}

	strcpy(function_name, "look_");
	strcat(function_name, object[HERE]->label);
	execute(function_name);

	/* GIVE THE LOCATION THE ATTRIBUTES 'VISITED', 'KNOWN', AND 'MAPPED' NOW 
     * THAT THE LOOK FUNCTION HAS RUN */
	object[HERE]->attributes = object[HERE]->attributes | KNOWN;
	object[HERE]->attributes = object[HERE]->attributes | VISITED;
	object[HERE]->attributes = object[HERE]->attributes | MAPPED;

	execute("+object_descriptions");

	strcpy(function_name, "after_look_");
	strcat(function_name, object[HERE]->label);
	execute(function_name);

	execute("+after_look");
}

