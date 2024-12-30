## Version 0.7, Treaty of Babel Revision 12

This is the source code for `babel`, the [Treaty of Babel][babel] analysis tool. `Babel` performs all the fundamental operations for dealing with the features offered by the treaty, and serves as a flexible multi-tool for analyzing and working with treaty-compliant files.

[babel]: https://babel.ifarchive.org/

Namely, `babel`:

* extracts metadata from story files
* extracts cover art from story files
* determines the format of a story file
* determines the IFID of a story file
* verifies the correctness of iFiction metadata
* bundles story files, metadata, and cover art together in the [blorb format][blorb]

[blorb]: https://eblong.com/zarf/blorb/

`Babel` is also a portable C API for accessing the functionality provided by the treaty.

Included with the `babel` source is a suite of programs intended to extend `babel`'s functionality and help users start taking advantage of the benefits provided by the treaty.

The babel suite includes:

* `babel-get`: Fetches metadata and cover art from a variety of sources
* `babel-list`: Demo program to describe all story files in a directory
* `ifiction-aggregate`: Combine multiple iFiction files into one
* `ifiction-xtract`: Search metadata for a named value
* `babel-cache.pl`: Build a cache of metadata and cover art for your story file collection
* `babel-marry.pl`: Encapsulate your story collection with its metadata
* `simple-marry`: Simplified babel-marry for windows users
* `babel-wed.pl`: Find and merge a story file with its metadata (also available as windows executable)

Parts of `babel` are intended for use in other programs.  When adapting
`babel`'s source, the files `babel.c`, `babel_story_functions.c` and
`babel_ifiction_functions.c` will probably not prove useful.  However, you
may wish to use `babel_handler`, which provides a framework for loading a
story file, selecting the proper treaty modules, and seamlessly handling
blorb-wrapped files.

## Building Babel

To build `babel`:

1. compile all the source files in this directory
2. link them together
3. the end

For folks who find makefiles more useful than generalizations, there is a
makefile provided for `babel`.  The makefile is currently configured for
Borland's 32-bit C compiler.  Comment out those lines and uncomment the block
which follows for gcc.

To compile `babel-get`, first compile `babel`, then do the same thing in the
`babel-get` directory.

To compile `ifiction-aggregate`, `ifiction-xtract`, `babel-list`, and `simple-marry`,
first compile `babel`, then compile the relevant C file in the `extras/` directory
(These may rely on `#include` files from the `babel` directory, so, for example,
to compile ifiction-aggregate, `gcc -c -I.. ifiction-aggregate.c`), then link the
object file to the `babel` and `ifiction` libraries (`babel.lib` and `ifiction.lib`
under Windows, `babel.a` and `ifiction.a` most everywhere else.  eg.
`gcc -o ifiction-aggregate ifiction-aggregate.o ../babel.a ../ifiction.a`)

## Contributing to Babel

`Babel` is intended to accept contributions in the form of treaty modules
as defined by the [Treaty of Babel Appendix][treaty-a].

[treaty-a]: https://babel.ifarchive.org/babel.html#appendix-babel-a-users-guide

These modules should use the declarations made in `treaty.h`.
The file `treaty_builder.h` generates a generic framework which simplifies
the task of writing treaty modules.  Its use is not required for treaty
compliance, but it should prove useful.

## Attribution

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
