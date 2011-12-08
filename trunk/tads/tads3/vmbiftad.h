/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbiftad.h - function set definition - TADS function set
Function
  
Notes
  
Modified
  12/06/98 MJRoberts  - Creation
*/

#ifndef VMBIFTAD_H
#define VMBIFTAD_H

#include "os.h"
#include "vmbif.h"
#include "utf8.h"


/* ------------------------------------------------------------------------ */
/*
 *   Random number generator configuration.  Define one of the following
 *   configuration variables to select a random number generation
 *   algorithm:
 *   
 *   VMBIFTADS_RNG_LCG - linear congruential generator
 *.  VMBIFTADS_RNG_ISAAC - ISAAC (cryptographic hash generator) 
 */

/* use ISAAC */
#define VMBIFTADS_RNG_ISAAC

#ifdef VMBIFTADS_RNG_ISAAC
#include "vmisaac.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   TADS function set built-in functions 
 */
class CVmBifTADS: public CVmBif
{
public:
    /* function vector */
    static vm_bif_desc bif_table[];

    /*
     *   General functions 
     */
    static void datatype(VMG_ uint argc);
    static void getarg(VMG_ uint argc);
    static void firstobj(VMG_ uint argc);
    static void nextobj(VMG_ uint argc);
    static void randomize(VMG_ uint argc);
    static void rand(VMG_ uint argc);
    static void toString(VMG_ uint argc);
    static void toInteger(VMG_ uint argc);
    static void toNumber(VMG_ uint argc);
    static void gettime(VMG_ uint argc);
    static void re_match(VMG_ uint argc);
    static void re_search(VMG_ uint argc);
    static void re_group(VMG_ uint argc);
    static void re_replace(VMG_ uint argc);
    static void savepoint(VMG_ uint argc);
    static void undo(VMG_ uint argc);
    static void save(VMG_ uint argc);
    static void restore(VMG_ uint argc);
    static void restart(VMG_ uint argc);
    static void get_max(VMG_ uint argc);
    static void get_min(VMG_ uint argc);
    static void make_string(VMG_ uint argc);
    static void get_func_params(VMG_ uint argc);
    static void sprintf(VMG_ uint argc);

protected:
    /* enumerate objects (common handler for firstobj and nextobj) */
    static void enum_objects(VMG_ uint argc, vm_obj_id_t start_obj);

    /* common handler for toInteger and toNumber */
    static void toIntOrNum(VMG_ uint argc, int int_only);
};


/* ------------------------------------------------------------------------ */
/*
 *   Global information for the TADS intrinsics.  We allocate this
 *   structure with the VM global variables - G_bif_tads_globals contains
 *   the structure.  
 */
class CVmBifTADSGlobals
{
public:
    /* creation */
    CVmBifTADSGlobals(VMG0_);

    /* deletion */
    ~CVmBifTADSGlobals();

    /* regular expression parser and searcher */
    class CRegexParser *rex_parser;
    class CRegexSearcherSimple *rex_searcher;

    /* 
     *   global variable for the last regular expression search string (we
     *   need to hold onto this because we might need to extract group-match
     *   substrings from it) 
     */
    struct vm_globalvar_t *last_rex_str;

    /* -------------------------------------------------------------------- */
    /*
     *   Linear Congruential Random Number Generator state 
     */
#ifdef VMBIFTADS_RNG_LCG

    /* linear congruential generator seed value */
    long rand_seed;

#endif /* VMBIFTADS_RNG_LCG */

    /* -------------------------------------------------------------------- */
    /*
     *   ISAAC Random Number Generator state
     */
#ifdef VMBIFTADS_RNG_ISAAC

    /* ISAAC state structure */
    struct isaacctx *isaac_ctx;
    
#endif /* VMBIFTADS_RNG_ISAAC */
};

/* end of section protected against multiple inclusion */
#endif /* VMBIFTAD_H */


/* ------------------------------------------------------------------------ */
/*
 *   TADS function set vector.  Define this only if VMBIF_DEFINE_VECTOR has
 *   been defined, so that this file can be included for the prototypes alone
 *   without defining the function vector.
 *   
 *   Note that this vector is specifically defined outside of the section of
 *   the file protected against multiple inclusion.  
 */
#ifdef VMBIF_DEFINE_VECTOR

/* TADS general data manipulation functions */
vm_bif_desc CVmBifTADS::bif_table[] =
{
    { &CVmBifTADS::datatype, 1, 0, FALSE },                            /* 0 */
    { &CVmBifTADS::getarg, 1, 0, FALSE },                              /* 1 */
    { &CVmBifTADS::firstobj, 0, 2, FALSE },                            /* 2 */
    { &CVmBifTADS::nextobj, 1, 2, FALSE },                             /* 3 */
    { &CVmBifTADS::randomize, 0, 0, FALSE },                           /* 4 */
    { &CVmBifTADS::rand, 1, 0, TRUE },                                 /* 5 */
    { &CVmBifTADS::toString, 1, 1, FALSE },                            /* 6 */
    { &CVmBifTADS::toInteger, 1, 1, FALSE },                           /* 7 */
    { &CVmBifTADS::gettime, 0, 1, FALSE },                             /* 8 */
    { &CVmBifTADS::re_match, 2, 1, FALSE },                            /* 9 */
    { &CVmBifTADS::re_search, 2, 1, FALSE },                          /* 10 */
    { &CVmBifTADS::re_group, 1, 0, FALSE },                           /* 11 */
    { &CVmBifTADS::re_replace, 3, 2, FALSE },                         /* 12 */
    { &CVmBifTADS::savepoint, 0, 0, FALSE },                          /* 13 */
    { &CVmBifTADS::undo, 0, 0, FALSE },                               /* 14 */
    { &CVmBifTADS::save, 1, 0, FALSE },                               /* 15 */
    { &CVmBifTADS::restore, 1, 0, FALSE },                            /* 16 */
    { &CVmBifTADS::restart, 0, 0, FALSE },                            /* 17 */
    { &CVmBifTADS::get_max, 1, 0, TRUE },                             /* 18 */
    { &CVmBifTADS::get_min, 1, 0, TRUE },                             /* 19 */
    { &CVmBifTADS::make_string, 1, 1, FALSE },                        /* 20 */
    { &CVmBifTADS::get_func_params, 1, 0, FALSE },                    /* 21 */
    { &CVmBifTADS::toNumber, 1, 1, FALSE },                           /* 23 */
    { &CVmBifTADS::sprintf, 1, 0, TRUE }                              /* 24 */
};

#endif /* VMBIF_DEFINE_VECTOR */
