/* types.h --- Header file for all complex types used in JACL.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include "constants.h"

struct stack_type {
	int             arguments[MAX_WORDS];
    int             str_arguments[MAX_WORDS][256];
	int             integer[MAX_WORDS];
	int             text[MAX_WORDS][256];
	int             command[MAX_WORDS][256];
    int				object_pointers[4];
	char            text_buffer[1024];
	char			override[81];
	char			default_function[81];
	char           *word[MAX_WORDS];
	int				wp;
	int				argcount;
	int				integercount;
	int				textcount;
	int				commandcount;
#ifdef GLK
	glsi32          address;
#else
	long			address;
#endif
	struct function_type *function;
	int				object_list[4][MAX_OBJECTS];
	int				list_size[4];
	int				max_size[4];
};

struct object_type {
	char            label[41];
	char            article[11];
	char            definite[11];
	struct			name_type *first_name;
	struct			name_type *first_plural;
	char            inventory[41];
	char            described[81];
	int				user_attributes;
	int				user_attributes_backup;
	int				attributes;
	int				attributes_backup;
	int             integer[16];
	int             integer_backup[16];
	int				nosave;
};

struct integer_type {
	char            name[41];
	int             value;
	int             value_backup;
	struct integer_type *next_integer;
};

struct cinteger_type {
	char            name[41];
	int             value;
	struct cinteger_type *next_cinteger;
};

struct attribute_type {
	char            name[41];
	int				value;
	struct attribute_type *next_attribute;
};

struct string_type {
	char            name[41];
	char            value[256];
	struct string_type *next_string;
};

struct function_type {
	char            name[81];
#ifdef GLK
	glui32 			position;
#else
	long 			position;
#endif

	int             self;
    int				call_count;
    int				call_count_backup;
	struct function_type *next_function;
};

struct command_type {
    char            word[41];
    struct command_type *next;
};

#ifdef GLK
struct window_type {
	char            name[41];
	winid_t         glk_window;
	glui32			glk_type;
	struct window_type *next_window;
};
#endif

struct word_type {
	char            word[41];
	struct word_type *first_child;
	struct word_type *next_sibling;
};

struct synonym_type {
	char            original[41];
	char            standard[41];
	struct synonym_type *next_synonym;
};

struct name_type {
	char            name[41];
	struct name_type *next_name;
};

struct filter_type {
	char            word[41];
	struct filter_type *next_filter;
};

#ifndef GLK
struct parameter_type {
	char            name[41];
	char            container[41];
	int             low;
	int             high;
	struct parameter_type *next_parameter;
};
#endif
