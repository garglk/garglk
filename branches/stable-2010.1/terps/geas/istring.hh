/* istring.hh
 * 
 * Case-insensitive string
 */

#ifndef __istring_hh
#define __istring_hh

#include <string>
#include <cctype>

//bool   ci_equal    (char ch1, char ch2);
//bool   ci_less_eq  (char ch1, char ch2);
//bool   ci_less     (char ch1, char ch2);
//size_t ci_find     (const std::string &str1, const std::string &str2);
//size_t ci_diff     (const std::string &str1, const std::string &str2);
//bool   cis_equal   (const std::string &str1, const std::string &str2);
//bool   cis_less_eq (const std::string &str1, const std::string &str2);
//bool   cis_less    (const std::string &str1, const std::string &str2);
  
bool c_equal_i (char ch1, char ch2);
size_t ci_find      (const std::string &str1, const std::string &str2);
bool   ci_equal    (const std::string &str1, const std::string &str2);
bool   ci_less_eq  (const std::string &str1, const std::string &str2);
bool   ci_less     (const std::string &str1, const std::string &str2);
bool   ci_notequal (const std::string &str1, const std::string &str2);
bool   ci_gt_eq    (const std::string &str1, const std::string &str2);
bool   ci_gt       (const std::string &str1, const std::string &str2);

class CI_EQUAL {
public:
  bool operator() (const std::string &str1, const std::string &str2)
  { return ci_equal (str1, str2); }
};

class CI_LESS_EQ {
public:
  bool operator() (const std::string &str1, const std::string &str2)
  { return ci_less_eq (str1, str2); }
};

class CI_LESS {
public:
  bool operator() (const std::string &str1, const std::string &str2)
  { 
    return ci_less (str1, str2);
  }
};

extern CI_EQUAL   ci_equal_obj;
extern CI_LESS    ci_less_obj;
extern CI_LESS_EQ ci_less_eq_obj;

#endif
