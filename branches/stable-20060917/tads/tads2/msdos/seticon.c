#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/seticon.c,v 1.2 1999/05/17 02:52:19 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  seticon.c - change an icon in an executable to one in an .ICO file
Function
  Replaces an icon in a .EXE file with the icon in a .ICO file.  This
  function cannot add resources to an exectuable, but can only overwrite
  existing resources; hence, we can only replace an existing icon in the
  .EXE file with an icon with exactly the same characteristics.  If the
  .ICO file contains multiple types of icons (for example, a 16x16 and
  a 32x32), we'll replace only the types that are already stored in the
  .EXE file.
Notes
  
Modified
  05/29/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

/* ------------------------------------------------------------------------ */
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
int find_resource_dir(FILE *fp, res_info_t *res_info)
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
        DWORD endp;

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
long find_resource(FILE *fp, long *data_size, const res_info_t *res_info,
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
int replace_icon(FILE *fpexe, int icon_idx, FILE *fpico)
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

    /* look up the icon group rsource by index */
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

/* ------------------------------------------------------------------------ */
/*
 *   Test section 
 */

#if 0
int main(int argc, char **argv)
{
    FILE *fpexe;
    int   icon_idx;
    FILE *fpico;
    int   err;

    /* check arguments */
    if (argc != 4)
        errexit("usage: seticon <exe-file> <icon-index> <icon-file>");

    /* open the exe file */
    if ((fpexe = fopen(argv[1], "r+b")) == 0)
        errexit("error opening EXE file");

    /* get the icon index in the EXE file */
    icon_idx = atoi(argv[2]);

    /* open the ICO file */
    if ((fpico = fopen(argv[3], "rb")) == 0)
        errexit("error opening ICO file");

    /* go replace the icon */
    err = replace_icon(fpexe, icon_idx, fpico);
    if (err == 0)
        printf("icon replaced!\n");
    else
        printf("error from replace_icon: %d\n", err);

    /* done */
    fclose(fpico);
    fclose(fpexe);
    exit(0);
    return 0;
}

#endif
