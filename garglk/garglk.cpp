// Copyright (C) 2022 by Chris Spiegel
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

#include <cstdlib>
#include <fstream>
#include <ios>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include "format.h"

#include "garglk.h"
#include "gi_blorb.h"

std::string gli_program_name = "Unknown";
std::string gli_program_info;
std::string gli_story_name;
std::string gli_story_title;

bool gli_exiting = false;

void garglk_set_program_name(const char *name)
{
    gli_program_name = name;
    wintitle();
}

void garglk_set_program_info(const char *info)
{
    gli_program_info = info;
}

void garglk_set_story_name(const char *name)
{
    gli_story_name = name;
    wintitle();
}

void garglk_set_story_title(const char *title)
{
    gli_story_title = title;
    wintitle();
}

int garglk_tads_os_banner_size(winid_t win)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int size = dwin->scrollmax;
    if (dwin->numchars != 0) {
        size++;
    }
    return size;
}

// All normal program termination should go through here instead of
// directly calling std::exit() to ensure that gli_exiting is properly
// set. Some code in destructors needs to be careful what it's doing
// during program exit, and uses this flag to check that.
void gli_exit(int status)
{
    gli_exiting = true;
    std::exit(status);
}

bool garglk::read_file(const std::string &filename, std::vector<unsigned char> &buf)
{
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }

    try {
        buf.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    } catch (const std::bad_alloc &) {
        return false;
    }

    return !f.fail();
}

void garglk_window_get_size_pixels(window_t *win, glui32 *width, glui32 *height)
{
    glui32 wid;
    glui32 hgt;

    // glk_window_get_size() already does what we need for blank, pair,
    // and graphics windows.
    glk_window_get_size(win, &wid, &hgt);

    // For text windows, the size should be reported in pixels, and
    // since glk_window_get_size() does integer division, this can't
    // just multiply by gli_cellw/gli_cellh; calculate directly.
    if (win->type == wintype_TextGrid) {
        wid = win->bbox.x1 - win->bbox.x0;
        hgt = win->bbox.y1 - win->bbox.y0;
    } else if (win->type == wintype_TextBuffer) {
        wid = win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2;
        hgt = win->bbox.y1 - win->bbox.y0 - gli_tmarginy * 2;
    }

    if (width != nullptr) {
        *width = wid;
    }

    if (height != nullptr) {
        *height = hgt;
    }
}

// Map tuples of (usage, filename, offset) to IDs
static std::map<std::tuple<glui32, std::string, glui32>, glui32> resource_ids;

// Map IDs to chunks loaded from files
static std::map<glui32, std::map<glui32, std::vector<unsigned char>>> resource_maps;

// Insert a data chunk into the specified resource map, returning a resource ID
static glui32 gli_insert_resource(glui32 usage, std::vector<unsigned char> data)
{
    auto &map = resource_maps[usage];

    glui32 id = 1;
    if (!map.empty()) {
        id = map.rbegin()->first + 1;
    }

    map.insert({id, std::move(data)});

    return id;
}

const std::map<glui32, std::vector<unsigned char>> &gli_get_resource_map(glui32 usage)
{
    return resource_maps[usage];
}

glui32 garglk_add_resource_from_file(glui32 usage, const char *filename_, glui32 offset, glui32 len)
{
    std::string filename(filename_);

#ifdef _WIN32
    static const std::vector<std::string> reserved_names = {
        "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4",
        "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2",
        "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
    };

    auto lower = garglk::downcase(filename);
    for (const auto &reserved_name : reserved_names) {
        if (lower == reserved_name || lower.find(reserved_name + ".") == 0) {
            return 0;
        }
    }

    std::string sep = "/\\";
#else
    std::string sep = "/";
#endif

    if (filename.find_first_of(sep) != std::string::npos) {
        return 0;
    }

    auto key = std::make_tuple(usage, filename, offset);

    try {
        return resource_ids.at(key);
    } catch (const std::out_of_range &) {
    }

    filename = Format("{}/{}", gli_workdir, filename_);

    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        return 0;
    }

    if (!f.seekg(offset, std::ios::beg)) {
        return 0;
    }

    // vector resize/map insert can throw memory-related exceptions.
    try {
        std::vector<unsigned char> data(len);
        if (!f.read(reinterpret_cast<char *>(data.data()), len)) {
            return 0;
        }

        auto id = gli_insert_resource(usage, std::move(data));

        resource_ids.insert({key, id});

        return id;
    } catch (...) {
        return 0;
    }
}
