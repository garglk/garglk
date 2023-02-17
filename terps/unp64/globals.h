/*
 C++ version based on code adapted to ScummVM by Avijeet Maurya
 */

/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef UNP64_GLOBALS_H
#define UNP64_GLOBALS_H

#include "unp64.h"

namespace Unp64 {

class Globals {
public:
	// unp64
	UnpStr _unp;
	int _parsePar = 1;
	int _iter = 0;

	// 6502 emu
	int _byted011[2] = {0, 0};
	int _retfire = 0xff;
	int _retspace = 0xff;

public:
	Globals();
	~Globals();
};

extern Globals *g_globals;

#define _G(FIELD) (g_globals->FIELD)

} // End of namespace Unp64

#endif
