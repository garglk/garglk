/* parser.h --- Parsing of player's commands
 * Created by C.W. Betts on 1/7/22.
 * (C) 2022 Stuart Allen, distribute and use
 * according to GNU GPL, see file COPYING for details.
 */


#ifndef parser_h
#define parser_h

#include "constants.h"

extern int			it;
extern int			them[];
extern int			her;
extern int			him;
extern int			after_from;
extern int			last_exact;
extern int			custom_error;

extern char			default_function[];
extern int			oops_word;

extern int			object_list[4][MAX_OBJECTS];
extern int			list_size[];
extern int			max_size[];

#endif /* parser_h */
