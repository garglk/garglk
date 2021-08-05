#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/test_chr.cpp,v 1.2 1999/05/17 02:52:31 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_chr.cpp - character mapper test 
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#include "charmap.h"
#include "resload.h"
#include "vmimage.h"
#include "t3test.h"

int main(int argc, char **argv)
{
    CResLoader *loader;
    CCharmapToLocal *map;
    static wchar_t teststr[] =
    {
        0xA9,                                             /* copyright sign */
        0xA1,                                  /* inverted exclamation mark */
        0xA2,                                                  /* cent sign */
        0xD0,                                                        /* eth */
        0x17A,                                 /* small letter z with acute */
        0x102,                               /* capital letter a with breve */
        0x401,                                /* cyrillic capital letter IO */
        0x622,                                               /* arabic alef */
        0x3B1,                                         /* greek small alpha */
        0x3B2,                                          /* greek small beta */
        0x30,                                                    /* digit 0 */
        0x31,                                                    /* digit 1 */
        0x32,                                                    /* digit 2 */
        0x33,                                                    /* digit 3 */
        0x34,                                                    /* digit 4 */
        0x35,                                                    /* digit 5 */
        0x36,                                                    /* digit 6 */
        0x37,                                                    /* digit 7 */
        0x38,                                                    /* digit 8 */
        0x39,                                                    /* digit 9 */
        0x41,                                                  /* capital A */
        0x42,                                                  /* capital B */
        0x43,                                                  /* capital C */
        0x44,                                                  /* capital D */
        0x45,                                                  /* capital E */
        0x2248,                                        /* almost equal sign */
        0x2264,                               /* less than or equal to sign */
        0
    };
    char buf[256];
    char mapbuf[256];
    utf8_ptr up;
    size_t len;
    size_t i;
    char pathbuf[OSFNMAX];

    /* initialize for testing */
    test_init();

    /* create a resource loader */
    os_get_special_path(pathbuf, sizeof(pathbuf), argv[0], OS_GSP_T3_RES);
    loader = new CResLoader(pathbuf);

    /* create a single-byte translator and load it with DOS code page 437 */
    map = CCharmapToLocal::load(loader, "charmap/cp437");
    if (map == 0)
    {
        printf("unable to load cp437 mapping file\n");
        exit(1);
    }

    /* create a UTF-8 string to map */
    up.set(buf);
    up.setwcharsz(teststr, sizeof(buf));

    /* check how much space we need */
    up.set(buf);
    len = map->map_utf8z(0, 0, up);
    printf("space needed for mapping to cp437: %lu\n", (unsigned long)len);

    /* map it for real this time */
    map->map_utf8z(mapbuf, sizeof(mapbuf), up);

    /* display the result */
    for (i = 0 ; i < len ; ++i)
        printf("%02x ", (unsigned char)mapbuf[i]);
    printf("\n");

    printf("%s\n", mapbuf);

    /* success */
    exit(0);
}

