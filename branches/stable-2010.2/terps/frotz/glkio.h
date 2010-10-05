/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* glkio.h -- make stdio calls use glk i/o instead */

#undef FILE
typedef struct glk_stream_struct FILE;

#undef EOF
#define EOF				(-1)

#undef ungetc
#define ungetc(f,c)

#undef fclose
#define fclose(f)       (glk_stream_close(f, NULL), 0)
#undef ferror
#define ferror(f)       (0)     /* No ferror() equivalent */
#undef fgetc
#define fgetc(f)        (glk_get_char_stream(f))
#undef fgets
#define fgets(a, n, f)  (glk_get_line_stream(f, a, n))
#undef fread
#define fread(a,s,n,f)  (glk_get_buffer_stream(f, (char *)a, s*n))
#undef fwrite
#define fwrite(a,s,n,f)  (glk_put_buffer_stream(f, (char *)a, s*n), 0)
#undef fprintf
#define fprintf(f,s,a)  (glk_put_string_stream(f, a), 0)
#undef fputc
#define fputc(c, f)     (glk_put_char_stream(f, (unsigned char)(c)), 0)
#undef fputwc
#define fputwc(c, f)    (glk_put_char_stream_uni(f, (zchar)(c)), 0);
#undef fputs
#define fputs(s, f)     (glk_put_buffer_stream(f, s, strlen(s)), 0)
#undef ftell
#define ftell(f)        (glk_stream_get_position(f))
#undef fseek
#define fseek(f, p, m)  (glk_stream_set_position(f, p, m), 0)

#undef SEEK_SET
#define SEEK_SET        seekmode_Start
#undef SEEK_CUR
#define SEEK_CUR		seekmode_Current
#undef SEEK_END
#define SEEK_END        seekmode_End

FILE *frotzopenprompt(int flag);
FILE *frotzreopen(int flag);
FILE *frotzopen(char *filename, int flag);
