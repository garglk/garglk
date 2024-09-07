/* Emglken implementation */

#include <stddef.h>
#include "glk.h"
#include "glkstart.h"
#include "osemglken.h"
#include "osifctyp.h"

/* Access a file - determine if the file exists. */
int osfacc(const char *fname) {
    if (fname[0] == '\0') {
        return 1;
    }
    frefid_t fref = glkunix_fileref_create_by_name_uncleaned(0, fname, 0);
    if (fref) {
        int result = glk_fileref_does_file_exist(fref);
        glk_fileref_destroy(fref);
        if (result) {
            return 0;
        }
    }
    return 1;
}

/* Close a file. */
void osfcls(osfildef *fp) {
    glk_stream_close(fp, 0);
}

/* Delete a file. */
int osfdel(const char *fname) {
    if (fname[0] == '\0') {
        return -1;
    }
    frefid_t fref = glkunix_fileref_create_by_name_uncleaned(0, fname, 0);
    if (fref) {
        glk_fileref_delete_file(fref);
        glk_fileref_destroy(fref);
    }
    return 0;
}

/* Flush buffered writes to a file. */
int osfflush(osfildef *fp) {
    return 0;
}

/* Get a character from a file. */
int osfgetc(osfildef *fp) {
    return glk_get_char_stream(fp);
}

/* Get a line of text from a text file. */
char *osfgets(char *buf, size_t len, osfildef *fp) {
    glui32 count = glk_get_line_stream(fp, buf, len);
    if (count == 0) {
        return NULL;
    }
    return buf;
}

osfildef *open_file_generic(const char *fname, glui32 binary, glui32 fmode, glui32 trunc) {
    frefid_t fref = glkunix_fileref_create_by_name_uncleaned(binary, fname, 0);
    if (fref) {
        if (trunc) {
            glk_fileref_delete_file(fref);
        }
        osfildef *result = glk_stream_open_file(fref, fmode, 0);
        glk_fileref_destroy(fref);
        return result;
    }
    return NULL;
}

/* Open binary file for reading. */
osfildef *osfoprb(const char *fname, os_filetype_t typ) {
    return open_file_generic(fname, fileusage_BinaryMode, filemode_Read, 0);
}

/* Open text file for reading. */
osfildef *osfoprt(const char *fname, os_filetype_t typ) {
    return open_file_generic(fname, fileusage_TextMode, filemode_Read, 0);
}

/* Open binary file for reading/writing.  If the file already exists,
 * truncate the existing contents.  Create a new file if it doesn't
 * already exist. */
osfildef *osfoprwtb(const char *fname, os_filetype_t typ) {
    return open_file_generic(fname, fileusage_BinaryMode, filemode_ReadWrite, 1);
}

/* Open text file for reading/writing.  If the file already exists,
 * truncate the existing contents.  Create a new file if it doesn't
 * already exist. */
osfildef *osfoprwtt(const char *fname, os_filetype_t typ) {
    return open_file_generic(fname, fileusage_TextMode, filemode_ReadWrite, 1);
}

/* Open binary file for writing. */
osfildef *osfopwb(const char *fname, os_filetype_t typ) {
    return open_file_generic(fname, fileusage_BinaryMode, filemode_Write, 0);
}

/* Open text file for writing. */
osfildef *osfopwt(const char *fname, os_filetype_t typ) {
    return open_file_generic(fname, fileusage_TextMode, filemode_Write, 0);
}

/* Get the current seek location in the file. */
long osfpos(osfildef *fp) {
    return glk_stream_get_position(fp);
}

/* print a null-terminated string to osfildef* file */
void os_fprintz(osfildef *fp, const char *str) {
    glk_put_string_stream(fp, (char*) str);
}

/* print a counted-length string (which might not be null-terminated) to a file */
void os_fprint(osfildef *fp, const char *str, size_t len) {
    glk_put_buffer_stream(fp, (char*) str, len);
}

/* Write a line of text to a text file. */
int osfputs(const char *buf, osfildef *fp) {
    glk_put_string_stream(fp, (char*) buf);
    return 0;
}

/* Read bytes from file. */
int osfrb(osfildef *fp, void *buf, int bufl) {
    glk_get_buffer_stream(fp, buf, bufl);
    return 0;
}

/* Read bytes from file and return the number of bytes read. */
size_t osfrbc(osfildef *fp, void *buf, size_t bufl) {
    return glk_get_buffer_stream(fp, buf, bufl);
}

/* Seek to a location in the file. */
int osfseek(osfildef *fp, long pos, int mode) {
    glk_stream_set_position(fp, pos, mode);
    return 0;
}

/* Write bytes to file. */
int osfwb(osfildef *fp, const void *buf, int bufl) {
    glk_put_buffer_stream(fp, (void*) buf, bufl);
    return 0;
}

/* Rename a file. */
// I don't think we should really need this, as it's only in net code, but TADS won't compile without it
int os_rename_file(const char *oldname, const char *newname) {
    return 0;
}