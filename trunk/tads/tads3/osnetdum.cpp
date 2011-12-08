/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnetdum.cpp - dummy implementation of TADS networking for non-supporting
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

#include "t3std.h"
#include "osifcnet.h"

void os_net_init(VMG0_)
{
}

void os_net_cleanup()
{
}

void osnet_disconnect_webui(int)
{
}
