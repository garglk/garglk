/* treaty_builder.h common macros to build a treaty module
 *
 * 2006 By L. Ross Raszewski
 *
 * This file is public domain, but be aware that any changes to it may
 * cause it to cease to be compliant with the Treaty of Babel.
 *
 * This file depends on treaty.h
 *
 * The purpose of this file is to simplify the building of a treaty
 * module.  It automatically generates a generic treaty function.
 *
 * Usage:
 *
 * #define the following values:
 *  FORMAT              The treaty name of the format
 *  HOME_PAGE           A string containing the URL of the format home page
 *  FORMAT_EXT          A string containing a comma separated list of common
 *                       extensions for game files in this format
 *  NO_METADATA         If the format does not support metadata
 *  NO_COVER            If the format does not support cover art
 *  CUSTOM_EXTENSION    If game files should not always use the first listed extension
 *
 * (Note: Formats which support metadata and cover art via a container should
 * define NO_METADATA and NO_COVER as container support is handled separately)
 *
 * #include "treaty_builder.h"
 * Define the following functions:
 *   static int32 get_story_file_IFID(void *, int32, char *, int32);
 *   static int32 claim_story_file(void *, int32);
 * Define the following functions if NO_METADATA is not defined:
 *   static int32 get_story_file_metadata_extent(void *, int32);
 *   static int32 get_story_file_metadata(void *, int32, char *, int32);
 * Define the following functions if NO_COVER is not defined
 *   static int32 get_story_file_cover_extent(void *, int32);
 *   static int32 get_story_file_cover_format(void *, int32);
 *   static int32 get_story_file_cover(void *, int32, void *, int32);
 * Define the following if CUSTOM_EXTENSION is defined
 *   static int32 get_story_file_extension(void *, int32, char *, int32);
 *
 * The two-parameter functions take the story file and story file extent
 * as parameters.  The four-parameter ones also take the output
 * buffer and its extent.  They perform the corresponding task to the
 * similarly-named selector.
 *
 * This file also defines the macro ASSERT_OUTPUT_SIZE(x) which
 * returns INVALID_USAGE_RV if output_extent is less than x.
 *
 * #define CONTAINER_FORMAT  before inclusion to generate a container
 * module.  A container module should define three additional functions:
 *    static int32 get_story_format(void *, int32, char *, int32);
 *    static int32 get_story_extent(void *, int32);
 *    static int32 get_story_file(void *, int32, void *, int32);
 *
 */

#ifndef TREATY_BUILDER
#define TREATY_BUILDER

#include "treaty.h"
#include <string.h>

#define ASSERT_OUTPUT_SIZE(x) do { if (output_extent < (x)) return INVALID_USAGE_RV; } while (0)

#ifndef NO_METADATA
static int32 get_story_file_metadata_extent(void *, int32);
static int32 get_story_file_metadata(void *, int32, char *, int32);
#endif
#ifndef NO_COVER
static int32 get_story_file_cover_extent(void *, int32);
static int32 get_story_file_cover_format(void *, int32);
static int32 get_story_file_cover(void *, int32, void *, int32);
#endif
static int32 get_story_file_IFID(void *, int32, char *, int32);
static int32 claim_story_file(void *, int32);
#ifdef CONTAINER_FORMAT
static int32 get_story_file(void *, int32, void *, int32);
static int32 get_story_format(void *, int32, char *, int32);
static int32 get_story_extent(void *, int32);
#endif
#ifdef CUSTOM_EXTENSION
static int32 get_story_file_extension(void *, int32, char *, int32);
#else
#include <stdio.h>
static int32 get_story_file_extension(void *sf, int32 extent, char *out, int32 output_extent)
{
 int i;

 if (!sf || !extent) return INVALID_STORY_FILE_RV;

 for(i=0;FORMAT_EXT[i] && FORMAT_EXT[i]!=',';i++);
 ASSERT_OUTPUT_SIZE(i+1);
 memcpy(out,FORMAT_EXT,i);
 out[i]=0;
 return strlen(out);
}

#endif

#define TREATY_FUNCTION(X)  DEEP_TREATY_FUNCTION(X)
#define DEEP_TREATY_FUNCTION(X) X ## _treaty
#define dSTRFRY(X) #X
#define STRFRY(X) dSTRFRY(X)

int32 TREATY_FUNCTION(FORMAT)(int32 selector,
                   void *story_file, int32 extent,
                   void *output, int32 output_extent)
{
 int32 ll, csf;
 if ((TREATY_SELECTOR_INPUT & selector) &&
     (csf=claim_story_file(story_file, extent))<NO_REPLY_RV)
         return INVALID_STORY_FILE_RV;
 

 if ((TREATY_SELECTOR_OUTPUT & selector) &&
     (output_extent ==0 ||
     output==NULL))
        return INVALID_USAGE_RV;
 switch(selector)
 {
  case GET_HOME_PAGE_SEL:
                ASSERT_OUTPUT_SIZE((signed) strlen(HOME_PAGE)+1);
                strcpy((char *) output,HOME_PAGE);
                return NO_REPLY_RV;
  case GET_FORMAT_NAME_SEL:
                ASSERT_OUTPUT_SIZE(512);
                strncpy((char *) output,STRFRY(FORMAT),output_extent-1);
                return NO_REPLY_RV;
  case GET_FILE_EXTENSIONS_SEL:
                ll=(strlen(FORMAT_EXT)+1) < 512 ? (strlen(FORMAT_EXT)+1):512;
                ASSERT_OUTPUT_SIZE(ll);
                strncpy((char *) output, FORMAT_EXT, output_extent);
                return NO_REPLY_RV;
  case CLAIM_STORY_FILE_SEL:
                return csf;
  case GET_STORY_FILE_METADATA_EXTENT_SEL:
#ifndef NO_METADATA
                return get_story_file_metadata_extent(story_file, extent);
#endif
  case GET_STORY_FILE_METADATA_SEL:
#ifndef NO_METADATA
                return get_story_file_metadata(story_file, extent, (char *)output, output_extent);
#else
                return NO_REPLY_RV;
#endif
  case GET_STORY_FILE_COVER_EXTENT_SEL:
#ifndef NO_COVER
                return get_story_file_cover_extent(story_file, extent);
#endif
  case GET_STORY_FILE_COVER_FORMAT_SEL:
#ifndef NO_COVER
                return get_story_file_cover_format(story_file, extent);
#endif
  case GET_STORY_FILE_COVER_SEL:
#ifndef NO_COVER
                return get_story_file_cover(story_file, extent, output, output_extent);
#else
                return NO_REPLY_RV;
#endif
  case GET_STORY_FILE_IFID_SEL:
                return get_story_file_IFID(story_file, extent, (char *)output, output_extent);
  case GET_STORY_FILE_EXTENSION_SEL:
                return get_story_file_extension(story_file, extent, (char *)output, output_extent);
#ifdef CONTAINER_FORMAT
  case CONTAINER_GET_STORY_FORMAT_SEL:
                return get_story_format(story_file, extent, (char *)output, output_extent);
  case CONTAINER_GET_STORY_EXTENT_SEL:
                return get_story_extent(story_file, extent);
  case CONTAINER_GET_STORY_FILE_SEL:
                return get_story_file(story_file, extent, output, output_extent);
#endif

 }
 return UNAVAILABLE_RV;
}


#else
#error "treaty_builder should be used as most once in any source file";
#endif
