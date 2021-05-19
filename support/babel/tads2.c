/* 
 *   tads2.c - Treaty of Babel module for Tads 2 files
 *   
 *   This file depends on treaty_builder.h
 *   
 *   This file is public domain, but note that any changes to this file may
 *   render it noncompliant with the Treaty of Babel
 *   
 *   Modified
 *.   04/15/2006 LRRaszewski - Separated tads2.c and tads3.c 
 *.   04/08/2006 LRRaszewski - changed babel API calls to threadsafe versions
 *.   04/08/2006 MJRoberts  - initial implementation
 */

#define FORMAT tads2
#define HOME_PAGE "http://www.tads.org"
#define FORMAT_EXT ".gam"


#include "treaty_builder.h"
#include "tads.h"

#define T2_SIGNATURE "TADS2 bin\012\015\032"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/*
 *   get a story file's IFID
 */
static int32 get_story_file_IFID(void *story_file, int32 extent,
                                 char *output, int32 output_extent)
{
    /* use the common tads IFID extractor/generator */
    return tads_get_story_file_IFID(story_file, extent,
                                    output, output_extent);
}

/*
 *   determine if a given story file is one of ours 
 */
static int32 claim_story_file(void *story_file, int32 extent)
{
    /* check our signature */
    if (tads_match_sig(story_file, extent, T2_SIGNATURE))
        return VALID_STORY_FILE_RV;

    /* not one of ours */
    return INVALID_STORY_FILE_RV;
}

/*
 *   Get the size of the iFiction metadata for the game 
 */
static int32 get_story_file_metadata_extent(void *story_file, int32 extent)
{
    /* use the common tads iFiction synthesizer */
    return tads_get_story_file_metadata_extent(story_file, extent);
}

/*
 *   Get the iFiction metadata for the game
 */
static int32 get_story_file_metadata(void *story_file, int32 extent,
                                     char *buf, int32 bufsize)
{
    /* use the common tads iFiction synthesizer */
    return tads_get_story_file_metadata(story_file, extent, buf,  bufsize);
}

static int32 get_story_file_cover_extent(void *story_file, int32 story_len)
{
    /* use the common tads cover file extractor */
    return tads_get_story_file_cover_extent(story_file, story_len);
}

/*
 *   Get the format of the cover art 
 */
static int32 get_story_file_cover_format(void *story_file, int32 story_len)
{
    /* use the common tads cover file extractor */
    return tads_get_story_file_cover_format(story_file, story_len);
}

/*
 *   Get the cover art data 
 */
static int32 get_story_file_cover(void *story_file, int32 story_len,
                                  void *outbuf, int32 output_extent)
{
    /* use the common tads cover file extractor */
    return tads_get_story_file_cover(story_file, story_len,
                                     outbuf, output_extent);
}

