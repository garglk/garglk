// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2023 by Adrian Welcker.
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


#if defined(_MSC_VER) && defined(__clang__)

// This is all a bunch of poopy nonsense, but alas...

#define main deputy_main
int deputy_main(int argc, char *argv[]);

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    int argc;
    char **argv;

    char *myName = new char[MAX_PATH + 1];
    if (GetModuleFileNameA(0, myName, MAX_PATH + 1) == 0) {
        MessageBoxA(nullptr, "Unable to determine who I am. Aborting.", "Gargoyle Existential Crisis", MB_ICONERROR);
        return EXIT_FAILURE;
    }

    // adapted from: https://stackoverflow.com/a/74999569

    wchar_t **wargv = CommandLineToArgvW(lpCmdLine, &argc);
    if (!wargv) return -1;

    // Count the number of bytes necessary to store the UTF-8 versions of those strings
    int argv_strSize = 0;
    for (int i = 0; i < argc; i++)
        argv_strSize += WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr) + 1;

    argc += 1;  // argv[0] needs to be our executable name, which CommandLineToArgvW does not account for.
    argv = new char *[argc+1];
    argv[0] = myName;
    char *arg = new char[argv_strSize];
    int used = 0;
    for (int i = 1; i < argc; i++) {
        argv[i] = arg + used;
        used += WideCharToMultiByte(CP_UTF8, 0, wargv[i-1], -1, arg, argv_strSize - used, nullptr, nullptr)+1;
    }
    argv[argc] = nullptr;

    LocalFree(wargv);
    return main(argc, argv);
}

#endif

int main(int argc, char *argv[])
{
    if (argc == 0) {
        std::cerr << "argv[0] is null, aborting\n";
        std::exit(EXIT_FAILURE);
    }

    glkunix_startup_t startdata;
    startdata.argc = argc;
    startdata.argv = new char *[argc];
    std::memcpy(startdata.argv, argv, argc * sizeof(char *));

    garglk_startup(argc, argv);

    if (!glkunix_startup_code(&startdata)) {
        glk_exit();
    }

    glk_main();
    glk_exit();

    return 0;
}
