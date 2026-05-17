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

#ifndef GARGLK_IMGLOAD_H
#define GARGLK_IMGLOAD_H

#include <stdexcept>
#include <string>
#include <vector>

#include "format.h"
#include "garglk.h"

struct ImageLoadError : public std::runtime_error {
    ImageLoadError(const std::string &format, const std::string &msg) : std::runtime_error(Format("{} [{}]", msg, format)) {
    }
};

Canvas<4> gli_load_image_jpeg(const std::vector<unsigned char> &buf);
Canvas<4> gli_load_image_png(const std::vector<unsigned char> &buf);

#endif
