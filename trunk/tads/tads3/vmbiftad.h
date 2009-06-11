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


/* ------------------------------------------------------------------------ */
/*
 *   TADS function set built-in functions 
 */
class CVmBifTADS: public CVmBif
{
public:
    /*
     *   General functions 
     */
    static void datatype(VMG_ uint argc);
    static void getarg(VMG_ uint argc);
    static void firstobj(VMG_ uint argc);
    static void nextobj(VMG_ uint argc);
    static void randomize(VMG_ uint argc);
    static void rand(VMG_ uint argc);
    static void cvtstr(VMG_ uint argc);
    static void cvtnum(VMG_ uint argc);
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

protected:
    /* enumerate objects (common handler for firstobj and nextobj) */
    static void enum_objects(VMG_ uint argc, vm_obj_id_t start_obj);
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

/* ------------------------------------------------------------------------ */
/*
 *   ISAAC Random Number Generator definitions
 */
#ifdef VMBIFTADS_RNG_ISAAC

#define ISAAC_RANDSIZL   (8)
#define ISAAC_RANDSIZ    (1<<ISAAC_RANDSIZL)

struct isaacctx
{
    ulong cnt;
    ulong rsl[ISAAC_RANDSIZ];
    ulong mem[ISAAC_RANDSIZ];
    ulong a;
    ulong b;
    ulong c;
};
#endif /* VMBIFTADS_RNG_ISAAC */

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
void (*G_bif_tadsgen[])(VMG_ uint) =
{
    &CVmBifTADS::datatype,
    &CVmBifTADS::getarg,
    &CVmBifTADS::firstobj,
    &CVmBifTADS::nextobj,
    &CVmBifTADS::randomize,
    &CVmBifTADS::rand,
    &CVmBifTADS::cvtstr,
    &CVmBifTADS::cvtnum,
    &CVmBifTADS::gettime,
    &CVmBifTADS::re_match,
    &CVmBifTADS::re_search,
    &CVmBifTADS::re_group,
    &CVmBifTADS::re_replace,
    &CVmBifTADS::savepoint,
    &CVmBifTADS::undo,
    &CVmBifTADS::save,
    &CVmBifTADS::restore,
    &CVmBifTADS::restart,
    &CVmBifTADS::get_max,
    &CVmBifTADS::get_min,
    &CVmBifTADS::make_string,
    &CVmBifTADS::get_func_params
};

#endif /* VMBIF_DEFINE_VECTOR */
