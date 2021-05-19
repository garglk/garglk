/* hugo.c  Treaty of Babel module for hugo files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT hugo
#define HOME_PAGE "http://www.generalcoffee.com"
#define FORMAT_EXT ".hex"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

static int32 get_story_file_IFID(void *s_file, int32 extent, char *output, int32 output_extent)
{

 int32 i,j;
 char ser[9];
 char buffer[32];
 char *story_file = (char *) s_file;


 if (extent<0x0B) return INVALID_STORY_FILE_RV;

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
 
 memcpy(ser, (char *) story_file+0x03, 8);
 ser[8]=0;

 for(j=0;j<8;j++)
  if (!isalnum(ser[j])) ser[j]='-';


 sprintf(buffer,"HUGO-%d-%02X-%02X-%s",story_file[0],story_file[1], story_file[2],ser);

 ASSERT_OUTPUT_SIZE((signed) strlen(buffer)+1);
 strcpy((char *)output,buffer);
 return 1;
}

static uint32 read_hugo_addx(unsigned char *from)
{
 return ((uint32) from[0])| ((uint32)from[1] << 8);
}

static int32 claim_story_file(void *story_file, int32 exten)
{
 unsigned char *sf=(unsigned char *)story_file;
 int32 i;
 int32 scale;
 uint32 extent=(uint32) exten;

 if (!story_file || extent < 0x28) return  INVALID_STORY_FILE_RV;

 /* 39 is the largest version currently accepted by the Hugo interpreter:
    https://github.com/garglk/garglk/blob/master/terps/hugo/source/hemisc.c#L1310-L1362
 */
 if (sf[0] > 39) return INVALID_STORY_FILE_RV;
 
 if (sf[0]<34) scale=4;
 else scale=16;
 for(i=3;i<0x0B;i++) if (sf[i]<0x20 || sf[i]>0x7e) return INVALID_STORY_FILE_RV;
 for(i=0x0b;i<0x18;i+=2)
 if (read_hugo_addx(sf+i) * scale > extent) return INVALID_STORY_FILE_RV;

 return VALID_STORY_FILE_RV;
}
