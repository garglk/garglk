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
#include "vmimport.h"
#include "vmpredef.h"
#include "vmlookup.h"
#include "vmnetfil.h"


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
                 *   object, as we expose these as special types.  
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


/*
 *   seed the rng 
 */
void CVmBifTADS::randomize(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* 
     *   load the ISAAC initialization vector with some truly random data
     *   from the operating system 
     */
    os_gen_rand_bytes((unsigned char *)G_bif_tads_globals->isaac_ctx->rsl,
                      sizeof(G_bif_tads_globals->isaac_ctx->rsl));

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
 *   Map a random 32-bit number to a smaller range 0..range-1.
 *   
 *   Use the "multiplication" method Knuth describes in TAoCP Vol 2 section
 *   3.4.2.  This treats the source value as a fractional value in the range
 *   [0..1); we multiply this fractional value by the upper bound to get a
 *   linearly distributed number [0..range).  To turn a 32-bit integer into a
 *   fractional value in the range [0..1), divide by 2^32.
 *   
 *   We do this calculation entirely with 32-bit integer arithmetic.  The
 *   nominal calculation we're performing is:
 *   
 *.      rand_val = (ulong)((((double)rand_val) / 4294967296.0)
 *.                         * (double)range);
 *   
 *   To do the arithmetic entirely with integers, we refactor this by first
 *   calculating the 64-bit product of rand_val * range, then dividing the
 *   product by 2^32.  This isn't possible to do directly with 32-bit ints,
 *   of course.  But the division is particularly easy because it's the same
 *   as a 32-bit right-shift.  So the trick is to factor out the low-order
 *   32-bits of the product and the high-order 32-bits, which we can do with
 *   some bit shifting.  
 */
static inline ulong rand_range(ulong rand_val, ulong range)
{
    /* calculate the high-order 32 bits of (rand_val / 2^32 * range) */
    ulong hi = (((rand_val >> 16) & 0xffff) * ((range >> 16) & 0xffff))
               + ((((rand_val >> 16) & 0xffff) * (range & 0xffff)) >> 16)
               + (((rand_val & 0xffff) * ((range >> 16) & 0xffff)) >> 16);
    
    /* calculate the low-order 32 bits */
    ulong lo = ((((rand_val >> 16) & 0xffff) * (range & 0xffff)) & 0xffff)
               + (((rand_val & 0xffff) * ((range >> 16) & 0xffff)) & 0xffff)
               + ((((rand_val & 0xffff) * (range & 0xffff)) >> 16) & 0xffff);

    /* add the carry from the low part into the high part to get the result */
    return hi + (lo >> 16);
}

/* calculate a random number in the range [lower, upper] */
static inline ulong rand_range(ulong rand_val, ulong lower, ulong upper)
{
    return lower + rand_range(rand_val, upper - lower + 1);
}

/* random number with two ranges */
static inline ulong rand_range(ulong rand_val,
                               ulong lo1, ulong hi1,
                               ulong lo2, ulong hi2)
{
    rand_val = rand_range(rand_val, (hi1 - lo1 + 1) + (hi2 - lo2 + 1));
    return rand_val + (rand_val <= hi1 - lo1 ? lo1 : lo2 - (hi1 - lo1 + 1));
}

/* random number with three ranges */
static inline ulong rand_range(ulong rand_val,
                               ulong lo1, ulong hi1,
                               ulong lo2, ulong hi2,
                               ulong lo3, ulong hi3)
{
    rand_val = rand_range(rand_val,
                          (hi1 - lo1 + 1)
                          + (hi2 - lo2 + 1)
                          + (hi3 - lo3 + 1));
    return rand_val
        + (rand_val <= hi1 - lo1 ? lo1 :
           rand_val <= hi1 - lo1 + hi2 - lo2 + 1 ? lo2 - (hi1 - lo1 + 1) :
           lo3 - (hi1 - lo1 + 1) - (hi2 - lo2 + 1));
}

/* ------------------------------------------------------------------------ */
/*
 *   Random string template parser 
 */
class RandStrParser
{
public:
    /* initialize - parse the template string */
    RandStrParser(const char *src, size_t len);

    /* delete */
    ~RandStrParser();

    /* execute the template and return a string object result */
    void exec(VMG_ vm_val_t *result);

    /* get the current character */
    wchar_t getch() { return rem > 0 ? p.getch() : 0; }

    /* parse an integer value */
    int parseInt()
    {
        /* parse digits in the input */
        int acc;
        for (acc = 0 ; more() && is_digit(getch()) ; skip())
        {
            /* add this digit into the accumulator */
            acc *= 10;
            acc += value_of_digit(getch());
        }

        /* return the accumulator value */
        return acc;
    }

    /* skip the current character */
    void skip()
    {
        if (rem != 0)
            p.inc(&rem);
    }

    /* is there more input? */
    int more() const { return rem != 0; }

    /* get the number of bytes remaining */
    size_t getrem() const { return rem; }

protected:
    /* source string pointer and remaining length */
    utf8_ptr p;
    size_t rem;

    /* root of the parse tree */
    class RandStrNode *tree;
};

class RandStrNode
{
public:
    RandStrNode()
    {
        firstChild = lastChild = 0;
        nextSibling = 0;
    }

    virtual ~RandStrNode()
    {
        while (firstChild != 0)
        {
            RandStrNode *nxt = firstChild->nextSibling;
            delete firstChild;
            firstChild = nxt;
        }
    }

    /* calculate the maximum length for the generated string for this node */
    virtual int maxlen() = 0;

    /* generate the string for this node */
    virtual void generate(VMG_ wchar_t *&dst) = 0;

    /* add a child */
    void addChild(RandStrNode *chi)
    {
        if (lastChild != 0)
            lastChild->nextSibling = chi;
        else
            firstChild = chi;
        lastChild = chi;
    }

    /* tree links */
    RandStrNode *firstChild, *lastChild;
    RandStrNode *nextSibling;
};

/* 
 *   range element - this covers a single character or single 'a-z' range
 *   within a [character list] expression 
 */
class RandStrRange: public RandStrNode
{
public:
    RandStrRange(wchar_t c1, wchar_t c2)
    {
        if (c2 > c1)
            this->c1 = c1, this->c2 = c2;
        else
            this->c1 = c2, this->c2 = c1;
    }

    virtual int maxlen() { return 1; }
    virtual void generate(VMG_ wchar_t *&dst) { }

    /* low and high end of the character range, inclusive */
    wchar_t c1, c2;
};

/* literal string - this is a string of characters enclosed in quotes */
class RandStrLit: public RandStrNode
{
public:
    RandStrLit(class RandStrParser *p)
    {
        /* 
         *   Allocate our buffer.  To make this easy, we use the total byt
         *   length of the string remaining.  This is more than we'll
         *   actually ever need, on two counts: we could have more bytes than
         *   characters in the remaining source, since some characters could
         *   be multi-byte; and the quoted section probably isn't the whole
         *   rest of the string.  So we'll waste a little memory.  But this
         *   object is short-lived and the wasted space is usually trivial,
         *   so it's not worth the additional work to be more precise in
         *   allocating the buffer.  
         */
        buf = new wchar_t[p->getrem()];
        len = 0;

        /* skip the open quote, then parse the contents */
        for (p->skip() ; p->more() ; p->skip())
        {
            wchar_t ch;
            
            /* if we're at a close quote, we might be done */
            if ((ch = p->getch()) == '"')
            {
                /* skip the quote */
                p->skip();

                /* if it's not stuttered, we're done */
                if ((ch = p->getch()) != '"')
                    break;
            }

            /* add this character to our buffer */
            buf[len++] = ch;
        }
    }

    ~RandStrLit()
    {
        delete [] buf;
    }

    virtual int maxlen() { return len; }
    virtual void generate(VMG_ wchar_t *&dst)
    {
        /* copy our string */
        memcpy(dst, buf, len*sizeof(*dst));

        /* advance the pointer */
        dst += len;
    }

protected:
    /* our buffer */
    wchar_t *buf;

    /* number of charactres in the buffer */
    size_t len;
};

/* atom - this is a single character specifier or a [character list] */
class RandStrAtom: public RandStrNode
{
public:
    RandStrAtom(class RandStrParser *p)
    {
        /* we don't have a character class or literal character yet */
        ch = 0;
        isLit = FALSE;
        
        /* we have no character list items yet */
        listChars = 0;
        
        /* check for a character list of the form [abcw-z] */
        if (p->getch() == '[')
        {
            /* keep going until we reach the ']' or end of string */
            for (p->skip() ; p->more() && p->getch() != ']' ; )
            {
                /* get the next character */
                wchar_t c1 = p->getch();

                /* check for quoting */
                if (c1 == '%')
                {
                    /* skip the '%' and get the quoted character */
                    p->skip();
                    c1 = (p->more() ? p->getch() : '%');
                }

                /* skip the character */
                p->skip();

                /* if the next character is '-', we have a range expression */
                if (p->getch() == '-')
                {
                    /* skip the '-' */
                    p->skip();

                    /* if there's a '%', skip it as well */
                    if (p->getch() == '%')
                        p->skip();

                    /* get the upper bound character */
                    wchar_t c2 = (p->more() ? p->getch() : c1);

                    /* skip the second character */
                    p->skip();

                    /* add the range */
                    addChild(new RandStrRange(c1, c2));
                    listChars += (c2 > c1 ? c2 - c1 : c1 - c2) + 1;
                }
                else
                {
                    /* it's just this character in the range */
                    addChild(new RandStrRange(c1, c1));
                    listChars += 1;
                }
            }

            /* skip the ']' */
            p->skip();
        }
        else if (p->getch() == '%')
        {
            /* 
             *   quoted character expression - skip the '%' and store the
             *   single character 
             */
            p->skip();
            ch = p->more() ? p->getch() : '%';

            /* note that this is a literal character expression */
            isLit = TRUE;

            /* skip the character */
            p->skip();
        }
        else
        {
            /* character class expression - store it and skip it */
            ch = p->getch();
            p->skip();
        }
    }

    /* an atom generates exactly one character */
    virtual int maxlen() { return 1; }

    /* generate the atom */
    virtual void generate(VMG_ wchar_t *&dst)
    {
        /* 
         *   If we have a character list expression, choose a character from
         *   the list.  Otherwise, generate a character according to our
         *   character class code.  
         */
        wchar_t outc;
        if (listChars != 0)
        {
            /* pick a random index in our range */
            wchar_t n = (wchar_t)rand_range(rng_next(vmg0_), listChars);

            /* find the range containing this character */
            for (RandStrNode *chi = firstChild ; chi != 0 ;
                 chi = chi->nextSibling)
            {
                /* cast the child - we know it's a range node */
                RandStrRange *r = (RandStrRange *)chi;

                /* if the index is in this range, we've found it */
                if (n <= r->c2 - r->c1)
                {
                    outc = r->c1 + n;
                    break;
                }

                /* deduct the size of this range from the index */
                n -= (r->c2 - r->c1 + 1);
            }
        }
        else if (isLit)
        {
            /* literal character expression */
            outc = ch;
        }
        else
        {
            /* no list, so generate a character based on the class code */
            ulong rand_val = rng_next(vmg0_);
            switch (ch)
            {
            case 'i':
                /* printable ASCII character 32-126 */
                outc = (wchar_t)rand_range(rand_val, 32, 126);
                break;
                
            case 'l':
                /* printable Latin-1 character 32-126, 160-255 */
                outc = (wchar_t)rand_range(rand_val, 32, 126, 160, 255);
                break;
                
            case 'u':
                /* 
                 *   Printable Unicode character.  This excludes undefined
                 *   character, the private use area, and the control
                 *   characters.  
                 */
                for (;;)
                {
                    /* exclude the control and private use ranges */
                    outc = (wchar_t)rand_range(
                        rand_val, 32, 127, 160, 0xDFFF, 0xF900, 0xFFFE);
                    
                    /* if it's a unicode character, we're set */
                    if (t3_is_unichar(ch))
                        break;
                    
                    /* pick a new random number */
                    rand_val = rng_next(vmg0_);
                }
                break;
                
            case 'd':
                /* decimal digit 0-9 */
                outc = (wchar_t)rand_range(rand_val, '0', '9');
                break;
                
            case 'X':
                /* upper-case hex digit 0-9 A-F */
                outc = (wchar_t)rand_range(rand_val, '0', '9', 'A', 'F');
                break;
                
            case 'x':
                /* lower-case hex digit 0-9 a-f */
                outc = (wchar_t)rand_range(rand_val, '0', '9', 'a', 'f');
                break;
                
            case 'A':
                /* letter A-Z */
                outc = (wchar_t)rand_range(rand_val, 'A', 'Z');
                break;
                
            case 'a':
                /* letter a-z */
                outc = (wchar_t)rand_range(rand_val, 'a', 'z');
                break;
                
            case 'b':
                /* random byte value 0-255 */
                outc = (wchar_t)rand_range(rand_val, 0, 255);
                break;
                
            case 'c':
                /* mixed-case letter A-Z a-z */
                outc = (wchar_t)rand_range(rand_val, 'a', 'z', 'A', 'Z');
                break;
                
            case 'z':
                /* mixed-case letter or number 0-9 a-z A-Z */
                outc = (wchar_t)rand_range(rand_val,
                                           '0', '9', 'a', 'z', 'A', 'Z');
                break;

            default:
                /* no character */
                return;
            }
        }

        /* store the output character */
        *dst++ = outc;
    }

protected:
    /* for a single-character expression, the character */
    wchar_t ch;

    /* is 'ch' a literal character, or a character code expression? */
    int isLit;

    /* number of characters included in all of [character list] elements */
    int listChars;
};

/* 
 *   repeat group - this is an item optionally followed by a {repeat count},
 *   a '*', or a '?' 
 */
class RandStrRepeat: public RandStrNode
{
public:
    RandStrRepeat(RandStrParser *p);

    /* 
     *   If we have a finite upper bound, our maximum length is our child
     *   item length times our maximum repeat count.  If there's no upper
     *   bound, our output is in principle infinite, but the probability of
     *   the string being at least a given length N is (2/3)^N, so the odds
     *   drop off very rapidly as N increases; at N=25, we're down to one in
     *   25,000.  Return 50, and use this as a hard cap in the generator.  
     */
    static const int MAX_UNBOUNDED = 50;
    virtual int maxlen()
    {
        return firstChild->maxlen() * (hi < 0 ? MAX_UNBOUNDED : hi);
    }

    /* generate */
    virtual void generate(VMG_ wchar_t *&dst)
    {
        if (hi >= 0)
        {
            /* 
             *   Finite upper bound.  Pick a random repeat count from lo to
             *   hi, and generate our child that many times.  
             */
            for (ulong cnt = rand_range(rng_next(vmg0_), lo, hi) ;
                 cnt != 0 ; --cnt)
                firstChild->generate(vmg_ dst);
        }
        else
        {
            /* no upper bound - start with the fixed lower bound */
            int i;
            for (i = 0 ; i < lo ; ++i)
                firstChild->generate(vmg_ dst);

            /* 
             *   Now add additional items one at a time, with 67% probability
             *   for each added item.  Stop when we fail the roll.  
             */
            for (i = 0 ;
                 rand_range(rng_next(vmg0_), 0, 99) < 67 && i < MAX_UNBOUNDED ;
                 ++i)
                firstChild->generate(vmg_ dst);
        }
    }

    /* 
     *   Repeat range.  If hi is negative, there's no upper bound.  Rather
     *   than picking a number from 0 to infinity uniformly (which is
     *   obviously impractical, since the expectation value is infinite), we
     *   have a 50% chance of adding each additional item. 
     */
    int lo, hi;
};

/* concatenation list - this is a list of repeat groups */
class RandStrCatList: public RandStrNode
{
public:
    RandStrCatList(RandStrParser *p)
    {
        /* 
         *   parse repeat groups until we reach the end of the string, the
         *   end of the alternative, or the end of the parenthesized group 
         */
        while (p->more() && p->getch() != ')' && p->getch() != '|')
            addChild(new RandStrRepeat(p));
    }

    /* 
     *   we generate a concatenation of our children, so our length is the
     *   sum of our child lengths 
     */
    virtual int maxlen()
    {
        int sum = 0;
        for (RandStrNode *chi = firstChild ; chi != 0 ;
             sum += chi->maxlen(), chi = chi->nextSibling) ;

        return sum;
    }

    /* generate */
    virtual void generate(VMG_ wchar_t *&dst)
    {
        /* generate each child sequentially */
        for (RandStrNode *chi = firstChild ; chi != 0 ; chi = chi->nextSibling)
            chi->generate(vmg_ dst);
    }
};

/* alternative list - this is a list of CatList items separated by '|' */
class RandStrAltList: public RandStrNode
{
public:
    RandStrAltList(RandStrParser *p, int isOuterExpr)
    {
        /* no alternatives yet */
        altCount = 0;

        /* 
         *   parse alternatives until we reach the end of the string or a
         *   close paren (unless we're the outermost expression, in which
         *   case we treat ')' as an ordinary character since there's no
         *   enclosing group to close) 
         */
        while (p->more() && (isOuterExpr || p->getch() != ')'))
        {
            /* parse the next alternative and add it to our child list */
            addChild(new RandStrCatList(p));
            altCount += 1;

            /* if there's a '|', skip it */
            if (p->getch() == '|')
                p->skip();
        }
    }

    /* 
     *   when we generate, we choose one child to generate, so our maximum
     *   length is the highest of our individual child lengths
     */
    virtual int maxlen()
    {
        int lmax = 0;
        for (RandStrNode *chi = firstChild ; chi != 0 ; chi = chi->nextSibling)
        {
            int l = chi->maxlen();
            if (l > lmax)
                lmax = l;
        }

        return lmax;
    }

    /* generate */
    virtual void generate(VMG_ wchar_t *&dst)
    {
        /* pick one of our alternatives at random */
        int n = rand_range(rng_next(vmg0_), altCount);

        /* find it and generate it */
        RandStrNode *chi;
        for (chi = firstChild ; n != 0 && chi != 0 ;
             --n, chi = chi->nextSibling) ;

        /* generate the expression */
        if (chi != 0)
            chi->generate(vmg_ dst);
    }

    /* number of alternatives */
    int altCount;
};

/* 
 *   repeat group - this is an atom or parenthesized expression, optionally
 *   followed by a postfix repeat count 
 */
RandStrRepeat::RandStrRepeat(RandStrParser *p)
{
    /* if we have an open paren, parse the enclosed alt list */
    if (p->getch() == '(')
    {
        /* skip the open paren */
        p->skip();
        
        /* parse the alt list */
        addChild(new RandStrAltList(p, FALSE));
        
        /* if there's a close paren, skip it */
        if (p->getch() == ')')
            p->skip();
    }
    else if (p->getch() == '"')
    {
        /* parse the literal string */
        addChild(new RandStrLit(p));
    }
    else
    {
        /* no parens or quotes, so this is a simple character atom */
        addChild(new RandStrAtom(p));
    }
    
    /* check for {n}, {n-m}, *, or ? */
    if (p->getch() == '{')
    {
        /* skip the '{' */
        p->skip();
        
        /* parse the low end of the range */
        lo = p->parseInt();
        
        /* if we're at a ',', get the high end of the range */
        if (p->getch() == ',')
        {
            /* there's a separate high part - get it */
            for (p->skip() ; is_space(p->getch()) ; p->skip()) ;

            /* 
             *   if the high part is missing, there's no upper bound;
             *   otherwise parse the finite upper bound 
             */
            if (p->getch() == '}' || p->getch() == '\0')
            {
                /* -1 means "infinity" */
                hi = -1;
            }
            else
            {
                /* parse the upper bound */
                hi = p->parseInt();

                /* make sure hi > lo */
                if (hi < lo)
                {
                    int tmp = hi;
                    hi = lo;
                    lo = tmp;
                }
            }
        }
        else
        {
            /* there's only the one number, so it's the low and high */
            hi = lo;
        }
        
        /* skip the '}' */
        if (p->getch() == '}')
            p->skip();
    }
    else if (p->getch() == '+')
    {
        /* one or more */
        lo = 1;
        hi = -1;
        p->skip();
    }
    else if (p->getch() == '*')
    {
        /* zero or more */
        lo = 0;
        hi = -1;
        p->skip();
    }
    else if (p->getch() == '?')
    {
        /* zero or one */
        lo = 0;
        hi = 1;
        p->skip();
    }
    else
    {
        /* no repeat count - generate exactly once */
        lo = hi = 1;
    }
}

/* 
 *   main parser implementation 
 */
RandStrParser::RandStrParser(const char *src, size_t len)
{
    /* set up our string pointer */
    this->p.set((char *)src);
    this->rem = len;

    /* parse the tree starting at the top of the recursive descent */
    tree = new RandStrAltList(this, TRUE);
}

RandStrParser::~RandStrParser()
{
    /* delete the parse tree */
    delete tree;
}

void RandStrParser::exec(VMG_ vm_val_t *result)
{
    /* calculate the maximum length of the result */
    size_t len = tree->maxlen();

    /* if the maximum length is zero, just return an empty string */
    if (len == 0)
    {
        result->set_obj(CVmObjString::create(vmg_ FALSE, 0));
        return;
    }

    /* allocate a wchar_t buffer of the maximum length */
    wchar_t *buf = new wchar_t[len];

    /* generate the string */
    wchar_t *dst = buf;
    tree->generate(vmg_ dst);

    /* build the return string */
    result->set_obj(CVmObjString::create(vmg_ FALSE, buf, dst - buf));

    /* done with the temporary buffer */
    delete [] buf;
}

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
    int32 range;
    int use_range;
    int choose_an_arg = FALSE;
    int choose_an_ele = FALSE;
    ulong rand_val;

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
    else if (argc == 1
             && G_stk->get(0)->is_listlike(vmg0_)
             && (range = G_stk->get(0)->ll_length(vmg0_)) >= 0)
    {
        /* use the range of 1..length */
        use_range = TRUE;

        /* note that we're choosing from a list-like object */
        choose_an_ele = TRUE;

        /* note - leave the object on the stack as gc protection */
    }
    else if (argc == 1
             && G_stk->get(0)->get_as_string(vmg0_) != 0)
    {
        /* 
         *   It's a string, giving a template for generating a new random
         *   string.  First, get the string argument.  
         */
        const char *tpl = G_stk->get(0)->get_as_string(vmg0_);
        size_t tplbytes = vmb_get_len(tpl);
        tpl += VMB_LEN;

#if 1
        /* parse it */
        RandStrParser *rsp = new RandStrParser(tpl, tplbytes);

        err_try
        {
            /* generate the string */
            vm_val_t ret;
            rsp->exec(vmg_ &ret);

            /* return it */
            retval(vmg_ &ret);
        }
        err_finally
        {
            /* delete our parser on the way out */
            delete rsp;
        }
        err_end;
        
#else
        /* figure its character length */
        utf8_ptr tplp((char *)tpl);
        size_t tplchars = tplp.len(tplbytes);

        /* allocate temporary space for the result, as wide characters */
        wchar_t *buf = new wchar_t[tplchars], *dst;

        /* run through the template and generate random characters */
        for (dst = buf ; tplbytes != 0 ; )
        {
            /* generate a random number for this character */
            rand_val = rng_next(vmg0_);

            /* get and skip the next template character */
            wchar_t tch = tplp.getch();
            tplp.inc(&tplbytes);

            /* translate the template character */
            wchar_t ch;
            switch (tch)
            {
            case '.':
                /* printable ASCII character 32-126 */
                ch = (wchar_t)rand_range(rand_val, 32, 126);
                break;

            case '?':
                /* printable Latin-1 character 32-126, 160-255 */
                ch = (wchar_t)rand_range(rand_val, 32, 126, 160, 255);
                break;

            case '*':
                /* 
                 *   Printable Unicode character.  This excludes undefined
                 *   character, the private use area, and the control
                 *   characters.  
                 */
                for (;;)
                {
                    /* exclude the control and private use ranges */
                    ch = (wchar_t)rand_range(
                        rand_val, 32, 127, 160, 0xDFFF, 0xF900, 0xFFFE);

                    /* if it's a unicode character, we're set */
                    if (t3_is_unichar(ch))
                        break;

                    /* pick a new random number */
                    rand_val = rng_next(vmg0_);
                }
                break;

            case '9':
                /* digit 0-9 */
                ch = (wchar_t)rand_range(rand_val, '0', '9');
                break;

            case 'X':
                /* upper-case hex digit 0-9 A-F */
                ch = (wchar_t)rand_range(rand_val, '0', '9', 'A', 'F');
                break;

            case 'x':
                /* lower-case hex digit 0-9 a-f */
                ch = (wchar_t)rand_range(rand_val, '0', '9', 'a', 'f');
                break;

            case 'A':
                /* letter A-Z */
                ch = (wchar_t)rand_range(rand_val, 'A', 'Z');
                break;

            case 'a':
                /* letter a-z */
                ch = (wchar_t)rand_range(rand_val, 'a', 'z');
                break;

            case 'b':
                /* random byte value 0-255 */
                ch = (wchar_t)rand_range(rand_val, 0, 255);
                break;

            case 'c':
                /* mixed-case letter A-Z a-z */
                ch = (wchar_t)rand_range(rand_val, 'a', 'z', 'A', 'Z');
                break;

            case 'z':
                /* mixed-case letter or number 0-9 a-z A-Z */
                ch = (wchar_t)rand_range(rand_val,
                                         '0', '9', 'a', 'z', 'A', 'Z');
                break;

            case '[':
                /* 
                 *   Character range.  Scan the range to count the number of
                 *   characters included.  
                 */
                {
                    /* 
                     *   allocate a list of range descriptors - use 'len2' as
                     *   the range count, since at the most we could have one
                     *   range per byte (but it'll usually be less) 
                     */
                    struct rdesc
                    {
                        int set(wchar_t c)
                        {
                            start = end = c;
                            return 1;
                        }
                        int set(wchar_t a, wchar_t b)
                        {
                            if (b > a)
                                start = a, end = b;
                            else
                                start = b, end = a;
                            return end - start + 1;
                        }
                        wchar_t start;
                        wchar_t end;
                    };
                    rdesc *ranges = new rdesc[tplbytes];
                    int nranges = 0, nchars = 0;

                    /* scan for the closing ']' */
                    for ( ; tplbytes != 0 && tplp.getch() != ']' ;
                          tplp.inc(&tplbytes))
                    {
                        /* check for escapes */
                        wchar_t rch = tplp.getch();
                        if (rch == '%')
                        {
                            tplp.inc(&tplbytes);
                            rch = tplp.getch();
                        }
                        
                        /* if the next character is '-', it's a range */
                        if (tplbytes > 1 && tplp.getch_at(1) == '-')
                        {
                            /* skip the current character and the '-' */
                            tplp.inc(&tplbytes);
                            tplp.inc(&tplbytes);

                            /* if the next character is quoted, skip the % */
                            if (tplbytes > 1 && tplp.getch() == '%')
                                tplp.inc(&tplbytes);

                            /* 
                             *   The range count includes everything from
                             *   'rch' to the current character.  
                             */
                            if (tplbytes != 0)
                                nchars += ranges[nranges++].set(
                                    rch, tplp.getch());
                        }
                        else
                        {
                            /* it's a single character range */
                            nchars += ranges[nranges++].set(rch);
                        }
                        
                        /* if we're out of characters, we're done */
                        if (tplbytes == 0)
                            break;
                    }

                    /* pick a number from 0 to rcnt */
                    ch = (wchar_t)rand_range(rand_val, nchars);

                    /* find the character */
                    for (int i = 0 ; i < nranges ; ++i)
                    {
                        /* if it's in the current range, apply it */
                        const rdesc *r = &ranges[i];
                        if (ch <= r->end - r->start)
                        {
                            ch += r->start;
                            break;
                        }

                        /* 
                         *   it's not in this range, so deduct the range
                         *   length from the remainder and keep going 
                         */
                        ch -= r->end - r->start + 1;
                    }

                    /* done with the range list */
                    delete [] ranges;

                    /* skip the closing ']' */
                    if (tplbytes != 0 && tplp.getch() == ']')
                        tplp.inc(&tplbytes);
                }
                break;

            case '%':
                /* copy the next character unchanged */
                if (tplbytes != 0)
                {
                    ch = tplp.getch();
                    tplp.inc(&tplbytes);
                }
                else
                    ch = '%';
                break;

            default:
                /* no character */
                continue;
            }

            /* save it in our temp buffer, and note its utf8 size */
            *dst++ = ch;
        }

        /* convert the temp buffer to a real string */
        retval_obj(vmg_ CVmObjString::create(vmg_ FALSE, buf, dst - buf));

        /* done with the temporary buffer */
        delete [] buf;
#endif
        
        /* discard the argument */
        G_stk->discard();

        /* we're done */
        return;
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
        rand_val = rand_range(rand_val, range);

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
    else if (choose_an_ele)
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
            /* get the selected list element */
            G_stk->get(0)->ll_index(vmg_ &val, rand_val + 1);
        }

        /* set the result */
        retval(vmg_ &val);

        /* discard our gc protection */
        G_stk->discard();
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
 *   toString - convert to string 
 */
void CVmBifTADS::toString(VMG_ uint argc)
{
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 3);

    /* pop the argument */
    vm_val_t val;
    G_stk->pop(&val);

    /* if there's a radix specified, pop it as well */
    int radix = (argc >= 2 ? pop_int_val(vmg0_) : 10);

    /* the radix must be from 2 to 36 */
    if (radix < 2 || radix > 36)
        err_throw(VMERR_BAD_VAL_BIF);

    /* assume unsigned */
    int flags = TOSTR_UNSIGNED;;

    /* 
     *   If the 'isSigned' argument is present, pop it as a bool; otherwise
     *   treat the value as signed if the radix is decimal, otherwise
     *   unsigned. 
     */
    if (argc >= 3 ? pop_bool_val(vmg0_) : radix == 10)
        flags &= ~TOSTR_UNSIGNED;

    /* convert the value */
    char buf[50];
    vm_val_t new_str;
    const char *p = CVmObjString::cvt_to_str(
        vmg_ &new_str, buf, sizeof(buf), &val, radix, flags);

    /* save the new string on the stack to protect from garbage collection */
    G_stk->push(&new_str);

    /* 
     *   if the return value wasn't already a new object, create a string
     *   from the return value 
     */
    if (new_str.typ == VM_OBJ)
    {
        /* we've already allocated a new string - return it */
        retval_obj(vmg_ new_str.val.obj);
    }
    else
    {
        /* we just have a string in a buffer - create a new string from it */
        retval_obj(vmg_ CVmObjString::create(
            vmg_ FALSE, p + VMB_LEN, vmb_get_len(p)));
    }

    /* done with the new string */
    G_stk->discard();
}

/*
 *   toInteger - convert to an integer
 */
void CVmBifTADS::toInteger(VMG_ uint argc)
{
    /* convert as an integer only */
    toIntOrNum(vmg_ argc, TRUE);
}

/*
 *   toNumber - convert to an integer or BigNumber 
 */
void CVmBifTADS::toNumber(VMG_ uint argc)
{
    /* convert as integer or BigNumber */
    toIntOrNum(vmg_ argc, FALSE);
}

/*
 *   Common handler for toInteger and toNumber 
 */
void CVmBifTADS::toIntOrNum(VMG_ uint argc, int int_only)
{    
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* check for BigNumber values */
    vm_val_t *valp = G_stk->get(0);
    if (valp->typ == VM_OBJ
        && CVmObjBigNum::is_bignum_obj(vmg_ valp->val.obj))
    {
        /*
         *   If we only want integer results, try converting the BigNumber to
         *   an integer.  Otherwise, simply return the BigNumber as-is. 
         */
        if (int_only)
        {
            /* we only want an integer result - convert to int */
            long intval = ((CVmObjBigNum *)vm_objp(vmg_ valp->val.obj))
                          ->convert_to_int();

            /* return the integer value */
            retval_int(vmg_ intval);
        }
        else
        {
            /* BigNumber results are okay - just return the BigNumber */
            retval(vmg_ valp);
        }
        
        /* discard arguments, and we're done */
        G_stk->discard(argc);
        return;
    }

    /* if it's already an integer, just return the same value */
    if (valp->typ == VM_INT)
    {
        /* just return the argument value */
        retval_int(vmg_ valp->val.intval);

        /* discard arguments and return */
        G_stk->discard(argc);
        return;
    }

    /* if it's true or nil, convert to 1 or 0 */
    if (valp->typ == VM_TRUE || valp->typ == VM_NIL)
    {
        /* return 1 for true, 0 for nil */
        retval_int(vmg_ valp->typ == VM_TRUE ? 1 : 0);
        G_stk->discard(argc);
        return;
    }

    /* the only other type of value we can convert is a string */
    const char *strp = G_stk->get(0)->get_as_string(vmg0_);
    if (strp == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the string length and buffer pointer */
    size_t len = vmb_get_len(strp);
    strp += VMB_LEN;

    /* if there's a radix specified, pop it as well; the default is 10 */
    int radix = 10;
    if (argc >= 2)
    {
        /* get the radix from the stack */
        radix = G_stk->get(1)->num_to_int();

        /* make sure it's in the valid range */
        if (radix < 2 || radix > 36)
            err_throw(VMERR_BAD_VAL_BIF);
    }

    /* get a version of the string stripped of leading and trailing spaces */
    const char *p2 = strp;
    size_t len2 = len;
    for ( ; len2 != 0 && is_space(*p2) ; ++p2, --len2) ;
    for ( ; len2 != 0 && is_space(*(p2 + len2 - 1)) ; --len2) ;

    /* 
     *   Check for "nil" and "true", ignoring leading and trailing spaces; if
     *   it matches either of those, return the corresponding boolean value.
     *   Otherwise, parse the string as an integer value in the given radix.
     */
    if (len2 == 3 && memcmp(p2, "nil", 3) == 0)
    {
        /* the value for "nil" is 0 */
        retval_int(vmg_ 0);
    }
    else if (len2 == 4 && memcmp(p2, "true", 3) == 0)
    {
        /* the value for "true" is 1 */
        retval_int(vmg_ 1);
    }
    else
    {
        /* parse the string as an integer (orBigNumber if it's too large) */
        vm_val_t val;
        CVmObjString::parse_num_val(vmg_ &val, strp, len, radix, int_only);
        retval(vmg_ &val);
    }

    /* discard arguments */
    G_stk->discard(argc);
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
    start_idx = 1;
    if (v3 != 0)
    {
        /* check the type */
        if (v3->typ != VM_INT)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get the value */
        start_idx = (int)v3->val.intval;
    }

    /* 
     *   remember the last search string (the second argument), and reset any
     *   old group registers (since they'd point into the previous string) 
     */
    G_bif_tads_globals->last_rex_str->val = *v2;
    G_bif_tads_globals->rex_searcher->clear_group_regs();

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

    /* if the starting index is negative, it's from the end of the string */
    start_idx += (start_idx < 0 ? (int)p.len(len) : -1);

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
    start_idx = 1;
    if (v3 != 0)
    {
        /* check the type */
        if (v3->typ != VM_INT)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get the value */
        start_idx = (int)v3->val.intval;
    }

    /* 
     *   remember the last search string (the second argument), and clear out
     *   any old group registers (since they'd point into the old string) 
     */
    G_bif_tads_globals->last_rex_str->val = *v2;
    G_bif_tads_globals->rex_searcher->clear_group_regs();

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

    /* if the starting index is negative, it's from the end of the string */
    start_idx += (start_idx < 0 ? (int)p.len(len) : -1);

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
        /* 
         *   We got a match - calculate the character index of the match
         *   offset, adjusted to a 1-base.  The character index is simply the
         *   number of characters in the part of the string up to the match
         *   index.  Note that we have to add the starting index to get the
         *   actual index in the overall string, since 'p' points to the
         *   character at the starting index.  
         */
        size_t char_idx = p.len(match_idx) + start_idx + 1;

        /* calculate the character length of the match */
        utf8_ptr matchp(p.getptr() + match_idx);
        size_t char_len = matchp.len(match_len);

        /* allocate a string containing the match */
        vm_obj_id_t match_str_obj =
            CVmObjString::create(vmg_ FALSE, matchp.getptr(), match_len);

        /* push it momentarily as protection against garbage collection */
        G_stk->push()->set_obj(match_str_obj);

        /* 
         *   set up a 3-element list to contain the return value:
         *   [match_start_index, match_length, match_string] 
         */
        char buf[VMB_LEN + VMB_DATAHOLDER * 3];
        vmb_put_len(buf, 3);
        char *dst = buf + VMB_LEN;
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
    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* get the group number to retrieve */
    int groupno = pop_int_val(vmg0_);

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
    const char *last_str =
        G_bif_tads_globals->last_rex_str->val.get_as_string(vmg0_);

    /* get the register */
    const re_group_register *reg =
        G_bif_tads_globals->rex_searcher->get_group_reg(groupno);

    /* if the group wasn't set, or there's no last string, return nil */
    if (last_str == 0 || reg->start_ofs == -1 || reg->end_ofs == -1)
    {
        retval_nil(vmg0_);
        return;
    }
    
    /* set up for a list with three elements */
    char buf[VMB_LEN + 3*VMB_DATAHOLDER];
    vmb_put_len(buf, 3);
    char *dst = buf + VMB_LEN;

    /* get the starting offset from the group register */
    int start_byte_ofs = reg->start_ofs;

    /* 
     *   The first element is the character index of the group text in the
     *   source string.  Calculate the character index by adding 1 to the
     *   character length of the text preceding the group; calculate the
     *   character length from the byte length of that string.  Note that the
     *   starting in the group register is stored from the starting point of
     *   the search, not the start of the string, so we need to add in the
     *   starting point in the search.  
     */
    utf8_ptr p((char *)last_str + VMB_LEN);
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
    vm_obj_id_t strobj = CVmObjString::create(
        vmg_ FALSE, p.getptr(), reg->end_ofs - reg->start_ofs);
    put_list_obj(&dst, strobj);

    /* save the string on the stack momentarily to protect against GC */
    G_stk->push()->set_obj(strobj);

    /* create and return the list value */
    retval_obj(vmg_ CVmObjList::create(vmg_ FALSE, buf));

    /* we no longer need the garbage collector protection */
    G_stk->discard();
}

/*
 *   re_replace pattern/replacement structure.  This is used to handle arrays
 *   of arguments: each element represents one pattern->replacement mapping.
 */
struct re_replace_arg
{
    re_replace_arg()
    {
        s = 0;
        pat = 0;
        our_pat = FALSE;
        rpl_func.set_nil();
        rpl_argc = 0;
        rpl_str = 0;
        match_valid = FALSE;
    }

    ~re_replace_arg()
    {
        /* if we created the pattern object, delete it */
        if (pat != 0 && our_pat)
            CRegexParser::free_pattern(pat);
        if (s != 0)
            delete s;
    }

    void set(VMG_ const vm_val_t *patv, const vm_val_t *rplv)
    {
        const char *str;

        /* create our searcher if we haven't yet */
        if (s == 0)
            s = new CRegexSearcherSimple(G_bif_tads_globals->rex_parser);

        /* retrieve the compiled RexPattern or uncompiled pattern string */
        if (patv->typ == VM_OBJ
            && CVmObjPattern::is_pattern_obj(vmg_ patv->val.obj))
        {
            /* it's a pattern object - get its compiled pattern structure */
            pat = ((CVmObjPattern *)vm_objp(vmg_ patv->val.obj))
                  ->get_pattern(vmg0_);
        }
        else if ((str = patv->get_as_string(vmg0_)) != 0)
        {
            /* compile the string */
            re_status_t stat;
            stat = G_bif_tads_globals->rex_parser->compile_pattern(
                str + VMB_LEN, vmb_get_len(str), &pat);

            /* if that failed, we don't have a pattern */
            if (stat != RE_STATUS_SUCCESS)
                pat = 0;

            /* note that we allocated the pattern, so we have to delete it */
            our_pat = TRUE;
        }
        else
        {
            /* invalid type */
            err_throw(VMERR_BAD_TYPE_BIF);
        }

        /* 
         *   Save the replacement value.  This can be a string, nil (which we
         *   treat like an empty string), or a callback function. 
         */
        if (rplv->typ == VM_NIL)
        {
            /* treat it as an empty string */
            rpl_str = "\000\000";
        }
        else if ((str = rplv->get_as_string(vmg0_)) != 0)
        {
            /* save the string value */
            rpl_str = str;
        }
        else if (rplv->is_func_ptr(vmg0_))
        {
            /* it's a function or invokable object */
            rpl_func = *rplv;

            /* get the number of arguments it expects */
            CVmFuncPtr f(vmg_ rplv);
            rpl_argc = f.is_varargs() ? -1 : f.get_max_argc();
        }
        else
        {
            /* invalid type */
            err_throw(VMERR_BAD_TYPE_BIF);
        }
    }

    void search(VMG_ const char *str, int start_idx, const char *last_str)
    {
        /* if we have a pattern, search for it */
        if (pat != 0)
        {
            /* do the search */
            match_idx = s->search_for_pattern(pat, str + VMB_LEN, last_str,
                                              vmb_get_len(str) - start_idx,
                                              &match_len);

            /* if we found it, adjust to the absolute offset in the string */
            if (match_idx >= 0)
                match_idx += start_idx;
        }
        else
        {
            /* no pattern -> no match */
            match_idx = -1;
        }

        /* whether or not we matched, our result is now valid */
        match_valid = TRUE;
    }

    /* our search pattern */
    re_compiled_pattern *pat;

    /* Did we create the pattern?  If so, delete it on destruction. */
    int our_pat;

    /* our replacement string, or null if it's a callback function */
    const char *rpl_str;

    /* our replacement function, in lieu of a string */
    vm_val_t rpl_func;

    /* the number of arguments rpl_func expects, or -1 for varargs */
    int rpl_argc;

    /* the byte index and length in the source string of our last match */
    int match_idx;
    int match_len;

    /* 
     *   Is this last match data valid?  This is false if we haven't done the
     *   first search yet, or if the last replacement overlapped our matching
     *   text at all.  
     */
    int match_valid;

    /* searcher - this holds the group registers for the last match */
    CRegexSearcherSimple *s;
};

/*
 *   re_replace flags 
 */

/* replace all matches (if omitted, replaces only the first match) */
#define VMBIFTADS_REPLACE_ALL     0x0001

/* ignore case in matching the search string */
#define VMBIFTADS_REPLACE_NOCASE  0x0002

/* 
 *   follow case: lower-case characters in the the replacement text are
 *   converted to follow the case pattern of the matched text (all lower,
 *   initial capital, or all capitals) 
 */
#define VMBIFTADS_REPLACE_FOLLOW_CASE  0x0004

/* 
 *   serial replacement: when a list of search patterns is used, we replace
 *   each occurrence of the first pattern over the whole string, then we
 *   start over with the result and scan for occurrences of the second
 *   pattern, replacing each of these, and so on.  If this flag isn't
 *   specified, the default is parallel scanning: we find the leftmost
 *   occurrence of any of the patterns and replace it; then we scan the
 *   remainder of the string for the leftmost occurrence of any pattern, and
 *   replace that one; and so on.  The serial case is equivalent to making a
 *   series of calls to this function with the individual search patterns.  
 */
#define VMBIFTADS_REPLACE_SERIAL  0x0008

/*
 *   Replace only the first occurrence. 
 */
#define VMBIFTADS_REPLACE_ONCE    0x0010


/*
 *   re_replace - search for a pattern in a string, and apply a
 *   replacement pattern
 */
void CVmBifTADS::re_replace(VMG_ uint argc)
{
    vm_val_t patval, rplval;
    int fargc;
    const char *str;
    const char *rpl;
    ulong flags;
    vm_val_t search_val;
    int match_idx;
    int match_len;
    utf8_ptr p;
    size_t rem;
    int groupno;
    const re_group_register *reg;
    vm_obj_id_t ret_obj;
    utf8_ptr dstp;
    int start_idx;
    int pat_cnt, rpl_cnt;
    int pat_is_list, rpl_is_list;
    re_replace_arg *pats = 0;
    int group_cnt;
    int start_char_idx;
    int skip_bytes;
    int i;
    int match_has_upper = FALSE, match_has_lower = FALSE;
    vm_rcdesc rc;
    vm_val_t *argp = G_stk->get(0);
        
    /* check arguments */
    check_argc_range(vmg_ argc, 3, 5);

    /* remember the pattern and replacement string values */
    patval = *G_stk->get(0);
    rplval = *G_stk->get(2);

    /* check whether the pattern is given as an array or as a single value */
    if (patval.is_listlike(vmg0_)
        && (pat_cnt = patval.ll_length(vmg0_)) >= 0)
    {
        /* It's a list.  Check first to see if it's empty. */
        if (pat_cnt == 0)
        {
            /* 
             *   empty list, so there's no work to do - simply return the
             *   original subject string unchanged 
             */
            retval(vmg_ G_stk->get(1));
            G_stk->discard(argc);
            return;
        }

        /* flag it as a list */
        pat_is_list = TRUE;
    }
    else
    {
        /* it's a single value */
        pat_cnt = 1;
        pat_is_list = FALSE;
    }

    /* check to see if the replacement is a list */
    if (rplval.is_listlike(vmg0_)
        && (rpl_cnt = rplval.ll_length(vmg0_)) >= 0)
    {
        /* flag it as a list */
        rpl_is_list = TRUE;
    }
    else
    {
        /* we have a single replacement value */
        rpl_cnt = 1;
        rpl_is_list = FALSE;
    }

    /* allocate the argument array */
    pats = new re_replace_arg[pat_cnt];

    /* catch any errors so that we can free our arg array on the way out */
    err_try
    {
        /* set up the pattern/replacement array from the arguments */
        int need_rc = FALSE;
        for (i = 0 ; i < pat_cnt ; ++i)
        {
            /* get the next pattern from the list, or the single pattern */
            vm_val_t patele;
            if (pat_is_list)
            {
                /* we have a list - get the next element */
                patval.ll_index(vmg_ &patele, i + 1);
            }
            else
            {
                /* we have a single pattern item */
                patele = patval;
            }

            /* 
             *   Get the next replacement from the list, or the single
             *   replacement.  If we have a single value for the replacement,
             *   every pattern has the same replacement.  If we have a list,
             *   the pattern has the replacement at the corresponding index.
             *   If it's a list and we're past the last replacement index,
             *   use an empty string.  
             */
            vm_val_t rplele;
            if (rpl_is_list)
            {
                /* 
                 *   we have a list - if we have an item at the current
                 *   index, it's the corresponding replacement for the
                 *   current pattern; if we've exhausted the replacement
                 *   list, use an empty string as the replacement 
                 */
                if (i < rpl_cnt)
                    rplval.ll_index(vmg_ &rplele, i + 1);
                else
                    rplele.set_nil();
            }
            else
            {
                /* single replacement item - it applies to all patterns */
                rplele = rplval;
            }

            /* fill in this argument item */
            pats[i].set(vmg_ &patele, &rplele);

            /* if this involves a callback, we'll need a recursive context */
            need_rc |= (pats[i].rpl_str == 0);
        }

        /* set up the recursive caller context if needed */
        if (need_rc)
            rc.init(vmg_ "rexReplace", bif_table, 12, argp, argc);

        /* 
         *   Get the search string.  Note that we want to retain the original
         *   value information for the search string, since we'll end up
         *   returning it unchanged if we don't find the pattern.  
         */
        search_val = *G_stk->get(1);

        /* pop the flags; use ReplaceAll if not present */
        flags = VMBIFTADS_REPLACE_ALL;
        if (argc >= 4)
        {
            /* check the type */
            if (G_stk->get(3)->typ != VM_INT)
                err_throw(VMERR_INT_VAL_REQD);

            /* retrieve the value */
            flags = G_stk->get(3)->val.intval;
        }

        /*
         *   Check for old flags.  Before 3.1, there was only one flag bit
         *   defined: ALL=1.  This means there were only two valid values for
         *   'flags': 0 for ONCE mode, 1 for ALL mode.
         *   
         *   In 3.1, we added a bunch of new flags.  At the same time, we
         *   made the default ALL mode, because this is the more common case.
         *   Unfortunately, this creates a compatibility issue.  A new
         *   program that specifies one of the new flags might leave out the
         *   ONCE or ALL bits, since ALL is the default.  However, we can't
         *   just take the absence of the ONCE bit as meaning ALL, because
         *   that would hose old programs that explicitly specify ONCE as 0
         *   (no bits set).
         *   
         *   Here's how we deal with this: we prohibit new programs from
         *   passing 0 for the flags, requiring them to specify at least one
         *   bit.  So if the flags value is zero, we must have an old program
         *   that passed ONCE.  In this case, explicitly set the ONCE bit.
         *   If we have any non-zero value, we must have either a new program
         *   OR an old program that included the ALL bit.  In either case,
         *   ALL is the default, so if the ONCE bit ISN'T set, explicitly set
         *   the ALL bit.  
         */
        if (flags == 0)
        {
            /* old program with the old ONCE flag - set the new ONCE flag */
            flags = VMBIFTADS_REPLACE_ONCE;
        }
        else if (!(flags & VMBIFTADS_REPLACE_ONCE))
        {
            /* 
             *   new program without the ONCE flag, OR an old program with
             *   the ALL flag - explicitly set the ALL flag 
             */
            flags |= VMBIFTADS_REPLACE_ALL;
        }
        
        /* turn off case sensitivity if specified in the flags */
        if (flags & VMBIFTADS_REPLACE_NOCASE)
        {
            for (i = 0 ; i < pat_cnt ; ++i)
                pats[i].s->set_default_case_sensitive(FALSE);
        }

        /* pop the starting index, if present; use index 1 if not present */
        start_char_idx = 1;
        if (argc >= 5)
        {
            /* check the type */
            if (G_stk->get(4)->typ != VM_INT)
                err_throw(VMERR_INT_VAL_REQD);

            /* get the value */
            start_char_idx = G_stk->get(4)->val.intval;
        }
        
        /* push a nil placeholder for the result value */
        G_stk->push()->set_nil();
        
        /* if this is a serial search, we'll start at the first pattern */
        int serial_idx = 0;

        /* make sure the search string is indeed a string */
        str = search_val.get_as_string(vmg0_);
        if (str == 0)
            err_throw(VMERR_STRING_VAL_REQD);

        /* set up a utf8 pointer to the search string */
        utf8_ptr strp((char *)str + VMB_LEN);

        /* adjust the starting index */
        start_char_idx += (start_char_idx < 0
                           ? (int)strp.len(vmb_get_len(str)) : -1);

    restart_search:
        /* get the string again in case we're doing a serial iteration */
        str = search_val.get_as_string(vmg0_);

        /* 
         *   remember the last search string globally, for group extraction;
         *   and forget any old group registers, since they'd point into the
         *   old string we're superseding 
         */
        G_bif_tads_globals->last_rex_str->val = search_val;
        G_bif_tads_globals->rex_searcher->clear_group_regs();

        /* 
         *   don't allocate anything for the result yet - we'll wait to do
         *   that until we actually find a match, so that we don't allocate
         *   memory unnecessarily 
         */
        ret_obj = VM_INVALID_OBJ;
        CVmObjString *ret_str = 0;
        dstp.set((char *)0);
        
        /* 
         *   figure out how many bytes at the start of the string to skip
         *   before our first replacement 
         */
        for (p.set((char *)str + VMB_LEN), rem = vmb_get_len(str), i = 0 ;
             i < start_char_idx && rem != 0 ; ++i, p.inc(&rem)) ;

        /* the current offset in the string is the byte skip offset */
        skip_bytes = p.getptr() - (str + VMB_LEN);
        
        /* note that we haven't done any replacements yet */
        int did_rpl = FALSE;

        /*
         *   Start searching from the beginning of the string.  Build the
         *   result string as we go.  
         */
        for (start_idx = skip_bytes ; (size_t)start_idx < vmb_get_len(str) ; )
        {
            const char *last_str;
            int best_pat = -1;
            
            /* figure out where the next search starts */
            last_str = str + VMB_LEN + start_idx;

            /* do the next serial or parallel search */
            if (flags & VMBIFTADS_REPLACE_SERIAL)
            {
                /* 
                 *   Serial search: search for one item at a time.  If we're
                 *   out of items, we're done.  
                 */
                if (serial_idx >= pat_cnt)
                    break;

                /* search for the next item */
                pats[serial_idx].search(vmg_ str, start_idx, last_str);

                /* if we didn't get a match, we're done */
                if (pats[serial_idx].match_idx < 0)
                    break;

                /* this is our replacement match */
                best_pat = serial_idx;
            }
            else
            {
                /* 
                 *   Parallel search: search for all of the items, and
                 *   replace the leftmost match.  We might still have valid
                 *   search results for some items on past iterations, but
                 *   others might have overlapped replacement text, in which
                 *   case we'll have to refresh them.  So do a search for
                 *   each item that's marked as invalid.  
                 */

                /* search for each item */
                for (i = 0 ; i < pat_cnt ; ++i)
                {
                    /* refresh this search, if it's invalid */
                    if (!pats[i].match_valid)
                        pats[i].search(vmg_ str, start_idx, last_str);

                    /* if this is the leftmost result so far, remember it */
                    if (pats[i].match_idx >= 0
                        && (best_pat < 0
                            || pats[i].match_idx < pats[best_pat].match_idx))
                        best_pat = i;
                }

                /* if we didn't find a match, we're done */
                if (best_pat < 0)
                    break;
            }

            /* 
             *   Keep the leftmost pattern we matched.  Note that we want a
             *   relative offset from last_str, so adjust from the absolute
             *   offset in the string that the pats[] entry uses. 
             */
            match_idx = pats[best_pat].match_idx - start_idx;
            match_len = pats[best_pat].match_len;
            rpl = pats[best_pat].rpl_str;
            rplval = pats[best_pat].rpl_func;
            fargc = pats[best_pat].rpl_argc;

            /* 
             *   if we have the 'follow case' flag, note the capitalization
             *   pattern of the match 
             */
            if ((flags & VMBIFTADS_REPLACE_FOLLOW_CASE) != 0)
            {
                /* no upper or lower case letters yet */
                match_has_upper = match_has_lower = FALSE;

                /* scan the match text */
                for (p.set((char *)last_str + match_idx), rem = match_len ;
                     rem != 0 ; p.inc(&rem))
                {
                    /* get this character */
                    wchar_t ch = p.getch();

                    /* note whether it's upper or lower case */
                    match_has_upper |= t3_is_upper(ch);
                    match_has_lower |= t3_is_lower(ch);
                }
            }

            /* note the group count */
            group_cnt = pats[best_pat].pat->group_cnt;

            /* copy the group registers to the global searcher registers */
            G_bif_tads_globals->rex_searcher->copy_group_regs(
                pats[best_pat].s);

            /* note that we're doing a replacement */
            did_rpl = TRUE;
            
            /*
             *   If we haven't allocated a result string yet, do so now,
             *   since we finally know we actually need one.  
             */
            if (ret_obj == VM_INVALID_OBJ)
            {
                /*   
                 *   We don't know yet how much space we'll need for the
                 *   result, so this is only a temporary allocation.  As a
                 *   rough guess, use three times the length of the input
                 *   string.  We'll expand this as needed as we build the
                 *   string, and shrink it down to the real size when we're
                 *   done.  
                 */
                ret_obj = CVmObjString::create(vmg_ FALSE, vmb_get_len(str)*3);

                /* save it in our stack slot, for gc protection */
                G_stk->get(0)->set_obj(ret_obj);

                /* get the string object pointer */
                ret_str = (CVmObjString *)vm_objp(vmg_ ret_obj);

                /* get a pointer to the result buffer */
                dstp.set(ret_str->cons_get_buf());

                /* copy the initial part that we skipped */
                if (skip_bytes != 0)
                {
                    memcpy(dstp.getptr(), str + VMB_LEN, skip_bytes);
                    dstp.set(dstp.getptr() + skip_bytes);
                }
            }

            /* copy the part up to the start of the matched text, if any */
            if (match_idx > 0)
            {
                /* ensure space */
                dstp.set(ret_str->cons_ensure_space(
                    vmg_ dstp.getptr(), match_idx, 512));
                
                /* copy the part from the last match to this match */
                memcpy(dstp.getptr(), last_str, match_idx);
                
                /* advance the output pointer */
                dstp.set(dstp.getptr() + match_idx);
            }

            /* apply the replacement (callback or string) */
            if (rpl != 0)
            {
                /* we haven't substituted any alphabetic character yet */
                int alpha_rpl_cnt = 0;
                
                /* 
                 *   copy the replacement string into the output string,
                 *   expanding group substitutions 
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
                                    if (reg->start_ofs != -1
                                        && reg->end_ofs != -1)
                                    {
                                        size_t glen;
                                        
                                        /* get the group length */
                                        glen = reg->end_ofs - reg->start_ofs;
                                        
                                        /* ensure space */
                                        dstp.set(ret_str->cons_ensure_space(
                                            vmg_ dstp.getptr(), glen, 512));
                                        
                                        /* copy the data */
                                        memcpy(dstp.getptr(),
                                               str + VMB_LEN + reg->start_ofs,
                                               glen);
                                        
                                        /* advance past it */
                                        dstp.set(dstp.getptr() + glen);
                                    }
                                }
                                break;
                                
                            case '*':
                                /* ensure space */
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), match_len, 512));
                                
                                /* add the entire matched string */
                                memcpy(dstp.getptr(), last_str + match_idx,
                                       match_len);
                                dstp.set(dstp.getptr() + match_len);
                                break;
                                
                            case '%':
                                /* ensure space (the '%' is just one byte) */
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), 1, 512));
                                
                                /* add a single '%' */
                                dstp.setch('%');
                                break;
                                
                            default:
                                /* 
                                 *   ensure space (we need 1 byte for the
                                 *   '%', up to 3 for the other character) 
                                 */
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), 4, 512));
                                
                                /* add the entire sequence unchanged */
                                dstp.setch('%');
                                dstp.setch(p.getch());
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* it's an ordinary literal charater - fetch it */
                        wchar_t ch = p.getch();

                        /* ensure we have space for it (UTF8 -> 3 bytes max) */
                        dstp.set(ret_str->cons_ensure_space(
                            vmg_ dstp.getptr(), 3, 512));

                        /* 
                         *   if we're in 'follow case' mode, adjust the case
                         *   of lower-case literal letters in the replacement
                         *   text 
                         */
                        if ((flags & VMBIFTADS_REPLACE_FOLLOW_CASE) != 0
                            && t3_is_lower(ch))
                        {
                            /* 
                             *   Check the mode: if we have all upper-case in
                             *   the match, convert the replacement to all
                             *   caps; all lower-case in the match -> all
                             *   lower in the replacement; mixed ->
                             *   capitalize the first letter only 
                             */
                            if (match_has_upper && !match_has_lower)
                            {
                                /* all upper-case - convert to upper */
                                ch = t3_to_upper(ch);
                            }
                            else if (match_has_lower && !match_has_upper)
                            {
                                /* all lower-case - leave it as-is */
                            }
                            else
                            {
                                /* mixed case - capitalize the first leter */
                                if (alpha_rpl_cnt++ == 0)
                                    ch = t3_to_upper(ch);
                            }
                        }
                        
                        /* copy this character literally */
                        dstp.setch(ch);
                    }
                }
            }
            else
            {
                /* push the callback args - matchStr, matchIdx, origStr */
                const int pushed_argc = 3;
                G_stk->push(&search_val);
                G_interpreter->push_int(
                    vmg_ strp.len(start_idx + match_idx) + 1);
                G_interpreter->push_obj(vmg_ CVmObjString::create(
                    vmg_ FALSE, last_str + match_idx, match_len));

                    /* adjust argc for what the callback actually wants */
                int fargc = pats[best_pat].rpl_argc;
                if (fargc < 0 || fargc > pushed_argc)
                    fargc = pushed_argc;

                /* call the callback */
                G_interpreter->call_func_ptr(vmg_ &rplval, fargc, &rc, 0);

                /* discard extra arguments */
                G_stk->discard(pushed_argc - fargc);
                
                /* if the return value isn't nil, copy it into the result */
                if (G_interpreter->get_r0()->typ != VM_NIL)
                {
                    /* get the string */
                    const char *r =
                        G_interpreter->get_r0()->get_as_string(vmg0_);
                    if (r == 0)
                        err_throw(VMERR_STRING_VAL_REQD);
                    
                    /* ensure space for it in the result */
                    dstp.set(ret_str->cons_ensure_space(
                        vmg_ dstp.getptr(), vmb_get_len(r), 512));
                    
                    /* store it */
                    memcpy(dstp.getptr(), r + VMB_LEN, vmb_get_len(r));
                    dstp.set(dstp.getptr() + vmb_get_len(r));
                }
            }
            
            /* advance past this matched string for the next search */
            start_idx += match_idx + match_len;
            
            /* skip to the next character if it was a zero-length match */
            if (match_len == 0 && (size_t)start_idx < vmb_get_len(str))
            {
                /* ensure space */
                dstp.set(ret_str->cons_ensure_space(
                    vmg_ dstp.getptr(), 3, 512));
                
                /* copy the character we're skipping to the output */
                p.set((char *)str + VMB_LEN + start_idx);
                dstp.setch(p.getch());
                
                /* move on to the next character */
                start_idx += 1;
            }

            /* 
             *   In a parallel search, discard any match that started before
             *   the new starting point.  Those are no longer valid because
             *   they matched original text that was wholly or partially
             *   replaced by the current iteration.  
             */
            for (i = 0 ; i < pat_cnt ; ++i)
            {
                /* invalidate this match if it's before the replacement */
                if (pats[i].match_idx >= 0 && pats[i].match_idx < start_idx)
                    pats[i].match_valid = FALSE;
            }
            
            /* if we're only performing a single replacement, stop now */
            if (!(flags & VMBIFTADS_REPLACE_ALL))
                break;
        }

        /* if we did any replacements on this round, finish the string */
        if (ret_obj != VM_INVALID_OBJ)
        {
            /* ensure space for the remainder after the last match */
            dstp.set(ret_str->cons_ensure_space(
                vmg_ dstp.getptr(), vmb_get_len(str) - start_idx, 512));
            
            /* add the part after the end of the matched text */
            if ((size_t)start_idx < vmb_get_len(str))
            {
                memcpy(dstp.getptr(), str + VMB_LEN + start_idx,
                       vmb_get_len(str) - start_idx);
                dstp.set(dstp.getptr() + vmb_get_len(str) - start_idx);
            }
            
            /* set the actual length of the string */
            ret_str->cons_shrink_buffer(vmg_ dstp.getptr());

            /* return the string */
            retval_obj(vmg_ ret_obj);
        }
        else
        {
            /* we didn't replace anything, so keep the original string */
            retval(vmg_ G_stk->get(2));
        }

        /* 
         *   If this is a serial search, and we have another item in the
         *   search list, go back and start over with the current result as
         *   the new search string.  Exception: if we're in REPLACE ONCE
         *   mode, and we've already done a replacement, we're finished.  
         */
        if ((flags & VMBIFTADS_REPLACE_SERIAL) != 0
            && ((flags & VMBIFTADS_REPLACE_ALL) != 0 || !did_rpl)
            && ++serial_idx < pat_cnt)
        {
            /* use the return value as the new search value */
            *G_stk->get(2) = search_val = *G_interpreter->get_r0();
            
            /* go back for a brand new round */
            goto restart_search;
        }

        /* discard the arguments and gc protection items */
        G_stk->discard(argc + 1);
    }
    err_finally
    {
        /* if we allocated an argument array, delete it */
        if (pats != 0)
            delete [] pats;
    }
    err_end;
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
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* get the filename argument */
    const vm_val_t *filespec = G_stk->get(0);

    /* 
     *   if there's a metadata table argument, fetch it (but leave the value
     *   on the stack for gc protection) 
     */
    CVmObjLookupTable *metatab = 0;
    if (argc >= 2)
    {
        /* it must be a LookupTable object, or nil */
        vm_val_t *v = G_stk->get(1);
        if (v->typ == VM_NIL)
        {
            /* nil, so there's no table - just discard the argument */
            G_stk->discard();
        }
        else
        {
            /* it's not nil, so it has to be a LookupTable object */
            if (!CVmObjLookupTable::is_lookup_table_obj(vmg_ v->val.obj))
                err_throw(VMERR_BAD_TYPE_BIF);

            /* get the lookup table object */
            metatab = (CVmObjLookupTable *)vm_objp(vmg_ v->val.obj);
        }
    }

    /* set up for network storage server access, if applicable */
    vm_rcdesc rc(vmg_ "saveGame", bif_table, 15, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmNetFile::open(
        vmg_ filespec, &rc, NETF_WRITE | NETF_CREATE | NETF_TRUNC,
        OSFTT3SAV, "application/x-t3vm-state");

    /* open the file and save the game */
    CVmFile *file = 0;
    err_try
    {
        /* open the file */
        osfildef *fp = osfoprwtb(netfile->lclfname, OSFTT3SAV);
        if (fp == 0)
            err_throw(VMERR_CREATE_FILE);

        /* set up the file writer */
        file = new CVmFile();
        file->set_file(fp, 0);

        /* save the state */
        CVmSaveFile::save(vmg_ file, metatab);

        /* close the file */
        delete file;
        file = 0;
    }
    err_catch(exc)
    {
        /* close the file if it's still open */
        if (file != 0)
            delete file;
        
        /* abandon the network file */
        if (netfile != 0)
            netfile->abandon(vmg0_);

        /* rethrow the error */
        err_rethrow();
    }
    err_end;

    /* close out the network file */
    netfile->close(vmg0_);

    /* discard arguments */
    G_stk->discard(argc);

    /* no return value */
    retval_nil(vmg0_);
}

/*
 *   restore
 */
void CVmBifTADS::restore(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* get the filename or spec */
    const vm_val_t *filespec = G_stk->get(0);

    /* set up for network storage server access, if applicable */
    vm_rcdesc rc(vmg_ "restoreGame", bif_table, 16, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmNetFile::open(
        vmg_ filespec, &rc, NETF_READ, OSFTT3SAV, "application/x-t3vm-state");

    /* open the file and restore the game */
    CVmFile *file = 0;
    int err = 0;
    err_try
    {
        /* open the file */
        osfildef *fp = osfoprb(netfile->lclfname, OSFTT3SAV);
        if (fp == 0)
            err_throw(VMERR_FILE_NOT_FOUND);

        /* set up the file reader */
        file = new CVmFile(fp, 0);

        /* restore the state */
        err = CVmSaveFile::restore(vmg_ file);

        /* close our local file */
        delete file;
        file = 0;
    }
    err_catch(exc)
    {
        /* close the file if it's still open */
        if (file != 0)
            delete file;

        /* abandon the network file if we didn't close it out */
        if (netfile != 0)
            netfile->abandon(vmg0_);

        /* rethrow the error */
        err_rethrow();
    }
    err_end;

    /* if an error occurred, throw an exception */
    if (err != 0)
        err_throw(err);

    /* close out the network file */
    netfile->close(vmg0_);

    /* discard arguments */
    G_stk->discard(argc);

    /* no return value */
    retval_nil(vmg0_);
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

    /* no return value */
    retval_nil(vmg0_);
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

    /* if there's one list-like argument, get the max of the list elements */
    if (argc == 1 && cur_max.is_listlike(vmg0_))
    {
        /* get the list length */
        vm_val_t lst = cur_max;
        uint cnt = lst.ll_length(vmg0_);

        /* if it's a zero-element list, it's an error */
        if (cnt == 0)
            err_throw(VMERR_BAD_VAL_BIF);

        /* get the first element as the tentative maximum */
        lst.ll_index(vmg_ &cur_max, 1);

        /* compare each additional list element */
        for (i = 2 ; i <= cnt ; ++i)
        {
            /* compare the current element and keep it if it's the highest */
            vm_val_t ele;
            lst.ll_index(vmg_ &ele, i);
            if (ele.compare_to(vmg_ &cur_max) > 0)
                cur_max = ele;
        }
    }
    else
    {
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

    /* if there's one list-like argument, get the max of the list elements */
    if (argc == 1 && cur_min.is_listlike(vmg0_))
    {
        /* get the list length */
        vm_val_t lst = cur_min;
        uint cnt = lst.ll_length(vmg0_);

        /* if it's a zero-element list, it's an error */
        if (cnt == 0)
            err_throw(VMERR_BAD_VAL_BIF);

        /* get the first element as the tentative maximum */
        lst.ll_index(vmg_ &cur_min, 1);

        /* compare each additional list element */
        for (i = 2 ; i <= cnt ; ++i)
        {
            /* compare the current element and keep it if it's the highest */
            vm_val_t ele;
            lst.ll_index(vmg_ &ele, i);
            if (ele.compare_to(vmg_ &cur_min) < 0)
                cur_min = ele;
        }
    }
    else
    {
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
    const char *strp = 0;
    int lst_len = -1;
    size_t i;
    utf8_ptr dst;
    
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* get the base value */
    G_stk->pop(&val);

    /* if there's a repeat count, get it */
    rpt = (argc >= 2 ? pop_long_val(vmg0_) : 1);

    /* if the repeat count is less than zero, it's an error */
    if (rpt < 0)
        err_throw(VMERR_BAD_VAL_BIF);

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
        lst_len = val.ll_length(vmg0_);

    do_list:
        /* 
         *   Run through the list and get the size of each character, so
         *   we can determine how long the string will have to be. 
         */
        for (new_str_len = 0, i = 1 ; (int)i <= lst_len ; ++i)
        {
            /* get this element */
            vm_val_t ele_val;
            val.ll_index(vmg_ &ele_val, i);

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
        if (val.is_listlike(vmg0_) && (lst_len = val.ll_length(vmg0_)) >= 0)
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
        if (lst_len >= 0)
        {
            /* run through the list */
            for (i = 1 ; i <= (size_t)lst_len ; ++i)
            {
                /* get this element */
                vm_val_t ele_val;
                val.ll_index(vmg_ &ele_val, i);

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
    CVmFuncPtr hdr;
    vm_obj_id_t lst_obj;
    CVmObjList *lst;

    /* check arguments */
    check_argc(vmg_ argc, 1);

    /* set up a method header pointer for the function pointer argument */
    if (!hdr.set(vmg_ G_stk->get(0)))
        err_throw(VMERR_FUNCPTR_VAL_REQD);

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

    /* set the optional argument count */
    val.set_int(hdr.get_opt_argc());
    lst->cons_set_element(1, &val);

    /* set the varargs flag */
    val.set_logical(hdr.is_varargs());
    lst->cons_set_element(2, &val);

    /* return the list */
    retval_obj(vmg_ lst_obj);
}

/* ------------------------------------------------------------------------ */
/*
 *   sprintf
 */

/* bufprintf format string pointer */
struct bpptr
{
    bpptr(const char *p, size_t len)
    {
        this->p = p;
        this->len = len;
    }

    bpptr(bpptr &src) { set(src); }

    /* set from another pointer */
    void set(bpptr &src)
    {
        this->p = src.p;
        this->len = src.len;
    }

    /* do we have more in our buffer? */
    int more() const { return len != 0; }

    /* get the current character */
    char getch() const { return len != 0 ? *p : '\0'; }

    /* get and skip the current character */
    char skipch()
    {
        if (len != 0)
        {
            char ch = *p;
            ++p, --len;
            return ch;
        }
        else
            return '\0';
    }

    /* 
     *   match a character: if we match, return true and skip the character,
     *   otherwise just return false 
     */
    int match_skip(char ch)
    {
        if (getch() == ch)
        {
            inc();
            return TRUE;
        }
        else
            return FALSE;
    }

    /* skip the current character */
    void inc()
    {
        if (len != 0)
            ++p, --len;
    }

    /* get the next wide character */
    wchar_t getwch() const { return len != 0 ? utf8_ptr::s_getch(p) : 0; }

    /* get and skip the next wide character */
    wchar_t skipwch()
    {
        wchar_t ch = getwch();
        incwch();
        return ch;
    }

    /* skip the next wide character */
    void incwch()
    {
        if (len != 0)
        {
            size_t clen = utf8_ptr::s_charsize(*p);
            if (clen > len)
                clen = len;
            p += clen;
            len -= clen;
        }
    }

    /* parse an integer value */
    int atoi()
    {
        int acc = 0;
        while (is_digit(getch()))
        {
            acc *= 10;
            acc += value_of_digit(skipch());
        }
        return acc;
    }

    /* parse an integer value; return -1 if there's no integer here */
    int check_atoi() { return is_digit(getch()) ? atoi() : -1; }

    /* current string pointer */
    const char *p;

    /* remaining length in bytes */
    size_t len;
};

/* format_int() flags */
#define FI_UNSIGNED  0x0001
#define FI_CAPS      0x0002

/* '%' formatting options */
struct fmtopts
{
    fmtopts()
    {
        sign = '\0';
        group = 0;
        prec = -1;
        width = -1;
        left_align = FALSE;
        pad = ' ';
        pound = FALSE;
    }

    /* 
     *   sign character to show for positive numbers: '\0' to show nothing, '
     *   ' to show a space, '+' to show a plus 
     */
    char sign;

    /* group charater, or '\0' if not grouping */
    wchar_t group;

    /* width (%10d); -1 means no width spec */
    int width;

    /* precision (%.10s); -1 means no precision spec */
    int prec;

    /* true -> left align the value in its field; false -> right align */
    int left_align;

    /* padding character; default is space */
    wchar_t pad;

    /* 
     *   '#' flag: e E f g G -> always use a decimal point; g G -> keep
     *   trailing zeros; x X o -> use 0x/0X/0 prefix for non-zero values 
     */
    int pound;
};

/* bufprintf output writer */
struct bpwriter
{
    bpwriter(VMG_ CVmObjString *str)
    {
        this->vmg = VMGLOB_ADDR;
        this->str = str;
        this->dst = str->cons_get_buf();
    }

    /* close - set the final string length */
    void close()
    {
        VMGLOB_PTR(vmg);
        str->cons_shrink_buffer(vmg_ dst);
    }

    /* write a character */
    void putch(char ch)
    {
        VMGLOB_PTR(vmg);
        dst = str->cons_append(vmg_ dst, &ch, 1, 64);
    }

    /* write a UTF8 character */
    void putwch(wchar_t ch)
    {
        VMGLOB_PTR(vmg);
        dst = str->cons_append(vmg_ dst, ch, 64);
    }

    /* write a UTF8 character multiple times */
    void putwch(wchar_t ch, int cnt)
    {
        while (cnt-- != 0)
            putwch(ch);
    }

    /* write a string */
    void puts(const char *str) { puts(str, strlen(str)); }
    void puts(const char *str, size_t len)
    {
        VMGLOB_PTR(vmg);
        dst = this->str->cons_append(vmg_ dst, str, len, 64);
    }

    /* format a string value */
    void format_string(VMG_ const vm_val_t *val, const fmtopts &opts)
    {
        /* if we don't have a string value, cast it to string */
        vm_val_t strval;
        val->cast_to_string(vmg_ &strval);
        G_stk->push(&strval);

        /* get the string buffer and length */
        const char *str = strval.get_as_string(vmg0_);
        size_t bytes = vmb_get_len(str);
        str += VMB_LEN;

        /* get the length of the string in characters */
        utf8_ptr p((char *)str);
        size_t chars = p.len(bytes);

        /* If the string is longer than the precision, truncate it */
        if (opts.prec >= 0 && (int)chars > opts.prec)
        {
            /* limit the length (in both chars and bytes) to the precision */
            chars = opts.prec;
            bytes = p.bytelen(chars);
        }

        /* write it out with the appropriate padding and alignment */
        format_with_padding(vmg_ str, bytes, chars, opts);

        /* discard our gc protection */
        G_stk->discard();
    }

    /* format a character value */
    void format_char(VMG_ const vm_val_t *val)
    {
        /* check what we have */
        const char *str = val->get_as_string(vmg0_);
        if (str != 0)
        {
            /* it's a string - show the first character */
            size_t len = vmb_get_len(str);
            str += VMB_LEN;
            if (len != 0)
            {
                /* write the first character */
                putwch(utf8_ptr::s_getch(str));
            }
            else
            {
                /* empty string - write a null character */
                putch((char)0);
            }
        }
        else
        {
            /* it's not a string - get the integer value */
            int i = val->cast_to_int(vmg0_);

            /* make sure it's in range */
            if (i < 0 || i > 65535)
                err_throw(VMERR_NUM_OVERFLOW);

            /* write the character value */
            putwch((wchar_t)i);
        }
    }

    /* format an internal UTF-8 length+string, with padding and alignment */
    void format_with_padding(VMG_ const char *str, const fmtopts &opts)
    {
        /* figure the byte length and get the buffer */
        size_t bytes = vmb_get_len(str);
        str += VMB_LEN;

        /* figure the character length */
        utf8_ptr p((char *)str);
        size_t chars = p.len(bytes);

        /* write it out */
        format_with_padding(vmg_ str, bytes, chars, opts);
    }

    /* format a UTF-8 string value, adding padding and alignment */
    void format_with_padding(VMG_ const char *str,
                             size_t bytes, size_t chars,
                             const fmtopts &opts)
    {
        /* 
         *   Figure the amount of padding: if the width is larger than the
         *   string length (in characters), pad by the difference.  
         */
        int npad = (opts.width > (int)chars ? opts.width - chars : 0);

        /* if right-aligning, write the padding */
        if (!opts.left_align)
            putwch(opts.pad, npad);

        /* write the string */
        puts(str, bytes);

        /* if left-aligning, write the padding */
        if (opts.left_align)
            putwch(opts.pad, npad);
    }

    /* format an integer value */
    void format_int(VMG_ const vm_val_t *val, int radix,
                    const char *type_prefix,
                    const fmtopts &opts, int flags = 0)
    {
        /* a stack buffer for conversions that can use it */
        char buf[256];

        /* check the type */
        switch (val->typ)
        {
        case VM_NIL:
            /* format nil as zero */
            format_int(vmg_ "0", 1, type_prefix, opts, flags);
            break;

        case VM_TRUE:
            /* format true as one */
            format_int(vmg_ "1", 1, type_prefix, opts, flags);
            break;

        case VM_INT:
            {
                /* set flags - round to integer, unsigned if applicable */
                int cvtflags = TOSTR_ROUND;
                if ((flags & FI_UNSIGNED) != 0)
                    cvtflags |= TOSTR_UNSIGNED;

                /* get the basic string representation of the integer */
                const char *p = CVmObjString::cvt_int_to_str(
                    buf, sizeof(buf), val->val.intval, radix, cvtflags);

                /* apply our extra formatting to the basic string rep */
                format_int(vmg_ p + VMB_LEN, vmb_get_len(p),
                           type_prefix, opts, flags);
            }
            break;

        default:
            {
                /* 
                 *   figure the string conversion flags: round to integer,
                 *   and use an unsigned interpretation if the format is
                 *   unsigned 
                 */
                int tsflags = TOSTR_ROUND;
                if (flags & FI_UNSIGNED)
                    tsflags |= TOSTR_UNSIGNED;

                /* cast the value to a numeric type */
                vm_val_t num;
                val->cast_to_num(vmg_ &num);
                G_stk->push(&num);

                /* ...thence to string, to get a printable representation */
                vm_val_t str;
                const char *p = CVmObjString::cvt_to_str(
                    vmg_ &str, buf, sizeof(buf), &num, radix, tsflags);
                G_stk->push(&str);

                /* apply our extra formatting to the basic string rep */
                format_int(vmg_ p + VMB_LEN, vmb_get_len(p),
                           type_prefix, opts, flags);

                /* discard the GC protection */
                G_stk->discard(2);
            }
            break;
        }
    }

    /* 
     *   Format an integer value for which we've generated a basic string
     *   buffer value.  This applies our extra formatting - padding, plus
     *   sign, alignment, case conversion.  
     */
    void format_int(VMG_ const char *p, size_t len,
                    const char *type_prefix,
                    const fmtopts &opts, int flags)
    {
        /* if they're trying to pawn off an empty string on us, use "0" */
        if (p == 0 || len == 0)
            p = "0", len = 1;

        /* note if the value is all zeros */
        int zero = TRUE;
        const char *p2 = p;
        for (size_t i = 0 ; i < len ; ++i)
        {
            if (*p2 != '0')
            {
                zero = FALSE;
                break;
            }
        }

        /* 
         *   get the number of digits: assume that the whole thing is digits
         *   except for a leading minus sign 
         */
        int digits = len;
        if (*p == '-')
            --digits;

        /* 
         *   Figure the display width required.  Start with the length of the
         *   string.  If we the "sign" option is set and we don't have a "-"
         *   sign, add space for a "+" sign.  If the "group" option is set,
         *   add a comma for each group of three digits.  If the '#' flag is
         *   set, add the type prefix if the value is nonzero.  
         */
        int dispwid = len;
        if (opts.sign != '\0' && *p != '-')
            dispwid += 1;
        if (opts.group != 0)
            dispwid += ((len - (*p == '-' ? 1 : 0)) - 1)/3;
        if (opts.pound && type_prefix != 0 && !zero)
            dispwid += strlen(type_prefix);

        /*
         *   If there's a precision setting, it means that we're to add
         *   leading zeros to bring the number of digits up to the
         *   precision. 
         */
        if (digits < opts.prec)
            dispwid += opts.prec - digits;

        /* 
         *   Figure the amount of padding.  If there's an explicit width spec
         *   in the options, and the display width is less than the width
         *   spec, pad by the differene.  
         */
        int padcnt = (opts.width > dispwid ? opts.width - dispwid : 0);

        /* if they want right alignment, add padding characters before */
        if (!opts.left_align)
            putwch(opts.pad, padcnt);

        /* add the + sign if needed */
        if (opts.sign && *p != '-')
            putch(opts.sign);

        /* if there's a '-' sign, write it */
        if (*p == '-')
            putch(*p++), --len;

        /* add the type prefix */
        if (opts.pound && type_prefix != 0 && !zero)
            puts(type_prefix);

        /* add the leading zeros for the precision */
        for (int i = digits ; i < opts.prec ; ++i)
            putch('0');

        /* write the digits, adding grouping commas and converting case */
        for (int dig = 0 ; len != 0 ; ++p, --len, ++dig)
        {
            /* 
             *   if this isn't the first digit, and we have a multiple of
             *   three digits remaining, and we're using grouping, add the
             *   group comma 
             */
            if (opts.group != 0 && dig != 0 && len % 3 == 0)
                putwch(opts.group);

            /* write this character, converting case as needed */
            if (is_digit(*p))
                putch(*p);
            else if ((flags & FI_CAPS) != 0)
                putch((char)toupper(*p));
            else
                putch((char)tolower(*p));
        }

        /* if they want left alignment, add padding characters after */
        if (opts.left_align)
            putwch(opts.pad, padcnt);
    }

    /* format a floating-point value */
    void format_float(VMG_ const vm_val_t *val, char type_spec,
                      const fmtopts &opts)
    {
        /* a stack buffer for conversions that can use it */
        char buf[256];

        /* 
         *   Use the precision specified, with a default of 6 digits; assume
         *   that there's no maximum number of digits. 
         */
        int prec = (opts.prec >= 0 ? opts.prec : 6);
        int maxdigs = -1;

        /* figure the formatter flags */
        ulong flags = VMBN_FORMAT_LEADING_ZERO;

        /* if the sign option is set, always show a sign */
        if (opts.sign == '+')
            flags |= VMBN_FORMAT_SIGN;
        else if (opts.sign == ' ')
            flags |= VMBN_FORMAT_POS_SPACE;

        /* 
         *   if the '#' flag is set, always show a decimal point, and keep
         *   trailing zeros with 'g' and 'G' 
         */
        if (opts.pound)
        {
            flags |= VMBN_FORMAT_POINT;
            if (type_spec == 'g' || type_spec == 'G')
                flags |= VMBN_FORMAT_TRAILING_ZEROS;
        }

        /* for 'E' or 'G', use capital 'E' for the exponent indicator */
        if (type_spec == 'E' || type_spec == 'G')
            flags |= VMBN_FORMAT_EXP_CAP;

        /* 
         *   for 'e' or 'E', always use scientific notation, with a sign
         *   symbol in the exponent value 
         */
        if (type_spec == 'e' || type_spec == 'E')
            flags |= VMBN_FORMAT_EXP | VMBN_FORMAT_EXP_SIGN;

        /* if the type code is 'g' or 'G', use "compact" notation */
        if (type_spec == 'g' || type_spec == 'G')
        {
            /* set the compact notation flag */
            flags |= VMBN_FORMAT_COMPACT | VMBN_FORMAT_EXP_SIGN;

            /* the precision is the number of significant digits to show */
            flags |= VMBN_FORMAT_MAXSIG;
            maxdigs = prec;
            prec = -1;
        }

        /* cast the value to a numeric type */
        vm_val_t num;
        val->cast_to_num(vmg_ &num);
        G_stk->push(&num);

        /* check the type */
        switch (num.typ)
        {
        case VM_INT:
            {
                /* format the integer as though it were a float */
                const char *p = CVmObjBigNum::cvt_int_to_string_buf(
                    vmg_ buf, sizeof(buf), num.val.intval,
                    maxdigs, -1, prec, 3, flags);
                
                /* write it out, adding padding and alignment */
                format_with_padding(vmg_ p, opts);
            }
            break;

        case VM_OBJ:
            /* if it's an object, it should be a BigNumber */
            if (CVmObjBigNum::is_bignum_obj(vmg_ num.val.obj))
            {
                /* get the BigNumber object */
                CVmObjBigNum *bn = (CVmObjBigNum *)vm_objp(vmg_ num.val.obj);

                /* format the BigNumber */
                vm_val_t str;
                const char *p = bn->cvt_to_string_buf(
                    vmg_ &str, buf, sizeof(buf),
                    maxdigs, -1, prec, 3, flags);
                G_stk->push(&str);

                /* write it out, adding padding and alignment */
                format_with_padding(vmg_ p, opts);

                /* discard gc protection */
                G_stk->discard();
            }
            break;

        default:
            break;
        }

        /* discard our gc protection */
        G_stk->discard();
    }

    /* result string */
    CVmObjString *str;

    /* current write pointer */
    char *dst;

    /* VM globals */
    vm_globals *vmg;
};


/*
 *   Internal sprintf formatter.  The arguments are on the stack in the usual
 *   function call order, with the first argument at top of stack.  We'll
 *   allocate a new String object to hold the result.  
 */
static void tsprintf(VMG_ vm_val_t *retval, const char *fmtp, size_t fmtl,
                     int arg0, int argc)
{
    /* set up a format string reader */
    bpptr fmt(fmtp, fmtl);

    /* 
     *   Create a string to hold the result.  Use 150% the length of the
     *   format string as a guess, with a minimum of 64 characters; we'll
     *   expand this as needed as we go. 
     */
    size_t init_len = (fmtl < 43 ? 64 : fmtl*3/2);
    retval->set_obj(CVmObjString::create(vmg_ FALSE, init_len));

    /* push it for gc protection */
    G_stk->push(retval);

    /* adjust the argument base for our additions */
    arg0 += 1;

    /* set up an output writer */
    bpwriter dst(vmg_ (CVmObjString *)vm_objp(vmg_ retval->val.obj));

    /* set up a nil value for missing arguments */
    vm_val_t nil_val;
    nil_val.set_nil();

    /* 
     *   Scan the format string.  The string is in utf8 format as always, but
     *   our format codes are all plain ASCII characters, so a simple byte
     *   scan is perfectly adequate.  
     */
    for (int argpos = 1 ; fmt.more() ; )
    {
        /* check for format codes */
        char ch = fmt.skipch();
        if (ch == '%')
        {
            /* format specifier - remember where the '%' was */
            const char *pct = fmt.p - 1;

            /* set up our initial default options */
            fmtopts opts;
            int argno = -1;

            /* parse flags until we're out of them */
            for (int flags_done = FALSE ; !flags_done ; )
            {
                /* check the next character */
                switch (fmt.getch())
                {
                case '[':
                    /* argument number specifier - "[digits]" */
                    fmt.inc();
                    argno = fmt.atoi();
                    fmt.match_skip(']');
                    break;
                    
                case '+':
                    /* note the sign specifier */
                    opts.sign = '+';
                    fmt.inc();
                    break;

                case ' ':
                    /* note the blank-for-plus specifier */
                    opts.sign = ' ';
                    fmt.inc();
                    break;

                case ',':
                    /* note the group specifier */
                    opts.group = ',';
                    fmt.inc();
                    break;

                case '-':
                    /* note the left-alignment specifier */
                    opts.left_align = TRUE;
                    fmt.inc();
                    break;

                case '_':
                    /* padding spec - the next charater is the pad char */
                    fmt.inc();
                    opts.pad = fmt.skipwch();
                    break;

                case '#':
                    /* pound flag - special flag per type */
                    opts.pound = TRUE;
                    fmt.inc();
                    break;

                default:
                    /* it's not an option flag, so we're done with flags */
                    flags_done = TRUE;
                    break;
                }
            }

            /* 
             *   Next comes the width, but there's one more flag character
             *   that has a special position just before the width: '0', to
             *   specify leading zero padding.
             */
            if (fmt.match_skip('0'))
            {
                /* obey this only if we're in right-align mode */
                if (!opts.left_align)
                    opts.pad = '0';
            }

            /* check for a width specifier */
            opts.width = fmt.check_atoi();

            /* check for a precision specifier */
            if (fmt.match_skip('.'))
                opts.prec = fmt.atoi();

            /* we're at the type specifier */
            char type_spec = fmt.skipch();

            /* presume we'll use an argument */
            int used_arg = TRUE;

            /* retrieve the current argument value */
            int argi = (argno > 0 ? argno : argpos);
            const vm_val_t *val =
                (argi <= argc ? G_stk->get(arg0 + argi - 1) : &nil_val);

            /* apply the substitution based on the type */
            switch (type_spec)
            {
            case '%':
                /* literal % - copy it */
                dst.putch('%');

                /* this doesn't use an argument */
                used_arg = FALSE;
                break;

            case 'b':
                /* number -> binary integer */
                dst.format_int(vmg_ val, 2, 0, opts, FI_UNSIGNED);
                break;

            case 'c':
                /* number -> Unicode character */
                dst.format_char(vmg_ val);
                break;

            case 'd':
                /* number -> decimal integer */
                dst.format_int(vmg_ val, 10, 0, opts);
                break;

            case 'u':
                /* number -> decimal integer, unsigned interpretation */
                dst.format_int(vmg_ val, 10, 0, opts, FI_UNSIGNED);
                break;

            case 'e':
            case 'E':
            case 'f':
            case 'g':
            case 'G':
                /* number -> floating point */
                dst.format_float(vmg_ val, type_spec, opts);
                break;

            case 'o':
                /* number -> octal integer */
                dst.format_int(vmg_ val, 8, "0", opts, FI_UNSIGNED);
                break;

            case 's':
                /* string */
                dst.format_string(vmg_ val, opts);
                break;

            case 'x':
                /* number -> hex integer, lowercase letters */
                dst.format_int(vmg_ val, 16, "0x", opts, FI_UNSIGNED);
                break;

            case 'X':
                /* number -> hex integer, uppercase letters */
                dst.format_int(vmg_ val, 16, "0X", opts,
                               FI_UNSIGNED | FI_CAPS);
                break;

            default:
                /* anything else is invalid - just copy the source % string */
                dst.puts(pct, fmt.p - pct);

                /* we didn't use an argument after all */
                used_arg = FALSE;
                break;
            }

            /* if we used a positional argument, count it */
            if (used_arg && argno < 0)
                ++argpos;
        }
        else
        {
            /* just copy this byte verbatim */
            dst.putch(ch);
        }
    }

    /* set the final result string length */
    dst.close();

    /* done with our gc protection */
    G_stk->discard();
}

/*
 *   sprintf
 */
void CVmBifTADS::sprintf(VMG_ uint argc)
{
    /* we need at least one argument */
    if (argc < 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* get the format string */
    const char *fmt = G_stk->get(0)->get_as_string(vmg0_);
    if (fmt == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the length and string buffer */
    size_t fmtl = vmb_get_len(fmt);
    fmt += VMB_LEN;

    /* do the sprintf */
    vm_val_t retv;
    tsprintf(vmg_ &retv, fmt, fmtl, 1, argc - 1);

    /* return the new string */
    retval(vmg_ &retv);

    /* discard arguments */
    G_stk->discard(argc);
}
