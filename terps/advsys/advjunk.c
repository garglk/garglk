

#include "header.h"
#define UNIX
#include <stdio.h>

long _seed = 1L;

#ifndef UNIX
int rand()
{
    _seed *= 397204094L;
    return (_seed & 0x7FFF);
}

srand(n)
  long n;
{
   _seed = n;
}
#endif

int getch()
{
#ifdef UNIX
    return getchar();
#else
    int ch;
    if ((ch = bdos(1) & 0xFF) == '\r') { bdos(6,'\n'); ch = '\n'; }
    return (ch);
#endif
}

void waitch()
{
#ifndef UNIX
    bdos(7);
#endif
}

void putch(ch,fp)
  int ch; FILE *fp;
{
#ifdef UNIX
    putc(ch,fp);
#else
    aputc(ch,fp);
#endif
}

int advsave(char *hdr,int hlen,char *save,int slen)
{
    frefid_t fdref;
	strid_t fd;

	fdref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Write, 0);
	fd = glk_stream_open_file(fdref, filemode_Write, 0);

    glk_put_buffer_stream(fd,hdr,hlen);
	
    /* write the data */
    glk_put_buffer_stream(fd,save,slen);

    /* close the file and return successfully */
    glk_stream_close(fd, NULL);
	glk_fileref_destroy(fdref);
    return (1);
}

int advrestore(char *hdr,int hlen,char *save,int slen)
{
    char hbuf[50],*p;
	frefid_t fdref;
	strid_t fd;
	 
	
	if (hlen > 50)
        error("save file header buffer too small");

	fdref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Read, 0);
	fd = glk_stream_open_file(fdref, filemode_Read, 0);


    /* read the header */
    if (glk_get_buffer_stream(fd,hbuf,hlen) != hlen) {
        glk_stream_close(fd, NULL);
		glk_fileref_destroy(fdref);
        return (0);
    }

    /* compare the headers */
    for (p = hbuf; hlen--; )
        if (*hdr++ != *p++) {
            trm_str("This save file does not match the adventure!\n");
            return (0);
        }

    /* read the data */
    if (glk_get_buffer_stream(fd,save,slen) != slen) {
        glk_stream_close(fd, NULL);
		glk_fileref_destroy(fdref);
        return (0);
    }

    /* close the file and return successfully */
    glk_stream_close(fd, NULL);
	glk_fileref_destroy(fdref);
    return (1);
}


