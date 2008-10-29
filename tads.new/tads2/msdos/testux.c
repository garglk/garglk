/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
  testux.c - example of TADS user exit

  This is an example of how to write a TADS user exit function.  This
  particular user exit doesn't do anything useful, but it illustrates
  how to build a user exit.

  Building user exits is an inherently complex process that requires
  knowledge of C assembler programming.  Please consult your compiler
  manual for details on the operations described below.  If you use
  a different compiler than described below, you may or may not be able
  to build user exits with it; generally, you'll need the equivalent
  functionality described below to make it work.

  Building user exits varies by operating system.  We expect that most
  user exits will directly access the computer hardware or the operating
  system; user exits that do so will be non-portable, so you will have
  to rewrite the exit for each operating system.

  Note, however, that the user exit mechanism itself, apart from the
  details of compiling the C code, is portable; if your code doesn't
  perform any platform-specific hardware or operating system operations,
  and conforms to the usual C portability rules (with respect to the sizes
  of types, ordering of bytes, and so on), your C code will be portable;
  the only thing you'll have to change when porting to a different
  platform is the procedure to build the executable code.  This example
  program should be fully portable to all platforms that support the TADS
  external function feature, and should not require any source code
  changes when moving from platform to platform.

  -------------------------------------------------------------------------
  To build for 32-bit Windows platforms (Windows 95 and NT) with
  Microsoft Visual C++ 5.0 or higher:

    Compile this file with the following command (replacing "TadsDirectory"
    with the path to your TADS executables directory):

       cl /LD /I"TadsDirectory" testux.c /link /export:main /out:myfunc.dll

    This will produce a file called MYFUNC.DLL.  Put this DLL file in
    the same directory as the testux.gam file.

  -------------------------------------------------------------------------
  To build for 32-bit Windows (95 and NT) with Borland C 4.5 or later:

     First, create the file TESTUX.DEF with the following contents:

       exports
         main=_main

     Now compile this file using this command, replacing "BorlandLibDir"
     with the path to your Borland C library files (usually \BC45\LIB),
     and "TadsDir" with the path to your TADS executables:

       bcc32 -WD -L"BorlandLibDir" -I"TadsDir" testux.c

     Finally, rename the resulting DLL file to MYFUNC.DLL, so that it
     matches the name of the external function.  Put this DLL file in
     the same directory as the testux.gam file.

  -------------------------------------------------------------------------
  To build on Macintosh with Think C:
    Create a new project, and use "Source/Add..." to add this file
    Use "Project/Set Project Type..." to set the project type
      to "Code Resource", with a type of XFCN, any number for the
      resource ID, and myfunc for the name.
    Use "Project/Build Code Resource..." to compile this file and
      generate the code resource.  For testing purposes, you can
      add your XFCN to the TADS Compiler's resource fork by checking
      the "merge" box, finding the TADS Compiler, and typing its
      name; if you do this, you can use this user exit directly
      from the compiler's "-d" debug playing mode.  When you are
      ready to make a self-loading executable file from your game,
      first build your game's self-loading executable, then use
      "Project/Build Code Resource..." to add it directly (with the
      "merge" option) to your game's executable file.

  -------------------------------------------------------------------------
  To build on MS-DOS with Turbo C:
    Compile this file with tiny memory model (tcc -mt -c -DMSDOS testux.c)
    Link with tadsux.obj with /t option (tlink /t tadsux testux,testux.bin)
    Compile the game file (tc testux.t)
    Add testux.bin to the game file as a code resource
      (tadsrsc testux.gam -+testux.bin=myfunc)

  -------------------------------------------------------------------------
  To build on MS-DOS with Microsoft C 6.00 or higher:
    Compile this file with tiny memory model - be sure to define MSDOS
      (cl -c -AT -DMSDOS -Gs -Zl testux.c)
    Link with tadsux.obj (link tadsux testux,testux.exe;)
    Run the DOS program EXE2BIN on the output (exe2bin testux.exe testux.bin)
    Compile the game file (tc testux.t)
    Add testux.bin to the game file as a code resource
      (tadsrsc testux.gam -+testux.bin=myfunc)

  -------------------------------------------------------------------------
  To build on Atari ST with Mark Williams C:
    Compile this file with small model (cc -c -VSMALL testux.c)
    Link with tadsux.o:  ld -o testux.bin -s tadsux.o testux.o
    Compile the game file (tc testux.t)
    Add testux.bin to the game file as a code resource
      (tadsrsc testux.gam -+testux.bin=myfunc)

    If you use a compiler other than MWC, you will need to build using
    small model (i.e., all addressing is PC-relative) and ensure that
    the program is linked so that the entrypoint is at offset 0 within
    the .prg file.  Linking as shown above should work with other
    compilers.

    WARNING!  The MWC compiler appears to have a bug with respect to
    the -VSMALL switch.  Although the compiler correctly generates
    PC-relative addressing for subroutine calls and user data, it
    still uses absolute addressing for its internal "switch" tables
    (these are the tables the compiler builds for jumping to the
    appropriate "case" label in a "switch" statement).  Therefore,
    you cannot use "switch" statements in user exit programs; use
    an if-else tree (as in the program below) instead.

    If you encounter strange errors (probably bus or address errors),
    look at the object code your compiler generates, and make sure no
    load-time relocation fixups are needed.  User exits must be entirely
    free of relocation fixups, since the compiler just loads the
    binary image of your program and executes it starting at offset 0.
*/

#include "tadsexit.h"               /* defines needed macros and structures */


/*
 *   The main function of a user exit is always called main().  It has
 *   one argument: a context pointer (the structure is defined in
 *   tadsexit.h).
 */
int main(ctx)
tadsuxdef osfar_t *ctx;                                  /* context pointer */
{
    long num;
    int  typ;
    int  err = 0;                     /* presume that we will be successful */
    tads_strdesc oldstr;
    char osfar_t *oldp;
    char osfar_t *newstr;
    char osfar_t *p;
    int  len;

    /* get argument type */
    typ = tads_tostyp(ctx);

    /* figure out what to do based on type */
    if (typ == TADS_NUMBER)
    {
        num = tads_popnum(ctx);                           /* get the number */
        num <<= 1;                                             /* double it */
        tads_pushnum(ctx, num);                     /* return doubled value */
    }
    else if (typ == TADS_SSTRING)
    {
        /* get the string descriptor */
        oldstr = tads_popstr(ctx);
        
        /* note that this is just a descriptor - get length and text ptr */
        len = tads_strlen(ctx, oldstr);                   /* get its length */
        oldp = tads_strptr(ctx, oldstr);         /* get pointer to its text */

        /* allocate space for a new string */
        newstr = tads_stralo(ctx, len);
        
        /* reverse the string, copying into new buffer */
        oldp += len;                          /* point to END of old string */
        for (p = newstr ; len ; --len)
            *p++ = *--oldp;

        /* push the new value - use tads_pushastr, since we allocated it */
        tads_pushastr(ctx, newstr);
    }
    else if (typ == TADS_NIL)
    {
        tads_pop(ctx);
        tads_pushtrue(ctx);
    }
    else if (typ == TADS_TRUE)
    {
        tads_pop(ctx);
        tads_pushnil(ctx);
    }
    else
    {
        /* we can't handle this datatype - return an error */
        err = 1;
    }

    return err;
}
