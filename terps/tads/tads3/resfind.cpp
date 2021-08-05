/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  resfind.cpp - find a multimedia resource in a tads 2 or tads 3 game file
Function
  Searches a compiled tads 2 game file or a compiled tads 3 image file
  for a multimedia resource of a given name.  The caller doesn't have to
  know which tads version created the file; we'll sense the file type and
  parse it accordingly.

  This implementation is independent of any tads 2 or tads 3 subsystem
  except osifc, which it uses for portable file I/O and portable byte
  format conversions.
Notes
  
Modified
  09/24/01 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>

#include <os.h>
#include "t3std.h"
#include "resfind.h"

/* ------------------------------------------------------------------------ */
/* 
 *   tads 2 file signature 
 */
#define T2_SIG "TADS2 bin\012\015\032"

/* 
 *   tads 3 file signature 
 */
#define T3_SIG "T3-image\015\012\032"



/* ------------------------------------------------------------------------ */
/*
 *   Find a resource in a tads 2 game file 
 */
static int t2_find_res(osfildef *fp, const char *resname,
                       tads_resinfo *info)
{
    char buf[300];
    unsigned long startpos;
    size_t resname_len;
    int found;

    /* we haven't found what we're looking for yet */
    found = FALSE;

    /* note the length of the name we're seeking */
    resname_len = strlen(resname);

    /* 
     *   note the seek location of the start of the tads 2 game file stream
     *   within the file - if the game file is embedded in a larger file
     *   stream, the seek locations we find within the file are relative to
     *   this starting location 
     */
    startpos = osfpos(fp);

    /* 
     *   skip past the tads 2 file header (13 bytes for the signature, 7
     *   bytes for the version header, 2 bytes for the flags, 26 bytes for
     *   the timestamp) 
     */
    osfseek(fp, 13 + 7 + 2 + 26, OSFSK_CUR);

    /* 
     *   scan the sections in the file; stop on $EOF, and skip everything
     *   else but HTMLRES, which is the section type that 
     */
    for (;;)
    {
        unsigned long endofs;
        
        /* read the section type and next-section pointer */
        if (osfrb(fp, buf, 1)
            || osfrb(fp, buf + 1, (int)((unsigned char)buf[0] + 4)))
        {
            /* failed to read it - give up */
            return FALSE;
        }

        /* note the ending position of this section */
        endofs = t3rp4u(buf + 1 + (unsigned char)buf[0]);

        /* check the type */
        if (buf[0] == 7 && memcmp(buf+1, "HTMLRES", 7) == 0)
        {
            unsigned long entry_cnt;
            unsigned long i;
            
            /* 
             *   It's a multimedia resource block.  Read the index table
             *   header (which contains the number of entries and a reserved
             *   uint32).  
             */
            if (osfrb(fp, buf, 8))
                return FALSE;

            /* get the number of entries from the header */
            entry_cnt = t3rp4u(buf);

            /* read the entries */
            for (i = 0 ; i < entry_cnt ; ++i)
            {
                unsigned long res_ofs;
                unsigned long res_siz;
                unsigned short name_len;

                /* read the entry header */
                if (osfrb(fp, buf, 10))
                    return FALSE;

                /* parse the header */
                res_ofs = t3rp4u(buf);
                res_siz = t3rp4u(buf + 4);
                name_len = osrp2(buf + 8);

                /* read the entry's name */
                if (name_len > sizeof(buf) || osfrb(fp, buf, name_len))
                    return FALSE;

                /* 
                 *   if it matches the name we're looking for, note that we
                 *   found it 
                 */
                if (name_len == resname_len
                    && memicmp(resname, buf, name_len) == 0)
                {
                    /* 
                     *   note that we found it, and note its resource size
                     *   and offset in the return structure - but keep
                     *   scanning the rest of the directory, since we need
                     *   to know where the directory ends to know where the
                     *   actual resources begin 
                     */
                    found = TRUE;
                    info->seek_pos = res_ofs;
                    info->siz = res_siz;
                }
            }

            /* 
             *   if we found our resource, the current seek position is the
             *   base of the offset we found in the directory; so fix up the
             *   offset to give the actual file location 
             */
            if (found)
            {
                /* fix up the offset with the actual file location */
                info->seek_pos += osfpos(fp);

                /* tell the caller we found it */
                return TRUE;
            }

            /* we didn't find it - seek to the end of this section */
            osfseek(fp, endofs + startpos, OSFSK_SET);
        }
        else if (buf[0] == 4 && memcmp(buf+1, "$EOF", 4) == 0)
        {
            /* 
             *   that's the end of the file - we've finished without finding
             *   the resource, so return failure 
             */
            return FALSE;
        }
        else
        {
            /* 
             *   this isn't a section we're interested in - skip to the end
             *   of the section and keep going
             */
            osfseek(fp, endofs + startpos, OSFSK_SET);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a resource in a T3 image file
 */
static int t3_find_res(osfildef *fp, const char *resname,
                       tads_resinfo *info)
{
    char buf[256];
    size_t resname_len;
    
    /* note the length of the name we're seeking */
    resname_len = strlen(resname);

    /* 
     *   skip the file header - 11 bytes for the signature, 2 bytes for the
     *   format version, 32 reserved bytes, and 24 bytes for the timestamp 
     */
    osfseek(fp, 11 + 2 + 32 + 24, OSFSK_CUR);

    /* scan the data blocks */
    for (;;)
    {
        unsigned long siz;
        
        /* read the block header */
        if (osfrb(fp, buf, 10))
            return FALSE;

        /* get the block size */
        siz = t3rp4u(buf + 4);

        /* check the type */
        if (memcmp(buf, "MRES", 4) == 0)
        {
            unsigned long base_ofs;
            unsigned int entry_cnt;
            unsigned int i;

            /* 
             *   remember the current seek position - the data seek location
             *   for each index entry is given as an offset from this
             *   location 
             */
            base_ofs = osfpos(fp);

            /* read the number of entries */
            if (osfrb(fp, buf, 2))
                return FALSE;

            /* parse the entry count */
            entry_cnt = osrp2(buf);

            /* read the entries */
            for (i = 0 ; i < entry_cnt ; ++i)
            {
                unsigned long entry_ofs;
                unsigned long entry_siz;
                unsigned int entry_name_len;
                char *p;
                size_t rem;

                /* read this index entry's header */
                if (osfrb(fp, buf, 9))
                    return FALSE;

                /* parse the header */
                entry_ofs = t3rp4u(buf);
                entry_siz = t3rp4u(buf + 4);
                entry_name_len = (unsigned char)buf[8];

                /* read the entry's name */
                if (osfrb(fp, buf, entry_name_len))
                    return FALSE;

                /* XOR the bytes of the name with 0xFF */
                for (p = buf, rem = entry_name_len ; rem != 0 ; ++p, --rem)
                    *p ^= 0xFF;

                /* if this is the one we're looking for, return it */
                if (entry_name_len == resname_len
                    && memicmp(resname, buf, resname_len) == 0)
                {
                    /* 
                     *   fill in the return information - note that the
                     *   entry offset given in the header is an offset from
                     *   data block's starting location, so fix this up to
                     *   an absolute seek location for the return value
                     */
                    info->seek_pos = base_ofs + entry_ofs;
                    info->siz = entry_siz;

                    /* return success */
                    return TRUE;
                }
            }
        }
        else if (memcmp(buf, "EOF ", 4) == 0)
        {
            /* 
             *   end of file - we've finished without finding the resource,
             *   so return failure 
             */
            return FALSE;
        }
        else
        {
            /* 
             *   we don't care about anything else - just skip this block
             *   and keep going; to skip the block, simply seek ahead by the
             *   size of the block's contents as given in the block header 
             */
            osfseek(fp, siz, OSFSK_CUR);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a multimedia resource with the given name in the given file.  The
 *   file must be positioned at the start of the tads game file when we're
 *   invoked - this allows searching for a resource within a game file that
 *   is embedded in a larger file stream, since we don't care where within
 *   the osfildef stream the tads game file starts.
 *   
 *   Fills in the resource information structure with the seek offset and
 *   size of the resource in the file and returns true if the resource is
 *   found; returns false if a resource with the given name doesn't exist in
 *   the file.  
 */
int tads_find_resource_fp(osfildef *fp, const char *resname,
                          tads_resinfo *info)
{
    char buf[12];

    /* read the signature */
    if (!osfrb(fp, buf, 12))
    {
        /* seek back to the start of the header */
        osfseek(fp, -12, OSFSK_CUR);
        
        /* check which signature we have */
        if (memcmp(buf, T2_SIG, strlen(T2_SIG)) == 0)
        {
            /* it's a tads 2 game file - read it accordingly */
            return t2_find_res(fp, resname, info);
        }
        else if (memcmp(buf, T3_SIG, strlen(T3_SIG)) == 0)
        {
            /* it's a t3 image file - read it accordingly */
            return t3_find_res(fp, resname, info);
        }
    }

    /* 
     *   if we get here, it means either that we couldn't read the
     *   signature, or that we didn't recognize the signature - in either
     *   case, we can't parse the file at all, so we can't find the resource 
     */
    return FALSE;
}

/*
 *   Find a resource in a file, given the filename. 
 *   
 *   Fills in the resource information structure with the seek offset and
 *   size of the resource in the file and returns true if the resource is
 *   found; returns false if a resource with the given name doesn't exist in
 *   the file.  
 */
int tads_find_resource(const char *fname, const char *resname,
                       tads_resinfo *info)
{
    osfildef *fp;
    int found;

    /* open the file */
    if ((fp = osfoprb(fname, OSFTGAME)) == 0
        && (fp = osfoprb(fname, OSFTT3IMG)) == 0)
    {
        /* we couldn't open the file, so there's no resource to be found */
        return FALSE;
    }

    /* find the resource in the file */
    found = tads_find_resource_fp(fp, resname, info);

    /* we're done with the file - close it */
    osfcls(fp);

    /* return our found or not-found indication */
    return found;
}

/* ------------------------------------------------------------------------ */
/*
 *   Testing main - looks for a given resource in a given file, and copies
 *   it to standard output if found.  
 */
#ifdef TEST

int main(int argc, char **argv)
{
    osfildef *fp;
    const char *fname;
    const char *resname;
    tads_resinfo res_info;
    unsigned long rem;
    
    /* check usage */
    if (argc != 3)
    {
        fprintf(stderr, "usage: resfind <filename> <resname>\n");
        exit(2);
    }

    /* get the arguments */
    fname = argv[1];
    resname = argv[2];

    /* open the file */
    if ((fp = osfoprb(fname, OSFTGAME)) == 0
        && (fp = osfoprb(fname, OSFTT3IMG)) == 0)
    {
        fprintf(stderr, "unable to open file \"%s\"\n", fname);
        exit(2);
    }

    /* find the resource */
    if (!tads_find_resource_fp(fp, resname, &res_info))
    {
        fprintf(stderr, "unable to find resource \"%s\"\n", resname);
        osfcls(fp);
        exit(1);
    }

    /* seek to the resource */
    osfseek(fp, res_info.seek_pos, OSFSK_SET);

    /* copy the resource to standard output */
    for (rem = res_info.siz ; rem != 0 ; )
    {
        char buf[1024];
        size_t cur;

        /* 
         *   read up to the buffer size or up to the resource size
         *   remaining, whichever is smaller 
         */
        cur = sizeof(buf);
        if (cur > rem)
            cur = (size_t)rem;

        /* read the chunk */
        if (osfrb(fp, buf, cur))
        {
            fprintf(stderr, "error reading %u bytes from file\n",
                    (unsigned)cur);
            osfcls(fp);
            exit(2);
        }

        /* copy the chunk to standard output */
        fwrite(buf, cur, 1, stdout);

        /* deduct the amount we just read from the size remaining */
        rem -= cur;
    }

    /* done with the file - close it */
    osfcls(fp);

    /* success */
    exit(0);
    return 0;
}


#endif /* TEST */
