/* html.c  Treaty of Babel module for HTML files
 * Written 2020 By Andrew Plotkin
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT html
#define HOME_PAGE "https://babel.ifarchive.org/"
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

/* Same as the above, except that str and str2 can be separated by any text that does not contain "<" or ">". This is a crude way to locate an HTML tag with an attribute.
   Kids, don't parse HTML this way.
 */
static int32 find_text_pair_in_file(void *story_file, int32 extent, char *str, char *str2)
{
    int len = strlen(str);
    int len2 = strlen(str2);
    int32 ix, jx;

    if (len == 0) {
        return -1;
    }
    
    for (ix=0; ix<extent-len; ix++) {
        if (strncasecmp(story_file+ix, str, len) == 0) {
            for (jx=ix+len; jx<extent-len2; jx++) {
                char ch = *(char *)(story_file+jx);
                if (ch == '<' || ch == '>')
                    break;
                if (strncasecmp(story_file+jx, str2, len2) == 0) {
                    return ix;
                }
            }
        }
    }

    return -1;
}

static int32 find_attribute_value(void *story_file, int32 extent, char *output, int32 output_extent, int32 pos, char* attribute_prefix) {
    void *starttag = story_file + pos;
    void *endtag = memchr(starttag, '>', extent-pos);
    if (endtag) {
        int32 attrpos = find_text_in_file(starttag, endtag-starttag, attribute_prefix);
        if (attrpos != -1) {
            attrpos += strlen(attribute_prefix);
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
    /* Couldn't find the attribute. */
    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
    int32 ix;
    int32 pos = find_text_pair_in_file(story_file, extent, "<meta", "property=\"ifiction:ifid\"");
    if (pos != -1) {
        return find_attribute_value(story_file, extent, output, output_extent, pos, "content=\"");
    }

    /* UUID style */
    for (ix=0; ix<extent-7; ix++) {
        if (memcmp((char *)story_file+ix, "UUID://", 7) == 0) {
            int32 jx;
            for (jx=ix+7; jx<extent && ((char *)story_file)[jx]!='/'; jx++);
            if (jx < extent) {
                ix += 7;
                ASSERT_OUTPUT_SIZE(jx-ix+1);
                memcpy(output, (char *)story_file+ix, jx-ix);
                output[jx-ix]=0;
                return VALID_STORY_FILE_RV;
            }
            break;
        }
    }
    
    /* Twine 2 */
    pos = find_text_in_file(story_file, extent, "<tw-storydata");
    if (pos != -1) {
        return find_attribute_value(story_file, extent, output, output_extent, pos, "ifid=\"");
    }

    /* Generate IFID from MD5 */
    ASSERT_OUTPUT_SIZE(8);
    strcpy(output,"HTML-");
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

    /* every string counts as invalid HTML, but these are pretty definitive */
    if (find_text_in_file(story_file, extent, "<html") != -1) {
        return VALID_STORY_FILE_RV;
    }
    if (find_text_in_file(story_file, extent, "<!doctype html") != -1) {
        return VALID_STORY_FILE_RV;
    }
    if (find_text_pair_in_file(story_file, extent, "<meta", "property=\"ifiction:ifid\"") != -1) {
        return VALID_STORY_FILE_RV;
    }
    return INVALID_STORY_FILE_RV;
}
