/* Emglken implementation */

#ifndef OSEMGLKEN_H
#define OSEMGLKEN_H

#include "glk.h"
#include "osifctyp.h"

/* File handle structure for osfxxx functions. */
typedef struct glk_stream_struct osfildef;

int osfacc(const char *fname);
void osfcls(osfildef *fp);
int osfdel(const char *fname);
int osfflush(osfildef *fp);
int osfgetc(osfildef *fp);
char *osfgets(char *buf, size_t len, osfildef *fp);
osfildef *osfoprb(const char *fname, os_filetype_t typ);
osfildef *osfoprt(const char *fname, os_filetype_t typ);
osfildef *osfoprwtb(const char *fname, os_filetype_t typ);
osfildef *osfoprwtt(const char *fname, os_filetype_t typ);
osfildef *osfopwb(const char *fname, os_filetype_t typ);
osfildef *osfopwt(const char *fname, os_filetype_t typ);
long osfpos(osfildef *fp);
int osfputs(const char *buf, osfildef *fp);
int osfrb(osfildef *fp, void *buf, int bufl);
size_t osfrbc(osfildef *fp, void *buf, size_t bufl);
int osfseek(osfildef *fp, long pos, int mode);
int osfwb(osfildef *fp, const void *buf, int bufl);
int os_rename_file(const char *oldname, const char *newname);

#endif /* OSEMGLKEN_H */