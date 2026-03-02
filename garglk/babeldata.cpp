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

static std::string xml_unescape(const std::string &s)
{
    std::string result;
    result.reserve(s.size());

    for (std::string::size_type i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            if (s.compare(i, 4, "&lt;") == 0) {
                result += '<';
                i += 3;
            } else if (s.compare(i, 4, "&gt;") == 0) {
                result += '>';
                i += 3;
            } else if (s.compare(i, 5, "&amp;") == 0) {
                result += '&';
                i += 4;
            } else {
                result += '&';
            }
        } else {
            result += s[i];
        }
    }

    return result;
}

static nonstd::optional<std::string> get_tag(const std::string &metadata, const std::string &key)
{
    auto value = garglk::unique(
        ifiction_get_tag(
            const_cast<char *>(metadata.c_str()),
            const_cast<char *>("bibliographic"),
            const_cast<char *>(key.c_str()),
            nullptr),
        [](void *p) { std::free(p); });

    if (value != nullptr) {
        return xml_unescape(std::string(value.get()));
    }

    return nonstd::nullopt;
}

// Collapse runs of whitespace to a single space and trim leading/trailing whitespace.
static std::string simplify(const std::string &s)
{
    std::string result;
    bool in_space = true;

    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!in_space) {
                result += ' ';
                in_space = true;
            }
        } else {
            result += c;
            in_space = false;
        }
    }

    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

static std::vector<std::string> parse_description(const std::string &raw)
{
    std::vector<std::string> paragraphs;
    std::string::size_type pos = 0;

    while (pos < raw.size()) {
        auto br = raw.find("<br/>", pos);
        std::string chunk;
        if (br == std::string::npos) {
            chunk = raw.substr(pos);
            pos = raw.size();
        } else {
            chunk = raw.substr(pos, br - pos);
            pos = br + 5; // skip "<br/>"
        }

        auto paragraph = simplify(chunk);
        if (!paragraph.empty()) {
            paragraphs.push_back(std::move(paragraph));
        }
    }

    return paragraphs;
}

static nonstd::optional<garglk::GameInfo> parse_ifiction(const std::string &metadata)
{
    if (metadata.empty()) {
        return nonstd::nullopt;
    }

    auto title = get_tag(metadata, "title");
    auto author = get_tag(metadata, "author");
    if (!title.has_value() || !author.has_value()) {
        return nonstd::nullopt;
    }

    garglk::GameInfo info;
    info.title = std::move(*title);
    info.author = std::move(*author);
    info.headline = get_tag(metadata, "headline");

    auto raw_description = get_tag(metadata, "description");
    if (raw_description.has_value()) {
        info.description = parse_description(*raw_description);
    }

    return info;
}

nonstd::optional<garglk::GameInfo> garglk::get_game_info(const std::string &filename)
{
    auto ctx = garglk::unique(get_babel_ctx(), release_babel_ctx);
    if (babel_init_ctx(const_cast<char *>(filename.c_str()), ctx.get()) == nullptr) {
        babel_release_ctx(ctx.get());
        return nonstd::nullopt;
    }

    nonstd::optional<garglk::GameInfo> info;

    // Extract and parse iFiction metadata.
    int meta_size = babel_treaty_ctx(GET_STORY_FILE_METADATA_EXTENT_SEL, nullptr, 0, ctx.get());
    if (meta_size > 0) {
        try {
            std::vector<char> metadata(meta_size);
            if (babel_treaty_ctx(GET_STORY_FILE_METADATA_SEL, metadata.data(), metadata.size(), ctx.get()) > 0) {
                info = parse_ifiction(std::string(metadata.data(), metadata.size()));
            }
        } catch (const std::bad_alloc &) {
        }
    }

    if (!info.has_value()) {
        babel_release_ctx(ctx.get());
        return nonstd::nullopt;
    }

    // Extract IFID.
    std::vector<char> ifid_buf(512);
    if (babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, ifid_buf.data(), ifid_buf.size(), ctx.get()) > 0) {
        info->ifid = std::string(ifid_buf.data());
    }

    // Extract cover art.
    int cover_size = babel_treaty_ctx(GET_STORY_FILE_COVER_EXTENT_SEL, nullptr, 0, ctx.get());
    if (cover_size > 0) {
        try {
            std::vector<std::uint8_t> cover(cover_size);
            if (babel_treaty_ctx(GET_STORY_FILE_COVER_SEL, cover.data(), cover.size(), ctx.get()) > 0) {
                info->cover = std::move(cover);
            }
        } catch (const std::bad_alloc &) {
        }
    }

    babel_release_ctx(ctx.get());
    return info;
}

void gli_initialize_babel(const std::string &filename)
{
    auto ctx = garglk::unique(get_babel_ctx(), release_babel_ctx);
    if (babel_init_ctx(const_cast<char *>(filename.c_str()), ctx.get()) != nullptr) {
        int meta_size = babel_treaty_ctx(GET_STORY_FILE_METADATA_EXTENT_SEL, nullptr, 0, ctx.get());
        if (meta_size > 0) {
            try {
                std::vector<char> metadata(meta_size);
                if (babel_treaty_ctx(GET_STORY_FILE_METADATA_SEL, metadata.data(), metadata.size(), ctx.get()) > 0) {
                    auto info = parse_ifiction(std::string(metadata.data(), metadata.size()));
                    if (info.has_value()) {
                        auto title = Format("{} - {}", info->title, info->author);
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

nonstd::optional<garglk::GameInfo> garglk::get_game_info(const std::string &)
{
    return nonstd::nullopt;
}

void gli_initialize_babel(const std::string &)
{
}

#endif // BABEL_HANDLER
