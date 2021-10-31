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

#ifndef __geas_state_hh
#define __geas_state_hh

#include <string>
#include <vector>
#include <map>
#include "general.hh"

struct PropertyRecord
{
  std::string name, data;
  PropertyRecord (std::string in_name, std::string in_data) : name (in_name), data (in_data) {}
};

struct ObjectRecord
{
  std::string name, parent;
  bool hidden, invisible;
  
  //ObjectRecord (std::string in_name, std::string in_parent) : name (in_name), parent (in_parent), hidden (false), concealed (false) {}

};

struct ExitRecord
{
  std::string src, dest;
  ExitRecord (const std::string &in_src, const std::string &in_dest) : src(in_src), dest(in_dest) {}
};

struct TimerRecord
{
  std::string name;
  bool is_running;
  uint interval, timeleft;
};

struct SVarRecord
{
private:
  std::vector<std::string> data;
public:
  std::string name;

  SVarRecord () {}
  SVarRecord (const std::string &in_name) : name (in_name) { set (0, ""); }
  size_t size() const { return data.size(); }
  size_t max() const { return size() - 1; }
  void set (size_t i, const std::string &val) { if (i >= size()) data.resize(i+1); data[i] = val; }
  std::string get (size_t i) const { if (i < size()) return data[i]; return "!";}
  void set (const std::string &val) { data[0] = val; }
  std::string get() const { return data[0]; }
};

struct IVarRecord
{
private:
  std::vector<int> data;
public:
  std::string name;

  IVarRecord () {}
  IVarRecord (const std::string &in_name) : name (in_name) { set (0, 0); }
  size_t size() const { return data.size(); }
  size_t max() const { return size() - 1; }
  void set (size_t i, int val) { if (i >= size()) data.resize(i+1); data[i] = val; }
  int get (size_t i) const { if (i < size()) return data[i]; else return -32767;}
  void set(int val) { data[0] = val; }
  int get() const { return data[0]; }
};

class GeasFile;
class GeasInterface;

struct GeasState
{
  //private:
  //std::auto_ptr<GeasFile> gf;

public:
  bool running;
  std::string location;
  std::vector<PropertyRecord> props;
  std::vector<ObjectRecord> objs;
  std::vector<ExitRecord> exits;
  std::vector<TimerRecord> timers;
  std::vector<SVarRecord> svars;
  std::vector<IVarRecord> ivars;
  //std::map <std::string, std::string> obj_types;

  //void register_block (std::string blockname, std::string blocktype);

  GeasState () {}
  //GeasState (GeasRunner &, const GeasFile &);
  GeasState (GeasInterface &, const GeasFile &);
  /*
  bool has_svar (string s) { for (uint i = 0; i < svars.size(); i ++) if (svars[i].name == s) return true; }
  uint find_svar (string s) { for (uint i = 0; i < svars.size(); i ++) if (svars[i].name == s) return i; svars.push_back (SVarRecord (s)); return svars.size() - 1;}
  string get_svar (string s, uint index) { if (!has_svar(s)) return "!"; return svars[find_svar(s)].get(index); }
  string get_svar (string s) { return get_svar (s, 0); }

  bool has_ivar (string s) { for (uint i = 0; i < ivars.size(); i ++) if (ivars[i].name == s) return true; }
  uint find_ivar (string s) { for (uint i = 0; i < ivars.size(); i ++) if (ivars[i].name == s) return i; ivars.push_back (IVarRecord (s)); return ivars.size() - 1;}
  int get_ivar (string s, uint index) { if (!has_ivar(s)) return -32767; return ivars[find_ivar(s)].get(index); }
  int get_ivar (string s) { return get_ivar (s, 0); }
  */
};

extern void save_game_to (const std::string &gamename, const std::string &savename, const GeasState &gs);

extern std::ostream &operator<< (std::ostream &o, const std::map <std::string, std::string> &m);
extern std::ostream &operator<< (std::ostream &o, const PropertyRecord &pr);
extern std::ostream &operator<< (std::ostream &o, const ObjectRecord &objr);
extern std::ostream &operator<< (std::ostream &o, const ExitRecord &er);
extern std::ostream &operator<< (std::ostream &o, const TimerRecord &tr);
extern std::ostream &operator<< (std::ostream &o, const SVarRecord &sr);
extern std::ostream &operator<< (std::ostream &o, const IVarRecord &ir);
extern std::ostream &operator<< (std::ostream &o, const GeasState &gs);


#endif
