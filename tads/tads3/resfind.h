/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  resfind.cpp - find a multimedia resource in a tads 2 or tads 3 game file
Function
  Searches a compiled tads 2 game file or a compiled tads 3 image file
  for a multimedia resource of a given name.  The caller doesn't have to
  know which tads version created the file; we'll sense the file type and
  parse it accordingly.

  This implementation is independent of any tads 2 or tads 3 subsystem
  except osifc, which it uses for portable file I/O and portable byte format
  conversions.
Notes

Modified
  09/24/01 MJRoberts  - Creation
*/

#ifndef RESFIND_H
#define RESFIND_H

/*
 *   Resource locator.  This gives the seek offset and size of a resource
 *   within a larger file. 
 */
struct tads_resinfo
{
    /* seek location of start of resource */
    unsigned long seek_pos;

    /* size of resource in bytes */
    unsigned long siz;
};

/*
 *   Find a multimedia resource with the given name in the given file.  The
 *   file must be positioned at the start of the tads game file when we're
 *   invoked - this allows searching for a resource within a game file that
 *   is embedded in a larger file stream, since we don't care where within
 *   the osfildef stream the tads game file starts.
 *   
 *   Fills in the resource information structure with the seek offset and
 *   size of the resource in the file and returns true if the resource is
 *   found; returns false if a resource with the given name doesn't exist in
 *   the file.  
 */
int tads_find_resource_fp(osfildef *fp, const char *resname,
                          tads_resinfo *info);

/*
 *   Find a resource in a file, given the filename. 
 *   
 *   Fills in the resource information structure with the seek offset and
 *   size of the resource in the file and returns true if the resource is
 *   found; returns false if a resource with the given name doesn't exist in
 *   the file.  
 */
int tads_find_resource(const char *fname, const char *resname,
                       tads_resinfo *info);


#endif /* RESFIND_H */
