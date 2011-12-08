/* Copyright (c) 2003 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnogen.c - module for use when osgen.c (or osgen3.c) module isn't used
Function
  This module defines externals needed by osdos.c.  These externals are
  normally provided by osgen.c or osgen3.c.  When those modules aren't
  needed, but osdos.c is, this module can be linked in to provide the
  missing externals.
Notes
  
Modified
  08/30/03 MJRoberts  - Creation
*/

/* "plain" mode flag */
int os_f_plain = 1;

void os_set_save_ext(const char *ext)
{
}
char *os_get_save_ext()
{
    return 0;
}
