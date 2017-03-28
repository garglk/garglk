/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmisaac.h - TADS 3 VM ISAAC random number generator implementation
Function
  
Notes
  
Modified
  04/11/10 MJRoberts  - Creation
*/

#ifndef VMISAAC_H
#define VMISAAC_H

#include "t3std.h"

/* data block size */
#define ISAAC_RANDSIZL   (8)
#define ISAAC_RANDSIZ    (1<<ISAAC_RANDSIZL)

/* ISAAC RNG context */
struct isaacctx
{
    /* RNG context */
    uint32_t cnt;
    uint32_t rsl[ISAAC_RANDSIZ];
    uint32_t mem[ISAAC_RANDSIZ];
    uint32_t a;
    uint32_t b;
    uint32_t c;
};

#define isaac_rand(r) \
    ((r)->cnt-- == 0 ? \
     (isaac_gen_group(r), (r)->cnt=ISAAC_RANDSIZ-1, (r)->rsl[(r)->cnt]) : \
     (r)->rsl[(r)->cnt])

/* 
 *   Initialize.  If flag is true, then use the contents of ctx->rsl[] to
 *   initialize ctx->mm[]; otherwise, we'll use a fixed starting
 *   configuration.  
 */
void isaac_init(isaacctx *ctx, int flag);

/* 
 *   Get/set the internal state.  This allows saving and restoring the
 *   internal state of the RNG.  'get' returns the size of the byte buffer
 *   required; call with buf==0 to determine the size needed.
 *   
 *   'get' and 'set' both expect the caller to know the correct size from a
 *   size-query call to 'get'.  The size passed to 'set' must always exactly
 *   match the size returned from 'get', since anything else could corrupt
 *   the internal state.
 */
size_t isaac_get_state(isaacctx *ctx, char *buf);
void isaac_set_state(isaacctx *ctx, const char *buf);

/* generate a group of random values */
void isaac_gen_group(isaacctx *ctx);

#endif /* VMISAAC_H */
