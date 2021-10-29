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

#ifndef __geas_impl_hh
#define __geas_impl_hh

#include "GeasRunner.hh"
#include "geas-state.hh"
#include "LimitStack.hh"
#include "general.hh"

struct match_binding
{
  std::string var_name;
  std::string var_text;
  uint start, end;
  //operator std::string();
  std::string tostring();
  match_binding (std::string vn, uint i) : var_name (vn), start (i) {}
  void set (std::string vt, uint i) { var_text = vt; end = i; }
};

extern std::ostream &operator<< (std::ostream &, const match_binding &);


struct match_rv
{
  bool success;
  std::vector<match_binding> bindings;
  //match_rv (bool b, const std::vector<std::string> &v) : success(b), bindings(v) {}
  match_rv () : success(false) {}
  match_rv (bool b, const match_rv &rv) : success(b), bindings (rv.bindings) {}
  operator bool () { return success; }
};

extern std::ostream &operator<< (std::ostream &o, const match_rv &rv);
/*
  inline ostream &operator<< (ostream &o, const match_rv &rv) 
{
  //o << "match_rv {" << (rv.success ? "TRUE" : "FALSE") << ": " << rv.bindings << "}"; 
  o << "match_rv {" << (rv.success ? "TRUE" : "FALSE") << ": [";
  //o << rv.bindings.size();
  //o << rv.bindings;
  for (uint i = 0; i < rv.bindings.size(); i ++)
    o << rv.bindings[i] << ", ";
  o << "]}"; 
  return o; 
}
*/

class geas_implementation : public GeasRunner
{
  //GeasInterface *gi;
  GeasFile gf;
  //bool running;
  bool dont_process, outputting;
  LimitStack <GeasState> undo_buffer;
  std::vector <std::string> function_args;
  std::string this_object;
  v2string current_places;
  bool is_running_;
  Logger logger;
  
public:
  geas_implementation (GeasInterface *in_gi)
     : GeasRunner (in_gi), undo_buffer (20), is_running_(true) {}
  //void set_game (std::string s);
  void set_game (const std::string &s);

  bool is_running () const;
  std::string get_banner ();
  void run_command (const std::string &);
  bool try_match (std::string s, bool, bool);
  match_rv match_command (std::string input, std::string action) const;
  match_rv match_command (std::string input, uint ichar,
			  std::string action, uint achar, match_rv rv) const;
  bool dereference_vars (std::vector<match_binding> &bindings, bool is_internal) const;
  bool dereference_vars (std::vector<match_binding>&, const std::vector<std::string>&, bool is_internal) const;
  bool match_object (const std::string &text, const std::string &name, bool is_internal = false) const;
  void set_vars (const std::vector<match_binding> &v);
  bool run_commands (std::string, const GeasBlock *, bool is_internal = false);

  void display_error (std::string errorname, std::string object = "");

  std::string substitute_synonyms (std::string) const;

  void set_svar (const std::string &, const std::string &);
  void set_svar (const std::string &, size_t, const std::string &);
  void set_ivar (const std::string &, int);
  void set_ivar (const std::string &, size_t, int);

  std::string get_svar (const std::string &) const;
  std::string get_svar (const std::string &, size_t) const;
  int get_ivar (const std::string &) const;
  int get_ivar (const std::string &, size_t) const;

  bool find_ivar (const std::string &, size_t &) const;
  bool find_svar (const std::string &, size_t &) const;

  void regen_var_look ();
  void regen_var_dirs ();
  void regen_var_objects ();
  void regen_var_room ();

  void look();

  std::string displayed_name (const std::string &object) const;
  //std::string get_obj_name (const std::vector<std::string> &args) const;
  std::string get_obj_name (const std::string &name, const std::vector<std::string> &where, bool is_internal) const;

  bool has_obj_property (const std::string &objname, const std::string &propname) const;
  bool get_obj_property (const std::string &objname, const std::string &propname,
			 std::string &rv) const;
  bool has_obj_action (const std::string &obj, const std::string &prop) const;
  bool get_obj_action (const std::string &objname, const std::string &actname,
		       std::string &rv) const;
  std::string exit_dest (const std::string &room, const std::string &dir, bool *is_act = NULL) const;
  std::vector<std::vector<std::string> > get_places (const std::string &room);

  void set_obj_property (const std::string &obj, const std::string &prop);
  void set_obj_action (const std::string &obj, const std::string &act);
  void move (const std::string &obj, const std::string &dest);
  void goto_room (const std::string &room);
  std::string get_obj_parent (const std::string &obj);
  
  void print_eval (const std::string &);
  void print_eval_p (const std::string &);
  std::string eval_string (const std::string &s);
  std::string eval_param (const std::string &s) { assert (is_param(s)); return eval_string (param_contents(s)); }


  void run_script_as (const std::string &, const std::string &);
  void run_script (const std::string &);
  void run_script (const std::string &, std::string &);
  void run_procedure (const std::string &);
  void run_procedure (const std::string &, std::vector<std::string> args);
  std::string run_function (const std::string &);
  std::string run_function (const std::string &, std::vector<std::string> args);
  std::string bad_arg_count (const std::string &);

  bool eval_conds (const std::string &);
  bool eval_cond (const std::string &);
  GeasState state;

  virtual void tick_timers();
  virtual v2string get_inventory();
  virtual v2string get_room_contents();
  v2string get_room_contents(const std::string &);
  virtual vstring get_status_vars();
  virtual std::vector<bool> get_valid_exits();


  inline void print_formatted (const std::string &s) const { if (outputting) gi->print_formatted(s); }
  inline void print_normal (const std::string &s) const { if (outputting) gi->print_normal(s); }
  inline void print_newline () const { if (outputting) gi->print_newline(); }

  /*
  inline void print_formatted (std::string s) const {
    if (outputting)
      gi->print_formatted(s); 
    else
      gi->print_formatted ("{{" + s + "}}");
  }
  inline void print_normal (std::string s) const
  {
    if (outputting)
      gi->print_normal (s);
    else
      gi->print_normal("{{" + s + "}}");
  }
  inline void print_newline() const { 
    if (outputting)
      gi->print_newline();
    else
      gi->print_normal ("{{|n}}");
  }
  */
};

#endif
