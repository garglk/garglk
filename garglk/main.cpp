// Copyright (C) 2006-2009 by Tor Andersson.
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
#include <cstring>
#include <iostream>

#include "glk.h"
#include "glkstart.h"
#include "garglk.h"

int main(int argc, char *argv[])
{
    if (argc == 0) {
        std::cerr << "argv[0] is NULL, aborting\n";
        std::exit(EXIT_FAILURE);
    }

    glkunix_startup_t startdata;
    startdata.argc = argc;
    startdata.argv = new char *[argc];
    std::memcpy(startdata.argv, argv, argc * sizeof(char *));

    gli_startup(argc, argv);

    if (!glkunix_startup_code(&startdata)) {
        glk_exit();
    }

    glk_main();
    glk_exit();

    return 0;
}
