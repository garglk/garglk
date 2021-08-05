#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmhash.cpp,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhash.cpp - hash table implementation
Function
  
Notes
  
Modified
  10/25/97 MJRoberts  - Creation
*/

#include <assert.h>
#include <memory.h>
#include <string.h>

#ifndef STD_H
#include "t3std.h"
#endif
#ifndef VMHASH_H
#include "vmhash.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Simple case-insensitive hash function implementation.
 */
unsigned int CVmHashFuncCI::compute_hash(const char *s, size_t l) const
{
    uint acc;

    /*
     *   Add up all the character values in the string, converting all
     *   characters to upper-case.
     */
    for (acc = 0 ; l != 0 ; ++s, --l)
    {
        uchar c;

        c = (uchar)(is_lower(*s) ? to_upper(*s) : *s);
        acc += c;
    }

    /* return the accumulated value */
    return acc;
}

/* ------------------------------------------------------------------------ */
/*
 *   Simple case-sensitive hash function implementation 
 */
unsigned int CVmHashFuncCS::compute_hash(const char *s, size_t l) const
{
    uint acc;

    /*
     *   add up all the character values in the string, treating case as
     *   significant 
     */
    for (acc = 0 ; l != 0 ; ++s, --l)
    {
        uchar c;

        c = (uchar)*s;
        acc += c;
    }

    /* return the accumulated value */
    return acc;
}


/* ------------------------------------------------------------------------ */
/*
 *   Hash table symbol entry.  This is an abstract class; subclasses must
 *   provide a symbol-matching method.  
 */
CVmHashEntry::CVmHashEntry(const char *str, size_t len, int copy)
{
    /* not linked into a list yet */
    nxt_ = 0;

    /* see if we can use the original string or need to make a private copy */
    if (copy)
    {
        char *buf;

        /* allocate space for a copy */
        buf = new char[len];

        /* copy it into our buffer */
        memcpy(buf, str, len * sizeof(*buf));

        /* remember it */
        str_ = buf;
    }
    else
    {
        /* we can use the original */
        str_ = str;
    }

    /* remember the length */
    len_ = len;

    /* remember whether or not we own the string */
    copy_ = copy;
}

CVmHashEntry::~CVmHashEntry()
{
    /* if we made a private copy of the string, we own it, so delete it */
    if (copy_)
        delete [] (char *)str_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CVmHashEntry providing a case-insensitive
 *   symbol match implementation 
 */
int CVmHashEntryCI::matches(const char *str, size_t len) const
{
    /*
     *   it's a match if the strings are the same length and all
     *   characters match, ignoring case 
     */
    return (len == len_
            && memicmp(str, str_, len * sizeof(*str)) == 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CVmHashEntry providing a case-sensitive symbol
 *   match implementation 
 */
int CVmHashEntryCS::matches(const char *str, size_t len) const
{
    /*
     *   it's a match if the strings are the same length and all
     *   characters match, treating case as significant 
     */
    return (len == len_
            && memcmp(str, str_, len * sizeof(*str)) == 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash table 
 */

void CVmHashTable::init(int hash_table_size,
                        CVmHashFunc *hash_function, int own_hash_func,
                        CVmHashEntry **hash_array)
{
    CVmHashEntry **entry;
    size_t i;

    /* make sure it's a power of two */
    assert(is_power_of_two(hash_table_size));

    /* make sure we got a hash function */
    assert(hash_function != 0);

    /* save the hash function */
    hash_function_ = hash_function;
    own_hash_func_ = own_hash_func;

    /* check to see if the caller provided the hash array */
    if (hash_array != 0)
    {
        /* the caller allocated the table - store a reference to it */
        table_ = hash_array;
        table_size_ = hash_table_size;

        /* note that the table belongs to the caller, so we don't delete it */
        own_hash_table_ = FALSE;
    }
    else
    {
        /* allocate the table */
        table_ = new CVmHashEntry *[hash_table_size];
        table_size_ = hash_table_size;

        /* note that we own the table and must delete it */
        own_hash_table_ = TRUE;
    }

    /* clear the table */
    for (entry = table_, i = 0 ; i < table_size_ ; ++i, ++entry)
        *entry = 0;
}

CVmHashTable::~CVmHashTable()
{
    /* delete the hash function object if I own it */
    if (own_hash_func_)
        delete hash_function_;

    /* delete each entry in the hash table */
    delete_all_entries();

    /* delete the hash table */
    if (own_hash_table_)
        delete [] table_;
}

/*
 *   delete all entries in the hash table, but keep the table itself 
 */
void CVmHashTable::delete_all_entries()
{
    CVmHashEntry **tableptr;
    size_t i;

    for (tableptr = table_, i = 0 ; i < table_size_ ; ++i, ++tableptr)
    {
        CVmHashEntry *entry;
        CVmHashEntry *nxt;

        /* delete each entry in the list at this element */
        for (entry = *tableptr ; entry ; entry = nxt)
        {
            /* remember the next entry */
            nxt = entry->nxt_;

            /* delete this entry */
            delete entry;
        }

        /* there's nothing at this table entry now */
        *tableptr = 0;
    }
}

/*
 *   Verify that a value is a power of two.  Hash table sizes must be
 *   powers of two. 
 */
int CVmHashTable::is_power_of_two(int n)
{
    /* divide by two until we have an odd number */
    while ((n & 1) == 0) n >>= 1;

    /* make sure the result is 1 */
    return (n == 1);
}

/*
 *   Compute the hash value for an entry 
 */
unsigned int CVmHashTable::compute_hash(CVmHashEntry *entry) const
{
    return compute_hash(entry->getstr(), entry->getlen());
}

/*
 *   Compute the hash value for a string 
 */
unsigned int CVmHashTable::compute_hash(const char *str, size_t len) const
{
    return adjust_hash(hash_function_->compute_hash(str, len));
}

/*
 *   Add an object to the table
 */
void CVmHashTable::add(CVmHashEntry *entry)
{
    unsigned int hash;

    /* compute the hash value for this entry */
    hash = compute_hash(entry);

    /* link it into the slot for this hash value */
    entry->nxt_ = table_[hash];
    table_[hash] = entry;
}

/*
 *   Remove an object 
 */
void CVmHashTable::remove(CVmHashEntry *entry)
{
    unsigned int hash;

    /* compute the hash value for this entry */
    hash = compute_hash(entry);

    /* 
     *   if it's the first item in the chain, advance the head over it;
     *   otherwise, we'll need to find the previous item to unlink it 
     */
    if (table_[hash] == entry)
    {
        /* it's the first item - simply advance the head to the next item */
        table_[hash] = entry->nxt_;
    }
    else
    {
        CVmHashEntry *prv;
        
        /* find the previous item in the list for this hash value */
        for (prv = table_[hash] ; prv != 0 && prv->nxt_ != entry ;
             prv = prv->nxt_) ;

        /* if we found it, unlink this item */
        if (prv != 0)
            prv->nxt_ = entry->nxt_;
    }
}

/*
 *   Find an object in the table matching a given string. 
 */
CVmHashEntry *CVmHashTable::find(const char *str, size_t len) const
{
    unsigned int hash;
    CVmHashEntry *entry;

    /* compute the hash value for this entry */
    hash = compute_hash(str, len);

    /* scan the list at this hash value looking for a match */
    for (entry = table_[hash] ; entry ; entry = entry->nxt_)
    {
        /* if this one matches, return it */
        if (entry->matches(str, len))
            return entry;
    }

    /* didn't find anything */
    return 0;
}

/*
 *   Enumerate hash matches for a given string
 */
void CVmHashTable::enum_hash_matches(const char *str, size_t len,
                                     void (*cb)(void *cbctx,
                                                CVmHashEntry *entry),
                                     void *cbctx)
{
    unsigned int hash;

    /* compute the hash value for this entry */
    hash = compute_hash(str, len);

    /* enumerate matches at the hash value */
    enum_hash_matches(hash, cb, cbctx);
}

/*
 *   Enumerate hash matches for a given hash code 
 */
void CVmHashTable::enum_hash_matches(unsigned int hash,
                                     void (*cb)(void *cbctx,
                                                CVmHashEntry *entry),
                                     void *cbctx)
{
    CVmHashEntry *entry;

    /* adjust the hash value for the table size */
    hash = adjust_hash(hash);

    /* enumerate the complete list of entries at this hash value */
    for (entry = table_[hash] ; entry ; entry = entry->nxt_)
    {
        /* call the callback with this entry */
        (*cb)(cbctx, entry);
    }
}

/*
 *   Find an object in the table matching a given leading substring.
 *   We'll return the longest-named entry that matches a leading substring
 *   of the given string.  For example, if there's are entires A, AB, ABC,
 *   and ABCE, and this routine is called to find something matching
 *   ABCDEFGH, we'll return ABC as the match (not ABCE, since it doesn't
 *   match any leading substring of the given string, and not A or AB,
 *   even though they match, since ABC also matches and it's longer).  
 */
CVmHashEntry *CVmHashTable::find_leading_substr(const char *str, size_t len)
{
    size_t sublen;
    CVmHashEntry *entry;

    /* 
     *   try to find each leading substring, starting with the longest,
     *   decreasing by one character on each iteration, until we've used
     *   the whole string 
     */
    for (sublen = len ; sublen > 0 ; --sublen)
    {
        /* if this substring matches, use it */
        if ((entry = find(str, sublen)) != 0)
            return entry;
    }

    /* we didn't find it */
    return 0;
}

/*
 *   Enumerate all entries 
 */
void CVmHashTable::enum_entries(void (*func)(void *, CVmHashEntry *),
                                void *ctx)
{
    CVmHashEntry **tableptr;
    size_t i;

    /* go through each hash value */
    for (tableptr = table_, i = 0 ; i < table_size_ ; ++i, ++tableptr)
    {
        CVmHashEntry *entry;
        CVmHashEntry *nxt;

        /* go through each entry at this hash value */
        for (entry = *tableptr ; entry ; entry = nxt)
        {
            /* 
             *   remember the next entry, in case the callback deletes the
             *   current entry 
             */
            nxt = entry->nxt_;

            /* invoke the callback on this entry */
            (*func)(ctx, entry);
        }
    }
}

/*
 *   Enumerate all entries - ultra-safe version.  This version should be
 *   used when the callback code might delete arbitrary entries from the
 *   hash table.  This version is slower than the standard enum_entries, but
 *   will tolerate any changes to the table made in the callback.  
 */
void CVmHashTable::safe_enum_entries(void (*func)(void *, CVmHashEntry *),
                                     void *ctx)
{
    CVmHashEntry **tableptr;
    size_t i;

    /* go through each hash value */
    for (tableptr = table_, i = 0 ; i < table_size_ ; ++i, ++tableptr)
    {
        size_t list_idx;

        /* 
         *   start at the first (0th) entry in the current hash chain, and
         *   keep going until we run out of entries in the chain 
         */
        for (list_idx = 0 ; ; ++list_idx)
        {
            CVmHashEntry *entry;
            size_t j;

            /* 
             *   Scan the hash chain for the current entry index.
             *   
             *   This is the part that makes this version slower than the
             *   standard version and safer than the standard version.  It's
             *   slower than enum_entries() because we must scan the chain
             *   list on every iteration to find the next entry, whereas
             *   enum_entries() simply keeps a pointer to the next entry.
             *   It's safer because we don't keep any pointers - if next
             *   element is deleted in the callback in enum_entries(), that
             *   stored next pointer would be invalid, but we store no
             *   pointers that could become stale.  
             */
            for (j = 0, entry = *tableptr ; j < list_idx && entry != 0 ;
                 entry = entry->nxt_, ++j) ;

            /* if we failed to find the entry, we're done with this chain */
            if (entry == 0)
                break;

            /* invoke the callback on this entry */
            (*func)(ctx, entry);
        }
    }
}


/*
 *   Move all entries in this table to a new table 
 */
void CVmHashTable::move_entries_to(CVmHashTable *new_tab)
{
    CVmHashEntry **tableptr;
    size_t i;

    /* go through each hash value */
    for (tableptr = table_, i = 0 ; i < table_size_ ; ++i, ++tableptr)
    {
        CVmHashEntry *entry;
        CVmHashEntry *nxt;

        /* go through each entry at this hash value */
        for (entry = *tableptr ; entry ; entry = nxt)
        {
            /* 
             *   remember the next entry, since we'll be unlinking it from
             *   this table, which will render the nxt_ member unusable
             *   for the purposes of completing this enumeration 
             */
            nxt = entry->nxt_;

            /* 
             *   clear the 'next' pointer in this entry, to unlink it from
             *   our table - since everything is being removed, there's no
             *   need to worry about what came before us 
             */
            entry->nxt_ = 0;

            /* add the entry to the new hash table */
            new_tab->add(entry);
        }

        /* 
         *   clear this hash value chain head - we've now removed
         *   everything from it 
         */
        *tableptr = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugging Functions 
 */

#ifdef T3_DEBUG

/*
 *   dump information the hash table to stderr
 */
void CVmHashTable::debug_dump() const
{
    long total;
    long longest;
    long avg;
    int over_avg;
    int empty;
    size_t i;
    
    /* gather statistics on the hash table */
    for (total = longest = 0, empty = 0, i = 0 ; i < table_size_ ; ++i)
    {
        CVmHashEntry *cur;
        int curlen;
        
        /* scan this chain */
        for (curlen = 0, cur = table_[i] ; cur != 0 ; cur = cur->nxt_)
        {
            ++curlen;
            ++total;
        }

        /* if it's empty, so note */
        if (curlen == 0)
            ++empty;

        /* if it's longer than the longest, so note */
        if (curlen > longest)
            longest = curlen;
    }

    /* calculate the average length */
    avg = total/table_size_;

    /* count chains over average length */
    for (over_avg = 0, i = 0 ; i < table_size_ ; ++i)
    {
        CVmHashEntry *cur;
        int curlen;

        /* scan this chain */
        for (curlen = 0, cur = table_[i] ; cur != 0 ; cur = cur->nxt_)
            ++curlen;

        /* if it's over average length, note it */
        if (curlen > avg)
            ++over_avg;
    }

        
    /* display the statistics */
    fprintf(stderr,
            "hash table: total %ld, longest %ld, average %ld\n"
            "number of buckets over average length: %d\n"
            "number of empty buckets: %d\n",
            total, longest, avg, over_avg, empty);
}

#else /* T3_DEBUG */

/* dummy functions for release builds */
void CVmHashTable::debug_dump() const { }


#endif /* T3_DEBUG */

