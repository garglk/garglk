/* zcode.c  Treaty of Babel module for Z-code files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT zcode
#define HOME_PAGE "http://www.inform-fiction.org"
#define FORMAT_EXT ".z3,.z4,.z5,.z6,.z7,.z8"
#define NO_METADATA
#define NO_COVER
#define CUSTOM_EXTENSION
#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 uint32 i,j;
 char ser[7];
 char buffer[32];


 if (extent<0x1D) return INVALID_STORY_FILE_RV;
 memcpy(ser, (char *) story_file+0x12, 6);
 ser[6]=0;
 /* Detect vintage story files */
 if (!(ser[0]=='8' || ser[0]=='9' ||
     (ser[0]=='0' && ser[1]>='0' && ser[1]<='5')))
 {
  for(i=0;i<(uint32) extent-7;i++) if (memcmp((char *)story_file+i,"UUID://",7)==0) break;
  if (i<(uint32) extent) /* Found explicit IFID */
  {
   for(j=i+7;j<(uint32)extent && ((char *)story_file)[j]!='/';j++);
   if (j<(uint32) extent)
   {
    i+=7;
    ASSERT_OUTPUT_SIZE((int32) (j-i));
    memcpy(output,(char *)story_file+i,j-i);
    output[j-i]=0;
    return 1;
   }
  }
 }
 /* Did not find intact IFID.  Build one */
 i=((unsigned char *)story_file)[2] << 8 |((unsigned char *)story_file)[3];
 for(j=0;j<6;j++)
  if (!isalnum(ser[j])) ser[j]='-';

 j=((unsigned char *)story_file)[0x1C] << 8 |((unsigned char *)story_file)[0x1D];

 if (strcmp(ser,"000000") && isdigit(ser[0]) && ser[0]!='8')
  sprintf(buffer,"ZCODE-%d-%s-%04X",i,ser,j);
 else
  sprintf(buffer,"ZCODE-%d-%s",i,ser);

 ASSERT_OUTPUT_SIZE((signed) strlen(buffer)+1);
 strcpy((char *)output,buffer);
 return 1;

}

static uint32 read_zint(unsigned char *sf)
{
 return ((uint32)sf[0] << 8) | ((uint32) sf[1]);

}
static int32 claim_story_file(void *story_file, int32 extent)
{
 unsigned char *sf=(unsigned char *)story_file;
 uint32 i,j;
 if (extent<0x3c ||
     sf[0] < 1 ||
     sf[0] > 8
    ) return INVALID_STORY_FILE_RV;
 for(i=4;i<=14;i+=2)
 {
  j=read_zint(sf+i);
  if (j>(uint32) extent || j < 0x40) return INVALID_STORY_FILE_RV;
 }

 return VALID_STORY_FILE_RV;
}
static int32 get_story_file_extension(void *sf, int32 extent, char *out, int32 output_extent)
{
 int v;
 if (!extent) return INVALID_STORY_FILE_RV;
 v= ((char *) sf)[0];
 if (v>9) ASSERT_OUTPUT_SIZE(5);
 else ASSERT_OUTPUT_SIZE(4);
 sprintf(out,".z%d",v);
 return 3+(v>9);

}
