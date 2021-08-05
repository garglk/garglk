/* $Header$ */

/* 
 *   Copyright (c) 2012 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmdate.h - CVmObjDate object
Function
  Defines the Date intrinsic class
Notes
  
Modified
  01/23/12 MJRoberts  - Creation
*/

#ifndef VMDATE_H
#define VMDATE_H

#include <math.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"


/* ------------------------------------------------------------------------ */
/*
 *   Service class for calendar date conversions.  This converts between
 *   calendar dates and day number values, using our Epoch of March 1, year
 *   0.
 */
struct caldate_t
{
    /* create at the Epoch (3/1/0000 midnight UTC) */
    caldate_t() : y(0), m(3), d(1) { }

    /* create from a calendar date */
    caldate_t(int y, int m, int d) : y(y), m(m), d(d) { }

    /* create from a day number */
    caldate_t(int32_t dayno) { set_dayno(dayno); }

    /* set to the current date (UTC) */
    void set_now()
    {
        int32_t dayno, daytime;
        now(dayno, daytime);
        set_dayno(dayno);
    }

    /* get the Julian day number */
    double julian_dayno() const { return dayno() + 1721119.5; }

    /* figure the date on the Julian calendar */
    void julian_date(int &jy, int &jm, int &jd)
    {
        int32_t z = (int32_t)floor(julian_dayno() + 0.5);

        int32_t a = z;
        int32_t b = a + 1524;
        int32_t c = (int32_t)floor((b - 122.1) / 365.25);
        int32_t d = (int32_t)floor(365.25 * c);
        int32_t e = (int32_t)floor((b - d) / 30.6001);

        jm = e < 14 ? e - 1 : e - 13;
        jy = jm > 2 ? c - 4716 : c - 4715;
        jd = b - d - (int32_t)floor(30.6001 * e);
    }

    /* set my internal (Gregorian) date from a Julian date */
    void set_julian_date(int jy, int jm, int jd)
    {
        /* adjust the month to 1-12 range */
        div_t mq = div(jm - 1, 12);
        if (mq.rem <= 0)
            mq.rem += 12, mq.quot -= 1;
        jm = mq.rem + 1;
        jy += mq.quot;
        

        /* 
         *   Calculate the Julian day number for the julian date, then
         *   subtract our Epoch's Julian day number to get our day number.
         */
        if (jm <= 2)
        {
            jy -= 1;
            jm += 12;
        }
        set_dayno((int32_t)(floor(365.25 * (jy + 4716))
                            + floor(30.6001 * (jm + 1))
                            + jd - 1524.5 - 1721119.5));
    }

    /* set from a day number */
    void set_dayno(int32_t dayno)
    {
        int32_t y = (int32_t)floor((10000*(double)dayno + 14780)/3652425.0);
        int32_t d = dayno - (365*y + divfl(y, 4) - divfl(y, 100)
                             + divfl(y, 400));
        if (d < 0)
        {
            y -= 1;
            d = dayno - (365*y + divfl(y, 4) - divfl(y, 100) + divfl(y, 400));
        }
        int32_t m = (100*d + 52)/3060;

        this->y = y + (m+2)/12;
        this->m = (m + 2)%12 + 1;
        this->d = d - (m*306 + 5)/10 + 1;
    }

    /*
     *   Normalize the date.  This takes the current m/d/y setting,
     *   calculates the day number it represents, and then figure the m/d/y
     *   for that day number.  This ensures that a date entered with an
     *   overflow in one of the fields (e.g., "February 30") is translated
     *   into a proper calendar date. 
     */
    void normalize_date()
    {
        set_dayno(dayno());
    }

    /* 
     *   Integer divide-and-get-floor calculation - i.e., round towards
     *   negative infinity.  This is the same as ordinary C integer division
     *   for positive numbers, but for negative numbers most C
     *   implementations round towards zero.
     */
    static inline int32_t divfl(int32_t a, int32_t b)
    {
        ldiv_t ld = ldiv(a, b);
        return (ld.rem < 0 ? ld.quot - 1 : ld.quot);
    }

    /* get the day number of this calendar date */
    int32_t dayno() const
    {
        /* 
         *   Adjust the month to our odd range where the year starts in
         *   March: 0 = March, 1 = April, ... 11 = Feb.  (If this yields a
         *   negative remainder, adjust to a positive index by going back a
         *   year.)
         */
        div_t mq = div(this->m - 3, 12);
        if (mq.rem < 0)
            mq.rem += 12, mq.quot -= 1;

        int m = mq.rem;
        int y = this->y + mq.quot;
        return 365*y + divfl(y, 4) - divfl(y, 100) + divfl(y, 400)
            + (m*306 + 5)/10 + (d-1);
    }

    /* day number of the Unix Epoch (1/1/1970 UTC) */
    static const int32_t UNIX_EPOCH_DAYNO = 719468;

    /* get the weekday for this date (0=Sunday, 1=Monday, etc) */
    int weekday() const
    {
        const static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int yy = y - (m < 3 ? 1 : 0);
        return (yy + yy/4 - yy/100 + yy/400 + t[m-1] + d) % 7;
    }

    /* get the ISO weekday for this date (1=Monday, ... 7=Sunday) */
    int iso_weekday() const
        { int w = weekday(); return w == 0 ? 7 : w; }

    /*
     *   Figure the week number, where week 1 starts on the first <weekday>
     *   of the year (0=Sunday). 
     */
    int weekno(int weekday) const
    {
        /* 
         *   Figure days until the first <weekday> of the year: that's the
         *   start of week 1, so the day number of the start of week 1 is the
         *   jan1 day number plus this delta.  The start of week 0 is 7 days
         *   before that.
         */
        caldate_t jan1(y, 1, 1);
        int delta = (7 + weekday - jan1.weekday()) % 7;
        int week0_dayno = jan1.dayno() + delta - 7;

        /* figure which week we're in and return the result */
        return (dayno() - week0_dayno)/7;
    }

    /*
     *   Figure the ISO-8601 week number of this date.  Week 1 is the week
     *   containing the year's first Thursday, and the first day of each week
     *   is Monday.  Fills in 'year' (if provided) with the year the week
     *   belongs to, which might be the previous or next calendar year.
     */
    int iso_weekno(int *year) const
    {
        /* 
         *   Find the Thursday of the current ISO week.  To do this, figure
         *   our weekday, subtract the ISO weekday number minus 1 to get the
         *   nearest preceding Monday (the start of the ISO week), then add 3
         *   to get the following Thursday.  The year containing the Thursday
         *   of a given week is by definition the year which that whole week
         *   (starting on Monday) belongs to.
         */
        int wday = iso_weekday();
        int32_t this_thu_dayno = dayno() - (wday-1) + 3;
        caldate_t this_thu(this_thu_dayno);

        /* the year containing the Thursday is the week's calendar year */
        if (year != 0)
            *year = this_thu.y;

        /*
         *   Find the first Thursday of the calendar year we just calculated.
         *   ISO week 1 of a given calendar year is the week that contains
         *   the first Thursday of the year.  So find the first Thursday on
         *   or after Jan 1.
         */
        caldate_t jan1(this_thu.y, 1, 1);
        int jan1_wday = jan1.weekday();
        int32_t first_thu_dayno = jan1.dayno() + ((11 - jan1_wday) % 7);

        /*
         *   Calculate the number of weeks (== the number of days divided by
         *   seven) between the Thursday of the target week and the Thursday
         *   of the first week.  This gives us the difference in week
         *   numbers; since the first week is week #1, adding 1 to the
         *   difference gives us the target week number.
         */
        return ((this_thu_dayno - first_thu_dayno)/7) + 1;
    }

    /* get the current system date/time (UTC) */
    static void now(int32_t &dayno, int32_t &daytime);

    /* normalize a date/time to bring the time within 00:00-24:00 */
    static void normalize(int32_t &dayno, int32_t &daytime);

    /* calendar year, month (1=January), and day of month */
    int y;
    int m;
    int d;
};


/* ------------------------------------------------------------------------ */
/*
 *   Multi-calendar interface.  This virtualizes the calculations in
 *   caldate_t for different calendars. 
 */
struct multicaldate_t
{
    virtual ~multicaldate_t() { }
    virtual int32_t dayno() const = 0;
    virtual void set_dayno(int32_t d) = 0;
    virtual int weekday() const = 0;
    virtual int iso_weekday() const = 0;
    virtual int iso_weekno(int *year) const = 0;

    /* get the day number of January 1 of year 'y' */
    virtual int32_t jan1_dayno() const = 0;

    /* 
     *   get the week number for this date, for the week starting on the
     *   given weekday
     */
    int weekno(int weekday) const
    {
        /* 
         *   Figure days until the first <weekday> of the year: that's the
         *   start of week 1, so the day number of the start of week 1 is the
         *   jan1 day number plus this delta.  The start of week 0 is 7 days
         *   before that.
         */
        caldate_t jan1(jan1_dayno());
        int delta = (7 + weekday - jan1.weekday()) % 7;
        int week0_dayno = jan1.dayno() + delta - 7;

        /* figure which week we're in and return the result */
        return (dayno() - week0_dayno)/7;
    }

    virtual void set_date(int y, int m, int d) = 0;
    int y() const { return y_; }
    int m() const { return m_; }
    int d() const { return d_; }

protected:
    /* year, month, day on the subclass calendar */
    int y_, m_, d_;
};

struct gregcaldate_t: multicaldate_t
{
    gregcaldate_t() { }
    gregcaldate_t(int32_t d) { gregcaldate_t::set_dayno(d); }
    
    virtual int32_t dayno() const
    {
        return cd.dayno();
    }

    virtual void set_dayno(int32_t d)
    {
        cd.set_dayno(d);
        y_ = cd.y;
        d_ = cd.d;
        m_ = cd.m;
    }

    virtual void set_date(int y, int m, int d)
    {
        y_ = cd.y = y;
        m_ = cd.m = m;
        d_ = cd.d = d;
    }

    virtual int32_t jan1_dayno() const
    {
        caldate_t jan1(y_, 1, 1);
        return jan1.dayno();
    }

    virtual int weekday() const { return cd.weekday(); }
    virtual int iso_weekday() const { return cd.iso_weekday(); }
    virtual int iso_weekno(int *year) const { return cd.iso_weekno(year); }

    caldate_t cd;
};

struct julcaldate_t: multicaldate_t
{
    virtual int32_t dayno() const
    {
        return cd.dayno();
    }

    virtual void set_dayno(int32_t d)
    {
        cd.set_dayno(d);
        cd.julian_date(y_, m_, d_);
    }

    virtual void set_date(int y, int m, int d)
    {
        cd.set_julian_date(y, m, d);
        y_ = y;
        m_ = m;
        d_ = d;
    }

    virtual int32_t jan1_dayno() const
    {
        caldate_t jan1;
        jan1.set_julian_date(y_, 1, 1);
        return jan1.dayno();
    }

    virtual int weekday() const { return cd.weekday(); }
    virtual int iso_weekday() const { return cd.iso_weekday(); }
    virtual int iso_weekno(int *year) const { return cd.iso_weekno(year); }

    caldate_t cd;
};



/* ------------------------------------------------------------------------ */
/*
 *   A Date value is stored internally as two 32-bit integers: the day number
 *   of the date, which is the number of days since March 1, year 0, midnight
 *   UTC; and the time of day as the number of milliseconds past midnight
 *   (UTC) on that day.
 *   
 *   The image file data block is arranged as follows:
 *   
 *.   UINT4 day_number
 *.   UINT2 time_of_day_ms
 */

/* ------------------------------------------------------------------------ */
/*
 *   Forward declarations 
 */
typedef struct date_parse_result date_parse_result;


/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension data structure.  This contains the date (as a
 *   Gregorian day number since March 1, 0000 UTC), time of day (as the
 *   number of milliseconds past midnight), and the TimeZone object that we
 *   use to determine the local time zone for formatting the date as a string
 *   or extracting calendar or clock elements.
 */
struct vm_date_ext
{
    /* allocate the structure */
    static vm_date_ext *alloc_ext(VMG_ class CVmObjDate *self);

    /* day number - number of days since March 1, year 0000 UTC */
    int32_t dayno;

    /* time of day - number of milliseconds past midnight UTC on dayno*/
    int32_t daytime;
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjDate intrinsic class definition
 */

class CVmObjDate: public CVmObject
{
    friend class CVmMetaclassDate;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* is this a CVmObjDate object? */
    static int is_date_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* get my date and time values */
    int32_t get_dayno() const { return get_ext()->dayno; }
    int32_t get_daytime() const { return get_ext()->daytime; }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* create with a given timestamp */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              int32_t dayno, int32_t daytime);

    /* create from an os_time_t value */
    static vm_obj_id_t create_from_time_t(VMG_ int in_root_set, os_time_t t);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);


    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* cast to a string */
    const char *cast_to_string(VMG_ vm_obj_id_t self, vm_val_t *newstr) const;

    /* format into a string buffer */
    void format_string_buf(VMG_ char *buf, size_t buflen) const;

    /* format using a template */
    size_t format_date(VMG_ char *buf, size_t buflen,
                       const char *fmt, size_t fmtlen,
                       class CVmDateLocale *lc,
                       const multicaldate_t *date,
                       int32_t dayno, int32_t daytime,
                       const char *tzabbr, int32_t tzofs) const;

    /* date arithmetic - add an integer or BigNumber to add days */
    int add_val(VMG_ vm_val_t *result, vm_obj_id_t self, const vm_val_t *val);

    /* 
     *   date arithmetic - subtract an integer or BigNumber to subtract days
     *   from the date, or subtract another Date to calculate the number of
     *   days between the dates 
     */
    int sub_val(VMG_ vm_val_t *result, vm_obj_id_t self, const vm_val_t *val);

    /* compare two dates */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;
    int compare_to(VMG_ vm_obj_id_t self, const vm_val_t *val) const;

    /* 
     *   receive savepoint notification - we don't keep any
     *   savepoint-relative records, so we don't need to do anything here 
     */
    void notify_new_savept() { }
    
    /* apply an undo record - we're immutable, so there's no undo */
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    
    /* discard an undo record */
    void discard_undo(VMG_ struct CVmUndoRecord *) { }
    
    /* mark our undo record references */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark our references (we have none) */
    void mark_refs(VMG_ uint) { }

    /* remove weak references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* reload from an image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* Date objects are immutable, so they can't change after loading */
    int is_changed_since_load() const { return FALSE; }

protected:
    /* create a with no initial contents */
    CVmObjDate() { ext_ = 0; }

    /* create with a given timestamp */
    CVmObjDate(VMG_ int32_t dayno, uint32_t daytime);

    /* get my extension data */
    vm_date_ext *get_ext() const { return (vm_date_ext *)ext_; }

    /* get my date/time in the given time zone; returns the format abbr */
    const char *get_local_time(
        int32_t &dayno, int32_t &daytime, class CVmTimeZone *tz) const
        { int32_t ofs; return get_local_time(dayno, daytime, ofs, tz); }

    /* 
     *   get my date/time in the given time zone, filling in the time zone's
     *   offset from UTC in milliseconds 
     */
    const char *get_local_time(
        int32_t &dayno, int32_t &daytime, int32_t &tzofs,
        class CVmTimeZone *tz) const;

    /* get my date (as a day number since 1/1/0000) in the local time zone */
    int32_t get_local_date(class CVmTimeZone *tz) const
    {
        int32_t dayno, daytime;
        (void)get_local_time(dayno, daytime, tz);
        return dayno;
    }

    /* get a TimeZone argument to one of our methods */
    static class CVmTimeZone *get_tz_arg(VMG_ uint argn, uint argc);

    /* load or reload image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* internal parsing routines */
    static int parse_string_fmt(
        VMG_ date_parse_result *res,
        const char *&str, size_t &len,
        const char *fmt, size_t fmtlen, class CVmDateLocale *lc);
    static int parse_date_string(
        VMG_ int32_t &dayno, int32_t &daytime,
        const char *str, const vm_val_t *custom, class CVmDateLocale *lc,
        multicaldate_t *cal,
        int32_t refday, int32_t reftime, CVmTimeZone *reftz,
        struct date_parse_result *resultp,
        struct date_parse_string *fmtlist, int *nfmtlist);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* parseDate method */
    int getp_parseDate(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_parseDate(vmg_ retval, argc); }

    /* static property evaluator for parseDate */
    static int s_getp_parseDate(VMG_ vm_val_t *retval, uint *argc);

    /* parseJulianDate method */
    int getp_parseJulianDate(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_parseJulianDate(vmg_ retval, argc); }

    /* static property evaluator for parseDate */
    static int s_getp_parseJulianDate(VMG_ vm_val_t *retval, uint *argc);

    /* common handler for parseDate, parseJulianDate */
    static int common_parseDate(VMG_ vm_val_t *retval, uint *argc,
                                multicaldate_t *cal);

    /* general handler for formatDate, formatJulianDate */
    int formatDateGen(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                      multicaldate_t *date);

    /* formatDate method */
    int getp_formatDate(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* formatJulianDate method */
    int getp_formatJulianDate(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* compareTo */
    int getp_compareTo(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getDate method */
    int getp_getDate(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getJulianDay method */
    int getp_getJulianDay(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getJulianDate method */
    int getp_getJulianDate(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getISOWeekDate */
    int getp_getISOWeekDate(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getTime method */
    int getp_getTime(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* addInterval method */
    int getp_addInterval(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* findWeekday method */
    int getp_findWeekday(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* setLocaleInfo method */
    int getp_setLocaleInfo(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_setLocaleInfo(vmg_ retval, argc); }

    /* static property evaluator for setLocalInfo */
    static int s_getp_setLocaleInfo(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluation function table */
    static int (CVmObjDate::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjDate metaclass registration table object 
 */
class CVmMetaclassDate: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "date/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjDate();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjDate();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjDate::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjDate::call_stat_prop(
            vmg_ result, pc_ptr, argc, prop);
    }

    int set_stat_prop(VMG_ class CVmUndo *, vm_obj_id_t /*self*/,
                      vm_val_t * /* class_state */,
                      vm_prop_id_t /*prop*/, const vm_val_t * /*val*/)
    {
        return TRUE;
    }
};

#endif /* VMDATE_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjDate)
