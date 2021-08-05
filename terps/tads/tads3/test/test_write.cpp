/* 
 *   Copyright 1999, 2002 Michael J. Roberts
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
 *   Write test T3 image file.  
 */

#include <stdlib.h>
#include <stdio.h>

#include "t3std.h"
#include "vmfile.h"
#include "vmwrtimg.h"
#include "t3test.h"

/*
 *   display an argument and exit 
 */
static void errexit(const char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

int main(int argc, char **argv)
{
    CVmFile *fp = 0;
    CVmImageWriter *volatile iw = 0;

    /* initialize for testing */
    test_init();

    /* initialize error stack */
    err_init(1024);

    /* check arguments */
    if (argc != 2)
        errexit("usage: test_write <output-file>");

    /* open the file */
    fp = new CVmFile();

    err_try
    {
        static const char *meta[] =
        {
            "tads-object"
        };
        static const char *bif[] =
        {
            "tads/030000"
        };
        static const unsigned char code0[] =
        {
            /* 
             *   method header 
             */
            0x00,                                        /* parameter count */
            0x00,                                               /* reserved */
            0x02, 0x00,                             /* local variable count */
            0x20, 0x00,                              /* maximum stack usage */
            0x00, 0x00,                           /* exception table offset */
            0x00, 0x00,                         /* debugging records offset */
            0x00, 0x00, 0x00, 0x00,                      /* defining object */

            /*
             *   code 
             */

            // local i;   // -> local #0
            // local j;   // -> local #1
            //
            // say('hello, world!\n');
            0x05, 0x00, 0x00, 0x00, 0x00, // pushstr  0x00000000
            0xB1, 0x00, 0x01,         //    builtin_a 0
            
            // for (i := 1 ; i < 100 ; ++i)
            // {
            0xDA, 0x00,               //    onelcl1   0
                                      // $1:
            0x80, 0x00,               //    getlcl1   0
            0x03, 0x64,               //    pushint8  100
            0x96, 0x29, 0x00,         //    jgt       $2  ; +41

            //   j := i*3;
            0x80, 0x00,               //    getlcl1   0
            0x03, 0x03,               //    pushint8  3
            0x24,                     //    mul
            0xE0, 0x01,               //    setlcl1   1

            //   say('i = ' + i + ', j = ' + j + '\n');
            0x05, 0x10, 0x00, 0x00, 0x00, // pushstr  0x00000010
            0x80, 0x00,               //    getlcl1   0
            0x22,                     //    add
            0x05, 0x16, 0x00, 0x00, 0x00, // pushstr  0x00000016
            0x22,                     //    add
            0x80, 0x01,               //    getlcl1   1
            0x22,                     //    add
            0x05, 0x1E, 0x00, 0x00, 0x00, // pushstr  0x0000001E
            0x22,                     //    add
            0xB1, 0x00, 0x01,         //    builtin_a 0
            
            // } // end of for
            0xD0, 0x00, 0x00,         //    inclcl    0
            0x91, 0xD4, 0xFF,         //    jmp       $1  ; -44
                                      // $2:

            // END OF FUNCTION
            0x51                      //    retnil
        };
        static const unsigned char const0[] =
        {
            /* 0 = 0x0000 */
            /* string "hello, world!\n" */
            0x0E, 0x00,  // length = 14
            'h', 'e', 'l', 'l', 'o', ',', ' ',
            'w', 'o', 'r', 'l', 'd', '!', 0x0A,

            /* 16 = 0x0010 */
            /* string "i = " */
            0x04, 0x00,  // length = 4
            'i', ' ', '=', ' ',

            /* 22 = 0x0016 */
            /* string ", j = " */
            0x06, 0x00,  // length = 6
            ',', ' ', 'j', ' ', '=', ' ',

            /* 30 = 0x001E */
            /* string "\n" */
            0x01, 0x00,  // length = 1
            0x0A
        };
        static const char tool_data[4] = { 't', 'e', 's', 't' };
        
        /* open the output file */
        fp->open_write(argv[1], OSFTT3IMG);

        /* create an image writer */
        iw = new CVmImageWriter(fp);

        /* start the file */
        iw->prepare(1, tool_data);

        /* write the entrypoint - enter at code offset zero */
        iw->write_entrypt(0, 14, 10, 10, 2, 6, 4, 1);

        /* write an empty function set dependency table */
        iw->write_func_dep(bif, sizeof(bif)/sizeof(bif[0]));

        /* write our metaclass dependency table */
        iw->write_meta_dep(meta, sizeof(meta)/sizeof(meta[0]));

        /* write our code pool definition and pages */
        iw->write_pool_def(1, 1, 4096, TRUE);
        iw->write_pool_page(1, 0, (const char *)code0, sizeof(code0),
                            TRUE, 0);

        /* write the constant pool definition and pages */
        iw->write_pool_def(2, 1, 4096, TRUE);
        iw->write_pool_page(2, 0, (const char *)const0, sizeof(const0),
                            TRUE, 0);

        /* finish the image file data */
        iw->finish();
    }
    err_catch(exc)
    {
        printf("exception caught: %d\n", exc->get_error_code());
    }
    err_end;

    /* delete the image writer */
    if (iw != 0)
        delete iw;
    
    /* release our file object */
    if (fp != 0)
        delete fp;

    /* delete the error context */
    err_terminate();

    /* done */
    return 0;
}

