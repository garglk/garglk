#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2012 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmdate.cpp - CVmObjDate object
Function
  Implements the Date intrinsic class
Notes
  
Modified
  01/23/12 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>

#include "t3std.h"
#include "vmdate.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmmcreg.h"
#include "vmfile.h"
#include "vmbignum.h"
#include "vmbif.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmvec.h"
#include "vmlookup.h"
#include "utf8.h"
#include "vmuni.h"
#include "vmtz.h"
#include "vmtzobj.h"


/* ------------------------------------------------------------------------ */
/*
 *   Round a double to int32 
 */
static int32_t round_int32(double d)
{
    double i;
    return (int32_t)(modf(d, &i) >= 0.5
                     ? (d >= 0 ? ceil(d) : floor(d))
                     : (d < 0 ? ceil(d) : floor(d)));
}


/* ------------------------------------------------------------------------ */
/*
 *   get the current date/time in UTC
 */
void caldate_t::now(int32_t &dayno, int32_t &daytime)
{
    /* get the current date and time in Unix Epoch format */
    os_time_t s;
    long ns;
    os_time_ns(&s, &ns);

    /* 
     *   's' is the number of seconds since the Unix Epoch, 1/1/1970 UTC.
     *   Get our day number: divide by seconds per day to get days past the
     *   Unix Epoch, then adjust to our Epoch (3/1/0000 UTC) by adding the
     *   number of days from our Epoch to the Unix Epoch. 
     */
    dayno = (int32_t)(s/(24*60*60) + caldate_t::UNIX_EPOCH_DAYNO);

    /* 
     *   convert 's' to the time of day, adding in the fractional portion
     *   from 'ns' but only keeping millisecond precision 
     */
    daytime = (uint32_t)((s % (24*60*60))*1000 + ns/1000000);
}


/* 
 *   normalize a date/time value: if the daytime value is before 0000 hours
 *   or after 2400 hours, bring it back within bounds and adjust the day
 *   number 
 */
void caldate_t::normalize(int32_t &dayno, int32_t &daytime)
{
    /* if we're outside the 0000-2400 bounds, adjust the time and day */
    ldiv_t q = ldiv(daytime, 24*60*60*1000);
    dayno += (int32_t)q.quot;
    if ((daytime = (int32_t)q.rem) < 0)
    {
        daytime += 24*60*60*1000;
        dayno -= 1;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Locale string retrieval
 */
class CVmDateLocale
{
public:
    CVmDateLocale(VMG0_)
    {
        /* get the state Vector from the Date static class state holder */
        const vm_val_t *val = vm_classobj_for(CVmObjDate)->get_class_state();
        vecid = val->typ == VM_OBJ ? val->val.obj : VM_INVALID_OBJ;
        vec = vm_objid_cast(CVmObjVector, vecid);
    }

    /* get a locale string */
    const char *get(VMG_ size_t &len, int idx)
    {
        /* if it's negative, it's invalid */
        if (idx < 0)
        {
            len = 0;
            return 0;
        }

        /* try getting it from the state vector */
        if (vec != 0 && idx < (int)vec->get_element_count())
        {
            /* get the element */
            vm_val_t ele;
            vec->get_element(idx, &ele);

            /* if it's a string, return it */
            const char *str = ele.get_as_string(vmg0_);
            if (str != 0)
            {
                len = vmb_get_len(str);
                return str + VMB_LEN;
            }
        }

        /* we didn't find it in the state vector; return the default */
        if (idx < ndefaults)
        {
            len = strlen(defaults[idx]);
            return defaults[idx];
        }
        
        /* invalid entry */
        len = 0;
        return 0;
    }

    /* 
     *   Index a locale list.  If 'sticky' is true, the last item actually
     *   present in the list will be returned if the index is past the end of
     *   the list. 
     */
    const char *index_list(VMG_ size_t &itemlen, int item, int idx, int sticky)
    {
        /* get the item */
        size_t len;
        const char *str = get(vmg_ len, item);

        /* if we didn't find it, return failure */
        if (str == 0)
        {
            itemlen = 0;
            return 0;
        }

        /* skip items (delimited by commas) until we reach the target index */
        for ( ; idx > 0 && len != 0 ; --idx, ++str, --len)
        {
            /* find the next comma */
            const char *comma = lib_strnchr(str, len, ',');
            if (comma == 0)
            {
                /* 
                 *   No more commas, so the index is past the end of the
                 *   list.  If 'sticky' is true, use the last item; otherwise
                 *   return "not found".
                 */
                if (sticky)
                    break;
                else
                {
                    itemlen = 0;
                    return 0;
                }
            }
            
            /* advance to the comma */
            len -= comma - str;
            str = comma;
        }
        
        /* we have the item; measure its length, up to the next '=' or ',' */
        const char *p;
        for (p = str, itemlen = 0 ; len != 0 && *p != '=' && *p != ',' ;
             --len, ++p, ++itemlen) ;
        
        /* return the item pointer */
        return str;
    }

    /* set a locale string */
    void set(VMG_ CVmUndo *undo, int idx, const vm_val_t *val)
    {
        /* if we don't have a vector, create one */
        if (vec == 0)
        {
            /* create the vector */
            vecid = CVmObjVector::create(vmg_ FALSE, ndefaults);
            vec = vm_objid_cast(CVmObjVector, vecid);

            /* initialize all elements to nil */
            vm_val_t nil;
            nil.set_nil();
            for (int i = 0 ; i < ndefaults ; ++i)
                vec->append_element(vmg_ vecid, &nil);

            /* store the vector in the class object's state holder */
            vm_val_t val;
            val.set_obj(vecid);
            vm_classobj_for(CVmObjDate)->set_class_state(&val);
        }

        /* if it's out of bounds, it's an error */
        if (idx < 0 || idx >= ndefaults)
            err_throw(VMERR_INDEX_OUT_OF_RANGE);

        /* set the specified element */
        vec->set_element_undo(vmg_ vecid, idx, val);
    }

    /* defaults */
    static const char *defaults[];
    static const int ndefaults;

    /* our state Vector */
    vm_obj_id_t vecid;
    CVmObjVector *vec;
};

/* list of default locale settings */
const char *CVmDateLocale::defaults[] =
{
    /* 0 - month full names */
    "January,February,March,April,May,June,July,August,"
    "September,October,November,December",

    /* 1 - month name abbreviations */
    "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep=Sept,Oct,Nov,Dec",

    /* 2 - weekday full names */
    "Sunday,Monday,Tuesday,Wednesday,Thursday,Friday,Saturday",

    /* 3 - weekday abbreviations */
    "Sun,Mon,Tue,Wed,Thu,Fri,Sat",

    /* 4 - AM/PM indicators (upper, lower) */
    "AM=A.M.,PM=P.M.,am,pm",

    /* 5 - era (AD/BC) indicators */
    "AD=A.D.=CE=C.E.,BC=B.C.=BCE=B.C.E.",

    /* 6 - parsing format filter - 'us' or 'eu' */
    "us",

    /* 7 - ordinal suffixes */
    "st,nd,rd,th,st,nd,rd",

    /* 8 - timestamp format string */
    "%a %b %#d %T %Y",

    /* 9 - time format string */
    "%H:%M:%S",

    /* 10 - default date format */
    "%m/%d/%Y",

    /* 11 - short date format */
    "%m/%d/%y",

    /* 12 - 12-hour clock format */
    "%#I:%M:%S %P",

    /* 13 - 24-hour clock format */
    "%H:%M",

    /* 14 - 24-hour clock with seconds */
    "%H:%M:%S"
};
const int CVmDateLocale::ndefaults = countof(CVmDateLocale::defaults);

/* locale indices */
static const int LC_MONTH = 0;
static const int LC_MON = 1;
static const int LC_WEEKDAY = 2;
static const int LC_WKDY = 3;
static const int LC_AMPM = 4;
static const int LC_ERA = 5;
static const int LC_PARSE_FILTER = 6;
static const int LC_ORDSUF = 7;
static const int LC_FMT_TIMESTAMP = 8;
static const int LC_FMT_TIME = 9;
static const int LC_FMT_DATE = 10;
static const int LC_FMT_SHORTDATE = 11;
static const int LC_FMT_12HOUR = 12;
static const int LC_FMT_24HOUR = 13;
static const int LC_FMT_24HOURSECS = 14;


/*
 *   Figure the ordinal suffix index (in LC_ORDSUF) for a given number.  The
 *   suffix indices are:
 *   
 *.     0 = 1st
 *.     1 = 2nd
 *.     2 = 3rd
 *.     3 = Nth
 *.     4 = X1st (e.g., 21st, 31st, 121st)
 *.     5 = X2nd
 *.     6 = X3rd
 */
static int ordinal_index(int n)
{
    /* get the last digit */
    int lastdig = n % 10;

    /* 
     *   if it's in the teens, or the last digit is 0 or 4-9, use the generic
     *   Nth suffix 
     */
    if (lastdig == 0 || lastdig >= 4 || (n > 10 && n < 20))
        return 3;

    /* if it's in the 1-3 range, use 1st/2nd/3rd; otherwise use X1st/etc */
    return lastdig + (n < 10 ? -1 : 3);
}


/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_date_ext *vm_date_ext::alloc_ext(VMG_ CVmObjDate *self)
{
    /* calculate how much space we need */
    size_t siz = sizeof(vm_date_ext);

    /* allocate the memory */
    vm_date_ext *ext = (vm_date_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjDate object statics 
 */

/* metaclass registration object */
static CVmMetaclassDate metaclass_reg_obj;
CVmMetaclass *CVmObjDate::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjDate::*CVmObjDate::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjDate::getp_undef,                                           /* 0 */
    &CVmObjDate::getp_parseDate,                                       /* 1 */
    &CVmObjDate::getp_parseJulianDate,                                 /* 2 */
    &CVmObjDate::getp_formatDate,                                      /* 3 */
    &CVmObjDate::getp_formatJulianDate,                                /* 4 */
    &CVmObjDate::getp_compareTo,                                       /* 5 */
    &CVmObjDate::getp_getDate,                                         /* 6 */
    &CVmObjDate::getp_getJulianDay,                                    /* 7 */
    &CVmObjDate::getp_getJulianDate,                                   /* 8 */
    &CVmObjDate::getp_getISOWeekDate,                                  /* 9 */
    &CVmObjDate::getp_getTime,                                        /* 10 */
    &CVmObjDate::getp_addInterval,                                    /* 11 */
    &CVmObjDate::getp_findWeekday,                                    /* 12 */
    &CVmObjDate::getp_setLocaleInfo                                   /* 13 */
};

/* static property indices */
const int VMDATE_PARSEDATE = 1;
const int VMDATE_PARSEJULIANDATE = 2;
const int VMDATE_SETLOCALINFO = 13;

/* ------------------------------------------------------------------------ */
/*
 *   Construction
 */
CVmObjDate::CVmObjDate(VMG_ int32_t dayno, uint32_t daytime)
{
    /* allocate our extension structure */
    vm_date_ext *ext = vm_date_ext::alloc_ext(vmg_ this);
    ext_ = (char *)ext;

    /* fill it in */
    ext->dayno = dayno;
    ext->daytime = daytime;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get my date and time in the given local time zone 
 */
const char *CVmObjDate::get_local_time(
    int32_t &dayno, int32_t &daytime, int32_t &ofs, CVmTimeZone *tz) const
{
    /* get my UTC date and time */
    dayno = get_ext()->dayno;
    daytime = get_ext()->daytime;

    /* convert to my local time zone */
    return tz->utc_to_local(dayno, daytime, ofs);
}

/* ------------------------------------------------------------------------ */
/*
 *   Date/time parser 
 */

/* string capture item */
struct date_parse_string
{
    date_parse_string() { p = 0; len = 0; }
    void set(const char *p, size_t len) { this->p = p; this->len = len; }
    const char *p;
    size_t len;
};

/*
 *   Parse results structure.  This keeps track of which information we've
 *   filled in so far.
 */
struct date_parse_result
{
    date_parse_result(multicaldate_t *cal)
    {
        /* remember my calendar interface */
        this->cal = cal;
        
        /* 
         *   Initialize most elements to invalid, out-of-bounds values to
         *   indicate they haven't been set yet.  INT_MIN satisfies this for
         *   all of these, so for the sake of clarity use it for all of them.
         *   For the month and day, only positive values are meaningful, so
         *   any negative value will suffice as the "invalid" token.  The
         *   hour, minute, second, and milliseconds must be non-negative to
         *   be valid, so any negative value is invalid.  The time zone
         *   offset must be between -24 hours (-24*60 minutes) and +24 hours
         *   (+24*60 minutes), so a large negative value is invalid.  The
         *   year can mathematically be any integer value, but the ultimate
         *   representation of the date can only hold a 32-bit day number,
         *   which limits the year number to about +/-5 million; so INT_MIN,
         *   which is about minus 2 billion on a 32-bit machine, is a
         *   negative year that we can't represent.  (This shouldn't be a big
         *   loss; prehistoric dates aren't very meaningful anyway, as there
         *   are by definition no historical records to correlate with
         *   calculated dates.)  The timezone pointer is different in that
         *   it's a pointer, so null is the natural 'undefined' there.
         */
        era = 0;
        yy = INT_MIN;
        yy_needs_century = FALSE;
        mm = INT_MIN;
        dd = INT_MIN;

        ampm = 0;
        hh = INT_MIN;
        mi = INT_MIN;
        ss = INT_MIN;
        ms = INT_MIN;
        tzofs = INT_MIN;
        w = INT_MIN;
        tz = 0;
    }

    date_parse_result(date_parse_result &src)
    {
        memcpy(this, &src, sizeof(*this));
    }

    /* translate a time from local to UTC using our parsed timezone info */
    void local_to_utc(int32_t &dayno, int32_t &daytime)
    {
        /*   
         *   If we have a specific timezone offset, use the offset; otherwise
         *   translate via the time zone object.
         *   
         *   Note that it's possible for a parsed date to specify both an
         *   offset and a zone.  If a seasonal zone abbreviation like "PDT"
         *   is used, it tells us both the timezone (America/Los_Angeles in
         *   the case of PDT) and the offset (-7 hours for PDT).  The offset
         *   overrides the implied offset for the timezone because it
         *   specifies a particular seasonal time setting; if only the
         *   timezone is specified, we use the wall clock time for the zone.
         *   For example, "12/1/2000 3:00 America/Los_Angeles" has a -8 hour
         *   offset, since Pacific Standard Time is in effect in this zone in
         *   December, whereas "12/1/2000 3:00 PDT" has a -7 hour offset,
         *   since it's specfically a PDT time even though the wall clock
         *   time in Los Angeles in December is Pacific Standard Time.
         */
        if (has_tzofs())
        {
            /* we have an offset - it overrides the time zone */
            daytime -= tzofs;
            caldate_t::normalize(dayno, daytime);
        }
        else
        {
            /* no offset, so use the timezone */
            tz->local_to_utc(dayno, daytime);
        }
    }

    /* translate from UTC to local using our parsed timezone info */
    void utc_to_local(int32_t &dayno, int32_t &daytime)
    {
        if (has_tzofs())
        {
            daytime += tzofs;
            caldate_t::normalize(dayno, daytime);
        }
        else
        {
            int32_t ofs;
            tz->utc_to_local(dayno, daytime, ofs);
        }
    }

    /* 
     *   set a default date - this sets any undefined date components
     *   individually from the given date 
     */
    void def_date(int32_t dayno)
    {
        /* convert the day number to a calendar date */
        cal->set_dayno(dayno);

        /* apply the era to the year, if both are present */
        if (era != 0 && has_year())
        {
            switch (era)
            {
            case 1:
                /* 
                 *   astronomical '+' notation - the internal year number
                 *   matches the nominal year 
                 */
                break;
                
            case 2:
                /* 
                 *   AD - the internal year matches the nominal year, but an
                 *   explicit AD overrides the need for a century; e.g., AD
                 *   87 is pretty clear that we're talking about an
                 *   historical date in the first century 
                 */
                yy_needs_century = FALSE;
                break;
                
            case -1:
                /* 
                 *   astronomical '-' notation - the internal year number
                 *   matches the nominal negative year number, since we use
                 *   the same notation internally
                 */
                yy = -yy;
                break;
                
            case -2:
                /* BC - 1 BC is internal year 0, 2 BC is year -1, etc */
                yy = -yy + 1;
                yy_needs_century = FALSE;
                break;
            }
        }

        /* set the default year, month, and day from the given date */
        def_year(cal->y());
        def_month(cal->m());
        def_day(cal->d());

        /* 
         *   If the year was specified without a century, choose the century
         *   such that it yields the year nearest to the current year.  This
         *   is equivalent to finding the year within 50 years of the current
         *   year.  For 2012, this makes the range 1962 to 2061 - so '62 is
         *   taken to be 1962, '85 is 1985, but '15 is 2015, and 61 is 2061.
         */
        if (yy_needs_century)
        {
            yy += ((cal->y() / 100) * 100);
            if (yy < cal->y() - 50)
                yy += 100;
            if (yy > cal->y() + 49)
                yy -= 100;
        }
    }

    /* 
     *   set a default time - this sets any undefined time components
     *   individually from the given time 
     */
    void def_time(int m, int h, int s, int ms)
    {
        /* apply the PM indicator to the time, if present and in range */
        if (ampm == 1 && hh == 12)
            hh = 0;
        else if (ampm == 2 && hh < 12)
            hh += 12;
        
        def_hour(h);
        def_minute(m);
        def_second(s);
        def_msec(ms);
    }

    /* compute the day number */
    int32_t dayno() const
    {
        /* if there's an ISO weekday, convert it to a calendar date */
        if (has_iso_week() && has_year())
        {
            /* 
             *   Calculate the "correction factor" - this is the ISO weekday
             *   number of January 4 of the year, plus 3.  Note that this is
             *   explicitly defined as Jan 4 *Gregorian* by the standard, so
             *   this isn't relative to the calendar we're parsing for.
             */
            caldate_t jan4(yy, 1, 4);
            int corr = jan4.iso_weekday() + 3;

            /* 
             *   Set the date to "January 'ord'", where 'ord' is the ISO
             *   ordinal day number calculated from the weekday and
             *   correction factor.  This won't necessarily be a valid date
             *   in January, but caldate_t handles overflows in the day by
             *   carrying into the month and year as needed.
             */
            caldate_t cd(yy, 1, w - corr);
            return cd.dayno();
        }
        else
        {
            /* set the calendar date to our year, month, and day */
            cal->set_date(yy, mm, dd);

            /* return the day number from the calendar date */
            return cal->dayno();
        }
    }

    /* compute the time of day */
    int32_t daytime() const
    {
        return hh*60*60*1000 + mi*60*1000 + ss*1000 + ms;
    }

    /*
     *   Set defaults for the various components.  These will set the
     *   component only if it hasn't been set to a valid value already. 
     */
    void def_year(int y) { if (!has_year()) yy = y; }
    void def_month(int m) { if (!has_month()) mm = m; }
    void def_day(int d) { if (!has_day()) dd = d; }

    void def_hour(int h) { if (!has_hour()) hh = h; }
    void def_minute(int m) { if (!has_minute()) mi = m; }
    void def_second(int s) { if (!has_second()) ss = s; }
    void def_msec(int m) { if (!has_msec()) ms = m; }

    void def_tzofs(int t) { if (!has_tzofs()) tzofs = t; }
    void def_tz(CVmTimeZone *z) { if (!has_tz()) tz = z; }

    /* default to the local time zone */
    void def_local_tz(VMG0_)
    {
        if (!has_tz())
            tz = G_tzcache->get_local_zone(vmg0_);
    }

    /* determine if the various elements are set */
    int has_era() const { return era != 0; }
    int has_year() const { return yy != INT_MIN; }
    int has_month() const { return mm != INT_MIN; }
    int has_day() const { return dd != INT_MIN; }
    int has_iso_week() const { return w != INT_MIN; }

    int has_ampm() const { return ampm != 0; }
    int has_hour() const { return hh != INT_MIN; }
    int has_minute() const { return mi != INT_MIN; }
    int has_second() const { return ss != INT_MIN; }
    int has_msec() const { return ms != INT_MIN; }

    int has_tzofs() const { return tzofs != INT_MIN; }
    int has_tz() const { return tz != 0; }

    /* the calendar we're using to parse the date */
    multicaldate_t *cal;

    /* era: 1 = '+', 2 = AD, -1 = '-', -2 = BC */
    int era;

    /* date - year, month, day */
    int yy;
    int mm;
    int dd;

    /* 
     *   ISO 8601 week+day - this is the value W*7+D from a date in ISO 8601
     *   format, where W is the week number (1-53) and D is the day of week
     *   number (1-7). 
     */
    int w;

    /* flag: the year was specified without a century */
    int yy_needs_century;

    /* time - hour, minute, second, milliseconds on 24-hour clock */
    int ampm;
    int hh;
    int mi;
    int ss;
    int ms;

    /* timezone */
    CVmTimeZone *tz;

    /* time zone by GMT offset, in milliseconds */
    int tzofs;

    /* string capture items for the various components */
    date_parse_string pera, pyy, pmm, pdd, pdoy, pw;
    date_parse_string pampm, phh, pmi, pss, pms, punix, ptz;
};


/* match a format element in a template string */
static inline int match_fmt(const char *fmt, const char *fmtend,
                            const char *ref)
{
    size_t len = strlen(ref);
    return (len == (size_t)(fmtend - fmt) && memcmp(ref, fmt, len) == 0);
}

/* 
 *   match a digit string; the template gives pairs of digits with the bounds
 *   to match - e.g., "09" matches one digit from 0 to 9, "12" matches one
 *   digit from 1 to 2, "0209" matches 00 to 29, etc.  We can also match ','
 *   to a period or comma.
 */
static int match_digits(int &acc, const char *&str, size_t &len,
                        const char *tpl, date_parse_string &capture)
{
    /* set up a private pointer to the string */
    const char *p = str;
    size_t rem = len;

    /* match each template pair */
    acc = 0;
    for ( ; *tpl != '\0' ; tpl += 2, ++p, --rem)
    {
        /* check for the special match to a period or comma */
        if (*tpl == ',')
        {
            /* back up to make up for the normal double increment */
            --tpl;
            
            /* allow it to match, but don't require it */
            if (rem != 0 && (*p == ',' || *p == '.'))
                continue;

            /* it doesn't match; simply say on the same input character */
            --p, ++rem;
            continue;
        }
        
        /* if this input char isn't a digit, we don't have a match */
        if (rem == 0 || !is_digit(*p))
            return FALSE;

        /* if this digit is out of range, it's not a match */
        if (*p < tpl[0] || *p > tpl[1])
            return FALSE;

        /* matched this digit - add it to the accumulator */
        acc *= 10;
        acc += value_of_digit(*p);
    }

    /* set the capture data */
    capture.p = str;
    capture.len = p - str;

    /* advance the caller's string pointers past the match */
    str = p;
    len = rem;

    /* return success */
    return TRUE;
}

/*
 *   match literal text 
 */
static int match_lit(const char *&str, size_t &len,
                     const char *lit, size_t litlen,
                     date_parse_string &capture)
{
    /* if there's no literal, there's no match */
    if (lit == 0)
        return 0;

    /* if the template starts with "^", it means do not fold case */
    int fold_case = TRUE;
    if (litlen != 0 && *lit == '^')
        fold_case = FALSE, ++lit, --litlen;

    /* try matching the template text */
    size_t matchlen;
    if (fold_case)
    {
        /* compare the leading substring of 'str' with case folding */
        if (t3_compare_case_fold(lit, litlen, str, len, &matchlen) != 0)
            return FALSE;
    }
    else if (len >= litlen)
    {
        /* compare exactly as given with no case folding */
        if (memcmp(lit, str, litlen) != 0)
            return FALSE;
        matchlen = litlen;
    }
    else
    {
        /* it's too short to match */
        return FALSE;
    }

    /* get a pointer to the next character after the end of the match */
    utf8_ptr p((char *)str + matchlen);
    size_t rem = len - matchlen;

    /* make sure we ended on a suitable boundary */
    if (rem != 0 && litlen != 0)
    {
        /* get the last character of the matched text */
        wchar_t last = p.getch_before(1);
        wchar_t next = p.getch();

        /* if we ended on alphabetic, make sure the next is non-alpha */
        if (isalpha(last) && isalpha(next))
            return FALSE;

        /* if we ended on a number, maek sure the next is non-numeric */
        if (isdigit(last) && isdigit(next))
            return FALSE;
    }
    
    /* set the capture data */
    capture.p = str;
    capture.len = p.getptr() - str;

    /* advance the caller's string pointers past the match */
    str = p.getptr();
    len = rem;

    /* return success */
    return TRUE;
}
static int match_lit(const char *&str, size_t &len, const char *lit,
                     date_parse_string &capture)
{
    return match_lit(str, len, lit, lit != 0 ? strlen(lit) : 0, capture);
}

/*
 *   Match literal text from a list.  The list is given as a string of
 *   comma-separated elements, ending with a semicolon.  Elements can have
 *   aliases specified with '=', as in "sep=sept, ...".  Spaces after commas
 *   are ignored.
 */
static int match_list(int &acc, const char *&str, size_t &len,
                      const char *lst, size_t lstlen,
                      date_parse_string &capture)
{
    /* no matches yet */
    int bestlen = 0;
    int bestidx = 0;

    /* skip leading spaces */
    for ( ; lstlen != 0 && is_space(*lst) ; ++lst, --lstlen) ;
    
    /* keep going until we're out of list */
    for (int idx = 1 ; lstlen != 0 ; )
    {
        /* find the next delimiter */
        const char *p = lst;
        size_t rem = lstlen;
        for ( ; rem != 0 && *p != '=' && *p != ',' && *p != ';' ; ++p, --rem) ;

        /* try matching this text */
        const char *curstr = str;
        size_t curlen = len;
        date_parse_string part;
        if (p - lst != 0 && match_lit(curstr, curlen, lst, p - lst, part))
        {
            /* if this is the best one so far, remember it */
            if (curstr - str > bestlen)
            {
                bestlen = curstr - str;
                bestidx = idx;
            }
        }

        /* 
         *   If we're at an '=', an alias for this same item follows, so keep
         *   going without upping the index.  If we're at a comma, we're on
         *   to the next item.  If we're at a ';' or the end of the string,
         *   it's the end of the list, so we've failed to find a match.
         */
        if (rem == 0 || *p == ';')
        {
            /* end of the list */
            break;
        }
        else if (*p == ',')
        {
            /* next list item follows - up the index */
            ++idx;

            /* skip the comma and any spaces before the next item */
            for (++p, --rem ; rem != 0 && is_space(*p) ; ++p, --rem) ;
        }
        else if (*p == '=')
        {
            /* alias - just skip the '=' */
            ++p, --rem;
        }

        /* on to the next item */
        lst = p;
        lstlen = rem;
    }

    /* if we found a match, return it */
    if (bestlen != 0)
    {
        capture.p = str;
        capture.len = bestlen;
        acc = bestidx;
        str += bestlen;
        len -= bestlen;
        return TRUE;
    }
    else
        return FALSE;
}

/*
 *   match a timezone offset - [GMT] +/- hh [:][mi] [:][ss]
 */
static int match_tzofs(VMG_ date_parse_result *res,
                       const char *&str, size_t &len)
{
    const char *p = str;
    size_t rem = len;

    /* first, skip "gmt" if present */
    date_parse_string part;
    int gmtlit = match_lit(p, rem, "^GMT", part);

    /* get the sign */
    if (rem == 0 || (*p != '+' && *p != '-'))
        return FALSE;
    int s = (*p == '+' ? 1 : -1);

    /* skip the sign */
    ++p, --rem;

    /* 
     *   Get the "hh".  If the "GMT" literal was present, allow a single
     *   digit; otherwise require the two-digit format. 
     */
    int hh;
    if (!match_digits(hh, p, rem, "0509", part)
        && (!gmtlit || !match_digits(hh, p, rem, "09", part)))
        return FALSE;

    /* 
     *   check for ":" - if present, the minutes must be present, otherwise
     *   they're optional 
     */
    int mi = 0;
    if (rem != 0 && *p == ':')
    {
        /* skip the ":", then require the minutes */
        ++p, --rem;
        if (!match_digits(mi, p, rem, "0509", part))
            return FALSE;
    }
    else
    {
        /* no ":", so the minuts are optional */
        match_digits(mi, p, rem, "0509", part);
    }

    /* likewise, check for ":ss" */
    int ss = 0;
    if (rem != 0 && *p == ':')
    {
        ++p, --rem;
        if (!match_digits(ss, p, rem, "0509", part))
            return FALSE;
    }
    else
    {
        match_digits(ss, p, rem, "0509", part);
    }

    /* success - figure the offset in seconds */
    int32_t ofs = s * ((hh*60*60) + (mi*60) + ss);

    /* look up the zone by offset */
    res->tz = G_tzcache->get_gmtofs_zone(vmg_ ofs);

    /* also set the explicit offset value (in milliseconds) */
    res->tzofs = ofs * 1000;

    /* set the capture data */
    res->ptz.p = str;
    res->ptz.len = p - str;

    /* advance the caller's vars past the parsed tzofs and return success */
    str = p;
    len = rem;
    return TRUE;
}

/*
 *   Match a timezone by name 
 */
static int match_tzname(VMG_ date_parse_result *res,
                        const char *&str, size_t &len)
{
    /* check for a zone name string ("America/Los_Angeles", etc) */
    if (len != 0 && isalpha(*str))
    {
        const char *p = str;
        size_t rem = len;
        for ( ; rem != 0 ; ++p, --rem)
        {
            /* allow letters, or / or _ if followed by a letter */
            if (isalpha(*p)
                || ((*p == '_' || *p == '/')
                    && rem > 1 && isalpha(p[1])))
                continue;

            /* otherwise we're done */
            break;
        }

        /* if it contains a slash, look up the zoneinfo database name */
        CVmTimeZone *tz = 0;
        if (lib_strnchr(str, p - str, '/') != 0)
        {
            if ((tz = G_tzcache->get_db_zone(vmg_ str, p - str)) != 0)
            {
                /* success - this is a valid timezone name */
                res->ptz.p = str;
                res->ptz.len = p - str;
                str = p;
                len = rem;
                res->tz = tz;
                return TRUE;
            }
        }

        /* didn't find a zone name; try an abbreviation */
        int32_t tzofs;
        tz = G_tzcache->get_zone_by_abbr(vmg_ &tzofs, str, p - str);
        if (tz != 0)
        {
            /* 
             *   Success - we found an abbreviation; this usually gives us
             *   both a timezone and a specific offset, since an abbreviation
             *   is specific to standard or daylight time.  However, some
             *   abbreviations don't specify an offset, since they're used in
             *   the same zone for both standard and daylight time (e.g.,
             *   Australian EST); these are flagged by tzofs coming back as
             *   INT32MINVAL.
             */
            res->ptz.p = str;
            res->ptz.len = p - str;
            str = p;
            len = rem;
            res->tz = tz;
            if (tzofs != INT32MINVAL)
                res->tzofs = tzofs;
            return TRUE;
        }
    }

    /* not a time zone */
    return FALSE;
}

/*
 *   Parse a date/time string against a given template
 */
int CVmObjDate::parse_string_fmt(VMG_ date_parse_result *res,
                                 const char *&str, size_t &len,
                                 const char *fmt, size_t fmtl,
                                 CVmDateLocale *lc)
{
    /* skip leading spaces and tabs in the source string */
    for ( ; len != 0 && is_space(*str) ; ++str, --len) ;

    /* run through the format string */
    while (len != 0)
    {
        /* skip leading spaces in the format string */
        for ( ; fmtl != 0 && *fmt == ' ' ; --fmtl, ++fmt) ;

        /* stop if we're at the end of the string */
        if (fmtl == 0)
            break;
        
        /* find the next space or end of string */
        const char *sp = lib_strnchr(fmt, fmtl, ' ');
        sp = (sp != 0 ? sp : fmt + fmtl);

        /* parse the type */
        switch (fmt[0])
        {
        case 'd':
            if (match_fmt(fmt, sp, "d"))
            {
                /* one- or two-digit day of month */
                int acc;
                if (match_digits(acc, str, len, "0209", res->pdd)
                    || match_digits(acc, str, len, "3301", res->pdd)
                    || match_digits(acc, str, len, "09", res->pdd))
                    res->dd = acc;
                else
                    return FALSE;
            }
            else if (match_fmt(fmt, sp, "dd"))
            {
                /* two-digit day of month */
                int acc;
                if (match_digits(acc, str, len, "0009", res->pdd)
                    || match_digits(acc, str, len, "1209", res->pdd)
                    || match_digits(acc, str, len, "3301", res->pdd))
                    res->dd = acc;
                else
                    return FALSE;
            }
            else if (match_fmt(fmt, sp, "ddth"))
            {
                /* one/two-digit day of month, plus optional ordinal suffix */
                int acc;
                if (match_digits(acc, str, len, "0209", res->pdd)
                    || match_digits(acc, str, len, "3301", res->pdd)
                    || match_digits(acc, str, len, "09", res->pdd))
                    res->dd = acc;
                else
                    return FALSE;
                
                /* get the locale ordinal for the digit */
                size_t ordl;
                const char *ord = lc->index_list(
                    vmg_ ordl, LC_ORDSUF, ordinal_index(acc), TRUE);
                
                /* compare it */
                date_parse_string pdd;
                if (match_lit(str, len, ord, ordl, pdd))
                {
                    /* matched - add this into the capture data */
                    res->pdd.len += pdd.len;
                }
            }
            else if (match_fmt(fmt, sp, "doy"))
            {
                /* day of year */
                int acc;
                if (match_digits(acc, str, len, "000019", res->pdoy)
                    || match_digits(acc, str, len, "001909", res->pdoy)
                    || match_digits(acc, str, len, "120909", res->pdoy)
                    || match_digits(acc, str, len, "330509", res->pdoy)
                    || match_digits(acc, str, len, "336606", res->pdoy))
                {
                    /* 
                     *   set the date to January 'doy' - this isn't
                     *   necessarily a valid calendar date in January, but
                     *   caldate_t handles overflows in the day by carrying
                     *   to the month and year as needed 
                     */
                    res->mm = 1;
                    res->dd = acc;
                }
            }
            else
                goto other;
            break;

        case 'm':
            if (match_fmt(fmt, sp, "month"))
            {
                /* 
                 *   Month by long name, short name, or Roman numeral.  Get
                 *   the locale strings for the month names; the Roman
                 *   numerals are fixed, so we can set up a static string for
                 *   those. 
                 */
                size_t mm_long_len, mm_abbr_len;
                const char *mm_long = lc->get(vmg_ mm_long_len, LC_MONTH);
                const char *mm_abbr = lc->get(vmg_ mm_abbr_len, LC_MON);
                static const char *romans =
                    "^I,^II,^III,^IV,^V,^VI,^VII,^VIII,^IX,^X,^XI,^XII;";
                
                /* try each variation */
                int acc;
                if (match_list(acc, str, len, mm_long, mm_long_len, res->pmm)
                    || match_list(
                        acc, str, len, mm_abbr, mm_abbr_len, res->pmm)
                    || match_list(
                        acc, str, len, romans, strlen(romans), res->pmm))
                    res->mm = acc;
                else
                    return FALSE;
                
                /* specifying a month makes the default day the 1st */
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "mon"))
            {
                /* month by short name */
                size_t mm_abbr_len;
                const char *mm_abbr = lc->get(vmg_ mm_abbr_len, LC_MON);
                int acc;
                if (match_list(acc, str, len, mm_abbr, mm_abbr_len, res->pmm))
                    res->mm = acc;
                else
                    return FALSE;
                
                /* specifying a month makes the default day the 1st */
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "m"))
            {
                /* one- or two-digit month */
                int acc;
                if (match_digits(acc, str, len, "0009", res->pmm)
                    || match_digits(acc, str, len, "1102", res->pmm)
                    || match_digits(acc, str, len, "09", res->pmm))
                    res->mm = acc;
                else
                    return FALSE;
                
                /* specifying a month makes the default day the 1st */
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "mm"))
            {
                /* two-digit month */
                int acc;
                if (match_digits(acc, str, len, "0009", res->pmm)
                    || match_digits(acc, str, len, "1102", res->pmm))
                    res->mm = acc;
                else
                    return FALSE;
                
                /* specifying a month makes the default day the 1st */
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "mi"))
            {
                /* two-digit minutes */
                int acc;
                if (match_digits(acc, str, len, "0509", res->pmi))
                    res->mi = acc;
                else
                    return FALSE;

                /* specifying the minute makes the default ss.frac zero */
                res->def_second(0);
                res->def_msec(0);
            }
            else
                goto other;
            break;

        case 'W':
            if (match_fmt(fmt, sp, "W"))
            {
                /* 
                 *   ISO week - a two-digit number giving the week of the
                 *   year, optionally followed by a day number 
                 */
                int w;
                if (match_digits(w, str, len, "0019", res->pw)
                    || match_digits(w, str, len, "1409", res->pw)
                    || match_digits(w, str, len, "5503", res->pw))
                {
                    /* check for a weekday following */
                    int d = 1;
                    if (len >= 2
                        && *str == '-'
                        && strchr("1234567", str[1]) != 0)
                    {
                        d = value_of_digit(str[1]);
                        str += 2, len -= 2;
                        res->pw.len += 2;
                    }
                    else if (len >= 1 && strchr("1234567", str[0]) != 0)
                    {
                        d = value_of_digit(str[0]);
                        str += 1, len -= 1;
                        res->pw.len += 1;
                    }
                    
                    /* matched */
                    res->w = w*7 + d;
                    return TRUE;
                }
                else
                    return FALSE;
            }
            else
                goto other;
            break;

        case 'y':
            if (match_fmt(fmt, sp, "y"))
            {
                /* one- to seven-digit year */
                int acc;
                if (match_digits(acc, str, len, "09,090909,090909", res->pyy)
                    || match_digits(acc, str, len, "090909,090909", res->pyy)
                    || match_digits(acc, str, len, "0909,090909", res->pyy)
                    || match_digits(acc, str, len, "09090909", res->pyy)
                    || match_digits(acc, str, len, "090909", res->pyy))
                    res->yy = acc;
                else if (match_digits(acc, str, len, "0909", res->pyy)
                         || match_digits(acc, str, len, "09", res->pyy))
                {
                    res->yy = acc;
                    res->yy_needs_century = TRUE;

                    if (res->yy >= 5879650 || res->yy <= -5879650)
                        err_throw(VMERR_NUM_OVERFLOW);
                }
                else
                    return FALSE;
                
                /* specifying the year makes the default date 1/1 */
                res->def_month(1);
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "yy"))
            {
                /* two-digit year; century comes from the reference year */
                int acc;
                if (match_digits(acc, str, len, "0909", res->pyy))
                {
                    res->yy = acc;
                    res->yy_needs_century = TRUE;
                }
                else
                    return FALSE;
                
                /* specifying the year makes the default date 1/1 */
                res->def_month(1);
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "yyyy"))
            {
                /* four-digit year */
                int acc;
                if (match_digits(acc, str, len, "09090909", res->pyy))
                    res->yy = acc;
                else
                    return FALSE;
                
                /* specifying the year makes the default date 1/1 */
                res->def_month(1);
                res->def_day(1);
            }
            else if (match_fmt(fmt, sp, "ye"))
            {
                /* 
                 *   Year with optional era designator (AD/BC).  The era can
                 *   come before or after the year, with spaces in between. 
                 */
                size_t era_len;
                const char *era_lst = lc->get(vmg_ era_len, LC_ERA);
                int idx;
                int era = 0;
                if (match_list(idx, str, len, era_lst, era_len, res->pera))
                {
                    /* note the era - 2 for AD, -2 for BC */
                    res->era = era = (idx == 1 ? 2 : -2);
                    
                    /* allow spaces */
                    for ( ; len != 0 && isspace(*str) ; ++str, --len) ;
                }
                
                /* match the year - this part is required */
                int acc;
                if (match_digits(acc, str, len, "09,090909,090909", res->pyy)
                    || match_digits(acc, str, len, "090909,090909", res->pyy)
                    || match_digits(acc, str, len, "0909,090909", res->pyy)
                    || match_digits(acc, str, len, "09090909", res->pyy)
                    || match_digits(acc, str, len, "090909", res->pyy)
                    || match_digits(acc, str, len, "0909", res->pyy)
                    || match_digits(acc, str, len, "09", res->pyy))
                    res->yy = acc;
                else
                    return FALSE;

                /* we accept large year numbers, so check range */
                if (res->yy >= 5879650 || res->yy <= -5879650)
                    err_throw(VMERR_NUM_OVERFLOW);

                /* if we didn't have a prefixed era, check for a postfix era */
                if (era == 0)
                {
                    /* skip optional spaces */
                    const char *str2 = str;
                    size_t len2 = len;
                    for ( ; len2 != 0 && isspace(*str2) ; ++str2, --len2) ;
                    
                    /* check for the era */
                    if (match_list(
                        idx, str2, len2, era_lst, era_len, res->pera))
                    {
                        /* found it - set the era identifier */
                        res->era = era = (idx == 1 ? 2 : -2);
                        
                        /* advance past it in the main string */
                        str = str2;
                        len = len2;
                    }
                }
                
                /* 
                 *   if we matched one or two digits, with no era, we need a
                 *   default century 
                 */
                if (era == 0 && res->pyy.len <= 2)
                    res->yy_needs_century = TRUE;
            }
            else
                goto other;
            break;

        case 'e':
            if (match_fmt(fmt, sp, "era"))
            {
                /* AD/BC era designator */
                size_t era_len;
                const char *era_lst = lc->get(vmg_ era_len, LC_ERA);
                int idx;
                if (match_list(idx, str, len, era_lst, era_len, res->pera))
                {
                    /* set the era to 2 for AD or -2 for BC */
                    res->era = (idx == 1 ? 2 : -2);
                }
                else
                    return FALSE;
            }
            else
                goto other;
            break;

        case 'h':
            if (match_fmt(fmt, sp, "h"))
            {
                /* one- or two-digit hours */
                int acc;
                if (match_digits(acc, str, len, "0019", res->phh)
                    || match_digits(acc, str, len, "1102", res->phh)
                    || match_digits(acc, str, len, "09", res->phh))
                    res->hh = acc;
                else
                    return FALSE;
                
                /* specifying the hour makes the default mm:ss.frac zero */
                res->def_minute(0);
                res->def_second(0);
                res->def_msec(0);
            }
            else if (match_fmt(fmt, sp, "hh"))
            {
                /* two-digit hours */
                int acc;
                if (match_digits(acc, str, len, "0109", res->phh)
                    || match_digits(acc, str, len, "2204", res->phh))
                    res->hh = acc;
                else
                    return FALSE;
                
                /* specifying the hour makes the default minute zero */
                res->def_minute(0);
                res->def_second(0);
                res->def_msec(0);
            }
            else
                goto other;
            break;

        case 'i':
            if (match_fmt(fmt, sp, "i"))
            {
                /* one- or two-digit minutes */
                int acc;
                if (match_digits(acc, str, len, "0509", res->pmi)
                    || match_digits(acc, str, len, "09", res->pmi))
                    res->mi = acc;
                else
                    return FALSE;
                
                /* specifying the minute makes the default ss.frac zero */
                res->def_second(0);
                res->def_msec(0);
            }
            else
                goto other;
            break;

        case 's':
            if (match_fmt(fmt, sp, "ss"))
            {
                /* two-digit seconds */
                int acc;
                if (match_digits(acc, str, len, "0509", res->pss))
                    res->ss = acc;
                else
                    return FALSE;
                
                /* specifying the second makes the default fraction zero */
                res->def_msec(0);
            }
            else if (match_fmt(fmt, sp, "s"))
            {
                /* one- or two-digit seconds */
                int acc;
                if (match_digits(acc, str, len, "0509", res->pss)
                    || match_digits(acc, str, len, "09", res->pss))
                    res->ss = acc;
                else
                    return FALSE;
                
                /* specifying the second makes the default fraction zero */
                res->def_msec(0);
            }
            else if (match_fmt(fmt, sp, "ssfrac"))
            {
                /* fractional seconds */
                if (len >= 1 && isdigit(str[1]))
                {
                    /* 
                     *   parse digits - accumulate up to four digits so that
                     *   we can round to milliseconds 
                     */
                    res->pms.p = str;
                    res->pms.len = 0;
                    int acc = 0, mul = 10000, trailing = 0;
                    for ( ; len != 0 && isdigit(*str) ;
                          ++str, --len, ++res->pms.len, mul /= 10)
                    {
                        /* 
                         *   add the next digit only if significant,
                         *   otherwise note any non-zero trailing digits 
                         */
                        if (mul > 1)
                            acc *= 10, acc += value_of_digit(*str);
                        else if (*str != '0')
                            trailing = 1;
                    }

                    /* apply the multiplier if non-zero */
                    if (mul != 0)
                        acc *= mul;

                    /* we have 10*ms - divide by 10 to get ms */
                    int lastdig = acc % 10;
                    acc /= 10;

                    /* 
                     *   round up if the last digit is 6+, 5+ with a non-zero
                     *   digit following, or exactly 5 with an odd second
                     *   digit 
                     */
                    if (lastdig > 5
                        || (lastdig == 5
                            && (trailing
                                || ((acc % 10) & 1) != 0)))
                        ++acc;
                    
                    /* store as integer milliseconds */
                    res->ms = acc;
                }
                else
                    return FALSE;
            }
            else
                goto other;
            break;

        case 'u':
            if (match_fmt(fmt, sp, "unix"))
            {
                /* 
                 *   Unix timestamp value - any number of digits giving a
                 *   positive or negative offset in seconds from the Unix
                 *   Epoch (1/1/1970) 
                 */
                int s = 1;
                if (len >= 2 && str[0] == '-' && is_digit(str[1]))
                {
                    res->punix.p = str, res->punix.len = 1;
                    s = -1, ++str, --len;
                }
                else if (len >= 1 && is_digit(str[0]))
                {
                    res->punix.p = str, res->punix.len = 0;
                }
                else
                    return FALSE;
                
                /* 
                 *   Parse digits; use a double in case we have more than a
                 *   32-bit int's worth.  (We only work in whole integers
                 *   here, but we might need more precision than an int32.  A
                 *   double is enough for any day we can represent: we have a
                 *   32-bit day; 86400 seconds per day is about 17 additional
                 *   bits, for a total of 49.  An IEEE double has a 52-bit
                 *   mantissa, so a double can exactly represent a Unix
                 *   timestamp for any day number we can represent.)
                 */
                double acc = 0.0;
                for ( ; len > 0 && is_digit(*str) ;
                      ++str, --len, ++res->punix.len)
                {
                    acc *= 10.0;
                    acc += value_of_digit(*str);
                }
                
                /* apply the sign */
                acc *= s;
                
                /* split into days and seconds */
                double days = floor(acc / (24.*60.*60.));
                int32_t secs = (int32_t)fmod(acc, 24.*60.*60.);
                
                /* make sure it fits an int32 */
                if (days > INT32MAXVAL)
                    return FALSE;
                
                /* 
                 *   convert to a calendar date, adjusting from the Unix
                 *   Epoch to our Epoch 
                 */
                res->cal->set_dayno(
                    (int32_t)days + caldate_t::UNIX_EPOCH_DAYNO);
                
                /* set the time and day in the parse results */
                res->yy = res->cal->y();
                res->mm = res->cal->m();
                res->dd = res->cal->d();
                
                res->hh = secs/(60*60);
                res->mi = (secs/60) % 60;
                res->ss = secs % 60;
                res->ms = 0;
                
                /* Unix timestamps are relative to UTC */
                res->tzofs = 0;
            }
            else
                goto other;
            break;

        case 'a':
            if (match_fmt(fmt, sp, "ampm"))
            {
                /* month by short name */
                size_t ampm_len;
                const char *ampm_str = lc->get(vmg_ ampm_len, LC_AMPM);
                int acc;
                if (match_list(acc, str, len, ampm_str, ampm_len, res->pampm))
                    res->ampm = acc;
                else
                    return FALSE;
            }
            else
                goto other;
            break;

        case 't':
            if (match_fmt(fmt, sp, "tz"))
            {
                /* timezone by name, abbreviation, or offset */
                if (!match_tzofs(vmg_ res, str, len)
                    && !match_tzname(vmg_ res, str, len))
                    return FALSE;
            }
            else
                goto other;
            break;

        case 'g':
            if (match_fmt(fmt, sp, "gmtofs"))
            {
                /* timezone offset from GMT */
                if (!match_tzofs(vmg_ res, str, len))
                    return FALSE;
            }
            else
                goto other;
            break;

        default:
        other:
            if (match_fmt(fmt, sp, "+-"))
            {
                /* 
                 *   Astronomical-notation era designator - this is the
                 *   system where the year before AD 1 is Year 0, the year
                 *   before that -1, etc.  Positive years can optionally have
                 *   a plus sign.
                 */
                if (match_lit(str, len, "+", res->pera))
                    res->era = 1;
                else if (match_lit(str, len, "-", res->pera))
                    res->era = -1;
                else
                    return FALSE;
            }
            else
            {
                /* 
                 *   Anything else is a list of literals to match.  If it
                 *   ends with '*', we can match zero or more; if it ends
                 *   with '+', we can match one or more; otherwise we match
                 *   exactly one. 
                 */
                const char *fmtend = sp;
                char suffix = *(sp-1);
                int mincnt = (suffix == '*' || suffix == '?' ? 0 : 1);
                int maxcnt = (suffix == '*' || suffix == '+' ? 65535 : 1);
                fmtend -= (strchr("*?+", suffix) != 0 ? 1 : 0);
                
                /* scan the character list */
                int cnt = 0;
                utf8_ptr strp((char *)str);
                while (cnt < maxcnt && len != 0)
                {
                    int found = FALSE;
                    utf8_ptr fmtp((char *)fmt);
                    for ( ; fmtp.getptr() < fmtend ; fmtp.inc())
                    {
                        /* get the current template and source characters */
                        wchar_t fc = fmtp.getch();
                        wchar_t sc = strp.getch();
                        
                        /* 
                         *   check for specials: \ quotes the next, _ is a
                         *   space 
                         */
                        if (fc == '\\' && fmtp.getptr() < fmtend)
                        {
                            fmtp.inc();
                            fc = fmtp.getch();
                        }
                        else if (fc == '_')
                        {
                            fc = ' ';
                        }
                        
                        /* check for a match */
                        if (fc == sc)
                        {
                            found = TRUE;
                            ++cnt;
                            strp.inc(&len);
                            str = strp.getptr();
                            break;
                        }
                    }
                    if (!found)
                        break;
                }
                
                /* if we didn't match the minimum number, it's a mismatch */
                if (cnt < mincnt)
                    return FALSE;
            }
            break;
        }
            
        /* advance past this field */
        fmtl -= sp - fmt;
        fmt = sp;
    }

    /* if we didn't exhaust the format, we have no match */
    if (fmtl != 0)
        return FALSE;

    /* we matched the format - skip trailing spaces */
    for ( ; len != 0 && is_space(*str) ; ++str, --len) ;

    /* return success */
    return TRUE;
}

/* custom/builtin format string iterator */
struct format_iter
{
    format_iter(VMG_ const char **builtin, size_t builtin_cnt,
                const vm_val_t *custom)
    {
        /* save the source lists */
        this->builtin = builtin;
        this->builtin_cnt = builtin_cnt;
        this->custom = custom;

        /* presume we'll start in the custom list */
        this->mode = 0;

        /* figure how many builtin items we have, if any */
        if (custom == 0 || custom->typ == VM_NIL)
        {
            /* no custom items - just use the builtin list */
            this->custom_cnt = 0;
            this->mode = 1;
        }
        else if (custom->get_as_string(vmg0_) != 0)
            this->custom_cnt = 1;
        else if (custom->is_listlike(vmg0_))
            this->custom_cnt = custom->ll_length(vmg0_);
        else
            err_throw(VMERR_BAD_TYPE_BIF);

        /* start at the top of each list */
        this->bidx = 0;
        this->cidx = 0;
    }

    /* get the next string */
    const char *next(VMG_ size_t &len)
    {
        /* assume we won't find anythign */
        len = 0;

        /* keep going until we find something */
        for (;;)
        {
            if (mode == 0)
            {
                /* custom list - if exhausted, we're done */
                if (cidx >= custom_cnt)
                    return 0;

                /* get the next custom item, according to the source type */
                vm_val_t ele;
                if (custom->is_listlike(vmg0_))
                {
                    /* get the next list element */
                    custom->ll_index(vmg_ &ele, ++cidx);

                    /* if it's nil, switch to the builtin list */
                    if (ele.typ == VM_NIL)
                    {
                        mode = 1;
                        continue;
                    }
                }
                else
                {
                    /* it must be a single string */
                    ele = *custom;
                }

                /* retrieve the string value */
                const char *str = ele.get_as_string(vmg0_);
                if (str != 0)
                {
                    len = vmb_get_len(str);
                    str += VMB_LEN;
                }
                return str;
            }
            else
            {
                /* builtin list - if exhausted, return to the custom list */
                if (bidx >= builtin_cnt)
                {
                    mode = 0;
                    continue;
                }

                /* return the next builtin item */
                len = strlen(builtin[bidx]);
                return builtin[bidx++];
            }
        }
    }

    /* current list we're reading from - 0=custom, 1=builtin */
    int mode;

    /* builtin list, and current index in list */
    const char **builtin;
    size_t builtin_cnt;
    size_t bidx;

    /* custom string or list, and current index in list */
    const vm_val_t *custom;
    size_t custom_cnt;
    size_t cidx;
};

/*
 *   Parse a date string in a constructor argument. 
 */
int CVmObjDate::parse_date_string(
    VMG_ int32_t &dayno, int32_t &daytime,
    const char *str, const vm_val_t *custom, CVmDateLocale *lc,
    multicaldate_t *cal, int32_t refday, int32_t reftime, CVmTimeZone *reftz,
    date_parse_result *resultp, date_parse_string *fmtlist, int *nfmtlist)
{
    /* get the string length and buffer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* format list */
    static const char *fmt[] =
    {
        /* US-style dates, with the month first */
        "[us]m / d / y",
        "[us]m / d",
        "[us]m - d",
        "[us]m . d",
        "[us]m .\t- d .\t- y",

        /* European-style date formats, with day/month order */
        "[eu]d / m / y",
        "[eu]d / m",
        "[eu]d - m",
        "[eu]d . m",
        "[eu]d .\t- m .\t- y",

        /* match some special computer formats first */
        "yyyy mm dd",                             /* 8-digit year month day */
        "yyyy .? doy",                   /* PostreSQL year with day-of-year */

        /* universal date formats */
        "m / y",
        "m - y",
        "month _.\t-* ddth _,.\t+ ye",
        "ddth _.\t-* month _,.\t-* ye",
        "month _.\t-* ddth",
        "ddth _.\t-* month",
        "month - dd - ye",
        "month _\t.-* y",
        "month",
        "+- y - mm - dd",
        "y / m / d",
        "y - m - d",
        "y / m",
        "y - m",
        "ye _\t.-* month _\t.-* d",
        "ye _\t.-* month",
        "ye - month - dd",
        "yyyy",
        "era _* y",
        "y _* era",

        /* time formats */
        "h _? ampm",
        "h : mi",
        "h .: mi _? ampm",
        "h .: mi .: ss _? ampm",
        "h : mi : ss",
        "h : mi : ss .: ssfrac _? ampm",
        "h : mi : ss .: ssfrac",
        "tT? hh .: mi",
        "tT? hh mi",
        "tT? hh .: mi .: ss",
        "tT? hh mi SS",
        "tT? hh .: mi .: ss _? tz",
        "tT? hh .: mi .: ss . ssfrac",
        "tz",

        /* special formats (ISO, other software, etc) */
        "d / mon / yyyy : hh : mi : ss _ gmtofs",         /* Unix log files */
        "yyyy : mm : dd _ hh : mi : ss",                            /* EXIF */
        "yyyy -? \\W W",                          /* ISO year with ISO week */
        "yyyy - mm - dd \\T hh : mi : ss . ssfrac",                 /* SOAP */
        "yyyy - mm - dd \\T hh : mi : ss . ssfrac gmtofs",          /* SOAP */
        "@ unix",                                         /* Unix timestamp */
        "yyyy mm dd \\T hh : mi : ss",                            /* XMLRPC */
        "yyyy mm dd \\t hh mi ss",                      /* XMLRPC - compact */
        "yyyy - m - d \\T h : i : s"                                /* WDDX */
    };

    /* get the template filter in "[xx]" format */
    size_t filterlen;
    const char *filter = lc->get(vmg_ filterlen, LC_PARSE_FILTER);
    
    /* keep separate results for each pass */
    date_parse_result result(cal);

    /*
     *   Run through the format list as many times as it takes to either
     *   consume the whole source string, or fail to find a matching format.
     *   We have multiple format parts that can be specified individually or
     *   in combination (time, date, time + date, date + time, etc).
     */
    int fcapi;
    for (fcapi = 0 ; len != 0 ; ++fcapi)
    {
        /* we haven't found a best result for this pass yet */
        int bestlen = 0;
        date_parse_result bestres(cal);
        
        /* search the list for a match */
        format_iter fi(vmg_ fmt, countof(fmt), custom);
        size_t curfmtl;
        const char *curfmt;
        while ((curfmt = fi.next(vmg_ curfmtl)) != 0)
        {
            /* set up at the current position */
            const char *curstr = str;
            size_t curlen = len;

            /* remember the original format string */
            const char *curfmt0 = curfmt;
            size_t curfmtl0 = curfmtl;

            /* if this is a "[us]" or "[eu]" local entry, filter it */
            if (curfmtl >= 3 && curfmt[0] == '[')
            {
                /* get the ']' */
                const char *rb = lib_strnchr(curfmt+1, curfmtl-1, ']');

                /* 
                 *   if we found it, and the contents of the brackets don't
                 *   match the locale filter string, filter out this format 
                 */
                if (rb != 0
                    && (rb - curfmt - 1 != (int)filterlen
                        || memicmp(curfmt + 1, filter, filterlen) != 0))
                    continue;

                /* skip the filter string */
                curfmtl -= rb - curfmt + 1;
                curfmt = rb + 1;
            }

            /* start with the results from previous passes */
            date_parse_result curres(result);

            /* try matching this format at the current position */
            if (parse_string_fmt(vmg_ &curres, curstr, curlen,
                                 curfmt, curfmtl, lc))
            {
                /* if this is the longest match of this pass, keep it */
                int reslen = curstr - str;
                if (reslen > bestlen)
                {
                    /* remember this as the best result so far */
                    bestlen = reslen;
                    bestres = curres;

                    /* remember the format string it matches, if desired */
                    if (nfmtlist != 0 && fcapi < *nfmtlist)
                        fmtlist[fcapi].set(curfmt0, curfmtl0);
                }
            }
        }

        /* if we didn't find a match on this round, we've failed */
        if (bestlen == 0)
            return FALSE;

        /* skip what we matched for the best result of this pass */
        str += bestlen;
        len -= bestlen;

        /* keep the best result for this pass */
        result = bestres;

        /* skip spaces and other separator characters */
        for ( ; len != 0 && strchr(" \t;,:", *str) != 0 ; ++str, --len) ;
    }

    /* default to the reference time zone */
    result.def_tz(reftz);

    /* adjust the reference time to the appropriate local time zone */
    result.utc_to_local(refday, reftime);

    /* set missing date elements to the corresponding reference date values */
    result.def_date(refday);
    
    /* set any missing time elements to midnight or zero past the hour/min */
    result.def_time(0, 0, 0, 0);

    /* convert to our internal day and time representation */
    dayno = result.dayno();
    daytime = result.daytime();
    caldate_t::normalize(dayno, daytime);

    /* translate the final time value from the specified local time to UTC */
    result.local_to_utc(dayno, daytime);

    /* copy back the results if the caller's interested */
    if (resultp != 0)
        memcpy(resultp, &result, sizeof(result));

    /* tell the caller how many format strings we matched, if desired */
    if (nfmtlist != 0)
        *nfmtlist = fcapi;

    /* success */
    return TRUE;
}

/*
 *   Parse constructor arguments for a (...Y,M,D) or (...Y,M,D,HH,MI,SS,MS)
 *   argument list.  This parses the arguments from the stack and fills in
 *   the day number and time of day values accordingly.  We don't make any
 *   time zone adjustments; we just return the value as specified.
 */
static void parse_ymd_args(VMG_ int argofs, int argc,
                           int32_t &dayno, int32_t &daytime)
{
    /* get the year, month, and day */
    int32_t y = G_stk->get(argofs + 0)->num_to_int(vmg0_);
    int32_t m = G_stk->get(argofs + 1)->num_to_int(vmg0_);
    int32_t d = G_stk->get(argofs + 2)->num_to_int(vmg0_);

    /* figure the day number for the given calendar date */
    caldate_t cd(y, m, d);
    dayno = cd.dayno();
    
    /* 
     *   if this is the 7-argument version, get the time; otherwise it's
     *   implicitly midnight on the given date
     */
    daytime = 0;
    if (argc == 7)
    {
        /* get the hour and minute as integers, and seconds */
        int hh = G_stk->get(argofs + 3)->num_to_int(vmg0_);
        int mm = G_stk->get(argofs + 4)->num_to_int(vmg0_);
        int ss = G_stk->get(argofs + 5)->num_to_int(vmg0_);
        int ms = G_stk->get(argofs + 6)->num_to_int(vmg0_);
        
        /* 
         *   Combine the time components into 'daytime' in milliseconds.
         *   This could be a very large number, so work in double until we
         *   can normalize it.  Note that we're working in whole numbers only
         *   (no fractions), so a double will be exact - all we're worried
         *   about here is the extra integer precision it can hold, not the
         *   fractional part.
         */
        double dt = hh*60.*60.*1000. + mm*60.*1000. + ss*1000. + ms;

        /* figure the overflow into the day number (and make sure it fits) */
        double dd = floor(dt / (24.*60.*60.*1000.));
        dt = fmod(dt, 24.*60.*60.*1000.);

        /* apply the overflow to the day number, and make sure it fits */
        dd += dayno;
        if (dd < INT32MINVAL || dd > INT32MAXVAL)
            err_throw(VMERR_NUM_OVERFLOW);

        /* it's all normalized and in range, so convert back to int32 */
        dayno = (int32_t)dd;
        daytime = (int32_t)dt;
    }

    /* normalize the result */
    caldate_t::normalize(dayno, daytime);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create with a given timestamp 
 */
vm_obj_id_t CVmObjDate::create(VMG_ int in_root_set,
                               int32_t dayno, int32_t daytime)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjDate(vmg_ dayno, daytime);
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create from an os_time_t timestamp 
 */
vm_obj_id_t CVmObjDate::create_from_time_t(VMG_ int in_root_set, os_time_t t)
{
    /* 
     *   turn the time_t into days and milliseconds after the Unix Epoch (use
     *   doubles to ensure we don't overflow an int) 
     */
    double d = (double)t;
    double dn = floor(d / (24*60*60));
    double dms = (d - (dn * (24*60*60))) * 1000;

    /* adjust to our internal Epoch */
    dn += caldate_t::UNIX_EPOCH_DAYNO;

    /* we have the time in our own format now, so go create the object */
    return create(vmg_ in_root_set, (int32_t)dn, (int32_t)dms);
}

/* ------------------------------------------------------------------------ */
/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjDate::create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
{
    int32_t dayno;
    int32_t daytime;
    const char *str;
    
    /* check for the various argument lists */
    if (argc == 0)
    {
        /* no arguments - get the current date and time in the local zone */
        caldate_t::now(dayno, daytime);
    }
    else if ((argc >= 1 && argc <= 3)
             && (str = G_stk->get(0)->get_as_string(vmg0_)) != 0)
    {
        /* 
         *   The first argument is a date in string format.  The optional
         *   second argument is the reference time zone; the optional third
         *   argument is the reference date.
         */

        /* get the reference time zone */
        CVmTimeZone *reftz;
        if (argc >= 2 && G_stk->get(1)->typ != VM_NIL)
            reftz = get_tz_arg(vmg_ 1, argc);
        else
            reftz = G_tzcache->get_local_zone(vmg0_);

        /* get the reference date */
        int32_t refday, reftime;
        if (argc >= 3 && G_stk->get(2)->typ != VM_NIL)
        {
            /* get the reference Date object */
            CVmObjDate *refdate = vm_val_cast(CVmObjDate, G_stk->get(2));
            if (refdate == 0)
                err_throw(VMERR_BAD_TYPE_BIF);

            /* take the reference point from the Date object */
            refday = refdate->get_ext()->dayno;
            reftime = refdate->get_ext()->daytime;
        }
        else
        {
            /* no reference date, so use the current time */
            caldate_t::now(refday, reftime);
        }

        /* parse the format */
        CVmDateLocale lc Pvmg0_P;
        gregcaldate_t cal;
        if (!parse_date_string(vmg_ dayno, daytime, str, 0, &lc, &cal,
                               refday, reftime, reftz, 0, 0, 0))
            err_throw(VMERR_BAD_VAL_BIF);
    }
    else if (argc == 2
             && G_stk->get(0)->is_numeric(vmg0_)
             && (str = G_stk->get(1)->get_as_string(vmg0_)) != 0)
    {
        /* check the type code */
        double dn, dms;
        if (vmb_get_len(str) == 1 && (str[2] == 'U' || str[2] == 'u'))
        {
            /* 
             *   'U' - Unix time, as seconds past 1/1/1970, UTC.  Get the
             *   argument as a double, and convert it to our day numbering
             *   scheme by dividing by seconds per day and adding the Unix
             *   Epoch's day number.  (An ordinary double is big enough to
             *   hold the largest Unix seconds-since-Epoch value we can
             *   represent: our range is limited by the 32-bit day number, so
             *   in terms of seconds that's (2^31-1)*86400, which needs about
             *   49 bits.  A standard double's mantissa is 52 bits, so it can
             *   represent any integer in our range exactly.)
             */
            double d = G_stk->get(0)->num_to_double(vmg0_);
            dn = floor(d / (24*60*60));
            dms = (d - (dn * (24*60*60))) * 1000;
            dn += caldate_t::UNIX_EPOCH_DAYNO;
        }
        else if (vmb_get_len(str) == 1 && (str[2] == 'J' || str[2] == 'j'))
        {
            /* 
             *   'J' - Julian day number; get the value, and adjust it to our
             *   Epoch by subtracting the Julian day number of our Epoch.
             *   The whole part is our day number; the fractional part is the
             *   fraction of a day past midnight (the .5 in the Epoch
             *   adjustment recalibrates from noon to midnight).
             */
            bignum_t<32> bdayno(vmg_ G_stk->get(0));
            bdayno -= 1721119.5;
            dn = floor((double)bdayno);
            dms = (double)((bdayno - dn) * (long)(24*60*60*1000));
        }
        else
        {
            /* unrecognized code */
            err_throw(VMERR_BAD_VAL_BIF);
        }

        /* make sure the day number fits an int32 */
        if (dn > INT32MAXVAL || dn < INT32MINVAL)
            err_throw(VMERR_NUM_OVERFLOW);
        dayno = (int32_t)dn;
        
        /* convert milliseconds to int32 */
        daytime = round_int32(dms);
    }
    else if ((argc == 3 || argc == 7)
             && G_stk->get(0)->is_numeric(vmg0_)
             && G_stk->get(1)->is_numeric(vmg0_)
             && G_stk->get(2)->is_numeric(vmg0_)
             && (argc == 3
                 || (G_stk->get(3)->is_numeric(vmg0_)
                     && G_stk->get(4)->is_numeric(vmg0_)
                     && G_stk->get(5)->is_numeric(vmg0_)
                     && G_stk->get(6)->is_numeric(vmg0_))))
    {
        /* parse the Y/M/D or Y/M/D HH:MI:SS arguments */
        parse_ymd_args(vmg_ 0, argc, dayno, daytime);

        /* convert from the local time zone to UTC */
        G_tzcache->get_local_zone(vmg0_)->local_to_utc(dayno, daytime);
    }
    else if ((argc == 4 || argc == 8)
             && G_stk->get(0)->is_numeric(vmg0_)
             && G_stk->get(1)->is_numeric(vmg0_)
             && G_stk->get(2)->is_numeric(vmg0_)
             && (argc == 4
                 || (G_stk->get(3)->is_numeric(vmg0_)
                     && G_stk->get(4)->is_numeric(vmg0_)
                     && G_stk->get(5)->is_numeric(vmg0_)
                     && G_stk->get(6)->is_numeric(vmg0_))))
    {
        /* parse the Y/M/D or Y/M/D HH:MI:SS arguments */
        parse_ymd_args(vmg_ 0, argc - 1, dayno, daytime);

        /* convert from the given time zone to UTC */
        get_tz_arg(vmg_ argc - 1, argc)->local_to_utc(dayno, daytime);
    }
    else
    {
        /* wrong arguments */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* create the object */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
    new (vmg_ id) CVmObjDate(vmg_ dayno, daytime);

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/* 
 *   notify of deletion 
 */
void CVmObjDate::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjDate::set_prop(VMG_ class CVmUndo *undo,
                          vm_obj_id_t self, vm_prop_id_t prop,
                          const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjDate::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                         vm_obj_id_t self, vm_obj_id_t *source_obj,
                         uint *argc)
{
    uint func_idx;
    
    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    
    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }
    
    /* inherit default handling from our base class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/*
 *   get a static (class) property 
 */
int CVmObjDate::call_stat_prop(VMG_ vm_val_t *result,
                               const uchar **pc_ptr, uint *argc,
                               vm_prop_id_t prop)
{
    /* look up the function */
    uint midx = G_meta_table
                ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* check for our static properties */
    switch (midx)
    {
    case VMDATE_PARSEDATE:
        return s_getp_parseDate(vmg_ result, argc);
        
    case VMDATE_PARSEJULIANDATE:
        return s_getp_parseJulianDate(vmg_ result, argc);

    case VMDATE_SETLOCALINFO:
        return s_getp_setLocaleInfo(vmg_ result, argc);
    }

    /* not one of hours; inherit the superclass handling */
    return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
}

/* ------------------------------------------------------------------------ */
/*
 *   cast to a string 
 */
const char *CVmObjDate::cast_to_string(
    VMG_ vm_obj_id_t self, vm_val_t *newstr) const
{
    /* format the date into a buffer */
    char buf[64];
    format_string_buf(vmg_ buf, sizeof(buf));

    /* create the return string */
    newstr->set_obj(CVmObjString::create(vmg_ FALSE, buf, strlen(buf)));
    return newstr->get_as_string(vmg0_);
}

/*
 *   Format into a string buffer using the default representation - for
 *   implied conversions, toString, and the debugger
 */
void CVmObjDate::format_string_buf(VMG_ char *buf, size_t buflen) const
{
    /* get the day number and time of day in local time */
    int32_t dayno, daytime, tzofs;
    const char *tzabbr = get_local_time(
        dayno, daytime, tzofs, G_tzcache->get_local_zone(vmg0_));

    /* convert the day number to a calendar date */
    gregcaldate_t date(dayno);

    /* format using a default template */
    CVmDateLocale lc Pvmg0_P;
    format_date(vmg_ buf, buflen, "%Y-%m-%d %H:%M:%S", 17, &lc,
                &date, dayno, daytime, tzabbr, tzofs);
}

/* ------------------------------------------------------------------------ */
/*
 *   Date arithmetic - add an integer or BigNumber: adds the given number of
 *   days to the date. 
 */
int CVmObjDate::add_val(VMG_ vm_val_t *result, vm_obj_id_t /*self*/,
                        const vm_val_t *val)
{
    /* get my date and time */
    int32_t dayno = get_ext()->dayno;
    int32_t daytime = get_ext()->daytime;
    
    /* the other value must be something numeric */
    if (val->typ == VM_INT)
    {
        /* it's a simple integer - add it as a number of days */
        dayno += val->val.intval;
    }
    else
    {
        /* it's not an int; calculate in the bignum domain */
        bignum_t<32>d(vmg_ val);

        /* make sure the integer portion won't overflow */
        double dd = floor((double)d);
        if (dd + dayno > INT32MAXVAL || dd + dayno < INT32MINVAL)
            err_throw(VMERR_NUM_OVERFLOW);

        /* add the whole part to my day number */
        dayno += (int32_t)dd;

        /* 
         *   convert the fractional part to milliseconds (by multiplying it
         *   by the number of milliseconds in a day), then add it to my time
         *   value 
         */
        daytime += round_int32((double)((d - dd) * (long)(24L*60*60*1000)));

        /* normalize, to carry time overflow/underflow into the date */
        caldate_t::normalize(dayno, daytime);
    }

    /* return a new Date object representing the result */
    result->set_obj(create(vmg_ FALSE, dayno, daytime));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Date arithmetic - subtract an integer or BigNumber to subtract a number
 *   of days from the date; or subtract another Date value to calculate the
 *   number of days between the dates. 
 */
int CVmObjDate::sub_val(VMG_ vm_val_t *result, vm_obj_id_t /*self*/,
                        const vm_val_t *val)
{
    /* get my date and time */
    int32_t dayno = get_ext()->dayno;
    int32_t daytime = get_ext()->daytime;

    /* the other value must be a Date or something numeric */
    if (val->typ == VM_OBJ && is_date_obj(vmg_ val->val.obj))
    {
        /* get the other date's contents */
        CVmObjDate *vdate = (CVmObjDate *)vm_objp(vmg_ val->val.obj);
        int32_t vdayno = vdate->get_ext()->dayno;
        int32_t vdaytime = vdate->get_ext()->daytime;

        /* we need fractional days, so calculate as bignum_t */
        bignum_t<32> d1((long)dayno), t1((long)daytime);
        bignum_t<32> d2((long)vdayno), t2((long)vdaytime);
        d1 += (t1 / (long)(24*60*60*1000));
        d2 += (t2 / (long)(24*60*60*1000));

        /* return a BigNumber containing the result */
        result->set_obj(CVmObjBigNum::create(vmg_ FALSE, d1 - d2));

        /* handled (we don't want to return a Date, as the other paths do) */
        return TRUE;
    }
    else if (val->typ == VM_INT)
    {
        /* it's a simple integer - subtract the number of days */
        dayno -= val->val.intval;
    }
    else
    {
        /* it's not an int; calculate in the bignum domain */
        bignum_t<32> d(vmg_ val);

        /* make sure the integer portion won't overflow */
        double dd = floor((double)d);
        if (dd - dayno > INT32MAXVAL || dd - dayno < INT32MINVAL)
            err_throw(VMERR_NUM_OVERFLOW);

        /* add the whole part to my day number */
        dayno -= (int32_t)dd;

        /* 
         *   convert the fractional part to milliseconds (by multiplying it
         *   by the number of milliseconds in a day), then subtract it from
         *   my time value 
         */
        daytime -= round_int32((double)((d - dd) * (long)(24L*60*60*1000)));

        /* normalize, to carry time overflow/underflow into the date */
        caldate_t::normalize(dayno, daytime);
    }

    /* return a new Date object representing the result */
    result->set_obj(create(vmg_ FALSE, dayno, daytime));

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Check two Date values for equality 
 */
int CVmObjDate::equals(VMG_ vm_obj_id_t, const vm_val_t *val, int) const
{
    if (val->typ == VM_OBJ && is_date_obj(vmg_ val->val.obj))
    {
        /* it's a Date - get the two day number and time values */
        CVmObjDate *vdate = (CVmObjDate *)vm_objp(vmg_ val->val.obj);
        int32_t day1 = get_ext()->dayno, day2 = vdate->get_ext()->dayno;
        int32_t time1 = get_ext()->daytime, time2 = vdate->get_ext()->daytime;

        /* we're equal if we represent the same date and time */
        return day1 == day2 && time1 == time2;
    }
    else
    {
        /* we're not equal to any other type */
        return FALSE;
    }
}


/*
 *   Compare two Date values 
 */
int CVmObjDate::compare_to(VMG_ vm_obj_id_t, const vm_val_t *val) const
{
    /* check the other object's type */
    if (val->typ == VM_OBJ && is_date_obj(vmg_ val->val.obj))
    {
        /* it's a Date - get the two day number and time values */
        CVmObjDate *vdate = (CVmObjDate *)vm_objp(vmg_ val->val.obj);
        int32_t day1 = get_ext()->dayno, day2 = vdate->get_ext()->dayno;
        int32_t time1 = get_ext()->daytime, time2 = vdate->get_ext()->daytime;

        /* compare by date and time */
        return (day1 != day2 ? day1 - day2 : time1 - time2);
    }
    else
    {
        /* we can't compare to other types */
        err_throw(VMERR_INVALID_COMPARISON);
        AFTER_ERR_THROW(return 0;)
    }
}


/* ------------------------------------------------------------------------ */
/* 
 *   load from an image file 
 */
void CVmObjDate::load_from_image(VMG_ vm_obj_id_t self,
                                 const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image file 
 */
void CVmObjDate::reload_from_image(VMG_ vm_obj_id_t self,
                                   const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjDate::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* allocate the extension */
    vm_date_ext *ext = vm_date_ext::alloc_ext(vmg_ this);
    ext_ = (char *)ext;

    /* read the image data */
    ext->dayno = osrp4s(ptr);
    ext->daytime = osrp4(ptr+4);
}


/* 
 *   save to a file 
 */
void CVmObjDate::save_to_file(VMG_ class CVmFile *fp)
{
    /* get our extension */
    vm_date_ext *ext = get_ext();

    /* write the extension data */
    fp->write_int4(ext->dayno);
    fp->write_uint4(ext->daytime);
}

/* 
 *   restore from a file 
 */
void CVmObjDate::restore_from_file(VMG_ vm_obj_id_t self,
                                   CVmFile *fp, CVmObjFixup *)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* allocate the extension structure */
    vm_date_ext *ext = vm_date_ext::alloc_ext(vmg_ this);
    ext_ = (char *)ext;

    /* read the data */
    ext->dayno = fp->read_int4();
    ext->daytime = fp->read_uint4();
}

/* ------------------------------------------------------------------------ */
/*
 *   Construct a list of integer values for return from a method
 */
static void make_int_list(VMG_ vm_val_t *retval, int cnt, ...)
{
    /* create the list */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, cnt));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* start the varargs scan */
    va_list args;
    va_start(args, cnt);

    /* add the elements */
    for (int i = 0 ; i < cnt ; ++i)
    {
        vm_val_t ele;
        ele.set_int(va_arg(args, int));
        lst->cons_set_element(i, &ele);
    }

    /* done with the varargs */
    va_end(args);
}

/*
 *   Add a string to a return list 
 */
static void enlist_str(VMG_ CVmObjList *lst, int idx,
                       const char *str, size_t len)
{
    /* set up a new string, or nil if the string is null */
    vm_val_t ele;
    if (str != 0)
        ele.set_obj(CVmObjString::create(vmg_ FALSE, str, len));
    else
        ele.set_nil();

    /* add the list element */
    lst->cons_set_element(idx, &ele);
}

/* add a string from a date_parse_string */
static void enlist_str(VMG_ CVmObjList *lst, int idx,
                       const date_parse_string &str)
{
    enlist_str(vmg_ lst, idx, str.p, str.len);
}

/* ------------------------------------------------------------------------ */
/*
 *   parseDate method (static) 
 */
int CVmObjDate::s_getp_parseDate(VMG_ vm_val_t *retval, uint *oargc)
{
    /* parse the date using the Gregorian calendar */
    gregcaldate_t cal;
    return common_parseDate(vmg_ retval, oargc, &cal);
}

/*
 *   parseJulianDate method (static) 
 */
int CVmObjDate::s_getp_parseJulianDate(VMG_ vm_val_t *retval, uint *oargc)
{
    /* parse the date using the Julian calendar */
    julcaldate_t cal;
    return common_parseDate(vmg_ retval, oargc, &cal);
}

/*
 *   common parseDate handler for parseDate, parseJulianDate 
 */
int CVmObjDate::common_parseDate(VMG_ vm_val_t *retval, uint *oargc,
                                 multicaldate_t *cal)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 3, FALSE);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* set up a locale state interface */
    CVmDateLocale lc Pvmg0_P;

    /* retrieve the string to parse */
    const char *src = G_stk->get(0)->get_as_string(vmg0_);
    if (src == 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* retrieve the format string/list argument, if present */
    const vm_val_t *custom = 0;
    if (argc >= 2 && G_stk->get(1)->typ != VM_NIL)
        custom = G_stk->get(1);

    /* retrieve the reference date, if present */
    int32_t refday, reftime;
    if (argc >= 3 && G_stk->get(2)->typ != VM_NIL)
    {
        /* get the reference date */
        CVmObjDate *refdate = vm_val_cast(CVmObjDate, G_stk->get(2));
        if (refdate == 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get its contents */
        refday = refdate->get_ext()->dayno;
        reftime = refdate->get_ext()->daytime;
    }
    else
    {
        /* no reference date argument - get the current date and time */
        caldate_t::now(refday, reftime);
    }

    /* retrieve the reference time zone, if present */
    CVmTimeZone *reftz = get_tz_arg(vmg_ 3, argc);

    /* parse the date */
    int32_t dayno, daytime;
    date_parse_result result(cal);
    date_parse_string fmt_match[20];
    int fmt_match_cnt = countof(fmt_match);
    if (parse_date_string(vmg_ dayno, daytime, src, custom, &lc,
                          cal, refday, reftime, reftz, &result,
                          fmt_match, &fmt_match_cnt))
    {
        /* create the return list */
        retval->set_obj(CVmObjList::create(vmg_ FALSE, 5));
        G_stk->push(retval);
        CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);
        lst->cons_clear();

        /* element [1] is the return date */
        vm_val_t ele;
        ele.set_obj(CVmObjDate::create(vmg_ FALSE, dayno, daytime));
        lst->cons_set_element(0, &ele);

        /* [2] is the TimeZone object, if applicable */
        ele.set_nil();
        if (result.tz != reftz)
            ele.set_obj(CVmObjTimeZone::create(vmg_ result.tz));
        lst->cons_set_element(1, &ele);

        /* [3] is the fixed timezone offset in seconds, if applicable */
        ele.set_nil();
        if (result.has_tzofs())
            ele.set_int(result.tzofs / 1000);
        lst->cons_set_element(2, &ele);

        /* [4] next is the format string matched */
        ele.set_obj(CVmObjList::create(vmg_ FALSE, fmt_match_cnt));
        lst->cons_set_element(3, &ele);
        CVmObjList *flst = (CVmObjList *)vm_objp(vmg_ ele.val.obj);
        flst->cons_clear();

        /* populate the format string list */
        for (int fi = 0 ; fi < fmt_match_cnt ; ++fi)
            enlist_str(vmg_ flst, fi, fmt_match[fi]);

        /* [5] is a sublist with the source substrings */
        ele.set_obj(CVmObjList::create(vmg_ FALSE, 13));
        lst->cons_set_element(4, &ele);
        CVmObjList *slst = (CVmObjList *)vm_objp(vmg_ ele.val.obj);
        slst->cons_clear();

        /* now add the date component source strings */
        enlist_str(vmg_ slst, 0, result.pera);
        enlist_str(vmg_ slst, 1, result.pyy);
        enlist_str(vmg_ slst, 2, result.pmm);
        enlist_str(vmg_ slst, 3, result.pdd);
        enlist_str(vmg_ slst, 4, result.pdoy);
        enlist_str(vmg_ slst, 5, result.pw);
        enlist_str(vmg_ slst, 6, result.pampm);
        enlist_str(vmg_ slst, 7, result.phh);
        enlist_str(vmg_ slst, 8, result.pmi);
        enlist_str(vmg_ slst, 9, result.pss);
        enlist_str(vmg_ slst, 10, result.pms);
        enlist_str(vmg_ slst, 11, result.punix);
        enlist_str(vmg_ slst, 12, result.ptz);

        /* done with the list gc protection */
        G_stk->discard();
    }
    else
    {
        /* return nil */
        retval->set_nil();
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   formatDate method
 */
int CVmObjDate::getp_formatDate(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *oargc)
{
    gregcaldate_t cd;
    return formatDateGen(vmg_ self, retval, oargc, &cd);
}

/*
 *   formatJulianDate method 
 */
int CVmObjDate::getp_formatJulianDate(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *oargc)
{
    julcaldate_t cd;
    return formatDateGen(vmg_ self, retval, oargc, &cd);
}

/*
 *   general date formatter
 */
int CVmObjDate::formatDateGen(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *oargc, multicaldate_t *date)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the format string */
    const char *fmt = G_stk->get(0)->get_as_string(vmg0_);
    if (fmt == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the format string length and buffer pointer */
    size_t fmtlen = vmb_get_len(fmt);
    fmt += VMB_LEN;

    /* get my date and time in local time */
    CVmTimeZone *tz = get_tz_arg(vmg_ 1, argc);
    int32_t dayno, daytime, tzofs;
    const char *tzabbr = get_local_time(dayno, daytime, tzofs, tz);

    /* convert the day number to a calendar date */
    date->set_dayno(dayno);

    /* do a formatting pass to determine how much space we need */
    CVmDateLocale lc Pvmg0_P;
    size_t buflen = format_date(
        vmg_ 0, 0, fmt, fmtlen, &lc, date, dayno, daytime, tzabbr, tzofs);

    /* create a string for the result */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, buflen));
    CVmObjString *str = vm_objid_cast(CVmObjString, retval->val.obj);

    /* format the string into the buffer */
    format_date(vmg_ str->cons_get_buf(), buflen, fmt, fmtlen, &lc,
                date, dayno, daytime, tzabbr, tzofs);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* macro: write a character to the output buffer if there's room  */
#define wrtch(ch) (++outlen, buflen > 0 ? *buf++ = ch, --buflen : 0)

/* macros: write a null-terminated/counted length string */
#define wrtstr(str) wrtstrl(str, strlen(str))
#define wrttstr(str) wrtstrl(str + VMB_LEN, vmb_get_len(str))
#define wrtstrl(str, len) _wrtstrl(buf, buflen, outlen, str, len)

/* service routine: write a counted-length string */
static void _wrtstrl(char *&buf, size_t &buflen, size_t &outlen,
                     const char *str, size_t len)
{
    /* if it fits, copy the whole thing; otherwise copy as much as fits */
    if (len <= buflen)
    {
        memcpy(buf, str, len);
        buf += len;
        buflen -= len;
    }
    else if (buflen != 0)
    {
        memcpy(buf, str, buflen);
        buf += buflen;
        buflen = 0;
    }

    /* count it all in the output */
    outlen += len;
}

/* macro: write an n-digit number */
#define wrtnum(v, ndigits) \
    _wrtnum(buf, buflen, outlen, v, ndigits, pound, leadsp, roman)

/* service routine: write an n-digit number */
static void _wrtnum(char *&buf, size_t &buflen, size_t &outlen,
                    int v, int ndigits, int pound, char leadsp, int roman)
{
    char numbuf[20];
    int i = 0;
    int neg = FALSE;
    
    /* note the sign */
    if (v < 0)
    {
        wrtch('-');
        v = -v;
        neg = TRUE;
    }

    /* if desired, generate a Roman numeral, if it's in range (1-4999) */
    if (roman && !neg && v >= 1 && v <= 4999)
    {
        static const struct
        {
            const char *r;
            int val;
        } r[] = {
            { "M", 1000 },
            { "CM", 900 },
            { "D", 500 },
            { "CD", 400 },
            { "C", 100 },
            { "XC", 90 },
            { "L", 50 },
            { "X", 10 },
            { "IX", 9 },
            { "V", 5 },
            { "IV", 4 },
            { "I", 1 }
        };
        for (int i = 0 ; v != 0 && i < countof(r) ; )
        {
            /* if this numeral fits, append it */
            if (r[i].val <= v)
            {
                /* it fits - write it and deduct it from the balance */
                wrtstr(r[i].r);
                v -= r[i].val;
            }
            else
            {
                /* doesn't fit - move on to the next lower numeral */
                ++i;
            }
        }
        return;
    }

    /* generate digits */
    do {
        div_t q = div(v, 10);
        v = q.quot;
        numbuf[i++] = '0' + q.rem;
    } while (v != 0);

    /* 
     *   if we didn't generate as many digits as the field requires, add
     *   leading zeros or spaces, as desired
     */
    if (i < ndigits && (leadsp != 0 || !pound))
    {
        /* add leading digits or the specified spaces */
        char lead = leadsp != 0 ? leadsp : '0';
        for (int j = i ; j < ndigits ; ++j)
            wrtch(lead);
    }

    /* write the digits */
    for ( ; i > 0 ; --i, wrtch(numbuf[i])) ;
}

/* write a locale string list item */
#define wrtlistitem(item, idx, sticky) \
    _wrtlistitem(vmg_ buf, buflen, outlen, lc, item, idx, sticky)
static void _wrtlistitem(VMG_ char *&buf, size_t &buflen, size_t &outlen,
                         CVmDateLocale *lc, int item, int idx, int sticky)
{
    /* get the list item */
    size_t len;
    const char *str = lc->index_list(vmg_ len, item, idx, sticky);
    
    /* write the item */
    for ( ; len != 0 ; ++str, --len)
        wrtch(*str);
}

/*
 *   Internal date formatter.  Formats into a buffer; if no buffer is
 *   provided, counts the length required.  Note that the date is specified
 *   in the *local* time zone - the caller must adjust to local time before
 *   calling.  Null-terminates the result if there's room, but doesn't
 *   include the null terminator in the size count.
 *   
 *   Most of our format codes are the same as for C/C++/php strftime, which
 *   are mostly the same as MySQL DATE_FORMAT().  We support all of the
 *   strftime formats, with the same letter codes, except for the php tab
 *   (%t) and newline (%n), which I don't see any good reason to include when
 *   we could just as well use \t and \n.  We support all of the MySQL format
 *   options, although several have different codes (MySQL mostly follows the
 *   strftime codes, though).
 */
size_t CVmObjDate::format_date(VMG_ char *buf, size_t buflen,
                               const char *fmt, size_t fmtlen,
                               CVmDateLocale *lc,
                               const multicaldate_t *date,
                               int32_t dayno, int32_t daytime,
                               const char *tzabbr, int32_t tzofs) const
{
    const char *subfmt;
    size_t subfmtl;
    
    /* we haven't written anything to the buffer yet */
    size_t outlen = 0;

    /* scan the format string */
    for ( ; fmtlen != 0 ; ++fmt, --fmtlen)
    {
        if (*fmt == '%' && fmtlen >= 2)
        {
            /* skip the '%' */
            ++fmt, --fmtlen;

            /* parse flags */
            int pound = FALSE;
            int minus = FALSE;
            int roman = FALSE;
            char leadsp = 0;
            for (int found_flag = TRUE ; found_flag && fmtlen >= 2 ; )
            {
                /* assume we won't find another flag on this pass */
                found_flag = FALSE;
                
                /* check for the '#' flag (meaning varies by format code) */
                if (fmtlen >= 2 && *fmt == '#')
                {
                    pound = found_flag = TRUE;
                    ++fmt, --fmtlen;
                }
                
                /* ' ' and '\ ' (replace leading zeros with spaces) */
                if (fmtlen >= 2 && (*fmt == ' ' || *fmt == 0x15))
                {
                    leadsp = *fmt;
                    found_flag = TRUE;
                    ++fmt, --fmtlen;
                }

                /* check for '-' flag */
                if (fmtlen >= 2 && *fmt == '-')
                {
                    minus = found_flag = TRUE;
                    ++fmt, --fmtlen;
                }

                /* check for '&' flag */
                if (fmtlen >= 2 && *fmt == '&')
                {
                    roman = found_flag = TRUE;
                    ++fmt, --fmtlen;
                }
            }

            /* get the format code */
            switch (*fmt)
            {
            case 'a':
                /* abbreviated day name */
                wrtlistitem(LC_WKDY, date->weekday(), FALSE);
                break;

            case 'A':
                /* full day name */
                wrtlistitem(LC_WEEKDAY, date->weekday(), FALSE);
                break;

            case 'd':
                /* two-digit day of month (or one digit with '#') */
                wrtnum(date->d(), 2);
                break;

            case 'j':
                /* day of year 001-366, three digits with leading zeros */
                {
                    /* 
                     *   we can calculate the day of the year by subtracting
                     *   the day number of Jan 1 from the given date (and
                     *   adding 1 to get into range 1-366) 
                     */
                    caldate_t jan1(date->y(), 1, 1);
                    int j = dayno - jan1.dayno() + 1;
                    wrtnum(j, 3);
                }
                break;

            case 'J':
                /* 
                 *   Julian day number (the 4713 BC kind); '#' suppresses the
                 *   fractional part 
                 */
                {
                    /* get the day and time */
                    long dn = get_dayno(), dt = get_daytime();

                    /* if the time is past noon, it's on the next day */
                    if (dt > 12*60*60*1000)
                        dn += 1, dt -= 12*60*60*1000;

                    /* 
                     *   figure the combined date/time value, adjusting the
                     *   day number to the Julian day Epoch 
                     */
                    bignum_t<32> bdn(dn), bdt(dt);
                    bdn += 1721119L;
                    bdn += (bdt / (long)(24*60*60*1000));

                    /* format it and write it */
                    char jbuf[64];
                    bdn.format(jbuf, sizeof(jbuf), -1, 10);
                    wrttstr(jbuf);
                }
                break;

            case 'u':
                /* day of week 1-7 Monday-Sunday (ISO weekday) */
                wrtnum(date->iso_weekday(), 1);
                break;

            case 'w':
                /* day of week 0-6 Sunday-Saturday */
                wrtnum(date->weekday(), 1);
                break;

            case 't':
                /* day of month with ordinal suffix (non-strftime) */
                wrtnum(date->d(), 1);
                wrtlistitem(LC_ORDSUF, ordinal_index(date->d()), TRUE);
                break;

            case 'U':
                /* week number, 00-53, week 1 starting with first Sunday */
                wrtnum(date->weekno(0), 2);
                break;

            case 'W':
                /* week number, 00-53, week 1 starting with first Monday */
                wrtnum(date->weekno(1), 2);
                break;

            case 'V':
                /* ISO-8601:1988 week number of the year */
                wrtnum(date->iso_weekno(0), 2);
                break;

            case 'b':
                /* abbreviated month name */
                wrtlistitem(LC_MON, date->m() - 1, FALSE);
                break;

            case 'B':
                /* full month name */
                wrtlistitem(LC_MONTH, date->m() - 1, FALSE);
                break;

            case 'm':
                /* two-digit month 01-12 (one digit with #) */
                wrtnum(date->m(), 2);
                break;

            case 'C':
                /* two-digit century (e.g., 19 for 1900-1999) */
                wrtnum(date->y() / 100, 2);
                break;

            case 'g':
            case 'G':
                /* ISO-8601:1988 year ('g'=2 digits, 'G'=4 digits) */
                {
                    /* ISO-8601:1988 week number of the year */
                    int iy;
                    date->iso_weekno(&iy);
                    wrtnum(iy, *fmt == 'g' ? 2 : 4);
                }
                break;

            case 'y':
                /* two-digit year */
                wrtnum(ldiv(date->y(), 100).rem, 2);
                break;

            case 'Y':
                /* four-digit year */
                wrtnum(date->y(), 4);
                break;

            case 'e':
            case 'E':
                /* 
                 *   Year with AD/BC era before/after the year number.  For
                 *   'e', the era is always after, or always before on '-'.
                 *   For 'E', the era is AD before/BC after, or reversed on
                 *   '-'. 
                 */
                {
                    /* figure the era: positive years are AD, <=0 are BC */
                    int idx = 0; // assume AD
                    int yy = date->y();
                    if (yy <= 0)
                    {
                        idx = 1; // BC
                        yy = -yy + 1;
                    }

                    /* 
                     *   Figure the display order: "%-e" puts the era first
                     *   in all case; "%E" puts AD first; "%-E" puts BC first
                     */
                    if ((*fmt == 'e' && minus)
                        || (*fmt == 'E' && !minus && idx == 0)
                        || (*fmt == 'E' && minus && idx == 1))
                    {
                        /* era first */
                        wrtlistitem(LC_ERA, idx, FALSE);
                        wrtch(' ');
                        wrtnum(yy, 1);
                    }
                    else
                    {
                        /* year first */
                        wrtnum(yy, 1);
                        wrtch(' ');
                        wrtlistitem(LC_ERA, idx, FALSE);
                    }
                }
                break;

            case 'H':
                /* two-digit hour, 24-hour clock */
                wrtnum(daytime/(60*60*1000), 2);
                break;

            case 'I':
                /* two-digit hour, 12-hour clock */
                {
                    int hh = daytime/(60*60*1000);
                    if (hh == 0)
                        hh = 12;
                    else if (hh >= 13)
                        hh -= 12;

                    wrtnum(hh, 2);
                }
                break;

            case 'M':
                /* two-digit minute */
                wrtnum(daytime/(60*1000) % 60, 2);
                break;

            case 'P':
                /* upper-case AM or PM */
                wrtlistitem(LC_AMPM, daytime/(60*60*1000) >= 12 ? 1 : 0, FALSE);
                break;

            case 'p':
                /* lower-case am or pm */
                wrtlistitem(LC_AMPM, daytime/(60*60*1000) >= 12 ? 3 : 2, FALSE);
                break;

            case 'r':
                /* full 12-hour clock time: %I:%M:%S %P */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_12HOUR);
                goto do_subfmt;

            do_subfmt:
                {
                    /* recursively format the new format string */
                    size_t l = format_date(
                        vmg_ buf, buflen, subfmt, subfmtl, lc,
                        date, dayno, daytime, tzabbr, tzofs);

                    /* count it in our output length */
                    outlen += l;

                    /* advance our buffer pointer past the copied text */
                    if (buflen >= l)
                        buflen -= l, buf += l;
                    else
                        buf += buflen, buflen = 0;
                }
                break;

            case 'R':
                /* 24-hour clock time w/minutes: %H:%M */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_24HOUR);
                goto do_subfmt;

            case 'S':
                /* two-digit seconds */
                wrtnum(daytime/1000 % 60, 2);
                break;

            case 'N':
                /* three-digit milliseconds (non-strftime) */
                wrtnum(daytime % 1000, 3);
                break;

            case 'T':
                /* 24-hour clock time w/seconds: %H:%M:%S */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_24HOURSECS);
                goto do_subfmt;

            case 'X':
                /* locale time representation without the date */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_TIME);
                goto do_subfmt;

            case 'z':
                /* time zone abbreviation */
                {
                    for (const char *p = tzabbr ; *p != '\0' ; ++p)
                        wrtch(*p);
                }
                break;

            case 'Z':
                /* time zone UTC offset, four digits (+0500) */
                {
                    int32_t o = tzofs;
                    if (o < 0)
                        wrtch('-'), o = -o;
                    else
                        wrtch('+');

                    wrtnum(o/(60*60*1000), 2);
                    pound = FALSE;
                    wrtnum(o/(60*1000) % 60, 2);
                }
                break;

            case 'c':
                /* preferred locale date and time stamp */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_TIMESTAMP);
                goto do_subfmt;

            case 'D':
                /* short date - %m/%d/%y */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_SHORTDATE);
                goto do_subfmt;

            case 'F':
                /* database-style date: %Y-%m-%d */
                subfmt = "%Y-%m-%d";
                subfmtl = 8;
                goto do_subfmt;

            case 's':
                /* Unix Epoch timestamp as an integer */
                {
                    /* 
                     *   The Unix timestamp is the number of seconds after
                     *   (or before, if negative) 1/1/1970 00:00 UTC.  First
                     *   calculate the number of days after (before) 1/1/1970
                     *   by subtracting the Unix Epoch day number from our
                     *   internal day number; then multiply that by the
                     *   number of seconds in a day (24*60*60) to get the
                     *   Unix timestamp of midnight on that day, then add the
                     *   number of seconds into the day ('daytime' stores
                     *   milliseconds, so divide it by 1000).
                     *   
                     *   Note that we need to adjust back to UTC by
                     *   subtracting the time zone offset.
                     *   
                     *   As we've discussed elsewhere in this file, a double
                     *   is big enough for the Unix timestamp value for any
                     *   day number we can store, and represents it exactly
                     *   since we're working in whole numbers.
                     */
                    double utime = (dayno - caldate_t::UNIX_EPOCH_DAYNO)
                                   * 24.*60.*60.
                                   + daytime/1000
                                   - tzofs/1000;

                    /* format it to a buffer and copy to the output */
                    char ubuf[60];
                    sprintf(ubuf, "%.0lf", utime);
                    wrtstr(ubuf);
                }
                break;

            case 'x':
                /* preferred locale date representation */
                subfmt = lc->get(vmg_ subfmtl, LC_FMT_DATE);
                goto do_subfmt;

            case '%':
                /* a literal % */
                wrtch('%');
                break;

            default:
                /* 
                 *   anything else is an error; copy the whole %x literally
                 *   to make the unparsed format character apparent
                 */
                wrtch('%');
                wrtch(*fmt);
                break;
            }
        }
        else
        {
            /* it's an ordinary character, so copy it literally */
            wrtch(*fmt);
        }
    }

    /* add a nul if there's room (but don't count it in the result length) */
    if (buflen > 0)
        *buf++ = '\0';

    /* return the total space needed */
    return outlen;
}

/* done with our local macros */
#undef wrtch
#undef wrt2dig


/* ------------------------------------------------------------------------ */
/*
 *   compareTo method
 */
int CVmObjDate::getp_compareTo(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the other date */
    CVmObjDate *other = vm_val_cast(CVmObjDate, G_stk->get(0));
    if (other == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the respective dates */
    int32_t dayno = get_ext()->dayno;
    int32_t daytime = get_ext()->daytime;
    int32_t odayno = other->get_ext()->dayno;
    int32_t odaytime = other->get_ext()->daytime;

    /* do the comparison */
    retval->set_int(dayno != odayno ? dayno - odayno : daytime - odaytime);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getDate method
 */
int CVmObjDate::getp_getDate(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get my date in local time */
    int32_t dayno = get_local_date(get_tz_arg(vmg_ 0, argc));

    /* express it as a calendar date */
    caldate_t cd(dayno);

    /* return [year, month, monthday, weekday] */
    make_int_list(vmg_ retval, 4, cd.y, cd.m, cd.d, cd.weekday() + 1);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getJulianDay method
 */
int CVmObjDate::getp_getJulianDay(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the Julian day number for midnight UTC on my day number */
    caldate_t cd(get_ext()->dayno);
    bignum_t<32> jday(cd.julian_dayno());

    /* add my fraction of a day past midnight UTC */
    bignum_t<32> jt((long)get_ext()->daytime);
    jday += jt / (long)(24*60*60*1000);

    /* return a BigNumber result */
    retval->set_obj(CVmObjBigNum::create(vmg_ FALSE, &jday));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getJulianDate method
 */
int CVmObjDate::getp_getJulianDate(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get my date in local time */
    int32_t dayno = get_local_date(get_tz_arg(vmg_ 0, argc));

    /* get the julian date */
    caldate_t cd(dayno);
    int y, m, d;
    cd.julian_date(y, m, d);

    /* 
     *   Return [year, month, monthday, weekday].  Note that there's no
     *   separate Julian weekday calculation, since the Julian calendar and
     *   Gregorian agree on the day of the week for every day.
     */
    make_int_list(vmg_ retval, 4, y, m, d, cd.weekday() + 1);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getISOWeekDate method
 */
int CVmObjDate::getp_getISOWeekDate(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get my date in local time */
    int32_t dayno = get_local_date(get_tz_arg(vmg_ 0, argc));

    /* express it as a calendar date */
    caldate_t cd(dayno);

    /* return [iso year, iso week, iso day] */
    int iy;
    int iw = cd.iso_weekno(&iy);
    make_int_list(vmg_ retval, 3, iy, iw, cd.iso_weekday());

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getTime method
 */
int CVmObjDate::getp_getTime(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get my date and time in local time */
    int32_t dayno, daytime;
    get_local_time(dayno, daytime, get_tz_arg(vmg_ 0, argc));

    /* return [hour, minute, second, msec] */
    make_int_list(vmg_ retval, 4,
                  daytime/(24*60*60*1000),
                  daytime/(60*60*1000) % 60,
                  daytime/(60*1000) % 60,
                  daytime % 1000);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   addInterval method
 */
int CVmObjDate::getp_addInterval(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the interval list */
    vm_val_t *lst = G_stk->get(0);
    if (!lst->is_listlike(vmg0_))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* retrieve my date as a calendar object */
    int32_t dayno = get_ext()->dayno;
    caldate_t cd(dayno);

    /* retrieve my time as a double, to allow for overflows from int32 */
    double daytime = get_ext()->daytime;

    /* add each element from the list */
    int cnt = lst->ll_length(vmg0_);
    for (int i = 1 ; i <= cnt ; ++i)
    {
        /* get this interval element */
        vm_val_t ele;
        lst->ll_index(vmg_ &ele, i);

        /* treat nil as zero */
        if (ele.typ == VM_NIL)
            continue;

        /* add it to the appropriate date or time component */
        switch (i)
        {
        case 1: cd.y += ele.num_to_int(vmg0_); break;
        case 2: cd.m += ele.num_to_int(vmg0_); break;
        case 3: cd.d += ele.num_to_int(vmg0_); break;

        case 4: daytime += ele.num_to_double(vmg0_)*60.*60.*1000.; break;
        case 5: daytime += ele.num_to_double(vmg0_)*60.*1000.; break;
        case 6: daytime += ele.num_to_double(vmg0_)*1000.; break;
        }
    }

    /* carry overflows from the time into the day */
    double day_carry = floor(daytime / (24.*60.*60.*1000.));
    daytime -= day_carry * (24.*60.*60.*1000.);

    if (day_carry + cd.d > INT32MAXVAL || day_carry + cd.d < INT32MINVAL)
        err_throw(VMERR_NUM_OVERFLOW);

    cd.d += (int32_t)day_carry;

    /* return the new date, using our same local timezone */
    retval->set_obj(create(vmg_ FALSE, cd.dayno(), (int32_t)daytime));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   findWeekday method
 */
int CVmObjDate::getp_findWeekday(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(2, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the day of week and 'which' arguments */
    int32_t wday = G_stk->get(0)->num_to_int(vmg0_);
    int32_t which = G_stk->get(1)->num_to_int(vmg0_);

    /* check the weekday and 'which' ranges */
    if (wday < 1 || wday > 7 || which == 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* adjust wday to 0=Sunday */
    wday -= 1;

    /* get my date in local time */
    CVmTimeZone *tz = get_tz_arg(vmg_ 2, argc);
    int32_t dayno = get_local_date(tz);

    /* figure my weekday */
    caldate_t cd(dayno);
    int my_wday = cd.weekday();

    /* figure the difference between the target day and my day, mod 7 */
    int delta = (7 + wday - my_wday) % 7;

    /* 
     *   Go forward that many days, or backwards (7 - delta), to get the
     *   first occurrence before/after my date.  Then go forwards or
     *   backwards by additional weeks as needed.
     */
    if (which > 0)
        dayno += delta + (which-1)*7;
    else
        dayno -= ((7 - delta) % 7) + (-which-1)*7;

    /* return a new date at midnight on the given day */
    int32_t daytime = 0;
    tz->local_to_utc(dayno, daytime);
    retval->set_obj(create(vmg_ FALSE, dayno, daytime));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a TimeZone argument.  If the argument is nil or omitted, we'll
 *   return the default local system TimeZone object.  If it's a TimeZone
 *   object, we'll return that as given.  If it's a string or integer, we'll
 *   construct a TimeZone object using that value per the TimeZone
 *   constructors.
 *   
 *   'argn' is the argument number to fetch; 'argc' is the actual number of
 *   arguments passed to the method. 
 */
CVmTimeZone *CVmObjDate::get_tz_arg(VMG_ uint argn, uint argc)
{
    /* if the argument is nil or missing, use the default local zone */
    vm_val_t *argp = G_stk->get(argn);
    if (argn == argc || argp->typ == VM_NIL)
        return G_tzcache->get_local_zone(vmg0_);

    /* check for a TimeZone object */
    CVmObjTimeZone *tzobj;
    if (argn < argc && (tzobj = vm_val_cast(CVmObjTimeZone, argp)) != 0)
        return tzobj->get_tz();

    /* otherwise, accept anything that the TimeZone constructor accepts */
    return CVmObjTimeZone::parse_ctor_args(vmg_ argp, argc - argn);
}

/* ------------------------------------------------------------------------ */
/*
 *   setLocaleInfo method (static method)
 */
int CVmObjDate::s_getp_setLocaleInfo(VMG_ vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 0, TRUE);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* set up a locale state interface */
    CVmDateLocale lc Pvmg0_P;

    /* parse arguments */
    vm_val_t *argp = G_stk->get(0);
    if (argc == 1 && argp->is_listlike(vmg0_))
    {
        /* list of the first N elements */
        int n = argp->ll_length(vmg0_);
        for (int i = 0 ; i < n ; ++i)
        {
            /* get the element */
            vm_val_t ele;
            argp->ll_index(vmg_ &ele, i+1);

            /* set the next index */
            lc.set(vmg_ G_undo, i, &ele);
        }
    }
    else if ((argc & 0x0001) == 0)
    {
        /* even number of arguments; treat them Type, String pairs */
        for (uint i = 0 ; i < argc ; i += 2, argp -= 2)
            lc.set(vmg_ G_undo, argp->num_to_int(vmg0_), argp - 1);
    }
    else
    {
        /* unknown arguments */
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}
