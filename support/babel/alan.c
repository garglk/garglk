/* alan.c  Treaty of Babel module for ALAN files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT alan
#define HOME_PAGE "http://www.alanif.se/"
#define FORMAT_EXT ".acd,.a3c"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>


typedef unsigned char byte;

static uint32 read_alan_int_at(byte *from)
{
  return ((uint32) from[3])| ((uint32)from[2] << 8) |
    ((uint32) from[1]<<16)| ((uint32)from[0] << 24);
}

static bool magic_word_found(byte *story_file) {
  if(story_file == NULL)
    return false;
  return memcmp(story_file,"ALAN",4) == 0;
}

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
  if (story_file || extent)
    { }

  if (!magic_word_found(story_file))
    {
      ASSERT_OUTPUT_SIZE(5);
      strcpy(output,"ALAN-");
      return INCOMPLETE_REPLY_RV;
    }
  else
    {
      /*
        Alan v3 stores IFIDs in the story file in a format that might differ between versions.
        So just scan for "UUID://".
        Alan files seem to have lower-case hex in the "UUID://" string,
        which is not what we want, but we accept it for historic
        reasons. We therefore don't call find_uuid_ifid_marker().
      */
      int32 i, j, k;

      for(i=0;i<extent;i++) if (memcmp((char *)story_file+i,"UUID://",7)==0) break;
      if (i<extent) /* Found explicit IFID */
        {
          for(j=i+7;j<extent && ((char *)story_file)[j]!='/';j++);
          if (j<extent)
            {
              i+=7;
              ASSERT_OUTPUT_SIZE(j-i);
              for(k=0;k<j-i;k++)
                {
                  output[k]=toupper(((char *)story_file)[i+k]);
                }
              output[j-i]=0;
              return VALID_STORY_FILE_RV;
            }
        }
      ASSERT_OUTPUT_SIZE(5);
      strcpy(output,"ALAN-");
      return INCOMPLETE_REPLY_RV;
    }
}


static bool crc_is_correct(byte *story_file, int32 size_in_awords) {
  /* Size of AcodeHeader is 50 Awords = 200 bytes */
  int32 calculated_crc = 0;
  int32 crc_in_file = read_alan_int_at(story_file+46*4);

  for (int i=50*4;i<(size_in_awords*4);i++)
    calculated_crc+=story_file[i];

  /* Some Alan 3 games seem to have added 284 to their internal checksum.
     We allow this error.
     (It would be more conservative to only allow this error for a few games:
     A Very Hairy Fish-Mess, The Ngah Angah School of Forbidden Wisdom,
     Room 206, IN-D-I-GO SOUL, The Christmas Party. But we're not going
     to be that fussy.) */
  return (calculated_crc == crc_in_file || calculated_crc + 284 == crc_in_file);
}


/*
  The claim algorithm for Alan files is:
  * For Alan 3, check for the magic word
  * load the file length in blocks
  * check that the file length is correct
  * For alan 2, each word between byte address 24 and 81 is a
  word address within the file, so check that they're all within
  the file
  * Locate the checksum and verify that it is correct
  */
static int32 claim_story_file(void *story_file, int32 extent_in_bytes)
{
  byte *sf = (byte *) story_file;
  uint32 size_in_awords, i;

  if (extent_in_bytes < 160)
    /* File is shorter than any Alan file header */
    return INVALID_STORY_FILE_RV;

  size_in_awords=read_alan_int_at(sf+4);

  if (!magic_word_found(sf))
    { /* Identify Alan 2.x */
      uint32 crc = 0;

      if (size_in_awords > extent_in_bytes/4) return INVALID_STORY_FILE_RV;
      for (i=24;i<81;i+=4)
        if (read_alan_int_at(sf+i) > extent_in_bytes/4) return INVALID_STORY_FILE_RV;
      for (i=160;i<(size_in_awords*4);i++)
        crc+=sf[i];
      if (crc!=read_alan_int_at(sf+152)) return INVALID_STORY_FILE_RV;
      return VALID_STORY_FILE_RV;
    }
  else
    { /* Identify Alan 3 */
      size_in_awords=read_alan_int_at(sf+3*4); /* hdr.size @ 3 */

      if (!crc_is_correct(sf, size_in_awords))
        {
          switch (read_alan_int_at(sf+46*4))
            {
              case 1427594: /* Enter The Dark */
              case 8683866: /* Waldo’s Pie */
                return VALID_STORY_FILE_RV;
              default:
                return INVALID_STORY_FILE_RV;
            }
        }
      return VALID_STORY_FILE_RV;
    }
}
