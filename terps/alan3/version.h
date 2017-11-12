#ifndef _version_h_
#define _version_h_ 1

#include <time.h>

typedef struct {
  char*  string;
  int    version;
  int    revision;
  int    correction;
  time_t time;
  char*  state;
} Version;

typedef struct {
  char*   name;
  char*   slogan;
  char*   shortHeader;
  char*   longHeader;
  char*   date;
  char*   time;
  char*   user;
  char*   host;
  char*   ostype;
  Version version;
} Product;

#endif
