/* Copyright (c) 2012 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osstzprs.h - portable POSIX-style "TZ" variable parser
Function
  This routine parses a POSIX-style "TZ" variable, filling in an
  os_tzinfo_t structure for os_get_timezone_info().  This is a portable
  routine designed as a helper for implementing os_get_timezone_info() on
  platforms where POSIX TZ strings can be used.  Many Unix platforms use this
  convention.

  We parse the following formats:

    EST5       - time zone with standard time only, no daylight time
    EST5EDT    - standard plus daylight time, with implied 1:00 daylight offset
    EST5EDT4   - standard plus daylight time, with explicit daylight offset
    EST5:00    - offset can include minutes
    EST5:00:00 - ...and seconds
    EST5EDT,M3.2.0,M11.1.0  - adds rules for daylight time start/stop dates

  This isn't a full drop-in replacement for os_get_timezone_info(), since
  (a) we don't want to assume that the Unix getenv() API is the way (or the
  only way) to get the string to parse, and (b) some platforms that use the
  TZ format might also accept other formats or have other ways of setting
  the timezone.  For Unix-type platforms platforms that don't need to check
  for other syntax, this should be all you need to do to implement
  os_get_timezone_info():

    #include "osstzprs.h"
    int os_get_timezone_info(struct os_tzinfo_t *info)
    {
      const char *tz = getenv("TZ");
      return tz != 0 ? oss_parse_posix_tz(info, tz, strlen(tz), TRUE) : FALSE;
    }

Notes
  
Modified
  02/13/12 MJRoberts  - Creation
*/

#ifndef OSSTZPRS_H
#define OSSTZPRS_H

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *   Parse a string using the syntax of the POSIX "TZ" environment variable.
 *   If successful, fills in *info and returns true; if the string isn't in
 *   one of the recognized formats, returns false.
 *   
 *   'west_is_positive' says whether UTC offsets are positive or negative to
 *   the west.  The POSIX TZ convention is that west is positive, as in
 *   EST5EDT (New York time, 5 hours west of UTC), so this should be set to
 *   TRUE if the source material actually comes from a TZ variable.  The
 *   reason we offer this option at all is that the zoneinfo database uses
 *   the opposite convention, where New York would be EST-5EDT.
 */
int oss_parse_posix_tz(struct os_tzinfo_t *info,
                       const char *tz, size_t len,
                       int west_is_positive);

#ifdef __cplusplus
}
#endif

#endif /* OSSTZPRS_H */
