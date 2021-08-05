/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmdbg.h - T3 VM debugging API
Function
  Provides an interface to debugging operations in the VM
Notes
  
Modified
  11/23/99 MJRoberts  - Creation
*/

#ifndef VMDBG_H
#define VMDBG_H

#include "vmglob.h"
#include "vmfunc.h"
#include "vmhash.h"
#include "vmobj.h"
#include "tcprstyp.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   Internal breakpoint tracking record 
 */
struct CVmDebugBp
{
    /* create */
    CVmDebugBp();

    /* delete */
    ~CVmDebugBp();

    /* notify of VM termination */
    void do_terminate(VMG0_);

    /* get/set the in-use flag */
    int is_in_use() const { return in_use_; }
    void set_in_use(int f) { in_use_ = f; }

    /* get/set the disabled flag */
    int is_disabled() const { return disabled_; }
    void set_disabled(int f) { disabled_ = f; }

    /* 
     *   set the breakpoint's information - returns zero on success,
     *   non-zero if an error occurs (such as compiling the condition
     *   expression) 
     */
    int set_info(VMG_ const uchar *code_addr,
                 const char *cond, int change,
                 int disabled, char *errbuf, size_t errbuflen);

    /* get my code address */
    const uchar *get_code_addr() const { return code_addr_; }

    /* 
     *   Set the condition text - returns zero on success, non-zero if an
     *   error occurs compiling the expression.  If 'change' is true, then we
     *   break when the condition changes; otherwise, we break when the
     *   condition becomes true.  
     */
    int set_condition(VMG_ const char *cond, int change,
                      char *errbuf, size_t errbuflen);

    /* 
     *   delete the breakpoint - remove the breakpoint from the code and
     *   mark it as unused 
     */
    void do_delete(VMG0_);

    /* 
     *   Set or remove the BP instruction at my code address.  If 'always'
     *   is set, we'll update the instruction regardless of whether the
     *   debugger has control or not; normally, we'll only update the
     *   instruction when the debugger doesn't have control, since we
     *   don't leave breakpoint instructions in the code at other times. 
     */
    void set_bp_instr(VMG_ int set, int always);

    /* check to see if I have a condition */
    int has_condition() const
        { return has_cond_ && compiled_cond_ != 0; }

    /* 
     *   Is our condition a stop-on-change condition?  This returns true if
     *   so, false if this we instead have a stop-when-true condition.  Note
     *   that this information is not meaningful unless has_condition()
     *   returns true.  
     */
    int stop_on_change() const { return stop_on_change_; }

    /* 
     *   evaluate the condition - return true if the condition evaluates
     *   to true (non-zero integer, true, or any object, string, or list
     *   value), false if not 
     */
    int eval_cond(VMG0_);

    /* 
     *   determine if this is a global breakpoint - a global breakpoint is
     *   one that is not associated with a code location (indicated by a
     *   code address of zero) 
     */
    int is_global() const { return code_addr_ == 0; }

private:
    /* code address of breakpoint */
    const uchar *code_addr_;

    /* condition expression */
    char *cond_;

    /* length of buffer allocated for cond, if any */
    size_t cond_buf_len_;

    /* code object with compiled expression */
    vm_globalvar_t *compiled_cond_;

    /* 
     *   The previous value of the condition.  If this is a stop-on-change
     *   condition, we'll stop as soon as the current value of the expression
     *   differs from this saved value.  If we have a condition, but we
     *   haven't yet computed the "old" value, this will have the 'empty'
     *   type.  
     */
    vm_globalvar_t *prv_val_;

    /* 
     *   original instruction byte at this breakpoint (we replace the
     *   instruction byte with the BP instruction, but we must remember
     *   the original for when we clear or disable the breakpoint) 
     */
    uchar orig_instr_;

    /* flag: breakpoint is in use */
    uint in_use_ : 1;

    /* flag: breakpoint has a conditional expression */
    uint has_cond_ : 1;

    /* flag: our condition is stop-on-change, not stop-when-true */
    uint stop_on_change_ : 1;

    /* flag: breakpoint is disabled */
    uint disabled_ : 1;

};

/* maximum number of breakpoints */
const size_t VMDBG_BP_MAX = 100;

/* ------------------------------------------------------------------------ */
/*
 *   Structure for saving debugger step modes.  This is used to save and
 *   restore the step modes before doing a recursive evaluation. 
 */
struct vmdbg_step_save_t
{
    int old_step;
    int old_step_in;
    int old_step_out;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger API object 
 */
class CVmDebug
{
    friend class CVmRun;
    friend struct CVmDebugBp;
    
public:
    CVmDebug(VMG0_);
    ~CVmDebug();

    /*
     *   Initialize.  The VM will call this after setting up all of the VM
     *   globals, but before loading the image file. 
     */
    void init(VMG_ const char *image_filename);

    /*
     *   Initialization phase 2 - this is called just after we finish
     *   loading the image file.  The debugger can perform any final
     *   set-up that can't be handled until after the image is loaded. 
     */
    void init_after_load(VMG0_);

    /*
     *   Terminate.  The VM will call this when shutting down, before
     *   deleting any of the globals.  
     */
    void terminate(VMG0_);

    /*
     *   Determine if the loaded program was compiled for debugging.
     *   We'll check to see if the image loader found a GSYM (global
     *   symbol table) block in the file.  
     */
    int image_has_debug_info(VMG0_) const;

    /* determine if the debugger has control */
    int is_in_debugger() const { return in_debugger_ != 0; }

    /*
     *   Get the break event object.  Routines that wait for OS_Event list
     *   should include this in their event lists, so that they unblock
     *   immediately if the user requests a manual debugger break. 
     */
    class OS_Event *get_break_event()
    {
        break_event_->add_ref();
        return break_event_;
    }


    /* -------------------------------------------------------------------- */
    /*
     *   Add an entry to a reverse-lookup hash table.  The image file
     *   loader must call this function for each object, function, and
     *   property symbol it loads from the debug records in an image file. 
     */
    void add_rev_sym(const char *sym, size_t sym_len,
                     tc_symtype_t sym_type, ulong sym_val);

    /*
     *   Look up a symbol given the type and identifier.  Returns a
     *   pointer to a null-terminated string giving the name of the
     *   symbol, or null if the given identifier doesn't have an
     *   associated symbol.  
     */
    const char *objid_to_sym(vm_obj_id_t objid) const
        { return find_rev_sym(obj_rev_table_, (ulong)objid); }
    
    const char *propid_to_sym(vm_prop_id_t propid) const
        { return find_rev_sym(prop_rev_table_, (ulong)propid); }

    const char *funcaddr_to_sym(pool_ofs_t func_addr) const
        { return find_rev_sym(func_rev_table_, (ulong)func_addr); }

    const char *funchdr_to_sym(VMG_ const uchar *func_addr, char *buf) const;

    const char *enum_to_sym(ulong enum_id) const
        { return find_rev_sym(enum_rev_table_, enum_id); }

    const char *bif_to_sym(uint setidx, uint funcidx) const
    {
        ulong idx = ((ulong)setidx << 16) | funcidx;
        return find_rev_sym(bif_rev_table_, idx);
    }

    /* 
     *   Given a symbol name, get the final modifying object.  If the symbol
     *   is a synthesized modified base object, we'll find the actual symbol
     *   that was used in the source code with 'modify.' 
     */
    const char *get_modifying_sym(const char *base_sym) const;


    /* -------------------------------------------------------------------- */
    /*
     *   Method header list 
     */

    /* allocate the method header list */
    void alloc_method_header_list(ulong cnt);

    /*
     *   Given a code pool address, find the method header containing the
     *   address.  This searches the method header list for the nearest
     *   method header whose address is less than the given address.  
     */
    const uchar *find_method_header(VMG_ const uchar *addr);

    /* get the number of method headers */
    ulong get_method_header_cnt() const { return method_hdr_cnt_; }

    /* set the given method header list entry */
    void set_method_header(ulong idx, ulong addr)
    {
        /* store the given entry */
        method_hdr_[idx] = addr;
    }

    /* get the given method header entry */
    ulong get_method_header(ulong idx) const { return method_hdr_[idx]; }


    /* -------------------------------------------------------------------- */
    /*
     *   Get information on the source location at a given stack level.
     *   If successful, fills in the filename pointer and line number and
     *   returns zero.  If no source information is available at the stack
     *   level, returns non-zero to indicate failure.
     *   
     *   Level 0 is the current stack level, 1 is the enclosing frame, and
     *   so on.  
     */
    int get_source_info(VMG_ const char **fname, unsigned long *linenum,
                        int level) const;


    /* 
     *   Enumerate local variables at the given stack level - 0 is the
     *   current stack level, 1 is the enclosing frame, and so on.  
     */
    void enum_locals(VMG_ void (*cbfunc)(void *, const char *, size_t),
                     void *cbctx, int level);

    /*
     *   Build a stack trace listing, invoking the callback for each level
     *   of the stack trace.  If 'source_info' is true, we'll include the
     *   source file name and line number for each point in the trace.  
     */
    void build_stack_listing(VMG_
                             void (*cbfunc)(void *ctx,
                                            const char *str, int strl),
                             void *cbctx, int source_info);
    

    /* -------------------------------------------------------------------- */
    /*
     *   Breakpoints 
     */

    /*
     *   Toggle a breakpoint at the given code location.  Returns zero on
     *   success, non-zero on failure.  If successful, fills in *bpnum
     *   with the breakpoint ID, and fills in *did_set with true if we set
     *   a breakpoint, false if we deleted an existing breakpoint at the
     *   location.  cond is null if the breakpoint is unconditional, or a
     *   pointer to a character string giving the source code for an
     *   expression that must evaluate to true when the breakpoint is
     *   encountered for the breakpoint to suspend execution.  
     */
    int toggle_breakpoint(VMG_ const uchar *code_addr,
                          const char *cond, int change,
                          int *bpnum, int *did_set,
                          char *errbuf, size_t errbuflen);

    /* toggle the disabled status of a breakpoint */
    void toggle_breakpoint_disable(VMG_ int bpnum);

    /* set a breakpoint's disabled status */
    void set_breakpoint_disable(VMG_ int bpnum, int disable);

    /* determine if a breakpoint is disabled - returns true if so */
    int is_breakpoint_disabled(VMG_ int bpnum);

    /* set a breakpoint's condition expression - returns 0 on success */
    int set_breakpoint_condition(VMG_ int bpnum, const char *cond, int change,
                                 char *errbuf, size_t errbuflen);

    /* delete a breakpoint given the breakpoint ID */
    void delete_breakpoint(VMG_ int bpnum);

    /* -------------------------------------------------------------------- */
    /*
     *   Set execution mode.  These calls do not immediately resume
     *   execution, but merely set the mode that will be in effect when
     *   execution resumes.  These calls can only be made while the
     *   program is stopped.  
     */
    
    /*
     *   Set single-step mode to STEP IN.  In this mode, we will stop as
     *   soon as we reach a new statement.
     */
    void set_step_in()
    {
        /* set single-step and step-in modes */
        single_step_ = TRUE;
        step_in_ = TRUE;
        step_out_ = FALSE;

        /* we're in run mode again */
        break_event_->reset();
    }

    /*
     *   Set DEBUG TRACE mode.  This activates single-step mode from within a
     *   VM intrinsic. 
     */
    void set_debug_trace()
    {
        /* 
         *   set single-step mode so that we stop as soon as we're back in
         *   byte code 
         */
        set_step_in();

        /* 
         *   clear any current statement location, so that we stop
         *   immediately as soon as we get back to byte code, even if our
         *   last debugger entry was in the same line of code 
         */
        cur_stm_start_ = cur_stm_end_ = 0;
    }

    /* 
     *   Set single-step mode for a 'break' key (Ctrl-C, Ctrl+Break, etc.,
     *   according to local conventions).  This asynchronously interrupts the
     *   interpreter and breaks into the debugger while the program is
     *   running, which is useful to break out of infinite loops or very
     *   lengthy processing.  
     */
    void set_break_stop()
    {
        /* set single-step-in mode */
        set_step_in();

        /* break out of any event wait */
        break_event_->signal();

        /* 
         *   Forget our last execution location, so that we'll stop again
         *   even if we haven't moved from our last break location.  Since
         *   we're explicitly breaking execution, we want to stop whether
         *   we've gone anywhere or not.  
         */
        cur_stm_start_ = cur_stm_end_ = 0;
    }

    /*
     *   Set single-step mode to STEP OVER.  In this mode, we will stop as
     *   soon as we reach a new statement at the same stack level as the
     *   current statement or at an enclosing stack level. 
     */
    void set_step_over(VMG0_);

    /*
     *   Set single-step mode to STEP OUT.  In this mode, we will stop as
     *   soon as we reach a new statement at a stack level enclosing the
     *   current statement's stack level. 
     */
    void set_step_out(VMG0_);

    /*
     *   Set single-step mode to GO.  In this mode, we won't stop until we
     *   reach a breakpoint.
     */
    void set_go()
    {
        /* clear single-step mode */
        single_step_ = FALSE;
        step_in_ = FALSE;
        step_out_ = FALSE;
        break_event_->reset();
    }

    /*
     *   Save the step mode in preparation for a recursive execution (of a
     *   debugger expression), and set the mode for the recursive
     *   execution.  Turns off stepping modes. 
     */
    void prepare_for_eval(vmdbg_step_save_t *info)
    {
        /* save the original execution mode */
        info->old_step = single_step_;
        info->old_step_in = step_in_;
        info->old_step_out = step_out_;

        /* set execution mode to RUN */
        single_step_ = FALSE;
        step_in_ = FALSE;
        step_out_ = FALSE;
    }

    /* restore original execution modes after recursive execution */
    void restore_from_eval(vmdbg_step_save_t *info)
    {
        single_step_ = info->old_step;
        step_in_ = info->old_step_in;
        step_out_ = info->old_step_out;
    }

    /* 
     *   format a value, allocating space (the caller must free the space,
     *   using t3free) 
     */
    char *format_val(VMG_ const struct vm_val_t *val);

    /* format a value into a buffer */
    size_t format_val(VMG_ char *buf, size_t buflen,
                      const struct vm_val_t *val);

    /* format a value in the special __value# format */
    void format_special(VMG_ char *buf, size_t buflen,
                        const struct vm_val_t *val);


    /* -------------------------------------------------------------------- */
    /*
     *   Set the execution pointer to a new location.  The new code
     *   address is given as an absolute code pool address.  The new
     *   location must be within the current method.  Updates
     *   *exec_ofs_ptr with the method offset of the new location.  
     */
    int set_exec_ofs(const uchar **exec_ptr, const uchar *code_addr)
    {
        /* set the method offset pointer */
        *exec_ptr = code_addr;

        /* success */
        return 0;
    }

    /*
     *   Determine if a code location is within the current active method.
     *   Returns true if so, false if not. 
     */
    int is_in_current_method(VMG_ const uchar *code_addr);

    /* -------------------------------------------------------------------- */
    /*
     *   Evaluate an expression.  Returns zero on success, non-zero on
     *   failure.  'level' is the stack level at which to evaluate the
     *   expression - 0 is the currently active method, 1 is its caller,
     *   and so on.  
     */
    int eval_expr(VMG_ char *result, size_t result_len, const char *expr,
                  int level, int *is_lval, int *is_openable,
                  void (*aggcb)(void *, const char *, int, const char *),
                  void *aggctx, int speculative);

    /*
     *   Compile an expression.  Fills in *code_obj with a handle to the
     *   code pool object containing the compiled byte-code.  If dst_buf
     *   is non-null, we will format a message describing any error that
     *   occurs into the buffer.
     *   
     *   We'll compile the expression using the given local symbol table
     *   and the given active stack level.  Note that the local symbol
     *   table specified need not be the local symbol table for the given
     *   active stack level, since we're only compiling, not evaluating,
     *   the expression; for example, when compiling a condition
     *   expression for a breakpoint, we'll normally compile the
     *   expression in the context of the breakpoint's source location,
     *   which might not even be in the active stack when the condition is
     *   compiled.  
     */
    void compile_expr(VMG_ const char *expr,
                      int level, class CVmDbgSymtab *local_symtab,
                      int self_valid, int speculative, int *is_lval,
                      vm_obj_id_t *code_obj,
                      struct CVmDynCompResults *results);

    /* -------------------------------------------------------------------- */
    /*
     *   Get/set the UI context.  This information is for use by the UI
     *   code.  We don't use the UI context for anything; we just maintain
     *   it so that the UI can keep track of what's going on between
     *   calls. 
     */
    void *get_ui_ctx() const { return ui_ctx_; }
    void set_ui_ctx(void *ctx) { ui_ctx_ = ctx; }

protected:
    /* -------------------------------------------------------------------- */
    /*
     *   CVmRun API - the byte-code execution loop uses these routines 
     */
    
    /* 
     *   Step into the debugger - the byte-code execution loop calls this
     *   just before executing each instruction when the interpreter is in
     *   single-step mode, or when a breakpoint is encountered.  We'll
     *   activate the debugger user interface if necessary.  This returns
     *   in order to resume execution.  bp is true if we encountered a
     *   breakpoint, false if we're single-stepping.  
     */
    void step(VMG_ const uchar **pc_ptr, const uchar *entry, int bp,
              int error_code);

    /*
     *   Step through a return.  This doesn't actually stop in the debugger,
     *   but for the purposes of STEP OVER, notes that we're leaving a frame
     *   level.  This is unnecessary in most cases, since we'd do a step() at
     *   the next instruction in the caller anyway.  However, if the caller
     *   is native code that will turn around and call another byte-code
     *   routine, this ensures that we notice that we've left one byte-code
     *   routine and entered another.  If we didn't do this, and the next
     *   routine called had more arguments, we'd miss single-stepping into
     *   the next routine because we'd think we were at an enclosing level
     *   rather than a peer level.  
     */
    void step_return(VMG0_);

    /* 
     *   synchronize the internal execution point - acts like a step()
     *   without actually stopping 
     */
    void sync_exec_pos(VMG_ const uchar *pc_ptr, const uchar *entry);

    /* 
     *   Get single-step mode - returns true if we're stopping at each
     *   instruction, false if we're running until we hit a breakpoint.
     *   The byte-code execution loop should call step() on each
     *   instruction if this returns true.
     *   
     *   We must also single-step through code if there are any active
     *   global breakpoints, even when the user is not interactively
     *   single-stepping.  
     */
    int is_single_step() const
        { return single_step_ || global_bp_cnt_ != 0; }


    /* -------------------------------------------------------------------- */
    /*
     *   Internal operations 
     */

    /*
     *   Get information on the execution location at the given stack
     *   level 
     */
    int get_stack_level_info(VMG_ int level, class CVmFuncPtr *func_ptr,
                             class CVmDbgLinePtr *line_ptr,
                             const uchar **stm_start, const uchar **stm_end)
        const;

    /* look up a symbol in one of our reverse mapping tables */
    const char *find_rev_sym(
        const class CVmHashTable *hashtab, ulong val) const;

    /* find a breakpoint given a code address */
    CVmDebugBp *find_bp(const uchar *code_addr);

    /* allocate a new breakpoint record */
    CVmDebugBp *alloc_bp();

    /* 
     *   suspend/resume breakpoints - this doesn't affect the permanent
     *   status of any breakpoint, but simply allows for removal of BP
     *   instructions from the code while the debugger has control 
     */
    void suspend_all_bps(VMG0_);
    void restore_all_bps(VMG0_);

private:
    /* flag: debugger has control */
    unsigned int in_debugger_ : 1;
    
    /* 
     *   single-step mode - if this is true, we're stepping through code;
     *   otherwise, we're running until we hit a breakpoint 
     */
    unsigned int single_step_ : 1;

    /* 
     *   step-in mode - if this is true, and single_step_ is true, we'll
     *   break as soon as we reach a new statement anywhere; otherwise,
     *   we'll stop only if we reach a new statement at the same stack
     *   level as the current statement (in other words, we're stepping
     *   over subroutine calls made by the current statement) 
     */
    unsigned int step_in_ : 1 ;

    /* 
     *   Step-out mode - if this is true, and single_step_ is true, we'll
     *   break as soon as we reach a new statement outside the current
     *   call level.  (This mode flag is actually only important for
     *   native code interaction, because we actually know to stop on a
     *   step-out using step-over with an enclosing stack level.)  
     */
    unsigned int step_out_ : 1;

    /* 
     *   step-over-breakpoint mode - if this is true, we're stepping over
     *   a breakpoint 
     */
    unsigned int step_over_bp_ : 1;

    /* 
     *   Original step flags - these store the step flags that will be in
     *   effect after we finish a step_over_bp operation 
     */
    unsigned int orig_single_step_ : 1;
    unsigned int orig_step_in_ : 1;
    unsigned int orig_step_out_ : 1;

    /* 
     *   flag: we've been initialized during program load; when this flag is
     *   set, we'll have to make corresponding uninitializations when the
     *   program terminates 
     */
    unsigned int program_inited_ : 1;

    /* breakpoint being stepped over */
    CVmDebugBp *step_over_bp_bp_;

    /*
     *   Step-resume stack frame level.  When step_in_ is false, we won't
     *   resume stepping code until the frame pointer is at this level or
     *   an enclosing level.  
     */
    size_t step_frame_depth_;

    /* 
     *   Method header for the current function.  We set this each time we
     *   enter the debugger via step(). 
     */
    CVmFuncPtr func_ptr_;

    /*
     *   Debug records pointer for the current function.  We set this each
     *   time we enter the debugger via step().  Note that we must keep
     *   track of whether the debug pointer is valid, because some
     *   functions might not have debug tables at all.  
     */
    CVmDbgTablePtr dbg_ptr_;
    unsigned int dbg_ptr_valid_ : 1;

    /* function header pointer for current function */
    const uchar *entry_;

    /* current program counter */
    const uchar *pc_;

    /* debugger line records for current statement */
    CVmDbgLinePtr cur_stm_line_;

    /* 
     *   Current statement boundaries.  We set this each time we enter the
     *   debugger via step().  We usually step through code until we reach
     *   the beginning of a new statement; we use these boundaries to
     *   determine when we leave the current statement.  As long as the
     *   current byte-code offset is within these boundaries, (inclusive),
     *   we know we're within the same statement.  
     */
    const uchar *cur_stm_start_;
    const uchar *cur_stm_end_;

    /*
     *   User interface context.  We maintain this location for use by the
     *   UI code to store its context information. 
     */
    void *ui_ctx_;

    /*
     *   Reverse-mapping hash tables for various symbol types.  These hash
     *   tables allow us to find the symbol for a given object ID,
     *   property ID, or function address. 
     */
    class CVmHashTable *obj_rev_table_;
    class CVmHashTable *prop_rev_table_;
    class CVmHashTable *func_rev_table_;
    class CVmHashTable *enum_rev_table_;
    class CVmHashTable *bif_rev_table_;

    /* breakpoints */
    CVmDebugBp bp_[VMDBG_BP_MAX];

    /* 
     *   Number of global breakpoints in effect (when this is non-zero, we
     *   must trace through code even in 'go' mode, so we can evaluate the
     *   global breakpoints repeatedly and thereby catch when the first
     *   one hits).  This only counts enabled global breakpoints - if a
     *   global breakpoint is disabled it must be removed from this count,
     *   since we won't need to consider it when checking for hits.  
     */
    int global_bp_cnt_;

    /* host interface (for the compiler's use) */
    class CTcHostIfcDebug *hostifc_;

    /* 
     *   method header list - this is a list of the method headers in the
     *   program, sorted by address 
     */
    ulong *method_hdr_;
    ulong method_hdr_cnt_;

    /* 
     *   Debugger break event.  We'll signal this whenever the user tells us
     *   to break into the program.  Event wait routines (such as
     *   getNetEvent()) can include this in their wait list so that they're
     *   interrupted immediately on a debug break.  
     */
    class OS_Event *break_event_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugger programmatic interface to the user interface.  This is to be
 *   implemented by each UI subsystem.  
 */
class CVmDebugUI
{
public:
    /* 
     *   Initialize.  We'll call this once before entering any other
     *   functions, so that the UI code can set up its private
     *   information.  The UI can create a private context and store a
     *   pointer via G_debugger->set_ui_ctx().  
     */
    static void init(VMG_ const char *image_filename);

    /*
     *   Initialization, phase 2 - this is called just after we've
     *   finished loading the image file.
     */
    static void init_after_load(VMG0_);

    /*
     *   Terminate.  We'll call this before terminating, to allow the UI
     *   code to release any resources it allocated. 
     */
    static void terminate(VMG0_);
    
    /* 
     *   Invoke the UI main command loop entrypoint.  The engine calls
     *   this whenever we hit a breakpoint or reach a single-step point.
     *   This routine should not return until it is ready to let the
     *   program continue execution. 
     */
    static void cmd_loop(VMG_ int bp_number, int error_code, const uchar **pc);
};


/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for reverse-mapping hash tables.  These tables allow
 *   the debugger to look up a symbol name given the symbol's value: an
 *   object ID, a property ID, or a function address.  We map each of
 *   these types of values to a ulong value, which we use as the hash key.
 */
class CVmHashEntryDbgRev: public CVmHashEntry
{
public:
    /*
     *   Construct the entry.  sym_val is the symbol's value (an object
     *   ID, a property ID, or a function address) coerced to a ulong.
     */
    CVmHashEntryDbgRev(ulong sym_val, const char *sym, size_t len);
    ~CVmHashEntryDbgRev();

    /* check for a match */
    virtual int matches(const char *str, size_t len) const;

    /* get the symbol name for this entry */
    const char *get_sym() const { return sym_; }
    size_t get_sym_len() const { return sym_len_; }

protected:
    char *sym_;
    size_t sym_len_;
};

/*
 *   Hash function for reverse mapping tables.  This hash function doesn't
 *   make any assumptions about the range of character values, since we're
 *   using sequences of raw binary bytes for hash keys.  
 */
class CVmHashFuncDbgRev: public CVmHashFunc
{
public:
    unsigned int compute_hash(const char *str, size_t len) const;
};


/* ------------------------------------------------------------------------ */
/*
 *   Condition compilation macros.  Some files can be compiled for
 *   stand-alone use or use within a debugger application; when compiled
 *   stand-alone, certain debugger-related operations are removed, which
 *   can improve performance over the debugger-enabled version. 
 */
#ifdef VM_DEBUGGER
/*
 *   DEBUGGER-ENABLED VERSION 
 */

/* include debugger-only code */
#define VM_IF_DEBUGGER(x)  x

/* do NOT include non-debugger-only code */
#define VM_IF_NOT_DEBUGGER(x)

#else /* VM_DEBUGGER */
/*
 *   STAND-ALONE (NON-DEBUGGER) VERSION
 */

/* do NOT include debugger-only code in a stand-alone version */
#define VM_IF_DEBUGGER(x)

/* include non-debugger-only code */
#define VM_IF_NOT_DEBUGGER(x)  x

#endif /* VM_DEBUGGER */

#endif /* VMDBG_H */

