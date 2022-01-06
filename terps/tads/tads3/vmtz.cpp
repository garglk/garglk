/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtz.cpp - TADS time zone management
Function
  Implements the time zone cache manager and time zone objects.
Notes
  None
Modified
  02/05/12 MJRoberts  - creation
*/

#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include "t3std.h"
#include "osstzprs.h"
#include "vmtz.h"
#include "vmhash.h"
#include "vmglob.h"
#include "resload.h"
#include "vmhost.h"
#include "vmdate.h"
#include "vmlst.h"
#include "vmstr.h"
#include "vmtzobj.h"


/* ------------------------------------------------------------------------ */
/*
 *   zone table hash entry 
 */

/* hash table entry types */
#define ZONE_HASH_DB     1               /* defined in the zoneinfo db file */
#define ZONE_HASH_LINK   2                  /* link (alias) to another zone */
#define ZONE_HASH_SYNTH  3            /* synthesized from an on os_tzinfo_t */

class ZoneHashEntry: public CVmHashEntryCI
{
public:
    ZoneHashEntry(const char *str, size_t len, int copy, int typ)
        : CVmHashEntryCI(str, len, copy)
    {
        this->tz = 0;
        this->typ = typ;
        this->nxt = 0;
        this->link_from = 0;
    }

    virtual ~ZoneHashEntry()
    {
        /* if we loaded a timezone object, delete it */
        if (tz != 0)
            delete tz;
    }

    /* resolve links - by default, we don't have any links */
    virtual ZoneHashEntry *resolve_links() { return this; }

    /* get my country code, if I have one */
    virtual const char *get_country() { return 0; }

    /* match my country code to that of another zone */
    int match_country(const char *country)
    {
        const char *this_country = get_country();
        return (this_country != 0 && country[0] != '\0'
                && memcmp(this_country, country, 2) == 0);
    }

    /* entry type (ZONE_HASH_xxx) */
    int typ;

    /* the cached zone object, if loaded */
    CVmTimeZone *tz;

    /* the head of the list of zones that link to us */
    class ZoneHashEntryLink *link_from;

    /*
     *   In addition to the hash table, we keep a simple linked list of all
     *   of the loaded zone objects.  This lets us do iteraitons over the
     *   loaded list more easily (and quickly) than using the hash table. 
     */
    ZoneHashEntry *nxt;
};

class ZoneHashEntryDb: public ZoneHashEntry
{
public:
    ZoneHashEntryDb(const char *str, size_t len, int copy,
                    uint32_t seekpos, const char *country)
        : ZoneHashEntry(str, len, copy, ZONE_HASH_DB), seekpos(seekpos)
    {
        memcpy(this->country, country, 2);
    }

    /* seek position in the database file */
    uint32_t seekpos;

    /* country code */
    virtual const char *get_country() { return country; }
    char country[2];
};

class ZoneHashEntryLink: public ZoneHashEntry
{
public:
    ZoneHashEntryLink(const char *str, size_t len, int copy,
                      ZoneHashEntry *link_to)
        : ZoneHashEntry(str, len, copy, ZONE_HASH_LINK)
    {
        /* add myself to the target's 'from' list */
        this->link_to = link_to->resolve_links();
        link_nxt = link_to->link_from;
        this->link_to->link_from = this;
    }

    /* resolve links - get the target hash entry for a linked hash entry */
    ZoneHashEntry *resolve_links() { return link_to->resolve_links(); }

    /* get the country from my resolved link */
    const char *get_country() { return resolve_links()->get_country(); }

    /* the zone we link to */
    ZoneHashEntry *link_to;

    /* the next link that links to our link_to */
    ZoneHashEntryLink *link_nxt;
};

class ZoneHashEntrySynth: public ZoneHashEntry
{
public:
    ZoneHashEntrySynth(const char *str, size_t len, int copy,
                       const os_tzinfo_t *desc)
        : ZoneHashEntry(str, len, copy, ZONE_HASH_SYNTH)
    {
        /* copy the description */
        memcpy(&synth, desc, sizeof(synth));
    }

    /* synthetic zone description */
    os_tzinfo_t synth;
};


/* ------------------------------------------------------------------------ */
/* 
 *   zone search spec 
 */
struct ZoneSearchSpec
{
    ZoneSearchSpec() { abbr[0] = '\0'; gmtofs = INT32MAXVAL; dst = 'S'; }
    ZoneSearchSpec(const char *abbr, int32_t gmtofs, char dst)
        : gmtofs(gmtofs), dst(dst)
    {
        set_abbr(abbr);
    }

    void set(const char *abbr, int32_t gmtofs, char dst)
    {
        set_abbr(abbr);
        this->gmtofs = gmtofs;
        this->dst = dst;
    }
    void set(const char *abbr, size_t abbrlen, int32_t gmtofs, char dst)
    {
        set_abbr(abbr, abbrlen);
        this->gmtofs = gmtofs;
        this->dst = dst;
    }

    /* abbreviation string */
    char abbr[16];
    void set_abbr(const char *str, size_t len)
        { lib_strcpy(abbr, sizeof(abbr), str, len); }
    void set_abbr(const char *str)
        { set_abbr(str, str != 0 ? strlen(str) : 0); }

    /* GMT offset in milliseconds */
    int32_t gmtofs;

    /* 'D' for daylight time, 'S' for standard time */
    char dst;
};

/* ------------------------------------------------------------------------ */
/*
 *   Abbreviation hash entry.  This records a mapping from a zone name
 *   abbreviation (e.g., "EST") to a list of associated zone objects.
 */

/* individual mapping from an abbreviation to a zone */
struct AbbrToZone
{
    /* the zone we link to */
    ZoneHashEntry *zone;

    /* the GMT offset for this abbreviation in this zone, in milliseconds */
    int32_t gmtofs;

    /* 
     *   'D' if this is daylight savings time, 'S' for standard time, 'B' if
     *   it the abbreviation can refer to both types (Australian zones work
     *   this way: EST is Australian Eastern Standard Time AND Eastern Summer
     *   Time).  When we have a 'B' zone, we'll also have the regular 'D' and
     *   'S' entries for it, so that searches for the specific types will
     *   succeed; the 'B' entry is for get_zone_by_abbr(), to let it know
     *   that the zone isn't to be treated as a single fixed offset the way,
     *   say, PDT would be.
     */
    char dst;
};

/* hash entry */
class AbbrHashEntry: public CVmHashEntryCI
{
public:
    AbbrHashEntry(const char *str, size_t len, int copy, int entry_cnt)
        : CVmHashEntryCI(str, len, copy)
    {
        this->entry_cnt = entry_cnt;
        this->entries = new AbbrToZone[entry_cnt];
    }

    ~AbbrHashEntry()
    {
        delete [] entries;
    }

    void set_zone(int i, ZoneHashEntry *zone, int32_t gmtofs, char dst)
    {
        entries[i].zone = zone;
        entries[i].gmtofs = gmtofs;
        entries[i].dst = dst;
    }

    /* search for a match to a search spec */
    ZoneHashEntry *search(int start_idx, const ZoneSearchSpec *spec)
    {
        for (int i = start_idx ; i < entry_cnt ; ++i)
        {
            if (entries[i].gmtofs == spec->gmtofs
                && entries[i].dst == spec->dst)
                return entries[i].zone;
        }

        return 0;
    }

    /* search for a match to a search spec and a zone object */
    ZoneHashEntry *search(int start_idx, const ZoneSearchSpec *spec,
                          const ZoneHashEntry *zone)
    {
        for (int i = start_idx ; i < entry_cnt ; ++i)
        {
            if (entries[i].zone == zone
                && entries[i].gmtofs == spec->gmtofs
                && entries[i].dst == spec->dst)
                return entries[i].zone;
        }

        return 0;
    }
    
    int entry_cnt;
    AbbrToZone *entries;
};


/* ------------------------------------------------------------------------ */
/*
 *   Time zone cache manager 
 */

/*
 *   construction 
 */
CVmTimeZoneCache::CVmTimeZoneCache()
{
    /* create the zone tables */
    db_tab_ = new CVmHashTable(64, new CVmHashFuncCI(), TRUE);
    synth_tab_ = new CVmHashTable(64, new CVmHashFuncCI(), TRUE);

    /* we haven't loaded our zone database yet */
    index_loaded_ = index_loaded_ok_ = FALSE;
    zone_bytes_ = 0;
    link_bytes_ = 0;
    abbr_tab_ = 0;
    abbr_bytes_ = 0;
    first_loaded_ = 0;
    local_zone_ = 0;
}

/*
 *   destruction 
 */
CVmTimeZoneCache::~CVmTimeZoneCache()
{
    /* delete our zone table, zone index, and abbreviation table */
    if (db_tab_ != 0)
        delete db_tab_;
    if (synth_tab_ != 0)
        delete synth_tab_;
    if (abbr_tab_ != 0)
        delete abbr_tab_;

    /* if we have a local zone object, delete it */
    if (local_zone_ != 0)
        delete local_zone_;

    /* if we loaded the file data sections, delete them */
    if (zone_bytes_ != 0)
        delete [] zone_bytes_;
    if (link_bytes_ != 0)
        delete [] link_bytes_;
    if (abbr_bytes_ != 0)
        delete [] abbr_bytes_;
}

/* ------------------------------------------------------------------------ */
/*
 *   Translate a ZoneHashEntry to a CVmTimeZone object 
 */
CVmTimeZone *CVmTimeZoneCache::tz_from_hash(VMG_ ZoneHashEntry *entry)
{
    /* if there's no table entry, there's nothing to load */
    if (entry == 0)
        return 0;

    /* resolve links */
    entry = entry->resolve_links();

    /* if we haven't loaded the timezone object yet, do so now */
    if (entry->tz == 0)
    {
        /* load it */
        entry->tz = CVmTimeZone::load(vmg_ entry);

        /* if successful, add it to the loaded zone list */
        if (entry->tz != 0)
        {
            entry->nxt = first_loaded_;
            first_loaded_ = entry;
        }
    }

    /* return the cached object */
    return entry->tz;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a zone name in various formats 
 */
CVmTimeZone *CVmTimeZoneCache::parse_zone(VMG_ const char *name, size_t len)
{
    /* treat a null, empty string, or ":local" as the local zone */
    if (name == 0 || len == 0
        || (len == 6 && memcmp(name, ":local", 6) == 0))
        return get_local_zone(vmg0_);

    /* check for special zones: "Z", "UTC", "GMT" all mean UTC+0 */
    if (lib_stricmp(name, len, "z") == 0
        || lib_stricmp(name, len, "utc") == 0
        || lib_stricmp(name, len, "gmt") == 0)
        return get_gmtofs_zone(vmg_ 0);

    /* try +8, -830, +8:30, -8:00:00 */
    CVmTimeZone *tz;
    if ((tz = parse_zone_hhmmss(vmg_ "", name, len)) != 0)
        return tz;
    
    /* try UTC+8, UTC-830, UTC+8:30, UTC-8:00:00 */
    if ((tz = parse_zone_hhmmss(vmg_ "utc", name, len)) != 0)
        return tz;

    /* try parsing as a POSIX-style EST5EDT string */
    if ((tz = parse_zone_posixTZ(vmg_ name, len)) != 0)
        return tz;
    
    /* it's not one of our special formats; look it up in the database */
    if ((tz = G_tzcache->get_db_zone(vmg_ name, len)) != 0)
        return tz;

    /* return failure */
    return 0;
}

/*
 *   Parse a time zone spec in simplified POSIX TZ notation: EST-5, EST-5EDT,
 *   EST-5EDT-4.
 */
CVmTimeZone *CVmTimeZoneCache::parse_zone_posixTZ(
    VMG_ const char *name, size_t len)
{
    /* 
     *   Run it through the POSIX TZ parser.  Note that the zoneinfo format
     *   uses negative offsets going west (e.g., EST-5), in contrast to the
     *   original TZ format. 
     */
    os_tzinfo_t desc;
    if (oss_parse_posix_tz(&desc, name, len, FALSE))
    {
        /* got it - check to see if it's already defined */
        ZoneHashEntry *entry = (ZoneHashEntry *)synth_tab_->find(name, len);
        if (entry == 0)
        {
            /* it's not there yet - create a new entry for it */
            entry = new ZoneHashEntrySynth(name, len, TRUE, &desc);
            synth_tab_->add(entry);
        }

        /* find or create its timezone object */
        return tz_from_hash(vmg_ entry);
    }

    /* it's not ours */
    return 0;
}

/* 
 *   get a zone specified in a +hh, +hmm, +hhmm, +hh:mm or +hh:mm:ss format 
 */
CVmTimeZone *CVmTimeZoneCache::parse_zone_hhmmss(
    VMG_ const char *prefix, const char *name, size_t len)
{
    /* check for the prefix - if it doesn't match, return failure */
    size_t plen = strlen(prefix);
    if (plen != 0 && (len < plen || memicmp(name, prefix, plen) != 0))
        return 0;

    /* skip the prefix */
    name += plen;
    len -= plen;

    /* parse the offset */
    int32_t ofs;
    if (!parse_hhmmss(ofs, name, len, TRUE) || len != 0)
        return 0;

    /* convert the hh:mm:ss value to seconds and look up the zone */
    return get_gmtofs_zone(vmg_ ofs);
}

/*
 *   parse a [+-]h[:mm[:ss]] time/offset value 
 */
int CVmTimeZoneCache::parse_hhmmss(
    int32_t &ofs, const char *&name, size_t &len, int sign_required)
{
    /* check for the sign */
    int s = 1;
    if (len > 0 && name[0] == '+')
        ++name, --len;
    else if (len > 0 && name[0] == '-')
        ++name, --len, s = -1;
    else if (sign_required)
        return FALSE;

    /* parse the hours */
    const char *hhp = name;
    int hh = lib_atoi_adv(name, len);

    /* if we had no digits, or more than four in a row, fail */
    size_t hhlen = name - hhp;
    if (hhlen < 1 || hhlen > 4)
        return FALSE;

    /* if we had three or more consecutive digits, it's hmm or hhmm format */
    int mm = 0, ss = 0;
    if (hhlen >= 3)
    {
        /* the hour is actually hours/minutes - divide out the components */
        mm = hh % 100;
        hh /= 100;
    }
    else
    {
        /* not hmm/hhmm format, so it's hh[:mm[:ss]] - check for minutes */
        if (len >= 3 && *name == ':' && isdigit(name[1]) && isdigit(name[2]))
        {
            /* parse the minutes */
            ++name, --len;
            mm = lib_atoi_adv(name, len);
            
            /* check for seconds */
            if (len >= 3 && *name == ':'
                && isdigit(name[1]) && isdigit(name[2]))
            {
                ++name, --len;
                ss = lib_atoi_adv(name, len);
            }
        }
    }

    /* success - compute the offset value in seconds */
    ofs = s*(hh*60*60 + mm*60 + ss);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up or load a time zone from the database
 */
CVmTimeZone *CVmTimeZoneCache::get_db_zone(VMG_ const char *name, size_t len)
{
    /* if we haven't loaded the zone index data yet, do so now */
    load_db_index(vmg0_);

    /* look up the hash table entry */
    return tz_from_hash(vmg_ (ZoneHashEntry *)db_tab_->find(name, len));
}

/*
 *   Generate an offset string in the minimal format: h, h:mm, h:mm:ss.
 */
#define F_HIDE_ZERO  0x0001                       /* suppress a zero offset */
#define F_PLUS_SIGN  0x0002         /* use a plus sign for positive offsets */
static void gen_ofs_string(char *buf, size_t buflen,
                           const char *prefix, int32_t ofs, int flags)
{
    /* figure the sign */
    const char *signch = "";
    if (ofs < 0)
        ofs = -ofs, signch = "-";
    else if ((flags & F_PLUS_SIGN) != 0)
        signch = "+";

    /* decompose the offset into hours, minutes, and seconds */
    int hh = ofs / (60*60);
    int mm = ofs/60 % 60;
    int ss = ofs % 60;

    /* 
     *   figure the format: prefix, sign character, then h:mm:ss if we have
     *   non-zero seconds, h:mm if we have non-zero minutes but no seconds,
     *   or just the hours if the minutes and seconds are both zero 
     */ 
    const char *fmt = (ss != 0 ? "%s%s%d:%02d:%02d" :
                       mm != 0 ? "%s%s%d:%02d" :
                       hh != 0 || (flags & F_HIDE_ZERO) == 0 ? "%s%s%d" :
                       "%s");

    /* generate the string */
    t3sprintf(buf, buflen, fmt, prefix, signch, hh, mm, ss);
}

/*
 *   Look up or load a time zone for a GMT offset, in seconds
 */
CVmTimeZone *CVmTimeZoneCache::get_gmtofs_zone(VMG_ int32_t gmtofs_secs)
{
    /* build our special name string */
    char name[50];
    gen_ofs_string(name, sizeof(name), "UTC", gmtofs_secs,
                   F_HIDE_ZERO | F_PLUS_SIGN);

    /* if we already have an entry cached, return it */
    ZoneHashEntry *entry =
        (ZoneHashEntry *)synth_tab_->find(name, strlen(name));
    if (entry != 0)
        return tz_from_hash(vmg_ entry);

    /* create a new synthetic entry for the zone */
    os_tzinfo_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.std_ofs = gmtofs_secs;
    lib_strcpy(desc.std_abbr, sizeof(desc.std_abbr), name);
    entry = new ZoneHashEntrySynth(name, strlen(name), TRUE, &desc);

    /* add it to the hash table */
    synth_tab_->add(entry);

    /* translate it to a CVmTimeZone object */
    return tz_from_hash(vmg_ entry);
}

/*
 *   Look up a zone by abbreviation.
 *   
 *   Zone abbreviations are inherently ambiguous, and there's no reliable way
 *   to figure out which zone a user might mean.  The ideal solution would be
 *   for users to stop using zone abbreviations entirely and switch to
 *   zoneinfo location keys instead.  But that's a tough sell, especially
 *   since most users are blissfully unaware that the problem even exists -
 *   who in the US knows that "CST" is also the name for time zones in Cuba,
 *   China, and Australia?  It's unreasonable not to expect users to plug in
 *   abbreviations when entering date strings - especially given that the
 *   Date parser is otherwise very liberal about accepting free-form,
 *   colloquial formats.  So we feel it's best to give it the college try at
 *   understanding zone abbreviations, but recognizing that it's ultimately a
 *   "best effort" sort of task, in that it's not something we can cleanly
 *   and definitively solve.
 *   
 *   We fill in *gmtofs_ms with the GMT offset implied by the zone
 *   abbreviation.  If this is one of the funny Australian zones that uses
 *   the same abbreviation for both standard and daylight time, we return
 *   INT32MINVAL in *gmtofs_ms instead, to indicate that the abbreviation
 *   doesn't specify an offset by itself, but only specifies the zone; the
 *   offset in this case must be looked up via a date-sensitive query as
 *   though the zone had been specified by zoneinfo name instead of by
 *   abbreviation.
 */
CVmTimeZone *CVmTimeZoneCache::get_zone_by_abbr(
    VMG_ int32_t *gmtofs_ms, const char *name, size_t len)
{
    vmtzquery lresult;

    /* if we haven't loaded the zone index data yet, do so now */
    load_db_index(vmg0_);

    /* make sure we have an abbreviation table */
    if (abbr_tab_ == 0)
        return 0;

    /* look up the abbrevation */
    AbbrHashEntry *ah = (AbbrHashEntry *)abbr_tab_->find(name, len);

    /* if there's no table entry, or it's empty, there's nothing to find */
    if (ah == 0 || ah->entry_cnt == 0)
        return 0;

    /* get the local zone information */
    CVmTimeZone *ltz = get_local_zone(vmg0_);
    const char *country = ltz->country();

    /* we don't have our selection yet */
    AbbrToZone *atoz = 0, *ep;

    /*
     *   First, look to see if this is an abbreviation used in our system's
     *   local time zone.  If so use that, since the most likely case is that
     *   the user is referring to her own local time.
     */
    int i;
    for (i = 0, ep = ah->entries ; i < ah->entry_cnt ; ++i, ++ep)
    {
        if (ep->zone->tz == ltz)
        {
            atoz = ep;
            goto done;
        }
    }

    /*
     *   If that didn't work, look for a zone in the same country as the
     *   local time zone.  This seems like the best bet for matching the
     *   user's expectations.
     */
    for (i = 0, ep = ah->entries ; i < ah->entry_cnt ; ++i, ++ep)
    {
        if (ep->zone->match_country(country))
        {
            atoz = ep;
            goto done;
        }
    }

    /*
     *   If that failed, look for something within +- 3 hours of the local
     *   zone.  That will at least get something in the same part of the
     *   world, and probably in the same country or a nearby country.  Most
     *   zones that use the same abbreviation probably aren't right next door
     *   to each other, either, so this should be pretty selective.  (There
     *   are exceptions.  CST in North America is Central Standard Time at
     *   -6, but it's also Cuba Standard Time at -5.  Hopefully most nearby
     *   zone authorities aren't as mutually antagonistic as the US and Cuba
     *   and do a better job of coordinating their zone naming.)
     */
    ltz->query(&lresult);
    for (i = 0, ep = ah->entries ; i < ah->entry_cnt ; ++i, ++ep)
    {
        if (abs(lresult.gmtofs - ep->gmtofs) < 3*60*60*1000)
        {
            atoz = ep;
            goto done;
        }
    }

    /* 
     *   If all else fails, match the first zone in the list.  Our zoneinfo
     *   compiler places a specially hand-chosen primary zone for each
     *   abbreviation at the head of the abbreviation's zone list.  The hand
     *   picks favor US and European zones, so this isn't the ideal way to
     *   handle this globally, but in the absence of any other selection
     *   criteria there's not much else we can do. 
     */
    atoz = &ah->entries[0];

done:
    /* 
     *   Pass the abbreviation's offset back to the caller.  Most zone
     *   abbreviations refer to a fixed offset; e.g., PST is always -8, even
     *   in the summer when the US Pacific Time zone is observing daylight
     *   time for wall clock time.  Writing "PST" explicitly means that we're
     *   talking about the -8 setting, rather than the current wall clock
     *   setting.
     *   
     *   However, some zones (all in Australia, in the current zoneinfo
     *   database) use a single abbreviation for both standard and daylight
     *   time, so these zones *don't* carry the special fixed offset
     *   information with the abbreviation.  For example, Australian EST is
     *   Eastern Standard Time in the winter and Eastern Summer Time in the
     *   summer - same abbreviation, two different clock settings.  So if we
     *   determine that we're talking about Australian EST, we need to
     *   determine the offset from the time of year, as we would for a spec
     *   like America/Los_Angeles (as opposed to PST).  Abbreviations with
     *   multiple offsets in one zone are marked with 'B' as in the DST
     *   field.
     */
    *gmtofs_ms = (atoz->dst != 'B' ? atoz->gmtofs : INT32MINVAL);

    /* return the timezone object */
    return tz_from_hash(vmg_ atoz->zone);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the local system time zone.
 *   
 *   The local time zone is special, in that it's always represented as a
 *   distinct CVmTimeZone object (and a distinct entry in our hash table)
 *   from the object for the zone it represents.  This allows callers to
 *   distinguish a date that's implicitly in the local zone from one that's
 *   explicitly in the zone that happens to be the local zone.  For example,
 *   in the CVmObjDate package, a date specified as "1 pm" is implicitly in
 *   the local time zone, whereas a date specified as "1 pm PST" is
 *   explicitly in Pacific Standard Time.  If the local zone happens to be
 *   PST, both dates will be in PST now; but if the objects are saved and
 *   later restored on another machine that's in New York, the first date ("1
 *   pm") will be rendered in EST, whereas the second ("1 pm PST") will
 *   continue to be rendered in PST.  To make this possible, we have to know
 *   that the first date's CVmTimeZone object is the special "local" object,
 *   separate from the second date's CVmTimeZone object - even though both
 *   represent the same underlying database entry.
 *   
 *   To implement this, if we resolve the local zone to a database entry, we
 *   load a separate copy of the database entry so that the local zone is a
 *   distinct object.
 */
CVmTimeZone *CVmTimeZoneCache::get_local_zone(VMG0_)
{
    /* if we have a cached local zone, return it */
    if (local_zone_ != 0)
        return local_zone_;

    /* we haven't found our zone entry yet */
    ZoneHashEntry *entry = 0;

    /* see if we can get a zoneinfo database key from the operating system */
    char name[128];
    if (os_get_zoneinfo_key(name, sizeof(name))
        && load_db_index(vmg0_)
        && (entry = (ZoneHashEntry *)db_tab_->find(name, strlen(name))) != 0)
    {
        /* we found a database entry - load a separate copy */
        local_zone_ = CVmTimeZone::load(vmg_ entry);
    }

    /* if we didn't find it, try the lower-level timezone description */
    os_tzinfo_t info;
    if (local_zone_ == 0 && os_get_timezone_info(&info))
    {
        /*
         *   If they were able to give us separate standard and daylight
         *   offsets and abbreviations, search for the pair.  Otherwise
         *   search for the current one.
         *   
         *   Note that the os_tzinfo_t specifies offsets in seconds, whereas
         *   the ZoneSearchSpec uses milliseconds, so multiply accordingly.
         */
        if (info.std_ofs != info.dst_ofs
            && info.std_abbr[0] != '\0' && info.dst_abbr[0] != '\0')
        {
            ZoneSearchSpec spec[2];
            spec[0].set(info.std_abbr, info.std_ofs*1000, 'S');
            spec[1].set(info.dst_abbr, info.dst_ofs*1000, 'D');
            entry = search(vmg_ spec, 2);
        }
        else
        {
            /* 
             *   they couldn't distinguish standard and daylight, so just use
             *   the one that's currently in effect 
             */
            ZoneSearchSpec spec;
            if (info.is_dst)
                spec.set(info.dst_abbr, info.dst_ofs*1000, 'D');
            else
                spec.set(info.std_abbr, info.std_ofs*1000, 'S');
            entry = search(vmg_ &spec, 1);
        }
        
        /* check to see if we found a matching entry in the database */
        if (entry != 0)
        {
            /* we found an entry - load a separate copy */
            local_zone_ = CVmTimeZone::load(vmg_ entry);
        }
        else
        {
            /* 
             *   we didn't find an entry, so synthesize a new one based on
             *   the zone description we got from the OS 
             */
            local_zone_ = new CVmTimeZone(&info);
        }
    }
    
    /*   
     *   If we still don't have anything, try the the standard C++ library's
     *   time functions.  These won't give us as much information as we were
     *   hoping to get from the OS, and probably won't work at all given that
     *   the OS routines probably went to the same primary sources that the
     *   CRTL will look at, but it's worth trying as a last resort.
     */
    if (local_zone_ == 0)
    {
        /* strftime(%Z) format gives us the local time zone */
        os_time_t t = os_time(0);
        struct tm *tm = os_localtime(&t);
        strftime(name, sizeof(name), "%Z", tm);
        
        /* if we got a zone, look it up */
        if (name[0] != '\0')
        {
            /* 
             *   Get the current minutes in the day in local time, and the
             *   local time day number.  For the day number, use the year
             *   times 366 plus the day of the year; this doesn't correspond
             *   to a serial day number in our caldate_t system or really any
             *   other system, but it does produce monotonic values, which is
             *   all we need: our only use for this day number if to compare
             *   the UTC and local day numbers to see if the respective
             *   clocks are on different days, and if so in which direction. 
             */
            int ss_local = tm->tm_hour*60*60 + tm->tm_min*60 + tm->tm_sec;
            long dayno_local = tm->tm_year*366L + tm->tm_yday;
            char dst = tm->tm_isdst ? 'D' : 'S';
            
            /* repeat the exercise in local time */
            tm = os_gmtime(&t);
            int ss_utc = tm->tm_hour*60*60 + tm->tm_min*60 + tm->tm_sec;
            long dayno_utc = tm->tm_year*366L + tm->tm_yday;
            
            /* if the clocks are on different days, adjust accordingly */
            if (dayno_utc > dayno_local)
                ss_utc += 24*60*60;
            else if (dayno_utc < dayno_local)
                ss_utc -= 24*60*60;
            
            /* 
             *   Search for the zone.  For the GMT offset, figure the
             *   difference in milliseconds between local and UTC.
             */
            ZoneSearchSpec spec(name, (ss_local - ss_utc)*1000, dst);
            if ((entry = search(vmg_ &spec, 1)) != 0)
            {
                /* we found a database entry - load a private copy */
                local_zone_ = CVmTimeZone::load(vmg_ entry);
            }
            else
            {
                /* 
                 *   no database match; synthesize an entry based on the time
                 *   zone info we got from the C time functions
                 */
                os_tzinfo_t desc;
                memset(&desc, 0, sizeof(desc));
                desc.std_ofs = desc.dst_ofs = ss_local - ss_utc;
                lib_strcpy(desc.std_abbr, sizeof(desc.std_abbr), name);
                lib_strcpy(desc.dst_abbr, sizeof(desc.dst_abbr), name);
                desc.is_dst = tm->tm_isdst;
                local_zone_ = new CVmTimeZone(&desc);
            }
        }
    }
    
    /* if after all that we still have nothing, use UTC */
    if (local_zone_ == 0)
    {
        /* try looking up the basic UTC zone */
        os_tzinfo_t desc;
        memset(&desc, 0, sizeof(desc));
        strcpy(desc.std_abbr, "UTC");
        local_zone_ = new CVmTimeZone(&desc);
    }

    /* return the local zone we came up with */
    return local_zone_;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a missing database entry.  This represents a database zone that's
 *   not defined in the version of the database we're using, but was imported
 *   from a saved game created with another database version.
 */
CVmTimeZone *CVmTimeZoneCache::create_missing_zone(
    VMG_ const char *name, size_t namelen, const os_tzinfo_t *desc)
{
    /* create the entry */
    ZoneHashEntry *e = new ZoneHashEntrySynth(name, namelen, TRUE, desc);

    /* add it to our database table */
    db_tab_->add(e);

    /* return the new entry */
    return tz_from_hash(vmg_ e);
}

/* ------------------------------------------------------------------------ */
/* 
 *   search for a zone that matches all of the given search specs 
 */
ZoneHashEntry *CVmTimeZoneCache::search(
    VMG_ const ZoneSearchSpec *specs, int cnt)
{
    /* if there are no abbreviations, fail */
    if (!load_db_index(vmg0_) || abbr_tab_ == 0)
        return 0;

    /* if we're searching just one list, this is easy... */
    if (cnt == 1)
    {
        /* look up this abbreviation */
        AbbrHashEntry *e = (AbbrHashEntry *)abbr_tab_->find(
            specs[0].abbr, strlen(specs[0].abbr));
        if (e == 0)
            return 0;

        /* return the first entry that matches the spec */
        return e->search(0, specs);
    }
    else if (cnt == 2)
    {
        /* look up the two abbreviations */
        AbbrHashEntry *e0 = (AbbrHashEntry *)abbr_tab_->find(
            specs[0].abbr, strlen(specs[0].abbr));
        AbbrHashEntry *e1 = (AbbrHashEntry *)abbr_tab_->find(
            specs[1].abbr, strlen(specs[1].abbr));

        /* if either abbreviation is missing, fail */
        if (e0 == 0 || e0->entry_cnt == 0 || e1 == 0 || e1->entry_cnt == 0)
            return 0;

        /* find the earliest intersection */
        for (int i = 0, j = 0 ; i < e0->entry_cnt && j < e1->entry_cnt ;
             ++i, ++j)
        {
            /* find the next first-list element matching the first spec */
            ZoneHashEntry *z0 = e0->search(i, &specs[0]);
            if (z0 == 0)
                return 0;

            /* 
             *   look for a second-list element that matches the second spec
             *   AND the first-list zone we just matched 
             */
            ZoneHashEntry *z1 = e1->search(j, &specs[1], z0);
            if (z1 != 0)
                return z1;

            /* 
             *   no luck; find the next second-list element matching just the
             *   second spec, then find a first-list item matching z1 and the
             *   first spec
             */
            z1 = e1->search(j, &specs[1]);
            if ((z0 = e0->search(i+1, &specs[0], z1)) != 0)
                return z0;
        }

        /* didn't find it */
        return 0;
    }

    /* 
     *   we're really only used for STD+DST searches; if we're called upon
     *   for anything else, it's unexpected 
     */
    return 0;
}

    

/* ------------------------------------------------------------------------ */
/*
 *   Load the zoneinfo database index.  Returns true on success, false on
 *   failure.
 */
int CVmTimeZoneCache::load_db_index(VMG0_)
{
    /* if we've already loaded the index (or tried to), we're done */
    if (index_loaded_)
        return index_loaded_ok_;

    /* we've now tried loading the index */
    index_loaded_ = TRUE;

    /* open the zone file */
    osfildef *fp = open_zoneinfo_file(vmg0_);
    if (fp == 0)
        return FALSE;

    /* read the signature */
    char buf[24];
    int ok = !osfrb(fp, buf, 24) && memcmp(buf, "T3TZ", 4) == 0;

    /* if the signature checked out, read the zone table */
    uint32_t zone_table_len = 0, zone_cnt = 0;
    if (ok)
    {
        /* decode the zone name table length */
        zone_table_len = osrp4(buf + 16) - 4;
        zone_cnt = osrp4(buf + 20);
        
        /* allocate space to load the table */
        zone_bytes_ = new char[zone_table_len];

        /* read the table */
        ok = zone_bytes_ != 0 && !osfrb(fp, zone_bytes_, zone_table_len);
    }

    /* if everything is working so far, read the link table */
    uint32_t link_table_len = 0;
    if (ok && !osfrb(fp, buf, 8))
    {
        /* load the link table */
        link_table_len = osrp4(buf) - 4;
        /* uint32_t link_cnt = osrp4(buf+4) - not currently needed */
        link_bytes_ = new char[link_table_len];
        ok = link_bytes_ != 0 && !osfrb(fp, link_bytes_, link_table_len);
    }

    /* if we're still good, read the abbreviation mappings */
    uint32_t abbr_table_len = 0;
    if (ok && !osfrb(fp, buf, 8))
    {
        /* load the abbreviation table */
        abbr_table_len = osrp4(buf) - 4;
        /* uint32_t abbr_cnt = osrp4(buf + 4) - not currently needed */
        abbr_bytes_ = new char[abbr_table_len];
        ok = abbr_bytes_ != 0 && !osfrb(fp, abbr_bytes_, abbr_table_len);
    }

    /* we're done with the file */
    osfcls(fp);

    /* if we read the zone table successfully, create a hash table from it */
    ZoneHashEntry **zone_index = 0;
    if (ok)
    {
        /* create the numerical index */
        zone_index = new ZoneHashEntry *[zone_cnt];

        /* populate it */
        const char *zone_end = zone_bytes_ + zone_table_len;
        int zi = 0;
        for (const char *p = zone_bytes_ ; p < zone_end ; )
        {
            /* read and skip the name length prefix */
            size_t len = osrp1(p);
            ++p;

            /* create this entry */
            ZoneHashEntry *e = new ZoneHashEntryDb(
                p, len, FALSE, osrp4(p+len+1), p+len+1+4);

            /* add it to the hash table and zone index */
            db_tab_->add(e);
            zone_index[zi++] = e;

            /* skip the name, seek pointer, and country code */
            p += len + 1 + 4 + 2;
        }

        /* read the links */
        const char *link_end = link_bytes_ + link_table_len;
        for (const char *p = link_bytes_ ; p < link_end ; )
        {
            /* read and skip the name length */
            size_t len = osrp1(p);
            ++p;

            /* read the TO index */
            unsigned int to = osrp4(p + len + 1);
            
            /* look up the TO field */
            ZoneHashEntry *e_to = (to < zone_cnt ? zone_index[to] : 0);

            /* if we found it, create the new entry for the FROM */
            if (e_to != 0)
                db_tab_->add(new ZoneHashEntryLink(p, len, FALSE, e_to));

            /* skip the entry */
            p += len + 1 + 4;
        }
    }

    /* if there are no errors so far, read the abbreviation table */
    if (ok)
    {
        /* create the abbreviation hash table */
        abbr_tab_ = new CVmHashTable(64, new CVmHashFuncCI(), TRUE);

        /* populate it */
        const char *abbr_end = abbr_bytes_ + abbr_table_len;
        for (const char *p = abbr_bytes_ ; p < abbr_end ; )
        {
            /* read and skip the abbreviation length prefix */
            size_t len = osrp1(p);
            ++p;

            /* read the number of zone entries */
            int entry_cnt = osrp1(p + len + 1);

            /* create and add the hash table entry */
            AbbrHashEntry *e = new AbbrHashEntry(p, len, FALSE, entry_cnt);
            abbr_tab_->add(e);

            /* populate the entry */
            int ei;
            for (p += len + 1 + 1, ei = 0 ; ei < entry_cnt ; p += 9, ++ei)
            {
                /* look up the zone */
                uint32_t zi = osrp4(p);
                ZoneHashEntry *zone = (zi < zone_cnt ? zone_index[zi] : 0);
                
                /* add the entry */
                e->set_zone(ei, zone, osrp4s(p+4), p[8]);
            }
        }
    }

    /* we're done with the numerical zone index */
    if (zone_index != 0)
        delete [] zone_index;

    /* return the status */
    index_loaded_ok_ = ok;
    return ok;
}

/*
 *   Open the zoneinfo file 
 */
osfildef *CVmTimeZoneCache::open_zoneinfo_file(VMG0_)
{
    /* open the file through the system resource loader */
    return G_host_ifc->get_sys_res_loader()->open_res_file(
        "timezones.t3tz", 0, "TMZN");
}

/* ------------------------------------------------------------------------ */
/*
 *   Time zone object 
 */

/* file data block sizes */
const int TRANS_SIZE = 9;
const int TYPE_SIZE = 9;
const int RULE_SIZE = 17;

/*
 *   construction 
 */
CVmTimeZone::CVmTimeZone(ZoneHashEntryDb *entry, const char *file_data,
                         unsigned int trans_cnt, unsigned int type_cnt,
                         unsigned int rule_cnt, unsigned int abbr_bytes)
{
    /* keep a pointer to our hash table entry */
    hashentry_ = entry;

    /* since we have a hash entry, we don't need a separate name */
    name_ = 0;

    /* allocate our arrays (do this first, as they have cross references) */
    trans_ = (trans_cnt_ = trans_cnt) != 0 ? new vmtz_trans[trans_cnt] : 0;
    rule_ = (rule_cnt_ = rule_cnt) != 0 ? new vmtz_rule[rule_cnt] : 0;
    ttype_ = (ttype_cnt_ = type_cnt) != 0 ? new vmtz_ttype[type_cnt] : 0;
    abbr_ = (abbr_bytes != 0 ? new char[abbr_bytes] : 0);

    /* assume we have no descriptive data */
    country_[0] = '\0';
    coords_[0] = '\0';
    desc_ = 0;

    /* set up to read from the file data block */
    const char *p = file_data;
    unsigned int i;

    /* decode the transition list */
    vmtz_trans *t;
    for (t = trans_, i = 0 ; i < trans_cnt ; ++i, ++t, p += TRANS_SIZE)
    {
        t->dayno = osrp4s(p);
        t->daytime = osrp4s(p+4);
        unsigned int idx = osrp1(p+8);
        t->ttype = idx < type_cnt ? &ttype_[idx] : 0;
    }

    /* decode the rule list */
    vmtz_rule *r;
    for (r = rule_, i = 0 ; i < rule_cnt ; ++i, ++r, p += RULE_SIZE)
    {
        unsigned int idx = osrp1(p);
        r->fmt = idx < abbr_bytes ? abbr_ + idx : 0;
        r->mm = osrp1(p+1);
        r->when = osrp1(p+2);
        r->dd = osrp1(p+3);
        r->weekday = osrp1(p+4);
        r->at = osrp4s(p+5);
        r->gmtofs = osrp4s(p+9);
        r->save = osrp4s(p+13);

        /* decode the time zone from the AT data */
        r->at_zone = (r->at >> 24) & 0xff;
        r->at &= 0x00ffffff;
    }

    /* decode the type list */
    vmtz_ttype *tt;
    for (tt = ttype_, i = 0 ; i < type_cnt ; ++i, ++tt, p += TYPE_SIZE)
    {
        tt->gmtofs = osrp4s(p);
        tt->save = osrp4s(p+4);
        unsigned int idx = osrp1(p+8);
        tt->fmt = idx < abbr_bytes ? abbr_ + idx : 0;
    }

    /* copy the format abbreviation strings */
    if (abbr_bytes != 0)
    {
        memcpy(abbr_, p, abbr_bytes);
        p += abbr_bytes;
    }

    /* save the descriptive data, if available */
    if (*p != '\0')
    {
        /* save the country code */
        lib_strcpy(country_, sizeof(country_), p, 2);
        p += 2;

        /* save the coordinate string */
        lib_strcpy(coords_, sizeof(coords_), p, 15);
        p += 16;

        /* save the description */
        size_t dlen = osrp1(p);
        desc_ = new char[dlen + 1];
        lib_strcpy(desc_, dlen + 1, p+1, dlen);
        p += dlen + 2;
    }
    else
    {
        /* null byte for country - no descriptive information */
    }

#if 0// DEBUG $$$
    for (t = trans_, i = 0 ; i < trans_cnt ; ++i, ++t)
    {
        caldate_t cd(t->dayno);
        printf("%04d-%02d-%02d %02d:%02d  -> GMT%+d (%+d) %s\n",
               cd.y, cd.m, cd.d,
               (int)t->daytime/(60*60), (int)->daytime/60 % 60,
               t->ttype->gmtofs, t->ttype->save, t->ttype->fmt);
    }
#endif
}

/*
 *   Create a synthesized zone from an os descriptor
 */
CVmTimeZone::CVmTimeZone(ZoneHashEntrySynth *entry)
{
    /* keep a pointer to our zone entry */
    hashentry_ = entry;

    /* since we have a hash entry, we don't need a separate name */
    name_ = 0;

    /* initialize from the descriptor */
    init(&entry->synth);
}

/*
 *   Create a synthesized zone from an os descriptor 
 */
CVmTimeZone::CVmTimeZone(const os_tzinfo_t *desc)
{
    /* no hash entry */
    hashentry_ = 0;

    /* 
     *   Synthesize a POSIX-style name from the abbreviation and offset.  If
     *   we have both standard and daylight abbreviations and distinct
     *   offsets, generate a POSIX-style "EST5DST" string.  Otherwise just
     *   use the active abbreviation and offset.
     *   
     *   Note that the offset in POSIX strings uses the opposite sign from
     *   what we use - EST is +5 in POSIX strings vs -5 in the zoneinfo
     *   scheme (which is what we use).
     */
    char buf[128];
    if (desc->std_abbr[0] != 0 && desc->dst_abbr[0] != 0
        && desc->std_ofs != desc->dst_ofs)
    {
        /* 
         *   Use both abbreviations, as in EST5DST.  If the daylight offset
         *   is one hour higher than the standard offset, omit the offset
         *   from the daylight part, since that's the default; otherwise
         *   include it.
         */
        char buf1[128], buf2[128];
        gen_ofs_string(buf1, sizeof(buf1), desc->std_abbr, -desc->std_ofs, 0);
        if (desc->dst_ofs == desc->std_ofs + 3600)
            lib_strcpy(buf2, sizeof(buf2), desc->dst_abbr);
        else
            gen_ofs_string(buf2, sizeof(buf2),
                           desc->dst_abbr, -desc->dst_ofs, 0);

        /* build the combined string */
        t3sprintf(buf, sizeof(buf), "%s%s", buf1, buf2);
    }
    else if (desc->is_dst)
    {
        /* use the daylight abbreviation only */
        gen_ofs_string(buf, sizeof(buf), desc->dst_abbr, -desc->dst_ofs, 0);
    }
    else
    {
        /* use the standard time abbreviation only */
        gen_ofs_string(buf, sizeof(buf), desc->std_abbr, -desc->std_ofs, 0);
    }

    /* save the name */
    name_ = lib_copy_str(buf);

    /* initialize from the descriptor */
    init(desc);
}

/*
 *   Initialize from an OS timezone descriptor 
 */
void CVmTimeZone::init(const os_tzinfo_t *desc)
{    
    /* we don't have any transitions or rules */
    trans_ = 0;
    trans_cnt_ = 0;
    rule_ = 0;
    rule_cnt_ = 0;

    /* make a copy of our one abbreviation(s) */
    size_t std_abbr_len = strlen(desc->std_abbr);
    size_t dst_abbr_len = strlen(desc->dst_abbr);
    abbr_ = new char[std_abbr_len + dst_abbr_len + 2];

    char *std_abbr = abbr_;
    strcpy(std_abbr, desc->std_abbr);

    char *dst_abbr = abbr_ + std_abbr_len + 1;
    strcpy(dst_abbr, desc->dst_abbr);

    /* if rules for the start/end dates are specified, use them */
    if ((desc->dst_start.jday != 0
         || desc->dst_start.yday != 0
         || desc->dst_start.month != 0)
        && (desc->dst_end.jday != 0
            || desc->dst_end.yday != 0
            || desc->dst_end.month != 0))
    {
        /* we have start/end rules - create our copies */
        rule_cnt_ = 2;
        rule_ = new vmtz_rule[2];

        /* 
         *   Set the rules from the OS description.  Note that the OS
         *   description gives offsets in seconds, whereas we store
         *   everything in milliseconds. 
         */
        rule_[0].set(&desc->dst_start, dst_abbr, desc->std_ofs*1000,
                     (desc->dst_ofs - desc->std_ofs)*1000);
        rule_[1].set(&desc->dst_end, std_abbr, desc->std_ofs*1000, 0);

        /*
         *   allocate space for two types - one for standard time, one for
         *   daylight time
         */
        ttype_ = new vmtz_ttype[2];
        ttype_cnt_ = 2;
        
        /* set up the standard time information */
        ttype_[0].gmtofs = desc->std_ofs * 1000;
        ttype_[0].save = 0;
        ttype_[0].fmt = std_abbr;
        
        /* set up the daylight time information */
        ttype_[1].gmtofs = desc->dst_ofs * 1000;
        ttype_[1].save = (desc->dst_ofs - desc->std_ofs) * 1000;
        ttype_[1].fmt = dst_abbr;
    }
    else
    {
        /*
         *   There are no transition rules, so we only have use for whichever
         *   setting is currently in effect. 
         */
        ttype_ = new vmtz_ttype[1];
        ttype_cnt_ = 1;

        /* use the current setting only */
        ttype_[0].save = 0;
        if (desc->is_dst)
        {
            ttype_[0].gmtofs = desc->dst_ofs * 1000;
            ttype_[0].fmt = dst_abbr;
        }
        else
        {
            ttype_[0].gmtofs = desc->std_ofs * 1000;
            ttype_[0].fmt = std_abbr;
        }
    }

    /* we have no descriptive information */
    country_[0] = '\0';
    coords_[0] = '\0';
    desc_ = 0;
}

/*
 *   destruction 
 */
CVmTimeZone::~CVmTimeZone()
{
    /* delete allocated items */
    lib_free_str(name_);
    if (trans_ != 0)
        delete [] trans_;
    if (ttype_ != 0)
        delete [] ttype_;
    if (rule_ != 0)
        delete [] rule_;
    if (abbr_ != 0)
        delete [] abbr_;
    if (desc_ != 0)
        delete [] desc_;
}

/*
 *   Load or create a CVmTimeZone object from a hash table entry.  This
 *   creates a new object, even if the hash table entry already has an
 *   associated object.
 */
CVmTimeZone *CVmTimeZone::load(VMG_ ZoneHashEntry *entry)
{
    /* load according to the data source in the hash table entry */
    if (entry->typ == ZONE_HASH_DB)
    {
        /* it's in the database file - open the file */
        osfildef *fp = CVmTimeZoneCache::open_zoneinfo_file(vmg0_);
        if (fp == 0)
            return 0;

        /* seek to our data */
        osfseek(fp, ((ZoneHashEntryDb *)entry)->seekpos, OSFSK_SET);

        /* read the length prefix */
        char pfx[2];
        int ok = !osfrb(fp, pfx, 2);
        
        /* allocate space for the file data */
        unsigned int alo = osrp2(pfx);
        char *buf = ok ? new char[alo] : 0;
        
        /* read the file data */
        ok = buf != 0 && !osfrb(fp, buf, alo);
        
        /* decode the header */
        unsigned int trans_cnt = osrp2(buf);
        unsigned int type_cnt = osrp2(buf+2);
        unsigned int rule_cnt = osrp1(buf+4);
        unsigned int abbr_bytes = osrp1(buf+5);
        
        /* create the time zone object */
        CVmTimeZone *tz = 0;
        if (ok)
        {
            tz = new CVmTimeZone(
                (ZoneHashEntryDb *)entry,
                buf+6, trans_cnt, type_cnt, rule_cnt, abbr_bytes);
        }

        /* release resources */
        if (buf != 0)
            delete [] buf;
        osfcls(fp);

        /* return the loaded object */
        return tz;
    }
    else if (entry->typ == ZONE_HASH_LINK)
    {
        /* load from the linked object */
        return load(vmg_ entry->resolve_links());
    }
    else if (entry->typ == ZONE_HASH_SYNTH)
    {
        /* create from the zone descriptor */
        return new CVmTimeZone((ZoneHashEntrySynth *)entry);
    }
    else
    {
        /* unknown type */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the zone name 
 */
const char *CVmTimeZone::get_name(size_t &len) const
{
    /* use the name, or the hash table name, or the primary abbreviation */
    if (name_ != 0)
    {
        /* we have an explicit name field - use it */
        len = strlen(name_);
        return name_;
    }
    else if (hashentry_ != 0)
    {
        /* get the name from our hash table entry */
        len = hashentry_->getlen();
        return hashentry_->getstr();
    }
    else
    {
        /* no name or hash entry - use our primary abbreviation */
        len = strlen(abbr_);
        return abbr_;
    }
}

/*
 *   Populate a List object with our alias list.  This sets the first element
 *   to our primary name, and remaining elements to aliases.
 */
vm_obj_id_t CVmTimeZone::get_name_list(VMG0_) const
{
    /* no names found so far */
    int cnt = 0;

    /* count names, depending on what information we have */
    if (hashentry_ != 0)
    {
        /* the hash entry gives us our primary name */
        cnt += 1;
        
        /* count incoming alias links */
        for (ZoneHashEntryLink *e = hashentry_->link_from ; e != 0 ;
             e = e->link_nxt, ++cnt) ;
    }
    else if (name_ != 0)
    {
        /* we have only the one primary name */
        cnt += 1;
    }
    else if (abbr_ != 0 && abbr_[0] != '\0')
    {
        /* no name, but we at least have an abbreviation we can use in lieu */
        cnt += 1;
    }

    /* allocate the result list; stack it for gc protection */
    vm_obj_id_t lstid = CVmObjList::create(vmg_ FALSE, cnt);
    G_stk->push()->set_obj(lstid);
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ lstid);
    lst->cons_clear();

    /* start at index 0 */
    int idx = 0;
    vm_val_t ele;

    /* 
     *   add our primary name - this is our explicit name_ string, our hash
     *   entry name, or our primary abbreviation (in order of priority) 
     */
    if (hashentry_ != 0)
    {
        /* set the name from our hash entry */
        ele.set_obj(CVmObjString::create(
            vmg_ FALSE, hashentry_->getstr(), hashentry_->getlen()));
        lst->cons_set_element(idx++, &ele);

        /* add our aliases */
        for (ZoneHashEntryLink *link = hashentry_->link_from ; link != 0 ;
             link = link->link_nxt)
        {
            ele.set_obj(CVmObjString::create(
                vmg_ FALSE, link->getstr(), link->getlen()));
            lst->cons_set_element(idx++, &ele);
        }
    }
    else if (name_ != 0)
    {
        /* the first item is our primary name */
        ele.set_obj(CVmObjString::create(vmg_ FALSE, name_, strlen(name_)));
        lst->cons_set_element(idx++, &ele);
    }
    else if (abbr_ != 0 && abbr_[0] != '\0')
    {
        /* use our primary abbreviation as our name */
        ele.set_obj(CVmObjString::create(vmg_ FALSE, abbr_, strlen(abbr_)));
        lst->cons_set_element(idx++, &ele);
    }

    /* discard the gc protection */
    G_stk->discard();

    /* return the new list object */
    return lstid;
}

/*
 *   Get a history item for a given date 
 */
vm_obj_id_t CVmTimeZone::get_history_item(
    VMG_ int32_t dayno, int32_t daytime) const
{
    /* query the transition for the given date/time */
    vmtzquery result;
    query(&result, dayno, daytime, FALSE);

    /* synthesize a transition description */
    vmtz_ttype tty(&result);
    vmtz_trans t(result.start_dayno, result.start_daytime, &tty);

    /* build and return the List representation */
    return t.get_as_list(vmg0_);
}

/*
 *   Get a list of history items 
 */
vm_obj_id_t CVmTimeZone::get_history_list(VMG0_) const
{
    /* allocate the result list; stack it for gc protection */
    vm_obj_id_t lstid = CVmObjList::create(vmg_ FALSE, trans_cnt_ + 1);
    G_stk->push()->set_obj(lstid);
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ lstid);
    lst->cons_clear();

    /* set the special first item for the pre-establishment settings */
    vmtz_trans t_pre(INT32MINVAL, INT32MINVAL, &ttype_[0]);
    vm_val_t ele;
    ele.set_obj(t_pre.get_as_list(vmg0_));
    lst->cons_set_element(0, &ele);

    /* run through the transitions */
    vmtz_trans *t = trans_;
    for (int i = 1 ; i <= trans_cnt_ ; ++i, ++t)
    {
        /* add this transition to the list */
        ele.set_obj(t->get_as_list(vmg0_));
        lst->cons_set_element(i, &ele);
    }
    
    /* discard the gc protection and return the result list */
    G_stk->discard(1);
    return lstid;
}

/*
 *   Get a list of the ongoing rules 
 */
vm_obj_id_t CVmTimeZone::get_rule_list(VMG0_) const
{
    /* allocate the result list; stack it for gc protection */
    vm_obj_id_t lstid = CVmObjList::create(vmg_ FALSE, rule_cnt_);
    G_stk->push()->set_obj(lstid);
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ lstid);
    lst->cons_clear();

    /* run through the rules */
    vmtz_rule *r = rule_;
    for (int i = 0 ; i < rule_cnt_ ; ++i, ++r)
    {
        /* create a sublist to describe this rule */
        vm_val_t ele;
        ele.set_obj(CVmObjList::create(vmg_ FALSE, 10));
        lst->cons_set_element(i, &ele);

        /* get the sublist and clear it out */
        CVmObjList *elst = (CVmObjList *)vm_objp(vmg_ ele.val.obj);
        elst->cons_clear();

        /* [1] = abbreviation */
        ele.set_obj(CVmObjString::create(vmg_ FALSE, r->fmt, strlen(r->fmt)));
        elst->cons_set_element(0, &ele);

        /* [2] = standard time offset in milliseconds */
        ele.set_int(r->gmtofs);
        elst->cons_set_element(1, &ele);

        /* [3] = daylight savings delta in milliseconds */
        ele.set_int(r->save);
        elst->cons_set_element(2, &ele);

        /* [4] = encoded 'when' rule: "Mar lastSun", "Mar Sun>=7", "Mar 7" */
        char buf[128];
        static const char *mon = "JanFebMarAprMayJunJulAugSepOctNov";
        static const char *wkdy = "SunMonTueWedThuFriSat";
        static const char *tpl[] = {
            "%.0s%02d", "last %.3s", "%.3s>=%d", "%.3s<=d"
        };
        if (r->when == 0 && r->dd > 31)
        {
            /* day of month past 31 -> it's actually a day of the year */
            t3sprintf(buf, sizeof(buf), "DOY %d", r->dd);
        }
        else if (r->when >= 0 && r->when < countof(tpl)
                 && r->weekday >= 1 && r->weekday <= 7)
        {
            /* valid 'when' code */
            memcpy(buf, mon + (r->mm - 1)*3, 3);
            buf[3] = ' ';
            t3sprintf(buf+4, sizeof(buf)-4, tpl[(unsigned char)r->when],
                      wkdy + (r->weekday-1)*3, r->dd);
        }
        else
        {
            /* invalid 'when' code */
            t3sprintf(buf, sizeof(buf), "INVAL(when=%d, weekday=%d, dd=%d)",
                      r->when, r->weekday, r->dd);
        }
            
        ele.set_obj(CVmObjString::create(vmg_ FALSE, buf, strlen(buf)));
        elst->cons_set_element(3, &ele);

        /* [5] = month */
        ele.set_int(r->mm);
        elst->cons_set_element(4, &ele);

        /* [6] = when */
        ele.set_int(r->when);
        elst->cons_set_element(5, &ele);

        /* [7] = day of the month */
        ele.set_int(r->dd);
        elst->cons_set_element(6, &ele);

        /* [8] = weekday */
        ele.set_int(r->weekday);
        elst->cons_set_element(7, &ele);

        /* [9] = at time */
        ele.set_int(r->at);
        elst->cons_set_element(8, &ele);

        /* [10] = at zone */
        char u = (r->at_zone == 0x80 ? 'u' : r->at_zone == 0x40 ? 's' : 'w');
        ele.set_obj(CVmObjString::create(vmg_ FALSE, &u, 1));
        elst->cons_set_element(9, &ele);
    }

    /* discard the gc protection and return the result list */
    G_stk->discard(1);
    return lstid;
}

/* ------------------------------------------------------------------------ */
/*
 *   Query the time zone information at the current moment 
 */
void CVmTimeZone::query(vmtzquery *result) const
{
    /* get the current system time */
    int32_t dayno, daytime;
    caldate_t::now(dayno, daytime);

    /* query the time zone information */
    query(result, dayno, daytime, FALSE);
}

/*
 *   Query the time zone information at the given time (UTC); daytime is
 *   milliseconds after midnight.
 */
void CVmTimeZone::query(vmtzquery *result, int32_t dayno, int32_t daytime,
                        int local) const
{
    /* 
     *   if we're after the last transition, or we have no transitions, check
     *   for ongoing rules 
     */
    if (trans_cnt_ == 0
        || trans_[trans_cnt_-1].compare_to(dayno, daytime, local) <= 0)
    {
        /*
         *   We won't find our answer in the transition list, so check for
         *   ongoing rules that cover out-of-bounds dates. 
         */
        if (rule_cnt_ != 0)
        {        
            /*
             *   We have ongoing rules.
             *   
             *   Starting in the year AFTER the target date, search for the
             *   last (in time) rule that fires on or before the target date.
             *   This is the rule that defines the time zone for the target
             *   date.  If we don't find a rule on or before the target date,
             *   back up one year and repeat.
             *   
             *   The reason we have to start next year is that there's a
             *   possibility that the target date will be in the following
             *   year in local time; rules are generally stated in local
             *   time, so this difference could matter.  For example, if the
             *   target date is Dec 31 2010 23:59 UTC, that could be Jan 1
             *   2011 07:59 local time.  If we then had a rule that fired at
             *   01:00 on Jan 1, that rule in 2011 would be the correct
             *   result of our search.  This is a pathological case that's
             *   unlikely to arise in the wild, but it's not impossible.
             *   Likewise, a target date on Jan 1 could end up preceding a
             *   rule that fires on Dec 31 the previous year; looping
             *   backwards on year until we find a rule that fires on or
             *   before the target will catch these cases.
             *   
             *   Note that our zoneinfo compiler guarantees that it will
             *   produce at least one complete cycle fixed transitions (in
             *   our trans_ list) for the open-ended rules.  Since we're
             *   after our last transition, we know that the period before a
             *   rule's trigger date is controlled by the previous rule (in
             *   time order, treating the list as circular) and not by any
             *   other zone history change.  This makes it possible to work
             *   backwards through the list, since we don't have to look for
             *   other possible zone definitions in the period before each
             *   rule.
             */
            caldate_t cd(dayno);
            for (int yy = cd.y + 1 ; yy >= cd.y - 2 ; --yy)
            {
                /* look for the last rule firing on or after the target */
                int latest = -1;
                int32_t latest_rday = 0, latest_rtime = 0;
                for (int i = 0 ; i < rule_cnt_ ; ++i)
                {
                    /* 
                     *   we might need the previous rule to resolve the
                     *   current rule; the list is circular because it forms
                     *   an annual cycle 
                     */
                    int iprv = (i == 0 ? rule_cnt_ : i) - 1;
                    
                    /* get the concrete date for this rule in this year */
                    int32_t rday, rtime;
                    rule_[i].resolve(rday, rtime, yy, &rule_[iprv]);
                    
                    /* adjust to local time if that's what we're looking for */
                    if (local)
                    {
                        rtime += rule_[i].gmtofs + rule_[i].save;
                        caldate_t::normalize(rday, rtime);
                    }
                    
                    /* 
                     *   If the target date is after the resolved rule firing
                     *   time, and this rule is later than the last one we
                     *   matched, this is the best match so far.  (Assume
                     *   that we never have two rules on the same day, so we
                     *   don't need to bother checking rtime against the last
                     *   matching rule.)  Note that we don't match exactly at
                     *   the rule time - our policy is to put the moment of a
                     *   transition in the previous period.
                     */
                    if ((dayno > rday || (dayno == rday && daytime > rtime))
                        && (latest < 0 || rday > latest_rday))
                    {
                        latest = i;
                        latest_rday = rday;
                        latest_rtime = rtime;
                    }
                }
                
                /* if we found a match, return its settings */
                if (latest >= 0)
                {
                    result->set(&rule_[latest], latest_rday, latest_rtime);
                    return;
                }
            }
            
            /* 
             *   We should never get here - it should be *almost* impossible
             *   not to find a match in the prior year, but not quite
             *   impossible: if a zone only has rules that fire late on Dec
             *   31, we could have a target early on Jan 1 UTC which, when
             *   translated to local time, is before all of the Dec 31 rules.
             *   But it should be truly impossible not to find a match in
             *   target year minus 2 - no UTC-to-local difference could take
             *   us back a whole year.  So there's something wrong in our
             *   logic if we get here...
             */
            assert(FALSE);
        }
        else if (trans_cnt_ != 0)
        {
            /* 
             *   There are no rules, and we're after the last transition, so
             *   the last transition is permanent. 
             */
            result->set(&trans_[trans_cnt_-1]);
        }
        else
        {
            /*
             *   There are no rules and no transitions.  This means that the
             *   zone is defined entirely by its first time type entry. 
             */
            result->set(&ttype_[0], INT32MINVAL, INT32MINVAL);
            return;
        }
    }

    /*
     *   If we're at or before the first transition, use the
     *   pre-establishment settings.  This is usually the local mean time
     *   settings from before a standard time zone was first established at
     *   this location.
     */
    if (trans_[0].compare_to(dayno, daytime, local) >= 0)
    {
        /* the pre-establishment settings are in ttype[0] */
        result->set(&ttype_[0], INT32MINVAL, INT32MINVAL);
        return;
    }

    /*
     *   The date is within our transition list, so search for the nearest
     *   transition on or before the target date.  We're looking for a
     *   date/time after our first transition and before our last, so it's a
     *   date covered by a history item.  Search our transition list for the
     *   target date/time. 
     */
    int lo = 0, hi = trans_cnt_ - 1, cur;
    while (lo < hi)
    {
        /* split the difference */
        cur = (hi + lo)/2;

        /* check this entry */
        vmtz_trans *tcur = &trans_[cur];
        int dir = tcur->compare_to(dayno, daytime, local);
        if (dir < 0)
        {
            /* tcur < target - search the upper half */
            lo = cur + 1;
        }
        else if (dir > 0)
        {
            /* tcur > target - search the lower half */
            hi = cur - 1;
        }
        else
        {
            /* 
             *   exact match - treat the moment of the transition as being in
             *   the previous interval, since we usually define the moment of
             *   the transition in terms of the local time that was in effect
             *   up until that moment
             */
#if 0 /* Gargoyle: Modified from upstream. */
            result->set(tcur > 0 ? tcur - 1 : tcur);
#else
            result->set(tcur != 0 ? tcur - 1 : tcur);
#endif
            return;
        }
    }

    /* check the final 'lo' value; it might have ended up one too high */
    cur = trans_[lo].compare_to(dayno, daytime, local) >= 0
          && lo > 0
          && trans_[lo-1].compare_to(dayno, daytime, local) < 0 ? lo - 1 : lo;

    /* set the result */
    result->set(&trans_[cur]);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare my date to the given date, which can be expressed in either UTC
 *   or local wall clock time.
 */
int vmtz_trans::compare_to(int32_t dayno, int32_t daytime, int local) const
{
    /* get my value, converting to local time if required */
    int32_t dd = this->dayno, tt = this->daytime;
    if (local)
    {
        tt += ttype->gmtofs + ttype->save;
        caldate_t::normalize(dd, tt);
    }
    
    /* compare by day, then by time if on the same day */
    return (dd != dayno ? dd - dayno : tt - daytime);
}

/*
 *   Create a List object representing the transition 
 */
vm_obj_id_t vmtz_trans::get_as_list(VMG0_) const
{
    /* create a result list for this item - [date, ofs, save, fmt] */
    vm_obj_id_t lstid = CVmObjList::create(vmg_ FALSE, 4);
    G_stk->push()->set_obj(lstid);

    /* get the list and clear it out, since we're building it in pieces */
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ lstid);
    lst->cons_clear();

    /* 
     *   Element [1] is the transition time as a Date object, or nil if the
     *   date is INT32MINVALs (which represents the beginning of time, for
     *   the pre-establishment settings).
     */
    vm_val_t ele;
    if (dayno == INT32MINVAL && daytime == INT32MINVAL)
        ele.set_nil();
    else
        ele.set_obj(CVmObjDate::create(vmg_ FALSE, dayno, daytime));
    lst->cons_set_element(0, &ele);

    /* element [2] is the UTC offset */
    ele.set_int(ttype->gmtofs);
    lst->cons_set_element(1, &ele);
    
    /* element [3] is the daylight savings delta */
    ele.set_int(ttype->save);
    lst->cons_set_element(2, &ele);
    
    /* element [4] is the abbreviation string */
    ele.set_obj(CVmObjString::create(
        vmg_ FALSE, ttype->fmt, strlen(ttype->fmt)));
    lst->cons_set_element(3, &ele);

    /* discard gc protection and return the list object */
    G_stk->discard(1);
    return lstid;
}


/* ------------------------------------------------------------------------ */
/*
 *   synthesize a ttype from a query result 
 */
vmtz_ttype::vmtz_ttype(const vmtzquery *query)
{
    gmtofs = query->gmtofs;
    save = query->save;
    fmt = query->abbr;
}


/* ------------------------------------------------------------------------ */
/*
 *   Resolve a rule in the given year.  Returns the firing time in UTC.
 */
void vmtz_rule::resolve(int32_t &dayno, int32_t &daytime, int year,
                        const vmtz_rule *prev_rule)
{
    /* set up our year and month; we have yet to work out the day */
    int w, delta;
    caldate_t cd(year, mm, 0);

    /* figure out the day according to our type */
    switch (when)
    {
    case 0:
        /* the rule takes effect on a the fixed day 'dd' */
        cd.d = dd;
        break;

    case 1:
        /* 
         *   The rule takes effect on the last <weekday> of the month.  Get
         *   the weekday of the last day of this month (which we can figure
         *   using day 0 of next month). 
         */
        cd.m += 1;
        w = cd.weekday() + 1;

        /* figure the days from mm/last to the previous target weekday */
        delta = w - weekday;
        if (delta < 0)
            delta += 7;

        /* go back that many days */
        cd.d -= delta;
        break;

    case 2:
        /* first <weekday> on or after <dd> - get the weekday of <dd> */
        cd.d = dd;
        w = cd.weekday() + 1;

        /* figure days between <dd> and the next <weekday> */
        delta = weekday - w;
        if (delta < 0)
            delta += 7;

        /* go ahead that many days */
        cd.d += delta;
        break;

    case 3:
        /* first <weekday> on or before <dd> - get the weekday of <dd> */
        cd.d = dd;
        w = cd.weekday() + 1;

        /* figure the days from <dd> to the next <weekday> */
        delta = weekday - w;
        if (delta < 0)
            delta += 7;

        /* go ahead that many days and back a week */
        cd.d += delta - 7;
        break;
    }

    /* we now have our effective date and time */
    dayno = cd.dayno();
    daytime = at;

    /* adjust the time zone */
    switch (at_zone)
    {
    case 0x80:
        /* the rule was stated in UTC, which is what we're after */
        break;

    case 0x00:
        /* 
         *   The rule was stated in local wall clock time, in the period in
         *   effect just before the rule.  That means we use the zone
         *   settings of the *previous* rule, which defines the period
         *   leading up to this rule's firing time.  Convert to UTC by
         *   subtracting the standard time offset plus the daylight offset
         *   for the prior period.
         */
        daytime -= prev_rule->gmtofs + prev_rule->save;
        break;

    case 0x40:
        /* 
         *   the rule was stated in local standard time, so subtract the
         *   standard time offset for the prior period 
         */
        daytime -= prev_rule->gmtofs;
        break;
    }

    /* normalize the time to keep it within 0-23:59:59 */
    caldate_t::normalize(dayno, daytime);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set from an OS descriptor 
 */
void vmtz_rule::set(const os_tzrule_t *desc, const char *abbr,
                    int32_t gmtofs, int32_t save)
{
    /* set the basic description */
    this->fmt = abbr;
    this->gmtofs = gmtofs;
    this->save = save;

    /* the OS description always uses local wall clock time */
    this->at = desc->time * 1000;
    this->at_zone = 0;

    /* transcode the date to our format */
    if (desc->jday != 0)
    {
        /* 
         *   "Jn" = nth day of the year, NOT counting Feb 29.  Since the date
         *   mapping is the same for leap years and non-leap years, this is
         *   just an obtuse way of specifying a month and day.  So, figure
         *   the mm/dd date that the Jn date represents, and map it to our
         *   mode 0 (day <dd> of <month>).  We can figure the month/day by
         *   setting up a calendar with "January N" in any non-leap year -
         *   since it's a non-leap year, it won't have a Feb 29, so we'll get
         *   the right mapping for dates after J59.
         */
        caldate_t cd(2011, 1, desc->jday);
        cd.normalize_date();
        this->when = 0;
        this->mm = (char)cd.m;
        this->dd = (short)cd.d;
    }
    else if (desc->yday != 0)
    {
        /* 
         *   "n" = nth day of the year, counting Feb 29.  We can encode this
         *   as "January n", even if n > 31, since our calendar calculator
         *   handle overflows in the day of the month by carrying them into
         *   subsequent months. 
         */
        this->when = 0;
        this->mm = 1;
        this->dd = (short)desc->yday;
    }
    else if (desc->month != 0)
    {
        /* Month/week/day spec */
        this->mm = (char)desc->month;
        this->weekday = (char)desc->day;
        if (desc->week == 5)
        {
            /* week 5 means "last <weekday>", which is our mode 1 */
            this->when = 1;
        }
        else
        {
            /* 
             *   week 1-4 means "nth <weekday>"; we can encode this as mode
             *   2, "weekday>=n", where n is 1 for the first week, 8 for the
             *   second week, 15 for the third week, 22 for the fourth week
             */
            this->when = 2;
            this->dd = (char)(desc->week - 1)*7 + 1;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert a UTC time to local 
 */
const char *CVmTimeZone::utc_to_local(
    int32_t &dayno, int32_t &daytime, int32_t &tzofs)
{
    /* look up the time zone information at the given time */
    vmtzquery result;
    query(&result, dayno, daytime, FALSE);

    /* figure the full offset */
    tzofs = result.gmtofs + result.save;

    /* adjust the time to local by adding the GMT offset */
    daytime += tzofs;
    caldate_t::normalize(dayno, daytime);

    /* return the abbreviation */
    return result.abbr;
}

/*
 *   Convert a local time to UTC 
 */
void CVmTimeZone::local_to_utc(int32_t &dayno, int32_t &daytime)
{
    /* look up the time zone information at the given local time */
    vmtzquery result;
    query(&result, dayno, daytime, TRUE);
    
    /* adjust the time to local by subtracting the GMT offset */
    daytime -= result.gmtofs + result.save;
    caldate_t::normalize(dayno, daytime);
}

