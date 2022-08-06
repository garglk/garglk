## Version 0.6, Treaty of Babel Revision 11

This is the source code for babel, the [Treaty of Babel][babel] analysis tool.

[babel]: https://babel.ifarchive.org/

Most of this code is (c) 2006-2010 by L. Ross Raszewski

The following files are public domain:

- zcode.c
- glulx.c
- executable.c
- level9.c
- magscrolls.c
- agt.c
- hugo.c
- advsys.c
- misc.c
- alan.c
- adrift.c
- html.c
- treaty.h
- treaty_builder.h

The following files are Copyright (C) 1999, 2000, 2002 Aladdin Enterprises:

- md5.c
- md5.h

And are used in accordance with their licenses.

The following files are Copyright 2006 by Emily Short:

- test/bronze/Bronze.zblorb

All other files are (c) 2006 by L. Ross Raszewski and are released under
the Creative Commons Attribution 4.0 License.

To view a copy of this license, visit
https://creativecommons.org/licenses/by/4.0/ or send a letter to

    Creative Commons,
    PO Box 1866,
    Mountain View, CA 94042, USA.

To build babel:

1. compile all the source files in this directory
2. link them together
3. the end

For folks who find makefiles more useful than generalizations, there is a
makefile provided for babel.  The makefile is currently configured for
Borland's 32-bit C compiler.  Comment out those lines and uncomment the block
which follows for gcc.

To compile babel-get, first compile babel, then do the same thing in the
babel-get directory.

To compile ifiction-aggregate, ifiction-xtract, babel-list, and simple-marry,
first compile babel, then compile the relevant C file in the extras/ directory
(These may rely on #include files from the babel directory, so, for example,
to compile ifiction-aggregate, `gcc -c -I.. ifiction-aggregate.c`), then link the
opbject file to the babel and ifiction libraries (babel.lib and ifiction.lib
under Windows, babel.a and ifiction.a most everywhere else.  eg.
`gcc -o ifiction-aggregate ifiction-aggregate.o ../babel.a ../ifiction.a`)

Babel is intended to accept contributions in the form of treaty modules
as defined by the treaty of babel section 2.3.2.

These modules should use the declarations made in treaty.h.
The file treaty_builder.h generates a generic framework which simplifies
the task of writing treaty modules.  Its use is not required for treaty
compliance, but it should prove useful.

Parts of babel are intended for use in other programs.  When adapting
babel's source, the files babel.c, babel_story_functions.c and
babel_ifiction_functions.c will probably not prove useful.  However, you
may wish to use babel_handler, which provides a framework for loading a
story file, selecting the proper treaty modules, and seamlessly handling
blorb-wrapped files.

