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

#ifndef __geasfile_hh
#define __geasfile_hh

#include "general.hh"
#include <vector>
#include <string>
#include <ostream>
#include <iostream>
#include <set>
#include <map>

class GeasInterface;

class reserved_words;

struct GeasBlock
{
  ////// lname == lowercase name 
  ////// nname == normal name
  ////// parent == initial parent object (lowercased)
  // name == name
  // parent == initial parent object 
  std::string blocktype, name, parent;
  std::vector<std::string> data;
  //GeasBlock (const std::vector<std::string> &, std::string, uint, bool);
  GeasBlock () {}
};

struct GeasFile
{
  GeasInterface *gi;
  void debug_print (const std::string &s) const;

  //vector<GeasBlock> rooms, objects, textblocks, functions, procedures, types;
  //GeasBlock synonyms, game;
  std::vector <GeasBlock> blocks;

  //std::vector<GeasBlock> rooms, objects, textblocks, functions, procedures,
  //  types, synonyms, game, variables, timers, choices;
  std::map <std::string, std::string> obj_types;
  std::map <std::string, std::vector<size_t> > type_indecies;

  void register_block (const std::string &blockname, const std::string &blocktype);

  const GeasBlock &block (const std::string &type, size_t index) const;
  size_t size (const std::string &type) const;

  void read_into (const std::vector<std::string>&, const std::string &, uint, bool, const reserved_words&, const reserved_words&);



  GeasFile () {}
  explicit GeasFile (const std::vector<std::string> &in_data,
		     GeasInterface *gi);

  bool obj_has_property (const std::string &objname, const std::string &propname) const;
  bool get_obj_property (const std::string &objname, const std::string &propname,
			 std::string &rv) const;

  void get_type_property (const std::string &typenamex, const std::string &propname,
			  bool &, std::string &) const;
  bool obj_of_type (const std::string &object, const std::string &type) const;
  bool type_of_type (const std::string &subtype, const std::string &supertype) const;

  std::set<std::string> get_obj_keys (const std::string &) const;
  void get_obj_keys (const std::string &, std::set<std::string> &) const;
  void get_type_keys (const std::string &, std::set<std::string> &) const;

  bool obj_has_action (const std::string &objname, const std::string &propname) const;
  bool get_obj_action (const std::string &objname, const std::string &propname,
		       std::string &rv) const;
  void get_type_action (const std::string &typenamex, const std::string &propname,
			bool &, std::string &) const;
  std::string static_eval (const std::string &) const;
  std::string static_ivar_lookup (const std::string &varname) const;
  std::string static_svar_lookup (const std::string &varname) const;

  const GeasBlock *find_by_name (const std::string &type, const std::string &name) const;
};

std::ostream &operator << (std::ostream &, const GeasBlock &);
std::ostream &operator << (std::ostream &, const GeasFile &);



#endif
