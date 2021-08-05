/* $Header$ */

/* Copyright (c) 2012 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmfindrep.h - find/replace common code template
Function
  Defines the common code template for String.findReplace() and rexReplace().
  The two functions are very similar, but have some slight interface
  differences.  Implementing as a common subroutine has a measurable
  negative impact on performance; typical TADS games make heavy use of
  these functions, so optimizing them for speed is beneficial.  By using
  a common code template, we can avoid the extra calls and run-time
  condition checking that's necessary in a common subroutine; in the
  template version, we essentially unroll the code and apply the conditions
  and test the conditions statically.  This makes for larger but faster
  code.
Notes
  
Modified
  06/17/12 MJRoberts  - Creation
*/

#ifndef VMFINDREP_H
#define VMFINDREP_H

/* ------------------------------------------------------------------------ */
/*
 *   re_replace flags 
 */

/* replace all matches (if omitted, replaces only the first match) */
#define VMBIFTADS_REPLACE_ALL     0x0001

/* ignore case in matching the search string */
#define VMBIFTADS_REPLACE_NOCASE  0x0002

/* 
 *   follow case: lower-case characters in the the replacement text are
 *   converted to follow the case pattern of the matched text (all lower,
 *   initial capital, or all capitals) 
 */
#define VMBIFTADS_REPLACE_FOLLOW_CASE  0x0004

/* 
 *   serial replacement: when a list of search patterns is used, we replace
 *   each occurrence of the first pattern over the whole string, then we
 *   start over with the result and scan for occurrences of the second
 *   pattern, replacing each of these, and so on.  If this flag isn't
 *   specified, the default is parallel scanning: we find the leftmost
 *   occurrence of any of the patterns and replace it; then we scan the
 *   remainder of the string for the leftmost occurrence of any pattern, and
 *   replace that one; and so on.  The serial case is equivalent to making a
 *   series of calls to this function with the individual search patterns.  
 */
#define VMBIFTADS_REPLACE_SERIAL  0x0008

/*
 *   Replace only the first occurrence. 
 */
#define VMBIFTADS_REPLACE_ONCE    0x0010


/*
 *   re_replace pattern/replacement structure.  This is used to handle arrays
 *   of arguments: each element represents one pattern->replacement mapping.
 */
struct re_replace_arg
{
    re_replace_arg()
    {
        s = 0;
        pat = 0;
        pat_str = 0;
        rpl_func.set_nil();
        match_valid = FALSE;
    }

    ~re_replace_arg()
    {
        /* if we created the pattern object, delete it */
        if (pat != 0 && our_pat)
            CRegexParser::free_pattern(pat);
        if (s != 0)
            delete s;
    }

    void set(VMG_ const vm_val_t *patv, const vm_val_t *rplv,
             int str_to_pat, int32_t flags)
    {
        /* save the flags */
        this->flags = flags;

        /* retrieve the compiled RexPattern or uncompiled pattern string */
        const char *str;
        if (patv->typ == VM_OBJ
            && CVmObjPattern::is_pattern_obj(vmg_ patv->val.obj))
        {
            /* create the searcher, if we haven't already */
            create_searcher(vmg0_);

            /* it's a pattern object - get its compiled pattern structure */
            pat = ((CVmObjPattern *)vm_objp(vmg_ patv->val.obj))
                  ->get_pattern(vmg0_);

            /* we didn't create the pattern object */
            our_pat = FALSE;
        }
        else if ((str = patv->get_as_string(vmg0_)) != 0)
        {
            /* it's a string - decide how to treat it */
            if (str_to_pat)
            {
                /* create the searcher */
                create_searcher(vmg0_);

                /* we treat strings as regular expressions - compile it */
                re_status_t stat;
                stat = G_bif_tads_globals->rex_parser->compile_pattern(
                    str + VMB_LEN, vmb_get_len(str), &pat);

                /* if that failed, we don't have a pattern */
                if (stat != RE_STATUS_SUCCESS)
                    pat = 0;

                /* make a note that we allocated the pattern */
                our_pat = TRUE;
            }
            else
            {
                /* we treat strings as simple literal search strings */
                pat_str = str;
            }
        }
        else
        {
            /* invalid type */
            err_throw(VMERR_BAD_TYPE_BIF);
        }

        /* 
         *   Save the replacement value.  This can be a string, nil (which we
         *   treat like an empty string), or a callback function. 
         */
        if (rplv->typ == VM_NIL)
        {
            /* treat it as an empty string */
            rpl_str = "\000\000";
        }
        else if ((str = rplv->get_as_string(vmg0_)) != 0)
        {
            /* save the string value */
            rpl_str = str;
        }
        else if (rplv->is_func_ptr(vmg0_))
        {
            /* it's a function or invokable object */
            rpl_str = 0;
            rpl_func = *rplv;

            /* get the number of arguments it expects */
            CVmFuncPtr f(vmg_ rplv);
            rpl_argc = f.is_varargs() ? -1 : f.get_max_argc();
        }
        else
        {
            /* invalid type */
            err_throw(VMERR_BAD_TYPE_BIF);
        }
    }

    /* create the regex searcher */
    void create_searcher(VMG0_)
    {
        /* create our searcher if we haven't yet */
        if (s == 0)
        {
            /* create the searcher object */
            s = new CRegexSearcherSimple(G_bif_tads_globals->rex_parser);

            /* turn off case sensitivity in the searcher if desired */
            if ((flags & VMBIFTADS_REPLACE_NOCASE) != 0)
                s->set_default_case_sensitive(FALSE);
        }
    }

    void search(VMG_ const char *str, int start_idx, const char *last_str)
    {
        /* if we have a pattern, search for it */
        if (pat_str != 0)
        {
            /* 
             *   whether or not we find a match, the search will be valid
             *   when we return 
             */
            match_valid = TRUE;
            match_len = 0;

            /* set up a pointer to the current position */
            utf8_ptr p((char *)str + VMB_LEN + start_idx);

            /* figure the number of bytes left in the string */
            size_t rem = vmb_get_len(str) - start_idx;

            /* note the length of the search string */
            size_t pat_len = vmb_get_len(pat_str);

            /* scan for a match, according to case sensitivity */
            if (!(flags & VMBIFTADS_REPLACE_NOCASE))
            {
                /* case sensitive - scan for a match */
                for ( ; rem >= pat_len ; p.inc(&rem))
                {
                    /* no case folding, so simply compare byte by byte */
                    if (memcmp(p.getptr(), pat_str + VMB_LEN, pat_len) == 0)
                    {
                        /* success - remember the match length and position */
                        match_len = pat_len;
                        match_idx = p.getptr() - (str + VMB_LEN);

                        /* we're done */
                        return;
                    }
                }
            }
            else
            {
                /* case-insensitive - scan for a match */
                for ( ; rem >= pat_len ; p.inc(&rem))
                {
                    /* compare with case folding */
                    size_t ml;
                    if (t3_compare_case_fold(
                        pat_str + VMB_LEN, pat_len,
                        p.getptr(), rem, &ml) == 0)
                    {
                        /* success - set the match index, and we're done */
                        match_idx = p.getptr() - (str + VMB_LEN);
                        match_len = ml;
                        return;
                    }
                }
            }

            /* we didn't a match - set the match index to so indicate */
            match_idx = -1;
        }
        if (pat != 0)
        {
            /* search for the regular expression */
            match_idx = s->search_for_pattern(
                pat, str + VMB_LEN, last_str,
                vmb_get_len(str) - start_idx, &match_len);

            /* if we found it, adjust to the absolute offset in the string */
            if (match_idx >= 0)
                match_idx += start_idx;
        }
        else
        {
            /* no pattern -> no match */
            match_idx = -1;
        }

        /* whether or not we matched, our result is now valid */
        match_valid = TRUE;
    }

    /* our search pattern, or null if we're searching for a literal string */
    re_compiled_pattern *pat;

    /* our search string, or null if we're searching for a pattern */
    const char *pat_str;

    /* Did we create the pattern?  If so, delete it on destruction. */
    int our_pat;

    /* our replacement string, or null if it's a callback function */
    const char *rpl_str;

    /* our replacement function, in lieu of a string */
    vm_val_t rpl_func;

    /* the number of arguments rpl_func expects, or -1 for varargs */
    int rpl_argc;

    /* search flags */
    int32_t flags;

    /* the byte index and length in the source string of our last match */
    int match_idx;
    int match_len;

    /* 
     *   Is this last match data valid?  This is false if we haven't done the
     *   first search yet, or if the last replacement overlapped our matching
     *   text at all.  
     */
    int match_valid;

    /* searcher - this holds the group registers for the last match */
    CRegexSearcherSimple *s;
};

/* ------------------------------------------------------------------------ */
/* 
 *   <mode> values for vm_find_replace<mode>
 */
#define VMFINDREPLACE_StringFindReplace  0
#define VMFINDREPLACE_rexReplace         1

/*
 *   Common find-and-replace handler for String.findReplace() and
 *   rexReplace().  The two functions are essentially the same; the only
 *   differences are:
 *   
 *   1. The arguments are in a slightly different order.
 *.      rexReplace(pattern, subject, replacement, flags?, index?)
 *.      subject.findReplace(pattern, replacement, flags?, index?)
 *   
 *   Note the placement of the subject string argument; it moves outside the
 *   argument list to the target object position in the String method call
 *   version, so the subsequent arguments all move down one position.
 *   
 *   2. In String.findReplace(), the 'pattern' argument is taken as literal
 *   text if a string value is passed.  In rexReplace(), a string value for
 *   'pattern' is taken as an uncompiled regular expression.  For either
 *   function, a RexPattern object can be passed, in which case it's treated
 *   as a regular expression (obviously).
 *   
 *   3. If 'pattern' is a literal string, 'replacement' is also treated as
 *   literal text; if 'pattern' is a regular expression (either explicitly as
 *   a RexPattern object, or as a string taken to be an uncompiled regular
 *   expression in rexReplace()), '%' sequences are used for group
 *   replacements; e.g., '%1' is replaced with the matching text for the
 *   first parenthesized group in the regex, '%2' by the second group's
 *   matching text, etc.
 */
template<int mode> inline void vm_find_replace(
    VMG_ vm_val_t *result, int argc,
    const vm_val_t *subj, const char *subj_str)
{
    int i;
    vm_rcdesc rc;
    const int new_str_ofs =
        (mode == VMFINDREPLACE_StringFindReplace ? 1 : 2);
    const int pat_str_is_regex = (mode == VMFINDREPLACE_rexReplace);

    /* remember the original subject string */
    const vm_val_t *orig_subj = subj;

    /* get the flags; use ReplaceAll if omitted */
    int32_t flags = VMBIFTADS_REPLACE_ALL;
    const vm_val_t *flagp;
    if (new_str_ofs + 1 < argc &&
        (flagp = G_stk->get(new_str_ofs + 1))->typ != VM_NIL)
    {
        /* check the type */
        if (flagp->typ != VM_INT)
            err_throw(VMERR_INT_VAL_REQD);

        /* retrieve the value */
        flags = flagp->val.intval;
    }

    /*
     *   Check for old flags.  Before 3.1, there was only one flag bit
     *   defined: ALL=1.  This means there were only two valid values for
     *   'flags': 0 for ONCE mode, 1 for ALL mode.
     *   
     *   In 3.1, we added a bunch of new flags.  At the same time, we made
     *   the default ALL mode, because this is the more common case.
     *   Unfortunately, this creates a compatibility issue.  A new program
     *   that specifies one of the new flags might leave out the ONCE or ALL
     *   bits, since ALL is the default.  However, we can't just take the
     *   absence of the ONCE bit as meaning ALL, because that would hose old
     *   programs that explicitly specify ONCE as 0 (no bits set).
     *   
     *   Here's how we deal with this: we prohibit new programs from passing
     *   0 for the flags, requiring them to specify at least one bit.  So if
     *   the flags value is zero, we must have an old program that passed
     *   ONCE.  In this case, explicitly set the ONCE bit.  If we have any
     *   non-zero value, we must have either a new program OR an old program
     *   that included the ALL bit.  In either case, ALL is the default, so
     *   if the ONCE bit ISN'T set, explicitly set the ALL bit.  
     */
    if (flags == 0)
    {
        /* old program with the old ONCE flag - set the new ONCE flag */
        flags = VMBIFTADS_REPLACE_ONCE;
    }
    else if (!(flags & VMBIFTADS_REPLACE_ONCE))
    {
        /* 
         *   new program without the ONCE flag, OR an old program with the
         *   ALL flag - explicitly set the ALL flag 
         */
        flags |= VMBIFTADS_REPLACE_ALL;
    }
    
    /* assume that the limit is one or infinity, per the 'flags' value */
    int limit = ((flags & VMBIFTADS_REPLACE_ONCE) != 0 ? 1 : -1);

    /* if there's an explicit 'limit' argument, fetch it */
    if (new_str_ofs + 3 < argc)
    {
        /* nil means explicitly unlimited; otherwise it's an int */
        vm_val_t *limitp = G_stk->get(new_str_ofs + 3);
        if (limitp->typ == VM_NIL)
        {
            /* unlimited - signify with -1 */
            limit = -1;
        }
        else if (limitp->typ == VM_INT)
        {
            /* get the limit, and make sure it's non-negative */
            if ((limit = limitp->val.intval) < 0)
                err_throw(VMERR_BAD_VAL_BIF);

            /* 
             *   if the limit is zero, the result string will obviously be
             *   identical to the source string, so just return it now 
             */
            if (limit == 0)
            {
                *result = *subj;
                return;
            }
        }
        else
            err_throw(VMERR_INT_VAL_REQD);
    }

    /* get the search substring or pattern value */
    const vm_val_t *pat = G_stk->get(0);

    /* assume we have a single pattern to search for */
    int pat_cnt = 1;
    int pat_is_list = FALSE;

    /* 
     *   if we're treating strings as literals (rather than uncompiled
     *   regular expressions), assume we won't have any REs to search for
     */
    int have_pat = pat_str_is_regex;

    /* check whether the pattern is given as an array or as a single value */
    if (pat->is_listlike(vmg0_)
        && (pat_cnt = pat->ll_length(vmg0_)) >= 0)
    {
        /* It's a list.  Check first to see if it's empty. */
        if (pat_cnt == 0)
        {
            /* 
             *   empty list, so there's no work to do - simply return the
             *   original subject string unchanged 
             */
            *result = *subj;
            return;
        }

        /* flag it as a list */
        pat_is_list = TRUE;
    }

    /* get the replacement value from the stack */
    const vm_val_t *rpl = G_stk->get(new_str_ofs);

    /* assume we have a single replacement value */
    int rpl_cnt = 1;
    int rpl_is_list = FALSE;

    /* check to see if the replacement is a list */
    if (rpl->is_listlike(vmg0_)
        && (rpl_cnt = rpl->ll_length(vmg0_)) >= 0)
    {
        /* flag it as a list */
        rpl_is_list = TRUE;
    }

    /* allocate the argument array */
    re_replace_arg *pats = new re_replace_arg[pat_cnt];

    /* catch any errors so that we can free our arg array on the way out */
    err_try
    {
        /* assume we won't need a recursive callback context */
        int need_rc = FALSE;
        
        /* set up the pattern/replacement array from the arguments */
        for (i = 0 ; i < pat_cnt ; ++i)
        {
            /* get the next pattern from the list, or the single pattern */
            vm_val_t patele;
            if (pat_is_list)
            {
                /* we have a list - get the next element */
                pat->ll_index(vmg_ &patele, i + 1);
            }
            else
            {
                /* we have a single pattern item */
                patele = *pat;
            }

            /* 
             *   Get the next replacement from the list, or the single
             *   replacement.  If we have a single value for the replacement,
             *   every pattern has the same replacement.  If we have a list,
             *   the pattern has the replacement at the corresponding index.
             *   If it's a list and we're past the last replacement index,
             *   use an empty string.  
             */
            vm_val_t rplele;
            if (rpl_is_list)
            {
                /* 
                 *   we have a list - if we have an item at the current
                 *   index, it's the corresponding replacement for the
                 *   current pattern; if we've exhausted the replacement
                 *   list, use an empty string as the replacement 
                 */
                if (i < rpl_cnt)
                    rpl->ll_index(vmg_ &rplele, i + 1);
                else
                    rplele.set_nil();
            }
            else
            {
                /* single replacement item - it applies to all patterns */
                rplele = *rpl;
            }

            /* fill in this argument item */
            pats[i].set(vmg_ &patele, &rplele, pat_str_is_regex, flags);

            /* note if we have a regex */
            have_pat |= (pats[i].pat != 0);

            /* we need a callback context if the replacement is a function */
            need_rc |= (pats[i].rpl_str == 0);
        }

        /* initialize the recursive callback context if needed */
        if (need_rc)
        {
            if (mode == VMFINDREPLACE_StringFindReplace)
                rc.init(vmg_ "String.findReplace", orig_subj, 11,
                        G_stk->get(0), argc);
            else
                rc.init(vmg_ "rexReplace", CVmBifTADS::bif_table, 12,
                        G_stk->get(0), argc);
        }

        /* get the starting index, if present; use index 1 if not present */
        int start_char_idx = 1;
        const vm_val_t *idxp;
        if (new_str_ofs + 2 < argc
            && (idxp = G_stk->get(new_str_ofs + 2))->typ != VM_NIL)
        {
            /* check the type */
            if (idxp->typ != VM_INT)
                err_throw(VMERR_INT_VAL_REQD);
            
            /* get the value */
            start_char_idx = idxp->val.intval;
        }

        /* 
         *   push a nil placeholder for the result value (we keep it on the
         *   stack for gc protection) 
         */
        G_stk->push()->set_nil();
        
        /* if this is a serial search, we'll start at the first pattern */
        int serial_idx = 0;

        /* make sure the search string is indeed a string */
        const char *str = subj_str;
        if (str == 0 && (str = subj->get_as_string(vmg0_)) == 0)
            err_throw(VMERR_STRING_VAL_REQD);

        /* set up a utf8 pointer to the search string */
        utf8_ptr strp((char *)str + VMB_LEN);

        /* adjust the starting index */
        start_char_idx += (start_char_idx < 0
                           ? (int)strp.len(vmb_get_len(str)) : -1);

    restart_search:
        /* 
         *   remember the last search string globally, for group extraction;
         *   and forget any old group registers, since they'd point into the
         *   old string we're superseding 
         */
        G_bif_tads_globals->last_rex_str->val = *subj;
        if (have_pat)
            G_bif_tads_globals->rex_searcher->clear_group_regs();

        /* 
         *   don't allocate anything for the result yet - we'll wait to do
         *   that until we actually find a match, so that we don't allocate
         *   memory unnecessarily 
         */
        vm_obj_id_t ret_obj = VM_INVALID_OBJ;
        CVmObjString *ret_str = 0;

        /* set up a null output string pointer for now */
        utf8_ptr dstp;
        dstp.set((char *)0);
        
        /* 
         *   figure out how many bytes at the start of the string to skip
         *   before our first replacement 
         */
        utf8_ptr p;
        size_t rem;
        for (p.set((char *)str + VMB_LEN), rem = vmb_get_len(str), i = 0 ;
             i < start_char_idx && rem != 0 ; ++i, p.inc(&rem)) ;

        /* the current offset in the string is the byte skip offset */
        int skip_bytes = p.getptr() - (str + VMB_LEN);
        
        /*
         *   Start searching from the beginning of the string.  Build the
         *   result string as we go.  
         */
        int start_idx;
        for (start_idx = skip_bytes ; (size_t)start_idx < vmb_get_len(str) ; )
        {
            /* figure out where the next search starts */
            const char *last_str = str + VMB_LEN + start_idx;

            /* we haven't found a matching pattern yet */
            int best_pat = -1;

            /* do the next serial or parallel search */
            if ((flags & VMBIFTADS_REPLACE_SERIAL) != 0)
            {
                /* 
                 *   Serial search: search for one item at a time.  If we're
                 *   out of items, we're done.  
                 */
                if (serial_idx >= pat_cnt)
                    break;

                /* search for the next item */
                pats[serial_idx].search(vmg_ str, start_idx, last_str);

                /* if we didn't get a match, we're done */
                if (pats[serial_idx].match_idx < 0)
                    break;

                /* this is our replacement match */
                best_pat = serial_idx;
            }
            else
            {
                /* 
                 *   Parallel search: search for all of the items, and
                 *   replace the leftmost match.  We might still have valid
                 *   search results for some items on past iterations, but
                 *   others might have overlapped replacement text, in which
                 *   case we'll have to refresh them.  So do a search for
                 *   each item that's marked as invalid.  
                 */

                /* search for each item */
                for (i = 0 ; i < pat_cnt ; ++i)
                {
                    /* refresh this search, if it's invalid */
                    if (!pats[i].match_valid)
                        pats[i].search(vmg_ str, start_idx, last_str);

                    /* if this is the leftmost result so far, remember it */
                    if (pats[i].match_idx >= 0
                        && (best_pat < 0
                            || pats[i].match_idx < pats[best_pat].match_idx))
                        best_pat = i;
                }

                /* if we didn't find a match, we're done */
                if (best_pat < 0)
                    break;
            }

            /* 
             *   Keep the leftmost pattern we matched.  Note that we want a
             *   relative offset from last_str, so adjust from the absolute
             *   offset in the string that the pats[] entry uses. 
             */
            int match_idx = pats[best_pat].match_idx - start_idx;
            int match_len = pats[best_pat].match_len;
            const char *rplstr = pats[best_pat].rpl_str;
            vm_val_t rplfunc = pats[best_pat].rpl_func;

            /* 
             *   if we have the 'follow case' flag, note the capitalization
             *   pattern of the match 
             */
            int match_has_upper = FALSE, match_has_lower = FALSE;
            if ((flags & VMBIFTADS_REPLACE_FOLLOW_CASE) != 0)
            {
                /* scan the match text */
                for (p.set((char *)last_str + match_idx), rem = match_len ;
                     rem != 0 ; p.inc(&rem))
                {
                    /* get this character */
                    wchar_t ch = p.getch();

                    /* note whether it's upper or lower case */
                    match_has_upper |= t3_is_upper(ch);
                    match_has_lower |= t3_is_lower(ch);
                }
            }

            /* remember the group info, if we have a regex pattern */
            int group_cnt = 0;
            if (pats[best_pat].pat != 0)
            {
                /* note the group count */
                group_cnt = pats[best_pat].pat->group_cnt;

                /* copy the group registers to the global searcher registers */
                G_bif_tads_globals->rex_searcher->copy_group_regs(
                    pats[best_pat].s);
            }

            /* if we have a replacement limit, count this against the limit */
            if (limit > 0)
                --limit;
            
            /*
             *   If we haven't allocated a result string yet, do so now,
             *   since we finally know we actually need one.  
             */
            if (ret_obj == VM_INVALID_OBJ)
            {
                /*   
                 *   We don't know yet how much space we'll need for the
                 *   result, so this is only a temporary allocation.  As a
                 *   rough guess, use three times the length of the input
                 *   string.  We'll expand this as needed as we build the
                 *   string, and shrink it down to the real size when we're
                 *   done.  
                 */
                ret_obj = CVmObjString::create(vmg_ FALSE, vmb_get_len(str)*3);

                /* save it in our stack slot, for gc protection */
                G_stk->get(0)->set_obj(ret_obj);

                /* get the string object pointer */
                ret_str = (CVmObjString *)vm_objp(vmg_ ret_obj);

                /* get a pointer to the result buffer */
                dstp.set(ret_str->cons_get_buf());

                /* copy the initial part that we skipped */
                if (skip_bytes != 0)
                {
                    memcpy(dstp.getptr(), str + VMB_LEN, skip_bytes);
                    dstp.set(dstp.getptr() + skip_bytes);
                }
            }

            /* copy the part up to the start of the matched text, if any */
            if (match_idx > 0)
            {
                /* ensure space */
                dstp.set(ret_str->cons_ensure_space(
                    vmg_ dstp.getptr(), match_idx, 512));
                
                /* copy the part from the last match to this match */
                memcpy(dstp.getptr(), last_str, match_idx);
                
                /* advance the output pointer */
                dstp.set(dstp.getptr() + match_idx);
            }

            /* apply the replacement text */
            if (rplstr != 0)
            {
                /* get the length of the string */
                size_t rpllen = vmb_get_len(rplstr);

                /* we haven't substituted any alphabetic character yet */
                int alpha_rpl_cnt = 0;

                /* note whether we have a literal search string or a regex */
                int pat_is_str = (pats[best_pat].pat == 0);

                /* 
                 *   if we're replacing a string pattern (not a regex), and
                 *   there's no case following, we can plop in the whole
                 *   replacement string without examining it 
                 */
                if (pat_is_str
                    && !(flags & VMBIFTADS_REPLACE_FOLLOW_CASE))
                {
                    /* ensure space */
                    dstp.set(ret_str->cons_ensure_space(
                        vmg_ dstp.getptr(), rpllen, 512));

                    /* copy the replacement text */
                    memcpy(dstp.getptr(), rplstr + VMB_LEN, rpllen);

                    /* advance the pointer */
                    dstp.set(dstp.getptr() + rpllen);
                }
                else
                {
                    /* 
                     *   copy the replacement string into the output string,
                     *   expanding group substitutions if we have a regex
                     */
                    for (p.set((char *)rplstr + VMB_LEN),
                         rem = rpllen ; rem != 0 ; p.inc(&rem))
                    {
                        /* fetch the character */
                        wchar_t ch = p.getch();
                        
                        /* if replacing a regex, check for '%' sequences */
                        if (pat_is_str || ch != '%' || rem == 1)
                        {
                            /* 
                             *   ensure we have space for this character (one
                             *   UTF8 needs 3 bytes max) 
                             */
                            dstp.set(ret_str->cons_ensure_space(
                                vmg_ dstp.getptr(), 3, 512));
                            
                            /* 
                             *   if we're in 'follow case' mode, adjust the
                             *   case of lower-case literal letters in the
                             *   replacement text 
                             */
                            const wchar_t *u = 0;
                            if ((flags & VMBIFTADS_REPLACE_FOLLOW_CASE) != 0
                                && t3_is_lower(ch))
                            {
                                /* 
                                 *   Check the mode: if we have all
                                 *   upper-case in the match, convert the
                                 *   replacement to all caps; all lower-case
                                 *   in the match -> all lower in the
                                 *   replacement; mixed -> capitalize the
                                 *   first letter only 
                                 */
                                if (match_has_upper && !match_has_lower)
                                {
                                    /* all upper -> convert to upper */
                                    u = t3_to_upper(ch);
                                }
                                else if (match_has_upper && match_has_lower
                                         && alpha_rpl_cnt++ == 0)
                                {
                                    /* mixed case -> use title case */
                                    u = t3_to_title(ch);
                                }
                            }
                            
                            /* if we found a case conversion, apply it */
                            if (u != 0)
                            {
                                /* ensure we have room for the conversion */
                                size_t need = utf8_ptr::s_wstr_size(u);
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), need, 512));
                                
                                /* store the converted characters */
                                dstp.setwcharsz(u, need);
                            }
                            else
                            {
                                /* no case conversion - copy it as-is */
                                dstp.setch(ch);
                            }
                        }
                        else
                        {
                            /* skip the '%' and see what follows */
                            p.inc(&rem);
                            switch(p.getch())
                            {
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9':
                                {
                                    /* get the group number */
                                    int groupno =
                                        value_of_digit(p.getch()) - 1;
                                    
                                    /* if the group is valid, add its length */
                                    if (groupno < group_cnt)
                                    {
                                        /* get the register */
                                        const re_group_register *reg =
                                            G_bif_tads_globals->rex_searcher
                                            ->get_group_reg(groupno);
                                        
                                        /* if it's been set, add its text */
                                        if (reg->start_ofs != -1
                                            && reg->end_ofs != -1)
                                        {
                                            /* get the group length */
                                            size_t glen =
                                                reg->end_ofs - reg->start_ofs;
                                            
                                            /* ensure space */
                                            dstp.set(
                                                ret_str->cons_ensure_space(
                                                    vmg_ dstp.getptr(),
                                                    glen, 512));
                                            
                                            /* copy the data */
                                            memcpy(dstp.getptr(),
                                                str + VMB_LEN + reg->start_ofs,
                                                glen);
                                            
                                            /* advance past it */
                                            dstp.set(dstp.getptr() + glen);
                                        }
                                    }
                                }
                                break;
                                
                            case '*':
                                /* ensure space */
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), match_len, 512));
                                
                                /* add the entire matched string */
                                memcpy(dstp.getptr(), last_str + match_idx,
                                       match_len);
                                dstp.set(dstp.getptr() + match_len);
                                break;
                                
                            case '%':
                                /* ensure space (the '%' is just one byte) */
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), 1, 512));
                                
                                /* add a single '%' */
                                dstp.setch('%');
                                break;
                                
                            default:
                                /* 
                                 *   ensure space (we need 1 byte for the
                                 *   '%', up to 3 for the other character) 
                                 */
                                dstp.set(ret_str->cons_ensure_space(
                                    vmg_ dstp.getptr(), 4, 512));
                                
                                /* add the entire sequence unchanged */
                                dstp.setch('%');
                                dstp.setch(p.getch());
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                /* push the callback args - matchStr, matchIdx, subject */
                const int pushed_argc = 3;
                G_stk->push(orig_subj);
                G_interpreter->push_int(
                    vmg_ strp.len(start_idx + match_idx) + 1);
                G_interpreter->push_obj(vmg_ CVmObjString::create(
                    vmg_ FALSE, last_str + match_idx, match_len));

                /* adjust argc for what the callback actually wants */
                int fargc = pats[best_pat].rpl_argc;
                if (fargc < 0 || fargc > pushed_argc)
                    fargc = pushed_argc;

                /* call the callback */
                G_interpreter->call_func_ptr(vmg_ &rplfunc, fargc, &rc, 0);

                /* discard extra arguments */
                G_stk->discard(pushed_argc - fargc);
                
                /* if the return value isn't nil, copy it into the result */
                if (G_interpreter->get_r0()->typ != VM_NIL)
                {
                    /* get the string */
                    const char *r =
                        G_interpreter->get_r0()->get_as_string(vmg0_);
                    if (r == 0)
                        err_throw(VMERR_STRING_VAL_REQD);
                    
                    /* ensure space for it in the result */
                    dstp.set(ret_str->cons_ensure_space(
                        vmg_ dstp.getptr(), vmb_get_len(r), 512));
                    
                    /* store it */
                    memcpy(dstp.getptr(), r + VMB_LEN, vmb_get_len(r));
                    dstp.set(dstp.getptr() + vmb_get_len(r));
                }
            }
            
            /* advance past this matched string for the next search */
            start_idx += match_idx + match_len;
            
            /* skip to the next character if it was a zero-length match */
            if (match_len == 0 && (size_t)start_idx < vmb_get_len(str))
            {
                /* ensure space */
                dstp.set(ret_str->cons_ensure_space(
                    vmg_ dstp.getptr(), 3, 512));
                
                /* copy the character we're skipping to the output */
                p.set((char *)str + VMB_LEN + start_idx);
                dstp.setch(p.getch());
                
                /* move on to the next character */
                start_idx += 1;
            }

            /* 
             *   In a parallel search, discard any match that started before
             *   the new starting point.  Those are no longer valid because
             *   they matched original text that was wholly or partially
             *   replaced by the current iteration.  
             */
            for (i = 0 ; i < pat_cnt ; ++i)
            {
                /* invalidate this match if it's before the replacement */
                if (pats[i].match_idx >= 0 && pats[i].match_idx < start_idx)
                    pats[i].match_valid = FALSE;
            }
            
            /* if we've reached the replacement limit, stop now */
            if (limit == 0)
                break;
        }

        /* if we did any replacements on this round, finish the string */
        if (ret_obj != VM_INVALID_OBJ)
        {
            /* ensure space for the remainder after the last match */
            dstp.set(ret_str->cons_ensure_space(
                vmg_ dstp.getptr(), vmb_get_len(str) - start_idx, 512));
            
            /* add the part after the end of the matched text */
            if ((size_t)start_idx < vmb_get_len(str))
            {
                memcpy(dstp.getptr(), str + VMB_LEN + start_idx,
                       vmb_get_len(str) - start_idx);
                dstp.set(dstp.getptr() + vmb_get_len(str) - start_idx);
            }
            
            /* set the actual length of the string */
            ret_str->cons_shrink_buffer(vmg_ dstp.getptr());

            /* return the string */
            result->set_obj(ret_obj);
        }
        else
        {
            /* we didn't replace anything, so keep the original string */
            *result = *subj;
        }

        /* 
         *   If this is a serial search, and we have another item in the
         *   search list, go back and start over with the current result as
         *   the new search string.  Exception: if we've reached the
         *   replacement count limit, we're done.
         */
        if ((flags & VMBIFTADS_REPLACE_SERIAL) != 0
            && limit != 0
            && ++serial_idx < pat_cnt)
        {
            /* use the return value as the new search value */
            subj = result;
            *G_stk->get(2) = *subj;
            
            /* get the string again in case we're doing a serial iteration */
            str = subj->get_as_string(vmg0_);

            /* go back for a brand new round */
            goto restart_search;
        }

        /* discard our gc protection */
        G_stk->discard(1);
    }
    err_finally
    {
        /* if we allocated an argument array, delete it */
        if (pats != 0)
            delete [] pats;
    }
    err_end;
}

#endif /* VMFINDREP_H */
