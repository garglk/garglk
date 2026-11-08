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

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#ifdef GARGLK_CONFIG_JPEG_TURBO
#include <turbojpeg.h>
#else
#include <csetjmp>
#include <cstdio>

// jpeglib.h requires these to be defined.
using std::size_t;
using std::FILE;

#include <jpeglib.h>
#endif

#include <png.h>

#ifdef GARGLK_CONFIG_HAS_GIF
#include <gif_lib.h>
#endif

#include "imgload.h"

#include "garglk.h"

// Image headers can claim sizes far larger than the file, and on an
// overcommitting system an allocation that big doesn't fail until the kernel
// kills the process, so the size has to be checked up front. The limit matches
// Qt 6's default image allocation limit, 256MiB.
static Canvas<4> make_canvas(const char *format, std::size_t width, std::size_t height)
{
    static constexpr std::size_t max_pixels = 256 * 1024 * 1024 / 4;

    if (width == 0 || height == 0) {
        throw ImageLoadError(format, "image has no pixels");
    }

    if (width > max_pixels / height) {
        throw ImageLoadError(format, Format("image too large ({}x{})", width, height));
    }

    return Canvas<4>(static_cast<int>(width), static_cast<int>(height));
}

Canvas<4> gli_load_image_jpeg(const std::vector<unsigned char> &buf)
{
#ifdef GARGLK_CONFIG_JPEG_TURBO
    auto tj = garglk::unique(tjInitDecompress(), tjDestroy);

    int w, h, subsamp, colorspace;

    if (tjDecompressHeader3(tj.get(), buf.data(), buf.size(), &w, &h, &subsamp, &colorspace) != 0) {
        throw ImageLoadError("jpg", tjGetErrorStr2(tj.get()));
    }

    auto rgba = make_canvas("jpg", w, h);

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

    auto rgba = make_canvas("jpg", cinfo.output_width, cinfo.output_height);

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
    auto rgba = make_canvas("png", image.width, image.height);

    if (png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr) == 0) {
        throw ImageLoadError("png", image.message);
    }

    return rgba;
}

#ifdef GARGLK_CONFIG_HAS_GIF
struct GIFSource {
    const unsigned char *buffer;
    std::size_t size;
    std::size_t pos;
};

static int read_gif_from_memory(GifFileType *gif, GifByteType *dst, int n)
{
    auto *src = static_cast<GIFSource *>(gif->UserData);

    if (src->pos >= src->size) {
        return 0;
    }

    std::size_t remaining = src->size - src->pos;
    if (static_cast<std::size_t>(n) > remaining) {
        n = static_cast<int>(remaining);
    }

    std::copy(src->buffer + src->pos, src->buffer + src->pos + n, dst);
    src->pos += n;

    return n;
}

static std::string gif_error(int error)
{
    const char *msg = GifErrorString(error);
    return msg != nullptr ? msg : Format("unknown error {}", error);
}

// This follows what Qt's GIF decoder does with malformed images, so that the
// system and Qt image backends agree.
//
// Only the first frame is used, so decoding stops as soon as it's been read.
// Later frames aren't decoded, and if they're damaged, or the trailer is
// missing, the image still loads.
Canvas<4> gli_load_image_gif(const std::vector<unsigned char> &buf)
{
    GIFSource src{buf.data(), buf.size(), 0};

    int error;
    auto gif = garglk::unique(
            DGifOpen(&src, read_gif_from_memory, &error),
            [](GifFileType *g) { DGifCloseFile(g, nullptr); });

    if (gif == nullptr) {
        throw ImageLoadError("gif", gif_error(error));
    }

    auto check = [&gif](int result) {
        if (result == GIF_ERROR) {
            throw ImageLoadError("gif", gif_error(gif->Error));
        }
    };

    int transparent = NO_TRANSPARENT_COLOR;
    GifRecordType record;

    do {
        check(DGifGetRecordType(gif.get(), &record));

        if (record == TERMINATE_RECORD_TYPE) {
            throw ImageLoadError("gif", "no image data");
        }

        if (record == EXTENSION_RECORD_TYPE) {
            int code;
            GifByteType *ext;

            check(DGifGetExtension(gif.get(), &code, &ext));

            GraphicsControlBlock gcb;
            if (code == GRAPHICS_EXT_FUNC_CODE && ext != nullptr && DGifExtensionToGCB(ext[0], &ext[1], &gcb) == GIF_OK) {
                transparent = gcb.TransparentColor;
            }

            while (ext != nullptr) {
                check(DGifGetExtensionNext(gif.get(), &ext));
            }
        }
    } while (record != IMAGE_DESC_RECORD_TYPE);

    check(DGifGetImageDesc(gif.get()));

    const GifImageDesc &desc = gif->Image;
    const ColorMapObject *colormap = desc.ColorMap != nullptr ? desc.ColorMap : gif->SColorMap;

    // Without a transparent color, nothing in the image is transparent, even
    // the pixels below that would otherwise be.
    const int unset_alpha = transparent == NO_TRANSPARENT_COLOR ? 0xff : 0x00;

    auto color = [&](const ColorMapObject *map, int index) {
        if (map == nullptr) {
            return Pixel<4>(0x00, 0x00, 0x00, unset_alpha);
        }

        if (index >= map->ColorCount) {
            return Pixel<4>(0xff, 0xff, 0xff, unset_alpha);
        }

        const GifColorType &c = map->Colors[index];
        return Pixel<4>(c.Red, c.Green, c.Blue, index == transparent ? 0x00 : 0xff);
    };

    // An empty logical screen takes its size from the frame. Otherwise the
    // frame is clipped to the screen.
    const int width = gif->SWidth > 0 ? gif->SWidth : desc.Left + desc.Width;
    const int height = gif->SHeight > 0 ? gif->SHeight : desc.Top + desc.Height;

    auto rgba = make_canvas("gif", width, height);

    Pixel<4> background(0x00, 0x00, 0x00, unset_alpha);
    if (desc.Left > 0 || desc.Top > 0 || desc.Width < width || desc.Height < height) {
        if (transparent != NO_TRANSPARENT_COLOR) {
            background = color(colormap, transparent);
        } else if (gif->SColorMap != nullptr) {
            background = color(gif->SColorMap, gif->SBackGroundColor);
        }
    }

    rgba.fill(background);

    const int right = std::min(desc.Left + desc.Width, width);
    const int bottom = std::min(desc.Top + desc.Height, height);

    // Each interlaced pass but the last also fills the rows that later passes
    // will replace, so a truncated image has no holes in it. This isn't done
    // if there's a transparent color.
    auto read_row = [&](int y, int fill) {
        // Read one pixel at a time so that if the image is truncated, the
        // start of an incomplete row is kept. A failure can also come after
        // the frame's final pixel has been read, when giflib looks for the
        // end of the image data, so failure doesn't mean there's no pixel.
        // Index 255 is almost never used, so it's a good "no pixel" marker.
        for (int x = desc.Left; x < desc.Left + desc.Width; x++) {
            GifPixelType index = 0xff;
            const bool ok = DGifGetLine(gif.get(), &index, 1) == GIF_OK;

            if ((ok || index != 0xff) && x < width && y < height) {
                rgba[y][x] = color(colormap, index);
            }

            if (!ok) {
                return false;
            }
        }

        if (transparent == NO_TRANSPARENT_COLOR) {
            for (int dst = y + 1; y < height && dst <= y + fill && dst < bottom; dst++) {
                for (int x = desc.Left; x < right; x++) {
                    rgba[dst][x] = Pixel<4>(rgba[y][x]);
                }
            }
        }

        return true;
    };

    auto read_rows = [&] {
        struct Pass {
            int offset;
            int step;
            int fill;
        };

        static constexpr std::array<Pass, 4> interlaced{{{0, 8, 7}, {4, 8, 3}, {2, 4, 1}, {1, 2, 0}}};

        if (!desc.Interlace) {
            for (int y = 0; y < desc.Height; y++) {
                if (!read_row(desc.Top + y, 0)) {
                    return false;
                }
            }

            return true;
        }

        for (const auto &pass : interlaced) {
            for (int y = pass.offset; y < desc.Height; y += pass.step) {
                if (!read_row(desc.Top + y, pass.fill)) {
                    return false;
                }
            }
        }

        return true;
    };

    // Once image data has been seen, a failure just means the rest of the
    // image is missing. The code size byte has already been read, so that
    // means a sub-block size and at least one byte of data.
    const std::size_t data_start = src.pos;

    if (!read_rows() && src.size < data_start + 2) {
        throw ImageLoadError("gif", gif_error(gif->Error));
    }

    return rgba;
}
#else
Canvas<4> gli_load_image_gif(const std::vector<unsigned char> &)
{
    throw ImageLoadError("gif", "GIF support not available");
}
#endif
