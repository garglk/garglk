#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os0trxb.c - os0tr for TRX.EXE (16-bit protected-mode DOS version)
Function
  
Notes
  
Modified
  03/22/00 MJRoberts  - Creation
*/

#include "os.h"
#include "std.h"
#ifdef __DPMI16__
#include "ltk.h"
#endif
#include "trd.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static int main0(int argc, char **argv)
{
    int err;
    char *config_name;

    config_name = "config.tr";
#ifdef __DPMI16__
    ltkini(16384);
#else
#endif

    /*
     *   Scan for -plain argument, and apply if found 
     */
    {
        int i;
        for (i = 1 ; i < argc ; ++i)
        {
            if (!strcmp(argv[i], "-plain"))
                os_plain();
        }
    }

    os_init(&argc, argv, (char *)0, (char *)0, 0);

    os_instbrk(1);
    err = os0main2(argc, argv, trdmain, "", config_name, 0);
    os_instbrk(0);

    /* uninitialize the OS layer */
    os_uninit();

    return(err);
}

#define NEW_STACK_SIZE 65000

int main(int argc, char **argv)
{
    char *stk;
    
    /* create our own stack segment */
    stk = (char *)malloc(NEW_STACK_SIZE);
    if (stk == 0)
    {
        /* 
         *   couldn't create the expanded stack - take our chances with
         *   the original one 
         */
        return main0(argc, argv);
    }
    else
    {
        static int s_argc;
        static char **s_argv;
        static int s_ret;
        unsigned int s_seg;
        unsigned int s_ofs;

        /* 
         *   remember argc and argv in statics, since we're going to lose
         *   access to our stack momentarily 
         */
        s_argc = argc;
        s_argv = argv;

        /* extract the segment and offset from our stack memoryu pointer */
        s_seg = (unsigned int)((((unsigned long)stk) >> 16) & 0xffff);
        s_ofs = (unsigned int)(((unsigned long)stk) & 0xffff);

        /* clear the last 256 bytes of our stack */
        memset(stk + NEW_STACK_SIZE - 256, 0, 256);

        /* switch to our new stack */
        _asm {
            /* calculate the location of the top of our stack */
            mov  ax, s_seg
            mov  bx, s_ofs
            add  bx, NEW_STACK_SIZE - 256

            /* move ss:sp to the top of our new stack */
            mov  ss, ax
            mov  sp, bx

            /* set up bp as well */
            add bp, 2
            mov  bp, bx
        }

        /* run the program */
        s_ret = main0(s_argc, s_argv);

        /* 
         *   terminate without returning, since we're on this weird
         *   special stack we allocated 
         */
        exit(s_ret);
    }
}

