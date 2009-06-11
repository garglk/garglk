/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  gameinfo.h - extract and parse tads 2/3 game file information
Function
  Searches a compiled tads 2 game file or a compiled tads 3 image file
  for a game information resource.  If the resource is found, extracts and
  parses the resource.

  A game information resource is a set of name/value pairs giving
  information on the game, such as its name, author, and description.  Each
  value is simply a text string.  The game information is supplied by the
  author, and is intended to help archives and users by providing a way to
  obtain specific information about the game in a structured format directly
  from the game file itself.

  This implementation is independent of any tads 2 or tads 3 subsystem
  except osifc, which it uses for portable file I/O and portable byte format
  conversions.  We also depend upon the 'resfind' module, which extracts
  multimedia resources from tads game files.
Notes

Modified
  09/24/01 MJRoberts  - Creation
*/

#ifndef GAMEINFO_H
#define GAMEINFO_H

#include "os.h"

/*
 *   Base game information parser.  This base version, when used directly,
 *   uses UTF-8 encoding for all value strings.  If values are desired in a
 *   different character set, use the CTadsGameInfoLocal subclass.  
 */
class CTadsGameInfo
{
public:
    /* create */
    CTadsGameInfo();

    /* delete the object */
    virtual ~CTadsGameInfo();

    /*
     *   Parse a game file and extract the game information.  Returns FALSE
     *   if there is no game information in the file, the file is not a
     *   valid game file, or any other error occurs.  Returns TRUE on
     *   success.  
     */
    int read_from_file(const char *fname);

    /*
     *   Parse a file that's already open and positioned to the start of the
     *   game file data.  This can be used to parse a game file data stream
     *   that is embedded in a larger file.
     */
    int read_from_fp(osfildef *fp);

    /* 
     *   Find a value given the value name.  Returns a pointer to a
     *   null-terminated value string, which uses the character set
     *   appropriate to the game information reader; this is UTF-8 for this
     *   base class.  (The pointer returned refers to memory we manage, so
     *   the pointer is valid as long as 'this' is valid - i.e., as long as
     *   'this' isn't deleted.)  Returns null if there is no value with the
     *   given name.
     *   
     *   The name is given as an ASCII string and is insensitive to case.  
     */
    const char *get_val(const char *name) const;

    /*
     *   Enumerate all of the named values.  Invokes the callback interface
     *   (cb->tads_enum_game_info()) for each value.
     */
    void enum_values(class CTadsGameInfo_enum *cb);

protected:
    /* 
     *   Store a value string.  The value is given as UTF-8; the result
     *   should be in the appropriate character set for the subclass.  This
     *   is virtualized to allow subclasses to translate the value's
     *   character set before storing the value.
     *   
     *   The given value string is null-terminated, and is in memory managed
     *   by the class and retained as long as the class is valid.
     *   Implementations can thus return the given value string unchanged,
     *   if desired.  
     */
    virtual const char *store_value(const char *val, size_t len)
    {
        /*
         *   The base version can simply return a reference to the original
         *   string, since we want to store the values in UTF-8 as given.  
         */
        return val;
    }

    /* free a value string previously stored with store_value() */
    virtual void free_value(const char *)
    {
        /* 
         *   The base version doesn't allocate the value strings separately
         *   (it just uses the original pool of strings), so we don't have
         *   to free the strings separately.  We thus simply do nothing.
         */
    }

    /* free the value list */
    void free_value_list();

    /* parse the given file data; returns true on success, false on error */
    int parse_file(osfildef *fp, unsigned long res_seek_pos,
                   unsigned long res_size);
    
    /* buffer containing the contents of the game information */
    char *buf_;

    /* our list of name/value pairs */
    struct tads_valinfo *first_val_;
};

/*
 *   Game information extensions for the local character set.  This subclass
 *   has some extra functions that allow retrieving the game information in
 *   the local character set, rather than in utf-8.  By default, we use the
 *   local display character set as specified by os_get_charmap(), but the
 *   caller can specify a character set explicitly by name, or by giving us
 *   a character mapper object.  
 */
class CTadsGameInfoLocal: public CTadsGameInfo
{
public:
    /* initialize with the default character mapper */
    CTadsGameInfoLocal(const char *argv0);

    /* initialize with the named character mapper */
    CTadsGameInfoLocal(const char *argv0, const char *charset_name);

    /* initialize with the given character mapper */
    CTadsGameInfoLocal(class CCharmapToLocal *charmap);

    /* delete */
    ~CTadsGameInfoLocal();

protected:
    /* store a value in the local character set */
    virtual const char *store_value(const char *val, size_t len);

    /* free a value string previously stored with store_value() */
    virtual void free_value(const char *);

    /* initialize from a named character mapping */
    void init(const char *argv0, const char *charset_name);
    
    /* 
     *   our unicode-to-local character mapper, for the local character set
     *   methods; this is null if we haven't had a need for it yet 
     */
    class CCharmapToLocal *local_mapper_;
};



/*
 *   Enumerator interface.  This interface must be implemented in order to
 *   call CTadsGameInfo::enum_values().  
 */
class CTadsGameInfo_enum
{
public:
    /*
     *   Receive one name/value pair.  'name' points to a null-terminated
     *   ASCII string giving the value's name, and 'val' points to a
     *   null-terminated string giving the value text.  The value string
     *   will be encoded in the character set which the GameInfo reader is
     *   using.
     *   
     *   The name and value pointers refer to memory managed as part of the
     *   CTadsGameInfo object doing the enumerating.  The memory to which
     *   these pointers refer is valid as long as the CTadsGameInfo object
     *   is valid (i.e., until it's deleted).  
     */
    virtual void tads_enum_game_info(const char *name, const char *val) = 0;
};

#endif /* GAMEINFO_H */
