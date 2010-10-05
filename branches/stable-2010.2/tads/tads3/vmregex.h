/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmregex.h - regular expression parser for T3
Function
  
Notes
  Adapted from the TADS 2 regular expression parser.  This version
  uses UTF-8 strings rather than simple single-byte character strings,
  and is organized into a C++ class.
Modified
  04/11/99 CNebel     - Fix warnings.
  10/07/98 MJRoberts  - Creation
*/

#ifndef VMREGEX_H
#define VMREGEX_H

#include <stdlib.h>
#include "t3std.h"
#include "vmuni.h"
#include "utf8.h"
#include "vmerr.h"
#include "vmerrnum.h"


/* state ID */
typedef int re_state_id;

/* invalid state ID - used to mark null machines */
#define RE_STATE_INVALID   ((re_state_id)-1)

/* first valid state ID */
#define RE_STATE_FIRST_VALID  ((re_state_id)0)


/* ------------------------------------------------------------------------ */
/*
 *   Group register structure.  Each register keeps track of the starting
 *   and ending offset of the group's text within the original search
 *   string.  
 */
struct re_group_register
{
    int start_ofs;
    int end_ofs;
};

/* maximum number of group registers we keep */
#define RE_GROUP_REG_CNT  10

/* maximum group nesting depth */
#define RE_GROUP_NESTING_MAX  20

/* 
 *   the maximum number of separate loop variables we need is the same as
 *   the group nesting level, since we only need one loop variable per
 *   nested group 
 */
#define RE_LOOP_VARS_MAX  RE_GROUP_NESTING_MAX



/* ------------------------------------------------------------------------ */
/*
 *   Recognizer types. 
 */

enum re_recog_type
{
    /* invalid/uninitialized */
    RE_INVALID,

    /* literal character recognizer */
    RE_LITERAL,

    /* "epsilon" recognizer - match without consuming anything */
    RE_EPSILON,

    /* wildcard character */
    RE_WILDCARD,

    /* beginning and end of text */
    RE_TEXT_BEGIN,
    RE_TEXT_END,

    /* start and end of a word */
    RE_WORD_BEGIN,
    RE_WORD_END,

    /* word-char and non-word-char */
    RE_WORD_CHAR,
    RE_NON_WORD_CHAR,

    /* word-boundary and non-word-boundary */
    RE_WORD_BOUNDARY,
    RE_NON_WORD_BOUNDARY,

    /* a character range/exclusion range */
    RE_RANGE,
    RE_RANGE_EXCL,

    /* group entry/exit transition */
    RE_GROUP_ENTER,
    RE_GROUP_EXIT,

    /* 
     *   group matcher - the character code has the group number (0 for
     *   group 0, etc) rather than a literal to match 
     */
    RE_GROUP_MATCH,

    /* any alphabetic character */
    RE_ALPHA,

    /* any digit */
    RE_DIGIT,

    /* any upper-case alphabetic */
    RE_UPPER,

    /* any lower-case alphabetic */
    RE_LOWER,

    /* any alphanumeric */
    RE_ALPHANUM,

    /* space character */
    RE_SPACE,

    /* punctuation character */
    RE_PUNCT,

    /* newline character */
    RE_NEWLINE,

    /* null character (used in range recognizers) */
    RE_NULLCHAR,

    /* positive assertion */
    RE_ASSERT_POS,

    /* negative assertion */
    RE_ASSERT_NEG,

    /* loop entry: zero the associated loop variable */
    RE_ZERO_VAR,

    /* loop branch: inspect loop criteria and branch accordingly */
    RE_LOOP_BRANCH
};


/* ------------------------------------------------------------------------ */
/* 
 *   Denormalized state transition tuple.  Each tuple represents the
 *   complete set of transitions out of a particular state.  A particular
 *   state can have one character transition, or two epsilon transitions.
 *   Note that we don't need to store the state ID of the tuple itself in
 *   the tuple, because the state ID is the index of the tuple in an array
 *   of state tuples.  
 */
struct re_tuple
{
    /* recognizer type */
    re_recog_type typ;

    /* the character we must match to transition to the target state */
    union
    {
        /* 
         *   if this is a character transition, this is the character (used
         *   as the character literal in RE_LITERAL, and as the group ID in
         *   RE_GROUP_MATCH and in RE_EPSILON nodes with the group flag set) 
         */
        wchar_t ch;

        /* 
         *   if this has a sub-machine, this is the start and end info (used
         *   for RE_ASSERT_POS, RE_ASSERT_NEG) 
         */
        struct
        {
            re_state_id init;
            re_state_id final;
        } sub;

        /* 
         *   if this is a loop, the loop parameters (used for RE_ZERO_VAR,
         *   RE_LOOP_BRANCH) 
         */
        struct
        {
            int loop_min;
            int loop_max;
            int loop_var;
        } loop;

        /* 
         *   Character range match table - this is used if the recognizer
         *   type is RE_RANGE or RE_RANGE_EXCL; for other recognizer types,
         *   this is not used.
         *   
         *   If used, this is an array of pairs of characters.  In each pair,
         *   the first is the low end of the range, and the second is the
         *   high end of the range, both ends inclusive.  A single character
         *   takes up two entries, both identical, to specify a range of only
         *   one character.
         *   
         *   If the first character is '\0', then neither wchar_t is a
         *   character in the ordinary sense described above.  Instead, the
         *   second wchar_t is actually one of the recognizer type codes
         *   (re_recog_type) for a character class (RE_ALPHA, RE_DIGIT, etc).
         *   The pair in this case is to be taken to match (or exclude) the
         *   entire class.
         *   
         *   To represent a match for '\0', use '\0' for the first wchar_t
         *   and RE_NULLCHAR for the second wchar_t.  Note that the special
         *   meaning of '\0' in the first character of a pair makes it
         *   impossible to represent a range including a null byte with a
         *   single pair; instead, representing a range like [\000-\017]
         *   requires two pairs: the first pair is ('\0', RE_NULLCHAR), and
         *   the second pair is ('\001', '\017').  
         */
        struct
        {
            wchar_t *char_range;
            size_t char_range_cnt;
        } range;

    } info;

    /* the target states */
    re_state_id next_state_1;
    re_state_id next_state_2;

    /* flags */
    unsigned char flags;
};


/*
 *   Tuple flags 
 */

/* this state is being tested for a cycle */
#define RE_STATE_CYCLE_TEST   0x08

/* 
 *   for branching states: take the shortest, rather than longest, branch
 *   when both branches are successful 
 */
#define RE_STATE_SHORTEST     0x10


/* ------------------------------------------------------------------------ */
/*
 *   A "machine" description.  A machines is fully described by its initial
 *   and final state ID's.  
 */
struct re_machine
{
    /* the machine's initial state */
    re_state_id init;

    /* the machine's final state */
    re_state_id final;
};


/* ------------------------------------------------------------------------ */
/*
 *   Compiled pattern description.  This is not a complete compiled pattern,
 *   since the tuple array is separate; this is just a description of the
 *   compiled pattern that can be combined with the tuple array to form a
 *   full compiled pattern.
 */
struct re_compiled_pattern_base
{
    /* the pattern's machine description */
    re_machine machine;

    /* the number of tuples in the tuple array */
    re_state_id tuple_cnt;

    /* number of capturing groups in the expression */
    int group_cnt;

    /* maximum number of looping variables in the expression */
    int loop_var_cnt;

    /*
     *   <Case> or <NoCase> mode.  If this flag is clear, the search is not
     *   case-sensitive, so alphabetic characters in the pattern are matched
     *   without regard to case.  
     */
    int case_sensitive : 1;

    /* 
     *   <MIN> or <MAX> match mode -- if this flag is set, we match the
     *   longest string in case of ambiguity; otherwise we match the
     *   shortest.  
     */
    int longest_match : 1;

    /* 
     *   <FirstEnd> or <FirstBeg> match mode -- if this flag is set, we
     *   match (in a search) the string that starts first in case of
     *   ambiguity; otherwise, we match the string that ends first 
     */
    int first_begin : 1;
};

/*
 *   Compiled pattern object.  This is a pattern compiled and saved for use
 *   in searches and matches.  This is a compiled pattern description
 *   coupled with its tuple array, which in combination provide a complete
 *   compiled pattern.  
 */
struct re_compiled_pattern: re_compiled_pattern_base
{
    /* 
     *   the tuple array (the structure is overallocated to make room for
     *   tuple_cnt entries in this array) 
     */
    re_tuple tuples[1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Status codes 
 */
typedef enum
{
    /* success */
    RE_STATUS_SUCCESS = 0,

    /* compilation error - group nesting too deep */
    RE_STATUS_GROUP_NESTING_TOO_DEEP

} re_status_t;


/* ------------------------------------------------------------------------ */
/*
 *   Regular expression compilation context structure.  This tracks the
 *   state of the compilation and stores the resources associated with the
 *   compiled expression.  
 */
class CRegexParser
{
    friend class CRegexSearcher;
    friend class CRegexSearcherSimple;
        
public:
    /* initialize */
    CRegexParser();

    /* delete */
    ~CRegexParser();

    /* 
     *   Compile an expression and create a compiled pattern object, filling
     *   in *pattern with a pointer to the newly-allocated pattern object.
     *   The caller is responsible for freeing the pattern by calling
     *   free_pattern(pattern).  
     */
    re_status_t compile_pattern(const char *expr_str, size_t exprlen,
                                re_compiled_pattern **pattern);

    /* free a pattern previously created with compile_pattern() */
    static void free_pattern(re_compiled_pattern *pattern);

    /* set the default case sensitivity */
    void set_default_case_sensitive(int f) { default_case_sensitive_ = f; }

protected:
    /* reset the parser */
    void reset();

    /* allocate a new state ID */
    re_state_id alloc_state();

    /* set a transition from a state to a given destination state */
    void set_trans(re_state_id id, re_state_id dest_id,
                   re_recog_type typ, wchar_t ch);

    /* initialize a new machine, setting up the initial and final state */
    void init_machine(struct re_machine *machine);

    /* build a character recognizer */
    void build_char(struct re_machine *machine, wchar_t ch);

    /* build a special recognizer */
    void build_special(struct re_machine *machine,
                       re_recog_type typ, wchar_t ch);

    /* build a character range recognizer */
    void build_char_range(struct re_machine *machine, int exclusion);

    /* build a group recognizer */
    void build_group_matcher(struct re_machine *machine, int group_num);

    /* build a concatenation recognizer */
    void build_concat(struct re_machine *new_machine,
                      struct re_machine *lhs, struct re_machine *rhs);

    /* build a group machine */
    void build_group(struct re_machine *new_machine,
                     struct re_machine *sub_machine, int group_id);

    /* build a positive or negative assertion machine */
    void build_assert(struct re_machine *new_machine,
                      struct re_machine *sub_machine, int is_negative);

    /* build an alternation recognizer */
    void build_alter(struct re_machine *new_machine,
                     struct re_machine *lhs, struct re_machine *rhs);

    /* build a closure recognizer */
    void build_closure(struct re_machine *new_machine,
                       struct re_machine *sub, wchar_t specifier,
                       int shortest);

    /* build an interval matcher */
    void build_interval(struct re_machine *new_machine,
                        struct re_machine *sub, int min_val, int max_val,
                        int var_id, int shortest);

    /* build a null machine */
    void build_null_machine(struct re_machine *machine);

    /* determine if a machine is null */
    int is_machine_null(struct re_machine *machine);

    /* concate the second machine onto the first machine */
    void concat_onto(struct re_machine *dest, struct re_machine *rhs);

    /* alternate the second machine onto the first */
    void alternate_onto(struct re_machine *dest, struct re_machine *rhs);

    /* compile an expression */
    re_status_t compile(const char *expr_str, size_t exprlen,
                        re_compiled_pattern_base *pat);

    /* compile a character class or class range expression */
    int compile_char_class_expr(utf8_ptr *expr, size_t *exprlen,
                                re_machine *result_machine);

    /* parse an integer value */
    int parse_int(utf8_ptr *p, size_t *rem);

    /* add a character to our range buffer */
    void add_range_char(wchar_t ch) { add_range_char(ch, ch); }
    void add_range_char(wchar_t ch_lo, wchar_t ch_hi);

    /* add a character class to our range buffer */
    void add_range_class(re_recog_type cl);

    /* ensure space in the range buffer for another entry */
    void ensure_range_buf_space();

    /* break any infinite loops in the machine */
    void break_loops(re_machine *machine);

    /* find an infinite loop back to the given state */
    int find_loop(re_machine *machine, re_state_id cur_state);

    /* optimize away meaningless branch-to-branch transitions */
    void remove_branch_to_branch(re_machine *machine);
    void optimize_transition(const re_machine *machine, re_state_id *trans);

    /* next available state ID */
    re_state_id next_state_;

    /*
     *   The array of transition tuples.  We'll allocate this array and
     *   expand it as necessary.  
     */
    re_tuple *tuple_arr_;

    /* number of transition tuples allocated in the array */
    int tuples_alloc_;

    /* buffer for building range exprssions */
    wchar_t *range_buf_;

    /* current number of entries in range buffer */
    size_t range_buf_cnt_;

    /* maximum number of entries in range buffer */
    size_t range_buf_max_;

    /* are our expressions case-sensitive by default? */
    int default_case_sensitive_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Pattern recognizer state stack.  Each time we need to process a
 *   sub-state (a two-way epsilon, or a nested assertion), we stack the
 *   current state so that we can backtrack when we're done with the
 *   sub-expression.  The state we store consists of:
 *   
 *   - backtrack type - this is an arbitrary uchar identifier that the
 *   pattern matcher uses to identify where to go when we pop the state
 *   
 *   - the current state ID
 *   
 *   - the current offset in the string being matched
 *   
 *   - the state ID of the terminating state of the machine
 *   
 *   - saved group registers; we only store the ones we've actually
 *   modified, to avoid unnecessary copying
 *   
 *   - saved loop variables; we only store the ones we've actually modified,
 *   to avoid unnecessary copying 
 */

/* the base structure for a stacked state */
struct regex_stack_entry
{
    /* the backtrack type identifier */
    short typ;

    /* the starting offset in the string */
    int start_ofs;

    /* the current offset in the string */
    int str_ofs;

    /* the pattern state */
    re_state_id state;

    /* the final state of the machine */
    re_state_id final;

    /* the return value for this state */
    int retval;

    /* stack offset of previous frame */
    int prv_sp;
};

/* saved group/loop entry */
struct regex_stack_var
{
    /* 
     *   The ID - this is in the range 0..RE_GROUP_REG_CNT-1 for group
     *   registers, RE_GROUP_REG_CNT..RE_GROUP_REG_CNT+RE_LOOP_VARS_MAX-1
     *   for loop variables.  In other words, a loop variable is identified
     *   by its loop variable number plus RE_GROUP_REG_CNT.  The special ID
     *   value -1 indicates the integer 'retval' value (a saved return value
     *   for the stack state).  
     */
    int id;

    /* the value */
    union
    {
        re_group_register group;
        short loopvar;
        int retval;
    } val;
};

/* state stack class */
class CRegexStack
{
public:
    CRegexStack()
    {
        /* allocate the initial state buffer */
        bufsiz_ = 8192;
        buf_ = (char *)t3malloc(bufsiz_);

        /* we don't have anything on the stack yet */
        sp_ = -1;
        used_ = 0;
    }

    ~CRegexStack()
    {
        /* delete the stack buffer */
        t3free(buf_);
    }

    /* reset the stack */
    void reset()
    {
        /* empty the stack */
        sp_ = -1;
        used_ = 0;
    }

    /* push a new state */
    void push(ushort typ, int start_ofs, int str_ofs,
              re_state_id state, re_state_id final)
    {
        regex_stack_entry *fp;

        /* 
         *   Ensure we have enough space for the base state structure plus a
         *   full complement of group registers, loop variables, and return
         *   value.  We might not actually need all of the registers and
         *   loop variables, so we won't commit all of this space yet, but
         *   check in advance to make sure we have it so that we don't have
         *   to check again when and if we get around to consuming
         *   group/loop slots.  
         */
        ensure_space(sizeof(regex_stack_entry)
                     + ((RE_GROUP_REG_CNT + RE_LOOP_VARS_MAX + 1)
                        *sizeof(regex_stack_var)));

        /* allocate the base stack frame */
        fp = (regex_stack_entry *)alloc_space(sizeof(regex_stack_entry));

        /* set it up */
        fp->typ = typ;
        fp->start_ofs = start_ofs;
        fp->str_ofs = str_ofs;
        fp->state = state;
        fp->final = final;

        /* push it onto the stack */
        fp->prv_sp = sp_;
        sp_ = (char *)fp - buf_;
    }

    /* save a group register */
    void save_group_reg(int id, const re_group_register *regs)
    {
        regex_stack_var *var;

        /* allocate a new slot if needed and save the value */
        if (sp_ != -1 && (var = new_reg_or_var(id)) != 0)
            var->val.group = regs[id];
    }

    /* save a loop variable */
    void save_loop_var(int id, const short *loop_vars)
    {
        regex_stack_var *var;

        /* 
         *   allocate a new slot if needed and save the value; note that
         *   loop variables are identified by the loop variable ID plus the
         *   base index RE_GROUP_REG_CNT 
         */
        if (sp_ != -1 && (var = new_reg_or_var(id + RE_GROUP_REG_CNT)) != 0)
            var->val.loopvar = loop_vars[id];
    }

    /* 
     *   get the type of the state at top of stack; if there is no state,
     *   returns -1 
     */
    int get_top_type()
    {
        /* 
         *   if there's nothing on the stack, so indicate, otherwise get the
         *   type from the top stack element 
         */
        if (sp_ == -1)
            return -1;
        else
            return ((regex_stack_entry *)(buf_ + sp_))->typ;
    }

    /* get the stack frame at the given depth (0 is top of stack) */
    regex_stack_entry *get_frame(int depth)
    {
        regex_stack_entry *fp;

        /* traverse the given number of frames from the top of the stack */
        for (fp = (regex_stack_entry *)(buf_ + sp_) ; depth != 0 ; --depth)
            fp = (regex_stack_entry *)(buf_ + fp->prv_sp);

        /* return the frame pointer */
        return fp;
    }

    /* pop a state */
    void pop(int *start_ofs, int *str_ofs,
             re_state_id *state, re_state_id *final,
             re_group_register *regs, short *loop_vars)
    {
        regex_stack_entry *fp;
        regex_stack_var *var;

        /* get the stack pointer */
        fp = (regex_stack_entry *)(buf_ + sp_);

        /* restore the string offset and state ID */
        *start_ofs = fp->start_ofs;
        *str_ofs = fp->str_ofs;
        *state = fp->state;
        *final = fp->final;
        
        /* run through the saved registers/variables in the state */
        for (var = (regex_stack_var *)(fp + 1) ;
             var < (regex_stack_var *)(buf_ + used_) ; ++var)
        {
            /* sense the type */
            if (var->id < RE_GROUP_REG_CNT)
            {
                /* it's a group register */
                regs[var->id] = var->val.group;
            }
            else
            {
                /* it's a loop variable */
                loop_vars[var->id - RE_GROUP_REG_CNT] = var->val.loopvar;
            }
        }

        /* we're done with the stop stack frame, so discard it */
        discard();
    }

    /* discard the top stack state */
    void discard()
    {
        regex_stack_entry *fp;

        /* get the stack pointer */
        fp = (regex_stack_entry *)(buf_ + sp_);

        /* unwind the stack */
        used_ = (size_t)sp_;
        sp_ = fp->prv_sp;
    }

    /* 
     *   Save the current state at the top of the stack and push a new
     *   state.  This is used to traverse the second branch of a two-branch
     *   epsilon: we first have to save the results of the first branch,
     *   including the return value and its registers, and we then have to
     *   restore the initial register/loop state as it was before the first
     *   branch.
     *   
     *   We save the final state and restore the initial state by swapping
     *   the group registers in the saved state with those in the current
     *   state.  This brings back the initial conditions to the current
     *   machine state, while saving everything that's changed in the
     *   current machine state in the stack frame.  We'll likewise swap the
     *   machine state and string offset.  Later, this same final machine
     *   state can be restored by first restoring the machine state to the
     *   initial state, then popping this frame.
     *   
     *   On return, the stack frame that was active on entry will be set to
     *   contain the current machine state, and the current machine state
     *   will be replaced with what was in that stack frame.  In addition,
     *   we'll have pushed a new stack frame for the new current machine
     *   state.  
     */
    void save_and_push(int retval,
                       ushort typ, int *start_ofs, int *str_ofs,
                       re_state_id *state, re_state_id *final,
                       re_group_register *regs, short *loop_vars)
    {
        regex_stack_entry *fp;
        regex_stack_var *var;
        int tmp_ofs;
        re_state_id tmp_id;

        /* get the stack pointer */
        fp = (regex_stack_entry *)(buf_ + sp_);

        /* swap the string offset */
        tmp_ofs = *str_ofs;
        *str_ofs = fp->str_ofs;
        fp->str_ofs = tmp_ofs;

        /* swap the starting offset */
        tmp_ofs = *start_ofs;
        *start_ofs = fp->start_ofs;
        fp->start_ofs = tmp_ofs;

        /* swap the current machine state */
        tmp_id = *state;
        *state = fp->state;
        fp->state = tmp_id;

        /* swap the final machine state */
        tmp_id = *final;
        *final = fp->final;
        fp->final = tmp_id;

        /* swap all group and loop registers with the current state */
        for (var = (regex_stack_var *)(fp + 1) ;
             var < (regex_stack_var *)(buf_ + used_) ; ++var)
        {
            /* sense the type */
            if (var->id < RE_GROUP_REG_CNT)
            {
                re_group_register tmp;
                
                /* it's a group register */
                tmp = regs[var->id];
                regs[var->id] = var->val.group;
                var->val.group = tmp;
            }
            else
            {
                short tmp;
                
                /* it's a loop variable */
                tmp = loop_vars[var->id - RE_GROUP_REG_CNT];
                loop_vars[var->id - RE_GROUP_REG_CNT] = var->val.loopvar;
                var->val.loopvar = tmp;
            }
        }

        /* save the return value from the outgoing state */
        fp->retval = retval;

        /* push a copy of the restored state */
        push(typ, *start_ofs, *str_ofs, *state, *final);
    }

protected:
    /* 
     *   allocate a new register or group variable in the stack frame; if we
     *   find an existing copy of the same variable, we'll return null to
     *   indicate that we don't have to save it again 
     */
    regex_stack_var *new_reg_or_var(int id)
    {
        regex_stack_entry *fp;
        regex_stack_var *var;

        /* get the stack pointer */
        fp = (regex_stack_entry *)(buf_ + sp_);

        /* scan the frame for a register/variable with this ID */
        for (var = (regex_stack_var *)(fp + 1) ;
             var < (regex_stack_var *)(buf_ + used_) ; ++var)
        {
            /* if this is the one, we don't need to save it again */
            if (var->id == id)
                return 0;
        }

        /* we didn't find it, so return a new entry with the given ID */
        var = (regex_stack_var *)alloc_space(sizeof(regex_stack_var));
        var->id = id;
        return var;
    }
    
    /* ensure space in our stack buffer */
    void ensure_space(size_t siz)
    {
        /* if it's within range, we're fine */
        if (used_ + siz <= bufsiz_)
            return;

        /* expand */
        bufsiz_ += 8192;

        /* if it's too large, throw an error */
        if (bufsiz_ > OSMALMAX)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* reallocate at the new size */
        buf_ = (char *)t3realloc(buf_, bufsiz_);

        /* make sure we're not out of memory */
        if (buf_ == 0)
            err_throw(VMERR_OUT_OF_MEMORY);
    }

    /* 
     *   allocate space - the caller must have already checked that space is
     *   available 
     */
    char *alloc_space(size_t siz)
    {
        char *ret;

        /* figure out where the new object goes */
        ret = buf_ + used_;

        /* consume the space */
        used_ += siz;

        /* return the allocated space */
        return ret;
    }

    /* the stack buffer */
    char *buf_;
    size_t bufsiz_;

    /* offset of current stack frame */
    int sp_;

    /* number of bytes used so far */
    size_t used_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Regular Expression Searcher/Matcher.  This object encapsulates the
 *   group registers associated with a search.  
 */
class CRegexSearcher
{
public:
    CRegexSearcher();
    ~CRegexSearcher();

    /*
     *   Search for a compiled pattern.  Returns the byte offset of the
     *   match, or -1 if no match was found.  *result_len is filled in with
     *   the byte length of the match if we found one.  Note that the
     *   returned index and result_len values are byte lengths, not
     *   character lengths.
     *   
     *   The caller is responsible for providing a set of group registers,
     *   which must be an array of registers of size RE_GROUP_REG_CNT.  The
     *   caller also must save the original search string if it will be
     *   necessary to extract substrings based on the group registers.  
     */
    int search_for_pattern(const re_compiled_pattern *pattern,
                           const char *entirestr,
                           const char *searchstr, size_t searchlen,
                           int *result_len, re_group_register *regs);

    /*
     *   Check for a match to a previously compiled expression.  Returns the
     *   length of the match if we found a match, -1 if we found no match.
     *   This is not a search function; we merely match the leading
     *   substring of the given string to the given pattern.  Note that the
     *   returned length is a byte length, not a character length.  
     *   
     *   The caller is responsible for providing a set of group registers,
     *   which must be an array of registers of size RE_GROUP_REG_CNT.  The
     *   caller also must save the original search string if it will be
     *   necessary to extract substrings based on the group registers.  
     */
    int match_pattern(const re_compiled_pattern *pattern,
                      const char *entirestr,
                      const char *searchstr, size_t searchlen,
                      re_group_register *regs);

protected:
    /* match a string to a compiled expression */
    int match(const char *entire_str,
              const char *str, size_t origlen,
              const re_compiled_pattern_base *pattern,
              const re_tuple *tuple_arr,
              const struct re_machine *machine,
              re_group_register *regs, short *loop_vars);

    /* search for a regular expression within a string */
    int search(const char *entire_str,
               const char *str, size_t len,
               const re_compiled_pattern_base *pattern,
               const re_tuple *tuple_arr,
               const struct re_machine *machine,
               re_group_register *regs, int *result_len);

    /* clear a set of group registers */
    void clear_group_regs(re_group_register *regs)
    {
        int i;
        re_group_register *r;

        /* set the start and end offsets for all registers to -1 */
        for (r = regs, i = 0 ; i < RE_GROUP_REG_CNT ; ++i, ++r)
            r->start_ofs = r->end_ofs = -1;
    }

    /*
     *   Determine if a character is part of a word.  We consider letters
     *   and numbers to be word characters.  
     */
    static int is_word_char(wchar_t c)
    {
        return (t3_is_alpha(c) || t3_is_digit(c));
    }

    /* match state stack */
    CRegexStack stack_;
};

/*
 *   Simplified Searcher - this class provides some high-level methods that
 *   simplify one-off searches that combine compilation and searching into
 *   one step. 
 */
class CRegexSearcherSimple: public CRegexSearcher
{
public:
    CRegexSearcherSimple(class CRegexParser *parser)
    {
        /* remember my parser */
        parser_ = parser;
    }

    ~CRegexSearcherSimple()
    {
    }

    /* search for a pattern, using our internal group registers */
    int search_for_pattern(const re_compiled_pattern *pattern,
                           const char *entirestr,
                           const char *searchstr, size_t searchlen,
                           int *result_len)
    {
        /* remember the group count from the compiled pattern */
        group_cnt_ = pattern->group_cnt;

        /* clear the group registers */
        clear_group_regs(regs_);

        /* search for the compiled pattern using our group register */
        return CRegexSearcher::search_for_pattern(
            pattern, entirestr, searchstr, searchlen, result_len, regs_);
    }

    /* match a pattern, using our internal group registers */
    int match_pattern(const re_compiled_pattern *pattern,
                      const char *entirestr,
                      const char *searchstr, size_t searchlen)
    {
        /* remember the group count from the compiled pattern */
        group_cnt_ = pattern->group_cnt;

        /* clear the group registers */
        clear_group_regs(regs_);

        /* search for the compiled pattern using our group register */
        return CRegexSearcher::match_pattern(
            pattern, entirestr, searchstr, searchlen, regs_);
    }

    /*
     *   Compile an expression and search for a match within the given
     *   string.  Returns the byte offset of the match, or -1 if no match
     *   was found.  *result_len is filled in with the byte length of the
     *   match if we found one.  Note that the returned index and result_len
     *   values are byte lengths, not character lengths.  
     */
    int compile_and_search(const char *pattern, size_t patlen,
                           const char *entirestr,
                           const char *searchstr, size_t searchlen,
                           int *result_len);

    /*
     *   Compile an expression and check for a match.  Returns the byte
     *   length of the match if we found a match, -1 if we found no match.
     *   This is not a search function; we merely match the leading
     *   substring of the given string to the given pattern.  Note that the
     *   returned length is a byte length, not a character length.  
     */
    int compile_and_match(const char *pattern, size_t patlen,
                          const char *entirestr,
                          const char *searchstr, size_t searchlen);

    /*
     *   Get a group register.  0 refers to the first group; groups are
     *   numbered in left-to-right order by their opening parenthesis.  
     */
    const re_group_register *get_group_reg(int i) const { return &regs_[i]; }

    /* get the number of groups in the last pattern we searched */
    int get_group_cnt() const { return group_cnt_; }

protected:
    /* group registers */
    re_group_register regs_[RE_GROUP_REG_CNT];

    /* number of groups in last pattern we searched */
    int group_cnt_;

    /* my regular expression parser */
    class CRegexParser *parser_;
};

#endif /* VMREGEX_H */

