/*
** oem_glk.c - OEM definitions for the GlkTADS port
**
** Notes:
**   10 Jan 99: Initial creation
*/

#include "std.h"
#include "oem.h"

#ifdef XGLK
char G_tads_oem_app_name[] = "XTADS";
#else
char G_tads_oem_app_name[] = "GlkTADS";
#endif
char G_tads_oem_display_mode[] = "text-only";
char G_tads_oem_dbg_name[] = "tdb";
char G_tads_oem_author[] = "Maintained by Stephen Granade <stephen@granades.com>\n";
int G_tads_oem_copyright_prefix = TRUE;
