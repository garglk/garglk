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

#include "geas-util.hh"
#include <sstream>
#include <cctype>
#include "general.hh"
#include "istring.hh"

using namespace std;

int eval_int (const string &s)
{
  cerr << "eval_int (" << s << ")" << endl;

  uint index = 0, index2;
  string tmp;
  while (index < s.length() && isspace (s[index]))
    {
      cerr << "  index == " << index << endl;
      index ++;
    }
  if (index == s.length() || !isdigit (s[index]))
    {
      cerr << "Failed to match, returning 0" << endl;
      return 0;
    }
  for (index2 = index; index2 < s.length() && isdigit (s[index2]); index2 ++)
    {
      cerr << "  index2 == " << index2 << endl;
    }
  //;
  tmp = s.substr (index, index2 - index);
  cerr << "tmp == < " << tmp << ">" << endl;

  //cerr << "index == " << index << ", index2 == " << index2 
  //     << ", tmp == " << tmp << endl;

  int arg1 = atoi (tmp.c_str());
  cerr << "arg1 == " << arg1 << endl;
  index = index2;
  while (index < s.length() && isspace (s[index]))
    ++ index;
  if (index == s.length())
    return arg1;

  //cerr << "index == " << index << ", s.length() == " << s.length() << endl;

  char symbol = s[index];

  //cerr << "symbol == " << symbol << "; find --> " 
  //     << string("+-*/").find (symbol) << endl;

  if (string ("+-*/").find (symbol) == string::npos)
    return arg1;

  ++ index;
  while (index < s.length() && isspace (s[index]))
    ++ index;
  if (index == s.length() || ! isdigit (s[index]))
    {
      if (symbol == '*')
	return 0;
      return arg1;
    }
  index2 = index + 1;
  while (index2 < s.length() && isdigit (s[index2]))
    ++ index2;
  tmp = s.substr (index, index2 - index);
  int arg2 = atoi (tmp.c_str());
  
  switch (symbol)
    {
    case '+': return arg1 + arg2;
    case '-': return arg1 - arg2;
    case '*': return arg1 * arg2;
    case '/': return arg1 / arg2; 
      // TODO: division should use accountant's round
    }
  return 0;
}

string trim_braces (const string &s)
{
  if (s.length() > 1 && s[0] == '[' && s[s.length() - 1] == ']')
    return s.substr (1, s.length() - 2);
  else
    return s;
}

bool is_param (const string &s)
{
  return s.length() > 1 && s[0] == '<' && s[s.length() - 1] == '>';
}

string param_contents (const string &s)
{
  //cerr << "param_contents (" << s << ")" << endl;
  assert (is_param(s));
  return s.substr (1, s.length() - 2);
}

string nonparam (const string &type, const string &var)
{
  return "Non-parameter for " + type + " in \"" + var + "\"";
}

//ostream &operator << (ostream &o, const GeasBlock &gb) { return o; }
//string trim (string s, trim_modes) { return s; }

std::string string_geas_block (const GeasBlock &gb)
{
  ostringstream oss;
  oss << gb;  // temporary removed TODO
  return oss.str();
}


bool starts_with (const string &a, const string &b)
{
  return (a.length() >= b.length()) && (a.substr (0, b.length()) == b);
}
bool ends_with (const string &a, const string &b)
{
  return (a.length() >= b.length()) && 
    (a.substr (a.length() - b.length(), b.length()) == b);
}

bool starts_with_i (const string &a, const string &b)
{
  return (a.length() >= b.length()) && ci_equal (a.substr (0, b.length()), b);
  //  return starts_with (lcase(a), lcase(b));
}
bool ends_with_i (const string &a, const string &b)
{
  return (a.length() >= b.length()) && 
    ci_equal (a.substr (a.length() - b.length(), b.length()), b);
  //return ends_with (lcase(a), lcase(b));
}

string pcase (string s)
{
  if (s.length() == 0)
    return s;
  if (islower (s[0]))
    s[0] = toupper(s[0]);
  return s;
}

string ucase (string s)
{
  for (uint i = 0; i < s.length(); i ++)
    s[i] = toupper (s[i]);
  return s;
}

// There's a good chance s is already all-lowercase, in which case
// the test will avoid making a copy
string lcase (string s)
{
  for (uint i = 0; i < s.length(); i ++)
    if (isupper (s[i]))
      s[i] = tolower (s[i]);
  return s;
}

vector<string> split_param (const string &s)
{
  vector<string> rv;
  std::string::size_type c1 = 0, c2;

  for (;;)
    {
      c2 = s.find (';', c1);
      if (c2 == string::npos)
	{
	  rv.push_back (trim (s.substr (c1)));
	  return rv;
	}
      rv.push_back (trim (s.substr (c1, c2-c1)));
      c1 = c2 + 1;
    }
}

vector<string> split_f_args (const string &s)
{
  vector<string> rv = split_param (s);
  for (auto &i: rv)
    {
      const string &tmp = i;
      if (tmp[0] == '_')
	{
	  i[0] = ' ';
	}
      if (tmp[tmp.length() - 1] == '_')
	{
	  i[tmp.length() - 1] = ' ';
	}
    }
  return rv;
}

void show_split (const string &s)
{
  vector<string> tmp = split_param (s);
  cerr << "Splitting <" << s << ">: ";
  for (const auto &i: tmp)
    cerr << "<" << i << ">, ";
  cerr << "\n";
}

Logger::Nullstreambuf Logger::cnull;

Logger::Logger ()
    : logfilestr_(NULL), cerrbuf_(NULL)
{
  cerr.flush ();

  const char *const logfile = getenv ("GEAS_LOGFILE");
  if (logfile)
    {
      ofstream *filestr = new ofstream (logfile);
      if (filestr->fail ())
        delete filestr;
      else
        {
          logfilestr_ = filestr;
          cerrbuf_ = cerr.rdbuf (filestr->rdbuf ());
        }
    }

  if (!cerrbuf_)
    cerrbuf_ = cerr.rdbuf (&cnull);
}

Logger::~Logger () {
  cerr.flush ();

  cerr.rdbuf (cerrbuf_);
  cerrbuf_ = NULL;

  if (logfilestr_)
    {
      logfilestr_->close ();
      delete logfilestr_;
      logfilestr_ = NULL;
    }
}
