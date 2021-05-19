/* glulx.c  Treaty of Babel module for Glulx files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT glulx
#define HOME_PAGE "http://eblong.com/zarf/glulx"
#define FORMAT_EXT ".ulx"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

static uint32 read_int(unsigned char  *mem)
{
  uint32 i4 = mem[0],
                    i3 = mem[1],
                    i2 = mem[2],
                    i1 = mem[3];
  return i1 | (i2<<8) | (i3<<16) | (i4<<24);
}



static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 int32 i,j, k;
 char ser[7];
 char buffer[32];


 if (extent<256) return INVALID_STORY_FILE_RV;
 for(i=0;i<extent-7;i++) if (memcmp((char *)story_file+i,"UUID://",7)==0) break;
 if (i<extent) /* Found explicit IFID */
  {
   for(j=i+7;j<extent && ((char *)story_file)[j]!='/';j++);
   if (j<extent)
   {
    i+=7;
    ASSERT_OUTPUT_SIZE(j-i);
    memcpy(output,(char *)story_file+i,j-i);
    output[j-i]=0;
    return 1;
   }
  }

 /* Did not find intact IFID.  Build one */

 j=read_int((unsigned char *)story_file+32);
 k=read_int((unsigned char *)story_file+12);
 if (memcmp((char *)story_file+36,"Info",4)==0)
 { /* Inform generated */
  char *bb=(char *)story_file+52;
  k= (int) bb[0]<<8 | (int) bb[1];
  memcpy(ser,bb+2,6);
  ser[6]=0;
  for(i=0;i<6;i++) if (!isalnum(ser[i])) ser[i]='-';
  sprintf(buffer,"GLULX-%u-%s-%04X",k,ser,j);
 }
 else
  sprintf(buffer,"GLULX-%08X-%08X",k,j);

 ASSERT_OUTPUT_SIZE((signed) strlen(buffer)+1);
 strcpy((char *)output,buffer);
 return 1;

}

static int32 claim_story_file(void *story_file, int32 extent)
{
 if (extent<256 ||
     memcmp(story_file,"Glul",4)
    ) return INVALID_STORY_FILE_RV;
 return VALID_STORY_FILE_RV;
}
