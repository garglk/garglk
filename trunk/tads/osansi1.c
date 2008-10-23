/* osansi1.c -- basic file operations using stdio */

#include "os.h"
#include <unistd.h>	/* for access() */

/* 
 *   Open text file for reading.  Returns NULL on error.
 *   
 *   A text file differs from a binary file in that some systems perform
 *   translations to map between C conventions and local file system
 *   conventions; for example, on DOS, the stdio library maps the DOS
 *   CR-LF newline convention to the C-style '\n' newline format.  On many
 *   systems (Unix, for example), there is no distinction between text and
 *   binary files.  
 */
osfildef *osfoprt(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "r");
}

/* 
 *   Open text file for writing; returns NULL on error 
 */
osfildef *osfopwt(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "w");
}

/*
 *   Open text file for reading and writing, keeping the file's existing
 *   contents if the file already exists or creating a new file if no such
 *   file exists.  Returns NULL on error. 
 */
osfildef *osfoprwt(const char *fname, os_filetype_t typ)
{
	FILE *fp;
	fp = fopen(fname, "r+");
	if (!fp)
		fp = fopen(fname, "w+");
	return fp;
}

/* 
 *   Open text file for reading/writing.  If the file already exists,
 *   truncate the existing contents.  Create a new file if it doesn't
 *   already exist.  Return null on error.  
 */
osfildef *osfoprwtt(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "w+");
}

/* 
 *   Open binary file for writing; returns NULL on error.  
 */
osfildef *osfopwb(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "wb");
}

/* 
 *   Open source file for reading - use the appropriate text or binary
 *   mode.  
 */
osfildef *osfoprs(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "r");
}

/* 
 *   Open binary file for reading; returns NULL on error.  
 */
osfildef *osfoprb(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "rb");
}

/* 
 *   Open binary file for reading/writing.  If the file already exists, keep
 *   the existing contents.  Create a new file if it doesn't already exist.
 *   Return null on error.  
 */
osfildef *osfoprwb(const char *fname, os_filetype_t typ)
{
	FILE *fp;
	fp = fopen(fname, "r+b");
	if (!fp)
		fp = fopen(fname, "w+b");
	return fp;
}

/* 
 *   Open binary file for reading/writing.  If the file already exists,
 *   truncate the existing contents.  Create a new file if it doesn't
 *   already exist.  Return null on error.  
 */
osfildef *osfoprwtb(const char *fname, os_filetype_t typ)
{
	return fopen(fname, "w+b");
}

/* 
 *   Get a line of text from a text file.  Uses fgets semantics.  
 */
char *osfgets(char *buf, size_t len, osfildef *fp)
{
	return fgets(buf, len, fp);
}

/* 
 *   Write a line of text to a text file.  Uses fputs semantics.  
 */
int osfputs(const char *buf, osfildef *fp)
{
	return fputs(buf, fp);
}

/*
 *   Write to a text file.  os_fprintz() takes a null-terminated string,
 *   while os_fprint() takes an explicit separate length argument that might
 *   not end with a null terminator.  
 */
void os_fprintz(osfildef *fp, const char *str)
{
	fwrite(str, 1, strlen(str), fp);
}

void os_fprint(osfildef *fp, const char *str, size_t len)
{
	fwrite(str, 1, len, fp);
}

/* 
 *   Write bytes to file.  Return 0 on success, non-zero on error.  
 */
int osfwb(osfildef *fp, const void *buf, int bufl)
{
	return fwrite(buf, bufl, 1, fp) != 1;
}

/* 
 *   Read bytes from file.  Return 0 on success, non-zero on error.  
 */
int osfrb(osfildef *fp, void *buf, int bufl)
{
	return fread(buf, bufl, 1, fp) != 1;
}

/* 
 *   Read bytes from file and return the number of bytes read.  0
 *   indicates that no bytes could be read. 
 */
size_t osfrbc(osfildef *fp, void *buf, size_t bufl)
{
	return fread(buf, 1, bufl, fp);
}

/* 
 *   Get the current seek location in the file.  The first byte of the
 *   file has seek position 0.  
 */
long osfpos(osfildef *fp)
{
	return ftell(fp);
}

/* 
 *   Seek to a location in the file.  The first byte of the file has seek
 *   position 0.  Returns zero on success, non-zero on error.
 *   
 *   The following constants must be defined in your OS-specific header;
 *   these values are used for the "mode" parameter to indicate where to
 *   seek in the file:
 *   
 *   OSFSK_SET - set position relative to the start of the file
 *.  OSFSK_CUR - set position relative to the current file position
 *.  OSFSK_END - set position relative to the end of the file 
 */
int osfseek(osfildef *fp, long pos, int mode)
{
	return fseek(fp, pos, mode);
}

/* 
 *   Close a file.  
 */
void osfcls(osfildef *fp)
{
	fclose(fp);
}

/* 
 *   Delete a file.  Returns zero on success, non-zero on error. 
 */
int osfdel(const char *fname)
{
	return remove(fname);
}

/* 
 *   Access a file - determine if the file exists.  Returns zero if the
 *   file exists, non-zero if not.  (The semantics may seem a little
 *   weird, but this is consistent with the conventions used by most of
 *   the other osfxxx calls: zero indicates success, non-zero indicates an
 *   error.  If the file exists, "accessing" it was successful, so osfacc
 *   returns zero; if the file doesn't exist, accessing it gets an error,
 *   hence a non-zero return code.)  
 */
int osfacc(const char *fname)
{
	return access(fname, 0);
}

/* 
 *   Get a character from a file.  Provides the same semantics as fgetc().
 */
int osfgetc(osfildef *fp)
{
	return getc(fp);
}

