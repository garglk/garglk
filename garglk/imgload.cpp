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

#include <array>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <jpeglib.h>
#include <png.h>

#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

#define giblorb_ID_JPEG      (giblorb_make_id('J', 'P', 'E', 'G'))
#define giblorb_ID_PNG       (giblorb_make_id('P', 'N', 'G', ' '))

static std::shared_ptr<picture_t> load_image_png(std::FILE *fl, unsigned long id);
static std::shared_ptr<picture_t> load_image_jpeg(std::FILE *fl, unsigned long id);

struct PicturePair {
    std::shared_ptr<picture_t> picture;
    std::shared_ptr<picture_t> scaled;
};

std::map<unsigned long, PicturePair> picstore;

static int gli_piclist_refcount = 0; /* count references to loaded pictures */

void gli_piclist_increment()
{
    gli_piclist_refcount++;
}

void gli_piclist_decrement()
{
    if (gli_piclist_refcount > 0 && --gli_piclist_refcount == 0) {
        picstore.clear();
    }
}

static void gli_picture_store_original(std::shared_ptr<picture_t> pic)
{
    picstore[pic->id] = PicturePair{pic, nullptr};
}

static void gli_picture_store_scaled(std::shared_ptr<picture_t> pic)
{
    try {
        auto &picpair = picstore.at(pic->id);
        picpair.scaled = pic;
    } catch (const std::out_of_range &) {
    }
}

void gli_picture_store(std::shared_ptr<picture_t> pic)
{
    if (!pic) {
        return;
    }

    if (!pic->scaled) {
        gli_picture_store_original(pic);
    } else {
        gli_picture_store_scaled(pic);
    }
}

std::shared_ptr<picture_t> gli_picture_retrieve(unsigned long id, bool scaled)
{
    try {
        const auto &picpair = picstore.at(id);
        return scaled ? picpair.scaled : picpair.picture;
    } catch (const std::out_of_range &) {
        return nullptr;
    }
}

std::shared_ptr<picture_t> gli_picture_load(unsigned long id)
{
    std::unique_ptr<FILE, std::function<void(FILE *)>> fl;
    glui32 chunktype;

    auto pic = gli_picture_retrieve(id, false);
    if (pic) {
        return pic;
    }

    if (giblorb_get_resource_map() == nullptr) {
        std::array<unsigned char, 8> buf;
        std::string filename = gli_workdir + "/PIC" + std::to_string(id);

        fl = {std::fopen(filename.c_str(), "rb"), fclose};
        if (!fl) {
            return nullptr;
        }

        if (std::fread(buf.data(), 1, buf.size(), fl.get()) != buf.size()) {
            /* Can't read the first few bytes. Forget it. */
            return nullptr;
        }

        if (!png_sig_cmp(buf.data(), 0, buf.size())) {
            chunktype = giblorb_ID_PNG;
        } else if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) {
            chunktype = giblorb_ID_JPEG;
        } else {
            /* Not a readable file. Forget it. */
            return nullptr;
        }

        std::rewind(fl.get());
    }

    else {
        long pos;
        FILE *blorb_fl;
        giblorb_get_resource(giblorb_ID_Pict, id, &blorb_fl, &pos, nullptr, &chunktype);
        if (!blorb_fl) {
            return nullptr;
        }
        fl = {blorb_fl, [](FILE *) { return 0; }};
        std::fseek(fl.get(), pos, SEEK_SET);
    }

    const std::map<int, std::function<std::shared_ptr<picture_t>(FILE *, unsigned long)>> loaders = {
        {giblorb_ID_PNG, load_image_png},
        {giblorb_ID_JPEG, load_image_jpeg},
    };

    try {
        pic = loaders.at(chunktype)(fl.get(), id);
        if (pic != nullptr) {
            gli_picture_store(pic);
            return pic;
        }
    } catch (const std::out_of_range &) {
    }

    return nullptr;
}

static std::shared_ptr<picture_t> load_image_jpeg(std::FILE *fl, unsigned long id)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    std::array<JSAMPROW, 1> rowarray;
    int n, i;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = [](j_common_ptr cinfo) {
        throw cinfo->err;
    };

    try {
        jpeg_create_decompress(&cinfo);
        auto jpeg_free = garglk::unique(&cinfo, jpeg_destroy_decompress);

        jpeg_stdio_src(&cinfo, fl);
        jpeg_read_header(&cinfo, TRUE);
        jpeg_start_decompress(&cinfo);

        n = cinfo.output_components;

        auto pic = std::make_shared<picture_t>(id, cinfo.output_width, cinfo.output_height, false);

        std::vector<JSAMPLE> row(pic->w * n);
        rowarray[0] = row.data();

        while (cinfo.output_scanline < cinfo.output_height) {
            JDIMENSION y = cinfo.output_scanline;
            jpeg_read_scanlines(&cinfo, rowarray.data(), 1);
            if (n == 1) {
                for (i = 0; i < pic->w; i++) {
                    pic->rgba[y][i] = Pixel<4>(row[i], row[i], row[i], 0xff);
                }
            } else if (n == 3) {
                for (i = 0; i < pic->w; i++) {
                    pic->rgba[y][i] = Pixel<4>(row[i * 3 + 0], row[i * 3 + 1], row[i * 3 + 2], 0xff);
                }
            }
        }

        jpeg_finish_decompress(&cinfo);

        return pic;
    } catch (const jpeg_error_mgr *) {
        return nullptr;
    }
}

static std::shared_ptr<picture_t> load_image_png(std::FILE *fl, unsigned long id)
{
    png_image image;

    image.version = PNG_IMAGE_VERSION;
    image.opaque = nullptr;
    auto free_png = garglk::unique(&image, png_image_free);

    if (png_image_begin_read_from_stdio(&image, fl) == 0) {
        return nullptr;
    }

    image.format = PNG_FORMAT_RGBA;
    auto pic = std::make_shared<picture_t>(id, image.width, image.height, false);

    if (png_image_finish_read(&image, nullptr, pic->rgba.data(), 0, nullptr) == 0) {
        return nullptr;
    }

    return pic;
}
