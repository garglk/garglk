/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.                    *
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
#include <string.h>
#include <math.h>
#include "glk.h"
#include "garglk.h"

void gli_get_builtin_font(int idx, unsigned char **ptr, unsigned int *len);

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <math.h> /* for pow() */
#include "uthash.h" /* for kerning cache */

#define mul255(a,b) (((a) * ((b) + 1)) >> 8)
#define grayscale(r,g,b) ((30 * (r) + 59 * (g) + 11 * (b)) / 100)

#ifdef _WIN32
#define inline	__inline
#endif

typedef struct font_s font_t;
typedef struct bitmap_s bitmap_t;
typedef struct fentry_s fentry_t;
typedef struct kcache_s kcache_t;

struct bitmap_s
{
    int w, h, lsb, top, pitch;
    unsigned char *data;
};

struct fentry_s
{
    glui32 cid;
    int adv;
    bitmap_t glyph[GLI_SUBPIX];
};

struct kcache_s
{
    glui32 pair[2];
    int value;
    UT_hash_handle hh;
};

struct font_s
{
    FT_Face face;
    bitmap_t lowglyphs[256][GLI_SUBPIX];
    int lowadvs[256];
    unsigned char lowloaded[256/8];
    fentry_t *highentries;
    int num_highentries, alloced_highentries;
    int make_bold;
    int make_oblique;
    int kerned;
    kcache_t *kerncache;
};

/*
 * Globals
 */

static unsigned char gammamap[256];

static font_t gfont_table[8];

int gli_cellw = 8;
int gli_cellh = 8;

int gli_image_s = 0;
int gli_image_w = 0;
int gli_image_h = 0;
unsigned char *gli_image_rgb = NULL;

#if defined __APPLE__ || defined __EFL_4BPP__
static const int gli_bpp = 4;
#elif defined __EFL_1BPP__
static const int gli_bpp = 1;
#else
static const int gli_bpp = 3;
#endif

static FT_Library ftlib;
static FT_Matrix ftmat;

/*
 * Font loading
 */

static int touni(int enc)
{
    switch (enc)
    {
        case ENC_LIG_FI: return UNI_LIG_FI;
        case ENC_LIG_FL: return UNI_LIG_FL;
        case ENC_LSQUO: return UNI_LSQUO;
        case ENC_RSQUO: return UNI_RSQUO;
        case ENC_LDQUO: return UNI_LDQUO;
        case ENC_RDQUO: return UNI_RDQUO;
        case ENC_NDASH: return UNI_NDASH;
        case ENC_MDASH: return UNI_MDASH;
    }
    return enc;
}

static void gammacopy(unsigned char *dst, unsigned char *src, int n)
{
    while (n--)
        *dst++ = gammamap[*src++];
}

#define m28(x) ((x * 28) / 255)
#define m56(x) ((x * 56) / 255)
#define m85(x) ((x * 85) / 255)

static void gammacopy_lcd(unsigned char *dst, unsigned char *src, int w, int h, int pitch)
{
    const unsigned char zero[3] = { 0, 0, 0 };
    unsigned char *dp, *sp;
    int x, y;

    for (y = 0; y < h; y++)
    {
        sp = &src[y * pitch];
        dp = &dst[y * pitch];
        for (x = 0; x < w; x += 3)
        {
            const unsigned char *lf = x > 0 ? sp - 3 : zero;
            const unsigned char *rt = x < w - 3 ? sp + 3 : zero;
            unsigned char ct[3];
            ct[0] = gammamap[sp[0]];
            ct[1] = gammamap[sp[1]];
            ct[2] = gammamap[sp[2]];
            dp[0] = m28(lf[1]) + m56(lf[2]) + m85(ct[0]) + m56(ct[1]) + m28(ct[2]);
            dp[1] = m28(lf[2]) + m56(ct[0]) + m85(ct[1]) + m56(ct[2]) + m28(rt[0]);
            dp[2] = m28(ct[0]) + m56(ct[1]) + m85(ct[2]) + m56(rt[0]) + m28(rt[1]);
            sp += 3;
            dp += 3;
        }
    }
}

static int findhighglyph(glui32 cid, fentry_t *entries, int length)
{
    int start = 0, end = length, mid = 0;
    while (start < end)
    {
        mid = (start + end) / 2;
        if (entries[mid].cid == cid)
            return mid;
        else if (entries[mid].cid < cid)
            start = mid + 1;
        else
            end = mid;
    }
    return ~mid;
}

static void loadglyph(font_t *f, glui32 cid)
{
    FT_Vector v;
    int err;
    glui32 gid;
    int x;
    bitmap_t glyphs[GLI_SUBPIX];
    int adv;

    gid = FT_Get_Char_Index(f->face, cid);
    if (gid <= 0)
        gid = FT_Get_Char_Index(f->face, '?');

    for (x = 0; x < GLI_SUBPIX; x++)
    {
        v.x = (x * 64) / GLI_SUBPIX;
        v.y = 0;

        FT_Set_Transform(f->face, 0, &v);

        err = FT_Load_Glyph(f->face, gid,
                FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
        if (err)
            winabort("FT_Load_Glyph");

        if (f->make_bold)
            FT_Outline_Embolden(&f->face->glyph->outline, FT_MulFix(f->face->units_per_EM, f->face->size->metrics.y_scale) / 24);

        if (f->make_oblique)
            FT_Outline_Transform(&f->face->glyph->outline, &ftmat);

        if (gli_conf_lcd)
            err = FT_Render_Glyph(f->face->glyph, FT_RENDER_MODE_LCD);
        else
            err = FT_Render_Glyph(f->face->glyph, FT_RENDER_MODE_LIGHT);
        if (err)
            winabort("FT_Render_Glyph");

        adv = (f->face->glyph->advance.x * GLI_SUBPIX + 32) / 64;

        glyphs[x].lsb = f->face->glyph->bitmap_left;
        glyphs[x].top = f->face->glyph->bitmap_top;
        glyphs[x].w = f->face->glyph->bitmap.width;
        glyphs[x].h = f->face->glyph->bitmap.rows;
        glyphs[x].pitch = f->face->glyph->bitmap.pitch;
        glyphs[x].data =
            malloc(glyphs[x].pitch * glyphs[x].h);
                if (gli_conf_lcd)
                    gammacopy_lcd(glyphs[x].data,
                            f->face->glyph->bitmap.buffer,
                            glyphs[x].w, glyphs[x].h, glyphs[x].pitch);
                else
                    gammacopy(glyphs[x].data,
                            f->face->glyph->bitmap.buffer,
                            glyphs[x].pitch * glyphs[x].h);
    }

    if (cid < 256)
    {
        f->lowloaded[cid/8] |= (1 << (cid%8));
        f->lowadvs[cid] = adv;
        memcpy(f->lowglyphs[cid], glyphs, sizeof glyphs);
    }
    else
    {
        int idx = findhighglyph(cid, f->highentries, f->num_highentries);
        if (idx < 0)
        {
            idx = ~idx;

            /* make room if needed */
            if (f->alloced_highentries == f->num_highentries)
            {
                fentry_t *newentries;
                int newsize = f->alloced_highentries * 2;
                if (!newsize)
                    newsize = 2;
                newentries = malloc(newsize * sizeof(fentry_t));
                if (!newentries)
                    return;
                if (f->highentries)
                {
                    memcpy(newentries, f->highentries, f->num_highentries * sizeof(fentry_t));
                    free(f->highentries);
                }
                f->highentries = newentries;
                f->alloced_highentries = newsize;
            }

            /* insert new glyph */
            memmove(&f->highentries[idx+1], &f->highentries[idx], (f->num_highentries - idx) * sizeof(fentry_t));
            f->highentries[idx].cid = cid;
            f->highentries[idx].adv = adv;
            memcpy(f->highentries[idx].glyph, glyphs, sizeof glyphs);
            f->num_highentries++;
        }
    }
}

static void loadfont(font_t *f, char *name, float size, float aspect, int style)
{
    static char *map[8] =
    {
        "LuxiMonoRegular",
        "LuxiMonoBold",
        "LuxiMonoOblique",
        "LuxiMonoBoldOblique",
        "CharterBT-Roman",
        "CharterBT-Bold",
        "CharterBT-Italic",
        "CharterBT-BoldItalic"
    };

    char afmbuf[1024];
    unsigned char *mem;
    unsigned int len;
    int err = 0;
    int i;

    memset(f, 0, sizeof (font_t));

#ifdef BUNDLED_FONTS
    for (i = 0; i < 8; i++)
    {
        if (!strcmp(name, map[i]))
        {
            gli_get_builtin_font(i, &mem, &len);
            err = FT_New_Memory_Face(ftlib, mem, len, 0, &f->face);
            if (err)
                winabort("FT_New_Face: %s: 0x%x", name, err);
            break;
        }
    }
#else
    i = 8;
#endif /* BUNDLED_FONTS */

    if (i == 8)
    {
        err = FT_New_Face(ftlib, name, 0, &f->face);
        if (err)
            winabort("FT_New_Face: %s: 0x%x", name, err);
        if (strstr(name, ".PFB") || strstr(name, ".PFA") ||
                strstr(name, ".pfb") || strstr(name, ".pfa"))
        {
            strcpy(afmbuf, name);
            strcpy(strrchr(afmbuf, '.'), ".afm");
            FT_Attach_File(f->face, afmbuf);
            strcpy(afmbuf, name);
            strcpy(strrchr(afmbuf, '.'), ".AFM");
            FT_Attach_File(f->face, afmbuf);
        }
    }

    err = FT_Set_Char_Size(f->face, size * aspect * 64, size * 64, 72, 72);
    if (err)
        winabort("FT_Set_Char_Size: %s", name);

    err = FT_Select_Charmap(f->face, ft_encoding_unicode);
    if (err)
        winabort("FT_Select_CharMap: %s", name);

    memset(f->lowloaded, 0, sizeof f->lowloaded);
    f->alloced_highentries = 0;
    f->num_highentries = 0;
    f->highentries = NULL;
    f->kerned = FT_HAS_KERNING(f->face);
    f->kerncache = NULL;

    switch (style)
    {
        case FONTR:
            f->make_bold = FALSE;
            f->make_oblique = FALSE;
            break;

        case FONTB:
            f->make_bold = !(f->face->style_flags & FT_STYLE_FLAG_BOLD);
            f->make_oblique = FALSE;
            break;

        case FONTI:
            f->make_bold = FALSE;
            f->make_oblique = !(f->face->style_flags & FT_STYLE_FLAG_ITALIC);
            break;

        case FONTZ:
            f->make_bold = !(f->face->style_flags & FT_STYLE_FLAG_BOLD);
            f->make_oblique = !(f->face->style_flags & FT_STYLE_FLAG_ITALIC);
            break;
    }
}

void gli_initialize_fonts(void)
{
    float monoaspect = gli_conf_monoaspect;
    float propaspect = gli_conf_propaspect;
    float monosize = gli_conf_monosize;
    float propsize = gli_conf_propsize;
    int err;
    int i;

    for (i = 0; i < 256; i++)
        gammamap[i] = pow(i / 255.0, gli_conf_gamma) * 255.0;

    err = FT_Init_FreeType(&ftlib);
    if (err)
        winabort("FT_Init_FreeType");

    /* replace built-in fonts with configured system font */
    fontload();
    fontreplace(gli_conf_monofont, MONOF);
    fontreplace(gli_conf_propfont, PROPF);
    fontunload();

    /* create oblique transform matrix */
    ftmat.xx = 0x10000L;
    ftmat.yx = 0x00000L;
    ftmat.xy = 0x03000L;
    ftmat.yy = 0x10000L;

    loadfont(&gfont_table[0], gli_conf_monor, monosize, monoaspect, FONTR);
    loadfont(&gfont_table[1], gli_conf_monob, monosize, monoaspect, FONTB);
    loadfont(&gfont_table[2], gli_conf_monoi, monosize, monoaspect, FONTI);
    loadfont(&gfont_table[3], gli_conf_monoz, monosize, monoaspect, FONTZ);

    loadfont(&gfont_table[4], gli_conf_propr, propsize, propaspect, FONTR);
    loadfont(&gfont_table[5], gli_conf_propb, propsize, propaspect, FONTB);
    loadfont(&gfont_table[6], gli_conf_propi, propsize, propaspect, FONTI);
    loadfont(&gfont_table[7], gli_conf_propz, propsize, propaspect, FONTZ);

    loadglyph(&gfont_table[0], '0');

    gli_cellh = gli_leading;
    gli_cellw = (gfont_table[0].lowadvs['0'] + GLI_SUBPIX - 1) / GLI_SUBPIX;
}

/*
 * Drawing
 */

void gli_draw_pixel(int x, int y, unsigned char alpha, unsigned char *rgb)
{
    unsigned char *p = gli_image_rgb + y * gli_image_s + x * gli_bpp;
    unsigned char invalf = 255 - alpha;
    if (x < 0 || x >= gli_image_w)
        return;
    if (y < 0 || y >= gli_image_h)
        return;
#ifdef WIN32
    p[0] = rgb[2] + mul255((short)p[0] - rgb[2], invalf);
    p[1] = rgb[1] + mul255((short)p[1] - rgb[1], invalf);
    p[2] = rgb[0] + mul255((short)p[2] - rgb[0], invalf);
#elif defined __APPLE__ || defined __EFL_4BPP__
    p[0] = rgb[2] + mul255((short)p[0] - rgb[2], invalf);
    p[1] = rgb[1] + mul255((short)p[1] - rgb[1], invalf);
    p[2] = rgb[0] + mul255((short)p[2] - rgb[0], invalf);
    p[3] = 0xFF;
#elif defined __EFL_1BPP__
    int gray = grayscale( rgb[0], rgb[1], rgb[2] );
    p[0] = gray + mul255((short)p[0] - gray, invalf);
#else
    p[0] = rgb[0] + mul255((short)p[0] - rgb[0], invalf);
    p[1] = rgb[1] + mul255((short)p[1] - rgb[1], invalf);
    p[2] = rgb[2] + mul255((short)p[2] - rgb[2], invalf);
#endif
}

void gli_draw_pixel_lcd(int x, int y, unsigned char *alpha, unsigned char *rgb)
{
    unsigned char *p = gli_image_rgb + y * gli_image_s + x * gli_bpp;
    unsigned char invalf[3];
        invalf[0] = 255 - alpha[0];
        invalf[1] = 255 - alpha[1];
        invalf[2] = 255 - alpha[2];
    if (x < 0 || x >= gli_image_w)
        return;
    if (y < 0 || y >= gli_image_h)
        return;
#ifdef WIN32
    p[0] = rgb[2] + mul255((short)p[0] - rgb[2], invalf[2]);
    p[1] = rgb[1] + mul255((short)p[1] - rgb[1], invalf[1]);
    p[2] = rgb[0] + mul255((short)p[2] - rgb[0], invalf[0]);
#elif defined __APPLE__ || defined __EFL_4BPP__
    p[0] = rgb[2] + mul255((short)p[0] - rgb[2], invalf[2]);
    p[1] = rgb[1] + mul255((short)p[1] - rgb[1], invalf[1]);
    p[2] = rgb[0] + mul255((short)p[2] - rgb[0], invalf[0]);
    p[3] = 0xFF;
#elif defined __EFL_1BPP__
    int gray = grayscale( rgb[0], rgb[1], rgb[2] );
    int invalfgray = grayscale( invalf[0], invalf[1], invalf[2] );
    p[0] = gray + mul255((short)p[0] - gray, invalfgray);
#else
    p[0] = rgb[0] + mul255((short)p[0] - rgb[0], invalf[0]);
    p[1] = rgb[1] + mul255((short)p[1] - rgb[1], invalf[1]);
    p[2] = rgb[2] + mul255((short)p[2] - rgb[2], invalf[2]);
#endif
}


static inline void draw_bitmap(bitmap_t *b, int x, int y, unsigned char *rgb)
{
    int i, k, c;
    for (k = 0; k < b->h; k++)
    {
        for (i = 0; i < b->w; i ++)
        {
            c = b->data[k * b->pitch + i];
            gli_draw_pixel(x + b->lsb + i, y - b->top + k, c, rgb);
        }
    }
}

static inline void draw_bitmap_lcd(bitmap_t *b, int x, int y, unsigned char *rgb)
{
    int i, j, k;
    for (k = 0; k < b->h; k++)
    {
        for (i = 0, j = 0; i < b->w; i += 3, j ++)
        {
            gli_draw_pixel_lcd(x + b->lsb + j, y - b->top + k, b->data + k * b->pitch + i, rgb);
        }
    }
}

void gli_draw_clear(unsigned char *rgb)
{
    unsigned char *p;
    int x, y;
    
#ifdef __EFL_1BPP__
    int gray = grayscale( rgb[0], rgb[1], rgb[2] );
#endif
    for (y = 0; y < gli_image_h; y++)
    {
        p = gli_image_rgb + y * gli_image_s;
        for (x = 0; x < gli_image_w; x++)
        {
#ifdef WIN32
            *p++ = rgb[2];
            *p++ = rgb[1];
            *p++ = rgb[0];
#elif defined __APPLE__ || defined __EFL_4BPP__
            *p++ = rgb[2];
            *p++ = rgb[1];
            *p++ = rgb[0];
            *p++ = 0xFF;
#elif defined __EFL_1BPP__
            *p++ = gray;
#else
            *p++ = rgb[0];
            *p++ = rgb[1];
            *p++ = rgb[2];
#endif
        }
    }
}

void gli_draw_rect(int x0, int y0, int w, int h, unsigned char *rgb)
{
    unsigned char *p0;
    int x1 = x0 + w;
    int y1 = y0 + h;
    int x, y;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;

    if (x0 > gli_image_w) x0 = gli_image_w;
    if (y0 > gli_image_h) y0 = gli_image_h;
    if (x1 > gli_image_w) x1 = gli_image_w;
    if (y1 > gli_image_h) y1 = gli_image_h;

    p0 = gli_image_rgb + y0 * gli_image_s + x0 * gli_bpp;

#ifdef __EFL_1BPP__
    int gray = grayscale( rgb[0], rgb[1], rgb[2] );
#endif
    for (y = y0; y < y1; y++)
    {
        unsigned char *p = p0;
        for (x = x0; x < x1; x++)
        {
#ifdef WIN32
            *p++ = rgb[2];
            *p++ = rgb[1];
            *p++ = rgb[0];
#elif defined __APPLE__ || defined __EFL_4BPP__
            *p++ = rgb[2];
            *p++ = rgb[1];
            *p++ = rgb[0];
            *p++ = 0xFF;
#elif defined __EFL_1BPP__
            *p++ = gray;
#else
            *p++ = rgb[0];
            *p++ = rgb[1];
            *p++ = rgb[2];
#endif
        }
        p0 += gli_image_s;
    }
}

static int charkern(font_t *f, int c0, int c1)
{
    FT_Vector v;
    int err;
    int g0, g1;

    if (!f->kerned)
        return 0;

    kcache_t *item = malloc(sizeof(kcache_t));
    memset(item, 0, sizeof(kcache_t));
    item->pair[0] = c0;
    item->pair[1] = c1;

    kcache_t *match = NULL;
    HASH_FIND(hh, f->kerncache, item->pair, 2 * sizeof(glui32), match);

    if (match)
    {
        free(item);
        return match->value;
    }

    g0 = FT_Get_Char_Index(f->face, touni(c0));
    g1 = FT_Get_Char_Index(f->face, touni(c1));

    if (g0 == 0 || g1 == 0)
        return 0;

    err = FT_Get_Kerning(f->face, g0, g1, FT_KERNING_UNFITTED, &v);
    if (err)
        winabort("FT_Get_Kerning");

    item->value = (v.x * GLI_SUBPIX) / 64.0;
    HASH_ADD_KEYPTR(hh, f->kerncache, item->pair, 2 * sizeof(glui32), item);

    return item->value;
}

static void getglyph(font_t *f, glui32 cid, int *adv, bitmap_t **glyphs)
{
    if (cid < 256)
    {
        if ((f->lowloaded[cid/8] & (1 << (cid%8))) == 0)
            loadglyph(f, cid);
        *adv = f->lowadvs[cid];
        *glyphs = f->lowglyphs[cid];
    }
    else
    {
        int idx = findhighglyph(cid, f->highentries, f->num_highentries);
        if (idx < 0)
        {
            loadglyph(f, cid);
            idx = ~idx;
        }
        *adv = f->highentries[idx].adv;
        *glyphs = f->highentries[idx].glyph;
    }
}

int gli_string_width(int fidx, unsigned char *s, int n, int spw)
{
    font_t *f = &gfont_table[fidx];
    int dolig = ! FT_IS_FIXED_WIDTH(f->face);
    int prev = -1;
    int w = 0;

    if ( FT_Get_Char_Index(f->face, UNI_LIG_FI) == 0 )
        dolig = 0;
    if ( FT_Get_Char_Index(f->face, UNI_LIG_FL) == 0 )
        dolig = 0;

    while (n--)
    {
        bitmap_t *glyphs;
        int adv;
        int c = touni(*s++);

        if (dolig && n && c == 'f' && *s == 'i')
        {
          c = UNI_LIG_FI;
          s++;
          n--;
        }
        if (dolig && n && c == 'f' && *s == 'l')
        {
          c = UNI_LIG_FL;
          s++;
          n--;
        }

        getglyph(f, c, &adv, &glyphs);

        if (prev != -1)
            w += charkern(f, prev, c);

        if (spw >= 0 && c == ' ')
            w += spw;
        else
            w += adv;

        prev = c;
    }

    return w;
}

int gli_draw_string(int x, int y, int fidx, unsigned char *rgb,
        unsigned char *s, int n, int spw)
{
    font_t *f = &gfont_table[fidx];
    int dolig = ! FT_IS_FIXED_WIDTH(f->face);
    int prev = -1;
    glui32 c;
    int px, sx;

    if ( FT_Get_Char_Index(f->face, UNI_LIG_FI) == 0 )
        dolig = 0;
    if ( FT_Get_Char_Index(f->face, UNI_LIG_FL) == 0 )
        dolig = 0;

    while (n--)
    {
        bitmap_t *glyphs;
        int adv;

        c = touni(*s++);

        if (dolig && n && c == 'f' && *s == 'i')
        {
          c = UNI_LIG_FI;
          s++;
          n--;
        }
        if (dolig && n && c == 'f' && *s == 'l')
        {
          c = UNI_LIG_FL;
          s++;
          n--;
        }

        getglyph(f, c, &adv, &glyphs);

        if (prev != -1)
            x += charkern(f, prev, c);

        px = x / GLI_SUBPIX;
        sx = x % GLI_SUBPIX;

                if (gli_conf_lcd)
                    draw_bitmap_lcd(&glyphs[sx], px, y, rgb);
                else
                    draw_bitmap(&glyphs[sx], px, y, rgb);

        if (spw >= 0 && c == ' ')
            x += spw;
        else
            x += adv;

        prev = c;
    }

    return x;
}

int gli_draw_string_uni(int x, int y, int fidx, unsigned char *rgb,
        glui32 *s, int n, int spw)
{
    font_t *f = &gfont_table[fidx];
    int dolig = ! FT_IS_FIXED_WIDTH(f->face);
    int prev = -1;
    glui32 c;
    int px, sx;

    if ( FT_Get_Char_Index(f->face, UNI_LIG_FI) == 0 )
        dolig = 0;
    if ( FT_Get_Char_Index(f->face, UNI_LIG_FL) == 0 )
        dolig = 0;

    while (n--)
    {
        bitmap_t *glyphs;
        int adv;

        c = *s++;

        if (dolig && n && c == 'f' && *s == 'i')
        {
          c = UNI_LIG_FI;
          s++;
          n--;
        }
        if (dolig && n && c == 'f' && *s == 'l')
        {
          c = UNI_LIG_FL;
          s++;
          n--;
        }

        getglyph(f, c, &adv, &glyphs);

        if (prev != -1)
            x += charkern(f, prev, c);

        px = x / GLI_SUBPIX;
        sx = x % GLI_SUBPIX;

                if (gli_conf_lcd)
                    draw_bitmap_lcd(&glyphs[sx], px, y, rgb);
                else
                    draw_bitmap(&glyphs[sx], px, y, rgb);

        if (spw >= 0 && c == ' ')
            x += spw;
        else
            x += adv;

        prev = c;
    }

    return x;
}

int gli_string_width_uni(int fidx, glui32 *s, int n, int spw)
{
    font_t *f = &gfont_table[fidx];
    int dolig = ! FT_IS_FIXED_WIDTH(f->face);
    int prev = -1;
    int w = 0;

    if ( FT_Get_Char_Index(f->face, UNI_LIG_FI) == 0 )
        dolig = 0;
    if ( FT_Get_Char_Index(f->face, UNI_LIG_FL) == 0 )
        dolig = 0;

    while (n--)
    {
        bitmap_t *glyphs;
        int adv;
        int c = *s++;

        if (dolig && n && c == 'f' && *s == 'i')
        {
          c = UNI_LIG_FI;
          s++;
          n--;
        }
        if (dolig && n && c == 'f' && *s == 'l')
        {
          c = UNI_LIG_FL;
          s++;
          n--;
        }

        getglyph(f, c, &adv, &glyphs);

        if (prev != -1)
            w += charkern(f, prev, c);

        if (spw >= 0 && c == ' ')
            w += spw;
        else
            w += adv;

        prev = c;
    }

    return w;
}

void gli_draw_caret(int x, int y)
{
    x = x / GLI_SUBPIX;
    if (gli_caret_shape == 0)
    {
        gli_draw_rect(x+0, y+1, 1, 1, gli_caret_color);
        gli_draw_rect(x-1, y+2, 3, 1, gli_caret_color);
        gli_draw_rect(x-2, y+3, 5, 1, gli_caret_color);
    }
    else if (gli_caret_shape == 1)
    {
        gli_draw_rect(x+0, y+1, 1, 1, gli_caret_color);
        gli_draw_rect(x-1, y+2, 3, 1, gli_caret_color);
        gli_draw_rect(x-2, y+3, 5, 1, gli_caret_color);
        gli_draw_rect(x-3, y+4, 7, 1, gli_caret_color);
    }
    else if (gli_caret_shape == 2)
        gli_draw_rect(x+0, y-gli_baseline+1, 1, gli_leading-2, gli_caret_color);
    else if (gli_caret_shape == 3)
        gli_draw_rect(x+0, y-gli_baseline+1, 2, gli_leading-2, gli_caret_color);
    else
        gli_draw_rect(x+0, y-gli_baseline+1, gli_cellw, gli_leading-2, gli_caret_color);
}

void gli_draw_picture(picture_t *src, int x0, int y0, int dx0, int dy0, int dx1, int dy1)
{
    unsigned char *sp, *dp;
    int x1, y1, sx0, sy0, sx1, sy1;
    int x, y, w, h;

    sx0 = 0;
    sy0 = 0;
    sx1 = src->w;
    sy1 = src->h;

    x1 = x0 + src->w;
    y1 = y0 + src->h;

    if (x1 <= dx0 || x0 >= dx1) return;
    if (y1 <= dy0 || y0 >= dy1) return;
    if (x0 < dx0)
    {
      sx0 += dx0 - x0;
      x0 = dx0;
    }
    if (y0 < dy0)
    {
      sy0 += dy0 - y0;
      y0 = dy0;
    }
    if (x1 > dx1)
    {
      sx1 += dx1 - x1;
      x1 = dx1;
    }
    if (y1 > dy1)
    {
      sy1 += dy1 - y1;
      y1 = dy1;
    }

    sp = src->rgba + (sy0 * src->w + sx0) * 4;
    dp = gli_image_rgb + y0 * gli_image_s + x0 * gli_bpp;

    w = sx1 - sx0;
    h = sy1 - sy0;

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            unsigned char sa = sp[x*4+3];
            unsigned char na = 255 - sa;
            unsigned char sr = mul255(sp[x*4+0], sa);
            unsigned char sg = mul255(sp[x*4+1], sa);
            unsigned char sb = mul255(sp[x*4+2], sa);
#ifdef __EFL_1BPP__
            unsigned char sgray = grayscale(sr, sg, sb);
#endif
#ifdef WIN32
            dp[x*3+0] = sb + mul255(dp[x*3+0], na);
            dp[x*3+1] = sg + mul255(dp[x*3+1], na);
            dp[x*3+2] = sr + mul255(dp[x*3+2], na);
#elif defined __APPLE__ || defined __EFL_4BPP__
            dp[x*4+0] = sb + mul255(dp[x*4+0], na);
            dp[x*4+1] = sg + mul255(dp[x*4+1], na);
            dp[x*4+2] = sr + mul255(dp[x*4+2], na);
            dp[x*4+3] = 0xFF;
#elif defined __EFL_1BPP__
            dp[x] = sgray + mul255(dp[x], na);
#else
            dp[x*3+0] = sr + mul255(dp[x*3+0], na);
            dp[x*3+1] = sg + mul255(dp[x*3+1], na);
            dp[x*3+2] = sb + mul255(dp[x*3+2], na);
#endif
        }
        sp += src->w * 4;
        dp += gli_image_s;
    }
}

