/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  gameinfl.cpp - game info local character set extensions
Function
  Defines functions for operating on GameInfo records in the local
  character set.  We implement these in a module separate from gameinfo.cpp
  so that callers who don't need the character set translations won't have
  to link in the character mapper module and everything it brings in.
Notes
  
Modified
  07/16/02 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>

#include <os.h>
#include "t3std.h"
#include "utf8.h"
#include "vmerr.h"
#include "charmap.h"
#include "resload.h"

#include "resfind.h"
#include "gameinfo.h"

/* 
 *   initialize with the default character mapper 
 */
CTadsGameInfoLocal::CTadsGameInfoLocal(const char *argv0)
{
    char charset_name[32];
    
    /* get the default display character set */
    os_get_charmap(charset_name, OS_CHARMAP_DISPLAY);

    /* initialize with the selected character set */
    init(argv0, charset_name);
}

/* 
 *   initialize with the named character mapper 
 */
CTadsGameInfoLocal::CTadsGameInfoLocal(const char *argv0,
                                       const char *charset_name)
{
    /* initialize with the selected character set */
    init(argv0, charset_name);
}

/* 
 *   initialize with the given character mapper 
 */
CTadsGameInfoLocal::CTadsGameInfoLocal(class CCharmapToLocal *charmap)
{
    /* remember the character set */
    local_mapper_ = charmap;
}

/*
 *   initialize with a named character set 
 */
void CTadsGameInfoLocal::init(const char *argv0, const char *charset_name)
{
    char res_path[OSFNMAX];
    CResLoader *res_loader;
    
    /* 
     *   Initialize the T3 error context, in case the caller hasn't already
     *   done so; this is necessary so that we can call the resource loader.
     *   (Note that it's harmless to re-initialize the error subsystem if
     *   it's already been initialized; the subsystem will just count the
     *   new reference and otherwise ignore the request.)  
     */
    err_init(512);

    /* set up a resource loader */
    os_get_special_path(res_path, sizeof(res_path), argv0, OS_GSP_T3_RES);
    res_loader = new CResLoader(res_path);

    /* create a character mapper for the given character set */
    local_mapper_ = CCharmapToLocal::load(res_loader, charset_name);

    /* if that failed, use a plain ascii mapper (which never fails) */
    if (local_mapper_ == 0)
        local_mapper_ = CCharmapToLocal::load(res_loader, "us-ascii");

    /* done with the resource loader - delete it */
    delete res_loader;

    /* we're done with the error subsystem, so release our reference */
    err_terminate();
}

/*
 *   Delete 
 */
CTadsGameInfoLocal::~CTadsGameInfoLocal()
{
    /* 
     *   delete the value list explicitly (we must call this in our subclass
     *   destructor, even though the base class destructor will also call
     *   it, because we override free_value() and thus need to make sure the
     *   free_value() invocations happen in the context of the subclass
     *   destructor)
     */
    free_value_list();

    /* release our reference on the local mapper */
    local_mapper_->release_ref();
}

/*
 *   Store a value string.  We'll allocate a buffer and translate the string
 *   to the local character set.  
 */
const char *CTadsGameInfoLocal::store_value(const char *val, size_t len)
{
    char *new_str;
    size_t new_len;
    utf8_ptr p;

    /* set up a utf-8 pointer to the string */
    p.set((char *)val);

    /* get the length it takes to map the name */
    new_len = local_mapper_->map_utf8z(0, 0, p);

    /* add space for a null terminator */
    ++new_len;

    /* allocate space for the string */
    new_str = (char *)osmalloc(new_len);

    /* map the string */
    local_mapper_->map_utf8z(new_str, new_len, p);

    /* return the new string */
    return new_str;
}

/*
 *   Free a value previously stored 
 */
void CTadsGameInfoLocal::free_value(const char *val)
{
    /* we allocated the string with osmalloc, so free it with osfree */
    osfree((char *)val);
}

