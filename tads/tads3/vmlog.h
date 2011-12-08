/* $Header$ */

/* Copyright (c) 2011 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmlog.h - system log file
Function
  Functions for writing messages to the system log file
Notes
  Strings passed to vm_log_xxx functions are expected to be in the local
  file system character set, unless otherwise noted per function.  These
  functions generally just write the bytes of the strings as given,
  without any character set mapping, so it's up to the caller to map to
  the file character set if necessary.  Plain ASCII strings don't need
  mapping, since virtually all modern machines use character sets that
  include ASCII as a subset - this includes UTF-8, ISO-8859-X, Win CP12xx,
  etc, but excludes 16-bit Unicode mappings (UCS-2) and EBCDIC.
Modified
  12/01/11 MJRoberts  - Creation
*/

#ifndef VMLOG_H
#define VMLOG_H

#include "t3std.h"
#include "vmglob.h"

/* log a message with sprintf-style formatting */
void vm_log_fmt(VMG_ const char *fmt, ...);

/* log a message string */
void vm_log(VMG_ const char *str, size_t len);

#endif /* VMLOG_H */

