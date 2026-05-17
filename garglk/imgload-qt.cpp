// Copyright (C) 2026 by Chris Spiegel.
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

#include <cstring>
#include <vector>

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>

#include "garglk.h"
#include "imgload.h"

namespace {

Canvas<4> load_image(const std::vector<unsigned char> &buf, const char *format)
{
    auto bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(buf.data()), buf.size());
    QBuffer imgbuf(&bytes);
    imgbuf.open(QIODevice::ReadOnly);

    QImageReader reader(&imgbuf, format);
    QImage img = reader.read();
    if (img.isNull()) {
        throw ImageLoadError(format, reader.errorString().toStdString());
    }

    img = img.convertToFormat(QImage::Format_RGBA8888);
    if (img.isNull()) {
        throw ImageLoadError(format, "failed to convert image to RGBA8888");
    }

    const int w = img.width();
    const int h = img.height();
    const int row_bytes = w * 4;

    Canvas<4> rgba(w, h);

    if (img.bytesPerLine() == w * 4) {
        std::memcpy(rgba.data(), img.constBits(), row_bytes * h);
    } else {
        for (int y = 0; y < h; y++) {
            std::memcpy(rgba.data() + y * row_bytes, img.constScanLine(y), row_bytes);
        }
    }

    return rgba;
}

}

Canvas<4> gli_load_image_jpeg(const std::vector<unsigned char> &buf)
{
    return load_image(buf, "jpeg");
}

Canvas<4> gli_load_image_png(const std::vector<unsigned char> &buf)
{
    return load_image(buf, "png");
}
