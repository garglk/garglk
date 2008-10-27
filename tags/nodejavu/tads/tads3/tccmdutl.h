/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tccmdutl.cpp - TADS 3 Compiler command-line parsing utilities
Function
  Defines some utility functions for command-line parsing.
Notes

Modified
  04/03/02 MJRoberts  - Creation
*/

#ifndef TCCMDUTL_H
#define TCCMDUTL_H

#include <stdlib.h>


/*
 *   Command utilities class.  We use a class to group these functions into
 *   a class namespace
 */
class CTcCommandUtil
{
public:
    /* get an option argument */
    static char *get_opt_arg(int argc, char **argv, int *curarg, int optlen);

    /* parse an options file */
    static int parse_opt_file(osfildef *fp, char **argv,
                              class CTcOptFileHelper *helper);
};

/*
 *   Helper interface for parse_opt_file helper.  An implementation of this
 *   interface must be provided to parse_opt_file().
 */
class CTcOptFileHelper
{
public:
    /* 
     *   Allocate an option string.  Allocates the given number of bytes of
     *   memory.  Strings allocated with this must be freed with
     *   free_opt_file_str().  
     */
    virtual char *alloc_opt_file_str(size_t len) = 0;

    /* free a string allocated with alloc_opt_file_str() */
    virtual void free_opt_file_str(char *str) = 0;

    /* 
     *   Process a comment line.  We call this for each line that we find
     *   starting with "#" and for each blank line.
     */
    virtual void process_comment_line(const char *str) = 0;

    /* process a non-comment line */
    virtual void process_non_comment_line(const char *str) = 0;

    /* 
     *   Process a configuration line.  Once we see a configuration flag (a
     *   line reading "[Config:xxx]"), we'll process all subsequent text in
     *   the file through this function.
     *   
     *   'config_id' is the 'xxx' value in the [Config:xxx] configuration
     *   flag line that started the section.  'is_id_line' is true if we're
     *   processing the "[Config:xxx]" line itself, false if it's any other
     *   line within the [Config:xxx] section.
     *   
     *   Note that this routine receives ALL lines within a configuration
     *   section, including comment lines.  Once we're inside a configuration
     *   block, the entire contents are opaque to the generic processor,
     *   since even the comment format is up to the configuration section's
     *   owner to define.  
     */
    virtual void process_config_line(const char *config_id,
                                     const char *str,
                                     int is_id_line) = 0;
};

#endif /* TCCMDUTL_H */

