/* Adapted from src/osportable.cc from FrobTADS 1.2.3.
 * FrobTADS copyright (C) 2009 Nikos Chantziaras.
 */

/* This file implements some of the functions described in
 * tads2/osifc.h.  We don't need to implement them all, as most of them
 * are provided by tads2/osnoui.c and tads2/osgen3.c.
 *
 * This file implements the "portable" subset of these functions;
 * functions that depend upon curses/ncurses are defined in oscurses.cc.
 */
#if 0
#include "common.h"
#include "osstzprs.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#if 0
#include <sys/param.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif
#ifdef HAVE_GLOB_H
#include <glob.h>
#endif


#include "os.h"
#ifdef RUNTIME
// We utilize the Tads 3 Unicode character mapping facility even for
// Tads 2 (only in the interpreter).
#include "charmap.h"
// Need access to command line options: globalApp->options. Only at
// runtime - compiler uses different mechanism for command line
// options.
#include "frobtadsapp.h"
#endif // RUNTIME
#endif

#ifdef GARGOYLE
#include "osextra.h"
#include "charmap.h"
#include <time.h>
#include <dirent.h>
#include <limits.h>

#if defined(_WIN32)
#include <windows.h>
#define MKDIR_TAKES_ONE_ARG 1
#define lstat stat
#elif defined(__APPLE__)
#define HAVE_GETTIMEOFDAY
#include <sys/time.h>
#else
#define HAVE_CLOCK_GETTIME
#endif
#define HAVE_MKDIR 1
#define T3_RES_DIR 0
#define T3_INC_DIR 0
#define T3_LIB_DIR 0
#define T3_LOG_FILE 0
#endif /* GARGOYLE */


/* Safe strcpy.
 * (Copied from tads2/msdos/osdos.c)
 */
static void
safe_strcpy(char *dst, size_t dstlen, const char *src)
{
    size_t copylen;

    /* do nothing if there's no output buffer */
    if (dst == 0 || dstlen == 0)
        return;

    /* do nothing if the source and destination buffers are the same */
    if (dst == src)
        return;

    /* use an empty string if given a null string */
    if (src == 0)
        src = "";

    /* 
     *   figure the copy length - use the smaller of the actual string size
     *   or the available buffer size, minus one for the null terminator 
     */
    copylen = strlen(src);
    if (copylen > dstlen - 1)
        copylen = dstlen - 1;

    /* copy the string (or as much as we can) */
    memcpy(dst, src, copylen);

    /* null-terminate it */
    dst[copylen] = '\0';
}

#if 0
/* Service routine.
 * Try to bring returned by system charset name to a name of one of
 * the encodings in cmaplib.t3r.  The proper way to do it is to
 * maintain an external database of charset aliases, which user can
 * edit.  But this adds unnecessary complexity, which we really don't
 * need.  So, the "database" is internal.
 *
 * TODO: Restore previous bahavior when we fix out curses interface.
 * For now, we always return "us-ascii".
 */
static const char*
get_charset_alias( const char* charset )
{
    const char* p;
    const char* c;

    // Special case.
    if (!stricmp(charset, "ANSI_X3.4-1968")) return "us-ascii";

    // If it's in ru_RU.KOI8-R form, try to extract charset part
    // (everything after last point).

    // Find the last point in the charset name.
    for (p = charset, c = charset ; *p != '\0' ; ++p) {
        if (*p == '.') c = p + 1;
    }

    // '.' was the last symbol in charset name?
    if (*c == '\0') c = charset;

    if (!stricmp(c, "KOI8-R")) return "koi8-r";

    if (!stricmp(c, "ISO-8859-1"))  return "iso1";
    if (!stricmp(c, "ISO-8859-2"))  return "iso2";
    if (!stricmp(c, "ISO-8859-3"))  return "iso3";
    if (!stricmp(c, "ISO-8859-4"))  return "iso4";
    if (!stricmp(c, "ISO-8859-5"))  return "iso5";
    if (!stricmp(c, "ISO-8859-6"))  return "iso6";
    if (!stricmp(c, "ISO-8859-7"))  return "iso7";
    if (!stricmp(c, "ISO-8859-8"))  return "iso8";
    if (!stricmp(c, "ISO-8859-9"))  return "iso9";
    if (!stricmp(c, "ISO-8859-10")) return "iso10";
    if (!stricmp(c, "8859-1"))      return "iso1";
    if (!stricmp(c, "8859-2"))      return "iso2";
    if (!stricmp(c, "8859-3"))      return "iso3";
    if (!stricmp(c, "8859-4"))      return "iso4";
    if (!stricmp(c, "8859-5"))      return "iso5";
    if (!stricmp(c, "8859-6"))      return "iso6";
    if (!stricmp(c, "8859-7"))      return "iso7";
    if (!stricmp(c, "8859-8"))      return "iso8";
    if (!stricmp(c, "8859-9"))      return "iso9";
    if (!stricmp(c, "8859-10"))     return "iso10";

    if (!stricmp(c, "CP1250"))  return "cp1250";
    if (!stricmp(c, "CP-1250")) return "cp1250";
    if (!stricmp(c, "CP_1250")) return "cp1250";
    if (!stricmp(c, "CP1251"))  return "cp1251";
    if (!stricmp(c, "CP-1251")) return "cp1251";
    if (!stricmp(c, "CP_1251")) return "cp1251";
    if (!stricmp(c, "CP1252"))  return "cp1252";
    if (!stricmp(c, "CP-1252")) return "cp1252";
    if (!stricmp(c, "CP_1252")) return "cp1252";
    if (!stricmp(c, "CP1253"))  return "cp1253";
    if (!stricmp(c, "CP-1253")) return "cp1253";
    if (!stricmp(c, "CP_1253")) return "cp1253";
    if (!stricmp(c, "CP1254"))  return "cp1254";
    if (!stricmp(c, "CP-1254")) return "cp1254";
    if (!stricmp(c, "CP_1254")) return "cp1254";
    if (!stricmp(c, "CP1255"))  return "cp1255";
    if (!stricmp(c, "CP-1255")) return "cp1255";
    if (!stricmp(c, "CP_1255")) return "cp1255";
    if (!stricmp(c, "CP1256"))  return "cp1256";
    if (!stricmp(c, "CP-1256")) return "cp1256";
    if (!stricmp(c, "CP_1256")) return "cp1256";
    if (!stricmp(c, "CP1257"))  return "cp1257";
    if (!stricmp(c, "CP-1257")) return "cp1257";
    if (!stricmp(c, "CP_1257")) return "cp1257";
    if (!stricmp(c, "CP1258"))  return "cp1258";
    if (!stricmp(c, "CP-1258")) return "cp1258";
    if (!stricmp(c, "CP_1258")) return "cp1258";

    if (!stricmp(c, "CP437"))  return "cp437";
    if (!stricmp(c, "CP-437")) return "cp437";
    if (!stricmp(c, "CP_437")) return "cp437";
    if (!stricmp(c, "PC437"))  return "cp437";
    if (!stricmp(c, "PC-437")) return "cp437";
    if (!stricmp(c, "PC_437")) return "cp437";
    if (!stricmp(c, "CP737"))  return "cp737";
    if (!stricmp(c, "CP-737")) return "cp737";
    if (!stricmp(c, "CP_737")) return "cp737";
    if (!stricmp(c, "PC737"))  return "cp737";
    if (!stricmp(c, "PC-737")) return "cp737";
    if (!stricmp(c, "PC_737")) return "cp737";
    if (!stricmp(c, "CP775"))  return "cp775";
    if (!stricmp(c, "CP-775")) return "cp775";
    if (!stricmp(c, "CP_775")) return "cp775";
    if (!stricmp(c, "PC775"))  return "cp775";
    if (!stricmp(c, "PC-775")) return "cp775";
    if (!stricmp(c, "PC_775")) return "cp775";
    if (!stricmp(c, "CP850"))  return "cp850";
    if (!stricmp(c, "CP-850")) return "cp850";
    if (!stricmp(c, "CP_850")) return "cp850";
    if (!stricmp(c, "PC850"))  return "cp850";
    if (!stricmp(c, "PC-850")) return "cp850";
    if (!stricmp(c, "PC_850")) return "cp850";
    if (!stricmp(c, "CP852"))  return "cp852";
    if (!stricmp(c, "CP-852")) return "cp852";
    if (!stricmp(c, "CP_852")) return "cp852";
    if (!stricmp(c, "PC852"))  return "cp852";
    if (!stricmp(c, "PC-852")) return "cp852";
    if (!stricmp(c, "PC_852")) return "cp852";
    if (!stricmp(c, "CP855"))  return "cp855";
    if (!stricmp(c, "CP-855")) return "cp855";
    if (!stricmp(c, "CP_855")) return "cp855";
    if (!stricmp(c, "PC855"))  return "cp855";
    if (!stricmp(c, "PC-855")) return "cp855";
    if (!stricmp(c, "PC_855")) return "cp855";
    if (!stricmp(c, "CP857"))  return "cp857";
    if (!stricmp(c, "CP-857")) return "cp857";
    if (!stricmp(c, "CP_857")) return "cp857";
    if (!stricmp(c, "PC857"))  return "cp857";
    if (!stricmp(c, "PC-857")) return "cp857";
    if (!stricmp(c, "PC_857")) return "cp857";
    if (!stricmp(c, "CP860"))  return "cp860";
    if (!stricmp(c, "CP-860")) return "cp860";
    if (!stricmp(c, "CP_860")) return "cp860";
    if (!stricmp(c, "PC860"))  return "cp860";
    if (!stricmp(c, "PC-860")) return "cp860";
    if (!stricmp(c, "PC_860")) return "cp860";
    if (!stricmp(c, "CP861"))  return "cp861";
    if (!stricmp(c, "CP-861")) return "cp861";
    if (!stricmp(c, "CP_861")) return "cp861";
    if (!stricmp(c, "PC861"))  return "cp861";
    if (!stricmp(c, "PC-861")) return "cp861";
    if (!stricmp(c, "PC_861")) return "cp861";
    if (!stricmp(c, "CP862"))  return "cp862";
    if (!stricmp(c, "CP-862")) return "cp862";
    if (!stricmp(c, "CP_862")) return "cp862";
    if (!stricmp(c, "PC862"))  return "cp862";
    if (!stricmp(c, "PC-862")) return "cp862";
    if (!stricmp(c, "PC_862")) return "cp862";
    if (!stricmp(c, "CP863"))  return "cp863";
    if (!stricmp(c, "CP-863")) return "cp863";
    if (!stricmp(c, "CP_863")) return "cp863";
    if (!stricmp(c, "PC863"))  return "cp863";
    if (!stricmp(c, "PC-863")) return "cp863";
    if (!stricmp(c, "PC_863")) return "cp863";
    if (!stricmp(c, "CP864"))  return "cp864";
    if (!stricmp(c, "CP-864")) return "cp864";
    if (!stricmp(c, "CP_864")) return "cp864";
    if (!stricmp(c, "PC864"))  return "cp864";
    if (!stricmp(c, "PC-864")) return "cp864";
    if (!stricmp(c, "PC_864")) return "cp864";
    if (!stricmp(c, "CP865"))  return "cp865";
    if (!stricmp(c, "CP-865")) return "cp865";
    if (!stricmp(c, "CP_865")) return "cp865";
    if (!stricmp(c, "PC865"))  return "cp865";
    if (!stricmp(c, "PC-865")) return "cp865";
    if (!stricmp(c, "PC_865")) return "cp865";
    if (!stricmp(c, "CP866"))  return "cp866";
    if (!stricmp(c, "CP-866")) return "cp866";
    if (!stricmp(c, "CP_866")) return "cp866";
    if (!stricmp(c, "PC866"))  return "cp866";
    if (!stricmp(c, "PC-866")) return "cp866";
    if (!stricmp(c, "PC_866")) return "cp866";
    if (!stricmp(c, "CP869"))  return "cp869";
    if (!stricmp(c, "CP-869")) return "cp869";
    if (!stricmp(c, "CP_869")) return "cp869";
    if (!stricmp(c, "PC869"))  return "cp869";
    if (!stricmp(c, "PC-869")) return "cp869";
    if (!stricmp(c, "PC_869")) return "cp869";
    if (!stricmp(c, "CP874"))  return "cp874";
    if (!stricmp(c, "CP-874")) return "cp874";
    if (!stricmp(c, "CP_874")) return "cp874";
    if (!stricmp(c, "PC874"))  return "cp874";
    if (!stricmp(c, "PC-874")) return "cp874";
    if (!stricmp(c, "PC_874")) return "cp874";

    // There are Mac OS Roman, Central European, Cyrillic, Greek,
    // Icelandic and Turkish charmaps in cmaplib.t3r. But what codes
    // system supposed to return?

    // Unrecognized.
    return charset;
}
#endif

/* Open text file for reading and writing.
 */
osfildef*
osfoprwt( const char* fname, os_filetype_t )
{
//  assert(fname != 0);
    // Try opening the file in read/write mode.
    osfildef* fp = fopen(fname, "r+");
    // If opening the file failed, it probably means that it doesn't
    // exist.  In that case, create a new file in read/write mode.
    if (fp == 0) fp = fopen(fname, "w+");
    return fp;
}


/* Open binary file for reading/writing.
 */
osfildef*
osfoprwb( const char* fname, os_filetype_t )
{
//  assert(fname != 0);
    osfildef* fp = fopen(fname, "r+b");
    if (fp == 0) fp = fopen(fname, "w+b");
    return fp;
}

/* Duplicate a file hand.e
 */
osfildef*
osfdup(osfildef *orig, const char *mode)
{
    char realmode[5];
    char *p = realmode;
    const char *m;

    /* verify that there aren't any unrecognized mode flags */
    for (m = mode ; *m != '\0' ; ++m)
    {
        if (strchr("rw+bst", *m) == 0)
            return 0;
    }

    /* figure the read/write mode - translate r+ and w+ to r+ */
    if ((mode[0] == 'r' || mode[0] == 'w') && mode[1] == '+')
        *p++ = 'r', *p++ = '+';
    else if (mode[0] == 'r')
        *p++ = 'r';
    else if (mode[0] == 'w')
        *p++ = 'w';
    else
        return 0;

    /* end the mode string */
    *p = '\0';

    /* duplicate the handle in the given mode */
    return fdopen(dup(fileno(orig)), mode);
}


/* Convert string to all-lowercase.
 */
char*
os_strlwr( char* s )
{
//  assert(s != 0);
    for (int i = 0; s[i] != '\0'; ++i) {
#if 0
        s[i] = tolower(s[i]);
#endif
        s[i] = tolower((unsigned char)s[i]);
    }
    return s;
}


/* Seek to the resource file embedded in the current executable file.
 *
 * We don't support this (and probably never will).
 */
osfildef*
os_exeseek( const char*, const char* )
{
#if 0
    return static_cast(osfildef*)(0);
#endif
    return static_cast<osfildef*>(0);
}


#if 0
/* Create and open a temporary file.
 */
osfildef*
os_create_tempfile( const char* fname, char* buf )
{
    if (fname != 0 and fname[0] != '\0') {
        // A filename has been specified; use it.
        return fopen(fname, "w+b");
    }

    //ASSERT(buf != 0);

    // No filename needed; create a nameless tempfile.
    buf[0] = '\0';
    return tmpfile();
}


/* Delete a temporary file created with os_create_tempfile().
 */
int
osfdel_temp( const char* fname )
{
    //ASSERT(fname != 0);

    if (fname[0] == '\0' or remove(fname) == 0) {
        // Either it was a nameless temp-file and has been
        // already deleted by the system, or deleting it
        // succeeded.  In either case, the operation was
        // successful.
        return 0;
    }
    // It was not a nameless tempfile and remove() failed.
    return 1;
}


/* Generate a temporary filename.
 */
int
os_gen_temp_filename( char* buf, size_t buflen )
{
    // Get the temporary directory: use the environment variable TMPDIR
    // if it's defined, otherwise use P_tmpdir; if even that's not
    // defined, return failure.
    const char* tmp = getenv("TMPDIR");
    if (tmp == 0)
        tmp = P_tmpdir;
    if (tmp == 0)
        return false;

    // Build a template filename for mkstemp.
    snprintf(buf, buflen, "%s/tads-XXXXXX", tmp);

    // Generate a unique name and open the file.
    int fd = mkstemp(buf);
    if (fd >= 0) {
        // Got it - close the placeholder file and return success.
        close(fd);
        return true;
    }
    // Failed.
    return false;
}
#endif

/* tads2/osnoui.c defines DOS versions of those routine when MSDOS is defined.
 */
#ifndef MSDOS

/* Get the temporary file path.
 */
void
os_get_tmp_path( char* buf )
{
#ifdef _WIN32
    GetTempPath(OSFNMAX, buf);
#else
    // Try the usual env. variable first.
    const char* tmpDir = getenv("TMPDIR");

    // If no such variable exists, try P_tmpdir (if defined in
    // <stdio.h>).
#ifdef P_tmpdir
    if (tmpDir == 0 or tmpDir[0] == '\0') {
        tmpDir = P_tmpdir;
    }
#endif

    // If we still lack a path, use "/tmp".
    if (tmpDir == 0 or tmpDir[0] == '\0') {
        tmpDir = "/tmp";
    }

    // If the directory doesn't exist or is not a directory, don't
    // provide anything at all (which means that the current
    // directory will be used).
    struct stat inf;
    int statRet = stat(tmpDir, &inf);
    if (statRet != 0 or (inf.st_mode & S_IFMT) != S_IFDIR) {
        buf[0] = '\0';
        return;
    }

    strcpy(buf, tmpDir);

    // Append a slash if necessary.
    size_t len = strlen(buf);
    if (buf[len - 1] != '/') {
        buf[len] = '/';
        buf[len + 1] = '\0';
    }
#endif
}

/* Create a directory.
 */
int
os_mkdir( const char* dir, int create_parents )
{
    //assert(dir != 0);

    if (dir[0] == '\0')
        return true;

    // Copy the directory name to a new string so we can strip any trailing
    // path seperators.
    size_t len = strlen(dir);
    char* tmp = new char[len + 1];
    strncpy(tmp, dir, len);
    while (tmp[len - 1] == OSPATHCHAR)
        --len;
    tmp[len] = '\0';

    // If we're creating intermediate diretories, and the path contains
    // multiple elements, recursively create the parent directories first.
    if (create_parents and strchr(tmp, OSPATHCHAR) != 0) {
        char par[OSFNMAX];

        // Extract the parent path.
        os_get_path_name(par, sizeof(par), tmp);

        // If the parent doesn't already exist, create it recursively.
        if (osfacc(par) != 0 and not os_mkdir(par, true)) {
            delete[] tmp;
            return false;
        }
    }

    // Create the directory.
    int ret =
#if HAVE_MKDIR
#   if MKDIR_TAKES_ONE_ARG
             mkdir(tmp);
#   else
             mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO);
#   endif
#elif HAVE__MKDIR
             _mkdir(tmp);
#else
#   error "Neither mkdir() nor _mkdir() is available on this system."
#endif
    delete[] tmp;
    return ret == 0;
}

/* Remove a directory.
 */
int
os_rmdir( const char *dir )
{
    return rmdir(dir) == 0;
}

#endif // not MSDOS

/* Get a file's mode/type.  This returns the same information as
 * the 'mode' member of os_file_stat_t from os_file_stat(), so we
 * simply call that routine and return the value.
 */
int
osfmode( const char *fname, int follow_links, unsigned long *mode,
         unsigned long* attr )
{
    os_file_stat_t s;
    int ok;
    if ((ok = os_file_stat(fname, follow_links, &s)) != false) {
        if (mode != NULL)
            *mode = s.mode;
        if (attr != NULL)
            *attr = s.attrs;
    }
    return ok;
}

/* Get full stat() information on a file.
 */
int
os_file_stat( const char *fname, int follow_links, os_file_stat_t *s )
{
    struct stat buf;
    if ((follow_links ? stat(fname, &buf) : lstat(fname, &buf)) != 0)
        return false;

    s->sizelo = (uint32_t)(buf.st_size & 0xFFFFFFFF);
    s->sizehi = sizeof(buf.st_size) > 4
                ? (uint32_t)((buf.st_size >> 32) & 0xFFFFFFFF)
                : 0;
    s->cre_time = buf.st_ctime;
    s->mod_time = buf.st_mtime;
    s->acc_time = buf.st_atime;
    s->mode = buf.st_mode;
    s->attrs = 0;

#ifdef _WIN32
    s->attrs = oss_get_file_attrs(fname);
#else
    if (os_get_root_name(fname)[0] == '.') {
        s->attrs |= OSFATTR_HIDDEN;
    }

    // If we're the owner, check if we have read/write access.
    if (geteuid() == buf.st_uid) {
        if (buf.st_mode & S_IRUSR)
            s->attrs |= OSFATTR_READ;
        if (buf.st_mode & S_IWUSR)
            s->attrs |= OSFATTR_WRITE;
        return true;
    }

    // Check if one of our groups matches the file's group and if so, check
    // for read/write access.

    // Also reserve a spot for the effective group ID, which might
    // not be included in the list in our next call.
    int grpSize = getgroups(0, NULL) + 1;
    // Paranoia.
    if (grpSize > NGROUPS_MAX or grpSize < 0)
        return false;
    gid_t* groups = new gid_t[grpSize];
    if (getgroups(grpSize - 1, groups + 1) < 0) {
        delete[] groups;
        return false;
    }
    groups[0] = getegid();
    int i;
    for (i = 0; i < grpSize and buf.st_gid != groups[i]; ++i)
        ;
    delete[] groups;
    if (i < grpSize) {
        if (buf.st_mode & S_IRGRP)
            s->attrs |= OSFATTR_READ;
        if (buf.st_mode & S_IWGRP)
            s->attrs |= OSFATTR_WRITE;
        return true;
    }

    // We're neither the owner of the file nor do we belong to its
    // group.  Check whether the file is world readable/writable.
    if (buf.st_mode & S_IROTH)
        s->attrs |= OSFATTR_READ;
    if (buf.st_mode & S_IWOTH)
        s->attrs |= OSFATTR_WRITE;
#endif
    return true;
}

/* Manually resolve a symbolic link.
 */
int
os_resolve_symlink( const char *fname, char *target, size_t target_size )
{
#ifdef _WIN32
    return false;
#else
    // get the stat() information for the *undereferenced* link; if
    // it's not actually a link, there's nothing to resolve
    struct stat buf;
    if (lstat(fname, &buf) != 0 or (buf.st_mode & S_IFLNK) == 0)
        return false;

    // read the link contents (maxing out at the buffer size)
    size_t copylen = (size_t)buf.st_size;
    if (copylen > target_size - 1)
        copylen = target_size - 1;
    if (readlink(fname, target, copylen) < 0)
        return false;

    // null-terminate the result and return success
    target[copylen] = '\0';
    return true;
#endif
}

#if 0
/* Get a suitable seed for a random number generator.
 *
 * We don't just use the system-clock, but build a number as random as
 * the mechanisms of the standard C-library allow.  This is somewhat of
 * an overkill, but it's simple enough so here goes.  Someone has to
 * write a nuclear war simulator in TADS to test this thoroughly.
 */
void
os_rand( long* val )
{
    //assert(val != 0);
    // Use the current time as the first seed.
    srand(static_cast(unsigned int)(time(0)));

    // Create the final seed by generating a random number using
    // high-order bits, because on some systems the low-order bits
    // aren't very random.
    *val = 1 + static_cast(long)(static_cast(long double)(65535) * rand() / (RAND_MAX + 1.0));
}


/* Generate random bytes for use in seeding a PRNG (pseudo-random number
 * generator).  This is an extended version of os_rand() for PRNGs that use
 * large seed vectors containing many bytes, rather than the simple 32-bit
 * seed that os_rand() assumes.
 */
void os_gen_rand_bytes( unsigned char* buf, size_t len )
{
    // Read bytes from /dev/urandom to fill the buffer (use /dev/urandom
    // rather than /dev/random, so that we don't block for long periods -
    // /dev/random can be quite slow because it's designed not to return
    // any bits until a fairly high threshold of entropy has been reached).
    int f = open("/dev/urandom", O_RDONLY);
    read(f, (void*)buf, len);
    close(f);
}
#endif

/* Get the current system high-precision timer.
 *
 * We provide four (4) implementations of this function:
 *
 *   1. clock_gettime() is available
 *   2. gettimeofday() is available
 *   3. ftime() is available
 *   4. Neither is available - fall back to time()
 *
 * Note that HAVE_CLOCK_GETTIME, HAVE_GETTIMEOFDAY and HAVE_FTIME are
 * mutually exclusive; if one of them is defined, the others aren't.  No
 * need for #else here.
 *
 * Although not required by the TADS VM, these implementations will
 * always return 0 on the first call.
 */
#ifdef HAVE_CLOCK_GETTIME
// The system has the clock_gettime() function.
long
os_get_sys_clock_ms( void )
{
    // We need to remember the exact time this function has been
    // called for the first time, and use that time as our
    // zero-point.  On each call, we simply return the difference
    // in milliseconds between the current time and our zero point.
    static struct timespec zeroPoint;

    // Did we get the zero point yet?
    static bool initialized = false;

    // Not all systems provide a monotonic clock; check if it's
    // available before falling back to the global system-clock.  A
    // monotonic clock is guaranteed not to change while the system
    // is running, so we prefer it over the global clock.
    static const clockid_t clockType =
#ifdef HAVE_CLOCK_MONOTONIC
        CLOCK_MONOTONIC;
#else
        CLOCK_REALTIME;
#endif
    // We must get the current time in each call.
    struct timespec currTime;

    // Initialize our zero-point, if not already done so.
    if (not initialized) {
        clock_gettime(clockType, &zeroPoint);
        initialized = true;
    }

    // Get the current time.
    clock_gettime(clockType, &currTime);

    // Note that tv_nsec contains *nano*seconds, not milliseconds,
    // so we need to convert it; a millisec is 1.000.000 nanosecs.
    return (currTime.tv_sec - zeroPoint.tv_sec) * 1000
         + (currTime.tv_nsec - zeroPoint.tv_nsec) / 1000000;
}

/* Get the time since the Unix Epoch in seconds and nanoseconds.
 */
void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // Get the current time.
    static const clockid_t clockType = CLOCK_REALTIME;
    struct timespec currTime;
    clock_gettime(clockType, &currTime);

    // return the data
    *seconds = currTime.tv_sec;
    *nanoseconds = currTime.tv_nsec;
}
#endif // HAVE_CLOCK_GETTIME

#ifdef HAVE_GETTIMEOFDAY
// The system has the gettimeofday() function.
long
os_get_sys_clock_ms( void )
{
    static struct timeval zeroPoint;
    static bool initialized = false;
    // gettimeofday() needs the timezone as a second argument.  This
    // is obsolete today, but we don't care anyway; we just pass
    // something.
#if 0
    struct timezone bogus = {0, 0};
#endif
    struct timeval currTime;

    if (not initialized) {
#if 0
        gettimeofday(&zeroPoint, &bogus);
#endif
        gettimeofday(&zeroPoint, NULL);
        initialized = true;
    }

#if 0
    gettimeofday(&currTime, &bogus);
#endif
    gettimeofday(&currTime, NULL);

    // 'tv_usec' contains *micro*seconds, not milliseconds.  A
    // millisec is 1.000 microsecs.
    return (currTime.tv_sec - zeroPoint.tv_sec) * 1000 + (currTime.tv_usec - zeroPoint.tv_usec) / 1000;
}

/* Get the time since the Unix Epoch in seconds and nanoseconds.
 */
void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // get the time
#if 0
    struct timezone bogus = {0, 0};
#endif
    struct timeval currTime;
#if 0
    gettimeofday(&currTime, &bogus);
#endif
    gettimeofday(&currTime, NULL);

    // return the data, converting milliseconds to nanoseconds
    *seconds = currTime.tv_sec;
    *nanoseconds = currTime.tv_usec * 1000;
}
#endif // HAVE_GETTIMEOFDAY


#ifdef HAVE_FTIME
// The system has the ftime() function.  ftime() is in <sys/timeb.h>.
#include <sys/timeb.h>
long
os_get_sys_clock_ms( void )
{
    static timeb zeroPoint;
    static bool initialized = false;
    struct timeb currTime;

    if (not initialized) {
        ftime(&zeroPoint);
        initialized = true;
    }

    ftime(&currTime);
    return (currTime.time - zeroPoint.time) * 1000 + (currTime.millitm - zeroPoint.millitm);
}

/* Get the time since the Unix Epoch in seconds and nanoseconds.
 */
void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // get the time
    struct timeb currTime;
    ftime(&currTime);

    // return the data, converting milliseconds to nanoseconds
    *seconds = currTime.time;
    *nanoseconds = (long)currTime.millitm * 1000000;
}
#endif // HAVE_FTIME


#ifndef HAVE_CLOCK_GETTIME
#ifndef HAVE_GETTIMEOFDAY
#ifndef HAVE_FTIME
// Use the standard time() function from the C library.  We lose
// millisecond-precision, but that's the best we can do with time().
long
os_get_sys_clock_ms( void )
{
    static time_t zeroPoint = time(0);
    return (time(0) - zeroPoint) * 1000;
}

void
os_time_ns( os_time_t *seconds, long *nanoseconds )
{
    // get the regular time() value in seconds; we have no
    // higher precision element to return, so return zero
#if 0
    *seconds = time();
#endif
    *seconds = time(0);
    *nanoseconds = 0;
}
#endif
#endif
#endif


/* Get filename from startup parameter, if possible.
 *
 * TODO: Find out what this is supposed to do.
 */
int
os_paramfile( char* )
{
    return 0;
}


/* Open a directory search.
 */
int os_open_dir(const char *dirname, osdirhdl_t *hdl)
{
    return (*hdl = opendir(dirname)) != NULL;
}

/* Read the next result in a directory search.
 */
int os_read_dir(osdirhdl_t hdl, char *buf, size_t buflen)
{
    // Read the next directory entry - if we've exhausted the search,
    // return failure.
    struct dirent *d = readdir(hdl);
    if (d == 0)
        return false;

    // return this entry
    safe_strcpy(buf, buflen, d->d_name);
    return true;
}

/* Close a directory search.
 */
void os_close_dir(osdirhdl_t hdl)
{
    closedir(hdl);
}


/* Determine if the given filename refers to a special file.
 *
 * tads2/osnoui.c defines its own version when MSDOS is defined.
 */
#ifndef MSDOS
os_specfile_t
os_is_special_file( const char* fname )
{
    // We also check for "./" and "../" instead of just "." and
    // "..".  (We use OSPATHCHAR instead of '/' though.)
    const char selfWithSep[3] = {'.', OSPATHCHAR, '\0'};
    const char parentWithSep[4] = {'.', '.', OSPATHCHAR, '\0'};
    if ((strcmp(fname, ".") == 0) or (strcmp(fname, selfWithSep) == 0)) return OS_SPECFILE_SELF;
    if ((strcmp(fname, "..") == 0) or (strcmp(fname, parentWithSep) == 0)) return OS_SPECFILE_PARENT;
    return OS_SPECFILE_NONE;
}
#endif


#if 1
extern "C" void canonicalize_path(char *path);
#else
/* Canonicalize a path: remove ".." and "." relative elements.
 * (Copied from tads2/osnoui.c)
 */
static void
canonicalize_path(char *path)
{
    char *p;
    char *start;

    /* keep going until we're done */
    for (start = p = path ; ; ++p)
    {
        /* if it's a separator, note it and process the path element */
        if (*p == '\\' || *p == '/' || *p == '\0')
        {
            /* 
             *   check the path element that's ending here to see if it's a
             *   relative item - either "." or ".." 
             */
            if (p - start == 1 && *start == '.')
            {
                /* 
                 *   we have a '.' element - simply remove it along with the
                 *   path separator that follows 
                 */
                if (*p == '\\' || *p == '/')
                    memmove(start, p + 1, strlen(p+1) + 1);
                else if (start > path)
                    *(start - 1) = '\0';
                else
                    *start = '\0';
            }
            else if (p - start == 2 && *start == '.' && *(start+1) == '.')
            {
                char *prv;

                /* 
                 *   we have a '..' element - find the previous path element,
                 *   if any, and remove it, along with the '..' and the
                 *   subsequent separator 
                 */
                for (prv = start ;
                     prv > path && (*(prv-1) != '\\' || *(prv-1) == '/') ;
                     --prv) ;

                /* if we found a separator, remove the previous element */
                if (prv > start)
                {
                    if (*p == '\\' || *p == '/')
                        memmove(prv, p + 1, strlen(p+1) + 1);
                    else if (start > path)
                        *(start - 1) = '\0';
                    else
                        *start = '\0';
                }
            }

            /* note the start of the next element */
            start = p + 1;
        }

        /* stop at the end of the string */
        if (*p == '\0')
            break;
    }
}
#endif

#ifdef _WIN32
/* This file only ever calls realpath() with resolved_name as NULL. */
static char *realpath(const char *file_name, char *resolved_name)
{
    resolved_name = (char *)malloc(PATH_MAX + 1);
    if (resolved_name == NULL)
        return NULL;

    DWORD len = GetFullPathName(file_name, PATH_MAX + 1, resolved_name, NULL);
    if (!len || len > PATH_MAX)
        return NULL;

    return resolved_name;
}
#endif


/* Resolve symbolic links in a path.  It's okay for 'buf' and 'path'
 * to point to the same buffer if you wish to resolve a path in place.
 */
static void
resolve_path( char *buf, size_t buflen, const char *path )
{
    // Starting with the full path string, try resolving the path with
    // realpath().  The tricky bit is that realpath() will fail if any
    // component of the path doesn't exist, but we need to resolve paths
    // for prospective filenames, such as files or directories we're
    // about to create.  So if realpath() fails, remove the last path
    // component and try again with the remainder.  Repeat until we
    // can resolve a real path, or run out of components to remove.
    // The point of this algorithm is that it will resolve as much of
    // the path as actually exists in the file system, ensuring that
    // we resolve any links that affect the path.  Any portion of the
    // path that doesn't exist obviously can't refer to a link, so it
    // will be taken literally.  Once we've resolved the longest prefix,
    // tack the stripped portion back on to form the fully resolved
    // path.

    // make a writable copy of the path to work with
    size_t pathl = strlen(path);
    char *mypath = new char[pathl + 1];
    memcpy(mypath, path, pathl + 1);

    // start at the very end of the path, with no stripped suffix yet
    char *suffix = mypath + pathl;
    char sl = '\0';

    // keep going until we resolve something or run out of path
    for (;;)
    {
        // resolve the current prefix, allocating the result
        char *rpath = realpath(mypath, 0);

        // un-split the path
        *suffix = sl;

        // if we resolved the prefix, return the result
        if (rpath != 0)
        {
            // success - if we separated a suffix, reattach it
            if (*suffix != '\0')
            {
                // reattach the suffix (the part after the '/')
                for ( ; *suffix == '/' ; ++suffix) ;
                os_build_full_path(buf, buflen, rpath, suffix);
            }
            else
            {
                // no suffix, so we resolved the entire path
                safe_strcpy(buf, buflen, rpath);
            }

            // done with the resolved path
            free(rpath);

            // ...and done searching
            break;
        }

        // no luck with realpath(); search for the '/' at the end of the
        // previous component in the path 
        for ( ; suffix > mypath && *(suffix-1) != '/' ; --suffix) ;

        // skip any redundant slashes
        for ( ; suffix > mypath && *(suffix-1) == '/' ; --suffix) ;

        // if we're at the root element, we're out of path elements
        if (suffix == mypath)
        {
            // we can't resolve any part of the path, so just return the
            // original path unchanged
            safe_strcpy(buf, buflen, mypath);
            break;
        }

        // split the path here into prefix and suffix, and try again
        sl = *suffix;
        *suffix = '\0';
    }

    // done with our writable copy of the path
    delete [] mypath;
}

/* Is the given file in the given directory?
 */
int
os_is_file_in_dir( const char* filename, const char* path,
                   int allow_subdirs, int match_self )
{
    char filename_buf[OSFNMAX], path_buf[OSFNMAX];
    size_t flen, plen;

    // Absolute-ize the filename, if necessary.
    if (not os_is_file_absolute(filename)) {
        os_get_abs_filename(filename_buf, sizeof(filename_buf), filename);
        filename = filename_buf;
    }

    // Absolute-ize the path, if necessary.
    if (not os_is_file_absolute(path)) {
        os_get_abs_filename(path_buf, sizeof(path_buf), path);
        path = path_buf;
    }

    // Canonicalize the paths, to remove .. and . elements - this will make
    // it possible to directly compare the path strings.  Also resolve it
    // to the extent possible, to make sure we're not fooled by symbolic
    // links.
    safe_strcpy(filename_buf, sizeof(filename_buf), filename);
    canonicalize_path(filename_buf);
    resolve_path(filename_buf, sizeof(filename_buf), filename_buf);
    filename = filename_buf;

    safe_strcpy(path_buf, sizeof(path_buf), path);
    canonicalize_path(path_buf);
    resolve_path(path_buf, sizeof(path_buf), path_buf);
    path = path_buf;

    // Get the length of the filename and the length of the path.
    flen = strlen(filename);
    plen = strlen(path);

    // If the path ends in a separator character, ignore that.
    if (plen > 0 and path[plen-1] == OSPATHCHAR)
        --plen;

    // if the names match, return true if and only if we're matching the
    // directory to itself
    if (plen == flen && memcmp(filename, path, flen) == 0)
        return match_self;

    // Check that the filename has 'path' as its path prefix.  First, check
    // that the leading substring of the filename matches 'path', ignoring
    // case.  Note that we need the filename to be at least two characters
    // longer than the path: it must have a path separator after the path
    // name, and at least one character for a filename past that.
    if (flen < plen + 2 or memcmp(filename, path, plen) != 0)
        return false;

    // Okay, 'path' is the leading substring of 'filename'; next make sure
    // that this prefix actually ends at a path separator character in the
    // filename.  (This is necessary so that we don't confuse "c:\a\b.txt"
    // as matching "c:\abc\d.txt" - if we only matched the "c:\a" prefix,
    // we'd miss the fact that the file is actually in directory "c:\abc",
    // not "c:\a".)
    if (filename[plen] != OSPATHCHAR)
        return false;

    // We're good on the path prefix - we definitely have a file that's
    // within the 'path' directory or one of its subdirectories.  If we're
    // allowed to match on subdirectories, we already have our answer
    // (true).  If we're not allowed to match subdirectories, we still have
    // one more check, which is that the rest of the filename is free of
    // path separator charactres.  If it is, we have a file that's directly
    // in the 'path' directory; otherwise it's in a subdirectory of 'path'
    // and thus isn't a match.
    if (allow_subdirs) {
        // Filename is in the 'path' directory or one of its
        // subdirectories, and we're allowed to match on subdirectories, so
        // we have a match.
        return true;
    }

    // We're not allowed to match subdirectories, so scan the rest of
    // the filename for path separators.  If we find any, the file is
    // in a subdirectory of 'path' rather than directly in 'path'
    // itself, so it's not a match.  If we don't find any separators,
    // we have a file directly in 'path', so it's a match.
    const char* p;
    for (p = filename; *p != '\0' and *p != OSPATHCHAR ; ++p)
        ;

    // If we reached the end of the string without finding a path
    // separator character, it's a match .
    return *p == '\0';
}


/* Get the absolute path for a given filename.
 */
int
os_get_abs_filename( char* buf, size_t buflen, const char* filename )
{
    // If the filename is already absolute, copy it; otherwise combine
    // it with the working directory.
    if (os_is_file_absolute(filename))
    {
        // absolute - copy it as-is
        safe_strcpy(buf, buflen, filename);
    }
    else
    {
        // combine it with the working directory to get the path
        char pwd[OSFNMAX];
        if (getcwd(pwd, sizeof(pwd)) != 0)
            os_build_full_path(buf, buflen, pwd, filename);
        else
            safe_strcpy(buf, buflen, filename);
    }

    // canonicalize the result
    canonicalize_path(buf);

    // Try getting the canonical path from the OS (allocating the
    // result buffer).
    char* newpath = realpath(filename, 0);
    if (newpath != 0) {
        // Copy the output (truncating if it's too long).
        safe_strcpy(buf, buflen, newpath);
        free(newpath);
        return true;
    }

    // realpath() failed, but that's okay - realpath() only works if the
    // path refers to an existing file, but it's valid for callers to
    // pass non-existent filenames, such as names of files they're about
    // to create, or hypothetical paths being used for comparison
    // purposes or for future use.  Simply return the canonical path
    // name we generated above.
    return true;
}


/* ------------------------------------------------------------------------ */
/*
 * Get the file system roots.  Unix has the lovely unified namespace with
 * just the one root, /, so this is quite simple.
 */
size_t os_get_root_dirs(char *buf, size_t buflen)
{
    static const char ret[] = { '/', 0, 0 };
    
    // if there's room, copy the root string "/" and an extra null
    // terminator for the overall list
    if (buflen >= sizeof(ret))
        memcpy(buf, ret, sizeof(ret));

    // return the required size
    return sizeof(ret);
}


/* ------------------------------------------------------------------------ */
/* Get a special directory path.
 *
 * If env. variables exist, they always override the compile-time
 * defaults.
 *
 * We ignore the argv parameter, since on Unix binaries aren't stored
 * together with data on disk.
 */
void
os_get_special_path( char* buf, size_t buflen, const char* argv0, int id )
{
    const char* res;

    switch (id) {
      case OS_GSP_T3_RES:
        res = getenv("T3_RESDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_RES_DIR;
        }
        break;

      case OS_GSP_T3_INC:
        res = getenv("T3_INCDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_INC_DIR;
        }
        break;

      case OS_GSP_T3_LIB:
        res = getenv("T3_LIBDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_LIB_DIR;
        }
        break;

      case OS_GSP_T3_USER_LIBS:
        // There's no compile-time default for user libs.
        res = getenv("T3_USERLIBDIR");
        break;

      case OS_GSP_T3_SYSCONFIG:
        res = getenv("T3_CONFIG");
        if (res == 0 and argv0 != 0) {
            os_get_path_name(buf, buflen, argv0);
            return;
        }
        break;

      case OS_GSP_LOGFILE:
        res = getenv("T3_LOGDIR");
        if (res == 0 or res[0] == '\0') {
            res = T3_LOG_FILE;
        }
        break;

      default:
        // TODO: We could print a warning here to inform the
        // user that we're outdated.
        res = 0;
    }

    if (res != 0) {
        // Only use the detected path if it exists and is a
        // directory.
        struct stat inf;
        int statRet = stat(res, &inf);
        if (statRet == 0 and (inf.st_mode & S_IFMT) == S_IFDIR) {
            strncpy(buf, res, buflen - 1);
            return;
        }
    }
    // Indicate failure.
    buf[0] = '\0';
}


#if 0
/* Generate a filename for a character-set mapping file.
 *
 * Follow DOS convention: start with the current local charset
 * identifier, then the internal ID, and the suffix ".tcp".  No path
 * prefix, which means look in current directory.  This is what we
 * want, because mapping files are supposed to be distributed with a
 * game, not with the interpreter.
 */
void
os_gen_charmap_filename( char* filename, char* internal_id, char* /*argv0*/ )
{
    char mapname[32];

    os_get_charmap(mapname, OS_CHARMAP_DISPLAY);

    // Theoretically, we can get mapname so long that with 4-letter
    // internal id and 4-letter extension '.tcp' it will be longer
    // than OSFNMAX. Highly unlikely, but...
    size_t len = strlen(mapname);
    if (len > OSFNMAX - 9) len = OSFNMAX - 9;

    memcpy(filename, mapname, len);
    strcat(filename, internal_id);
    strcat(filename, ".tcp");
}

#endif

/* Receive notification that a character mapping file has been loaded.
 */
void
os_advise_load_charmap( char* /*id*/, char* /*ldesc*/, char* /*sysinfo*/ )
{
}


/* T3 post-load UI initialization.  This is a hook provided by the T3
 * VM to allow the UI layer to do any extra initialization that depends
 * upon the contents of the loaded game file.  We don't currently need
 * any extra initialization here.
 */
#ifdef RUNTIME
void
os_init_ui_after_load( class CVmBifTable*, class CVmMetaTable* )
{
    // No extra initialization required.
}
#endif


/* Get the full filename (including directory path) to the executable
 * file, given the argv[0] parameter passed into the main program.
 *
 * On Unix, you can't tell what the executable's name is by just looking
 * at argv, so we always indicate failure.  No big deal though; this
 * information is only used when the interpreter's executable is bundled
 * with a game, and we don't support this at all.
 */
int
os_get_exe_filename( char*, size_t, const char* )
{
    return false;
}


#if 0
/* Generate the name of the character set mapping table for Unicode
 * characters to and from the given local character set.
 */
void
os_get_charmap( char* mapname, int charmap_id )
{
    const char* charset = 0;  // Character set name.

#ifdef RUNTIME
    // If there was a command line option, it takes precedence.
    // User knows better, so do not try to modify his setting.
    if (globalApp->options.characterSet[0] != '\0') {
        // One charset for all.
        strncpy(mapname, globalApp->options.characterSet, 32);
        return;
    }
#endif

    // There is absolutely no robust way to determine the local
    // character set.  Roughly speaking, we have three options:
    //
    // Use nl_langinfo() function.  Not always available.
    //
    // Use setlocale(LC_CTYPE, NULL).  This only works if user set
    // locale which is actually installed on the machine, or else
    // it will return NULL.  But we don't need locale, we just need
    // to know what the user wants.
    //
    // Manually look up environment variables LC_ALL, LC_CTYPE and
    // LANG.
    //
    // However, not a single one will provide us with a reliable
    // name of local character set.  There is no standard way to
    // name charsets.  For a single set we can get almost anything:
    // Windows-1251, Win-1251, CP1251, CP-1251, ru_RU.CP1251,
    // ru_RU.CP-1251, ru_RU.CP_1251...  And the only way is to
    // maintain a database of aliases.

#if HAVE_LANGINFO_CODESET
    charset = nl_langinfo(CODESET);
#else
    charset = getenv("LC_CTYPE");
    if (charset == 0 or charset[0] == '\0') {
        charset = getenv("LC_ALL");
        if (charset == 0 or charset[0] == '\0') {
            charset = getenv("LANG");
        }
    }
#endif

    if (charset == 0) {
        strcpy(mapname, "us-ascii");
    } else {
        strcpy(mapname, get_charset_alias(charset));
    }
    return;
}

#endif

/* Translate a character from the HTML 4 Unicode character set to the
 * current character set used for display.
 *
 * Note that this function is used only by Tads 2.  Tads 3 does mappings
 * automatically.
 *
 * We omit this implementation when not compiling the interpreter (in
 * which case os_xlat_html4 will have been defined as an empty macro in
 * osfrobtads.h).  We do this because we don't want to introduce any
 * TADS 3 dependencies upon the TADS 2 compiler, which should compile
 * regardless of whether the TADS 3 sources are available or not.
 */
#ifndef os_xlat_html4
void
os_xlat_html4( unsigned int html4_char, char* result, size_t result_buf_len )
{
    // HTML 4 characters are Unicode.  Tads 3 provides just the
    // right mapper: Unicode to ASCII.  We make it static in order
    // not to create a mapper on each call and save CPU cycles.
    static CCharmapToLocalASCII mapper;
    result[mapper.map_char(html4_char, result, result_buf_len)] = '\0';
}
#endif

#if 0
/* =====================================================================
 *
 * The functions defined below are not needed by the interpreter, or
 * have a curses-specific implementation and are therefore only used
 * when building the compiler (the compiler doesn't use curses, just
 * plain stdio).
 */
#ifndef RUNTIME

/* Wait for a character to become available from the keyboard.
 */
void
os_waitc( void )
{
    getchar();
}


/* Read a character from the keyboard.
 */
int
os_getc( void )
{
    return getchar();
}


/* Read a character from the keyboard and return the low-level,
 * untranslated key code whenever possible.
 */
int
os_getc_raw( void )
{
    return getchar();
}


/* Check for user break.
 *
 * We only provide a dummy here; we might do something more useful in
 * the future though.
 */
int
os_break( void )
{
    return false;
}


/* Yield CPU.
 *
 * We don't need this.  It's only useful for Windows 3.x and maybe pre-X Mac OS
 * and other systems with brain damaged multitasking.
 */
int
os_yield( void )
{
    return false;
}


/* Sleep for a while.
 *
 * The compiler doesn't need this.
 */
void
os_sleep_ms( long )
{
}

#endif /* ! RUNTIME */
#endif
