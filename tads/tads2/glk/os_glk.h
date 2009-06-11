/*
** os_glk.h - Definitions which are specific to the Glk 'platform'
**
** Notes:
**   10 Jan 99: Initial creation
*/

#ifndef OS_GLK_INCLUDED
#define OS_GLK_INCLUDED

/* ------------------------------------------------------------------------ */
#include "glk.h"
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>

/* If you have type conflicts (i.e. uint, ulong, and ushort are already
   defined) then #define OS_TYPES_DEFINED */
#ifdef OS_TYPES_DEFINED
# define OS_USHORT_DEFINED
# define OS_UINT_DEFINED
# define OS_ULONG_DEFINED
#endif

/* Who we are (for oem.h) */
#define TADS_OEM_NAME "Stephen Granade <stephen@granades.com>"

/*
** By default, I have made the buffer sizes big. If this is a problem,
** by all means adjust these values.
*/
#define TRD_SETTINGS_DEFINED
#define TRD_HEAPSIZ  65535
#define TRD_STKSIZ   512
#define TRD_UNDOSIZ  60000

/*
** Since the default buffer sizes have changed, I need to change the
** usage strings to print the new defaults.
*/
#define TRD_HEAPSIZ_MSG "  -mh size      heap size (default 65535 bytes)"
#define TRD_STKSIZ_MSG  "  -ms size      stack size (default 512 elements)"
#define TRD_UNDOSIZ_MSG \
        "  -u size       set undo size (0 to disable; default 60000)"


/* maximum width (in characters) of a line of text */
#define OS_MAXWIDTH  255

/* By default, don't use a swapfile */
#define OS_DEFAULT_SWAP_ENABLED 0

/* Replace stricmp with strcasecmp */
#define strnicmp strncasecmp
#define stricmp strcasecmp

/* The non-standard name of the runtime must be reflected in the usage info */
#define OS_TR_USAGE  "usage: tadsr [options] file"

/* far pointer type qualifier (null on most platforms) */
#define osfar_t
#define far

/* maximum theoretical size of malloc argument */
#define OSMALMAX ((size_t)0xffffffff)

/* cast an expression to void */
#define DISCARD (void)

/*
** Some machines are missing memmove, so we use our own memcpy/memmove
** routine instead.
*/
void *our_memcpy(void *dst, const void *src, size_t size);
#define memcpy our_memcpy
#define memmove our_memcpy
/* and, yes, zarf will slap me for that last #define */

/*
** Turn on/off a busy cursor. Under Glk, we don't handle this at all.
*/
#define os_csr_busy(show_as_busy)

/* 
** If long cache-manager macros are NOT allowed, define
** OS_MCM_NO_MACRO.  This forces certain cache manager operations to
** be functions, which results in substantial memory savings.
*/
#define OS_MCM_NO_MACRO

/* likewise for the error handling subsystem */
#define ERR_NO_MACRO

/*
** If error messages are to be included in the executable, define
** ERR_LINK_MESSAGES.  Otherwise, they'll be read from an external
** file that is to be opened with oserrop().
*/
#define ERR_LINK_MESSAGES

/* round a size to worst-case alignment boundary */
#define osrndsz(s) (((s)+3) & ~3)

/* round a pointer to worst-case alignment boundary */
#define osrndpt(p) ((uchar *)((((ulong)(p)) + 3) & ~3))

/* service macros for osrp2 etc. */
#define osc2u(p, i) ((uint)(((uchar *)(p))[i]))
#define osc2l(p, i) ((ulong)(((uchar *)(p))[i]))

/* read unaligned portable 2-byte value, returning int */
#define osrp2(p) (osc2u(p, 0) + (osc2u(p, 1) << 8))

/* write int to unaligned portable 2-byte value */
#define oswp2(p, i) ((((uchar *)(p))[1] = (i)>>8), (((uchar *)(p))[0] = (i)&255))

/* read unaligned portable 4-byte value, returning long */
#define osrp4(p) \
 (osc2l(p, 0) + (osc2l(p, 1) << 8) + (osc2l(p, 2) << 16) + (osc2l(p, 3) << 24))

/* write long to unaligned portable 4-byte value */
#define oswp4(p, i) \
 ((((uchar *)(p))[0] = (i)), (((uchar *)(p))[1] = (i)>>8),\
  (((uchar *)(p))[2] = (i)>>16, (((uchar *)(p))[3] = (i)>>24)))

/* allocate storage - malloc where supported */
/* void *osmalloc(size_t siz); */
#define osmalloc malloc

/* free storage allocated with osmalloc */
/* void osfree(void *block); */
#define osfree free

/* copy a structure - dst and src are structures, not pointers */
#define OSCPYSTRUCT(dst, src) ((dst) = (src))

/* maximum length of a filename */
#define OSFNMAX 1024

/* normal path separator character */
#define OSPATHCHAR '/'

/* alternate path separator characters */
#define OSPATHALT "/"

/* URL path separator */
#define OSPATHURL "/"

/* character to separate paths from one another */
#define OSPATHSEP ':'

/* os file structure */
typedef struct glk_stream_struct osfildef;

/* main program exit codes */
#define OSEXSUCC 0                                 /* successful completion */
#define OSEXFAIL 1                                        /* error occurred */

/* Our 'OS' name */
#define OS_SYSTEM_NAME "Glk"

/*
** File handling
*/

/* open text file for reading; returns NULL on error */
osfildef *osfoprt(char *fname, glui32 typ);

/* open text file for 'volatile' (deny none) reading; returns NULL on error */
#define osfoprtv(fname, typ) osfoprt(fname, typ)

/* open text file for writing; returns NULL on error */
osfildef *osfopwt(char *fname, glui32 typ);

/* open text file for reading/writing; don't truncate */
osfildef *osfoprwt(char *fname, glui32 typ);

/* open text file for reading/writing; truncate; returns NULL on error */
osfildef *osfoprwtt(char *fname, glui32 typ);

/* open binary file for writing; returns NULL on error */
osfildef *osfopwb(char *fname, glui32 typ);

/* open source file for reading */
osfildef *osfoprs(char *fname, glui32 typ);

/* open binary file for reading; returns NULL on erorr */
osfildef *osfoprb(char *fname, glui32 typ);

/* open binary file for 'volatile' reading; returns NULL on erorr */
#define osfoprbv(fname, typ) osfoprb(fname, typ)

/* get a line of text from a text file (fgets semantics) */
/*char *osfgets(char *buf, size_t len, osfildef *fp);*/
#define osfgets(buf, len, fp) glk_get_line_stream(fp, buf, (glui32)len)

/* open binary file for reading/writing; don't truncate */
osfildef *osfoprwb(char *fname, glui32 typ);

/* open binary file for reading/writing; truncate; returns NULL on error */
osfildef *osfoprwtb(char *fname, glui32 typ);

/* write bytes to file; TRUE ==> error */
/*int osfwb(osfildef *fp, uchar *buf, int bufl);*/
/*#define osfwb(fp, buf, bufl) (glk_put_buffer_stream(fp, buf, (glui32)bufl) == TRUE) [TK fix me when zarf updates glk] */
int osfwb(osfildef *fp, unsigned char *buf, int bufl);

/* read bytes from file; TRUE ==> error */
/*int osfrb(osfildef *fp, uchar *buf, int bufl);*/
#define osfrb(fp, buf, bufl) (glk_get_buffer_stream(fp, buf, (glui32)bufl) != (glui32)bufl)

/* read bytes from file and return count; returns # bytes read, 0=error */
/*size_t osfrbc(osfildef *fp, uchar *buf, size_t bufl);*/
#define osfrbc(fp, buf, bufl) (glk_get_buffer_stream(fp, buf, (glui32)bufl))

/* get position in file */
/*long osfpos(osfildef *fp);*/
#define osfpos(fp) (glk_stream_get_position(fp))

/* seek position in file; TRUE ==> error */
/*int osfseek(osfildef *fp, long pos, int mode);*/
/*#define osfseek(fp, pos, mode) (glk_stream_set_position(fp, (glsi32)pos, mode)) [TK fix me too!] */
int osfseek(osfildef *pf, long pos, int mode);

#define OSFSK_SET  seekmode_Start
#define OSFSK_CUR  seekmode_Current
#define OSFSK_END  seekmode_End

/* close a file */
/*void osfcls(osfildef *fp);*/
#define osfcls(fp) (glk_stream_close(fp, NULL))

/* delete a file - TRUE if error */
int osfdel(char *fname);

/* access a file - 0 if file exists */
int osfacc(const char *fname);

/* get a character from a file */
/*int osfgetc(osfildef *fp);*/
#define osfgetc(fp) (glk_get_char_stream(fp))

/* write a string to a file */
/*void osfputs(char *buf, osfildef *fp);*/
/* #define osfputs(buf, fp) (glk_put_string_stream(fp, buf)) TK et moi! */
int osfputs(char *buf, osfildef *fp);

/* ignore OS_LOADDS definitions */
# define OS_LOADDS

/* yield CPU; returns TRUE if user requested interrupt, FALSE to continue */
int os_yield(void);

/* We don't worry about setting a time zone, so zero this function */
#define os_tzset()

/* 
** Update progress display with linfdef info, if appropriate.  This can
** be used to provide a status display during compilation.  Most
** command-line implementations will just ignore this notification; this
** can be used for GUI compiler implementations to provide regular
** display updates during compilation to show the progress so far.  
*/
#define os_progress(linf)

/*
** Macros which match single/double quote characters. Some systems have
** strange quote characters (such as the Mac); modify these as needed
** for those systems.
*/
#define os_squote(c) ((c) == '\'')
#define os_dquote(c) ((c) == '"')
#define os_qmatch(a, b) ((a) == (b))

/*
** Options for this operating system
*/
/*#define USE_EXPAUSE*/

#define RUNTIME

#define OS_GETS_OK       0
#define OS_GETS_EOF      1
#define OS_GETS_TIMEOUT  2


/* ------------------------------------------------------------------------ */
/*
**   If the system "long description" (for the banner) isn't defined,
**   make it the same as the platform ID string.  
*/
#ifndef OS_SYSTEM_LDESC
# define OS_SYSTEM_LDESC  OS_SYSTEM_NAME
#endif

/*
**   If a system patch sub-level isn't defined, define it here as zero.
**   The patch sub-level is used on some systems where a number of ports
**   are derived from a base port (for example, a large number of ports
**   are based on the generic Unix port).  For platforms like the Mac,
**   where the porting work applies only to that one platform, this
**   sub-level isn't meaningful.
*/
#ifndef OS_SYSTEM_PATCHSUBLVL
# define OS_SYSTEM_PATCHSUBLVL  "0"
#endif

/*
**   Ports can define a special TDB startup message, which is displayed
**   after the copyright/version banner.  If it's not defined at this
**   point, define it to an empty string. 
*/
#ifndef OS_TDB_STARTUP_MSG
# define OS_TDB_STARTUP_MSG ""
#endif


/* ------------------------------------------------------------------------ */
/*
** Include generic interface definitions for routines that must be
** implemented separately on each platform.
*/
#include "osifc.h"


/* This seems wacky, but it's the easiest way to turn off "[More]" etc. */
#define MAC_OS


#endif /* OS_GLK_INCLUDED */

