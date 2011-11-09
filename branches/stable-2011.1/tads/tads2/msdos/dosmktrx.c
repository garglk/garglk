/* 
 *   Copyright (c) 1990, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  dosmktrx - DOS TRX builder
Function
  Combines the generic runtime module (TRX) with a game data file (.GAM) to
  produce a single file that contains the entire game (code + data).
Notes
  Define MKTRX_SETICON to include icon-setting code.  The icon-setting
  code is applicable only to 32-bit Windows executables (PE format files).
Modified
  11/16/92 MJRoberts     - add end signature
  11/11/91 MJRoberts     - creation
*/

#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Icon-setting code for PE (win32 exe) files 
 */
#ifdef MKTRX_SETICON

#include <Windows.h>

/*
 *   Icon resource file format structures 
 */

#pragma pack( push )
#pragma pack( 2 )

/* icon directory entry in ICO file */
typedef struct
{
    BYTE        bWidth;          // Width, in pixels, of the image
    BYTE        bHeight;         // Height, in pixels, of the image
    BYTE        bColorCount;     // Number of colors in image (0 if >=8bpp)
    BYTE        bReserved;       // Reserved ( must be 0)
    WORD        wPlanes;         // Color Planes
    WORD        wBitCount;       // Bits per pixel
    DWORD       dwBytesInRes;    // How many bytes in this resource?
    DWORD       dwImageOffset;   // Where in the file is this image?
} ICONDIRENTRY, *LPICONDIRENTRY;

/* icon directory in ICO file */
typedef struct
{
    WORD           idReserved;   // Reserved (must be 0)
    WORD           idType;       // Resource Type (1 for icons)
    WORD           idCount;      // How many images?
    ICONDIRENTRY   idEntries[1]; // An entry for each image (idCount of 'em)
} ICONDIR, *LPICONDIR;

/* group icon directory entry in EXE file */
typedef struct{
    BYTE   bWidth;               // Width, in pixels, of the image
    BYTE   bHeight;              // Height, in pixels, of the image
    BYTE   bColorCount;          // Number of colors in image (0 if >=8bpp)
    BYTE   bReserved;            // Reserved
    WORD   wPlanes;              // Color Planes
    WORD   wBitCount;            // Bits per pixel
    DWORD  dwBytesInRes;         // how many bytes in this resource?
    WORD   nID;                  // the ID
} GRPICONDIRENTRY, *LPGRPICONDIRENTRY;

/* group icon directory header in EXE file */
typedef struct {
    WORD            idReserved;   // Reserved (must be 0)
    WORD            idType;       // Resource type (1 for icons)
    WORD            idCount;      // How many images?
    GRPICONDIRENTRY idEntries[1]; // The entries for each image
} GRPICONDIR, *LPGRPICONDIR;

#pragma pack( pop )    


/* ------------------------------------------------------------------------ */
/*
 *   exit with an error 
 */
static void errexit(const char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

/*
 *   Resource directory information structure 
 */
typedef struct res_info_t res_info_t;
struct res_info_t
{
    /* seek address of the resource directory */
    DWORD  dir_pos;

    /* virtual address base of resource data */
    DWORD  base_addr;
};

/*
 *   Look up the resource directory information.  Fills in the res_info
 *   structure with the resource data.  Returns zero on success, or a
 *   non-zero error code on failure.  
 */
static int find_resource_dir(FILE *fp, res_info_t *res_info)
{
    IMAGE_DOS_HEADER doshdr;
    IMAGE_NT_HEADERS nthdr;
    DWORD cnt;
    DWORD lastsect;
    DWORD start;
    
    /* read the old-style DOS header */
    if (fread(&doshdr, sizeof(doshdr), 1, fp) != 1)
        return 1;
    if (doshdr.e_magic != IMAGE_DOS_SIGNATURE)
        return 2;

    /* seek to the PE header and read that */
    fseek(fp, doshdr.e_lfanew, SEEK_SET);
    if (fread(&nthdr, sizeof(nthdr), 1, fp) != 1)
        return 3;
    if (nthdr.Signature != IMAGE_NT_SIGNATURE)
        return 4;

    /* find the first image section */
    start = doshdr.e_lfanew
            + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader)
            + nthdr.FileHeader.SizeOfOptionalHeader;
    fseek(fp, start, SEEK_SET);

    /* read image sections */
    for (lastsect = 0, cnt = 0 ; cnt < nthdr.FileHeader.NumberOfSections ;
         ++cnt)
    {
        IMAGE_SECTION_HEADER imghdr;

        /* read this section header */
        if (fread(&imghdr, sizeof(imghdr), 1, fp) != 1)
            return 5;

        /* if this is the resource section, return it */
        if (memcmp(imghdr.Name, ".rsrc", IMAGE_SIZEOF_SHORT_NAME) == 0)
        {
            /* fill in the information */
            res_info->dir_pos = imghdr.PointerToRawData;
            res_info->base_addr = imghdr.VirtualAddress;

            /* return success */
            return 0;
        }
    }

    /* 
     *   we went through all the sections and failed to find a resource
     *   section - return an error 
     */
    return 6;
}

/*
 *   Get a file offset and size for a resource, given the resource type
 *   and either the identifier or the index.  If 'id_is_index' is true,
 *   the id is the 0-based index; otherwise, it's the ID.  Returns zero if
 *   the resource is not found, otherwise returns the seek offset in the
 *   file of the resource's data, and fills in *data_size with the size of
 *   the data.  
 */
static long find_resource(FILE *fp, long *data_size,
                          const res_info_t *res_info,
                          LPSTR res_type, int res_id, int id_is_index)
{
    IMAGE_RESOURCE_DIRECTORY dirhdr;
    int i;
    int found;
    IMAGE_RESOURCE_DIRECTORY_ENTRY direntry;
    IMAGE_RESOURCE_DATA_ENTRY dataentry;

    /* read the root directory, which contains the type index */
    fseek(fp, res_info->dir_pos, SEEK_SET);
    if (fread(&dirhdr, sizeof(dirhdr), 1, fp) != 1)
        return 0;

    /* find the desired type */
    for (i = 0, found = FALSE ;
         i < dirhdr.NumberOfNamedEntries + dirhdr.NumberOfIdEntries ; ++i)
    {
        /* read the next entry */
        if (fread(&direntry, sizeof(direntry), 1, fp) != 1)
            return 0;

        /* check to see if it's the type we want */
        if (direntry.Id == (WORD)res_type)
        {
            found = TRUE;
            break;
        }
    }

    /* if we didn't find the type we want, give up */
    if (!found)
        return 0;

    /* we found the type index - read its directory header */
    fseek(fp, res_info->dir_pos + direntry.OffsetToDirectory, SEEK_SET);
    if (fread(&dirhdr, sizeof(dirhdr), 1, fp) != 1)
        return 0;

    /* now search the directory for our index or ID */
    for (i = 0, found = FALSE ;
         i < dirhdr.NumberOfNamedEntries + dirhdr.NumberOfIdEntries ; ++i)
    {
        /* read the next entry */
        if (fread(&direntry, sizeof(direntry), 1, fp) != 1)
            return 0;

        /* if this is the one we want, return it */
        if ((id_is_index && res_id == i)
            || (!id_is_index && !direntry.NameIsString
                && direntry.Id == res_id))
        {
            found = TRUE;
            break;
        }
    }

    /* if we didn't find it, give up */
    if (!found)
        return 0;

    /* 
     *   got it - this entry will point to yet another directory, which
     *   will contain the actual data; seek to the directory and read its
     *   first member 
     */
    fseek(fp, res_info->dir_pos + direntry.OffsetToDirectory, SEEK_SET);
    if (fread(&dirhdr, sizeof(dirhdr), 1, fp) != 1
        || fread(&direntry, sizeof(direntry), 1, fp) != 1)
        return 0;

    /* seek to and read the first directory entry's data header */
    fseek(fp, res_info->dir_pos + direntry.OffsetToData, SEEK_SET);
    if (fread(&dataentry, sizeof(dataentry), 1, fp) != 1)
        return 0;

    /* set the size from the directory entry */
    *data_size = dataentry.Size;

    /* 
     *   compute and return the data address - note that the address is
     *   relative to the resource section's virtual address setting, so we
     *   must adjust the virtual address to determine the seek address in
     *   the file 
     */
    return dataentry.OffsetToData - res_info->base_addr + res_info->dir_pos;
}

/*
 *   Replace the icon at icon_idx in the EXE file with the icon in the ICO
 *   file.  Returns zero on success, non-zero on error.  
 */
static int replace_icon(FILE *fpexe, int icon_idx, FILE *fpico)
{
    res_info_t res_info;
    long  group_pos;
    long  group_size;
    int   err = 0;
    int   i;
    GRPICONDIR group_dir;
    GRPICONDIRENTRY *group_dir_entry = 0;
    ICONDIR  icon_dir;
    ICONDIRENTRY  *icon_dir_entry = 0;
    ICONDIRENTRY  *cur_ico;

    /* seek to the start of the executable */
    fseek(fpexe, 0, SEEK_SET);

    /* read the ICO directory header */
    if (fread(&icon_dir,
              sizeof(icon_dir) - sizeof(ICONDIRENTRY), 1, fpico) != 1)
    {
        err = 101;
        goto done;
    }

    /* read the ICO directory entries */
    icon_dir_entry = (ICONDIRENTRY *)malloc(icon_dir.idCount
                                            * sizeof(ICONDIRENTRY));
    if (icon_dir_entry == 0)
    {
        err = 102;
        goto done;
    }
    if (fread(icon_dir_entry, icon_dir.idCount * sizeof(ICONDIRENTRY),
              1, fpico) != 1)
    {
        err = 103;
        goto done;
    }

    /* find the resource directory in the EXE file */
    err = find_resource_dir(fpexe, &res_info);
    if (err != 0)
        goto done;

    /* look up the icon group resource by index */
    group_pos = find_resource(fpexe, &group_size, &res_info,
                              RT_GROUP_ICON, icon_idx, TRUE);

    /* seek to and read the group icon directory */
    fseek(fpexe, group_pos, SEEK_SET);
    if (fread(&group_dir, sizeof(group_dir) - sizeof(GRPICONDIRENTRY),
              1, fpexe) != 1)
    {
        err = 104;
        goto done;
    }

    /* read the group directory entries */
    group_dir_entry = (GRPICONDIRENTRY *)malloc(group_dir.idCount
                                                * sizeof(GRPICONDIRENTRY));
    if (group_dir_entry == 0)
    {
        err = 105;
        goto done;
    }
    if (fread(group_dir_entry, group_dir.idCount * sizeof(GRPICONDIRENTRY),
              1, fpexe) != 1)
    {
        err = 106;
        goto done;
    }

    /* 
     *   go through the image types in the replacement icon file; for each
     *   one, try to find the corresponding entry in the EXE file, and
     *   replace it 
     */
    for (i = 0, cur_ico = icon_dir_entry ; i < icon_dir.idCount ;
         ++i, ++cur_ico)
    {
        int j;
        GRPICONDIRENTRY *cur_grp;
        int found;

        /* try to find this icon type in the EXE group icon directory */
        for (found = FALSE, j = 0, cur_grp = group_dir_entry ;
             j < group_dir.idCount ; ++j, ++cur_grp)
        {
            /* see if it matches */
            if (cur_grp->bWidth == cur_ico->bWidth
                && cur_grp->bHeight == cur_ico->bHeight
                && cur_grp->bColorCount == cur_ico->bColorCount
                && cur_grp->dwBytesInRes == cur_ico->dwBytesInRes)
            {
                long  icon_pos;
                long  icon_size;
                long  rem;

                /* note that we found it */
                found = TRUE;

                /* look up this icon entry */
                icon_pos = find_resource(fpexe, &icon_size, &res_info,
                                         RT_ICON, cur_grp->nID, FALSE);
                if (icon_pos == 0)
                {
                    err = 107;
                    goto done;
                }

                /* seek to the icon resource in the EXE file */
                fseek(fpexe, icon_pos, SEEK_SET);

                /* seek to the icon image in the ICO file */
                fseek(fpico, cur_ico->dwImageOffset, SEEK_SET);

                /* copy data from the ICO file to the EXE file */
                for (rem = cur_ico->dwBytesInRes ; rem != 0 ; )
                {
                    size_t cur;
                    char   copybuf[512];

                    /* 
                     *   read up to the buffer size or the amount
                     *   remaining, whichever is less 
                     */
                    cur = sizeof(copybuf);
                    if (rem < (long)cur)
                        cur = (size_t)rem;
                    if (fread(copybuf, cur, 1, fpico) != 1)
                    {
                        err = 108;
                        goto done;
                    }

                    /* write it out to the EXE file */
                    if (fwrite(copybuf, cur, 1, fpexe) != 1)
                    {
                        err = 109;
                        goto done;
                    }

                    /* deduct the amount read from the total */
                    rem -= cur;
                }

                /* no need to look any further */
                break;
            }
        }

        /* if we didn't find it, report an error */
        if (!found)
        {
            /* 
             *   note the error, but keep looking anyway, because there
             *   may still be other resources that we can successfully
             *   use; this error is essentially just a warning that there
             *   were some icon types in the ICO file that we couldn't
             *   store into the EXE file 
             */
            err = 110;
        }
    }

done:
    /* free any memory we allocated */
    if (group_dir_entry != 0)
        free(group_dir_entry);
    if (icon_dir_entry != 0)
        free(icon_dir_entry);

    /* return the result */
    return err;
}


#endif /* MKTRX_SETICON

/* ------------------------------------------------------------------------ */
/*
 *   A couple of OS routines we lifted from osgen.c
 */

static void os_defext(char *fn, char *ext)
{
    char *p = fn + strlen(fn);
    while (p > fn)
    {
        --p;
        if (*p == '.') return;                  /* already has an extension */
        if (*p == '/' || *p == '\\' || * p== ':') break;    /* found a path */
    }
    strcat(fn, ".");                                           /* add a dot */
    strcat(fn, ext);                                   /* add the extension */
}

static void os_remext(char *fn)
{
    char *p = fn + strlen(fn);
    while (p > fn)
    {
        --p;
        if (*p == '.')
        {
            *p = '\0';
            return;
        }
        if (*p == '/' || *p == '\\' || *p == ':') return;
    }
}

static char *os_get_root_name(char *buf)
{
    char *rootname;

    /* scan the name for path separators */
    for (rootname = buf ; *buf != '\0' ; ++buf)
    {
        /* if this is a path separator, remember it */
        if (*buf == '/' || *buf == '\\' || *buf == ':')
        {
            /* 
             *   It's a path separators - for now, assume the root will
             *   start at the next character after this separator.  If we
             *   find another separator later, we'll forget about this one
             *   and use the later one instead.  
             */
            rootname = buf + 1;
        }
    }

    /* return the last root name candidate */
    return rootname;
}

/*
 *   Write a section header.  The section header consists of a check value
 *   that indicates that the seek position is correct, and a section type
 *   code.  
 *   
 *   The typecode must always be four bytes long.  
 */
static void write_header(FILE *fpout, const char *typecode)
{
    long comp;
    
    /* compute the check code */
    comp = ~ftell(fpout);

    /* write the check and the type code */
    if (fwrite(&comp, sizeof(comp), 1, fpout) != 1
        || fwrite(typecode, 4, 1, fpout) != 1)
    {
        fprintf(stderr, "error writing section header (%s)\n", typecode);
        exit(3);
    }
}

/*
 *   Write the end-of-section trailing header.  The trailing header
 *   consists of a check indicating its own seek location, so we can tell
 *   that the trailing header is indeed a trailing header; a type code;
 *   and the seek position of the leading data block header.
 *   
 *   The typecode must always be four bytes long.  
 */
static void write_trailing_header(FILE *fpout, long startofs,
                                  const char *typecode)
{
    long comp;
    
    /* compute the check */
    comp = ~ftell(fpout);

    /* write the trailing header */
    if (fwrite(&comp, sizeof(comp), 1, fpout) != 1
        || fwrite(typecode, 4, 1, fpout) != 1
        || fwrite(&startofs, sizeof(startofs), 1, fpout) != 1)
    {
        fprintf(stderr, "error writing trailing header (%s)\n", typecode);
        exit(3);
    }
}

/*
 *   A great big buffer for copying the files around.
 */
static char buf[30*1024];

static void append_file(FILE *fpout, FILE *fpin, char *typ, int put_size)
{
    long  startofs;

    /* remember where this starts */
    startofs = ftell(fpout);

    /* write the section header */
    write_header(fpout, typ);

    /* write the size prefix if desired */
    if (put_size)
    {
        long siz;

        /* seek to the end to get the size, then back to the start */
        fseek(fpin, 0L, SEEK_END);
        siz = ftell(fpin);
        fseek(fpin, 0L, SEEK_SET);

        /* write the size prefix */
        if (fwrite(&siz, sizeof(siz), 1, fpout) != 1)
        {
            fprintf(stderr, "error writing output file\n");
            exit(3);
        }
    }

    /* copy the data from the input file to the output file */
    for ( ;; )
    {
        size_t siz;
        
        if (!(siz = fread(buf, 1, sizeof(buf), fpin))) break;
        if (fwrite(buf, 1, siz, fpout) != siz)
        {
            fprintf(stderr, "error writing output file\n");
            exit(3);
        }
    }

    /* write the trailing descriptor block */
    write_trailing_header(fpout, startofs, typ);
}


int main(int argc, char **argv)
{
    FILE   *fptrx, *fpgam, *fpout;
    char    trxnam[_MAX_PATH];
    char    gamnam[_MAX_PATH];
    char    outnam[_MAX_PATH];
    char   *config_file;
    char   *argv0 = argv[0];
    size_t  siz;
    char   *p;
    int     curarg;
    char   *savext;
    int     use_html_exe = FALSE;
    int     use_t3_exe = FALSE;
    int     use_prot_exe = FALSE;
    int     use_win32_exe = FALSE;
    int     show_logo = TRUE;
    char   *charlib;
    char    charlibname[_MAX_PATH];
    char   *res_type = "TGAM";
#ifdef MKTRX_SETICON
    char    iconname[_MAX_PATH];
    int     set_icon = FALSE;
#endif

    /* presume we won't find a saved game extension option */
    savext = 0;

    /* presume we won't find a character map library */
    charlib = 0;

    /*
     *   Scan options 
     */
    for (curarg = 1 ; curarg < argc ; ++curarg)
    {
        /* if it's not an option, we're done */
        if (argv[curarg][0] != '-')
            break;

        /* check the option */
        if (stricmp(argv[curarg], "-savext") == 0)
        {
            /* the next argument is the saved game extension */
            ++curarg;
            if (curarg < argc)
                savext = argv[curarg];
        }
        else if (stricmp(argv[curarg], "-html") == 0)
        {
            /* note that we want to use the HTML TADS executable */
            use_html_exe = TRUE;
        }
        else if (stricmp(argv[curarg], "-prot") == 0)
        {
            /* note that we want to use the protected-mode run-time */
            use_prot_exe = TRUE;
        }
        else if (stricmp(argv[curarg], "-win32") == 0)
        {
            /* note that we want to use the 32-bit Windows run-time */
            use_win32_exe = TRUE;
        }
        else if (stricmp(argv[curarg], "-nologo") == 0)
        {
            /* hide the logo */
            show_logo = FALSE;
        }
        else if (stricmp(argv[curarg], "-t3") == 0)
        {
            /* use a T3 executable */
            use_t3_exe = TRUE;
        }
        else if (stricmp(argv[curarg], "-clib") == 0)
        {
            ++curarg;
            if (curarg < argc)
                charlib = argv[curarg];
        }
#ifdef MKTRX_SETICON
        else if (stricmp(argv[curarg], "-icon") == 0)
        {
            /* the next argument is the icon file */
            ++curarg;
            if (curarg < argc)
            {
                /* 
                 *   copy the name of the icon file and apply the .ICO
                 *   extension by default 
                 */
                strcpy(iconname, argv[curarg]);
                os_defext(iconname, "ico");

                /* note that we want to set the icon */
                set_icon = TRUE;
            }
        }
#endif
        else if (stricmp(argv[curarg], "-type") == 0)
        {
            /* the next argument is the type string */
            ++curarg;
            if (curarg < argc && strlen(argv[curarg]) == 4)
            {
                /* set the resource type name to this argument */
                res_type = argv[curarg];
            }
            else
                printf("usage error - "
                       "type must be exactly four characters\n");
        }
        else
        {
            /* display the usage message */
            curarg = argc;
            break;
        }
    }

    /*
     *   If there's a config file, get it now, and remove it from the
     *   arguments so the rest of the argument parser doesn't have to be
     *   bothered with it (which is important, since the rest of the
     *   argument parser is positional) 
     */
    if (curarg < argc && argv[curarg][0] == '@')
    {
        config_file = &argv[curarg][1];
        ++curarg;
    }
    else
        config_file = 0;

    /* show the banner, unless we've been asked not to do so */
    if (show_logo)
    {
        printf("maketrx v3.0.0   ");
        printf("Copyright (c) 1992, 2000 by Michael J. Roberts.\n");
    }

    /* 
     *   check to make sure we have enough arguments remaining - we need
     *   at least one argument for the game filename 
     */
    if (curarg >= argc)
    {
        fprintf(stderr,
                "usage:  maketrx [options] [@config] [source] game [dest]\n"
                "  config = [optional] configuration file\n"
                "  source = [optional] TADS interpreter executable\n"
                "  game   = your compiled .gam or .t3 file\n"
                "  dest   = [optional] output .exe executable file\n"
                "\n"
                "If 'source' is not specified, the program will look for "
                "the interpreter in\n"
                "the same directory as MAKETRX.EXE.  If 'dest' is not "
                "specified, the program\n"
                "will use the same filename as the 'game' with the "
                "extension replaced\n"
                "with .EXE.\n"
                "\n"
                "Options:\n"
                "   -html          Use the HTML TADS 2 run-time (HTMLT2.EXE)\n"
#ifdef MKTRX_SETICON
                "   -icon icofile  Use the icon in 'icofile' for the desktop "
                "icon for the\n"
                "                  output file (for HTML TADS only)\n"
#endif
                "   -prot          Use the 16-bit DOS protected mode run-time"
                " (TRX.EXE)\n"
                "   -win32         Use the 32-bit Windows console-mode "
                "run-time (T2R32.EXE)\n"
                "   -savext ext    Use 'ext' as the saved game extension "
                "at run-time\n"
                "   -t3            Use TADS 3 executables (T3RUN/HTMLT3)\n"
                "   -clib resfile  Add 'resfile' as a CLIB (T3 character map "
                "library) section\n"
                "                  Note: -t3 implies '-clib "
                "charmap\\cmaplib.t3r' if charmap\\cmaplib.t3r exists\n"
               );
        exit(1);
    }

    /* 
     *   check for the run-time executable - if we have three more
     *   arguments, they've explicitly told us which run-time executable
     *   to use; otherwise, try to find the default executable 
     */
    if (curarg + 2 >= argc)
    {
        size_t len;
        
        /* 
         *   the run-time executable was not specified - get the implicit
         *   TR.EXE location from argv[0] if possible 
         */
        strcpy(trxnam, argv0);
        p = trxnam;
        if (strlen(p) == 0)
        {
            fprintf(stderr,
                    "error: you must specify the full path to TR.EXE as the\n"
                    "first argument (run this program again with no arguments"
                    "\nfor the usage message)\n");
            exit(1);
        }

        /* use first argument as the game */
        strcpy(gamnam, argv[curarg]);

        /* if there's a .t3 suffix on the input file, assume -t3 mode */
        if ((len = strlen(gamnam)) > 3
            && stricmp(gamnam + len - 3, ".t3") == 0)
            use_t3_exe = TRUE;

        /* find the end of the path prefix */
        for (p += strlen(p) - 1 ; p > trxnam
             && *p != '/' && *p != '\\' && *p != ':' ; --p);
        if (*p == '/' || *p == '\\' || *p == ':') ++p;

        /* copy the executable name */
        if (use_t3_exe)
        {
            /* build using the appropriate T3 executable */
            if (use_html_exe)
                strcpy(p, "htmlt3.exe");
            else
                strcpy(p, "t3run.exe");
        }
        else
        {
            /* use the appropriate TADS 2 executable */
            if (use_html_exe)
                strcpy(p, "htmlt2.exe");
            else if (use_prot_exe)
                strcpy(p, "trx.exe");
            else if (use_win32_exe)
                strcpy(p, "t2r32.exe");
            else
                strcpy(p, "tr.exe");
        }

        /* 
         *   if there's no directory path prefix at all, and the file we're
         *   looking for doesn't exist, then we must have an argv[0] that
         *   found this program on the PATH environment variable; scan the
         *   PATH for a directory containing the file we're looking for 
         */
        if (access(trxnam, 0) && p == trxnam)
        {
            char fullname[_MAX_PATH];
            char *path;
            
            /* get the PATH setting */
            path = getenv("PATH");

            /* if we found it, scan it */
            while (path != 0)
            {
                size_t len;
                
                /* scan for a semicolon */
                for (p = path ; *p != ';' && *p != '\0' ; ++p) ;

                /* 
                 *   get the length of this element, but limit it to our
                 *   buffer size, leaving space for the filename we need to
                 *   append (our filenames are all 8.3, so we need at most 12
                 *   characters including the dot, plus one for the null
                 *   terminator) 
                 */
                len = p - path;
                if (len > _MAX_PATH - 13)
                    len = _MAX_PATH - 13;

                /* check this path element if it's not empty */
                if (len != 0)
                {
                    /* extract the path element */
                    memcpy(fullname, path, len);

                    /* add a path separator if there isn't one already */
                    if (len != 0
                        && fullname[len-1] != '\\' && fullname[len-1] != '/')
                        fullname[len++] = '\\';
                    
                    /* append the name of the file we're looking for */
                    strcpy(fullname + len, trxnam);
                    
                    /* if this is the one, use this path */
                    if (!access(fullname, 0))
                    {
                        /* use this as the full interpreter path */
                        strcpy(trxnam, fullname);
                        
                        /* no need to look any further */
                        break;
                    }
                }
                    
                /* 
                 *   this isn't the one; if we found a semicolon, move to the
                 *   next path element, otherwise we're done 
                 */
                if (*p == ';')
                    path = p + 1;
                else
                    path = 0;
            }
        }

        /* if no destination is specified, use game (but strip extension) */
        if (curarg + 1 >= argc)
        {
            strcpy(outnam, argv[curarg]);
            os_remext(outnam);
        }
        else
            strcpy(outnam, argv[curarg + 1]);
    }
    else
    {
        /* get TR.EXE path from explicit first argument */
        strcpy(trxnam, argv[curarg]);
        strcpy(gamnam, argv[curarg + 1]);
        strcpy(outnam, argv[curarg + 2]);
        os_defext(trxnam, "exe");
    }

    /* apply default extensions */
    os_defext(trxnam, "exe");
    os_defext(gamnam, use_t3_exe ? "t3" : "gam");
    os_defext(outnam, "exe");
    
    if (!(fptrx = fopen(trxnam, "rb")))
    {
        fprintf(stderr, "unable to open program file \"%s\"\n", trxnam);
        exit(2);
    }
    if (!(fpgam = fopen(gamnam, "rb")))
    {
        fprintf(stderr, "unable to open .GAM file \"%s\"\n", gamnam);
        exit(2);
    }
    if (!(fpout = fopen(outnam, "w+b")))
    {
        fprintf(stderr, "unable to open output file \"%s\"\n", outnam);
        exit(2);
    }

    /* 
     *   if we're in t3 mode, and no CLIB file was specified, and we can
     *   find 'cmaplib.t3r' in the same directory as the original
     *   interpreter .exe file, add cmaplib.t3r to as the implicit CLIB
     *   section 
     */
    if (use_t3_exe && charlib == 0)
    {
        /* build the name of the implicit file */
        strcpy(charlibname, trxnam);
        strcpy(os_get_root_name(charlibname), "charmap\\cmaplib.t3r");
        if (!access(charlibname, 0))
            charlib = charlibname;
    }
    
    /* Copy the all of TR.EXE original to the output file */
    for ( ;; )
    {
        if (!(siz = fread(buf, 1, sizeof(buf), fptrx))) break;
        if (fwrite(buf, 1, siz, fpout) != siz)
        {
            fprintf(stderr, "error writing output file\n");
            exit(3);
        }
    }

    /* write the config file, if present */
    if (config_file)
    {
        FILE *fpconfig;

        /* open the config file */
        fpconfig = fopen(config_file, "rb");
        if (!fpconfig)
        {
            fprintf(stderr, "unable to open configuration file \"%s\"\n",
                    config_file);
            exit(2);
        }

        /* copy the config file to the output file */
        append_file(fpout, fpconfig, "RCFG", 1);
        fclose(fpconfig);
    }

    /* add the game file */
    append_file(fpout, fpgam, res_type, 0);

    /*
     *   If there's a save file extension, add the SAVX resource to
     *   specify the extension 
     */
    if (savext != 0)
    {
        long startofs;
        unsigned short len;

        /* remember the starting location */
        startofs = ftell(fpout);

        /* write the header */
        write_header(fpout, "SAVX");

        /* write the length of the extension, followed by the extension */
        len = strlen(savext);
        if (fwrite(&len, sizeof(len), 1, fpout) != 1
            || fwrite(savext, len, 1, fpout) != 1)
        {
            fprintf(stderr, "error writing output file\n");
            exit(3);
        }

        /* write the trailing header */
        write_trailing_header(fpout, startofs, "SAVX");
    }

    /* if there's a character library file, add it */
    if (charlib != 0)
    {
        FILE *fpchar;
        
        /* open the file */
        fpchar = fopen(charlib, "rb");

        /* make sure we got it */
        if (fpchar != 0)
        {
            /* append it */
            append_file(fpout, fpchar, "CLIB", 0);

            /* done with the file */
            fclose(fpchar);
        }
        else
        {
            fprintf(stderr, "unable to open character library (CLIB) file "
                    "\"%s\"\n", charlib);
        }
    }

#ifdef MKTRX_SETICON
    /*
     *   If they want to set the icon, do so now 
     */
    if (set_icon)
    {
        FILE   *fpico;
        int     err;

        /* open the icon file */
        fpico = fopen(iconname, "rb");
        if (fpico == 0)
        {
            /* report the error */
            fprintf(stderr, "error opening icon file \"%s\"\n", iconname);
        }
        else
        {
            /* 
             *   replace the icon - always replace the icon at index zero,
             *   since this is the icon used to represent the application
             *   on the desktop 
             */
            err = replace_icon(fpout, 0, fpico);
            if (err != 0)
            {
                if (err < 6)
                    fprintf(stderr,
                            "error replacing icon: .EXE file is invalid\n");
                else if (err <= 106)
                    fprintf(stderr,
                            "error replacing icon: .EXE file does not "
                            "contain any icon resources\n");
                else if (err == 107)
                    fprintf(stderr,
                            "error replacing icon: desktop icon is not "
                            "present in .EXE file\n");
                else if (err == 110)
                    fprintf(stderr,
                            "Error replacing icon: one or more icon formats "
                            "in this .ICO file are not present\n"
                            "in the .EXE file -- "
                            "these formats could not be replaced.\n");
                else
                    fprintf(stderr,
                            "error: icon could not be replaced in "
                            ".EXE file\n");
            }
        }
    }
#endif

    /* close all the files and terminate successfully */
    fclose(fptrx);
    fclose(fpgam);
    fclose(fpout);
    exit(0);
    return 0;
}


