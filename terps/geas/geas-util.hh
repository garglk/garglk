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

#ifndef __geas_util_hh
#define __geas_util_hh

#include "general.hh"

#define ARRAYSIZE(ar)  ((sizeof(ar))/(sizeof(*ar)))

#include <string>
#include "readfile.hh"
#include <map>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cassert>

typedef std::vector<std::string> vstring;

static inline int parse_int (const std::string &s) { return atoi(s.c_str()); }

extern vstring split_param (const std::string &s);
extern vstring split_f_args (const std::string &s);

extern bool is_param (const std::string &s);
extern std::string param_contents (const std::string &s);

extern std::string nonparam (const std::string &, const std::string &);

extern std::string string_geas_block (const GeasBlock &);

extern bool starts_with (const std::string &, const std::string &);
extern bool ends_with (const std::string &, const std::string &);

extern std::string string_int (int i);
extern std::string string_int (uint i);
extern std::string string_int (size_t i);

extern std::string trim_braces (const std::string &s);

extern int eval_int (const std::string &s);

extern std::string pcase (std::string s);
extern std::string ucase (std::string s);
extern std::string lcase (std::string s);

//ostream &operator<< (ostream &o, const vector<string> &v);
//template<class T> std::ostream &operator<< (std::ostream &o, const std::vector<T> &v) { return o;}

/*
template<class K, class V, class CMP, class ALLOC> ostream &operator<< (ostream &o, map<K, V, CMP, ALLOC> &m)
{
  //map <K,V, CMP, ALLOC>::iterator i;
  std::string i;
  for (i = m.begin(); i != m.end(); i ++)
    ;
  //o << "    " << i->first << ", " << i->second << "\n";
  return o;
};
*/



template<class T> std::ostream &operator << (std::ostream &o, std::vector<T> v)
{
  o << "{ '";
  for (uint i = 0; i < v.size(); i ++)
    {
      o << v[i];
      if (i + 1 < v.size())
	o << "', '";
    }
  o << "' }";
  return o;
}

template <class KEYTYPE, class VALTYPE> bool has (std::map<KEYTYPE, VALTYPE> m, KEYTYPE key) { return m.find (key) != m.end(); };

class Logger
{
 public:
  Logger ();
  ~Logger ();

 private:
  class Nullstreambuf : public std::streambuf
  {
   protected:
    int overflow (int c)
    {
      return traits_type::not_eof (c);
    }
  };

  std::ofstream *logfilestr_;
  std::streambuf *cerrbuf_;
  static Nullstreambuf cnull;
};

#endif
