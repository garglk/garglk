/* twine.c  Treaty of Babel module for Twine files
 * Copyright 2020 By Andrew Plotkin
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT twine
#define HOME_PAGE "https://twinery.org/"
#define FORMAT_EXT ".html"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

/* Searches case-insensitively for the string str in the story file.
   The string must be null-terminated. The story file can have nulls
   anywhere, of course.

   Returns the found position, or -1 for not found.
*/
static int32 find_text_in_file(void *story_file, int32 extent, char *str)
{
    int len = strlen(str);
    int32 ix;

    if (len == 0) {
        return -1;
    }
    
    for (ix=0; ix<extent-len; ix++) {
        if (strncasecmp(story_file+ix, str, len) == 0)
            return ix;
    }

    return -1;
}

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
    int32 pos = find_text_in_file(story_file, extent, "<tw-storydata");
    if (pos != -1) {
        /* Search the <tw-storydata> tag for ifid="..." */
        void *starttag = story_file + pos;
        void *endtag = memchr(starttag, '>', extent-pos);
        if (endtag) {
            int32 attrpos = find_text_in_file(starttag, endtag-starttag, "ifid=\"");
            if (attrpos != -1) {
                attrpos += 6; /* skip the ifid=" */
                void *endattr = memchr(starttag+attrpos, '"', endtag-(starttag+attrpos));
                if (endattr) {
                    /* Got it. */
                    int32 attrlen = endattr - (starttag+attrpos);
                    ASSERT_OUTPUT_SIZE(attrlen+1);
                    memcpy(output, starttag+attrpos, attrlen);
                    output[attrlen] = '\0';
                    return VALID_STORY_FILE_RV;
                }
            }
        }

        /* Couldn't find the ifid attribute. */
        return INVALID_STORY_FILE_RV;
    }
    
    /* This version of Twine is too old to include an IFID, so we'll
       generate one from the MD5. */
    ASSERT_OUTPUT_SIZE(8);
    strcpy(output,"TWINE-");
    return INCOMPLETE_REPLY_RV;
}

static int32 claim_story_file(void *story_file, int32 extent)
{
    if (find_text_in_file(story_file, extent, "<tw-storydata") != -1) {
        /* Twine 2 or later. See https://github.com/iftechfoundation/twine-specs/blob/master/twine-2-htmloutput-spec.md#story-data */
        return VALID_STORY_FILE_RV;
    }

    if (find_text_in_file(story_file, extent, "modifier=\"twee\"") != -1) {
        /* Twine 1, almost certainly. */
        return VALID_STORY_FILE_RV;
    }
        
    return INVALID_STORY_FILE_RV;
}
