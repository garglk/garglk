TADS 3 Test Suite
Copyright (c) 1999, 2012 by Michael J. Roberts.  All Rights Reserved.

The author hereby grants permission to anyone to use the files
contained in this test suite in conjunction with testing TADS on any
platform.  Anyone may copy and distribute these files, provided that
the full test suite is included without changes in any copies
distributed, and that this copyright notice is retained without
changes in any copies.

These files provide a test suite for TADS 3.  The test harness
consists of several portable command-line programs, some Windows
command scripts ("batch files," whose filenames end in .bat), a
set of TADS 3 source files (.t), and references logs (.log).

The test suite is automated, and consists of a series of individual
tests.  For each test, the test harness compiles a source file, then
runs the resulting program, capturing its (stdout) output into a file.
The test harness then compares the captured output with a reference
log which stores the correct results.  If the captured output and
the reference log are byte-for-byte identical, the test succeeds;
otherwise, the test fails, because the program did not produce the
correct results.

In order to use the test suite with a non-Windows system, it's
necessary to translate the Windows command scripts into your local
shell scripting language.  The scripts are simple and should be
reasonably self-explanatory.  Next, you must build the test programs;
these should be easily buildable on a system to which the rest of
TADS 3 has been ported, and you can refer to the Windows makefile for
an example of the build configuration necessary for compiling these
programs.  Finally, you must have available a command shell that is
capable of capturing a program's stdout and stderr output (on
Windows, we use capture.cpp, but this program is specific to Windows
and is not portable to other systems), and a "diff" utility that can
textually compare two files to determine if they have any
differences.  Tools of this nature are available on most systems with
C compilers, since these are essential programmer's tools.

For contact information, please visit www.tads.org.
