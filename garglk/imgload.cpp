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
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "format.h"
#include "imgload.h"

#include "garglk.h"
#include "gi_blorb.h"

namespace {

struct PicturePair {
    std::shared_ptr<picture_t> picture;
    std::shared_ptr<picture_t> scaled;
};

std::unordered_map<unsigned long, PicturePair> picstore;

int gli_piclist_refcount = 0; // count references to loaded pictures

void gli_picture_store_original(const std::shared_ptr<picture_t> &pic)
{
    picstore[pic->id] = PicturePair{pic, nullptr};
}

void gli_picture_store_scaled(const std::shared_ptr<picture_t> &pic)
{
    try {
        auto &picpair = picstore.at(pic->id);
        picpair.scaled = pic;
    } catch (const std::out_of_range &) {
    }
}

}

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

    if (giblorb_get_resource_map() != nullptr) {
        if (!giblorb_copy_resource(giblorb_ID_Pict, id, chunktype, buf)) {
            return nullptr;
        }
    } else {
        const auto &resource_map = gli_get_resource_map(giblorb_ID_Pict);
        if (!resource_map.empty()) {
            try {
                buf = resource_map.at(id);
            } catch (const std::out_of_range &) {
                return nullptr;
            }
        } else {
            auto filename = Format("{}/PIC{}", gli_workdir, id);

            if (!garglk::read_file(filename, buf)) {
                return nullptr;
            }
        }

        if (buf.size() < 8) {
            return nullptr;
        }

        static constexpr std::array<unsigned char, 8> png_sig{
            137, 80, 78, 71, 13, 10, 26, 10
        };

        if (std::equal(png_sig.begin(), png_sig.end(), buf.begin())) {
            chunktype = giblorb_ID_PNG;
        } else if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) {
            chunktype = giblorb_ID_JPEG;
        } else {
            // Not a readable file. Forget it.
            gli_strict_warning(Format("unable to load image {}: unknown format", id));
            return nullptr;
        }
    }

    static const std::unordered_map<int, std::function<Canvas<4>(const std::vector<unsigned char> &)>> loaders = {
        {giblorb_ID_PNG, gli_load_image_png},
        {giblorb_ID_JPEG, gli_load_image_jpeg},
    };

    try {
        auto rgba = loaders.at(chunktype)(buf);
        auto pic = std::make_shared<picture_t>(id, std::move(rgba), false);
        gli_picture_store(pic);
        return pic;
    } catch (const ImageLoadError &e) {
        gli_strict_warning(Format("unable to load image {}: {}", id, e.what()));
    } catch (const std::out_of_range &) {
    }

    return nullptr;
}
