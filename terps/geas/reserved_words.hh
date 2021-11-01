/***************************************************************************
 *                                                                         *
 * Copyright (C) 2006 by Mark J. Tilford                                   *
 *                                                                         *
 * This file is part of Geas.                                              *
 *                                                                         *
 * Geas is free software; you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * Geas is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with Geas; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *                                                                         *
 ***************************************************************************/

#ifndef __reserved_words_hh
#define __reserved_words_hh

#include <map>
#include <string>
#include <stdarg.h>
#include <iostream>
#include "general.hh"

class reserved_words
{
private:
  typedef std::map<std::string, bool> maptype;
  maptype data;

public:
  bool operator[] (const std::string &s) const { return has(s); }
  bool has (const std::string &s) const
  {  
    maptype::const_iterator i = data.find (s);
    return i != data.end() && (*i).second;
  }

  void dump (std::ostream &o) const
  {
    o << "RW {";
    maptype::const_iterator i = data.begin();
    while (i != data.end())
      {
	o << (*i).first;
	i ++;
	if (i != data.end())
	  o << ", ";
      }
    o << "}";
  }

  reserved_words (const char *c, ...)
  {
    va_list ap;
    va_start (ap, c);

    while (c != NULL) 
      {
	data[std::string(c)] = true;
	//std::cout << "Pushing <" << c << "> into map" << std::endl;
	c = va_arg (ap, char*);
      }
    va_end(ap);
  }
};

inline std::ostream &operator<< (std::ostream &o, const reserved_words &rw)
 { rw.dump(o); return o; }

#endif
