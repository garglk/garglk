#ifndef _GLKIO_H_
#define _GLKIO_H_

/*----------------------------------------------------------------------*\

  glkio.h

  Header file for Glk output for Alan interpreter

\*----------------------------------------------------------------------*/

#include "glk.h"

extern winid_t glkMainWin;
extern winid_t glkStatusWin;

/* NB: this header must be included in any file which calls printf() */

#define printf glkio_printf
void glkio_printf(char *, ...);

#ifdef MAP_STDIO_TO_GLK
#define fgetc(stream) glk_get_char_stream(stream)
#define rewind(stream) glk_stream_set_position(stream, 0, seekmode_Start);
#define fwrite(buf, elementSize, count, stream) glk_put_buffer_stream(stream, buf, elementSize*count);
#define fread(buf, elementSize, count, stream) glk_get_buffer_stream(stream, buf, elementSize*count);
#define fclose(stream) glk_stream_close(stream, NULL)
#define fgets(buff, len, stream) glk_get_line_stream(stream, buff, len)
#endif

#endif
