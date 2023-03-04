// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2010 by Ben Cressey, Chris Spiegel.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <array>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// jpeglib.h requires these to be defined.
using std::size_t;
using std::FILE;

#include <jpeglib.h>
#include <png.h>

#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

#define giblorb_ID_JPEG      (giblorb_make_id('J', 'P', 'E', 'G'))
#define giblorb_ID_PNG       (giblorb_make_id('P', 'N', 'G', ' '))

static std::shared_ptr<picture_t> load_image_png(const std::vector<unsigned char> &buf, unsigned long id);
static std::shared_ptr<picture_t> load_image_jpeg(const std::vector<unsigned char> &buf, unsigned long id);

namespace {

struct LoadError : public std::runtime_error {
    LoadError(const std::string &format, const std::string &msg) : std::runtime_error(msg + " [" + format + "]") {
    }
};

struct PicturePair {
    std::shared_ptr<picture_t> picture;
    std::shared_ptr<picture_t> scaled;
};

}

static std::unordered_map<unsigned long, PicturePair> picstore;

static int gli_piclist_refcount = 0; // count references to loaded pictures

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

static void gli_picture_store_original(const std::shared_ptr<picture_t> &pic)
{
    picstore[pic->id] = PicturePair{pic, nullptr};
}

static void gli_picture_store_scaled(const std::shared_ptr<picture_t> &pic)
{
    try {
        auto &picpair = picstore.at(pic->id);
        picpair.scaled = pic;
    } catch (const std::out_of_range &) {
    }
}

void gli_picture_store(const std::shared_ptr<picture_t> &pic)
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
    glui32 chunktype;

    auto pic = gli_picture_retrieve(id, false);
    if (pic) {
        return pic;
    }

    std::vector<unsigned char> buf;

    if (giblorb_get_resource_map() == nullptr) {
        std::string filename = gli_workdir + "/PIC" + std::to_string(id);

        if (!garglk::read_file(filename, buf) || buf.size() < 8) {
            return nullptr;
        }

        if (png_sig_cmp(buf.data(), 0, 8) == 0) {
            chunktype = giblorb_ID_PNG;
        } else if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) {
            chunktype = giblorb_ID_JPEG;
        } else {
            // Not a readable file. Forget it.
            return nullptr;
        }
    } else {
        if (!giblorb_copy_resource(giblorb_ID_Pict, id, chunktype, buf)) {
            return nullptr;
        }
    }

    const std::unordered_map<int, std::function<std::shared_ptr<picture_t>(const std::vector<unsigned char> &, unsigned long)>> loaders = {
        {giblorb_ID_PNG, load_image_png},
        {giblorb_ID_JPEG, load_image_jpeg},
    };

    try {
        pic = loaders.at(chunktype)(buf, id);
        if (pic != nullptr) {
            gli_picture_store(pic);
            return pic;
        }
    } catch (const LoadError &e) {
        gli_strict_warning("unable to load image " + std::to_string(id) + ": " + e.what());
    } catch (const std::out_of_range &) {
    }

    return nullptr;
}

static std::shared_ptr<picture_t> load_image_jpeg(const std::vector<unsigned char> &buf, unsigned long id)
{
    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    std::array<JSAMPROW, 1> rowarray;
    int n;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = [](j_common_ptr cinfo) {
        std::array<char, JMSG_LENGTH_MAX> msg;
        cinfo->err->format_message(cinfo, msg.data());
        throw LoadError("jpeg", msg.data());
    };

    jpeg_create_decompress(&cinfo);
    auto jpeg_free = garglk::unique(&cinfo, jpeg_destroy_decompress);

    jpeg_mem_src(&cinfo, buf.data(), buf.size());
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    n = cinfo.output_components;

    // Currently Gargoyle only understands 1 or 3 components.
    if (n != 1 && n != 3) {
        return nullptr;
    }

    Canvas<4> rgba(cinfo.output_width, cinfo.output_height);

    std::vector<JSAMPLE> row(cinfo.output_width * n);
    rowarray[0] = row.data();

    while (cinfo.output_scanline < cinfo.output_height) {
        JDIMENSION y = cinfo.output_scanline;
        jpeg_read_scanlines(&cinfo, rowarray.data(), 1);
        if (n == 1) {
            for (int i = 0; i < rgba.width(); i++) {
                rgba[y][i] = Pixel<4>(row[i], row[i], row[i], 0xff);
            }
        } else if (n == 3) {
            for (int i = 0; i < rgba.width(); i++) {
                rgba[y][i] = Pixel<4>(row[i * 3 + 0], row[i * 3 + 1], row[i * 3 + 2], 0xff);
            }
        }
    }

    jpeg_finish_decompress(&cinfo);

    return std::make_shared<picture_t>(id, rgba, false);
}

static std::shared_ptr<picture_t> load_image_png(const std::vector<unsigned char> &buf, unsigned long id)
{
    png_image image;

    image.version = PNG_IMAGE_VERSION;
    image.opaque = nullptr;
    auto free_png = garglk::unique(&image, png_image_free);

    if (png_image_begin_read_from_memory(&image, buf.data(), buf.size()) == 0) {
        throw LoadError("png", image.message);
    }

    image.format = PNG_FORMAT_RGBA;
    Canvas<4> rgba(image.width, image.height);

    if (png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr) == 0) {
        throw LoadError("png", image.message);
    }

    auto pic = std::make_shared<picture_t>(id, rgba, false);

    return pic;
}
