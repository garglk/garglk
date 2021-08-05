Text Adventure Development System - TADS 3
Source Code Distribution
Copyright (c) 1999, 2003 by Michael J. Roberts.  All Rights Reserved.


This is the C++ source code for TADS 3.  In order to help make TADS
available on a wide range of computer systems, we're distributing
this source code.  TADS 3 and this source code are copyrighted works,
so please read the license information below if you're going to use
these files.  We want to keep TADS consistent in features and
functionality across all of the different computers it works on, so
we ask that you do not make any changes to this code beyond what is
necessary to make TADS work on your type of computer system.  In
addition, the author retains the rights to TADS and any derivative
works based on this source code.

If your main interest in TADS is for writing or playing a game, you
probably don't need this source code, since executable versions of
TADS are available for a number of platforms.  This source code is
meant primarily for people who want to port TADS to new systems,
or track down and fix problems with the system code itself.


------------------------------------------------------------------------------
HOW TO PORT TADS 3 TO A NEW PLATFORM

To use this code, you will need a C++ compiler, and you may have to
do some C or C++ coding to customize TADS to your system, in order to
build a working version of TADS from this source code.

This code depends upon the TADS 2 Operating System Interface (osifc)
layer, which is not included with this distribution.  You can obtain
the TADS 2 source code, including TADS 2 osifc implementations for
many operating systems, via FTP from

  ftp://ftp.ifarchive.org/if-archive/programming/tads2/source

(Note that ftp.ifarchive.org is the new home, effective August 2001,
of the former ftp.gmd.de archive.)

TADS 3 uses the identical osifc implementation that TADS 2 uses.
This means that if TADS 2 has been ported to your operating system,
you will probably not have to write any new code to port TADS 3 - all
you'll have to do is create a makefile (or your local equivalent) and
build TADS 3 using the same osifc implementation files that you used
for TADS 2.

If TADS 2 has not already been ported to your system, you must first
write the code necessary to implement the osifc layer for your
platform.  You don't need to port all of TADS 2, but you must port at
least the osifc code.  Refer to the file PORTNOTE.TXT in the TADS 2
source code distribution for information on how to do this.



------------------------------------------------------------------------------
CONTACTING THE AUTHOR

For contact information, please visit www.tads.org.

If you port this software to a new platform, I'd appreciate hearing
about any changes you had to make to the portable code to make TADS
work on your system.  My goal is for the portable part of the code to
compile and run everywhere without any changes; all of the
port-specific code should be isolated to the "osifc" layer.  Any time
the portable part of the code proves to be less than portable, I
consider it a bug and will try to fix it.


------------------------------------------------------------------------------
COPYRIGHT INFORMATION

TADS 3 is "freeware," which means that it's copyrighted software that
the author makes available free of charge but subject to certain
conditions.  Please see the accompanying license file LICENSE.TXT for
license information.

