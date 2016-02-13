/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtz.h - TADS time zone management
Function
  TADS uses the IANA zoneinfo database (aka the Olson database) for
  converting time values between universal time (UTC) and local time
  representations.  This is used in particular in the Date intrinsic class,
  which provides a bytecode API for time and date conversions, formatting,
  and arithmetic.

  This zoneinfo database lets us determine the correct local time, given a
  UTC time, in just about any regional time zone on just about any date since
  time zones were invented.  It takes into account the ever-changing daylight
  savings rules as well as historical time zone realignments and
  redefinitions.  We don't just project the current time zone rules back in
  time, as most operating systems do in their time managers; the zoneinfo
  database tells us the actual changes that occurred.  For example, in the
  United States, we'll properly start daylight savings time from the last
  Sunday in April in 1986, but use the second Sunday in April in 2007 and
  later.  Likewise, we'll properly display EWT (Eastern War Time) if asked
  for a local time in New York in 1943.

  We use a custom binary version of the zoneinfo database.  The TADS source
  code distribution includes a compiler (written in TADS) that reads the
  zoneinfo sources files as distributed by IANA and generates our custom
  binary file.  We don't compile the file merely to have it in binary form,
  but rather because the source form of the information requires quite a lot
  of work to interpret.  It uses a considerable amount of abstraction, both
  to make the zone histories and rules more readily human-readable and to
  make them more compact and reusable (e.g., it allows for expressing rules
  at the national level that span multiple time zones, such as in the US).
  The main reason to compile the material is to boil down the abstraction to
  a form that can be used efficiently at run-time.  While we're at it we
  generate a binary file format that's fast and easy to read.

  This package includes a cache manager that reads zone data from the
  binary file on demand.  The zoneinfo database (and thus our custom binary
  version of it) contains quite a lot of information, since it has historical
  records on each of about 450 time zones around the world.  It amounts to
  about 300k of compiled binary data in our format.  Rather than read all of
  that into memory, we'll read it as needed per time zone.  Most programs
  will use only the local time zone of the local system, so in most cases
  we'll never need to read more than one zone.
Notes
  None
Modified
  02/05/12 MJRoberts  - creation
*/

#ifndef VMTZ_H
#define VMTZ_H

#include "t3std.h"
#include "vmglob.h"
#include "vmobj.h"


/* ------------------------------------------------------------------------ */
/*
 *   Time zone cache.  This loads time zone data on demand.
 */
class CVmTimeZoneCache
{
    friend class CVmTimeZone;
    
public:
    CVmTimeZoneCache();
    ~CVmTimeZoneCache();

    /*
     *   Parse a timezone name, returning a CVmTimeZone object representing
     *   the zone.  This accepts names in the following formats:
     *   
     *.    :local       - the local zone
     *   
     *.    UTC+offset   - or GMT+ofs, Z+ofs, or simply +ofs: a zone at the
     *.                   given hh[:mm[:ss]] offset from UTC.  This type of
     *.                   zone uses a fixed offset year round and for all
     *.                   dates, not tied to any location's history.
     *   
     *.    zoneinfoName - the name of a zoneinfo database entry, such as
     *.                   "America/Los_Angeles".  This type uses the zone
     *.                   history from the database.
     */
    class CVmTimeZone *parse_zone(VMG_ const char *name, size_t len);

    /* 
     *   Get a time zone object by name from the zoneinfo database; returns
     *   the cached copy if present, otherwise loads the entry from disk.
     */
    class CVmTimeZone *get_db_zone(VMG_ const char *name, size_t len);
    class CVmTimeZone *get_db_zone(VMG_ const char *name)
        { return get_db_zone(vmg_ name, strlen(name)); }

    /* 
     *   Get a time zone object by GMT offset, in seconds.  Returns a zone
     *   that represents the fixed GMT offset, not for any particular
     *   location.
     */
    class CVmTimeZone *get_gmtofs_zone(VMG_ int32_t gmtofs_secs);

    /*
     *   Get a time zone object by abbreviation.  This searches the
     *   abbreviation list for a match, and returns the corresponding
     *   timezone object if found.  We also fill in the GMT offset for the
     *   abbreviation - this is required when the zone is specified by
     *   abbreviation because an abbreviation carries a standard time or
     *   daylight time designation, which we'd normally infer from the zone
     *   and date information.  An explicit daylight or standard time
     *   designation overrides the time that's normally in effect on the
     *   given date.  For example, "1/1/2012 12:00 PDT" corresopnds to
     *   1/1/2012 19:00 GMT, whereas "1/1/2012 12:00" America/Los_Angeles
     *   corresponds to 20:00 GMT.  The difference is that standard time is
     *   normally in effect on that date: a time specified by location
     *   (America/Los_Angeles) is interpreted as a wall clock time for that
     *   date according to the prevailing standard/daylight rules for the
     *   zone, whereas a time explicitly in PDT is interpreted as daylight
     *   time regardless of whether daylight or standard time is actually in
     *   effect in the zone on the given date.
     */
    class CVmTimeZone *get_zone_by_abbr(VMG_ int32_t *gmtofs_ms,
                                        const char *name, size_t len);

    /* 
     *   Create a missing database zone.  This creates a placeholder zone
     *   representing a zone imported from a saved game using a different
     *   version of the database that includes a zone that's not defined in
     *   the version we're using.
     */
    class CVmTimeZone *create_missing_zone(
        VMG_ const char *name, size_t namelen, const os_tzinfo_t *desc);

    /* open the zoneinfo binary file */
    static osfildef *open_zoneinfo_file(VMG0_);

    /*
     *   Get the system default local time zone.  Returns a CVmTimeZone
     *   object representing the local zone. 
     */
    class CVmTimeZone *get_local_zone(VMG0_);

    /* is the given zone the special local zone? */
    int is_local_zone(class CVmTimeZone *zone) const
        { return zone == local_zone_; }

protected:
    /* load the zoneinfo database index from the binary file */
    int load_db_index(VMG0_);

    /* get or load the CVmTimeZone object for a hash entry */
    CVmTimeZone *tz_from_hash(VMG_ class ZoneHashEntry *entry);

    /* parse a "UTC+-h[:mm[:ss]]" offset string into a zone */
    CVmTimeZone *parse_zone_hhmmss(
        VMG_ const char *prefix, const char *name, size_t len);

    /* parse a zone spec in POSIX TZ format (EST5, EST5EDT, EST5EDT4) */
    CVmTimeZone *parse_zone_posixTZ(VMG_ const char *name, size_t len);

    /* parse an [+-]h[:mm[:ss]] time/offset value */
    int parse_hhmmss(int32_t &ofs, const char *&name, size_t &len,
                     int sign_required);

    /* search for a set of zone specifications */
    class ZoneHashEntry *search(
        VMG_ const struct ZoneSearchSpec *specs, int cnt);
    class ZoneHashEntry *search(
        VMG_ const struct ZoneSearchSpec *specs, class AbbrHashEntry **e,
        int cnt, class ZoneHashEntry *zone);

    /* have we attempted to load the index yet? */
    int index_loaded_;

    /* did we successfully load the index? */
    int index_loaded_ok_;

    /* 
     *   Cached local zone object.  Determining the local time zone can be a
     *   non-trivial amount of work, so we cache this after we figure it out
     *   the first time. 
     */
    class CVmTimeZone *local_zone_;

    /* zone name hash table for zones loaded from the zoneinfo database */
    class CVmHashTable *db_tab_;

    /* zone name hash table for synthesized zones - GMT+ofs, EST5EDT, etc */
    class CVmHashTable *synth_tab_;

    /* abbreviation hash table */
    class CVmHashTable *abbr_tab_;

    /* head of linked list of *loaded* zone objects */
    class ZoneHashEntry *first_loaded_;

    /* zone index, loaded from the binary file */
    char *zone_bytes_;

    /* link index, loaded from the binary file */
    char *link_bytes_;

    /* abbreviation mapping index, loaded from the binary file */
    char *abbr_bytes_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Time type.  This describes the settings for a time zone during the
 *   period covered by a transition.
 */
struct vmtz_ttype
{
    vmtz_ttype() { }
    vmtz_ttype(const struct vmtzquery *query);
    vmtz_ttype(int32_t gmtofs, int32_t save, const char *fmt)
    {
        this->gmtofs = gmtofs;
        this->save = save;
        this->fmt = fmt;
    }

    /* 
     *   standard time ofset, in milliseconds - add this to UTC to get local
     *   standard time 
     */
    int32_t gmtofs;

    /* 
     *   daylight time offset, in milliseconds - add to local standard time
     *   to get local wall clock time 
     */
    int32_t save;

    /* time zone format string abbreviation ("EDT", etc) */
    const char *fmt;
};

/*
 *   Transition - this records a change in a time zone's rules.  Each
 *   transition covers the period from its FROM date (inclusive) to (but not
 *   including) the next transition's FROM date.
 */
struct vmtz_trans
{
    vmtz_trans() { }
    vmtz_trans(int32_t dayno, int32_t daytime, vmtz_ttype *ttype)
    {
        this->dayno = dayno;
        this->daytime = daytime;
        this->ttype = ttype;
    }
        
    /* 
     *   Date/time of the transition, relative to 3/1/0000 UTC; the time is
     *   in milliseconds after midnight.  This is starting time for the
     *   transition - it's the moment that our ttype comes into effect.
     */
    int32_t dayno;
    int32_t daytime;

    /* compare my date to the given date/time (time in milliseconds) */
    int compare_to(int32_t dayno, int32_t daytime, int local) const;

    /* create a List object representing the transition */
    vm_obj_id_t get_as_list(VMG0_) const;

    /* time zone settings in effect on and after this transition */
    struct vmtz_ttype *ttype;
};

/*
 *   Time zone daylight savings rule.  This describes a switch between
 *   standard time and daylight time.  Any given rule fires once a year.
 *   Rules apply to the period after the last enumerated transition in the
 *   time zone; this allows us to project the current rules that apply in the
 *   zone forward in time indefinitely, without having to enumerate all of
 *   the possible transitions (which would be impractical given that our time
 *   type can in principle represent millions of years into the future).
 */
struct vmtz_rule
{
    /* the format abbreviation to use when the rule is in effect */
    const char *fmt;

    /* the gmt offset for standard time during this rule, in milliseconds */
    int32_t gmtofs;

    /* daylight savings time offset from standard time, in milliseconds */
    int32_t save;

    /* the month the rule takes effect (1=January) */
    char mm;

    /* 
     *   When in the month the rule takes effect:
     *   
     *.    0 -> takes effect on the fixed day of the month <dd>
     *.    1 -> last <weekday> of the month
     *.    2 -> first <weekday> on or after <dd>
     *.    3 -> first <weekday> on or before <dd>
     */
    char when;

    /* 
     *   The day of the month the rule takes effect (1-366).  (This can be
     *   greater than 31, because it's also used to encode POSIX TZ-style
     *   rules with "Julian day of year counting Feb 29".  Our calendar
     *   calculator can handle overflows in the day of month, carrying them
     *   automatically into subsequent months, so we can encode the Nth day
     *   of the year as simply January Nth, and we'll get the right result
     *   even if N > 31.
     */
    short dd;
    
    /* the day of the week the rule takes effect (1=Sunday) */
    char weekday;

    /* 
     *   Time zone for the effective time/date: 0 -> local wall clock time
     *   (with the zone settings for the period immediately before the rule
     *   takes effect); 0x40 -> local standard time (for the period
     *   immediately before the rule takes effect), 0x80 -> UTC 
     */
    uchar at_zone;

    /* time of day the rule takes effect, as milliseconds after midnight */
    int32_t at;

    /* resolve this rule in the given year */
    void resolve(int32_t &dayno, int32_t &daytime, int year,
                 const vmtz_rule *prev_rule);

    /* set from an OS DST start/end date descriptor */
    void set(const os_tzrule_t *desc, const char *abbr,
             int32_t gmtofs, int32_t save);
};


/* ------------------------------------------------------------------------ */
/*
 *   Time zone info query structure
 */
struct vmtzquery
{
    /* starting date/time of the transition or rule that applies */
    int32_t start_dayno;
    int32_t start_daytime;

    /* GMT offset for standard time, in milliseconds */
    int32_t gmtofs;

    /* daylight savings offset from standard time, in milliseconds */
    int32_t save;

    /* zone name abbreviation ("EST", etc) */
    const char *abbr;

    /* set from a transition item */
    void set(const vmtz_trans *trans)
        { set(trans->ttype, trans->dayno, trans->daytime); }

    /* set from a time type */
    void set(const vmtz_ttype *tty, int32_t dayno, int32_t daytime)
    {
        start_dayno = dayno;
        start_daytime = daytime;
        gmtofs = tty->gmtofs;
        save = tty->save;
        abbr = tty->fmt;
    }

    /* set from a rule */
    void set(const vmtz_rule *rule, int32_t dayno, int32_t daytime)
    {
        start_dayno = dayno;
        start_daytime = daytime;
        gmtofs = rule->gmtofs;
        save = rule->save;
        abbr = rule->fmt;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Time zone object.
 */
class CVmTimeZone
{
public:
    /* create from a timezone descriptor */
    CVmTimeZone(const os_tzinfo_t *desc);

    /* destruction */
    ~CVmTimeZone();

    /* load from the zoneinfo database file */
    static CVmTimeZone *load(VMG_ class ZoneHashEntry *entry);

    /* get the primary name of the zone */
    const char *get_name(size_t &len) const;

    /* create a List object with this zone's names and aliases */
    vm_obj_id_t get_name_list(VMG0_) const;

    /* get the single history item that applies to the given date */
    vm_obj_id_t get_history_item(VMG_ int32_t dayno, int32_t daytime) const;

    /* create a List object populated with the transition list */
    vm_obj_id_t get_history_list(VMG0_) const;

    /* create a List object populated with the ongoing rules */
    vm_obj_id_t get_rule_list(VMG0_) const;

    /* country code, if available (ISO3166 two-letter country code) */
    const char *country() const { return country_; }

    /* coordinates, if available (+DDMM+DDDMM or +DDMMSS+DDDMMSS format) */
    const char *coords() const { return coords_; }

    /* zone.tab description/comment */
    const char *desc() const { return desc_; }

    /* 
     *   Convert a local wall clock time in this zone to UTC.  'daytime' is
     *   in milliseconds after midnight.
     */
    void local_to_utc(int32_t &dayno, int32_t &daytime);

    /* 
     *   Convert a UTC time to local time in this zone.  Returns the format
     *   abbreviation for the converted time (e.g., PDT for Pacific Daylight
     *   Time).  'daytime' is in milliseconds after midnight.  Fills in
     *   'tzofs' with the time zone's offset from UTC, in milliseconds.
     */
    const char *utc_to_local(int32_t &dayno, int32_t &daytime, int32_t &tzofs);

    /* 
     *   Query the time zone information at the given time/right now.  The
     *   time is in milliseconds after midnight. 
     */
    void query(vmtzquery *result,
               int32_t dayno, int32_t daytime, int local) const;
    void query(vmtzquery *result) const;

protected:
    /* create from binary data loaded from the file */
    CVmTimeZone(class ZoneHashEntryDb *entry, const char *file_data,
                unsigned int trans_cnt, unsigned int type_cnt,
                unsigned int rule_cnt, unsigned int abbr_bytes);

    /* create a synthetic zone */
    CVmTimeZone(class ZoneHashEntrySynth *entry);

    /* initialize from an OS timezone descriptor */
    void init(const os_tzinfo_t *desc);

    /* my hash table entry */
    class ZoneHashEntry *hashentry_;

    /* my name; used only if we don't have a hash entry */
    char *name_;

    /* transition list */
    int trans_cnt_;
    vmtz_trans *trans_;

    /* time type list */
    int ttype_cnt_;
    vmtz_ttype *ttype_;

    /* rule list */
    int rule_cnt_;
    vmtz_rule *rule_;

    /* abbreviations */
    char *abbr_;

    /* country code */
    char country_[3];

    /* coordinates */
    char coords_[16];

    /* comments/description from zone.tab */
    char *desc_;
};

#endif
