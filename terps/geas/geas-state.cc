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

#include "geas-state.hh"
#include "GeasRunner.hh"
#include "geas-util.hh"
#include "readfile.hh"
#include "general.hh"

#include <sstream>
#include <ostream>
#include <fstream>
#include <iostream>

using namespace std;

ostream &operator<< (ostream &o, const map <string, string> &m)
{
  for (map <string, string>::const_iterator i = m.begin(); i != m.end(); i ++)
    o << (*i).first << " -> " << (*i).second << "\n";
  return o;
}

class GeasOutputStream { 
  ostringstream o; 
  
public: 
  GeasOutputStream &put (string s) { o << s; o.put (char (0)); return *this; }
  GeasOutputStream &put (char ch) { o.put (ch); return *this; }
  GeasOutputStream &put (int i) { o << i; o.put (char (0)); return *this; }
  GeasOutputStream &put (uint i) { o << i; o.put (char (0)); return *this; }
  GeasOutputStream &put (unsigned long i) { o << i; o.put (char (0)); return *this; } // for Mac OS X ...
  GeasOutputStream &put (unsigned long long i) { o << i; o.put (char (0)); return *this; }
  

  void write_out (const string &gamename, const string &savename)
  {
    ofstream ofs;
    ofs.open (savename.c_str());
    if (!ofs.is_open())
      throw string ("Unable to open \"" + savename + "\"");
    ofs << "QUEST300" << char(0) << gamename << char(0);
    string tmp = o.str();
    for (uint i = 0; i < tmp.size(); i ++)
      ofs << char (255 - tmp[i]);
    cerr << "Done writing save game\n";
  }
};

template <class T> void write_to (GeasOutputStream &gos, const vector<T> &v) 
{ 
  gos.put(v.size());
  for (uint i = 0; i < v.size(); i ++)
    write_to (gos, v[i]);
}
  
void write_to (GeasOutputStream &gos, const PropertyRecord &pr)
{
  gos.put (pr.name).put (pr.data);
}

void write_to (GeasOutputStream &gos, const ObjectRecord &pr)
{
  gos.put (pr.name).put (char(pr.hidden ? 0 : 1))
    .put (char(pr.invisible ? 0 : 1)).put (pr.parent);
}

void write_to (GeasOutputStream &gos, const ExitRecord &er)
{
  gos.put (er.src).put (er.dest);
}

void write_to (GeasOutputStream &gos, const TimerRecord &tr)
{
  gos.put (tr.name).put (tr.is_running ? 0 : 1).put (tr.interval)
    .put (tr.timeleft);
}


void write_to (GeasOutputStream &gos, const SVarRecord &svr)
{
  gos.put (svr.name);
  gos.put (svr.max());
  for (uint i = 0; i < svr.size(); i ++)
    gos.put (svr.get(i));
}

void write_to (GeasOutputStream &gos, const IVarRecord &ivr)
{
  gos.put (ivr.name);
  gos.put (ivr.max());
  for (uint i = 0; i < ivr.size(); i ++)
    gos.put (ivr.get(i));
}

void write_to (GeasOutputStream &gos, const GeasState &gs)
{
  gos.put (gs.location);
  write_to (gos, gs.props);
  write_to (gos, gs.objs);
  write_to (gos, gs.exits);
  write_to (gos, gs.timers);
  write_to (gos, gs.svars);
  write_to (gos, gs.ivars);
}

void save_game_to (const std::string &gamename, const std::string &savename, const GeasState &gs)
{
  GeasOutputStream gos;
  write_to (gos, gs);
  gos.write_out (gamename, savename);
}

GeasState::GeasState (GeasInterface &gi, const GeasFile &gf)
{
  running = false;

  cerr << "GeasState::GeasState()" << endl;
  for (uint i = 0; i < gf.size ("game"); i ++)
    {
      //const GeasBlock &go = gf.game[i];
      //register_block ("game", "game");
      ObjectRecord data;
      data.name = "game";
      data.parent = "";
      data.hidden = false;
      data.invisible = true;
      objs.push_back (data);
    }

  cerr << "GeasState::GeasState() done setting game" << endl;
  for (uint i = 0; i < gf.size ("room"); i ++)
    {
      const GeasBlock &go = gf.block ("room", i);
      ObjectRecord data;
      //data.name = go.lname;
      data.name = go.name;
      data.parent = "";
      data.hidden = data.invisible = true;
      //register_block (data.name, "room");
      objs.push_back (data);
    }

  cerr << "GeasState::GeasState() done setting rooms" << endl;
  for (uint i = 0; i < gf.size ("object"); i++)
    {
      const GeasBlock &go = gf.block ("object", i);
      ObjectRecord data;
      //data.name = go.lname;
      data.name = go.name;
      if (go.parent == "")
	data.parent = "";
      else
	//data.parent = lcase (param_contents (go.parent));
	data.parent = param_contents (go.parent);
      //register_block (data.name, "object");
      data.hidden = data.invisible = false;
      objs.push_back (data);
    }

  cerr << "GeasState::GeasState() done setting objects" << endl;
  for (uint i = 0; i < gf.size("timer"); i ++)
    {
      const GeasBlock &go = gf.block("timer", i);
      //cerr << "GS::GS: Handling timer " << go << "\n";
      TimerRecord tr;
      string interval = "", status = "";
      for (uint j = 0; j < go.data.size(); j ++)
	{
	  string line = go.data[j];
	  std::string::size_type c1, c2;
	  string tok = first_token (line, c1, c2);
	  if (tok == "interval")
	    {
	      tok = next_token (line, c1, c2);
	      if (!is_param (tok))
		gi.debug_print (nonparam ("interval", line));
	      else
		interval = param_contents(tok);
	    }
	  else if (tok == "enabled" || tok == "disabled")
	    {
	      if (status != "")
		gi.debug_print ("Repeated status for timer");
	      else
		    status = tok;
	    }
	  else if (tok == "action")
	    {
	    }
	  else
	    {
	      gi.debug_print ("Bad timer line " + line);
	    }
	}
      //tr.name = go.lname;
      tr.name = go.name;
      tr.is_running = (status == "enabled");
      tr.interval = tr.timeleft = parse_int (interval);
      //register_block (tr.name, "timer");
      timers.push_back (tr);
    }

  cerr << "GeasState::GeasState() done with timers" << endl;
  for (uint i = 0; i < gf.size("variable"); i ++)
    {
      const GeasBlock &go (gf.block("variable", i));
      cerr << "GS::GS: Handling variable #" << i << ": " << go << endl;
      string vartype;
      string value;
      for (uint j = 0; j < go.data.size(); j ++)
	{
	  string line = go.data[j];
	  cerr << "   Line #" << j << " of var: \"" << line << "\"" << endl;
	  std::string::size_type c1, c2;
	  string tok = first_token (line, c1, c2);
	  if (tok == "type")
	    {
	      tok = next_token (line, c1, c2);
	      if (tok == "")
		gi.debug_print (string("Missing variable type in ")
				+ string_geas_block (go));
	      else if (vartype != "")
		gi.debug_print (string ("Redefining var. type in ")
				+ string_geas_block (go));
	      else if (tok == "numeric" || tok == "string")
		vartype = tok;
	      else
		gi.debug_print (string ("Bad var. type ") + line);
	    }
	  else if (tok == "value")
	    {
	      tok = next_token (line, c1, c2);
	      if (!is_param (tok))
		gi.debug_print (string ("Expected parameter in " + line));
	      else
		value = param_contents (tok);
	    }
	  else if (tok == "display" || tok == "onchange")
	    {
	    }
	  else
	    {
	      gi.debug_print (string ("Bad var. line: ") + line);
	    }
	}
      if (vartype == "" || vartype == "numeric")
	{
	  IVarRecord ivr;
	  //ivr.name = go.lname;
	  ivr.name = go.name;
	  ivr.set (0, parse_int (value));
	  ivars.push_back (ivr);
	  //register_block (ivr.name, "numeric");
	}
      else
	{
	  SVarRecord svr;
	  //svr.name = go.lname;
	  svr.name = go.name;
	  svr.set (0, value);
	  svars.push_back (svr);
	  //register_block (svr.name, "string");
	}
    }
  //cerr << obj_types << endl;
  cerr << "GeasState::GeasState() done with variables" << endl;
}

ostream &operator<< (ostream &o, const PropertyRecord &pr) 
{ 
  o << pr.name << ", data == " << pr.data; 
  return o; 
}

ostream &operator<< (ostream &o, const ObjectRecord &objr) 
{ 
  o << objr.name << ", parent == " << objr.parent; 
  if (objr.hidden) 
    o << ", hidden"; 
  if (objr.invisible)
    o << ", invisible";
  return o; 
}

ostream &operator<< (ostream &o, const ExitRecord er) 
{ 
  return o << er.src << ": " << er.dest;
}

ostream &operator<< (ostream &o, const TimerRecord &tr) 
{
  return o << tr.name << ": " << (tr.is_running ? "" : "not ") << "running (" 
	   << tr.timeleft << " // " << tr.interval << ")"; 
}

ostream &operator<< (ostream &o, const SVarRecord &sr) 
{ 
  o << sr.name << ": ";
  if (sr.size () == 0)
    o << "(empty)";
  else if (sr.size() <= 1)
    o << "<" << sr.get(0) << ">";
  else
    for (uint i = 0; i < sr.size(); i ++)
      {
	o << i << ": <" << sr.get(i) << ">";
	if (i + 1 < sr.size())
	  o << ", ";
      }
  return o;
}

ostream &operator<< (ostream &o, const IVarRecord &ir) 
{ 
  o << ir.name << ": ";
  if (ir.size () == 0)
    o << "(empty)";
  else if (ir.size() <= 1)
    o << ir.get(0);
  else
    for (uint i = 0; i < ir.size(); i ++)
      {
	o << i << ": " << ir.get(i);
	if (i + 1 < ir.size())
	  o << ", ";
      }
  return o;
}

ostream &operator<< (ostream &o, const GeasState &gs)
{
  o << "location == " << gs.location << "\nprops: \n";
 
  for (uint i = 0; i < gs.props.size(); i ++)
    o << "    " << i << ": " << gs.props[i] << "\n";

  o << "objs:\n";
  for (uint i = 0; i < gs.objs.size(); i ++)
    o << "    " << i << ": " << gs.objs[i] << "\n";

  o << "exits:\n";
  for (uint i = 0; i < gs.exits.size(); i ++)
    o << "    " << i << ": " << gs.exits[i] << "\n";

  o << "timers:\n";
  for (uint i = 0; i < gs.timers.size(); i ++)
    o << "    " << i << ": " << gs.timers[i] << "\n";

  o << "string variables:\n";
  for (uint i = 0; i < gs.svars.size(); i ++)
    o << "    " << i << ": " << gs.svars[i] << "\n";

  o << "integer variables:\n";
  for (uint i = 0; i < gs.svars.size(); i ++)
    o << "    " << i << ": " << gs.svars[i] << "\n";

  return o;
}
