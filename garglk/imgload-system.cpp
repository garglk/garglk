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

#include <memory>

#ifdef GARGLK_CONFIG_JPEG_TURBO
#include <turbojpeg.h>
#else
#include <array>
#include <csetjmp>
#include <cstdio>

// jpeglib.h requires these to be defined.
using std::size_t;
using std::FILE;

#include <jpeglib.h>
#endif

#include <png.h>

#include "imgload.h"

#include "garglk.h"

Canvas<4> gli_load_image_jpeg(const std::vector<unsigned char> &buf)
{
#ifdef GARGLK_CONFIG_JPEG_TURBO
    auto tj = garglk::unique(tjInitDecompress(), tjDestroy);

    int w, h, subsamp, colorspace;

    if (tjDecompressHeader3(tj.get(), buf.data(), buf.size(), &w, &h, &subsamp, &colorspace) != 0) {
        throw ImageLoadError("jpg", tjGetErrorStr2(tj.get()));
    }

    Canvas<4> rgba(w, h);

    if (colorspace == TJCS_CMYK || colorspace == TJCS_YCCK) {
        if (tjDecompress2(tj.get(), buf.data(), buf.size(), rgba.data(), w, w * tjPixelSize[TJPF_CMYK], h, TJPF_CMYK, TJFLAG_ACCURATEDCT) != 0) {
            throw ImageLoadError("jpg", tjGetErrorStr2(tj.get()));
        }

        auto *p = rgba.data();
        for (int i = 0; i < w * h; i++) {
            double K = p[i * 4 + 3];
            p[i * 4 + 0] = p[i * 4 + 0] * K / 255.0;
            p[i * 4 + 1] = p[i * 4 + 1] * K / 255.0;
            p[i * 4 + 2] = p[i * 4 + 2] * K / 255.0;
            p[i * 4 + 3] = 0xff;
        }
    } else {
        if (tjDecompress2(tj.get(), buf.data(), buf.size(), rgba.data(), w, w * tjPixelSize[TJPF_RGBA], h, TJPF_RGBA, TJFLAG_ACCURATEDCT) != 0) {
            throw ImageLoadError("jpg", tjGetErrorStr2(tj.get()));
        }
    }

    return rgba;
#else
    struct error_mgr {
        jpeg_error_mgr pub;
        std::jmp_buf jbuf;
        char msg[JMSG_LENGTH_MAX];
    };

    jpeg_decompress_struct cinfo{};
    error_mgr jerr;
    std::array<JSAMPROW, 1> rowarray;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = [](j_common_ptr cinfo) {
        auto *err = reinterpret_cast<error_mgr *>(cinfo->err);
        cinfo->err->format_message(cinfo, err->msg);
        std::longjmp(err->jbuf, 1);
    };

    auto jpeg_free = garglk::unique(&cinfo, jpeg_destroy_decompress);

    if (setjmp(jerr.jbuf) != 0) {
        throw ImageLoadError("jpg", jerr.msg);
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, buf.data(), buf.size());
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    if (cinfo.out_color_space != JCS_GRAYSCALE &&
        cinfo.out_color_space != JCS_RGB &&
        cinfo.out_color_space != JCS_CMYK) {

        throw ImageLoadError("jpg", "invalid JPEG color space (not grayscale, rgb, or cmyk)");
    }

    Canvas<4> rgba(cinfo.output_width, cinfo.output_height);

    std::vector<JSAMPLE> row(cinfo.output_width * cinfo.output_components);
    rowarray[0] = row.data();

    // Reset, because types with non-trivial destructors have been
    // created since the last setjmp(), and longjmping over them is
    // undefined behavior.
    if (setjmp(jerr.jbuf) != 0) {
        throw ImageLoadError("jpg", jerr.msg);
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        JDIMENSION y = cinfo.output_scanline;
        jpeg_read_scanlines(&cinfo, rowarray.data(), 1);
        if (cinfo.out_color_space == JCS_GRAYSCALE) {
            for (int i = 0; i < rgba.width(); i++) {
                rgba[y][i] = Pixel<4>(row[i], row[i], row[i], 0xff);
            }
        } else if (cinfo.out_color_space == JCS_RGB) {
            for (int i = 0; i < rgba.width(); i++) {
                rgba[y][i] = Pixel<4>(row[i * 3 + 0], row[i * 3 + 1], row[i * 3 + 2], 0xff);
            }
        } else if (cinfo.out_color_space == JCS_CMYK) {
            for (int i = 0; i < rgba.width(); i++) {
                double K = row[i * 4 + 3];
                rgba[y][i] = Pixel<4>(
                        row[i * 4 + 0] * K / 255.0,
                        row[i * 4 + 1] * K / 255.0,
                        row[i * 4 + 2] * K / 255.0,
                        0xff);
            }
        }
    }

    jpeg_finish_decompress(&cinfo);

    return rgba;
#endif
}

Canvas<4> gli_load_image_png(const std::vector<unsigned char> &buf)
{
    png_image image{};

    image.version = PNG_IMAGE_VERSION;
    auto free_png = garglk::unique(&image, png_image_free);

    if (png_image_begin_read_from_memory(&image, buf.data(), buf.size()) == 0) {
        throw ImageLoadError("png", image.message);
    }

    image.format = PNG_FORMAT_RGBA;
    Canvas<4> rgba(image.width, image.height);

    if (png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr) == 0) {
        throw ImageLoadError("png", image.message);
    }

    return rgba;
}
