#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/test_gets.cpp,v 1.2 1999/05/17 02:52:31 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_gets.cpp - test reading from an input file
Function
  
Notes
  
Modified
  04/14/99 MJRoberts  - Creation
*/

#include "os.h"
#include "t3std.h"
#include "tcsrc.h"
#include "resload.h"
#include "vmimage.h"
#include "t3test.h"


static void errexit(const char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

int main(int argc, char **argv)
{
    CTcSrcFile *src;
    CResLoader *resl;
    int lnum;
    int charset_error;
    int default_charset_error;
    char pathbuf[OSFNMAX];
    
    /* initialize for testing */
    test_init();

    /* make sure we got an input file argument */
    if (argc != 2)
        errexit("usage: test_gets <input-text-filename>");

    /* create a resource loader */
    os_get_special_path(pathbuf, sizeof(pathbuf), argv[0], OS_GSP_T3_RES);
    resl = new CResLoader(pathbuf);

    /* create a reader for the file */
    src = CTcSrcFile::open_source(argv[1], resl, 0, &charset_error,
                                  &default_charset_error);
    if (src == 0)
    {
        if (default_charset_error)
            errexit("unable to open default character set");
        else
            errexit("unable to create reader for input file");
    }

    /* warn on #charset errors */
    if (charset_error)
        printf("warning: cannot load character set specified in #charset");

    /* read lines and display them */
    for (lnum = 1 ; ; ++lnum)
    {
        char buf[256];

        /* read a line */
        if (src->read_line(buf, sizeof(buf)))
            break;

        /* display what we read */
        printf("%d:%s\n", lnum, buf);
    }

    /* delete our objects */
    delete src;
    delete resl;

    /* success */
    return 0;
}


