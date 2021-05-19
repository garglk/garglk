/* agt.c  Treaty of Babel module for AGX-encapsulated AGT files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT agt
#define HOME_PAGE "http://www.ifarchive.org/indexes/if-archiveXprogrammingXagt"
#define FORMAT_EXT ".agx"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>


static char AGX_MAGIC[4] = { 0x58, 0xC7, 0xC1, 0x51 };

/* Helper functions to unencode integers from AGT source */
static int32 read_agt_short(unsigned char *sf)
{
 return sf[0] | (int32) sf[1]<<8;
}
static int32 read_agt_int(unsigned char *sf)
{
 return (read_agt_short(sf+2) << 16) | read_agt_short(sf);

}

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 int32 l, game_version, game_sig;
 unsigned char *sf=(unsigned char *)story_file;

 /* Read the position of the game desciption block */
 l=read_agt_int(sf+32);
 if (extent<l+6) return INVALID_STORY_FILE_RV;
 game_version = read_agt_short(sf+l);
 game_sig=read_agt_int(sf+l+2);
 ASSERT_OUTPUT_SIZE(19);
 sprintf(output,"AGT-%05d-%08X",game_version,game_sig);
 return 1;
}

/* The claim algorithm for AGT is to check for the magic word
   defined above
*/
static int32 claim_story_file(void *story_file, int32 extent)
{


 if (extent<36 || memcmp(story_file,AGX_MAGIC,4)) return INVALID_STORY_FILE_RV;
 return VALID_STORY_FILE_RV;

}
