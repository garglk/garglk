/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmpack.h - binary data stream pack/unpack
Function
  Converts between a binary data stream and TADS types, using a list of
  type codes to drive the conversion.  This is a convenient way of reading
  and writing structured binary files.  Our implementation uses generic
  data streams, so it can work with various underlying data sources; in
  particular, we provide File and ByteArray interfaces.
Notes
  
Modified
  10/01/10 MJRoberts  - Creation
*/

#ifndef VMPACK_H
#define VMPACK_H

#include "t3std.h"
#include "vmtype.h"


class CVmPack
{
public:
    /*
     *   Pack data from stack arguments into the given stream.  'arg_index'
     *   is the index in the stack of the format string argument; the data
     *   value arguments come after it on the stack.  'argc' is the total
     *   number of arguments, including the format string.  
     */
    static void pack(VMG_ int arg_index, int argc, 
                     class CVmDataSource *dst);

    /*
     *   Unpack data from the given stream into a list.  
     */
    static void unpack(VMG_ vm_val_t *retval, const char *fmt, size_t fmtlen,
                       class CVmDataSource *src);


    /* write padding bytes */
    static void write_padding(class CVmDataSource *dst, char ch, long cnt);
    static void write_padding(class CVmDataSource *dst,
                              char ch1, char ch2, long cnt);

protected:
    /* 
     *   Pack a group.  pack() calls this to start packing the main string,
     *   and we recurse for each parenthesized group we find.  
     */
    static void pack_group(VMG_ struct CVmPackPos *pos,
                           struct CVmPackArgs *args,
                           struct CVmPackGroup *group,
                           int list_per_iter);

    /* pack a subgroup */
    static void pack_subgroup(VMG_ struct CVmPackPos *p,
                              struct CVmPackArgs *args,
                              struct CVmPackGroup *parent,
                              struct CVmPackType *prefix_count);

    /* pack an item or iterated list of items */
    static void pack_item(VMG_ struct CVmPackGroup *group,
                          struct CVmPackArgs *args,
                          struct CVmPackType *typ,
                          struct CVmPackType *prefix_count);

    /* pack a single item */
    static void pack_one_item(VMG_ struct CVmPackGroup *group,
                              struct CVmPackArgs *args,
                              struct CVmPackType *t,
                              struct CVmPackType *prefix_count);

    /* unpack a group */
    static void unpack_group(VMG_ struct CVmPackPos *p,
                             class CVmObjList *retlst, int *retcnt,
                             struct CVmPackGroup *group, int list_per_iter);

    /* unpack a subgroup */
    static void unpack_subgroup(VMG_ CVmPackPos *p,
                                class CVmObjList *retlst, int &retcnt,
                                struct CVmPackGroup *group,
                                const int *prefix_count);

    /* create a sublist for unpacking a "[ ]!" group */
    static CVmObjList *create_group_sublist(
        VMG_ CVmObjList *parent_list, int *parent_cnt);

    /* unpack an iterated item */
    static void unpack_iter_item(VMG_ struct CVmPackPos *p,
                                 class CVmObjList *retlst, int &retcnt,
                                 struct CVmPackGroup *group,
                                 const int *prefix_count);

    /* unpack an item into the list */
    static void unpack_into_list(VMG_ class CVmObjList *retlst, int &retcnt,
                                 class CVmDataSource *src,
                                 const struct CVmPackType *t,
                                 const struct CVmPackGroup *group);

    /* unpack a single item */
    static void unpack_item(VMG_ vm_val_t *val,
                            CVmDataSource *src,
                            const struct CVmPackType *t,
                            const struct CVmPackGroup *group);

    /* parse a type code; increments the pointer past the parsed input */
    static void parse_type(struct CVmPackPos *p, struct CVmPackType *info,
                           const struct CVmPackGroup *group);

    /* 
     *   parse the suffix modifiers for a group, given a pointer to the
     *   opening parenthesis of the group 
     */
    static void parse_group_mods(const struct CVmPackPos *p,
                                 struct CVmPackType *gt);

    /* find the close parenthesis/bracket of a group */
    static void skip_group(struct CVmPackPos *p, wchar_t close_paren);

    /* parse and skip a group and its modifiers */
    static void skip_group_mods(struct CVmPackPos *p, struct CVmPackType *gt);

    /* parse suffix modifiers */
    static void parse_mods(struct CVmPackPos *p, struct CVmPackType *t);
};

#endif /* VMPACK_H */
