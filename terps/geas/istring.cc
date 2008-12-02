#include "istring.hh"
#include <cstring>
#include <iostream>
#include <algorithm>

using namespace std;

CI_EQUAL   ci_equal_obj;
CI_LESS    ci_less_obj;
CI_LESS_EQ ci_less_eq_obj;

// Code for testing case insensitively by John Harrison 

bool c_equal_i(char ch1, char ch2)
{
  return tolower((unsigned char)ch1) == tolower((unsigned char)ch2);
}

/*
bool ci_less_eq (char ch1, char ch2)
{
  return tolower((unsigned char)ch1) <= tolower((unsigned char)ch2);
}

bool ci_less (char ch1, char ch2)
{
  return tolower((unsigned char)ch1) < tolower((unsigned char)ch2);
}
*/

size_t ci_find(const string& str1, const string& str2)
{
  string::const_iterator pos = search (str1.begin (), str1.end (), 
				       str2.begin (), str2.end (), c_equal_i);
  if (pos == str1.end())
    return string::npos;
  else
    return pos - str1.begin ();
}

/*
uint ci_diff (const string &str1, const string &str2)
{
  if (str1.length() > str2.length()) return ci_diff (str2, str1);
  uint rv;
  for (rv = 0; rv < str1.length() && ci_equal (str1[rv], str2[rv]); rv ++)
    ;
  return rv;
}

bool cis_equal (const string &str1, const string &str2)
{
  uint pos = ci_diff (str1, str2);
  return pos == str1.length() && pos == str2.length();
}

bool cis_less_eq (const string &str1, const string &str2)
{
  uint pos = ci_diff (str1, str2);  
  return pos == str1.length() || ci_less (str1[pos], str2[pos]);
}

bool cis_less (const string &str1, const string &str2)
{
  uint pos = ci_diff (str1, str2);  

  return (pos < str1.length() && ci_less (str1[pos], str2[pos])) ||
    (pos == str1.length() && pos < str2.length());
}
*/


int my_stricmp(const char *s1, const char *s2) {
  for ( ; *s1 && *s2 && (toupper((unsigned char)*s1) ==
			 toupper((unsigned char)*s2)); ++s1, ++s2)
    ;
  return toupper((unsigned char) *s1) - toupper((unsigned char) *s2);
}

bool ci_equal    (const string &str1, const string &str2) { return my_stricmp (str1.c_str(), str2.c_str()) == 0; }
bool ci_less_eq  (const string &str1, const string &str2) { return my_stricmp (str1.c_str(), str2.c_str()) <= 0; }
bool ci_less     (const string &str1, const string &str2) { return my_stricmp (str1.c_str(), str2.c_str()) < 0; }
bool ci_notequal (const string &str1, const string &str2) { 
  std::cerr << "ci_notequal ('" << str1 << "', '" << str2 << "') -> " 
	    << ( my_stricmp (str1.c_str(), str2.c_str()) != 0) << "\n";
  return my_stricmp (str1.c_str(), str2.c_str()) != 0; }
bool ci_gt_eq  (const string &str1, const string &str2) { return my_stricmp (str1.c_str(), str2.c_str()) >= 0; }
bool ci_gt     (const string &str1, const string &str2) { return my_stricmp (str1.c_str(), str2.c_str()) > 0; }



