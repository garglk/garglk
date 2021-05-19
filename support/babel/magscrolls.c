/* magscrolls.c  Treaty of Babel module for Z-code files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT magscrolls
#define HOME_PAGE "http://www.if-legends.org/~msmemorial/memorial.htm"
#define FORMAT_EXT ".mag"
#define NO_COVER
#define NO_METADATA

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

struct maginfo
{
  int gv;
  char header[21];
  char *title;
  int bafn;
  int year;
  char *ifid;
  char *author;
  char *meta;
};


static struct maginfo manifest[] = {
        { 0, "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",
          "The Pawn",
          0,
          1985,
          "MAGNETIC-1",
          "Rob Steggles",
        },
        { 1, "\000\004\000\001\007\370\000\000\340\000\000\000\041\064\000\000\040\160\000\000",
          "Guild of Thieves",
          0,
          1987,
          "MAGNETIC-2",
          "Rob Steggles",
        },
        { 2, "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",
          "Jinxter",
          0,
          1987,
          "MAGNETIC-3",
          "Georgina Sinclair and Michael Bywater",
        },
        { 4, "\000\004\000\001\045\140\000\001\000\000\000\000\161\017\000\000\035\210\000\001",
          "Corruption",
          0,
          1988,
          "MAGNETIC-4",
          "Rob Steggles and Hugh Steers",
        },
        { 4, "\000\004\000\001\044\304\000\001\000\000\000\000\134\137\000\000\040\230\000\001",
          "Fish!",
          0,
          1988,
          "MAGNETIC-5",
          "John Molloy, Pete Kemp, Phil South, Rob Steggles",
        },
        { 4, "\000\003\000\000\377\000\000\000\340\000\000\000\221\000\000\000\036\000\000\001",
          "Corruption",
          0,
          1988,
          "MAGNETIC-4",
          "Rob Steggles and Hugh Steers",
        },
        { 4, "\000\003\000\001\000\000\000\000\340\000\000\000\175\000\000\000\037\000\000\001",
          "Fish!",
          0,
          1988,
          "MAGNETIC-5",
          "John Molloy, Pete Kemp, Phil South, Rob Steggles",
        },
        { 4, "\000\003\000\000\335\000\000\000\140\000\000\000\064\000\000\000\023\000\000\000",
          "Myth",
          0,
          1989,
          "MAGNETIC-6",
          "Paul Findley",
        },
        { 4, "\000\004\000\001\122\074\000\001\000\000\000\000\114\146\000\000\057\240\000\001",
          "Wonderland",
          0,
          1990,
          "MAGNETIC-7",
          "David Bishop",
        },
        { 0, "0", NULL, 0, 0, NULL, NULL }
        };

static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
 int i;
 unsigned char *sf=(unsigned char *)story_file;
 if (extent < 42) return INVALID_STORY_FILE_RV;

 for(i=0;manifest[i].title;i++)
  if ((sf[13]<3 && manifest[i].gv==sf[13]) || memcmp(manifest[i].header,sf+12,20)==0)
   {
    ASSERT_OUTPUT_SIZE(((int32) strlen(manifest[i].ifid)+1));
    strcpy(output,manifest[i].ifid);
    return 1;
   }
 strcpy(output,"MAGNETIC-");
 return INCOMPLETE_REPLY_RV;
}

static int32 claim_story_file(void *story_file, int32 extent)
{
 if (extent<42 ||
     memcmp(story_file,"MaSc",4)
    ) return INVALID_STORY_FILE_RV;
 return VALID_STORY_FILE_RV;
}

