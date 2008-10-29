/*
** glkstart.h - The OS-specific header file for GlkTADS. If you port
**              GlkTADS to another Glk library, use #defines so that
**              all the code can be together in one big nasty file.
**
** Notes:
**   14 Jan 99: Initial creation
*/

#ifndef GT_START_H
# define GT_START_H

/* We define our own TRUE and FALSE and NULL, because ANSI
   is a strange world. */
# ifndef TRUE
#  define TRUE 1
# endif
# ifndef FALSE
#  define FALSE 0
# endif
# ifndef NULL
#  define NULL 0
# endif

# ifdef GLKUNIX

#  define glkunix_arg_End (0)
#  define glkunix_arg_ValueFollows (1)
#  define glkunix_arg_NoValue (2)
#  define glkunix_arg_ValueCanFollow (3)
#  define glkunix_arg_NumberValue (4)

typedef struct glkunix_argumentlist_struct {
    char *name;
    int argtype;
    char *desc;
} glkunix_argumentlist_t;

typedef struct glkunix_startup_struct {
    int argc;
    char **argv;
} glkunix_startup_t;

extern glkunix_argumentlist_t glkunix_arguments[];

extern int glkunix_startup_code(glkunix_startup_t *data);

extern strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, 
    glui32 rock);

# endif /* GLKUNIX */

#endif /* GT_START_H */

