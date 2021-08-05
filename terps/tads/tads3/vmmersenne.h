/* $Header$ */

/* Copyright (c) 2012 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmmersenne.h - Mersenne twister random number generator definitions
Function
  
Notes
  
Modified
  02/28/12 MJRoberts  - Creation
*/

#ifndef VMMERSENNE_H
#define VMMERSENNE_H

#include "t3std.h"


/*
 *   Mersenne Twister MT19937 algorithm 
 */
class CVmMT19937
{
public:
    CVmMT19937()
    {
        idx = 0;
    }

    /* seed from OS-generated random data */
    void seed_random()
    {
        /* fill the mt[] array with random bytes */
        os_gen_rand_bytes((unsigned char *)mt, sizeof(mt));
    }

    /* 
     *   Seed from an integer value.  This uses an LCG to extend the integer
     *   to fill the array.  This follows the reference implementation's
     *   seeding mechanism, which lets us test against reference output.
     */
    void seed(uint32_t s)
    {
        /* do a pass through the mt array with LCG values from the seed */
        base_seed(s);

        /* 
         *   discard a few initial values to prime the pump - the MT
         *   algorithm is considered slow at getting started (i.e., it needs
         *   to run through a few iterations before it starts generating good
         *   numbers)
         */
        for (int i = 0 ; i < 37 ; ++i)
            rand();
    }

    /* 
     *   Seed the generator from bytes.  The seed can be any size; we'll use
     *   a combination of hashing and LCG generation to populate the internal
     *   state vector.
     */
    void seed(const char *buf, size_t len)
    {
        /*
         *   Use the mechanism from the reference implementation for seeding
         *   from multiple values.  First do a fixed seeding of the mt array
         *   to fill it with nonzero bits. 
         */
        base_seed(19650218);

        /* treat the source as an array of int32s */
        size_t icnt = (len + 3)/4;

        /* if there's a partial int32 at the end, figure its value */
        uint32_t partial = 0;
        if (icnt*4 > len)
        {
            int mul = 1;
            for (size_t i = (len/4)*4 ; i < len ; ++i, mul *= 256)
                partial += buf[i]*mul;
        }

        /* 
         *   do a pass over the mt array, doing the sort of LCG
         *   initialization we do with a single int seed, but folding each
         *   seed value into the running LCG feedback register
         */
        int i = 1, j = 0, k = NMT > icnt ? NMT : icnt;
        for ( ; k != 0 ; --k)
        {
            /* get the current seed (the last one might be partial) */
            uint32_t seed = (j+1)*4 <= (int)len ? osrp4(buf + j*4) : partial;
           
            /* generate an LCG value mixing in this seed value */
            mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 0x19660D))
                    + seed + j;

            /* on to the next mt index, wrapping at the end */
            if (++i >= NMT)
            {
                mt[0] = mt[NMT-1];
                i = 1;
            }

            /* on to the next seed index, wrapping at the end */
            if (++j >= (int)icnt)
                j = 0;
        }

        /* make another pass over the mt array */
        for (k = NMT-1 ; k != 0 ; --k)
        {
            mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 0x5D588B65)) - i;
            if (++i >= NMT)
            {
                mt[0] = mt[NMT-1];
                i = 1;
            }
        }

        /* and finally, make sure the initial array isn't all zeros */
        mt[0] = 0x80000000UL;

        /* flush out the first full mt array */
        idx = 0;
        for (i = 0 ; i <= NMT ; ++i)
            rand();
    }

    /* generate a random number in the full uint32_t range */
    uint32_t rand()
    {
        /* if we're out of untempered numbers, generate the next set */
        if (idx >= NMT)
        {
            generate_untempered();
            idx = 0;
        }

        /* generate a tempered result */
        uint32_t y = mt[idx];
        y ^= (y >> 11);
        y ^= ((y << 7) & 0x9d2c5680);
        y ^= ((y << 15) & 0xefc60000);
        y ^= (y >> 18);

        /* on to the next index */
        ++idx;

        /* return the result */
        return y;
    }

    /* get the size of the serialized state vector */
    size_t get_state_size() const { return 2 + NMT*4; }

    /* serialize the state into a byte buffer */
    void get_state(char *buf)
    {
        oswp2(buf, (short)idx);
        for (int i = 0, dst = 2 ; i < NMT ; ++i, dst += 4)
            oswp4(buf + dst, mt[i]);
    }

    /* restore the state from a byte buffer */
    void put_state(const char *buf)
    {
        idx = osrp2(buf) % NMT;
        for (int i = 0, src = 2 ; i < NMT ; ++i, src += 4)
            mt[i] = osrp4(buf + src);
    }

protected:
    /* basic seed generator */
    void base_seed(uint32_t s)
    {
        mt[0] = s;
        for (idx = 1 ; idx < NMT ; ++idx)
            mt[idx] = 0x6c078965 * (mt[idx-1] ^ (mt[idx-1] >> 30)) + idx;
    }

    /* generate a new set of 624 untempered numbers */
    void generate_untempered()
    {
        for (int i = 0 ; i < NMT ; ++i)
        {
            uint32_t y = (mt[i] & 0x80000000) + (mt[(i+1) % NMT] & 0x7fffffff);
            mt[i] = mt[(i + 397) % NMT] ^ (y >> 1);
            if ((y & 1) != 0)
                mt[i] ^= 0x9908b0df;
        }
    }
    
    /* internal state vector */
    static const int NMT = 624;
    uint32_t mt[NMT];

    /* current element index */
    int idx;
};


#endif /* VMMERSENNE_H */
