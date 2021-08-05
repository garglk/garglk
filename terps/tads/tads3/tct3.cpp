#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/tct3.cpp,v 1.5 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3.cpp - TADS 3 Compiler - T3 VM Code Generator
Function
  Generate code for the T3 VM
Notes
  
Modified
  05/08/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <assert.h>

#include "t3std.h"
#include "os.h"
#include "tcprs.h"
#include "tct3.h"
#include "tcgen.h"
#include "vmtype.h"
#include "vmwrtimg.h"
#include "vmfile.h"
#include "tcmain.h"
#include "tcerr.h"
#include "vmbignum.h"
#include "vmrunsym.h"
#include "vmhash.h"
#include "vmmeta.h"
#include "tct3unas.h"
#include "vmop.h"


/* ------------------------------------------------------------------------ */
/*
 *   Named argument call table.  The code body maintains a list of tables,
 *   one per call with named arguments.  At the end of the code body, we
 *   generate the tables as part of the instruction stream.  
 */
struct CTcNamedArgTab
{
    CTcNamedArgTab(const CTcNamedArgs *args)
    {
        /* remember the argument list */
        arglist = args->args;
        cnt = args->cnt;

        /* create a code label for the table's opcode stream location */
        lbl = G_cs->new_label_fwd();

        /* not in a list yet */
        nxt = 0;
    }

    /* generate this table */
    void generate()
    {
        /* define the label location and start the table */
        CTcPrsNode::def_label_pos(lbl);
        G_cg->write_op(OPC_NAMEDARGTAB);

        /* write a placeholder for the table size */
        ulong start = G_cs->get_ofs();
        G_cs->write2(0);

        /* write the number of arguments in the list */
        G_cs->write2(cnt);

        /* write placeholders for the index entries */
        ulong idx = G_cs->get_ofs(), idx0 = idx;
        for (CTPNArg *arg = arglist->get_arg_list_head() ; arg != 0 ;
             arg = arg->get_next_arg())
        {
            /* if it's a named argument, write an index placeholder */
            if (arg->is_named_param())
                G_cs->write2(0);
        }

        /* write a final index marker, for computing the last item's length */
        G_cs->write2(0);

        /* write the strings */
        for (CTPNArg *arg = arglist->get_arg_list_head() ; arg != 0 ;
             arg = arg->get_next_arg())
        {
            /* if it's a named argument, write it out */
            if (arg->is_named_param())
            {
                /* write its index pointer */
                G_cs->write2_at(idx, G_cs->get_ofs() - idx0);

                /* write the string */
                G_cs->write(arg->get_name(), arg->get_name_len());

                /* advance the index pointer */
                idx += 2;
            }
        }

        /* write the final index marker */
        G_cs->write2_at(idx, G_cs->get_ofs() - idx0);

        /* go back and fill in the table length prefix */
        G_cs->write2_at(start, G_cs->get_ofs() - start - 2);
    }

    /* argument list */
    const CTPNArglist *arglist;

    /* number of named arguments */
    int cnt;

    /* code label for the table's location in the opcode stream */
    CTcCodeLabel *lbl;

    /* next in list */
    CTcNamedArgTab *nxt;
};


/* ------------------------------------------------------------------------ */
/*
 *   T3 Target Code Generator class 
 */

/*
 *   initialize the code generator 
 */
CTcGenTarg::CTcGenTarg()
{
    int i;
    
    /* 
     *   we haven't written any instructions yet - fill the pipe with
     *   no-op instructions so that we don't think we can combine the
     *   first instruction with anything previous 
     */
    last_op_ = OPC_NOP;
    second_last_op_ = OPC_NOP;

    /* 
     *   we haven't seen any strings or lists yet - set the initial
     *   maximum lengths to zero 
     */
    max_str_len_ = 0;
    max_list_cnt_ = 0;

    /* we haven't generated any code yet */
    max_bytecode_len_ = 0;

    /* there are no metaclasses defined yet */
    meta_head_ = meta_tail_ = 0;
    meta_cnt_ = 0;

    /* no function sets defined yet */
    fnset_head_ = fnset_tail_ = 0;
    fnset_cnt_ = 0;

    /* 
     *   Add the built-in metaclass entries.  The order of these entries
     *   is fixed to match the TCT3_METAID_xxx constants - if this order
     *   changes, those constants must change to match.  
     */
    add_meta("tads-object");
    add_meta("list");
    add_meta("dictionary2/030000");
    add_meta("grammar-production/030000");
    add_meta("vector");
    add_meta("anon-func-ptr");
    add_meta("int-class-mod/030000");
    add_meta("lookuptable/030003");

    /*
     *   Set up the initial translation table for translating from our
     *   internal metaclass index values - TCT3_METAID_xxx - to the *actual*
     *   image file dependency index.  When we're compiling a program, these
     *   are simply in our own internal order - index N in our scheme is
     *   simply index N in the actual image file - because we get to lay out
     *   exactly what the dependency table looks like.  However, we need the
     *   translation table for when we're being used as part of the debugger
     *   - in those cases, we don't get to dictate the dependency table
     *   layout, because the image file (and thus the dependency table) might
     *   have been generated by a different version of the compiler.  In
     *   those cases, the image file loader will have to set up the
     *   translation information for us.  
     */
    for (i = 0 ; i <= TCT3_METAID_LAST ; ++i)
        predef_meta_idx_[i] = i;
    
    /* start at the first valid property ID */
    next_prop_ = 1;

    /* start at the first valid object ID */
    next_obj_ = 1;

    /* allocate an initial sort buffer */
    sort_buf_size_ = 4096;
    sort_buf_ = (char *)t3malloc(sort_buf_size_);

    /* not in a constructor */
    in_constructor_ = FALSE;

    /* not in an operator overload */
    in_op_overload_ = FALSE;

    /* no debug line record pointers yet */
    debug_line_cnt_ = 0;
    debug_line_head_ = debug_line_tail_ = 0;

    /* normal (static compiler, non-debug) evaluation mode */
    eval_for_dyn_ = FALSE;
    eval_for_debug_ = FALSE;
    speculative_ = FALSE;
    debug_stack_level_ = 0;

    /* no multi-method initializer object yet */
    mminit_obj_ = VM_INVALID_OBJ;

    /* create the string interning hash table */
    strtab_ = new CVmHashTable(1024, new CVmHashFuncCS(), TRUE);
}

/*
 *   delete the code generator 
 */
CTcGenTarg::~CTcGenTarg()
{
    /* delete all of the metaclass list entries */
    while (meta_head_ != 0)
    {
        tc_meta_entry *nxt;
        
        /* remember the next item */
        nxt = meta_head_->nxt;

        /* delete this item */
        t3free(meta_head_);

        /* move on */
        meta_head_ = nxt;
    }

    /* delete all of the function set list entries */
    while (fnset_head_ != 0)
    {
        tc_fnset_entry *nxt;

        /* remember the next item */
        nxt = fnset_head_->nxt;

        /* delete this item */
        t3free(fnset_head_);

        /* move on */
        fnset_head_ = nxt;
    }

    /* delete our sort buffer */
    t3free(sort_buf_);

    /* delete the debug line record pointers */
    while (debug_line_head_ != 0)
    {
        tct3_debug_line_page *nxt;

        /* remember the next one */
        nxt = debug_line_head_->nxt;

        /* delete this one */
        t3free(debug_line_head_);

        /* move on */
        debug_line_head_ = nxt;
    }

    /* delete the string table */
    delete strtab_;
}

/* start loading the image file metaclass dependency table */
void CTcGenTarg::start_image_file_meta_table()
{
    int i;
    
    /* 
     *   clear out all of the entries - set them to -1 to indicate that
     *   they're invalid 
     */
    for (i = 0 ; i <= TCT3_METAID_LAST ; ++i)
        predef_meta_idx_[i] = -1;
}

/*
 *   Load an image file metaclass dependency table.  When we're being used in
 *   a debugger, the image loader must call this with the run-time dependency
 *   table to establish the translation from our internal metaclass
 *   identifiers (TCT3_METAID_xxx) to the actual run-time index values.  
 */
void CTcGenTarg::load_image_file_meta_table(
    const char *nm, size_t len, int idx)
{
    /* 
     *   Check the name against our known names.  If it matches one of the
     *   known types, store the actual index in our translation table under
     *   our internal index entry.  If it's not a known name, ignore it - we
     *   only care about the names we actually pre-define, because those are
     *   the only ones we need to generate our own code for.  
     */
    if (len == 11 && memcmp(nm, "tads-object", 11) == 0)
        predef_meta_idx_[TCT3_METAID_TADSOBJ] = idx;
    else if (len == 4 && memcmp(nm, "list", 4) == 0)
        predef_meta_idx_[TCT3_METAID_LIST] = idx;
    else if (len == 11 && memcmp(nm, "dictionary2", 11) == 0)
        predef_meta_idx_[TCT3_METAID_DICT] = idx;
    else if (len == 18 && memcmp(nm, "grammar-production", 18) == 0)
        predef_meta_idx_[TCT3_METAID_GRAMPROD] = idx;
    else if (len == 6 && memcmp(nm, "vector", 6) == 0)
        predef_meta_idx_[TCT3_METAID_VECTOR] = idx;
    else if (len == 13 && memcmp(nm, "anon-func-ptr", 13) == 0)
        predef_meta_idx_[TCT3_METAID_ANONFN] = idx;
    else if (len == 13 && memcmp(nm, "int-class-mod", 13) == 0)
        predef_meta_idx_[TCT3_METAID_ICMOD] = idx;
    else if (len == 11 && memcmp(nm, "lookuptable", 11) == 0)
        predef_meta_idx_[TCT3_METAID_LOOKUP_TABLE] = idx;
}

/*
 *   End the image file metaclass dependency table. 
 */
void CTcGenTarg::end_image_file_meta_table()
{
    int i;
    
    /* 
     *   Scan the metaclass translation table and make sure they've all been
     *   set.  If any are unset, it means that these metaclasses aren't
     *   available; since we depend upon these metaclasses, this means that
     *   we can't generate code interactively, so we can't act as a debugger.
     */
    for (i = 0 ; i <= TCT3_METAID_LAST ; ++i)
    {
        /* if this entry hasn't been set properly, abort */
        if (predef_meta_idx_[i] == -1)
            err_throw(VMERR_IMAGE_INCOMPAT_VSN_DBG);
    }
}

/*
 *   Add an entry to the metaclass dependency table 
 */
int CTcGenTarg::add_meta(const char *nm, size_t len,
                         CTcSymMetaclass *sym)
{
    tc_meta_entry *ent;
    size_t extra_len;
    const char *extra_ptr;
    const char *p;
    size_t rem;

    /* 
     *   if the name string doesn't contain a slash, allocate enough space
     *   to add an implied version suffix of "/000000" 
     */
    for (p = nm, rem = len ; rem != 0 && *p != '/' ; ++p, --rem) ;
    if (rem == 0)
    {
        /* we didn't find a version suffix - add space for one */
        extra_len = 7;
        extra_ptr = "/000000";
    }
    else
    {
        /* 
         *   there's already a version suffix - but make sure we have
         *   space for a six-character string 
         */
        if (rem < 7)
        {
            /* add zeros to pad out to a six-place version string */
            extra_len = 7 - rem;
            extra_ptr = "000000";
        }
        else
        {
            /* we need nothing extra */
            extra_len = 0;
            extra_ptr = 0;
        }
    }
    
    /* allocate a new entry for the item */
    ent = (tc_meta_entry *)t3malloc(sizeof(tc_meta_entry) + len + extra_len);
    if (ent == 0)
        err_throw(TCERR_CODEGEN_NO_MEM);

    /* copy the name into the entry */
    memcpy(ent->nm, nm, len);

    /* add any extra version suffix information */
    if (extra_len != 0)
        memcpy(ent->nm + len, extra_ptr, extra_len);

    /* null-terminate the name string in the entry */
    ent->nm[len + extra_len] = '\0';

    /* remember the symbol */
    ent->sym = sym;

    /* link the entry in at the end of the list */
    ent->nxt = 0;
    if (meta_tail_ != 0)
        meta_tail_->nxt = ent;
    else
        meta_head_ = ent;
    meta_tail_ = ent;

    /* count the entry, returning the index of the entry in the list */
    return meta_cnt_++;
}

/*
 *   Find a metaclass index given the global identifier.
 */
tc_meta_entry *CTcGenTarg::find_meta_entry(const char *nm, size_t len,
                                           int update_vsn, int *entry_idx)
{
    tc_meta_entry *ent;
    int idx;
    size_t name_len;
    const char *p;
    const char *vsn;
    size_t vsn_len;
    size_t rem;

    /* find the version suffix, if any */
    for (rem = len, p = nm ; rem != 0 && *p != '/' ; --rem, ++p) ;

    /* note the length of the name portion (up to the '/') */
    name_len = len - rem;

    /* note the version string, if there is one */
    if (rem != 0)
    {
        vsn = p + 1;
        vsn_len = rem - 1;
    }
    else
    {
        vsn = 0;
        vsn_len = 0;
    }

    /* search the existing entries */
    for (idx = 0, ent = meta_head_ ; ent != 0 ; ent = ent->nxt, ++idx)
    {
        size_t ent_name_len;
        char *ent_vsn;

        /* find the version suffix in the entry */
        for (ent_vsn = ent->nm ; *ent_vsn != '\0' && *ent_vsn != '/' ;
             ++ent_vsn) ;

        /* the name is the part up to the '/' */
        ent_name_len = ent_vsn - ent->nm;

        /* note the length of the name and the version suffix */
        if (*ent_vsn == '/')
        {
            /* the version is what follows the '/' */
            ++ent_vsn;
        }
        else
        {
            /* there is no version suffix */
            ent_vsn = 0;
        }

        /* if this is the one, return it */
        if (ent_name_len == name_len && memcmp(ent->nm, nm, name_len) == 0)
        {
            /* 
             *   if this version number is higher than the version number
             *   we previously recorded, remember the new, higher version
             *   number 
             */
            if (update_vsn && ent_vsn != 0 && strlen(ent_vsn) == vsn_len
                && memcmp(vsn, ent_vsn, vsn_len) > 0)
            {
                /* store the new version string */
                memcpy(ent_vsn, vsn, vsn_len);
            }

            /* tell the caller the index, and return the entry */
            *entry_idx = idx;
            return ent;
        }
    }

    /* we didn't find it */
    return 0;
}

/* 
 *   Get the property ID for the given method table index in the given
 *   metaclass.  The metaclass ID is given as the internal metaclass ID
 *   (without the "/version" suffix), not as the external class name.
 *   
 *   The method table index is 0-based.  
 */
CTcPrsNode *CTcGenTarg::get_metaclass_prop(const char *name, ushort idx) const
{
    vm_prop_id_t propid;

    /*
     *   If we have a loaded metaclass table from the running program, use
     *   it.  This will be the case if we're doing a dynamic or debugging
     *   compilation.  If we do, use it, since it has the direct information
     *   on the loaded classes.
     *   
     *   If there's no metaclass table, this is an ordinary static
     *   compilation, so get the metaclass information from the symbol table.
     */
    if (G_metaclass_tab != 0)
    {
        /* look up the metaclass in the table */
        vm_meta_entry_t *entry = G_metaclass_tab->get_entry_by_id(name);

        /* if  we didn't find the entry, return failure */
        if (entry == 0)
            return 0;

        /* 
         *   Get the property from the table.  Note that xlat_func() uses a
         *   1-based index. 
         */
        if ((propid = entry->xlat_func(idx + 1)) == VM_INVALID_PROP)
            return 0;
    }
    else
    {
        /* static compilation - look up the metaclass in the symbol table */
        CTcSymMetaclass *sym = G_cg->find_meta_sym(name, strlen(name));
        if (sym == 0)
            return 0;
        
        /* get the metaclass method table entry at the given index */
        CTcSymMetaProp *mprop = sym->get_nth_prop(idx);
        if (mprop == 0 || mprop->prop_ == 0)
            return 0;

        /* get the property ID from the method table entry */
        propid = mprop->prop_->get_prop();
    }

    /* set up a constant value for the property ID */
    CTcConstVal pv;
    pv.set_prop(propid);

    /* return a constant parse node for the property ID */
    return new CTPNConst(&pv);
}

/*
 *   Find a metaclass symbol given the global identifier 
 */
class CTcSymMetaclass *CTcGenTarg::find_meta_sym(const char *nm, size_t len)
{
    tc_meta_entry *ent;
    int idx;

    /* find the entry */
    ent = find_meta_entry(nm, len, TRUE, &idx);

    /* 
     *   if we found it, return the associated metaclass symbol; if
     *   there's no entry, there's no symbol 
     */
    if (ent != 0)
        return ent->sym;
    else
        return 0;
}


/*
 *   Find or add a metaclass entry 
 */
int CTcGenTarg::find_or_add_meta(const char *nm, size_t len,
                                 CTcSymMetaclass *sym)
{
    tc_meta_entry *ent;
    int idx;

    /* find the entry */
    ent = find_meta_entry(nm, len, TRUE, &idx);

    /* if we found it, return the index */
    if (ent != 0)
    {
        /* 
         *   found it - if it didn't already have a symbol mapping, use
         *   the new symbol; if there is a symbol in the table entry
         *   already, however, do not change it 
         */
        if (ent->sym == 0)
            ent->sym = sym;

        /* return the index */
        return idx;
    }

    /* we didn't find an existing entry - add a new one */
    return add_meta(nm, len, sym);
}

/*
 *   Get the symbol for a given metaclass dependency table entry 
 */
CTcSymMetaclass *CTcGenTarg::get_meta_sym(int meta_idx)
{
    tc_meta_entry *ent;

    /* find the list entry at the given index */
    for (ent = meta_head_ ; ent != 0 && meta_idx != 0 ;
         ent = ent->nxt, --meta_idx) ;

    /* if we didn't find the entry, do nothing */
    if (ent == 0 || meta_idx != 0)
        return 0;

    /* return this entry's symbol */
    return ent->sym;
}

/*
 *   Get the external name for the given metaclass index
 */
const char *CTcGenTarg::get_meta_name(int meta_idx) const
{
    tc_meta_entry *ent;

    /* find the list entry at the given index */
    for (ent = meta_head_ ; ent != 0 && meta_idx != 0 ;
         ent = ent->nxt, --meta_idx) ;

    /* if we didn't find the entry, do nothing */
    if (ent == 0 || meta_idx != 0)
        return 0;

    /* return this entry's external name */
    return ent->nm;
}

/*
 *   Set the symbol for a given metaclass dependency table entry 
 */
void CTcGenTarg::set_meta_sym(int meta_idx, class CTcSymMetaclass *sym)
{
    tc_meta_entry *ent;
    
    /* find the list entry at the given index */
    for (ent = meta_head_ ; ent != 0 && meta_idx != 0 ;
         ent = ent->nxt, --meta_idx) ;

    /* if we didn't find the entry, do nothing */
    if (ent == 0 || meta_idx != 0)
        return;

    /* set this entry's symbol */
    ent->sym = sym;
}

/*
 *   Add an entry to the function set dependency table 
 */
ushort CTcGenTarg::add_fnset(const char *nm, size_t len)
{
    tc_fnset_entry *ent;
    const char *sl;
    size_t sl_len;
    ushort idx;

    /* find the version part of the new name, if present */
    for (sl = nm, sl_len = len ; sl_len != 0 && *sl != '/' ; ++sl, --sl_len) ;

    /* look for an existing entry with the same name prefix */
    for (idx = 0, ent = fnset_head_ ; ent != 0 ; ent = ent->nxt, ++idx)
    {
        char *ent_sl;
        
        /* find the version part of this entry's name, if present */
        for (ent_sl = ent->nm ; *ent_sl != '\0' && *ent_sl != '/' ;
             ++ent_sl) ;

        /* check to see if the prefixes match */
        if (ent_sl - ent->nm == sl - nm
            && memcmp(ent->nm, nm, sl - nm) == 0)
        {
            /*
             *   This one matches.  Keep the one with the higher version
             *   number.  If one has a version number and the other doesn't,
             *   keep the one with the version number.  
             */
            if (*ent_sl == '/' && sl_len != 0)
            {
                /* 
                 *   Both have version numbers - keep the higher version.
                 *   Limit the version length to 6 characters plus the
                 *   slash. 
                 */
                if (sl_len > 7)
                    sl_len = 7;

                /* check if the new version number is higher */
                if (memcmp(sl, ent_sl, sl_len) > 0)
                {
                    /* the new one is higher - copy it over the old one */
                    memcpy(ent_sl, sl, sl_len);
                }

                /* 
                 *   in any case, we're going to keep the existing entry, so
                 *   we're done - just return the existing entry's index 
                 */
                return idx;
            }
            else if (*ent_sl == '/')
            {
                /* 
                 *   only the old entry has a version number, so keep it and
                 *   ignore the new definition - this means we're done, so
                 *   just return the existing item's index 
                 */
                return idx;
            }
            else
            {
                /* 
                 *   Only the new entry has a version number, so store the
                 *   new version number.  To do this, simply copy the new
                 *   entry over the old entry, but limit the version number
                 *   field to 7 characters including the slash.  
                 */
                if (sl_len > 7)
                    len -= (sl_len - 7);

                /* copy the new value */
                memcpy(ent->nm, nm, len);

                /* done - return the existing item's index */
                return idx;
            }
        }
    }

    /* 
     *   Allocate a new entry for the item.  Always allocate space for a
     *   version number, even if the entry doesn't have a version number -
     *   if the part from the slash on is 7 characters or more, add nothing,
     *   else add enough to pad it out to seven characters.  
     */
    ent = (tc_fnset_entry *)t3malloc(sizeof(tc_fnset_entry) + len
                                     + (sl_len < 7 ? 7 - sl_len : 0));
    if (ent == 0)
        err_throw(TCERR_CODEGEN_NO_MEM);

    /* copy the name into the entry */
    memcpy(ent->nm, nm, len);
    ent->nm[len] = '\0';
    
    /* link the entry in at the end of the list */
    ent->nxt = 0;
    if (fnset_tail_ != 0)
        fnset_tail_->nxt = ent;
    else
        fnset_head_ = ent;
    fnset_tail_ = ent;

    /* count the entry, returning the index of the entry in the list */
    return fnset_cnt_++;
}

/*
 *   get a function set's name given its index 
 */
const char *CTcGenTarg::get_fnset_name(int idx) const
{
    tc_fnset_entry *ent;

    /* scan the linked list to find the given index */
    for (ent = fnset_head_ ; idx != 0 && ent != 0 ; ent = ent->nxt, --idx) ;

    /* return the one we found */
    return ent->nm;
}

/*
 *   Determine if we can skip an opcode because it is unreachable from the
 *   previous instruction.  
 */
int CTcGenTarg::can_skip_op()
{
    /* 
     *   if the previous instruction was a return or throw of some kind,
     *   we can skip any subsequent opcodes until a label is defined 
     */
    switch(last_op_)
    {
    case OPC_RET:
    case OPC_RETVAL:
    case OPC_RETTRUE:
    case OPC_RETNIL:
    case OPC_THROW:
    case OPC_JMP:
    case OPC_LRET:
        /* it's a return, throw, or jump - this new op is unreachable */
        return TRUE;

    default:
        /* this new op is reachable */
        return FALSE;
    }
}

/*
 *   Remove the last JMP instruction 
 */
void CTcGenTarg::remove_last_jmp()
{
    /* a JMP instruction is three bytes long, so back up three bytes */
    G_cs->dec_ofs(3);
}

/*
 *   Add a line record 
 */
void CTcGenTarg::add_line_rec(CTcTokFileDesc *file, long linenum)
{
    /* include line records only in debug mode */
    if (G_debug)
    {
        /* 
         *   clear the peephole, to ensure that the line boundary isn't
         *   blurred by code optimization 
         */
        clear_peephole();

        /* add the record to the code stream */
        G_cs->add_line_rec(file, linenum);
    }
}

/*
 *   Write an opcode to the output stream.  We'll watch for certain
 *   combinations of opcodes being generated, and apply peephole
 *   optimization when we see sequences that can be collapsed to more
 *   efficient single instructions.  
 */
void CTcGenTarg::write_op(uchar opc)
{
    int prv_len;
    int op_len;

    /* write the new opcode byte to the output stream */
    G_cs->write((char)opc);

    /* we've only written one byte so far for the current instruction */
    op_len = 1;

    /* presume the previous instruction length is just one byte */
    prv_len = 1;

    /* 
     *   check for pairs of instructions that we can reduce to more
     *   efficient single instructions 
     */
try_combine:
    switch(opc)
    {
    case OPC_DUP:
        /* combine GETR0 + DUP -> DUPR0 */
        if (last_op_ == OPC_GETR0)
        {
            opc = OPC_DUPR0;
            goto combine;
        }
        break;

    case OPC_DISC:
        /* combine DISC+DISC -> DISC1<2> */
        if (last_op_ == OPC_DISC)
        {
            /* 
             *   Switch the old DISC to DISC1<2>.  We're deleting a one-byte
             *   op and replacing a one-byte op with a two-byte op, so we're
             *   going from two bytes to two bytes total - no net deletion
             *   (op_len = 0).  The new DISC1 is a two-byte instruction
             *   (prv_len).  
             */
            op_len = 0;
            prv_len = 2;
            opc = OPC_DISC1;

            /* write the <2> operand, overwriting the second DISC */
            G_cs->write_at(G_cs->get_ofs() - 1, 2);

            /* go replace the op */
            goto combine;
        }
        break;

    case OPC_DISC1:
        /* combine DISC1<n> + DISC -> DISC1<n+1> */
        if (last_op_ == OPC_DISC
            && (uchar)G_cs->get_byte_at(G_cs->get_ofs() - 2) != 0xff)
        {
            /* up the DISC1 operand */
            G_cs->write_at(G_cs->get_ofs() - 2,
                           G_cs->get_byte_at(G_cs->get_ofs() - 2) + 1);

            /* keep the DISC1 */
            opc = OPC_DISC1;
            prv_len = 2;

            /* apply the combination */
            goto combine;
        }
        break;

    case OPC_JF:
        /* 
         *   if the last instruction was a comparison, we can use the
         *   opposite compare-and-jump instruction 
         */
        switch(last_op_)
        {
        case OPC_NOT:
            /* invert the sense of the test */
            opc = OPC_JT;
            goto combine;

        combine:
            /* 
             *   delete the new opcode we wrote, since we're going to combine
             *   it with the preceding opcode 
             */
            G_cs->dec_ofs(op_len);

            /* overwrite the preceding opcode with the new combined opcode */
            G_cs->write_at(G_cs->get_ofs() - prv_len, opc);

            /* roll back our internal peephole */
            last_op_ = second_last_op_;
            second_last_op_ = OPC_NOP;

            /* 
             *   we've deleted our own opcode, so the current (most recent)
             *   instruction in the output stream has the length of the
             *   current opcode 
             */
            op_len = prv_len;

            /* presume the previous opcode is one byte again */
            prv_len = 1;

            /* 
             *   go back for another try, since we may be able to do a
             *   three-way combination (for example, GT/NOT/JT would
             *   change to GT/JF, which would in turn change to JLE) 
             */
            goto try_combine;

        case OPC_EQ:
            opc = OPC_JNE;
            goto combine;

        case OPC_NE:
            opc = OPC_JE;
            goto combine;
            
        case OPC_LT:
            opc = OPC_JGE;
            goto combine;
            
        case OPC_LE:
            opc = OPC_JGT;
            goto combine;
            
        case OPC_GT:
            opc = OPC_JLE;
            goto combine;
            
        case OPC_GE:
            opc = OPC_JLT;
            goto combine;

        case OPC_GETR0:
            opc = OPC_JR0F;
            goto combine;
        }
        break;

    case OPC_JE:
        /* 
         *   if we just pushed nil, convert the PUSHNIL + JE to JNIL, since
         *   we simply want to jump if a value is nil 
         */
        if (last_op_ == OPC_PUSHNIL)
        {
            /* convert it to a jump-if-nil */
            opc = OPC_JNIL;
            goto combine;
        }
        break;

    case OPC_JNE:
        /* if we just pushed nil, convert to JNOTNIL */
        if (last_op_ == OPC_PUSHNIL)
        {
            /* convert to jump-if-not-nil */
            opc = OPC_JNOTNIL;
            goto combine;
        }
        break;

    case OPC_JT:
        /* 
         *   if the last instruction was a comparison, we can use a
         *   compare-and-jump instruction 
         */
        switch(last_op_)
        {
        case OPC_NOT:
            /* invert the sense of the test */
            opc = OPC_JF;
            goto combine;
            
        case OPC_EQ:
            opc = OPC_JE;
            goto combine;

        case OPC_NE:
            opc = OPC_JNE;
            goto combine;

        case OPC_LT:
            opc = OPC_JLT;
            goto combine;

        case OPC_LE:
            opc = OPC_JLE;
            goto combine;

        case OPC_GT:
            opc = OPC_JGT;
            goto combine;

        case OPC_GE:
            opc = OPC_JGE;
            goto combine;

        case OPC_GETR0:
            opc = OPC_JR0T;
            goto combine;
        }
        break;

    case OPC_NOT:
        /* 
         *   If the previous instruction was a comparison test of some
         *   kind, we can invert the sense of the test.  If the previous
         *   instruction was a BOOLIZE op, we can eliminate it entirely,
         *   because the NOT will perform the same conversion before
         *   negating the value.  If the previous was a NOT, we're
         *   inverting an inversion; we can simply perform a single
         *   BOOLIZE to get the same effect.  
         */
        switch(last_op_)
        {
        case OPC_EQ:
            opc = OPC_NE;
            goto combine;

        case OPC_NE:
            opc = OPC_EQ;
            goto combine;

        case OPC_GT:
            opc = OPC_LE;
            goto combine;

        case OPC_GE:
            opc = OPC_LT;
            goto combine;

        case OPC_LT:
            opc = OPC_GE;
            goto combine;

        case OPC_LE:
            opc = OPC_GT;
            goto combine;

        case OPC_BOOLIZE:
            opc = OPC_NOT;
            goto combine;

        case OPC_NOT:
            opc = OPC_BOOLIZE;
            goto combine;
        }
        break;

    case OPC_RET:
        /* 
         *   If we're writing a return instruction immediately after
         *   another return instruction, we can skip the additional
         *   instruction, since it will never be reached.  This case
         *   typically arises only when we generate the catch-all RET
         *   instruction at the end of a function. 
         */
        switch(last_op_)
        {
        case OPC_RET:
        case OPC_RETVAL:
        case OPC_RETNIL:
        case OPC_RETTRUE:
            /* simply suppress this additional RET instruction */
            return;
        }
        break;

    case OPC_RETNIL:
        /* we don't need to write two RETNIL's in a row */
        if (last_op_ == OPC_RETNIL)
            return;
        break;

    case OPC_RETTRUE:
        /* we don't need to write two RETTRUE's in a row */
        if (last_op_ == OPC_RETTRUE)
            return;
        break;

    case OPC_RETVAL:
        /* check the last opcode */
        switch(last_op_)
        {
        case OPC_GETR0:
            /* 
             *   if we just pushed R0 onto the stack, we can compress the
             *   GETR0 + RETVAL sequence into a simple RET, since RET leaves
             *   the R0 value unchanged 
             */
            opc = OPC_RET;
            goto combine;

        case OPC_PUSHNIL:
            /* PUSHNIL + RET can be converted to RETNIL */
            opc = OPC_RETNIL;
            goto combine;

        case OPC_PUSHTRUE:
            /* PUSHTRUE + RET can be converted to RETTRUE */
            opc = OPC_RETTRUE;
            goto combine;
        }
        break;

    case OPC_SETLCL1:
        /* combine GETR0 + SETLCL1 -> SETLCL1R0 */
        if (last_op_ == OPC_GETR0)
        {
            /* generate a combined SETLCL1R0 */
            opc = OPC_SETLCL1R0;
            goto combine;
        }

        /* combine DUP + SETLCL1 -> GETSETLCL1 */
        if (last_op_ == OPC_DUP)
        {
            opc = OPC_GETSETLCL1;
            goto combine;
        }

        /* combine DUPR0 + SETLCL1 -> GETSETLCL1R0 */
        if (last_op_ == OPC_DUPR0)
        {
            opc = OPC_GETSETLCL1R0;
            goto combine;
        }
        break;

        break;

    case OPC_GETPROP:
        /* check the previous instruction for combination possibilities */
        switch(last_op_)
        {
        case OPC_GETLCL1:
            /* get property of one-byte-addressable local */
            opc = OPC_GETPROPLCL1;

            /* overwrite the preceding two-byte instruction */
            prv_len = 2;
            goto combine;

        case OPC_GETLCLN0:
        case OPC_GETLCLN1:
        case OPC_GETLCLN2:
        case OPC_GETLCLN3:
        case OPC_GETLCLN4:
        case OPC_GETLCLN5:
            /* add a byte to pad things to the same length as a GETLCL1 */
            G_cs->write(0);

            /* write the local number operand of the GETPROPLCL1 */
            G_cs->write_at(G_cs->get_ofs() - 2, last_op_ - OPC_GETLCLN0);

            /* get property of one-byte-addressable local */
            opc = OPC_GETPROPLCL1;

            /* overwrite the preceding now-two-byte instruction */
            prv_len = 2;
            goto combine;

        case OPC_GETR0:
            /* get property of R0 */
            opc = OPC_GETPROPR0;
            goto combine;
        }
        break;

    case OPC_CALLPROP:
        /* check the previous instruction */
        switch(last_op_)
        {
        case OPC_GETR0:
            /* call property of R0 */
            opc = OPC_CALLPROPR0;
            goto combine;
        }
        break;

    case OPC_INDEX:
        /* we can combine small integer constants with INDEX */
        switch(last_op_)
        {
        case OPC_PUSH_0:
        case OPC_PUSH_1:
            /* 
             *   We can combine these into IDXINT8, but we must write an
             *   extra byte for the index value.  Go back and plug in the
             *   extra index value byte, and add another byte at the end of
             *   the stream to compensate for the insertion.  (We're just
             *   going to remove and overwrite everything after the inserted
             *   byte, so don't bother actually fixing up that part with real
             *   data; we merely need to make sure we have the right number
             *   of bytes in the stream.)  
             */
            G_cs->write_at(G_cs->get_ofs() - 1,
                           last_op_ == OPC_PUSH_0 ? 0 : 1);
            G_cs->write(0);

            /* combine the instructions */
            opc = OPC_IDXINT8;
            prv_len = 2;
            goto combine;

        case OPC_PUSHINT8:
            /* combine the PUSHINT8 + INDEX into IDXINT8 */
            opc = OPC_IDXINT8;
            prv_len = 2;
            goto combine;
        }
        break;

    case OPC_IDXINT8:
        /* we can replace GETLCL1 + IDXINT8 with IDXLCL1INT8 */
        if (last_op_ == OPC_GETLCL1)
        {
            /* add the index operand at the second byte after the GETLCL1 */
            uchar idx = G_cs->get_byte_at(G_cs->get_ofs() - op_len + 1);
            G_cs->write_at(G_cs->get_ofs() - op_len, idx);

            /* 
             *   we're going from GETLCL1 <l> IDXINT8 <i> (4 bytes) to
             *   IDXLCL1INT8 <l> <i> (3 bytes), for a net deletion of one
             *   byte - but that's only if we've already written the <i>
             *   operand, so the net deletion is actually op_len-1
             */
            op_len -= 1;

            /* go back and combine into what's now a three-byte opcode */
            opc = OPC_IDXLCL1INT8;
            prv_len = 3;
            goto combine;
        }
        if (last_op_ >= OPC_GETLCLN0 && last_op_ <= OPC_GETLCLN5)
        {
            /* write the local# at the byte after the GETLCLNx */
            G_cs->write_at(G_cs->get_ofs() - op_len, last_op_ - OPC_GETLCLN0);

            /* 
             *   if we haven't already written the IDXINT8 operand, write a
             *   placeholder for it, since 'combine:' can't add bytes 
             */
            if (op_len == 1)
                G_cs->write(0);

            /* 
             *   We're going from GETLCLNx IDXINT8 <i> (3 bytes) to
             *   IDXLCL1INT8 <l> <i> (3 bytes), so there's no net deletion.
             */
            op_len = 0;

            /* go back and combine into the three-byte index-by-local op */
            opc = OPC_IDXLCL1INT8;
            prv_len = 3;
            goto combine;
        }
        break;

    case OPC_SETIND:
        /* we can replace GETLCL1 + <small int> + SETIND with SETINDLCL1I8 */
        if (second_last_op_ == OPC_GETLCL1
            || (second_last_op_ >= OPC_GETLCLN0
                && second_last_op_ <= OPC_GETLCLN5))
        {
            int idx, lcl;
            int getlcl_len, push_len;

            /* get the local number and the GETLCL instruction length */
            if (second_last_op_ == OPC_GETLCL1)
            {
                /* it's GETLCL1 <n> - two bytes */
                getlcl_len = 2;
            }
            else
            {
                /* no operands - the local is encoded in the opcode itself */
                lcl = second_last_op_ - OPC_GETLCLN0;
                getlcl_len = 1;
            }

            /* check the middle opcode for an int push */
            switch(last_op_)
            {
            case OPC_PUSHINT8:
                /* get the index from the PUSHINT8 */
                idx = (int)(uchar)G_cs->get_byte_at(G_cs->get_ofs() - 2);
                push_len = 2;

                /* get the local, if it's a GETLCL1 */
                if (second_last_op_ == OPC_GETLCL1)
                    lcl = G_cs->get_byte_at(G_cs->get_ofs() - push_len - 2);

                /* 
                 *   PUSHINT8 pushes a signed value, but SETINDLCL1I8 uses an
                 *   unsigned value, so make sure the PUSHINT8 value is in
                 *   range 
                 */
                if (idx < 127)
                    goto combine_setind;

                /* not combinable */
                break;

            case OPC_PUSHINT:
                /* get the int value */
                idx = G_cs->read4_at(G_cs->get_ofs() - 5);
                push_len = 5;

                /* get the local, if it's a GETLCL1 */
                if (second_last_op_ == OPC_GETLCL1)
                    lcl = G_cs->get_byte_at(G_cs->get_ofs() - push_len - 2);

                /* if the index is 0-255, we can combine the instructions */
                if (idx >= 0 && idx <= 255)
                    goto combine_setind;

                /* not combinable */
                break;

            case OPC_PUSH_0:
            case OPC_PUSH_1:
                /* it's a one-byte PUSH */
                idx = last_op_ - OPC_PUSH_0;
                push_len = 1;

                /* get the local, if it's a GETLCL1 */
                if (second_last_op_ == OPC_GETLCL1)
                    lcl = G_cs->get_byte_at(G_cs->get_ofs() - push_len - 2);

            combine_setind:
                /* write the operands of the SETINDLCLI8 */
                G_cs->write_at(
                    G_cs->get_ofs() - getlcl_len - push_len, (uchar)lcl);
                G_cs->write_at(
                    G_cs->get_ofs() - getlcl_len - push_len + 1, (uchar)idx);

                /* 
                 *   Go back and do the combine.  Remove GETLCL<var>,
                 *   PUSH<idx>, and SETIND (getlcl_len + push_len + 1 bytes),
                 *   and replace them with SETINDLCL1I8<var><idx> (3 bytes).
                 *   The net deletion goes in op_len, and the new length goes
                 *   in prv_len.  Also, since we're combining three
                 *   instructions into one, we're losing our second-to-last
                 *   opcode.  
                 */
                opc = OPC_SETINDLCL1I8;
                second_last_op_ = OPC_NOP;
                op_len = getlcl_len + push_len - 2;
                prv_len = 3;
                goto combine;
            }
        }
        break;

    default:
        /* write this instruction as-is */
        break;
    }
    
    /* remember the last opcode we wrote */
    second_last_op_ = last_op_;
    last_op_ = opc;
}

/*
 *   Write a CALLPROP instruction, combining with preceding opcodes if
 *   possible.  
 */
void CTcGenTarg::write_callprop(int argc, int varargs, vm_prop_id_t prop)
{
    /* 
     *   if the previous instruction was GETLCL1, combine it with the
     *   CALLPROP to form a single CALLPROPLCL1 instruction 
     */
    if (last_op_ == OPC_GETLCL1
        || (last_op_ >= OPC_GETLCLN0 && last_op_ <= OPC_GETLCLN5))
    {
        uchar lcl;

        /* if it was a GETLCLNx, expand it out to a GETLCL1 */
        if (last_op_ == OPC_GETLCL1)
        {
            /* it's already a GETLCL1 - just get the index */
            lcl = G_cs->get_byte_at(G_cs->get_ofs() - 1);

            /* back up and delete the GETLCL1 instruction */
            G_cs->dec_ofs(2);
        }
        else
        {
            /* it's a GETLCLNx - infer the local number from the opcode */
            lcl = last_op_ - OPC_GETLCLN0;

            /* delete the instruction */
            G_cs->dec_ofs(1);
        }

        /* roll back the peephole for the instruction deletion */
        last_op_ = second_last_op_;
        second_last_op_ = OPC_NOP;

        /* write the varargs modifier if appropriate */
        if (varargs)
            write_op(OPC_VARARGC);

        /* write the CALLPROPLCL1 */
        write_op(OPC_CALLPROPLCL1);
        G_cs->write((char)argc);
        G_cs->write(lcl);
        G_cs->write_prop_id(prop);
    }
    else
    {
        /* generate the varargs modifier if appropriate */
        if (varargs)
            write_op(OPC_VARARGC);

        /* we have arguments - generate a CALLPROP */
        write_op(OPC_CALLPROP);
        G_cs->write((char)argc);
        G_cs->write_prop_id(prop);
    }

    /* callprop removes arguments and the object */
    note_pop(argc + 1);
}

/*
 *   Note a string's length 
 */
void CTcGenTarg::note_str(size_t len)
{
    /* if it's the longest so far, remember it */
    if (len > max_str_len_)
    {
        /* 
         *   flag an warning the length plus overhead would exceed 32k
         *   (only do this the first time we cross this limit) 
         */
        if (len > (32*1024 - VMB_LEN)
            && max_str_len_ <= (32*1024 - VMB_LEN))
            G_tok->log_warning(TCERR_CONST_POOL_OVER_32K);

        /* remember the length */
        max_str_len_ = len;
    }
}

/*
 *   note a list's length 
 */
void CTcGenTarg::note_list(size_t element_count)
{
    /* if it's the longest list so far, remember it */
    if (element_count > max_list_cnt_)
    {
        /* flag a warning if the stored length would be over 32k */
        if (element_count > ((32*1024 - VMB_LEN) / VMB_DATAHOLDER)
            && max_list_cnt_ <= ((32*1024 - VMB_LEN) / VMB_DATAHOLDER))
            G_tok->log_warning(TCERR_CONST_POOL_OVER_32K);

        /* remember the length */
        max_list_cnt_ = element_count;
    }
}

/*
 *   Note a bytecode block length 
 */
void CTcGenTarg::note_bytecode(ulong len)
{
    /* if it's the longest bytecode block yet, remember it */
    if (len > max_bytecode_len_)
    {
        /* flag a warning the first time we go over 32k */
        if (len >= 32*1024 && max_bytecode_len_ < 32*1024)
            G_tok->log_warning(TCERR_CODE_POOL_OVER_32K);
        
        /* remember the new length */
        max_bytecode_len_ = len;
    }
}

/*
 *   Hash table entry for an interned string.  This keeps track of a string
 *   that we've previously generated to the data segment, for reuse if we
 *   need to generate another copy of the same string literal.  
 */
class CInternEntry: public CVmHashEntryCS
{
public:
    CInternEntry(const char *str, size_t len, CTcStreamAnchor *anchor)
        : CVmHashEntryCS(str, len, FALSE)
    {
        anchor_ = anchor;
    }

    /* the string's fixup list anchor */
    CTcStreamAnchor *anchor_;
};

/*
 *   Add a string to the constant pool 
 */
void CTcGenTarg::add_const_str(const char *str, size_t len,
                               CTcDataStream *ds, ulong ofs)
{
    /* 
     *   If it's a short string, look to see if we've already written it.  If
     *   so, we can reuse the previous copy.  The odds of a string being
     *   duplicated generally decrease rapidly with the length of the string,
     *   and the cost of checking for an old copy increases, so don't bother
     *   if the string is over a threshold length.  
     */
    const size_t intern_threshold = 40;
    if (len < intern_threshold)
    {
        /* look for an old copy */
        CInternEntry *e = (CInternEntry *)strtab_->find(str, len);

        /* 
         *   If we found it, simply add a fixup for the new use to the
         *   existing fixup list.  This will cause the caller to point the
         *   new literal to the existing copy, without generating a duplicate
         *   copy in the data segment.  
         */
        if (e != 0)
        {
            /* 
             *   add a fixup to the existing string's fixup list for the
             *   caller's new reference 
             */
            CTcAbsFixup::add_abs_fixup(e->anchor_->fixup_list_head_, ds, ofs);

            /* we're done */
            return;
        }
    }

    /* 
     *   Add an anchor for the item, and add a fixup for the reference
     *   from ds@ofs to the item. 
     */
    CTcStreamAnchor *anchor = G_ds->add_anchor(0, 0);
    CTcAbsFixup::add_abs_fixup(anchor->fixup_list_head_, ds, ofs);

    /*
     *   If this is a short string, add it to our list of interned strings.
     *   This will let us reuse this copy of the string in the data segment
     *   if we need to generate another literal later with identical
     *   contents.  
     */
    if (len < intern_threshold)
        strtab_->add(new CInternEntry(str, len, anchor));

    /* write the length prefix */
    G_ds->write2(len);

    /* write the string bytes */
    G_ds->write(str, len);

    /* note the length of the string stored */
    note_str(len);
}

/*
 *   Add a list to the constant pool 
 */
void CTcGenTarg::add_const_list(CTPNList *lst,
                                CTcDataStream *ds, ulong ofs)
{
    int i;
    CTPNListEle *cur;
    ulong dst;
    CTcStreamAnchor *anchor;

    /* 
     *   Add an anchor for the item, and add a fixup for the reference
     *   from ds@ofs to the item.  
     */
    anchor = G_ds->add_anchor(0, 0);
    CTcAbsFixup::add_abs_fixup(anchor->fixup_list_head_, ds, ofs);

    /* 
     *   Reserve space for the list.  We need to do this first, because
     *   the list might contain elements which themselves must be written
     *   to the data stream; we must therefore reserve space for the
     *   entire list before we start writing its elements. 
     */
    dst = G_ds->reserve(2 + lst->get_count()*VMB_DATAHOLDER);

    /* set the length prefix */
    G_ds->write2_at(dst, lst->get_count());
    dst += 2;

    /* store the elements */
    for (i = 0, cur = lst->get_head() ; cur != 0 ;
         ++i, cur = cur->get_next(), dst += VMB_DATAHOLDER)
    {
        CTcConstVal *ele;

        /* get this element */
        ele = cur->get_expr()->get_const_val();

        /* write it to the element buffer */
        write_const_as_dh(G_ds, dst, ele);
    }

    /* make sure the list wasn't corrupted */
    if (i != lst->get_count())
        G_tok->throw_internal_error(TCERR_CORRUPT_LIST);

    /* note the number of elements in the list */
    note_list(lst->get_count());
}

/*
 *   Generate a BigNumber object 
 */
vm_obj_id_t CTcGenTarg::gen_bignum_obj(
    const char *txt, size_t len, int promoted)
{
    /* if the value was promoted from integer due to overflow, warn */
    if (promoted)
        G_tok->log_warning(TCERR_INT_CONST_OV);

    /* write to BigNumber object stream */
    CTcDataStream *str = G_bignum_stream;

    /* generate a new object ID for the BigNumber */
    vm_obj_id_t id = new_obj_id();

    /* 
     *   add the object ID to the non-symbol object list - this is
     *   necessary to ensure that the object ID is fixed up during linking 
     */
    G_prs->add_nonsym_obj(id);

    /* 
     *   Get the BigNumber representation, inferring the radix.  The
     *   tokenizer can pass us integer strings with '0x' (hex) or '0' (octal)
     *   prefixes when the numbers they contain are too big for the 32-bit
     *   signed integer type.  Floating-point values never have a radix
     *   prefix.
     */
    size_t bnlen;
    char *bn = CVmObjBigNum::parse_str_alo(bnlen, txt, len, 0);

    /* write the OBJS header - object ID plus byte count for metaclass data */
    str->write_obj_id(id);
    str->write2(bnlen);

    /* write the BigNumber contents */
    str->write(bn, bnlen);

    /* free the BigNumber value */
    delete [] bn;

    /* return the new object ID */
    return id;
}

/*
 *   Generate a RexPattern object 
 */
vm_obj_id_t CTcGenTarg::gen_rexpat_obj(const char *txt, size_t len)
{
    /* write to RexPattern object stream */
    CTcDataStream *rs = G_rexpat_stream;

    /* generate a new object ID for the RexPattern object */
    vm_obj_id_t id = new_obj_id();

    /* add the object ID to the non-symbol object list */
    G_prs->add_nonsym_obj(id);

    /* 
     *   write the OBJS header - object ID plus byte count for metaclass data
     *   (a DATAHOLDER pointing to the constant string)
     */
    rs->write_obj_id(id);
    rs->write2(VMB_DATAHOLDER);

    /* 
     *   Add the source text as an ordinary constant string.  Generate its
     *   fixup in the RexPattern stream, in the DATAHOLDER we're about to
     *   write. 
     */
    add_const_str(txt, len, rs, rs->get_ofs() + 1);

    /* 
     *   Set up the constant string value.  Use zero as the pool offset,
     *   since the fixup for the constant string will fill this in when the
     *   pool layout is finalized. 
     */
    vm_val_t val;
    val.set_sstring(0);

    /* write the RexPattern data (a DATAHOLDER for the const string) */
    char buf[VMB_DATAHOLDER];
    vmb_put_dh(buf, &val);
    rs->write(buf, VMB_DATAHOLDER);

    /* return the new object ID */
    return id;
}

/*
 *   Convert a constant value from a CTcConstVal (compiler internal
 *   representation) to a vm_val_t (interpreter representation). 
 */
void CTcGenTarg::write_const_as_dh(CTcDataStream *ds, ulong ofs,
                                   const CTcConstVal *src)
{
    vm_val_t val;
    char buf[VMB_DATAHOLDER];

    /* convert according to the value's type */
    switch(src->get_type())
    {
    case TC_CVT_NIL:
        val.set_nil();
        break;

    case TC_CVT_TRUE:
        val.set_true();
        break;

    case TC_CVT_INT:
        val.set_int(src->get_val_int());
        break;

    case TC_CVT_FLOAT:
        /* generate the BigNumber object */
        val.set_obj(gen_bignum_obj(src->get_val_float(),
                                   src->get_val_float_len(),
                                   src->is_promoted()));

        /* add a fixup for the object ID */
        if (G_keep_objfixups)
            CTcIdFixup::add_fixup(&G_objfixup, ds, ofs + 1, val.val.obj);
        break;

    case TC_CVT_SSTR:
        /* 
         *   Store the string in the constant pool.  Note that our fixup
         *   is at the destination stream offset plus one, since the
         *   DATAHOLDER has the type byte followed by the offset value.  
         */
        add_const_str(src->get_val_str(), src->get_val_str_len(),
                      ds, ofs + 1);
        
        /* 
         *   set the offset to zero for now - the fixup that
         *   add_const_str() generates will take care of supplying the
         *   real value 
         */
        val.set_sstring(0);
        break;


    case TC_CVT_RESTR:
        /* generate the RexPattern object */
        val.set_obj(gen_rexpat_obj(src->get_val_str(),
                                   src->get_val_str_len()));

        /* add a fixup for the object ID */
        if (G_keep_objfixups)
            CTcIdFixup::add_fixup(&G_objfixup, ds, ofs + 1, val.val.obj);
        break;

    case TC_CVT_LIST:
        /* 
         *   Store the sublist in the constant pool.  Our fixup is at the
         *   destination stream offset plus one, since the DATAHOLDER has
         *   the type byte followed by the offset value.  
         */
        add_const_list(src->get_val_list(), ds, ofs + 1);

        /* 
         *   set the offset to zero for now - the fixup that
         *   add_const_list() generates will take care of supplying the
         *   real value 
         */
        val.set_list(0);
        break;

    case TC_CVT_OBJ:
        /* set the object ID value */
        val.set_obj((vm_obj_id_t)src->get_val_obj());

        /* 
         *   add a fixup (at the current offset plus one, for the type
         *   byte) if we're keeping object ID fixups 
         */
        if (G_keep_objfixups)
            CTcIdFixup::add_fixup(&G_objfixup, ds, ofs + 1,
                                  src->get_val_obj());
        break;

    case TC_CVT_ENUM:
        /* set the enum value */
        val.set_enum(src->get_val_enum());

        /* add a fixup */
        if (G_keep_enumfixups)
            CTcIdFixup::add_fixup(&G_enumfixup, ds, ofs + 1,
                                  src->get_val_enum());
        break;

    case TC_CVT_PROP:
        /* set the property ID value */
        val.set_propid((vm_prop_id_t)src->get_val_prop());

        /* 
         *   add a fixup (at the current offset plus one, for the type
         *   byte) if we're keeping property ID fixups 
         */
        if (G_keep_propfixups)
            CTcIdFixup::add_fixup(&G_propfixup, ds, ofs + 1,
                                  src->get_val_prop());
        break;

    case TC_CVT_FUNCPTR:
        /* 
         *   use a placeholder value of zero for now - a function's final
         *   address is never known until after all code generation has
         *   been completed (the fixup will take care of supplying the
         *   correct value when the time comes) 
         */
        val.set_fnptr(0);

        /* 
         *   Add a fixup.  The fixup is at the destination stream offset
         *   plus one, because the DATAHOLDER has a type byte followed by
         *   the function pointer value.  
         */
        src->get_val_funcptr_sym()->add_abs_fixup(ds, ofs + 1);
        break;

    case TC_CVT_ANONFUNCPTR:
        /* use a placeholder of zero for now, until we fix up the pointer */
        val.set_fnptr(0);

        /* add a fixup for the code body */
        src->get_val_anon_func_ptr()->add_abs_fixup(ds, ofs + 1);
        break;

    case TC_CVT_VOCAB_LIST:
        /* 
         *   it's an internal vocabulary list type - this is used as a
         *   placeholder only, and will be replaced during linking with an
         *   actual vocabulary string list 
         */
        val.typ = VM_VOCAB_LIST;
        break;

    case TC_CVT_UNK:
    case TC_CVT_BIFPTR:
        /* unknown - ignore it */
        break;
    }

    /* write the vm_val_t in DATA_HOLDER format into the stream */
    vmb_put_dh(buf, &val);
    ds->write_at(ofs, buf, VMB_DATAHOLDER);
}

/*
 *   Write a DATAHOLDER at the current offset in a stream
 */
void CTcGenTarg::write_const_as_dh(CTcDataStream *ds, 
                                   const CTcConstVal *src)
{
    /* write to the current stream offset */
    write_const_as_dh(ds, ds->get_ofs(), src);
}

/*
 *   Notify that parsing is finished 
 */
void CTcGenTarg::parsing_done()
{
    /* nothing special to do */
}


/*
 *   notify the code generator that we're replacing an object 
 */
void CTcGenTarg::notify_replace_object(ulong stream_ofs)
{
    uint flags;

    /* set the 'replaced' flag in the flags prefix */
    flags = G_os->read2_at(stream_ofs);
    flags |= TCT3_OBJ_REPLACED;
    G_os->write2_at(stream_ofs, flags);
}


/*
 *   Set the starting offset of the current method 
 */
void CTcGenTarg::set_method_ofs(ulong ofs)
{
    /* tell the exception table object about it */
    get_exc_table()->set_method_ofs(ofs);

    /* remember it in the code stream */
    G_cs->set_method_ofs(ofs);
}



/* ------------------------------------------------------------------------ */
/*
 *   Method header generator
 */

/*
 *   Open a method 
 */
void CTcGenTarg::open_method(
    CTcCodeStream *stream,
    CTcSymbol *fixup_owner_sym, CTcAbsFixup **fixup_list_head,
    CTPNCodeBody *code_body, CTcPrsSymtab *goto_tab,
    int argc, int opt_argc, int varargs,
    int is_constructor, int is_op_overload, int is_self_available,
    tct3_method_gen_ctx *ctx)
{
    /* set the code stream as the current code generator output stream */
    G_cs = ctx->stream = stream;

    /* set the method properties in the code generator */
    set_in_constructor(is_constructor);
    set_in_op_overload(is_op_overload);
    stream->set_self_available(is_self_available);

    /* we obviously can't combine any past instructions */
    clear_peephole();

    /* clear the old line records */
    stream->clear_line_recs();

    /* clear the old frame list */
    stream->clear_local_frames();

    /* clear the old exception table */
    get_exc_table()->clear_table();

    /* reset the stack depth counters */
    reset_sp_depth();

    /* there are no enclosing 'switch' or block statements yet */
    stream->set_switch(0);
    stream->set_enclosing(0);

    /* 
     *   remember where the method header starts - we'll need to come back
     *   and fix up some placeholder entries in "Close" 
     */
    ctx->method_ofs = stream->get_ofs();

    /* set the method start offset in the code generator */
    set_method_ofs(ctx->method_ofs);

    /* set up a fixup anchor for the new method */
    ctx->anchor = stream->add_anchor(fixup_owner_sym, fixup_list_head);

    /* set the anchor in the associated symbol, if applicable */
    if (fixup_owner_sym != 0)
        fixup_owner_sym->set_anchor(ctx->anchor);

    /* 
     *   Generate the function header.  At the moment, we don't know the
     *   stack usage, exception table offset, or debug record offset, since
     *   these all come after the byte code; we won't know how big the byte
     *   code is until after we generate it.  For now, write zero bytes as
     *   placeholders for these slots; we'll come back and fix them up to
     *   their real values after we've generated the byte code.  
     */
    stream->write(argc | (varargs ? 0x80 : 0));           /* argument count */
    stream->write((char)opt_argc);    /* additional optional argument count */
    stream->write2(0); /* number of locals - won't know until after codegen */
    stream->write2(0);      /* total stack - won't know until after codegen */
    stream->write2(0);             /* exception table offset - presume none */
    stream->write2(0);    /* debug record offset - presume no debug records */

    /*
     *   If the header size in the globals is greater than the fixed size we
     *   write, pad out the remainder with zero bytes. 
     */
    for (int i = G_sizes.mhdr - 10 ; i > 0 ; --i)
        stream->write(0);

    /* remember the starting offset of the code */
    ctx->code_start_ofs = stream->get_ofs();

    /* set the current code body being generated */
    ctx->old_code_body = stream->set_code_body(code_body);

    /* get the 'goto' symbol table for this function */
    stream->set_goto_symtab(goto_tab);
}

/*
 *   Close a method 
 */
void CTcGenTarg::close_method(int local_cnt,
                              CTcPrsSymtab *local_symtab,
                              CTcTokFileDesc *end_desc, long end_linenum,
                              tct3_method_gen_ctx *ctx,
                              CTcNamedArgTab *named_arg_tab_head)
{
    /* get the output code stream from the context */
    CTcCodeStream *stream = ctx->stream;
    
    /* 
     *   Generate a 'return' opcode with a default 'nil' return value - this
     *   will ensure that code that reaches the end of the procedure returns
     *   normally.  If this is a constructor, return the 'self' object rather
     *   than nil.
     *   
     *   We only need to generate this epilogue if the next instruction would
     *   be reachable.  If it's not reachable, then the code explicitly took
     *   care of all types of exits.  
     */
    if (!can_skip_op())
    {
        /* 
         *   add a line record for the implied return at the last source line
         *   of the code body 
         */
        add_line_rec(end_desc, end_linenum);

        /* write the appropriate return */
        if (is_in_constructor())
        {
            /* we're in a constructor - return 'self' */
            write_op(OPC_PUSHSELF);
            write_op(OPC_RETVAL);
        }
        else
        {
            /* 
             *   Normal method/function - return without a value (explicitly
             *   set R0 to nil, though, so we don't return something returned
             *   from a called function).  
             */
            write_op(OPC_RETNIL);
        }
    }

    /* generate any named argument tables */
    for (CTcNamedArgTab *nt = named_arg_tab_head ; nt != 0 ; nt = nt->nxt)
        nt->generate();

    /* 
     *   release labels allocated for the code block; this will log an error
     *   if any labels are undefined 
     */
    stream->release_labels();

    /* 
     *   Eliminate jump-to-jump sequences in the generated code.  Don't
     *   bother if we've found any errors, as the generated code will not
     *   necessarily be valid if this is the case.  
     */
    if (G_tcmain->get_error_count() == 0)
        remove_jumps_to_jumps(stream, ctx->code_start_ofs);

    /* note the code block's end point */
    ctx->code_end_ofs = stream->get_ofs();

    /*
     *   Fix up the local variable count in the function header.  We might
     *   allocate extra locals for internal use while generating code, so we
     *   must wait until after generating our code before we know the final
     *   local count.  
     */
    stream->write2_at(ctx->method_ofs + 2, local_cnt);

    /* 
     *   Fix up the total stack space indicator in the function header.  The
     *   total stack size must include the locals, as well as stack space
     *   needed for intermediate computations.  
     */
    stream->write2_at(ctx->method_ofs + 4, get_max_sp_depth() + local_cnt);

    /*
     *   Before writing the exception table, mark the endpoint of the range
     *   of the main local scope and any enclosing scopes.  
     */
    for (CTcPrsSymtab *t = local_symtab ; t != 0 ; t = t->get_parent())
        t->add_to_range(G_cs->get_ofs() - ctx->method_ofs);

    /* 
     *   Generate the exception table, if we have one.  If we have no
     *   exception records, leave the exception table offset set to zero to
     *   indicate that there is no exception table for the method.  
     */
    if (get_exc_table()->get_entry_count() != 0)
    {
        /* 
         *   write the exception table offset - it's at the current offset in
         *   the code 
         */
        stream->write2_at(ctx->method_ofs + 6,
                          stream->get_ofs() - ctx->method_ofs);

        /* write the table */
        get_exc_table()->write_to_code_stream();
    }
}

/*
 *   Clean up after generating a method 
 */
void CTcGenTarg::close_method_cleanup(tct3_method_gen_ctx *ctx)
{
    /*
     *   Tell the code generator our code block byte length so that it can
     *   keep track of the longest single byte-code block; it will use this
     *   to choose the code pool page size when generating the image file.  
     */
    note_bytecode(ctx->anchor->get_len(ctx->stream));

    /* we're no longer in a constructor, if we ever were */
    set_in_constructor(FALSE);

    /* likewise an operator overload method */
    set_in_op_overload(FALSE);

    /* clear the current code body */
    ctx->stream->set_code_body(ctx->old_code_body);

    /* always leave the main code stream active by default */
    G_cs = G_cs_main;
}

/* ------------------------------------------------------------------------ */
/*
 *   Do post-CALL cleanup 
 */
void CTcGenTarg::post_call_cleanup(const CTcNamedArgs *named_args)
{
    /* write the named argument table pointer, if necessary */
    if (named_args != 0 && named_args->cnt != 0)
    {
        /* 
         *   We need a named argument table.  Add the table to the curren
         *   code body, which will generate a label for us to use to
         *   reference the table.  
         */
        CTcCodeLabel *tab_lbl = G_cs->get_code_body()
                                ->add_named_arg_tab(named_args);

        /* write the op: NamedArgPtr <named arg cnt>, <table offset> */
        G_cg->write_op(OPC_NAMEDARGPTR);
        G_cs->write((char)named_args->cnt);
        G_cs->write_ofs2(tab_lbl, 0);

        /* NamedArgPtr pops the arguments when executed */
        G_cg->note_pop(named_args->cnt);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Run through the generated code stream starting at the given offset (and
 *   running up to the current offset), and eliminate jump-to-jump sequences:
 *   
 *   - whenever we find any jump instruction that points directly to an
 *   unconditional jump, we'll change the first jump so that it points to
 *   the target of the second jump, saving the unnecessary stop at the
 *   intermediate jump
 *   
 *   - whenever we find an unconditional jump to any return or throw
 *   instruction, we'll replace the jump with a copy of the target
 *   return/throw instruction.  
 */
void CTcGenTarg::remove_jumps_to_jumps(CTcCodeStream *str, ulong start_ofs)
{
    ulong ofs;
    ulong end_ofs;
    uchar prv_op;
    
    /* 
     *   scan the code stream starting at the given offset, continuing
     *   through the current offset 
     */
    prv_op = OPC_NOP;
    for (ofs = start_ofs, end_ofs = str->get_ofs() ; ofs < end_ofs ; )
    {
        uchar op;
        ulong target_ofs;
        ulong orig_target_ofs;
        uchar target_op;
        int done;
        int chain_len;
        
        /* check the byte code instruction at the current location */
        switch(op = str->get_byte_at(ofs))
        {
        case OPC_RETVAL:
            /* 
             *   If our previous opcode was PUSHTRUE or PUSHNIL, we can
             *   replace the previous opcode with RETTRUE or RETNIL.  This
             *   sequence can occur when we generate conditional code that
             *   returns a value; in such cases, we sometimes can't elide
             *   the PUSHx/RETVAL sequence during the original code
             *   generation because the RETVAL itself is the target of a
             *   label and thus must be retained as a separate instruction.
             *   Converting the PUSHTRUE or PUSHNIL here won't do any harm,
             *   as we'll still leave the RETVAL as a separate instruction.
             *   Likewise, if the previous instruction was GET_R0, we can
             *   change it to a simple RET.  
             */
            switch(prv_op)
            {
            case OPC_PUSHTRUE:
                /* convert the PUSHTRUE to a RETTRUE */
                str->write_at(ofs - 1, OPC_RETTRUE);
                break;

            case OPC_PUSHNIL:
                /* convert the PUSHNIL to a RETNIL */
                str->write_at(ofs - 1, OPC_RETNIL);
                break;

            case OPC_GETR0:
                /* convert the GETR0 to a RET */
                str->write_at(ofs - 1, OPC_RET);
                break;
            }

            /* skip the RETVAL */
            ofs += 1;
            break;
            
        case OPC_PUSHSTRI:
            /* 
             *   push in-line string: we have a UINT2 operand giving the
             *   length in bytes of the string, followed by the bytes of the
             *   string, so read the uint2 and then skip that amount plus
             *   three additional bytes (one for the opcode, two for the
             *   uint2 itself) 
             */
            ofs += 3 + str->readu2_at(ofs+1);
            break;

        case OPC_SWITCH:
            /* 
             *   Switch: we have a UINT2 giving the number of cases,
             *   followed by the cases, followed by an INT2; each case
             *   consists of a DATAHOLDER plus a UINT2, for a total of 7
             *   bytes.  The total is thus 5 bytes (the opcode, the case
             *   count UINT2, the final INT2) plus 7 bytes times the number
             *   of cases.  
             */
            ofs += 1 + 2 + (VMB_DATAHOLDER + 2)*str->readu2_at(ofs+1) + 2;
            break;

        case OPC_NAMEDARGTAB:
            /* 
             *   NamedArgTab - the first operand is a UINT2 giving the length
             *   of the table 
             */
            ofs += 1 + 2 + str->readu2_at(ofs+1);
            break;

        case OPC_JMP:
            /*
             *   Unconditional jump: check for a jump to a RETURN of any
             *   kind or a THROW.  If the destination consists of either of
             *   those, replace the JMP with the target instruction.  If the
             *   destination is an unconditional JMP, iteratively check its
             *   destination.  
             */
            orig_target_ofs = target_ofs = ofs + 1 + str->read2_at(ofs + 1);

            /* 
             *   Iterate through any chain of JMP's we find.  Abort if we
             *   try following a chain longer than 20 jumps, in case we
             *   should encounter any circular chains. 
             */
            for (done = FALSE, chain_len = 0 ; !done && chain_len < 20 ;
                 ++chain_len)
            {
                switch(target_op = str->get_byte_at(target_ofs))
                {
                case OPC_RETVAL:
                    /*
                     *   Check for a special sequence that we can optimize
                     *   even better than the usual.  If we have a GETR0
                     *   followed by a JMP to a RETVAL, then we can
                     *   eliminate the JMP *and* the GETR0, and just convert
                     *   the GETR0 to a RET.
                     */
                    if (prv_op == OPC_GETR0)
                    {
                        /* 
                         *   The GETR0 is the byte before the original JMP:
                         *   simply replace it with a RET.  Note that we can
                         *   leave the original jump intact, in case anyone
                         *   else is pointing to it.  
                         */
                        str->write_at(ofs - 1, OPC_RET);

                        /* we're done iterating the chain of jumps-to-jumps */
                        done = TRUE;
                    }
                    else
                    {
                        /* handle the same as any return instruction */
                        goto any_RET;
                    }
                    break;

                case OPC_RETNIL:
                case OPC_RETTRUE:
                case OPC_RET:
                case OPC_THROW:
                any_RET:
                    /* 
                     *   it's a THROW or RETURN of some kind - simply copy
                     *   it to the current slot; write NOP's over our jump
                     *   offset operand, to make sure the code continues to
                     *   be deterministically readable 
                     */
                    str->write_at(ofs, target_op);
                    str->write_at(ofs + 1, (uchar)OPC_NOP);
                    str->write_at(ofs + 2, (uchar)OPC_NOP);

                    /* we're done iterating the chain of jumps-to-jumps */
                    done = TRUE;
                    break;

                case OPC_JMP:
                    /* 
                     *   We're jumping to another jump - there's no reason
                     *   to stop at the intermediate jump instruction, since
                     *   we can simply jump directly to the destination
                     *   address.  Calculate the new target address, and
                     *   continue iterating, in case this jumps to something
                     *   we can further optimize away.  
                     */
                    target_ofs = target_ofs + 1
                                 + str->read2_at(target_ofs + 1);

                    /* 
                     *   if it's a jump to the original location, it must be
                     *   some unreachable code that generated a circular
                     *   jump; ignore it in this case 
                     */
                    if (target_ofs == ofs)
                        done = TRUE;

                    /* proceed */
                    break;

                default:
                    /* 
                     *   For anything else, we're done with any chain of
                     *   jumps to jumps.  If we indeed found a new target
                     *   address, rewrite the original JMP instruction so
                     *   that it jumps directly to the end of the chain
                     *   rather than going through the intermediate jumps.  
                     */
                    if (target_ofs != orig_target_ofs)
                        str->write2_at(ofs + 1, target_ofs - (ofs + 1));

                    /* we're done iterating */
                    done = TRUE;
                    break;
                }
            }

            /* whatever happened, skip past the jump */
            ofs += 3;

            /* done */
            break;

        case OPC_JT:
        case OPC_JF:
        case OPC_JE:
        case OPC_JNE:
        case OPC_JGT:
        case OPC_JGE:
        case OPC_JLT:
        case OPC_JLE:
        case OPC_JST:
        case OPC_JSF:
        case OPC_JNIL:
        case OPC_JNOTNIL:
        case OPC_JR0T:
        case OPC_JR0F:
            /*
             *   We have a jump (conditional or otherwise).  Check the
             *   target instruction to see if it's an unconditional jump; if
             *   so, then we can jump straight to the target of the second
             *   jump, since there's no reason to stop at the intermediate
             *   jump instruction on our way to the final destination.  Make
             *   this check iteratively, so that we eliminate any chain of
             *   jumps to jumps and land at our final non-jump instruction
             *   in one go. 
             */
            orig_target_ofs = target_ofs = ofs + 1 + str->read2_at(ofs + 1);
            for (done = FALSE, chain_len = 0 ; !done && chain_len < 20 ;
                 ++chain_len)
            {
                uchar target_op;

                /* get the target opcode */
                target_op = str->get_byte_at(target_ofs);

                /* 
                 *   if the target is an unconditional JMP, we can retarget
                 *   the original instruction to jump directly to the target
                 *   of the target JMP, bypassing the target JMP entirely and
                 *   thus avoiding some unnecessary work at run-time 
                 */
                if (target_op == OPC_JMP)
                {
                    /* 
                     *   retarget the original jump to go directly to the
                     *   target of the target JMP 
                     */
                    target_ofs = target_ofs + 1
                                 + str->read2_at(target_ofs + 1);
                    
                    /* 
                     *   continue scanning for more opportunities, as the new
                     *   target could also point to something we can bypass 
                     */
                    continue;
                }
                
                /*
                 *   Certain combinations are special.  If the original
                 *   opcode was a JST or JSF, and the target is a JT or JF,
                 *   we can recode the sequence so that the original opcode
                 *   turns into a more efficient JT or JF and jumps directly
                 *   past the JT or JF.  If we have a JST or JSF jumping to a
                 *   JST or JSF, we can also recode that sequence to bypass
                 *   the second jump.  In both cases, we can recode the
                 *   sequence because the original jump will unequivocally
                 *   determine the behavior at the target jump in such a way
                 *   that we can compact the sequence into a single jump.  
                 */
                switch(op)
                {
                case OPC_JSF:
                    /* 
                     *   the original is a JSF: we can recode a jump to a
                     *   JSF, JST, JF, or JT 
                     */
                    switch(target_op)
                    {
                    case OPC_JSF:
                        /* 
                         *   We're jumping to another JSF.  Since the
                         *   original jump will only reach the target jump if
                         *   the value on the top of the stack is false, and
                         *   will then leave this same value on the stack to
                         *   be tested again with the target JSF, we know the
                         *   target JSF will perform its jump and leave the
                         *   stack unchanged again.  So, we can simply
                         *   retarget the original jump to the target of the
                         *   target JSF.  
                         */
                        target_ofs = target_ofs + 1
                                     + str->read2_at(target_ofs + 1);

                        /* keep scanning for additional opportunities */
                        break;

                    case OPC_JST:
                    case OPC_JT:
                        /*
                         *   We're jumping to a JST or a JT.  Since the JSF
                         *   will only reach the JST/JT on a false value, we
                         *   know the JST/JT will NOT jump - we know for a
                         *   fact it will pop the non-true stack element and
                         *   proceed without jumping.  Therefore, we can
                         *   avoid saving the value from the original JSF,
                         *   which means we can recode the original as the
                         *   simpler JF (which doesn't bother saving the
                         *   false value), and jump on false directly to the
                         *   instruction after the target JST/JT.  
                         */
                        str->write_at(ofs, (uchar)OPC_JF);
                        op = OPC_JF;

                        /* jump to the instruction after the target JST/JT */
                        target_ofs += 3;

                        /* keep looking for more jumps */
                        break;

                    case OPC_JF:
                        /* 
                         *   We're jumping to a JF: we know the JF will jump,
                         *   because we had to have - and then save - a false
                         *   value for the JSF to reach the JF in the first
                         *   place.  Since we know for a fact the target JF
                         *   will remove the false value and jump to its
                         *   target, we can bypass the target JF by recoding
                         *   the original instruction as a simpler JF and
                         *   jumping directly to the target of the target JF.
                         */
                        str->write_at(ofs, (uchar)OPC_JF);
                        op = OPC_JF;

                        /* jump to the tartet of the target JF */
                        target_ofs = target_ofs + 1
                                     + str->read2_at(target_ofs + 1);

                        /* keep scanning for more jumps */
                        break;

                    default:
                        /* can't make any assumptions about other targets */
                        done = TRUE;
                        break;
                    }
                    break;

                case OPC_JST:
                    /* 
                     *   the original is a JST: recode it if the target is a
                     *   JSF, JST, JF, or JT 
                     */
                    switch(target_op)
                    {
                    case OPC_JST:
                        /* JST jumping to JST: jump to the target's target */
                        target_ofs = target_ofs + 1
                                     + str->read2_at(target_ofs + 1);

                        /* keep looking */
                        break;

                    case OPC_JSF:
                    case OPC_JF:
                        /* 
                         *   JST jumping to JSF/JF: the JSF/JF will
                         *   definitely pop the stack and not jump (since the
                         *   original JST will have left a true value on the
                         *   stack), so we can recode the JST as a more
                         *   efficient JT and jump to the instruction after
                         *   the JSF/JF target 
                         */
                        str->write_at(ofs, (uchar)OPC_JT);
                        op = OPC_JT;

                        /* jump to the instruction after the target */
                        target_ofs += 3;

                        /* keep looking */
                        break;

                    case OPC_JT:
                        /* 
                         *   JST jumping to JT: the JT will definitely pop
                         *   and jump, so we can recode the original as a
                         *   simpler JT and jump to the target's target 
                         */
                        str->write_at(ofs, (uchar)OPC_JT);
                        op = OPC_JT;

                        /* jump to the target of the target */
                        target_ofs = target_ofs + 1
                                     + str->read2_at(target_ofs + 1);

                        /* keep scanning */
                        break;

                    default:
                        /* can't make any assumptions about other targets */
                        done = TRUE;
                        break;
                    }
                    break;

                default:
                    /* 
                     *   we can't make assumptions about anything else, so
                     *   we've come to the end of the road - stop scanning 
                     */
                    done = TRUE;
                    break;
                }
            }

            /* 
             *   if we found a chain of jumps, replace our original jump
             *   target with the final jump target, bypassing the
             *   intermediate jumps 
             */
            if (target_ofs != orig_target_ofs)
                str->write2_at(ofs + 1, target_ofs - (ofs + 1));

            /* skip past the jump */
            ofs += 3;

            /* done */
            break;

        default:
            /* 
             *   everything else is a fixed-size instruction, so simply
             *   consult our table of instruction lengths to determine the
             *   offset of the next instruction 
             */
            ofs += CVmOpcodes::op_siz[op];
            break;
        }

        /* remember the preceding opcode */
        prv_op = op;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic T3 node 
 */

/* 
 *   generate a jump-ahead instruction, returning a new label which serves
 *   as the jump destination 
 */
CTcCodeLabel *CTcPrsNode::gen_jump_ahead(uchar opc)
{
    /* 
     *   check to see if we should suppress the jump for peephole
     *   optimization 
     */
    if (G_cg->can_skip_op())
        return 0;

    /* emit the opcode */
    G_cg->write_op(opc);

    /* allocate a new label */
    CTcCodeLabel *lbl = G_cs->new_label_fwd();

    /* 
     *   write the forward offset to the label (this will generate a fixup
     *   record attached to the label, so that we'll come back and fix it
     *   up when the real offset is known) 
     */
    G_cs->write_ofs2(lbl, 0);

    /* return the forward label */
    return lbl;
}

/*
 *   Allocate a new label at the current write position 
 */
CTcCodeLabel *CTcPrsNode::new_label_here()
{
    /* 
     *   suppress any peephole optimizations at this point -- someone
     *   could jump directly to this instruction, so we can't combine an
     *   instruction destined for this point with anything previous 
     */
    G_cg->clear_peephole();

    /* create and return a label at the current position */
    return G_cs->new_label_here();
}

/* 
 *   define the position of a code label 
 */
void CTcPrsNode::def_label_pos(CTcCodeLabel *lbl)
{
    /* if the label is null, ignore it */
    if (lbl == 0)
        return;

    /* 
     *   try eliminating a jump-to-next-instruction sequence: if the last
     *   opcode was a JMP to this label, remove the last instruction
     *   entirely 
     */
    if (G_cg->get_last_op() == OPC_JMP
        && G_cs->has_fixup_at_ofs(lbl, G_cs->get_ofs() - 2))
    {
        /* remove the fixup pointing to the preceding JMP */
        G_cs->remove_fixup_at_ofs(lbl, G_cs->get_ofs() - 2);
        
        /* the JMP is unnecessary - remove it */
        G_cg->remove_last_jmp();
    }
    
    /* define the label position and apply the fixup */
    G_cs->def_label_pos(lbl);

    /* 
     *   whenever we define a label, we must suppress any peephole
     *   optimizations at this point - someone could jump directly to this
     *   instruction, so we can't combine an instruction destined for this
     *   point with anything previous 
     */
    G_cg->clear_peephole();
}

/*
 *   Generate code for an if-else conditional test.  The default
 *   implementation is to evaluate the expression, and jump to the false
 *   branch if the expression is false (or jump to the true part if the
 *   expression is true and there's no false part).
 */
void CTcPrsNode::gen_code_cond(CTcCodeLabel *then_label,
                               CTcCodeLabel *else_label)
{
    /* generate our expression code */
    gen_code(FALSE, TRUE);

    /* 
     *   if we have a 'then' part, jump to the 'then' part if the condition
     *   is true; otherwise, jump to the 'else' part if the condition is
     *   false 
     */
    if (then_label != 0)
    {
        /* we have a 'then' part, so jump if true to the 'then' part */
        G_cg->write_op(OPC_JT);
        G_cs->write_ofs2(then_label, 0);
    }
    else
    {
        /* we have an 'else' part, so jump if false to the 'else' */
        G_cg->write_op(OPC_JF);
        G_cs->write_ofs2(else_label, 0);
    }

    /* the JF or JT pops an element off the stack */
    G_cg->note_pop();
}

/*
 *   generate code for assignment to this node 
 */
int CTcPrsNode::gen_code_asi(int, int phase, tc_asitype_t, const char *,
                             CTcPrsNode *,
                             int ignore_error, int, void **)
{
    /* 
     *   If ignoring errors, the caller is trying to assign if possible but
     *   doesn't require it to be possible; simply return false to indicate
     *   that nothing happened if this is the case.  Also ignore errors on
     *   phase 2, since we'll already have reported an error on phase 1.  
     */
    if (ignore_error || phase > 1)
        return FALSE;
    
    /* we should never get here - throw an internal error */
    G_tok->throw_internal_error(TCERR_GEN_BAD_LVALUE);
    AFTER_ERR_THROW(return FALSE;)
}

/*
 *   generate code for taking the address of this node 
 */
void CTcPrsNode::gen_code_addr()
{
    /* we should never get here - throw an internal error */
    G_tok->throw_internal_error(TCERR_GEN_BAD_ADDR);
}

/*
 *   Generate code to call the expression as a function or method.  
 */
void CTcPrsNode::gen_code_call(int discard, int argc, int varargs,
                               CTcNamedArgs *named_args)
{
    /* function/method calls are never valid in speculative mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);
    
    /*
     *   For default nodes, assume that the result of evaluating the
     *   expression contained in the node is a method or function pointer.
     *   First, generate code to evaluate the expression, which should
     *   yield an appropriate pointer value. 
     */
    gen_code(FALSE, FALSE);

    /* 
     *   if we have a varargs list, modify the call instruction that
     *   follows to make it a varargs call 
     */
    if (varargs)
    {
        /* swap the top of the stack to get the arg counter back on top */
        G_cg->write_op(OPC_SWAP);

        /* write the varargs modifier */
        G_cg->write_op(OPC_VARARGC);
    }

    /* generate an indirect function call through the pointer */
    G_cg->write_op(OPC_PTRCALL);
    G_cs->write((char)argc);

    /* PTRCALL pops the arguments plus the function pointer */
    G_cg->note_pop(argc + 1);

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* 
     *   if the caller isn't going to discard the return value, push the
     *   result, which is sitting in R0 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   Generate code for operator 'new' applied to this node
 */
void CTcPrsNode::gen_code_new(int, int, int, CTcNamedArgs *, int, int)
{
    /* operator 'new' cannot be applied to a default node */
    G_tok->log_error(TCERR_INVAL_NEW_EXPR);
}

/*
 *   Generate code for a member evaluation 
 */
void CTcPrsNode::gen_code_member(int discard,
                                 CTcPrsNode *prop_expr, int prop_is_expr,
                                 int argc, int varargs,
                                 CTcNamedArgs *named_args)
{
    /* evaluate my own expression to yield the object value */
    gen_code(FALSE, FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the generic code to generate the rest */
    s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                     argc, varargs, named_args);
}

/*
 *   Generic code to generate the rest of a member expression after the
 *   left side of the '.' has been generated.  This can be used for cases
 *   where the left of the '.' is an arbitrary expression, and hence must
 *   be evaluated at run-time.
 */
void CTcPrsNode::s_gen_member_rhs(int discard,
                                  CTcPrsNode *prop_expr, int prop_is_expr,
                                  int argc, int varargs,
                                  CTcNamedArgs *named_args)
{
    vm_prop_id_t prop;

    /* we can't call methods with arguments in speculative mode */
    if (G_cg->is_speculative()
        && (argc != 0 || (named_args != 0 && named_args->cnt != 0)))
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* get or generate the property ID value */
    prop = prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* 
     *   if we got a property ID, generate a simple GETPROP or CALLPROP
     *   instruction; otherwise, generate a PTRCALLPROP instruction
     */
    if (prop != VM_INVALID_PROP)
    {
        /* 
         *   we have a constant property ID - generate a GETPROP or
         *   CALLPROP with the property ID as constant data 
         */
        if (argc == 0)
        {
            /* no arguments - generate a GETPROP */
            G_cg->write_op(G_cg->is_speculative()
                           ? OPC_GETPROPDATA : OPC_GETPROP);
            G_cs->write_prop_id(prop);

            /* this pops an object element */
            G_cg->note_pop();
        }
        else
        {
            /* write the CALLPROP instruction */
            G_cg->write_callprop(argc, varargs, prop);
        }
    }
    else
    {
        if (G_cg->is_speculative())
        {
            /* 
             *   speculative - use PTRGETPROPDATA to ensure we don't cause
             *   any side effects 
             */
            G_cg->write_op(OPC_PTRGETPROPDATA);
        }
        else
        {
            /* 
             *   if we have a varargs list, modify the call instruction
             *   that follows to make it a varargs call 
             */
            if (varargs)
            {
                /* swap to get the arg counter back on top */
                G_cg->write_op(OPC_SWAP);
                
                /* write the varargs modifier */
                G_cg->write_op(OPC_VARARGC);
            }

            /* a property pointer is on the stack - write a PTRCALLPROP */
            G_cg->write_op(OPC_PTRCALLPROP);
            G_cs->write((int)argc);
        }

        /* 
         *   ptrcallprop/ptrgetpropdata removes arguments, the object, and
         *   the property 
         */
        G_cg->note_pop(argc + 2);
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if we're not discarding the result, push it from R0 */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   Generate code to get the property ID of the expression.
 */
vm_prop_id_t CTcPrsNode::gen_code_propid(int check_only, int is_expr)
{
    /* 
     *   simply evaluate the expression normally, anticipating that this
     *   will yield a property ID value at run-time 
     */
    if (!check_only)
        gen_code(FALSE, FALSE);

    /* tell the caller that there's no constant ID available */
    return VM_INVALID_PROP;
}

/* ------------------------------------------------------------------------ */
/*
 *   "self" 
 */

/*
 *   generate code 
 */
void CTPNSelf::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);
    
    /* if we're not discarding the result, push the "self" object */
    if (!discard)
    {
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->note_push();
    }
}

/*
 *   evaluate a property 
 */
void CTPNSelf::gen_code_member(int discard,
                               CTcPrsNode *prop_expr, int prop_is_expr,
                               int argc, int varargs,
                               CTcNamedArgs *named_args)
{
    vm_prop_id_t prop;
    
    /* make sure "self" is available */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow arguments in speculative eval mode */
    if (G_cg->is_speculative()
        && (argc != 0 || (named_args != 0 && named_args->cnt != 0)))
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* generate the property value */
    prop = prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* 
     *   if we got a property ID, generate a simple GETPROPSELF or
     *   CALLPROPSELF; otherwise, generate a PTRCALLPROPSELF 
     */
    if (prop != VM_INVALID_PROP)
    {
        /* 
         *   we have a constant property ID - generate a GETPROPDATA,
         *   GETPROPSELF, or CALLPROPSELF with the property ID as
         *   immediate data 
         */
        if (G_cg->is_speculative())
        {
            /* speculative - use GETPROPDATA */
            G_cg->write_op(OPC_PUSHSELF);
            G_cg->write_op(OPC_GETPROPDATA);
            G_cs->write_prop_id(prop);

            /* we pushed one element (self) and popped it back off */
            G_cg->note_push();
            G_cg->note_pop();
        }
        else if (argc == 0)
        {
            /* no arguments - generate a GETPROPSELF */
            G_cg->write_op(OPC_GETPROPSELF);
            G_cs->write_prop_id(prop);
        }
        else
        {
            /* add a varargs modifier if appropriate */
            if (varargs)
                G_cg->write_op(OPC_VARARGC);
            
            /* we have arguments - generate a CALLPROPSELF */
            G_cg->write_op(OPC_CALLPROPSELF);
            G_cs->write((char)argc);
            G_cs->write_prop_id(prop);

            /* this removes arguments */
            G_cg->note_pop(argc);
        }
    }
    else
    {
        /* 
         *   a property pointer is on the stack - use PTRGETPROPDATA or
         *   PTRCALLPROPSELF, depending on the speculative mode 
         */
        if (G_cg->is_speculative())
        {
            /* speculative - use PTRGETPROPDATA after pushing self */
            G_cg->write_op(OPC_PUSHSELF);
            G_cg->write_op(OPC_PTRGETPROPDATA);

            /* we pushed self then removed self and the property ID */
            G_cg->note_push();
            G_cg->note_pop(2);
        }
        else
        {
            /* 
             *   if we have a varargs list, modify the call instruction
             *   that follows to make it a varargs call 
             */
            if (varargs)
            {
                /* swap to get the arg counter back on top */
                G_cg->write_op(OPC_SWAP);
                
                /* write the varargs modifier */
                G_cg->write_op(OPC_VARARGC);
            }

            /* a prop pointer is on the stack - write a PTRCALLPROPSELF */
            G_cg->write_op(OPC_PTRCALLPROPSELF);
            G_cs->write((int)argc);
            
            /* this removes arguments and the property pointer */
            G_cg->note_pop(argc + 1);
        }
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if the result is needed, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}


/*
 *   generate code for an object before a '.' 
 */
vm_obj_id_t CTPNSelf::gen_code_obj_predot(int *is_self)
{
    /* make sure "self" is available */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* tell the caller that this is "self" */
    *is_self = TRUE;
    return VM_INVALID_OBJ;
}


/* ------------------------------------------------------------------------ */
/*
 *   "replaced" 
 */

/*
 *   evaluate 'replaced' on its own - this simply yields a function pointer
 *   to the modified base code 
 */
void CTPNReplaced::gen_code(int discard, int for_condition)
{
    /* get the modified base function symbol */
    CTcSymFunc *mod_base = G_cs->get_code_body()->get_replaced_func();

    /* make sure we're in a 'modify func()' context */
    if (mod_base == 0)
        G_tok->log_error(TCERR_REPLACED_NOT_AVAIL);

    /* this expression yields a pointer to the modified base function */
    G_cg->write_op(OPC_PUSHFNPTR);

    /* add a fixup for the current code location */
    if (mod_base != 0)
        mod_base->add_abs_fixup(G_cs);

    /* write a placeholder offset - arbitrarily use zero */
    G_cs->write4(0);

    /* note the push */
    G_cg->note_push();
}

/*
 *   'replaced()' call - this invokes the modified base code 
 */
void CTPNReplaced::gen_code_call(int discard, int argc, int varargs,
                                 CTcNamedArgs *named_args)
{
    /* get the modified base function symbol */
    CTcSymFunc *mod_base = G_cs->get_code_body()->get_replaced_func();

    /* make sure we're in a 'modify func()' context */
    if (mod_base == 0)
        G_tok->log_error(TCERR_REPLACED_NOT_AVAIL);

    /* write the varargs modifier if appropriate */
    if (varargs)
        G_cg->write_op(OPC_VARARGC);

    /* generate the call instruction and argument count */
    G_cg->write_op(OPC_CALL);
    G_cs->write((char)argc);

    /* generate a fixup for the call to the modified base code */
    if (mod_base != 0)
        mod_base->add_abs_fixup(G_cs);

    /* add a placeholder for the function address */
    G_cs->write4(0);

    /* call removes arguments */
    G_cg->note_pop(argc);

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* make sure the argument count is correct */
    if (mod_base != 0 && !mod_base->argc_ok(argc))
    {
        char buf[128];
        G_tok->log_error(TCERR_WRONG_ARGC_FOR_FUNC,
                         (int)mod_base->get_sym_len(), mod_base->get_sym(),
                         mod_base->get_argc_desc(buf), argc);
    }

    /* if we're not discarding, push the return value from R0 */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "targetprop" 
 */

/*
 *   generate code 
 */
void CTPNTargetprop::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_TARGETPROP_NOT_AVAIL);

    /* if we're not discarding the result, push the target property ID */
    if (!discard)
    {
        G_cg->write_op(OPC_PUSHCTXELE);
        G_cs->write(PUSHCTXELE_TARGPROP);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "targetobj" 
 */

/*
 *   generate code 
 */
void CTPNTargetobj::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_TARGETOBJ_NOT_AVAIL);

    /* if we're not discarding the result, push the target object ID */
    if (!discard)
    {
        G_cg->write_op(OPC_PUSHCTXELE);
        G_cs->write(PUSHCTXELE_TARGOBJ);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "definingobj" 
 */

/*
 *   generate code 
 */
void CTPNDefiningobj::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_DEFININGOBJ_NOT_AVAIL);

    /* if we're not discarding the result, push the defining object ID */
    if (!discard)
    {
        G_cg->write_op(OPC_PUSHCTXELE);
        G_cs->write(PUSHCTXELE_DEFOBJ);
        G_cg->note_push();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   "invokee" 
 */

/*
 *   generate code 
 */
void CTPNInvokee::gen_code(int discard, int)
{
    /* if we're not discarding the result, push the invokee value */
    if (!discard)
    {
        G_cg->write_op(OPC_PUSHCTXELE);
        G_cs->write(PUSHCTXELE_INVOKEE);
        G_cg->note_push();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   "inherited" 
 */
void CTPNInh::gen_code(int, int)
{
    /* 
     *   we should never be asked to generate an "inherited" node
     *   directly; these nodes should always be generated as part of
     *   member evaluation 
     */
    G_tok->throw_internal_error(TCERR_GEN_CODE_INH);
}

/*
 *   evaluate a property 
 */
void CTPNInh::gen_code_member(int discard,
                              CTcPrsNode *prop_expr, int prop_is_expr,
                              int argc, int varargs, CTcNamedArgs *named_args)
{
    vm_prop_id_t prop;

    /* don't allow 'inherited' in speculative evaluation mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /*
     *   If we're in a multi-method context, inherited() has a different
     *   meaning from the ordinary method context meaning.
     */
    for (CTPNCodeBody *cb = G_cs->get_code_body() ; cb != 0 ;
         cb = cb->get_enclosing())
    {
        /* check for a multi-method context */
        CTcSymFunc *f = cb->get_func_sym();
        if (f != 0 && f->is_multimethod())
        {
            /* generate the multi-method inherited() code */
            gen_code_mminh(f, discard, prop_expr, prop_is_expr,
                           argc, varargs, named_args);

            /* we're done */
            return;
        }
    }

    /* 
     *   make sure "self" is available - we obviously can't inherit
     *   anything if we're not in an object's method 
     */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* a type list ('inherited<>') is invalid for regular method inheritance */
    if (typelist_ != 0)
        G_tok->log_error(TCERR_MMINH_BAD_CONTEXT);

    /* generate the property value */
    prop = prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* 
     *   if we got a property ID, generate a simple INHERIT;
     *   otherwise, generate a PTRINHERIT 
     */
    if (prop != VM_INVALID_PROP)
    {
        /* generate a varargs modifier if necessary */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);
        
        /* we have a constant property ID - generate a regular INHERIT */
        G_cg->write_op(OPC_INHERIT);
        G_cs->write((char)argc);
        G_cs->write_prop_id(prop);

        /* this removes arguments */
        G_cg->note_pop(argc);
    }
    else
    {
        /* 
         *   if we have a varargs list, modify the call instruction that
         *   follows to make it a varargs call 
         */
        if (varargs)
        {
            /* swap the top of the stack to get the arg counter back on top */
            G_cg->write_op(OPC_SWAP);
            
            /* write the varargs modifier */
            G_cg->write_op(OPC_VARARGC);
        }

        /* a property pointer is on the stack - write a PTRINHERIT */
        G_cg->write_op(OPC_PTRINHERIT);
        G_cs->write((int)argc);

        /* this removes arguments and the property pointer */
        G_cg->note_pop(argc + 1);
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if the result is needed, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

void CTPNInh::gen_code_mminh(CTcSymFunc *func, int discard,
                             CTcPrsNode *prop_expr, int prop_is_expr,
                             int argc, int varargs,
                             CTcNamedArgs *named_args)
{
    /*
     *   If the function has an associated global symbol from a modification,
     *   use the global symbol instead.  The modified function doesn't have a
     *   real symbol, so we can't use it; but fortunately, the global symbol
     *   applies to all modified versions as well, since they all inherit
     *   from the same base function.  
     */
    while (func->get_mod_global() != 0)
        func = func->get_mod_global();

    /*
     *   There are two forms of inherited() for multi-methods.
     *   
     *   inherited<types>(args) - this form takes an explicit type list, to
     *   invoke a specific overridden version.
     *   
     *   inherited(args) - this form invokes the next inherited version,
     *   determined dynamically at run-time.  
     */
    if (typelist_ == 0)
    {
        /*
         *   It's the inherited(args) format.
         *   
         *   Call _multiMethodCallInherited(fromFunc, args).  We've already
         *   pushed the args list, so just add the source function argument
         *   (which is simply our defining function).  
         */
        func->gen_code(FALSE);

        /* look up _multiMethodCallInherited */
        CTcSymFunc *mmci = (CTcSymFunc *)G_prs->get_global_symtab()->find(
            "_multiMethodCallInherited", 25);

        if (mmci == 0 || mmci->get_type() != TC_SYM_FUNC)
        {
            /* undefined or incorrectly defined - log an error */
            G_tok->log_error(TCERR_MMINH_MISSING_SUPPORT_FUNC,
                             25, "_multiMethodCallInherited");
        }
        else
        {
            /* generate the call */
            mmci->gen_code_call(discard, argc + 1, varargs, named_args);
        }
    }
    else
    {
        /* 
         *   It's the inherited<types>(args) format.
         */

        /* 
         *   Get the base name for the function.  'func' is the decorated
         *   name for the containing function, which is of the form
         *   'Base*type1;type2...'.  The base name is the part up to the
         *   asterisk.  
         */
        const char *nm = func->getstr(), *p;
        size_t rem = func->getlen();
        for (p = nm ; rem != 0 && *p != '*' ; ++p, --rem) ;

        /* make a token for the base name */
        CTcToken btok;
        btok.set_text(nm, p - nm);
        
        /* build the decorated name for the target function */
        CTcToken dtok;
        typelist_->decorate_name(&dtok, &btok);

        /* look up the decorated name */
        CTcSymFunc *ifunc = (CTcSymFunc *)G_prs->get_global_symtab()->find(
            dtok.get_text(), dtok.get_text_len());

        /* if we found it, call it */
        if (ifunc != 0 && ifunc->get_type() == TC_SYM_FUNC)
        {
            /* generate the call */
            ifunc->gen_code_call(discard, argc, varargs, named_args);
        }
        else
        {
            /* function not found */
            G_tok->log_error(TCERR_MMINH_UNDEF_FUNC, (int)(p - nm), nm);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "inherited class"
 */
void CTPNInhClass::gen_code(int discard, int for_condition)
{
    /* 
     *   we should never be asked to generate an "inherited" node
     *   directly; these nodes should always be generated as part of
     *   member evaluation 
     */
    G_tok->throw_internal_error(TCERR_GEN_CODE_INH);
}

/*
 *   evaluate a property 
 */
void CTPNInhClass::gen_code_member(int discard,
                                   CTcPrsNode *prop_expr, int prop_is_expr,
                                   int argc, int varargs,
                                   CTcNamedArgs *named_args)
{
    vm_prop_id_t prop;
    CTcSymbol *objsym;
    vm_obj_id_t obj;

    /* 
     *   make sure "self" is available - we obviously can't inherit
     *   anything if we're not in an object's method 
     */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow 'inherited' in speculative evaluation mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* get the superclass name symbol */
    objsym = G_cs->get_symtab()->find_or_def_undef(sym_, len_, FALSE);
    
    /* if it's not an object, we can't inherit from it */
    obj = objsym->get_val_obj();
    if (obj == VM_INVALID_OBJ)
    {
        G_tok->log_error(TCERR_INH_NOT_OBJ, (int)len_, sym_);
        return;
    }

    /* generate the property value */
    prop = prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* 
     *   if we got a property ID, generate a simple EXPINHERIT; otherwise,
     *   generate a PTREXPINHERIT 
     */
    if (prop != VM_INVALID_PROP)
    {
        /* add a varargs modifier if needed */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);
        
        /* we have a constant property ID - generate a regular EXPINHERIT */
        G_cg->write_op(OPC_EXPINHERIT);
        G_cs->write((char)argc);
        G_cs->write_prop_id(prop);
        G_cs->write_obj_id(obj);

        /* this removes argumnts */
        G_cg->note_pop(argc);
    }
    else
    {
        /* 
         *   if we have a varargs list, modify the call instruction that
         *   follows to make it a varargs call 
         */
        if (varargs)
        {
            /* swap the top of the stack to get the arg counter back on top */
            G_cg->write_op(OPC_SWAP);
            
            /* write the varargs modifier */
            G_cg->write_op(OPC_VARARGC);
        }
        
        /* a property pointer is on the stack - write a PTREXPINHERIT */
        G_cg->write_op(OPC_PTREXPINHERIT);
        G_cs->write((int)argc);
        G_cs->write_obj_id(obj);

        /* this removes arguments and the property pointer */
        G_cg->note_pop(argc + 1);
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if the result is needed, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "delegated" 
 */
void CTPNDelegated::gen_code(int discard, int for_condition)
{
    /* 
     *   we should never be asked to generate a "delegated" node directly;
     *   these nodes should always be generated as part of member evaluation 
     */
    G_tok->throw_internal_error(TCERR_GEN_CODE_DELEGATED);
}

/*
 *   evaluate a property 
 */
void CTPNDelegated::gen_code_member(int discard,
                                    CTcPrsNode *prop_expr, int prop_is_expr,
                                    int argc, int varargs,
                                    CTcNamedArgs *named_args)
{
    vm_prop_id_t prop;

    /* 
     *   make sure "self" is available - we obviously can't delegate
     *   anything if we're not in an object's method 
     */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow 'delegated' in speculative evaluation mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* generate the delegatee expression */
    delegatee_->gen_code(FALSE, FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* generate the property value */
    prop = prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* 
     *   if we got a property ID, generate a simple DELEGATE; otherwise,
     *   generate a PTRDELEGATE 
     */
    if (prop != VM_INVALID_PROP)
    {
        /* add a varargs modifier if needed */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);

        /* we have a constant property ID - generate a regular DELEGATE */
        G_cg->write_op(OPC_DELEGATE);
        G_cs->write((char)argc);
        G_cs->write_prop_id(prop);

        /* this removes arguments and the object value */
        G_cg->note_pop(argc + 1);
    }
    else
    {
        /* 
         *   if we have a varargs list, modify the call instruction that
         *   follows to make it a varargs call 
         */
        if (varargs)
        {
            /* swap the top of the stack to get the arg counter back on top */
            G_cg->write_op(OPC_SWAP);

            /* write the varargs modifier */
            G_cg->write_op(OPC_VARARGC);
        }

        /* a property pointer is on the stack - write a PTRDELEGATE */
        G_cg->write_op(OPC_PTRDELEGATE);
        G_cs->write((int)argc);

        /* this removes arguments, the object, and the property pointer */
        G_cg->note_pop(argc + 2);
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if the result is needed, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   "argcount" 
 */
void CTPNArgc::gen_code(int discard, int)
{
    /* generate the argument count, if we're not discarding */
    if (!discard)
    {
        if (G_cg->is_eval_for_debug())
        {
            /* generate a debug argument count evaluation */
            G_cg->write_op(OPC_GETDBARGC);
            G_cs->write2(G_cg->get_debug_stack_level());
        }
        else
        {
            /* generate the normal argument count evaluation */
            G_cg->write_op(OPC_GETARGC);
        }

        /* we push one element */
        G_cg->note_push();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   constant
 */
void CTPNConst::gen_code(int discard, int)
{
    /* if we're discarding the value, do nothing */
    if (discard)
        return;
    
    /* generate the appropriate type of push for the value */
    switch(val_.get_type())
    {
    case TC_CVT_NIL:
        G_cg->write_op(OPC_PUSHNIL);
        break;

    case TC_CVT_TRUE:
        G_cg->write_op(OPC_PUSHTRUE);
        break;

    case TC_CVT_INT:
        /* write the push-integer instruction */
        s_gen_code_int(val_.get_val_int());

        /* s_gen_code_int notes a push, which we'll do also, so cancel it */
        G_cg->note_pop();
        break;

    case TC_CVT_FLOAT:
        /* write the push-object instruction for the BigNumber object */
        G_cg->write_op(OPC_PUSHOBJ);

        /* generate the BigNumber object and write its ID */
        G_cs->write_obj_id(G_cg->gen_bignum_obj(
            val_.get_val_float(), val_.get_val_float_len(),
            val_.is_promoted()));
        break;

    case TC_CVT_RESTR:
        /* write the push-object instruction for the RexPattern object */
        G_cg->write_op(OPC_PUSHOBJ);

        /* generate the RexPattern object and write its ID */
        G_cs->write_obj_id(G_cg->gen_rexpat_obj(
            val_.get_val_str(), val_.get_val_str_len()));
        break;

    case TC_CVT_SSTR:
        /* generate the string */
        s_gen_code_str(&val_);

        /* s_gen_code_int notes a push, which we'll do also, so cancel it */
        G_cg->note_pop();
        break;

    case TC_CVT_LIST:
        /* write the instruction */
        G_cg->write_op(OPC_PUSHLST);

        /* 
         *   add the list to the constant pool, creating a fixup at the
         *   current code stream location 
         */
        G_cg->add_const_list(val_.get_val_list(), G_cs, G_cs->get_ofs());

        /* 
         *   write a placeholder address - this will be corrected by the
         *   fixup that add_const_list() created for us
         */
        G_cs->write4(0);
        break;

    case TC_CVT_OBJ:
        /* generate the object ID */
        G_cg->write_op(OPC_PUSHOBJ);
        G_cs->write_obj_id(val_.get_val_obj());
        break;

    case TC_CVT_PROP:
        /* generate the property address */
        G_cg->write_op(OPC_PUSHPROPID);
        G_cs->write_prop_id(val_.get_val_prop());
        break;

    case TC_CVT_ENUM:
        /* generate the enum value */
        G_cg->write_op(OPC_PUSHENUM);
        G_cs->write_enum_id(val_.get_val_enum());
        break;

    case TC_CVT_FUNCPTR:
        /* generate the function pointer instruction */
        G_cg->write_op(OPC_PUSHFNPTR);

        /* add a fixup for the function address */
        val_.get_val_funcptr_sym()->add_abs_fixup(G_cs);

        /* write out a placeholder - arbitrarily use zero */
        G_cs->write4(0);
        break;

    case TC_CVT_ANONFUNCPTR:
        /* generate the function pointer instruction */
        G_cg->write_op(OPC_PUSHFNPTR);

        /* add a fixup for the code body address */
        val_.get_val_anon_func_ptr()->add_abs_fixup(G_cs);

        /* write our a placeholder */
        G_cs->write4(0);
        break;

    default:
        /* anything else is an internal error */
        G_tok->throw_internal_error(TCERR_GEN_UNK_CONST_TYPE);
    }

    /* all of these push a value */
    G_cg->note_push();
}

int CTPNConst::gen_code_asi(int discard, int phase,
                            tc_asitype_t typ, const char *op,
                            CTcPrsNode *, 
                            int ignore_errors, int, void **)
{
    /* 
     *   If ignoring errors, just indicate that we can't do the assignment.
     *   Also ignore errors on phase 2, since we'll already have generated an
     *   error during phase 1. 
     */
    if (ignore_errors || phase > 1)
        return FALSE;

    /* 
     *   We can't assign to a constant.  The parser will catch most errors of
     *   this type before we ever get here, but for expressions that resolve
     *   to global symbols (such as objects or functions), it can't know
     *   until code generation time that the expression won't be assignable. 
     */
    G_tok->log_error(typ == TC_ASI_PREINC || typ == TC_ASI_PREDEC
                     || typ == TC_ASI_POSTINC || typ == TC_ASI_POSTDEC
                     ? TCERR_INVALID_UNARY_LVALUE : TCERR_INVALID_LVALUE,
                     op);
    return FALSE;
}


/*
 *   generate code to push an integer value 
 */
void CTPNConst::s_gen_code_int(long intval)
{
    /* push the smallest format that will fit the value */
    if (intval == 0)
    {
        /* write the special PUSH_0 instruction */
        G_cg->write_op(OPC_PUSH_0);
    }
    else if (intval == 1)
    {
        /* write the special PUSH_1 instruction */
        G_cg->write_op(OPC_PUSH_1);
    }
    else if (intval < 127 && intval >= -128)
    {
        /* it fits in eight bits */
        G_cg->write_op(OPC_PUSHINT8);
        G_cs->write((char)intval);
    }
    else
    {
        /* it doesn't fit in 8 bits - use a full 32 bits */
        G_cg->write_op(OPC_PUSHINT);
        G_cs->write4(intval);
    }

    /* however we did it, we left one value on the stack */
    G_cg->note_push();
}

/*
 *   Generate code to push a symbol's name as a string constant value 
 */
void CTPNConst::s_gen_code_str(const CTcSymbol *sym)
{
    /* generate the string for the symbol name */
    s_gen_code_str(sym->get_sym(), sym->get_sym_len());
}

/*
 *   Generate code to push a symbol's name as a string constant value,
 *   according to the code generation mode.
 */
void CTPNConst::s_gen_code_str_by_mode(const CTcSymbol *sym)
{
    /* check the mode */
    if (G_cg->is_eval_for_dyn())
    {
        /* dynamic evaluation - use in-line strings */
        CTPNDebugConst::s_gen_code_str(sym->get_sym(), sym->get_sym_len());
    }
    else
    {
        /* static compilation - use the constant pool */
        s_gen_code_str(sym->get_sym(), sym->get_sym_len());
    }
}

/*
 *   Generate code to push a string constant value 
 */
void CTPNConst::s_gen_code_str(const CTcConstVal *val)
{
    s_gen_code_str(val->get_val_str(), val->get_val_str_len());
}

/*
 *   Generate code to push a string constant value 
 */
void CTPNConst::s_gen_code_str(const char *str, size_t len)
{
    /* write the instruction to push a constant pool string */
    G_cg->write_op(OPC_PUSHSTR);

    /* note the push */
    G_cg->note_push();

    /* 
     *   add the string to the constant pool, creating a fixup at the current
     *   code stream location 
     */
    G_cg->add_const_str(str, len, G_cs, G_cs->get_ofs());

    /* 
     *   write a placeholder address - this will be corrected by the fixup
     *   that add_const_str() created for us 
     */
    G_cs->write4(0);
}

/*
 *   Generate code to apply operator 'new' to the constant.  We can apply
 *   'new' only to constant object values. 
 */
void CTPNConst::gen_code_new(int discard,
                             int argc, int varargs, CTcNamedArgs *named_args,
                             int /*from_call*/, int is_transient)
{
    /* check the type */
    switch(val_.get_type())
    {
    case TC_CVT_OBJ:
        /* 
         *   Treat this the same as any other 'new' call.  An object symbol
         *   folded into a constant is guaranteed to be of metaclass
         *   TadsObject - that's the only kind of symbol we'll ever fold this
         *   way. 
         */
        CTcSymObj::s_gen_code_new(discard,
                                  val_.get_val_obj(), val_.get_val_obj_meta(),
                                  argc, varargs, named_args, is_transient);
        break;

    default:
        /* can't apply 'new' to other constant values */
        G_tok->log_error(TCERR_INVAL_NEW_EXPR);
        break;
    }
}

/*
 *   Generate code to make a function call to this expression.  If we're
 *   calling a function, we can generate this directly.  
 */
void CTPNConst::gen_code_call(int discard, int argc,
                              int varargs, CTcNamedArgs *named_args)
{
    /* check our type */
    switch(val_.get_type())
    {
    case TC_CVT_FUNCPTR:
        /* generate a call to our function symbol */
        val_.get_val_funcptr_sym()->gen_code_call(
            discard, argc, varargs, named_args);
        break;

    default:
        /* other types cannot be called */
        G_tok->log_error(TCERR_CANNOT_CALL_CONST);
        break;
    }
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTPNConst::gen_code_propid(int check_only, int is_expr)
{
    /* check the type */
    switch(val_.get_type())
    {
    case TC_CVT_PROP:
        /* return the constant property ID */
        return (vm_prop_id_t)val_.get_val_prop();

    default:
        /* other values cannot be used as properties */
        if (!check_only)
            G_tok->log_error(TCERR_INVAL_PROP_EXPR);
        return VM_INVALID_PROP;
    }
}


/*
 *   Generate code for a member evaluation 
 */
void CTPNConst::gen_code_member(int discard,
                                CTcPrsNode *prop_expr, int prop_is_expr,
                                int argc, int varargs, CTcNamedArgs *named_args)
{
    /* check our constant type */
    switch(val_.get_type())
    {
    case TC_CVT_OBJ:
        /* call the object symbol code to do the work */
        CTcSymObj::s_gen_code_member(discard, prop_expr, prop_is_expr,
                                     argc, val_.get_val_obj(),
                                     varargs, named_args);
        break;

    case TC_CVT_LIST:
    case TC_CVT_SSTR:
    case TC_CVT_RESTR:
    case TC_CVT_FLOAT:
        /* 
         *   list/string/BigNumber constant - generate our value as
         *   normal, then use the standard member generation 
         */
        gen_code(FALSE, FALSE);

        /* if we have an argument counter, put it back on top */
        if (varargs)
            G_cg->write_op(OPC_SWAP);

        /* use standard member generation */
        CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                     argc, varargs, named_args);
        break;

    default:
        G_tok->log_error(TCERR_INVAL_OBJ_EXPR);
        break;
    }
}


/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTPNConst::gen_code_obj_predot(int *is_self)
{
    /* we're certainly not "self" */
    *is_self = FALSE;

    /* if I don't have an object value, this is illegal */
    if (val_.get_type() != TC_CVT_OBJ)
    {
        G_tok->log_error(TCERR_INVAL_OBJ_EXPR);
        return VM_INVALID_OBJ;
    }

    /* report our constant object value */
    return val_.get_val_obj();
}

/* ------------------------------------------------------------------------ */
/*
 *   debugger constant 
 */
void CTPNDebugConst::gen_code(int discard, int for_condition)
{
    /* if we're discarding the value, do nothing */
    if (discard)
        return;

    /* generate the appropriate type of push for the value */
    switch(val_.get_type())
    {
    case TC_CVT_SSTR:
        if (val_.get_val_str() == 0)
        {
            /* validate the string address */
            if (G_vmifc
                && !G_vmifc->validate_pool_str(val_.get_val_str_ofs()))
            {
                /* treat it as nil */
                G_cg->write_op(OPC_PUSHNIL);
                G_cg->note_push();
                break;
            }

            /* we have a pre-resolved pool offset */
            G_cg->write_op(OPC_PUSHSTR);
            G_cs->write4(val_.get_val_str_ofs());
            G_cg->note_push();
        }
        else
        {
            /* write the in-line string instruction */
            s_gen_code_str(val_.get_val_str(), val_.get_val_str_len());
        }
        break;

    case TC_CVT_LIST:
        if (val_.get_val_list() == 0)
        {
            /* validate the list address */
            if (G_vmifc
                && !G_vmifc->validate_pool_list(val_.get_val_list_ofs()))
            {
                /* treat it as nil */
                G_cg->write_op(OPC_PUSHNIL);
                G_cg->note_push();
                break;
            }

            /* we have a pre-resolved pool offset */
            G_cg->write_op(OPC_PUSHLST);
            G_cs->write4(val_.get_val_list_ofs());
            G_cg->note_push();
        }
        else
        {
            /* we should never have a regular constant list when debugging */
            assert(FALSE);
        }
        break;

    case TC_CVT_FUNCPTR:
        /* generate the function pointer instruction */
        G_cg->write_op(OPC_PUSHFNPTR);

        /* 
         *   write the actual function address - no need for fixups in the
         *   debugger, since everything's fully resolved 
         */
        G_cs->write4(val_.get_val_funcptr_sym()->get_code_pool_addr());

        /* note the value push */
        G_cg->note_push();
        break;

    case TC_CVT_BIFPTR:
        /* validate the property ID if possible */
        if (G_vmifc &&
            !G_vmifc->validate_bif(
                val_.get_val_bifptr_sym()->get_func_set_id(),
                val_.get_val_bifptr_sym()->get_func_idx()))
        {
            /* treat it as nil */
            G_cg->write_op(OPC_PUSHNIL);
            G_cg->note_push();
            break;
        }       

        /* generate the built-in function pointer instruction */
        G_cg->write_op(OPC_PUSHBIFPTR);
        G_cs->write2(val_.get_val_bifptr_sym()->get_func_idx());
        G_cs->write2(val_.get_val_bifptr_sym()->get_func_set_id());

        /* note the value push */
        G_cg->note_push();
        break;

    case TC_CVT_ANONFUNCPTR:
        /* 
         *   we should never see an anonymous function pointer constant in
         *   the debugger - these will always be represented as objects
         *   intead 
         */
        assert(FALSE);
        break;

    case TC_CVT_FLOAT:
        {
            /* 
             *   find the 'BigNumber' metaclass - if it's not defined, we
             *   can't create BigNumber values 
             */
            CTcSymMetaclass *sym =
                (CTcSymMetaclass *)G_prs->get_global_symtab()->find(
                    "BigNumber", 9);
            if (sym == 0 || sym->get_type() != TC_SYM_METACLASS)
                err_throw(VMERR_INVAL_DBG_EXPR);

            /* push the floating value as an immediate string */
            G_cg->write_op(OPC_PUSHSTRI);
            G_cs->write2(val_.get_val_str_len());
            G_cs->write(val_.get_val_str(), val_.get_val_str_len());

            /* create the new BigNumber object from the string */
            G_cg->write_op(OPC_NEW2);
            G_cs->write2(1);
            G_cs->write2(sym->get_meta_idx());

            /* retrieve the value */
            G_cg->write_op(OPC_GETR0);

            /* 
             *   note the net push of one value (we pushed the argument,
             *   popped the argument, and pushed the new object) 
             */
            G_cg->note_push();
        }
        break;

    case TC_CVT_RESTR:
        {
            /* find the RexPattern metaclass */
            CTcSymMetaclass *sym =
                (CTcSymMetaclass *)G_prs->get_global_symtab()->find(
                    "RexPattern", 10);
            if (sym == 0 || sym->get_type() != TC_SYM_METACLASS)
                err_throw(VMERR_INVAL_DBG_EXPR);

            /* push the pattern source string value as an immediate string */
            G_cg->write_op(OPC_PUSHSTRI);
            G_cs->write2(val_.get_val_str_len());
            G_cs->write(val_.get_val_str(), val_.get_val_str_len());

            /* create the new RexPattern object from the string */
            G_cg->write_op(OPC_NEW2);
            G_cs->write2(1);
            G_cs->write2(sym->get_meta_idx());
            
            /* retrieve the value */
            G_cg->write_op(OPC_GETR0);

            /* 
             *   note the net push of one value (we pushed the argument,
             *   popped the argument, and pushed the new object) 
             */
            G_cg->note_push();
        }
        break;

    case TC_CVT_OBJ:
        /* validate the object ID if possible */
        if (G_vmifc && !G_vmifc->validate_obj(val_.get_val_obj()))
        {
            /* treat it as nil */
            G_cg->write_op(OPC_PUSHNIL);
            G_cg->note_push();
            break;
        }

        /* use the default handling */
        goto use_default;

    case TC_CVT_PROP:
        /* validate the property ID if possible */
        if (G_vmifc &&
            !G_vmifc->validate_prop((tctarg_prop_id_t)val_.get_val_prop()))
        {
            /* treat it as nil */
            G_cg->write_op(OPC_PUSHNIL);
            G_cg->note_push();
            break;
        }

        /* use the default handling */
        goto use_default;

    default:
    use_default:
        /* handle normally for anything else */
        CTPNConst::gen_code(discard, for_condition);
        break;
    }
}

/*
 *   Generate a string value in a debug/dynamic code context.  This generates
 *   an in-line string rather than a constant pool reference.  
 */
void CTPNDebugConst::s_gen_code_str(const char *str, size_t len)
{
    /* write the PUSHSTRI <len> <string bytes> */
    G_cg->write_op(OPC_PUSHSTRI);
    G_cs->write2(len);
    G_cs->write(str, len);

    /* note the value push */
    G_cg->note_push();
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic Unary Operator 
 */

/* 
 *   Generate a unary-operator opcode.  We assume that the opcode has no
 *   side effects other than to compute the result, so we do not generate
 *   the opcode at all if 'discard' is true; we do, however, always
 *   generate code for the subexpression to ensure that its side effects
 *   are performed.
 *   
 *   In most cases, the caller simply should pass through its 'discard'
 *   status, since the result of the subexpression is generally needed
 *   only when the result of the enclosing expression is needed.
 *   
 *   In most cases, the caller should pass FALSE for 'for_condition',
 *   because applying an operator to the result generally requires that
 *   the result be properly converted for use as a temporary value.
 *   However, when the caller knows that its own opcode will perform the
 *   same conversions that a conditional opcode would, 'for_condition'
 *   should be TRUE.  In most cases, the caller's own 'for_condition'
 *   status is not relevant and should thus not be passed through.  
 */
void CTPNUnary::gen_unary(uchar opc, int discard, int for_condition)
{
    /* 
     *   Generate the operand.  Pass through the 'discard' status to the
     *   operand - if the result of the parent operator is being
     *   discarded, then so is the result of this subexpression.  In
     *   addition, pass through the caller's 'for_condition' disposition.  
     */
    sub_->gen_code(discard, for_condition);

    /* apply the operator if we're not discarding the result */
    if (!discard)
        G_cg->write_op(opc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic Binary Operator
 */

/*
 *   Generate a binary-operator opcode.
 *   
 *   In most cases, the caller's 'discard' status should be passed
 *   through, since the results of the operands are usually needed if and
 *   only if the results of the enclosing expression are needed.
 *   
 *   In most cases, the caller should pass FALSE for 'for_condition'.
 *   Only when the caller knows that the opcode will perform the same
 *   conversions as a BOOLIZE instruction should it pass TRUE for
 *   'for_condition'.  
 */
void CTPNBin::gen_binary(uchar opc, int discard, int for_condition)
{
    /* 
     *   generate the operands, passing through the discard and
     *   conditional status 
     */
    left_->gen_code(discard, for_condition);
    right_->gen_code(discard, for_condition);

    /* generate our operand if we're not discarding the result */
    if (!discard)
    {
        /* apply the operator */
        G_cg->write_op(opc);

        /* 
         *   binary operators all remove two values and push one, so there's
         *   a net pop 
         */
        G_cg->note_pop();
    }
}

/*
 *   Generate a binary-operator opcode for a compound assignment.  This omits
 *   generating the left operand, because the compound assignment operator
 *   will already have done that.  
 */
void CTPNBin::gen_binary_ca(uchar opc)
{
    /* generate the right operand (the left is already on the stack) */
    right_->gen_code(FALSE, FALSE);

    /* generate the operator */
    G_cg->write_op(opc);

    /* a binary op removes two values and pushes one */
    G_cg->note_pop();
}


/* ------------------------------------------------------------------------ */
/*
 *   logical NOT
 */
void CTPNNot::gen_code(int discard, int)
{
    /*
     *   Generate the subexpression and apply the NOT opcode.  Note that
     *   we can compute the subexpression as though we were applying a
     *   condition, because the NOT opcode takes exactly the same kind of
     *   input as any condition opcode; we can thus avoid an extra
     *   conversion in some cases.  
     */
    gen_unary(OPC_NOT, discard, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Boolean-ize operator
 */
void CTPNBoolize::gen_code(int discard, int for_condition)
{
    /*
     *   If the result will be used for a conditional, there's no need to
     *   generate an instruction to convert the value to boolean.  The opcode
     *   that will be used for the condition will perform exactly the same
     *   conversions that this opcode would apply; avoid the redundant work
     *   in this case, and simply generate the underlying expression
     *   directly.  
     */
    if (for_condition)
    {
        /* generate the underlying expression without modification */
        sub_->gen_code(discard, for_condition);

        /* done */
        return;
    }

    /* 
     *   if the sub-expression is already guaranteed to be a boolean value,
     *   there's nothing we need to do 
     */
    if (sub_->is_bool())
        return;
    
    /*
     *   Generate the subexpression and apply the BOOLIZE operator.  Since
     *   we're explicitly boolean-izing the value, there's no need for the
     *   subexpression to do the same thing, so the subexpression can
     *   pretend it's generating for a conditional.  
     */
    gen_unary(OPC_BOOLIZE, discard, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   bitwise NOT 
 */
void CTPNBNot::gen_code(int discard, int)
{
    gen_unary(OPC_BNOT, discard, FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   arithmetic positive
 */
void CTPNPos::gen_code(int discard, int)
{
    /* 
     *   simply generate our operand, since the operator itself has no
     *   effect 
     */
    sub_->gen_code(discard, FALSE); 
}

/* ------------------------------------------------------------------------ */
/*
 *   unary arithmetic negative
 */
void CTPNNeg::gen_code(int discard, int)
{
    gen_unary(OPC_NEG, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   pre-increment
 */
void CTPNPreInc::gen_code(int discard, int)
{
    /* use the static handler */
    CTPNPreInc_gen_code(sub_, discard);
}

/* static pre-inc code generator, for sharing with other nodes */
void CTPNPreInc_gen_code(CTcPrsNode *sub, int discard)
{
    /* ask the subnode to generate it */
    void *ctx;
    if (!sub->gen_code_asi(discard, 1, TC_ASI_PREINC, "++",
                           0, FALSE, TRUE, &ctx))
    {
        /* increment the value at top of stack */
        G_cg->write_op(OPC_INC);

        /* 
         *   generate a simple assignment back to the subexpression; if
         *   we're using the value, let the simple assignment leave its
         *   value on the stack, since the result is the value *after* the
         *   increment 
         */
        sub->gen_code_asi(discard, 2, TC_ASI_PREINC, "++",
                          0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   pre-decrement
 */
void CTPNPreDec::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    void *ctx;
    if (!sub_->gen_code_asi(discard, 1, TC_ASI_PREDEC, "--",
                            0, FALSE, TRUE, &ctx))
    {
        /* decrement the value at top of stack */
        G_cg->write_op(OPC_DEC);

        /* 
         *   generate a simple assignment back to the subexpression; if
         *   we're using the value, let the simple assignment leave its
         *   value on the stack, since the result is the value *after* the
         *   decrement 
         */
        sub_->gen_code_asi(discard, 2, TC_ASI_PREDEC, "--",
                           0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   post-increment
 */
void CTPNPostInc::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    void *ctx;
    if (!sub_->gen_code_asi(discard, 1, TC_ASI_POSTINC, "++",
                            0, FALSE, TRUE, &ctx))
    {
        /* 
         *   if we're keeping the result, duplicate the value at top of
         *   stack prior to the increment - since this is a
         *   post-increment, the result is the value *before* the
         *   increment 
         */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }

        /* increment the value at top of stack */
        G_cg->write_op(OPC_INC);

        /* 
         *   Generate a simple assignment back to the subexpression.
         *   Discard the result of this assignment, regardless of whether
         *   the caller wants the result of the overall expression,
         *   because we've already pushed the actual result, which is the
         *   original value before the increment operation.
         */
        sub_->gen_code_asi(TRUE, 2, TC_ASI_POSTINC, "++",
                           0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   post-decrement
 */
void CTPNPostDec::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    void *ctx;
    if (!sub_->gen_code_asi(discard, 1, TC_ASI_POSTDEC, "--",
                            0, FALSE, TRUE, &ctx))
    {
        /* 
         *   if we're keeping the result, duplicate the value at top of
         *   stack prior to the decrement - since this is a
         *   post-decrement, the result is the value *before* the
         *   decrement 
         */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }

        /* decrement the value at top of stack */
        G_cg->write_op(OPC_DEC);

        /* 
         *   Generate a simple assignment back to the subexpression.
         *   Discard the result of this assignment, regardless of whether
         *   the caller wants the result of the overall expression,
         *   because we've already pushed the actual result, which is the
         *   original value before the decrement operation.
         */
        sub_->gen_code_asi(TRUE, 2, TC_ASI_POSTDEC, "--",
                           0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   operator 'new'
 */
void CTPNNew::gen_code(int discard, int /*for condition*/)
{
    /* 
     *   ask my subexpression to generate the code - at this point we
     *   don't know the number of arguments, so pass in zero for now 
     */
    sub_->gen_code_new(discard, 0, FALSE, 0, FALSE, transient_);
}

/* ------------------------------------------------------------------------ */
/*
 *   operator 'delete'
 */
void CTPNDelete::gen_code(int, int)
{
    /* 'delete' generates no code for T3 VM */
}

/* ------------------------------------------------------------------------ */
/*
 *   comma operator
 */
void CTPNComma::gen_code(int discard, int for_condition)
{
    /* 
     *   Generate each side's code.  Note that the left side is *always*
     *   discarded, regardless of whether the result of the comma operator
     *   will be discarded.  After we generate our subexpressions, there's
     *   nothing left to do, since the comma operator itself doesn't
     *   change anything - we simply use the right operand result as our
     *   result.
     *   
     *   Pass through the 'for_condition' status to the right operand,
     *   since we pass through its result to the caller.  For the left
     *   operand, treat it as a condition - we don't care about the result
     *   value, so don't bother performing any extra conversions on it.  
     */
    left_->gen_code(TRUE, TRUE);
    right_->gen_code(discard, for_condition);
}

/* ------------------------------------------------------------------------ */
/*
 *   logical OR (short-circuit logic)
 */
void CTPNOr::gen_code(int discard, int for_condition)
{
    CTcCodeLabel *lbl;
    
    /* 
     *   First, evaluate the left-hand side; we need the result even if
     *   we're discarding the overall expression, since we will check the
     *   result to see if we should even evaluate the right-hand side.
     *   We're using the value for a condition, so don't bother
     *   boolean-izing it.  
     */
    left_->gen_code(FALSE, TRUE);

    /* 
     *   If the left-hand side is true, there's no need to evaluate the
     *   right-hand side (and, in fact, we're not even allowed to evaluate
     *   the right-hand side because of the short-circuit logic rule).
     *   So, if the lhs is true, we want to jump around the code to
     *   evaluate the rhs, saving the 'true' result if we're not
     *   discarding the overall result. 
     */
    lbl = gen_jump_ahead(discard ? OPC_JT : OPC_JST);

    /* 
     *   Evaluate the right-hand side.  We don't need to save the result
     *   unless we need the result of the overall expression.  Generate
     *   the value as though we were going to booleanize it ourselves,
     *   since we'll do just that (hence pass for_condition = TRUE).  
     */
    right_->gen_code(discard, TRUE); 

    /*
     *   If we discarded the result, we generated a JT which explicitly
     *   popped a value.  If we didn't discard the result, we generated a
     *   JST; this may or may not pop the value.  However, if it doesn't
     *   pop the value (save on true), it will bypass the right side
     *   evaluation, and will thus "pop" that value in the sense that it
     *   will never be pushed.  So, note a pop either way.  
     */
    G_cg->note_pop();

    /* define the label for the jump over the rhs */
    def_label_pos(lbl);

    /* 
     *   If the result is not going to be used directly for a condition, we
     *   must boolean-ize the value.  This isn't necessary if both of the
     *   operands are already guaranteed to be boolean values.  
     */
    if (!for_condition && !(left_->is_bool() && right_->is_bool()))
        G_cg->write_op(OPC_BOOLIZE);
}

/*
 *   Generate code for the short-circuit OR when used in a condition.  We can
 *   use the fact that we're being used conditionally to avoid actually
 *   pushing the result value onto the stack, instead simply branching to the
 *   appropriate point in the enclosing control structure instead.  
 */
void CTPNOr::gen_code_cond(CTcCodeLabel *then_label,
                           CTcCodeLabel *else_label)
{
    CTcCodeLabel *internal_then;

    /*
     *   First, generate the conditional code for our left operand.  If the
     *   condition is true, we can short-circuit the rest of the expression
     *   by jumping directly to the 'then' label.  If the caller provided a
     *   'then' label, we can jump directly to the caller's 'then' label;
     *   otherwise, we must synthesize our own internal label, which we'll
     *   define at the end of our generated code so that we'll fall through
     *   on true to the enclosing code.  In any case, we want to fall through
     *   if the condition is false, so that control will flow to the code for
     *   our right operand if the left operand is false.  
     */
    internal_then = (then_label == 0 ? G_cs->new_label_fwd() : then_label);
    left_->gen_code_cond(internal_then, 0);

    /* 
     *   Now, generate code for our right operand.  We can generate this code
     *   using the caller's destination labels directly: if we reach this
     *   code at all, it's because the left operand was false, in which case
     *   the result is simply the value of the right operand.  
     */
    right_->gen_code_cond(then_label, else_label);

    /* 
     *   If we created an internal 'then' label, it goes at the end of our
     *   generated code: this ensures that we fall off the end of our code
     *   if the left subexpression is true, which is what the caller told us
     *   they wanted when they gave us a null 'then' label.  If the caller
     *   gave us an explicit 'then' label, we'll have jumped there directly
     *   if the first subexpression was true.  
     */
    if (then_label == 0)
        def_label_pos(internal_then);
}

/* ------------------------------------------------------------------------ */
/*
 *   logical AND (short-circuit logic)
 */
void CTPNAnd::gen_code(int discard, int for_condition)
{
    /* 
     *   first, evaluate the left-hand side; we need the result even if
     *   we're discarding the overall expression, since we will check the
     *   result to see if we should even evaluate the right-hand side 
     */
    left_->gen_code(FALSE, TRUE);
 
    /* 
     *   If the left-hand side is false, there's no need to evaluate the
     *   right-hand side (and, in fact, we're not even allowed to evaluate
     *   the right-hand side because of the short-circuit logic rule).
     *   So, if the lhs is false, we want to jump around the code to
     *   evaluate the rhs, saving the false result if we're not discarding
     *   the overall result.  
     */
    CTcCodeLabel *lbl = gen_jump_ahead(discard ? OPC_JF : OPC_JSF);

    /* 
     *   Evaluate the right-hand side.  We don't need to save the result
     *   unless we need the result of the overall expression.  
     */
    right_->gen_code(discard, TRUE);
 
    /* define the label for the jump over the rhs */
    def_label_pos(lbl);

    /*
     *   If we discarded the result, we generated a JF which explicitly
     *   popped a value.  If we didn't discard the result, we generated a
     *   JSF; this may or may not pop the value.  However, if it doesn't
     *   pop the value (save on false), it will bypass the right side
     *   evaluation, and will thus "pop" that value in the sense that it
     *   will never be pushed.  So, note a pop either way.  
     */
    G_cg->note_pop();

    /* 
     *   If the result is not going to be used directly for a condition, we
     *   must boolean-ize the value.  This isn't necessary if both of the
     *   sub-expressions are already guaranteed to be boolean values.  
     */
    if (!for_condition && !(left_->is_bool() && right_->is_bool()))
        G_cg->write_op(OPC_BOOLIZE);
}

/*
 *   Generate code for the short-circuit AND when used in a condition.  We
 *   can use the fact that we're being used conditionally to avoid actually
 *   pushing the result value onto the stack, instead simply branching to the
 *   appropriate point in the enclosing control structure instead.  
 */
void CTPNAnd::gen_code_cond(CTcCodeLabel *then_label,
                            CTcCodeLabel *else_label)
{
    CTcCodeLabel *internal_else;

    /*
     *   First, generate the conditional code for our left operand.  If the
     *   condition is false, we can short-circuit the rest of the expression
     *   by jumping directly to the 'else' label.  If the caller provided an
     *   'else' label, we can jump directly to the caller's 'else' label;
     *   otherwise, we must synthesize our own internal label, which we'll
     *   define at the end of our generated code so that we'll fall through
     *   on false to the enclosing code.  In any case, we want to fall
     *   through if the condition is true, so that control will flow to the
     *   code for our right operand if the left operand is true.  
     */
    internal_else = (else_label == 0 ? G_cs->new_label_fwd() : else_label);
    left_->gen_code_cond(0, internal_else);

    /* 
     *   Now, generate code for our right operand.  We can generate this code
     *   using the caller's destination labels directly: if we reach this
     *   code at all, it's because the left operand was true, in which case
     *   the result is simply the value of the right operand.  
     */
    right_->gen_code_cond(then_label, else_label);

    /* 
     *   If we created an internal 'else' label, it goes at the end of our
     *   generated code: this ensures that we fall off the end of our code
     *   if the left subexpression is false, which is what the caller told
     *   us they wanted when they gave us a null 'else' label.  If the
     *   caller gave us an explicit 'else' label, we'll have jumped there
     *   directly if the first subexpression was false.  
     */
    if (else_label == 0)
        def_label_pos(internal_else);
}


/* ------------------------------------------------------------------------ */
/*
 *   bitwise OR
 */
void CTPNBOr::gen_code(int discard, int)
{
    gen_binary(OPC_BOR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise AND
 */
void CTPNBAnd::gen_code(int discard, int)
{
    gen_binary(OPC_BAND, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise XOR
 */
void CTPNBXor::gen_code(int discard, int)
{
    gen_binary(OPC_XOR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   greater-than
 */
void CTPNGt::gen_code(int discard, int)
{
    gen_binary(OPC_GT, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   greater-or-equal
 */
void CTPNGe::gen_code(int discard, int)
{
    gen_binary(OPC_GE, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   less-than
 */
void CTPNLt::gen_code(int discard, int)
{
    gen_binary(OPC_LT, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   less-or-equal
 */
void CTPNLe::gen_code(int discard, int)
{
    gen_binary(OPC_LE, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   compare for equality
 */
void CTPNEq::gen_code(int discard, int)
{
    gen_binary(OPC_EQ, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   compare for inequality
 */
void CTPNNe::gen_code(int discard, int)
{
    gen_binary(OPC_NE, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   'is in' 
 */
void CTPNIsIn::gen_code(int discard, int)
{
    CTPNArglist *lst;
    CTPNArg *arg;
    CTcCodeLabel *lbl_found;
    CTcCodeLabel *lbl_done;

    /* allocate our 'found' label */
    lbl_found = G_cs->new_label_fwd();

    /* 
     *   allocate our 'done' label - we only need to do this if we don't
     *   have a constant true value and we're not discarding the result
     */
    if (!const_true_ && !discard)
        lbl_done = G_cs->new_label_fwd();

    /* generate my left-side expression */
    left_->gen_code(FALSE, FALSE);

    /* the right side is always an argument list */
    lst = (CTPNArglist *)right_;

    /* compare to each element in the list on the right */
    for (arg = lst->get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg())
    {
        /* 
         *   duplicate the left-side value, so we don't have to generate
         *   it again for this comparison 
         */
        G_cg->write_op(OPC_DUP);

        /* generate this list element */
        arg->gen_code(FALSE, FALSE);

        /* if they're equal, jump to the 'found' label */
        G_cg->write_op(OPC_JE);
        G_cs->write_ofs2(lbl_found, 0);

        /* we pushed one more (DUP) and popped two (JE) */
        G_cg->note_push(1);
        G_cg->note_pop(2);
    }

    /* 
     *   Generate the code that comes at the end of all of tests when we
     *   fail to find any matches - we simply discard the left-side value
     *   from the stack, push our 'nil' value, and jump to the end label.
     *   
     *   If we have a constant 'true' value, there's no need to do any of
     *   this, because we know that, even after testing all of our
     *   non-constant values, there's a constant value that makes the
     *   entire expression true, and we can thus just fall through to the
     *   'found' code.
     *   
     *   If we're discarding the result, there's no need to push a
     *   separate value for the result, so we can just fall through to the
     *   common ending code in this case.  
     */
    if (!const_true_ && !discard)
    {
        G_cg->write_op(OPC_DISC);
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->write_op(OPC_JMP);
        G_cs->write_ofs2(lbl_done, 0);
    }

    /* 
     *   Generate the 'found' code - this discards the left-side value and
     *   pushes our 'true' result.  Note that there's no reason to push
     *   our result if we're discarding it.  
     */
    def_label_pos(lbl_found);
    G_cg->write_op(OPC_DISC);

    /* 
     *   if we're discarding the result, just note the pop of the left
     *   value; otherwise, push our result 
     */
    if (discard)
        G_cg->note_pop();
    else
        G_cg->write_op(OPC_PUSHTRUE);

    /* our 'done' label is here, if we needed one */
    if (!const_true_ && !discard)
        def_label_pos(lbl_done);
}

/* ------------------------------------------------------------------------ */
/*
 *   'not in' 
 */
void CTPNNotIn::gen_code(int discard, int)
{
    CTPNArglist *lst;
    CTPNArg *arg;
    CTcCodeLabel *lbl_found;
    CTcCodeLabel *lbl_done;

    /* allocate our 'found' label */
    lbl_found = G_cs->new_label_fwd();

    /* 
     *   allocate our 'done' label - we only need to do this if we don't
     *   have a constant false value 
     */
    if (!const_false_ && !discard)
        lbl_done = G_cs->new_label_fwd();

    /* generate my left-side expression */
    left_->gen_code(FALSE, FALSE);

    /* the right side is always an argument list */
    lst = (CTPNArglist *)right_;

    /* compare to each element in the list on the right */
    for (arg = lst->get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg())
    {
        /* 
         *   duplicate the left-side value, so we don't have to generate
         *   it again for this comparison 
         */
        G_cg->write_op(OPC_DUP);

        /* generate this list element */
        arg->gen_code(FALSE, FALSE);

        /* if they're equal, jump to the 'found' label */
        G_cg->write_op(OPC_JE);
        G_cs->write_ofs2(lbl_found, 0);

        /* we pushed one more (DUP) and popped two (JE) */
        G_cg->note_push(1);
        G_cg->note_pop(2);
    }

    /* 
     *   Generate the code that comes at the end of all of tests when we
     *   fail to find any matches - we simply discard the left-side value
     *   from the stack, push our 'true' value, and jump to the end label.
     *   
     *   If we have a constant 'nil' value, however, there's no need to do
     *   any of this, because we know that, even after testing all of our
     *   non-constant values, there's a matching constant value that makes
     *   the entire expression false (because 'not in' is false if we find
     *   a match), and we can thus just fall through to the 'found' code.  
     */
    if (!const_false_ && !discard)
    {
        G_cg->write_op(OPC_DISC);
        G_cg->write_op(OPC_PUSHTRUE);
        G_cg->write_op(OPC_JMP);
        G_cs->write_ofs2(lbl_done, 0);
    }

    /* 
     *   generate the 'found' code - this discards the left-side value and
     *   pushes our 'nil' result (because the result of 'not in' is false
     *   if we found the value) 
     */
    def_label_pos(lbl_found);
    G_cg->write_op(OPC_DISC);

    /* push the result, or note the pop if we're just discarding it */
    if (discard)
        G_cg->note_pop();
    else
        G_cg->write_op(OPC_PUSHNIL);

    /* our 'done' label is here, if we needed one */
    if (!const_false_ && !discard)
        def_label_pos(lbl_done);
}

/* ------------------------------------------------------------------------ */
/*
 *   bit-shift left
 */
void CTPNShl::gen_code(int discard, int)
{
    gen_binary(OPC_SHL, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   arithmetic shift right '>>'
 */
void CTPNAShr::gen_code(int discard, int)
{
    gen_binary(OPC_ASHR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   logical shift right '>>>'
 */
void CTPNLShr::gen_code(int discard, int)
{
    gen_binary(OPC_LSHR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   multiply
 */
void CTPNMul::gen_code(int discard, int)
{
    /* if either side is zero or one, we can apply special handling */
    if (left_->is_const_int(0))
    {
        /* evaluate the right for side effects and discard the result */
        right_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();

        /* done */
        return;
    }
    else if (right_->is_const_int(0))
    {
        /* evaluate the left for side effects and discard the result */
        left_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();

        /* done */
        return;
    }
    else if (left_->is_const_int(1))
    {
        /* 
         *   evaluate the right side - it's the result; note that, because
         *   of the explicit multiplication, we must compute logical
         *   results using assignment (not 'for condition') rules 
         */
        right_->gen_code(discard, FALSE);

        /* done */
        return;
    }
    else if (right_->is_const_int(1))
    {
        /* evaluate the right side - it's the result */
        left_->gen_code(discard, FALSE);

        /* done */
        return;
    }

    /* apply generic handling */
    gen_binary(OPC_MUL, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   divide
 */
void CTPNDiv::gen_code(int discard, int for_cond)
{
    /* if dividing by 1, we can skip the whole thing (except side effects) */
    if (right_->is_const_int(1))
    {
        /* 
         *   simply generate the left side for side effects; actually
         *   doing the arithmetic has no effect 
         */
        left_->gen_code(discard, for_cond);
        return;
    }

    /* if the left side is zero, the result is always zero */
    if (left_->is_const_int(0))
    {
        /* evaluate the right for side effects, but discard the result */
        right_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        return;
    }

    /* use generic code generation */
    gen_binary(OPC_DIV, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   modulo
 */
void CTPNMod::gen_code(int discard, int for_condition)
{
    /* if dividing by 1, we can skip the whole thing (except side effects) */
    if (right_->is_const_int(1))
    {
        /* 
         *   simply generate the left side for side effects; actually
         *   doing the arithmetic has no effect 
         */
        left_->gen_code(discard, for_condition);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        return;
    }

    /* if the left side is zero, the result is always zero */
    if (left_->is_const_int(0))
    {
        /* evaluate the right for side effects, but discard the result */
        right_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        return;
    }

    /* use generic processing */
    gen_binary(OPC_MOD, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   subtract
 */
void CTPNSub::gen_code(int discard, int for_cond)
{
    /* check for subtracting 1, which we can accomplish more efficiently */
    if (right_->is_const_int(1))
    {
        /* 
         *   We're subtracting one - use decrement.  The decrement
         *   operator itself has no side effects, so we can pass through
         *   the 'discard' status to the subnode.  
         */
        left_->gen_code(discard, FALSE);

        /* apply decrement if we're not discarding the result */
        if (!discard)
            G_cg->write_op(OPC_DEC);
    }
    else
    {
        /* we can't do anything special - use the general-purpose code */
        gen_binary(OPC_SUB, discard, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   add
 */
void CTPNAdd::gen_code(int discard, int)
{
    /* check for adding 1, which we can accomplish more efficiently */
    if (right_->is_const_int(1))
    {
        /* 
         *   We're adding one - use increment.  The increment operator
         *   itself has no side effects, so we can pass through the
         *   'discard' status to the subnode.  
         */
        left_->gen_code(discard, FALSE);
        
        /* apply increment if we're not discarding the result */
        if (!discard)
            G_cg->write_op(OPC_INC);
    }
    else
    {
        /* we can't do anything special - use the general-purpose code */
        gen_binary(OPC_ADD, discard, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   simple assignment
 */
void CTPNAsi::gen_code(int discard, int)
{
    /* 
     *   Ask the left subnode to generate a simple assignment to the value
     *   on the right.  Simple assignments cannot be refused, so we don't
     *   need to try to do any assignment work ourselves.  
     */
    left_->gen_code_asi(discard, 1, TC_ASI_SIMPLE, "=",
                        right_, FALSE, TRUE, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   add and assign
 */
void CTPNAddAsi::gen_code(int discard, int)
{
    /* use the static handler */
    CTPNAddAsi_gen_code(left_, right_, discard);
}

/* static handler, for sharing with other node types */
void CTPNAddAsi_gen_code(CTcPrsNode *left, CTcPrsNode *right, int discard)
{
    /* 
     *   ask the left subnode to generate an add-and-assign; if it can't,
     *   handle it generically 
     */
    void *ctx;
    if (!left->gen_code_asi(discard, 1, TC_ASI_ADD, "+=",
                            right, FALSE, TRUE, &ctx))
    {
        /* 
         *   There's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused.  We've already generated
         *   the left side above, so generate the right side.  
         */
        right->gen_code(FALSE, FALSE);

        /* generate the add (which pops two and adds one -> net one pop) */
        G_cg->write_op(OPC_ADD);
        G_cg->note_pop();

        /* generate the simple assignment to the lhs */
        left->gen_code_asi(discard, 2, TC_ASI_ADD, "+=",
                           0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   subtract and assign
 */
void CTPNSubAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a subtract-and-assign; if it
     *   can't, handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_SUB, "-=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_SUB);
        left_->gen_code_asi(discard, 2, TC_ASI_SUB, "-=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   multiply and assign
 */
void CTPNMulAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a multiply-and-assign; if it
     *   can't, handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_MUL, "*=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_MUL);
        left_->gen_code_asi(discard, 2, TC_ASI_MUL, "*=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   divide and assign
 */
void CTPNDivAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a divide-and-assign; if it
     *   can't, handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_DIV, "/=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_DIV);
        left_->gen_code_asi(discard, 2, TC_ASI_DIV, "/=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   modulo and assign
 */
void CTPNModAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a mod-and-assign; if it can't,
     *   handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_MOD, "%=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_MOD);
        left_->gen_code_asi(discard, 2, TC_ASI_MOD, "%=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise-AND and assign
 */
void CTPNBAndAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an AND-and-assign; if it can't,
     *   handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_BAND, "&=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_BAND);
        left_->gen_code_asi(discard, 2, TC_ASI_BAND, "&=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise-OR and assign
 */
void CTPNBOrAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an OR-and-assign; if it can't,
     *   handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_BOR, "|=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_BOR);
        left_->gen_code_asi(discard, 2, TC_ASI_BOR, "|=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise-XOR and assign
 */
void CTPNBXorAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an XOR-and-assign; if it can't,
     *   handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_BXOR, "^=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_XOR);
        left_->gen_code_asi(discard, 2, TC_ASI_BXOR, "^=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bit-shift left and assign
 */
void CTPNShlAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an shift-left-and-assign; if it
     *   can't, handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_SHL, "<<=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_SHL);
        left_->gen_code_asi(discard, 2, TC_ASI_SHL, "<<=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   arithmetic shift right and assign '>>='
 */
void CTPNAShrAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a shift-right-and-assign; if it
     *   can't, handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_ASHR, ">>=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_ASHR);
        left_->gen_code_asi(discard, 2, TC_ASI_ASHR, ">>=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   logical shift right and assign '>>>=' 
 */
void CTPNLShrAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a shift-right-and-assign; if it
     *   can't, handle it generically 
     */
    void *ctx;
    if (!left_->gen_code_asi(discard, 1, TC_ASI_LSHR, ">>>=",
                             right_, FALSE, TRUE, &ctx))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary_ca(OPC_LSHR);
        left_->gen_code_asi(discard, 2, TC_ASI_LSHR, ">>>=",
                            0, FALSE, TRUE, &ctx);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   subscript a list/array value
 */
void CTPNSubscript::gen_code(int discard, int)
{
    gen_binary(OPC_INDEX, discard, FALSE);
}

/*
 *   assign to a subscripted value
 */
int CTPNSubscript::gen_code_asi(int discard, int phase,
                                tc_asitype_t typ, const char *op,
                                CTcPrsNode *rhs,
                                int, int xplicit, void **ctx)
{
    void *sctx;

    /* 
     *   An assignment to a subscript expression is not what it appears.
     *   It's actually a compound assignment that evaluates the subscript
     *   expression, assigns the RHS, and then assigns the *result* of that
     *   first assignment to the left operand of the index operator.  In
     *   other words, a[b]=c is effectively rewritten as this:
     *   
     *.     a = a.operator[]=(b, c)
     *   
     *   which, by analogy with the other compound assignment operators, we
     *   can rewrite as
     *   
     *.     a []== (b, c)
     */
    if (typ == TC_ASI_SIMPLE)
    {
        int rhs_depth;
        int extra_disc = 0;
        
        /*
         *   Simple assignment.  Generate the value to assign to the element
         *   - that's the right side of the assignment operator.  If rhs is
         *   null, it means the caller has already done this.  
         */
        if (rhs != 0)
        {
            /* generate code for the RHS */
            rhs->gen_code(FALSE, FALSE);
        }
        
        /* 
         *   if we're not discarding the result, duplicate the value to be
         *   assigned, so that we leave a copy on the stack after we're
         *   finished.  This is necessary because the SETIND instruction will
         *   consume one copy of the value.
         */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }
        
        /* note the stack depth where the RHS value resides */
        rhs_depth = G_cg->get_sp_depth();

        /* 
         *   Generate the value to be subscripted - that's my left-hand side.
         *   Generate this as phase 1 of a two-phase assignment, with the
         *   special pseudo-operator '[]='.  Ignore the return value; no one
         *   can fully handle this type of assignment in phase 1.  Also, note
         *   that we don't yet have the right-hand value to assign - it's the
         *   result of the SETIND yet to come.  
         */
        left_->gen_code_asi(FALSE, 1, TC_ASI_IDX, op,
                            0, TRUE, xplicit, &sctx);

        /*
         *   The phase 1 []= code could have left context information on the
         *   stack that it will need in phase 2, in addition to the LHS value
         *   that we need for the SETIND we're about to generate.  For the
         *   SETIND, we need the RHS value at index 1, just above the LHS
         *   value.  So if the new depth is more than 1 greater than the RHS
         *   depth, we need to do some stack rearrangement.
         *   
         *   If phase 1 left *nothing* on the stack, it means that the value
         *   being indexed can't handle assignments at all, as in
         *   self[i]=val.  This is acceptable: we can still perform the []=
         *   portion to assign to the indexed element, but we won't be able
         *   to do the second part, which is assigning the result of the []=
         *   operator to the lhs of the [] operator.  We'll simply ignore
         *   that second part in this case.
         */
        if (G_cg->get_sp_depth() == rhs_depth)
        {
            /* 
             *   the lhs of [] is not assignable; simply generate the lhs so
             *   that we can index it 
             */
            left_->gen_code(FALSE, FALSE);
        }
        else if (G_cg->get_sp_depth() > rhs_depth + 1)
        {
            /* figure the distance to the RHS */
            int dist = G_cg->get_sp_depth() - rhs_depth;

            /* the instruction can only accommodate a distance up to 255 */
            if (dist > 255)
                G_tok->log_error(TCERR_EXPR_TOO_COMPLEX);

            /* re-push the RHS */
            G_cg->write_op(OPC_GETSPN);
            G_cs->write((uchar)dist);
            G_cg->note_push();

            /* 
             *   swap it with the left operand of the [] to get everything in
             *   the proper order 
             */
            G_cg->write_op(OPC_SWAP);

            /* 
             *   the original RHS we pushed is now superfluous, so we'll need
             *   to explicitly pop it when we're done 
             */
            extra_disc += 1;
        }

        /* generate the index value - that's my right-hand side */
        right_->gen_code(FALSE, FALSE);

        /* generate the assign-to-indexed-value opcode */
        G_cg->write_op(OPC_SETIND);
        
        /* setind pops three and pushes one - net of pop 2 */
        G_cg->note_pop(2);

        /*
         *   The top value now on the stack is the new container value.  The
         *   new container will be different from the old container in some
         *   cases (with lists, for example, because we must create a new
         *   list object to contain the modified list value).  Therefore, if
         *   my left-hand side is an lvalue, we must assign the new container
         *   to the left-hand side - this makes something like "x[1] = 5"
         *   actually change the value in "x" if "x" is a local variable.  If
         *   my left-hand side isn't an lvalue, don't bother with this step,
         *   and simply discard the new container value.
         *   
         *   Handle the assignment to the new container as phase 2 of the
         *   '[]=' pseudo-operator that we set up above.  This ensures that
         *   the left operand had a chance to save any sub-expression values,
         *   which it can now reuse.
         *   
         *   Regardless of whether we're keeping the result of the overall
         *   expression, we're definitely not keeping the result of assigning
         *   the new container - the result of the assignment is the value
         *   assigned, not the container.  Thus, discard = true in this call.
         *   
         *   
         *   There's a special case that's handled through the peep-hole
         *   optimizer: if we are assigning to a local variable and indexing
         *   with a constant integer value, we will have converted the whole
         *   operation to a SETINDLCL1I8.  That instruction takes care of
         *   assigning the value back to the rvalue, so we don't need to
         *   generate a separate rvalue assignment.  
         */
        if (G_cg->get_last_op() == OPC_SETINDLCL1I8)
        {
            /* 
             *   no assignment is necessary - we just need to account for the
             *   difference in the stack arrangement with this form of the
             *   assignment, which is that we don't leave the container value
             *   on the stack 
             */
            G_cg->note_pop();
        }
        else if (!left_->gen_code_asi(TRUE, 2, TC_ASI_IDX, op,
                                      0, TRUE, xplicit, &sctx))
        {
            /* no assignment is possible; discard the new container value */
            G_cg->write_op(OPC_DISC);
            G_cg->note_pop();
        }

        /* discard any extra stack elements */
        while (extra_disc-- != 0)
        {
            G_cg->write_op(OPC_DISC);
            G_cg->note_pop();
        }

        /* handled */
        return TRUE;
    }
    else if (phase == 1)
    {
        /* 
         *   Compound assignment, phase 1.  Generate the lvalue's value, and
         *   also leave the indexee and index values on the stack separately
         *   for use later in doing the assignment to indexee[index].  
         */
        
        /* 
         *   Generate the value to be subscripted - that's my left-hand side.
         *   Generate it as phase 1 of assignment through my pseudo-operator
         *   []=. 
         */
        left_->gen_code_asi(FALSE, 1, TC_ASI_IDX, op,
                            0, TRUE, xplicit, &sctx);

        /* 
         *   save the context in the caller's context holder, so that it'll
         *   get passed back to us on the second pass 
         */
        *ctx = sctx;

        /* generate the index value - that's my right-hand side */
        right_->gen_code(FALSE, FALSE);

        /* duplicate them both so we have them later, for the assignment */
        G_cg->write_op(OPC_DUP2);
        G_cg->note_push(2);

        /* generate the INDEX operator to compute indexee[index] */
        G_cg->write_op(OPC_INDEX);
        G_cg->note_pop();

        /*
         *   If the caller pushed a RHS value for its own use in assigning,
         */

        /* tell the caller to proceed (i.e., we didn't finish the job) */
        return FALSE;
    }
    else if (phase == 2)
    {
        /*
         *   Compound assignment, phase 2.  We now have the combined value to
         *   be assigned on the stack, followed by the indexee and index
         *   values on the stack.  
         */

        /* restore the left-side phase 1 context from the caller */
        sctx = *ctx;

        /* if the assigned value will be further assigned, save a copy */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }

        /* 
         *   The stack is now (bottom to top) INDEXEE INDEX RHS RHS (if
         *   !discard) or INDEXEE INDEX RHS (if discard).  SETIND needs it to
         *   be (RHS) RHS INDEXEE INDEX.  Swap the top one or two elements
         *   with the next two to rearrange into SETIDX order.  
         */
        if (discard)
        {
            /* INDEXEE INDEX RHS -> RHS INDEX INDEXEE */
            G_cg->write_op(OPC_SWAPN);
            G_cs->write(0);
            G_cs->write(2);

            /* -> RHS INDEXEE INDEX */
            G_cg->write_op(OPC_SWAP);
        }
        else
        {
            /* swap INDEXEE INDEX with RHS RHS */
            G_cg->write_op(OPC_SWAP2);
        }

        /* assign the rhs to the indexed value */
        G_cg->write_op(OPC_SETIND);
        G_cg->note_pop(2);

        /* 
         *   The new container value is now on the stack; assign it to our
         *   left-hand side.  Note that the SETINDLCL1I8 optimization (see
         *   the simple assignment case above) is impossible here, because we
         *   had to generate the code to carry out the combination operator
         *   (the '+' in '+=', for example) before the SETIND.  There's thus
         *   nothing for the SETIND to combine with.
         *   
         *   Generate as phase 2 of the []= assignment we started in phase 1.
         */
        if (!left_->gen_code_asi(TRUE, 2, TC_ASI_IDX, op,
                                 0, TRUE, xplicit, &sctx))
        {
            /* no assignment was necessary; discard the container value */
            G_cg->write_op(OPC_DISC);
            G_cg->note_pop();
        }

        /* handled */
        return TRUE;
    }
    else
    {
        /* ignore any other phases */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   If-nil operator 
 */
void CTPNIfnil::gen_code(int discard, int for_condition)
{
    /* generate the first operand */
    first_->gen_code(FALSE, FALSE);

    /* check to see if we're keeping the result or merely testing it */
    if (!discard)
    {
        /* we're keep the result - make a copy for the JNOTNIL */
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }

    /* if it's not nil, we're done - jump to the end of the operator */
    CTcCodeLabel *lbl_end = gen_jump_ahead(OPC_JNOTNIL);
    G_cg->note_pop();

    /* 
     *   We're on the 'nil' branch now, meaning we're not going to yield the
     *   first value after all.  If we're yielding anything, discard the
     *   extra yielding copy we made of the first value. 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_DISC);
        G_cg->note_pop();
    }

    /* generate the 'nil' branch, yielding the second operand */
    second_->gen_code(discard, for_condition);

    /* this is the end of the test - generate the end label */
    def_label_pos(lbl_end);
}

/* ------------------------------------------------------------------------ */
/*
 *   conditional operator
 */
void CTPNIf::gen_code(int discard, int for_condition)
{
    /* 
     *   Generate the condition value - we need the value regardless of
     *   whether the overall result is going to be used, because we need
     *   it to determine which branch to take.  Generate the subexpression
     *   for a condition, so that we don't perform any extra unnecessary
     *   conversions on it.  
     */
    first_->gen_code(FALSE, TRUE);
 
    /* if the condition is false, jump to the 'else' expression part */
    CTcCodeLabel *lbl_else = gen_jump_ahead(OPC_JF);

    /* JF pops a value */
    G_cg->note_pop();

    /* 
     *   Generate the 'then' expression part.  Only request a return value if
     *   it has one AND we're not discarding it.  If it doesn't return a
     *   value, and we actually need one, we'll supply a default 'nil' value
     *   next.  This value will be our yielded value (in this branch,
     *   anyway), so pass through the for-condition flag.  
     */
    second_->gen_code(discard || !second_->has_return_value(), for_condition);

    /* 
     *   If this expression has no return value, and we need the return
     *   value, supply nil as the result.
     */
    if (!discard && !second_->has_return_value())
    {
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }
 
    /* unconditionally jump over the 'else' part */
    CTcCodeLabel *lbl_end = gen_jump_ahead(OPC_JMP);

    /* set the label for the 'else' part */
    def_label_pos(lbl_else);

    /* 
     *   Generate the 'else' part.  Only request a return value if it has one
     *   AND we're not discarding it.  Pass through 'discard' and
     *   'for_condition', since this result is our result.  
     */
    third_->gen_code(discard || !third_->has_return_value(), for_condition);

    /* 
     *   If this expression has no return value, and we need the return
     *   value, supply nil as the result.  
     */
    if (!discard && !third_->has_return_value())
    {
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }
 
    /* 
     *   Because of the jump, we only evaluate one of the two expressions
     *   we generated, so note an extra pop for the branch we didn't take.
     *   Note that if either one pushes a value, both will, since we'll
     *   explicitly have pushed nil for the one that doesn't generate a
     *   value to keep the stack balanced on both branches.
     *   
     *   If neither of our expressions yields a value, don't pop anything
     *   extra, since we won't think we've pushed two values in the course
     *   of generating the two expressions.  
     */
    if (second_->has_return_value() || third_->has_return_value())
        G_cg->note_pop();
 
    /* set the label for the end of the expression */
    def_label_pos(lbl_end);
}

/* ------------------------------------------------------------------------ */
/*
 *   symbol
 */
void CTPNSym::gen_code(int discard, int)
{
    /* 
     *   Look up the symbol; if it's undefined, add a default property
     *   symbol entry if possible.  Then ask the symbol to generate the
     *   code.  
     */
    G_cs->get_symtab()
        ->find_or_def_prop_implied(get_sym_text(), get_sym_text_len(),
                                   FALSE, G_cs->is_self_available())
        ->gen_code(discard);
}

/*
 *   assign to a symbol
 */
int CTPNSym::gen_code_asi(int discard, int phase,
                          tc_asitype_t typ, const char *op,
                          CTcPrsNode *rhs,
                          int ignore_errors, int xplicit, void **ctx)
{
    /* 
     *   Look up the symbol; if it's undefined and there's a "self" object
     *   available, define it as a property by default, since a property
     *   is the only kind of symbol that we could possibly assign to
     *   without having defined anywhere in the program.  Once we have the
     *   symbol, tell it to generate the code for assigning to it.  
     */
    return G_cs->get_symtab()
        ->find_or_def_prop_implied(get_sym_text(), get_sym_text_len(),
                                   FALSE, G_cs->is_self_available())
        ->gen_code_asi(discard, phase, typ, op, rhs,
                       ignore_errors, xplicit, ctx);
}

/*
 *   take the address of the symbol 
 */
void CTPNSym::gen_code_addr()
{
    /* 
     *   Look up our symbol in the global symbol table, then ask the
     *   resulting symbol to generate the appropriate code.  If the symbol
     *   isn't defined, assume it's a property.
     *   
     *   Note that we look only in the global symbol table, because local
     *   symbols have no address value.  So, even if the symbol is defined in
     *   the local table, ignore the local definition and look at the global
     *   definition.  
     */
    G_prs->get_global_symtab()
        ->find_or_def_prop_explicit(get_sym_text(), get_sym_text_len(),
                                    FALSE)
        ->gen_code_addr();
}

/*
 *   call the symbol 
 */
void CTPNSym::gen_code_call(int discard, int argc, int varargs,
                            CTcNamedArgs *named_args)
{
    /*
     *   Look up our symbol in the symbol table, then ask the resulting
     *   symbol to generate the appropriate call.  The symbol is
     *   implicitly a property (if in a method context), since that's the
     *   only kind of undefined symbol that we could be calling.  
     */
    G_cs->get_symtab()
        ->find_or_def_prop_implied(get_sym_text(), get_sym_text_len(),
                                   FALSE, G_cs->is_self_available())
        ->gen_code_call(discard, argc, varargs, named_args);
}

/*
 *   generate code for 'new' 
 */
void CTPNSym::gen_code_new(int discard, int argc, int varargs,
                           CTcNamedArgs *named_args,
                           int /*from_call*/, int is_transient)
{
    /*
     *   Look up our symbol, then ask the resulting symbol to generate the
     *   'new' code.  If the symbol is undefined, add an 'undefined' entry
     *   to the table; we can't implicitly create an object symbol. 
     */
    G_cs->get_symtab()
        ->find_or_def_undef(get_sym_text(), get_sym_text_len(), FALSE)
        ->gen_code_new(discard, argc, varargs, named_args, is_transient);
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTPNSym::gen_code_propid(int check_only, int is_expr)
{
    CTcSymbol *sym;
    CTcPrsSymtab *symtab;

    /*
     *   Figure out where to look for the symbol.  If the symbol was given
     *   as an expression (in other words, it was explicitly enclosed in
     *   parentheses), look it up in the local symbol table, since it
     *   could refer to a local.  Otherwise, it must refer to a property,
     *   so look only in the global table.
     *   
     *   If the symbol isn't defined already, define it as a property now.
     *   Because the symbol is explicitly on the right side of a member
     *   evaluation, we can define it as a property whether or not there's
     *   a valid "self" in this context.  
     */
    if (is_expr)
    {
        /* it's an expression - look it up in the local symbol table */
        symtab = G_cs->get_symtab();
    }
    else
    {
        /* it's a simple symbol - look only in the global symbol table */
        symtab = G_prs->get_global_symtab();
    }

    /* 
     *   look it up (note that this will always return a valid symbol,
     *   since it will create one if we can't find an existing entry) 
     */
    sym = symtab->find_or_def_prop(get_sym_text(), get_sym_text_len(), FALSE);

    /* ask the symbol to generate the property reference */
    return sym->gen_code_propid(check_only, is_expr);
}

/*
 *   generate code for a member expression 
 */
void CTPNSym::gen_code_member(int discard, CTcPrsNode *prop_expr,
                              int prop_is_expr,
                              int argc, int varargs, CTcNamedArgs *named_args)
{
    /* 
     *   Look up the symbol, and let it do the work.  There's no
     *   appropriate default for the symbol, so leave it undefined if we
     *   can't find it. 
     */
    G_cs->get_symtab()
        ->find_or_def_undef(get_sym_text(), get_sym_text_len(), FALSE)
        ->gen_code_member(discard, prop_expr, prop_is_expr,
                          argc, varargs, named_args);
}

/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTPNSym::gen_code_obj_predot(int *is_self)
{
    /* 
     *   Look up the symbol, and let it do the work.  There's no default
     *   type for the symbol, so leave it undefined if we don't find it. 
     */
    return G_cs->get_symtab()
        ->find_or_def_undef(get_sym_text(), get_sym_text_len(), FALSE)
        ->gen_code_obj_predot(is_self);
}

/* ------------------------------------------------------------------------ */
/*
 *   resolved symbol 
 */
void CTPNSymResolved::gen_code(int discard, int)
{
    /* let the symbol handle it */
    sym_->gen_code(discard);
}

/*
 *   assign to a symbol 
 */
int CTPNSymResolved::gen_code_asi(int discard, int phase,
                                  tc_asitype_t typ, const char *op,
                                  CTcPrsNode *rhs,
                                  int ignore_errors, int xplicit, void **ctx)
{
    /* let the symbol handle it */
    return sym_->gen_code_asi(discard, phase, typ, op, rhs, 
                              ignore_errors, xplicit, ctx);
}

/*
 *   take the address of the symbol 
 */
void CTPNSymResolved::gen_code_addr()
{
    /* let the symbol handle it */
    sym_->gen_code_addr();
}

/*
 *   call the symbol 
 */
void CTPNSymResolved::gen_code_call(int discard, int argc, int varargs,
                                    CTcNamedArgs *named_args)
{
    /* let the symbol handle it */
    sym_->gen_code_call(discard, argc, varargs, named_args);
}

/*
 *   generate code for 'new' 
 */
void CTPNSymResolved::gen_code_new(int discard, int argc,
                                   int varargs, CTcNamedArgs *named_args,
                                   int /*from_call*/, int is_transient)
{
    /* let the symbol handle it */
    sym_->gen_code_new(discard, argc, varargs, named_args, is_transient);
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTPNSymResolved::gen_code_propid(int check_only, int is_expr)
{
    /* let the symbol handle it */
    return sym_->gen_code_propid(check_only, is_expr);
}

/*
 *   generate code for a member expression 
 */
void CTPNSymResolved::gen_code_member(int discard, 
                                      CTcPrsNode *prop_expr, int prop_is_expr,
                                      int argc, int varargs,
                                      CTcNamedArgs *named_args)
{
    /* let the symbol handle it */
    sym_->gen_code_member(discard, prop_expr, prop_is_expr,
                          argc, varargs, named_args);
}

/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTPNSymResolved::gen_code_obj_predot(int *is_self)
{
    /* let the symbol handle it */
    return sym_->gen_code_obj_predot(is_self);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger local variable symbol 
 */

/*
 *   generate code to evaluate the variable
 */
void CTPNSymDebugLocal::gen_code(int discard, int for_condition)
{
    /* if we're not discarding the value, push the local */
    if (!discard)
    {
        /* generate the debugger local/parameter variable instruction */
        G_cg->write_op(is_param_ ? OPC_GETDBARG : OPC_GETDBLCL);
        G_cs->write2(var_id_);
        G_cs->write2(frame_idx_);

        /* note that we pushed the value */
        G_cg->note_push();

        /* if it's a context local, get the value from the context array */
        if (ctx_arr_idx_ != 0)
        {
            CTPNConst::s_gen_code_int(ctx_arr_idx_);
            G_cg->write_op(OPC_INDEX);

            /* 
             *   the 'index' operation pops two values and pushes one, for a
             *   net of one pop 
             */
            G_cg->note_pop();
        }
    }
}

/*
 *   generate code for assigning to this variable 
 */
int CTPNSymDebugLocal::gen_code_asi(int discard, int phase,
                                    tc_asitype_t typ, const char *,
                                    CTcPrsNode *rhs,
                                    int, int, void **)
{
    /* check what we're doing */
    if (typ == TC_ASI_SIMPLE || phase == 2)
    {        
        /* 
         *   Simple assignment, or phase 2 of a compound assignment.  In
         *   either case, just assign the rvalue to the variable.  
         */
        if (rhs != 0)
            rhs->gen_code(FALSE, FALSE);

        /* 
         *   if we're not discarding the result, duplicate the value so we'll
         *   have a copy after the assignment 
         */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }
        
        /* check for a context property */
        if (ctx_arr_idx_ == 0)
        {
            /* 
             *   generate the debug-local-set instruction - the operands are
             *   the variable number and the stack frame index 
             */
            G_cg->write_op(is_param_ ? OPC_SETDBARG : OPC_SETDBLCL);
            G_cs->write2(var_id_);
            G_cs->write2(frame_idx_);
        }
        else
        {
            /* get the local containing our context object */
            G_cg->write_op(OPC_GETDBLCL);
            G_cs->write2(var_id_);
            G_cs->write2(frame_idx_);
            
            /* set the actual variable value in the context object */
            CTPNConst::s_gen_code_int(ctx_arr_idx_);
            G_cg->write_op(OPC_SETIND);
            G_cg->write_op(OPC_DISC);
            
            /* 
             *   we did three pops (SETIND), then a push (SETIND), then a pop
             *   (DISC) - this is a net of three extra pops 
             */
            G_cg->note_pop(3);
        }
        
        /* the debug-local-set removes the rvalue from the stack */
        G_cg->note_pop();
        
        /* handled */
        return TRUE;
    }
    else if (phase == 1)
    {
        /* 
         *   Compound assignment, phase 1.  Simply generate our value and let
         *   the caller proceed with the generic combination operator. 
         */
        gen_code(FALSE, FALSE);
        return FALSE;
    }
    else
    {
        /* ignore other phases */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted string.  The 'discard' status is irrelevant, because we
 *   evaluate double-quoted strings for their side effects.  
 */
void CTPNDstr::gen_code(int discard, int)
{
    /* if we're not discarding the value, it's an error */
    if (!discard)
        G_tok->log_error(TCERR_DQUOTE_IN_EXPR, (int)len_, str_);

    /* generate the instruction to display it */
    G_cg->write_op(OPC_SAY);

    /* add the string to the constant pool, creating a fixup here */
    G_cg->add_const_str(str_, len_, G_cs, G_cs->get_ofs());

    /* write a placeholder value, which will be corrected by the fixup */
    G_cs->write4(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted debug string 
 */
void CTPNDebugDstr::gen_code(int, int)
{
    /* generate code to push the in-line string */
    G_cg->write_op(OPC_PUSHSTRI);
    G_cs->write2(len_);
    G_cs->write(str_, len_);

    /* write code to display the value */
    G_cg->write_op(OPC_SAYVAL);

    /* note that we pushed the string and then popped it */
    G_cg->note_push();
    G_cg->note_pop();
}

/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted string embedding 
 */

/*
 *   create an embedding 
 */
CTPNDstrEmbed::CTPNDstrEmbed(CTcPrsNode *sub)
    : CTPNDstrEmbedBase(sub)
{
}

/*
 *   Generate code for a double-quoted string embedding 
 */
void CTPNDstrEmbed::gen_code(int, int)
{
    /* note the stack depth before generating the expression */
    int orig_depth = G_cg->get_sp_depth();
    
    /* 
     *   Generate code for the embedded expression.  If the expression has a
     *   return value, generate the value so that it can be displayed in the
     *   string; but don't request a value if it doesn't have one, as a
     *   return value is optional in this context.  This is a normal value
     *   invocation, not a conditional, so we need any applicable normal
     *   value conversions.  
     */
    sub_->gen_code(!sub_->has_return_value(), FALSE);

    /* 
     *   If the code generation left anything on the stack, generate code
     *   to display the value via the default display function.  
     */
    if (G_cg->get_sp_depth() > orig_depth)
    {
        /* add a SAYVAL instruction */
        G_cg->write_op(OPC_SAYVAL);

        /* SAYVAL pops the argument value */
        G_cg->note_pop();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   embedded <<one of>> list 
 */
void CTPNStrOneOf::gen_code(int discard, int for_condition)
{
    /*
     *   There are two components to the <<one of>> list.  First, there's the
     *   list of strings.  Second, there's an anonymous OneOfIndexGen object
     *   that we create; this object holds our state and generates our index
     *   values.
     *   
     *   In the simple case, the list is just a constant list of constant
     *   strings.  However, if the enclosing string is a dstring, each of
     *   these will be a dstring; and even if the outer string is an sstring,
     *   these could contain further embedded expressions, in which case they
     *   won't be constant.
     *   
     *   If the list is constant, we generate list[generator.getNextIndex()].
     *   This selects a string from the list and leaves it on the stack,
     *   which is just what our parent node wants.
     *   
     *   If the list isn't constant, we generate a switch on
     *   generator.getNextIndex(), and then fill in the case table from the
     *   list: each case label is the index, and each case's body is the code
     *   generated for the list element, followed by a 'break' to the end of
     *   the switch.  The effect once again is to pick an item from the list
     *   and push its value onto the stack.  
     */

    /* look up the getNextIndex property */
    CTcSymProp *propsym = (CTcSymProp *)G_prs->get_global_symtab()->
                          find_or_def_prop_explicit("getNextIndex", 12, FALSE);
    if (propsym->get_type() != TC_SYM_PROP)
    {
        G_tok->log_error(TCERR_ONEOF_REQ_GETNXT);
        return;
    }

    /* synthesize a constant value for the property */
    CTcConstVal propc;
    propc.set_prop(propsym->get_prop());
    CTPNConst *getNextIndex = new CTPNConst(&propc);

    /* generate the list */
    CTcConstVal *c = lst_->get_const_val();
    if (c != 0)
    {
        /* the list is a constant - push the list value */
        lst_->gen_code(FALSE, FALSE);

        /* call generator.getNextIndex() to get the next index */
        state_obj_->gen_code_member(FALSE, getNextIndex, FALSE, 0, FALSE, 0);

        /* index the list (this pops two values and pushes one) */
        G_cg->write_op(OPC_INDEX);
        G_cg->note_pop();
    }
    else
    {
        /* the list is non-constant - start by generating the index */
        state_obj_->gen_code_member(FALSE, getNextIndex, FALSE, 0, FALSE, 0);

        /* note the number of list elements */
        int i, cnt = lst_->get_count();

        /* generate the SWITCH <case_cnt> */
        G_cg->write_op(OPC_SWITCH);
        G_cs->write2(cnt);
        G_cg->note_pop();

        /* 
         *   Build the case table.  Each label is an index value; leave the
         *   jump offset zero for now, since we'll have to fill it in as we
         *   generate the code. 
         */
        ulong caseofs = G_cs->get_ofs();
        for (i = 1 ; i <= cnt ; ++i)
        {
            /* write the case label as the 1-based index value */
            char buf[VMB_DATAHOLDER];
            vm_val_t val;
            val.set_int(i);
            vmb_put_dh(buf, &val);
            G_cs->write(buf, VMB_DATAHOLDER);

            /* write a placeholder for the jump offset */
            G_cs->write2(0);
        }

        /* write a placeholder for the default case */
        G_cs->write2(0);

        /* create a forward label for the 'break' to the end of the switch */
        CTcCodeLabel *brklbl = G_cs->new_label_fwd();

        /* 
         *   Now run through the table and generate the code for each list
         *   element. 
         */
        CTPNListEle *ele;
        for (i = 1, ele = lst_->get_head() ; ele != 0 && i <= cnt ;
             ++i, caseofs += VMB_DATAHOLDER + 2, ele = ele->get_next())
        {
            /* set the jump to this case in the case table */
            G_cs->write2_at(caseofs + VMB_DATAHOLDER,
                            G_cs->get_ofs() - (caseofs + VMB_DATAHOLDER));

            /* generate the code for this list element */
            ele->get_expr()->gen_code(discard, for_condition);;

            /* generate a jump to the end of the table */
            G_cg->write_op(OPC_JMP);
            G_cs->write_ofs2(brklbl, 0);

            /* 
             *   since all of these branches proceed in parallel, only one
             *   push counts, so don't count every iteration - pretend that
             *   we've already popped the value that this case generated 
             */
            if (!discard)
                G_cg->note_pop();
        }

        /* write the default case - just push nil */
        G_cs->write2_at(caseofs, G_cs->get_ofs() - caseofs);
        if (!discard)
        {
            G_cg->write_op(OPC_PUSHNIL);
            G_cg->note_push();
        }

        /* this is the end of the switch - set the 'break' label here */
        def_label_pos(brklbl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Create or define an internal compiler-defined function in the program. 
 */
static CTcSymFunc *get_internal_func(const char *nm,
                                     int argc, int opt_argc, int varargs,
                                     int has_retval)
{
    /* get the symbol length */
    size_t len = strlen(nm);

    /* look up the function symbol */
    CTcSymFunc *fn =
        (CTcSymFunc *)G_prs->get_global_symtab()->find_delete_weak(nm, len);
    if (fn == 0)
    {
        /* it's not defined - create an implied declaration for it */
        fn = new CTcSymFunc(nm, len, FALSE, argc, opt_argc, varargs,
                            has_retval, FALSE, FALSE, TRUE, TRUE);

        /* add it to the global symbol table */
        G_prs->get_global_symtab()->add_entry(fn);
    }
    else if (fn->get_type() != TC_SYM_FUNC)
    {
        /* it's defined, but not as a function - this is an error */
        G_tok->log_error(TCERR_REDEF_AS_FUNC, (int)len, nm);
        fn = 0;
    }

    /* return the function */
    return fn;
}

/*
 *   Get an internal built-in function by name 
 */
static CTcSymBif *get_internal_bif(const char *nm, int err)
{
    /* look up the symbol */
    size_t len = strlen(nm);
    CTcSymBif *fn = (CTcSymBif *)G_prs->get_global_symtab()->find(nm, len);

    /* if it wasn't found, flag an error */
    if (fn == 0)
        G_tok->log_error(err, (int)len, nm);

    /* return the function */
    return fn;
}

/*
 *   Call an internal built-in function by name.  This is for simple calls
 *   with fixed arguments and no named parameters, keeping the return value.
 */
static void call_internal_bif(const char *nm, int err, int argc)
{
    /* look up the function */
    CTcSymBif *fn = get_internal_bif(nm, err);

    /* if we got it, call it */
    if (fn != 0)
        fn->gen_code_call(FALSE, argc, FALSE, 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Argument list
 */
void CTPNArglist::gen_code_arglist(int *varargs, CTcNamedArgs &named_args)
{
    CTPNArg *arg;
    int i;
    int fixed_cnt;
    int pushed_varargs_counter;

    /* 
     *   scan the argument list for varargs - if we have any, we must
     *   treat all of them as varargs 
     */
    for (*varargs = FALSE, fixed_cnt = named_args.cnt = 0,
         named_args.args = this, arg = get_arg_list_head() ;
         arg != 0 ;
         arg = arg->get_next_arg())
    {
        /* if this is a varargs argument, we have varargs */
        if (arg->is_varargs())
        {
            /* note it */
            *varargs = TRUE;
        }
        else if (arg->is_named_param())
        {
            /* count the named argument */
            named_args.cnt += 1;
        }
        else
        {
            /* count another fixed positional argument */
            ++fixed_cnt;
        }
    }

    /*
     *   If we have named arguments, push them ahead of the position
     *   arguments.  
     */
    if (named_args.cnt != 0)
    {
        /* 
         *   Run through the argument list again, evaluating only named
         *   arguments on this pass.
         */
        for (i = argc_, arg = get_arg_list_head() ; arg != 0 ;
             arg = arg->get_next_arg(), --i)
        {
            /* if it's a named argument, push it */
            if (arg->is_named_param())
                arg->gen_code(FALSE, FALSE);
        }
    }

    /* 
     *   Push each positional argument in the list.  The arguments on the
     *   stack go in right-to-left order.  The parser builds the internal
     *   list of argument expressions in reverse order, so we must merely
     *   follow the list from head to tail.
     *   
     *   We need each argument value to be pushed (hence discard = false),
     *   and we need the assignable value of each argument expression (hence
     *   for_condition = false).  
     */
    for (pushed_varargs_counter = FALSE, i = argc_,
         arg = get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg(), --i)
    {
        int depth;

        /* skip named arguments on this pass */
        if (arg->is_named_param())
            continue;

        /* note the stack depth before generating the value */
        depth = G_cg->get_sp_depth();

        /* 
         *   check for varargs - if this is first varargs argument, push
         *   the counter placeholder 
         */
        if (arg->is_varargs() && !pushed_varargs_counter)
        {
            /* 
             *   write code to push the fixed argument count - we can use
             *   this as a starting point, since we always know we have
             *   this many argument to start with; we'll dynamically add
             *   in the variable count at run-time 
             */
            CTPNConst::s_gen_code_int(fixed_cnt);
            
            /* note that we've pushed the counter */
            pushed_varargs_counter = TRUE;

            /* 
             *   we will take the extra value off when we evaluate the
             *   varargs counter, so simply count it as removed now 
             */
            G_cg->note_pop();
        }

        /* generate the argument's code */
        arg->gen_code(FALSE, FALSE);

        /* 
         *   if we've pushed the variable argument counter value onto the
         *   stack, and this a fixed argument, swap the top two stack
         *   elements to get the argument counter back to the top of the
         *   stack; if this is a varargs argument there's no need, since
         *   it will have taken care of this 
         */
        if (pushed_varargs_counter && !arg->is_varargs())
            G_cg->write_op(OPC_SWAP);

        /* ensure that it generated something */
        if (G_cg->get_sp_depth() <= depth)
            G_tok->log_error(TCERR_ARG_EXPR_HAS_NO_VAL, i);
    }
}

/* 
 *   ------------------------------------------------------------------------
 */
/*
 *   argument list entry
 */
void CTPNArg::gen_code(int, int)
{
    /* 
     *   Generate the argument expression.  We need the value (hence
     *   discard = false), and we need the assignable value (hence
     *   for_condition = false). 
     */
    get_arg_expr()->gen_code(FALSE, FALSE);

    /* 
     *   if this is a list-to-varargs conversion, generate the conversion
     *   instruction 
     */
    if (is_varargs_)
    {
        /* write the opcode */
        G_cg->write_op(OPC_MAKELSTPAR);

        /* note the extra push and pop for the argument count */
        G_cg->note_push();
        G_cg->note_pop();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   function/method call
 */

/*
 *   create 
 */
CTPNCall::CTPNCall(CTcPrsNode *func, class CTPNArglist *arglist)
    : CTPNCallBase(func, arglist)
{
    /* the T3 instruction set limits calls to 127 arguments */
    if (arglist->get_argc() > 127)
        G_tok->log_error(TCERR_TOO_MANY_CALL_ARGS);
}


/*
 *   generate code 
 */
void CTPNCall::gen_code(int discard, int for_condition)
{
    /* check for special functions */
    if (get_func()->sym_text_matches("rand", 4)
        && gen_code_rand(discard, for_condition))
        return;

    /* push the argument list */
    int varargs;
    CTcNamedArgs named_args;
    get_arg_list()->gen_code_arglist(&varargs, named_args);

    /* generate an appropriate call instruction */
    get_func()->gen_code_call(discard, get_arg_list()->get_argc(),
                              varargs, &named_args);
}

/*
 *   Generate code for a call to 'rand'.  If we have multiple arguments,
 *   rand() is special because it doesn't evaluate all of its arguments.
 *   Instead, it picks an argument at random, and evaluates only that single
 *   selected argument, yielding the result.  
 */
int CTPNCall::gen_code_rand(int discard, int for_condition)
{
    /* get our argument list, and the number of arguments */
    CTPNArglist *args = get_arg_list();
    int argc = args->get_argc();

    /* if we have zero or one argument, use the normal call */
    if (argc < 2)
        return FALSE;

    /* if we have variable arguments, use the normal call */
    CTPNArg *arg;
    for (arg = args->get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg())
    {
        if (arg->is_varargs())
            return FALSE;
    }

    /* 
     *   We have a fixed number of arguments greater than one, so this is the
     *   special form of the rand() call.  First, generate an ordinary call
     *   to rand() to select a number from 0 to argc-1.
     */
    CTcNamedArgs named_args;
    named_args.cnt = 0;
    CTPNConst::s_gen_code_int(argc);
    get_func()->gen_code_call(FALSE, 1, FALSE, &named_args);

    /* generate the SWITCH <case_cnt> */
    G_cg->write_op(OPC_SWITCH);
    G_cs->write2(argc);
    G_cg->note_pop();

    /* 
     *   Build the case table.  The rand() call gave us an integer from 0 to
     *   argc-1 inclusive, so our case labels are simply the argument index
     *   values, starting at zero.  Note that the argument list is stored
     *   right to left, so the index values of the list elements run from
     *   argc-1 down to 0.  Since we're making a random selection anyway the
     *   order shouldn't really matter; however, for the sake of regression
     *   tests for pre-3.1 games, we want to use the same index ordering that
     *   rand() did in the old version, where rand() itself chose one of its
     *   pre-evaluated arguments.  When running in regression mode with a
     *   fixed random number stream, using the same index ordering will
     *   ensure that we get the same results when running existing tests.  
     */
    ulong caseofs = G_cs->get_ofs();
    int i;
    for (i = argc - 1 ; i >= 0 ; --i)
    {
        /* write the case label as the 0-based argument index */
        char buf[VMB_DATAHOLDER];
        vm_val_t val;
        val.set_int(i);
        vmb_put_dh(buf, &val);
        G_cs->write(buf, VMB_DATAHOLDER);

        /* write a placeholder for the jump offset */
        G_cs->write2(0);
    }

    /* write a placeholder for the default case */
    G_cs->write2(0);

    /* create a forward label for the 'break' to the end of the switch */
    CTcCodeLabel *brklbl = G_cs->new_label_fwd();

    /* generate the argument expressions */
    for (arg = args->get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg(), caseofs += VMB_DATAHOLDER + 2)
    {
        /* set the jump to this case in the case table */
        G_cs->write2_at(caseofs + VMB_DATAHOLDER,
                        G_cs->get_ofs() - (caseofs + VMB_DATAHOLDER));

        /* generate the argument expression */
        arg->get_arg_expr()->gen_code(discard, for_condition);

        /* generate a jump to the end of the table */
        G_cg->write_op(OPC_JMP);
        G_cs->write_ofs2(brklbl, 0);

        /* we generate the branches in parallel, so only one push counts */
        if (!discard)
            G_cg->note_pop();
    }

    /* write the default case - just push nil if we need a value */
    G_cs->write2_at(caseofs, G_cs->get_ofs() - caseofs);
    if (!discard)
    {
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }

    /* this is the end of the switch - set the 'break' label here */
    def_label_pos(brklbl);

    /* tell the caller that we handled the code generation */
    return TRUE;
}

/*
 *   Generate code for operator 'new'.  A 'new' with an argument list
 *   looks like a function call: NEW(CALL(object-contents, ARGLIST(...))).
 */
void CTPNCall::gen_code_new(int discard, int argc, int varargs,
                            CTcNamedArgs *named_args,
                            int from_call, int is_transient)
{
    /* 
     *   if this is a recursive call from another 'call' node, it's not
     *   allowed - we'd be trying to use the result of a call as the base
     *   class of the 'new', which is illegal 
     */
    if (from_call)
    {
        G_tok->log_error(TCERR_INVAL_NEW_EXPR);
        return;
    }

    /* use our own named_args structure if the caller didn't provide one */
    CTcNamedArgs my_named_args;
    if (named_args == 0)
        named_args = &my_named_args;
    
    /* generate the argument list */
    get_arg_list()->gen_code_arglist(&varargs, *named_args);

    /* generate the code for the 'new' call */
    get_func()->gen_code_new(discard, get_arg_list()->get_argc(),
                             varargs, named_args, TRUE, is_transient);
}


/* ------------------------------------------------------------------------ */
/*
 *   member property evaluation
 */
void CTPNMember::gen_code(int discard, int)
{
    /* ask the object expression to generate the code */
    get_obj_expr()->gen_code_member(discard, get_prop_expr(), prop_is_expr_,
                                    0, FALSE, 0);
}

/* context for two-phase compound assignment to a member expression */
struct member_ca_ctx
{
    member_ca_ctx(int is_self, vm_obj_id_t obj, vm_prop_id_t prop)
    {
        this->is_self = is_self;
        this->obj = obj;
        this->prop = prop;
    }
    
    int is_self;
    vm_obj_id_t obj;
    vm_prop_id_t prop;
};

/*
 *   assign to member expression
 */
int CTPNMember::gen_code_asi(int discard, int phase,
                             tc_asitype_t typ, const char *op,
                             CTcPrsNode *rhs,
                             int ignore_errors, int, void **ctx)
{
    int is_self;
    vm_obj_id_t obj;
    vm_prop_id_t prop;

    /* if this is phase 2 of a compound assignment, retrieve the context */
    if (phase == 2)
    {
        /* cast the generic context pointer to our structure */
        member_ca_ctx *cctx = (member_ca_ctx *)*ctx;

        /* retrieve the context information */
        is_self = cctx->is_self;
        obj = cctx->obj;
        prop = cctx->prop;

        /* we're done with the context object - delete it */
        delete cctx;
    }

    /* 
     *   if this is a simple assignment, start by generating the right-hand
     *   side, unless the caller has already done so 
     */
    if (typ == TC_ASI_SIMPLE && rhs != 0)
        rhs->gen_code(FALSE, FALSE);

    /* 
     *   If this is a simple assignment, or it's phase 2 of a two-phase
     *   assignment, we now have the RHS at top of stack, and we're about to
     *   perform the actual assignment.  If the caller wants to further use
     *   the assigned value, push a copy, since we'll consume the RHS value
     *   currently on the stack with our SETPROP or related instruction. 
     */
    if (!discard && (typ == TC_ASI_SIMPLE || phase == 2))
    {
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }

    /*
     *   If this is a simple assignment, or phase 1 of a two-phase compound
     *   assignment, generate the object and property expressions. 
     */
    if (typ == TC_ASI_SIMPLE || phase == 1)
    {
        /* 
         *   Determine what we have on the left: we could have self, a
         *   constant object value, or any other expression.  
         */
        obj = get_obj_expr()->gen_code_obj_predot(&is_self);
        
        /* 
         *   determine what kind of property expression we have - don't
         *   generate any code for now, since we may need to generate some
         *   more code ahead of the property generation 
         */
        prop = get_prop_expr()->gen_code_propid(TRUE, prop_is_expr_);
        
        /* determine what we need to do based on the operands */
        if (prop == VM_INVALID_PROP)
        {
            /* 
             *   We're assigning through a property pointer -- we must
             *   generate a PTRSETPROP instruction.
             *   
             *   Before we generate the property expression, we must generate
             *   the object expression.  If we got a constant object, we must
             *   generate code to push that object value; otherwise, the code
             *   to generate the object value is already generated.  
             */
            if (is_self)
            {
                /* self - generate code to push the "self" value */
                G_cg->write_op(OPC_PUSHSELF);
                G_cg->note_push();
            }
            else if (obj != VM_INVALID_OBJ)
            {
                /* constant object - generate code to push the value */
                G_cg->write_op(OPC_PUSHOBJ);
                G_cs->write_obj_id(obj);
                G_cg->note_push();
            }

            /* generate the property value expression */
            get_prop_expr()->gen_code_propid(FALSE, prop_is_expr_);
            
            /*
             *   If this is phase 1 of a two-phase compound assignment, we'll
             *   need the object and property values that we've just stacked
             *   again in phase 2.  Save separate copies via DUP2.  
             */
            if (typ != TC_ASI_SIMPLE)
            {
                G_cg->write_op(OPC_DUP2);
                G_cg->note_push(2);
            }
        }
        else
        {
            /* 
             *   We're assigning to a fixed property ID.  If the object
             *   expression was not 'self' or a fixed object ID, we have an
             *   object value on the stack; otherwise we have nothing on the
             *   stack.
             *   
             *   For a two-phase assignment, we'll need the object value
             *   again during the second phase, so if we did indeed stack a
             *   value, make a copy of it for our use in phase 2.  
             */
            if (typ != TC_ASI_SIMPLE && !is_self && obj == VM_INVALID_OBJ)
            {
                G_cg->write_op(OPC_DUP);
                G_cg->note_push();
            }
        }
    }

    /*
     *   If this is phase 1 of a two-phase assignment, stop here: we've
     *   stacked the necessary obj and prop information for the actual
     *   assignment codegen in phase 2, and we've stacked the obj.prop value
     *   for the combination operator to access.  Save context information
     *   and return, telling the caller to proceed with the unrolled
     *   operation.  
     */
    if (typ != TC_ASI_SIMPLE && phase == 1)
    {
        /* save the object and property information in the phase context */
        *ctx = new member_ca_ctx(is_self, obj, prop);

        /* evaluate the property */
        if (prop == VM_INVALID_PROP)
        {
            /* property pointer - call with zero arguments */
            G_cg->write_op(OPC_PTRCALLPROP);
            G_cs->write(0);

            /* this pops the object and property */
            G_cg->note_pop(2);
        }
        else
        {
            /* constant property */
            if (is_self)
            {
                /* get property of self */
                G_cg->write_op(OPC_GETPROPSELF);
                G_cs->write_prop_id(prop);
            }
            else if (obj != VM_INVALID_OBJ)
            {
                /* constant object */
                G_cg->write_op(OPC_OBJGETPROP);
                G_cs->write_obj_id(obj);
                G_cs->write_prop_id(prop);
            }
            else
            {
                /* it's an object expression */
                G_cg->write_op(OPC_GETPROP);
                G_cs->write_prop_id(prop);

                /* this pops the object value */
                G_cg->note_pop();
            }
        }

        /* all of these leave the value in R0 - retrieve it */
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();

        /* tell the caller to proceed with the unrolled assignment */
        return FALSE;
    }

    /*
     *   If we're doing a simple assignment, or this is phase 2 of a compound
     *   assignment, it's time to do the actual assignment. 
     */
    if (typ == TC_ASI_SIMPLE || phase == 2)
    {
        /* determine how to make the assignment based on the operands */
        if (prop == VM_INVALID_PROP)
        {
            /*
             *   If this is phase 2 of a compound assignment, get the stack
             *   back in order.  We currently have (from bottom to top) obj
             *   prop RHS RHS (if !discard), or obj prop RHS (if discard).
             *   Swap the top one or two elements for the next two.  
             */
            if (typ != TC_ASI_SIMPLE && phase == 2)
            {
                if (discard)
                {
                    /* obj prop RHS -> RHS prop obj */
                    G_cg->write_op(OPC_SWAPN);
                    G_cs->write(0);
                    G_cs->write(2);
                    
                    /* RHS prop obj -> RHS obj prop */
                    G_cg->write_op(OPC_SWAP);
                }
                else
                {
                    /* obj prop RHS RHS -> RHS RHS obj prop */
                    G_cg->write_op(OPC_SWAP2);
                }
            }
                
            /* generate the PTRSETPROP instruction */
            G_cg->write_op(OPC_PTRSETPROP);

            /* ptrsetprop removes three elements */
            G_cg->note_pop(3);
        }
        else
        {
            /* constant property value */

            /* 
             *   If this is phase 2, and we have an object on the stack, we
             *   need to rearrange the stack into the proper order:
             *   
             *   !discard: stack = (obj RHS RHS), and we need (RHS RHS obj).
             *   
             *   discard: stack = (obj RHS), and we need (obj RHS).  Just do
             *   a SWAP.
             *   
             *   If there's no object on the stack, there's no need to do any
             *   rearrangement.  
             */
            if (phase == 2 && !is_self && obj == VM_INVALID_OBJ)
            {
                if (discard)
                {
                    /* obj RHS -> RHS obj */
                    G_cg->write_op(OPC_SWAP);
                }
                else
                {
                    /* obj RHS RHS -> RHS RHS obj */
                    G_cg->write_op(OPC_SWAPN);
                    G_cs->write(0);
                    G_cs->write(2);
                }
            }

            /* 
             *   We have several instructions to choose from.  If we're
             *   assigning to a property of "self", use SETPROPSELF.  If
             *   we're assigning to a constant object, use OBJSETPROP.
             *   Otherwise, use the plain SETPROP.
             */
            if (is_self)
            {
                /* write the SETPROPSELF */
                G_cg->write_op(OPC_SETPROPSELF);
                G_cs->write_prop_id(prop);

                /* setpropself removes the RHS value */
                G_cg->note_pop();
            }
            else if (obj != VM_INVALID_OBJ)
            {
                /* write the OBJSETPROP */
                G_cg->write_op(OPC_OBJSETPROP);
                G_cs->write_obj_id(obj);
                G_cs->write_prop_id(prop);

                /* objsetprop removes the RHS value */
                G_cg->note_pop();
            }
            else
            {
                /* 
                 *   write the normal SETPROP; we already generated the code
                 *   to push the object value, so it's where it should be 
                 */
                G_cg->write_op(OPC_SETPROP);
                G_cs->write_prop_id(prop);
                
                /* setprop removes the value and the object */
                G_cg->note_pop(2);
            }
        }
    }

    /* we're done with the operation */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   member with argument list
 */
void CTPNMemArg::gen_code(int discard, int)
{
    /* push the argument list */
    int varargs;
    CTcNamedArgs named_args;
    get_arg_list()->gen_code_arglist(&varargs, named_args);

    /* ask the object expression to generate the code */
    get_obj_expr()->gen_code_member(discard, get_prop_expr(), prop_is_expr_,
                                    get_arg_list()->get_argc(),
                                    varargs, &named_args);
}

/* ------------------------------------------------------------------------ */
/*
 *   construct a list
 */
void CTPNList::gen_code(int discard, int for_condition)
{
    /*
     *   Before we construct the list dynamically, check to see if the
     *   list is constant.  If it is, we need only built the list in the
     *   constant pool, and push its offset.  
     */
    if (is_const())
    {
        /* push the value only if we're not discarding it */
        if (!discard)
        {
            /* write the instruction */
            G_cg->write_op(OPC_PUSHLST);

            /* add the list to the constant pool */
            G_cg->add_const_list(this, G_cs, G_cs->get_ofs());

            /* 
             *   write a placeholder address, which will be corrected by
             *   the fixup that add_const_list() created 
             */
            G_cs->write4(0);

            /* note the push */
            G_cg->note_push();
        }
    }
    else
    {
        /*
         *   It's not a constant list, so we must generate code to construct
         *   a list dynamically.  Push each element of the list.  We need
         *   each value (hence discard = false), and we require the
         *   assignable value of each expression (hence for_condition =
         *   false).  Push the argument list in reverse order, since the
         *   run-time metaclass requires this ordering.  
         */
        for (CTPNListEle *ele = get_tail() ; ele != 0 ; ele = ele->get_prev())
            ele->gen_code(FALSE, FALSE);

        /* generate a NEW instruction for an object of metaclass LIST */
        if (get_count() <= 255)
        {
            /* the count will fit in one byte - use the short form */
            G_cg->write_op(OPC_NEW1);
            G_cs->write((char)get_count());
            G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
        }
        else
        {
            /* count doesn't fit in one byte - use the long form */
            G_cg->write_op(OPC_NEW2);
            G_cs->write2(get_count());
            G_cs->write2(G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
        }
        
        /* new1/new2 remove arguments */
        G_cg->note_pop(get_count());

        /* if we're not discarding the value, push it */
        if (!discard)
        {
            G_cg->write_op(OPC_GETR0);
            G_cg->note_push();
        }
    }

    /*
     *   If this is a lookup table list, and we're not discarding the value,
     *   create the actual LookupTable object from the list source.  If we're
     *   discarding the value there's no need to create the object, since
     *   doing so will have no meaningful side effects.  
     */
    if (!discard && is_lookup_table_)
    {
        /* 
         *   To construct the lookup table, we simply pass the list of [key,
         *   value, key, value...] elements to the LookupTable constructor.
         *   The list is already on the stack, so simply call the constructor
         *   with one argument.  
         */
        G_cg->write_op(OPC_NEW1);
        G_cs->write(1);
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_LOOKUP_TABLE));

        /* NEW1 removes arguments */
        G_cg->note_pop(1);

        /* push the result */
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   list element
 */
void CTPNListEle::gen_code(int discard, int for_condition)
{
    /* generate the subexpression */
    expr_->gen_code(discard, for_condition);
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic T3-specific symbol class 
 */

/*
 *   generate code to take the address of a symbol - in general, we cannot
 *   take the address of a symbol, so we'll just log an error
 */
void CTcSymbol::gen_code_addr()
{
    G_tok->log_error(TCERR_NO_ADDR_SYM, (int)get_sym_len(), get_sym());
}

/*
 *   generate code to assign to the symbol - in general, we cannot assign
 *   to a symbol, so we'll just log an error 
 */
int CTcSymbol::gen_code_asi(int, int phase, tc_asitype_t, const char *,
                            class CTcPrsNode *,
                            int ignore_error, int, void **)
{
    /* 
     *   If we're ignoring errors, simply return false to indicate that
     *   nothing happened.  Ignore errors on phase 2, since we'll already
     *   have generated an error on phase 1.  
     */
    if (ignore_error || phase > 1)
        return FALSE;

    /* log the error */
    G_tok->log_error(TCERR_CANNOT_ASSIGN_SYM, (int)get_sym_len(), get_sym());

    /* 
     *   even though we didn't generate anything, this has been fully
     *   handled - the caller shouldn't attempt to generate any additional
     *   code for this 
     */
    return TRUE;
}

/*
 *   Generate code for calling the symbol.  By default, we can't call a
 *   symbol. 
 */
void CTcSymbol::gen_code_call(int, int, int, CTcNamedArgs *)
{
    /* log an error */
    G_tok->log_error(TCERR_CANNOT_CALL_SYM, (int)get_sym_len(), get_sym());
}

/*
 *   Generate code for operator 'new' 
 */
void CTcSymbol::gen_code_new(int, int, int, CTcNamedArgs *, int)
{
    G_tok->log_error(TCERR_INVAL_NEW_EXPR);
}

/* 
 *   evaluate a property ID 
 */
vm_prop_id_t CTcSymbol::gen_code_propid(int check_only, int is_expr)
{
    /* by default, a symbol cannot be used as a property ID */
    if (!check_only)
        G_tok->log_error(TCERR_SYM_NOT_PROP, (int)get_sym_len(), get_sym());

    /* we can't return a valid property ID */
    return VM_INVALID_PROP;
}

/*
 *   evaluate a member expression 
 */
void CTcSymbol::gen_code_member(int discard,
                                CTcPrsNode *prop_expr, int prop_is_expr,
                                int argc, int varargs,
                                CTcNamedArgs *named_args)
{
    /* by default, a symbol cannot be used as an object expression */
    G_tok->log_error(TCERR_SYM_NOT_OBJ, (int)get_sym_len(), get_sym());
}

/*
 *   generate code for an object expression before a '.' 
 */
vm_obj_id_t CTcSymbol::gen_code_obj_predot(int *is_self)
{
    /* by default, a symbol cannot be used as an object expression */
    G_tok->log_error(TCERR_SYM_NOT_OBJ, (int)get_sym_len(), get_sym());

    /* indicate that we don't have a constant object */
    *is_self = FALSE;
    return VM_INVALID_OBJ;
}



/* ------------------------------------------------------------------------ */
/*
 *   T3-specific function symbol class 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymFunc::gen_code(int discard)
{
    /* 
     *   function address are always unknown during code generation;
     *   generate a placeholder instruction and add a fixup record for it 
     */
    G_cg->write_op(OPC_PUSHFNPTR);

    /* add a fixup for the current code location */
    add_abs_fixup(G_cs);

    /* write a placeholder offset - arbitrarily use zero */
    G_cs->write4(0);

    /* note the push */
    G_cg->note_push();
}

/*
 *   take the address of the function 
 */
void CTcSymFunc::gen_code_addr()
{
    /* '&function' is the same as 'function' with no arguments */
    gen_code(FALSE);
}


/*
 *   call the symbol 
 */
void CTcSymFunc::gen_code_call(int discard, int argc, int varargs,
                               CTcNamedArgs *named_args)
{
    /* we can't call a function that doesn't have a prototype */
    if (!has_proto_)
    {
        G_tok->log_error(TCERR_FUNC_CALL_NO_PROTO,
                         (int)get_sym_len(), get_sym());
        return;
    }

    /*
     *   If this is a multi-method base function, a call to the function is
     *   actually a call to _multiMethodCall('name', args).  
     */
    if (is_multimethod_base_)
    {
        /* make a list out of the arguments */
        if (varargs)
        {
            G_cg->write_op(OPC_VARARGC);
            G_cs->write((char)argc);
        }
        else if (argc <= 255)
        {
            G_cg->write_op(OPC_NEW1);
            G_cs->write((char)argc);
        }
        else
        {
            G_cg->write_op(OPC_NEW2);
            G_cs->write2(argc);
        }
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
        G_cg->note_pop(argc);

        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();

        /* add the base function pointer argument */
        CTcConstVal funcval;
        funcval.set_funcptr(this);
        CTPNConst func(&funcval);
        func.gen_code(FALSE, FALSE);

        /* look up _multiMethodCall */
        CTcSymFunc *mmc = get_internal_func(
            "_multiMethodCall", 1, 0, TRUE, TRUE);

        /* 
         *   Generate the call.  Note that there are always two arguments at
         *   this point: the base function pointer, and the argument list.
         *   The argument list is just one argument because we've already
         *   constructed a list out of it.  
         */
        if (mmc != 0)
            mmc->gen_code_call(discard, 2, FALSE, named_args);
    }
    else
    {
        /* write the varargs modifier if appropriate */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);

        /* generate the call instruction and argument count */
        G_cg->write_op(OPC_CALL);
        G_cs->write((char)argc);

        /* check the mode */
        if (G_cg->is_eval_for_dyn())
        {
            /* 
             *   dynamic (run-time) compilation - we know the absolute
             *   address already, since all symbols are pre-resolved in the
             *   debugger 
             */
            G_cs->write4(get_code_pool_addr());
        }
        else
        {
            /* 
             *   Normal compilation - we won't know the function's address
             *   until after generation is completed, so add a fixup for the
             *   current location, then write a placeholder for the offset
             *   field.  
             */
            add_abs_fixup(G_cs);
            G_cs->write4(0);
        }

        /* call removes arguments */
        G_cg->note_pop(argc);

        /* do post-call cleanup: named arg removal, etc */
        G_cg->post_call_cleanup(named_args);

        /* make sure the argument count is correct */
        if (!argc_ok(argc))
        {
            char buf[128];
            G_tok->log_error(TCERR_WRONG_ARGC_FOR_FUNC,
                             (int)get_sym_len(), get_sym(),
                             get_argc_desc(buf), argc);
        }

        /* if we're not discarding, push the return value from R0 */
        if (!discard)
        {
            G_cg->write_op(OPC_GETR0);
            G_cg->note_push();
        }
    }
}

/*
 *   Get my code pool address.  Valid only after linking. 
 */
ulong CTcSymFunc::get_code_pool_addr() const
{
    /* check for an absolute address */
    if (abs_addr_valid_)
    {
        /* 
         *   we have an absolute address - this means the symbol was
         *   loaded from a fully-linked image file (specifically, from the
         *   debug records) 
         */
        return abs_addr_;
    }
    else
    {
        /* 
         *   we don't have an absolute address, so our address must have
         *   been determined through a linking step - get the final
         *   address from the anchor 
         */
        return anchor_->get_addr();
    }
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymFunc::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;
    
    /* add an entry for our absolute address */
    val.set_fnptr(get_code_pool_addr());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific object symbol class 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymObj::gen_code(int discard)
{
    /* write code to push the object ID */
    if (!discard)
    {
        /* push the object */
        G_cg->write_op(OPC_PUSHOBJ);
        G_cs->write_obj_id(obj_id_);

        /* note the push */
        G_cg->note_push();
    }
}

/*
 *   take the address of the object
 */
void CTcSymObj::gen_code_addr()
{
    /* act as though we were pushing the object ID directly */
    gen_code(FALSE);
}

/*
 *   Generate a 'new' expression 
 */
void CTcSymObj::gen_code_new(int discard, int argc,
                             int varargs, CTcNamedArgs *named_args,
                             int is_transient)
{
    /* use our static generator */
    s_gen_code_new(discard, obj_id_, metaclass_,
                   argc, varargs, named_args, is_transient);
}

/*
 *   Generate a 'new' expression.  (This is a static method so that this
 *   code can be used by all of the possible expression types to which
 *   'new' can be applied.)
 *   
 *   This type of generation applies only to objects of metaclass TADS
 *   Object.  
 */
void CTcSymObj::s_gen_code_new(int discard, vm_obj_id_t obj_id,
                               tc_metaclass_t meta,
                               int argc, int varargs,
                               CTcNamedArgs *named_args,
                               int is_transient)
{
    /* 
     *   push the base class object - this is always the first argument
     *   (hence last pushed) to the metaclass constructor 
     */
    G_cg->write_op(OPC_PUSHOBJ);
    G_cs->write_obj_id(obj_id);

    /* note the push */
    G_cg->note_push();

    /* 
     *   note that we can only allow 126 arguments to a constructor,
     *   because we must add the implicit superclass argument 
     */
    if (argc > 126)
        G_tok->log_error(TCERR_TOO_MANY_CTOR_ARGS);

    /* 
     *   if we have varargs, swap the top stack elements to get the
     *   argument count back on top, and then generate the varargs
     *   modifier opcode 
     */
    if (varargs)
    {
        /* swap the top stack elements to get argc back to the top */
        G_cg->write_op(OPC_SWAP);

        /* 
         *   increment the argument count to account for the superclass
         *   object argument 
         */
        G_cg->write_op(OPC_INC);

        /* write the varargs modifier opcode */
        G_cg->write_op(OPC_VARARGC);
    }

    /* 
     *   write the NEW instruction - since we always add TADS Object to
     *   our metaclass table before we start compiling any code, we know
     *   it always has a small metaclass number and will always fit in the
     *   short form of the instruction
     *   
     *   Note that the actual argument count we generate is one higher
     *   than the source code argument list, because we add the implicit
     *   first argument to the metaclass constructor 
     */
    G_cg->write_op(is_transient ? OPC_TRNEW1 : OPC_NEW1);
    G_cs->write((char)(argc + 1));

    /* write out the dependency table index for the metaclass */
    switch (meta)
    {
    case TC_META_TADSOBJ:
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_TADSOBJ));
        break;

    default:
        /* we can't use 'new' on symbols of other metaclasses */
        G_tok->log_error(TCERR_BAD_META_FOR_NEW);
        G_cs->write(0);
        break;
    }

    /* new1 removes the arguments */
    G_cg->note_pop(argc + 1);

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* 
     *   if they're not discarding the value, push the new object
     *   reference, which will be in R0 when the constructor returns 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   Generate code for a member expression 
 */
void CTcSymObj::gen_code_member(int discard,
                                CTcPrsNode *prop_expr, int prop_is_expr,
                                int argc, int varargs,
                                CTcNamedArgs *named_args)
{
    s_gen_code_member(discard, prop_expr, prop_is_expr,
                      argc, obj_id_, varargs, named_args);
}

/*
 *   Static method to generate code for a member expression.  This is
 *   static so that constant object nodes can share it.  
 */
void CTcSymObj::s_gen_code_member(int discard,
                                  CTcPrsNode *prop_expr, int prop_is_expr,
                                  int argc, vm_obj_id_t obj_id,
                                  int varargs, CTcNamedArgs *named_args)
{
    vm_prop_id_t prop;

    /* 
     *   generate the property expression - don't generate the code right
     *   now even if code generation is necessary, because this isn't the
     *   right place for it; for now, simply check to determine if we're
     *   going to need to generate any code for the property expression 
     */
    prop = prop_expr->gen_code_propid(TRUE, prop_is_expr);

    /* don't allow method calls with arguments in speculative mode */
    if (argc != 0 && G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);
    
    /* check for a constant property value */
    if (prop != VM_INVALID_PROP)
    {
        /* generate an OBJGETPROP or OBJCALLPROP as appropriate */
        if (G_cg->is_speculative())
        {
            /* speculative evaluation - use GETPROPDATA */
            G_cg->write_op(OPC_PUSHOBJ);
            G_cs->write_obj_id(obj_id);
            G_cg->write_op(OPC_GETPROPDATA);
            G_cs->write_prop_id(prop);

            /* we pushed the object, then popped it */
            G_cg->note_push();
            G_cg->note_pop();
        }
        else if (argc == 0)
        {
            /* no arguments - use OBJGETPROP */
            G_cg->write_op(OPC_OBJGETPROP);
            G_cs->write_obj_id(obj_id);
            G_cs->write_prop_id(prop);
        }
        else
        {
            /* generate a varargs modifier if needed */
            if (varargs)
                G_cg->write_op(OPC_VARARGC);
            
            /* arguments - use OBJCALLPROP */
            G_cg->write_op(OPC_OBJCALLPROP);
            G_cs->write((char)argc);
            G_cs->write_obj_id(obj_id);
            G_cs->write_prop_id(prop);

            /* objcallprop removes arguments */
            G_cg->note_pop(argc);
        }
    }
    else
    {
        /* 
         *   non-constant property value - we must first push the object
         *   value, then push the property value, then write a PTRCALLPROP
         *   instruction 
         */

        /* generate the object push */
        G_cg->write_op(OPC_PUSHOBJ);
        G_cs->write_obj_id(obj_id);

        /* note the pushes */
        G_cg->note_push();

        /* keep the argument counter on top if necessary */
        if (varargs)
            G_cg->write_op(OPC_SWAP);

        /* generate the property push */
        prop_expr->gen_code_propid(FALSE, prop_is_expr);

        /* generate the PTRCALLPROP or PTRGETPROPDATA */
        if (G_cg->is_speculative())
        {
            /* speculative - use the data-only property evaluation */
            G_cg->write_op(OPC_PTRGETPROPDATA);
        }
        else
        {
            /* 
             *   if we have a varargs list, modify the call instruction
             *   that follows to make it a varargs call 
             */
            if (varargs)
            {
                /* swap to get the arg counter back on top */
                G_cg->write_op(OPC_SWAP);
                
                /* write the varargs modifier */
                G_cg->write_op(OPC_VARARGC);
            }
            
            /* normal - call the property */
            G_cg->write_op(OPC_PTRCALLPROP);
            G_cs->write((int)argc);
        }

        /* ptrcallprop removes the arguments, the object, and the property */
        G_cg->note_pop(argc + 2);
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if they want the result, push it onto the stack */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTcSymObj::gen_code_obj_predot(int *is_self)
{
    /* return our constant object reference */
    *is_self = FALSE;
    return obj_id_;
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymObj::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_obj(obj_id_);
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Invokable object 
 */
void CTcSymFuncObj::gen_code_call(int discard, int argc, int varargs,
                                  CTcNamedArgs *named_args)
{
    /* write the varargs modifier if appropriate */
    if (varargs)
        G_cg->write_op(OPC_VARARGC);

    /* push my value as the function pointer */
    gen_code(discard);

    /* generate the pointer-call instruction and argument count */
    G_cg->write_op(OPC_PTRCALL);
    G_cs->write((char)argc);

    /* ptrcall removes arguments and the function pointer */
    G_cg->note_pop(argc + 1);
    
    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);
    
    /* if we're not discarding, push the return value from R0 */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   T3-specific property symbol class 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymProp::gen_code(int discard)
{
    /* 
     *   Evaluating a property is equivalent to calling the property on
     *   the "self" object with no arguments.  If there's no "self"
     *   object, an unqualified property evaluation is not possible, so
     *   log an error if this is the case.  
     */
    if (!G_cs->is_self_available())
    {
        G_tok->log_error(TCERR_PROP_NEEDS_OBJ, (int)get_sym_len(), get_sym());
        return;
    }

    if (G_cg->is_speculative())
    {
        /* push 'self', then evaluate the property in data-only mode */
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->write_op(OPC_GETPROPDATA);
        G_cs->write_prop_id(prop_);

        /* we pushed the 'self' value then popped it again */
        G_cg->note_push();
        G_cg->note_pop();
    }
    else
    {
        /* generate the call to 'self' */
        G_cg->write_op(OPC_GETPROPSELF);
        G_cs->write_prop_id(prop_);
    }

    /* if they're not discarding the value, push the result */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   evaluate a member expression 
 */
void CTcSymProp::gen_code_member(int discard,
                                 CTcPrsNode *prop_expr, int prop_is_expr,
                                 int argc, int varargs,
                                 CTcNamedArgs *named_args)
{
    /* generate code to evaluate the property */
    gen_code(FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the standard member generation */
    CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                 argc, varargs, named_args);
}

/*
 *   take the address of the property
 */
void CTcSymProp::gen_code_addr()
{
    /* write code to push the property ID */
    G_cg->write_op(OPC_PUSHPROPID);
    G_cs->write_prop_id(prop_);

    /* note the push */
    G_cg->note_push();
}

/*
 *   assign to a property, implicitly of the "self" object 
 */
int CTcSymProp::gen_code_asi(int discard, int phase,
                             tc_asitype_t typ, const char *,
                             class CTcPrsNode *rhs,
                             int /*ignore_errors*/, int /*explicit*/,
                             void ** /*ctx*/)
{
    /* if there's no "self" object, we can't make this assignment */
    if (!G_cs->is_self_available())
    {
        /* log an error if it's phase 1 */
        if (phase == 1)
            G_tok->log_error(TCERR_SETPROP_NEEDS_OBJ,
                             (int)get_sym_len(), get_sym());

        /* 
         *   indicate that we're finished, since there's nothing more we
         *   can do here 
         */
        return TRUE;
    }

    /* check what we're doing */
    if (typ == TC_ASI_SIMPLE)
    {
        /* 
         *   Simple assignment.  Generate the right-hand side's expression
         *   for assignment, unless the caller has already done so.
         */
        if (rhs != 0)
            rhs->gen_code(FALSE, FALSE);

        /* 
         *   if we're not discarding the value, make a copy - we'll consume a
         *   copy in the SETPROP instruction, so we need one more copy to
         *   return to the enclosing expression 
         */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }
        
        /* 
         *   write the SETPROP instruction - use the special form to assign
         *   to "self" 
         */
        G_cg->write_op(OPC_SETPROPSELF);
        G_cs->write_prop_id(prop_);
        
        /* setpropself removes the value */
        G_cg->note_pop();

        /* handled */
        return TRUE;
    }
    else if (phase == 1)
    {
        /* 
         *   Phase 1 of a compound assignment.  Generate the property value
         *   so that the compound operator can combine it with the RHS. 
         */
        gen_code(FALSE);

        /* proceed with the unrolled assignment */
        return FALSE;
    }
    else if (phase == 2)
    {
        /* 
         *   Phase 2 of a compound assignment.  The combined value of
         *   (self.prop <op> RHS) is on the stack, so complete the
         *   assignment.  
         */

        /* if the caller will want to use the RHS value, make a copy */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }

        /* write the SETPROP */
        G_cg->write_op(OPC_SETPROPSELF);
        G_cs->write_prop_id(prop_);

        /* setpropself removes the value */
        G_cg->note_pop();

        /* done */
        return TRUE;
    }
    else
    {
        /* other phases aren't handled */
        return FALSE;
    }
}

/*
 *   call the symbol 
 */
void CTcSymProp::gen_code_call(int discard, int argc, int varargs,
                               CTcNamedArgs *named_args)
{
    /* 
     *   if there's no "self", we can't invoke a property without an
     *   explicit object reference 
     */
    if (!G_cs->is_self_available())
    {
        G_tok->log_error(TCERR_PROP_NEEDS_OBJ, (int)get_sym_len(), get_sym());
        return;
    }

    /* don't allow calling with arguments in speculative mode */
    if (G_cg->is_speculative()
        && (argc != 0 || (named_args != 0 && named_args->cnt != 0)))
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* generate code to invoke the property of "self" */
    if (G_cg->is_speculative())
    {
        /* push 'self', then get the property in data-only mode */
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->write_op(OPC_GETPROPDATA);
        G_cs->write_prop_id(get_prop());

        /* we pushed 'self' then popped it again */
        G_cg->note_push();
        G_cg->note_pop();
    }
    else if (argc == 0)
    {
        /* use the instruction with no arguments */
        G_cg->write_op(OPC_GETPROPSELF);
        G_cs->write_prop_id(get_prop());
    }
    else
    {
        /* write the varargs modifier if appropriate */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);
        
        /* use the instruction with arguments */
        G_cg->write_op(OPC_CALLPROPSELF);
        G_cs->write((char)argc);
        G_cs->write_prop_id(get_prop());

        /* callpropself removes arguments */
        G_cg->note_pop(argc);
    }

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if we're not discarding, push the return value from R0 */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTcSymProp::gen_code_propid(int check_only, int is_expr)
{
    /*
     *   If I'm to be treated as an expression (which indicates that the
     *   property symbol is explicitly enclosed in parentheses in the
     *   original source code expression), then I must evaluate this
     *   property of self.  Otherwise, I yield literally the property ID. 
     */
    if (is_expr)
    {
        /* generate code unless we're only checking */
        if (!check_only)
        {
            /* evaluate this property of self */
            G_cg->write_op(OPC_GETPROPSELF);
            G_cs->write_prop_id(get_prop());

            /* leave the result on the stack */
            G_cg->write_op(OPC_GETR0);
            G_cg->note_push();
        }

        /* tell the caller to use the stack value */
        return VM_INVALID_PROP;
    }
    else
    {
        /* simple '.prop' - return my property ID as a constant value */
        return get_prop();
    }
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymProp::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_propid(get_prop());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Enumerator symbol 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymEnum::gen_code(int discard)
{
    if (!discard)
    {
        /* generate code to push the enum value */
        G_cg->write_op(OPC_PUSHENUM);
        G_cs->write_enum_id(get_enum_id());

        /* note the push */
        G_cg->note_push();
    }
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymEnum::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_enum(get_enum_id());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific local variable/parameter symbol class 
 */

/*
 *   generate code to evaluate the symbol 
 */
void CTcSymLocal::gen_code(int discard)
{
    /* generate code to push the local, if we're not discarding it */
    if (!discard)
    {
        /* 
         *   generate as a context local if required, otherwise as an
         *   ordinary local variable 
         */
        if (is_ctx_local_)
        {
            /* generate the context array lookup */
            if (ctx_var_num_ <= 255 && get_ctx_arr_idx() <= 255)
            {
                /* we can do this whole operation with one instruction */
                G_cg->write_op(OPC_IDXLCL1INT8);
                G_cs->write((uchar)ctx_var_num_);
                G_cs->write((uchar)get_ctx_arr_idx());

                /* this pushes one value */
                G_cg->note_push();
            }
            else
            {
                /* get our context array */
                s_gen_code_getlcl(ctx_var_num_, FALSE);
                
                /* get our value from the context array */
                CTPNConst::s_gen_code_int(get_ctx_arr_idx());
                G_cg->write_op(OPC_INDEX);
                
                /* the INDEX operation removes two values and pushes one */
                G_cg->note_pop();
            }
        }
        else
        {
            /* generate as an ordinary local */
            s_gen_code_getlcl(get_var_num(), is_param());
        }
    }

    /* 
     *   Mark the value as referenced, whether or not we're generating the
     *   code - the value has been logically referenced in the program
     *   even if the result of evaluating it isn't needed.  
     */
    set_val_used(TRUE);
}

/*
 *   generate code to push a local onto the stack 
 */
void CTcSymLocal::s_gen_code_getlcl(int var_num, int is_param)
{
    /* use the shortest form of the instruction that we can */
    if (is_param && var_num < 4)
    {
        /* arguments 0-3 have dedicated opcodes */
        G_cg->write_op(OPC_GETARGN0 + var_num);
    }
    else if (!is_param && var_num < 6)
    {
        /* locals 0-5 have dedicated opcodes */
        G_cg->write_op(OPC_GETLCLN0 + var_num);
    }
    else if (var_num <= 255)
    {
        /* 8-bit local number - use the one-byte form */
        G_cg->write_op(is_param ? OPC_GETARG1 : OPC_GETLCL1);
        G_cs->write((char)var_num);
    }
    else
    {
        /* local number won't fit in 8 bits - use the two-byte form */
        G_cg->write_op(is_param ? OPC_GETARG2 : OPC_GETLCL2);
        G_cs->write2(var_num);
    }

    /* note the push */
    G_cg->note_push();
}

/*
 *   assign a value 
 */
int CTcSymLocal::gen_code_asi(
    int discard, int phase, tc_asitype_t typ, const char *,
    class CTcPrsNode *rhs, int ignore_errors, int xplicit, void **)
{
    /* 
     *   if this is an explicit assignment, mark the variable as having had a
     *   value assigned to it 
     */
    if (xplicit)
        set_val_assigned(TRUE);

    /* 
     *   if the assignment is anything but simple, this references the
     *   value as well 
     */
    if (typ != TC_ASI_SIMPLE)
        set_val_used(TRUE);

    /* 
     *   If this is a context variable, use standard assignment (i.e.,
     *   generate the result first, then generate a simple assignment to the
     *   variable).  Otherwise, we might be able to generate a fancy
     *   combined calculate-and-assign sequence, depending on the type of
     *   assignment calculation we're performing.
     */
    if (is_ctx_local_ && typ != TC_ASI_SIMPLE && phase == 1)
    {
        /* 
         *   It's a context local, and it's not a simple assignment, so we
         *   can't perform any special calculate-and-assign sequence.  For
         *   phase 1, generate the value and tell the caller to calculate the
         *   full result first and then try again using simple assignment.
         */
        gen_code(FALSE);
        return FALSE;
    }

    /* 
     *   If this is phase 2 of a compound assignment, treat it the same as a
     *   simple assignment.  The RHS is on the stack, and we simply need to
     *   complete the assignment to the local.
     */
    if (phase == 2)
        typ = TC_ASI_SIMPLE;

    /* 
     *   check the type of assignment - we can optimize the code
     *   generation to use more compact instruction sequences for certain
     *   types of assignments 
     */
    int adding;
    switch(typ)
    {
    case TC_ASI_SIMPLE:
        /* 
         *   Simple assignment to local/parameter.  Check for some special
         *   cases: when assigning a constant value of 0, 1, or nil to a
         *   local, we can generate a short instruction 
         */
        if (!is_param() && !is_ctx_local_ && rhs != 0 && rhs->is_const())
        {
            CTcConstVal *cval;

            /* get the constant value */
            cval = rhs->get_const_val();
            
            /* check for nil and 0 or 1 values */
            if (cval->get_type() == TC_CVT_NIL)
            {
                /* it's nil - generate NILLCL1 or NILLCL2 */
                if (get_var_num() <= 255)
                {
                    G_cg->write_op(OPC_NILLCL1);
                    G_cs->write((char)get_var_num());
                }
                else
                {
                    G_cg->write_op(OPC_NILLCL2);
                    G_cs->write2(get_var_num());
                }

                /* if not discarding, leave nil on the stack */
                if (!discard)
                {
                    G_cg->write_op(OPC_PUSHNIL);
                    G_cg->note_push();
                }

                /* handled */
                return TRUE;
            }
            else if (cval->get_type() == TC_CVT_INT
                     && (cval->get_val_int() == 0 
                         || cval->get_val_int() == 1))
            {
                int ival;

                /* get the integer value */
                ival = cval->get_val_int();
                
                /* 0 or 1 - generate ZEROLCLn or ONELCLn */
                if (get_var_num() <= 255)
                {
                    G_cg->write_op(ival == 0 ? OPC_ZEROLCL1 : OPC_ONELCL1);
                    G_cs->write((char)get_var_num());
                }
                else
                {
                    G_cg->write_op(ival == 0 ? OPC_ZEROLCL2 : OPC_ONELCL2);
                    G_cs->write2(get_var_num());
                }

                /* if not discarding, leave the value on the stack */
                if (!discard)
                {
                    G_cg->write_op(ival == 0 ? OPC_PUSH_0 : OPC_PUSH_1);
                    G_cg->note_push();
                }

                /* handled */
                return TRUE;
            }
        }

        /* 
         *   If we got here, we can't generate a specialized constant
         *   assignment - so, first, generate the right-hand side's value
         *   normally.  (If no 'rhs' is specified, the value is already on
         *   the stack.)  
         */
        if (rhs != 0)
            rhs->gen_code(FALSE, FALSE);

        /* leave an extra copy of the value on the stack if not discarding */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }

        /* now assign the value at top of stack to the variable */
        gen_code_setlcl();

        /* handled */
        return TRUE;

    case TC_ASI_ADD:
        adding = TRUE;
        goto add_or_sub;
        
    case TC_ASI_SUB:
        adding = FALSE;

    add_or_sub:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
        {
            if (phase == 1)
                gen_code(FALSE);
            return FALSE;
        }
        
        /* 
         *   Add/subtract to a local/parameter.  If the right-hand side is a
         *   constant integer value, we might be able to generate a special
         *   instruction to add/subtract it.  
         */
        if (rhs != 0
            && adding
            && rhs->is_const()
            && rhs->get_const_val()->get_type() == TC_CVT_INT)
        {
            long ival;

            /* get the integer value to assign */
            ival = rhs->get_const_val()->get_val_int();

            /* 
             *   if the right-hand side's integer value fits in one byte,
             *   generate the short (8-bit) instruction; otherwise,
             *   generate the long (32-bit) format 
             */
            if (ival == 1)
            {
                /* adding one - increment the local */
                G_cg->write_op(OPC_INCLCL);
                G_cs->write2(get_var_num());
            }
            else if (ival == -1)
            {
                /* subtracting one - decrement the local */
                G_cg->write_op(OPC_DECLCL);
                G_cs->write2(get_var_num());
            }
            else if (ival <= 127 && ival >= -128
                     && get_var_num() <= 255)
            {
                /* fits in 8 bits - use the 8-bit format */
                G_cg->write_op(OPC_ADDILCL1);
                G_cs->write((char)get_var_num());
                G_cs->write((char)ival);
            }
            else
            {
                /* 
                 *   either the value or the variable number doesn't fit
                 *   in 8 bits - use the 32-bit format 
                 */
                G_cg->write_op(OPC_ADDILCL4);
                G_cs->write2(get_var_num());
                G_cs->write4(ival);
            }
        }
        else
        {
            /* 
             *   We don't have a special instruction for the right side,
             *   so generate it normally and add/subtract the value.  (If
             *   there's no 'rhs' value specified, it means that the value
             *   is already on the stack, so there's nothing extra for us
             *   to generate.)  
             */
            if (rhs != 0)
                rhs->gen_code(FALSE, FALSE);
            
            /* write the ADDTOLCL instruction */
            G_cg->write_op(adding ? OPC_ADDTOLCL : OPC_SUBFROMLCL);
            G_cs->write2(get_var_num());

            /* addtolcl/subfromlcl remove the rvalue */
            G_cg->note_pop();
        }

        /* 
         *   if not discarding, push the result onto the stack; do this by
         *   simply evaluating the local, which is the simplest and most
         *   efficient way to obtain the result of the computation 
         */
        if (!discard)
            gen_code(FALSE);

        /* handled */
        return TRUE;

    case TC_ASI_PREINC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
        {
            if (phase == 1)
                gen_code(FALSE);
            return FALSE;
        }

        /* generate code to increment the local */
        G_cg->write_op(OPC_INCLCL);
        G_cs->write2(get_var_num());

        /* if we're not discarding, push the local's new value */
        if (!discard)
            gen_code(FALSE);

        /* handled */
        return TRUE;

    case TC_ASI_POSTINC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
        {
            if (phase == 1)
                gen_code(FALSE);
            return FALSE;
        }

        /* 
         *   if we're not discarding, push the local's value prior to
         *   incrementing it - this will be the result we'll leave on the
         *   stack 
         */
        if (!discard)
            gen_code(FALSE);

        /* generate code to increment the local */
        G_cg->write_op(OPC_INCLCL);
        G_cs->write2(get_var_num());

        /* handled */
        return TRUE;

    case TC_ASI_PREDEC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
        {
            if (phase == 1)
                gen_code(FALSE);
            return FALSE;
        }

        /* generate code to decrement the local */
        G_cg->write_op(OPC_DECLCL);
        G_cs->write2(get_var_num());

        /* if we're not discarding, push the local's new value */
        if (!discard)
            gen_code(FALSE);

        /* handled */
        return TRUE;

    case TC_ASI_POSTDEC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
        {
            if (phase == 1)
                gen_code(FALSE);
            return FALSE;
        }

        /* 
         *   if we're not discarding, push the local's value prior to
         *   decrementing it - this will be the result we'll leave on the
         *   stack 
         */
        if (!discard)
            gen_code(FALSE);

        /* generate code to decrement the local */
        G_cg->write_op(OPC_DECLCL);
        G_cs->write2(get_var_num());

        /* handled */
        return TRUE;

    default:
        /* 
         *   For other special assignment types, simply generate the value of
         *   the local, and tell the caller to proceed with the unrolled
         *   assignment.  
         */
        gen_code(FALSE);
        return FALSE;
    }
}

/*
 *   generate code to assigin the value at top of stack to the local
 *   variable 
 */
void CTcSymLocal::gen_code_setlcl()
{
    /* check to see if we're a context local (as opposed to a stack local) */
    if (is_ctx_local_)
    {
        /* generate the assignment using the appropriate sequence */
        if (ctx_var_num_ <= 255 && get_ctx_arr_idx() <= 255)
        {
            /* we can fit this in a single instruction */
            G_cg->write_op(OPC_SETINDLCL1I8);
            G_cs->write((uchar)ctx_var_num_);
            G_cs->write((uchar)get_ctx_arr_idx());

            /* this pops the value being assigned */
            G_cg->note_pop();
        }
        else
        {
            /* get our context array */
            s_gen_code_getlcl(ctx_var_num_, FALSE);
            
            /* set our value in the context array */
            CTPNConst::s_gen_code_int(get_ctx_arr_idx());
            G_cg->write_op(OPC_SETIND);
            G_cg->write_op(OPC_DISC);
            
            /* 
             *   the SETIND pops three values and pushes one (for a net two
             *   pops), and the DISC pops one more value, so our total is
             *   three pops 
             */
            G_cg->note_pop(3);
        }
    }
    else
    {
        /* we're just a plain stack variable */
        gen_code_setlcl_stk();
    }
}

/*
 *   Generate code to store the value at the top of the stack into the given
 *   local stack slot.  Note that this routine will not work with a context
 *   local - it only works if the variable is known to be a stack variable.  
 */
void CTcSymLocal::s_gen_code_setlcl_stk(int var_num, int is_param)
{
    /* use the shortest form that will fit our variable index */
    if (var_num <= 255)
    {
        /* use the one-byte instruction */
        G_cg->write_op(is_param ? OPC_SETARG1 : OPC_SETLCL1);
        G_cs->write((char)var_num);
    }
    else
    {
        /* big number - use the two-byte instruction */
        G_cg->write_op(is_param ? OPC_SETARG2 : OPC_SETLCL2);
        G_cs->write2(var_num);
    }

    /* the setarg/setlcl ops remove the rvalue */
    G_cg->note_pop();
}

/*
 *   call the symbol 
 */
void CTcSymLocal::gen_code_call(
    int discard, int argc, int varargs, CTcNamedArgs *named_args)
{
    /* 
     *   to call a local, we'll simply evaluate the local normally, then
     *   call through the resulting (presumed) property or function
     *   pointer value 
     */
    gen_code(FALSE);

    /* 
     *   if we have a varargs list, modify the call instruction that
     *   follows to make it a varargs call 
     */
    if (varargs)
    {
        /* swap the top of the stack to get the arg counter back on top */
        G_cg->write_op(OPC_SWAP);

        /* write the varargs modifier */
        G_cg->write_op(OPC_VARARGC);
    }

    /* don't allow this at all in speculative mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* call the result as a function or method pointer */
    G_cg->write_op(OPC_PTRCALL);
    G_cs->write((char)argc);

    /* ptrcall removes the arguments and the function pointer */
    G_cg->note_pop(argc + 1);

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if we're not discarding the value, push the result */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTcSymLocal::gen_code_propid(int check_only, int /*is_expr*/)
{
    /*
     *   treat the local as a property-valued expression; generate the
     *   code for the local, then tell the caller that no constant value
     *   is available, since the local's property ID value should be on
     *   the stack 
     */
    if (!check_only)
        gen_code(FALSE);

    /* tell the caller to use the stack value */
    return VM_INVALID_PROP;
}

/*
 *   evaluate a member expression 
 */
void CTcSymLocal::gen_code_member(
    int discard, CTcPrsNode *prop_expr, int prop_is_expr,
    int argc, int varargs, CTcNamedArgs *named_args)
{
    /* generate code to evaluate the local */
    gen_code(FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the standard member generation */
    CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                 argc, varargs, named_args);
}

/*
 *   write to a debug record 
 */
int CTcSymLocal::write_to_debug_frame(int test_only)
{
    /* if this is test-only mode, just note that we will write it */
    if (test_only)
        return TRUE;

    /* 
     *   For version 2+ static compilation, write the symbol name
     *   out-of-line, to the separate stream for local variable names.  This
     *   allows us to consolidate the names so that each name is stored only
     *   once in the file.  This saves a lot of space since there are a few
     *   common local names that appear over and over.  Version 1 always
     *   wrote names in-line, and we also have to write the names in-line
     *   when generating dynamic code, since we can't add to the constant
     *   pool in this case.
     *   
     *   Also, store all 1- and 2-character names in-line.  1-character names
     *   are smaller if stored in-line since they'll take only three bytes
     *   (two bytes for the length, one byte for the string) vs four bytes
     *   for a shared pool pointer.  2-char names are a wash on the frame
     *   storage, but it's still more efficient to store them in-line because
     *   we avoid also creating the constant pool entry for them.  
     */
    int inl = (len_ <= 2
               || G_sizes.dbg_fmt_vsn < 2
               || G_cg->is_eval_for_dyn());
    
    /* 
     *   write my ID - if we're a context variable, we want to write the
     *   context variable ID; otherwise write our stack location as normal 
     */
    if (is_ctx_local_)
        G_cs->write2(ctx_var_num_);
    else
        G_cs->write2(var_num_);

    /* compute my flags */
    int flags = 0;
    if (is_param_)
        flags |= 0x0001;
    if (is_ctx_local_)
        flags |= 0x0002;
    if (!inl)
        flags |= 0x0004;

    /* write my flags */
    G_cs->write2(flags);

    /* write my local context array index */
    G_cs->write2(get_ctx_arr_idx());

    /* add zeros to pad any future version information for the target VM */
    for (int i = G_sizes.lcl_hdr - 6 ; i > 0 ; --i)
        G_cs->write(0);

    /* 
     *   write the name - either to the separate stream for local names, or
     *   inline if necessary 
     */
    if (inl)
    {
        /* write the symbol name to the table in-line */
        G_cs->write2(len_);
        G_cs->write(str_, len_);
    }
    else
    {
        /* add a fixup from the table to the local stream */
        CTcStreamAnchor *anchor = G_lcl_stream->add_anchor(0, 0);
        CTcAbsFixup::add_abs_fixup(anchor->fixup_list_head_,
                                   G_cs, G_cs->get_ofs());

        /* add the placeholder to the table (this is what we fix up) */
        G_cs->write4(0);

        /* write the name to the local stream */
        G_lcl_stream->write2(len_);
        G_lcl_stream->write(str_, len_);
    }

    /* we did write this symbol */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Dynamic code local.  This is a symbol for a local variable in an active
 *   stack frame during dynamic compilation.  
 */

/*
 *   generate code to evaluate the variable's value 
 */
void CTcSymDynLocal::gen_code(int discard)
{
    /* there are no side effects, so do nothing if discarding the value */
    if (discard)
        return;

    /* 
     *   to retrieve a dynamic frame local, we simply need to evaluate
     *   fref[index], where 'fref' is the StackFrameRef object and 'index' is
     *   an integer giving the frame index of the variable 
     */

    /* push the frame object */
    G_cg->write_op(OPC_PUSHOBJ);
    G_cs->write_obj_id(fref_);
    G_cg->note_push();

    /* push the variable's frame index */
    CTPNConst::s_gen_code_int(varnum_);

    /* index the frame object */
    G_cg->write_op(OPC_INDEX);

    /* INDEX pops two items and pushes one for a net of one pop */
    G_cg->note_pop(1);

    /* 
     *   if this is a context local, we now have the context object, so we
     *   need to further index it by the variable's context index 
     */
    if (ctxidx_ != 0)
    {
        /* index the context object by the context index */
        CTPNConst::s_gen_code_int(ctxidx_);
        G_cg->write_op(OPC_INDEX);

        /* INDEX pops two items and pushes one */
        G_cg->note_pop(1);
    }
}

/*
 *   generate code to assign to the variable 
 */
int CTcSymDynLocal::gen_code_asi(int discard, int phase,
                                 tc_asitype_t typ, const char *,
                                 class CTcPrsNode *rhs,
                                 int, int, void **)
{
    /* if this is a complex assignment, use the default two-phase process */
    if (typ != TC_ASI_SIMPLE && phase == 1)
    {
        /* 
         *   generate our value, and tell the caller to calculate the
         *   compound expression result and assign the value with a separate
         *   call 
         */
        gen_code(FALSE);
        return FALSE;
    }

    /* if the rhs isn't already on the stack, generate it */
    if (rhs != 0)
        rhs->gen_code(FALSE, FALSE);

    /* 
     *   if we're not disarding the rhs value, make an extra copy, as we'll
     *   consume our copy in the performing the assignment 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }

    /* 
     *   to assign to a local variable, perform a set-index operation through
     *   the frame object - that is, evaluate "fref[index] = value" 
     */
    
    /* push the frame object */
    G_cg->write_op(OPC_PUSHOBJ);
    G_cs->write_obj_id(fref_);
    G_cg->note_push();

    /* push the variable's frame index */
    CTPNConst::s_gen_code_int(varnum_);

    /* 
     *   if this is a context local, frame[index] yields the context object,
     *   so we need to retrieve that before proceeding 
     */
    if (ctxidx_ != 0)
    {
        /* get frame[varnum] - that gives us the context object */
        G_cg->write_op(OPC_INDEX);

        /* INDEX pops two items and pushes one for a net 1 pop */
        G_cg->note_pop();

        /* 
         *   now push the context index - the actual assignment will be
         *   ctxobj[ctxidx] = rhs 
         */
        CTPNConst::s_gen_code_int(ctxidx_);
    }

    /* perform the assignment */
    G_cg->write_op(OPC_SETIND);

    /* 
     *   Discard the new container value.  This is the result of doing an
     *   index-assign through the frame object, which is simply the original
     *   frame object; even if it were changing, it's not something we'd need
     *   to store anywhere since this is an internal indexing operation.  
     */
    G_cg->write_op(OPC_DISC);

    /* 
     *   the SETIND pops the rhs, index, and indexee, and pushes the
     *   container, which we then discard, for a net pop of 3 
     */
    G_cg->note_pop(3);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Built-in function symbol
 */

/*
 *   Evaluate the symbol.  Invoking a built-in function without an
 *   argument list is simply a call to the built-in function with no
 *   arguments.  
 */
void CTcSymBif::gen_code(int discard)
{
    /* generate a call */
    gen_code_call(discard, 0, FALSE, 0);
}

/*
 *   Generate code to call the built-in function 
 */
void CTcSymBif::gen_code_call(int discard, int argc, int varargs,
                              CTcNamedArgs *named_args)
{
    /* don't allow calling built-in functions in speculative mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);
    
    /* check for minimum and maximum arguments */
    if (argc < min_argc_)
    {
        G_tok->log_error(TCERR_TOO_FEW_FUNC_ARGS,
                         (int)get_sym_len(), get_sym());
    }
    else if (!varargs_ && argc > max_argc_)
    {
        G_tok->log_error(TCERR_TOO_MANY_FUNC_ARGS,
                         (int)get_sym_len(), get_sym());
    }

    /* write the varargs modifier if appropriate */
    if (varargs)
        G_cg->write_op(OPC_VARARGC);

    /* generate the call */
    if (get_func_set_id() < 4 && get_func_idx() < 256)
    {
        uchar short_ops[] =
            { OPC_BUILTIN_A, OPC_BUILTIN_B, OPC_BUILTIN_C, OPC_BUILTIN_D };
        
        /* 
         *   it's one of the first 256 functions in one of the first four
         *   function sets - we can generate a short instruction 
         */
        G_cg->write_op(short_ops[get_func_set_id()]);
        G_cs->write((char)argc);
        G_cs->write((char)get_func_idx());
    }
    else
    {
        /* it's not in the default set - use the longer instruction */
        if (get_func_idx() < 256)
        {
            /* low function index - write the short form */
            G_cg->write_op(OPC_BUILTIN1);
            G_cs->write((char)argc);
            G_cs->write((char)get_func_idx());
        }
        else
        {
            /* big function index - write the long form */
            G_cg->write_op(OPC_BUILTIN2);
            G_cs->write((char)argc);
            G_cs->write2(get_func_idx());
        }

        /* write the function set ID */
        G_cs->write((char)get_func_set_id());
    }

    /* the built-in functions always remove arguments */
    G_cg->note_pop(argc);

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* 
     *   if they're not discarding the value, push it - the value is
     *   sitting in R0 after the call returns
     */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   take the address of the built-in function
 */
void CTcSymBif::gen_code_addr()
{
    /* generate PUSHBIFPTR <function set index> <function index> */
    G_cg->write_op(OPC_PUSHBIFPTR);
    G_cs->write2(func_idx_);
    G_cs->write2(func_set_id_);

    /* this pushes one element */
    G_cg->note_push();
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymBif::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add an entry for our absolute address */
    val.set_bifptr(func_set_id_, func_idx_);
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}

/* ------------------------------------------------------------------------ */
/*
 *   External function symbol 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymExtfn::gen_code(int /*discard*/)
{
    //$$$ to be implemented
    assert(FALSE);
}

/*
 *   generate a call to the symbol
 */
void CTcSymExtfn::gen_code_call(int /*discard*/, int /*argc*/, int /*varargs*/,
                                CTcNamedArgs * /*named_args*/)
{
    //$$$ to be implemented
    assert(FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Code Label symbol 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymLabel::gen_code(int discard)
{
    /* it's not legal to evaluate a code label; log an error */
    G_tok->log_error(TCERR_CANNOT_EVAL_LABEL, (int)get_sym_len(), get_sym());
}

/* ------------------------------------------------------------------------ */
/*
 *   Metaclass symbol 
 */

/*
 *   generate code for evaluating the symbol 
 */
void CTcSymMetaclass::gen_code(int discard)
{
    /* 
     *   mark it as referenced - if the metaclass wasn't defined in this file
     *   but was merely imported from a symbol file, we now need to write it
     *   to the object file anyway, since its class object ID has been
     *   referenced 
     */
    ref_ = TRUE;
    
    /* 
     *   the metaclass name refers to the IntrinsicClass instance
     *   associated with the metaclass 
     */
    G_cg->write_op(OPC_PUSHOBJ);
    G_cs->write_obj_id(class_obj_);

    /* note the push */
    G_cg->note_push();
}

/*
 *   generate code for operator 'new' applied to the metaclass 
 */
void CTcSymMetaclass::gen_code_new(int discard, int argc,
                                   int varargs, CTcNamedArgs *named_args,
                                   int is_transient)
{
    /* if this is an external metaclass, we can't generate code */
    if (ext_)
    {
        G_tok->log_error(TCERR_EXT_METACLASS, (int)get_sym_len(), get_sym());
        return;
    }

    /* if we have varargs, write the modifier */
    if (varargs)
        G_cg->write_op(OPC_VARARGC);
    
    if (meta_idx_ <= 255 && argc <= 255)
    {
        G_cg->write_op(is_transient ? OPC_TRNEW1 : OPC_NEW1);
        G_cs->write((char)argc);
        G_cs->write((char)meta_idx_);
    }
    else
    {
        G_cg->write_op(is_transient ? OPC_TRNEW1 : OPC_NEW2);
        G_cs->write2(argc);
        G_cs->write2(meta_idx_);
    }

    /* new1/new2 remove arguments */
    G_cg->note_pop(argc);

    /* do post-call cleanup: named arg removal, etc */
    G_cg->post_call_cleanup(named_args);

    /* if we're not discarding the value, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* 
 *   generate a member expression 
 */
void CTcSymMetaclass::gen_code_member(int discard, CTcPrsNode *prop_expr,
                                      int prop_is_expr,
                                      int argc, int varargs,
                                      CTcNamedArgs *named_args)
{
    /* if this is an external metaclass, we can't generate code */
    if (ext_)
    {
        G_tok->log_error(TCERR_EXT_METACLASS, (int)get_sym_len(), get_sym());
        return;
    }

    /* generate code to push our class object onto the stack */
    gen_code(FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the standard member generation */
    CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                 argc, varargs, named_args);
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymMetaclass::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    /* don't do this for external metaclasses */
    if (ext_)
        return;

    /* add our entry */
    vm_val_t val;
    val.set_obj(get_class_obj());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}


/* ------------------------------------------------------------------------ */
/*
 *   Exception Table 
 */

/*
 *   create 
 */
CTcT3ExcTable::CTcT3ExcTable()
{
    /* allocate an initial table */
    exc_alloced_ = 1024;
    table_ = (CTcT3ExcEntry *)t3malloc(exc_alloced_ * sizeof(table_[0]));

    /* no entries are in use yet */
    exc_used_ = 0;

    /* method offset is not yet known */
    method_ofs_ = 0;
}


/*
 *   add an entry to our table 
 */
void CTcT3ExcTable::add_catch(ulong protected_start_ofs,
                              ulong protected_end_ofs,
                              ulong exc_obj_id, ulong catch_block_ofs)
{
    CTcT3ExcEntry *entry;

    /* if necessary, expand our table */
    if (exc_used_ == exc_alloced_)
    {
        /* expand the table a bit */
        exc_alloced_ += 1024;

        /* reallocate the table at the larger size */
        table_ = (CTcT3ExcEntry *)
                 t3realloc(table_, exc_alloced_ * sizeof(table_[0]));
    }

    /* 
     *   set up the new entry - store the offsets relative to the method
     *   header start address 
     */
    entry = table_ + exc_used_;
    entry->start_ofs = protected_start_ofs - method_ofs_;
    entry->end_ofs = protected_end_ofs - method_ofs_;
    entry->exc_obj_id = exc_obj_id;
    entry->catch_ofs = catch_block_ofs - method_ofs_;

    /* consume the new entry */
    ++exc_used_;
}

/*
 *   write our exception table to the code stream 
 */
void CTcT3ExcTable::write_to_code_stream()
{
    CTcT3ExcEntry *entry;
    size_t i;

    /* write the number of entries as a UINT2 */
    G_cs->write2(exc_used_);

    /* write the entries */
    for (i = 0, entry = table_ ; i < exc_used_ ; ++i, ++entry)
    {
        /* write this entry */
        G_cs->write2(entry->start_ofs);
        G_cs->write2(entry->end_ofs);
        G_cs->write_obj_id(entry->exc_obj_id);
        G_cs->write2(entry->catch_ofs);

        /* pad any excess beyond the v1 size with zero bytes */
        for (int j = G_sizes.exc_entry - 10 ; j > 0 ; --j)
            G_cs->write(0);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Parameter default value enumeration context. 
 */
struct defval_ctx
{
    defval_ctx()
    {
        nv = 0;
        nvalo = 128;
        v = new CTcSymLocal *[nvalo];
    }

    ~defval_ctx()
    {
        delete [] v;
    }

    void add(CTcSymLocal *l)
    {
        /* if there's not room, expand the array */
        if (nv == nvalo)
        {
            CTcSymLocal **vnew = new CTcSymLocal*[nvalo + 128];
            memcpy(vnew, v, nvalo * sizeof(*vnew));
            delete [] v;
            v = vnew;
            nvalo += 128;
        }

        /* add it */
        v[nv++] = l;
    }

    /* sort the list in ascending order of sequence number */
    void sort() { qsort(v, nv, sizeof(*v), &compare); }
    static int compare(const void *a0, const void *b0)
    {
        const CTcSymLocal *a = *(const CTcSymLocal **)a0;
        const CTcSymLocal *b = *(const CTcSymLocal **)b0;

        return a->get_defval_seqno() - b->get_defval_seqno();
    }

    /* generate code to initialize an optional positional parameter */
    static void gen_opt_positional(CTcSymLocal *lcl)
    {
        /*   
         *   The code we want to generate is this, in pseudo code (where 'n'
         *   is our position among the arguments):
         *   
         *.    if (n <= argcount)
         *.       lcl = getArg(n)
         *.    else
         *.       lcl = default_expression;
         *   
         *   The 'else' applies only if there's a default expression with the
         *   local; otherwise we simply leave it with the nil initial value.
         */

        /* if n > argcount... */
        CTPNConst::s_gen_code_int(lcl->get_param_index() + 1);
        G_cg->write_op(OPC_GETARGC);
        G_cg->note_push();

        /* ...jump to the 'not found' branch... */
        CTcCodeLabel *nf_lbl = G_cs->new_label_fwd();
        G_cg->write_op(OPC_JGT);
        G_cs->write_ofs2(nf_lbl, 0);
        G_cg->note_pop(2);
        
        /* getArg(n) */
        CTPNConst::s_gen_code_int(lcl->get_param_index() + 1);
        call_internal_bif("getArg", TCERR_OPT_PARAM_MISSING_FUNC, 1);

        /* if there's a default value, generate it */
        if (lcl->get_defval_expr() != 0)
        {
            /* we're done with the 'then' branch - jump to the assignment */
            CTcCodeLabel *asi_lbl = G_cs->new_label_fwd();
            G_cg->write_op(OPC_JMP);
            G_cs->write_ofs2(asi_lbl, 0);
            
            /* define the 'not found' label - come here to set the default */
            CTcPrsNode::def_label_pos(nf_lbl);

            /* generate the default expression */
            lcl->get_defval_expr()->gen_code(FALSE, FALSE);

            /* we're ready to do the assignment */
            CTcPrsNode::def_label_pos(asi_lbl);

            /* 
             *   since we're sharing the assignment (coming up next) with the
             *   main branch, we need to account for the extra push on this
             *   branch 
             */
            G_cg->note_pop();
        }

        /* 
         *   Assign the value we decided upon to the local.  Don't count this
         *   as an explicit assignment of a value for warning purposes; this
         *   is effectively the same kind of assignment as the kind that
         *   binds an actual argument to this formal parameter, so it
         *   shouldn't count as any more explicit an assignment than that
         *   does.  
         */
        lcl->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=",
                          0, FALSE, FALSE, 0);

        /* 
         *   if there was no default value, the 'not found' branch jumps
         *   here, past the assignment, since we have no value to assign 
         */
        if (lcl->get_defval_expr() == 0)
            CTcPrsNode::def_label_pos(nf_lbl);
    }

    /* array of local variables with default values */
    CTcSymLocal **v;

    /* allocated size of 'v' array */
    int nvalo;

    /* number of entries in 'v' array */
    int nv;
};

/* ------------------------------------------------------------------------ */
/*
 *   Code body 
 */

/*
 *   generate code 
 */
void CTPNCodeBody::gen_code(int, int)
{
    /* if I've been replaced, don't bother generating any code */
    if (replaced_)
        return;

    /* 
     *   Open the method header.
     *   
     *   Generate to the static stream if this is a static initializer
     *   method, otherwise to the main stream. 
     *   
     *   Anchor the fixups in the associated symbol table entry, if any.  We
     *   maintain our own fixup list if we don't have a symbol, otherwise we
     *   use the one from our symbol table entry - in either case, we have to
     *   keep track of it ourselves, because a code body might be reachable
     *   through multiple references (a function, for example, has a global
     *   symbol table entry - fixups referencing us might already have been
     *   created by the time we generate our code).  
     */
    tct3_method_gen_ctx gen_ctx;
    G_cg->open_method(
        is_static_ ? G_cs_static : G_cs_main,
        fixup_owner_sym_, fixup_list_anchor_,
        this, gototab_, argc_, opt_argc_, varargs_,
        is_constructor_, op_overload_, self_valid_, &gen_ctx);

    /* 
     *   Add a line record at the start of the method for all of the
     *   generated method prolog code.  Some prolog code can throw errors, so
     *   we want a line record at the start of the code body to indicate the
     *   location of any such errors. 
     */
    if (start_desc_ != 0)
        G_cs->add_line_rec(start_desc_, start_linenum_);

    /* 
     *   Add each local symbol table enclosing the code body's primary
     *   local symbol table to the frame list.  The outermost code body
     *   table can be outside the primary code body table for situations
     *   such as anonymous functions.  Since these tables are outside of
     *   any statements, we must explicitly add them to ensure that they
     *   are assigned debugging frame ID's and are written to the debug
     *   data.
     */
    if (lcltab_ != 0)
    {
        /* add each frame outside the primary frame to the code gen list */
        for (CTcPrsSymtab *tab = lcltab_->get_parent() ; tab != 0 ;
             tab = tab->get_parent())
            G_cs->set_local_frame(tab);
    }

    /* the method's local symbol table is now the active symbol table */
    G_cs->set_local_frame(lcltab_);

    /* if we have a local context, initialize it */
    if (has_local_ctx_)
    {
        /* write code to create the new Vector to store the context locals */
        CTPNConst::s_gen_code_int(local_ctx_arr_size_);
        G_cg->write_op(OPC_DUP);
        G_cg->write_op(OPC_NEW1);
        G_cs->write(2);
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_VECTOR));

        /* retrieve the object value */
        G_cg->write_op(OPC_GETR0);

        /*
         *   we duplicated the vector size argument, then we popped it and
         *   pushed the object; so we have a maximum of one extra push and a
         *   net of zero 
         */
        G_cg->note_push();
        G_cg->note_pop();

        /* store the new object in the context local variable */
        CTcSymLocal::s_gen_code_setlcl_stk(local_ctx_var_, FALSE);

        /* 
         *   go through our symbol table, and copy each parameter that's
         *   also a context local into its context local slot 
         */
        if (lcltab_ != 0)
            lcltab_->enum_entries(&enum_for_param_ctx, this);
    }

    /* 
     *   If we have a varargs-list parameter, generate the code to set up
     *   the list value from the actual parameters.  Note that we must do
     *   this after we set up the local context, in case the varargs list
     *   parameter variable is a context local, in which case it will need
     *   to be stored in the context, in which case we need the context to
     *   be initialized first.  
     */
    if (varargs_list_)
    {
        /* generate the PUSHPARLST instruction to create the list */
        G_cg->write_op(OPC_PUSHPARLST);
        G_cs->write((uchar)argc_);

        /* 
         *   we pushed at least one value (the list); we don't know how many
         *   others we might have pushed, but it doesn't matter because the
         *   interpreter is responsible for checking for stack space 
         */
        G_cg->note_push();

        /* store the list in our varargs parameter list local */
        varargs_list_local_->gen_code_setlcl();
    }

    /* 
     *   generate other special parameter setup code: named arguments and
     *   default values 
     */
    if (lcltab_ != 0)
    {
        /* 
         *   generate bindings for named parameters and optional positional
         *   parameters that don't have default expressions, and build the
         *   array of optional parameters with default expressions
         */
        defval_ctx dctx;
        lcltab_->enum_entries(&enum_for_named_params, &dctx);

        /* sort the default value list in left-to-right parameter order */
        dctx.sort();

        /* generate the default value assignments */
        for (int i = 0 ; i < dctx.nv ; ++i)
        {
            /* get this local */
            CTcSymLocal *lcl = dctx.v[i];

            /* generate the proper code depending on the parameter type */
            if (lcl->is_named_param())
            {
                /* 
                 *   Named parameter with a default.
                 *   
                 *   If the default is a constant, call t3GetNamedArg(name,
                 *   defVal).  This returns the argument value, or the
                 *   default value if the argument isn't defined.
                 *   
                 *   If the default value expression isn't a constant, we
                 *   can't evaluate it until we know whether or not the
                 *   argument is defined, since the expression could have
                 *   side effects that we don't want to trigger unless we
                 *   really need to evaluate the expression.  So in this
                 *   case, we have to look up the argument first, and set the
                 *   default only the argument doesn't exist.
                 *   
                 *   Note that we could handle both cases with the
                 *   non-constant handling, since it would make no difference
                 *   semantically to defer evaluating a constant until after
                 *   checking to see if the argument exists.  However, the
                 *   separate version for constants yields faster and smaller
                 *   byte code, so we use it as an optimization for a common
                 *   case.  
                 */
                if (lcl->get_defval_expr()->is_const())
                {
                    /* constant default - use t3GetNamedArg(name, defval) */
                    lcl->get_defval_expr()->gen_code(FALSE, FALSE);
                    CTPNConst::s_gen_code_str_by_mode(lcl);
                    call_internal_bif("t3GetNamedArg",
                                      TCERR_NAMED_PARAM_MISSING_FUNC, 2);
                }
                else
                {
                    /* 
                     *   Non-constant default value.  We don't want to
                     *   evaluate the argument unless necessary in this case,
                     *   so start by calling t3GetNamedArg(name).  This will
                     *   throw an error if the argument doesn't exist, so do
                     *   this in an implied try/catch block.  If we return
                     *   without an error, simply proceed with the value
                     *   returned.  If an error is thrown, catch the error
                     *   and *then* evaluate the default value.  
                     */
                    ulong start_ofs = G_cs->get_ofs();
                    CTPNConst::s_gen_code_str_by_mode(lcl);
                    call_internal_bif("t3GetNamedArg",
                                      TCERR_NAMED_PARAM_MISSING_FUNC, 1);

                    /* that's the end of the implied 'try' section */
                    ulong end_ofs = G_cs->get_ofs() - 1;

                    /* 
                     *   On a successful return, the result value will be at
                     *   top of stack, so just jump past the evaluation of
                     *   the default value.  Since we're done with this
                     *   branch, clear what it left on the stack, since the
                     *   'catch' branch will leave the same thing on the
                     *   stack in parallel.  
                     */
                    CTcCodeLabel *asi_lbl = gen_jump_ahead(OPC_JMP);
                    G_cg->note_pop();

                    /* this is the start of our implied 'catch' block */
                    G_cg->get_exc_table()->add_catch(
                        start_ofs, end_ofs, VM_INVALID_OBJ, G_cs->get_ofs());
                    G_cg->note_push();

                    /* discard the exception */
                    G_cg->write_op(OPC_DISC);
                    G_cg->note_pop();

                    /* generate the default value expression */
                    lcl->get_defval_expr()->gen_code(FALSE, FALSE);

                    /* this is the common 'assign' branch point */
                    def_label_pos(asi_lbl);
                }

                /* assign the top-of-stack value to the local */
                lcl->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=", 0, 
                                  FALSE, FALSE, 0);
            }
            else
            {
                /* it's a positional parameter - generate it */
                dctx.gen_opt_positional(lcl);
            }
        }
    }

    /*
     *   If this is a dynamic function (a DynamicFunc object), and it's
     *   defined with the 'function' keyword (not 'method'), and it has a
     *   'self', we need to set up the method context to point to the
     *   enclosing frame object's method context.  
     */
    if (G_cg->is_eval_for_dyn()
        && is_dyn_func_
        && self_valid_
        && (self_referenced_ || full_method_ctx_referenced_))
    {
        /*
         *   We need the method context.  For dyanmic code, the invokee is
         *   the DynamicFunc, and the DynamicFunc makes the context available
         *   via its indexed value [1].  
         */
        G_cg->write_op(OPC_PUSHCTXELE);
        G_cs->write(PUSHCTXELE_INVOKEE);
        CTPNConst::s_gen_code_int(1);
        G_cg->write_op(OPC_INDEX);

        /* we pushed the invokee and popped it again with the INDEX */
        G_cg->note_push();
        G_cg->note_pop();

        /* 
         *   The invokee will store the appropriate object in its [1]
         *   element, according to which type of method context we need.
         *   Generate the appropriate 'set' for it.  
         */
        if (full_method_ctx_referenced_)
        {
            /* load the full method context */
            G_cg->write_op(OPC_LOADCTX);
        }
        else
        {
            /* load the 'self' object */
            G_cg->write_op(OPC_SETSELF);
        }

        /* that pops the context object */
        G_cg->note_pop();
    }

    /* 
     *   Generate code to initialize each enclosing-context-pointer local -
     *   these variables allow us to find the context objects while we're
     *   running inside this function.
     *   
     *   Before 3.1, we had to generate context level 1 last, because this
     *   level sets 'self' for an anonymous function to the saved 'self' from
     *   the context.  Starting with 3.1, the stack frame has a separate
     *   entry for the invokee, so we no longer conflate the anonymous
     *   function object with 'self'.  
     */
    CTcCodeBodyCtx *cur_ctx;
    int ctx_idx;
    for (ctx_idx = 0, cur_ctx = ctx_head_ ; cur_ctx != 0 ;
         cur_ctx = cur_ctx->nxt_, ++ctx_idx)
    {
        /* 
         *   Get this context value, stored in invokee[n+2].  Note that the
         *   context object indices start at 2 because the FUNCPTR value for
         *   the code itself is at index 1.  
         */
        G_cg->write_op(OPC_PUSHCTXELE);
        G_cs->write(PUSHCTXELE_INVOKEE);
        CTPNConst::s_gen_code_int(ctx_idx + 2);
        G_cg->write_op(OPC_INDEX);

        /* 
         *   we pushed the object, then popped the object and index and
         *   pushed the indexed value - this is a net of no change with one
         *   maximum push 
         */
        G_cg->note_push();
        G_cg->note_pop();

        /*
         *   If this is context level 1, and this context has a 'self', and
         *   we need either 'self' or the full method context from the
         *   lexically enclosing scope, generate code to load the self or the
         *   full method context (as appropriate) from our local context.
         *   
         *   The enclosing method context is always stored in the context at
         *   level 1, because this is inherently shared context for all
         *   enclosed lexical scopes.  We thus only have to worry about this
         *   for context level 1.  
         */
        if (cur_ctx->level_ == 1
            && !is_anon_method_
            && self_valid_
            && (self_referenced_ || full_method_ctx_referenced_))
        {
            CTPNCodeBody *outer;
            
            /* 
             *   we just put our context object on the stack in preparation
             *   for storing it - make a duplicate copy of it for our own
             *   purposes 
             */
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
            
            /* get the saved method context from the context object */
            CTPNConst::s_gen_code_int(TCPRS_LOCAL_CTX_METHODCTX);
            G_cg->write_op(OPC_INDEX);
                
            /* 
             *   Load the context.  We must check the outermost context to
             *   determine what it stored, because we must load whatever it
             *   stored.  
             */
            if ((outer = get_outermost_enclosing()) != 0
                && outer->local_ctx_needs_full_method_ctx())
            {
                /* load the full method context */
                G_cg->write_op(OPC_LOADCTX);
            }
            else
            {
                /* load the 'self' object */
                G_cg->write_op(OPC_SETSELF);
            }
            
            /* 
             *   we popped two values and pushed one in the INDEX, then
             *   popped a value in the LOADCTX or SETSELF: the net is removal
             *   of two elements and no additional maximum depth 
             */
            G_cg->note_pop(2);
        }

        /* store the context value in the appropriate local variable */
        CTcSymLocal::s_gen_code_setlcl_stk(cur_ctx->var_num_, FALSE);
    }

    /* 
     *   if we created our own local context, and we have a 'self' object,
     *   and we need access to the 'self' object or the full method context
     *   from anonymous functions that refer to the local context, generate
     *   code to store the appropriate data in the local context 
     */
    if (has_local_ctx_ && self_valid_
        && (local_ctx_needs_self_ || local_ctx_needs_full_method_ctx_))
    {
        /* 
         *   Check to see what we need.  If we're marked as needing the full
         *   method context, OR our outermost enclosing context has the full
         *   context, we need to generate the full context.  Intermediate
         *   nesting levels of anonymous functions can need less context than
         *   inner levels, but the context we actually generate at the outer
         *   level depends on the innermost function's needs.  So we simply
         *   need to follow the lead of the outermost level, since that's
         *   what the inner levels will all do.  
         */
        CTPNCodeBody *outer = get_outermost_enclosing();
        if (local_ctx_needs_full_method_ctx_
            || (outer != 0 && outer->local_ctx_needs_full_method_ctx()))
        {
            /* 
             *   we need the full method context - generate code to store it
             *   and push a reference to it onto the stack 
             */
            G_cg->write_op(OPC_STORECTX);
        }
        else
        {
            /* we only need 'self' - push it */
            G_cg->write_op(OPC_PUSHSELF);
        }

        /* we just pushed one value */
        G_cg->note_push();

        /* assign the value to the context variable */
        if (local_ctx_var_ <= 255 && TCPRS_LOCAL_CTX_METHODCTX <= 255)
        {
            /* we can make the assignment with a single instruction */
            G_cg->write_op(OPC_SETINDLCL1I8);
            G_cs->write((uchar)local_ctx_var_);
            G_cs->write(TCPRS_LOCAL_CTX_METHODCTX);

            /* that pops one value */
            G_cg->note_pop();
        }
        else
        {
            /* get the context object */
            CTcSymLocal::s_gen_code_getlcl(local_ctx_var_, FALSE);
            
            /* store the data in the local context object */
            CTPNConst::s_gen_code_int(TCPRS_LOCAL_CTX_METHODCTX);
            G_cg->write_op(OPC_SETIND);

            /* discard the indexed result */
            G_cg->write_op(OPC_DISC);
        
            /* 
             *   the SETIND pops three values and pushes one, then we pop one
             *   more with the DISC - this is a net three pops with no extra
             *   maximum depth 
             */
            G_cg->note_pop(3);
        }
    }

    /* generate the compound statement, if we have one */
    if (stm_ != 0)
        stm_->gen_code(TRUE, TRUE);

#ifdef T3_DEBUG
    if (G_cg->get_sp_depth() != 0)
    {
        printf("---> stack depth is %d after block codegen (line %ld)!\n",
               G_cg->get_sp_depth(), end_linenum_);
        if (fixup_owner_sym_ != 0)
            printf("---> code block for %.*s\n",
                   (int)fixup_owner_sym_->get_sym_len(),
                   fixup_owner_sym_->get_sym());
    }
#endif

    /* close the method */
    G_cg->close_method(local_cnt_, lcltab_, end_desc_, end_linenum_,
                       &gen_ctx, named_arg_tables_);

    /* remember the head of the nested symbol table list */
    first_nested_symtab_ = G_cs->get_first_frame();

    /* 
     *   Generate debug records.  If we're in debug mode, include the full
     *   symbolic debugging information, including source line locations.
     *   For release builds, include only the local variable symbols and
     *   frame information; we need those even in release builds, for
     *   reflection purposes.
     *   
     *   If we're generating this code for a debugger evaluation, omit
     *   symbols.  We don't need reflection support for debugger expressions,
     *   since there doesn't seem to be any practical reason you'd need it
     *   there.  
     */
    if (!G_cg->is_eval_for_debug())
        build_debug_table(gen_ctx.method_ofs, G_debug);

    /* check for unreferenced labels and issue warnings */
    check_unreferenced_labels();

    /* show the disassembly of the code block if desired */
    if (G_disasm_out != 0)
        show_disassembly(gen_ctx.method_ofs,
                         gen_ctx.code_start_ofs, gen_ctx.code_end_ofs);

    /* clean up globals for the end of the method */
    G_cg->close_method_cleanup(&gen_ctx);
}

/*
 *   disassembly stream source implementation 
 */
class CTcUnasSrcCodeBody: public CTcUnasSrc
{
public:
    CTcUnasSrcCodeBody(CTcCodeStream *str,
                       unsigned long code_start_ofs,
                       unsigned long code_end_ofs)
    {
        /* remember the stream */
        str_ = str;

        /* start at the starting offset */
        cur_ofs_ = code_start_ofs;

        /* remember the ending offset */
        end_ofs_ = code_end_ofs;
    }

    /* read the next byte */
    int next_byte(char *ch)
    {
        /* if there's anything left, return it */
        if (cur_ofs_ < end_ofs_)
        {
            /* return the next byte */
            *ch = str_->get_byte_at(cur_ofs_);
            ++cur_ofs_;
            return 0;
        }
        else
        {
            /* indicate end of file */
            return 1;
        }
    }

    /* get the current offset */
    ulong get_ofs() const { return cur_ofs_; }

protected:
    /* code stream */
    CTcCodeStream *str_;

    /* current offset */
    unsigned long cur_ofs_;

    /* offset of end of code stream */
    unsigned long end_ofs_;
};

/*
 *   Show the disassembly of this code block 
 */
void CTPNCodeBody::show_disassembly(unsigned long start_ofs,
                                    unsigned long code_start_ofs,
                                    unsigned long code_end_ofs)
{
    int argc;
    int locals;
    int total_stk;
    unsigned exc_rel;
    unsigned dbg_rel;

    /* first, dump the header */
    argc = (unsigned char)G_cs->get_byte_at(start_ofs);
    locals = G_cs->readu2_at(start_ofs + 2);
    total_stk = G_cs->readu2_at(start_ofs + 4);
    exc_rel = G_cs->readu2_at(start_ofs + 6);
    dbg_rel = G_cs->readu2_at(start_ofs + 8);
    G_disasm_out->print("%8lx .code\n", start_ofs);
    G_disasm_out->print("         .argcount %d%s\n",
                        (argc & 0x7f),
                        (argc & 0x80) != 0 ? "+" : "");
    G_disasm_out->print("         .locals %d\n", locals);
    G_disasm_out->print("         .maxstack %d\n", total_stk);

    /* set up a code stream reader and dump the code stream */
    CTcUnasSrcCodeBody src(G_cs, code_start_ofs, code_end_ofs);
    CTcT3Unasm::disasm(&src, G_disasm_out);

    /* show the exception table, if there is one */
    if (exc_rel != 0)
    {
        unsigned long exc_ofs;
        unsigned long exc_end_ofs;

        /* get the starting address */
        exc_ofs = start_ofs + exc_rel;

        /* 
         *   get the length - it's the part up to the debug records, or the
         *   part up to the current code offset if there are no debug records
         */
        exc_end_ofs = (dbg_rel != 0 ? start_ofs + dbg_rel : G_cs->get_ofs());

        /* show the table */
        G_disasm_out->print(".exceptions\n");
        CTcUnasSrcCodeBody exc_src(G_cs, exc_ofs, exc_end_ofs);
        CTcT3Unasm::show_exc_table(&exc_src, G_disasm_out, start_ofs);
    }

    /* add a blank line at the end */
    G_disasm_out->print("\n");
}

/*
 *   Check for unreferenced local variables 
 */
void CTPNCodeBody::check_locals()
{
    CTcPrsSymtab *tab;
    
    /* check for unreferenced locals in each nested scope */
    for (tab = first_nested_symtab_ ; tab != 0 ; tab = tab->get_list_next())
    {
        /* check this table */
        tab->check_unreferenced_locals();
    }
}

/* 
 *   local symbol table enumerator for checking for parameter symbols that
 *   belong in the local context 
 */
void CTPNCodeBody::enum_for_param_ctx(void *, class CTcSymbol *sym)
{
    /* if this is a local, check it further */
    if (sym->get_type() == TC_SYM_LOCAL || sym->get_type() == TC_SYM_PARAM)
    {
        CTcSymLocal *lcl = (CTcSymLocal *)sym;

        /* 
         *   if it's a parameter, and it's also a context variable, its
         *   value needs to be moved into the context 
         */
        if (lcl->is_param() && lcl->is_ctx_local())
        {
            /* get the actual parameter value from the stack */
            CTcSymLocal::s_gen_code_getlcl(lcl->get_var_num(), TRUE);

            /* 
             *   Store the value in the context variable.  This is an implied
             *   assignment equivalent to binding a formal to its actual
             *   value. 
             */
            lcl->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=",
                              0, TRUE, FALSE, 0);
        }
    }
}

/*
 *   local symbol table enumerator for generating named argument bindings 
 */
void CTPNCodeBody::enum_for_named_params(void *ctx, class CTcSymbol *sym)
{
    /* 
     *   If this is a local, and it's a named parameter, generate code to
     *   bind it.  If it's optional and has a default value expression, skip
     *   it on this pass, since we need to generate default expressions in
     *   the order in which they appear in the argument list to ensure that
     *   dependencies are resolved properly and side effects happen in the
     *   proper order.  
     */
    if (sym->get_type() == TC_SYM_LOCAL)
    {
        /* get the symbol, properly cast */
        CTcSymLocal *lcl = (CTcSymLocal *)sym;

        /* check what we have */
        if (lcl->get_defval_expr() != 0)
        {
            /*   
             *   It's an optional parameter (positional or named) with a
             *   default value.  We can't generate the binding yet, since we
             *   need to generate these in left-to-right order, and the table
             *   enumeration is in hash-table order instead.  So just add it
             *   to the context list, which we'll sort into the proper
             *   left-right sequence later.
             */
            ((defval_ctx *)ctx)->add(lcl);
        }
        else if (lcl->is_named_param())
        {
            /* 
             *   It's a named parameter - generate the binding code.  If it's
             *   optional, call t3GetNamedArg(name, nil) to use the default
             *   nil value if it's not defined.  If it's not optional, use
             *   t3GetNamedArg(name) to throw an error on failure.
             */
            int fargc = 1;
            if (lcl->is_opt_param())
            {
                /* optional parameter - call t3GetNamedArg(argname, nil) */
                G_cg->write_op(OPC_PUSHNIL);
                G_cg->note_push();
                ++fargc;
            }

            /* push the parameter name and call the appropriate builtin */
            CTPNConst::s_gen_code_str_by_mode(sym);
            call_internal_bif("t3GetNamedArg",
                              TCERR_NAMED_PARAM_MISSING_FUNC, fargc);

            /* 
             *   Assign the result to the local.  This is an implied
             *   assignment equivalent to binding an actual argument value to
             *   this formal. 
             */
            lcl->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=",
                              0, FALSE, FALSE, 0);
        }
        else if (lcl->is_opt_param())
        {
            /* 
             *   It's an optional parameter without a default value (we know
             *   there's no default, since we handled that case above).
             *   Generate code to load it if present.  
             */
            defval_ctx::gen_opt_positional(lcl);
        }
    }
}

/*
 *   Add a named argument call table to the list.  This is invoked each time
 *   we generate a call that uses named parameters.  We defer generation of
 *   the argument name tables until the end of the method, to avoid having to
 *   jump past inline tables while executing opcodes.  
 */
CTcCodeLabel *CTPNCodeBody::add_named_arg_tab(const CTcNamedArgs *named_args)
{
    /* allocate the tracking object */
    CTcNamedArgTab *tab = new (G_prsmem) CTcNamedArgTab(named_args);

    /* link it into our list */
    tab->nxt = named_arg_tables_;
    named_arg_tables_ = tab;

    /* return the code label for the table */
    return tab->lbl;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger records and local symbol table
 */

/* local symbol enumeration callback context */
struct write_local_to_debug_frame_ctx
{
    /* are we just counting the entries? */
    int count_only;

    /* number of symbols written so far */
    int count;
};

/*
 *   Callback for symbol table enumeration - write a local variable entry to
 *   the code stream for a debug frame record.  
 */
void CTPNCodeBody::write_local_to_debug_frame(void *ctx0, CTcSymbol *sym)
{
    write_local_to_debug_frame_ctx *ctx;

    /* cast our context */
    ctx = (write_local_to_debug_frame_ctx *)ctx0;

    /* write it out */
    if (sym->write_to_debug_frame(ctx->count_only))
    {
        /* we wrote the symbol - count it */
        ++(ctx->count);
    }
}

/*
 *   Build the debug information table for a code body 
 */
void CTPNCodeBody::build_debug_table(ulong start_ofs, int include_lines)
{
    size_t i;

    /*
     *   Count up the frames that contain local variables.  Frames that
     *   contain no locals can be elided to reduce the size of the file,
     *   since they carry no information.  
     */
    CTcPrsSymtab *frame;
    int frame_cnt;
    for (frame_cnt = 0, frame = G_cs->get_first_frame() ; frame != 0 ;
         frame = frame->get_list_next())
    {
        /* enumerate the entries through our counting callback */
        write_local_to_debug_frame_ctx cbctx;
        cbctx.count = 0;
        cbctx.count_only = TRUE;
        frame->enum_entries(&write_local_to_debug_frame, &cbctx);

        /* check to see if it has any locals */
        if (cbctx.count != 0)
        {
            /* it has locals - renumber it to the current list position */
            ++frame_cnt;
            frame->set_list_index(frame_cnt);
        }
        else
        {
            /* no locals - flag it as empty by setting its list index to 0 */
            frame->set_list_index(0);
        }
    }

    /*
     *   If we're not writing line records, and we don't have any frames with
     *   local variables, skip writing the debug table.  The debug table in
     *   this case is for local variable reflection information only, so if
     *   we don't have any locals, we don't need the table at all.  
     */
    if (!include_lines && frame_cnt == 0)
        return;

    /* fix up the debug record offset in the prolog to point here */
    G_cs->write2_at(start_ofs + 8, G_cs->get_ofs() - start_ofs);

    /*
     *   Write the debug table header.  In the current VM version, this is
     *   empty, so we have nothing to write.  However, if we're dynamically
     *   targeting a newer version, pad out any expected header space with
     *   zeros.  
     */
    for (int j = G_sizes.dbg_hdr ; j > 0 ; --j)
        G_cs->write(0);

    /* 
     *   Add this offset to our list of line records.  If we're creating an
     *   object file, upon re-loading the object file, we'll need to go
     *   through all of the line record tables and fix up the file references
     *   to the final file numbering system at link time, so we need this
     *   memory of where the line record are.  We'll also need to fix up the
     *   local variable name records to point to the consolidated set of
     *   strings in the constant pool.  
     */
    G_cg->add_debug_line_table(G_cs->get_ofs());

    /* if desired, write the source code location ("line") records */
    if (include_lines)
    {
        /* write the number of line records */
        G_cs->write2(G_cs->get_line_rec_count());

        /* write the line records themselves */
        for (i = 0 ; i < G_cs->get_line_rec_count() ; ++i)
        {
            /* get this record */
            tcgen_line_t *rec = G_cs->get_line_rec(i);

            /* write the offset of the statement's first opcode */
            G_cs->write2(rec->ofs);

            /* write the source file ID and line number */
            G_cs->write2(rec->source_id);
            G_cs->write4(rec->source_line);

            /* 
             *   Get the frame for the line record.  If the frame is empty,
             *   use the parent frame instead, since we omit empty frames.
             */
            CTcPrsSymtab *fr = rec->frame;
            while (fr != 0 && fr->get_list_index() == 0)
                fr = fr->get_parent();

            /* write the frame ID */
            G_cs->write2(fr == 0 ? 0 : fr->get_list_index());

            /* add any padding expected in the target VM version */
            for (int j = G_sizes.dbg_line - 10 ; j > 0 ; --j)
                G_cs->write(0);
        }
    }
    else
    {
        /* no line records - write zero as the line count */
        G_cs->write2(0);
    }

    /* 
     *   write a placeholder pointer to the next byte after the end of the
     *   frame table 
     */
    ulong post_ptr_ofs = G_cs->get_ofs();
    G_cs->write2(0);

    /* write the frame count */
    G_cs->write2(frame_cnt);

    /* 
     *   Write a placeholder frame index table.  We will come back and fix up
     *   this table as we actually write out the frames, but we don't
     *   actually know how big the individual frame records will be yet, so
     *   we can only write placeholders for them for now.  First, note where
     *   the frame index table begins.  
     */
    ulong index_ofs = G_cs->get_ofs();

    /* write the placeholder index entries */
    for (i = 0 ; i < (size_t)frame_cnt ; ++i)
        G_cs->write2(0);

    /* write the individual frames */
    for (frame = G_cs->get_first_frame() ; frame != 0 ;
         frame = frame->get_list_next())
    {
        /* 
         *   if this frame has a zero list index, it means that it contains
         *   no writable symbols, so we can omit it from the debug table 
         */
        if (frame->get_list_index() == 0)
            continue;

        /* 
         *   go back and fill in the correct offset (from the entry itself)
         *   in the index table entry for this frame 
         */
        G_cs->write2_at(index_ofs, G_cs->get_ofs() - index_ofs);

        /* move on to the next index entry */
        index_ofs += 2;

        /* 
         *   Get the effective parent.  If the actual parent doesn't have a
         *   list index, it's empty, so we're not writing it.  Find the
         *   nearest parent of the parent that we'll actually write, and use
         *   that as our effective parent. 
         */
        CTcPrsSymtab *par;
        for (par = frame->get_parent() ;
             par != 0 && par->get_list_index() == 0 ;
             par = par->get_parent()) ;

        /* write the ID of the enclosing frame */
        G_cs->write2(par != 0 ? par->get_list_index() : 0);

        /* 
         *   write a placeholder for the count of the number of entries in
         *   the frame, and remember where the placeholder is so we can come
         *   back and fix it up later 
         */
        ulong count_ofs = G_cs->get_ofs();
        G_cs->write2(0);

        /* add the bytecode range covered, if there's room in the format */
        if (G_sizes.dbg_frame >= 8)
        {
            G_cs->write2(frame->get_start_ofs());
            G_cs->write2(frame->get_end_ofs());
        }

        /* pad out any extra space */
        for (int j = G_sizes.dbg_frame - 8 ; j > 0 ; --j)
            G_cs->write(0);

        /* initialize the enumeration callback context */
        write_local_to_debug_frame_ctx cbctx;
        cbctx.count = 0;
        cbctx.count_only = FALSE;

        /* write this frame table's entries */
        frame->enum_entries(&write_local_to_debug_frame, &cbctx);

        /* go back and fix up the symbol count */
        G_cs->write2_at(count_ofs, cbctx.count);
    }

    /* 
     *   go back and fill in the post-pointer offset - this is a pointer to
     *   the next byte after the end of the frame table; write the offset
     *   from the post-pointer field to the current location 
     */
    G_cs->write2_at(post_ptr_ofs, G_cs->get_ofs() - post_ptr_ofs);

    /* 
     *   write the required UINT4 zero value after the frame table - this
     *   is a placeholder for future expansion (if we add more information
     *   to the debug table later, this value will be non-zero to indicate
     *   the presence of the additional information) 
     */
    G_cs->write4(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   'return' statement 
 */

/*
 *   generate code 
 */
void CTPNStmReturn::gen_code(int, int)
{
    int val_on_stack;
    int need_gen;

    /* add a line record */
    add_debug_line_rec();

    /* presume we'll generate a value */
    need_gen = TRUE;
    val_on_stack = FALSE;

    /* generate the return value expression, if appropriate */
    if (expr_ != 0)
    {
        /* 
         *   it's an error if we're in a constructor, because a
         *   constructor implicitly always returns 'self' 
         */
        if (G_cg->is_in_constructor())
            log_error(TCERR_CONSTRUCT_CANNOT_RET_VAL);

        /* check for a constant expression */
        if (expr_->is_const())
        {
            switch(expr_->get_const_val()->get_type())
            {
            case TC_CVT_NIL:
            case TC_CVT_TRUE:
                /* 
                 *   we can use special constant return instructions for
                 *   these, so there's no need to generate the value 
                 */
                need_gen = FALSE;
                break;

            default:
                /* 
                 *   other types don't have constant-return opcodes, so we
                 *   must generate the expression code 
                 */
                need_gen = TRUE;
                break;
            }
        }

        /* if necessary, generate the value */
        if (need_gen)
        {
            int depth;

            /* note the initial stack depth */
            depth = G_cg->get_sp_depth();

            /*  
             *   Generate the value.  We are obviously not discarding the
             *   value, and since returning a value is equivalent to
             *   assigning the value, we must use the stricter assignment
             *   (not 'for condition') rules for logical expressions 
             */
            expr_->gen_code(FALSE, FALSE);

            /* note whether we actually left a value on the stack */
            val_on_stack = (G_cg->get_sp_depth() > depth);
        }
        else
        {
            /* 
             *   we obviously aren't leaving a value on the stack if we
             *   don't generate anything 
             */
            val_on_stack = FALSE;
        }
    }

    /* 
     *   Before we return, let any enclosing statements generate any code
     *   necessary to leave their scope (in particular, we must invoke
     *   'finally' handlers in any enclosing 'try' blocks).
     *   
     *   Note that we generated the expression BEFORE we call any
     *   'finally' handlers.  This is necessary because something we call
     *   in the course of evaluating the return value could have thrown an
     *   exception; if we were to call the 'finally' clauses before
     *   generating the return value, we could invoke the 'finally' clause
     *   twice (once explicitly, once in the handling of the thrown
     *   exception), which would be incorrect.  By generating the
     *   'finally' calls after the return expression, we're sure that the
     *   'finally' blocks are invoked only once - either through the
     *   throw, or else now, after there's no more possibility of a
     *   'throw' before the return.  
     */
    if (G_cs->get_enclosing() != 0)
    {
        int did_save_retval;
        uint fin_ret_lcl;

        /* 
         *   if we're going to generate any subroutine calls, and we have
         *   a return value on the stack, we need to save the return value
         *   in a local to make sure the calculated value isn't affected
         *   by the subroutine call 
         */
        if (val_on_stack
            && G_cs->get_enclosing()->will_gen_code_unwind_for_return()
            && G_cs->get_code_body() != 0)
        {
            /* allocate a local variable to save the return value */
            fin_ret_lcl = G_cs->get_code_body()->alloc_fin_ret_lcl();

            /* save the return value in a stack temporary for a moment */
            CTcSymLocal::s_gen_code_setlcl_stk(fin_ret_lcl, FALSE);

            /* 
             *   note that we saved the return value, so we can retrieve
             *   it later 
             */
            did_save_retval = TRUE;
        }
        else
        {
            /* note that we didn't save the return value */
            did_save_retval = FALSE;
        }

        /* generate the unwind */
        G_cs->get_enclosing()->gen_code_unwind_for_return();

        /* if we saved the return value, retrieve it */
        if (did_save_retval)
            CTcSymLocal::s_gen_code_getlcl(fin_ret_lcl, FALSE);
    }

    /* check for an expression to return */
    if (G_cg->is_in_constructor())
    {
        /* we're in a constructor - return 'self' */
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->write_op(OPC_RETVAL);
    }
    else if (expr_ == 0)
    {
        /* 
         *   there's no expression - generate a simple void return (but
         *   explicitly return nil, so we don't return something left in
         *   R0 from a previous function call we made) 
         */
        G_cg->write_op(OPC_RETNIL);
    }
    else
    {
        /* check for a constant expression */
        if (expr_->is_const())
        {
            switch(expr_->get_const_val()->get_type())
            {
            case TC_CVT_NIL:
                /* generate a RETNIL instruction */
                G_cg->write_op(OPC_RETNIL);
                break;

            case TC_CVT_TRUE:
                /* generate a RETTRUE instruction */
                G_cg->write_op(OPC_RETTRUE);
                break;

            default:
                break;
            }
        }

        /* 
         *   if we needed code generation to evaluate the return value, we
         *   now need to return the value 
         */
        if (need_gen)
        {
            /* 
             *   Other types don't have constant-return opcodes.  We
             *   already generated the expression value (before invoking
             *   the enclosing 'finally' handlers, if any), so the value
             *   is on the stack, and all we need to do is return it.
             *   
             *   If we didn't actually leave a value on the stack, we'll
             *   just return nil.  
             */
            if (val_on_stack)
            {
                /* generate the return-value opcode */
                G_cg->write_op(OPC_RETVAL);
                
                /* RETVAL removes an element from the stack */
                G_cg->note_pop();
            }
            else
            {
                /* 
                 *   The depth didn't change - they must have evaluated an
                 *   expression involving a dstring or void function.
                 *   Return nil instead of the non-existent value.  
                 */
                G_cg->write_op(OPC_RETNIL);
            }
        }
    }
}
    
/* ------------------------------------------------------------------------ */
/*
 *   Static property initializer statement 
 */
void CTPNStmStaticPropInit::gen_code(int, int)
{
    int depth;
    
    /* add a line record */
    add_debug_line_rec();

    /* note the initial stack depth */
    depth = G_cg->get_sp_depth();

    /* generate the expression, keeping the generated value */
    expr_->gen_code(FALSE, FALSE);

    /* ensure that we generated a value; if we didn't, push nil by default */
    if (G_cg->get_sp_depth() <= depth)
    {
        /* push a default nil value */
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }

    /* 
     *   duplicate the value on the stack, so we can assign it to
     *   initialize the property and also return it 
     */
    G_cg->write_op(OPC_DUP);
    G_cg->note_push();

    /* write the SETPROPSELF to initialize the property */
    G_cg->write_op(OPC_SETPROPSELF);
    G_cs->write_prop_id(prop_);

    /* SETPROPSELF removes the value */
    G_cg->note_pop();

    /* return the value (which we duplicated on the stack) */
    G_cg->write_op(OPC_RETVAL);

    /* RETVAL removes the value */
    G_cg->note_pop();
}


/* ------------------------------------------------------------------------ */
/*
 *   Object Definition Statement 
 */

/*
 *   generate code 
 */
void CTPNStmObject::gen_code(int, int)
{
    /* if this object has been replaced, don't generate any code for it */
    if (replaced_)
        return;

    /* add an implicit constructor if necessary */
    add_implicit_constructor();

    /* get the appropriate stream for generating the data */
    CTcDataStream *str = obj_sym_->get_stream();

    /* clear the internal flags */
    uint internal_flags = 0;

    /* 
     *   if we're a modified object, set the 'modified' flag in the object
     *   header 
     */
    if (modified_)
        internal_flags |= TCT3_OBJ_MODIFIED;

    /* set the 'transient' flag if appropriate */
    if (transient_)
        internal_flags |= TCT3_OBJ_TRANSIENT;

    /* clear the object flags */
    uint obj_flags = 0;

    /* 
     *   If we're specifically marked as a 'class' object, or we're a
     *   modified object, set the 'class' flag in the object flags.  
     */
    if (is_class_ || modified_)
        obj_flags |= TCT3_OBJFLG_CLASS;

    /* remember our starting offset in the object stream */
    ulong start_ofs = str->get_ofs();

    /* 
     *   store our stream offset in our defining symbol, for storage in
     *   the object file 
     */
    obj_sym_->set_stream_ofs(start_ofs);

    /* write our internal flags */
    str->write2(internal_flags);

    /* 
     *   First, write the per-object image file "OBJS" header - each
     *   object starts with its object ID and the number of bytes in the
     *   object's metaclass-specific data.  For now, write zero as a
     *   placeholder for our data size.  Note that this is a
     *   self-reference: it must be modified if the object is renumbered.  
     */
    str->write_obj_id_selfref(obj_sym_);
    str->write2(0);

    /* write a placeholder for the superclass count */
    str->write2(0);

    /* write the fixed property count */
    str->write2(proplist_.cnt_);

    /* write the object flags */
    str->write2(obj_flags);

    /*
     *   First, go through the superclass list and verify that each
     *   superclass is actually an object.  
     */
    int bad_sc = FALSE;
    int sc_cnt = 0;
    for (CTPNSuperclass *sc = get_first_sc() ; sc != 0 ; sc = sc->nxt_)
    {
        /* look up the superclass in the global symbol table */
        CTcSymObj *sc_sym = (CTcSymObj *)sc->get_sym();

        /* make sure it's defined, and that it's really an object */
        if (sc_sym == 0)
        {
            /* not defined */
            log_error(TCERR_UNDEF_SYM_SC,
                      (int)sc->get_sym_len(), sc->get_sym_txt(),
                      (int)obj_sym_->get_sym_len(), obj_sym_->get_sym());

            /* note that we have an invalid superclass */
            bad_sc = TRUE;
        }
        else if (sc_sym->get_type() != TC_SYM_OBJ)
        {
            /* log an error */
            log_error(TCERR_SC_NOT_OBJECT,
                      (int)sc_sym->get_sym_len(), sc_sym->get_sym());

            /* note that we have an invalid superclass */
            bad_sc = TRUE;
        }
        else
        {
            /* count the superclass */
            ++sc_cnt;

            /* write the superclass to the object header */
            str->write_obj_id(sc_sym->get_obj_id());
        }
    }

    /* 
     *   If we detected a 'bad template' error when we were parsing the
     *   object definition, and all of our superclasses are valid, report the
     *   template error.
     *   
     *   Do not report this error if we have any undefined or invalid
     *   superclasses, because (1) we've already reported one error for this
     *   object definition (the bad superclass error), and (2) the missing
     *   template is likely just a consequence of the bad superclass, since
     *   we can't have scanned the proper superclass's list of templates if
     *   they didn't tell us the correct superclass to start with.  When they
     *   fix the superclass list and re-compile the code, it's likely that
     *   this will fix the template problem as well, since we'll probably be
     *   able to find the template give the corrected superclass list.
     *   
     *   If we found an undescribed class anywhere in our hierarchy, a
     *   template simply cannot be used with this object; otherwise, the
     *   error is that we failed to find a suitable template 
     */
    if (has_bad_template() && !bad_sc)
        log_error(has_undesc_sc()
                  ? TCERR_OBJ_DEF_CANNOT_USE_TEMPLATE
                  : TCERR_OBJ_DEF_NO_TEMPLATE);

    /* go back and write the superclass count to the header */
    str->write2_at(start_ofs + TCT3_TADSOBJ_HEADER_OFS, sc_cnt);

    /*
     *   Write the properties.  We're required to write the properties in
     *   sorted order of property ID, but we can't do that yet, because
     *   the property ID's aren't finalized until after linking.  For now,
     *   just write them out in the order in which they were defined.  
     */
    for (CTPNObjProp *prop = proplist_.first_ ; prop != 0 ; prop = prop->nxt_)
    {
        /* make sure we have a valid property symbol */
        if (prop->get_prop_sym() != 0)
        {
            /* write the property ID */
            str->write_prop_id(prop->get_prop_sym()->get_prop());

            /* generate code for the property */
            prop->gen_code(FALSE, FALSE);
        }
    }

    /* 
     *   go back and write the size of our metaclass-specific data - this
     *   goes at offset 4 in the T3 generic metaclass header
     */
    str->write2_at(start_ofs + TCT3_META_HEADER_OFS + 4,
                   str->get_ofs() - (start_ofs + TCT3_META_HEADER_OFS + 6));
}

/*
 *   Check for unreferenced local variables 
 */
void CTPNStmObject::check_locals()
{
    /* check for unreferenced locals for each property */
    for (CTPNObjProp *prop = proplist_.first_ ; prop != 0 ; prop = prop->nxt_)
        prop->check_locals();
}
