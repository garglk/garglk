// $Id: git_unix.c,v 1.5 2004/01/25 18:44:51 iain Exp $

// unixstrt.c: Unix-specific code for Glulxe.
// Designed by Andrew Plotkin <erkyrath@eblong.com>
// http://www.eblong.com/zarf/glulx/index.html

#include "git.h"
#include <glk.h>
#include <glkstart.h> // This comes with the Glk library.

#ifdef USE_MMAP
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#endif

// The only command-line argument is the filename.
glkunix_argumentlist_t glkunix_arguments[] =
{
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

#define CACHE_SIZE (256 * 1024L)
#define UNDO_SIZE (2 * 1024 * 1024L)

#ifdef GARGLK

#include <string.h>

int gHasInited = 0;
char * gStartupError = 0;

void fatalError (const char * s)
{
	winid_t win;
	if (!gHasInited)
	{
		win = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
		glk_set_window(win);
	}
	/* pray that this goes somewhere reasonable... */
	glk_put_string("\n*** fatal error: ");
	glk_put_string((char*)s);
	glk_put_string(" ***\n");
	glk_exit();
}

#else

void fatalError (const char * s)
{
    fprintf (stderr, "*** fatal error: %s ***\n", s);
    exit (1);
}

#endif /* GARGLK */

#ifdef USE_MMAP
// Fast loader that uses some fancy Unix features.

const char * gFilename = 0;

int glkunix_startup_code(glkunix_startup_t *data)
{
#ifdef GARGLK
	{
		char buf[255];
		sprintf(buf, "Git %d.%d.%d", GIT_MAJOR, GIT_MINOR, GIT_PATCH);
		garglk_set_program_name(buf);
		sprintf(buf, "Git %d.%d.%d by Iain Merrick and David Kinder\n",
				GIT_MAJOR, GIT_MINOR, GIT_PATCH);
		garglk_set_program_info(buf);
	}
#endif /* GARGLK */

    if (data->argc <= 1)
    {
#ifdef GARGLK
        gStartupError = "No file given";
        return 1;
#else
        printf ("usage: git gamefile.ulx\n");
        return 0;
#endif
    }

#ifdef GARGLK
	{
		char *s;
		s = strrchr(data->argv[1], '\\');
		if (s) garglk_set_story_name(s+1);
		s = strrchr(data->argv[1], '/');
		if (s) garglk_set_story_name(s+1);
	}
#endif /* GARGLK */

    gFilename = data->argv[1];
    return 1;
}

void glk_main ()
{
    int          file;
    struct stat  info;
    const char * ptr;
    
#ifdef GARGLK
	if (gStartupError)
		fatalError(gStartupError);
#endif

    file = open (gFilename, O_RDONLY);
    if (file < 0)
        goto error;

    if (fstat (file, &info) != 0)
        goto error;
    
    if (info.st_size < 256)
    {
#ifdef GARGLK
        fatalError("This is too small to be a glulx file.");
#else
        fprintf (stderr, "This is too small to be a glulx file.\n");
        exit (1);
#endif
    }

    ptr = mmap (NULL, info.st_size, PROT_READ, MAP_PRIVATE, file, 0);
    if (ptr == MAP_FAILED)
        goto error;

#ifdef GARGLK
	gHasInited = 1;
#endif
        
    git (ptr, info.st_size, CACHE_SIZE, UNDO_SIZE);
    munmap ((void*) ptr, info.st_size);
    return;
    
error:
#ifdef GARGLK
    sprintf(errmsg, "git: %s", strerror(errno));
    fatalError(errmsg);
#else
    perror ("git");
    exit (errno);
#endif
}

#else
// Generic loader that should work anywhere.

strid_t gStream = 0;

int glkunix_startup_code(glkunix_startup_t *data)
{
#ifdef GARGLK
	{
		char buf[255];
		sprintf(buf, "Git %d.%d.%d", GIT_MAJOR, GIT_MINOR, GIT_PATCH);
		garglk_set_program_name(buf);
		sprintf(buf, "Git %d.%d.%d by Iain Merrick and David Kinder\n",
				GIT_MAJOR, GIT_MINOR, GIT_PATCH);
		garglk_set_program_info(buf);
	}
#endif /* GARGLK */

    if (data->argc <= 1)
    {
#ifdef GARGLK
        gStartupError = "No file given";
        return 1;
#else
        printf ("usage: git gamefile.ulx\n");
        return 0;
#endif
    }

#ifdef GARGLK
	{
		char *s;
		s = strrchr(data->argv[1], '\\');
		if (s) garglk_set_story_name(s+1);
		s = strrchr(data->argv[1], '/');
		if (s) garglk_set_story_name(s+1);
	}
#endif /* GARGLK */

    gStream = glkunix_stream_open_pathname ((char*) data->argv[1], 0, 0);
    return 1;
}

void glk_main ()
{
#ifdef GARGLK
	if (gStartupError)
		fatalError(gStartupError);
#endif

    if (gStream == NULL)
        fatalError ("could not open game file");

#ifdef GARGLK
    gHasInited = 1;
#endif

    gitWithStream (gStream, CACHE_SIZE, UNDO_SIZE);
}

#endif // USE_MMAP
