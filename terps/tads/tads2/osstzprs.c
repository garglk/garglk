/* Copyright (c) 2012 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osstzprs.c - portable POSIX-style "TZ" variable parser
Function
  This routine parses a POSIX-style "TZ" variable, filling in an
  os_tzinfo_t structure for os_get_timezone_info().
Notes
  
Modified
  02/13/12 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "os.h"
#include "osstzprs.h"
#include "lib.h"


/* ------------------------------------------------------------------------ */
/*
 *   Internal service routines 
 */

/* 
 *   parse a time zone name; returns the end of the name; optionally copies
 *   the string to an output buffer 
 */
static const char *parse_zone_name(char *dst, size_t dstlen,
                                   const char *p, size_t *len)
{
    /* if there's a pointer, scan the string */
    if (p != 0)
    {
        /* skip alphabetics */
        for ( ; *len != 0 && isalpha(*p) ; ++p, --*len)
        {
            /* 
             *   copy to the output if there's room for this character plus a
             *   null terminator 
             */
            if (dstlen > 1)
                *dst++ = *p;
        }
    }

    /* null-terminate the output if there's room */
    if (dstlen > 0)
        *dst = '\0';

    /* return the final pointer */
    return p;
}

/* parse digits */
static const char *parse_digits(int *val, const char *p, size_t *len)
{
    for (*val = 0 ; *len != 0 && isdigit(*p) ; ++p, --*len)
    {
        *val *= 10;
        *val += *p - '0';
    }
    return p;
}

/* 
 *   Parse an offset value - [+-]hh[:mm[:ss]]; returns a pointer to the next
 *   character after the parsed offset value, and fills in '*secs' with the
 *   number of seconds represented by the offset value.
 */
static const char *parse_offset(int *secs, const char *p, size_t *len)
{
    int sign = 1, hh = 0, mm = 0, ss = 0;
    
    /* check for a sign (which must be followed by a digit) */
    if (*len >= 2 && *p == '+' && isdigit(p[1]))
        ++p, --*len;
    else if (*len >= 2 && *p == '-' && isdigit(p[1]))
        sign = -1, ++p, --*len;

    /* if we don't have a digit, it's not a number */
    if (*len == 0 || !isdigit(*p))
        return p;

    /* parse the hours */
    p = parse_digits(&hh, p, len);

    /* check for minutes */
    if (*len >= 3 && *p == ':' && isdigit(p[1]) && isdigit(p[2]))
    {
        /* parse the minutes */
        ++p, --*len;
        p = parse_digits(&mm, p, len);

        /* check for seconds */
        if (*len >= 3 && *p == ':' && isdigit(p[1]) && isdigit(p[2]))
        {
            ++p, --*len;
            p = parse_digits(&ss, p, len);
        }
    }

    /* compute the full time in seconds, and apply the sign */
    *secs = sign*(hh*60*60 + mm*60 + ss);

    /* return the updated pointer */
    return p;
}

/*
 *   Parse a DST start/end rule 
 */
static const char *parse_dst_rule(struct os_tzrule_t *rule,
                                  const char *p, size_t *len)
{
    /* remember the starting point - we'll return this on failure */
    const char *start = p;

    /* check the date portion */
    if (*len >= 2 && (*p == 'J' || *p == 'j') && isdigit(p[1]))
    {
        /* Julian day, not counting February 29 */
        ++p, --*len;
        p = parse_digits(&rule->jday, p, len);
    }
    else if (*len >= 1 && isdigit(*p))
    {
        /* day of year, counting February 29 */
        p = parse_digits(&rule->yday, p, len);

        /* TZ uses range 0-365 for this format, but yday uses 1-366 */
        rule->yday += 1;
    }
    else if (*len >= 2 && *p == 'M' && isdigit(p[1]))
    {
        /* month.week.day format - start with the month */
        ++p, --*len;
        p = parse_digits(&rule->month, p, len);
        if (rule->month < 1 || rule->month > 12)
            return start;

        /* check for the required '.' and a following digit */
        if (*len < 2 || *p != '.' || !isdigit(p[1]))
            return start;

        /* parse the week */
        ++p, --*len;
        p = parse_digits(&rule->week, p, len);
        if (rule->week < 1 || rule->week > 5)
            return start;
        
        /* check for the required '.' and a following digit */
        if (*len < 2 || *p != '.' || !isdigit(p[1]))
            return start;
        
        /* parse the day of the week; adjust from 0-6 to our 1-7 range */
        ++p, --*len;
        p = parse_digits(&rule->day, p, len);
        rule->day += 1;
        if (rule->day < 1 || rule->day > 7)
            return start;
    }
    else
    {
        /* not a valid DST start/end format */
        return p;
    }

    /* we got the date; check for a time portion */
    if (*len >= 2 && *p == '/' && isdigit(p[1]))
    {
        /* parse the time; this has the same parsing rules as an offset */
        ++p, --*len;
        p = parse_offset(&rule->time, p, len);
    }
    else
    {
        /* the default is 2:00 AM */
        rule->time = 2*60*60;
    }

    /* success - return the updated pointer */
    return p;
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse a "TZ" environment variable string 
 */
int oss_parse_posix_tz(struct os_tzinfo_t *info, const char *tz, size_t len,
                       int west_is_positive)
{
    const char *p;
    const char *std_name = 0, *dst_name = 0;
    size_t std_name_len = 0, dst_name_len = 0;
    int std_ofs = 0, dst_ofs = 0;
    struct os_tzrule_t dst_start, dst_end;

    /* clear the daylight savings start/end dates */
    memset(&dst_start, 0, sizeof(dst_start));
    memset(&dst_end, 0, sizeof(dst_end));

    /* if there's no TZ string, return failure */
    if (tz == 0)
        return FALSE;

    /* skip leading spaces, just in case; if that leaves nothing, fail */
    for ( ; len != 0 && isspace(*tz) ; ++tz, --len) ;
    if (len == 0)
        return FALSE;

    /* parse the standard time name; if that's empty, fail */
    std_name = tz;
    tz = parse_zone_name(0, 0, tz, &len);
    if ((std_name_len = tz - std_name) == 0)
        return FALSE;

    /* parse the offset value */
    p = tz;
    if ((tz = parse_offset(&std_ofs, tz, &len)) == p)
        return FALSE;

    /* if we're using west-positive notation, convert to os_tzinfo_t specs */
    if (west_is_positive)
        std_ofs = -std_ofs;

    /* if we're not done, check for a daylight zone */
    p = tz;
    if (len != 0 && (tz = parse_zone_name(0, 0, tz, &len)) != p)
    {
        /* found a daylight zone name - note it */
        dst_name = p;
        dst_name_len = tz - p;
        
        /* 
         *   check for an optional offset value; if it's not present,
         *   daylight time is implicitly one hour ahead of standard time 
         */
        p = tz;
        if ((tz = parse_offset(&dst_ofs, tz, &len)) == p)
            dst_ofs = std_ofs + 60*60;
        else if (west_is_positive)
            dst_ofs = -dst_ofs;
    }
    else
    {
        /* there's no daylight zone, so just repeat the standard offset */
        dst_ofs = std_ofs;
    }

    /* check for the daylight time start/end dates */
    if (len != 0 && *tz == ',')
    {
        /* parse the start date */
        p = ++tz;
        --len;
        if ((tz = parse_dst_rule(&dst_start, tz, &len)) == p)
            return FALSE;

        /* if there's a start date, there has to be an end date */
        if (len == 0 || *tz != ',')
            return FALSE;

        /* parse the end date */
        p = ++tz;
        --len;
        if ((tz = parse_dst_rule(&dst_end, tz, &len)) == p)
            return FALSE;
    }
    
    /* we need to be done now, other than trailing spaces */
    for ( ; len != 0 && isspace(*tz) ; ++tz) ;
    if (len != 0)
        return FALSE;

    /* clear the result structure */
    memset(info, 0, sizeof(*info));

    /* set the offsets */
    info->std_ofs = std_ofs;
    info->dst_ofs = dst_ofs;

    /* copy the abbreviation strings */
    parse_zone_name(info->std_abbr, sizeof(info->std_abbr),
                    std_name, &std_name_len);
    parse_zone_name(info->dst_abbr, sizeof(info->dst_abbr),
                    dst_name, &dst_name_len);

    /* copy the daylight start/end rules */
    memcpy(&info->dst_start, &dst_start, sizeof(info->dst_start));
    memcpy(&info->dst_end, &dst_end, sizeof(info->dst_end));

    /* success */
    return TRUE;
}

#ifdef TEST

static void show_dst(const struct os_tzrule_t *rule)
{
    if (rule->jday != 0)
        printf(", jday %d at %d", rule->jday, rule->time);
    else if (rule->yday != 0)
        printf(", yday %d at %d", rule->yday, rule->time);
    else if (rule->month != 0)
        printf(", M%d W%d D%d at %d",
               rule->month, rule->week, rule->day, rule->time);
}

int main(int argc, char **argv)
{
    static const char *t[] = {
        "EST5",
        "EST5,",
        "EST5,M4.1.0",
        "EST5,M4.1.0,M10.5.0",
        "EST5EDT",
        "EST+5EDT",
        "EST5EDT4",
        "PST8",
        "PST8PDT",
        "PST8PDT7",
        "PST+8PDT+7",
        "PST8PDT6:30",
        "HST10",
        "CET-1",
        "CET-1CEST",
        "CET-1CEST-2",
        "GMTO0:1GST-0:59",
        "GMTO0:0:1GST-1:59:59",
        "PST8PDT,M3.5.0,M10.1.0",
        "PST8PDT,M4.1.0/2:30,M10.5.0/2:30",
        "PST8PDT,J91/3,275/0",
        "PST8PDT,J91/3,275/0x",
        0
    };
    int i;
    struct os_tzinfo_t info;

    for (i = 0 ; t[i] != 0 ; ++i)
    {
        printf("%s -> ", t[i]);
        if (oss_parse_posix_tz(&info, t[i], strlen(t[i]), TRUE))
        {
            printf("'%s' %d, '%s' %d",
                   info.std_abbr, info.std_ofs, info.dst_abbr, info.dst_ofs);
            show_dst(&info.dst_start);
            show_dst(&info.dst_end);
            printf("\n");
        }
        else
        {
            printf("invalid\n");
        }
    }
}

#endif /* TEST */
