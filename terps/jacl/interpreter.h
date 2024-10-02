/* parser.h --- Parsing of player's commands
 * Created by C.W. Betts on 1/6/22.
 * (C) 2022 Stuart Allen, distribute and use
 * according to GNU GPL, see file COPYING for details.
 */

#ifndef interpreter_h
#define interpreter_h

#include "csv.h"
#include "types.h"

extern struct csv_parser parser_csv;

extern struct cinteger_type *current_cinteger;
extern struct string_type *current_cstring;
extern char         integer_buffer[16];
extern int          interrupted;
extern int          resolved_attribute;

#endif /* interpreter_h */
