/* $Header: d:/cvsroot/tads/tads3/vmhash.h,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhash.h - hash table implementation
Function
  
Notes
  
Modified
  10/25/97 MJRoberts  - Creation
*/

#ifndef VMHASH_H
#define VMHASH_H

#ifndef STD_H
#include "t3std.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Hash function interface class.  Hash table clients must implement an
 *   appropriate hash function to use with the hash table; this abstract
 *   class provides the necessary interface.  
 */
class CVmHashFunc
{
public:
    virtual ~CVmHashFunc() { }
    virtual unsigned int compute_hash(const char *str, size_t len) const = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Hash table symbol entry.  This is an abstract class; subclasses must
 *   provide a symbol-matching method.  
 */
class CVmHashEntry
{
public:
    /*
     *   Construct the hash entry.  'copy' indicates whether we should
     *   make a private copy of the value; if not, the caller must keep
     *   the original string around as long as this hash entry is around.
     *   If 'copy' is true, we'll make a private copy of the string
     *   immediately, so the caller need not keep it around after
     *   constructing the entry.  
     */
    CVmHashEntry(const char *str, size_t len, int copy);
    virtual ~CVmHashEntry();

    /* determine if this entry matches a given string */
    virtual int matches(const char *str, size_t len) const = 0;

    /* list link */
    CVmHashEntry *nxt_;

    /* get the string pointer and length */
    const char *getstr() const { return str_; }
    size_t getlen() const { return len_; }

protected:
    const char *str_;
    size_t len_;
    int copy_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Hash table 
 */
class CVmHashTable
{
public:
    /*
     *   Construct a hash table.  If own_hash_func is true, the hash table
     *   object takes ownership of the hash function object, so the hash
     *   table object will delete the hash function object when the table
     *   is deleted.  
     */
    CVmHashTable(int hash_table_size, CVmHashFunc *hash_function,
                 int own_hash_func)
    {
        /* initialize, allocating our own hash array */
        init(hash_table_size, hash_function, own_hash_func, 0);
    }

    /*
     *   Construct a hash table, using memory allocated and owned by the
     *   caller for the hash array. 
     */
    CVmHashTable(int hash_table_size, CVmHashFunc *hash_function,
                 int own_hash_func, CVmHashEntry **hash_array)
    {
        /* initialize, using the provided hash array */
        init(hash_table_size, hash_function, own_hash_func, hash_array);
    }

    /* delete the hash table */
    ~CVmHashTable();

    /*
     *   Add a symbol.  If 'copy' is true, it means that we need to make
     *   a private copy of the string; otherwise, the caller must ensure
     *   that the string remains valid as long as the hash table entry
     *   remains valid, since we'll just store a pointer to the original
     *   string.  IMPORTANT: the hash table takes over ownership of the
     *   hash table entry; the hash table will delete this object when the
     *   hash table is deleted, so the client must not delete the entry
     *   once it's been added to the table.  
     */
    void add(CVmHashEntry *entry);

    /*
     *   Remove an object from the cache.  This routine does not delete
     *   the object. 
     */
    void remove(CVmHashEntry *entry);

    /*
     *   Delete all entries in the table 
     */
    void delete_all_entries();

    /*
     *   Find an entry in the table matching the given string 
     */
    CVmHashEntry *find(const char *str, size_t len) const;

    /* enumerate all entries that match the given string's hash value */
    void enum_hash_matches(const char *str, size_t len,
                           void (*cb)(void *cbctx, CVmHashEntry *entry),
                           void *cbctx);

    /* enumerate all entries with the given hash value */
    void enum_hash_matches(uint hash,
                           void (*cb)(void *cbctx, CVmHashEntry *entry),
                           void *cbctx);

    /* 
     *   Find an entry that matches the longest leading substring of the
     *   given string.  (For this routine, we find a match where the
     *   dictionary word is SHORTER than the given word.)  
     */
    CVmHashEntry *find_leading_substr(const char *str, size_t len);

    /*
     *   Enumerate all entries, invoking a callback for each entry in the
     *   table 
     */
    void enum_entries(void (*func)(void *ctx, class CVmHashEntry *entry),
                      void *ctx);

    /*
     *   Enumerate all entries safely.  This does the same thing as
     *   enum_entries(), but this version is safe to use regardless of any
     *   changes to the table that the callback makes.  
     */
    void safe_enum_entries(void (*func)(void *ctx, class CVmHashEntry *entry),
                           void *ctx);

    /*
     *   Move all of the entries in this hash table to a new hash table.
     *   After this is finished, this hash table will be empty, and the new
     *   hash table will be populated with all of the entries that were
     *   formerly in this table.  We don't reallocate any of the entries -
     *   they simply are unlinked from this table and moved into the new
     *   table.  This can be used to rebuild a hash table with a new bucket
     *   count or hash function.  
     */
    void move_entries_to(CVmHashTable *new_tab);
    
    /* dump information on the hash table to stderr for debugging */
    void debug_dump() const;

    /* compute the hash value for an entry/a string */
    unsigned int compute_hash(CVmHashEntry *entry) const;
    unsigned int compute_hash(const char *str, size_t len) const;

private:
    /* adjust a hash to the table size */
    unsigned int adjust_hash(unsigned int hash) const
        { return hash & (table_size_ - 1); }

    /* initialize */
    void init(int hash_table_size, CVmHashFunc *hash_function,
              int own_hash_func, CVmHashEntry **hash_array);
    
    /* internal service routine for checking hash table sizes for validity */
    int is_power_of_two(int n);

    /* the table of hash entries */
    CVmHashEntry **table_;
    size_t table_size_;

    /* hash function */
    CVmHashFunc *hash_function_;

    /* flag: I own the hash function and must delete it when done */
    unsigned int own_hash_func_ : 1;

    /* flag: I own the hash table array and must delete it when done */
    unsigned int own_hash_table_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Simple case-insensitive hash function 
 */
class CVmHashFuncCI: public CVmHashFunc
{
public:
    unsigned int compute_hash(const char *str, size_t len) const;
};

/* ------------------------------------------------------------------------ */
/*
 *   Simple case-sensitive hash function implementation 
 */
class CVmHashFuncCS: public CVmHashFunc
{
public:
    unsigned int compute_hash(const char *str, size_t len) const;
};

/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CVmHashEntry providing a case-insensitive
 *   symbol match implementation 
 */
class CVmHashEntryCI: public CVmHashEntry
{
public:
    CVmHashEntryCI(const char *str, size_t len, int copy)
        : CVmHashEntry(str, len, copy) { }

    virtual int matches(const char *str, size_t len) const;
};

/* ------------------------------------------------------------------------ */
/*
 *   Concrete subclass of CVmHashEntry providing a case-sensitive symbol
 *   match implementation 
 */
class CVmHashEntryCS: public CVmHashEntry
{
public:
    CVmHashEntryCS(const char *str, size_t len, int copy)
        : CVmHashEntry(str, len, copy) { }

    virtual int matches(const char *str, size_t len) const;
};


#endif /* VMHASH_H */
