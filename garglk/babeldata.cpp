// Copyright (C) 2011 by Ben Cressey.
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

#include <string>

#include "glk.h"
#include "garglk.h"

#ifdef BABEL_HANDLER

#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include "babel_handler.h"
#include "format.h"
#include "ifiction.h"
#include "treaty.h"

void gli_initialize_babel(const std::string &filename)
{
    auto ctx = garglk::unique(get_babel_ctx(), release_babel_ctx);
    if (babel_init_ctx(const_cast<char *>(filename.c_str()), ctx.get()) != nullptr) {
        int metaSize = babel_treaty_ctx(GET_STORY_FILE_METADATA_EXTENT_SEL, nullptr, 0, ctx.get());
        if (metaSize > 0) {
            try {
                std::vector<char> metadata(metaSize);
                if (babel_treaty_ctx(GET_STORY_FILE_METADATA_SEL, metadata.data(), metadata.size(), ctx.get()) > 0) {
                    auto get_metadata = [&metadata](const std::string &key) {
                        return garglk::unique(ifiction_get_tag(metadata.data(), const_cast<char *>("bibliographic"), const_cast<char *>(key.c_str()), nullptr), [](void *p) { std::free(p); });
                    };
                    auto story_title = get_metadata("title");
                    auto story_author = get_metadata("author");
                    if (story_title != nullptr && story_author != nullptr) {
                        std::string title;
                        title = Format("{} - {}", story_title.get(), story_author.get());
                        garglk_set_story_title(title.c_str());
                    }
                }
            } catch (const std::bad_alloc &) {
            }
        }
    }
    babel_release_ctx(ctx.get());
}

#else

void gli_initialize_babel(const std::string &)
{
}

#endif // BABEL_HANDLER
