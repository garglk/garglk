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
#include <string>

#include "garglk.h"

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
    std::ifstream f(filename);
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
