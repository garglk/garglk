/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  regex.c - Regular Expression Parser and Recognizer for T3
Function
  Parses and recognizes regular expressions.  Adapted to C++ and UTF-8
  from the TADS 2 implementation.
Notes
  Regular expression syntax:

     abc|def    either abc or def
     (abc)      abc
     abc+       abc, abcc, abccc, ...
     abc*       ab, abc, abcc, ...
     abc?       ab or abc
     x{n}       x exactly 'n' times
     x{n,}      x at least 'n' times
     x{,n}      x 0 to 'n' times
     x{n,m}     x from 'n' to 'm' times
     .          any single character
     abc$       abc at the end of the line
     ^abc       abc at the beginning of the line
     %^abc      literally ^abc
     [abcx-z]   matches a, b, c, x, y, or z
     [^abcx-z]  matches any character except a, b, c, x, y, or z
     [^]-q]     matches any character except ], -, or q
     x+?        use *shortest* working match to x+
     x*?        use shortest match to x*
     x{}?       use shortest match to x{}

  Note that using ']' or '-' in a character range expression requires
  special ordering.  If ']' is to be used, it must be the first character
  after the '^', if present, within the brackets.  If '-' is to be used,
  it must be the first character after the '^' and/or ']', if present.

  The following special sequences match literal characters by name (the
  names insensitive to case):

     <lparen>   (
     <rparen>   )
     <lsquare>  [
     <rsquare>  ]
     <lbrace>   {
     <rbrace>   }
     <langle>   <
     <rangle>   >
     <vbar>     |
     <caret>    ^
     <period>   .
     <dot>      .
     <squote>   '
     <dquote>   "
     <star>     *
     <plus>     +
     <percent>  %
     <question> ?
     <dollar>   $
     <backslash> \
     <return>   carriage return (character code 0x000D)
     <linefeed> line feed (character code 0x000A)
     <tab>      tab (character code 0x0009)
     <nul>      null character (character code 0x0000)
     <null>     null character (character code 0x0000)

  The following special sequences match character classes (these class
  names, like the character literal names above, are insensitive to case):

     <Alpha>    any alphabetic character
     <Upper>    any upper-case alphabetic character
     <Lower>    any lower-case alphabetic character
     <Digit>    any digit
     <AlphaNum> any alphabetic character or digit
     <Space>    space character
     <Punct>    punctuation character
     <Newline>  any newline character: \n\r, \r\n, \r, \n, 0x2028

  Character classes can be combined by separating class names with the '|'
  delimiter.  In addition, a class expression can be complemented to make
  it an exclusion expression by putting a '^' after the opening bracket.

     <Upper|Digit>  any uppercase letter or any digit
     <^Alpha>       anything except an alphabetic character
     <^Upper|Digit> anything except an uppercase letter or a digit
                    (note that the exclusion applies to the ENTIRE
                    expression, so in this case both uppercase letters
                    and digits are excluded)

  In addition, character classes and named literals can be combined:

     <Upper|Star|Plus>  Any uppercase letter, or '*' or '+'

  Finally, literal characters and literal ranges resembling '[]' ranges
  can be used in angle-bracket expressions, with the limitation that each
  character or range must be separated with the '|' delimiter:

     <a|b|c>        'a' or 'b' or 'c'
     <Upper|a|b|c>  any uppercase letter, or 'a', 'b', or 'c'
     <Upper|a-m>    any uppercase letter, or lowercase letters 'a' to 'm'

  '%' is used to escape the special characters: | . ( ) * ? + ^ $ % [
  (We use '%' rather than a backslash because it's less trouble to
  enter in a TADS string -- a backslash needs to be quoted with another
  backslash, which is error-prone and hard to read.  '%' doesn't need
  any special quoting in a TADS string, which makes it a lot more
  readable.)

  In addition, '%' is used to introduce additional special sequences:

     %1         text matching first parenthesized expression
     %9         text matching ninth parenthesized experssion
     %<         matches at the beginning of a word only
     %>         matches at the end of a word only
     %w         matches any word character
     %W         matches any non-word character
     %b         matches at any word boundary (beginning or end of word)
     %B         matches except at a word boundary

  For the word matching sequences, a word is any sequence of letters and
  numbers.

  The following special sequences modify the search rules:

     <Case> - makes the search case-sensitive.  This is the default.
     <NoCase> - makes the search case-insensitive.  <Case> and <NoCase>
              are both global - the entire expression is either
              case-sensitive or case-insensitive.

     <FE> or <FirstEnd> - select the substring that ends earliest, in
                case of an ambiguous match.  <FirstBegin> is the default.
     <FB> or <FirstBegin> - select the substring that starts earliest, in
                case of an ambiguous match.  This is the default.

     <Min> - select the shortest matching substring, in case of an
                ambiguous match.  <Max> is the default.
     <Max> - select the longest matching substring, in case of an
                ambiguous match.

Modified
  09/06/98 MJRoberts  - Creation
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmregex.h"
#include "utf8.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmuni.h"
#include "vmfile.h"

/* ------------------------------------------------------------------------ */
/*
 *   Initialize.
 */
CRegexParser::CRegexParser()
{
    /* no tuple array yet */
    tuple_arr_ = 0;
    tuples_alloc_ = 0;

    /* clear states */
    next_state_ = RE_STATE_FIRST_VALID;

    /* no range buffer yet */
    range_buf_ = 0;
    range_buf_cnt_ = 0;
    range_buf_max_ = 0;

    /* by default, our expressions are case sensitive by default */
    default_case_sensitive_ = TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Reset compiler - clears states and tuples 
 */
void CRegexParser::reset()
{
    int i;
    re_tuple *t;
    
    /* delete any range tables we've allocated */
    for (i = 0, t = tuple_arr_ ; i < next_state_ ; ++i, ++t)
    {
        /* if it's a range, delete the allocated range */
        if ((t->typ == RE_RANGE || t->typ == RE_RANGE_EXCL)
            && t->info.range.char_range != 0)
        {
            t3free(t->info.range.char_range);
            t->info.range.char_range = 0;
        }

        /* clear the tuple type */
        t->typ = RE_INVALID;
    }

    /* clear states */
    next_state_ = RE_STATE_FIRST_VALID;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete
 */
CRegexParser::~CRegexParser()
{
    /* reset state (to delete most of our memory structures) */
    reset();
    
    /* if we've allocated an array, delete it */
    if (tuple_arr_ != 0)
    {
        t3free(tuple_arr_);
        tuple_arr_ = 0;
    }

    /* delete our range buffer if we have one */
    if (range_buf_ != 0)
    {
        t3free(range_buf_);
        range_buf_ = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to our range buffer 
 */
void CRegexParser::add_range_char(wchar_t ch1, wchar_t ch2)
{
    /* 
     *   if the first character in the range is null, it requires special
     *   representation 
     */
    if (ch1 == 0)
    {
        /* 
         *   A null character must be represented using the character class
         *   notation for RE_NULLCHAR, because a null lead character in a
         *   pair has the special meaning of introducing a class specifier
         *   in the second character of the pair.  So, first add the null
         *   character as a class character.
         */
        add_range_class(RE_NULLCHAR);

        /* 
         *   if the second character of the range is null, we're done, since
         *   all we needed was the null itself 
         */
        if (ch2 == 0)
            return;

        /* 
         *   we've added the null character, so simply add the rest of the
         *   range starting at character code 1 
         */
        ch1 = 1;
    }

    /* make sure we have space in the range buffer for the added entry */
    ensure_range_buf_space();

    /* add the new entry */
    range_buf_[range_buf_cnt_++] = ch1;
    range_buf_[range_buf_cnt_++] = ch2;
}

/*
 *   Add a class to a character range.  The class identifier is one of the
 *   RE_xxx recognizer types representing a class: RE_ALPHA, RE_DIGIT, and
 *   so on.  
 */
void CRegexParser::add_range_class(re_recog_type cl)
{
    /* make sure we have space in the range buffer */
    ensure_range_buf_space();

    /* 
     *   add the new entry - the first character of the pair is a null
     *   character to indicate that the entry represents a class, and the
     *   second of the pair is the type code 
     */
    range_buf_[range_buf_cnt_++] = 0;
    range_buf_[range_buf_cnt_++] = (wchar_t)cl;
}

/*
 *   Check for space in the range buffer for a new range, expanding the
 *   buffer if necessary.
 */
void CRegexParser::ensure_range_buf_space()
{
    /* make sure we have room for another entry */
    if (range_buf_ == 0)
    {
        /* 
         *   allocate an initial size - it must be even, since we always
         *   consume elements in pairs 
         */
        range_buf_max_ = 128;
        range_buf_ = (wchar_t *)t3malloc(range_buf_max_ * sizeof(wchar_t));

        /* no entries yet */
        range_buf_cnt_ = 0;
    }
    else if (range_buf_cnt_ == range_buf_max_)
    {
        /* 
         *   reallocate the buffer at a larger size (the size must always be
         *   even, since we always add to the buffer in pairs) 
         */
        range_buf_max_ += 128;
        range_buf_ = (wchar_t *)t3realloc(range_buf_,
                                          range_buf_max_ * sizeof(wchar_t));
    }

    /* if the range buffer is null, throw an error */
    if (range_buf_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);
}


/* ------------------------------------------------------------------------ */
/*
 *   Allocate a new state ID 
 */
re_state_id CRegexParser::alloc_state()
{
    /*
     *   If we don't have enough room for another state, expand the array 
     */
    if (next_state_ >= tuples_alloc_)
    {
        int new_alloc;
        
        /* bump the size by a bit */
        new_alloc = tuples_alloc_ + 100;
        
        /* allocate or expand the array */
        if (tuples_alloc_ == 0)
        {
            /* allocate the initial memory block */
            tuple_arr_ = (re_tuple *)t3malloc(new_alloc * sizeof(re_tuple));
        }
        else
        {
            /* re-allocate the existing memory block */
            tuple_arr_ = (re_tuple *)t3realloc(tuple_arr_,
                                               new_alloc * sizeof(re_tuple));
        }

        /* remember the new allocation size */
        tuples_alloc_ = new_alloc;
    }

    /* initialize the next state */
    tuple_arr_[next_state_].next_state_1 = RE_STATE_INVALID;
    tuple_arr_[next_state_].next_state_2 = RE_STATE_INVALID;
    tuple_arr_[next_state_].info.ch = RE_EPSILON;
    tuple_arr_[next_state_].flags = 0;
    tuple_arr_[next_state_].typ = RE_INVALID;

    /* return the new state's ID */
    return next_state_++;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set a transition from a state to a given destination state.  
 */
void CRegexParser::set_trans(re_state_id id, re_state_id dest_id,
                             re_recog_type typ, wchar_t ch)
{
    re_tuple *tuple;

    /* ignore invalid states */
    if (id == RE_STATE_INVALID)
        return;
    
    /* 
     *   get the tuple containing the transitions for this state ID - the
     *   state ID is the index of the state's transition tuple in the
     *   array 
     */
    tuple = &tuple_arr_[id];

    /*
     *   If the first state pointer hasn't been set yet, set it to the new
     *   destination.  Otherwise, set the second state pointer.
     *   
     *   Only set the character on setting the first state.  When setting
     *   the second state, we must assume that the character for the state
     *   has already been set, since any given state can have only one
     *   character setting.  
     */
    if (tuple->next_state_1 == RE_STATE_INVALID)
    {
        /* set the transition type and character ID */
        tuple->typ = typ;
        tuple->info.ch = ch;

        /* set the first transition */
        tuple->next_state_1 = dest_id;
    }
    else
    {
        /* 
         *   set only the second transition state - don't set the character
         *   ID or transition type 
         */
        tuple->next_state_2 = dest_id;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize a new machine, giving it an initial and final state 
 */
void CRegexParser::init_machine(re_machine *machine)
{
    machine->init = alloc_state();
    machine->final = alloc_state();
}

/*
 *   Build a character recognizer
 */
void CRegexParser::build_char(re_machine *machine, wchar_t ch)
{
    /* initialize our new machine */
    init_machine(machine);

    /* allocate a transition tuple for the new state */
    set_trans(machine->init, machine->final, RE_LITERAL, ch);
}

/*
 *   Build a special type recognizer 
 */
void CRegexParser::build_special(re_machine *machine, re_recog_type typ,
                                 wchar_t ch)
{
    /* initialize our new machine */
    init_machine(machine);

    /* allocate a transition tuple for the new state */
    set_trans(machine->init, machine->final, typ, ch);
}

/*
 *   Build a character range recognizer.
 */
void CRegexParser::build_char_range(re_machine *machine,
                                    int exclusion)
{
    wchar_t *range_copy;
    
    /* initialize our new machine */
    init_machine(machine);

    /* allocate a transition table for the new state */
    set_trans(machine->init, machine->final,
              (exclusion ? RE_RANGE_EXCL : RE_RANGE), 0);

    /* allocate a copy of the range vector */
    range_copy = (wchar_t *)t3malloc(range_buf_cnt_ * sizeof(wchar_t));

    /* copy the caller's range */
    memcpy(range_copy, range_buf_, range_buf_cnt_ * sizeof(wchar_t));

    /* store it in the tuple */
    tuple_arr_[machine->init].info.range.char_range = range_copy;
    tuple_arr_[machine->init].info.range.char_range_cnt = range_buf_cnt_;
}


/*
 *   Build a group recognizer.  This is almost the same as a character
 *   recognizer, but matches a previous group rather than a literal
 *   character. 
 */
void CRegexParser::build_group_matcher(re_machine *machine, int group_num)
{
    /* initialize our new machine */
    init_machine(machine);

    /* 
     *   Allocate a transition tuple for the new state for the group ID.
     *   Rather than storing a literal character code, store the group ID
     *   in the character code slot.  
     */
    set_trans(machine->init, machine->final,
              RE_GROUP_MATCH, (wchar_t)group_num);
}


/*
 *   Build a concatenation recognizer 
 */
void CRegexParser::build_concat(re_machine *new_machine,
                                   re_machine *lhs, re_machine *rhs)
{
    /* initialize the new machine */
    init_machine(new_machine);

    /* 
     *   set up an epsilon transition from the new machine's initial state
     *   to the first submachine's initial state 
     */
    set_trans(new_machine->init, lhs->init, RE_EPSILON, 0);

    /*
     *   Set up an epsilon transition from the first submachine's final
     *   state to the second submachine's initial state 
     */
    set_trans(lhs->final, rhs->init, RE_EPSILON, 0);

    /*
     *   Set up an epsilon transition from the second submachine's final
     *   state to our new machine's final state 
     */
    set_trans(rhs->final, new_machine->final, RE_EPSILON, 0);
}

/*
 *   Build a group machine.  sub_machine contains the machine that
 *   expresses the group's contents; we'll fill in new_machine with a
 *   newly-created machine that encloses and marks the group.  
 */
void CRegexParser::build_group(re_machine *new_machine,
                               re_machine *sub_machine, int group_id)
{
    /* initialize the container machine */
    init_machine(new_machine);

    /* 
     *   Set up a group-entry transition from the new machine's initial
     *   state into the initial state of the group, and a group-exit
     *   transition from the group's final state into the container's final
     *   state.  For both transitions, store the group ID in the character
     *   field of the transition, to identify which group is affected.  
     */
    set_trans(new_machine->init, sub_machine->init,
              RE_GROUP_ENTER, (wchar_t)group_id);
    set_trans(sub_machine->final, new_machine->final,
              RE_GROUP_EXIT, (wchar_t)group_id);
}

/*
 *   Build an assertion machine 
 */
void CRegexParser::build_assert(re_machine *new_machine,
                                re_machine *sub_machine, int is_negative)
{
    /* initialize the container machine */
    init_machine(new_machine);

    /* allocate a transition tuple for the new state */
    set_trans(new_machine->init, new_machine->final,
              is_negative ? RE_ASSERT_NEG : RE_ASSERT_POS, 0);

    /* set the sub-state */
    tuple_arr_[new_machine->init].info.sub.init = sub_machine->init;
    tuple_arr_[new_machine->init].info.sub.final = sub_machine->final;
}


/*
 *   Build an alternation recognizer 
 */
void CRegexParser::build_alter(re_machine *new_machine,
                               re_machine *lhs, re_machine *rhs)
{
    /* initialize the new machine */
    init_machine(new_machine);

    /*
     *   Set up an epsilon transition from our new machine's initial state
     *   to the initial state of each submachine 
     */
    set_trans(new_machine->init, lhs->init, RE_EPSILON, 0);
    set_trans(new_machine->init, rhs->init, RE_EPSILON, 0);

    /*
     *   Set up an epsilon transition from the final state of each
     *   submachine to our final state 
     */
    set_trans(lhs->final, new_machine->final, RE_EPSILON, 0);
    set_trans(rhs->final, new_machine->final, RE_EPSILON, 0);
}

/*
 *   Build a closure recognizer
 */
void CRegexParser::build_closure(re_machine *new_machine, re_machine *sub,
                                 wchar_t specifier, int shortest)
{
    /* initialize the new machine */
    init_machine(new_machine);

    /* 
     *   Set up an epsilon transition from our initial state to the
     *   submachine's initial state.  However, if we're in shortest mode,
     *   wait to do this until after we've generated the rest of the
     *   machine, and instead set up the transition from the submachine's
     *   final state to our final state.  The order is important because
     *   we will favor the first branch taken when we find two matches of
     *   equal total length; hence we want to make the branch that will
     *   give us either the longest match or shortest match for this
     *   closure first, depending on which way we want to go.  
     */
    if (!shortest)
        set_trans(new_machine->init, sub->init, RE_EPSILON, 0);
    else
        set_trans(sub->final, new_machine->final, RE_EPSILON, 0);

    /*
     *   If this is an unbounded closure ('*' or '+', but not '?'), set up
     *   the loop transition that takes us from the new machine's final
     *   state back to its initial state.  We don't do this on the
     *   zero-or-one closure, because we can only match the expression
     *   once.  
     */
    if (specifier != '?')
    {
        /* set the transition */
        set_trans(sub->final, sub->init, RE_EPSILON, 0);

        /* if we have a 'shortest' modifier, flag it in this branch */
        if (shortest)
            tuple_arr_[sub->final].flags |= RE_STATE_SHORTEST;
    }

    /*
     *   If this is a zero-or-one closure or a zero-or-more closure, set
     *   up an epsilon transition from our initial state to our final
     *   state, since we can skip the entire subexpression.  We don't do
     *   this on the one-or-more closure, because we can't skip the
     *   subexpression in this case.  
     */
    if (specifier != '+')
    {
        /* set the transition */
        set_trans(new_machine->init, new_machine->final, RE_EPSILON, 0);

        /* if we have a 'shortest' modifier, flag it in this branch */
        if (shortest)
            tuple_arr_[new_machine->init].flags |= RE_STATE_SHORTEST;
    }

    /* 
     *   Set up a transition from the submachine's final state to our
     *   final state.  We waited until here to ensure proper ordering for
     *   longest-preferred.  If we're in shortest-preferred mode, though,
     *   set up the initial transition to the submachine instead.  
     */
    if (!shortest)
        set_trans(sub->final, new_machine->final, RE_EPSILON, 0);
    else
        set_trans(new_machine->init, sub->init, RE_EPSILON, 0);
}

void CRegexParser::build_interval(re_machine *new_machine,
                                  re_machine *sub, int min_val, int max_val,
                                  int var_id, int shortest)
{
    re_machine inner_machine;
    
    /* initialize the outer (new) machine */
    init_machine(new_machine);

    /* initialize the inner machine */
    init_machine(&inner_machine);

    /* 
     *   Set the loop transition into the submachine, and set the other to
     *   bypass the submachine.  If we have a 'shortest' modifier, take
     *   the bypass branch first, otherwise take the enter branch first. 
     */
    if (shortest)
    {
        set_trans(inner_machine.init, new_machine->final, RE_LOOP_BRANCH, 0);
        set_trans(inner_machine.init, sub->init, RE_LOOP_BRANCH, 0);
    }
    else
    {
        set_trans(inner_machine.init, sub->init, RE_LOOP_BRANCH, 0);
        set_trans(inner_machine.init, new_machine->final, RE_LOOP_BRANCH, 0);
    }

    /* 
     *   set the final transition of the submachine to come back to the
     *   loop branch point 
     */
    set_trans(sub->final, inner_machine.init, RE_EPSILON, 0);

    /* 
     *   set the outer machine to transition into the inner machine and
     *   zero the loop variable 
     */
    set_trans(new_machine->init, inner_machine.init, RE_ZERO_VAR, 0);

    /* set the variable ID in the ZERO_VAR node */
    tuple_arr_[new_machine->init].info.loop.loop_var = var_id;

    /* set up the loop parameters in the loop node */
    tuple_arr_[inner_machine.init].info.loop.loop_var = var_id;
    tuple_arr_[inner_machine.init].info.loop.loop_min = min_val;
    tuple_arr_[inner_machine.init].info.loop.loop_max = max_val;

    /* 
     *   if there's a 'shortest' modifier, note it in the loop node, so
     *   that we can take the bypass branch first whenever possible 
     */
    if (shortest)
        tuple_arr_[inner_machine.init].flags |= RE_STATE_SHORTEST;
}

/*
 *   Build a null machine 
 */
void CRegexParser::build_null_machine(re_machine *machine)
{
    machine->init = machine->final = RE_STATE_INVALID;
}

/* ------------------------------------------------------------------------ */
/*
 *   Determine if a machine is null 
 */
int CRegexParser::is_machine_null(re_machine *machine)
{
    return (machine->init == RE_STATE_INVALID);
}


/* ------------------------------------------------------------------------ */
/*
 *   Concatenate the second machine onto the first machine, replacing the
 *   first machine with the resulting machine.  If the first machine is a
 *   null machine (created with re_build_null_machine), we'll simply copy
 *   the second machine into the first. 
 */
void CRegexParser::concat_onto(re_machine *dest, re_machine *rhs)
{
    /* check for a null destination machine */
    if (is_machine_null(dest))
    {
        /* 
         *   the first machine is null - simply copy the second machine
         *   onto the first unchanged 
         */
        *dest = *rhs;
    }
    else
    {
        re_machine new_machine;
        
        /* build the concatenated machine */
        build_concat(&new_machine, dest, rhs);

        /* copy the concatenated machine onto the first machine */
        *dest = new_machine;
    }
}

/*
 *   Alternate the second machine onto the first machine, replacing the
 *   first machine with the resulting machine.  If the first machine is a
 *   null machine, this simply replaces the first machine with the second
 *   machine.  If the second machine is null, this simply leaves the first
 *   machine unchanged. 
 */
void CRegexParser::alternate_onto(re_machine *dest, re_machine *rhs)
{
    /* check to see if the first machine is null */
    if (is_machine_null(dest))
    {
        /* 
         *   the first machine is null - simply copy the second machine
         *   onto the first 
         */
        *dest = *rhs;
    }
    else
    {
        /* 
         *   if the second machine is null, don't do anything; otherwise,
         *   build the alternation 
         */
        if (!is_machine_null(rhs))
        {
            re_machine new_machine;
            
            /* build the alternation */
            build_alter(&new_machine, dest, rhs);

            /* replace the first machine with the alternation */
            *dest = new_machine;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Compile an expression 
 */
re_status_t CRegexParser::compile(const char *expr_str, size_t exprlen,
                                  re_compiled_pattern_base *pat)
{
    re_machine cur_machine;
    re_machine alter_machine;
    re_machine new_machine;
    int group_stack_level;
    struct
    {
        re_machine old_cur;
        re_machine old_alter;
        int group_id;
        int capturing;
        int neg_assertion;
        int pos_assertion;
    } group_stack[RE_GROUP_NESTING_MAX];
    utf8_ptr expr;

    /* reset everything */
    reset();

    /* we have no groups yet */
    pat->group_cnt = 0;

    /* we have no looping variables yet */
    pat->loop_var_cnt = 0;

    /* 
     *   set the default match modes - maximum, first-beginning,
     *   case-sensitive 
     */
    pat->longest_match = TRUE;
    pat->first_begin = TRUE;
    pat->case_sensitive = default_case_sensitive_;

    /* start out with no current machine and no alternate machine */
    build_null_machine(&cur_machine);
    build_null_machine(&alter_machine);

    /* nothing on the stack yet */
    group_stack_level = 0;

    /* loop until we run out of expression to parse */
    for (expr.set((char *)expr_str) ; exprlen != 0 ; expr.inc(&exprlen))
    {
        switch(expr.getch())
        {
        case '^':
            /*
             *   beginning of line - if we're not at the beginning of the
             *   current expression (i.e., we already have some
             *   concatentations accumulated), treat it as an ordinary
             *   character 
             */
            if (!is_machine_null(&cur_machine))
                goto normal_char;

            /* build a new start-of-text recognizer */
            build_special(&new_machine, RE_TEXT_BEGIN, 0);

            /* 
             *   concatenate it onto the string - note that this can't
             *   have any postfix operators 
             */
            concat_onto(&cur_machine, &new_machine);
            break;

        case '$':
            /*
             *   End of line specifier - if there's anything left after
             *   the '$' other than a close parens or alternation
             *   specifier, treat it as a normal character 
             */
            if (exprlen > 1
                && (expr.getch_at(1) != ')' && expr.getch_at(1) != '|'))
                goto normal_char;

            /* build a new end-of-text recognizer */
            build_special(&new_machine, RE_TEXT_END, 0);

            /* 
             *   concatenate it onto the string - note that this can't
             *   have any postfix operators 
             */
            concat_onto(&cur_machine, &new_machine);
            break;
            
        case '(':
            {
                int capturing;
                int pos_assertion;
                int neg_assertion;

                /* presume it's a capturing group */
                capturing = TRUE;

                /* presume it's not an assertion */
                pos_assertion = FALSE;
                neg_assertion = FALSE;
                
                /* 
                 *   Add a nesting level.  Push the current machine and
                 *   alternate machines onto the group stack, and clear
                 *   everything out for the new group.  
                 */
                if (group_stack_level > RE_GROUP_NESTING_MAX)
                {
                    /* we cannot proceed - return an error */
                    return RE_STATUS_GROUP_NESTING_TOO_DEEP;
                }
                
                /* save the current state on the stack */
                group_stack[group_stack_level].old_cur = cur_machine;
                group_stack[group_stack_level].old_alter = alter_machine;
                
                /* 
                 *   Assign the group a group ID - groups are numbered in
                 *   order of their opening (left) parentheses, so we want
                 *   to assign a group number now.  We won't actually need
                 *   to know the group number until we get to the matching
                 *   close paren, but we need to assign it now, so store
                 *   it in the group stack.  
                 */
                group_stack[group_stack_level].group_id = pat->group_cnt;
                
                /* check for special group flags */
                if (exprlen > 2 && expr.getch_at(1) == '?')
                {
                    switch(expr.getch_at(2))
                    {
                    case ':':
                        /* it's a non-capturing group */
                        capturing = FALSE;

                        /* skip two extra characters for the '?:' part */
                        expr.inc(&exprlen);
                        expr.inc(&exprlen);
                        break;
                        
                    case '=':
                        /* it's a positive assertion group */
                        pos_assertion = TRUE;

                        /* assertions don't capture */
                        capturing = FALSE;

                        /* skip two extra characters for the '?=' part */
                        expr.inc(&exprlen);
                        expr.inc(&exprlen);
                        break;
                        
                    case '!':
                        /* it's a negative assertion group */
                        neg_assertion = TRUE;

                        /* assertions don't capture */
                        capturing = FALSE;

                        /* skip two extra characters for the '?!' part */
                        expr.inc(&exprlen);
                        expr.inc(&exprlen);
                        break;
                        
                    default:
                        /* it's not a recognized sequence - ignore it */
                        break;
                    }
                }

                /* remember if the group is capturing */
                group_stack[group_stack_level].capturing = capturing;

                /* remember if it's an assertion of some kind */
                group_stack[group_stack_level].pos_assertion = pos_assertion;
                group_stack[group_stack_level].neg_assertion = neg_assertion;
                
                /* consume the group number if it's a capturing group */
                if (capturing)
                    ++(pat->group_cnt);
                
                /* push the level */
                ++group_stack_level;
                
                /* start the new group with empty machines */
                build_null_machine(&cur_machine);
                build_null_machine(&alter_machine);
            }
            break;

        case ')':
        do_close_paren:
            /* if there's nothing on the stack, ignore this */
            if (group_stack_level == 0)
                break;

            /* take a level off the stack */
            --group_stack_level;

            /* 
             *   Remove a nesting level.  If we have a pending alternate
             *   expression, build the alternation expression.  This will
             *   leave the entire group expression in alter_machine,
             *   regardless of whether an alternation was in progress or
             *   not.  
             */
            alternate_onto(&alter_machine, &cur_machine);

            /*
             *   Create a group machine that encloses the group and marks
             *   it with a group number.  We assigned the group number
             *   when we parsed the open paren, so read that group number
             *   from the stack.
             *   
             *   Note that this will leave 'new_machine' with the entire
             *   group machine.
             *   
             *   If this is a non-capturing group, don't bother building
             *   the new machine - just copy the current alternation
             *   machine onto the new machine.  
             */
            if (group_stack[group_stack_level].capturing)
            {
                /* it's a regular capturing group - add the group machine */
                build_group(&new_machine, &alter_machine,
                            group_stack[group_stack_level].group_id);
            }
            else if (group_stack[group_stack_level].pos_assertion
                     || group_stack[group_stack_level].neg_assertion)
            {
                /* it's an assertion - build the assertion group */
                build_assert(&new_machine, &alter_machine,
                             group_stack[group_stack_level].neg_assertion);
            }
            else
            {
                /* it's a non-capturing group - just add the group tree */
                new_machine = alter_machine;
            }

            /*
             *   Pop the stack - restore the alternation and current
             *   machines that were in progress before the group started. 
             */
            cur_machine = group_stack[group_stack_level].old_cur;
            alter_machine = group_stack[group_stack_level].old_alter;

            /* 
             *   If we just did an assertion, ignore any closure postfix
             *   operators.  Since an assertion doesn't consume any input
             *   text, applying a closure would create an infinite loop.  
             */
            if (group_stack[group_stack_level].pos_assertion
                || group_stack[group_stack_level].neg_assertion)
                goto skip_closures;

            /*
             *   Check the group expression (in new_machine) for postfix
             *   expressions 
             */
            goto apply_postfix;

        case '|':
            /* 
             *   Start a new alternation.  This ends the current
             *   alternation; if we have a previous pending alternate,
             *   build an alternation machine out of the previous
             *   alternate and the current machine and move that to the
             *   alternate; otherwise, simply move the current machine to
             *   the pending alternate. 
             */
            alternate_onto(&alter_machine, &cur_machine);

            /* 
             *   the alternation starts out with a blank slate, so null
             *   out the current machine 
             */
            build_null_machine(&cur_machine);
            break;

        case '<':
            /* check for our various special directives */
            if ((exprlen >= 4 && memicmp(expr.getptr(), "<FE>", 4) == 0)
                || (exprlen >= 10
                    && memicmp(expr.getptr(), "<FirstEnd>", 10) == 0))
            {
                /* turn off first-begin mode */
                pat->first_begin = FALSE;
            }
            else if ((exprlen >= 4 && memicmp(expr.getptr(), "<FB>", 4) == 0)
                     || (exprlen >= 12
                         && memicmp(expr.getptr(), "<FirstBegin>", 12) == 0))
            {
                /* turn on first-begin mode */
                pat->first_begin = TRUE;
            }
            else if (exprlen >= 5 && memicmp(expr.getptr(), "<Max>", 5) == 0)
            {
                /* turn on longest-match mode */
                pat->longest_match = TRUE;
            }
            else if (exprlen >= 5 && memicmp(expr.getptr(), "<Min>", 5) == 0)
            {
                /* turn off longest-match mode */
                pat->longest_match = FALSE;
            }
            else if (exprlen >= 6 && memicmp(expr.getptr(), "<Case>", 6) == 0)
            {
                /* turn on case sensitivity */
                pat->case_sensitive = TRUE;
            }
            else if (exprlen >= 8
                     && memicmp(expr.getptr(), "<NoCase>", 8) == 0)
            {
                /* turn off case sensitivity */
                pat->case_sensitive = FALSE;
            }
            else
            {
                /*
                 *   It's nothing else we recognize, so it must be a
                 *   character class or class range expression, which
                 *   consists of one or more classes, single characters, or
                 *   character ranges separated by '|' delimiters.  
                 */
                if (compile_char_class_expr(&expr, &exprlen, &new_machine))
                {
                    /* success - look for postfix operators */
                    goto apply_postfix;
                }
                else
                {
                    /* 
                     *   failure - treat the whole thing as ordinary
                     *   characters 
                     */
                    goto normal_char;
                }
            }

            /* skip everything up to the closing ">" */
            for ( ; exprlen > 0 && expr.getch() != '>' ; expr.inc(&exprlen)) ;
            break;

        case '%':
            /* 
             *   quoted character - skip the quote mark and see what we
             *   have 
             */
            expr.inc(&exprlen);

            /* check to see if we're at the end of the expression */
            if (exprlen == 0)
            {
                /* 
                 *   end of the string - ignore it, but undo the extra
                 *   increment of the expression index so that we exit the
                 *   enclosing loop properly 
                 */
                expr.dec(&exprlen);
                break;
            }

            /* see what we have */
            switch(expr.getch())
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
                /* group match - build a new literal group recognizer */
                build_group_matcher(&new_machine,
                                    value_of_digit(expr.getch()) - 1);

                /* apply any postfix expression to the group recognizer */
                goto apply_postfix;

            case '<':
                /* build a beginning-of-word recognizer */
                build_special(&new_machine, RE_WORD_BEGIN, 0);

                /* it can't be postfixed - just concatenate it */
                concat_onto(&cur_machine, &new_machine);
                break;

            case '>':
                /* build an end-of-word recognizer */
                build_special(&new_machine, RE_WORD_END, 0);

                /* it can't be postfixed - just concatenate it */
                concat_onto(&cur_machine, &new_machine);
                break;

            case 'w':
                /* word character */
                build_special(&new_machine, RE_WORD_CHAR, 0);
                goto apply_postfix;

            case 'W':
                /* non-word character */
                build_special(&new_machine, RE_NON_WORD_CHAR, 0);
                goto apply_postfix;

            case 'b':
                /* word boundary */
                build_special(&new_machine, RE_WORD_BOUNDARY, 0);

                /* it can't be postfixed */
                concat_onto(&cur_machine, &new_machine);
                break;

            case 'B':
                /* not a word boundary */
                build_special(&new_machine, RE_NON_WORD_BOUNDARY, 0);

                /* it can't be postfixed */
                concat_onto(&cur_machine, &new_machine);
                break;

            default:
                /* build a new literal character recognizer */
                build_char(&new_machine, expr.getch());

                /* apply any postfix expression to the character */
                goto apply_postfix;
            }
            break;

        case '.':
            /* 
             *   wildcard character - build a single character recognizer
             *   for the special wildcard symbol, then go check it for a
             *   postfix operator 
             */
            build_special(&new_machine, RE_WILDCARD, 0);
            goto apply_postfix;
            break;

        case '[':
            /* range expression */
            {
                int is_exclusive = FALSE;

                /* we have no entries yet */
                range_buf_cnt_ = 0;

                /* first, skip the open bracket */
                expr.inc(&exprlen);

                /* check to see if starts with the exclusion character */
                if (exprlen != 0 && expr.getch() == '^')
                {
                    /* skip the exclusion specifier */
                    expr.inc(&exprlen);

                    /* note it */
                    is_exclusive = TRUE;
                }

                /* 
                 *   if the first character is a ']', include it in the
                 *   range 
                 */
                if (exprlen != 0 && expr.getch() == ']')
                {
                    add_range_char(']');
                    expr.inc(&exprlen);
                }

                /*
                 *   if the next character is a '-', include it in the
                 *   range 
                 */
                if (exprlen != 0 && expr.getch() == '-')
                {
                    add_range_char('-');
                    expr.inc(&exprlen);
                }

                /* scan the character set */
                while (exprlen != 0 && expr.getch() != ']')
                {
                    wchar_t ch;
                    
                    /* note this character */
                    ch = expr.getch();

                    /* skip this character of the expression */
                    expr.inc(&exprlen);

                    /* check for a range */
                    if (exprlen != 0 && expr.getch() == '-')
                    {
                        wchar_t ch2;
                        
                        /* skip the '-' */
                        expr.inc(&exprlen);
                        if (exprlen != 0)
                        {
                            /* get the other end of the range */
                            ch2 = expr.getch();

                            /* skip the second character */
                            expr.inc(&exprlen);

                            /* if the range is reversed, swap it */
                            if (ch > ch2)
                            {
                                wchar_t tmp = ch;
                                ch = ch2;
                                ch2 = tmp;
                            }

                            /* add the range */
                            add_range_char(ch, ch2);
                        }
                    }
                    else
                    {
                        /* no range - add the one-character range */
                        add_range_char(ch);
                    }
                }

                /* create a character range machine */
                build_char_range(&new_machine, is_exclusive);

                /* apply any postfix operator */
                goto apply_postfix;
            }            
            break;

        default:
        normal_char:
            /* 
             *   it's an ordinary character - build a single character
             *   recognizer machine, and then concatenate it onto any
             *   existing machine 
             */
            build_char(&new_machine, expr.getch());

        apply_postfix:
            /*
             *   Check for a postfix operator, and apply it to the machine
             *   in 'new_machine' if present.  In any case, concatenate
             *   the 'new_machine' (modified by a postix operator or not)
             *   to the current machien.  
             */
            if (exprlen > 1)
            {
                switch(expr.getch_at(1))
                {
                case '*':
                case '+':
                case '?':
                    /*
                     *   We have a postfix closure operator.  Build a new
                     *   closure machine out of 'new_machine'.  
                     */
                    {
                        re_machine closure_machine;
                        int shortest;
                        
                        /* move onto the closure operator */
                        expr.inc(&exprlen);

                        /* 
                         *   if the next character is '?', it's a modifier
                         *   that indicates that we are to use the
                         *   shortest match - note it if so 
                         */
                        shortest = (exprlen > 1 && expr.getch_at(1) == '?');

                        /* build the closure machine */
                        build_closure(&closure_machine,
                                      &new_machine, expr.getch(), shortest);
                        
                        /* replace the original machine with the closure */
                        new_machine = closure_machine;
                        
                        /* 
                         *   skip any redundant closure symbols, keeping
                         *   only the first one we saw 
                         */
                    skip_closures:
                        while (exprlen > 1)
                        {
                            /* check for a simple closure suffix */
                            if (expr.getch_at(1) == '?'
                                || expr.getch_at(1) == '+'
                                || expr.getch_at(1) == '*')
                            {
                                /* skip it and keep looping */
                                expr.inc(&exprlen);
                                continue;
                            }

                            /* check for an interval */
                            if (expr.getch_at(1) == '{')
                            {
                                /* skip until we find the matching '}' */
                                while (exprlen > 1 && expr.getch_at(1) != '}')
                                    expr.inc(&exprlen);

                                /* go back for anything that follows */
                                continue;
                            }

                            /* if it's anything else, we're done discarding */
                            break;
                        }
                    }
                    break;

                case '{':
                    /* interval specifier */
                    {
                        int min_val;
                        int max_val;
                        re_machine interval_machine;
                        int shortest;
                        int var_id;

                        /* 
                         *   loops can never overlap, but can be nested;
                         *   so the only thing we have to worry about in
                         *   assigning a loop variable is the group
                         *   nesting depth 
                         */
                        var_id = group_stack_level;

                        /* note the highest variable ID we've seen */
                        if (var_id >= pat->loop_var_cnt)
                            pat->loop_var_cnt = var_id + 1;

                        /* presume neither min nor max will be specified */
                        min_val = -1;
                        max_val = -1;
                        
                        /* skip the current character and the '{' */
                        expr.inc(&exprlen);
                        expr.inc(&exprlen);
                        
                        /* parse the minimum count, if provided */
                        min_val = parse_int(&expr, &exprlen);

                        /* if there's a comma, parse the maximum value */
                        if (exprlen >= 1 && expr.getch() == ',')
                        {
                            /* skip the ',' and parse the number */
                            expr.inc(&exprlen);
                            max_val = parse_int(&expr, &exprlen);
                        }
                        else
                        {
                            /* 
                             *   there's no other value, so this is a
                             *   simple loop with the same value for min
                             *   and max 
                             */
                            max_val = min_val;
                        }

                        /* 
                         *   if we're not looking at a '}', skip
                         *   characters until we are 
                         */
                        while (exprlen != 0 && expr.getch() != '}')
                            expr.inc(&exprlen);

                        /* 
                         *   if there's a '?' following, it's a 'shortest'
                         *   modifier - note it 
                         */
                        shortest = FALSE;
                        if (exprlen > 1 && expr.getch_at(1) == '?')
                        {
                            /* note the modifier */
                            shortest = TRUE;

                            /* skip another character for the modifier */
                            expr.inc(&exprlen);
                        }

                        /* set up an interval node */
                        build_interval(&interval_machine, &new_machine,
                                       min_val, max_val, var_id, shortest);

                        /* replace the original machine with the interval */
                        new_machine = interval_machine;

                        /* skip any closure modifiers that follow */
                        goto skip_closures;
                    }
                    break;
                    
                default:
                    /* no postfix operator */
                    break;
                }
            }

            /*
             *   Concatenate the new machine onto the current machine
             *   under construction.  
             */
            concat_onto(&cur_machine, &new_machine);
            break;
        }

        /* if we've run down the expression string, go no further */
        if (exprlen == 0)
            break;
    }

    /* if there are any open parens outstanding, close them */
    if (group_stack_level != 0)
        goto do_close_paren;

    /* complete any pending alternation */
    alternate_onto(&alter_machine, &cur_machine);

    /* check for and break any infinite loops */
    break_loops(&alter_machine);

    /* remove meaningless branch-to-branch transitions */
    remove_branch_to_branch(&alter_machine);

    /* store the results in the caller's base pattern description */
    pat->machine = alter_machine;
    pat->tuple_cnt = next_state_;

    /* limit the group count to the maximum */
    if (pat->group_cnt > RE_GROUP_REG_CNT)
        pat->group_cnt = RE_GROUP_REG_CNT;

    /* limit the variable count to the maximum */
    if (pat->loop_var_cnt > RE_LOOP_VARS_MAX)
        pat->loop_var_cnt = RE_LOOP_VARS_MAX;

    /* no errors encountered */
    return RE_STATUS_SUCCESS;
}


/* 
 *   Parse a character class or class range expresion.  Returns true on
 *   success, in which case we will have built a class range expression in
 *   new_machine and updated expr and exprlen to skip the expression.
 *   Returns false on syntax error or other failure, in which case expr and
 *   exprlen will be unchanged.  
 */
int CRegexParser::compile_char_class_expr(utf8_ptr *expr, size_t *exprlen,
                                          re_machine *result_machine)
{
    utf8_ptr p;
    size_t rem;
    int is_exclusive;
    
    /* start at the character after the '<' */
    p = *expr;
    rem = *exprlen;
    p.inc(&rem);

    /* presume it won't be exclusive */
    is_exclusive = FALSE;

    /* check for an exclusion flag */
    if (rem != 0 && p.getch() == '^')
    {
        /* note the exclusion */
        is_exclusive = TRUE;
        
        /* skip the exclusion prefix */
        p.inc(&rem);
    }
    
    /* clear out the range builder buffer */
    range_buf_cnt_ = 0;
    
    /* 
     *   keep going until we find the closing delimiter (or run out of
     *   expression string) 
     */
    for (;;)
    {
        utf8_ptr start;
        wchar_t delim;
        size_t curcharlen;
        size_t curbytelen;
        
        /* remember where the current part starts */
        start = p;
        
        /* scan for the '>' or '|' */
        for (curcharlen = 0 ; rem != 0 ; p.inc(&rem), ++curcharlen)
        {
            /* get the next character */
            delim = p.getch();
            
            /* if it's '>' or '|', we're done */
            if (delim == '>' || delim == '|')
                break;
        }
        
        /* 
         *   If we reached the end of the expression without finding a
         *   closing delimiter, the expression is invalid - treat the whole
         *   thing (starting with the '<') as ordinary characters.  
         */
        if (rem == 0)
            return FALSE;
        
        /* get the length of this part */
        curbytelen = (size_t)(p.getptr() - start.getptr());
        
        /* 
         *   See what we have.  If we have a single character, it's a
         *   literal.  If we have a character, a hyphen, and another
         *   character, it's a literal range.  Otherwise, it must be one of
         *   our named classes.  
         */
        if (curcharlen == 1)
        {
            /* it's a literal - add the character */
            add_range_char(start.getch());
        }
        else if (curcharlen == 3 && start.getch_at(1) == '-')
        {
            /* it's a literal range - add it */
            add_range_char(start.getch(), start.getch_at(2));
        }
        else
        {
            struct char_class_t
            {
                /* expression name for the class */
                const char *name;
                
                /* length of the class name */
                size_t name_len;
                
                /* 
                 *   literal character, if the name represents a character
                 *   rather than a class - this is used only if code ==
                 *   RE_LITERAL 
                 */
                wchar_t ch;
                
                /* RE_xxx code for the class */
                re_recog_type code;
            };
            static const char_class_t classes[] =
            {
                { "alpha",    5, 0, RE_ALPHA },
                { "digit",    5, 0, RE_DIGIT },
                { "upper",    5, 0, RE_UPPER },
                { "lower",    5, 0, RE_LOWER },
                { "alphanum", 8, 0, RE_ALPHANUM },
                { "space",    5, 0, RE_SPACE },
                { "punct",    5, 0, RE_PUNCT },
                { "newline",  7, 0, RE_NEWLINE },
                { "langle",   6, '<', RE_LITERAL },
                { "rangle",   6, '>', RE_LITERAL },
                { "vbar",     4, '|', RE_LITERAL },
                { "caret",    5, '^', RE_LITERAL },
                { "squote",   6, '\'', RE_LITERAL },
                { "dquote",   6, '"', RE_LITERAL },
                { "star",     4, '*', RE_LITERAL },
                { "question", 8, '?', RE_LITERAL },
                { "percent",  7, '%', RE_LITERAL },
                { "dot",      3, '.', RE_LITERAL },
                { "period",   6, '.', RE_LITERAL },
                { "plus",     4, '+', RE_LITERAL },
                { "lsquare",  7, '[', RE_LITERAL },
                { "rsquare",  7, ']', RE_LITERAL },
                { "lparen",   6, '(', RE_LITERAL },
                { "rparen",   6, ')', RE_LITERAL},
                { "lbrace",   6, '{', RE_LITERAL },
                { "rbrace",   6, '}', RE_LITERAL },
                { "dollar",   6, '$', RE_LITERAL },
                { "backslash",9, '\\', RE_LITERAL },
                { "return",   6, 0x000D, RE_LITERAL },
                { "linefeed", 8, 0x000A, RE_LITERAL },
                { "tab",      3, 0x0009, RE_LITERAL },
                { "nul",      3, 0x0000, RE_LITERAL },
                { "null",     4, 0x0000, RE_LITERAL }
            };
            const char_class_t *cp;
            size_t i;
            int found;

            /* scan our name list for a match */
            for (cp = classes, i = 0, found = FALSE ;
                 i < sizeof(classes)/sizeof(classes[0]) ;
                 ++i, ++cp)
            {
                /* 
                 *   if the length matches and the name matches (ignoring
                 *   case), this is the one we want 
                 */
                if (curbytelen == cp->name_len
                    && memicmp(start.getptr(), cp->name, curbytelen) == 0)
                {
                    /* 
                     *   this is it - add either a class range or literal
                     *   range, depending on the meaning of the name 
                     */
                    if (cp->code == RE_LITERAL)
                    {
                        /* it's a name for a literal */
                        add_range_char(cp->ch);
                    }
                    else
                    {
                        /* 
                         *   It's a name for a character class.  As a
                         *   special case for efficiency, if this is the one
                         *   and only thing in this class expression, don't
                         *   create a range expression but instead create a
                         *   special for the class.
                         *   
                         *   Note that we can't do this for an exclusive
                         *   class, since we don't have any special matcher
                         *   for those - implement those with a character
                         *   range as usual.  
                         */
                        if (range_buf_cnt_ == 0
                            && delim == '>'
                            && !is_exclusive)
                        {
                            /*
                             *   This is the only thing, so build a special
                             *   to match this class - this is more
                             *   efficient to store and to match than a
                             *   range expression.  
                             */
                            build_special(result_machine, cp->code, 0);
                            
                            /* skip to the '>' */
                            *expr = p;
                            *exprlen = rem;
                            
                            /* 
                             *   we're done with the expresion - tell the
                             *   caller we were successful 
                             */
                            return TRUE;
                        }
                        else
                        {
                            /* 
                             *   it's not the only thing, so add the class
                             *   to the range list 
                             */
                            add_range_class(cp->code);
                        }
                    }
                    
                    /* note that we found a match */
                    found = TRUE;
                    
                    /* no need to scan further in our table */
                    break;
                }
            }
            
            /* if we didn't find a match, the whole expression is invalid */
            if (!found)
                return FALSE;
        }
        
        /* 
         *   if we found the '>', we're done; if we found a '|', skip it and
         *   keep going 
         */
        if (delim == '|')
        {
            /* skip the delimiter, and back for another round */
            p.inc(&rem);
        }
        else
        {
            /* we found the '>', so we're done - add the range recognizer */
            build_char_range(result_machine, is_exclusive);
            
            /* skip up to the '>' */
            *expr = p;
            *exprlen = rem;
            
            /* tell the caller we were successful */
            return TRUE;
        }
    }
}


/*
 *   Parse an integer value.  Returns -1 if there's no number.  
 */
int CRegexParser::parse_int(utf8_ptr *p, size_t *rem)
{
    int acc;
    
    /* if it's not a number to start with, simply return -1 */
    if (*rem == 0 || !is_digit(p->getch()))
        return -1;
    
    /* keep going as long as we find digits */
    for (acc = 0 ; *rem > 0 && is_digit(p->getch()) ; p->inc(rem))
    {
        /* add this digit into the accumulator */
        acc *= 10;
        acc += value_of_digit(p->getch());
    }

    /* return the accumulated result */
    return acc;
}

/*
 *   Break any infinite loops in the machine.  Check for cycles that
 *   consist solely of epsilon transitions, and break each cycle at the
 *   last alternating transition. 
 */
void CRegexParser::break_loops(re_machine *machine)
{
    re_state_id cur;
    
    /* 
     *   for each state, search the machine for cycles from the state back
     *   to itself 
     */
    for (cur = 0 ; cur < next_state_ ; ++cur)
    {
        /* test for loops from this state back to itself */
        find_loop(machine, cur);
    }
}

/*
 *   Find and break loops from the current state back to the given initial
 *   state completely via epsilon transitions.  Returns true if we found a
 *   loop back to the initial state, false if not.  
 */
int CRegexParser::find_loop(re_machine *machine, re_state_id cur_state)
{
    /* 
     *   keep going until we get to a final state or a non-epsilon
     *   transition 
     */
    for (;;)
    {
        re_tuple *tuple;

        /* 
         *   if this is the final state, there's nowhere else to go, so we
         *   evidently can't find the loop we were seeking
         */
        if (cur_state == machine->final)
            return FALSE;

        /* get the tuple for this state */
        tuple = &tuple_arr_[cur_state];

        /* 
         *   if this state has already been visited by a recursive caller,
         *   we're in a loop 
         */
        if ((tuple->flags & RE_STATE_CYCLE_TEST) != 0)
            return TRUE;
        
        /* if it's a two-transition epsilon state, handle it separately */
        if (tuple->typ == RE_EPSILON
            && tuple->next_state_2 != RE_STATE_INVALID)
        {
            unsigned char old_flags;
            
            /* 
             *   This is a branch.  Recursively try to find the loop in
             *   each branch.  If we do find the loop in either branch, it
             *   means that there are no branches closer to the final
             *   transition back to the original state, so we must break
             *   this branch.  
             */

            /* 
             *   mark the current state as being inspected, so if we
             *   encounter it again we'll know we've found a cycle 
             */
            old_flags = tuple->flags;
            tuple->flags |= RE_STATE_CYCLE_TEST;

            /* 
             *   try the second state first, since we won't have to
             *   shuffle around the slots to clear the second state 
             */
            if (find_loop(machine, tuple->next_state_2))
            {
                /* the second branch loops - break it */
                tuple->next_state_2 = RE_STATE_INVALID;
            }
            
            /* check the other branch */
            if (find_loop(machine, tuple->next_state_1))
            {
                /* 
                 *   the first branch loops - move the second branch to
                 *   the first slot, and clear the second slot (if we only
                 *   have one branch, it must always be in the first slot) 
                 */
                tuple->next_state_1 = tuple->next_state_2;
                tuple->next_state_2 = RE_STATE_INVALID;
            }

            /* we're done testing this state - restore its original marking */
            tuple->flags = old_flags;

            /* 
             *   if both branches are invalid, this entire state is
             *   unusable, so tell the caller to break its branch to here 
             */
            if (tuple->next_state_1 == RE_STATE_INVALID)
                return TRUE;
            
            /* 
             *   there's no need to continue either branch, since we've
             *   already followed them all the way to the end via the
             *   recursive call - and if we had a loop, we've broken it,
             *   so simply tell the caller there are no more loops along
             *   this path 
             */
            return FALSE;
        }

        /* see what kind of transition this is */
        switch(tuple->typ)
        {
        case RE_EPSILON:
        case RE_GROUP_ENTER:
        case RE_GROUP_EXIT:
            /* 
             *   epsilon or group transition - this could definitely be part
             *   of a loop, so move on to the next state and keep looking 
             */
            cur_state = tuple->next_state_1;

            /* 
             *   if this took us to an invalid state, we must have reached
             *   the final state of a sub-machine - we can go no further
             *   from here, so there are no loops in this branch 
             */
            if (cur_state == RE_STATE_INVALID)
                return FALSE;
            break;

        case RE_TEXT_BEGIN:
        case RE_TEXT_END:
        case RE_WORD_BEGIN:
        case RE_WORD_END:
        case RE_WORD_BOUNDARY:
        case RE_NON_WORD_BOUNDARY:
        case RE_ASSERT_POS:
        case RE_ASSERT_NEG:
        case RE_ZERO_VAR:
            /* 
             *   none of these transitions consumes input, so any of these
             *   could result in an infinite loop - continue down the
             *   current path 
             */
            cur_state = tuple->next_state_1;
            break;

        default:
            /* 
             *   all other states consume input, so this branch definitely
             *   can't loop back to the original state without consuming
             *   input - we do not need to proceed any further down the
             *   current branch, since it's not an infinite epsilon loop
             *   even if it does ultimately find its way back to the
             *   initial state 
             */
            return FALSE;
        }
    }
}

/*
 *   Optimization: remove meaningless branch-to-branch transitions.  An
 *   "epsilon" state that has only one outgoing transition does nothing
 *   except move to the next state, so any transition that points to such a
 *   state could just as well point to the destination of the epsilon's one
 *   outgoing transition.  
 */
void CRegexParser::remove_branch_to_branch(re_machine *machine)
{
    re_state_id cur;
    re_tuple *tuple;
    
    /* make sure the initial state points to the first meaningful state */
    optimize_transition(machine, &machine->init);

    /* scan all active states */
    for (cur = 0, tuple = tuple_arr_ ; cur < next_state_ ; ++cur, ++tuple)
    {
        /* if this isn't the final state, check it */
        if (cur != machine->final)
        {
            /* check both of our outgoing transitions */
            optimize_transition(machine, &tuple->next_state_1);
            optimize_transition(machine, &tuple->next_state_2);
        }
    }
}

/*
 *   Optimize a single transition to remove meaningless branch-to-branch
 *   transitions. 
 */
void CRegexParser::optimize_transition(const re_machine *machine,
                                       re_state_id *trans)
{
    /* keep going as long as we find meaningless forwarding branches */
    for (;;)
    {
        re_tuple *tuple_nxt;
        
        /* 
         *   if this transition points to the machine's final state, there's
         *   nowhere else to go from here 
         */
        if (*trans == RE_STATE_INVALID || *trans == machine->final)
            return;

        /* 
         *   if the transition points to anything other than a single-branch
         *   epsilon, we point to a meaningful next state, so there's no
         *   further branch-to-branch elimination we can perform 
         */
        tuple_nxt = &tuple_arr_[*trans];
        if (tuple_nxt->typ != RE_EPSILON
            || tuple_nxt->next_state_2 != RE_STATE_INVALID)
            return;

        /* 
         *   This transition points to a meaningless intermediate state, so
         *   we can simply skip the intermediate state and go directly from
         *   the current state to the target state's single target.  Once
         *   we've done this, continue scanning, because we might find that
         *   the new target state itself is a meaningless intermediate state
         *   that we can skip past as well (and so on, and so on - keep
         *   going until we find a real target state).  
         */
        *trans = tuple_nxt->next_state_1;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Compile an expression and return a newly-allocated pattern object.  
 */
re_status_t CRegexParser::compile_pattern(const char *expr_str,
                                          size_t exprlen,
                                          re_compiled_pattern **pattern)
{
    re_status_t stat;
    re_state_id i;
    re_tuple *t;
    re_compiled_pattern *pat;
    size_t siz;
    wchar_t *rp;
    re_compiled_pattern_base base_pat;

    /* presume we won't allocate a new pattern object */
    *pattern = 0;

    /* compile the pattern */
    if ((stat = compile(expr_str, exprlen, &base_pat)) != RE_STATUS_SUCCESS)
        return stat;

    /* 
     *   Start off with our basic space needs: we need space for the base
     *   structure, plus space for re_tuple array (actually, space for the
     *   number of allocated tuples minus one, since there's one built into
     *   the base structure).  
     */
    siz = sizeof(re_compiled_pattern)
          + (base_pat.tuple_cnt - 1)*sizeof(pat->tuples[0]);

    /* 
     *   Run through the tuples in the result machine and add up the amount
     *   of extra space we'll need for extra allocations (specifically,
     *   character ranges).  
     */
    for (i = 0, t = tuple_arr_ ; i < base_pat.tuple_cnt ; ++i, ++t)
    {
        /* check what kind of tuple this is */
        switch(t->typ)
        {
        case RE_RANGE:
        case RE_RANGE_EXCL:
            /* range - add in space for the character range data */
            siz += sizeof(t->info.range.char_range[0])
                   * t->info.range.char_range_cnt;
            break;

        default:
            /* other types require no extra storage */
            break;
        }
    }

    /* allocate space for the structure */
    *pattern = pat = (re_compiled_pattern *)t3malloc(siz);

    /* copy the base pattern to the result */
    memcpy(pat, &base_pat, sizeof(base_pat));

    /* copy the tuple array */
    memcpy(pat->tuples, tuple_arr_, pat->tuple_cnt * sizeof(pat->tuples[0]));

    /*
     *   Pack the range data onto the end of the tuple array in the data
     *   structure.  We already calculated how much space we'll need for this
     *   and included the space in the structure's allocation, so we simply
     *   need to find the location of the range data at the end of our tuple
     *   array.  
     */
    rp = (wchar_t *)&pat->tuples[pat->tuple_cnt];

    /* 
     *   run through the new tuple array and pack the range data into the new
     *   allocation unit 
     */
    for (i = 0, t = pat->tuples ; i < next_state_ ; ++i, ++t)
    {
        /* check what kind of tuple this is */
        switch(t->typ)
        {
        case RE_RANGE:
        case RE_RANGE_EXCL:
            /* 
             *   Character range.  Copy the original character range data
             *   into the new allocation unit, at the next available location
             *   in the allocation unit.  
             */
            memcpy(rp, t->info.range.char_range,
                   t->info.range.char_range_cnt
                   * sizeof(t->info.range.char_range[0]));

            /* fix up the tuple to point to the packed copy */
            t->info.range.char_range = rp;

            /* move past the space in our allocation unit */
            rp += t->info.range.char_range_cnt;
            break;

        default:
            /* other types contain no range data */
            break;
        }
    }

    /* success */
    return stat;
}

/* 
 *   free a pattern previously created with compile_pattern() 
 */
void CRegexParser::free_pattern(re_compiled_pattern *pattern)
{
    /* we allocate each pattern as a single unit, so it's easy to free */
    t3free(pattern);
}

/* ------------------------------------------------------------------------ */
/*
 *   Pattern recognizer 
 */

/*
 *   construct 
 */
CRegexSearcher::CRegexSearcher()
{
}

/*
 *   delete 
 */
CRegexSearcher::~CRegexSearcher()
{
}

/*
 *   Match a string to a compiled expression.  Returns the length of the
 *   match if successful, or -1 if no match was found.  
 */
int CRegexSearcher::match(const char *entire_str,
                          const char *str, const size_t origlen,
                          const re_compiled_pattern_base *pattern,
                          const re_tuple *tuple_arr,
                          const re_machine *machine,
                          re_group_register *regs,
                          short *loop_vars)
{
    size_t entire_str_len;
    re_state_id cur_state, final_state;
    utf8_ptr p;
    size_t curlen;
    int start_ofs;
    int _retval_;

    const int ST_EPS1 = 1;      /* doing first branch of two-branch epsilon */
    const int ST_EPS2 = 2;     /* doing second branch of two-branch epsilon */
    const int ST_ASSERT = 3;                          /* doing an assertion */

    /* macro to perform a"local return" */
#define local_return(retval) \
    _retval_ = (retval); \
    goto do_local_return;

    /* reset the stack */
    stack_.reset();

    /* start at the machine's initial state */
    cur_state = machine->init;

    /* note the final state */
    final_state = machine->final;

    /* 
     *   if we're starting in the final state, this is a zero-length
     *   pattern, which matches anything - immediately return success with a
     *   zero-length match 
     */
    if (cur_state == final_state)
        return 0;

    /* start at the beginning of the string */
    p.set((char *)str);
    curlen = origlen;

    /* note the offset from the start of the entire string */
    start_ofs = str - entire_str;
    entire_str_len = start_ofs + origlen;

    /* run the machine */
    for (;;)
    {
        const re_tuple *tuple;

        /* get the tuple for this state */
        tuple = &tuple_arr[cur_state];

        /* check the type of state we're processing */
        switch(tuple->typ)
        {
        case RE_ZERO_VAR:
            /* save the variable in the stack frame if necessary */
            stack_.save_loop_var(tuple->info.loop.loop_var, loop_vars);

            /* zero the loop variable and proceed */
            loop_vars[tuple->info.loop.loop_var] = 0;
            break;
            
        case RE_LOOP_BRANCH:
            /*
             *   This is a loop branch.  Check the variable to see if we've
             *   satisfied the loop condition yet:
             *   
             *   - If we haven't yet reached the minimum loop count, simply
             *   transition into the loop (i.e., take the 'enter' branch
             *   unconditionally).
             *   
             *   - If we have reached the maximum loop count (if there is
             *   one - if max is -1 then there's no upper bound), simply
             *   transition past the loop (i.e., take the 'bypass' branch
             *   unconditionally).
             *   
             *   - Otherwise, we must treat this just like an ordinary
             *   two-way epsilon branch.
             *   
             *   Note that, if we have a 'shortest' modifier, the bypass
             *   branch will be first, to encourage the indeterminate check
             *   to choose the short branch whenever possible; otherwise the
             *   enter branch will be first, so we take the long branch
             *   whenever possible.  
             */
            {
                short *varptr;
                
                /* get the variable value */
                varptr = &loop_vars[tuple->info.loop.loop_var];
                
                /* save the variable in the stack frame if necessary */
                stack_.save_loop_var(tuple->info.loop.loop_var, loop_vars);

                /* 
                 *   if we're not at the loop minimum yet, transition into
                 *   the loop body 
                 */
                if (*varptr < tuple->info.loop.loop_min)
                {
                    /* increment the loop counter */
                    ++(*varptr);
                    
                    /* unconditionally transfer into the loop */
                    if ((tuple->flags & RE_STATE_SHORTEST) == 0)
                        cur_state = tuple->next_state_1;
                    else
                        cur_state = tuple->next_state_2;
                    
                    /* we're done processing this state */
                    goto got_next_state;
                }
                
                /* 
                 *   if we've reached the loop maximum (if there is one),
                 *   transition past the loop 
                 */
                if (tuple->info.loop.loop_max >= 0
                    && *varptr >= tuple->info.loop.loop_max)
                {
                    /* unconditionally skip the loop */
                    if ((tuple->flags & RE_STATE_SHORTEST) == 0)
                        cur_state = tuple->next_state_2;
                    else
                        cur_state = tuple->next_state_1;
                    
                    /* we're done with this state */
                    goto got_next_state;
                }

                /* 
                 *   We don't know which way to go, so we must treat this as
                 *   a two-way epsilon branch.  Count this as another loop
                 *   iteration, since the branch that enters the loop will
                 *   come back here for another try.  The branch that skips
                 *   the loop doesn't care about the loop counter any more,
                 *   so we can just increment it and ignore the skip branch.
                 */
                ++(*varptr);
                goto two_way_epsilon;
            }
            /* not reached */
            
        case RE_GROUP_MATCH:
            {
                int group_num;
                re_group_register *group_reg;
                size_t reg_len;
                
                /* it's a group - get the group number */
                group_num = (int)tuple->info.ch;
                group_reg = &regs[group_num];

                /* 
                 *   if this register isn't defined, there's nothing to
                 *   match, so fail 
                 */
                if (group_reg->start_ofs == -1
                    || group_reg->end_ofs == -1)
                {
                    local_return(-1);
                }

                /* calculate the length of the register value */
                reg_len = group_reg->end_ofs - group_reg->start_ofs;

                /* if we don't have enough left to match, it fails */
                if (curlen < reg_len)
                {
                    local_return(-1);
                }

                /* if the string doesn't match exactly, we fail */
                if (memcmp(p.getptr(), entire_str + group_reg->start_ofs,
                           reg_len) != 0)
                {
                    local_return(-1);
                }

                /*
                 *   It matches exactly - skip the entire byte length of the
                 *   register in the source string 
                 */
                p.set(p.getptr() + reg_len);
                curlen -= reg_len;
            }
            break;

        case RE_TEXT_BEGIN:
            /* 
             *   Match only the exact beginning of the string - if we're
             *   anywhere else, this isn't a match.  If this succeeds, we
             *   don't skip any characters.  
             */
            if (p.getptr() != entire_str)
            {
                local_return(-1);
            }
            break;

        case RE_TEXT_END:
            /*
             *   Match only the exact end of the string - if we're anywhere
             *   else, this isn't a match.  Don't skip any characters on
             *   success.  
             */
            if (curlen != 0)
            {
                local_return(-1);
            }
            break;

        case RE_WORD_BEGIN:
            /* 
             *   If the previous character is a word character, we're not at
             *   the beginning of a word.  If we're at the beginning of the
             *   entire string, we need not check anything previous -
             *   there's no previous character, so we can't have a preceding
             *   word character.  
             */
            if (p.getptr() != entire_str
                && is_word_char(p.getch_before(1)))
            {
                local_return(-1);
            }

            /* 
             *   if we're at the end of the string, or the current character
             *   isn't a word character, we're not at the beginning of a
             *   word 
             */
            if (curlen == 0 || !is_word_char(p.getch()))
            {
                local_return(-1);
            }
            break;

        case RE_WORD_END:
            /*
             *   if the current character is a word character, we're not at
             *   the end of a word 
             */
            if (curlen != 0 && is_word_char(p.getch()))
            {
                local_return(-1);
            }
            
            /*
             *   if we're at the beginning of the string, or the previous
             *   character is not a word character, we're not at the end of
             *   a word 
             */
            if (p.getptr() == entire_str
                || !is_word_char(p.getch_before(1)))
            {
                local_return(-1);
            }
            break;

        case RE_WORD_CHAR:
            /* if it's not a word character, it's a failure */
            if (curlen == 0 || !is_word_char(p.getch()))
            {
                local_return(-1);
            }

            /* skip this character of input */
            p.inc(&curlen);
            break;

        case RE_NON_WORD_CHAR:
            /* if it's a word character, it's a failure */
            if (curlen == 0 || is_word_char(p.getch()))
            {
                local_return(-1);
            }

            /* skip the input */
            p.inc(&curlen);
            break;

        case RE_WORD_BOUNDARY:
        case RE_NON_WORD_BOUNDARY:
            {
                int prev_is_word;
                int next_is_word;
                int boundary;

                /*
                 *   Determine if the previous character is a word character
                 *   -- if we're at the beginning of the string, it's
                 *   obviously not, otherwise check its classification 
                 */
                if (p.getptr() == entire_str)
                {
                    /* 
                     *   at the start of the string, so there definitely
                     *   isn't a preceding word character 
                     */
                    prev_is_word = FALSE;
                }
                else
                {
                    /* look at the previous character */
                    prev_is_word = is_word_char(p.getch_before(1));
                }

                /* make the same check for the current character */
                next_is_word = (curlen != 0
                                && is_word_char(p.getch()));

                /*
                 *   Determine if this is a boundary - it is if the two
                 *   states are different 
                 */
                boundary = ((prev_is_word != 0) ^ (next_is_word != 0));

                /* 
                 *   make sure it matches what was desired, and return
                 *   failure if not 
                 */
                if ((tuple->typ == RE_WORD_BOUNDARY && !boundary)
                    || (tuple->typ == RE_NON_WORD_BOUNDARY && boundary))
                {
                    local_return(-1);
                }
            }
            break;
            
        case RE_WILDCARD:
            /* make sure we have a character to match */
            if (curlen == 0)
            {
                local_return(-1);
            }
            
            /* skip this character */
            p.inc(&curlen);
            break;

        case RE_RANGE:
        case RE_RANGE_EXCL:
            {
                int match;
                wchar_t ch;
                wchar_t *rp;
                size_t i;

                /* make sure we have a character to match */
                if (curlen == 0)
                {
                    local_return(-1);
                }

                /* get this character */
                ch = p.getch();

                /* search for the character in the range */
                for (i = tuple->info.range.char_range_cnt,
                     rp = tuple->info.range.char_range ;
                     i != 0 ; i -= 2, rp += 2)
                {
                    /* 
                     *   check for a class specifier; if it's not a class
                     *   specifier, treat it as a literal range, and check
                     *   case sensitivity 
                     */
                    if (rp[0] == '\0')
                    {
                        int match;

                        /*
                         *   The first character of the range pair is null,
                         *   which means that this isn't a literal range but
                         *   rather a class.  Check for a match to the
                         *   class.  
                         */
                        switch(rp[1])
                        {
                        case RE_ALPHA:
                            match = t3_is_alpha(ch);
                            break;

                        case RE_DIGIT:
                            match = t3_is_digit(ch);
                            break;

                        case RE_UPPER:
                            match = t3_is_upper(ch);
                            break;

                        case RE_LOWER:
                            match = t3_is_lower(ch);
                            break;

                        case RE_ALPHANUM:
                            match = t3_is_alpha(ch) || t3_is_digit(ch);
                            break;

                        case RE_SPACE:
                            match = t3_is_space(ch);
                            break;

                        case RE_PUNCT:
                            match = t3_is_punct(ch);
                            break;

                        case RE_NEWLINE:
                            match = (ch == 0x000A
                                     || ch == 0x000D
                                     || ch == 0x2028);
                            break;
                            
                        case RE_NULLCHAR:
                            match = (ch == 0);
                            break;
                            
                        default:
                            /* this shouldn't happen */
                            match = FALSE;
                            break;
                        }
                        
                        /* 
                         *   if we matched, we can stop looking; otherwise,
                         *   simply keep going, since there might be another
                         *   entry that does match 
                         */
                        if (match)
                            break;
                    }
                    else if (pattern->case_sensitive)
                    {
                        /* 
                         *   the search is case-sensitive - compare the
                         *   character to the range without case conversion 
                         */
                        if (ch >= rp[0] && ch <= rp[1])
                            break;
                    }
                    else if (t3_is_upper(rp[0]) && t3_is_upper(rp[1]))
                    {
                        wchar_t uch;
                        
                        /* 
                         *   the range is all upper-case letters - convert
                         *   the source character to upper-case for the
                         *   comparison 
                         */
                        uch = t3_to_upper(ch);
                        if (uch >= rp[0] && uch <= rp[1])
                            break;
                    }
                    else if (t3_is_lower(rp[0]) && t3_is_lower(rp[1]))
                    {
                        wchar_t lch;

                        /* 
                         *   the range is all lower-case letters - convert
                         *   the source character to upper-case for the
                         *   comparison 
                         */
                        lch = t3_to_lower(ch);
                        if (lch >= rp[0] && lch <= rp[1])
                            break;
                    }
                    else
                    {
                        /* 
                         *   The cases of the two ends of the range don't
                         *   agree, so there's nothing we can do for case
                         *   conversions.  Simply compare the range exactly.
                         */
                        if (ch >= rp[0] && ch <= rp[1])
                            break;
                    }
                }
                
                /* we matched if we stopped before exhausting the list */
                match = (i != 0);
                
                /* make sure we got what we wanted */
                if ((tuple->typ == RE_RANGE && !match)
                    || (tuple->typ == RE_RANGE_EXCL && match))
                {
                    local_return(-1);
                }

                /* skip this character of the input */
                p.inc(&curlen);
            }
            break;
            
        case RE_ALPHA:
            /* check for an alphabetic character */
            if (curlen == 0 || !t3_is_alpha(p.getch()))
            {
                local_return(-1);
            }
            
            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;
            
        case RE_DIGIT:
            /* check for a digit character */
            if (curlen == 0 || !t3_is_digit(p.getch()))
            {
                local_return(-1);
            }

            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;

        case RE_UPPER:
            /* check for an upper-case alphabetic character */
            if (curlen == 0 || !t3_is_upper(p.getch()))
            {
                local_return(-1);
            }

            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;

        case RE_LOWER:
            /* check for a lower-case alphabetic character */
            if (curlen == 0 || !t3_is_lower(p.getch()))
            {
                local_return(-1);
            }

            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;

        case RE_ALPHANUM:
            /* check for an alphabetic or digit character */
            if (curlen == 0
                || (!t3_is_alpha(p.getch()) && !t3_is_digit(p.getch())))
            {
                local_return(-1);
            }

            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;

        case RE_SPACE:
            /* check for a space of some kind */
            if (curlen == 0 || !t3_is_space(p.getch()))
            {
                local_return(-1);
            }

            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;

        case RE_PUNCT:
            /* check for a punctuation character of some kind */
            if (curlen == 0 || !t3_is_punct(p.getch()))
            {
                local_return(-1);
            }

            /* skip this character of the input, and continue */
            p.inc(&curlen);
            break;

        case RE_NEWLINE:
            /* check for a newline character of some kind */
            {
                wchar_t ch;

                /* if we're out of characters, we don't have a match */
                if (curlen == 0)
                {
                    local_return(-1);
                }

                /* get the character */
                ch = p.getch();

                /* if it's not a newline, we fail this match */
                if (ch != 0x000A && ch != 0x000d && ch != 0x2028)
                {
                    local_return(-1);
                }
            }

            /* skip this character of input and continue */
            p.inc(&curlen);
            break;

        case RE_ASSERT_POS:
        case RE_ASSERT_NEG:
            /*
             *   It's an assertion.  Run this as a sub-state: push the
             *   current state so that we can come back to it later. 
             */
            stack_.push(ST_ASSERT, start_ofs, p.getptr() - entire_str,
                        cur_state, final_state);

            /*
             *   In the sub-state, start with the sub-machine's initial
             *   state and finish with the sub-machine's final state. 
             */
            cur_state = tuple->info.sub.init;
            final_state = tuple->info.sub.final;

            /* in the sub-state, the sub-string to match starts here */
            start_ofs = p.getptr() - entire_str;

            /* 
             *   just proceed from here; we'll finish up with the assertion
             *   test when we reach the final sub-machine state and pop the
             *   stack state we pushed 
             */
            goto got_next_state;

        case RE_LITERAL:
            /* 
             *   ordinary character match - if there's not another
             *   character, we obviously fail 
             */
            if (curlen == 0)
            {
                local_return(-1);
            }

            /* 
             *   check case sensitivity - if we're not in case-sensitive
             *   mode, and both characters are alphabetic, perform a
             *   case-insensitive comparison; otherwise, perform an exact
             *   comparison 
             */
            if (tuple->info.ch == p.getch())
            {
                /* 
                 *   we have an exact match; there's no need to check for
                 *   any case conversions 
                 */
            }
            else if (!pattern->case_sensitive
                     && t3_is_alpha(tuple->info.ch) && t3_is_alpha(p.getch()))
            {
                /* 
                 *   We're performing a case-insensitive search, and both
                 *   characters are alphabetic.  Convert the string
                 *   character to the same case as the pattern character,
                 *   then compare them.  Note that we always use the case of
                 *   the pattern character, because this gives the pattern
                 *   control over the handling for languages where
                 *   conversions are ambiguous.  
                 */
                if (t3_is_upper(tuple->info.ch))
                {
                    /* 
                     *   the pattern character is upper-case - convert the
                     *   string character to upper-case and compare 
                     */
                    if (tuple->info.ch != t3_to_upper(p.getch()))
                    {
                        local_return(-1);
                    }
                }
                else if (t3_is_lower(tuple->info.ch))
                {
                    /* 
                     *   the pattern character is lower-case - compare to
                     *   the lower-case string character 
                     */
                    if (tuple->info.ch != t3_to_lower(p.getch()))
                    {
                        local_return(-1);
                    }
                }
                else
                {
                    /* 
                     *   the pattern character is neither upper nor lower
                     *   case, so we cannot perform a case conversion; we
                     *   know the match isn't exact, so we don't have a
                     *   match at all 
                     */
                    local_return(-1);
                }
            }
            else
            {
                /* 
                 *   the search is case-sensitive, or this pattern character
                 *   is non-alphabetic; since we didn't find an exact match,
                 *   the string does not match the pattern 
                 */
                local_return(-1);
            }
            
            /* skip this character of the input */
            p.inc(&curlen);
            break;

        case RE_GROUP_ENTER:
            /* 
             *   if the group index (given by the state's character ID) is
             *   in range, note the location in the string where the group
             *   starts 
             */
            if (tuple->info.ch < RE_GROUP_REG_CNT)
            {
                /* save the register in the stack frame if necessary */
                stack_.save_group_reg(tuple->info.ch, regs);
                
                /* store the new value */
                regs[tuple->info.ch].start_ofs = p.getptr() - entire_str;
            }
            break;

        case RE_GROUP_EXIT:
            /* 
             *   note the string location of the end of the group, if the
             *   group ID is in range 
             */
            if (tuple->info.ch < RE_GROUP_REG_CNT)
            {
                /* save the register in the stack frame if necessary */
                stack_.save_group_reg(tuple->info.ch, regs);

                /* store the new value */
                regs[tuple->info.ch].end_ofs = p.getptr() - entire_str;
            }
            break;

        case RE_EPSILON:
            /* it's an epsilon transition */
            if (tuple->next_state_2 == RE_STATE_INVALID)
            {
                /*
                 *   We have only one transition, so this state is entirely
                 *   deterministic.  Simply move on to the next state.  (We
                 *   try to optimize these types of transitions out of the
                 *   machine when compiling, but we handle them anyway in
                 *   case any survive optimization.)  
                 */
                cur_state = tuple->next_state_1;
            }
            else
            {
            two_way_epsilon:
                /*
                 *   This state has two possible transitions, and we don't
                 *   know which one to take.  So, try both, see which one
                 *   works better, and return the result.  Try the first
                 *   transition first.
                 *   
                 *   To test both branches, we push the machine state onto
                 *   the stack, test the first branch, then restore the
                 *   machine state and test the second branch.  We return
                 *   the better of the two branches.
                 *   
                 *   Set up to test the first branch by pushing the current
                 *   machine state, noting that we're in the first branch of
                 *   a two-branch epsilon so that we'll know to proceed to
                 *   the second branch when we finish with the first one.  
                 */
                stack_.push(ST_EPS1, start_ofs, p.getptr() - entire_str,
                            cur_state, final_state);

                /* the next state is the first branch of the epsilon */
                cur_state = tuple->next_state_1;

                /* match the sub-string from here */
                start_ofs = p.getptr() - entire_str;

                /* we have the next state set to go */
                goto got_next_state;
            }
            break;
            
        default:
            /* invalid node type */
            assert(FALSE);
            local_return(-1);
        }
        
        /* 
         *   if we got this far, we were successful - move on to the next
         *   state 
         */
        cur_state = tuple->next_state_1;
        
    got_next_state:
        /* we come here when we've already figured out the next state */
        ;

        /* 
         *   If we're in the final state, it means we've matched the
         *   pattern.  Return success by indicating the length of the string
         *   we matched.  
         */
        if (cur_state == final_state)
        {
            local_return(p.getptr() - (entire_str + start_ofs));
        }

        /* 
         *   if we're in an invalid state, the expression must have been
         *   ill-formed; return failure 
         */
        if (cur_state == RE_STATE_INVALID)
        {
            local_return(-1);
        }

        /* resume the loop */
        continue;

    do_local_return:
        /* check what kind of state we're returning to */
        switch(stack_.get_top_type())
        {
            int str_ofs;
            int ret1, ret2;
            int ret1_is_winner;
               
        case -1:
            /*
             *   There's nothing left on the state stack, so we're actually
             *   returning to our caller.  Simply return the given return
             *   value.  
             */
            return _retval_;

        case ST_EPS1:
            /*
             *   We're finishing the first branch of a two-branch epsilon.
             *   Swap the current state with the saved state on the stack,
             *   and push a new state for the second branch.  
             */
            str_ofs = p.getptr() - entire_str;
            stack_.save_and_push(_retval_, ST_EPS2, &start_ofs, &str_ofs,
                                 &cur_state, &final_state,
                                 regs, loop_vars);

            /* the next state is the second branch */
            cur_state = tuple_arr[cur_state].next_state_2;

            /* set up at the original string position */
            p.set((char *)entire_str + str_ofs);
            curlen = entire_str_len - str_ofs;

            /* the second branch substring starts at the current character */
            start_ofs = p.getptr() - entire_str;

            /* continue from here */
            break;

        case ST_EPS2:
            /* 
             *   We're finishing the second branch of a two-branch epsilon.
             *   First, get the two return values - the first branch is the
             *   return value saved in the second-from-the-top stack frame,
             *   and the second branch is the current return value.  
             */
            ret1 = stack_.get_frame(1)->retval;
            ret2 = _retval_;
            
            /*
             *   If they both failed, the whole thing failed.  Otherwise,
             *   return the longer or shorter of the two, depending on the
             *   current match mode, plus the length we ourselves matched
             *   previously.  Note that we return the register set from the
             *   winning match.  
             */
            if (ret1 < 0 && ret2 < 0)
            {
                /* 
                 *   They both failed - backtrack to the initial state by
                 *   popping the second branch's frame (which stores the
                 *   branch initial state) and discarding the first branch's
                 *   frame (which stores the first branch's *final* state).
                 *   Note that the only thing we care about from the stack
                 *   is the group registers and loop variables; everything
                 *   else will be restored from the enclosing frame that
                 *   we're returning to.  
                 */
                stack_.pop(&start_ofs, &str_ofs, &cur_state, &final_state,
                           regs, loop_vars);
                stack_.discard();

                /* set the string pointer */
                p.set((char *)entire_str + str_ofs);
                curlen = entire_str_len - str_ofs;

                /* return failure */
                local_return(-1);
            }

            /* 
             *   Choose the winner based on the match mode.  The match mode
             *   of interest is the one in the original two-way epsilon,
             *   which is the state in the stack frame at the top of the
             *   stack.  
             */
            if (pattern->longest_match
                && !(tuple_arr[stack_.get_frame(0)->state].flags
                     & RE_STATE_SHORTEST))
            {
                /* 
                 *   longest match - choose ret1 if ret2 isn't a good match,
                 *   or ret1 is longer than ret2 
                 */
                ret1_is_winner = (ret2 < 0 || ret1 >= ret2);
            }
            else
            {
                /* 
                 *   shortest match - choose ret1 if it's the only good
                 *   match, or if they're both good and it's the shorter one 
                 */
                ret1_is_winner = ((ret1 >= 0 && ret2 < 0)
                                  || (ret1 >= 0 && ret2 >= 0
                                      && ret1 <= ret2));
            }
            
            /* choose the winner */
            if (ret1_is_winner)
            {
                /*
                 *   The winner is the first branch.  First, pop the second
                 *   branch's initial state, since this is the baseline for
                 *   the first branch's final state. 
                 */
                stack_.pop(&start_ofs, &str_ofs, &cur_state, &final_state,
                           regs, loop_vars);

                /* add in the length up to the initial state at the epsilon */
                ret1 += str_ofs - start_ofs;

                /*
                 *   Second, pop the first branch's final state.  This is
                 *   stored relative to the initial state of the branch, so
                 *   we're ready to restore it now. 
                 */
                stack_.pop(&start_ofs, &str_ofs, &cur_state, &final_state,
                           regs, loop_vars);

                /* set the string pointer */
                p.set((char *)entire_str + str_ofs);
                curlen = entire_str_len - str_ofs;

                /* return the result */
                local_return(ret1);
            }
            else
            {
                regex_stack_entry *fp;
                
                /*
                 *   The winner is the second branch.  The current machine
                 *   state is the final state for the second branch, so we
                 *   simply need to discard the saved state for the two
                 *   branches.  
                 */

                /* add in the length up to the initial state at the epsilon */
                fp = stack_.get_frame(0);
                ret2 += fp->str_ofs - fp->start_ofs;

                /* discard the two branch states */
                stack_.discard();
                stack_.discard();
                
                /* return the result */
                local_return(ret2);
            }

        case ST_ASSERT:
            /*
             *   We're finishing an assertion sub-machine.  First, pop the
             *   machine state to get back to where we were.  
             */
            stack_.pop(&start_ofs, &str_ofs, &cur_state, &final_state,
                       regs, loop_vars);

            /* 
             *   set the string pointer and remaining length for the string
             *   offset we popped from the state 
             */
            p.set((char *)entire_str + str_ofs);
            curlen = entire_str_len - str_ofs;

            /* 
             *   If this is a positive assertion and it didn't match, return
             *   failure; if this is a negative assertion and it did match,
             *   return failure; otherwise, proceed without having consumed
             *   any input 
             */
            if (tuple_arr[cur_state].typ == RE_ASSERT_POS
                ? _retval_ < 0 : _retval_ >= 0)
            {
                local_return(-1);
            }

            /* resume where we left off */
            cur_state = tuple_arr[cur_state].next_state_1;
            break;
        }

        /* resume from here */
        goto got_next_state;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Search for a regular expression within a string.  Returns -1 if the
 *   string cannot be found, otherwise returns the offset from the start
 *   of the string to be searched of the start of the first match for the
 *   pattern.  
 */
int CRegexSearcher::search(const char *entirestr, const char *str, size_t len,
                           const re_compiled_pattern_base *pattern,
                           const re_tuple *tuple_arr,
                           const re_machine *machine, re_group_register *regs,
                           int *result_len)
{
    utf8_ptr p;
    re_group_register best_match_regs[RE_GROUP_REG_CNT];
    short loop_vars[RE_LOOP_VARS_MAX];
    int best_match_len;
    int best_match_start;
    const char *max_start_pos;

    /* we don't have a match yet */
    best_match_len = -1;
    best_match_start = -1;

    /* search the entire string */
    max_start_pos = str + len;
    
    /*
     *   Starting at the first character in the string, search for the
     *   pattern at each subsequent character until we either find the
     *   pattern or run out of string to test. 
     */
    for (p.set((char *)str) ; p.getptr() < max_start_pos ; p.inc(&len))
    {
        int matchlen;
        
        /* check for a match */
        matchlen = match(entirestr, p.getptr(), len,
                         pattern, tuple_arr, machine, regs, loop_vars);
        if (matchlen >= 0)
        {
            /* check our first-begin/first-end mode */
            if (pattern->first_begin)
            {
                /*   
                 *   We're in first-begin mode: return immediately,
                 *   because we want to find the match that starts at the
                 *   earliest point in the string; having found a match
                 *   here, there's no point in looking for a later match,
                 *   since it obviously won't start any earlier than this
                 *   one does.  
                 */
                *result_len = matchlen;
                return p.getptr() - str;
            }
            else
            {
                int keep;
                
                /*
                 *   We're in first-end mode.  We can't return yet,
                 *   because we might find something that ends before this
                 *   string does.
                 *   
                 *   If this is the first or best match so far, note it;
                 *   otherwise, ignore it.  In any case, continue looking.
                 */
                if (best_match_len == -1)
                {
                    /* 
                     *   this is our first match - it's definitely the
                     *   best so far 
                     */
                    keep = TRUE;
                }
                else
                {
                    int end_idx;
                    
                    /* calculate the ending index of this match */
                    end_idx = (p.getptr() - str) + matchlen;

                    /* see what we have */
                    if (end_idx < best_match_start + best_match_len)
                    {
                        /* 
                         *   this one ends earlier than the previous best
                         *   match so far -- we definitely want to keep
                         *   this one 
                         */
                        keep = TRUE;
                    }
                    else if (end_idx == best_match_start + best_match_len)
                    {
                        /* 
                         *   This one ends at exactly the same point as
                         *   our best match so far.  Decide which one to
                         *   keep based on the lengths.  If we're in
                         *   longest-match mode, keep the longer one,
                         *   otherwise keep the shorter one. 
                         */
                        if (pattern->longest_match)
                        {
                            /* 
                             *   longest-match mode - keep the old one,
                             *   since it's longer (it starts earlier and
                             *   ends at the same point) 
                             */
                            keep = FALSE;
                        }
                        else
                        {
                            /* 
                             *   shortest-match mode - keep the new one,
                             *   since it's shorter (it starts later and
                             *   ends at the same point) 
                             */
                            keep = TRUE;
                        }
                    }
                    else
                    {
                        /* 
                         *   this one isn't as good as the previous best
                         *   -- ignore this one and keep our previous
                         *   winner 
                         */
                        keep = FALSE;
                    }
                }
                
                /* if we're keeping this match, save it */
                if (keep)
                {
                    /* save the start index, length, and register contents */
                    best_match_start = p.getptr() - str;
                    best_match_len = matchlen;
                    memcpy(best_match_regs, regs, sizeof(best_match_regs));

                    /*
                     *   There's no point in looking for strings that
                     *   start beyond the end of the current match,
                     *   because they certainly won't end before this
                     *   match ends.  So, adjust our end-of-loop marker to
                     *   point to the next character past the end of our
                     *   match. 
                     */
                    max_start_pos = p.getptr() + matchlen;
                }
            }
        }
    }

    /* if we found a previous match, return it */
    if (best_match_len != -1)
    {
        /* set the caller's match length */
        *result_len = best_match_len;

        /* copy the saved match registers into the caller's registers */
        memcpy(regs, best_match_regs, sizeof(best_match_regs));

        /* return the starting index of the match */
        return best_match_start;
    }

    /* we didn't find a match */
    return -1;
}

/* ------------------------------------------------------------------------ */
/*
 *   Search for a previously-compiled pattern within the given string.
 *   Returns the offset of the match, or -1 if no match was found.  
 */
int CRegexSearcher::search_for_pattern(
    const re_compiled_pattern *pattern,
    const char *entirestr, const char *searchstr, size_t searchlen,
    int *result_len, re_group_register *regs)
{
    /* 
     *   search for the pattern in our copy of the string - use the copy so
     *   that the group registers stay valid even if the caller deallocates
     *   the original string after we return 
     */
    return search(entirestr, searchstr, searchlen, pattern, pattern->tuples,
                  &pattern->machine, regs, result_len);
}

/* ------------------------------------------------------------------------ */
/*
 *   Check for a match to a previously compiled expression.  Returns the
 *   length of the match if we found a match, -1 if we found no match.  This
 *   is not a search function; we merely match the leading substring of the
 *   given string to the given pattern.  
 */
int CRegexSearcher::match_pattern(
    const re_compiled_pattern *pattern,
    const char *entirestr, const char *searchstr, size_t searchlen, 
    re_group_register *regs)
{
    short loop_vars[RE_LOOP_VARS_MAX];

    /* match the string */
    return match(entirestr, searchstr, searchlen,
                 pattern, pattern->tuples, &pattern->machine,
                 regs, loop_vars);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compile an expression and check for a match.  Returns the length of the
 *   match if we found a match, -1 if we found no match.  This is not a
 *   search function; we merely match the leading substring of the given
 *   string to the given pattern.  
 *   
 *   If a pattern pattern will be used repeatedly in multiple searches or
 *   matches, it is more efficient to compile the pattern once and then
 *   re-use the compiled pattern for each search or match, since compiling a
 *   pattern takes time.  
 */
int CRegexSearcherSimple::compile_and_match(
    const char *patstr, size_t patlen,
    const char *entirestr, const char *searchstr, size_t searchlen)
{
    re_compiled_pattern_base pat;
    short loop_vars[RE_LOOP_VARS_MAX];

    /* no groups yet */
    group_cnt_ = 0;

    /* clear the group registers */
    clear_group_regs(regs_);

    /* compile the expression - return failure if we get an error */
    if (parser_->compile(patstr, patlen, &pat) != RE_STATUS_SUCCESS)
        return FALSE;

    /* remember the group count from the compiled pattern */
    group_cnt_ = pat.group_cnt;

    /* match the string */
    return match(entirestr, searchstr, searchlen,
                 &pat, parser_->tuple_arr_, &pat.machine, regs_, loop_vars);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compile an expression and search for a match within the given string.
 *   Returns the offset of the match, or -1 if no match was found.
 *   
 *   If a pattern will be used repeatedly in multiple searches or matches, it
 *   is more efficient to compile the pattern once, using compile_pattern(),
 *   and then re-use the compiled pattern for each search, since compiling a
 *   pattern takes time.  
 */
int CRegexSearcherSimple::compile_and_search(
    const char *patstr, size_t patlen,
    const char *entirestr, const char *searchstr, size_t searchlen,
    int *result_len)
{
    re_compiled_pattern_base pat;

    /* no groups yet */
    group_cnt_ = 0;

    /* clear the group registers */
    clear_group_regs(regs_);

    /* compile the expression - return failure if we get an error */
    if (parser_->compile(patstr, patlen, &pat) != RE_STATUS_SUCCESS)
        return -1;

    /* remember the group count from the compiled pattern */
    group_cnt_ = pat.group_cnt;

    /* 
     *   search for the pattern in our copy of the string - use the copy so
     *   that the group registers stay valid even if the caller deallocates
     *   the original string after we return 
     */
    return search(entirestr, searchstr, searchlen,
                  &pat, parser_->tuple_arr_, &pat.machine,
                  regs_, result_len);
}
