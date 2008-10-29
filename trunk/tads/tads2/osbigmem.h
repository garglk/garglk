/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osbigmem.h - define "big memory" defaults for the TADS 2 compiler/
  interpreter/debugger initialization parameters
Function
  This is a common header that osxxx.h headers can include to define
  the maximum values for the memory configuration parameters for the
  TADS 2 tools.  Many modern systems have sufficient memory that there's
  no reason not to choose the largest values for these parameters.
Notes
  
Modified
  12/04/01 MJRoberts  - Creation
*/

#ifndef OSBIGMEM_H
#define OSBIGMEM_H

/*
 *   Define the largest sizes for the memory configuration parameters 
 */
#define TCD_SETTINGS_DEFINED
#define TCD_POOLSIZ  (24 * 1024)
#define TCD_LCLSIZ   (16 * 1024)
#define TCD_HEAPSIZ  65535
#define TCD_STKSIZ   512
#define TCD_LABSIZ   8192

#define TRD_SETTINGS_DEFINED
#define TRD_HEAPSIZ  65535
#define TRD_STKSIZ   512
#define TRD_UNDOSIZ  60000       /* WARNING: above 65535 will cause crashes */

#define TDD_SETTINGS_DEFINED
#define TDD_HEAPSIZ  65535
#define TDD_STKSIZ   512
#define TDD_UNDOSIZ  60000       /* WARNING: above 65535 will cause crashes */
#define TDD_POOLSIZ  (24 * 1024)
#define TDD_LCLSIZ   0

/* 
 *   define usage strings for the new defaults 
 */
#define TCD_HEAPSIZ_MSG "  -mh size      heap size (default 65535 bytes)"
#define TCD_POOLSIZ_MSG \
    "  -mp size      parse node pool size (default 24576 bytes)"
#define TCD_LCLSIZ_MSG \
    "  -ml size      local symbol table size (default 16384 bytes)"
#define TCD_STKSIZ_MSG "  -ms size      stack size (default 512 elements)"
#define TCD_LABSIZ_MSG \
    "  -mg size      goto label table size (default 8192 bytes)"

#define TRD_HEAPSIZ_MSG "  -mh size      heap size (default 65535 bytes)"
#define TRD_STKSIZ_MSG  "  -ms size      stack size (default 512 elements)"
#define TRD_UNDOSIZ_MSG \
    "  -u size       set undo to size (0 to disable; default 60000)"

#define TDD_HEAPSIZ_MSG "  -mh size      heap size (default 65535 bytes)"
#define TDD_STKSIZ_MSG  "  -ms size      stack size (default 512 elements)"
#define TDD_UNDOSIZ_MSG \
    "  -u size       set undo to size (0 to disable; default 60000)"
#define TDD_POOLSIZ_MSG \
    "  -mp size      parse pool size (default 24576 bytes)"


#endif /* OSBIGMEM_H */
