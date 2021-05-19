/* adrift.c  Treaty of Babel module for Adrift files
 *
 * PROVISIONAL - Hold for someone else
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT adrift
#define HOME_PAGE "http://www.adrift.org.uk"
#define FORMAT_EXT ".taf"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

/* VB RNG constants */
#define  VB_RAND1      0x43FD43FD
#define  VB_RAND2      0x00C39EC3
#define  VB_RAND3      0x00FFFFFF
#define  VB_INIT       0x00A09E86
static int32 vbr_state;

/*
  Unobfuscates one byte from a taf file. This should be called on each byte
  in order, as the ADRIFT obfuscation function is stately.

  The de-obfuscation algorithm works by xoring the byte with the next
  byte in the sequence produced by the Visual Basic pseudorandom number
  generator, which is simulated here.
*/
static unsigned char taf_translate (unsigned char c)
{
 int32 r;

 vbr_state = (vbr_state*VB_RAND1+VB_RAND2) & VB_RAND3;
 r=UCHAR_MAX * (unsigned) vbr_state;
 r/=((unsigned) VB_RAND3)+1;
 return r^c;
}

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 int adv;
 unsigned char buf[4];
 unsigned char *sf=(unsigned char *)story_file;
 vbr_state=VB_INIT;

 if (extent <12) return INVALID_STORY_FILE_RV;

 buf[3]=0;
 /* Burn the first 8 bytes of translation */
 for(adv=0;adv<8;adv++) taf_translate(0);
 /* Bytes 8-11 contain the Adrift version number in the formay N.NN */
 buf[0]=taf_translate(sf[8]);
 taf_translate(0);
 buf[1]=taf_translate(sf[10]);
 buf[2]=taf_translate(sf[11]);
 adv=atoi((char *) buf);
 ASSERT_OUTPUT_SIZE(12);
 sprintf(output,"ADRIFT-%03d-",adv);
 return INCOMPLETE_REPLY_RV;

}

/* The claim algorithm for ADRIFT is to unobfuscate the first
   seven bytes, and check for the word "Version".
   It seems fairly unlikely that the obfuscated form of that
   word would occur in the wild
*/
static int32 claim_story_file(void *story_file, int32 extent)
{
 unsigned char buf[8];
 int i;
 unsigned char *sf=(unsigned char *)story_file;
 buf[7]=0;
 vbr_state=VB_INIT;
 if (extent<12) return INVALID_STORY_FILE_RV;
 for(i=0;i<7;i++) buf[i]=taf_translate(sf[i]);
 if (strcmp((char *)buf,"Version")) return INVALID_STORY_FILE_RV;
 return VALID_STORY_FILE_RV;

}
