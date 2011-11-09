/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <png.h>
#include <jpeglib.h>

#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

#define giblorb_ID_JPEG      (giblorb_make_id('J', 'P', 'E', 'G'))
#define giblorb_ID_PNG       (giblorb_make_id('P', 'N', 'G', ' '))

static void load_image_png(FILE *fl, picture_t *pic);
static void load_image_jpeg(FILE *fl, picture_t *pic);

static piclist_t *picstore = NULL;	/* cache all loaded pictures */
static int gli_piclist_refcount = 0;	/* count references to loaded pictures */

static void gli_picture_discard(picture_t *pic);

piclist_t *gli_piclist_search(unsigned long id)
{
    piclist_t *picptr;
    picture_t *pic;

    picptr = picstore;

    while (picptr != NULL)
    {
        pic = picptr->picture;

        if (pic && pic->id == id)
            return picptr;

        picptr = picptr->next;
    }

    return NULL;
}

void gli_piclist_clear(void)
{
    piclist_t *picptr, *tmpptr;

    picptr = picstore;

    while (picptr != NULL)
    {
        tmpptr = picptr;
        picptr = picptr->next;

        gli_picture_discard(tmpptr->picture);
        gli_picture_discard(tmpptr->scaled);

        free(tmpptr);
    }

    picstore = NULL;
}

void gli_piclist_increment(void)
{
    gli_piclist_refcount++;
}

void gli_piclist_decrement(void)
{
    if (gli_piclist_refcount > 0 && --gli_piclist_refcount == 0)
        gli_piclist_clear();
}

void gli_picture_store_original(picture_t *pic)
{
    piclist_t *newpic = malloc(sizeof(piclist_t));
    piclist_t *picptr;

    newpic->picture = pic;
    newpic->scaled = NULL;
    newpic->next = NULL;

    if (!picstore)
    {
        picstore = newpic;
        return;
    }

    picptr = picstore;

    while (picptr->next != NULL)
        picptr = picptr->next;

    picptr->next = newpic;
}

void gli_picture_store_scaled(picture_t *pic)
{
    piclist_t *picptr;

    picptr = gli_piclist_search(pic->id);

    if (!picptr)
        return;

    if (picptr->scaled)
        gli_picture_discard(picptr->scaled);

    picptr->scaled = pic;
}

void gli_picture_store(picture_t *pic)
{
    if (!pic)
        return;

    if (!pic->scaled)
        gli_picture_store_original(pic);
    else
        gli_picture_store_scaled(pic);
}

picture_t *gli_picture_retrieve(unsigned long id, int scaled)
{
    piclist_t *picptr;
    picture_t *pic;

    picptr = picstore;

    while (picptr != NULL)
    {
        if (!scaled)
            pic = picptr->picture;
        else
            pic = picptr->scaled;

        if (pic && pic->id == id)
            return pic;

        picptr = picptr->next;
    }

    return NULL;
}

static void gli_picture_discard(picture_t *pic)
{
    if (!pic)
        return;

    if (pic->rgba)
        free(pic->rgba);

    free(pic);
}

picture_t *gli_picture_load(unsigned long id)
{
    picture_t *pic;
    FILE *fl;
    int closeafter;
    glui32 chunktype;

    pic = gli_picture_retrieve(id, 0);

    if (pic)
        return pic;

    if (!giblorb_is_resource_map())
    {
        char filename[1024];
        unsigned char buf[8];

        sprintf(filename, "%s/PIC%ld", gli_workdir, id); 

        closeafter = TRUE;
        fl = fopen(filename, "rb");
        if (!fl)
            return NULL;

        if (fread(buf, 1, 8, fl) != 8)
        {
            /* Can't read the first few bytes. Forget it. */
            fclose(fl);
            return NULL;
        }

        if (!png_sig_cmp(buf, 0, 8))
        {
            chunktype = giblorb_ID_PNG;
        }
        else if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF)
        {
            chunktype = giblorb_ID_JPEG;
        }
        else
        {
            /* Not a readable file. Forget it. */
            fclose(fl);
            return NULL;
        }

        fseek(fl, 0, 0);
    }

    else
    {
        long pos;
        giblorb_get_resource(giblorb_ID_Pict, id, &fl, &pos, NULL, &chunktype);
        if (!fl)
            return NULL;
        fseek(fl, pos, 0);
        closeafter = FALSE;
    }

    pic = malloc(sizeof(picture_t));
    pic->refcount = 1;
    pic->w = 0;
    pic->h = 0;
    pic->rgba = NULL;
    pic->id = id;
    pic->scaled = FALSE;

    if (chunktype == giblorb_ID_PNG)
        load_image_png(fl, pic);

    if (chunktype == giblorb_ID_JPEG)
        load_image_jpeg(fl, pic);

    if (closeafter)
        fclose(fl);

    if (!pic->rgba)
    {
        free(pic);
        return NULL;
    }

    gli_picture_store(pic);

    return pic;
}

static void load_image_jpeg(FILE *fl, picture_t *pic)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW rowarray[1];
    JSAMPLE *row;
    unsigned char *p;
    int n, i;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fl);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    pic->w = cinfo.output_width;
    pic->h = cinfo.output_height;
    n = cinfo.output_components;
    pic->rgba = malloc(pic->w * pic->h * 4);

    p = pic->rgba;
    row = malloc(sizeof(JSAMPLE) * pic->w * n);
    rowarray[0] = row;

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, rowarray, 1);
        if (n == 1)
            for (i = 0; i < pic->w; i++)
            {
                *p++ = row[i]; *p++ = row[i]; *p++ = row[i];
                *p++ = 0xFF;
            }
        else if (n == 3)
            for (i = 0; i < pic->w; i++)
            {
                *p++ = row[i*3+0]; *p++ = row[i*3+1]; *p++ = row[i*3+2];
                *p++ = 0xFF;
            }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(row);
}

static void load_image_png(FILE *fl, picture_t *pic)
{
    int ix, x, y;
    int srcrowbytes;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;

    /* These are static so that the setjmp/longjmp error-handling of
       libpng doesn't mangle them. Horribly thread-unsafe, but we
       hope we don't run into that. */
    static png_bytep *rowarray;
    static png_bytep srcdata;

    rowarray = NULL;
    srcdata = NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        return;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        /* If we jump here, we had a problem reading the file */
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        if (rowarray)
            free(rowarray);
        if (srcdata)
            free(srcdata);
        return;
    }

    png_init_io(png_ptr, fl);

    png_read_info(png_ptr, info_ptr);

    pic->w = png_get_image_width(png_ptr, info_ptr);
    pic->h = png_get_image_height(png_ptr, info_ptr);

    png_set_strip_16(png_ptr);
    png_set_packing(png_ptr);
    png_set_expand(png_ptr);
    png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    srcrowbytes = png_get_rowbytes(png_ptr, info_ptr);

    assert(srcrowbytes == pic->w * 4 || srcrowbytes == pic->w * 3);

    rowarray = malloc(sizeof(png_bytep) * pic->h);
    srcdata = malloc(pic->w * pic->h * 4);

    for (ix=0; ix<pic->h; ix++)
        rowarray[ix] = srcdata + (ix * pic->w * 4);

    png_read_image(png_ptr, rowarray); 
    png_read_end(png_ptr, info_ptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    if (rowarray)
        free(rowarray);

    pic->rgba = srcdata;

    if (pic->w * 3 == srcrowbytes)
    {
        for (y = 0; y < pic->h; y++)
        {
            srcdata = pic->rgba + y * pic->w * 4;
            for (x = pic->w - 1; x >= 0; x--)
            {
                srcdata[x * 4 + 3] = 0xFF;
                srcdata[x * 4 + 2] = srcdata[x * 3 + 2];
                srcdata[x * 4 + 1] = srcdata[x * 3 + 1];
                srcdata[x * 4 + 0] = srcdata[x * 3 + 0];
            }
        }
    }
}

