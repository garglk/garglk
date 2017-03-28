#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *.  This file is part of TADS 3.
 *   
 *   This file includes all of the standard TADS system headers, so programs
 *   can include this header alone rather than having to include all of the
 *   separate headers individually.  
 */


/*
 *   To allow the standard library and header files to be used with
 *   alternative I/O intrinsics, we explicitly use macros for the I/O
 *   dependencies in the library - specifically, the I/O intrinsic header
 *   file name, and the name of the default stream output function.
 *   
 *   We define defaults for these if they aren't otherwise defined.  To
 *   compile library files with an alternative set of I/O intrinsics, define
 *   these symbols in the compiler (for example, by using the -D command-line
 *   option with t3make).  
 */
#ifndef TADS_IO_HEADER
#define TADS_IO_HEADER "tadsio.h"
#endif
#ifndef _tads_io_say
#define _tads_io_say(str) tadsSay(str)
#endif


/* include the T3 VM intrinsic function set */
#include "t3.h"

/* include the TADS general data manipulation function set */
#include "tadsgen.h"

/* include the TADS input/output function set */
#include TADS_IO_HEADER

/* include the system type definitions */
#include "systype.h"

/* if we're building a network program, include the net headers */
#ifdef TADS_INCLUDE_NET
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Break out of a callback iteration, such as a forEachInstance() loop.
 *   This can be used within the callback code to break out of the loop.  
 */
#define breakLoop throw BreakLoopSignal


/* ------------------------------------------------------------------------ */
/*
 *   Define a property value using an expression that's evaluated once per
 *   instance of the class where the property is defined.  This is used like
 *   so:
 *   
 *   class MyClass: MySuperClass
 *.    prop1 = perInstance(new SubObject())
 *.  ;
 *   
 *   Now, for each instance of MyClass, prop1 will be set to a separate
 *   instance of SubObject.
 *   
 *   Note that the per-instance property value is set "on demand" in each
 *   instance.  This means that a particular instance's copy of the property
 *   will be set when the property is first evaluated.  Note in particular
 *   that the value won't necessary be computed at compile time or during
 *   pre-initialization, because the value for a particular instance won't be
 *   calculated until the property is first used for a that instance.  
 */
#define perInstance(expr) (self.(targetprop) = (expr))


