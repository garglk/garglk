/* parser.h --- Parsing of player's commands
 * Created by C.W. Betts on 1/7/22.
 * (C) 2022 Stuart Allen, distribute and use
 * according to GNU GPL, see file COPYING for details.
 */

#ifndef encapsulate_h
#define encapsulate_h

extern char			text_buffer[];
extern const char	*word[];
extern int			noun[];
extern int			quoted[];
extern int			percented[];
extern int			wp;

extern char			function_name[];
extern char			temp_buffer[];
extern char			error_buffer[];
extern char			proxy_buffer[];

#endif /* encapsulate_h */
