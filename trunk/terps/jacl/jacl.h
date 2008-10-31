/* jacl.h --- Header file for all JACL code.
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <glk.h>

#include "version.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif

#ifdef WIN32
#define DIR_SEPARATOR '\\'
#define TEMP_DIR "temp\\"
#define INCLUDE_DIR "include\\"
#define CONFIG_FILE "..\\etc\\jacl.conf"
#endif

#ifndef WIN32
#define DIR_SEPARATOR '/'
#define TEMP_DIR "temp/"
#define INCLUDE_DIR "include/"
#define CONFIG_FILE "../etc/jacl.conf"
#endif
