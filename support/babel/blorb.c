/* blorb.c      Babel interface to blorb files
 * Copyright 2006 by L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution 4.0 License.
 * To view a copy of this license, visit
 * https://creativecommons.org/licenses/by/4.0/ or send a letter to
 * Creative Commons,
 * PO Box 1866,
 * Mountain View, CA 94042, USA.
 *
 * This file depends upon treaty_builder.h, misc.c and ifiction.c
 *
 * Header note: to add support for new executable chunk types, see
 * TranslateExec.
 *
 * This file defines a Treaty of Babel compatable module for handling blorb
 * files.  However, blorb files are not themselves a babel format. This module
 * is used internally by the babel program to handle blorbs.
 *
 * As a result, if GET_STORY_FILE_IFID_SEL returns NO_REPLY_RV,
 * you should check the story file against the babel registry before resorting
 * to the default IFID calculation.
 *
 */
#define FORMAT blorb
#define HOME_PAGE "http://eblong.com/zarf/blorb"
#define FORMAT_EXT ".blorb,.blb,.zblorb,.zlb,.gblorb,.glb"
#define CONTAINER_FORMAT
#include "treaty_builder.h"
#include <stdlib.h>
#include <ctype.h>

extern TREATY treaty_registry[];
/* The following is the translation table of Blorb chunk types to
   babel formats.  it is NULL-terminated. */
static char *TranslateExec[] = { "ZCOD", "zcode",
                                 "GLUL", "glulx",
                                 "TAD2", "tads2",
                                 "TAD3", "tads3",
                                 NULL, NULL };

void *my_malloc(int32, char *);
int32 ifiction_get_IFID(char *, char *, int32);

static uint32 read_int(void *inp)
{
  unsigned char *mem=(unsigned char *)inp;
  uint32 i4 = mem[0],
                    i3 = mem[1],
                    i2 = mem[2],
                    i1 = mem[3];
  return i1 | (i2<<8) | (i3<<16) | (i4<<24);
}


static int32 blorb_get_chunk(void *blorb_file, int32 extent, char *id, uint32 *begin, uint32 *output_extent)
{
 uint32 i=12, j;
 while(i<(uint32) extent-8)
 {
  if (memcmp(((char *)blorb_file)+i,id,4)==0)
  {
   *output_extent=read_int((char *)blorb_file+i+4);
   if (*output_extent > (uint32) extent) return NO_REPLY_RV;
   *begin=i+8;
   return 1;
  }

  j=read_int((char *)blorb_file+i+4);
  if (j%2) j++;
  i+=j+8;

 }
 return NO_REPLY_RV;
}
static int32 blorb_get_resource(void *blorb_file, int32 extent, char *rid, int32 nnumber, uint32 *begin, uint32 *output_extent)
{
 uint32 ridx_len;
 uint32 i,j;
 uint32 number=(uint32) nnumber;
 void *ridx;
 if (blorb_get_chunk(blorb_file, extent,"RIdx",&i,&ridx_len)==NO_REPLY_RV)
  return NO_REPLY_RV;

 ridx=(char *)blorb_file+i+4;
 ridx_len=read_int((char *)blorb_file+i);
 for(i=0;i<ridx_len;i++)
 { 
  if (memcmp((char *)ridx+(i*12),rid,4)==0 && read_int((char *)ridx+(i*12)+4)==number)
  {
   j=i;
   i=read_int((char *)ridx+(j*12)+8);
   *begin=i+8;
   *output_extent=read_int((char *)blorb_file+i+4);
   if (*begin > extent || *begin + *output_extent > extent)
     return NO_REPLY_RV;
   return 1;
  }
 }
 return NO_REPLY_RV;
}

#if 0  /* blorb_get_story_file is not currently used */
static int32 blorb_get_story_file(void *blorb_file, int32 extent, uint32 *begin, uint32 *output_extent)
{
 return blorb_get_resource(blorb_file, extent, "Exec", 0, begin, output_extent);

}
#endif /* 0 */

static int32 get_story_extent(void *blorb_file, int32 extent)
{
 uint32 i,j;
 if (blorb_get_resource(blorb_file, extent, "Exec", 0, &i, &j))
 {
  return j;
 }
 return NO_REPLY_RV;

}
static int32 get_story_file(void *blorb_file, int32 extent, void *output, int32 output_extent)
{
 uint32 i,j;
 if (blorb_get_resource(blorb_file, extent, "Exec", 0, &i, &j))
 {
  ASSERT_OUTPUT_SIZE((int32) j);
  memcpy(output,(char *)blorb_file+i,j);
  return j;
 }
 return NO_REPLY_RV;

}

char *blorb_chunk_for_name(char *name)
{
 static char buffer[5];
 int j;
 for(j=0;TranslateExec[j];j+=2)
  if (strcmp(name,TranslateExec[j+1])==0) return TranslateExec[j];
 for(j=0;j<4 && name[j];j++) buffer[j]=toupper(buffer[j]);
 while(j<4) buffer[j++]=' ';
 buffer[4]=0;
 return buffer;

}
static char *blorb_get_story_format(void *blorb_file, int32 extent)
{
 uint32 i, j;

 for(j=0;treaty_registry[j];j++)
 {
  static char fn[512];
  treaty_registry[j](GET_FORMAT_NAME_SEL,NULL,0,fn,512);
  if (blorb_get_chunk(blorb_file,extent,blorb_chunk_for_name(fn),&i, &i)) return fn;
 }
 return NULL;
}

static int32 get_story_format(void *blorb_file, int32 extent, char *output, int32 output_extent)
{
 char *o;
 o=blorb_get_story_format(blorb_file, extent);
 if (!o) return NO_REPLY_RV;
 ASSERT_OUTPUT_SIZE((signed) strlen(o)+1);
 strcpy(output,o);
 return strlen(o)+1;
}
static int32 get_story_file_metadata_extent(void *blorb_file, int32 extent)
{
 uint32 i,j;
 if (blorb_get_chunk(blorb_file,extent,"IFmd",&i,&j)) return j+1;
 return NO_REPLY_RV;
}
static int32 blorb_get_cover(void *blorb_file, int32 extent, uint32 *begin, uint32 *output_extent)
{
 uint32 i,j;
 if (blorb_get_chunk(blorb_file,extent,"Fspc",&i,&j))
 {
  if (j<4) return NO_REPLY_RV;
  i=read_int((char *)blorb_file+i);
  if (!blorb_get_resource(blorb_file,extent,"Pict",i,&i,&j)) return NO_REPLY_RV;
  *begin=i;
  *output_extent=j;
  if (memcmp((char *)blorb_file+i-8,"PNG ",4)==0) return PNG_COVER_FORMAT;
  else if (memcmp((char *)blorb_file+i-8,"JPEG",4)==0) return JPEG_COVER_FORMAT;
 }
 return NO_REPLY_RV;

}

static int32 get_story_file_cover_extent(void *blorb_file, int32 extent)
{
 uint32 i,j;
 if (blorb_get_cover(blorb_file,extent,&i,&j)) return j;
 return NO_REPLY_RV;
}

static int32 get_story_file_cover_format(void *blorb_file, int32 extent)
{
 uint32 i,j;
 return blorb_get_cover(blorb_file, extent, &i,&j);
}

static int32 get_story_file_IFID(void *b, int32 e, char *output, int32 output_extent)
{
 int32 j;
 char *md;
 j=get_story_file_metadata_extent(b,e);
 if (j<=0) return NO_REPLY_RV;
 md=(char *)my_malloc(j, "Metadata buffer");
 j=get_story_file_metadata(b,e,md,j);
 if (j<=0) return NO_REPLY_RV;

 j=ifiction_get_IFID(md,output,output_extent);
 free(md);
 return j;
}

static int32 get_story_file_metadata(void *blorb_file, int32 extent, char *output, int32 output_extent)
{
 uint32 i,j;
 if (!blorb_get_chunk(blorb_file, extent,"IFmd",&i,&j)) return NO_REPLY_RV;
 ASSERT_OUTPUT_SIZE((int32) j+1);
 memcpy(output,(char *)blorb_file+i,j);
 output[j]=0;
 return j+1;
}
static int32 get_story_file_cover(void *blorb_file, int32 extent, void *output, int32 output_extent)
{
 uint32 i,j;
 if (!blorb_get_cover(blorb_file, extent,&i,&j)) return NO_REPLY_RV;
 ASSERT_OUTPUT_SIZE((int32) j);
 memcpy(output,(char *)blorb_file+i,j);
 return j;
}

static int32 claim_story_file(void *story_file, int32 extent)
{
 int i;

 if (extent<16 ||
     memcmp(story_file,"FORM",4) ||
     memcmp((char *)story_file+8,"IFRS",4) 
    ) i= INVALID_STORY_FILE_RV;
 else i= NO_REPLY_RV;
 
 return i;

}
