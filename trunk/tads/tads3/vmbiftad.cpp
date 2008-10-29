#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMBIFTAD.CPP,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbiftad.cpp - TADS built-in function set for T3 VM
Function
  
Notes
  
Modified
  04/05/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "t3std.h"
#include "os.h"
#include "utf8.h"
#include "vmuni.h"
#include "vmbiftad.h"
#include "vmstack.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmpool.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmrun.h"
#include "vmregex.h"
#include "vmundo.h"
#include "vmfile.h"
#include "vmsave.h"
#include "vmbignum.h"
#include "vmfunc.h"
#include "vmpat.h"
#include "vmtobj.h"
#include "vmvec.h"
#include "vmpredef.h"


/* ------------------------------------------------------------------------ */
/*
 *   forward statics 
 */

#ifdef VMBIFTADS_RNG_ISAAC
static void isaac_init(isaacctx *ctx, int flag);
#endif /* VMBIFTADS_RNG_ISAAC */


/* ------------------------------------------------------------------------ */
/*
 *   Initialize the TADS intrinsics global state 
 */
CVmBifTADSGlobals::CVmBifTADSGlobals(VMG0_)
{
    /* allocate our regular expression parser */
    rex_parser = new CRegexParser();
    rex_searcher = new CRegexSearcherSimple(rex_parser);

    /* 
     *   Allocate a global variable to hold the most recent regular
     *   expression search string.  We need this in a global so that the last
     *   search string is always protected from garbage collection; we must
     *   keep the string because we might need it to extract a group-match
     *   substring.  
     */
    last_rex_str = G_obj_table->create_global_var();

#ifdef VMBIFTADS_RNG_LCG
    /* 
     *   Set the random number seed to a fixed starting value (this value
     *   is arbitrary; we chose it by throwing dice).  If the program
     *   wants another sequence, it can manually change this by calling
     *   the randomize() intrinsic in our function set, which seeds the
     *   generator with an OS-dependent starting value (usually based on
     *   the system's real-time clock, to ensure that each run will use a
     *   different starting value).  
     */
    rand_seed = 024136543305;
#endif

#ifdef VMBIFTADS_RNG_ISAAC
    /* create the ISAAC context structure */
    isaac_ctx = (struct isaacctx *)t3malloc(sizeof(struct isaacctx));

    /* initialize with a fixed seed vector */
    isaac_init(isaac_ctx, FALSE);
#endif
}

/*
 *   delete the TADS intrinsics global state 
 */
CVmBifTADSGlobals::~CVmBifTADSGlobals()
{
    /* delete our regular expression searcher and parser */
    delete rex_searcher;
    delete rex_parser;

    /* 
     *   note that we leave our last_rex_str global variable undeleted here,
     *   as we don't have access to G_obj_table (as there's no VMG_ to a
     *   destructor); this is okay, since the object table will take care of
     *   deleting the variable for us when the object table itself is deleted
     */

#ifdef VMBIFTADS_RNG_ISAAC
    /* delete the ISAAC context */
    t3free(isaac_ctx);
#endif
}

/* ------------------------------------------------------------------------ */
/*
 *   datatype - get the datatype of a given value
 */
void CVmBifTADS::datatype(VMG_ uint argc)
{
    vm_val_t val;
    vm_val_t retval;

    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* pop the value */
    G_stk->pop(&val);

    /* return the appropriate value for this type */
    retval.set_datatype(vmg_ &val);
    retval_int(vmg_ retval.val.intval);
}

/* ------------------------------------------------------------------------ */
/*
 *   getarg - get the given argument to the current procedure 
 */
void CVmBifTADS::getarg(VMG_ uint argc)
{
    int idx;

    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* get the argument index value */
    idx = pop_int_val(vmg0_);

    /* if the argument index is out of range, throw an error */
    if (idx < 1 || idx > G_interpreter->get_cur_argc(vmg0_))
        err_throw(VMERR_BAD_VAL_BIF);

    /* push the parameter value */
    *G_interpreter->get_r0() = *G_interpreter->get_param(vmg_ idx - 1);
}

/* ------------------------------------------------------------------------ */
/*
 *   firstobj - get the first object instance
 */
void CVmBifTADS::firstobj(VMG_ uint argc)
{
    /* check arguments */
    check_argc_range(vmg_ argc, 0, 2);

    /* enumerate objects starting with object 1 in the master object table */
    enum_objects(vmg_ argc, (vm_obj_id_t)1);
}

/*
 *   nextobj - get the next object instance after a given object
 */
void CVmBifTADS::nextobj(VMG_ uint argc)
{
    vm_val_t val;
    vm_obj_id_t prv_obj;

    /* check arguments */
    check_argc_range(vmg_ argc, 1, 3);

    /* get the previous object */
    G_interpreter->pop_obj(vmg_ &val);
    prv_obj = val.val.obj;

    /* 
     *   Enumerate objects starting with the next object in the master
     *   object table after the given object.  Reduce the argument count by
     *   one, since we've removed the preceding object.  
     */
    enum_objects(vmg_ argc - 1, prv_obj + 1);
}

/* enum_objects flags */
#define VMBIFTADS_ENUM_INSTANCES  0x0001
#define VMBIFTADS_ENUM_CLASSES    0x0002

/*
 *   Common handler for firstobj/nextobj object iteration
 */
void CVmBifTADS::enum_objects(VMG_ uint argc, vm_obj_id_t start_obj)
{
    vm_val_t val;
    vm_obj_id_t sc;
    vm_obj_id_t obj;
    unsigned long flags;

    /* presume no superclass filter will be specified */
    sc = VM_INVALID_OBJ;

    /* presume we're enumerating instances only */
    flags = VMBIFTADS_ENUM_INSTANCES;

    /* 
     *   check arguments - we can optionally have two more arguments: a
     *   superclass whose instances/subclasses we are to enumerate, and an
     *   integer giving flag bits 
     */
    if (argc == 2)
    {
        /* pop the object */
        G_interpreter->pop_obj(vmg_ &val);
        sc = val.val.obj;

        /* pop the flags */
        flags = pop_long_val(vmg0_);
    }
    else if (argc == 1)
    {
        /* check to see if it's an object or the flags integer */
        switch (G_stk->get(0)->typ)
        {
        case VM_INT:
            /* it's the flags */
            flags = pop_long_val(vmg0_);
            break;

        case VM_OBJ:
            /* it's the superclass filter */
            G_interpreter->pop_obj(vmg_ &val);
            sc = val.val.obj;
            break;

        default:
            /* invalid argument type */
            err_throw(VMERR_BAD_TYPE_BIF);
        }
    }

    /* presume we won't find anything */
    retval_nil(vmg0_);

    /* 
     *   starting with the given object, scan objects until we find one
     *   that's valid and matches our superclass, if one was provided 
     */
    for (obj = start_obj ; obj < G_obj_table->get_max_used_obj_id() ; ++obj)
    {
        /* 
         *   If it's valid, and it's not an intrinsic class modifier object,
         *   consider it further.  Skip intrinsic class modifiers, since
         *   they're not really separate objects; they're really part of the
         *   intrinsic class they modify, and all of the properties and
         *   methods of a modifier object are reachable through the base
         *   intrinsic class.  
         */
        if (G_obj_table->is_obj_id_valid(obj)
            && !CVmObjIntClsMod::is_intcls_mod_obj(vmg_ obj))
        {
            /* 
             *   if it's a class, skip it if the flags indicate classes are
             *   not wanted; if it's an instance, skip it if the flags
             *   indicate that instances are not wanted 
             */
            if (vm_objp(vmg_ obj)->is_class_object(vmg_ obj))
            {
                /* it's a class - skip it if classes are not wanted */
                if ((flags & VMBIFTADS_ENUM_CLASSES) == 0)
                    continue;
            }
            else
            {
                /* it's an instance - skip it if instances are not wanted */
                if ((flags & VMBIFTADS_ENUM_INSTANCES) == 0)
                    continue;
            }

            /* 
             *   if a superclass was specified, and it matches, we have a
             *   winner 
             */
            if (sc != VM_INVALID_OBJ)
            {
                /* if the object matches, return it */
                if (vm_objp(vmg_ obj)->is_instance_of(vmg_ sc))
                {
                    retval_obj(vmg_ obj);
                    break;
                }
            }
            else
            {
                /* 
                 *   We're enumerating all objects - but skip List and String
                 *   object, as we expose these are special types.  
                 */
                if (vm_objp(vmg_ obj)->get_as_list() == 0
                    && vm_objp(vmg_ obj)->get_as_string(vmg0_) == 0)
                {
                    retval_obj(vmg_ obj);
                    break;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Random number generators.  Define one of the following configuration
 *   variables to select a random number generation algorithm:
 *   
 *   VMBIFTADS_RNG_LCG - linear congruential generator
 *.  VMBIFTADS_RNG_ISAAC - ISAAC (cryptographic hash generator) 
 */

/* ------------------------------------------------------------------------ */
/*
 *   Linear Congruential Random-Number Generator.  This generator uses an
 *   algorithm from Knuth, The Art of Computer Programming, Volume 2, p.
 *   170, with parameters chosen from the same book for their good
 *   statistical properties and efficiency on 32-bit hardware.  
 */
#ifdef VMBIFTADS_RNG_LCG
/*
 *   randomize - seed the random-number generator 
 */
void CVmBifTADS::randomize(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* seed the generator */
    os_rand(&G_bif_tads_globals->rand_seed);
}

/*
 *   generate the next random number - linear congruential generator 
 */
static ulong rng_next(VMG0_)
{
    const ulong a = 1664525L;
    const ulong c = 1;

    /* 
     *   Generate the next random value using the linear congruential
     *   method described in Knuth, The Art of Computer Programming,
     *   volume 2, p170.
     *   
     *   Use 2^32 as m, hence (n mod m) == (n & 0xFFFFFFFF).  This is
     *   efficient and is well-suited to 32-bit machines, works fine on
     *   larger machines, and will even work on 16-bit machines as long as
     *   the compiler can provide us with 32-bit arithmetic (which we
     *   assume extensively elsewhere anyway).
     *   
     *   We use a = 1664525, a multiplier which has very good results with
     *   the Spectral Test (see Knuth p102) with our choice of m.
     *   
     *   Use c = 1, since this trivially satisfies Knuth's requirements
     *   about common factors.
     *   
     *   Note that the result of the multiplication might overflow a
     *   32-bit ulong for values of rand_seed that are not small.  This
     *   doesn't matter, since if it does, the machine will naturally
     *   truncate high-order bits to yield the result mod 2^32.  So, on a
     *   32-bit machine, the (&0xFFFFFFFF) part is superfluous, but it's
     *   harmless and is needed for machines with a larger word size.  
     */
    G_bif_tads_globals->rand_seed =
        (long)(((a * (ulong)G_bif_tads_globals->rand_seed) + 1) & 0xFFFFFFFF);
    return (ulong)G_bif_tads_globals->rand_seed;
}
#endif /* VMBIFTADS_RNG_LCG */

/* ------------------------------------------------------------------------ */
/*
 *   ISAAC random number generator. 
 */

#ifdef VMBIFTADS_RNG_ISAAC

/* service macros for ISAAC random number generator */
#define isaac_ind(mm,x)  ((mm)[(x>>2)&(ISAAC_RANDSIZ-1)])
#define isaac_step(mix,a,b,mm,m,m2,r,x) \
{ \
    x = *m;  \
    a = ((a^(mix)) + *(m2++)) & 0xffffffff; \
    *(m++) = y = (isaac_ind(mm,x) + a + b) & 0xffffffff; \
    *(r++) = b = (isaac_ind(mm,y>>ISAAC_RANDSIZL) + x) & 0xffffffff; \
}
#define isaac_rand(r) \
    ((r)->cnt-- == 0 ? \
    (isaac_gen_group(r), (r)->cnt=ISAAC_RANDSIZ-1, (r)->rsl[(r)->cnt]) : \
    (r)->rsl[(r)->cnt])

#define isaac_mix(a,b,c,d,e,f,g,h) \
{ \
    a^=b<<11; d+=a; b+=c; \
    b^=c>>2;  e+=b; c+=d; \
    c^=d<<8;  f+=c; d+=e; \
    d^=e>>16; g+=d; e+=f; \
    e^=f<<10; h+=e; f+=g; \
    f^=g>>4;  a+=f; g+=h; \
    g^=h<<8;  b+=g; h+=a; \
    h^=a>>9;  c+=h; a+=b; \
}

/* generate the group of numbers */
static void isaac_gen_group(isaacctx *ctx)
{
    ulong a;
    ulong b;
    ulong x;
    ulong y;
    ulong *m;
    ulong *mm;
    ulong *m2;
    ulong *r;
    ulong *mend;
    
    mm = ctx->mem;
    r = ctx->rsl;
    a = ctx->a;
    b = (ctx->b + (++ctx->c)) & 0xffffffff;
    for (m = mm, mend = m2 = m + (ISAAC_RANDSIZ/2) ; m<mend ; )
    {
        isaac_step(a<<13, a, b, mm, m, m2, r, x);
        isaac_step(a>>6,  a, b, mm, m, m2, r, x);
        isaac_step(a<<2,  a, b, mm, m, m2, r, x);
        isaac_step(a>>16, a, b, mm, m, m2, r, x);
    }
    for (m2 = mm; m2<mend; )
    {
        isaac_step(a<<13, a, b, mm, m, m2, r, x);
        isaac_step(a>>6,  a, b, mm, m, m2, r, x);
        isaac_step(a<<2,  a, b, mm, m, m2, r, x);
        isaac_step(a>>16, a, b, mm, m, m2, r, x);
    }
    ctx->b = b;
    ctx->a = a;
}

/* 
 *   Initialize.  If flag is true, then use the contents of ctx->rsl[] to
 *   initialize ctx->mm[]; otherwise, we'll use a fixed starting
 *   configuration.  
 */
static void isaac_init(isaacctx *ctx, int flag)
{
    int i;
    ulong a;
    ulong b;
    ulong c;
    ulong d;
    ulong e;
    ulong f;
    ulong g;
    ulong h;
    ulong *m;
    ulong *r;
    
    ctx->a = ctx->b = ctx->c = 0;
    m = ctx->mem;
    r = ctx->rsl;
    a = b = c = d = e = f = g = h = 0x9e3779b9;         /* the golden ratio */
    
    /* scramble the initial settings */
    for (i = 0 ; i < 4 ; ++i)
    {
        isaac_mix(a, b, c, d, e, f, g, h);
    }

    if (flag) 
    {
        /* initialize using the contents of ctx->rsl[] as the seed */
        for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
        {
            a += r[i];   b += r[i+1]; c += r[i+2]; d += r[i+3];
            e += r[i+4]; f += r[i+5]; g += r[i+6]; h += r[i+7];
            isaac_mix(a, b, c, d, e, f, g, h);
            m[i] = a;   m[i+1] = b; m[i+2] = c; m[i+3] = d;
            m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
        }

        /* do a second pass to make all of the seed affect all of m */
        for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
        {
            a += m[i];   b += m[i+1]; c += m[i+2]; d += m[i+3];
            e += m[i+4]; f += m[i+5]; g += m[i+6]; h += m[i+7];
            isaac_mix(a, b, c, d, e, f, g, h);
            m[i] = a;   m[i+1] = b; m[i+2] = c; m[i+3] = d;
            m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
        }
    }
    else
    {
        /* initialize using fixed initial settings */
        for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
        {
            isaac_mix(a, b, c, d, e, f, g, h);
            m[i] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
            m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
        }
    }

    /* fill in the first set of results */
    isaac_gen_group(ctx);

    /* prepare to use the first set of results */    
    ctx->cnt = ISAAC_RANDSIZ;
}

/*
 *   seed the rng 
 */
void CVmBifTADS::randomize(VMG_ uint argc)
{
    int i;
    long seed;
    
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* seed the generator */
    os_rand(&seed);

    /* 
     *   Fill in rsl[] with the seed. It doesn't do a lot of good to call
     *   os_rand() repeatedly, since this function might simply return the
     *   real-time clock value.  So, use the os_rand() seed value as the
     *   first rsl[] value, then use a simple linear congruential
     *   generator to fill in the rest of rsl[].  
     */
    for (i = 0 ; i < ISAAC_RANDSIZ ; ++i)
    {
        const ulong a = 1664525L;

        /* fill in this value from the previous seed value */
        G_bif_tads_globals->isaac_ctx->rsl[i] = (ulong)seed;

        /* generate the next lcg value */
        seed = (long)(((a * (ulong)seed) + 1) & 0xFFFFFFFF);
    }

    /* initialize with this rsl[] array */
    isaac_init(G_bif_tads_globals->isaac_ctx, TRUE);
}


/*
 *   generate the next random number - ISAAC (by Bob Jenkins,
 *   http://ourworld.compuserve.com/homepages/bob_jenkins/isaacafa.htm) 
 */
static ulong rng_next(VMG0_)
{
    /* return the next number */
    return isaac_rand(G_bif_tads_globals->isaac_ctx);
}
#endif /* VMBIFTADS_RNG_ISAAC */

/* ------------------------------------------------------------------------ */
/*
 *   rand - generate a random number, or choose an element randomly from a
 *   list of values or from our list of arguments.
 *   
 *   With one integer argument N, we choose a random number from 0 to N-1.
 *   
 *   With one list argument, we choose a random element of the list.
 *   
 *   With multiple arguments, we choose one argument at random and return
 *   its value.  Note that, because this is an ordinary built-in function,
 *   all of our arguments will be fully evaluated.  
 */
void CVmBifTADS::rand(VMG_ uint argc)
{
    ulong range;
    int use_range;
    int choose_an_arg;
    const char *listp;
    ulong rand_val;
    CVmObjVector *vec = 0;
    vm_obj_id_t vecid = VM_INVALID_OBJ;

    /* presume we're not going to choose from our arguments or from a list */
    choose_an_arg = FALSE;
    listp = 0;

    /* determine the desired range of values based on the arguments */
    if (argc == 0)
    {
        /* 
         *   if no argument is given, produce a random number in our full
         *   range - clear the 'use_range' flag to so indicate
         */
        use_range = FALSE;
    }
    else if (argc == 1 && G_stk->get(0)->typ == VM_INT)
    {
        /* we're returning a number in the range 0..(arg-1) */
        range = G_stk->get(0)->val.intval;
        use_range = TRUE;

        /* discard the argument */
        G_stk->discard();
    }
    else if (argc == 1)
    {
        /* check for a vector or a list */
        if (G_stk->get(0)->typ == VM_OBJ
            && CVmObjVector::is_vector_obj(vmg_ G_stk->get(0)->val.obj))
        {
            /* 
             *   it's a vector - get the object pointer, but leave it on the
             *   stack for GC protection for now 
             */
            vecid = G_stk->get(0)->val.obj;
            vec = (CVmObjVector *)vm_objp(vmg_ vecid);

            /* the range is 0..(vector_length-1) */
            range = vec->get_element_count();
            use_range = TRUE;
        }
        else
        {
            /* it must be a list - pop the list value */
            listp = pop_list_val(vmg0_);

            /* our range is 0..(list_element_count-1) */
            range = vmb_get_len(listp);
            use_range = TRUE;
        }
    }
    else
    {
        /* 
         *   produce a random number in the range 0..(argc-1) so that we
         *   can select one of our arguments 
         */
        range = argc;
        use_range = TRUE;

        /* note that we should choose an argument value */
        choose_an_arg = TRUE;
    }

    /* get the next random number */
    rand_val = rng_next(vmg0_);

    /*
     *   Calculate our random value in the range 0..(range-1).  If range
     *   == 0, simply choose a value across our full range.  
     */
    if (use_range)
    {
        unsigned long hi;
        unsigned long lo;
        
        /* 
         *   A range was specified, so choose in our range.  As Knuth
         *   suggests, don't simply take the low-order bits from the value,
         *   since these are the least random part.  Instead, use the method
         *   Knuth describes in TAOCP Vol 2 section 3.4.2.
         *   
         *   Avoid floating point arithmetic - use an integer calculation
         *   instead.  This code performs a 64-bit fixed-point calculation
         *   using 32-bit values.
         *   
         *   The calculation we're really performing is this:
         *   
         *   rand_val = (ulong)((((double)rand_val) / 4294967296.0)
         *.             * (double)range); 
         */

        /* calculate the high-order 32 bits of (rand_val / 2^32 * range) */
        hi = (((rand_val >> 16) & 0xffff) * ((range >> 16) & 0xffff))
             + ((((rand_val >> 16) & 0xffff) * (range & 0xffff)) >> 16)
             + (((rand_val & 0xffff) * ((range >> 16) & 0xffff)) >> 16);

        /* calculate the low-order 32 bits */
        lo = ((((rand_val >> 16) & 0xffff) * (range & 0xffff)) & 0xffff)
             + (((rand_val & 0xffff) * ((range >> 16) & 0xffff)) & 0xffff)
             + ((((rand_val & 0xffff) * (range & 0xffff)) >> 16) & 0xffff);

        /* 
         *   add the carry from the low part into the high part to get the
         *   result 
         */
        rand_val = hi + (lo >> 16);
    }

    /*
     *   Return the appropriate value, depending on our argument list 
     */
    if (choose_an_arg)
    {
        /* return the selected argument */
        retval(vmg_ G_stk->get((int)rand_val));

        /* discard all of the arguments */
        G_stk->discard(argc);
    }
    else if (vec != 0)
    {
        vm_val_t val;

        /* get the selected element */
        if (range == 0)
        {
            /* there are no elements to choose from, so return nil */
            val.set_nil();
        }
        else
        {
            vm_val_t idxval;

            /* get the selected vector element */
            idxval.set_int(rand_val + 1);
            vec->index_val(vmg_ &val, vecid, &idxval);
        }

        /* set the result */
        retval(vmg_ &val);

        /* discard our gc protection */
        G_stk->discard();
    }
    else if (listp != 0)
    {
        vm_val_t val;

        /* as a special case, if the list has zero elements, return nil */
        if (vmb_get_len(listp) == 0)
        {
            /* there are no elements to choose from, so return nil */
            val.set_nil();
        }
        else
        {
            /* get the selected list element */
            vmb_get_dh(listp + VMB_LEN
                       + (size_t)((rand_val * VMB_DATAHOLDER)), &val);
        }
            
        /* set the result */
        retval(vmg_ &val);
    }
    else
    {
        /* simply return the random number */
        retval_int(vmg_ (long)rand_val);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Bit-shift generator.  This is from Knuth, The Art of Computer
 *   Programming, volume 2.  This generator is designed to produce random
 *   strings of bits and is not suitable for use as a general-purpose RNG.
 *   
 *   Linear congruential generators are not ideal for generating random
 *   bits; their statistical properties seem better suited for generating
 *   values over a wider range.  This generator is specially designed to
 *   produce random bits, so it could be a useful complement to an LCG RNG.
 *   
 *   This code should not be enabled in its present state; it's retained
 *   in case we want in the future to implement a generator exclusively
 *   for random bits.  The ISAAC generator seems to be a good source of
 *   random bits as well as random numbers, so it seems unlikely that
 *   we'll need a separate random bit generator.  
 */

#ifdef VMBIFTADS_RNG_BITSHIFT
void CVmBifTADS::randbit(VMG_ uint argc)
{
    int top_bit;
    
    /* check arguments */
    check_argc(vmg_ argc, 0);

    top_bit = (G_bif_tads_globals->rand_seed & 0x8000000);
    G_bif_tads_globals->rand_seed <<= 1;
    if (top_bit)
        G_bif_tads_globals->rand_seed ^= 035604231625;

    retval_int(vmg_ (long)(G_bif_tads_globals->rand_seed & 1));
}
#endif /* VMBIFTADS_RNG_BITSHIFT */


/* ------------------------------------------------------------------------ */
/*
 *   cvtstr (toString) - convert to string 
 */
void CVmBifTADS::cvtstr(VMG_ uint argc)
{
    const char *p;
    char buf[50];
    vm_val_t val;
    int radix;
    vm_val_t new_str;
    
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* pop the argument */
    G_stk->pop(&val);

    /* if there's a radix specified, pop it as well */
    if (argc == 2)
    {
        /* get the radix from the stack */
        radix = pop_int_val(vmg0_);
    }
    else
    {
        /* use decimal by default */
        radix = 10;
    }

    /* convert the value */
    p = CVmObjString::cvt_to_str(vmg_ &new_str,
                                 buf, sizeof(buf), &val, radix);

    /* save the new string on the stack to protect from garbage collection */
    G_stk->push(&new_str);

    /* create and return a string from our new value */
    retval_obj(vmg_ CVmObjString::create(vmg_ FALSE,
                                         p + VMB_LEN, vmb_get_len(p)));

    /* done with the new string */
    G_stk->discard();
}

/*
 *   cvtnum (toInteger) - convert to an integer
 */
void CVmBifTADS::cvtnum(VMG_ uint argc)
{
    const char *strp;
    size_t len;
    int radix;
    vm_val_t *valp;
        
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* 
     *   check for a BigNumber and convert it (not very object-oriented,
     *   but this is a type-conversion routine, so special awareness of
     *   individual types isn't that weird) 
     */
    valp = G_stk->get(0);
    if (valp->typ == VM_OBJ
        && CVmObjBigNum::is_bignum_obj(vmg_ valp->val.obj))
    {
        long intval;
        
        /* convert it as a BigNumber */
        intval = ((CVmObjBigNum *)vm_objp(vmg_ valp->val.obj))
                 ->convert_to_int();

        /* discard arguments (ignore the radix in this case) */
        G_stk->discard(argc);

        /* return the integer value */
        retval_int(vmg_ intval);
        return;
    }

    /* if it's already an integer, just return the same value */
    if (valp->typ == VM_INT)
    {
        /* just return the argument value */
        retval_int(vmg_ valp->val.intval);

        /* discard arguments (ignore the radix in this case) */
        G_stk->discard(argc);

        /* done */
        return;
    }

    /* otherwise, it must be a string */
    strp = pop_str_val(vmg0_);
    len = vmb_get_len(strp);

    /* if there's a radix specified, pop it as well */
    if (argc == 2)
    {
        /* get the radix from the stack */
        radix = pop_int_val(vmg0_);

        /* make sure the radix is valid */
        switch(radix)
        {
        case 2:
        case 8:
        case 10:
        case 16:
            /* it's okay - proceed */
            break;

        default:
            /* other radix values are invalid */
            err_throw(VMERR_BAD_VAL_BIF);
        }
    }
    else
    {
        /* the default radix is decimal */
        radix = 10;
    }

    /* parse the value */
    if (len == 3 && memcmp(strp + VMB_LEN, "nil", 3) == 0)
    {
        /* the value is the constant 'nil' */
        retval_nil(vmg0_);
    }
    else if (len == 4 && memcmp(strp + VMB_LEN, "true", 3) == 0)
    {
        /* the value is the constant 'true' */
        retval_true(vmg0_);
    }
    else
    {
        utf8_ptr p;
        size_t rem;
        int is_neg;
        ulong acc;

        /* scan past leading spaces */
        for (p.set((char *)strp + VMB_LEN), rem = len ;
             rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

        /* presume it's positive */
        is_neg = FALSE;

        /* if the radix is 10, check for a leading + or - */
        if (radix == 10 && rem != 0)
        {
            if (p.getch() == '-')
            {
                /* note the sign and skip the character */
                is_neg = TRUE;
                p.inc(&rem);
            }
            else if (p.getch() == '+')
            {
                /* skip the character */
                p.inc(&rem);
            }
        }

        /* clear the accumulator */
        acc = 0;

        /* scan the digits */
        switch (radix)
        {
        case 2:
            for ( ; rem != 0 && (p.getch() == '0' || p.getch() == '1') ;
                  p.inc(&rem))
            {
                acc <<= 1;
                if (p.getch() == '1')
                    acc += 1;
            }
            break;
            
        case 8:
            for ( ; rem != 0 && is_odigit(p.getch()) ; p.inc(&rem))
            {
                acc <<= 3;
                acc += value_of_odigit(p.getch());
            }
            break;

        case 10:
            for ( ; rem != 0 && is_digit(p.getch()) ; p.inc(&rem))
            {
                acc *= 10;
                acc += value_of_digit(p.getch());
            }
            break;

        case 16:
            for ( ; rem != 0 && is_xdigit(p.getch()) ; p.inc(&rem))
            {
                acc <<= 4;
                acc += value_of_xdigit(p.getch());
            }
            break;
        }

        /* apply the sign, if appropriate, and set the return value */
        if (is_neg)
            retval_int(vmg_ -(long)acc);
        else
            retval_int(vmg_ (long)acc);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   put an integer value in a constant list, advancing the list write
 *   pointer 
 */
static void put_list_int(char **dstp, long intval)
{
    vm_val_t val;

    /* set up the integer value */
    val.set_int(intval);

    /* write it to the list */
    vmb_put_dh(*dstp, &val);

    /* advance the output pointer */
    *dstp += VMB_DATAHOLDER;
}

/*
 *   put an object value in a constant list, advancing the list write
 *   pointer 
 */
static void put_list_obj(char **dstp, vm_obj_id_t objval)
{
    vm_val_t val;

    /* set up the integer value */
    val.set_obj(objval);

    /* write it to the list */
    vmb_put_dh(*dstp, &val);

    /* advance the output pointer */
    *dstp += VMB_DATAHOLDER;
}


/*
 *   get the current time 
 */
void CVmBifTADS::gettime(VMG_ uint argc)
{
    int typ;
    time_t timer;
    struct tm *tblock;
    char buf[80];
    char *dst;
    
    /* check arguments */
    check_argc_range(vmg_ argc, 0, 1);

    /* if there's an argument, get the type of time value to return */
    if (argc == 1)
    {
        /* get the time type code */
        typ = pop_int_val(vmg0_);
    }
    else
    {
        /* use the default type */
        typ = 1;
    }

    /* check the type */
    switch(typ)
    {
    case 1:
        /* 
         *   default information - return the current time and date 
         */

        /* make sure the time zone is set up properly */
        os_tzset();

        /* get the local time information */
        timer = time(NULL);
        tblock = localtime(&timer);

        /* adjust values for return format */
        tblock->tm_year += 1900;
        tblock->tm_mon++;
        tblock->tm_wday++;
        tblock->tm_yday++;

        /*   
         *   build the return list: [year, month, day, day-of-week,
         *   day-of-year, hour, minute, second, seconds-since-1970] 
         */
        vmb_put_len(buf, 9);
        dst = buf + VMB_LEN;

        /* build return list value */
        put_list_int(&dst, tblock->tm_year);
        put_list_int(&dst, tblock->tm_mon);
        put_list_int(&dst, tblock->tm_mday);
        put_list_int(&dst, tblock->tm_wday);
        put_list_int(&dst, tblock->tm_yday);
        put_list_int(&dst, tblock->tm_hour);
        put_list_int(&dst, tblock->tm_min);
        put_list_int(&dst, tblock->tm_sec);
        put_list_int(&dst, (long)timer);

        /* allocate and return the list value */
        retval_obj(vmg_ CVmObjList::create(vmg_ FALSE, buf));

        /* done */
        break;

    case 2:
        /* 
         *   They want the high-precision system timer value, which returns
         *   the time in milliseconds from an arbitrary zero point.  
         */
        {
            unsigned long t;
            static unsigned long t_zero;
            static int t_zero_set = FALSE;

            /* retrieve the raw time from the operating system */
            t = os_get_sys_clock_ms();

            /* 
             *   We only have 31 bits of precision in our result (since we
             *   must fit the value into a signed integer), so we can only
             *   represent time differences of about 23 days.  Now, the
             *   value from the OS could be at any arbitrary point in our
             *   23-day range, so there's a nontrivial probability that the
             *   raw OS value is near enough to the wrapping point that a
             *   future call to this same function during the current
             *   session could encounter the wrap condition.  The caller is
             *   likely to be confused by this, because the time difference
             *   from this call to that future call would appear to be
             *   negative.
             *   
             *   There's obviously no way we can eliminate the possibility
             *   of a negative time difference if the current program
             *   session lasts more than 23 days of continuous execution.
             *   Fortunately, it seems unlikely that most sessions will be
             *   so long, which gives us a way to reduce the likelihood that
             *   the program will encounter a wrapped timer: we can adjust
             *   the zero point of the timer to the time of the first call
             *   to this function.  That way, the timer will wrap only if
             *   the program session runs continuously until the timer's
             *   range is exhausted.  
             */
            if (!t_zero_set)
            {
                /* this is the first call - remember the zero point */
                t_zero = t;
                t_zero_set = TRUE;
            }

            /* 
             *   Adjust the time by subtracting the zero point from the raw
             *   OS timer.  This will give us the number of milliseconds
             *   from our zero point.
             *   
             *   If the system timer has wrapped since our zero point, we'll
             *   get what looks like a negative number; but what we really
             *   have is a large positive number with a borrow from an
             *   unrepresented higher-precision portion, so the fact that
             *   this value is negative doesn't matter - it will still be
             *   sequential when treated as unsigned.  
             */
            t -= t_zero;

            /* 
             *   whatever we got, keep only the low-order 31 bits, since we
             *   only have 31 bits in which to represent an unsigned value
             */
            t &= 0x7fffffff;

            /* return the value we've calculated */
            retval_int(vmg_ t);
        }
        break;

    default:
        err_throw(VMERR_BAD_VAL_BIF);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   re_match - match a regular expression to a string
 */
void CVmBifTADS::re_match(VMG_ uint argc)
{
    const char *str;
    utf8_ptr p;
    size_t len;
    int match_len;
    vm_val_t *v1, *v2, *v3;
    int start_idx;
    CVmObjPattern *pat_obj = 0;
    const char *pat_str = 0;
    
    /* check arguments */
    check_argc_range(vmg_ argc, 2, 3);

    /* 
     *   make copies of the arguments, so we can pop the values without
     *   actually removing them from the stack - leave the originals on the
     *   stack for gc protection 
     */
    v1 = G_stk->get(0);
    v2 = G_stk->get(1);
    v3 = (argc >= 3 ? G_stk->get(2) : 0);
    G_stk->push(v2);
    G_stk->push(v1);

    /* note the starting index, if given */
    start_idx = 0;
    if (v3 != 0)
    {
        /* check the type */
        if (v3->typ != VM_INT)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get the value */
        start_idx = (int)v3->val.intval - 1;

        /* make sure it's in range */
        if (start_idx < 0)
            start_idx = 0;
    }

    /* remember the last search string (the second argument) */
    G_bif_tads_globals->last_rex_str->val = *v2;

    /* 
     *   check what we have for the pattern - we could have either a string
     *   giving the regular expression, or a RexPattern object with the
     *   compiled pattern 
     */
    if (G_stk->get(0)->typ == VM_OBJ
        && CVmObjPattern::is_pattern_obj(vmg_ G_stk->get(0)->val.obj))
    {
        vm_val_t pat_val;

        /* get the pattern object */
        G_stk->pop(&pat_val);
        pat_obj = (CVmObjPattern *)vm_objp(vmg_ pat_val.val.obj);
    }
    else
    {
        /* get the pattern string */
        pat_str = pop_str_val(vmg0_);
    }

    /* get the string to match */
    str = pop_str_val(vmg0_);
    len = vmb_get_len(str);
    p.set((char *)str + VMB_LEN);

    /* skip to the starting index */
    for ( ; start_idx > 0 && len != 0 ; --start_idx, p.inc(&len)) ;

    /* match the pattern */
    if (pat_obj != 0)
    {
        /* match the compiled pattern object */
        match_len = G_bif_tads_globals->rex_searcher->
                    match_pattern(pat_obj->get_pattern(vmg0_),
                                  str + VMB_LEN, p.getptr(), len);
    }
    else
    {
        /* match the pattern to the regular expression string */
        match_len = G_bif_tads_globals->rex_searcher->
                    compile_and_match(pat_str + VMB_LEN, vmb_get_len(pat_str),
                                      str + VMB_LEN, p.getptr(), len);
    }

    /* check for a match */
    if (match_len >= 0)
    {
        /* we got a match - calculate the character length of the match */
        retval_int(vmg_ (long)p.len(match_len));
    }
    else
    {
        /* no match - return nil */
        retval_nil(vmg0_);
    }

    /* discard the arguments */
    G_stk->discard(argc);
}

/*
 *   re_search - search for a substring matching a regular expression
 *   within a string 
 */
void CVmBifTADS::re_search(VMG_ uint argc)
{
    const char *str;
    utf8_ptr p;
    size_t len;
    int match_idx;
    int match_len;
    vm_val_t *v1, *v2, *v3;
    int start_idx;
    int i;
    CVmObjPattern *pat_obj = 0;
    const char *pat_str = 0;

    /* check arguments */
    check_argc_range(vmg_ argc, 2, 3);

    /* 
     *   make copies of the arguments, so we can pop the values without
     *   actually removing them from the stack - leave the originals on the
     *   stack for gc protection 
     */
    v1 = G_stk->get(0);
    v2 = G_stk->get(1);
    v3 = (argc >= 3 ? G_stk->get(2) : 0);
    G_stk->push(v2);
    G_stk->push(v1);

    /* note the starting index, if given */
    start_idx = 0;
    if (v3 != 0)
    {
        /* check the type */
        if (v3->typ != VM_INT)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get the value */
        start_idx = (int)v3->val.intval - 1;

        /* make sure it's in range */
        if (start_idx < 0)
            start_idx = 0;
    }

    /* remember the last search string (the second argument) */
    G_bif_tads_globals->last_rex_str->val = *v2;

    /* check to see if we have a RexPattern object or an uncompiled string */
    if (G_stk->get(0)->typ == VM_OBJ
        && CVmObjPattern::is_pattern_obj(vmg_ G_stk->get(0)->val.obj))
    {
        vm_val_t pat_val;

        /* get the pattern object */
        G_stk->pop(&pat_val);
        pat_obj = (CVmObjPattern *)vm_objp(vmg_ pat_val.val.obj);
    }
    else
    {
        /* get the pattern string */
        pat_str = pop_str_val(vmg0_);
    }
    
    /* get the string to search for the pattern */
    str = pop_str_val(vmg0_);
    p.set((char *)str + VMB_LEN);
    len = vmb_get_len(str);

    /* skip to the starting index */
    for (i = start_idx ; i > 0 && len != 0 ; --i, p.inc(&len)) ;

    /* search for the pattern */
    if (pat_obj != 0)
    {
        /* try finding the compiled pattern */
        match_idx = G_bif_tads_globals->rex_searcher->search_for_pattern(
            pat_obj->get_pattern(vmg0_),
            str + VMB_LEN, p.getptr(), len, &match_len);
    }
    else
    {
        /* try finding the regular expression string pattern */
        match_idx = G_bif_tads_globals->rex_searcher->compile_and_search(
            pat_str + VMB_LEN, vmb_get_len(pat_str),
            str + VMB_LEN, p.getptr(), len, &match_len);
    }

    /* check for a match */
    if (match_idx >= 0)
    {
        utf8_ptr matchp;
        size_t char_idx;
        size_t char_len;
        vm_obj_id_t match_str_obj;
        char *dst;
        char buf[VMB_LEN + VMB_DATAHOLDER * 3];

        /* 
         *   We got a match - calculate the character index of the match
         *   offset, adjusted to a 1-base.  The character index is simply the
         *   number of characters in the part of the string up to the match
         *   index.  Note that we have to add the starting index to get the
         *   actual index in the overall string, since 'p' points to the
         *   character at the starting index.  
         */
        char_idx = p.len(match_idx) + start_idx + 1;

        /* calculate the character length of the match */
        matchp.set(p.getptr() + match_idx);
        char_len = matchp.len(match_len);

        /* allocate a string containing the match */
        match_str_obj =
            CVmObjString::create(vmg_ FALSE, matchp.getptr(), match_len);

        /* push it momentarily as protection against garbage collection */
        G_stk->push()->set_obj(match_str_obj);

        /* 
         *   set up a 3-element list to contain the return value:
         *   [match_start_index, match_length, match_string] 
         */
        vmb_put_len(buf, 3);
        dst = buf + VMB_LEN;
        put_list_int(&dst, (long)char_idx);
        put_list_int(&dst, (long)char_len);
        put_list_obj(&dst, match_str_obj);

        /* allocate and return the list */
        retval_obj(vmg_ CVmObjList::create(vmg_ FALSE, buf));

        /* we no longer need the garbage collection protection */
        G_stk->discard();
    }
    else
    {
        /* no match - return nil */
        retval_nil(vmg0_);
    }

    /* discard the arguments */
    G_stk->discard(argc);
}

/*
 *   re_group - get the string matching a group in the most recent regular
 *   expression search or match 
 */
void CVmBifTADS::re_group(VMG_ uint argc)
{
    int groupno;
    const re_group_register *reg;
    char buf[VMB_LEN + 3*VMB_DATAHOLDER];
    char *dst;
    utf8_ptr p;
    vm_obj_id_t strobj;
    const char *last_str;
    int start_byte_ofs;
    
    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* get the group number to retrieve */
    groupno = pop_int_val(vmg0_);

    /* make sure it's in range */
    if (groupno < 1 || groupno > RE_GROUP_REG_CNT)
        err_throw(VMERR_BAD_VAL_BIF);

    /* adjust from a 1-base to a 0-base */
    --groupno;

    /* if the group doesn't exist in the pattern, return nil */
    if (groupno >= G_bif_tads_globals->rex_searcher->get_group_cnt())
    {
        retval_nil(vmg0_);
        return;
    }

    /* 
     *   get the previous search string - get a pointer directly to the
     *   contents of the string
     */
    last_str = G_bif_tads_globals->last_rex_str->val.get_as_string(vmg0_);

    /* get the register */
    reg = G_bif_tads_globals->rex_searcher->get_group_reg(groupno);

    /* if the group wasn't set, or there's no last string, return nil */
    if (last_str == 0 || reg->start_ofs == -1 || reg->end_ofs == -1)
    {
        retval_nil(vmg0_);
        return;
    }
    
    /* set up for a list with three elements */
    vmb_put_len(buf, 3);
    dst = buf + VMB_LEN;

    /* get the starting offset from the group register */
    start_byte_ofs = reg->start_ofs;

    /* 
     *   The first element is the character index of the group text in the
     *   source string.  Calculate the character index by adding 1 to the
     *   character length of the text preceding the group; calculate the
     *   character length from the byte length of that string.  Note that the
     *   starting in the group register is stored from the starting point of
     *   the search, not the start of the string, so we need to add in the
     *   starting point in the search.  
     */
    p.set((char *)last_str + VMB_LEN);
    put_list_int(&dst, p.len(start_byte_ofs) + 1);

    /* 
     *   The second element is the character length of the group text.
     *   Calculate the character length from the byte length. 
     */
    p.set(p.getptr() + start_byte_ofs);
    put_list_int(&dst, p.len(reg->end_ofs - reg->start_ofs));

    /*
     *   The third element is the string itself.  Create a new string
     *   containing the matching substring. 
     */
    strobj = CVmObjString::create(vmg_ FALSE, p.getptr(),
                                  reg->end_ofs - reg->start_ofs);
    put_list_obj(&dst, strobj);

    /* save the string on the stack momentarily to protect against GC */
    G_stk->push()->set_obj(strobj);

    /* create and return the list value */
    retval_obj(vmg_ CVmObjList::create(vmg_ FALSE, buf));

    /* we no longer need the garbage collector protection */
    G_stk->discard();
}

/*
 *   re_replace flags 
 */
#define VMBIFTADS_REPLACE_ALL  0x0001

/*
 *   re_replace - search for a pattern in a string, and apply a
 *   replacement pattern
 */
void CVmBifTADS::re_replace(VMG_ uint argc)
{
    vm_val_t patval, rplval;
    const char *str;
    const char *rpl;
    ulong flags;
    vm_val_t search_val;
    int match_idx;
    int match_len;
    size_t new_len;
    utf8_ptr p;
    size_t rem;
    int groupno;
    const re_group_register *reg;
    vm_obj_id_t ret_obj;
    utf8_ptr dstp;
    int match_cnt;
    int start_idx;
    re_compiled_pattern *cpat;
    int cpat_is_ours;
    int group_cnt;
    int start_char_idx;
    int skip_bytes;

    /* check arguments */
    check_argc_range(vmg_ argc, 4, 5);

    /* remember the pattern and replacement string values */
    patval = *G_stk->get(0);
    rplval = *G_stk->get(2);

    /* retrieve the compiled RexPattern or uncompiled pattern string */
    if (G_stk->get(0)->typ == VM_OBJ
        && CVmObjPattern::is_pattern_obj(vmg_ G_stk->get(0)->val.obj))
    {
        vm_val_t pat_val;
        CVmObjPattern *pat;

        /* get the pattern object */
        G_stk->pop(&pat_val);
        pat = (CVmObjPattern *)vm_objp(vmg_ pat_val.val.obj);

        /* get the compiled pattern structure */
        cpat = pat->get_pattern(vmg0_);

        /* the pattern isn't ours, so we don't need to delete it */
        cpat_is_ours = FALSE;
    }
    else
    {
        re_status_t stat;
        const char *pat_str;

        /* pop the pattern string */
        pat_str = pop_str_val(vmg0_);

        /* since we'll need it multiple times, compile it */
        stat = G_bif_tads_globals->rex_parser->compile_pattern(
            pat_str + VMB_LEN, vmb_get_len(pat_str), &cpat);

        /* if that failed, we don't have a pattern */
        if (stat != RE_STATUS_SUCCESS)
            cpat = 0;

        /* note that we allocated the pattern, so we have to delete it */
        cpat_is_ours = TRUE;
    }

    /* 
     *   Pop the search string and the replacement string.  Note that we want
     *   to retain the original value information for the search string,
     *   since we'll end up returning it unchanged if we don't find the
     *   pattern.  
     */
    G_stk->pop(&search_val);
    rpl = pop_str_val(vmg0_);

    /* remember the last search string */
    G_bif_tads_globals->last_rex_str->val = search_val;

    /* pop the flags */
    flags = pop_long_val(vmg0_);

    /* pop the starting index if given */
    start_char_idx = (argc >= 5 ? pop_int_val(vmg0_) - 1 : 0);

    /* make sure it's in range */
    if (start_char_idx < 0)
        start_char_idx = 0;

    /* 
     *   put the pattern, replacement string, and search string values back
     *   on the stack as protection against garbage collection 
     */
    G_stk->push(&patval);
    G_stk->push(&rplval);
    G_stk->push(&search_val);

    /* make sure the search string is indeed a string */
    str = search_val.get_as_string(vmg0_);
    if (str == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* 
     *   figure out how many bytes at the start of the string to skip before
     *   our first replacement 
     */
    for (p.set((char *)str + VMB_LEN), rem = vmb_get_len(str) ;
         start_char_idx > 0 && rem != 0 ; --start_char_idx, p.inc(&rem)) ;

    /* the current offset in the string is the byte skip offset */
    skip_bytes = p.getptr() - (str + VMB_LEN);

    /* 
     *   if we don't have a compiled pattern at this point, we're not going
     *   to be able to match anything, so we can just stop now and return the
     *   original string unchanged 
     */
    if (cpat == 0)
    {
        /* return the original search string */
        retval(vmg_ &search_val);
        goto done;
    }

    /* note the group count in the compiled pattern */
    group_cnt = cpat->group_cnt;

    /*
     *   First, determine how long the result string will be.  Search
     *   repeatedly if the REPLACE_ALL flag (0x0001) is set.
     */
    for (new_len = skip_bytes, match_cnt = 0, start_idx = skip_bytes ;
         (size_t)start_idx < vmb_get_len(str) ; ++match_cnt)
    {
        const char *last_str;

        /* figure out where the next search starts */
        last_str = str + VMB_LEN + start_idx;

        /* search for the pattern in the search string */
        match_idx = G_bif_tads_globals->rex_searcher->search_for_pattern(
            cpat, str + VMB_LEN, last_str, vmb_get_len(str) - start_idx,
            &match_len);

        /* if there was no match, there is no more replacing to do */
        if (match_idx == -1)
        {
            /* 
             *   if we haven't found a match before, there's no
             *   replacement at all to do -- just return the original
             *   string unchanged 
             */
            if (match_cnt == 0)
            {
                /* no replacement - return the original search string */
                retval(vmg_ &search_val);
                goto done;
            }
            else
            {
                /* we've found all of our matches - stop searching */
                break;
            }
        }

        /*
         *   We've found a match to replace.  Determine how much space we
         *   need for the replacement pattern with its substitution
         *   parameters replaced with the original string's matching text.
         *   
         *   First, add in the length of the part from the start of this
         *   segment of the search to the matched substring.  
         */
        new_len += match_idx;

        /* 
         *   now, scan the replacement string and add in its length and
         *   the lengths of substitution parameters 
         */
        for (p.set((char *)rpl + VMB_LEN), rem = vmb_get_len(rpl) ;
             rem != 0 ; p.inc(&rem))
        {
            /* check for '%' sequences */
            if (p.getch() == '%')
            {
                /* skip the '%' */
                p.inc(&rem);
                
                /* if there's anything left, see what we have */
                if (rem != 0)
                {
                    switch(p.getch())
                    {
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        /* get the group number */
                        groupno = value_of_digit(p.getch()) - 1;
                        
                        /* if this group is valid, add its length */
                        if (groupno < group_cnt)
                        {
                            /* get the register */
                            reg = G_bif_tads_globals->rex_searcher
                                  ->get_group_reg(groupno);
                            
                            /* if it's been set, add its length */
                            if (reg->start_ofs != -1 && reg->end_ofs != -1)
                                new_len += reg->end_ofs - reg->start_ofs;
                        }
                        break;
                        
                    case '*':
                        /* add the entire match size */
                        new_len += match_len;
                        break;
                        
                    case '%':
                        /* add a single '%' */
                        ++new_len;
                        break;
                        
                    default:
                        /* add the entire sequence unchanged */
                        new_len += 2;
                        break;
                    }
                }
            }
            else
            {
                /* count this character literally */
                new_len += p.charsize();
            }
        }

        /* start the next search after the end of this match */
        start_idx += match_idx + match_len;

        /* 
         *   if the match length was zero, skip one more character - a zero
         *   length match will just match again at the same spot forever, so
         *   once we replace it once we need to move on to avoid an infinite
         *   loop 
         */
        if (match_len == 0)
        {
            /* move past the input */
            start_idx += 1;

            /* we'll copy this character to the output, so make room for it */
            new_len += 1;
        }

        /* 
         *   if we're only replacing a single match, stop now; otherwise,
         *   continue looking 
         */
        if (!(flags & VMBIFTADS_REPLACE_ALL))
            break;
    }

    /* add in the size of the remainder of the string after the last match */
    new_len += vmb_get_len(str) - start_idx;

    /* allocate the result string */
    ret_obj = CVmObjString::create(vmg_ FALSE, new_len);

    /* get a pointer to the result buffer */
    dstp.set(((CVmObjString *)vm_objp(vmg_ ret_obj))->cons_get_buf());

    /* copy the initial part that we're skipping */
    if (skip_bytes != 0)
    {
        memcpy(dstp.getptr(), str + VMB_LEN, skip_bytes);
        dstp.set(dstp.getptr() + skip_bytes);
    }

    /*
     *   Once again, start searching from the beginning of the string.
     *   This time, build the result string as we go. 
     */
    for (start_idx = skip_bytes ; (size_t)start_idx < vmb_get_len(str) ; )
    {
        const char *last_str;

        /* figure out where the next search starts */
        last_str = str + VMB_LEN + start_idx;

        /* search for the pattern */
        match_idx = G_bif_tads_globals->rex_searcher->search_for_pattern(
            cpat, str + VMB_LEN, last_str, vmb_get_len(str) - start_idx,
            &match_len);

        /* stop if we can't find another match */
        if (match_idx < 0)
            break;
        
        /* copy the part up to the start of the matched text, if any */
        if (match_idx > 0)
        {
            /* copy the part from the last match to this match */
            memcpy(dstp.getptr(), last_str, match_idx);

            /* advance the output pointer */
            dstp.set(dstp.getptr() + match_idx);
        }

        /*
         *   Scan the replacement string again, and this time actually
         *   build the result.  
         */
        for (p.set((char *)rpl + VMB_LEN), rem = vmb_get_len(rpl) ;
             rem != 0 ; p.inc(&rem))
        {
            /* check for '%' sequences */
            if (p.getch() == '%')
            {
                /* skip the '%' */
                p.inc(&rem);
                
                /* if there's anything left, see what we have */
                if (rem != 0)
                {
                    switch(p.getch())
                    {
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        /* get the group number */
                        groupno = value_of_digit(p.getch()) - 1;
                        
                        /* if this group is valid, add its length */
                        if (groupno < group_cnt)
                        {
                            /* get the register */
                            reg = G_bif_tads_globals->rex_searcher
                                  ->get_group_reg(groupno);
                            
                            /* if it's been set, add its text */
                            if (reg->start_ofs != -1 && reg->end_ofs != -1)
                            {
                                size_t glen;

                                /* get the group length */
                                glen = reg->end_ofs - reg->start_ofs;

                                /* copy the data */
                                memcpy(dstp.getptr(),
                                       str + VMB_LEN + reg->start_ofs, glen);

                                /* advance past it */
                                dstp.set(dstp.getptr() + glen);
                            }
                        }
                        break;

                    case '*':
                        /* add the entire matched string */
                        memcpy(dstp.getptr(), last_str + match_idx,
                               match_len);
                        dstp.set(dstp.getptr() + match_len);
                        break;
                        
                    case '%':
                        /* add a single '%' */
                        dstp.setch('%');
                        break;
                        
                    default:
                        /* add the entire sequence unchanged */
                        dstp.setch('%');
                        dstp.setch(p.getch());
                        break;
                    }
                }
            }
            else
            {
                /* copy this character literally */
                dstp.setch(p.getch());
            }
        }

        /* advance past this matched string for the next search */
        start_idx += match_idx + match_len;

        /* skip to the next character if it was a zero-length match */
        if (match_len == 0)
        {
            /* copy the character we're skipping to the output */
            p.set((char *)str + VMB_LEN + start_idx);
            dstp.setch(p.getch());

            /* move on to the next character */
            start_idx += 1;
        }

        /* if we're only performing a single replacement, stop now */
        if (!(flags & VMBIFTADS_REPLACE_ALL))
            break;
    }

    /* add the part after the end of the matched text */
    if ((size_t)start_idx < vmb_get_len(str))
        memcpy(dstp.getptr(), str + VMB_LEN + start_idx,
               vmb_get_len(str) - start_idx);

    /* return the string */
    retval_obj(vmg_ ret_obj);

done:
    /* discard the garbage collection protection references */
    G_stk->discard(3);

    /* if we created the pattern string, delete it */
    if (cpat != 0 && cpat_is_ours)
        CRegexParser::free_pattern(cpat);
}

/* ------------------------------------------------------------------------ */
/*
 *   savepoint - establish an undo savepoint
 */
void CVmBifTADS::savepoint(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* establish the savepoint */
    G_undo->create_savept(vmg0_);
}

/*
 *   undo - undo changes to most recent savepoint
 */
void CVmBifTADS::undo(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* if no undo is available, return nil to indicate that we can't undo */
    if (G_undo->get_savept_cnt() == 0)
    {
        /* we can't undo */
        retval_nil(vmg0_);
    }
    else
    {
        /* undo to the savepoint */
        G_undo->undo_to_savept(vmg0_);

        /* tell the caller that we succeeded */
        retval_true(vmg0_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   save 
 */
void CVmBifTADS::save(VMG_ uint argc)
{
    char fname[OSFNMAX];
    CVmFile *file;
    osfildef *fp;
    
    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* get the filename as a null-terminated string */
    pop_str_val_fname(vmg_ fname, sizeof(fname));

    /* open the file */
    fp = osfoprwtb(fname, OSFTT3SAV);
    if (fp == 0)
        err_throw(VMERR_CREATE_FILE);

    /* set up the file writer */
    file = new CVmFile();
    file->set_file(fp, 0);

    err_try
    {
        /* save the state */
        CVmSaveFile::save(vmg_ file);
    }
    err_finally
    {
        /* close the file */
        delete file;
    }
    err_end;
}

/*
 *   restore
 */
void CVmBifTADS::restore(VMG_ uint argc)
{
    char fname[OSFNMAX];
    CVmFile *file;
    osfildef *fp;
    int err;

    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* get the filename as a null-terminated string */
    pop_str_val_fname(vmg_ fname, sizeof(fname));

    /* open the file */
    fp = osfoprb(fname, OSFTT3SAV);
    if (fp == 0)
        err_throw(VMERR_FILE_NOT_FOUND);

    /* set up the file reader */
    file = new CVmFile();
    file->set_file(fp, 0);

    err_try
    {
        /* restore the state */
        err = CVmSaveFile::restore(vmg_ file);
    }
    err_finally
    {
        /* close the file */
        delete file;
    }
    err_end;

    /* if an error occurred, throw an exception */
    if (err != 0)
        err_throw(err);
}

/*
 *   restart
 */
void CVmBifTADS::restart(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* reset the VM to the image file's initial state */
    CVmSaveFile::reset(vmg0_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the maximum value from a set of argument 
 */
void CVmBifTADS::get_max(VMG_ uint argc)
{
    uint i;
    vm_val_t cur_max;
    
    /* make sure we have at least one argument */
    if (argc < 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* start with the first argument as the presumptive maximum */
    cur_max = *G_stk->get(0);

    /* compare each argument in turn */
    for (i = 1 ; i < argc ; ++i)
    {
        /* 
         *   compare this value to the maximum so far; if this value is
         *   greater, it becomes the new maximum so far 
         */
        if (G_stk->get(i)->compare_to(vmg_ &cur_max) > 0)
            cur_max = *G_stk->get(i);
    }

    /* discard the arguments */
    G_stk->discard(argc);

    /* return the maximum value */
    retval(vmg_ &cur_max);
}

/*
 *   Get the minimum value from a set of argument 
 */
void CVmBifTADS::get_min(VMG_ uint argc)
{
    uint i;
    vm_val_t cur_min;

    /* make sure we have at least one argument */
    if (argc < 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* start with the first argument as the presumptive minimum */
    cur_min = *G_stk->get(0);

    /* compare each argument in turn */
    for (i = 1 ; i < argc ; ++i)
    {
        /* 
         *   compare this value to the minimum so far; if this value is
         *   less, it becomes the new minimum so far 
         */
        if (G_stk->get(i)->compare_to(vmg_ &cur_min) < 0)
            cur_min = *G_stk->get(i);
    }

    /* discard the arguments */
    G_stk->discard(argc);

    /* return the minimum value */
    retval(vmg_ &cur_min);
}

/* ------------------------------------------------------------------------ */
/*
 *   makeString - construct a string by repeating a character; by
 *   converting a unicode code point to a string; or by converting a list
 *   of unicode code points to a string 
 */
void CVmBifTADS::make_string(VMG_ uint argc)
{
    vm_val_t val;
    long rpt;
    vm_obj_id_t new_str_obj;
    CVmObjString *new_str;
    size_t new_str_len;
    char *new_strp;
    const char *lstp = 0;
    const char *strp = 0;
    size_t len;
    size_t i;
    utf8_ptr dst;
    
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* get the base value */
    G_stk->pop(&val);

    /* if there's a repeat count, get it */
    rpt = (argc >= 2 ? pop_long_val(vmg0_) : 1);

    /* if the repeat count is less than or equal to zero, make it 1 */
    if (rpt < 1)
        rpt = 1;

    /* leave the original value on the stack to protect it from GC */
    G_stk->push(&val);

    /* 
     *   see what we have, and calculate how much space we'll need for the
     *   result string 
     */
    switch(val.typ)
    {
    case VM_LIST:
        /* it's a list of integers giving unicode character values */
        lstp = G_const_pool->get_ptr(val.val.ofs);

    do_list:
        /* get the list count */
        len = vmb_get_len(lstp);

        /* 
         *   Run through the list and get the size of each character, so
         *   we can determine how long the string will have to be. 
         */
        for (new_str_len = 0, i = 1 ; i <= len ; ++i)
        {
            vm_val_t ele_val;
            
            /* get this element */
            CVmObjList::index_list(vmg_ &ele_val, lstp, i);

            /* if it's not an integer, it's an error */
            if (ele_val.typ != VM_INT)
                err_throw(VMERR_INT_VAL_REQD);

            /* add this character's byte size to the string size */
            new_str_len +=
                utf8_ptr::s_wchar_size((wchar_t)ele_val.val.intval);
        }
        break;

    case VM_SSTRING:
        /* get the string pointer */
        strp = G_const_pool->get_ptr(val.val.ofs);
        
    do_string:
        /* 
         *   it's a string - the output length is the same as the input
         *   length 
         */
        new_str_len = vmb_get_len(strp);
        break;

    case VM_INT:
        /* 
         *   it's an integer giving a unicode character value - we just
         *   need enough space to store this particular character 
         */
        new_str_len = utf8_ptr::s_wchar_size((wchar_t)val.val.intval);
        break;

    case VM_OBJ:
        /* check to see if it's a string */
        if ((strp = val.get_as_string(vmg0_)) != 0)
            goto do_string;

        /* check to see if it's a list */
        if ((lstp = val.get_as_list(vmg0_)) != 0)
            goto do_list;

        /* it's invalid */
        err_throw(VMERR_BAD_TYPE_BIF);
        break;

    default:
        /* other types are invalid */
        err_throw(VMERR_BAD_TYPE_BIF);
        break;
    }

    /* 
     *   if the length times the repeat count would be over the maximum
     *   16-bit string length, it's an error 
     */
    if (new_str_len * rpt > 0xffffL - VMB_LEN)
        err_throw(VMERR_BAD_VAL_BIF);

    /* multiply the length by the repeat count */
    new_str_len *= rpt;
    
    /* allocate the string and gets its buffer */
    new_str_obj = CVmObjString::create(vmg_ FALSE, new_str_len);
    new_str = (CVmObjString *)vm_objp(vmg_ new_str_obj);
    new_strp = new_str->cons_get_buf();
    
    /* set up the destination pointer */
    dst.set(new_strp);

    /* run through the number of iterations requested */
    for ( ; rpt != 0 ; --rpt)
    {
        /* build one iteration of the string, according to the type */
        if (lstp != 0)
        {
            /* run through the list */
            for (i = 1 ; i <= len ; ++i)
            {
                vm_val_t ele_val;
                
                /* get this element */
                CVmObjList::index_list(vmg_ &ele_val, lstp, i);

                /* add this character to the string */
                dst.setch((wchar_t)ele_val.val.intval);
            }
        }
        else if (strp != 0)
        {
            /* copy the string's contents into the output string */
            memcpy(dst.getptr(), strp + VMB_LEN, vmb_get_len(strp));

            /* advance past the bytes we copied */
            dst.set(dst.getptr() + vmb_get_len(strp));
        }
        else
        {
            /* set this int value */
            dst.setch((wchar_t)val.val.intval);
        }
    }

    /* return the new string */
    retval_obj(vmg_ new_str_obj);

    /* discard the GC protection */
    G_stk->discard();
}

/* ------------------------------------------------------------------------ */
/*
 *   getFuncParams 
 */
void CVmBifTADS::get_func_params(VMG_ uint argc)
{
    vm_val_t val;
    vm_val_t func;
    CVmFuncPtr hdr;
    vm_obj_id_t lst_obj;
    CVmObjList *lst;

    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* the argument can be an anonymous function object or function pointer */
    if (G_stk->get(0)->typ == VM_OBJ
        && G_predef->obj_call_prop != VM_INVALID_PROP)
    {
        uint argc = 0;
        vm_obj_id_t srcobj;
        
        /* it's an anonymous function - get the object */
        G_interpreter->pop_obj(vmg_ &func);

        /* retrieve its ObjectCallProp value, and make sure it's a function */
        if (!vm_objp(vmg_ func.val.obj)->get_prop(
            vmg_ G_predef->obj_call_prop, &func, func.val.obj, &srcobj, &argc)
            || func.typ != VM_FUNCPTR)
            err_throw(VMERR_FUNCPTR_VAL_REQD);
    }
    else
    {
        /* it's a simple function pointer - retrieve it */
        G_interpreter->pop_funcptr(vmg_ &func);
    }

    /* set up a pointer to the function header */
    hdr.set((const uchar *)G_code_pool->get_ptr(func.val.ofs));

    /* 
     *   Allocate our return list.  We need three elements: [minArgs,
     *   optionalArgs, isVarargs].  
     */
    lst_obj = CVmObjList::create(vmg_ FALSE, 3);

    /* get the list object, properly cast */
    lst = (CVmObjList *)vm_objp(vmg_ lst_obj);

    /* set the minimum argument count */
    val.set_int(hdr.get_min_argc());
    lst->cons_set_element(0, &val);

    /* 
     *   set the optional argument count (which is always zero for a
     *   function, since there is no way to specify named optional arguments
     *   for a function) 
     */
    val.set_int(0);
    lst->cons_set_element(1, &val);

    /* set the varargs flag */
    val.set_logical(hdr.is_varargs());
    lst->cons_set_element(2, &val);

    /* return the list */
    retval_obj(vmg_ lst_obj);

    /* 
     *   re-touch the currently executing method's code page to make sure
     *   it's the most recently used item in the cache, to avoid swapping it
     *   out 
     */
    G_interpreter->touch_entry_ptr_page(vmg0_);
}

