/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnetdum.h - dummy implementation of TADS networking for non-supporting
  platforms
Function
  This module provides a stub implementation of the portions of the
  portable networking interface that are required to link the rest of the
  system.  This should be used on platforms that don't provide TADS
  networking functionality.
Notes
  
Modified
  08/11/10 MJRoberts  - Creation
*/

#ifndef OSNETDUM_H
#define OSNETDUM_H

class OS_Counter
{
public:
    OS_Counter(long c = 1) { cnt = c; }
    long get() const { return cnt; }
    long inc() { return ++cnt; }
    long dec() { return --cnt; }

private:
    long cnt;
};

#include "vmrefcnt.h"

class OS_Event: public CVmRefCntObj
{
public:
    OS_Event(int /*manual_reset*/) { }
    void signal() { }
    void reset() { }
};


#endif /* OSNETDUM_H */
