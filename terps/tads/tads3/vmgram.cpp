#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmgram.cpp - T3 Grammar Production metaclass
Function
  
Notes
  
Modified
  02/15/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "tct3drv.h"
#include "tcprs.h"
#include "vmtype.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmlst.h"
#include "vmstr.h"
#include "vmgram.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmmeta.h"
#include "vmdict.h"
#include "vmtobj.h"
#include "vmpredef.h"
#include "vmfile.h"
#include "vmbif.h"
#include "vmdynfunc.h"
#include "vmpredef.h"


/* ------------------------------------------------------------------------ */
/*
 *   Grammar production object - undo record.  This records a deleted or
 *   replaced alternative at a given index.  
 */
#define VMGRAM_UNDO_DELETED    1                 /* alternative was deleted */
#define VMGRAM_UNDO_REPLACED   2                /* alternative was replaced */
#define VMGRAM_UNDO_ADDED      3                   /* alternative was added */
struct vmgram_undo_rec
{
    vmgram_undo_rec(int op, int idx, vmgram_alt_info *alt)
    {
        this->idx = idx;
        this->op = op;
        this->alt = alt;
    }

    ~vmgram_undo_rec()
    {
        /* delete our alternative */
        if (alt != 0)
            delete alt;
    }

    /* original index of 'alt' in the production's alternative list */
    int idx;

    /* operation that was performed */
    int op;

    /* the original alternative rule data */
    vmgram_alt_info *alt;
};


/* ------------------------------------------------------------------------ */
/*
 *   Grammar production object - memory pool.  This is a simple
 *   suballocator that obtains large blocks from the system allocator,
 *   then hands out pieces of those blocks.  Allocating is very cheap
 *   (just a pointer increment in most cases), and we don't track
 *   individual allocations and deletions but just throw away all
 *   suballocated memory at once.  
 */

/* size of each memory block */
const size_t VMGRAMPROD_MEM_BLOCK_SIZE = 16*1024;

/*
 *   memory pool block 
 */
struct CVmGramProdMemBlock
{
    /* next block in chain */
    CVmGramProdMemBlock *nxt_;

    /* bytes of the block */
    char buf_[VMGRAMPROD_MEM_BLOCK_SIZE];
};

/*
 *   memory pool object 
 */
class CVmGramProdMem
{
public:
    CVmGramProdMem()
    {
        /* no memory blocks yet */
        block_head_ = 0;
        block_cur_ = 0;

        /* we don't yet have a block */
        cur_rem_ = 0;
        cur_free_ = 0;
    }

    ~CVmGramProdMem()
    {
        /* delete each of our blocks */
        while (block_head_ != 0)
        {
            CVmGramProdMemBlock *nxt;
            
            /* remember the next block */
            nxt = block_head_->nxt_;
            
            /* delete this block */
            t3free(block_head_);

            /* move on to the next */
            block_head_ = nxt;
        }
    }

    /* reset - delete all previously sub-allocated memory */
    void reset()
    {
        /* start over with the first block */
        block_cur_ = block_head_;

        /* initialize the free pointer for the new block, if we have one */
        if (block_cur_ != 0)
        {
            /* start at the beginning of the new block */
            cur_free_ = block_cur_->buf_;

            /* this entire block is available */
            cur_rem_ = VMGRAMPROD_MEM_BLOCK_SIZE;
        }
        else
        {
            /* there are no blocks, so there's no memory available yet */
            cur_rem_ = 0;
            cur_free_ = 0;
        }
    }

    /* allocate memory */
    void *alloc(size_t siz)
    {
        void *ret;
        
        /* round the size to the local hardware boundary */
        siz = osrndsz(siz);

        /* if it exceeds the block size, fail */
        if (siz > VMGRAMPROD_MEM_BLOCK_SIZE)
            return 0;

        /* if we don't have enough space in this block, go to the next */
        if (siz > cur_rem_)
        {
            /* allocate a new block if necessary */
            if (block_cur_ == 0)
            {
                /* there's nothing in the list yet - set up the first block */
                block_head_ = (CVmGramProdMemBlock *)
                              t3malloc(sizeof(CVmGramProdMemBlock));

                /* activate the new block */
                block_cur_ = block_head_;

                /* there's nothing after this block */
                block_cur_->nxt_ = 0;
            }
            else if (block_cur_->nxt_ == 0)
            {
                /* we're at the end of the list - add a block */
                block_cur_->nxt_ = (CVmGramProdMemBlock *)
                                   t3malloc(sizeof(CVmGramProdMemBlock));

                /* advance to the new block */
                block_cur_ = block_cur_->nxt_;

                /* the new block is the last in the chain */
                block_cur_->nxt_ = 0;
            }
            else
            {
                /* another block follows - advance to it */
                block_cur_ = block_cur_->nxt_;
            }

            /* start at the beginning of the new block */
            cur_free_ = block_cur_->buf_;

            /* this entire block is available */
            cur_rem_ = VMGRAMPROD_MEM_BLOCK_SIZE;
        }

        /* the return value will be the current free pointer */
        ret = cur_free_;

        /* consume the space */
        cur_free_ += siz;
        cur_rem_ -= siz;

        /* return the block location */
        return ret;
    }

private:
    /* head of block list */
    CVmGramProdMemBlock *block_head_;

    /* block we're currently suballocating out of */
    CVmGramProdMemBlock *block_cur_;

    /* current free pointer in current block */
    char *cur_free_;

    /* amount of space remaining in current block */
    size_t cur_rem_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Alternative object 
 */

vmgram_alt_info::vmgram_alt_info(size_t ntoks)
{
    toks = new vmgram_tok_info[ntoks];
    tok_cnt = ntoks;
    score = 0;
    badness = 0;
    del = FALSE;
}

vmgram_alt_info::~vmgram_alt_info()
{
    delete [] toks;
}

/* ------------------------------------------------------------------------ */
/*
 *   Input token array entry.  We make a private copy of the input token list
 *   (i.e., the token list we're trying to match to our grammar rules) during
 *   parsing using one of these structures for each token.  
 */
struct vmgramprod_tok
{
    /* original token value */
    vm_val_t val_;

    /* token type */
    ulong typ_;

    /* token text */
    const char *txt_;

    /* length of the token text */
    size_t len_;

    /* hash value of the token */
    unsigned int hash_;
    
    /* number of vocabulary matches associated with the word */
    size_t match_cnt_;

    /* pointer to list of matches for the word */
    vmgram_match_info *matches_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Parsing state match object.  A match can be terminal, in which case
 *   it refers to a single token of input, or it can be a production, in
 *   which case it refers to a subtree of match objects.  
 */
struct CVmGramProdMatch
{
    /* allocate */
    static CVmGramProdMatch *alloc(CVmGramProdMem *mem, size_t tok_pos,
                                   const vm_val_t *tok_match_result,
                                   int matched_star,
                                   vm_prop_id_t target_prop,
                                   vm_obj_id_t proc_obj,
                                   const CVmGramProdMatch **sub_match_list,
                                   size_t sub_match_cnt)
    {
        CVmGramProdMatch *match;
        
        /* allocate space */
        match = (CVmGramProdMatch *)mem->alloc(sizeof(CVmGramProdMatch));

        /* initialize */
        match->tok_pos_ = tok_pos;
        match->matched_star_ = matched_star;
        match->target_prop_ = target_prop;
        match->proc_obj_ = proc_obj;
        match->sub_match_list_ = sub_match_list;
        match->sub_match_cnt_ = sub_match_cnt;

        /* save the token match value, if one was given */
        if (tok_match_result != 0)
            match->tok_match_result_ = *tok_match_result;
        else
            match->tok_match_result_.set_nil();

        /* return the new match */
        return match;
    }

    /* property ID with which this match is associated */
    vm_prop_id_t target_prop_;
    
    /* 
     *   token position of the matching token; ignored if the production
     *   subtree information is valid 
     */
    size_t tok_pos_;

    /*
     *   The token match value.  This is the result code from the Dictionary
     *   comparator's matchValues() function, which typically encodes
     *   information about how the value matched (truncation, case folding,
     *   accent approximation, etc) in bitwise flags.  
     */
    vm_val_t tok_match_result_;

    /* 
     *   flag: this is a '*' token in the rule; these don't really match any
     *   tokens at all, since they merely serve to stop parsing regardless
     *   of whether or not more input tokens are present 
     */
    int matched_star_;

    /* 
     *   object ID of match processor - if this is valid, this match
     *   refers to a production; otherwise, the match refers to a single
     *   token 
     */
    vm_obj_id_t proc_obj_;

    /* pointer to array of sub-matches */
    const CVmGramProdMatch **sub_match_list_;

    /* number of sub-match entries */
    size_t sub_match_cnt_;
};

/*
 *   Top-level match list holder 
 */
struct CVmGramProdMatchEntry
{
    /* the match tree */
    CVmGramProdMatch *match_;

    /* token position at the end of the match */
    size_t tok_pos_;

    /* next entry in the list */
    CVmGramProdMatchEntry *nxt_;
};

/*
 *   Parsing state object.  At any given time, we will have one or more of
 *   these state objects in our processing queue.  A state object tracks a
 *   parsing position - the token position, the alternative position, the
 *   match list so far for the alternative (i.e., the match for each item
 *   in the alternative up to but not including the current position), and
 *   the enclosing parsing state object (i.e., the parsing state that
 *   "recursed" into this alternative).  
 */
struct CVmGramProdState
{
    /* allocate */
    static CVmGramProdState *alloc(CVmGramProdMem *mem,
                                   int tok_pos, int alt_pos,
                                   CVmGramProdState *enclosing,
                                   const vmgram_alt_info *altp,
                                   vm_obj_id_t prod_obj,
                                   int circular_alt)
    {
        size_t match_cnt;
        CVmGramProdState *state;

        /* get the number of match list slots we'll need */
        match_cnt = altp->tok_cnt;

        /* allocate space, including space for the match list */
        state = (CVmGramProdState *)
                mem->alloc(sizeof(CVmGramProdState)
                           + (match_cnt - 1)*sizeof(CVmGramProdMatch *));

        /* initialize */
        state->init(tok_pos, alt_pos, enclosing, altp, prod_obj,
                    circular_alt);

        /* return the new item */
        return state;
    }
    
    /* initialize */
    void init(int tok_pos, int alt_pos, CVmGramProdState *enclosing,
              const vmgram_alt_info *altp, vm_obj_id_t prod_obj,
              int circular_alt)
    {
        /* remember the position parameters */
        tok_pos_ = tok_pos;
        alt_pos_ = alt_pos;

        /* we haven't matched a '*' token yet */
        matched_star_ = FALSE;

        /* remember our stack position */
        enclosing_ = enclosing;

        /* remember our alternative image data pointer */
        altp_ = altp;

        /* remember the production object */
        prod_obj_ = prod_obj;

        /* note if we had circular alternatives in the same production */
        circular_alt_ = circular_alt;

        /* no target property for sub-production yet */
        sub_target_prop_ = VM_INVALID_PROP;

        /* we're not in the queue yet */
        nxt_ = 0;
    }

    /* clone the state */
    CVmGramProdState *clone(CVmGramProdMem *mem)
    {
        CVmGramProdState *new_state;
        CVmGramProdState *new_enclosing;

        /* clone our enclosing state */
        new_enclosing = (enclosing_ != 0 ? enclosing_->clone(mem) : 0);
        
        /* create a new state object */
        new_state = alloc(mem, tok_pos_, alt_pos_, new_enclosing,
                          altp_, prod_obj_, circular_alt_);

        /* copy the target sub-production property */
        new_state->sub_target_prop_ = sub_target_prop_;

        /* copy the valid portion of my match list */
        if (alt_pos_ != 0)
            memcpy(new_state->match_list_, match_list_,
                   alt_pos_ * sizeof(match_list_[0]));

        /* we're not yet in any queue */
        new_state->nxt_ = 0;

        /* set the '*' flag in the cloned state */
        new_state->matched_star_ = matched_star_;

        /* return the clone */
        return new_state;
    }

    /* current token position, as an index into the token list */
    size_t tok_pos_;

    /* 
     *   flag: we've matched a '*' token in a production or subproduction,
     *   so we should consider all remaining tokens to have been consumed
     *   (we don't advance tok_pos_ past all of the tokens, because we
     *   separately want to keep track of the number of tokens we matched
     *   for everything up to the '*') 
     */
    int matched_star_;

    /* 
     *   current alternative position, as an index into the alternative's
     *   list of items 
     */
    size_t alt_pos_;

    /* 
     *   the enclosing state object - this is the state object that
     *   "recursed" to our position 
     */
    CVmGramProdState *enclosing_;

    /* pointer to alternative definition */
    const vmgram_alt_info *altp_;

    /* object ID of the production object */
    vm_obj_id_t prod_obj_;

    /* the target property for the pending sub-production */
    vm_prop_id_t sub_target_prop_;

    /* next production state in the processing queue */
    CVmGramProdState *nxt_;

    /* the production contains circular alternatives */
    int circular_alt_;

    /* 
     *   Match list.  This list is allocated with enough space for one
     *   match for each of our items (the number of items is given by
     *   vmgram_alt_tokcnt(altp_)).  At any given time, these will be
     *   valid up to but not including the current alt_pos_ index.  
     */
    const struct CVmGramProdMatch *match_list_[1];
};

/*
 *   Queues.  We maintain three queues: the primary work queue, which
 *   contains pending parsing states that we're still attempting to match;
 *   the success queue, which contains a list of match trees for
 *   successfully completed matches; and the badness queue, which contains
 *   a list of parsing states that we can try as fallbacks if we fail to
 *   find a match in any of the work queue items.  
 */
struct CVmGramProdQueue
{
    /* clear the queues */
    void clear()
    {
        work_queue_ = 0;
        badness_queue_ = 0;
        success_list_ = 0;
    }

    /* head of work queue */
    CVmGramProdState *work_queue_;

    /* head of badness queue */
    CVmGramProdState *badness_queue_;

    /* head of success list */
    CVmGramProdMatchEntry *success_list_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Grammar-Production object statics 
 */

/* metaclass registration object */
static CVmMetaclassGramProd metaclass_reg_obj;
CVmMetaclass *CVmObjGramProd::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjGramProd::
     *CVmObjGramProd::func_table_[])(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *argc) =
{
    &CVmObjGramProd::getp_undef,                                       /* 0 */
    &CVmObjGramProd::getp_parse,                                       /* 1 */
    &CVmObjGramProd::getp_get_gram_info,                               /* 2 */
    &CVmObjGramProd::getp_addAlt,                                      /* 3 */
    &CVmObjGramProd::getp_deleteAlt,                                   /* 4 */
    &CVmObjGramProd::getp_clearAlts                                    /* 5 */
};

/* property indices */
const int PROPIDX_deleteAlt = 4;


/* ------------------------------------------------------------------------ */
/*
 *   Grammar-Production metaclass implementation 
 */

/* 
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjGramProd::create_from_stack(VMG_ const uchar **pc_ptr,
                                              uint argc)
{
    /* create an empty new grammar production */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, TRUE, TRUE);
    new (vmg_ id) CVmObjGramProd(vmg0_);
    CVmObjGramProd *prod = (CVmObjGramProd *)vm_objp(vmg_ id);

    /* 
     *   mark the object as modified-since-load, since it was never part of
     *   the image file 
     */
    prod->get_ext()->modified_ = TRUE;

    /* check arguments */
    if (argc == 0)
    {
        /* no arguments - create a new production with no rules defined */
    }
    else
    {
        /* invalid argument list */
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new ID */
    return id;
}

/*
 *   constructor 
 */
CVmObjGramProd::CVmObjGramProd(VMG0_)
{
    /* allocate our extension structure from the variable heap */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(sizeof(vm_gram_ext), this);

    /* we have no image data yet */
    get_ext()->image_data_ = 0;
    get_ext()->image_data_size_ = 0;

    /* we haven't been modified yet */
    get_ext()->modified_ = FALSE;

    /* we haven't set up our alternatives yet */
    get_ext()->alts_ = 0;
    get_ext()->alt_cnt_ = 0;
    get_ext()->alt_alo_ = 0;

    /* we haven't cached any hash values yet */
    get_ext()->hashes_cached_ = FALSE;
    get_ext()->comparator_ = VM_INVALID_OBJ;

    /* presume we have no circular rules */
    get_ext()->has_circular_alt = FALSE;

    /* create our memory pool */
    get_ext()->mem_ = new CVmGramProdMem();

    /* 
     *   allocate initial property enumeration space (we'll expand this as
     *   needed, so the initial size is just a guess) 
     */
    get_ext()->prop_enum_max_ = 64;
    get_ext()->prop_enum_arr_ = (vmgram_match_info *)
                                t3malloc(get_ext()->prop_enum_max_
                                         * sizeof(vmgram_match_info));
}

/* 
 *   notify of deletion 
 */
void CVmObjGramProd::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data */
    if (ext_ != 0)
    {
        /* if we've allocated alternatives, delete them */
        vmgram_alt_info **alts = get_ext()->alts_;
        if (alts != 0)
        {
            /* delete the alternative objects */
            clear_alts();

            /* free the alternative array itself */
            t3free(alts);
        }

        /* delete our memory pool */
        delete get_ext()->mem_;

        /* delete our property enumeration space */
        t3free(get_ext()->prop_enum_arr_);
        
        /* free the extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   clear the alternative list 
 */
void CVmObjGramProd::clear_alts()
{
    /* delete each alternative */
    for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i)
        delete get_ext()->alts_[i];

    /* there are no more alternatives in the list */
    get_ext()->alt_cnt_ = 0;
}

/*
 *   ensure there's room for the given number of alternatives 
 */
void CVmObjGramProd::ensure_alts(size_t cnt, size_t margin)
{
    /* if there's not room, allocate a bigger array */
    if (cnt > get_ext()->alt_alo_)
    {
        /* allocate or reallocate at the required size plus the margin */
        cnt += margin;
        size_t siz = cnt * sizeof(get_ext()->alts_[0]);
        if (get_ext()->alts_ != 0)
        {
            get_ext()->alts_ = (vmgram_alt_info **)t3realloc(
                get_ext()->alts_, siz);
        }
        else
        {
            get_ext()->alts_ = (vmgram_alt_info **)t3malloc(siz);
        }

        /* set the new allocated count */
        get_ext()->alt_alo_ = cnt;
    }
}

/*
 *   insert an alternative at the given index (no undo) 
 */
void CVmObjGramProd::insert_alt(
    vm_obj_id_t self, size_t idx, vmgram_alt_info *alt)
{
    /* make sure we have room for one more alternative */
    ensure_alts(get_ext()->alt_cnt_ + 1, 8);

    /* get the alt list */
    vmgram_alt_info **alts = get_ext()->alts_;

    /* open a hole in the array for the new alternative */
    for (size_t i = get_ext()->alt_cnt_ ; i > idx ; --i)
        alts[i] = alts[i-1];

    /* adjust the count */
    get_ext()->alt_cnt_ += 1;

    /* plug in the new alternative */
    alts[idx] = alt;

    /* we've been modified since load time */
    get_ext()->modified_ = TRUE;

    /* note if this adds a circular rule */
    if (alt->tok_cnt != 0
        && alt->toks->typ == VMGRAM_MATCH_PROD
        && alt->toks->typinfo.prod_obj == self)
        get_ext()->has_circular_alt = TRUE;
}

/*
 *   delete the alternative at the given index (no undo) 
 */
void CVmObjGramProd::delete_alt(size_t idx)
{
    /* remove it from the array and close the gap */
    for ( ; idx + 1 < get_ext()->alt_cnt_ ; ++idx)
        get_ext()->alts_[idx] = get_ext()->alts_[idx+1];

    /* adjust the count */
    get_ext()->alt_cnt_ -= 1;

    /* we've been modified since load time */
    get_ext()->modified_ = TRUE;
}


/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjGramProd::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                             vm_obj_id_t self, vm_obj_id_t *source_obj,
                             uint *argc)
{
    uint func_idx;
    
    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* 
 *   set a property 
 */
void CVmObjGramProd::set_prop(VMG_ class CVmUndo *undo,
                              vm_obj_id_t self, vm_prop_id_t prop,
                              const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   load from an image file 
 */
void CVmObjGramProd::load_from_image(VMG_ vm_obj_id_t self,
                                     const char *ptr, size_t siz)
{
    /* remember where our image data comes from */
    get_ext()->image_data_ = ptr;
    get_ext()->image_data_size_ = siz;

    /* 
     *   Rebuild the list from the image file data.  Note that an image file
     *   doesn't have a fixup table, since it defines the object numbering
     *   space in the first place. 
     */
    CVmReadOnlyMemoryStream src(ptr, siz);
    load_alts(vmg_ self, &src, 0);

    /* we haven't been modified since being loaded from the image */
    get_ext()->modified_ = FALSE;
}

/*
 *   reset to the image file 
 */
void CVmObjGramProd::reset_to_image(VMG_ vm_obj_id_t self)
{
    /* if we we were modified and have image data, reload the image data */
    if (get_ext()->modified_ && get_ext()->image_data_ != 0)
    {
        /* load from our original data stream */
        CVmReadOnlyMemoryStream src(
            get_ext()->image_data_, get_ext()->image_data_size_);
        load_alts(vmg_ self, &src, 0);

        /* we haven't been modified since being loaded from the image */
        get_ext()->modified_ = FALSE;

        /* reset the comparator caching information */
        get_ext()->comparator_ = VM_INVALID_OBJ;
        get_ext()->hashes_cached_ = FALSE;
    }
}

/*
 *   save to a file 
 */
void CVmObjGramProd::save_to_file(VMG_ CVmFile *fp)
{
    /* if we've been dynamically modified, save our contents */
    if (get_ext()->modified_)
    {
        /* save to the file */
        CVmFileStream dst(fp);
        save_to_stream(vmg_ &dst);
    }
}

/*
 *   Save to a data stream 
 */
void CVmObjGramProd::save_to_stream(VMG_ CVmStream *fp)
{
    /* write the number of alternatives */
    fp->write_uint2(get_ext()->alt_cnt_);

    /* write the alternatives */
    vmgram_alt_info **altp = get_ext()->alts_;
    for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i, ++altp)
    {
        /* get the alternative pointer */
        vmgram_alt_info *alt = *altp;
        
        /* write the alternative header */
        fp->write_int2(alt->score);
        fp->write_int2(alt->badness);
        fp->write_uint4((ulong)alt->proc_obj);
        fp->write_uint2(alt->tok_cnt);
        
        /* write the token list */
        vmgram_tok_info *tok = alt->toks;
        for (size_t j = 0 ; j < alt->tok_cnt ; ++j, ++tok)
        {
            /* write the fixed part of the token */
            fp->write_uint2((uint)tok->prop);
            fp->write_byte((uchar)tok->typ);
            
            /* write the extra data according to the token type */
            switch (tok->typ)
            {
            case VMGRAM_MATCH_PROD:
                /* write the object ID */
                fp->write_uint4((ulong)tok->typinfo.prod_obj);
                break;
                
            case VMGRAM_MATCH_SPEECH:
                /* write the part-of-speech property */
                fp->write_uint2((uint)tok->typinfo.speech_prop);
                break;
                
            case VMGRAM_MATCH_NSPEECH:
                /* array of part-of-speech properties - write the count */
                fp->write_uint2(tok->typinfo.nspeech.cnt);
                
                /* write the array elements */
                for (size_t k = 0 ; k < tok->typinfo.nspeech.cnt ; ++k)
                    fp->write_uint2((uint)tok->typinfo.nspeech.props[k]);
                
                /* done */
                break;
                
            case VMGRAM_MATCH_LITERAL:
                /* literal string - write the length, then the bytes */
                fp->write_uint2(tok->typinfo.lit.len);
                fp->write_bytes(tok->typinfo.lit.str, tok->typinfo.lit.len);
                break;
                
            case VMGRAM_MATCH_TOKTYPE:
                /* token type */
                fp->write_uint4((ulong)tok->typinfo.toktyp_enum);
                break;
                
            case VMGRAM_MATCH_STAR:
                /* star - no additional data */
                break;
            }
        }
    }
}

/*
 *   restore from a file 
 */
void CVmObjGramProd::restore_from_file(VMG_ vm_obj_id_t self,
                                       CVmFile *fp, CVmObjFixup *fixups)
{
    /* load our alternative list from the file stream */
    CVmFileStream src(fp);
    load_alts(vmg_ self, &src, fixups);
}

/*
 *   Load alternative data from a data source.  This is our common handler
 *   for loading from the image file and restoring from a saved game file.  
 */
void CVmObjGramProd::load_alts(VMG_ vm_obj_id_t self,
                               CVmStream *src, CVmObjFixup *fixups)
{
    /* delete any existing alternative list */
    clear_alts();

    /* presume there will be no circular alternatives */
    get_ext()->has_circular_alt = FALSE;

    /* read the number of alternatives */
    size_t alt_cnt = src->read_uint2();

    /* ensure there's room for the new list */
    ensure_alts(alt_cnt, 0);

    /* load the alternatives */
    for (size_t i = 0 ; i < alt_cnt ; ++i)
    {
        /* read the alternative header */
        int score = src->read_int2();
        int badness = src->read_int2();
        vm_obj_id_t proc_obj = (vm_obj_id_t)src->read_uint4();
        uint tokcnt = src->read_uint2();

        /* fix up the processor object ID */
        if (fixups != 0)
            proc_obj = fixups->get_new_id(vmg_ proc_obj);

        /* create this alternative */
        vmgram_alt_info *alt = new vmgram_alt_info(tokcnt);
        alt->score = score;
        alt->badness = badness;
        alt->proc_obj = proc_obj;

        /* add it to the list in our extension */
        get_ext()->alt_cnt_ += 1;
        get_ext()->alts_[i] = alt;

        /* get the first token */
        vmgram_tok_info *tok = alt->toks;

        /* read the tokens */
        for (size_t j = 0 ; j < tokcnt ; ++j, ++tok)
        {
            /* read the fixed part of the token */
            tok->prop = (vm_prop_id_t)src->read_uint2();
            tok->typ = (vmgram_match_type)src->read_byte();

            /* read the extra data according to the token type */
            switch (tok->typ)
            {
            case VMGRAM_MATCH_PROD:
                /* read the object ID */
                tok->typinfo.prod_obj = (vm_obj_id_t)src->read_uint4();

                /* fix it up if necessary */
                if (fixups != 0)
                    tok->typinfo.prod_obj = fixups->get_new_id(
                        vmg_ tok->typinfo.prod_obj);
                break;

            case VMGRAM_MATCH_SPEECH:
                /* read the part-of-speech property */
                tok->typinfo.speech_prop = (vm_prop_id_t)src->read_uint2();
                break;

            case VMGRAM_MATCH_NSPEECH:
                /* array of part-of-speech properties */
                {
                    /* read the count */
                    size_t propcnt = src->read_uint2();

                    /* allocate the array */
                    tok->set_nspeech(propcnt);

                    /* read the properties */
                    for (size_t k = 0 ; k < propcnt ; ++k)
                    {
                        tok->typinfo.nspeech.props[k] =
                            (vm_prop_id_t)src->read_uint2();
                    }
                }
                break;

            case VMGRAM_MATCH_LITERAL:
                /* literal string */
                {
                    /* read the length */
                    size_t len = src->read_uint2();

                    /* allocate the string */
                    tok->set_literal(len);

                    /* read the string */
                    src->read_bytes(tok->typinfo.lit.str, len);
                }
                break;

            case VMGRAM_MATCH_TOKTYPE:
                /* token type */
                tok->typinfo.toktyp_enum = src->read_uint4();
                break;

            case VMGRAM_MATCH_STAR:
                /* star - no additional data */
                break;
            }
        }
        
        /* 
         *   If the first token of the alternative is a recursive reference
         *   to this production itself, we have a circular reference.  Note
         *   this, since we have to handle it specially when parsing. 
         */
        tok = alt->toks;
        if (tokcnt != 0
            && tok->typ == VMGRAM_MATCH_PROD
            && tok->typinfo.prod_obj == self)
        {
            /* note the existence of a circular reference */
            get_ext()->has_circular_alt = TRUE;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   apply undo 
 */
void CVmObjGramProd::apply_undo(VMG_ CVmUndoRecord *undo_rec)
{
    /* we only store pointer records, so ignore other types */
    if (undo_rec->id.ptrval != 0)
    {
        /* get our private undo record, properly cast */
        vmgram_undo_rec *rec = (vmgram_undo_rec *)undo_rec->id.ptrval;

        /* get the 'self' object from the undo record */
        vm_obj_id_t self = undo_rec->obj;

        /* get the original alternative object and its original index */
        vmgram_alt_info *alt = rec->alt;
        int idx = rec->idx;

        /* get our alternative list and count */
        vmgram_alt_info **alts = get_ext()->alts_;
        
        /* apply undo based on the event type */
        switch (rec->op)
        {
        case VMGRAM_UNDO_DELETED:
            /* deleted a record - re-insert it at the original index */
            insert_alt(self, idx, alt);

            /* the grammar prod object owns it now - clear it in the undo */
            rec->alt = 0;
            break;

        case VMGRAM_UNDO_REPLACED:
            /* replaced a record - restore it at the original index */
            alts[idx] = alt;
            break;

        case VMGRAM_UNDO_ADDED:
            /* added a new record - delete the record at this index */
            delete_alt(idx);

            /* check for circular references */
            check_circular_refs(self);
            break;
        }
    }
}

/*
 *   discard extra undo record information
 */
void CVmObjGramProd::discard_undo(VMG_ CVmUndoRecord *undo_rec)
{
    /* check for our private record type */
    if (undo_rec->id.ptrval != 0)
    {
        /* get our private undo record, properly cast */
        vmgram_undo_rec *rec = (vmgram_undo_rec *)undo_rec->id.ptrval;

        /* delete it */
        delete rec;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   mark object references in an undo record 
 */
void CVmObjGramProd::mark_undo_ref(VMG_ CVmUndoRecord *undo_rec)
{
    /* check for our private record type */
    if (undo_rec->id.ptrval != 0)
    {
        /* get our private undo record, properly cast */
        vmgram_undo_rec *rec = (vmgram_undo_rec *)undo_rec->id.ptrval;

        /* if it record an alternative, mark its references */
        if (rec->alt != 0)
            mark_alt_refs(vmg_ rec->alt, VMOBJ_REACHABLE);
    }
}

/*
 *   mark references 
 */
void CVmObjGramProd::mark_refs(VMG_ uint state)
{
    /* if we've been dynamically modified, we could have non-root refs */
    if (get_ext()->modified_)
    {
        /* run through our alternative list */
        vmgram_alt_info *const *altp = get_ext()->alts_;
        for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i, ++altp)
            mark_alt_refs(vmg_ *altp, state);
    }
}

/*
 *   mark references in an alternative 
 */
void CVmObjGramProd::mark_alt_refs(VMG_ const vmgram_alt_info *alt, uint state)
{
    /* mark the match object */
    if (alt->proc_obj != VM_INVALID_OBJ)
        G_obj_table->mark_all_refs(alt->proc_obj, state);

    /* run through the token list */
    vmgram_tok_info *tok = alt->toks;
    for (size_t j = 0 ; j < alt->tok_cnt ; ++j, ++tok)
    {
        /* only production matches have object references */
        if (tok->typ == VMGRAM_MATCH_PROD
            && tok->typinfo.prod_obj != VM_INVALID_OBJ)
        {
            /* mark the reference */
            G_obj_table->mark_all_refs(tok->typinfo.prod_obj, state);
        }
    }
}

/*
 *   Remove stale weak references. 
 */
void CVmObjGramProd::remove_stale_weak_refs(VMG0_)
{
    /* 
     *   Our reference to the dictionary comparator object is weak (we're
     *   only caching it - we don't want to prevent the object from being
     *   collected if no one else wants it).  So, forget it if the comparator
     *   is being deleted.  
     */
    if (get_ext()->comparator_ != VM_INVALID_OBJ
        && G_obj_table->is_obj_deletable(get_ext()->comparator_))
    {
        /* forget the comparator */
        get_ext()->comparator_ = VM_INVALID_OBJ;

        /* 
         *   our cached hash values depend on the comparator, so they're now
         *   invalid - forget about them 
         */
        get_ext()->hashes_cached_ = FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Context for enum_props_cb 
 */
struct enum_props_ctx
{
    /* object extension */
    vm_gram_ext *ext;
    
    /* number of properties in our list so far */
    size_t cnt;
};

/*
 *   Callback for enumerating word properties 
 */
void CVmObjGramProd::enum_props_cb(VMG_ void *ctx0, vm_prop_id_t prop,
                                   const vm_val_t * /*match_val*/)
{
    enum_props_ctx *ctx = (enum_props_ctx *)ctx0;
    vmgram_match_info *ep;
    size_t i;

    /* if this property is already in our list, don't add it again */
    for (i = 0, ep = ctx->ext->prop_enum_arr_ ; i < ctx->cnt ; ++i, ++ep)
    {
        /* if we already have this one, we can ignore it */
        if (ep->prop == prop)
            return;
    }
    
    /* we need to add it - if the array is full, expand it */
    if (ctx->cnt == ctx->ext->prop_enum_max_)
    {
        /* reallocate the array with more space */
        ctx->ext->prop_enum_max_ += 64;
        ctx->ext->prop_enum_arr_ = (vmgram_match_info *)
                                   t3realloc(ctx->ext->prop_enum_arr_,
                                             ctx->ext->prop_enum_max_
                                             * sizeof(vmgram_match_info));
    }

    /* add the property to the array */
    ep = &ctx->ext->prop_enum_arr_[ctx->cnt];
    ep->prop = prop;

    /* count the new entry */
    ctx->cnt++;
}

/* ------------------------------------------------------------------------ */
/*
 *   Execute the parseTokens method 
 */
int CVmObjGramProd::getp_parse(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc)
{
    vm_val_t *tokval;
    vm_val_t dictval;
    CVmObjDict *dict;
    int tok_cnt;
    int i;
    vmgramprod_tok *tok;
    int orig_argc = (argc != 0 ? *argc : 0);
    CVmGramProdQueue queues;
    CVmObjList *lst;
    int succ_cnt;
    CVmGramProdMatchEntry *match;
    static CVmNativeCodeDesc desc(2);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* reset our memory pool */
    get_ext()->mem_->reset();

    /* clear the work queues */
    queues.clear();

    /* 
     *   get the tokenList argument and make sure it's a list; leave it on
     *   the stack for now so it remains reachable for the garbage
     *   collector 
     */
    tokval = G_stk->get(0);
    if (!tokval->is_listlike(vmg0_)
        || (tok_cnt = tokval->ll_length(vmg0_)) < 0)
        err_throw(VMERR_LIST_VAL_REQD);

    /* check for a dictionary argument */
    if (orig_argc >= 2)
    {
        /* get the dictionary argument */
        dictval = *G_stk->get(1);

        /* make sure it's an object or nil */
        if (dictval.typ != VM_OBJ && dictval.typ != VM_NIL)
            err_throw(VMERR_OBJ_VAL_REQD);
    }
    else
    {
        /* use a nil dictionary by default */
        dictval.set_nil();
    }

    /* get the dictionary object and check that it's really a dictionary */
    if (dictval.typ != VM_NIL)
    {
        /* if it's not a CVmObjDict object, it's the wrong type */
        if (!CVmObjDict::is_dictionary_obj(vmg_ dictval.val.obj))
        {
            /* wrong type - throw an error */
            err_throw(VMERR_INVAL_OBJ_TYPE);
        }

        /* get the VM object pointer */
        dict = (CVmObjDict *)vm_objp(vmg_ dictval.val.obj);
    }
    else
    {
        /* there's no dictionary object */
        dict = 0;
    }

    /* 
     *   For quick and easy access, make our own private copy of the token
     *   and type lists.  First, allocate an array of token structures.  
     */
    tok = (vmgramprod_tok *)get_ext()->mem_
          ->alloc(tok_cnt * sizeof(vmgramprod_tok));

    /* copy the token string references and types into our list */
    for (i = 0 ; i < tok_cnt ; ++i)
    {
        vm_val_t ele_val;
        vm_val_t tok_typ_val;
        vm_val_t tok_str_val;
        int subcnt;
        const char *tokstrp;

        /* presume we won't have any vocabulary properties for the word */
        tok[i].match_cnt_ = 0;
        tok[i].matches_ = 0;

        /* get this element from the list, and treat it as a list */
        tokval->ll_index(vmg_ &ele_val, i + 1);

        /* make sure the sub-val is a list */
        if (!ele_val.is_listlike(vmg0_)
            || (subcnt = ele_val.ll_length(vmg0_)) < 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* 
         *   parse the token sublist: the first element is the token value,
         *   and the second element is the token type 
         */
        ele_val.ll_index(vmg_ &tok_str_val, 1);
        ele_val.ll_index(vmg_ &tok_typ_val, 2);

        /* use the value if it's an enumerator; if not, use zero */
        if (tok_typ_val.typ == VM_ENUM)
            tok[i].typ_ = tok_typ_val.val.enumval;
        else
            tok[i].typ_ = 0;

        /* get the token value as a string */
        tokstrp = tok_str_val.get_as_string(vmg0_);

        /* if it's a string, copy it; otherwise, ignore the value */
        if (tokstrp != 0)
        {
            /* remember the string value */
            tok[i].val_ = tok_str_val;

            /* point the token to the string */
            tok[i].txt_ = tokstrp + VMB_LEN;
            tok[i].len_ = vmb_get_len(tokstrp);

            /* calculate and store the token's hash value */
            tok[i].hash_ = calc_str_hash(
                vmg_ dict, &tok_str_val,
                tokstrp + VMB_LEN, vmb_get_len(tokstrp));
            
            /* 
             *   if we have a dictionary, enumerate the properties
             *   associated with the word 
             */
            if (dict != 0)
            {
                enum_props_ctx ctx;
                
                /* set up our callback context */
                ctx.ext = get_ext();
                ctx.cnt = 0;
                
                /* enumerate the properties */
                dict->enum_word_props(vmg_ &enum_props_cb, &ctx,
                                      &tok_str_val, tok[i].txt_, tok[i].len_);
                
                /* 
                 *   if we found any properties for the word, make a copy
                 *   for the token list 
                 */
                if (ctx.cnt != 0)
                {
                    /* allocate space for the list */
                    tok[i].match_cnt_ = ctx.cnt;
                    tok[i].matches_ =
                        (vmgram_match_info *)get_ext()->mem_
                        ->alloc(ctx.cnt * sizeof(vmgram_match_info));
                    
                    /* copy the list */
                    memcpy(tok[i].matches_, get_ext()->prop_enum_arr_,
                           ctx.cnt * sizeof(tok[i].matches_[0]));
                }
            }
        }
        else
        {
            /* it's not a string - use a null string value for this token */
            tok[i].txt_ = 0;
            tok[i].len_ = 0;
        }
    }

    /* enqueue a match state item for each of our alternatives */
    enqueue_alts(vmg_ get_ext()->mem_, tok, tok_cnt, 0, 0,
                 &queues, self, FALSE, 0, dict);

    /* process the work queue */
    process_work_queue(vmg_ get_ext()->mem_, tok, tok_cnt,
                       &queues, dict);

    /* count the entries in the success list */
    for (succ_cnt = 0, match = queues.success_list_ ; match != 0 ;
         ++succ_cnt, match = match->nxt_) ;

    /* allocate the return list */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, succ_cnt));
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* initially clear the elements of the return list to nil */
    lst->cons_clear();

    /* push the list momentarily to protect it from garbage collection */
    G_stk->push(retval);

    /* set the elements */
    for (succ_cnt = 0, match = queues.success_list_ ; match != 0 ;
         ++succ_cnt, match = match->nxt_)
    {
        vm_val_t tree_val;
        vm_val_t tok_match_val;
        size_t first_tok;
        size_t last_tok;
        
        /* 
         *   Create a list to hold the token match list - this is a list of
         *   the dictionary's comparator's matchValue() results for the
         *   tokens that matched literals in the grammar rule.  We need a
         *   list with one element per token that we matched.
         *   
         *   We can't actually populate this list yet, but all we need for
         *   the moment are references to it.  So, create the list; we'll
         *   fill it in as we traverse the match to build the result tree.  
         */
        tok_match_val.set_obj(
            CVmObjList::create(vmg_ FALSE, match->tok_pos_));

        /* push the token match value list for gc protection */
        G_stk->push(&tok_match_val);

        /* clear it out */
        ((CVmObjList *)vm_objp(vmg_ tok_match_val.val.obj))->cons_clear();

        /* build the object tree for this success object */
        build_match_tree(vmg_ match->match_, tokval, &tok_match_val,
                         &tree_val, &first_tok, &last_tok);

        /* discard our token match list's gc protection stack entry */
        G_stk->discard();

        /* 
         *   add the match tree's root object to the main return list (note
         *   that this protects it from garbage collection by virtue of the
         *   main return list being protected from garbage collection) 
         */
        lst->cons_set_element(succ_cnt, &tree_val);
    }

    /* discard the gc protection */
    G_stk->discard();

    /* discard arguments */
    G_stk->discard(orig_argc);

    /* evaluation successful */
    return TRUE;
}

/*
 *   Build an object tree for a match 
 */
void CVmObjGramProd::build_match_tree(VMG_ const CVmGramProdMatch *match,
                                      const vm_val_t *toklist,
                                      const vm_val_t *tokmatchlist,
                                      vm_val_t *retval,
                                      size_t *first_tok, size_t *last_tok)
{
    /* check to see what kind of match we have */
    if (match->proc_obj_ != VM_INVALID_OBJ)
    {
        vm_obj_id_t obj_id;
        CVmObjTads *objp;
        size_t i;

        /* 
         *   Create the object to hold the current tree level.  The only
         *   constructor argument is the superclass, which is the match's
         *   processor object. 
         */
        G_stk->push()->set_obj(match->proc_obj_);
        obj_id = CVmObjTads::create_from_stack(vmg_ 0, 1);

        /* get a pointer to the new object */
        objp = (CVmObjTads *)vm_objp(vmg_ obj_id);

        /* push this object to protect it from gc for a moment */
        G_stk->push()->set_obj(obj_id);

        /* 
         *   Initialize the caller's first/last token indices to an invalid
         *   range, in case we have no subproductions.  A production with no
         *   tokens and no subproductions matches no tokens at all; we
         *   indicate this by using an invalid range, with the first token
         *   index higher than the last token index.  Note that it's still
         *   important that we accurately reflect the end of our range for a
         *   zero-token match, since our enclosing nodes will need our
         *   position to figure out where they go.  
         */
        *first_tok = match->tok_pos_ + 1;
        *last_tok = match->tok_pos_;

        /* build the sub-match entries */
        for (i = 0 ; i < match->sub_match_cnt_ ; ++i)
        {
            size_t first_sub_tok;
            size_t last_sub_tok;
            vm_val_t val;

            /* build this subtree */
            build_match_tree(vmg_ match->sub_match_list_[i],
                             toklist, tokmatchlist,
                             &val, &first_sub_tok, &last_sub_tok);

            /* 
             *   If this is the first subtree, use it as the tentative
             *   limits for our overall match so far; otherwise, expand our
             *   limits if they are outside our range so far.
             *   
             *   If the submatch doesn't include any tokens, it obviously
             *   has no effect on our range.  The submatch range will
             *   indicate that the first token index is greater than the
             *   last token index if the submatch includes no tokens.  
             */
            if (i == 0)
            {
                /* it's the first subtree - it's all we know as yet */
                *first_tok = first_sub_tok;
                *last_tok = last_sub_tok;
            }
            else if (first_sub_tok <= last_sub_tok)
            {
                /* check to see if it expands our current range */
                if (first_sub_tok < *first_tok)
                    *first_tok = first_sub_tok;
                if (last_sub_tok > *last_tok)
                    *last_tok = last_sub_tok;
            }

            /* 
             *   save the subtree with the current match object if there's a
             *   property in which to save it 
             */
            if (match->sub_match_list_[i]->target_prop_ != VM_INVALID_PROP)
            {
                /* 
                 *   Set the processor object property for the value.  Note
                 *   that we don't have to keep undo for this change, since
                 *   we just created the tree object ourselves, and we don't
                 *   create any undo savepoints - an object created after
                 *   the most recent undo savepoint doesn't need to keep
                 *   undo information, since the entire object will be
                 *   deleted if we undo to the savepoint. 
                 */
                objp->set_prop(vmg_ 0, obj_id,
                               match->sub_match_list_[i]->target_prop_,
                               &val);
            }
        }

        /* 
         *   if we have exported properties for recording the token index
         *   range, save them with the object 
         */
        if (G_predef->gramprod_first_tok != VM_INVALID_PROP
            && G_predef->gramprod_last_tok != VM_INVALID_PROP)
        {
            vm_val_t val;

            /* save the first token index */
            val.set_int((int)*first_tok + 1);
            objp->set_prop(vmg_ 0, obj_id,
                           G_predef->gramprod_first_tok, &val);

            /* save the last token index */
            val.set_int((int)*last_tok + 1);
            objp->set_prop(vmg_ 0, obj_id,
                           G_predef->gramprod_last_tok, &val);
        }

        /* 
         *   if we have the token list property exported, set it to the
         *   original token list reference 
         */
        if (G_predef->gramprod_token_list != VM_INVALID_PROP)
        {
            /* save the token list reference */
            objp->set_prop(vmg_ 0, obj_id,
                           G_predef->gramprod_token_list, toklist);
        }

        /* if we have the token match list property exported, set it */
        if (G_predef->gramprod_token_match_list != VM_INVALID_PROP)
        {
            /* save the token match list reference */
            objp->set_prop(vmg_ 0, obj_id,
                           G_predef->gramprod_token_match_list, tokmatchlist);
        }

        /* discard our gc protection */
        G_stk->discard();

        /* the return value is the object */
        retval->set_obj(obj_id);
    }
    else
    {
        /* get the token list */
        int lstcnt = toklist->ll_length(vmg0_);

        /* make sure the index is in range */
        if ((int)match->tok_pos_ < lstcnt)
        {
            vm_val_t ele_val;
            CVmObjList *match_lst;
            
            /* get the token from the list */
            toklist->ll_index(vmg_ &ele_val, match->tok_pos_ + 1);

            /* 
             *   the token is itself a list, whose first element is the
             *   token's value - retrieve the value 
             */
            ele_val.ll_index(vmg_ retval, 1);

            /* 
             *   Store the token match result in the result list.  If this is
             *   a '*' match, it doesn't have a result list contribution,
             *   because '*' doesn't actually match any tokens (it merely
             *   stops parsing). 
             */
            if (!match->matched_star_)
            {
                /* get the match list */
                match_lst = (CVmObjList *)vm_objp(vmg_ tokmatchlist->val.obj);

                /* 
                 *   set the element at this token position in the match list
                 *   to the token match result 
                 */
                match_lst->cons_set_element(
                    match->tok_pos_, &match->tok_match_result_);
            }
        }
        else
        {
            /* 
             *   the index is past the end of the list - this must be a
             *   '*' token that matched nothing (i.e., the token list was
             *   fully consumed before we reached the '*'), so just return
             *   nil for the match value 
             */
            retval->set_nil();
        }

        /* 
         *   We match one token, so it's the first and last index.  Note
         *   that if this is a '*' match, we don't match any tokens.  
         */
        if (!match->matched_star_)
            *first_tok = *last_tok = match->tok_pos_;
    }
}

/*
 *   Process the work queue 
 */
void CVmObjGramProd::process_work_queue(VMG_ CVmGramProdMem *mem,
                                        const vmgramprod_tok *tok,
                                        size_t tok_cnt,
                                        CVmGramProdQueue *queues,
                                        CVmObjDict *dict)
{
    /* keep going until the work queue and badness queue are empty */
    for (;;)
    {
        /* 
         *   If the work queue is empty, fall back on the badness queue.
         *   Ignore the badness queue if we have any successful matches,
         *   since non-badness matches always take precedence over badness
         *   items. 
         */
        if (queues->work_queue_ == 0 && queues->success_list_ == 0)
        {
            int first;
            int min_badness;
            CVmGramProdState *cur;
            CVmGramProdState *prv;
            CVmGramProdState *nxt;
            
            /* find the lowest badness rating in the queue */
            for (first = TRUE, min_badness = 0, cur = queues->badness_queue_ ;
                 cur != 0 ; cur = cur->nxt_, first = FALSE)
            {
                /* 
                 *   if we're on the first item, note its badness as the
                 *   tentative minimum; otherwise, note the current item's
                 *   badness if it's lower than the lowest we've seen so
                 *   far 
                 */
                if (first || cur->altp_->badness < min_badness)
                    min_badness = cur->altp_->badness;
            }

            /* 
             *   move each of the items whose badness matches the minimum
             *   badness out of the badness queue and into the work queue 
             */
            for (cur = queues->badness_queue_, prv = 0 ; cur != 0 ;
                 cur = nxt)
            {
                /* remember the next item, in case we remove this one */
                nxt = cur->nxt_;
                
                /* if this item has minimum badness, move it */
                if (cur->altp_->badness == min_badness)
                {
                    /* unlink it from the badness queue */
                    if (prv == 0)
                        queues->badness_queue_ = cur->nxt_;
                    else
                        prv->nxt_ = cur->nxt_;

                    /* link it into the work queue */
                    cur->nxt_ = queues->work_queue_;
                    queues->work_queue_ = cur;
                }
                else
                {
                    /* 
                     *   this item is staying in the list - note it as the
                     *   item preceding the next item 
                     */
                    prv = cur;
                }
            }
        }

        /* if the work queue is still empty, we're out of work to do */
        if (queues->work_queue_ == 0)
            break;
        
        /* process the head of the work queue */
        process_work_queue_head(vmg_ mem, tok, tok_cnt, queues, dict);
    }
}

/*
 *   Process a work queue entry 
 */
void CVmObjGramProd::process_work_queue_head(VMG_ CVmGramProdMem *mem,
                                             const vmgramprod_tok *tok,
                                             size_t tok_cnt,
                                             CVmGramProdQueue *queues,
                                             CVmObjDict *dict)
{
    CVmGramProdState *state;
    CVmGramProdMatch *match;
    const vmgram_tok_info *tokp;
    int tok_matched_star;
    vm_prop_id_t enclosing_target_prop;
    
    /* get the first entry from the queue */
    state = queues->work_queue_;

    /* unlink this entry from the queue */
    queues->work_queue_ = state->nxt_;
    state->nxt_ = 0;

    /* get the token pointer for the next active entry in the alternative */
    tokp = state->altp_->toks + state->alt_pos_;

    /* presume we won't match a '*' token */
    tok_matched_star = FALSE;

    /* process the remaining items in the entry */
    while (state->alt_pos_ < state->altp_->tok_cnt)
    {
        int match;
        vm_val_t match_result;

        /* presume we won't find a match */
        match_result.set_nil();

        /* see what we have for this token */
        switch(tokp->typ)
        {
        case VMGRAM_MATCH_PROD:
            {
                vm_obj_id_t sub_obj_id;
                CVmObjGramProd *sub_objp;

                /*
                 *   This is a sub-production node.  Get the
                 *   sub-production object.  
                 */
                sub_obj_id = tokp->typinfo.prod_obj;

                /* make sure it's of the correct metaclass */
                if (!CVmObjGramProd::is_gramprod_obj(vmg_ sub_obj_id))
                {
                    /* wrong type - throw an error */
                    err_throw(VMERR_INVAL_OBJ_TYPE);
                }

                /* get the sub-production object */
                sub_objp = (CVmObjGramProd *)vm_objp(vmg_ sub_obj_id);

                /* 
                 *   set my subproduction target property, so that the
                 *   sub-production can set the target property correctly 
                 */
                state->sub_target_prop_ = tokp->prop;
                
                /* enqueue the alternatives for the sub-production */
                sub_objp->enqueue_alts(vmg_ mem, tok, tok_cnt,
                                       state->tok_pos_, state, queues,
                                       sub_obj_id, FALSE, 0, dict);
            }

            /* 
             *   Do not process the current state any further for now -
             *   we'll get back to it when (and if) we finish processing
             *   the sub-production.  Note that we don't even put the
             *   current state back in the queue - it's stacked behind the
             *   sub-production, and will be re-enqueued when we
             *   successfully finish with the sub-production.  
             */
            return;
            
        case VMGRAM_MATCH_SPEECH:
            /* part of speech - check to see if the current token matches */
            match = (state->tok_pos_ < tok_cnt
                     && find_prop_in_tok(&tok[state->tok_pos_],
                                         tokp->typinfo.speech_prop));
            break;

        case VMGRAM_MATCH_NSPEECH:
            /* 
             *   multiple parts of speech - check to see if the current token
             *   matches any of the parts of the speech 
             */
            if (state->tok_pos_ < tok_cnt)
            {
                size_t rem;
                const vm_prop_id_t *prop;

                /* presume we won't find a match */
                match = FALSE;

                /* check each item in the list */
                for (prop = tokp->typinfo.nspeech.props,
                     rem = tokp->typinfo.nspeech.cnt ;
                     rem != 0 ; --rem, ++prop)
                {
                    /* if this one matches, we have a match */
                    if (find_prop_in_tok(&tok[state->tok_pos_], *prop))
                    {
                        /* note the match */
                        match = TRUE;

                        /* no need to look any further at this token */
                        break;
                    }
                }
            }
            else
            {
                /* 
                 *   we're out of tokens, so we definitely don't have a
                 *   match, since we must match at least one of the possible
                 *   parts of speech of this item 
                 */
                match = FALSE;
            }
            break;
            
        case VMGRAM_MATCH_LITERAL:
            /* 
             *   Literal - check for a match to the string.  Test the hash
             *   values first, as this is much faster than comparing the
             *   strings, and at least tells us if the strings fail to match
             *   (and failing to match being by far the most common case,
             *   this saves us doing a full comparison most of the time).  
             */
            match = (state->tok_pos_ < tok_cnt
                     && tokp->typinfo.lit.hash == tok[state->tok_pos_].hash_
                     && tok_equals_lit(vmg_ &tok[state->tok_pos_],
                                       tokp->typinfo.lit.str,
                                       tokp->typinfo.lit.len,
                                       dict, &match_result));
            break;
            
        case VMGRAM_MATCH_TOKTYPE:
            /* token type */
            match = (state->tok_pos_ < tok_cnt
                     && (tok[state->tok_pos_].typ_
                         == tokp->typinfo.toktyp_enum));
            break;

        case VMGRAM_MATCH_STAR:
            /* 
             *   this matches anything remaining (it also matches if
             *   there's nothing remaining) 
             */
            match = TRUE;

            /* note that we matched a star in the state */
            state->matched_star_ = TRUE;

            /* note that we matched a star for this particular token */
            tok_matched_star = TRUE;
            break;
        }

        /* check for a match */
        if (match)
        {
            /* 
             *   we matched this token - add a token match to our state's
             *   match list for the current position 
             */
            state->match_list_[state->alt_pos_] =
                CVmGramProdMatch::alloc(mem, state->tok_pos_, &match_result,
                                        tok_matched_star, tokp->prop,
                                        VM_INVALID_OBJ, 0, 0);

            /* we're done with this alternative item - move on */
            state->alt_pos_++;

            /* 
             *   If we matched a real token, consume the matched input
             *   token.  For a '*', we don't want to consume anything, since
             *   a '*' merely stops parsing and doesn't actually match
             *   anything.  
             */
            if (!state->matched_star_)
                state->tok_pos_++;
            
            /* move on to the next alternative token item */
            ++tokp;
        }
        else
        {
            /* 
             *   This is not a match - reject this alternative.  We do not
             *   need to process this item further, so we can stop now,
             *   without returning this state to the work queue.  Simply
             *   abandon this work queue item.  
             */
            return;
        }
    }

    /* 
     *   If we make it here, we've reached the end of this alternative and
     *   have matched everything in it.  This means that the alternative
     *   was successful, so we can perform a 'reduce' operation to replace
     *   the matched tokens with this production.  
     */

    /* if we have an enclosing state, get its target property */
    enclosing_target_prop = (state->enclosing_ != 0
                             ? state->enclosing_->sub_target_prop_
                             : VM_INVALID_PROP);

    /* create a match for the entire alternative (i.e., the subproduction) */
    match = CVmGramProdMatch::alloc(
        mem, state->tok_pos_, 0, FALSE, enclosing_target_prop,
        state->altp_->proc_obj, &state->match_list_[0],
        state->altp_->tok_cnt);

    /*
     *   If the state we just matched was marked on creation as coming
     *   from an alternative with circular alternatives, we've just come
     *   up with a match for each circular alternative.  To avoid infinite
     *   recursion, we never enqueue the circular alternatives; however,
     *   now that we know we have a match for the first element of each
     *   circular alternative, we can check each of them to see if we can
     *   enqueue them.
     */
    if (state->circular_alt_)
    {
        vm_obj_id_t prod_obj_id;
        CVmObjGramProd *prod_objp;

        /* 
         *   Get the production from which this alternative came (there
         *   should be no need to check the metaclass, since we could only
         *   have created the state object from a valid production in the
         *   first place).
         *   
         *   First, make sure it's of the correct class.  (It almost
         *   certainly is, since the compiler should have generated this
         *   reference automatically, so there should be no possibility of a
         *   user error.  However, if it's not a production object, we'll
         *   probably crash by blindly casting it to one; so we'll check
         *   here, just to be sure that the compiler didn't do something
         *   wrong and the image file didn't become corrupted.)  
         */
        if (!CVmObjGramProd::is_gramprod_obj(vmg_ state->prod_obj_))
        {
            /* wrong type - throw an error */
            err_throw(VMERR_INVAL_OBJ_TYPE);
        }

        /* get the object, properly cast */
        prod_obj_id = state->prod_obj_;
        prod_objp = (CVmObjGramProd *)vm_objp(vmg_ prod_obj_id);

        /* 
         *   Enqueue the matching circular alternatives from the original
         *   production.  Our match becomes the first token match in the
         *   circular alternative (i.e., it matches the leading circular
         *   reference element).  
         */
        prod_objp->enqueue_alts(vmg_ mem, tok, tok_cnt, state->tok_pos_,
                                state->enclosing_, queues, prod_obj_id,
                                TRUE, match, dict);
    }

    /* check for an enclosing state to pop */
    if (state->enclosing_ != 0)
    {
        /* add the match to the enclosing state's match list */
        state->enclosing_->match_list_[state->enclosing_->alt_pos_] = match;
        state->enclosing_->alt_pos_++;

        /* 
         *   Move the enclosing state's token position to our token position
         *   - since it now encompasses our match, it has consumed all of
         *   the tokens therein.  Likewise, set the '*' flag in the parent
         *   to our own setting.  
         */
        state->enclosing_->tok_pos_ = state->tok_pos_;
        state->enclosing_->matched_star_ = state->matched_star_;

        /* enqueue the enclosing state so we can continue parsing it */
        enqueue_state(state->enclosing_, queues);
    }
    else
    {
        CVmGramProdMatchEntry *entry;
        
        /* 
         *   This is a top-level state, so we've completed parsing.  If
         *   we've consumed all input, or we matched a '*' token, we were
         *   successful.  If there's more input remaining, this is not a
         *   match.  
         */
        if (state->tok_pos_ < tok_cnt && !state->matched_star_)
        {
            /*
             *   There's more input remaining, so this isn't a match.
             *   Reject this alternative.  We do not need to process this
             *   item further, so stop now without returning this state
             *   object to the work queue. 
             */

            /* abandon this work queue item */
            return;
        }

        /* create a new entry for the match list */
        entry = (CVmGramProdMatchEntry *)mem->alloc(sizeof(*entry));
        entry->match_ = match;
        entry->tok_pos_ = state->tok_pos_;

        /* link the entry into the list */
        entry->nxt_ = queues->success_list_;
        queues->success_list_ = entry;
    }
}

/*
 *   Visit my alternatives and enqueue each one that is a possible match
 *   for a given token list.  
 */
void CVmObjGramProd::enqueue_alts(VMG_ CVmGramProdMem *mem,
                                  const vmgramprod_tok *tok,
                                  size_t tok_cnt, size_t start_tok_pos,
                                  CVmGramProdState *enclosing_state,
                                  CVmGramProdQueue *queues, vm_obj_id_t self,
                                  int circ_only,
                                  CVmGramProdMatch *circ_match,
                                  CVmObjDict *dict)
{
    vmgram_alt_info *const *altp;
    size_t i;
    int need_to_clone;
    int has_circ;
    vm_obj_id_t comparator;

    /* 
     *   We don't need to clone the enclosing state until we've set up one
     *   new state referring back to it.  However, if we're doing circular
     *   alternatives, the enclosing state could also be enqueued
     *   separately as its own state, so do clone all required copies in
     *   this case.  
     */
    need_to_clone = circ_only;

    /* note whether or not we have any circular alternatives */
    has_circ = get_ext()->has_circular_alt;

    /* 
     *   Cache hash values for all of our literals.  We need to do this if we
     *   haven't done it before, or if the comparator associated with the
     *   dictionary is different than it was last time we cached the values. 
     */
    comparator = (dict != 0 ? dict->get_comparator() : VM_INVALID_OBJ);
    if (!get_ext()->hashes_cached_ || get_ext()->comparator_ != comparator)
        cache_hashes(vmg_ dict);

    /* 
     *   run through our alternatives and enqueue each one that looks
     *   plausible 
     */
    for (altp = get_ext()->alts_, i = get_ext()->alt_cnt_ ; i != 0 ;
         --i, ++altp)
    {
        /* start at the first token remaining in the string */
        size_t tok_idx = start_tok_pos;

        /* get the first token for this alternative */
        vmgram_tok_info *first_tokp = (*altp)->toks;

        /* start at the first token */
        vmgram_tok_info *tokp = first_tokp;

        /* get the number of tokens in the alternative */
        size_t alt_tok_cnt = (*altp)->tok_cnt;
        
        /* presume we will find no mismatches in this check */
        int found_mismatch = FALSE;

        /* 
         *   note whether this alternative is a direct circular reference
         *   or not (it is directly circular if the first token points
         *   back to this same production) 
         */
        int is_circ = (alt_tok_cnt != 0
                       && tokp->typ == VMGRAM_MATCH_PROD
                       && tokp->typinfo.prod_obj == self);
        
        /* 
         *   we don't have a production before the current item (this is
         *   important because it determines whether we must consider the
         *   current token or any following token when trying to match a
         *   literal, part of speech, or token type) 
         */
        int prod_before = FALSE;

        /* scan the tokens in the alternative */
        for (size_t cur_alt_tok = 0 ; cur_alt_tok < alt_tok_cnt ;
             ++cur_alt_tok, ++tokp)
        {
            int match;
            int match_star;
            int ignore_item;
            vm_val_t match_result;

            /* presume we won't find a match */
            match = FALSE;
            match_star = FALSE;

            /* presume we won't be able to ignore the item */
            ignore_item = FALSE;

            /* 
             *   Try to find a match to the current alternative item.  If
             *   we've already found a mismatch, don't bother, since we need
             *   no further evidence to skip this alternative; in this case
             *   we're still looping over the alternative items simply to
             *   find the beginning of the next alternative.  
             */
            for ( ; !found_mismatch ; ++tok_idx)
            {
                /* see what kind of item we have */
                switch(tokp->typ)
                {
                case VMGRAM_MATCH_PROD:
                    /* 
                     *   We can't rule out a production without checking
                     *   it fully, which we're not doing on this pass;
                     *   however, note that we found a production, because
                     *   it means that we must from now on try skipping
                     *   any number of tokens before ruling out a match on
                     *   other grounds, since the production could
                     *   potentially match any number of tokens.
                     *   
                     *   If we're doing circular matches only, and this is
                     *   a circular production, and we're on the first
                     *   token in our list, we already know it matches, so
                     *   don't even count it as a production before the
                     *   item.  
                     */
                    if (is_circ && circ_only && cur_alt_tok == 0)
                    {
                        /* 
                         *   ignore the fact that it's a production, since
                         *   we already know it matches - we don't want to
                         *   be flexible about tokens following this item
                         *   since we know the exact number that match it 
                         */
                    }
                    else
                    {
                        /* note the production item */
                        prod_before = TRUE;
                    }

                    /* 
                     *   we can ignore this item for the purposes of
                     *   detecting a mismatch, because we can't tell
                     *   during this superficial scan whether it matches 
                     */
                    ignore_item = TRUE;
                    break;
                
                case VMGRAM_MATCH_SPEECH:
                    /* we must match a part of speech */
                    if (tok_idx < tok_cnt
                        && find_prop_in_tok(&tok[tok_idx],
                                            tokp->typinfo.speech_prop))
                    {
                        /* it's a match */
                        match = TRUE;
                    }
                    break;

                case VMGRAM_MATCH_NSPEECH:
                    /* multiple parts of speech */
                    if (tok_idx < tok_cnt)
                    {
                        vm_prop_id_t *prop;
                        size_t rem;

                        /* presume we won't find a match */
                        match = FALSE;

                        /* check each item in the list */
                        for (prop = tokp->typinfo.nspeech.props,
                             rem = tokp->typinfo.nspeech.cnt ; rem != 0 ;
                             --rem, ++prop)
                        {
                            /* if this one matches, we have a match */
                            if (find_prop_in_tok(&tok[tok_idx], *prop))
                            {
                                /* note the match */
                                match = TRUE;

                                /* no need to look any further */
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* we're out of tokens, so we have no match */
                        match = FALSE;
                    }
                    break;
                    
                case VMGRAM_MATCH_LITERAL:
                    /* 
                     *   Match a literal.  Compare the hash values first,
                     *   since a negative match on the hash values will tell
                     *   us for sure that the text won't match; since not
                     *   matching is by far the most common case, this saves
                     *   us the work of doing full string comparisons most of
                     *   the time. 
                     */
                    if (tok_idx < tok_cnt
                        && tok[tok_idx].txt_ != 0
                        && tokp->typinfo.lit.hash == tok[tok_idx].hash_
                        && tok_equals_lit(vmg_ &tok[tok_idx],
                                          tokp->typinfo.lit.str,
                                          tokp->typinfo.lit.len,
                                          dict, &match_result))
                    {
                        /* it's a match */
                        match = TRUE;
                    }
                    break;
                
                case VMGRAM_MATCH_TOKTYPE:
                    /* we must match a token type */
                    if (tok_idx < tok_cnt
                        && tok[tok_idx].typ_ == tokp->typinfo.toktyp_enum)
                        match = TRUE;
                    break;

                case VMGRAM_MATCH_STAR:
                    /* consume everything remaining on the line */
                    tok_idx = tok_cnt;

                    /* this is a match */
                    match = TRUE;
                    match_star = TRUE;
                    break;
                }

                /* if we found a match, we can stop scanning now */
                if (match)
                    break;

                /* 
                 *   we didn't match this token - if we have a production
                 *   before this point, we must keep scanning token, since
                 *   the production could end up matching any number of
                 *   tokens; if we don't have a production item, though,
                 *   we don't need to look any further, and we can rule
                 *   out this alternative immediately 
                 */
                if (!prod_before)
                    break;

                /* 
                 *   if we're ignoring this item (because we can't decide
                 *   now whether it's a match or not, and hence can't rule
                 *   it out), don't scan any tokens for the item 
                 */
                if (ignore_item)
                    break;

                /* 
                 *   if we didn't find a match, and we're out of tokens,
                 *   stop looking - we have no more tokens to look at to
                 *   find a match 
                 */
                if (!match && tok_idx >= tok_cnt)
                    break;
            }

            /* check to see if we found a match */
            if (match)
            {
                /* 
                 *   we found a match - skip the current token (unless we
                 *   matched a '*' item, in which case we've already
                 *   consumed all remaining tokens) 
                 */
                if (!match_star && tok_idx < tok_cnt)
                    ++tok_idx;
            }
            else if (!ignore_item)
            {
                /* 
                 *   we didn't match, and we can't ignore this item - note
                 *   that the alternative does not match 
                 */
                found_mismatch = TRUE;

                /* finish up with this alternative and proceed to the next */
                break;
            }
        }

        /* 
         *   If we didn't find a reason to rule out this alternative,
         *   enqueue the alternative.  Never enqueue circular references,
         *   unless we're ONLY enqueuing circular references.
         */
        if (!found_mismatch
            && ((!circ_only && !is_circ)
                || (circ_only && is_circ)))
        {
            CVmGramProdState *state;
            
            /* create and enqueue the new state */
            state = enqueue_new_state(mem, start_tok_pos, enclosing_state,
                                      *altp, self, &need_to_clone, queues,
                                      has_circ);

            /* 
             *   if we are enqueuing circular references, we already have
             *   a match for the first (circular) token, so fill it in 
             */
            if (circ_only)
            {
                CVmGramProdMatch *match;
                
                /* create a match for the alternative */
                match = CVmGramProdMatch::
                        alloc(mem, 0, 0, FALSE, first_tokp->prop,
                              circ_match->proc_obj_,
                              circ_match->sub_match_list_,
                              circ_match->sub_match_cnt_);

                /* set the first match in the token */
                state->match_list_[0] = match;

                /* set up at the second token */
                state->alt_pos_ = 1;
            }
        }
    }
}

/*
 *   Cache hash values for all of our literal tokens
 */
void CVmObjGramProd::cache_hashes(VMG_ CVmObjDict *dict)
{
    vm_obj_id_t comparator;
    vmgram_alt_info **alt;
    size_t i;

    /* get the comparator */
    comparator = (dict != 0 ? dict->get_comparator() : VM_INVALID_OBJ);

    /* run through our alternatives */
    for (i = get_ext()->alt_cnt_, alt = get_ext()->alts_ ;
         i != 0 ; --i, ++alt)
    {
        vmgram_tok_info *tok;
        size_t j;

        /* run through our tokens */
        for (j = (*alt)->tok_cnt, tok = (*alt)->toks ; j != 0 ; --j, ++tok)
        {
            /* if this is a literal token, calculate its hash value */
            if (tok->typ == VMGRAM_MATCH_LITERAL)
            {
                /* calculate and store our hash value */
                tok->typinfo.lit.hash = calc_str_hash(
                    vmg_ dict, 0, tok->typinfo.lit.str, tok->typinfo.lit.len);
            }
        }
    }

    /* note that we've cached hash values, and note the comparator we used */
    get_ext()->hashes_cached_ = TRUE;
    get_ext()->comparator_ = comparator;
}

/*
 *   Create and enqueue a new state 
 */
CVmGramProdState *CVmObjGramProd::
   enqueue_new_state(CVmGramProdMem *mem,
                     size_t start_tok_pos,
                     CVmGramProdState *enclosing_state,
                     const vmgram_alt_info *altp, vm_obj_id_t self,
                     int *need_to_clone, CVmGramProdQueue *queues,
                     int circular_alt)
{
    CVmGramProdState *state;

    /* create the new state */
    state = create_new_state(mem, start_tok_pos, enclosing_state,
                             altp, self, need_to_clone, circular_alt);
    
    /* 
     *   Add the item to the appropriate queue.  If the item has an
     *   associated badness, add it to the badness queue.  Otherwise, add
     *   it to the work queue.  
     */
    if (altp->badness != 0)
    {
        /* 
         *   we have a badness rating - add it to the badness queue, since
         *   we don't want to process it until we entirely exhaust better
         *   possibilities 
         */
        state->nxt_ = queues->badness_queue_;
        queues->badness_queue_ = state;
    }
    else
    {
        /* enqueue the state */
        enqueue_state(state, queues);
    }

    /* return the new state */
    return state;
}

/*
 *   Create a new state 
 */
CVmGramProdState *CVmObjGramProd::
   create_new_state(CVmGramProdMem *mem, size_t start_tok_pos,
                    CVmGramProdState *enclosing_state,
                    const vmgram_alt_info *altp, vm_obj_id_t self,
                    int *need_to_clone, int circular_alt)
{
    /* 
     *   if necessary, clone the enclosing state; we need to do this if we
     *   enqueue more than one nested alternative, since each nested
     *   alternative could parse the rest of the token list differently
     *   and thus provide a different result to the enclosing state 
     */
    if (*need_to_clone && enclosing_state != 0)
        enclosing_state = enclosing_state->clone(mem);

    /* 
     *   we will need to clone the enclosing state if we create another
     *   alternative after this one 
     */
    *need_to_clone = TRUE;

    /* create a new state object for the alternative */
    return CVmGramProdState::alloc(mem, start_tok_pos, 0,
                                   enclosing_state, altp, self,
                                   circular_alt);
}

/*
 *   Enqueue a state in the work queue
 */
void CVmObjGramProd::enqueue_state(CVmGramProdState *state,
                                   CVmGramProdQueue *queues)
{
    /* add it to the work queue */
    state->nxt_ = queues->work_queue_;
    queues->work_queue_ = state;
}

/*
 *   Determine if a token from the input matches a literal token, allowing
 *   the input token to be a truncated version of the literal token if the
 *   given truncation length is non-zero. 
 */
int CVmObjGramProd::tok_equals_lit(VMG_ const struct vmgramprod_tok *tok,
                                   const char *lit, size_t lit_len,
                                   CVmObjDict *dict, vm_val_t *result_val)
{
    /* 
     *   if there's a dictionary, ask it to do the comparison; otherwise,
     *   use an exact literal match 
     */
    if (dict != 0)
    {
        /* ask the dictionary for the comparison */
        return dict->match_strings(vmg_ &tok->val_, tok->txt_, tok->len_,
                                   lit, lit_len, result_val);
    }
    else
    {
        /* perform an exact comparison */
        result_val->set_int(tok->len_ == lit_len
                            && memcmp(tok->txt_, lit, lit_len) == 0);
        return result_val->val.intval;
    }
}

/*
 *   Calculate the hash value for a literal token, using our dictionary's
 *   comparator.  
 */
unsigned int CVmObjGramProd::calc_str_hash(VMG_ CVmObjDict *dict,
                                           const vm_val_t *strval,
                                           const char *str, size_t len)
{
    /* 
     *   if we have a dictionary, ask it to ask it for the hash value, as the
     *   dictionary will ask its comparator to do the work; otherwise,
     *   calculate our own hash value 
     */
    if (dict != 0)
    {
        /* 
         *   We're doing comparisons through the dictionary's comparator, so
         *   we must calculate hash values using the comparator as well to
         *   keep the hash and match operations consistent.  
         */
        return dict->calc_str_hash(vmg_ strval, str, len);
    }
    else
    {
        uint hash;

        /*
         *   We don't have a dictionary, so we're doing our own string
         *   comparisons, which we do as exact byte-for-byte matches.  We can
         *   thus calculate an arbitrary hash value of our own devising.  
         */
        for (hash = 0 ; len != 0 ; ++str, --len)
            hash += (unsigned char)*str;

        /* return the hash value we calculated */
        return hash;
    }
}


/*
 *   Find a property in a token's list.  Returns true if the property is
 *   there, false if not. 
 */
int CVmObjGramProd::find_prop_in_tok(const vmgramprod_tok *tok,
                                     vm_prop_id_t prop)
{
    size_t i;
    const vmgram_match_info *p;

    /* search for the property */
    for (i = tok->match_cnt_, p = tok->matches_ ; i != 0 ; --i, ++p)
    {
        /* if this is the one we're looking for, return success */
        if (p->prop == prop)
            return TRUE;
    }

    /* we didn't find it */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Execute the getGrammarInfo() method - builds and returns an information
 *   structure describing this grammar production object.  
 */
int CVmObjGramProd::getp_get_gram_info(VMG_ vm_obj_id_t self,
                                       vm_val_t *retval, uint *argc)
{
    vm_obj_id_t alt_lst_id;
    CVmObjList *alt_lst;
    size_t i;
    vmgram_alt_info **altp;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /*
     *   This routine uses considerable extra stack in the course of its
     *   work, so check in advance that we have enough space to run.  
     */
    if (!G_stk->check_space(8))
        err_throw(VMERR_STACK_OVERFLOW);

    /* create a list to hold the rule alternatives */
    alt_lst_id = CVmObjList::create(vmg_ FALSE, get_ext()->alt_cnt_);
    alt_lst = (CVmObjList *)vm_objp(vmg_ alt_lst_id);
    alt_lst->cons_clear();

    /* push it onto the stack for protection from garbage collection */
    G_stk->push()->set_obj(alt_lst_id);

    /* create the rule-alternative descriptions */
    for (i = 0, altp = get_ext()->alts_ ; i < get_ext()->alt_cnt_ ;
         ++i, ++altp)
    {
        size_t j;
        vmgram_tok_info *tokp;
        vm_val_t alt_obj_val;
        vm_obj_id_t tok_lst_id;
        CVmObjList *tok_lst;

        /* 
         *   if our imported GrammarAltInfo class isn't defined, we can't
         *   provide rule-alternative information, so just fill in the list
         *   with a nil 
         */
        if (G_predef->gramprod_gram_alt_info == VM_INVALID_OBJ)
        {
            /* add a nil to the list */
            alt_obj_val.set_nil();
            alt_lst->cons_set_element(i, &alt_obj_val);

            /* done with this slot */
            continue;
        }

        /* create a list to hold the token descriptors */
        tok_lst_id = CVmObjList::create(vmg_ FALSE, (*altp)->tok_cnt);
        tok_lst = (CVmObjList *)vm_objp(vmg_ tok_lst_id);
        tok_lst->cons_clear();

        /* push this momentarily for gc protection */
        G_stk->push()->set_obj(tok_lst_id);

        /*
         *   Run through the tokens in the rule alternative, and build a list
         *   of token descriptor structures. 
         */
        for (j = 0, tokp = (*altp)->toks ; j < (*altp)->tok_cnt ; ++j, ++tokp)
        {
            vm_val_t tok_obj_val;

            /* 
             *   if our imported GrammarAltTokInfo class isn't defined, we
             *   can't provide token information, so just fill in the list
             *   with a nil 
             */
            if (G_predef->gramprod_gram_alt_tok_info == VM_INVALID_OBJ)
            {
                /* add a nil to the list */
                tok_obj_val.set_nil();
                tok_lst->cons_set_element(j, &tok_obj_val);

                /* done with this slot */
                continue;
            }
            
            /* 
             *   The constructor arguments are the target property ID, the
             *   token type code, and extra information that depends on the
             *   type code.  We have to push the arguments backwards per the
             *   normal protocol, so start with the variant part.  
             */
            switch (tokp->typ)
            {
            case VMGRAM_MATCH_PROD:
                /* the extra information is the sub-production object */
                G_stk->push()->set_obj(tokp->typinfo.prod_obj);
                break;

            case VMGRAM_MATCH_SPEECH:
                /* the extra info is the part-of-speech property */
                G_stk->push()->set_propid(tokp->typinfo.speech_prop);
                break;

            case VMGRAM_MATCH_NSPEECH:
                /* the extra info is a list of part-of-speech properties */
                {
                    vm_obj_id_t pos_lst_id;
                    CVmObjList *pos_lst;
                    vm_prop_id_t *pp;
                    size_t k;

                    /* create the property list */
                    pos_lst_id = CVmObjList::create(
                        vmg_ FALSE, tokp->typinfo.nspeech.cnt);
                    pos_lst = (CVmObjList *)vm_objp(vmg_ pos_lst_id);

                    /* push it */
                    G_stk->push()->set_obj(pos_lst_id);

                    /* populate it */
                    for (k = 0, pp = tokp->typinfo.nspeech.props ;
                         k < tokp->typinfo.nspeech.cnt ;
                         ++k, ++pp)
                    {
                        vm_val_t val;
                        
                        /* fill in this element */
                        val.set_propid(*pp);
                        pos_lst->cons_set_element(k, &val);
                    }
                }
                break;

            case VMGRAM_MATCH_LITERAL:
                /* the extra info is the literal string value */
                G_stk->push()->set_obj(CVmObjString::create(
                    vmg_ FALSE, tokp->typinfo.lit.str,
                    tokp->typinfo.lit.len));
                break;

            case VMGRAM_MATCH_TOKTYPE:
                /* the extra information is the token class enum */
                G_stk->push()->set_enum(tokp->typinfo.toktyp_enum);
                break;

            case VMGRAM_MATCH_STAR:
                /* 
                 *   for a 'star' type, there is no extra information, but
                 *   push nil to keep the argument list uniform 
                 */
                G_stk->push()->set_nil();
                break;

            default:
                assert(FALSE);
                break;
            }

            /* now push the target property and type code */
            G_stk->push()->set_int(tokp->typ);
            if (tokp->prop != VM_INVALID_PROP)
                G_stk->push()->set_propid(tokp->prop);
            else
                G_stk->push()->set_nil();

            /* 
             *   the first argument to the constructor is the base class,
             *   which is the imported GrammarAltTokInfo class object 
             */
            G_stk->push()->set_obj(G_predef->gramprod_gram_alt_tok_info);

            /* create the object, at long last */
            tok_obj_val.set_obj(CVmObjTads::create_from_stack(vmg_ 0, 4));

            /* add it to our token list */
            tok_lst->cons_set_element(j, &tok_obj_val);
        }

        /* 
         *   Create the GrammarAltInfo object to desribe the alternative.
         *   Pass in the score, badness, match object, and token list as
         *   arguments to the constructor, which we'll count on to stash the
         *   information away in the new object.
         *   
         *   Note that the first constructor argument is the superclass,
         *   which is the imported GrammarAltInfo class object.  
         */
        G_stk->push()->set_obj(tok_lst_id);
        G_stk->push()->set_obj((*altp)->proc_obj);
        G_stk->push()->set_int((*altp)->badness);
        G_stk->push()->set_int((*altp)->score);
        G_stk->push()->set_obj(G_predef->gramprod_gram_alt_info);
        alt_obj_val.set_obj(CVmObjTads::create_from_stack(vmg_ 0, 5));

        /* add the new GrammarAltInfo to the main alternative list */
        alt_lst->cons_set_element(i, &alt_obj_val);

        /* 
         *   It's safe to discard the gc protection for the token list now,
         *   since a reference to is now stashed away in the GrammarAltInfo
         *   object, which in turn is referenced from our main alternative
         *   list, which we stashed on the stack earlier for gc protection.  
         */
        G_stk->discard();
    }

    /* it's safe to discard our gc protection for the main list now */
    G_stk->discard();

    /* return the main list */
    retval->set_obj(alt_lst_id);

    /* successful evaluation */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   deleteAlt() method - remove an alternative from our rule list 
 */
int CVmObjGramProd::getp_deleteAlt(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *oargc)
{
    const vm_val_t *v;
    
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the ID argument */
    vm_val_t *id = G_stk->get(0);

    /* get the dictionary, if present */
    vm_obj_id_t dict = VM_INVALID_OBJ;
    if (argc >= 2
        && (v = G_stk->get(1))->typ != VM_NIL
        && (v->typ != VM_OBJ
            || !CVmObjDict::is_dictionary_obj(vmg_ dict = v->val.obj)))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* push 'self' for gc protection */
    G_interpreter->push_obj(vmg_ self);

    /* check the ID type */
    const char *idstr;
    if (id->typ == VM_INT)
    {
        /* get the index of the alternative to delete */
        int idx = id->val.intval;

        /* make sure it's in range */
        if (idx < 1 || idx > (int)get_ext()->alt_cnt_)
            err_throw(VMERR_INDEX_OUT_OF_RANGE);

        /* mark the item for deletion */
        get_ext()->alts_[idx-1]->del = TRUE;
    }
    else if ((idstr = id->get_as_string(vmg0_)) != 0)
    {
        /* get the length and buffer pointer for the tag */
        size_t idlen = vmb_get_len(idstr);
        idstr += VMB_LEN;
        
        /* get the garmmarTag property import */
        vm_prop_id_t grammarTag = G_predef->gramprod_tag;

        /* set up a recursive invoker for grammarTag */
        vm_rcdesc rc(vmg_ "GrammarProd.deleteAlt", self,
                     PROPIDX_deleteAlt, id, argc);
        
        /*
         *   Delete by tag name.  Run through the alternatives and delete
         *   each one whose match object's grammarTag property matches the
         *   given tag name.  Work backwards through the array to avoid the
         *   overhead of closing gaps when we delete items.  
         */
        for (int idx = get_ext()->alt_cnt_ ; idx != 0 ; )
        {
            /* on to the next index */
            --idx;

            /* get this item's match object */
            vm_obj_id_t m = get_ext()->alts_[idx]->proc_obj;

            /* 
             *   if there's a match object, and the 'grammarTag' import is
             *   defined, check the match object's grammarTag value 
             */
            if (m != VM_INVALID_OBJ && grammarTag != VM_INVALID_PROP)
            {
                /* evaluate m.grammarProd */
                vm_val_t mval;
                mval.set_obj(m);
                G_interpreter->get_prop(
                    vmg_ 0, &mval, grammarTag, &mval, 0, &rc);

                /* if it's a string, compare it */
                const char *gt = G_interpreter->get_r0()->get_as_string(vmg0_);
                if (gt != 0
                    && vmb_get_len(gt) == idlen
                    && memcmp(gt + VMB_LEN, idstr, idlen) == 0)
                {
                    /* it's a match - mark it for deletion */
                    get_ext()->alts_[idx]->del = TRUE;
                }
            }
        }
    }
    else if (id->typ == VM_OBJ)
    {
        /* 
         *   Delete each alternative whose match object equals or is a
         *   subclass of the given object.  Work backwards from the end of
         *   the list to avoid the overhead of closing the gap when we delete
         *   an item earlier in the array.  
         */
        for (int idx = get_ext()->alt_cnt_ ; idx != 0 ; )
        {
            /* on to the next index */
            --idx;
            
            /* check for a match to the match object */
            vm_obj_id_t m = get_ext()->alts_[idx]->proc_obj;
            if (m != VM_INVALID_OBJ
                && (m == id->val.obj
                    || vm_objp(vmg_ m)->is_instance_of(vmg_ id->val.obj)))
            {
                /* it's a match - mark it for deletion */
                get_ext()->alts_[idx]->del = TRUE;
            }
        }
    }
    else
    {
        /* invalid type */
        err_throw(VMERR_BAD_VAL_BIF);
    }

    /* delete the marked batch */
    delete_marked_alts(vmg_ self, dict);

    /* return self */
    retval->set_obj(self);

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}

/*
 *   Delete the marked alternatives.  This runs through the alternative list
 *   and deletes each alternative marked with the 'del' flag.  We save undo
 *   for the deletions and update the dictionary (if provided) to remove
 *   literals associated with this production that are no longer used in any
 *   alternative after the deletions.  
 */
void CVmObjGramProd::delete_marked_alts(
    VMG_ vm_obj_id_t self, vm_obj_id_t dict)
{
    /* get the alt list */
    vmgram_alt_info **alts = get_ext()->alts_;

    /* if there's a dictionary, delete the words that are no longer in use */
    if (dict != VM_INVALID_OBJ
        && G_predef->misc_vocab != VM_INVALID_PROP)
    {
        /* get the dictionary object, properly cast */
        CVmObjDict *dictp = (CVmObjDict *)vm_objp(vmg_ dict);

        /* run through the alternatives being deleted */
        for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i)
        {
            /* get this item */
            vmgram_alt_info *alt = alts[i];

            /* if it's not being deleted, skip it */
            if (!alt->del)
                continue;

            /* run through its token list looking for literals */
            vmgram_tok_info *tok = alt->toks;
            for (size_t j = 0 ; j < alt->tok_cnt ; ++j, ++tok)
            {
                /* 
                 *   if it's a literal, and it's not defined in a surviving
                 *   alternative, delete it from the dictionary 
                 */
                if (tok->typ == VMGRAM_MATCH_LITERAL
                    && !find_literal_in_alts(
                        tok->typinfo.lit.str,tok->typinfo.lit.len))
                {
                    /* it's no longer used - remove it from the dictionary */
                    dictp->del_word(
                        vmg_ dict, tok->typinfo.lit.str, tok->typinfo.lit.len,
                        self, G_predef->misc_vocab);
                }
            }
        }
    }

    /* 
     *   Perform the deletions.  Work backwards from the end of the array to
     *   avoid overhead closing gaps in the array. 
     */
    for (size_t i = get_ext()->alt_cnt_ ; i > 0 ; )
    {
        /* get the next item */
        vmgram_alt_info *alt = alts[--i];

        /* if it's marked for deletion, delete it */
        if (alt->del)
        {
            /* unmark it, in case we later restore it via undo */
            alt->del = FALSE;

            /* 
             *   If this is the only remaining alternative with the same
             *   match object that's being deleted, rebuild the match
             *   object's grammarAltProps list.  If there's another
             *   alternative yet to be deleted that uses the same match
             *   object, don't bother doing the rebuild now, since we'll do
             *   it again when the loop reaches the earlier alternative, and
             *   we only need to do it once for the whole batch.  
             */
            int rebuild = TRUE;
            for (size_t j = 0 ; j < i ; ++j)
            {
                /* 
                 *   if this one's being deleted, and it has the same match
                 *   object, skip the rebuild now since we'll do it when we
                 *   get to 'j' in the main loop 
                 */
                if (alts[j]->del && alts[j]->proc_obj == alt->proc_obj)
                {
                    rebuild = FALSE;
                    break;
                }
            }

            /* do the rebuild if this is our last chance */
            if (rebuild)
                build_alt_props(vmg_ alt->proc_obj);

            /* delete it */
            delete_alt_undo(vmg_ self, i);
        }
    }

    /* check for circular references */
    check_circular_refs(self);
}

/*
 *   Scan surviving alternatives (not marked with 'del') for a given literal 
 */
int CVmObjGramProd::find_literal_in_alts(const char *str, size_t len)
{
    /* 
     *   Scan the surviving alternatives to see if this same literal token
     *   appears in any of them.  If not, it's no longer in use in the
     *   production, so delete it from the dictionary.  
     */
    for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i)
    {
        /* get this item */
        vmgram_alt_info *alt = get_ext()->alts_[i];
        
        /* if it's being deleted, skip it */
        if (alt->del)
            continue;

        /* it's still alive - scan its tokens */
        vmgram_tok_info *tok = alt->toks;
        for (size_t j = 0 ; j < alt->tok_cnt ; ++j, ++tok)
        {
            /* if this is a matching literal, the word is still in use */
            if (tok->typ == VMGRAM_MATCH_LITERAL
                && tok->typinfo.lit.len == len
                && memcmp(tok->typinfo.lit.str, str, len) == 0)
                return TRUE;
        }
    }

    /* didn't find it */
    return FALSE;
}

/*
 *   delete an alternative, keeping undo 
 */
void CVmObjGramProd::delete_alt_undo(VMG_ vm_obj_id_t self, int idx)

{
    /* create an undo record for deleting this item */
    vmgram_undo_rec *rec = new vmgram_undo_rec(
        VMGRAM_UNDO_DELETED, idx, get_ext()->alts_[idx]);

    /* save the record */
    if (!G_undo->add_new_record_ptr_key(vmg_ self, rec))
        delete rec;
    
    /* remove the alternative from the array */
    delete_alt(idx);
}

/*
 *   Check for circular references.  After deleting one or more alternatives,
 *   call this to see if we can clear the circular reference flag.  This
 *   scans the surviging alternatives, and clears the flag if none of them
 *   are circular.  
 */
void CVmObjGramProd::check_circular_refs(vm_obj_id_t self)
{
    /* presume no circular references */
    get_ext()->has_circular_alt = FALSE;

    /* scan the alternative list */
    vmgram_alt_info **altp = get_ext()->alts_;
    for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i, ++altp)
    {
        /* get the alternative */
        vmgram_alt_info *alt = *altp;
        
        /* check for a circular reference */
        if (alt->tok_cnt != 0
            && alt->toks->typ == VMGRAM_MATCH_PROD
            && alt->toks->typinfo.prod_obj == self)
        {
            /* it's circular - set the flag */
            get_ext()->has_circular_alt = TRUE;

            /* no need to look any further */
            break;
        }
    }
}

/*
 *   Rebuild the grammarAltProps list for a given match object.  This creates
 *   a list of all of the properties used in "->" expressions in all of the
 *   alternatives associated with the match object. 
 */
void CVmObjGramProd::build_alt_props(VMG_ vm_obj_id_t match_obj)
{
    /* if there's no grammarAltProps property export, skip this */
    if (G_predef->gramprod_alt_props == VM_INVALID_PROP)
        return;

    /* first do a scan without an output list to count the properties */
    size_t cnt = build_alt_props_list(vmg_ match_obj, 0);

    /* create the list */
    vm_val_t lstval;
    lstval.set_obj(CVmObjList::create(vmg_ FALSE, cnt));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ lstval.val.obj);

    /* build the list */
    build_alt_props_list(vmg_ match_obj, lst);

    /* keep only unique properties in the list */
    lst->cons_uniquify(vmg0_);

    /* store it in the match object under grammarAltProps */
    vm_objp(vmg_ match_obj)->set_prop(
        vmg_ G_undo, match_obj, G_predef->gramprod_alt_props, &lstval);
}

/*
 *   Build the grammarAltProps list for a given object, storing the results
 *   in the given list.  Returns the number of properties we found.
 */
size_t CVmObjGramProd::build_alt_props_list(
    VMG_ vm_obj_id_t match_obj, CVmObjList *lst)
{
    /* we haven't stored any properties yet */
    size_t n = 0;
    
    /* run through the surviving alternatives */
    vmgram_alt_info **alts = get_ext()->alts_;
    for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i, ++alts)
    {
        /* 
         *   if this alternative isn't being deleted, and it's associated
         *   with the given match object, add its properties to the list 
         */
        vmgram_alt_info *alt = *alts;
        if (!alt->del && alt->proc_obj == match_obj)
        {
            /* scan its token list */
            vmgram_tok_info *tok = alt->toks;
            for (size_t j = 0 ; j < alt->tok_cnt ; ++j, ++tok)
            {
                /* if this token has a target property, add it to the list */
                if (tok->prop != VM_INVALID_PROP)
                {
                    /* if we have a list, store it */
                    if (lst != 0)
                    {
                        vm_val_t v;
                        v.set_propid(tok->prop);
                        lst->cons_set_element(n, &v);
                    }
                    
                    /* count it */
                    ++n;
                }
            }
        }
    }

    /* return the number of properties we stored */
    return n;
}


/* ------------------------------------------------------------------------ */
/*
 *   clearAlts() method - removes all alternatives 
 */
int CVmObjGramProd::getp_clearAlts(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *oargc)
{
    const vm_val_t *v;

    /* check arguments */
    int argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the dictionary object, if specified */
    vm_obj_id_t dict = VM_INVALID_OBJ;
    if (argc >= 1
        && (v = G_stk->get(0))->typ != VM_NIL
        && (v->typ != VM_OBJ
            || !CVmObjDict::is_dictionary_obj(vmg_ dict = v->val.obj)))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* mark every alternative for deletion */
    for (size_t i = 0 ; i < get_ext()->alt_cnt_ ; ++i)
        get_ext()->alts_[i]->del = TRUE;

    /* save 'self' for gc protection */
    G_interpreter->push_obj(vmg_ self);

    /* delete the batch */
    delete_marked_alts(vmg_ self, dict);

    /* return self */
    retval->set_obj(self);

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   addAlt() method - add a new alternative
 */

/* compiler results/callback object for grammar rule parsing */
struct CVmGramCompResults: public CVmDynCompResults
{
    CVmGramCompResults(vm_obj_id_t self, CVmObjGramProd *selfp,
                       vm_obj_id_t dict, vm_obj_id_t match)
    {
        this->self = self;
        this->selfp = selfp;
        this->dict = dict;
        this->match = match;
    }

    /* save the parsed grammar rule data in our object */
    virtual void save_grammar(
        VMG_ CTcGramProdAlt *alts, CTcGramPropArrows *arrows)
    {
        /* add each alternative */
        for ( ; alts != 0 ; alts = alts->get_next())
            selfp->add_alt(vmg_ self, alts, dict, match);
    }

    /* self - the GrammarProd that we're adding rules to */
    vm_obj_id_t self;
    CVmObjGramProd *selfp;

    /* the Dictionary object in scope for the grammar rules */
    vm_obj_id_t dict;

    /* the match object for the alternatives */
    vm_obj_id_t match;
};

/* add an alternative to the production */
void CVmObjGramProd::add_alt(VMG_ vm_obj_id_t self, CTcGramProdAlt *alt,
                             vm_obj_id_t dict, vm_obj_id_t match)
{
    /* add it at the last position */
    int idx = get_ext()->alt_cnt_;

    /* count the tokens in the rule */
    CTcGramProdTok *tok;
    int ntoks;
    for (ntoks = 0, tok = alt->get_tok_head() ; tok != 0 ;
         ++ntoks, tok = tok->get_next()) ;

    /* create our internal alt object */
    vmgram_alt_info *ai = new vmgram_alt_info(ntoks);

    /* set the attributes */
    ai->proc_obj = match;
    ai->score = alt->get_score();
    ai->badness = alt->get_badness();

    /* translate from the parsed token list to our internal token list */
    vmgram_tok_info *ti = ai->toks;
    for (tok = alt->get_tok_head() ; tok != 0 ; ++ti, tok = tok->get_next())
    {
        /* set the property association */
        ti->prop = tok->get_prop_assoc();

        /* set the type and type-specific data */
        switch (tok->get_type())
        {
        case TCGRAM_UNKNOWN:
            ti->typ = VMGRAM_MATCH_UNDEF;
            break;

        case TCGRAM_PROD:
            ti->typ = VMGRAM_MATCH_PROD;
            ti->typinfo.prod_obj = tok->getval_prod()->get_obj_id();
            break;

        case TCGRAM_PART_OF_SPEECH:
            ti->typ = VMGRAM_MATCH_SPEECH;
            ti->typinfo.speech_prop = tok->getval_part_of_speech();
            break;

        case TCGRAM_LITERAL:
            /* set the literal in our token structure */
            ti->set_literal(
                tok->getval_literal_txt(), tok->getval_literal_len());

            /* add the token to the dictionary */
            if (dict != VM_INVALID_OBJ
                && G_predef->misc_vocab != VM_INVALID_PROP)
            {
                ((CVmObjDict *)vm_objp(vmg_ dict))->add_word(
                    vmg_ dict, ti->typinfo.lit.str, ti->typinfo.lit.len,
                    self, G_predef->misc_vocab);
            }
            break;

        case TCGRAM_TOKEN_TYPE:
            ti->typ = VMGRAM_MATCH_TOKTYPE;
            ti->typinfo.toktyp_enum = tok->getval_token_type();
            break;

        case TCGRAM_STAR:
            ti->typ = VMGRAM_MATCH_STAR;
            break;

        case TCGRAM_PART_OF_SPEECH_LIST:
            ti->set_nspeech(tok->getval_part_list_len());
            for (size_t i = 0 ; i < ti->typinfo.nspeech.cnt ; ++i)
                ti->typinfo.nspeech.props[i] = tok->getval_part_list_ele(i);
            break;
        }
    }

    /* add it to the object */
    insert_alt(self, idx, ai);

    /* create and add an undo record */
    vmgram_undo_rec *rec = new vmgram_undo_rec(VMGRAM_UNDO_ADDED, idx, 0);
    if (!G_undo->add_new_record_ptr_key(vmg_ self, rec))
        delete rec;
}

/* addAlt() implementation */
int CVmObjGramProd::getp_addAlt(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *oargc)
{
    const vm_val_t *v;
    
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(2, 2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* the first argument is the rule list */
    const vm_val_t *alt = G_stk->get(0);
    const char *altp = alt->get_as_string(vmg0_);
    if (altp == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the source length and buffer pointer */
    size_t altlen = vmb_get_len(altp);
    altp += VMB_LEN;

    /* the next argument is the match object class */
    vm_obj_id_t match = VM_INVALID_OBJ;
    v = G_stk->get(1);
    if (v->typ != VM_OBJ
        || !CVmObjTads::is_tadsobj_obj(vmg_ match = v->val.obj))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* next comes the optional dictionary argument */
    vm_obj_id_t dict = VM_INVALID_OBJ;
    if (argc >= 3
        && (v = G_stk->get(2))->typ != VM_NIL
        && (v->typ != VM_OBJ
            || !CVmObjDict::is_dictionary_obj(vmg_ (dict = v->val.obj))))
        err_throw(VMERR_INVAL_OBJ_TYPE);

    /* next comes the optional symbol table */
    vm_obj_id_t symtab = VM_INVALID_OBJ;
    if (argc >= 4 && (v = G_stk->get(3))->typ != VM_NIL)
    {
        /* get the value, and make sure it's an object */
        symtab = v->val.obj;
        if (v->typ != VM_OBJ)
            err_throw(VMERR_OBJ_VAL_REQD);
    }

    /* push self for gc protection */
    G_interpreter->push_obj(vmg_ self);

    /* dynamically compile the alt list */
    CVmGramCompResults results(self, this, dict, match);
    CVmDynamicCompiler *comp = CVmDynamicCompiler::get(vmg0_);
    comp->compile(vmg_ FALSE, symtab, VM_INVALID_OBJ, VM_INVALID_OBJ,
                  alt, altp, altlen, DCModeGramAlt, 0, &results);

    /* check for errors */
    if (results.err != 0)
        results.throw_error(vmg0_);

    /* rebuild the grammarAltProps property for the match object */
    build_alt_props(vmg_ match);

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* return self */
    retval->set_obj(self);

    /* handled */
    return TRUE;
}
