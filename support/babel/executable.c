/* executable.c  Treaty of Babel module for Z-code files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT executable
#define HOME_PAGE "http://http://en.wikipedia.org/wiki/Executable"
#define FORMAT_EXT ".exe"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

static char elfmagic[] = { 0x7f, 0x45, 0x4c, 0x46, 0 };
static char javamagic[] = { 0xCA, 0xFE, 0xBA, 0xBE, 0 };
static char amigamagic[] = { 0, 0, 3, 0xe7, 0 };
static char machomagic[] = { 0xFE, 0xED, 0xFA, 0xCE, 0};
struct exetype
{
 char *magic;
 char *name;
 int len;
};
static struct exetype magic[]= { { "MZ", "MZ", 2 },
                                 { elfmagic, "ELF", 4 },
                                 { javamagic, "JAVA", 4 },
                                 { amigamagic, "AMIGA", 4 },
                                 { "#! ", "SCRIPT", 3 },
                                 { machomagic, "MACHO",4 },
                                 { "APPL", "MAC",4 },
                                 { NULL, NULL, 0 } };

static char *deduce_magic(void *sf, int32 extent)
{
 int i;
 for(i=0;magic[i].magic;i++)
 if (extent >= magic[i].len && memcmp(magic[i].magic,sf,magic[i].len)==0)
  return magic[i].name;
 return NULL;
}
                                 
static int32 claim_story_file(void *sf, int32 extent)
{
 if (deduce_magic(sf,extent)) return VALID_STORY_FILE_RV;
 return NO_REPLY_RV;
}
static int32 get_story_file_IFID(void *sf, int32 extent, char *output, int32 output_extent)
{
 char *o;
 o=deduce_magic(sf,extent);
 if (!o) return 0;
 ASSERT_OUTPUT_SIZE((signed) strlen(o)+2);
 strcpy(output,o);
 strcat(output,"-");
 return INCOMPLETE_REPLY_RV;
}
