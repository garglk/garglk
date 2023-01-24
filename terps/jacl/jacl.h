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
#include <string.h>

#ifdef __NDS__
#include <nds.h>
#include <fat.h>
#endif

#ifdef GLK
#include <glk.h>
#include "gi_blorb.h"
#endif

#ifdef GARGLK
#include "garglk.h"
#endif

#ifdef FCGIJACL
#include <fcgi_stdio.h>
#endif

#include "version.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif

#ifdef WIN32
#define DIR_SEPARATOR '\\'
#define TEMP_DIR "temp\\"
#define DATA_DIR "data\\"
#define INCLUDE_DIR "include\\"
#endif

#ifndef WIN32
#define DIR_SEPARATOR '/'
#define DATA_DIR "data/"
#define TEMP_DIR "temp/"
#define INCLUDE_DIR "include/"
#endif

#ifdef __APPLE__
#define get_string jacl_get_string
#endif
