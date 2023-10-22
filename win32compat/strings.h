//* Partial strings.h implementation for Windows.
// 
// Copyright (c) 2023 by Adrian Welcker
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

#pragma once

#ifndef GARGLK_WIN32_STRINGS_H
#define GARGLK_WIN32_STRINGS_H

#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#endif  // !GARGLK_WIN32_STRINGS_H
