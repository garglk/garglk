// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2009 by Baltasar GarcÌa Perez-Schofield.
// Copyright (C) 2010 by Ben Cressey.
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

#ifndef GARGLK_LAUNCHER_H
#define GARGLK_LAUNCHER_H

#include <string>

namespace garglk {

void winmsg(const std::string &msg);
bool winterp(const std::string &exe, const std::string &flags, const std::string &game);
bool rungame(const std::string &game);

}

#endif
