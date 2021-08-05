/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmisaac.cpp - T3 VM ISAAC random number generator implementation
Function
  
Notes
  
Modified
  04/11/10 MJRoberts  - Creation
*/

#include "vmisaac.h"

/* service macros for ISAAC random number generator */
#define isaac_ind(mm,x)  ((mm)[(x>>2)&(ISAAC_RANDSIZ-1)])
#define isaac_step(mix,a,b,mm,m,m2,r,x) \
    { \
        x = *m;  \
        a = ((a^(mix)) + *(m2++)) & 0xffffffff; \
        *(m++) = y = (isaac_ind(mm,x) + a + b) & 0xffffffff; \
        *(r++) = b = (isaac_ind(mm,y>>ISAAC_RANDSIZL) + x) & 0xffffffff; \
    }

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
void isaac_gen_group(isaacctx *ctx)
{
    uint32_t a;
    uint32_t b;
    uint32_t x;
    uint32_t y;
    uint32_t *m;
    uint32_t *mm;
    uint32_t *m2;
    uint32_t *r;
    uint32_t *mend;

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
void isaac_init(isaacctx *ctx, int flag)
{
    int i;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t *m;
    uint32_t *r;

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
 *   Get the internal state 
 */
size_t isaac_get_state(isaacctx *ctx, char *buf)
{
    /* if the didn't provide a buffer, it's a size-needed query */
    const size_t sz = 4 * (4 + ISAAC_RANDSIZ + ISAAC_RANDSIZ);
    if (buf == 0)
        return sz;

    /* copy the scalar members */
    oswp4(buf, ctx->cnt); buf += 4;
    oswp4(buf, ctx->a);   buf += 4;
    oswp4(buf, ctx->b);   buf += 4;
    oswp4(buf, ctx->c);   buf += 4;

    /* copy the array members */
    for (int i = 0 ; i < ISAAC_RANDSIZ ; ++i, buf += 8)
    {
        oswp4(buf, ctx->rsl[i]);
        oswp4(buf+4, ctx->mem[i]);
    }

    return sz;
}

/*
 *   Set the internal state 
 */
void isaac_set_state(isaacctx *ctx, const char *buf)
{
    /* decode the buffer into our context */
    ctx->cnt = osrp4(buf); buf += 4;
    ctx->a = osrp4(buf);   buf += 4;
    ctx->b = osrp4(buf);   buf += 4;
    ctx->c = osrp4(buf);   buf += 4;
    for (int i = 0 ; i < ISAAC_RANDSIZ ; ++i, buf += 8)
    {
        ctx->rsl[i] = osrp4(buf);
        ctx->mem[i] = osrp4(buf+4);
    }

    /* sanity check the result: make sure 'cnt' is in range */
    if (ctx->cnt > ISAAC_RANDSIZ)
        ctx->cnt = 0;
}
