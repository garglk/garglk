/* types.h --- Header file for all complex types used in JACL.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#ifndef __JACL_TYPES_H__
#define __JACL_TYPES_H__

#include "constants.h"

// THIS STRUCTURE CONTAINS ALL THE INFORMATION THAT NEEDS TO BE 
// SAVED IN ORDER TO CALL parse() RECURSIVELY
struct proxy_type {
    int				object_pointers[4];				// NOUN1 -> NOUN4
	int             integer[MAX_WORDS];				// THE COMANDS INTEGERS
	char			text[MAX_WORDS][256];			// THE COMANDS STRINGS
	char			command[MAX_WORDS][256];		// THE WHOLE COMMAND
	int				object_list[4][MAX_OBJECTS];	// THE RESOLVED OBJECTS
	int				list_size[4];					// THE SIZE OF THE ABOVE LISTS
	int				max_size[4];					// THE LAST USED INDEX OF THE ABOVE LISTS
	int				start_of_this_command;			// PREPARSE STATE
	int				start_of_last_command;			// PREPARSE STATE
	int				integercount;					// THE NUMBER OF INTEGERS SAVED
	int				textcount;						// THE NUMBER OF STRINGS SAVED
	int				commandcount;					// THE NUMBER OF WORDS IN COMMAND
	int				last_exact;						// WORD POINTER FOR MATCH
	int				after_from;						// WORD POINTER FOR FROM WORD
};

struct stack_type {
	FILE 			*infile, *outfile;
	int             arguments[MAX_WORDS];
    char			str_arguments[MAX_WORDS][256];
	char            text_buffer[1024];
	char            called_name[1024];
	char			override[84];
	char			scope_criterion[24];
	char			default_function[84];
	const char		*word[MAX_WORDS];
	int				quoted[MAX_WORDS];
	int				wp;
	int				argcount;
	int			local, local_x, local_y, local_a;
	int				*loop_integer;
	int				*select_integer;
	int				criterion_value;
	int				criterion_type;
	int				criterion_negate;
	int				current_level;
	int				execution_level;
#ifdef GLK
	glsi32 			top_of_loop;
	glsi32   		top_of_select;
	glsi32   		top_of_while;
	glsi32   		top_of_iterate;
	glsi32   		top_of_update;
	glsi32  		top_of_do_loop;
	glsi32          address;
#else
	long   			top_of_loop;
	long   			top_of_select;
	long   			top_of_while;
	long   			top_of_iterate;
	long   			top_of_update;
	long   			top_of_do_loop;
	long			address;
#endif
	struct function_type *function;
};

struct object_type {
	char            label[44];
	char            article[12];
	char            definite[12];
	struct			name_type *first_name;
	struct			name_type *first_plural;
	char            inventory[44];
	char            described[84];
	int				user_attributes;
	int				user_attributes_backup;
	int				attributes;
	int				attributes_backup;
	int             integer[16];
	int             integer_backup[16];
	int				nosave;
};

struct integer_type {
	char            name[44];
	int             value;
	int             value_backup;
	struct integer_type *next_integer;
};

struct cinteger_type {
	char            name[44];
	int             value;
	struct cinteger_type *next_cinteger;
};

struct attribute_type {
	char            name[44];
	int				value;
	struct attribute_type *next_attribute;
};

struct string_type {
	char            name[44];
	char            value[1025];
	struct string_type *next_string;
};

struct function_type {
	char            name[84];
#ifdef GLK
	glui32 			position;
#else
	long 			position;
#endif

	int             self;
    int				call_count;
    int				call_count_backup;
	struct function_type *next_function;
	int				nosave;
};

struct command_type {
    char            word[44];
    struct command_type *next;
};

#ifdef GLK
struct window_type {
	char            name[44];
	winid_t         glk_window;
	glui32			glk_type;
	struct window_type *next_window;
};
#endif

struct word_type {
	char            word[44];
	struct word_type *first_child;
	struct word_type *next_sibling;
};

struct synonym_type {
	char            original[44];
	char            standard[44];
	struct synonym_type *next_synonym;
};

struct name_type {
	char            name[44];
	struct name_type *next_name;
};

struct filter_type {
	char            word[44];
	struct filter_type *next_filter;
};

#ifndef GLK
struct parameter_type {
	char            name[44];
	char            container[44];
	int             low;
	int             high;
	struct parameter_type *next_parameter;
};
#endif

#endif
