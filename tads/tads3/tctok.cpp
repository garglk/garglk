#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/tctok.cpp,v 1.5 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tctok.cpp - TADS3 compiler tokenizer
Function
  
Notes
  The tokenizer features an integrated C-style preprocessor.  The
  preprocessor is integrated into the tokenizer for efficiency; since
  the preprocessor uses the same lexical structure as the the TADS
  language, we need only tokenize the input stream once, and the result
  can be used both for preprocessing and for parsing.
Modified
  04/12/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "os.h"
#include "t3std.h"
#include "vmerr.h"
#include "vmhash.h"
#include "tcerr.h"
#include "tcerrnum.h"
#include "tctok.h"
#include "tcsrc.h"
#include "tcmain.h"
#include "tchost.h"
#include "tcprs.h"
#include "tctarg.h"
#include "charmap.h"
#include "vmdatasrc.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   Standard macro table.  This implements the interface using a standard
 *   hash table object.  
 */
class CTcBasicMacroTable: public CTcMacroTable
{
public:
    CTcBasicMacroTable(int hash_table_size, CVmHashFunc *hash_function,
                       int own_hash_func)
        : tab(hash_table_size, hash_function, own_hash_func)
    {
    }

    virtual void add(CVmHashEntry *entry) { tab.add(entry); }
    virtual void remove(CVmHashEntry *entry) { tab.remove(entry); }
    virtual CVmHashEntry *find(const char *str, size_t len)
        { return tab.find(str, len); }
    virtual void enum_entries(void (*func)(void *, CVmHashEntry *), void *ctx)
        { tab.enum_entries(func, ctx); }
    virtual void debug_dump() { tab.debug_dump(); }

private:
    /* our hash table */
    CVmHashTable tab;
};

/* ------------------------------------------------------------------------ */
/*
 *   string embedded expression context 
 */
void tok_embed_ctx::start_expr(wchar_t qu, int triple, int report)
{
    if (level < countof(stk))
    {
        s = stk + level;
        s->enter(qu, triple);
    }
    else if (report)
    {
        G_tok->log_error(TCERR_EMBEDDING_TOO_DEEP);
    }
    ++level;
}


/* ------------------------------------------------------------------------ */
/*
 *   Initialize the tokenizer 
 */
CTcTokenizer::CTcTokenizer(CResLoader *res_loader,
                           const char *default_charset)
{
    int i;
    os_time_t timer;
    struct tm *tblk;
    const char *tstr;
    char timebuf[50];
    struct kwdef
    {
        const char *kw_text;
        tc_toktyp_t kw_tok_id;
    };
    static const kwdef kwlist[] =
    {
        { "self", TOKT_SELF },
        { "targetprop", TOKT_TARGETPROP },
        { "targetobj", TOKT_TARGETOBJ },
        { "definingobj", TOKT_DEFININGOBJ },
        { "inherited", TOKT_INHERITED },
        { "delegated", TOKT_DELEGATED },
        { "argcount", TOKT_ARGCOUNT },
        { "if", TOKT_IF },
        { "else", TOKT_ELSE },
        { "for", TOKT_FOR },
        { "while", TOKT_WHILE },
        { "do", TOKT_DO },
        { "switch", TOKT_SWITCH },
        { "case", TOKT_CASE },
        { "default", TOKT_DEFAULT },
        { "goto", TOKT_GOTO },
        { "break", TOKT_BREAK },
        { "continue", TOKT_CONTINUE },
//      { "and", TOKT_AND },
//      { "or", TOKT_OR },
//      { "not", TOKT_NOT },
        { "function", TOKT_FUNCTION },
        { "return", TOKT_RETURN },
        { "local", TOKT_LOCAL },
        { "object", TOKT_OBJECT },
        { "nil", TOKT_NIL },
        { "true", TOKT_TRUE },
        { "pass", TOKT_PASS },
        { "external", TOKT_EXTERNAL },
        { "extern", TOKT_EXTERN },
        { "formatstring", TOKT_FORMATSTRING },
        { "class", TOKT_CLASS },
        { "replace", TOKT_REPLACE },
        { "modify", TOKT_MODIFY },
        { "new", TOKT_NEW },
//      { "delete", TOKT_DELETE },
        { "throw", TOKT_THROW },
        { "try", TOKT_TRY },
        { "catch", TOKT_CATCH },
        { "finally", TOKT_FINALLY },
        { "intrinsic", TOKT_INTRINSIC },
        { "dictionary", TOKT_DICTIONARY },
        { "grammar", TOKT_GRAMMAR },
        { "enum", TOKT_ENUM },
        { "template", TOKT_TEMPLATE },
        { "static", TOKT_STATIC },
        { "foreach", TOKT_FOREACH },
        { "export", TOKT_EXPORT },
        { "propertyset", TOKT_PROPERTYSET },
        { "transient", TOKT_TRANSIENT },
        { "replaced", TOKT_REPLACED },
        { "property", TOKT_PROPERTY },
        { "operator", TOKT_OPERATOR },
        { "method", TOKT_METHOD },
        { "invokee", TOKT_INVOKEE },

//      { "void", TOKT_VOID },
//      { "int", TOKT_INT },
//      { "string", TOKT_STRING },
//      { "list", TOKT_LIST },
//      { "boolean", TOKT_BOOLEAN },
//      { "any", TOKT_ANY },

        /* end-of-table marker */
        { 0, TOKT_INVALID }
    };
    const kwdef *kwp;
    
    /* remember my resource loader */
    res_loader_ = res_loader;

    /* there's no stream yet */
    str_ = 0;

    /* no external source yet */
    ext_src_ = 0;

    /* start numbering the file descriptors at zero */
    next_filedesc_id_ = 0;

    /* there are no file descriptors yet */
    desc_head_ = 0;
    desc_tail_ = 0;
    desc_list_ = 0;
    desc_list_cnt_ = desc_list_alo_ = 0;

    /* empty out the input line buffer */
    clear_linebuf();

    /* start out with a minimal line buffer size */
    linebuf_.ensure_space(4096);
    expbuf_.ensure_space(4096);

    /* set up at the beginning of the input line buffer */
    start_new_line(&linebuf_, 0);

    /* remember the default character set */
    default_charset_ = lib_copy_str(default_charset);

    /* we don't have a default character mapper yet */
    default_mapper_ = 0;

    /* create an input mapper for the default character set, if specified */
    if (default_charset != 0)
        default_mapper_ = CCharmapToUni::load(res_loader, default_charset);

    /* 
     *   if the default character set wasn't specified, or we failed to
     *   load a mapper for the specified character set, use a plain ASCII
     *   mapper 
     */
    if (default_mapper_ == 0)
        default_mapper_ = new CCharmapToUniASCII();

    /* presume we're not in preprocessor-only mode */
    pp_only_mode_ = FALSE;

    /* presume we're not in list-includes mode */
    list_includes_mode_ = FALSE;

    /* presume we're not in test report mode */
    test_report_mode_ = FALSE;

    /* allow preprocessing directives */
    allow_pp_ = TRUE;

    /* there are no previously-included files yet */
    prev_includes_ = 0;

    /* by default, use the "collapse" mode for newlines in strings */
    string_newline_spacing_ = NEWLINE_SPACING_COLLAPSE;

    /* start out with ALL_ONCE mode off */
    all_once_ = FALSE;

    /* by default, ignore redundant includes without warning */
    warn_on_ignore_incl_ = FALSE;

    /* there are no include path entries yet */
    incpath_head_ = incpath_tail_ = 0;

    /* not in a quoted string yet */
    in_quote_ = '\0';
    in_triple_ = FALSE;

    /* not in a #if block yet */
    if_sp_ = 0;
    if_false_level_ = 0;

    /* not processing a preprocessor constant expression */
    in_pp_expr_ = FALSE;

    /* we don't have a current or appended line yet */
    last_desc_ = 0;
    last_linenum_ = 0;
    appended_desc_ = 0;
    appended_linenum_ = 0;

    /* allocate the first token-list block */
    init_src_block_list();

    /* create the #define and #undef symbol tables */
    defines_ = new CTcBasicMacroTable(512, new CVmHashFuncCS(), TRUE);
    undefs_ = new CVmHashTable(64, new CVmHashFuncCS(), TRUE);

    /* create the special __LINE__ and __FILE__ macros */
    defines_->add(new CTcHashEntryPpLINE(this));
    defines_->add(new CTcHashEntryPpFILE(this));

    /* get the current time and date */
    timer = os_time(0);
    tblk = os_localtime(&timer);
    tstr = asctime(tblk);

    /* 
     *   add the __DATE__ macro - the format is "Mmm dd yyyy", where "Mmm"
     *   is the three-letter month name generated by asctime(), "dd" is
     *   the day of the month, with a leading space for numbers less than
     *   ten, and "yyyy" is the year. 
     */
    sprintf(timebuf, "'%.3s %2d %4d'",
            tstr + 4, tblk->tm_mday, tblk->tm_year + 1900);
    add_define("__DATE__", timebuf);

    /* add the __TIME__ macro - 24-hour "hh:mm:ss" format */
    sprintf(timebuf, "'%.8s'", tstr + 11);
    add_define("__TIME__", timebuf);

    /* 
     *   Allocate a pool of macro resources.  The number we start with is
     *   arbitrary, since we'll add more as needed, but we want to try to
     *   allocate enough up front that we avoid time-consuming memory
     *   allocations later.  On the other hand, we don't want to
     *   pre-allocate a huge number of objects that we'll never use.  
     */
    for (macro_res_avail_ = 0, macro_res_head_ = 0, i = 0 ; i < 7 ; ++i)
    {
        CTcMacroRsc *rsc;
        
        /* allocate a new object */
        rsc = new CTcMacroRsc();

        /* add it onto the master list */
        rsc->next_ = macro_res_head_;
        macro_res_head_ = rsc;

        /* add it onto the available list */
        rsc->next_avail_ = macro_res_avail_;
        macro_res_avail_ = rsc;
    }

    /* create the keyword hash table */
    kw_ = new CVmHashTable(64, new CVmHashFuncCS(), TRUE);

    /* populate the keyword table */
    for (kwp = kwlist ; kwp->kw_text != 0 ; ++kwp)
        kw_->add(new CTcHashEntryKw(kwp->kw_text, kwp->kw_tok_id));

    /* no ungot tokens yet */
    unget_head_ = unget_cur_ = 0;

    /* no string capture file */
    string_fp_ = 0;
    string_fp_map_ = 0;

    /* there's no current token yet */
    curtok_.settyp(TOKT_NULLTOK);
    curtok_.set_text("<Start of Input>", 16);
}

/*
 *   Initialize the source save block list 
 */
void CTcTokenizer::init_src_block_list()
{
    /* allocate the first source block */
    src_cur_ = src_head_ = new CTcTokSrcBlock();

    /* set up to write into the first block */
    src_ptr_ = src_head_->get_buf();
    src_rem_ = TCTOK_SRC_BLOCK_SIZE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Delete the tokenizer 
 */
CTcTokenizer::~CTcTokenizer()
{
    /* delete all streams */
    delete_source();

    /* delete the string capture file */
    if (string_fp_ != 0)
        delete string_fp_;

    /* delete all file descriptors */
    while (desc_head_ != 0)
    {
        /* remember the next descriptor */
        CTcTokFileDesc *nxt = desc_head_->get_next();

        /* delete this one */
        delete desc_head_;

        /* move on to the next one */
        desc_head_ = nxt;
    }

    /* delete the unget list */
    unget_cur_ = 0;
    while (unget_head_ != 0)
    {
        /* remember the next element */
        CTcTokenEle *nxt = unget_head_->getnxt();

        /* delete this element */
        delete unget_head_;

        /* advance to the next element */
        unget_head_ = nxt;
    }

    /* delete the file descriptor index array */
    if (desc_list_ != 0)
        t3free(desc_list_);

    /* delete our default character set string copy */
    lib_free_str(default_charset_);

    /* release our reference on our default character mapper */
    default_mapper_->release_ref();

    /* forget about all of our previous include files */
    while (prev_includes_ != 0)
    {
        tctok_incfile_t *nxt;

        /* remember the next file */
        nxt = prev_includes_->nxt;

        /* delete this one */
        t3free(prev_includes_);

        /* move on to the next one */
        prev_includes_ = nxt;
    }

    /* delete the include path list */
    while (incpath_head_ != 0)
    {
        tctok_incpath_t *nxt;

        /* remember the next entry in the path */
        nxt = incpath_head_->nxt;

        /* delete this entry */
        t3free(incpath_head_);

        /* move on to the next one */
        incpath_head_ = nxt;
    }

    /* delete the macro resources */
    while (macro_res_head_ != 0)
    {
        CTcMacroRsc *nxt;

        /* remember the next one */
        nxt = macro_res_head_->next_;

        /* delete this one */
        delete macro_res_head_;

        /* move on to the next one */
        macro_res_head_ = nxt;
    }

    /* delete the token list */
    delete src_head_;

    /* delete the #define and #undef symbol tables */
    delete defines_;
    delete undefs_;

    /* delete the keyword hash table */
    delete kw_;

    /* if we created a mapping for the string capture file, release it */
    if (string_fp_map_ != 0)
        string_fp_map_->release_ref();
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear the line buffer 
 */
void CTcTokenizer::clear_linebuf()
{
    /* clear the buffer */
    linebuf_.clear_text();

    /* reset our read point to the start of the line buffer */
    p_.set(linebuf_.get_buf());
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a textual representation of an operator token 
 */
const char *CTcTokenizer::get_op_text(tc_toktyp_t op)
{
    struct tokname_t
    {
        tc_toktyp_t typ;
        const char *nm;
    };
    static const tokname_t toknames[] =
    {
        { TOKT_EOF, "<end of file>" },
        { TOKT_SYM, "<symbol>" },
        { TOKT_INT, "<integer>" },
        { TOKT_SSTR, "<single-quoted string>" },
        { TOKT_DSTR, "<double-quoted string>" },
        { TOKT_DSTR_START, "<double-quoted string>" },
        { TOKT_DSTR_MID, "<double-quoted string>" },
        { TOKT_DSTR_END, "<double-quoted string>" },
        { TOKT_RESTR, "<regex string>" },
        { TOKT_LPAR, "(" },
        { TOKT_RPAR, ")" },
        { TOKT_COMMA, "," },
        { TOKT_DOT, "." },
        { TOKT_LBRACE, "{" },
        { TOKT_RBRACE, "}", },
        { TOKT_LBRACK, "[", },
        { TOKT_RBRACK, "]", },
        { TOKT_EQ, "=", },
        { TOKT_EQEQ, "==", },
        { TOKT_ASI, ":=" },
        { TOKT_PLUS, "+" },
        { TOKT_MINUS, "-" },
        { TOKT_TIMES, "*" },
        { TOKT_DIV, "/", },
        { TOKT_MOD, "%" },
        { TOKT_GT, ">" },
        { TOKT_LT, "<" },
        { TOKT_GE, ">=" },
        { TOKT_LE, "<=" },
        { TOKT_NE, "!=" },
        { TOKT_ARROW, "->" },
        { TOKT_COLON, ":" },
        { TOKT_SEM, ";" },
        { TOKT_AND, "&" },
        { TOKT_ANDAND, "&&" },
        { TOKT_OR, "|" },
        { TOKT_OROR, "||" },
        { TOKT_XOR, "^" },
        { TOKT_SHL, "<<" },
        { TOKT_ASHR, ">>" },
        { TOKT_LSHR, ">>>" },
        { TOKT_INC, "++" },
        { TOKT_DEC, "--" },
        { TOKT_PLUSEQ, "+=" },
        { TOKT_MINEQ, "-=" },
        { TOKT_TIMESEQ, "*=" },
        { TOKT_DIVEQ, "/=" },
        { TOKT_MODEQ, "%=" },
        { TOKT_ANDEQ, "&=" },
        { TOKT_OREQ, "|=" },
        { TOKT_XOREQ, "^=" },
        { TOKT_SHLEQ, "<<=" },
        { TOKT_ASHREQ, ">>=" },
        { TOKT_LSHREQ, ">>>=" },
        { TOKT_NOT, "! (not)" },
        { TOKT_BNOT, "~" },
        { TOKT_POUND, "#" },
        { TOKT_POUNDPOUND, "##" },
        { TOKT_POUNDAT, "#@" },
        { TOKT_ELLIPSIS, "..." },
        { TOKT_QUESTION, "?" },
        { TOKT_QQ, "??" },
        { TOKT_COLONCOLON, "::" },
        { TOKT_FLOAT, "<float>" },
        { TOKT_BIGINT, "<bigint>" },
        { TOKT_AT, "@" },
        { TOKT_DOTDOT, ".." },
        { TOKT_SELF, "self" },
        { TOKT_TARGETPROP, "targetprop" },
        { TOKT_TARGETOBJ, "targetobj" },
        { TOKT_DEFININGOBJ, "definingobj" },
        { TOKT_INHERITED, "inherited" },
        { TOKT_DELEGATED, "delegated" },
        { TOKT_IF, "if" },
        { TOKT_ELSE, "else" },
        { TOKT_FOR, "for" },
        { TOKT_WHILE, "while" },
        { TOKT_DO, "do" },
        { TOKT_SWITCH, "switch" },
        { TOKT_CASE, "case" },
        { TOKT_DEFAULT, "default" },
        { TOKT_GOTO, "goto" },
        { TOKT_BREAK, "break" },
        { TOKT_CONTINUE, "continue" },
        { TOKT_FUNCTION, "function" },
        { TOKT_RETURN, "return" },
        { TOKT_LOCAL, "local" },
        { TOKT_OBJECT, "object" },
        { TOKT_NIL, "nil" },
        { TOKT_TRUE, "true" },
        { TOKT_PASS, "pass" },
        { TOKT_EXTERNAL, "external" },
        { TOKT_EXTERN, "extern" },
        { TOKT_FORMATSTRING, "formatstring" },
        { TOKT_CLASS, "class" },
        { TOKT_REPLACE, "replace" },
        { TOKT_MODIFY, "modify" },
        { TOKT_NEW, "new" },
//      { TOKT_DELETE, "delete" },
        { TOKT_THROW, "throw" },
        { TOKT_TRY, "try" },
        { TOKT_CATCH, "catch" },
        { TOKT_FINALLY, "finally" },
        { TOKT_INTRINSIC, "intrinsic" },
        { TOKT_DICTIONARY, "dictionary" },
        { TOKT_GRAMMAR, "grammar" },
        { TOKT_ENUM, "enum" },
        { TOKT_TEMPLATE, "template" },
        { TOKT_STATIC, "static" },
        { TOKT_FOREACH, "foreach" },
        { TOKT_EXPORT, "export" },
        { TOKT_PROPERTYSET, "propertyset" },
        { TOKT_TRANSIENT, "transient" },
        { TOKT_REPLACED, "replaced" },
        { TOKT_PROPERTY, "property" },
        { TOKT_OPERATOR, "operator" },
        { TOKT_METHOD, "method" },
        { TOKT_INVOKEE, "invokee" },

//      { TOKT_VOID, "void" },
//      { TOKT_INTKW, "int" },
//      { TOKT_STRING, "string" },
//      { TOKT_LIST, "list" },
//      { TOKT_BOOLEAN, "boolean" },
//      { TOKT_ANY, "any"},

        { TOKT_INVALID, 0 }
    };
    const tokname_t *p;

    /* search for the token */
    for (p = toknames ; p->nm != 0 ; ++p)
    {
        /* if this is our token, return the associated name string */
        if (p->typ == op)
            return p->nm;
    }

    /* we didn't find it */
    return "<unknown>";
}

/* ------------------------------------------------------------------------ */
/*
 *   Reset the tokenizer.  Delete the current source object and all of the
 *   saved source text.  This can be used after compilation of a unit
 *   (such as a debugger expression) is completed and the intermediate
 *   parser state is no longer needed.  
 */
void CTcTokenizer::reset()
{
    /* delete the source object */
    delete_source();

    /* delete saved token text */
    if (src_head_ != 0)
    {
        /* delete the list */
        delete src_head_;

        /* re-initialize the source block list */
        init_src_block_list();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete the source file, if any, including any parent include files.
 */
void CTcTokenizer::delete_source()
{
    /* delete the current stream and all enclosing parents */
    while (str_ != 0)
    {
        CTcTokStream *nxt;

        /* remember the next stream in the list */
        nxt = str_->get_parent();

        /* delete this stream */
        delete str_;

        /* move up to the next one */
        str_ = nxt;
    }

    /* there are no more streams */
    str_ = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set up to read a source file.  Returns zero on success, or a non-zero
 *   error code on failure.  
 */
int CTcTokenizer::set_source(const char *src_filename, const char *orig_name)
{
    CTcTokFileDesc *desc;
    CTcSrcFile *src;
    int charset_error;
    int default_charset_error;
    
    /* empty out the input line buffer */
    clear_linebuf();

    /* set up at the beginning of the input line buffer */
    start_new_line(&linebuf_, 0);

    /* create a reader for the source file */
    src = CTcSrcFile::open_source(src_filename, res_loader_,
                                  default_charset_, &charset_error,
                                  &default_charset_error);
    if (src == 0)
    {
        /* if we had a problem loading the default character set, log it */
        if (default_charset_error)
            log_error(TCERR_CANT_LOAD_DEFAULT_CHARSET, default_charset_);
        
        /* return failure */
        return TCERR_CANT_OPEN_SRC;
    }

    /* find or create a file descriptor for this filename */
    desc = get_file_desc(src_filename, strlen(src_filename), FALSE,
                         orig_name, strlen(orig_name));

    /* 
     *   Create a stream to read the source file.  The new stream has no
     *   parent, because this is the top-level source file, and was not
     *   included from any other file.  
     */
    str_ = new CTcTokStream(desc, src, 0, charset_error, if_sp_);

    /* success */
    return 0;
}

/*
 *   Set up to read source code from a memory buffer 
 */
void CTcTokenizer::set_source_buf(const char *buf, size_t len)
{
    CTcSrcMemory *src;

    /* empty out the input line buffer */
    clear_linebuf();

    /* reset the scanning state to the start of a brand new stream */
    in_pp_expr_ = FALSE;
    last_linenum_ = 0;
    unsplicebuf_.clear_text();
    in_quote_ = '\0';
    in_triple_ = FALSE;
    comment_in_embedding_.reset();
    macro_in_embedding_.reset();
    main_in_embedding_.reset();
    if_sp_ = 0;
    if_false_level_ = 0;
    unget_cur_ = 0;

    /* set up at the beginning of the input line buffer */
    start_new_line(&linebuf_, 0);

    /* create a reader for the memory buffer */
    src = new CTcSrcMemory(buf, len, default_mapper_);

    /* 
     *   Create a stream to read the source file.  The new stream has no
     *   parent, because this is the top-level source file, and was not
     *   included from any other file.  
     */
    str_ = new CTcTokStream(0, src, 0, 0, if_sp_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Stuff text into the source stream.
 */
void CTcTokenizer::stuff_text(const char *txt, size_t len, int expand)
{
    CTcTokString expbuf;
    int p_ofs;
    
    /* if desired, expand macros */
    if (expand)
    {
        /* expand macros in the text, storing the result in 'expbuf' */
        expand_macros(&expbuf, txt, len);

        /* use the expanded version as the stuffed text now */
        txt = expbuf.get_text();
        len = expbuf.get_text_len();
    }

    /* get the current p_ offset */
    p_ofs = p_.getptr() - curbuf_->get_text();

    /* insert the text into the buffer */
    curbuf_->insert(p_ofs, txt, len);

    /* reset p_ in case the curbuf_ buffer was reallocated for expansion */
    start_new_line(curbuf_, p_ofs);
}

/* ------------------------------------------------------------------------ */
/*
 *   Find or create a file descriptor for a given filename
 */
CTcTokFileDesc *CTcTokenizer::get_file_desc(const char *fname,
                                            size_t fname_len,
                                            int always_create,
                                            const char *orig_fname,
                                            size_t orig_fname_len)
{
    CTcTokFileDesc *orig_desc;
    CTcTokFileDesc *desc;

    /* presume we won't find an original descriptor in the list */
    orig_desc = 0;

    /* 
     *   Search the list of existing descriptors to find one that matches.
     *   Do this regardless of whether we're allowed to re-use an existing
     *   one or not - even if we're creating a new one unconditionaly, we
     *   need to know if there's an earlier copy that already exists so we
     *   can associate the new one with the original. 
     */
    for (desc = desc_head_ ; desc != 0 ; desc = desc->get_next())
    {
        /* check for a name match */
        if (strlen(desc->get_fname()) == fname_len
            && memcmp(desc->get_fname(), fname, fname_len) == 0)
        {
            /* 
             *   if we're allowed to return an existing descriptor, return
             *   this one, since it's for the same filename 
             */
            if (!always_create)
                return desc;

            /* 
             *   we have to create a new descriptor even though we have an
             *   existing one - remember the original so we can point the
             *   new one back to the original 
             */
            orig_desc = desc;

            /* 
             *   no need to look any further - we've found the first
             *   instance of this filename in our list 
             */
            break;
        }
    }

    /* we didn't find a match - create a new descriptor */
    desc = new CTcTokFileDesc(fname, fname_len, next_filedesc_id_++,
                              orig_desc, orig_fname, orig_fname_len);

    /* link it in at the end of the master list */
    desc->set_next(0);
    if (desc_tail_ == 0)
        desc_head_ = desc;
    else
        desc_tail_->set_next(desc);
    desc_tail_ = desc;

    /* expand our array index if necessary */
    if (desc_list_cnt_ >= desc_list_alo_)
    {
        size_t siz;
        
        /* allocate or expand the array */
        desc_list_alo_ += 10;
        siz = desc_list_alo_ * sizeof(desc_list_[0]);
        if (desc_list_ == 0)
            desc_list_ = (CTcTokFileDesc **)t3malloc(siz);
        else
            desc_list_ = (CTcTokFileDesc **)t3realloc(desc_list_, siz);
    }

    /* add the new array entry */
    desc_list_[desc_list_cnt_++] = desc;

    /* return it */
    return desc;
}


/* ------------------------------------------------------------------------ */
/*
 *   Add an include path entry.  Each new entry goes at the end of the
 *   list, after all previous entries. 
 */
void CTcTokenizer::add_inc_path(const char *path)
{
    tctok_incpath_t *entry;
    
    /* create a new path list entry */
    entry = (tctok_incpath_t *)t3malloc(sizeof(tctok_incpath_t)
                                        + strlen(path));

    /* store the path in the entry */
    strcpy(entry->path, path);

    /* link this entry at the end of our list */
    if (incpath_tail_ != 0)
        incpath_tail_->nxt = entry;
    else
        incpath_head_ = entry;
    incpath_tail_ = entry;
    entry->nxt = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set the string capture file.  
 */
void CTcTokenizer::set_string_capture(osfildef *fp)
{
    /* delete any old capture file */
    if (string_fp_ != 0)
        delete string_fp_;

    /* 
     *   Remember the new capture file.  Use a duplicate handle, since we
     *   pass ownership of the handle to the CVmFileSource object (i.e.,
     *   it'll close the handle when done). 
     */
    string_fp_ = new CVmFileSource(osfdup(fp, "w"));

    /* 
     *   if we don't already have a character mapping to translate from
     *   our internal unicode characters back into the source file
     *   character set, create one now 
     */
    if (string_fp_map_ == 0)
    {
        /* try creating a mapping for the default character set */
        if (default_charset_ != 0)
            string_fp_map_ =
                CCharmapToLocal::load(res_loader_, default_charset_);

        /* if we couldn't create the mapping, use a default ASCII mapping */
        if (string_fp_map_ == 0)
            string_fp_map_ = CCharmapToLocal::load(res_loader_, "us-ascii");
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the next token in the input stream, reading additional lines from
 *   the source file as needed. 
 */
tc_toktyp_t CTcTokenizer::next()
{
    /* the current token is about to become the previous token */
    prvtok_ = curtok_;
    
    /* if there's an un-got token, return it */
    if (unget_cur_ != 0)
    {
        /* get the current unget token */
        curtok_ = *unget_cur_;

        /* we've now consumed this ungotten token */
        unget_cur_ = unget_cur_->getprv();

        /* return the new token's type */
        return curtok_.gettyp();
    }

    /* if there's an external source, get its next token */
    if (ext_src_ != 0)
    {
        const CTcToken *ext_tok;

        /* get the next token from the external source */
        ext_tok = ext_src_->get_next_token();

        /* check to see if we got a token */
        if (ext_tok == 0)
        {
            /* 
             *   restore the current token in effect before this source was
             *   active 
             */
            curtok_ = *ext_src_->get_enclosing_curtok();

            /* 
             *   this source has no more tokens - restore the enclosing
             *   source, and keep going so we try getting a token from it 
             */
            ext_src_ = ext_src_->get_enclosing_source();

            /* return the token type */
            return curtok_.gettyp();
        }
        else
        {
            /* we got a token - copy it to our internal token buffer */
            curtok_ = *ext_tok;

            /* return its type */
            return curtok_.gettyp();
        }
    }
    
    /* keep going until we get a valid token */
    for (;;)
    {
        /* 
         *   read the next token from the current line, applying
         *   appropriate string translations and storing strings and
         *   symbols in the source block list 
         */
        tc_toktyp_t typ = next_on_line_xlat_keep();

        /* if it's the "null" token, skip it and read another token */
        if (typ == TOKT_NULLTOK)
            continue;

        /* if we found a valid token, we're done - return the token */
        if (typ != TOKT_EOF)
            return typ;

        /* 
         *   if we're at the end of a preprocess line, don't read another
         *   line - just return end of file 
         */
        if (p_.getch() == TOK_END_PP_LINE)
            return TOKT_EOF;

        /* 
         *   we've reached the end of the line - read another line,
         *   applying preprocessing directives and expanding macros as
         *   needed 
         */
        if (read_line_pp())
        {
            /* no more lines are available - return end of file */
            return TOKT_EOF;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   clear external token sources, returning to the true input stream 
 */
void CTcTokenizer::clear_external_sources()
{
    /* 
     *   restore the current token as it was before the outermost external
     *   source was first established 
     */
    if (ext_src_ != 0)
    {
        CTcTokenSource *outer;

        /* find the outermost source */
        for (outer = ext_src_ ; outer->get_enclosing_source() != 0 ;
             outer = ext_src_->get_enclosing_source()) ;
        
        /* restore its original next token */
        curtok_ = *ext_src_->get_enclosing_curtok();
    }
    
    /* there's no external source now */
    ext_src_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Make a safely storable copy of the current token. 
 */
const CTcToken *CTcTokenizer::copycur()
{
    /* if it's already a type that we store safely, return the current token */
    if (is_tok_safe(curtok_.gettyp()))
        return getcur();

    /* save the current token's text in permanent tokenizer memory */
    curtok_.set_text(store_source(curtok_.get_text(), curtok_.get_text_len()),
                     curtok_.get_text_len());

    /* return the current token, now that we've made it safe */
    return &curtok_;
}

/*
 *   Make a safely storable copy of a given token. 
 */
void CTcTokenizer::copytok(CTcToken *dst, const CTcToken *src)
{
    /* start with an exact copy of the token */
    *dst = *src;

    /* if the token is a symbol, it already has a safe copy */
    if (is_tok_safe(src->gettyp()))
        return;

    /* save the token's text in permanent tokenizer memory */
    dst->set_text(store_source(dst->get_text(), dst->get_text_len()),
                  dst->get_text_len());
}

/*
 *   Is the given token type "safe"?  A safe token is one whose text is
 *   always saved in the tokenizer source list, which means that the text
 *   pointer is safe as long as the tokenizer object exists.  A non-safe
 *   token is one whose text pointer points directly into the current line
 *   buffer, meaning that its text buffer is only guaranteed to last until
 *   the next token fetch.  
 */
int CTcTokenizer::is_tok_safe(tc_toktyp_t typ)
{
    switch (typ)
    {
    case TOKT_SYM:
    case TOKT_SSTR:
    case TOKT_SSTR_START:
    case TOKT_SSTR_MID:
    case TOKT_SSTR_END:
    case TOKT_DSTR:
    case TOKT_DSTR_START:
    case TOKT_DSTR_MID:
    case TOKT_DSTR_END:
    case TOKT_RESTR:
    case TOKT_FLOAT:
    case TOKT_BIGINT:
        /* these types are always stored in the source list */
        return TRUE;

    default:
        /* other types are not always safely stored */
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Check to see if the current token matches the given text 
 */
int CTcTokenizer::cur_tok_matches(const char *txt, size_t len)
{
    /* if the length matches, and the text matches exactly, it matches */
    return (getcur()->get_text_len() == len
            && memcmp(getcur()->get_text(), txt, len) == 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Look ahead to see if we match a pair of symbol tokens 
 */
int CTcTokenizer::look_ahead(const char *s1, const char *s2)
{
    /* if the first token doesn't match, the sequence doesn't match */
    if (cur() != TOKT_SYM || !cur_tok_matches(s1, strlen(s1)))
        return FALSE;

    /* check the next token - if it matches, we have a sequence match */
    if (next() == TOKT_SYM && cur_tok_matches(s2, strlen(s2)))
    {
        /* got it - skip the second token and return success */
        next();
        return TRUE;
    }

    /* 
     *   no match - but we've already read the next token, so put it back
     *   before we return failure 
     */
    unget();
    return FALSE;
}

/*
 *   Peek ahead - same as look_ahead, but doesn't skip anything on a
 *   successful match. 
 */
int CTcTokenizer::peek_ahead(const char *s1, const char *s2)
{
    /* if the first token doesn't match, the sequence doesn't match */
    if (cur() != TOKT_SYM || !cur_tok_matches(s1, strlen(s1)))
        return FALSE;

    /* check the next token - if it matches, we have a sequence match */
    int match = (next() == TOKT_SYM && cur_tok_matches(s2, strlen(s2)));

    /* whatever happened, un-get the second token, since we're just peeking */
    unget();

    /* return the match indication */
    return match;
}


/* ------------------------------------------------------------------------ */
/*
 *   Un-get the current token 
 */
void CTcTokenizer::unget()
{
    /* unget, backing up to the internally saved previous token */
    unget(&prvtok_);
}

/*
 *   Un-get the current token and back up to the specified previous token. 
 */
void CTcTokenizer::unget(const CTcToken *prv)
{
    /* push the current token onto the unget stack */
    push(&curtok_);

    /* go back to the previous token */
    curtok_ = *prv;

    /* the internally saved previous token is no longer valid */
    prvtok_.settyp(TOKT_INVALID);
}

/*
 *   Push a token into the stream 
 */
void CTcTokenizer::push(const CTcToken *tok)
{
    /* if the unget list is empty, create the initial entry */
    if (unget_head_ == 0)
        unget_head_ = new CTcTokenEle();

    /* advance to the next slot in the list */
    if (unget_cur_ == 0)
    {
        /* no last ungot token - set up at the head of the list */
        unget_cur_ = unget_head_;
    }
    else if (unget_cur_->getnxt() != 0)
    {
        /* there's another slot available - advance to it */
        unget_cur_ = unget_cur_->getnxt();
    }
    else
    {
        /* the list is full - we need to allocate a new slot */
        CTcTokenEle *newele = new CTcTokenEle();

        /* link it at the end of the list */
        unget_cur_->setnxt(newele);
        newele->setprv(unget_cur_);

        /* advance to it */
        unget_cur_ = newele;
    }

    /* save the pushed token */
    unget_cur_->set(*tok);
}

/* ------------------------------------------------------------------------ */
/*
 *   Assume that we should have just found a '>>' terminating an embedded
 *   expression in a string.  If possible, back out the previous token and
 *   re-scan it as though it had started with '>>'.
 *   
 *   This is to be called by a higher-level parser when it determines that,
 *   syntactically, we should have found the '>>' leaving an embedded
 *   expression.  
 */
void CTcTokenizer::assume_missing_str_cont()
{
    xlat_string_to_src(&main_in_embedding_, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Skip whitespace and macro expansion markers 
 */
void CTcTokenizer::skip_ws_and_markers(utf8_ptr *p)
{
    /* keep going until we find something interesting */
    for (;;)
    {
        wchar_t cur;

        /* get the current character */
        cur = p->getch();

        /* 
         *   if it's a macro expansion end marker, skip it as though it
         *   were whitespace; otherwise, if it's whitespace, skip it;
         *   otherwise, we're done skipping leading whitespace 
         */
        if (cur == TOK_MACRO_EXP_END)
        {
            /* skip the embedded pointer value that follows */
            p->set(p->getptr() + 1 + sizeof(CTcHashEntryPp *));
        }
        else if (is_space(cur))
        {
            /* skip the space */
            p->inc();
        }
        else
        {
            /* it's not whitespace or equivalent - we're done */
            return;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the next token from the input stream, operating on the current
 *   line only.  
 */
tc_toktyp_t CTcTokenizer::next_on_line(utf8_ptr *p, CTcToken *tok,
                                       tok_embed_ctx *ec, int expanding)
{
    tc_toktyp_t typ;

    /* skip whitespace */
    skip_ws_and_markers(p);

    /* remember where the token starts */
    utf8_ptr start = *p;

    /* get the initial character */
    wchar_t cur = p->getch();

    /* if there's nothing left in the current line, return EOF */
    if (cur == '\0')
    {
        /* indicate end of file */
        typ = TOKT_EOF;
        goto done;
    }

    /* skip the initial character */
    p->inc();

    /* presume the token will not be marked as fully macro-expanded */
    tok->set_fully_expanded(FALSE);

    /* see what we have */
    switch(cur)
    {
    case TOK_MACRO_FORMAL_FLAG:
        /* 
         *   this is a two-byte formal parameter sequence in a macro
         *   expansion - skip the second byte of the two-byte sequence,
         *   and return the special token type for this sequence
         */
        typ = TOKT_MACRO_FORMAL;

        /* 
         *   skip the second byte - note that we want to skip exactly one
         *   byte, regardless of what the byte looks like as a utf-8
         *   partial character, since it's not a utf-8 character at all 
         */
        p->set(p->getptr() + 1);
        break;

    case TOK_MACRO_FOREACH_FLAG:
        /* 
         *   this is the special macro '#foreach' flag - return it as a
         *   special pseudo-token 
         */
        typ = TOKT_MACRO_FOREACH;
        break;

    case TOK_MACRO_IFEMPTY_FLAG:
        /* #ifempty macro flag */
        typ = TOKT_MACRO_IFEMPTY;
        break;

    case TOK_MACRO_IFNEMPTY_FLAG:
        /* #ifnempty macro flag */
        typ = TOKT_MACRO_IFNEMPTY;
        break;

    case TOK_MACRO_ARGCOUNT_FLAG:
        /* it's the special macro '#argcount' flag */
        typ = TOKT_MACRO_ARGCOUNT;
        break;

    case TOK_FULLY_EXPANDED_FLAG:
        /* set the token flag indicating that it has been fully expanded */
        tok->set_fully_expanded(TRUE);

        /* the token symbol starts at the byte after the flag byte */
        start = p->getptr();

        /* read the first character of the symbol */
        cur = p->getch();
        p->inc();

        /* tokenize the symbol that follows */
        goto tokenize_symbol;

    case TOK_END_PP_LINE:
        /* 
         *   Preprocess line-ending marker - when we reach the end of a
         *   preprocessor line, we can't read another source line, because
         *   a preprocessor directive consists of only a single logical
         *   source line.  Once we see this, return end-of-file until the
         *   caller explicitly reads a new source line.
         *   
         *   Keep the read pointer stuck on this flag byte, so that we
         *   return end-of-file on a subsequent attempt to get the next
         *   token.  
         */
        *p = start;
        typ = TOKT_EOF;
        break;

    case '0':
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
            /* 
             *   Start out with the leading digit in the accumulator.  Note
             *   that the character set internally is always UTF-8.  
             */
            ulong acc = value_of_digit(cur);

            /*
             *   The overflow limit differs for positive and negative values,
             *   but we can't be sure at this point which we have, since even
             *   if there's a '-' before the number, it could be a
             *   subtraction operator rather than a negation operator.  We
             *   can't know until we've folded constants whether the number
             *   is positive or negative.  Use the lower limit for now; the
             *   compiler will un-promote values after constant folding if
             *   they end up fitting after all, which will handle the case of
             *   INT32MINVAL, whose absolute value is higher than this limit.
             */
            const ulong ovlimit = 2147483647U;

            /* 
             *   If it's a leading zero, treat as octal or hex.  '0x' means
             *   hex; otherwise, '0' means octal.  
             */
            if (cur == '0')
            {
                /* check for hex - if it's not hex, it's octal */
                if (p->getch() == 'x' || p->getch() == 'X')
                {
                    /* skip the 'x' */
                    p->inc();

                    /* 
                     *   scan the hex number - keep going until we find
                     *   something that's not a hex digit 
                     */
                    for (;;)
                    {
                        /* get this character */
                        cur = p->getch();

                        /* if it's not a hex digit, stop scanning */
                        if (!is_xdigit(cur))
                            break;

                        /* check for 32-bit integer overflow */
                        if ((acc & 0xF0000000) != 0)
                            goto do_int_overflow;

                        /* 
                         *   Shift the accumulator and add this digit's value.
                         *   Note that we can save a test - if the character is
                         *   >= lower-case 'a', we know it's not an upper-case
                         *   letter because the lower-case letters all have
                         *   values above the upper-case letters in UTF-8
                         *   encoding (which we always use as the internal
                         *   character set).  Since we already know it's a
                         *   valid hex digit (we wouldn't be here if it
                         *   weren't), we can just check to see if it's at
                         *   least lower-case 'a', and we automatically know
                         *   then whether it's in the 'a'-'f' range or the
                         *   'A'-'F' range.  
                         */
                        acc *= 16;
                        acc += value_of_xdigit(cur);

                        /* move on */
                        p->inc();
                    }
                }
                else
                {
                    /* scan octal digits */
                    for ( ; is_odigit(p->getch()) ; p->inc())
                    {
                        /* check for overflow */
                        if ((acc & 0xE0000000) != 0)
                            goto do_int_overflow;

                        /* add the digit */
                        acc = 8*acc + value_of_odigit(p->getch());
                    }

                    /* 
                     *   If we stopped on a digit outside of the octal range,
                     *   consume any remaining digits, and flag it as an
                     *   error.  Leaving subsequent decimal digits as a
                     *   separate token tends to be confusing, since in most
                     *   cases the inclusion of decimal digits means that the
                     *   user didn't really intend this to be an octal number
                     *   after all.  For instance, the leading zero might be
                     *   there for formatting reasons, and the user simply
                     *   forgot to take into account that it triggers octal
                     *   interpretation.  
                     */
                    if (is_digit(p->getch()))
                    {
                        /* skip subsequent digits */
                        for (p->inc() ; is_digit(p->getch()) ; p->inc()) ;

                        /* flag the error */
                        if (!expanding)
                            log_error(TCERR_DECIMAL_IN_OCTAL,
                                      p->getptr() - start.getptr(),
                                      start.getptr());
                    }
                }
            }
            else
            {
                /* scan decimal digits */
                for ( ; is_digit(p->getch()) ; p->inc())
                {
                    /* check for integer overflow in the shift */
                    if (acc > 214748364)
                        goto do_int_overflow;

                    /* shift the accumulator */
                    acc *= 10;

                    /* get the digit */
                    int d = value_of_digit(p->getch());

                    /* check for overflow adding the digit */
                    if ((unsigned)acc > ovlimit - d)
                        goto do_int_overflow;

                    /* add the digit */
                    acc += d;
                }
            }

            /* 
             *   If we stopped at a decimal point or an exponent, it's a
             *   floating point number.  This doesn't count if we have two
             *   periods in a row - ".." - because that's either a range
             *   marker operator or an ellipsis operator.  
             */
            if (p->getch() == '.' && p->getch_at(1) != '.')
            {
                typ = TOKT_FLOAT;
                goto do_float;
            }
            else if (p->getch() == 'e' || p->getch() == 'E')
            {
                typ = TOKT_FLOAT;
                goto do_float;
            }

            /* it's an integer value */
            typ = TOKT_INT;

            /* set the integer value */
            tok->set_int_val(acc);
        }
        break;

    do_int_overflow:
        /* mark it as an integer that overflowed into a float type */
        typ = TOKT_BIGINT;
        
    do_float:
        {
            /* haven't found a decimal point yet */
            int found_decpt = FALSE;
            int is_hex = FALSE;

            /* start over at the beginning of the number */
            *p = start;

            /* skip any leading sign */
            if (p->getch() == '-')
                p->inc();

            /* skip any leading "0x" */
            if (typ == TOKT_BIGINT && p->getch() == '0')
            {
                wchar_t c1 = p->getch_at(1);
                if (c1 == 'x' || c1 == 'X')
                {
                    is_hex = TRUE;
                    p->inc_by(2);
                }
            }
            
            /* parse the rest of the float */
            for ( ; ; p->inc())
            {
                /* get this character and move on */
                cur = p->getch();

                /* see what we have */
                if (is_digit(cur))
                {
                    /* we have another digit; just keep going */
                }
                else if (is_hex && is_xdigit(cur))
                {
                    /* we have another hex digit, so keep going */
                }
                else if (!found_decpt && !is_hex && cur == '.')
                {
                    /* it's the decimal point - note it and keep going */
                    found_decpt = TRUE;

                    /* if it started as a promoted int, it's really a float */
                    if (typ == TOKT_BIGINT)
                        typ = TOKT_FLOAT;
                }
                else if (cur == 'e' || cur == 'E')
                {
                    /* it might not be an exponent - look ahead to find out */
                    utf8_ptr p2 = *p;
                    p2.inc();

                    /* if we have a sign, skip it */
                    if ((cur = p2.getch()) == '-' || cur == '+')
                        p2.inc();

                    /* we need at least one digit to make an exponent */
                    if (!is_digit(p2.getch()))
                        break;

                    /* skip digits */
                    while (is_digit(p2.getch()))
                        p2.inc();

                    /* advance to the end of the exponent */
                    *p = p2;

                    /* if it started as a promoted int, it's now a float */
                    if (typ == TOKT_BIGINT)
                        typ = TOKT_FLOAT;

                    /* the end of the exponent is the end of the number */
                    break;
                }
                else
                {
                    /* everything else ends the number */
                    break;
                }
            }
        }
        break;

    case '"':
    case '\'':
        *p = start;
        return tokenize_string(p, tok, ec, expanding);

    case '(':
        typ = TOKT_LPAR;
        if (ec != 0)
            ec->parens(1);
        break;

    case ')':
        typ = TOKT_RPAR;
        if (ec != 0)
            ec->parens(-1);
        break;

    case ',':
        typ = TOKT_COMMA;
        break;

    case '.':
        /* check for '..', '...', and floating-point numbers */
        if (p->getch() == '.')
        {
            /* it's either '..' or '...' */
            p->inc();
            if (p->getch() == '.')
            {
                p->inc();
                typ = TOKT_ELLIPSIS;
            }
            else
                typ = TOKT_DOTDOT;
        }
        else if (is_digit(p->getch()))
        {
            typ = TOKT_FLOAT;
            goto do_float;
        }
        else
            typ = TOKT_DOT;
        break;

    case '{':
        typ = TOKT_LBRACE;
        if (ec != 0)
            ec->parens(1);
        break;

    case '}':
        typ = TOKT_RBRACE;
        if (ec != 0)
            ec->parens(-1);
        break;

    case '[':
        typ = TOKT_LBRACK;
        if (ec != 0)
            ec->parens(1);
        break;

    case ']':
        typ = TOKT_RBRACK;
        if (ec != 0)
            ec->parens(-1);
        break;

    case '=':
        /* check for '==' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_EQEQ;
        }
        else
            typ = TOKT_EQ;
        break;

    case ':':
        /* check for '::' */
        if (p->getch() == ':')
        {
            p->inc();
            typ = TOKT_COLONCOLON;
        }
        else
            typ = TOKT_COLON;
        break;

    case '?':
        /* check for '??' */
        if (p->getch() == '?')
        {
            p->inc();
            typ = TOKT_QQ;
        }
        else
            typ = TOKT_QUESTION;
        break;

    case '+':
        /* check for '++' and '+=' */
        if (p->getch() == '+')
        {
            p->inc();
            typ = TOKT_INC;
        }
        else if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_PLUSEQ;
        }
        else
            typ = TOKT_PLUS;
        break;

    case '-':
        /* check for '--', '->' and '-=' */
        if (p->getch() == '-')
        {
            p->inc();
            typ = TOKT_DEC;
        }
        else if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_MINEQ;
        }
        else if (p->getch() == '>')
        {
            p->inc();
            typ = TOKT_ARROW;
        }
        else
            typ = TOKT_MINUS;
        break;

    case '*':
        /* check for '*=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_TIMESEQ;
        }
        else
            typ = TOKT_TIMES;
        break;

    case '/':
        /* check for '/=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_DIVEQ;
        }
        else
            typ = TOKT_DIV;
        break;

    case '%':
        /* check for '%=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_MODEQ;
        }
        else
            typ = TOKT_MOD;
        break;

    case '>':
        /* check for '>>=', '>>' and '>=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_GE;
        }
        else if (p->getch() == '>')
        {
            /* check for the end of an embedded expression */
            if (ec != 0 && ec->in_expr() && ec->parens() == 0)
            {
                *p = start;
                return tokenize_string(p, tok, ec, expanding);
            }

            /* check for '>>>' and '>>=' */
            p->inc();
            if (p->getch() == '>')
            {
                /* skip the '>' and check for '>>>=' */
                p->inc();
                if (p->getch() == '=')
                {
                    p->inc();
                    typ = TOKT_LSHREQ;
                }
                else
                    typ = TOKT_LSHR;
            }
            else if (p->getch() == '=')
            {
                p->inc();
                typ = TOKT_ASHREQ;
            }
            else
                typ = TOKT_ASHR;
        }
        else
            typ = TOKT_GT;
        break;

    case '<':
        /* check for '<<=', '<<', '<>', and '<=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_LE;
        }
        else if (p->getch() == '<')
        {
            /* check for '<<=' */
            p->inc();
            if (p->getch() == '=')
            {
                p->inc();
                typ = TOKT_SHLEQ;
            }
            else
                typ = TOKT_SHL;
        }
#if 0
        else if (p->getch() == '>')
        {
            /* '<>' is obsolete */
            if (!expanding)
                log_error(TCERR_LTGT_OBSOLETE);

            /* ... but for now proceed as though it's != */
            p->inc();
            typ = TOKT_NE;
        }
#endif
        else
            typ = TOKT_LT;
        break;

    case ';':
        typ = TOKT_SEM;
        break;

    case '&':
        /* check for '&&' and '&=' */
        if (p->getch() == '&')
        {
            p->inc();
            typ = TOKT_ANDAND;
        }
        else if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_ANDEQ;
        }
        else
            typ = TOKT_AND;
        break;

    case '|':
        /* check for '||' and '|=' */
        if (p->getch() == '|')
        {
            p->inc();
            typ = TOKT_OROR;
        }
        else if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_OREQ;
        }
        else
            typ = TOKT_OR;
        break;

    case '^':
        /* check for '^=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_XOREQ;
        }
        else
            typ = TOKT_XOR;
        break;

    case '!':
        /* check for '!=' */
        if (p->getch() == '=')
        {
            p->inc();
            typ = TOKT_NE;
        }
        else
            typ = TOKT_NOT;
        break;

    case '~':
        typ = TOKT_BNOT;
        break;

    case '@':
        typ = TOKT_AT;
        break;

    case '#':
        /* check for '##' and '#@' */
        if (p->getch() == '#')
        {
            p->inc();
            typ = TOKT_POUNDPOUND;
        }
        else if (p->getch() == '@')
        {
            p->inc();
            typ = TOKT_POUNDAT;
        }
        else
            typ = TOKT_POUND;
        break;

    case 'R':
        /* check for regular expression string syntax */
        if (p->getch() == '"' || p->getch() == '\'')
        {
            /* 
             *   It's a regular expression string.  Tokenize the string
             *   portion as normal, but mark it as a regex string rather than
             *   a regular string. 
             */
            *p = start;
            return tokenize_string(p, tok, ec, expanding);
        }
        /* not a regex string - fall through to default case */

    default:        
        /* check to see if it's a symbol */
        if (is_syminit(cur) || !is_ascii(cur))
        {
        tokenize_symbol:
            wchar_t non_ascii = 0;
            const char *stop = 0;

            /* note if we're starting with a non-ASCII character */
            if (!is_ascii(cur))
                non_ascii = cur;

            /* 
             *   scan the identifier (note that we've already skipped the
             *   first character, so we start out at length = 1) 
             */
            for (;;)
            {
                /* 
                 *   If it's a non-ASCII character, assume it's an accented
                 *   character that's meant (erroneously) to be part of the
                 *   symbol name; note the error, but keep the character in
                 *   the symbol even though it's illegal, since this is more
                 *   likely to keep us synchronized with the intended token
                 *   structure of the source.
                 *   
                 *   Otherwise, stop on any non-symbol character.
                 */
                wchar_t ch = p->getch();
                if (!is_ascii(ch))
                {
                    if (non_ascii == 0)
                        non_ascii = ch;
                }
                else if (!is_sym(ch))
                    break;

                /* skip this character */
                const char *prv = p->getptr();
                p->inc();

                /* if this character would push us over the limit, end here */
                if (stop == 0
                    && p->getptr() - start.getptr() >= TOK_SYM_MAX_LEN)
                    stop = prv;
            }

            /* if we found any non-ASCII characters, flag an error */
            if (non_ascii != 0)
                log_error(TCERR_NON_ASCII_SYMBOL,
                          (int)(p->getptr() - start.getptr()), start.getptr(),
                          (int)non_ascii);
            
            /* if we truncated the symbol, issue a warning */
            if (stop != 0 && !expanding)
                log_warning(TCERR_SYMBOL_TRUNCATED,
                            (int)(p->getptr() - start.getptr()), start.getptr(),
                            (int)(stop - start.getptr()), start.getptr());
            
            /* it's a symbol */
            typ = TOKT_SYM;
        }
        else
        {
            /* invalid token */
            typ = TOKT_INVALID;
        }
        break;
    }

done:
    /* set the type */
    tok->settyp(typ);

    /* set the text */
    tok->set_text(start.getptr(), p->getptr() - start.getptr());

    /* return the type */
    return typ;
}

/*
 *   get the next token, limiting to the length of the source buffer 
 */
tc_toktyp_t CTcTokenizer::next_on_line(const CTcTokString *srcbuf,
                                       utf8_ptr *p, CTcToken *tok,
                                       tok_embed_ctx *ec, int expanding)
{
    /* 
     *   save the embedding context, in case the next token isn't part of the
     *   narrowed section of the buffer we're scanning 
     */
    tok_embed_ctx oldec = *ec;

    /* get the next token */
    next_on_line(p, tok, ec, expanding);
    
    /* if the token is past the end of the line, return EOF */
    if (tok->get_text() >= srcbuf->get_text_end())
    {
        /* set the token to indicate end of line */
        tok->settyp(TOKT_EOF);

        /* set the token to point to the end of the buffer */
        tok->set_text(srcbuf->get_text_end(), 0);

        /* restore the embedding context */
        *ec = oldec;
    }
    
    /* return the token type */
    return tok->gettyp();
}

/*
 *   Get the next token on the line, translating escapes in strings.  This
 *   updates the line buffer in-place to incorporate the translated string
 *   text.  
 */
tc_toktyp_t CTcTokenizer::next_on_line_xlat(utf8_ptr *p, CTcToken *tok,
                                            tok_embed_ctx *ec)
{
    /* skip whitespace */
    skip_ws_and_markers(p);

    /* if this is a string, translate escapes */
    switch(p->getch())
    {
    case '"':
    case '\'':
        /* translate the string */
        return xlat_string(p, tok, ec);

    case '>':
        /* if we're in an embedding, check for '>>' */
        if (ec != 0 && ec->in_expr()
            && ec->parens() == 0
            && p->getch_at(1) == '>')
            return tokenize_string(p, tok, ec, FALSE);

        /* use the default case */
        goto do_normal;

    case 'R':
        /* check for a regex string */
        if (p->getch_at(1) == '"' || p->getch_at(1) == '\'')
            return tokenize_string(p, tok, ec, FALSE);

        /* not a regex string - fall through to the default handling */
        
    default:
    do_normal:
        /* for anything else, use the default tokenizer */
        return next_on_line(p, tok, ec, FALSE);
    }
}

/*
 *   Look up a keyword 
 */
int CTcTokenizer::look_up_keyword(const CTcToken *tok, tc_toktyp_t *kwtok)
{
    CTcHashEntryKw *kw;

    /* look it up in the keyword table */
    kw = (CTcHashEntryKw *)kw_->find(tok->get_text(), tok->get_text_len());
    if (kw != 0)
    {
        /* we found the keyword - set 'kw' to the keyword token id */
        *kwtok = kw->get_tok_id();

        /* tell the caller we found it */
        return TRUE;
    }
    else
    {
        /* tell the caller it's not a keyword */
        return FALSE;
    }
}

/*
 *   Get the next token on the line, translating escape sequences in
 *   strings, and storing strings and symbols in the source block list.
 *   This routine also translates keywords for token types.  
 */
tc_toktyp_t CTcTokenizer::next_on_line_xlat_keep()
{
    tc_toktyp_t typ;

    /* keep going until we find a valid symbol */
    for (;;)
    {
        /* skip whitespace and macro expansion flags */
        skip_ws_and_markers(&p_);
        
        /* see what we have */
        switch(p_.getch())
        {
        case '"':
        case '\'':
            /* it's a string - translate and save it */
            return xlat_string_to_src(&main_in_embedding_, FALSE);

        case '>':
            /* if we're in an embedding, this is the end of it */
            if (main_in_embedding_.in_expr()
                && main_in_embedding_.parens() == 0
                && p_.getch_at(1) == '>')
                return xlat_string_to_src(&main_in_embedding_, FALSE);

            /* use the normal parsing */
            goto do_normal;

        case 'R':
            /* check for a regex string */
            if (p_.getch_at(1) == '"' || p_.getch_at(1) == '\'')
                return xlat_string_to_src(&main_in_embedding_, FALSE);

            /* not a regex string - fall through to the default case */
            
        default:
        do_normal:
            /* for anything else, use the default tokenizer */
            typ = next_on_line(&p_, &curtok_, &main_in_embedding_, FALSE);
            
            /* check the token type */
            switch(typ)
            {
            case TOKT_SYM:
                /* symbol */
                {
                    const char *p;
                    CTcHashEntryKw *kw;
                
                    /* look it up in the keyword table */
                    kw = (CTcHashEntryKw *)kw_->find(curtok_.get_text(),
                        curtok_.get_text_len());
                    if (kw != 0)
                    {
                        /* replace the token with the keyword token type */
                        typ = kw->get_tok_id();
                        curtok_.settyp(typ);
                    }
                    else
                    {
                        /* ordinary symbol - save the text */
                        p = store_source(curtok_.get_text(),
                                         curtok_.get_text_len());
                        
                        /* 
                         *   change the token's text to point to the
                         *   source block, so that this token's text
                         *   pointer will remain permanently valid (the
                         *   original copy, in the source line buffer,
                         *   will be overwritten as soon as we read
                         *   another source line; we don't want the caller
                         *   to have to worry about this, so we return the
                         *   permanent copy) 
                         */
                        curtok_.set_text(p, curtok_.get_text_len());
                    }
                }
                break;

            case TOKT_FLOAT:
            case TOKT_BIGINT:
                /* floating-point number (or promoted large integer) */
                {
                    const char *p;

                    /* 
                     *   save the text so that it remains permanently
                     *   valid - we keep track of floats by the original
                     *   text, and let the code generator produce the
                     *   appropriate object file representation 
                     */
                    p = store_source(curtok_.get_text(),
                                     curtok_.get_text_len());
                    curtok_.set_text(p, curtok_.get_text_len());
                }
                break;

            case TOKT_INVALID:
                /* 
                 *   check for unmappable characters - these will show up as
                 *   Unicode U+FFFD, the "replacement character"; log it as
                 *   'unmappable' if applicable, otherwise as an invalid
                 *   character 
                 */
                if (utf8_ptr::s_getch(curtok_.get_text()) == 0xfffd)
                    log_error_curtok(TCERR_UNMAPPABLE_CHAR);
                else
                    log_error_curtok(TCERR_INVALID_CHAR);

                /* skip this character */
                p_.inc();
                
                /* keep going */
                continue;

            default:
                break;
            }
        }

        /* return the type */
        return typ;
    }
}


/*
 *   Translate the string at the current token position in the input
 *   stream to the source block list.  
 */
tc_toktyp_t CTcTokenizer::xlat_string_to_src(
    tok_embed_ctx *ec, int force_embed_end)
{
    /* 
     *   Reserve space for the entire rest of the line.  This is
     *   conservative, in that we will definitely need less space than
     *   this.  This might cause us to waste a little space here and
     *   there, since we will over-allocate when we have a short string
     *   early in a long line, but this will save us the time of scanning
     *   the string twice just to see how long it is. 
     */
    reserve_source(curbuf_->get_text_len() -
                   (p_.getptr() - curbuf_->get_text()));

    /* translate into the source block */
    tc_toktyp_t typ = xlat_string_to(
        src_ptr_, &p_, &curtok_, ec, force_embed_end);

    /* commit the space in the source block */
    commit_source(curtok_.get_text_len() + 1);

    /* return the string token */
    return typ;
}

/*
 *   Translate a string, setting up the token structure for the string,
 *   and writing the translated version of the string directly over the
 *   original source buffer of the string.
 *   
 *   Since a translated string can only shrink (because a translated
 *   escape sequence is always shorter than the original source version),
 *   we don't need a separate buffer, but can simply translate into the
 *   source buffer, overwriting the original string as we go.  
 */
tc_toktyp_t CTcTokenizer::xlat_string(utf8_ptr *p, CTcToken *tok,
                                      tok_embed_ctx *ec)
{
    /* 
     *   write the translated string over the original string's text,
     *   starting at the character after the quote 
     */
    char *dst = p->getptr() + 1;

    /* translate the string into our destination buffer */
    return xlat_string_to(dst, p, tok, ec, FALSE);
}

/*
 *   Count a run of consecutive quotes 
 */
static int count_quotes(const utf8_ptr *p, wchar_t qu)
{
    /* count quotes */
    int cnt = 0;
    for (utf8_ptr qp((utf8_ptr *)p) ; qp.getch() == qu ; qp.inc(), ++cnt);

    /* return the count */
    return cnt;
}

/*
 *   Translate a string, setting up the token structure for the string.
 *   We'll update the line buffer in-place to incorporate the translated
 *   string text.
 */
tc_toktyp_t CTcTokenizer::xlat_string_to(
    char *dstp, utf8_ptr *p, CTcToken *tok, tok_embed_ctx *ec,
    int force_embed_end)
{
    /* set up our output utf8 pointer */
    utf8_ptr dst(dstp);

    /* note the open quote character */
    wchar_t qu = p->getch();

    /* set the appropriate string token type */
    tok->settyp(qu == '"' ? TOKT_DSTR :
                qu == '\'' ? TOKT_SSTR :
                qu == 'R' ? TOKT_RESTR :
                ec != 0 ? ec->endtok() :
                TOKT_INVALID);

    /* if it's a regex string, skip the 'R' and note the actual quote type */
    if (qu == 'R')
    {
        p->inc();
        qu = p->getch();
    }

    /* 
     *   If we're at a quote (rather than at '>>' for continuing from an
     *   embedded expression), count consecutive open quotes, to determine if
     *   we're in a triple-quoted string.  
     */
    int triple = FALSE;
    if (qu == '"' || qu == '\'')
    {
        /* count the consecutive open quotes */
        if (count_quotes(p, qu) >= 3)
        {
            /* skip past the additional two open quotes */
            p->inc();
            p->inc();

            /* note that we're in a triple-quoted string */
            triple = TRUE;
        }
    }

    /* skip the open quote */
    p->inc();

    /* check for the end of an embedded expression (forced or actual) */
    if (force_embed_end)
    {
        /* 
         *   they want us to assume the embedding ends here, regardless of
         *   what we're looking at - act the same as though we had
         *   actually seen '>>', but don't skip any input (in fact, back
         *   up one, since we already skipped one character for what we
         *   had thought was the open quote 
         */
        p->dec();

        /* restore the enclosing string context */
        qu = ec->qu();
        triple = ec->triple();
        tok->settyp(ec->endtok());

        /* clear the caller's in-embedding status */
        ec->end_expr();
    }
    else if (qu == '>' && ec->parens() == 0)
    {
        /* skip the second '>' */
        p->inc();

        /* restore the enclosing string context */
        qu = ec->qu();
        triple = ec->triple();

        /* end the expression */
        ec->end_expr();
    }

    /* scan the string and translate quotes */
    for (;;)
    {
        /* get this character */
        wchar_t cur = p->getch();

        /* if this is the matching quote, we're done */
        if (cur == qu)
        {
            /* 
             *   If we're in a triple-quote string, count consecutive quotes.
             *   Triple quotes are greedy: if we have N>3 quotes in a row,
             *   the first N-3 are inside the string, and the last 3 are the
             *   terminating quotes. 
             */
            if (triple)
            {
                /* we need at least three quotes to end the string */
                int qcnt = count_quotes(p, qu);
                if (qcnt >= 3)
                {
                    /* copy all but the last three quotes to the output */
                    for ( ; qcnt > 3 ; --qcnt, p->inc())
                        dst.setch(qu);

                    /* skip the three quotes */
                    p->inc_by(3);

                    /* done with the string */
                    break;
                }
            }
            else
            {
                /* 
                 *   It's an ordinary string, which ends with just one
                 *   matching quote, so we're done no matter what follows.
                 *   Skip the quote and stop scanning.
                 */
                p->inc();
                break;
            }
        }

        /* 
         *   if we find an end-of-line within the string, it's an error -
         *   we should always splice strings together onto a single line
         *   before starting to tokenize the line 
         */
        if (cur == '\0')
        {
            /* set the token's text pointer */
            size_t bytelen = dst.getptr() - dstp;
            tok->set_text(dstp, bytelen);

            /* null-terminate the result string */
            dst.setch('\0');

            /* 
             *   get the length of the unterminated string so far, but for
             *   error logging, limit the length to twenty characters --
             *   we just want to give the user enough information to find
             *   the string in error, without making the error message
             *   huge 
             */
            utf8_ptr dp(dstp);
            size_t charlen = dp.len(bytelen);
            if (charlen > 20)
                bytelen = dp.bytelen(20);

            /*
             *   Check for a special heuristic case.  If the string was of
             *   zero length, and we have something sitting in our
             *   unsplice buffer, here's what probably happened: the input
             *   was missing a ">>" sequence at the end of an embedded
             *   expression, and the parser told us to put it back in.  We
             *   had earlier decided we needed to splice up to a quote to
             *   end what looked to us like an unterminated string.  If
             *   this is the case, we and the parser are working at cross
             *   purposes; the parser is smarter than we are, so we should
             *   synchronize with it.  
             */
            if (tok->get_text_len() == 0
                && qu == '"'
                && unsplicebuf_.get_text_len() != 0)
            {
                /* 
                 *   we must have spliced a line to finish a string -
                 *   insert the quote into the splice buffer, and ignore
                 *   it here 
                 */

                /* 
                 *   make sure there's room for one more character (plus a
                 *   null byte) 
                 */
                unsplicebuf_.ensure_space(unsplicebuf_.get_text_len() + 2);

                /* get the buffer pointer */
                char *buf = unsplicebuf_.get_buf();

                /* make room for the '"' */
                memmove(buf + 1, buf, unsplicebuf_.get_text_len());
                unsplicebuf_.set_text_len(unsplicebuf_.get_text_len() + 1);

                /* add the '"' */
                *buf = '"';

                /* 
                 *   return the 'null token' to tell the caller to try
                 *   again - do not log an error at this point 
                 */
                return TOKT_NULLTOK;
            }

            /* log the error */
            log_error(TCERR_UNTERM_STRING,
                      (char)qu, (int)bytelen, dstp, (char)qu);

            /* return the string type */
            return tok->gettyp();
        }

        /* if this is an escape, translate it */
        if (cur == '\\')
        {
            /* translate the escape */
            xlat_escape(&dst, p, qu, triple);
            continue;
        }
        else if (ec != 0
                 && tok->gettyp() != TOKT_RESTR
                 && cur == '<' && p->getch_at(1) == '<')
        {
            /* 
             *   It's the start of an embedded expression - change the type
             *   to so indicate.  If we think we have a regular SSTR or DSTR,
             *   switch to the appropriate START type, since the part up to
             *   here is actually the starting fragment of a string with an
             *   embedded expression.  If we think we're in an END section,
             *   switch to the corresponding MID section, since we're parsing
             *   a fragment that already followed an embedding.  The only
             *   other possibility is that we're in a MID section, in which
             *   case just stay in the same MID section.  
             */
            tc_toktyp_t tt = tok->gettyp();
            tok->settyp(tt == TOKT_DSTR ? TOKT_DSTR_START :
                        tt == TOKT_DSTR_END ? TOKT_DSTR_MID :
                        tt == TOKT_SSTR ? TOKT_SSTR_START :
                        tt == TOKT_SSTR_END ? TOKT_SSTR_MID :
                        tt);

            /* skip the << */
            p->inc_by(2);

            /*
             *   Check for a '%' sprintf-style formatting sequence.  If we
             *   have a '%' immediately after the second '<', it's a sprintf
             *   format code. 
             */
            if (p->getch() == '%')
            {
                /* remember the starting point of the format string */
                utf8_ptr fmt(p);

                /* scan the format spec */
                scan_sprintf_spec(p);

                /* translate escapes */
                utf8_ptr dst(&fmt);
                xlat_escapes(&dst, &fmt, p);

                /* push the format spec into the token stream */
                CTcToken ftok;
                ftok.set_text(fmt.getptr(), dst.getptr() - fmt.getptr());
                ftok.settyp(TOKT_FMTSPEC);
                push(&ftok);
            }

            /* tell the caller we're in an embedding */
            ec->start_expr(qu, triple, TRUE);

            /* stop scanning */
            break;
        }

        /* copy this character to the output position */
        dst.setch(cur);

        /* get the next character */
        p->inc();
    }

    /* set the token's text pointer */
    tok->set_text(dstp, dst.getptr() - dstp);

    /* null-terminate the result string */
    dst.setch('\0');

    /* return the string type */
    return tok->gettyp();
}

/*
 *   Translate all escapes in a string 
 */
void CTcTokenizer::xlat_escapes(utf8_ptr *dst, const utf8_ptr *srcp,
                                const utf8_ptr *endp)
{
    /* set up writable copy of the source */
    utf8_ptr p(srcp);
    
    /* scan the string */
    while (p.getptr() < endp->getptr())
    {
        /* check for an escape */
        wchar_t ch = p.getch();
        if (ch == '\\')
        {
            /* escape - translate it */
            xlat_escape(dst, &p, 0, FALSE);
        }
        else
        {
            /* ordinary character - copy it as is */
            dst->setch(ch);
            p.inc();
        }
    }
}

/*
 *   Skip a \ escape sequence 
 */
void CTcTokenizer::skip_escape(utf8_ptr *p)
{
    if (p->getch() == '\\')
    {
        /* translate an escape into an empty buffer */
        char buf[10];
        utf8_ptr dst(buf);
        xlat_escape(&dst, p, 0, FALSE);
    }
    else
    {
        /* just skip the character */
        p->inc();
    }
}

/*
 *   Translate a \ escape sequence 
 */
void CTcTokenizer::xlat_escape(utf8_ptr *dst, utf8_ptr *p,
                               wchar_t qu, int triple)
{
    int i, acc;
    
    /* get the character after the escape */
    p->inc();
    wchar_t cur = p->getch();

    /* see what we have */
    switch(cur)
    {
    case '^':
        /* caps - 0x000F */
        cur = 0x000F;
        break;
        
    case '"':
    case '\'':
        /* 
         *   If we're in triple-quote mode, and this is the matching quote, a
         *   single backslash escapes a whole run of consecutive quotes, so
         *   skip the whole run.  
         */
        if (triple && cur == qu)
        {
            /* copy and skip all consecutive quotes */
            for ( ; p->getch() == qu ; dst->setch(cur), p->inc()) ;
            
            /* we're done */
            return;
        }
        break;
        
    case 'v':
        /* miniscules - 0x000E */
        cur = 0x000E;
        break;
        
    case 'b':
        /* blank line - 0x000B */
        cur = 0x000B;
        break;
        
    case ' ':
        /* quoted space - 0x0015 */
        cur = 0x0015;
        break;

    case 'n':
        /* newline - explicitly use Unicode 10 character */
        cur = 10;
        break;

    case 'r':
        /* return - explicitly use Unicode 13 character */
        cur = 13;
        break;

    case 't':
        /* tab - explicitly use Unicode 9 character */
        cur = 9;
        break;

    case 'u':
        /* 
         *   Hex unicode character number.  Read up to 4 hex digits that
         *   follow the 'u', and use that as a Unicode character ID.  
         */
        for (i = 0, acc = 0, p->inc() ; i < 4 ; ++i, p->inc())
        {
            /* get the next character */
            cur = p->getch();
            
            /* 
             *   if it's another hex digit, add it into the accumulator;
             *   otherwise, we're done 
             */
            if (is_xdigit(cur))
                acc = 16*acc + value_of_xdigit(cur);
            else
                break;
        }
        
        /* use the accumulated value as the character number */
        dst->setch((wchar_t)acc);
        
        /* we've already skipped ahead to the next character, so we're done */
        return;
        
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
        /* 
         *   Octal ASCII character number.  Accumulate up to three octal
         *   numbers, and use the result as a character ID. 
         */
        for (i = 0, acc = 0 ; i < 3 ; ++i, p->inc())
        {
            /* get the next character */
            cur = p->getch();
            
            /* 
             *   if it's another digit, and it would leave our result in the
             *   0-255 range, count it; if not, we're done 
             */
            if (is_odigit(cur))
            {
                /* compute the new value */
                long new_acc = 8*acc + value_of_odigit(cur);
                
                /* if this would be too high, don't count it */
                if (new_acc > 255)
                    break;
                else
                    acc = new_acc;
            }
            else
                break;
        }
        
        /* use the accumulated value as the character number */
        dst->setch((wchar_t)acc);
        
        /* 
         *   continue with the current character, since we've already skipped
         *   ahead to the next one 
         */
        return;

    case 'x':
        /* 
         *   Hex ASCII character number.  Read up to two hex digits as a
         *   character number.
         */
        for (i = 0, acc = 0, p->inc() ; i < 2 ; ++i, p->inc())
        {
            /* get the next character */
            cur = p->getch();
            
            /* 
             *   if it's another hex digit, add it into the accumulator;
             *   otherwise, we're done 
             */
            if (is_xdigit(cur))
                acc = 16*acc + value_of_xdigit(cur);
            else
                break;
        }
        
        /* use the accumulated value as the character number */
        dst->setch((wchar_t)acc);
        
        /* 
         *   continue with the current character, since we've already skipped
         *   ahead to the next one 
         */
        return;
        
    case '<':
    case '>':
    case '\\':
        /* just copy these literally */
        break;
        
    default:
        /* log a pedantic error */
        log_pedantic(TCERR_BACKSLASH_SEQ, cur);
        
        /* copy anything else as-is, including the backslash */
        dst->setch('\\');
        break;
    }

    /* set the output character */
    dst->setch(cur);

    /* skip the current character */
    p->inc();
}

/*
 *   Scan a sprintf format spec 
 */
void CTcTokenizer::scan_sprintf_spec(utf8_ptr *p)
{
    /* skip the '%' */
    p->inc();

    /* scan the flags section */
    for (int done = FALSE ; !done ; )
    {
        switch (p->getch())
        {
        case '[':
            /* skip digits and the closing ']' */
            for (p->inc() ; is_digit(p->getch()) ; p->inc()) ;
            if (p->getch() == ']')
                p->inc();
            break;
            
        case '_':
            /* padding spec - skip this and the next character */
            p->inc();
            if (p->getch() != 0)
                skip_escape(p);
            break;
            
        case '-':
        case '+':
        case ' ':
        case ',':
        case '#':
            /* format spec - just skip it */
            p->inc();
            break;
            
        default:
            /* anything else means we're done with the flags */
            done = TRUE;
            break;
        }
    }
    
    /* scan the width */
    for ( ; is_digit(p->getch()) ; p->inc()) ;
    
    /* scan the precision */
    if (p->getch() == '.')
        for (p->inc() ; is_digit(p->getch()) ; p->inc()) ;
    
    /* the next character is the format spec - skip it */
    if (p->getch() != 0)
        skip_escape(p);
}


/*
 *   Skip a string, setting up the token structure for the string.  This
 *   routine only parses to the end of the line; if the line ends with the
 *   string unterminated, we'll flag an error.
 */
tc_toktyp_t CTcTokenizer::tokenize_string(
    utf8_ptr *p, CTcToken *tok, tok_embed_ctx *ec, int expanding)
{
    /* remember where the text starts */
    const char *start = p->getptr();

    /* check for a regex string - R'...' or R"..." */
    int regex = (p->getch() == 'R');
    if (regex)
        p->inc();

    /* note the quote type, for matching later */
    wchar_t qu = p->getch();

    /* skip the quote in the input */
    p->inc();

    /* check for triple quotes */
    int triple = FALSE;
    if ((qu == '"' || qu == '\'') && count_quotes(p, qu) >= 2)
    {
        /* note the triple quotes */
        triple = TRUE;

        /* skip the open quotes */
        p->inc();
        p->inc();
    }

    /* determine the token type based on the quote type */
    tc_toktyp_t typ;
    int allow_embedding;
    switch(qu)
    {
    case '\'':
        /* single-quoted string */
        typ = TOKT_SSTR;
        allow_embedding = (ec != 0);
        break;

    case '"':
        /* regular double-quoted string */
        typ = TOKT_DSTR;
        allow_embedding = (ec != 0);
        break;

    case '>':
        /* get the ending type for the embedding */
        if (ec != 0)
        {
            /* return to the enclosing string context */
            typ = ec->endtok();
            qu = ec->qu();
            triple = ec->triple();

            /* allow more embeddings */
            allow_embedding = TRUE;

            /* exit the expression context */
            ec->end_expr();
        }
        else
        {
            /* we can only finish an embedding if we were already in one */
            typ = TOKT_INVALID;
            qu = '"';
            allow_embedding = FALSE;
        }

        /* skip the extra '>' character */
        p->inc();
        break;

    default:
        /* anything else is invalid */
        typ = TOKT_INVALID;
        qu = '"';
        allow_embedding = FALSE;
        break;
    }

    /* if it's a regex string, it has special handling */
    if (regex)
    {
        /* embeddings aren't allowed in regex strings */
        allow_embedding = FALSE;

        /* the token type is 'regex string' */
        typ = TOKT_RESTR;
    }

    /* this is where the string's contents start */
    const char *contents_start = p->getptr();

    /* we don't know where it ends yet */
    const char *contents_end;

    /* scan the string */
    for (;;)
    {
        /* get the current character */
        wchar_t cur = p->getch();

        /* see what we have */
        if (cur == '\\')
        {
            /* escape sequence - skip an extra character */
            p->inc();

            /* 
             *   if we're in a triple-quoted string, and this matches the
             *   quote type, a single '\' escapes all consecutive quotes 
             */
            if (triple && p->getch() == qu)
            {
                /* skip the whole run of quotes */
                for ( ; p->getch() == qu ; p->inc()) ;

                /* take it from the top, as we've already skipped them all */
                continue;
            }
        }
        else if (cur == '<' && allow_embedding && p->getch_at(1) == '<')
        {
            /* 
             *   it's the start of an embedded expression - return the
             *   appropriate embedded string part type 
             */
            typ = (typ == TOKT_DSTR ? TOKT_DSTR_START :
                   typ == TOKT_DSTR_END ? TOKT_DSTR_MID :
                   typ == TOKT_SSTR ? TOKT_SSTR_START :
                   typ == TOKT_SSTR_END ? TOKT_SSTR_MID :
                   typ);

            /* remember that we're in an embedding in the token stream */
            ec->start_expr(qu, triple, !expanding);

            /* this is where the contents end */
            contents_end = p->getptr();

            /* skip the two embedding characters */
            p->inc();
            p->inc();

            /* we're done - set the text in the token */
            tok->set_text(start, p->getptr() - start);

            /* done */
            break;
        }
        else if (cur == qu)
        {
            /* 
             *   if we're in a triple-quoted string, it ends at a triple
             *   quote, except that any additional consecutive quotes go
             *   inside the string rather than outside 
             */
            if (triple)
            {
                /* we need at least three quotes in a row to end the string */
                int qcnt = count_quotes(p, qu);
                if (qcnt >= 3)
                {
                    /* the contents include any quotes before the last 3 */
                    p->inc_by(qcnt - 3);
                    contents_end = p->getptr();

                    /* skip the three close quotes */
                    p->inc_by(3);
                }
                else
                {
                    /* it's not ending; skip the quotes and carry on */
                    p->inc_by(qcnt);
                    continue;
                }
            }
            else
            {
                /* note where the string ends */
                contents_end = p->getptr();

                /* skip the closing quote */
                p->inc();
            }

            /* we're done - set the text in the token */
            tok->set_text(start, p->getptr() - start);
            
            /* done */
            break;
        }
        else if (cur == '\0')
        {
            /* this is where the contents end */
            contents_end = p->getptr();

            /* 
             *   We have an unterminated string.  If we're evaluating a
             *   preprocessor constant expression, log an error; otherwise
             *   let it go for now, since we'll catch the error during the
             *   normal tokenizing pass for parsing. 
             */
            if (G_tok->in_pp_expr_)
                log_error(TCERR_PP_UNTERM_STRING);

            /* set the partial text */
            tok->set_text(start, p->getptr() - start);
            
            /* end of line - return with the string unfinished */
            break;
        }

        /* skip this character of input */
        p->inc();
    }

    /* 
     *   if we're not in preprocessor mode, and we're saving string text,
     *   write the string to the string text output file 
     */
    if (!G_tok->in_pp_expr_ && G_tok->string_fp_ != 0
        && contents_start != contents_end)
    {
        /* write the line, translating back to the source character set */
        G_tok->string_fp_map_
            ->write_file(G_tok->string_fp_, contents_start,
                         (size_t)(contents_end - contents_start));

        /* add a newline */
        G_tok->string_fp_->write("\n", 1);
    }

    /* set the type in the token */
    tok->settyp(typ);

    /* return the token type */
    return tok->gettyp();
}


/* ------------------------------------------------------------------------ */
/*
 *   Read a source line and handle preprocessor directives.  This routine
 *   will transparently handle #include, #define, and other directives;
 *   when this routine returns, the input buffer will have a line of text
 *   that contains no # directive.
 *   
 *   Returns zero on success, non-zero upon reaching the end of the input.
 */
int CTcTokenizer::read_line_pp()
{
    /* 
     *   Read the next line from the input.  If that fails, return an end
     *   of file indication.  
     */
    int ofs = read_line(FALSE);
    if (ofs == -1)
        return 1;

    /* 
     *   before we process comments, note whether or not the line started
     *   out within a character string 
     */
    int started_in_string = (in_quote_ != '\0');
    
    /* set up our source pointer to the start of the new line */
    start_new_line(&linebuf_, ofs);

    /* skip leading whitespace */
    while (is_space(p_.getch()))
        p_.inc();
    
    /* 
     *   If this line begins with a '#', process the directive.  Ignore
     *   any initial '#' if the line started off in a string.  
     */
    if (!started_in_string && p_.getch() == '#' && allow_pp_)
    {
        struct pp_kw_def
        {
            const char *kw;
            int process_in_false_if;
            void (CTcTokenizer::*func)();
        };
        static pp_kw_def kwlist[] =
        {
            { "charset", FALSE, &CTcTokenizer::pp_charset },
            { "pragma",  FALSE, &CTcTokenizer::pp_pragma },
            { "include", FALSE, &CTcTokenizer::pp_include },
            { "define",  FALSE, &CTcTokenizer::pp_define },
            { "if",      TRUE,  &CTcTokenizer::pp_if },
            { "ifdef",   TRUE,  &CTcTokenizer::pp_ifdef },
            { "ifndef",  TRUE,  &CTcTokenizer::pp_ifndef },
            { "else",    TRUE,  &CTcTokenizer::pp_else },
            { "elif",    TRUE,  &CTcTokenizer::pp_elif },
            { "endif",   TRUE,  &CTcTokenizer::pp_endif },
            { "error",   FALSE, &CTcTokenizer::pp_error },
            { "undef",   FALSE, &CTcTokenizer::pp_undef },
            { "line",    FALSE, &CTcTokenizer::pp_line },
            { 0, 0, 0 }
        };
        pp_kw_def *kwp;
        const char *kwtxt;
        size_t kwlen;
        
        /* skip the '#' */
        p_.inc();
        
        /*
         *   If the line ended inside a comment, read the next line until
         *   we're no longer in a comment.  The ANSI C preprocessor rules
         *   say that a newline in a comment should not be treated as a
         *   lexical newline, so pretend that the next line is part of the
         *   preprocessor line in such a case. 
         */
        while (str_->is_in_comment())
        {
            size_t p_ofs;

            /* remember the current offset in the line buffer */
            p_ofs = p_.getptr() - linebuf_.get_buf();

            /* append another line - stop at the end of the stream */
            if (read_line(TRUE) == -1)
                break;

            /* restore the line pointer, in case the buffer moved */
            start_new_line(&linebuf_, p_ofs);
        }
        
        /* read the directive */
        next_on_line();

        /* 
         *   if we've reached the end of the line, it's a null directive;
         *   simply return an empty line 
         */
        if (curtok_.gettyp() == TOKT_EOF)
        {
            clear_linebuf();
            return 0;
        }

        /* get the text and length of the keyword */
        kwtxt = curtok_.get_text();
        kwlen = curtok_.get_text_len();

        /* if it's not a symbol, it's not a valid directive */
        if (curtok_.gettyp() != TOKT_SYM)
        {
            /* log the error and return an empty line */
            log_error(TCERR_INV_PP_DIR, (int)kwlen, kwtxt);
            clear_linebuf();
            return 0;
        }
        
        /* determine which keyword we have, and process it */
        for (kwp = kwlist ; kwp->kw != 0 ; ++kwp)
        {
            /* is this our keyword? */
            if (strlen(kwp->kw) == kwlen
                && memcmp(kwtxt, kwp->kw, kwlen) == 0)
            {
                /*
                 *   This is our directive.
                 *   
                 *   If we're in the false branch of a #if block, only
                 *   process the directive if it's a kind of directive
                 *   that we should process in false #if branches.  The
                 *   only directives that we process in #if branches are
                 *   those that would affect the #if branching, such as a
                 *   #endif or a nested #if.  
                 */
                if (!in_false_if() || kwp->process_in_false_if)
                {
                    /* invoke the handler to process the directive */
                    (this->*(kwp->func))();
                }
                else
                {
                    /* 
                     *   we're in a #if branch not taken - simply clear
                     *   the buffer 
                     */
                    clear_linebuf();
                }
                
                /* we don't need to look any further */
                break;
            }
        }

        /* 
         *   if we didn't find the keyword, log an error and otherwise
         *   ignore the entire line 
         */
        if (kwp->kw == 0)
            log_error(TCERR_INV_PP_DIR, (int)kwlen, kwtxt);
        
        /*
         *   Preprocessor lines must always be entirely self-contained.
         *   Therefore, it's not valid for a string to start on a
         *   preprocessor line and continue onto subsequent lines.  If
         *   we're marked as being inside a string, there must have been
         *   an error on the preprocessor line.  Simply clear the
         *   in-string flag; we don't need to issue an error at this
         *   point, since the preprocessor line handler should have
         *   already caught the problem and reported an error.  
         */
        in_quote_ = '\0';
    }
    else
    {
        /*
         *   There's no preprocessor directive.
         *   
         *   If we're in a false #if branch, return an empty line.  We
         *   return an empty line rather than skipping to the next line so
         *   that the caller sees the same number of lines as are in the
         *   original source.  
         */
        if (in_false_if())
        {
            /* 
             *   it's a #if not taken - we don't want to compile the line
             *   at all, so just clear it out 
             */
            clear_linebuf();
            expbuf_.clear_text();
        }
        else
        {
            /*
             *   If we ended the line in a string, splice additional lines
             *   onto the end of this line until we find the end of the
             *   string, then unsplice the part after the end of the
             *   string. 
             */
            if (in_quote_ != '\0')
            {
                /* splice additional lines to finish the quote */
                splice_string();
            }
            
            /*
             *   Expand macros in the line, splicing additional source
             *   lines if necessary to fill out any incomplete actual
             *   parameter lists.  
             */
            start_new_line(&linebuf_, 0);
            expand_macros_curline(TRUE, FALSE, FALSE);
        }

        /* store the line in the appropriate place */
        if (pp_only_mode_)
        {
            /* 
             *   we're only preprocessing - store the macro-expanded line
             *   back in the line buffer so that the caller can read out
             *   the final preprocessed text 
             */
            linebuf_.copy(expbuf_.get_text(), expbuf_.get_text_len());
        }
        else
        {
            /* 
             *   We're compiling - simply read subsequent tokens out of
             *   the expansion buffer.  
             */
            start_new_line(&expbuf_, 0);
        }
    }

    /* return success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Read the next line from the input file.  Returns a pointer to the
 *   start of the newly-read data on success, or null if we reach the end
 *   of the input.
 *   
 *   If 'append' is true, we'll add the line on to the end of the existing
 *   buffer; otherwise, we'll overwrite what's in the buffer.
 *   
 *   The only preprocessing performed in this routine is line-splicing.
 *   Any line that ends with a backslash character will be spliced with
 *   the following line, with the backslash and newline removed.
 *   
 *   The new line will be stored in our internal buffer, and will be
 *   null-terminated with the trailing newline removed.
 *   
 *   If we reach the end of the current file, and there's an enclosing
 *   file, we'll resume reading from the enclosing file.  Hence, when this
 *   routine returns non-zero, it indicates that we've reached the end of
 *   the entire source, not just of the current file.  
 */
int CTcTokenizer::read_line(int append)
{
    /* if there's no input stream, indicate end-of-file */
    if (str_ == 0)
        return -1;

    /* if we're not appending, clear out the line buffer */
    if (!append)
    {
        /* start with an empty line */
        clear_linebuf();

        /* note the current input position */
        last_desc_ = str_->get_desc();
        last_linenum_ = str_->get_next_linenum();
    }

    /* note where the new data starts */
    size_t len = linebuf_.get_text_len();
    size_t start_len = len;

    /* 
     *   if there's anything in the unsplice buffer, use it as the new
     *   line 
     */
    if (unsplicebuf_.get_text_len() != 0)
    {
        /* 
         *   Copy the unsplice buffer as the current line.  Note that we
         *   don't have to worry about any of the complicated cases, such
         *   as whether or not it ends with a newline or a backslash,
         *   because the unspliced line was already processed as an input
         *   line when we read it in the first place. 
         */
        linebuf_.append(unsplicebuf_.get_text(), unsplicebuf_.get_text_len());
        
        /* clear the unsplice buffer, since it's been consumed now */
        unsplicebuf_.clear_text();

        /* 
         *   make the current line the appended line - if we're
         *   unsplicing, it means that we appended, so the current line is
         *   now the line from which the last appended text came 
         */
        last_desc_ = appended_desc_;
        last_linenum_ = appended_linenum_;

        /* return the offset of the new text */
        return start_len;
    }

    /* if we're appending, note where the appendage is coming from */
    if (append)
    {
        /* remember the last source line appended */
        appended_desc_ = str_->get_desc();
        appended_linenum_ = str_->get_next_linenum();
    }

    /* keep going until we finish reading the input line */
    for ( ;; )
    {
        /* read a line of text from the input file */
        size_t curlen = str_->get_src()->read_line(
            linebuf_.get_buf() + len, linebuf_.get_buf_size() - len);

        /* check for end of file */
        if (curlen == 0)
        {
            /*
             *   We've reached the end of the current input stream.  If
             *   we've already read anything into the current line, it
             *   means that the file ended in mid-line, without a final
             *   newline character; ignore this and proceed with the line
             *   as it now stands in this case.  
             */
            if (len > start_len)
                break;

            /* 
             *   We've finished with this stream.  If there's a parent
             *   stream, return to it; otherwise, we're at the end of the
             *   source.  
             */

            /* 
             *   if we didn't close all of the #if/#ifdef levels opened
             *   within this file, flag one or more errors 
             */
            while (if_sp_ > str_->get_init_if_level())
            {
                /* get the filename from the #if stack */
                const char *fname = if_stack_[if_sp_ - 1].desc->get_fname();

                /* if we're in test reporting mode, use the root name only */
                if (test_report_mode_)
                    fname = os_get_root_name((char *)fname);

                /* log the error */
                log_error(TCERR_IF_WITHOUT_ENDIF,
                          if_stack_[if_sp_ - 1].linenum,
                          (int)strlen(fname), fname);

                /* discard the #if level */
                pop_if();
            }

            /* remember the old stream */
            CTcTokStream *old_str = str_;

            /* return to the parent stream, if there is one */
            str_ = str_->get_parent();

            /* delete the old stream now that we're done with it */
            delete old_str;

            /* note the new file the line will be coming from */
            if (!append && str_ != 0)
            {
                last_desc_ = str_->get_desc();
                last_linenum_ = str_->get_next_linenum();
            }

            /* if there's no stream, return end of file */
            if (str_ == 0)
                return -1;

            /* 
             *   restore the #pragma newline_spacing mode that was in effect
             *   when we interrupted the parent stream 
             */
            string_newline_spacing_ = str_->get_newline_spacing();

            /* if there's a parser, notify it of the new pragma C mode */
#if 0 // #pragma C is not currently used
            if (G_prs != 0)
                G_prs->set_pragma_c(str_->is_pragma_c());
#endif

            /* go back to read the next line from the parent */
            continue;
        }

        /* set the new length of the buffer contents */
        len += curlen - 1;
        linebuf_.set_text_len(len);

        /*
         *   Check the result to see if it ends in a newline.  If not, it
         *   means either that we don't have room in the buffer for the
         *   full source line, or we've reached the last line in the file,
         *   and it doesn't end with a newline.
         *   
         *   Note that the file reader will always supply us with '\n'
         *   newlines, regardless of the local operating system
         *   conventions.
         *   
         *   Also, check to see if the line ends with '\\'.  If so, remove
         *   the '\\' character and read the next line, since this
         *   indicates that the logical line continues onto the next
         *   newline-deliminted line.  
         */
        if (len != 0 && linebuf_.get_text()[len - 1] != '\n')
        {
            /* 
             *   There's no newline, hence the file reader wasn't able to
             *   fit the entire line into our buffer, or else we've read
             *   the last line in the file and there's no newline at the
             *   end.  If we haven't reached the end of the file, expand
             *   our line buffer to make room to read more from this same
             *   line.  
             */
            if (!str_->get_src()->at_eof())
                linebuf_.expand();
        }
        else if (len > 1 && linebuf_.get_text()[len - 2] == '\\')
        {
            /* 
             *   There's a backslash at the end of the line, so they want
             *   to continue this logical line.  Remove the backslash, and
             *   read the next line onto the end of the current line.
             *   
             *   Note that we must remove two characters from the end of
             *   the line (and tested for buf_[len-2] above) because we
             *   have both a backslash and a newline at the end of the
             *   line.  
             */
            len -= 2;
            linebuf_.set_text_len(len);

            /* count reading the physical line */
            str_->count_line();
        }
        else
        {
            /* remove the newline from the buffer */
            if (len != 0)
            {
                --len;
                linebuf_.set_text_len(len);
            }
            
            /* count reading the line */
            str_->count_line();

            /* done */
            break;
        }
    }

    /* 
     *   remove comments from the newly-read material - this replaces each
     *   comment by a single whitespace character 
     */
    process_comments(start_len);

    /* 
     *   we've successfully read a line -- return the offset of the start of
     *   the newly-read text 
     */
    return start_len;
}

/*
 *   Un-splice a line at the given point.  This breaks the current source
 *   line in two, keeping the part before the given point as the current
 *   line, but making the part from the given point to the end of the line
 *   a new source line.  We'll put the new source line into a special
 *   holding buffer, and then fetch this part as a new line the next time
 *   we read a line in read_line(). 
 */
void CTcTokenizer::unsplice_line(const char *new_line_start)
{
    /* make sure the starting point is within the current line */
    if (!(new_line_start >= linebuf_.get_text()
          && new_line_start <= linebuf_.get_text() + linebuf_.get_text_len()))
    {
        /* note the error - this is an internal problem */
        throw_internal_error(TCERR_UNSPLICE_NOT_CUR);
        return;
    }

    /* calculate the length of the part we're keeping */
    size_t keep_len = new_line_start - linebuf_.get_text();

    /* 
     *   prepend the remainder of the current line into the unsplice buffer
     *   (we prepend it because the unsplice line is text that comes after
     *   the current line - so anything in the current line comes before
     *   anything already in the unsplice buffer) 
     */
    unsplicebuf_.prepend(new_line_start, linebuf_.get_text_len() - keep_len);

    /* cut off the current line at the given point */
    linebuf_.set_text_len(keep_len);
}


/* ------------------------------------------------------------------------ */
/*
 *   Store text in the source array 
 */
const char *CTcTokenizer::store_source(const char *txt, size_t len)
{
    /* reserve space for the text */
    reserve_source(len);

    /* store it */
    const char *p = store_source_partial(txt, len);

    /* add a null terminator */
    static const char nt[1] = { '\0' };
    store_source_partial(nt, 1);

    /* return the pointer to the stored space */
    return p;
}

/*
 *   Store partial source; use this AFTER reserving the necessary space.  If
 *   you want null-termination, be sure to reserve the extra byte for that
 *   and include it in the string.  This can be used to build a string piece
 *   by piece; we simply add the text without null-terminating it.  
 */
const char *CTcTokenizer::store_source_partial(const char *txt, size_t len)
{
    /* remember where the string starts */
    const char *p = src_ptr_;

    /* store the text */
    memcpy(src_ptr_, txt, len);

    /* advance the source block write position and length */
    src_ptr_ += len;
    src_rem_ -= len;

    /* return the storage pointer */
    return p;
}

/*
 *   Reserve space for text in the source array.  This always reserves the
 *   requested amount of space, plus an extra byte for null termination.  
 */
void CTcTokenizer::reserve_source(size_t len)
{
    /* 
     *   if we don't have enough space for this line in the current source
     *   block, start a new block 
     */
    if (len + 1 > src_rem_)
    {
        CTcTokSrcBlock *blk;

        /* 
         *   if the line is too long for a source block, throw a fatal
         *   error 
         */
        if (len + 1 > TCTOK_SRC_BLOCK_SIZE)
            throw_fatal_error(TCERR_SRCLINE_TOO_LONG,
                              (long)TCTOK_SRC_BLOCK_SIZE);

        /* allocate a new block */
        blk = new CTcTokSrcBlock();

        /* link it into our list */
        src_cur_->set_next(blk);

        /* it's now the current block */
        src_cur_ = blk;

        /* start writing at the start of this block */
        src_rem_ = TCTOK_SRC_BLOCK_SIZE;
        src_ptr_ = blk->get_buf();
    }
}

/*
 *   Commit space previously reserved and now used in the source block
 *   list 
 */
void CTcTokenizer::commit_source(size_t len)
{
    /* advance the write position past the committed text */
    src_ptr_ += len;
    src_rem_ -= len;
}


/* ------------------------------------------------------------------------ */
/*
 *   Expand macros in the current line from the current source pointer,
 *   filling in expbuf_ with the expanded result.  
 */
int CTcTokenizer::expand_macros_curline(int read_more, int allow_defined,
                                        int append_to_expbuf)
{
    int err;
    
    /* expand macros in the current line */
    err = expand_macros(&linebuf_, &p_, &expbuf_, read_more, allow_defined,
                        append_to_expbuf);

    /* if that failed, return an error */
    if (err != 0)
        return err;

    /* 
     *   if we're in preprocessor mode, clean up the text for human
     *   consumption by removing our various expansion flags
     */
    if (pp_only_mode_)
        remove_expansion_flags(&expbuf_);

    /* return the result */
    return err;
}

/* ------------------------------------------------------------------------ */
/*
 *   Remove the special internal macro expansion flags from an expanded macro
 *   buffer. 
 */
void CTcTokenizer::remove_expansion_flags(CTcTokString *buf)
{
    utf8_ptr p;
    char *src;
    char *dst;

    /* 
     *   Scan the expansion buffer and remove all of the no-more-expansion
     *   flag bytes - we're done expanding the macro now, so we don't need
     *   this information any longer.  When we're writing out the
     *   preprocessed source for human viewing, we don't want to leave these
     *   internal markers in the expanded source.  
     */
    for (src = dst = buf->get_buf(), p.set(src) ; p.getch() != '\0' ; )
    {
        /* if this isn't a macro flag, copy it */
        if (p.getch() == TOK_MACRO_EXP_END)
        {
            /* skip the flag byte and the following embedded pointer */
            src += 1 + sizeof(CTcHashEntryPp *);
            p.set(src);
        }
        else if (p.getch() == TOK_FULLY_EXPANDED_FLAG)
        {
            /* skip the flag byte */
            ++src;
            p.set(src);
        }
        else
        {
            /* skip this character */
            p.inc();
            
            /* copy the bytes of this character as-is */
            while (src < p.getptr())
                *dst++ = *src++;
        }
    }

    /* set the new buffer length */
    buf->set_text_len(dst - buf->get_buf());
}

/* ------------------------------------------------------------------------ */
/*
 *   Expand macros in the current line, reading additional source lines if
 *   necessary.
 *   
 *   'src' is a pointer to the start of the text to expand; it must point
 *   into the 'srcbuf' buffer.  If 'src' is null, we'll simply start at
 *   the beginning of the source buffer.  
 */
int CTcTokenizer::expand_macros(CTcTokString *srcbuf, utf8_ptr *src,
                                CTcTokString *expbuf, int read_more,
                                int allow_defined, int append)
{
    tc_toktyp_t typ;
    CTcToken tok;
    CTcTokString *subexp;
    size_t startofs;
    utf8_ptr local_src;
    CTcTokStringRef local_srcbuf;
    CTcMacroRsc *res;
    int err;

    /* presume success */
    err = 0;

    /* get a macro expansion resource object */
    res = alloc_macro_rsc();
    if (res == 0)
        return 1;

    /* get our subexpression buffer from the resource object */
    subexp = &res->line_exp_;

    /* if there's no source buffer or source pointer, provide one */
    if (srcbuf == 0)
    {
        /* 
         *   there's no source buffer - provide our own non-allocated
         *   buffer tied to the caller's buffer 
         */
        local_srcbuf.set_buffer(src->getptr(), strlen(src->getptr()));
        srcbuf = &local_srcbuf;
    }
    else if (src == 0)
    {
        /* 
         *   there's no source pointer - start at the beginning of the
         *   source buffer 
         */
        local_src.set((char *)srcbuf->get_text());
        src = &local_src;
    }

    /* clear the expansion buffer, unless we're appending to the buffer */
    if (!append)
        expbuf->clear_text();

    /* 
     *   Make sure we have room for a copy of the source line.  This is an
     *   optimization for the simple case where we'll just copy the source
     *   line unchanged, so that we don't have to repeatedly expand the
     *   buffer; we will, however, expand the buffer dynamically later, if
     *   this pre-allocation should prove to be insufficient. 
     */
    expbuf->ensure_space(expbuf->get_text_len() + srcbuf->get_text_len());

    /* note the starting offset, if we have an underlying string buffer */
    startofs = src->getptr() - srcbuf->get_text();

    /* read the first token */
    typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_, TRUE);

    /* scan through the tokens on the line, looking for macros to expand */
    while (typ != TOKT_EOF)
    {
        /* 
         *   if it's a symbol, and it hasn't already been marked as fully
         *   expanded, look it up in the #define table 
         */
        if (typ == TOKT_SYM && !tok.get_fully_expanded())
        {
            CTcHashEntryPp *entry;

            /*
             *   Look up the symbol in the #define symbol table.  If we
             *   find it, expand the macro.  Otherwise, if the "defined"
             *   operator is active, check for that.
             *   
             *   Do not expand the macro if we find that it has already
             *   been expanded on a prior scan through the current text.  
             */
            entry = find_define(tok.get_text(), tok.get_text_len());
            if ((entry != 0
                 && !scan_for_prior_expansion(*src, srcbuf->get_text_end(),
                                              entry))
                || (allow_defined
                    && tok.get_text_len() == 7
                    && memcmp(tok.get_text(), "defined", 7) == 0))
            {
                size_t macro_ofs;
                size_t rem_len;
                int expanded;

                /* get the offset of the macro token in the source buffer */
                macro_ofs = tok.get_text() - srcbuf->get_text();
                
                /* expand it into our sub-expansion buffer */
                if (entry != 0)
                {
                    /* expand the macro */
                    err = expand_macro(res, subexp, srcbuf, src,
                                       macro_ofs, entry,
                                       read_more, allow_defined, &expanded);
                }
                else
                {
                    /* parse and expand the defined() operator */
                    err = expand_defined(subexp, srcbuf, src);

                    /* "defined" always expands if there's not an error */
                    expanded = TRUE;
                }

                /* if an error occurred, return failure */
                if (err)
                    goto done;

                /*
                 *   if we expanded something, append everything we
                 *   skipped preceding the macro, then rescan; otherwise,
                 *   just keep going without a rescan 
                 */
                if (expanded)
                {
                    /* copy the preceding text to the output */
                    expbuf->append(srcbuf->get_text() + startofs,
                                   macro_ofs - startofs);
                }
                else
                {
                    /* 
                     *   we didn't expand - get the next token after the
                     *   macro 
                     */
                    typ = next_on_line(srcbuf, src, &tok,
                                       &macro_in_embedding_, TRUE);

                    /* continue processing from this token */
                    continue;
                }

                /*
                 *   We must now insert the expansion into the source
                 *   buffer at the current point, and re-scan the
                 *   expansion, *along with* the rest of the original
                 *   source line (this is how ANSI C specifies the
                 *   process).
                 *   
                 *   If we can read more, we must be reading out of the
                 *   main input line buffer, so insert the expansion text
                 *   directly into the original source stream, and
                 *   continue reading out of the source stream; this will
                 *   simplify the case where we must read more data from
                 *   the file in the course of the expansion.  If we can't
                 *   read more, simply copy the remainder of the current
                 *   input line onto the expanded macro and use it as the
                 *   new input buffer.  
                 */

                /* get the current offset in the source line */
                startofs = src->getptr() - srcbuf->get_text();

                /* figure out how much is left on the current line */
                rem_len = srcbuf->get_text_len() - startofs;

                /* check to see if we can read more */
                if (read_more)
                {
                    /*
                     *   we're reading from the original line input buffer
                     *   -- insert the expansion into the source buffer at
                     *   the current point, replacing the original macro
                     *   text
                     */
                    
                    /* make sure we have room for adding the expansion text */
                    srcbuf->ensure_space(macro_ofs + rem_len
                                         + subexp->get_text_len());

                    /* make sure src is still pointing to the right place */
                    src->set(srcbuf->get_buf() + macro_ofs);
                
                    /* move the remainder of the current line to make room */
                    memmove(srcbuf->get_buf() + macro_ofs
                            + subexp->get_text_len(),
                            srcbuf->get_buf() + startofs,
                            rem_len);
                
                    /* insert the expansion text */
                    memcpy(srcbuf->get_buf() + macro_ofs, subexp->get_buf(),
                           subexp->get_text_len());
                
                    /* set the new source length */
                    srcbuf->set_text_len(macro_ofs + rem_len
                                         + subexp->get_text_len());

                    /* the new starting offset is the current position */
                    startofs = macro_ofs;

                    /* get the next token */
                    typ = next_on_line(srcbuf, src, &tok,
                                       &macro_in_embedding_, TRUE);
                    
                    /* continue processing from this token */
                    continue;
                }
                else
                {
                    /* 
                     *   we're reading from a read-only buffer -- add the
                     *   remainder of the source to the expansion buffer,
                     *   and recursively parse the remainder 
                     */
                    subexp->append(srcbuf->get_text() + startofs, rem_len);

                    /* 
                     *   evaluate the remainder recursively and append it
                     *   to the expansion already in progress 
                     */
                    err = expand_macros(subexp, 0, expbuf, FALSE,
                                        allow_defined, TRUE);

                    /* we're done */
                    goto done;
                }
            }
        }

        /* get the next token */
        typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_, TRUE);
    }

    /* add the remainder of the source to the output */
    expbuf->append(srcbuf->get_text() + startofs,
                   tok.get_text() - startofs - srcbuf->get_text());

done:
    /* release our macro resource object */
    release_macro_rsc(res);
    
    /* return the result */
    return err;
}

/*
 *   Allocate a macro resource object.  If we're out of resource objects
 *   in the pool, we'll add another object to the pool.  
 */
CTcMacroRsc *CTcTokenizer::alloc_macro_rsc()
{
    CTcMacroRsc *rsc;
    
    /* 
     *   if there's anything in the available list, take the first item
     *   off the list and return it 
     */
    if (macro_res_avail_ != 0)
    {
        /* remember the item to return */
        rsc = macro_res_avail_;

        /* remove it from the list */
        macro_res_avail_ = macro_res_avail_->next_avail_;

        /* return it */
        return rsc;
    }

    /* there's nothing on the available list - allocate a new item */
    rsc = new CTcMacroRsc();

    /* if that failed, return failure */
    if (rsc == 0)
    {
        log_error(TCERR_OUT_OF_MEM_MAC_EXP);
        return 0;
    }

    /* add it onto the master list */
    rsc->next_ = macro_res_head_;
    macro_res_head_ = rsc;

    /* return it */
    return rsc;
}

/*
 *   Release a macro resource, returning it to the pool 
 */
void CTcTokenizer::release_macro_rsc(CTcMacroRsc *rsc)
{
    /* put it back at the head of the available list */
    rsc->next_avail_ = macro_res_avail_;
    macro_res_avail_ = rsc;
}

/*
 *   Scan a buffer for a prior-expansion flag for a given macro.  We'll
 *   look through the buffer for a TOK_MACRO_EXP_END byte that mentions
 *   the given symbol table entry; we'll return true if found, false if
 *   not.  True means that the symbol has already been expanded on a prior
 *   scan of the text, so it should not be re-expanded now.  
 */
int CTcTokenizer::scan_for_prior_expansion(utf8_ptr src, const char *src_end,
                                           const CTcHashEntryPp *entry)
{
    /* scan the buffer for the expansion flag byte */
    while (src.getptr() < src_end)
    {
        /* if this is the flag, check what follows */
        if (src.getch() == TOK_MACRO_EXP_END)
        {
            CTcHashEntryPp *flag_entry;

            /* read the entry from the buffer */
            memcpy(&flag_entry, src.getptr() + 1, sizeof(flag_entry));

            /* if it matches, indicate that we found it */
            if (entry == flag_entry)
                return TRUE;

            /* it's not a match - keep scanning after this flag sequence */
            src.set(src.getptr() + 1 + sizeof(flag_entry));
        }
        else
        {
            /* it's not the flag - skip this character */
            src.inc();
        }
    }

    /* we didn't find it */
    return FALSE;
}

/*
 *   Go through a macro expansion and translate from end-of-expansion
 *   markers to individual token full-expansion markers.  This is used
 *   after we leave a recursion level to convert expanded text into text
 *   suitable for use in further expansion at an enclosing recursion
 *   level. 
 */
void CTcTokenizer::mark_full_exp_tokens(CTcTokString *dstbuf,
                                        const CTcTokString *srcbuf,
                                        int append) const
{
    utf8_ptr p;
    CTcToken tok;
    const char *start;
    tok_embed_ctx ec;

    /* clear the output buffer if we're not appending to existing text */
    if (!append)
        dstbuf->clear_text();

    /* remember the starting point */
    start = srcbuf->get_text();

    /* scan the source buffer */
    p.set((char *)start);
    for (;;)
    {
        CTcHashEntryPp *cur_entry;
        tc_toktyp_t typ;
        char ch;

        /* get the next token; stop at the end of the line */
        typ = next_on_line(srcbuf, &p, &tok, &ec, TRUE);
        if (typ == TOKT_EOF)
            break;

        /* 
         *   if this macro token is being expanded, and it's not already
         *   marked for no more expansion, mark it 
         */
        if (typ == TOKT_SYM
            && !tok.get_fully_expanded()
            && (cur_entry = find_define(tok.get_text(),
                                        tok.get_text_len())) != 0
            && scan_for_prior_expansion(p, srcbuf->get_text_end(), cur_entry))
        {
            /* 
             *   This token has been fully expanded in the substitution
             *   buffer but hasn't yet been marked as such - we must
             *   insert the fully-expanded marker.  First, add up to the
             *   current point to the output buffer.  
             */
            if (tok.get_text() > start)
                dstbuf->append(start, tok.get_text() - start);

            /* add the fully-expanded marker */
            ch = TOK_FULLY_EXPANDED_FLAG;
            dstbuf->append(&ch, 1);

            /* the new starting point is the start of the symbol token */
            start = tok.get_text();
        }
    }

    /* copy any remaining text to the output */
    if (tok.get_text() > start)
        dstbuf->append(start, tok.get_text() - start);

    /* 
     *   Remove any macro expansion end markers from the output buffer.
     *   We don't want to leave these around, because they don't apply to
     *   the enclosing buffer into which we'll substitute this result.
     *   Note that we've already ensured that these markers will be
     *   respected for the substitution text by inserting "fully expanded"
     *   markers in front of each token to which any of the markers we're
     *   removing should apply.  
     */
    remove_end_markers(dstbuf);
}


/*
 *   Remove end markers from a buffer 
 */
void CTcTokenizer::remove_end_markers(CTcTokString *buf)
{
    char *src;
    char *dst;
    utf8_ptr p;

    /* scan the buffer */
    for (src = dst = buf->get_buf(), p.set(src) ;
         p.getptr() < buf->get_text_end() ; )
    {
        /* check for our flag */
        if (p.getch() == TOK_MACRO_EXP_END)
        {
            /* skip the flag byte and the following embedded pointer */
            src += 1 + sizeof(CTcHashEntryPp *);
            p.set(src);
        }
        else
        {
            /* skip this character */
            p.inc();

            /* copy the bytes of this character as-is */
            while (src < p.getptr())
                *dst++ = *src++;
        }
    }

    /* set the new buffer size */
    buf->set_text_len(dst - buf->get_buf());
}


/*
 *   Expand the macro at the current token in the current line.
 *   
 *   'src' is a pointer to the current position in 'srcbuf'.  We'll update
 *   'src' to point to the next token after macro or its actual parameters
 *   list, if it has one.  
 */
int CTcTokenizer::expand_macro(CTcMacroRsc *rsc, CTcTokString *expbuf,
                               const CTcTokString *srcbuf, utf8_ptr *src,
                               size_t macro_srcbuf_ofs,
                               CTcHashEntryPp *entry, int read_more,
                               int allow_defined, int *expanded)
{
    CTcTokString *subexp;
    size_t argofs[TOK_MAX_MACRO_ARGS];
    size_t arglen[TOK_MAX_MACRO_ARGS];
    size_t startofs;
    const char *start;
    const char *end;
    int err;
    char flagbuf[1 + sizeof(entry)];

    /* presume we won't do any expansion */
    *expanded = FALSE;

    /* get our resources */
    subexp = &rsc->macro_exp_;

    /* remember our parsing starting offset */
    startofs = src->getptr() - srcbuf->get_text();

    /* clear the expansion output buffer */
    expbuf->clear_text();

    /* if the macro has arguments, scan the actuals */
    if (entry->has_args())
    {
        int found_actuals;

        /* read the macro arguments */
        if (parse_macro_actuals(srcbuf, src, entry, argofs, arglen,
                                read_more, &found_actuals))
        {
            err = 1;
            goto done;
        }

        /* 
         *   If we found no actuals, then this wasn't really an invocation
         *   of the macro after all - a function-like macro invoked with
         *   no arguments is simply not replaced.  Store the original text
         *   in the output buffer and return success.  
         */
        if (!found_actuals)
        {
            /* copy the original text */
            expbuf->copy(srcbuf->get_text() + macro_srcbuf_ofs,
                         startofs - macro_srcbuf_ofs);

            /* 
             *   restore the source read pointer to where it was when we
             *   started 
             */
            src->set((char *)srcbuf->get_text() + startofs);

            /* return success */
            err = 0;
            goto done;
        }
    }

    /* 
     *   if there are arguments, replace the macro and substitute actuals
     *   for the formals; otherwise, just copy the replacement text
     *   directly 
     */
    if (entry->get_argc() != 0)
    {
        /* substitute the actuals */
        if (substitute_macro_actuals(rsc, subexp, entry, srcbuf,
                                     argofs, arglen, allow_defined))
        {
            err = 1;
            goto done;
        }

        /* set up to parse from the expansion buffer */
        start = subexp->get_text();
        end = start + subexp->get_text_len();
    }
    else
    {
        /* 
         *   use our local source buffer that simply references the
         *   original expansion text, rather than making a copy of the
         *   expansion text 
         */
        start = entry->get_expansion();
        end = start + entry->get_expan_len();
    }

    /* copy the expansion into the output buffer */
    expbuf->copy(start, end - start);

    /*
     *   After the end of the expansion sequence, insert the
     *   fully-expanded flag plus a pointer to the symbol table entry that
     *   we just expanded.  This will allow us to detect during the
     *   re-scan of the expansion text that this symbol has already been
     *   expanded, in which case we must suppress further expansion of the
     *   symbol.  This allows us to follow the ANSI C rules for recursive
     *   macro usage.  
     */
    flagbuf[0] = TOK_MACRO_EXP_END;
    memcpy(&flagbuf[1], &entry, sizeof(entry));
    expbuf->append(flagbuf, sizeof(flagbuf));

    /* indicate that we expanded the macro */
    *expanded = TRUE;

    /* success */
    err = 0;

done:
    /* return the result */
    return err;
}

/*
 *   Parse a macro's actual parameter list, filling in the given hash
 *   table with the arguments.  Returns zero on success, non-zero on
 *   error.  'entry' is the macro's defining symbol table entry.
 */
int CTcTokenizer::parse_macro_actuals(const CTcTokString *srcbuf,
                                      utf8_ptr *src,
                                      const CTcHashEntryPp *entry,
                                      size_t argofs[TOK_MAX_MACRO_ARGS],
                                      size_t arglen[TOK_MAX_MACRO_ARGS],
                                      int read_more, int *found_actuals)
{
    tc_toktyp_t typ;
    CTcToken tok;
    int argc;
    int spliced;
    int i;

    /* presume we're not going to do any line splicing */
    spliced = FALSE;

    /* no arguments parsed yet */
    argc = 0;

    /* get the next token after the macro symbol */
    typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_, TRUE);

    /* splice another line if necessary */
    if (typ == TOKT_EOF && read_more)
    {
        /* splice a line */
        typ = actual_splice_next_line(srcbuf, src, &tok);

        /* note the splice */
        spliced = TRUE;
    }

    /* if we didn't find an open paren, there's no actual list after all */
    if (typ != TOKT_LPAR)
    {
        /* tell the caller we didn't find any actuals */
        *found_actuals = FALSE;

        /* if we spliced a line, unsplice it at the current token */
        if (spliced)
            unsplice_line(tok.get_text());

        /* return success */
        return 0;
    }

    /* remember the offset of the start of the first argument */
    argofs[argc] = tok.get_text() + tok.get_text_len() - srcbuf->get_text();

    /* skip the open paren */
    typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_, TRUE);

    /* read the arguments */
    while (typ != TOKT_RPAR)
    {
        utf8_ptr p;
        int paren_depth, bracket_depth, brace_depth;
        int sp_cnt;

        /* if we have too many arguments, it's an error */
        if ((argc >= entry->get_argc() && !entry->has_varargs())
            || argc >= TOK_MAX_MACRO_ARGS)
        {
            /* log the error */
            log_error(TCERR_PP_MANY_MACRO_ARGS,
                      (int)entry->getlen(), entry->getstr());

            /* scan ahead to to close paren or end of line */
            while (typ != TOKT_RPAR && typ != TOKT_EOF)
                typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_,
                                   TRUE);
            
            /* done scanning arguments */
            break;
        }
        
        /* 
         *   Skip tokens until we find the end of the argument.  An argument
         *   ends at:
         *   
         *   - a comma outside of nested parens, square brackets, or curly
         *   braces
         *   
         *   - a close paren that doesn't match an open paren found earlier
         *   in the same argument
         */
        paren_depth = bracket_depth = brace_depth = 0;
        for (;;)
        {
            /* 
             *   If it's a comma, and we're not in any sort of nested
             *   brackets (parens, square brackets, or curly braces), the
             *   comma ends the argument.  A comma within any type of
             *   brackets is part of the argument text.
             */
            if (typ == TOKT_COMMA
                && paren_depth == 0 && brace_depth == 0 && bracket_depth == 0)
                break;

            /*
             *   If it's a close paren, and it doesn't match an earlier open
             *   paren in the same argument, it's the end of the argument. 
             */
            if (typ == TOKT_RPAR && paren_depth == 0)
                break;

            /* 
             *   if it's an open or close paren, brace, or bracket, adjust
             *   the depth accordingly 
             */
            switch(typ)
            {
            case TOKT_LPAR:
                ++paren_depth;
                break;

            case TOKT_RPAR:
                --paren_depth;
                break;

            case TOKT_LBRACE:
                ++brace_depth;
                break;

            case TOKT_RBRACE:
                --brace_depth;
                break;

            case TOKT_LBRACK:
                ++bracket_depth;
                break;

            case TOKT_RBRACK:
                --bracket_depth;
                break;

            default:
                break;
            }
            
            /* get the next token */
            typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_,
                               TRUE);
            
            /* 
             *   if we're at the end of the line, and we're allowed to
             *   read more, splice the next line onto the current line 
             */
            if (typ == TOKT_EOF && read_more)
            {
                /* splice a line */
                typ = actual_splice_next_line(srcbuf, src, &tok);

                /* note that we've done some line splicing */
                spliced = TRUE;
            }
            
            /* if we've reached the end of the file, stop */
            if (typ == TOKT_EOF)
                break;
        }

        /* if we've reached the end of the file, stop */
        if (typ == TOKT_EOF)
            break;

        /* remove any trailing whitespace from the actual's text */
        sp_cnt = 0;
        p.set((char *)tok.get_text());
        while (p.getptr() > srcbuf->get_text() + argofs[argc])
        {
            wchar_t ch;

            /* move to the prior character */
            p.dec();

            /* if it's not a space, stop looking */
            ch = p.getch();
            if (!is_space(ch))
            {
                /* 
                 *   advance past this character so that we keep it in the
                 *   expansion 
                 */
                p.inc();

                /* 
                 *   if this last character was a backslash, and we removed
                 *   at least one space following it, keep the one space
                 *   that immediately follows the backslash, since that
                 *   space is part of the backslash's two-character escape
                 *   sequence 
                 */
                if (ch == '\\' && sp_cnt != 0)
                    p.inc();

                /* stop scanning */
                break;
            }

            /* that's one more trailing space we've removed - count it */
            ++sp_cnt;
        }

        /* note the argument length */
        arglen[argc] = (p.getptr() - srcbuf->get_text()) - argofs[argc];

        /* count the argument */
        ++argc;
        
        /* check for another argument */
        if (typ == TOKT_COMMA)
        {
            /* remember the offset of the start of this argument */
            argofs[argc] = tok.get_text() + tok.get_text_len()
                           - srcbuf->get_text();

            /* skip the comma and go back for another argument */
            typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_,
                               TRUE);
        }
        else if (typ == TOKT_RPAR)
        {
            /* 
             *   No need to look any further.  Note that we don't want to
             *   get another token, since we're done parsing the input
             *   now, and we want to leave the token stream positioned for
             *   the caller just after the extent of the macro, which, in
             *   the case of this function-like macro, ends with the
             *   closing paren.  
             */
            break;
        }
    }

    /* if we didn't find the right paren, flag the error */
    if (typ != TOKT_RPAR)
    {
        log_error(read_more
                  ? TCERR_PP_MACRO_ARG_RPAR : TCERR_PP_MACRO_ARG_RPAR_1LINE,
                  (int)entry->getlen(), entry->getstr());
        return 1;
    }

    /* remove leading and trailing whitespace from each argument */
    for (i = 0 ; i < argc ; ++i)
    {
        const char *start;
        const char *end;
        utf8_ptr p;
        size_t del_len;
        int sp_cnt;

        /* figure the limits of the argument text */
        start = srcbuf->get_text() + argofs[i];
        end = start + arglen[i];

        /* remove leading whitespace */
        for (p.set((char *)start) ; p.getptr() < end && is_space(p.getch()) ;
             p.inc()) ;

        /* set the new offset and length */
        del_len = p.getptr() - start;
        argofs[i] += del_len;
        arglen[i] -= del_len;
        start += del_len;

        /* remove trailing whitespace */
        p.set((char *)end);
        sp_cnt = 0;
        while (p.getptr() > start)
        {
            wchar_t ch;

            /* go to the prior character */
            p.dec();

            /* if it's not whitespace, keep it */
            ch = p.getch();
            if (!is_space(ch))
            {
                /* put the character back */
                p.inc();

                /* 
                 *   if this is a backslash, and a space follows, keep the
                 *   immediately following space, since it's part of the
                 *   backslash sequence 
                 */
                if (ch == '\\' && sp_cnt != 0)
                    p.inc();

                /* we're done scanning */
                break;
            }

            /* count another removed trailing space */
            ++sp_cnt;
        }

        /* adjust the length */
        arglen[i] -= (end - p.getptr());
    }

    /* 
     *   if we did any line splicing, cut off the rest of the line and
     *   push it back into the logical input stream as a new line - this
     *   will allow better error message positioning if errors occur in
     *   the remainder of the line, since this means we'll only
     *   artificially join onto one line the part of the new line that
     *   contained the macro parameters 
     */
    if (spliced)
        unsplice_line(tok.get_text() + tok.get_text_len());
    
    /* make sure we found enough arguments */
    if (argc < entry->get_min_argc())
    {
        /* fill in the remaining arguments with empty strings */
        for ( ; argc < entry->get_argc() ; ++argc)
        {
            argofs[argc] = 0;
            arglen[argc] = 0;
        }
        
        /* note the error, but proceed with empty arguments */
        log_warning(TCERR_PP_FEW_MACRO_ARGS,
                    (int)entry->getlen(), entry->getstr());
    }

    /* 
     *   if we have varargs, always supply an empty marker for the last
     *   argument 
     */
    if (entry->has_varargs() && argc < TOK_MAX_MACRO_ARGS)
    {
        argofs[argc] = 0;
        arglen[argc] = 0;
    }

    /* success - we found an actual parameter list */
    *found_actuals = TRUE;
    return 0;
}
                                      
/*
 *   Splice a line for macro actual parameters.  Sets the source pointer
 *   to the start of the new line.  Reads the first token on the spliced
 *   line and returns it.
 *   
 *   We will splice new lines until we find a non-empty line or reach the
 *   end of the input.  If this returns EOF, it indicates that we've
 *   reached the end of the entire input.  
 */
tc_toktyp_t CTcTokenizer::actual_splice_next_line(
    const CTcTokString *srcbuf, utf8_ptr *src, CTcToken *tok)
{
    /* add a space onto the end of the current line */
    linebuf_.append(" ", 1);

    /* keep going until we find a non-empty line */
    for (;;)
    {
        int new_line_ofs;
        tc_toktyp_t typ;

        /* splice the next line onto the current line */
        new_line_ofs = read_line(TRUE);

        /* 
         *   make sure we read additional lines as needed to complete any
         *   strings left open at the end of the line 
         */
        if (in_quote_ != '\0')
            splice_string();

        /* if there was no more, return end of file */
        if (new_line_ofs == -1)
            return TOKT_EOF;

        /* set the source to the start of the additional line */
        src->set((char *)linebuf_.get_text() + new_line_ofs);

        /* get the next token */
        typ = next_on_line(srcbuf, src, tok, &macro_in_embedding_, TRUE);

        /* if we didn't get EOF, it means we found a non-empty line */
        if (typ != TOKT_EOF)
            return typ;
    }
}

/*
 *   Substitute the actual parameters in a macro's expansion 
 */
int CTcTokenizer::substitute_macro_actuals(
    CTcMacroRsc *rsc, CTcTokString *subexp, CTcHashEntryPp *entry,
    const CTcTokString *srcbuf, const size_t *argofs, const size_t *arglen,
    int allow_defined)
{
    const char *start;
    utf8_ptr expsrc;
    CTcToken prvtok;
    CTcToken prvprvtok;
    CTcToken tok;
    tc_toktyp_t typ;
    CTcTokString *actual_exp_buf;
    const size_t expand_max = 10;
    static struct expand_info_t
    {
        /* type of expansion (#foreach, #ifempty, #ifnempty) */
        tc_toktyp_t typ;

        /* 
         *   flag: this is an iterator type (if this is true, the varargs
         *   formal should be expanded to the current argument given by our
         *   'arg' member; if this is false, the varargs formal should be
         *   expanded as the full varargs list) 
         */
        int is_iterator;
        
        /* the marker character that delimits the foreach arguments */
        wchar_t delim;

        /* location of start of expansion region for foreach */
        utf8_ptr start;

        /* current argument index */
        int arg;

        /* the current expansion part (0 = first part, etc) */
        int part;
    }
    expand_stack[expand_max], *expand_sp;

    /* get the actual expansion buffer from the resource object */
    actual_exp_buf = &rsc->actual_exp_buf_;

    /* 
     *   Scan the replacement text for formals, and replace each formal
     *   with the actual.  Set up a pointer at the start of the expansion
     *   text.  
     */
    start = entry->get_expansion();
    expsrc.set((char *)start);

    /* we don't yet have a previous token */
    prvtok.settyp(TOKT_EOF);
    prvprvtok.settyp(TOKT_EOF);

    /* clear the expansion buffer */
    subexp->clear_text();

    /* we have no #foreach/#ifempty/#ifnempty stack yet */
    expand_sp = expand_stack;

    /* scan the tokens in the expansion text */
    for (typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE) ;
         typ != TOKT_EOF ; )
    {
        /* 
         *   check to see if we've reached the end of a
         *   #foreach/#ifempty/#ifnempty 
         */
        if (expand_sp != expand_stack)
        {
            /* check to see if we're at the delimiter */
            if (utf8_ptr::s_getch(tok.get_text()) == (expand_sp-1)->delim)
            {
                /* copy the prior expansion so far */
                if (tok.get_text() > start)
                    subexp->append(start, tok.get_text() - start);

                /* go back to the start of the token */
                expsrc.set((char *)tok.get_text());

                /* see what kind of token we're expanding */
                switch((expand_sp-1)->typ)
                {
                case TOKT_MACRO_FOREACH:
                    /* it's a #foreach - process the appropriate part */
                    switch ((expand_sp-1)->part)
                    {
                    case 0:
                        /*
                         *   We've been doing the first part, which is the
                         *   main expansion per actual.  This delimiter thus
                         *   introduces the 'between' portion, which we copy
                         *   between each iteration, but not after the last
                         *   iteration.  So, if we've just done the last
                         *   actual, skip this part entirely; otherwise,
                         *   keep going, using this part.  
                         */
                        if (argofs[(expand_sp-1)->arg + 1] == 0)
                        {
                            /* skip this one remaining part */
                            skip_delimited_group(&expsrc, 1);
                            
                            /* we're finished with the iteration */
                            goto end_foreach;
                        }
                        else
                        {
                            /* 
                             *   we have more arguments, so we want to
                             *   expand this part - skip the deliter and
                             *   keep going 
                             */
                            expsrc.inc();
                            
                            /* we're now in the next part of the iterator */
                            (expand_sp-1)->part++;
                        }
                        break;
                        
                    case 1:
                        /* 
                         *   We've reached the end of the entire #foreach
                         *   string, so we're done with this iteration.
                         *   Skip the delimiter.  
                         */
                        expsrc.inc();
                        
                    end_foreach:
                        /* 
                         *   if we have more arguments, start over with the
                         *   next iteration; otherwise, pop the #foreach
                         *   level 
                         */
                        if (argofs[(expand_sp-1)->arg + 1] == 0)
                        {
                            /* no more arguments - pop the #foreach level */
                            --expand_sp;
                        }
                        else
                        {
                            /* we have more arguments - move to the next */
                            (expand_sp-1)->arg++;
                            
                            /* go back to the start of the expansion */
                            expsrc = (expand_sp-1)->start;
                            
                            /* we have no previous token for pasting ops */
                            prvtok.settyp(TOKT_EOF);
                            prvprvtok.settyp(TOKT_EOF);
                            
                            /* we're back in the first part of the iterator */
                            (expand_sp-1)->part = 0;
                        }
                        break;
                    }
                    break;

                case TOKT_MACRO_IFEMPTY:
                case TOKT_MACRO_IFNEMPTY:
                    /* 
                     *   #ifempty or #ifnempty - we've reached the end of
                     *   the conditional text, so simply pop a level and
                     *   keep going after the delimiter 
                     */

                    /* skip the delimiter */
                    expsrc.inc();

                    /* pop a level */
                    --expand_sp;

                    /* done */
                    break;

                default:
                    break;
                }

                /* the next chunk starts here */
                start = expsrc.getptr();

                /* get the next token */
                typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);

                /* we have the next token, so back and process it */
                continue;
            }
        }

        /* if it's a #foreach marker, start a #foreach iteration */
        if (typ == TOKT_MACRO_FOREACH && entry->has_varargs())
        {
            /* copy the prior expansion so far */
            if (tok.get_text() > start)
                subexp->append(start, tok.get_text() - start);

            /* push a #foreach level, if possible */
            if (expand_sp - expand_stack >= expand_max)
            {
                /* 
                 *   we can't create another level - log an error and ignore
                 *   this new level 
                 */
                log_error(TCERR_PP_FOREACH_TOO_DEEP);
            }
            else if (argofs[entry->get_argc() - 1] == 0)
            {
                /* 
                 *   we have no actuals for the variable part of the
                 *   formals, so we must iterate zero times through the
                 *   #foreach part - in other words, simply skip ahead to
                 *   the end of the #foreach 
                 */
                skip_delimited_group(&expsrc, 2);
            }
            else
            {
                /* remember and skip the marker character */
                expand_sp->delim = expsrc.getch();
                expsrc.inc();

                /* set the expansion type */
                expand_sp->typ = typ;

                /* 
                 *   remember the position where the #foreach started, since
                 *   we need to come back here for each use of the variable 
                 */
                expand_sp->start = expsrc;

                /* we're an iterator type */
                expand_sp->is_iterator = TRUE;

                /* 
                 *   Start at the first argument in the variable part of the
                 *   argument list.  The last formal corresponds to the
                 *   first variable argument.  
                 */
                expand_sp->arg = entry->get_argc() - 1;

                /* we're in the main expansion part of the expression */
                expand_sp->part = 0;

                /* push the new level */
                ++expand_sp;
            }

            /* the next chunk starts here */
            start = expsrc.getptr();

            /* get the next token */
            typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);

            /* we have the next token, so back and process it */
            continue;
        }

        /* if it's a varargs #ifempty or #ifnempty flag, expand it */
        if ((typ == TOKT_MACRO_IFEMPTY || typ == TOKT_MACRO_IFNEMPTY)
            && entry->has_varargs())
        {
            int is_empty;
            int expand;

            /* copy the prior expansion so far */
            if (tok.get_text() > start)
                subexp->append(start, tok.get_text() - start);

            /* determine if the varargs list is empty or not */
            is_empty = (argofs[entry->get_argc() - 1] == 0);

            /* 
             *   decide whether or not expand it, according to the empty
             *   state and the flag type 
             */
            expand = ((is_empty && typ == TOKT_MACRO_IFEMPTY)
                      || (!is_empty && typ == TOKT_MACRO_IFNEMPTY));

            /* 
             *   if we're going to expand it, push a level; otherwise, just
             *   skip the entire expansion 
             */
            if (expand)
            {
                /* make sure we have room for another level */
                if (expand_sp - expand_stack >= expand_max)
                {
                    /* no room - log an error and ignore the new level */
                    log_error(TCERR_PP_FOREACH_TOO_DEEP);
                }
                else
                {
                    /* remember and skip the delimiter */
                    expand_sp->delim = expsrc.getch();
                    expsrc.inc();

                    /* 
                     *   we're not an iterator type, so inherit the
                     *   enclosing level's meaning of the varargs formal 
                     */
                    if (expand_sp - expand_stack == 0)
                    {
                        /* outermost level - use the whole varargs list */
                        expand_sp->is_iterator = FALSE;
                    }
                    else
                    {
                        /* use the enclosing level's meaning */
                        expand_sp->is_iterator = (expand_sp-1)->is_iterator;
                        expand_sp->arg = (expand_sp-1)->arg;
                    }

                    /* set the expansion type */
                    expand_sp->typ = typ;

                    /* push the new level */
                    ++expand_sp;
                }
            }
            else
            {
                /* not expanding - just skip the entire expansion */
                skip_delimited_group(&expsrc, 1);
            }

            /* the next chunk starts here */
            start = expsrc.getptr();

            /* get the next token */
            typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);

            /* we have the next token, so back and process it */
            continue;
        }

        /* if it's a varargs #argcount indicator, expand it */
        if (typ == TOKT_MACRO_ARGCOUNT && entry->has_varargs())
        {
            char buf[20];
            int i;

            /* copy the prior expansion so far */
            if (tok.get_text() > start)
                subexp->append(start, tok.get_text() - start);

            /* 
             *   count the number of arguments after and including the
             *   variable argument placeholder 
             */
            for (i = entry->get_argc() - 1 ; argofs[i] != 0 ; ++i) ;

            /* make a string out of the variable argument count */
            sprintf(buf, "%d", i - (entry->get_argc() - 1));

            /* add the argument count to the output buffer */
            subexp->append(buf, strlen(buf));

            /* the next chunk starts after the #argcount */
            start = expsrc.getptr();

            /* get the next token */
            typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);

            /* we have the next token, so back and process it */
            continue;
        }

        /* if it's a symbol, check for an actual */
        if (typ == TOKT_MACRO_FORMAL)
        {
            const char *p;
            int argnum;
            size_t argnum_len;
            int pasting;
            int pasting_at_left, pasting_at_right;
            int stringize;
            char stringize_qu;
            tc_toktyp_t stringize_type;
            CTcToken paste_at_right_tok;

            /* assume we'll copy up to the start of this token */
            p = tok.get_text();

            /* 
             *   get the index of the actual in the argument vector --
             *   this is given by the second byte of the special macro
             *   parameter flag token 
             */
            argnum = (int)(uchar)tok.get_text()[1] - 1;

            /*
             *   If we have varargs, and this is the varargs argument, and
             *   the current #foreach stack level indicates that we're
             *   iterating through the varargs list, treat this as a
             *   reference to the current argument in the iteration.  
             */
            if (expand_sp != expand_stack
                && argnum == entry->get_argc() - 1
                && (expand_sp-1)->is_iterator)
            {
                /* 
                 *   we're on a #foreach iterator, and this is the varargs
                 *   formal - use the current #foreach iteration element
                 *   instead 
                 */
                argnum = (expand_sp-1)->arg;
            }

            /* 
             *   Get the length of this argument.  If we have varargs, and
             *   this is the last formal, which is the placeholder for the
             *   variable argument list, and we're not in a #foreach
             *   iterator, the value is the value of the entire string of
             *   variable arguments, including the commas.  
             */
            if (expand_sp == expand_stack
                && entry->has_varargs()
                && argnum == entry->get_argc() - 1)
            {
                int i;

                /* 
                 *   It's the full varargs list - use the length from the
                 *   first varargs argument to the last.  Find the last
                 *   argument.  
                 */
                for (i = argnum ;
                     i < TOK_MAX_MACRO_ARGS && argofs[i] != 0 ; ++i) ;

                /* 
                 *   The full list length is the distance from the offset of
                 *   the first to the end of the last.  If there are no
                 *   varargs arguments at all, the length is zero.  
                 */
                if (i == argnum)
                    argnum_len = 0;
                else
                    argnum_len = argofs[i-1] + arglen[i-1] - argofs[argnum];
            }
            else
            {
                /* 
                 *   it's not the full varargs list, so just use the length
                 *   of this single actual
                 */
                argnum_len = arglen[argnum];
            }
            
            /* assume we won't do any token pasting or stringizing */
            pasting = pasting_at_left = pasting_at_right = FALSE;
            stringize = FALSE;

            /* 
             *   if the previous token was a token-pasting operator,
             *   remove it and any preceding whitespace from the source
             *   material, since we want to append the actual parameter
             *   text directly after the preceding token 
             */
        check_paste_left:
            if (prvtok.gettyp() == TOKT_POUNDPOUND)
            {
                wchar_t prv_ch;
                
                /* 
                 *   note that we have token pasting - we're pasting
                 *   something to the left of this token (since we had a
                 *   "##" before this token 
                 */
                pasting = TRUE;
                pasting_at_left = TRUE;
                
                /* go back to the ## token */
                p = prvtok.get_text();
                
                /* remove any preceding whitespace */
                for (prv_ch = 0 ; p > start ; )
                {
                    const char *prvp;
                    
                    /* get the previous character */
                    prvp = utf8_ptr::s_dec((char *)p);
                    prv_ch = utf8_ptr::s_getch((char *)prvp);
                    
                    /* if it's not a space, we're done */
                    if (!is_space(prv_ch))
                        break;
                    
                    /* move back over this character */
                    p = prvp;
                }

                /*
                 *   Weird special case: if the previous character was a
                 *   comma, and the formal we're pasting is a variable
                 *   argument formal (i.e., the last formal in a varargs
                 *   macro), and the varargs list is empty, then remove the
                 *   comma.  This is a handy shorthand notation that allows
                 *   the varargs list to be added to a comma-delimited list,
                 *   such as a function call's actuals or the contents of a
                 *   list.  
                 */
                if (prv_ch == ','
                    && entry->has_varargs()
                    && argnum == entry->get_argc() - 1
                    && argofs[argnum] == 0)
                {
                    /* 
                     *   it's the special case - move back one more
                     *   character to delete the comma 
                     */
                    p = utf8_ptr::s_dec((char *)p);
                }
            }
            else if (prvtok.gettyp() == TOKT_POUND
                     || prvtok.gettyp() == TOKT_POUNDAT)
            {
                /* go back to the # token */
                p = prvtok.get_text();
                
                /* note that we have stringizing */
                stringize = TRUE;
                stringize_type = prvtok.gettyp();
                stringize_qu = (prvtok.gettyp() == TOKT_POUND
                                ? '"' : '\'');

                /* go back one more token */
                prvtok = prvprvtok;
                prvprvtok.settyp(TOKT_EOF);

                /* 
                 *   go back and check for pasting again, since we could
                 *   be pasting to a stringized token 
                 */
                goto check_paste_left;
            }
            
            /* copy the prior expansion so far */
            if (p > start)
                subexp->append(start, p - start);
            
            /* remember the symbol as the previous token */
            prvprvtok = prvtok;
            prvtok = tok;
            
            /* get the next token after the formal */
            typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);
            
            /* 
             *   If it's followed by a token-pasting operator, we need to
             *   paste the next token directly onto the end of the text we
             *   just added to the buffer, skipping any intervening
             *   whitespace; otherwise, we want to start adding again at
             *   the next character after the original token.
             */
            if (typ == TOKT_POUNDPOUND)
            {
                utf8_ptr old_expsrc;
                CTcToken old_tok;

                /* note that we have pasting to the right of this token */
                pasting = TRUE;
                pasting_at_right = TRUE;

                /* remember where we started */
                old_expsrc = expsrc;
                
                /* remember the current token for a moment */
                old_tok = tok;
                
                /* skip to the next token after the ## */
                typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);

                /* remember the token we're pasting to the right */
                paste_at_right_tok = tok;

                /* check for pasting to a stringizer */
                if (stringize && typ == stringize_type)
                {
                    /* 
                     *   leave the ## in the stream for now - we'll fix it
                     *   up when we stringize the next token, rather than
                     *   doing so now 
                     */
                    expsrc = old_expsrc;
                    tok = old_tok;
                }
                else
                {
                    /* 
                     *   remember that we have a token-pasting operator,
                     *   so that we can tell that we're pasting when we
                     *   look at the next token 
                     */
                    prvprvtok = prvtok;
                    prvtok = old_tok;
                }

                /* start next text from here */
                start = tok.get_text();
            }
            else
            {
                /* Start at the end of the symbol token */
                start = prvtok.get_text() + prvtok.get_text_len();
            }
            
            /* 
             *   If we're not doing any pasting, recursively expand macros
             *   in the actual expansion text.  If we're pasting, do not
             *   expand any macros in the expansion, since we want to do
             *   the pasting before we do any expanding.  
             */
            if (pasting && stringize)
            {
                int add_open;
                int add_close;

                /* presume we'll include the open and close quotes */
                add_close = TRUE;
                add_open = TRUE;

                /*
                 *   If we're pasting to the left, and the buffer so far
                 *   ends in the same quote we're adding to this token,
                 *   combine the strings by removing the preceding quote
                 *   and not adding the open quote on the new string 
                 */
                if (subexp->get_text_len() > 0
                    && *(subexp->get_text_end() - 1) == stringize_qu)
                {
                    /* remove the close quote from the expansion so far */
                    subexp->set_text_len(subexp->get_text_len() - 1);

                    /* don't add the open quote to the new string */
                    add_open = FALSE;
                }
                
                /* 
                 *   If we're pasting to the right, and we have a string
                 *   of the same type following, or we will be pasting a
                 *   stringizing pair, paste the two strings together to
                 *   form one string by removing the close quote from this
                 *   string and the open quote from the next string 
                 */
                if (pasting_at_right && *tok.get_text() == stringize_qu)
                    add_close = FALSE;
                
                /* 
                 *   We're both stringizing this argument and pasting
                 *   another token - first stringize the actual.
                 */
                stringize_macro_actual(subexp,
                                       srcbuf->get_text()
                                       + argofs[argnum], argnum_len,
                                       stringize_qu, add_open, add_close);

                /* 
                 *   if we decided to remove the closing quote, we want to
                 *   remove the open quote from the following string as
                 *   well - copy in the following string without its open
                 *   quote 
                 */
                if (!add_close)
                {
                    /* 
                     *   append the following token without its first
                     *   character (its open quote) 
                     */
                    subexp->append(tok.get_text() + 1,
                                   tok.get_text_len() - 1);

                    /* move on to the next token */
                    prvprvtok = prvtok;
                    prvtok = tok;
                    typ = next_on_line(&expsrc, &tok, &macro_in_embedding_,
                                       TRUE);

                    /* start from the new token */
                    start = tok.get_text();
                }
            }
            else if (pasting)
            {
                const char *argp;
                size_t len;
                int done;
                wchar_t quote_char;

                /* get the actual argument information */
                argp = srcbuf->get_text() + argofs[argnum];
                len = argnum_len;

                /* 
                 *   if we're pasting to the left of this token, and the
                 *   token starts with a fully-expanded flag, remove the
                 *   flag - we're making up a new token out of this and
                 *   what comes before, so the token that we fully
                 *   expanded is disappearing, so the fully-expanded
                 *   status no longer applies 
                 */
                if (pasting_at_left && *argp == TOK_FULLY_EXPANDED_FLAG)
                {
                    /* skip the flag */
                    ++argp;
                    --len;
                }

                /* presume we won't find any quoted strings */
                quote_char = 0;

                /* 
                 *   check for string concatenation to the left - if we're
                 *   concatenating two strings of the same type, remove
                 *   the adjacent quotes to make it a single string 
                 */
                if (pasting_at_left
                    && subexp->get_text_len() > 0
                    && (*argp == '\'' || *argp == '"')
                    && *(subexp->get_text_end() - 1) == *argp)
                {
                    /* remove the close quote from the expansion so far */
                    subexp->set_text_len(subexp->get_text_len() - 1);

                    /* remember the quote character */
                    quote_char = *argp;

                    /* don't add the open quote to the new string */
                    ++argp;
                    --len;
                }

                /* presume we won't have to do anything special */
                done = FALSE;

                /*
                 *   If we're pasting at the right, also remove any
                 *   fully-expanded flag just before the last token in the
                 *   expansion.  
                 */
                if (pasting_at_right)
                {
                    CTcToken old_tok;
                    CTcToken tok;
                    utf8_ptr p;

                    /* scan for the final token in the expansion string */
                    p.set((char *)argp);
                    old_tok.settyp(TOKT_INVALID);
                    while (p.getptr() < argp + len)
                    {
                        /* 
                         *   get another token - stop at EOF or if we go
                         *   past the bounds of the expansion text 
                         */
                        if (next_on_line(&p, &tok, &macro_in_embedding_,
                                         TRUE)
                            == TOKT_EOF
                            || tok.get_text() >= argp + len)
                            break;

                        /* remember the previous token */
                        old_tok = tok;
                    }

                    /* 
                     *   if the final token is a symbol, and it has the
                     *   fully-expanded flag, we must omit the flag from
                     *   the appended text 
                     */
                    if (old_tok.gettyp() == TOKT_SYM
                        && old_tok.get_fully_expanded())
                    {
                        /* 
                         *   append up to but not including the flag byte
                         *   preceding the final token 
                         */
                        subexp->append(argp, tok.get_text() - 1 - argp);

                        /* 
                         *   append from the last token to the end of the
                         *   expansion, skipping the flag byte
                         */
                        subexp->append(tok.get_text(),
                                       len - (tok.get_text() - argp));
                        
                        /* we've done the appending */
                        done = TRUE;
                    }
                    else if (quote_char != 0
                             && paste_at_right_tok.get_text_len() != 0
                             && *paste_at_right_tok.get_text() == quote_char)
                    {
                        /* 
                         *   we're pasting two strings together - append
                         *   up to but not including the close quote 
                         */
                        subexp->append(argp, len - 1);

                        /* 
                         *   append the next token, but do not include the
                         *   open quote 
                         */
                        subexp->append(paste_at_right_tok.get_text() + 1,
                                       paste_at_right_tok.get_text_len() - 1);

                        /* 
                         *   restart after the right token, since we've
                         *   now fully processed that token 
                         */
                        start = paste_at_right_tok.get_text()
                                + paste_at_right_tok.get_text_len();

                        /* we're done */
                        done = TRUE;
                    }
                }

                /* 
                 *   append the actual without expansion, if we haven't
                 *   already handled it specially 
                 */
                if (!done)
                    subexp->append(argp, len);
            }
            else if (stringize)
            {
                /* stringize the actual */
                stringize_macro_actual(subexp,
                                       srcbuf->get_text()
                                       + argofs[argnum], argnum_len,
                                       stringize_qu, TRUE, TRUE);
            }
            else
            {
                CTcTokStringRef actual_src_buf;
                
                /* recursively expand macros in the actual text */
                actual_src_buf.
                    set_buffer(srcbuf->get_text() + argofs[argnum],
                               argnum_len);
                if (expand_macros(&actual_src_buf, 0, actual_exp_buf,
                                  FALSE, allow_defined, FALSE))
                    return 1;
                
                /* 
                 *   Append the expanded actual, marking any
                 *   fully-expanded tokens as such and removing
                 *   end-of-expansion markers.
                 *   
                 *   We can't leave end-of-expansion markers in the
                 *   expanded actual text, because end-of-expansion
                 *   markers apply only to the current recursion level,
                 *   and we've now exited the actual's recursion level.
                 *   However, we must not expand further anything in the
                 *   actual's expansion that has already been fully
                 *   expanded.  To achieve both of these goals, we switch
                 *   here from marking the run of text (with the end
                 *   marker) to marking individual tokens. 
                 */
                mark_full_exp_tokens(subexp, actual_exp_buf, TRUE);
            }
            
            /* we've already read the next token, so proceed */
            continue;
        }
    
        /* remember the current token as the previous token */
        prvprvtok = prvtok;
        prvtok = tok;
        
        /* get the next token of the expansion */
        typ = next_on_line(&expsrc, &tok, &macro_in_embedding_, TRUE);
    }

    /* copy the remaining replacement text */
    subexp->append(start, tok.get_text() - start);

    /* success */
    return 0;
}

/*
 *   Skip the source of a delimited macro expansion area (#foreach,
 *   #ifempty, #ifnempty).
 */
void CTcTokenizer::skip_delimited_group(utf8_ptr *p, int parts_to_skip)
{
    wchar_t delim;

    /* get the delimiter character */
    delim = p->getch();

    /* 
     *   if the delimiter put us at the end of the line, there's nothing to
     *   skip 
     */
    if (delim == 0 || delim == TOK_END_PP_LINE)
        return;

    /* skip the delimiter */
    p->inc();

    /* keep going until we've skipped the desired number of parts */
    while (parts_to_skip != 0)
    {
        wchar_t ch;

        /* read the next character */
        ch = p->getch();

        /* if it's the end of the line, give up */
        if (ch == 0 || ch == TOK_END_PP_LINE)
        {
            /* 
             *   we ran out of input before reaching the delimiter, so this
             *   is implicitly the end of it 
             */
            return;
        }

        /* check what we have */
        if (ch == delim)
        {
            /* that's one less part to skip */
            --parts_to_skip;

            /* skip it */
            p->inc();
        }
        else if (ch == TOK_MACRO_FOREACH_FLAG)
        {
            /* it's a nested #foreach - skip all of its parts */
            skip_delimited_group(p, 2);
        }
        else if (ch == TOK_MACRO_IFEMPTY_FLAG
                 || ch == TOK_MACRO_IFNEMPTY_FLAG)
        {
            /* nested #ifempty or #ifnempty - skip its expansion */
            skip_delimited_group(p, 1);
        }
        else
        {
            /* it's nothing special to us - skip it */
            p->inc();
        }
    }
}

/*
 *   Stringize a macro actual parameter value into a macro expansion
 *   buffer 
 */
void CTcTokenizer::stringize_macro_actual(CTcTokString *expbuf,
                                          const char *actual_val,
                                          size_t actual_len, char quote_char,
                                          int add_open_quote,
                                          int add_close_quote)
{
    utf8_ptr src;
    const char *start;
    int in_inner_quote;
    wchar_t inner_quote_char;
    wchar_t prvch;
    
    /* add the open quote if desired */
    if (add_open_quote)
        expbuf->append(&quote_char, 1);

    /* remember the start of the current segment */
    start = actual_val;
    
    /* 
     *   add the characters of the actual parameter value, quoting any
     *   quotes or backslashes 
     */
    for (src.set((char *)actual_val),
         in_inner_quote = FALSE, inner_quote_char = '\0', prvch = '\0' ;
         src.getptr() < actual_val + actual_len ; )
    {
        wchar_t cur;

        /* get this character */
        cur = src.getch();

        /* compress runs of whitespace to single spaces */
        if (is_space(cur) && prvch != '\\')
        {
            /* append up to this character */
            if (src.getptr() > start)
                expbuf->append(start, src.getptr() - start);

            /* find the next non-space character */
            for ( ; src.getptr() < actual_val + actual_len ; src.inc())
            {
                if (!is_space(src.getch()))
                    break;
            }

            /* 
             *   if we're not at the start or end of the string, add a
             *   single space to replace the entire run of whitespace --
             *   don't do this at the start or end of the string, since
             *   we must remove leading and trailing whitespace
             */
            if (prvch != '\0' && src.getptr() < actual_val + actual_len)
                expbuf->append(" ", 1);

            /* note that the previous character is a space */
            prvch = cur;

            /* this is the new starting point */
            start = src.getptr();

            /* proceed - we're already at the next character */
            continue;
        }

        /* 
         *   Check to see if we need to quote this character.  Quote any
         *   quote mark matching the enclosing quotes; also quote any
         *   backslash that occurs within nested quotes within the source
         *   material, but not backslashes that occur originally outside
         *   quotes.  
         */
        if (cur == quote_char
            || (cur == '\\' && in_inner_quote))
        {
            /* append the segment up to (but not including) this character */
            if (src.getptr() > start)
                expbuf->append(start, src.getptr() - start);
            
            /* add an extra backslash */
            expbuf->append("\\", 1);

            /* remember the start of the next segment */
            start = src.getptr();
        }

        /* 
         *   if this is a quote character, and it's not itself escaped,
         *   reverse our in-quote flag 
         */
        if (prvch != '\\')
        {
            /* 
             *   If we're in an inner quote, and it's a match for the open
             *   inner quote, we're no longer in a quote.  Otherwise, if
             *   we're not in quotes and this is some kind of quote, enter
             *   the new quotes.  
             */
            if (in_inner_quote && cur == inner_quote_char)
            {
                /* we're leaving the inner quoted string */
                in_inner_quote = FALSE;
            }
            else if (!in_inner_quote && (cur == '"' || cur == '\''))
            {
                /* we're entering a new inner quoted string */
                in_inner_quote = TRUE;
                inner_quote_char = cur;
            }
        }

        /* remember this as the previous character */
        prvch = cur;

        /* move on to the next character */
        src.inc();
    }

    /* if there's anything in the final segment, append it */
    if (src.getptr() > start)
        expbuf->append(start, src.getptr() - start);

    /* add the close quote if desired */
    if (add_close_quote)
        expbuf->append(&quote_char, 1);
}

/*
 *   Expand a "defined" preprocessor operator 
 */
int CTcTokenizer::expand_defined(CTcTokString *subexp,
                                 const CTcTokString *srcbuf, utf8_ptr *src)
{
    CTcToken tok;
    tc_toktyp_t typ;
    int paren;
    int found;

    /* get the next token */
    typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_, FALSE);

    /* note whether we have an open paren; if we do, skip it */
    paren = (typ == TOKT_LPAR);
    if (paren)
        typ = next_on_line(srcbuf, src, &tok, &macro_in_embedding_, FALSE);

    /* get the symbol */
    if (typ != TOKT_SYM)
    {
        log_error(TCERR_PP_DEFINED_NO_SYM,
                  (int)tok.get_text_len(), tok.get_text());
        return 1; 
    }

    /* look to see if the symbol is defined */
    found = (find_define(tok.get_text(), tok.get_text_len()) != 0);

    /* expand the macro to "1" if found, "0" if not */
    subexp->copy(found ? "1" : "0", 1);

    /* check for and skip the matching close paren */
    if (paren)
    {
        /* require the closing paren */
        if (next_on_line(srcbuf, src, &tok, &macro_in_embedding_, FALSE)
            != TOKT_RPAR)
        {
            /* generate an error if we don't find it */
            log_error(TCERR_PP_DEFINED_RPAR);
            return 1;
        }
    }

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Process comments.  Replaces each character of a comment with a space. 
 */
void CTcTokenizer::process_comments(size_t start_ofs)
{
    utf8_ptr src;
    utf8_ptr dst;
    
    /* we haven't found a backslash followed by trailing space yet */
    int trailing_sp_after_bs = FALSE;

    /* 
     *   Scan the line.  When inside a comment, replace each character of
     *   the comment with a space.  When outside comments, simply copy
     *   characters intact.
     *   
     *   Note that we need a separate src and dst pointer, because the
     *   character length of the original and replaced characters may
     *   change.  Fortunately, the length will never do anything but
     *   shrink or stay the same, since the only change we make is to
     *   insert spaces, which are always one byte apiece in UTF-8; we can
     *   therefore update the buffer in place.  
     */
    for (src.set(linebuf_.get_buf() + start_ofs),
         dst.set(linebuf_.get_buf() + start_ofs) ;
         src.getch() != '\0' ; src.inc())
    {
        /* get the current character */
        wchar_t cur = src.getch();

        /* check to see if we're in a comment */
        if (str_->is_in_comment())
        {
            /* 
             *   check to see if the comment is ending, or if we have an
             *   apparent nested comment (which isn't allowed)
             */
            if (cur == '*' && src.getch_at(1) == '/')
            {
                /* 
                 *   skip an extra character of the source - we'll skip
                 *   one in the main loop, so we only need to skip one
                 *   more now 
                 */
                src.inc();
                
                /* we're no longer in a comment */
                str_->set_in_comment(FALSE);
            }
            else if (cur == '/' && src.getch_at(1) == '*')
            {
                /* looks like a nested comment - warn about it */
                if (!G_prs->get_syntax_only())
                    log_warning(TCERR_NESTED_COMMENT);
            }

            /* continue without copying anything from inside the comment */
            continue;
        }
        else if (in_quote_ != '\0')
        {
            /* see what we have */
            if (cur == '\\')
            {
                /* 
                 *   It's a backslash sequence -- copy the backslash to
                 *   the output, and skip it.  Note that we don't have to
                 *   worry about the line ending with a backslash, since
                 *   the line reader will already have considered that to
                 *   be a line splice.  
                 */
                src.inc();
                dst.setch(cur);

                /* get the next character, so we copy it directly */
                cur = src.getch();

                /* 
                 *   if we're in a triple-quoted string, and this is an
                 *   escaped quote, the backslash escapes a whole run of
                 *   consecutive quotes that follow 
                 */
                if (in_triple_ && cur == in_quote_)
                {
                    /* copy and skip any run of quotes, minus the last one */
                    for (int qcnt = count_quotes(&src, cur) ; qcnt > 1 ;
                         dst.setch(cur), src.inc(), --qcnt) ;
                }
            }
            else if (cur == in_quote_)
            {
                /* This is the close quote.  Check for triple quotes. */
                if (in_triple_)
                {
                    /* triple-quoted - we need three quotes in a row */
                    int qcnt = count_quotes(&src, cur);
                    if (qcnt >= 3)
                    {
                        /* copy and skip all but the last in the run */
                        for (int i = 1 ; i < qcnt ; ++i, src.inc())
                            dst.setch(cur);

                        /* close the string */
                        in_quote_ = '\0';
                    }
                }
                else
                {
                    /* regular string, so it ends at the matching quote */
                    in_quote_ = '\0';
                }
            }
            else if (cur == '<' && src.getch_at(1) == '<')
            {
                /* 
                 *   it's an embedded expression starting point - skip the
                 *   first of the '<' characters (the enclosing loop will
                 *   skip the second one)
                 */
                src.inc();

                /* we're in an embedding now */
                comment_in_embedding_.start_expr(in_quote_, in_triple_, FALSE);

                /* the string is done for now */
                in_quote_ = '\0';

                /* copy the extra '<' to the output */
                dst.setch('<');
            }
        }
        else
        {
            /*
             *   Monitor the stream for a backslash followed by trailing
             *   spaces.  If this is a backslash, note that we might have a
             *   backslash with trailing spaces; if it's a space, we might
             *   still have this, so leave the flag alone; if it's anything
             *   else, clear the flag, since we've found something other
             *   than backslashes and spaces.  
             */
            if (cur == '\\')
                trailing_sp_after_bs = TRUE;
            else if (!is_space(cur))
                trailing_sp_after_bs = FALSE;
            
            /* check to see if we're starting a comment */
            if (cur == '/')
            {
                switch(src.getch_at(1))
                {
                case '*':
                    /* note that we're starting a comment */
                    str_->set_in_comment(TRUE);

                    /* 
                     *   replace the starting slash with a space - this
                     *   will effectively replace the entire comment with
                     *   a single space, since we won't copy anything else
                     *   from inside the comment 
                     */
                    cur = ' ';
                    break;

                case '/':
                    /* 
                     *   comment to end of line - we can terminate the
                     *   line at the opening slash and return immediately,
                     *   because the entire rest of the line is to be
                     *   ignored 
                     */
                    dst.setch('\0');
                    return;

                default:
                    /* not a comment - copy it as-is */
                    break;
                }
            }
            else if (cur == '"' || cur == '\'')
            {
                /* it's the start of a new string */
                in_quote_ = cur;

                /* check for triple quotes */
                in_triple_ = (count_quotes(&src, cur) >= 3);

                /* if in triple quotes, copy and skip the extra two qutoes */
                if (in_triple_)
                {
                    src.inc_by(2);
                    dst.setch(cur);
                    dst.setch(cur);
                }
            }
            else if (cur < 0x09)
            {
                /* 
                 *   it's a special flag character - we need to guarantee
                 *   that this character never occurs in input (it
                 *   shouldn't anyway, since it's a control character), so
                 *   translate it to a space 
                 */
                cur = ' ';
            }
            else if (comment_in_embedding_.in_expr()
                     && (cur == '(' || cur == ')'))
            {
                /* adjust the paren level in an embedded expression */
                comment_in_embedding_.parens(cur == '(' ? 1 : -1);
            }
            else if (comment_in_embedding_.in_expr()
                     && comment_in_embedding_.parens() == 0
                     && cur == '>' && src.getch_at(1) == '>')
            {
                /* it's the end of an embedded expression */
                in_quote_ = comment_in_embedding_.qu();
                in_triple_ = comment_in_embedding_.triple();
                comment_in_embedding_.end_expr();

                /* skip the extra '>' and copy it to the output */
                src.inc();
                dst.setch('>');
            }
        }

        /* set the current character in the output */
        dst.setch(cur);
    }

    /* set the updated line buffer length */
    linebuf_.set_text_len(dst.getptr() - linebuf_.get_buf());

    /* 
     *   if we found a backslash with nothing following but whitespace, flag
     *   a warning, since they might have meant the backslash as a line
     *   continuation signal, but we're not interpreting it that way because
     *   of the trailing whitespace 
     */
    if (trailing_sp_after_bs)
        log_warning(TCERR_TRAILING_SP_AFTER_BS);
}

/*
 *   Splice strings.  Splice additional lines onto the current line until
 *   we find the end of the string. 
 */
void CTcTokenizer::splice_string()
{
    /* presume we'll find proper termination */
    char unterm = '\0';

    /* 
     *   remember the current in-quote and in-embedding status, as of the
     *   end of the current line - when we splice, the line reader will
     *   update these to the status at the end of the newly-read material,
     *   but we want to scan from the beginning of the newly-read material 
     */
    wchar_t in_quote = in_quote_;
    int in_triple = in_triple_;
    tok_embed_ctx old_ec = comment_in_embedding_;

    /* note if the previous line ended with an explicit \n sequence */
    int explicit_nl = (linebuf_.get_text_len() >= 2
                       && memcmp(linebuf_.get_text_end() - 2, "\\n", 2) == 0);
        
    /* keep going until we find the end of the string */
    utf8_ptr p((char *)0);
    for (;;)
    {
        /* apply the newline spacing mode */
        switch (string_newline_spacing_)
        {
        case NEWLINE_SPACING_COLLAPSE:
            /* 
             *   Replace the newline and any subsequent run of whitespace
             *   characters with a single space, unless the last line ended
             *   with an explicit newline.
             */
            if (!explicit_nl)
                linebuf_.append(" ", 1);
            break;

        case NEWLINE_SPACING_DELETE:
            /* delete the newline and subsequent whitespace */
            break;

        case NEWLINE_SPACING_PRESERVE:
            /* 
             *   preserve the newline and subsequent whitespace exactly as
             *   written in the source code, so convert the newline to an
             *   explicit \n sequence (in source form, since we're building a
             *   source line at this point)
             */
            linebuf_.append("\\n", 2);
            break;
        }

        /* splice another line */
        int new_line_ofs = read_line(TRUE);

        /* if we reached end of file, there's no more splicing we can do */
        if (new_line_ofs == -1)
            break;

        /* get a pointer to the new text */
        char *new_line_p = (char *)linebuf_.get_text() + new_line_ofs;
        p.set(new_line_p);

        /* 
         *   If we're in COLLAPSE or DELETE mode for newline spacing, skip
         *   leading spaces in the new line.  However, override this if the
         *   previous line ended with an explicit \n sequence; this tells us
         *   that the line break is explicitly part of the string and that
         *   the subsequent whitespace is to be preserved.
         */
        if (string_newline_spacing_ != NEWLINE_SPACING_PRESERVE
            && !explicit_nl)
            for ( ; is_space(p.getch()) ; p.inc()) ;

        /* check to see if the newly spliced text ends in an explicit \n */
        explicit_nl = (linebuf_.get_text_len() - new_line_ofs >= 2
                       && memcmp(linebuf_.get_text_end() - 2, "\\n", 2) == 0);

        /* if we skipped any spaces, remove them from the text */
        if (p.getptr() > new_line_p)
        {
            /* calculate the length of the rest of the line */
            size_t rem = linebuf_.get_text_len()
                         - (p.getptr() - linebuf_.get_buf());

            /* calculate the new length of the line */
            size_t new_len = (new_line_p - linebuf_.get_buf()) + rem;

            /* move the rest of the line down over the spaces */
            memmove(new_line_p, p.getptr(), rem);

            /* set the new length */
            linebuf_.set_text_len(new_len);
        }

        /* 
         *   If the new line contains only "}" or ";", presume that the
         *   string is unterminated and terminate it here.  (This
         *   heuristic could flag well-formed strings as erroneous, but
         *   users can always work around this by moving these characters
         *   onto lines that contain at least one other non-whitespace
         *   character.)  
         */
        p.set(new_line_p);
        if (p.getch() == '}' || p.getch() == ';')
        {
            /* skip trailing whitespace */
            for (p.inc() ; is_space(p.getch()) ; p.inc()) ;
            
            /* 
             *   if there's nothing else on the line, presume it's an
             *   unterminated string 
             */
            if (p.getch() == '\0')
            {
                /* log the error */
                log_error(TCERR_POSSIBLE_UNTERM_STR,
                          appended_linenum_);

                /* remember that it's unterminated */
                unterm = (char)in_quote;

                /* 
                 *   since we're adding a presumed close quote that never
                 *   appears in the text, we need to figure the new
                 *   in-string status for the line; clear the in-quote
                 *   flag, and re-scan comments from the current point on
                 *   the line 
                 */
                in_quote_ = '\0';
                process_comments(new_line_p - linebuf_.get_buf());

                /* we're done - unsplice from the start of the new line */
                p.set(new_line_p);
                goto done;
            }
        }

        /* scan for the end of the string */
        for (p.set(new_line_p) ; ; p.inc())
        {
            /* get this character */
            wchar_t cur = p.getch();

            /* see what we have */
            if (cur == '\\')
            {
                /* it's a backslash sequence - skip the extra character */
                p.inc();

                /* 
                 *   in a triple-quoted string, a backslash escapes a whole
                 *   run of consecutive quote characters 
                 */
                if (in_triple && p.getch() == in_quote)
                {
                    /* skip all but the last consecutive quote */
                    int qcnt = count_quotes(&p, in_quote);
                    p.inc_by(qcnt - 1);
                }
                else if (p.getch() == '<')
                {
                    /* skip a run of <'s */
                    for ( ; p.getch() == '<' ; p.inc()) ;
                }
            }
            else if (cur == in_quote)
            {
                /* it's our quote character - check for triple quotes */
                if (in_triple)
                {
                    /* in a triple-quoted string - we need 3+ to end it */
                    int qcnt = count_quotes(&p, cur);
                    if (qcnt >= 3)
                    {
                        /* we have at least 3 - skip them and exit */
                        p.inc_by(qcnt);
                        goto done;
                    }
                }
                else
                {
                    /* regular string - skip it and exit the string */
                    p.inc();
                    goto done;
                }
            }
            else if (cur == '<' && p.getch_at(1) == '<')
            {
                /* 
                 *   it's an embedded expression starter - skip the '<<'
                 *   sequence and stop scanning 
                 */
                p.inc();
                p.inc();

                /* check for a '%' sprintf code */
                if (p.getch() == '%')
                    scan_sprintf_spec(&p);

                /* done */
                goto done;
            }
            else if (cur == '\0')
            {
                /* end of line - go back and splice another line */
                break;
            }
        }
    }

done:
    /* if we actually spliced anything, unsplice it at the current point */
    if (p.getptr() != 0)
        unsplice_line(p.getptr());

    /* if we found an unterminated string, supply implicit termination */
    if (unterm != '\0')
        linebuf_.append(&unterm, 1);
}


/* ------------------------------------------------------------------------ */
/*
 *   Process a #pragma directive 
 */
void CTcTokenizer::pp_pragma()
{
    struct pp_kw_def
    {
        const char *kw;
        void (CTcTokenizer::*func)();
    };
    static pp_kw_def kwlist[] =
    {
//      { "c", &CTcTokenizer::pragma_c }, -- obsolete
        { "once", &CTcTokenizer::pragma_once },
        { "all_once", &CTcTokenizer::pragma_all_once },
        { "message", &CTcTokenizer::pragma_message },
        { "newline_spacing", &CTcTokenizer::pragma_newline_spacing },
        { "sourceTextGroup", &CTcTokenizer::pragma_source_text_group },
        { 0, 0 }
    };
    pp_kw_def *kwp;
    size_t kwlen;

    /* get the pragma keyword */
    if (next_on_line() != TOKT_SYM)
    {
        log_warning(TCERR_UNKNOWN_PRAGMA,
                    (int)curtok_.get_text_len(), curtok_.get_text());
        return;
    }

    /* get the keyword length */
    kwlen = curtok_.get_text_len();

    /* scan the pragma list */
    for (kwp = kwlist ; kwp->kw != 0 ; ++kwp)
    {
        /* is this our keyword? */
        if (strlen(kwp->kw) == kwlen
            && memicmp(curtok_.get_text(), kwp->kw, kwlen) == 0)
        {
            /* this is our keyword - invoke the handler */
            (this->*(kwp->func))();

            /* we're done */
            return;
        }
    }

    /* we didn't find it - generate a warning */
    log_warning(TCERR_UNKNOWN_PRAGMA, kwlen, curtok_.get_text());
}

#if 0 // #pragma C is not currently used
/*
 *   Process a #pragma C directive 
 */
void CTcTokenizer::pragma_c()
{
    tc_toktyp_t tok;
    int new_pragma_c;
    
    /* get the next token */
    tok = next_on_line();

    /* 
     *   "+" or empty (end of line or whitespace) indicates C mode; "-"
     *   indicates standard mode 
     */
    if (tok == TOKT_PLUS || tok == TOKT_EOF)
        new_pragma_c = TRUE;
    else if (tok == TOKT_MINUS)
        new_pragma_c = FALSE;
    else
    {
        log_warning(TCERR_BAD_PRAGMA_SYNTAX);
        new_pragma_c = str_->is_pragma_c();
    }

    /* 
     *   retain the pragma in the result if we're in preprocess-only mode,
     *   otherwise remove it 
     */
    if (!pp_only_mode_)
        clear_linebuf();

    /* set the mode in the stream */
    str_->set_pragma_c(new_pragma_c);

    /* if there's a parser, notify it of the change */
    if (G_prs != 0)
        G_prs->set_pragma_c(new_pragma_c);
}
#endif

/*
 *   Process a #pragma once directive 
 */
void CTcTokenizer::pragma_once()
{
    /* add this file to the ONCE list */
    add_include_once(str_->get_desc()->get_fname());

    /* don't retain this pragma in the result */
    clear_linebuf();
}

/*
 *   Process a #pragma all_once directive 
 */
void CTcTokenizer::pragma_all_once()
{
    tc_toktyp_t tok;

    /* get the next token */
    tok = next_on_line();

    /* 
     *   "+" or empty (end of line or whitespace) indicates ALL_ONCE mode;
     *   '-' indicates standard mode 
     */
    if (tok == TOKT_PLUS || tok == TOKT_EOF)
        all_once_ = TRUE;
    else if (tok == TOKT_MINUS)
        all_once_ = FALSE;
    else
        log_warning(TCERR_BAD_PRAGMA_SYNTAX);

    /* don't retain this pragma in the result */
    clear_linebuf();
}

/*
 *   Process a #pragma message directive 
 */
void CTcTokenizer::pragma_message()
{
    size_t startofs;

    /* 
     *   copy the source line through the "message" token to the macro
     *   expansion buffer - we don't want to expand that part, but we want
     *   it to appear in the expansion, so just copy the original 
     */
    startofs = (curtok_.get_text() + curtok_.get_text_len()
                - linebuf_.get_text());
    expbuf_.copy(linebuf_.get_text(), startofs);

    /* expand macros; don't allow reading additional lines */
    if (expand_macros_curline(FALSE, FALSE, TRUE))
    {
        clear_linebuf();
        return;
    }

    /* 
     *   If we're in normal compilation mode, display the message.  If we're
     *   in preprocess-only mode, simply retain the message in the
     *   preprocessed result, so that it shows up when the result is
     *   compiled.  
     *   
     *   Ignore messages in list-includes mode.  
     */
    if (!pp_only_mode_ && !list_includes_mode_)
    {
        /* set up at the first post-processed token */
        start_new_line(&expbuf_, startofs);

        /* if there's an open paren, skip it */
        if (next_on_line_xlat(0) == TOKT_LPAR)
            next_on_line_xlat(0);
        else
            log_warning(TCERR_BAD_PRAGMA_SYNTAX);

        /* keep going until we reach the closing paren */
        while (curtok_.gettyp() != TOKT_RPAR
               && curtok_.gettyp() != TOKT_EOF)
        {
            /* display this token */
            switch(curtok_.gettyp())
            {
            case TOKT_SSTR:
            case TOKT_DSTR:
            case TOKT_SYM:
                /* display the text of the token */
                msg_str(curtok_.get_text(), curtok_.get_text_len());
                break;

            case TOKT_INT:
                /* display the integer */
                msg_long(curtok_.get_int_val());
                break;

            default:
                /* ignore anything else */
                break;
            }

            /* get the next token */
            next_on_line_xlat(0);
        }

        /* end the line */
        msg_str("\n", 1);
        
        /* remove the message from the result text */
        clear_linebuf();
    }
    else
    {
        /* preprocessing - copy expanded text to line buffer */
        linebuf_.copy(expbuf_.get_text(), expbuf_.get_text_len());
    }
}

/*
 *   Process a #pragma newline_spacing(on/off) directive
 */
void CTcTokenizer::pragma_newline_spacing()
{
    newline_spacing_mode_t f;

    /* if we're in preprocess-only mode, just pass the pragma through */
    if (pp_only_mode_)
        return;
    
    /* get the '(' token and the on/off token */
    if (next_on_line() != TOKT_LPAR || next_on_line() != TOKT_SYM)
    {
        log_warning(TCERR_BAD_PRAGMA_SYNTAX);
        goto done;
    }

    /* note the new mode flag */
    if (curtok_.get_text_len() == 2
        && memcmp(curtok_.get_text(), "on", 2) == 0)
    {
        /* 'on' - this is the old version of 'collapse' */
        f = NEWLINE_SPACING_COLLAPSE;
    }
    else if (curtok_.get_text_len() == 8
             && memcmp(curtok_.get_text(), "collapse", 8) == 0)
    {
        f = NEWLINE_SPACING_COLLAPSE;
    }
    else if (curtok_.get_text_len() == 3
             && memcmp(curtok_.get_text(), "off", 3) == 0)
    {
        /* 'off' - the old version of 'delete' */
        f = NEWLINE_SPACING_DELETE;
    }
    else if (curtok_.get_text_len() == 6
             && memcmp(curtok_.get_text(), "delete", 6) == 0)
    {
        f = NEWLINE_SPACING_DELETE;
    }
    else if (curtok_.get_text_len() == 8
             && memcmp(curtok_.get_text(), "preserve", 8) == 0)
    {
        f = NEWLINE_SPACING_PRESERVE;
    }
    else
    {
        log_warning(TCERR_BAD_PRAGMA_SYNTAX);
        goto done;
    }

    /* make sure we have the ')' token */
    if (next_on_line() != TOKT_RPAR)
    {
        log_warning(TCERR_BAD_PRAGMA_SYNTAX);
        goto done;
    }

    /* set the new mode */
    string_newline_spacing_ = f;

done:
    /* done - discard this line buffer */
    clear_linebuf();
}


/*
 *   Process a #pragma sourceTextGroup(on/off) directive 
 */
void CTcTokenizer::pragma_source_text_group()
{
    tc_toktyp_t tok;
    int f;

    /* if we're in preprocess-only mode, just pass the pragma through */
    if (pp_only_mode_)
        return;

    /* get the '(' token and the on/off token, if present */
    if ((tok = next_on_line()) == TOKT_EOF)
    {
        /* no on/off - by default it's on */
        f = TRUE;
    }
    else if (tok == TOKT_LPAR && next_on_line() == TOKT_SYM)
    {
        /* get the on/off mode */
        if (curtok_.get_text_len() == 2
            && memcmp(curtok_.get_text(), "on", 2) == 0)
        {
            /* it's 'on' */
            f = TRUE;
        }
        else if (curtok_.get_text_len() == 3
                 && memcmp(curtok_.get_text(), "off", 3) == 0)
        {
            /* it's 'off' */
            f = FALSE;
        }
        else
        {
            log_warning(TCERR_BAD_PRAGMA_SYNTAX);
            goto done;
        }

        /* make sure we have the ')' token */
        if (next_on_line() != TOKT_RPAR)
        {
            log_warning(TCERR_BAD_PRAGMA_SYNTAX);
            goto done;
        }
    }
    else
    {
        /* anything else is invalid syntax */
        log_warning(TCERR_BAD_PRAGMA_SYNTAX);
        goto done;
    }

    /* set the new mode in the parser */
    G_prs->set_source_text_group_mode(f);

done:
    /* done - discard this line buffer */
    clear_linebuf();
}


/* ------------------------------------------------------------------------ */
/*
 *   Process a #charset directive 
 */
void CTcTokenizer::pp_charset()
{
    /* 
     *   Encountering a #charset directive within the tokenizer is always
     *   an error.  If the file opener managed to use a #charset, we'll
     *   never see it, because the file opener will have skipped it before
     *   giving us the file.
     *   
     *   If we flagged a #charset error when opening the file, indicate
     *   that the problem is that the character set given was unloadable;
     *   otherwise, the problem is that #charset is in the wrong place.  
     */
    log_error(str_ != 0 && str_->get_charset_error()
              ? TCERR_CANT_LOAD_CHARSET : TCERR_UNEXPECTED_CHARSET);

    /* don't retain this pragma in the result */
    clear_linebuf();
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #include directive 
 */
void CTcTokenizer::pp_include()
{
    wchar_t match;
    int is_local;
    int is_absolute;
    utf8_ptr fname;
    CTcSrcFile *new_src;
    int charset_error;
    int default_charset_error;
    char full_name[OSFNMAX];
    char lcl_name[OSFNMAX];
    int found;
    CTcTokFileDesc *desc;
    int expand;
    utf8_ptr start;

    /* presume we'll expand macros */
    expand = TRUE;

    /*
     *   Check to see if expansion is needed.  Macro expansion is needed
     *   only if the source line is not of one of the following forms:
     *   
     *.  #include "filename"
     *.  #include <filename> 
     */
    for (start = p_ ; is_space(p_.getch()) ; p_.inc()) ;
    switch(p_.getch())
    {
    case '<':
        /* look for a matching '>' */
        match = '>';
        goto find_match;

    case '"':
        /* look for a matching '"' */
        match = '"';
        goto find_match;

    find_match:
        /* find the matching character */
        for (p_.inc() ; p_.getch() != '\0' && p_.getch() != match ;
             p_.inc()) ;

        /* if we found it, check for other characters on the line */
        if (p_.getch() == match)
        {
            /* skip the matching character */
            p_.inc();

            /* skip whitespace */
            while (is_space(p_.getch()))
                p_.inc();

            /* 
             *   make sure there's nothing else on the line - if not, it's
             *   one of the approved formats, so there's no need to do
             *   macro expansion 
             */
            if (p_.getch() == 0)
                expand = FALSE;
        }
        break;
    }

    /* go back to read from the original starting point */
    p_ = start;
    
    /* expand macros if necessary */
    if (expand)
    {
        /* do the expansion */
        if (expand_macros_curline(FALSE, FALSE, FALSE))
        {
            /* clear the buffer and abort */
            clear_linebuf();
            return;
        }

        /* 
         *   remove any expansion flags, so that we don't have to worry about
         *   parsing or skipping them
         */
        remove_expansion_flags(&expbuf_);

        /* read from the expansion buffer */
        start_new_line(&expbuf_, 0);
    }

    /* skip leading whitespace */
    for ( ; is_space(p_.getch()) ; p_.inc()) ;

    /* we have to be looking at at '"' or '<' character */
    if (p_.getch() == '"')
    {
        /* look for a matching quote, and look for a local file */
        match = '"';
        is_local = TRUE;
    }
    else if (p_.getch() == '<')
    {
        /* look for a matching angle bracket, and look for a system file */
        match = '>';
        is_local = FALSE;
    }
    else
    {
        /* invalid syntax - log an error and ignore the line */
        log_error(TCERR_BAD_INC_SYNTAX);
        clear_linebuf();
        return;
    }

    /* skip the open quote, and remember where the filename starts */
    p_.inc();
    fname = p_;

    /* find the matching quote */
    for ( ; p_.getch() != '\0' && p_.getch() != match ; p_.inc()) ;

    /* if we didn't find the match, log an error and ignore the line */
    if (p_.getch() == '\0')
    {
        log_error(TCERR_BAD_INC_SYNTAX);
        clear_linebuf();
        return;
    }
    else
    {
        /*   
         *   We found the close quote.  Before we parse the filename, make
         *   one last check: if there's anything further on the line apart
         *   from whitespace, it's extraneous, so issue a warning.  
         */

        /* remember where the close quote is */
        utf8_ptr closep = p_;
        
        /* skip it, and then skip any trailing whitespace */
        for (p_.inc() ; is_space(p_.getch()) ; p_.inc()) ;

        /* if we're not at the end of the line, issue a warning */
        if (p_.getch() != '\0')
            log_warning(TCERR_EXTRA_INC_SYNTAX);

        /* 
         *   Null-terminate the filename.  (We know there's nothing else
         *   interesting in the buffer after the filename at this point, so
         *   we don't care about overwriting the quote or anything that might
         *   come after it.)  
         */
        closep.setch('\0');
    }

    /* check to see if the filename is absolute */
    is_absolute = os_is_file_absolute(fname.getptr());

    /* we have yet to find the file */
    found = FALSE;

    /* 
     *   in case the name is in portable URL notation, convert from URL
     *   notation to local notation; we'll consider this form of the name
     *   first, and only if we can't find it in this form will we try
     *   treating the name as using local filename conventions 
     */
    os_cvt_url_dir(lcl_name, sizeof(lcl_name), fname.getptr());

    /*
     *   Search for the included file.
     *   
     *   First, if it's a local file (in quotes rather than angle
     *   brackets), start the search in the directory containing the
     *   current file, then look in the directory containing the parent
     *   file, and so on.  If we fail to find it, proceed as for a
     *   non-local file.
     */
    if (is_local && last_desc_ != 0)
    {
        CTcTokStream *cur_str;
        char pathbuf[OSFNMAX];
        
        /* start with the current file, and search parents */
        for (cur_str = str_ ; cur_str != 0 ; cur_str = cur_str->get_parent())
        {
            /* get the path to the current file */
            os_get_path_name(pathbuf, sizeof(pathbuf),
                             last_desc_->get_fname());

            /* 
             *   try the URL-converted name first - this takes precedence
             *   over a local interpretation of the name 
             */
            os_build_full_path(full_name, sizeof(full_name),
                               pathbuf, lcl_name);
            if (!osfacc(full_name))
            {
                found = TRUE;
                break;
            }

            /* if it's a relative local name, try again with local naming */
            if (!is_absolute)
            {
                /* 
                 *   build the full filename, treating the name as using
                 *   local system conventions 
                 */
                os_build_full_path(full_name, sizeof(full_name),
                                   pathbuf, fname.getptr());
                
                /* if we found it, so note and stop searching */
                if (!osfacc(full_name))
                {
                    found = TRUE;
                    break;
                }
            }
        }
    }

    /*
     *   If we still haven't found the file (or if it's a non-local file,
     *   in angle brackets), search the include path.
     */
    if (!found)
    {
        tctok_incpath_t *inc_path;
        
        /* scan the include path */
        for (inc_path = incpath_head_ ; inc_path != 0 ;
             inc_path = inc_path->nxt)
        {
            /* try the URL-converted local name first */
            os_build_full_path(full_name, sizeof(full_name),
                               inc_path->path, lcl_name);
            if (!osfacc(full_name))
            {
                found = TRUE;
                break;
            }

            /* try with the local name, if it's a relative local name */
            if (!is_absolute)
            {
                /* build the full name for the file in this directory */
                os_build_full_path(full_name, sizeof(full_name),
                                   inc_path->path, fname.getptr());
                
                /* if we found it, stop searching */
                if (!osfacc(full_name))
                {
                    found = TRUE;
                    break;
                }
            }
        }
    }

    /* 
     *   If the filename specified an absolute path, and we didn't find a
     *   file with any of the local interpretations, look at the absolute
     *   path.  Note that our portable URL-style notation doesn't allow
     *   absolute notation, so we use only the exact name as specified in
     *   the #include directive as the absolute form.  
     */
    if (is_absolute && !found)
    {
        /* use the original filename as the full name */
        strcpy(full_name, fname.getptr());

        /* try finding the file */
        found = !osfacc(full_name);
    }

    /* 
     *   we have our copy of the filename now; we don't want to retain
     *   this directive in the preprocessed source, so clear out the line
     *   buffer now 
     */
    clear_linebuf();

    /* 
     *   if we didn't find the file anywhere, show an error and ignore the
     *   #include directive 
     */
    if (!found)
    {
        log_error(TCERR_INC_NOT_FOUND,
                  (int)strlen(fname.getptr()), fname.getptr());
        return;
    }
    
    /*
     *   Check the list of included files that are marked for inclusion
     *   only once.  If we've already included this file, ignore this
     *   redundant inclusion.  Check based on the full filename that we
     *   resolved from the search path.  
     */
    if (find_include_once(full_name))
    {
        /* log an error if appropriate */
        if (warn_on_ignore_incl_)
            log_warning(TCERR_REDUNDANT_INCLUDE,
                        (int)strlen(full_name), full_name);
        
        /* ignore this #include directive */
        return;
    }

    /* open a file source to read the file */
    new_src = CTcSrcFile::open_source(full_name, res_loader_,
                                      default_charset_, &charset_error,
                                      &default_charset_error);

    /* if we couldn't open the file, log an error and ignore the line */
    if (new_src == 0)
    {
        /* 
         *   if the error was due to the default character set, log that
         *   problem; otherwise, log the general file-open problem 
         */
        if (default_charset_error)
            log_error(TCERR_CANT_LOAD_DEFAULT_CHARSET, default_charset_);
        else
            log_error(TCERR_INC_NOT_FOUND,
                      (int)strlen(full_name), full_name);

        /* we can go no further */
        return;
    }

    /* get the descriptor for the source file */
    desc = get_file_desc(full_name, strlen(full_name), FALSE,
                         fname.getptr(),
                         fname.getptr() != 0 ? strlen(fname.getptr()) : 0);

    /* 
     *   remember the current #pragma newline_spacing mode, so we can restore
     *   it when we reinstate the current stream 
     */
    str_->set_newline_spacing(string_newline_spacing_);

    /* 
     *   Create and install the new file reader stream object.  By
     *   installing it as the current reader, we'll activate it so that
     *   the next line read will come from the new stream.  Note that the
     *   current stream becomes the parent of the new stream, so that we
     *   revert to the current stream when the new stream is exhausted;
     *   this will allow us to pick up reading from the current stream at
     *   the next line after the #include directive when we've finished
     *   including the new file.  
     */
    str_ = new CTcTokStream(desc, new_src, str_, charset_error, if_sp_);

    /*
     *   If we're in ALL_ONCE mode, it means that every single file we
     *   include should be included only once. 
     */
    if (all_once_)
        add_include_once(full_name);

    /* 
     *   if we're in list-includes mode, write the name of the include file
     *   to the standard output 
     */
    if (list_includes_mode_)
        G_hostifc->print_msg("#include %s\n", full_name);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a file to the include-once list.  Once a file is in this list, we
 *   won't include it again. 
 */
void CTcTokenizer::add_include_once(const char *fname)
{
    tctok_incfile_t *prvinc;

    /* if the file is already in the list, don't add it again */
    if (find_include_once(fname))
        return;
    
    /* create a new entry for the filename */
    prvinc = (tctok_incfile_t *)t3malloc(sizeof(tctok_incfile_t)
                                         + strlen(fname));

    /* save the filename */
    strcpy(prvinc->fname, fname);
    
    /* link the new entry into our list */
    prvinc->nxt = prev_includes_;
    prev_includes_ = prvinc;
}

/*
 *   Find a file in the list of files to be included only once.  Returns
 *   true if the file is in the list, false if not. 
 */
int CTcTokenizer::find_include_once(const char *fname)
{
    tctok_incfile_t *prvinc;

    /* search the list */
    for (prvinc = prev_includes_ ; prvinc != 0 ; prvinc = prvinc->nxt)
    {
        /* if this one matches, we found it, so return true */
        if (os_file_names_equal(fname, prvinc->fname))
            return TRUE;
    }

    /* we didn't find the file */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #define directive 
 */
void CTcTokenizer::pp_define()
{
    const char *macro_name;
    size_t macro_len;
    const char *argv[TOK_MAX_MACRO_ARGS];
    size_t argvlen[TOK_MAX_MACRO_ARGS];
    int argc;
    int has_args;
    const char *expan;
    size_t expan_len;
    CTcHashEntryPp *entry, *old_entry;
    int has_varargs;

    /* get the macro name */
    if (next_on_line() != TOKT_SYM)
    {
        log_error(TCERR_BAD_DEFINE_SYM,
                  (int)curtok_.get_text_len(), curtok_.get_text());
        clear_linebuf();
        return;
    }

    /* make a copy of the macro name */
    macro_name = curtok_.get_text();
    macro_len = curtok_.get_text_len();

    /* no arguments yet */
    argc = 0;

    /* presume we won't find a varargs marker */
    has_varargs = FALSE;

    /* 
     *   If there's a '(' immediately after the macro name, without any
     *   intervening whitespace, it has arguments; otherwise, it has no
     *   arguments.  Note which case we have.  
     */
    if (p_.getch() == '(')
    {
        int done;
        tc_toktyp_t tok;
        
        /* note that we have an argument list */
        has_args = TRUE;

        /* assume we're not done yet */
        done = FALSE;

        /* skip the paren and get the next token */
        p_.inc();
        tok = next_on_line();

        /* check for an empty argument list */
        if (tok == TOKT_RPAR)
        {
            /* note that we're done with the arguments */
            done = TRUE;
        }

        /* scan the argument list */
        while (!done)
        {
            /* if we have too many arguments, it's an error */
            if (argc >= TOK_MAX_MACRO_ARGS)
            {
                log_error(TCERR_TOO_MANY_MAC_PARMS,
                          macro_name, macro_len, TOK_MAX_MACRO_ARGS);
                clear_linebuf();
                return;
            }

            /* if we're at the end of the macro, it's an error */
            if (tok == TOKT_EOF)
            {
                /* log the error and ignore the line */
                log_error(TCERR_MACRO_NO_RPAR);
                clear_linebuf();
                return;
            }

            /* check for a valid initial symbol character */
            if (tok != TOKT_SYM)
            {
                log_error_curtok(TCERR_BAD_MACRO_ARG_NAME);
                clear_linebuf();
                return;
            }

            /* remember the argument name */
            argvlen[argc] = curtok_.get_text_len();
            argv[argc++] = curtok_.get_text();

            /* get the next token */
            tok = next_on_line();

            /* make sure we have a comma or paren following */
            if (tok == TOKT_COMMA)
            {
                /* we have more arguments - skip the comma */
                tok = next_on_line();
            }
            else if (tok == TOKT_ELLIPSIS)
            {
                /* skip the ellipsis */
                tok = next_on_line();

                /* note the varargs marker */
                has_varargs = TRUE;

                /* this must be the last argument */
                if (tok != TOKT_RPAR)
                {
                    /* log the error */
                    log_error_curtok(TCERR_MACRO_ELLIPSIS_REQ_RPAR);

                    /* discard the line and give up */
                    clear_linebuf();
                    return;
                }

                /* that's the last argument - we can stop now */
                done = TRUE;
            }
            else if (tok == TOKT_RPAR)
            {
                /* no more arguments - note that we can stop now */
                done = TRUE;
            }
            else
            {
                /* invalid argument - log an error and discard the line */
                log_error_curtok(TCERR_MACRO_EXP_COMMA);
                clear_linebuf();
                return;
            }
        }
    }
    else
    {
        /* 
         *   there are no arguments - the macro's expansion starts
         *   immediately after the end of the name and any subsequent
         *   whitespace 
         */
        has_args = FALSE;
    }

    /* skip whitespace leading up to the expansion */
    while (is_space(p_.getch()))
        p_.inc();

    /* the rest of the line is the expansion */
    expan = p_.getptr();

    /* don't allow defining "defined" */
    if (macro_len == 7 && memcmp(macro_name, "defined", 7) == 0)
    {
        /* log an error */
        log_error(TCERR_REDEF_OP_DEFINED);

        /* don't retain the directive in the preprocessed result */
        clear_linebuf();

        /* ignore the definition */
        return;
    }

    /* get the length of the expansion text */
    expan_len = strlen(expan);

    /* 
     *   remove any trailing whitespace from the expansion text; however,
     *   leave a trailing space if it's preceded by a backslash 
     */
    while (expan_len > 0
           && is_space(expan[expan_len-1])
           && !(expan_len > 1 && expan[expan_len-2] == '\\'))
        --expan_len;

    /* create an entry for the new macro */
    entry = new CTcHashEntryPpDefine(macro_name, macro_len, TRUE,
                                     has_args, argc, has_varargs,
                                     argv, argvlen, expan, expan_len);

    /* 
     *   Check the symbol table to see if this symbol is already defined with
     *   a different expansion.  If so, show a warning, but honor the new
     *   definition.  
     */
    old_entry = find_define(macro_name, macro_len);
    if (old_entry != 0)
    {
        /*
         *   Check for a trivial redefinition - if the number of arguments is
         *   the same, and the type (object-like or function-like) is the
         *   same, and the expansion string is identical, there's no need to
         *   warn, because the redefinition has no effect and can thus be
         *   safely ignored.  Note that we must ignore any differences in the
         *   whitespace in the expansions for this comparision.
         *   
         *   Compare the *parsed* versions of the expansions.  This ensures
         *   that we take into account changes to parameter names.  For
         *   example, the following two macros are equivalent, even though
         *   their unparsed source texts are different, because they'd still
         *   have identical expansions for any given parameter value:
         *   
         *.    #define A(x) x
         *.    #define A(y) y
         *   
         *   Likewise, the following two macros are NOT equivalent, even
         *   though their unparsed source texts are identical, because they
         *   could have different expansions for a given parameter value:
         *   
         *.    #define A(x) x
         *.    #define A(y) x
         */
        if ((old_entry->has_args() != 0) == (has_args != 0)
            && old_entry->get_argc() == argc
            && lib_strequal_collapse_spaces(entry->get_expansion(),
                                            entry->get_expan_len(),
                                            old_entry->get_expansion(),
                                            old_entry->get_expan_len()))
        {
            /* it's a non-trivial redefinition - ignore it */
            delete entry;
            goto done;
        }

        /* log a warning about the redefinition */
        log_warning(TCERR_MACRO_REDEF, (int)macro_len, macro_name);

        /* remove and delete the old entry */
        defines_->remove(old_entry);

        /* if the item isn't already in the #undef table, add it */
        if (find_undef(macro_name, macro_len) == 0)
        {
            /* 
             *   move the entry to the #undef table so that we can keep track
             *   of the fact that this macro's definition has changed in the
             *   course of the compilation 
             */
            undefs_->add(old_entry);
        }
        else
        {
            /* 
             *   the name is already in the #undef table, so we don't need
             *   another copy - just forget about the old entry entirely 
             */
            delete old_entry;
        }
    }

    /* add it to the hash table */
    defines_->add(entry);

done:
    /* don't retain the directive in the preprocessed source */
    clear_linebuf();
}


/* ------------------------------------------------------------------------ */
/*
 *   Process a #ifdef directive 
 */
void CTcTokenizer::pp_ifdef()
{
    /* process the ifdef/ifndef with a positive sense */
    pp_ifdef_or_ifndef(TRUE);
}

/*
 *   Process a #ifndef directive 
 */
void CTcTokenizer::pp_ifndef()
{
    /* process the ifdef/ifndef with a negative sense */
    pp_ifdef_or_ifndef(FALSE);
}

/*
 *   Process a #ifdef or #ifndef.  If 'sense' is true, we'll take the
 *   branch if the symbol is defined (hence #ifdef), otherwise we'll take
 *   it if the symbol isn't defined (hence #ifndef). 
 */
void CTcTokenizer::pp_ifdef_or_ifndef(int sense)
{
    char macro_name[TOK_SYM_MAX_BUFFER];
    int found;
    tok_if_t state;

    /* make sure we have a valid symbol */
    if (pp_get_lone_ident(macro_name, sizeof(macro_name)))
    {
        /* clear the line buffer */
        clear_linebuf();

        /* 
         *   push a true if to avoid cascading errors for matching #endif
         *   or #else 
         */
        push_if(TOKIF_IF_YES);

        /* we're done */
        return;
    }

    /* check to see if it's defined */
    found = (find_define(macro_name, strlen(macro_name)) != 0);

    /* 
     *   if we found it and they wanted it found, or we didn't find it and
     *   they didn't want it found, take a true branch; otherwise, take a
     *   false branch 
     */
    if ((sense != 0) == (found != 0))
        state = TOKIF_IF_YES;
    else
        state = TOKIF_IF_NO;

    /* push the new #if state */
    push_if(state);

    /* don't retain the directive in the preprocessed source */
    clear_linebuf();
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #if directive 
 */
void CTcTokenizer::pp_if()
{
    CTcConstVal val;
    
    /* expand macros; don't allow reading additional lines */
    if (expand_macros_curline(FALSE, TRUE, FALSE))
        goto do_error;

    /* 
     *   we don't need the original source line any more, and we don't
     *   want to copy it to the preprocessed output, so clear it 
     */
    clear_linebuf();

    /* parse out of the expansion buffer */
    start_new_line(&expbuf_, 0);

    /* parse the preprocessor expression */
    if (pp_parse_expr(&val, TRUE, TRUE, TRUE))
    {
        /* 
         *   we can't get a value; treat the expression as true and
         *   continue parsing, so that we don't throw off the #if nesting
         *   level 
         */
        val.set_bool(TRUE);
    }

    /* push the new state according to the value of the expression */
    push_if(val.get_val_bool() ? TOKIF_IF_YES : TOKIF_IF_NO);

    /* done */
    return;

do_error:
    /* clear the line buffer */
    clear_linebuf();

    /* 
     *   push a true if - even though we can't evaluate the condition, we
     *   can at least avoid a cascade of errors for the matching #endif
     *   and #else 
     */
    push_if(TOKIF_IF_YES);
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #elif directive 
 */
void CTcTokenizer::pp_elif()
{
    CTcConstVal val;

    /* expand macros; don't allow reading additional lines */
    if (expand_macros_curline(FALSE, TRUE, FALSE))
    {
        clear_linebuf();
        return;
    }

    /* parse out of the expansion buffer */
    start_new_line(&expbuf_, 0);

    /* parse the preprocessor expression */
    if (pp_parse_expr(&val, TRUE, TRUE, TRUE))
    {
        clear_linebuf();
        return;
    }

    /* 
     *   make sure that the #elif occurs in the same file as the
     *   corresponding #if 
     */
    if (if_sp_ <= str_->get_init_if_level())
    {
        /* log the error */
        log_error(TCERR_PP_ELIF_NOT_IN_SAME_FILE);
        
        /* clear the text and abort */
        clear_linebuf();
        return;
    }

    /* check the current #if state */
    switch(get_if_state())
    {
    case TOKIF_IF_YES:
        /* 
         *   we just took the #if branch, so don't take this or any
         *   subsequent #elif or #else branch, regardless of the value of
         *   the condition - set the state to DONE to indicate that we're
         *   skipping everything through the endif 
         */
        change_if_state(TOKIF_IF_DONE);
        break;
        
    case TOKIF_IF_NO:
        /*
         *   We haven't yet taken a #if or #elif branch, so we can take
         *   this branch if its condition is true.  If this branch's
         *   condition is false, stay with NO so that we will consider
         *   future #elif and #else branches. 
         */
        if (val.get_val_bool())
            change_if_state(TOKIF_IF_YES);
        break;
        
    case TOKIF_IF_DONE:
        /* 
         *   we've already taken a #if or #elif branch, so we must ignore
         *   this and subsequent #elif and #else branches until we get to
         *   our #endif - just stay in state DONE 
         */
        break;

    case TOKIF_NONE:
    case TOKIF_ELSE_YES:
    case TOKIF_ELSE_NO:
        /* 
         *   we're not in a #if branch at all, or we're inside a #else; a
         *   #elif is not legal here 
         */
        log_error(TCERR_PP_ELIF_WITHOUT_IF);
        break;
    }

    /* don't retain the directive in the preprocessed source */
    clear_linebuf();
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #else directive 
 */
void CTcTokenizer::pp_else()
{
    /* make sure there's nothing but whitespace on the line */
    if (next_on_line() != TOKT_EOF)
        log_error(TCERR_PP_EXTRA);

    /* 
     *   make sure that the #else occurs in the same file as the
     *   corresponding #if 
     */
    if (if_sp_ <= str_->get_init_if_level())
    {
        /* log the error */
        log_error(TCERR_PP_ELSE_NOT_IN_SAME_FILE);

        /* clear the text and abort */
        clear_linebuf();
        return;
    }

    /* check our current #if state */
    switch(get_if_state())
    {
    case TOKIF_IF_YES:
    case TOKIF_IF_DONE:
        /* 
         *   we've already taken a true #if branch, so we don't want to
         *   process the #else part - switch to a false #else branch 
         */
        change_if_state(TOKIF_ELSE_NO);
        break;

    case TOKIF_IF_NO:
        /* 
         *   we haven't yet found a true #if branch, so take the #else
         *   branch -- switch to a true #else branch 
         */
        change_if_state(TOKIF_ELSE_YES);
        break;

    case TOKIF_NONE:
    case TOKIF_ELSE_YES:
    case TOKIF_ELSE_NO:
        /* 
         *   we're not in a #if at all, or we're in a #else - log an error
         *   and ignore it 
         */
        log_error(TCERR_PP_ELSE_WITHOUT_IF);
        break;
    }

    /* don't retain the directive in the preprocessed source */
    clear_linebuf();
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #endif directive 
 */
void CTcTokenizer::pp_endif()
{
    /* make sure the rest of the line is blank */
    if (next_on_line() != TOKT_EOF)
        log_error(TCERR_PP_EXTRA);

    /* ignore the rest of the line */
    clear_linebuf();

    /* if we're not in a #if in the same file it's an error */
    if (if_sp_ == 0)
    {
        log_error(TCERR_PP_ENDIF_WITHOUT_IF);
        return;
    }
    else if (if_sp_ <= str_->get_init_if_level())
    {
        log_error(TCERR_PP_ENDIF_NOT_IN_SAME_FILE);
        return;
    }

    /* pop a #if level */
    pop_if();

    /* don't retain the directive in the preprocessed source */
    clear_linebuf();
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #error directive 
 */
void CTcTokenizer::pp_error()
{
    size_t startofs;

    /* 
     *   copy the source line through the "error" token to the macro
     *   expansion buffer - we don't want to expand that part, but we want
     *   it to appear in the expansion, so just copy the original 
     */
    startofs = (curtok_.get_text() + curtok_.get_text_len()
                - linebuf_.get_text());
    expbuf_.copy(linebuf_.get_text(), startofs);

    /* expand macros; don't allow reading additional lines */
    if (expand_macros_curline(FALSE, FALSE, TRUE))
    {
        clear_linebuf();
        return;
    }

    /* clean up any expansion flags embedded in the buffer */
    remove_expansion_flags(&expbuf_);

    /*
     *   If we're in preprocess-only mode, simply retain the text in the
     *   processed result, so that the error is processed on a subsequent
     *   compilation of the result; otherwise, display the error.  
     *   
     *   Ignore #error directives in list-includes mode as well.  
     */
    if (!pp_only_mode_ && !list_includes_mode_)
    {
        /* display the error */
        log_error(TCERR_ERROR_DIRECTIVE,
                  (int)expbuf_.get_text_len() - startofs,
                  expbuf_.get_text() + startofs);
        
        /* clear the directive from the result */
        clear_linebuf();
    }
    else
    {
        /* preprocessing - copy expanded text to line buffer */
        linebuf_.copy(expbuf_.get_text(), expbuf_.get_text_len());
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #undef directive 
 */
void CTcTokenizer::pp_undef()
{
    char macro_name[TOK_SYM_MAX_BUFFER];

    /* get the macro name */
    if (pp_get_lone_ident(macro_name, sizeof(macro_name)))
    {
        clear_linebuf();
        return;
    }

    /* remove it */
    undefine(macro_name);

    /* don't retain the directive in the preprocessed source */
    clear_linebuf();
}

/*
 *   Programmatically delete a preprocesor symbol 
 */
void CTcTokenizer::undefine(const char *sym, size_t len)
{
    CTcHashEntryPp *entry;

    /* 
     *   find the macro - if it wasn't defined, silently ignore it, since
     *   it's legal to #undef a symbol that wasn't previously defined 
     */
    entry = find_define(sym, len);
    if (entry != 0 && entry->is_undefable())
    {
        /* remove it */
        defines_->remove(entry);

        /* if it's not already in the #undef table, move it there */
        if (find_undef(sym, len) == 0)
        {
            /* move it to the #undef table */
            undefs_->add(entry);
        }
        else
        {
            /* 
             *   the name is already in the #undef table, so we don't need to
             *   add it again - we can forget about this entry entirely 
             */
            delete entry;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a #line directive 
 */
void CTcTokenizer::pp_line()
{
    CTcConstVal val_line;
    CTcConstVal val_fname;
    CTcTokFileDesc *desc;

    /* expand macros; don't allow reading additional lines */
    if (expand_macros_curline(FALSE, TRUE, FALSE))
    {
        clear_linebuf();
        return;
    }

    /* 
     *   we don't need the original source line any more, and we don't
     *   want to copy it to the preprocessed output, so clear it
     */
    clear_linebuf();

    /* set up to parse from the expansion */
    start_new_line(&expbuf_, 0);

    /* evaluate the line number expression */
    if (pp_parse_expr(&val_line, TRUE, FALSE, TRUE))
        return;

    /* if it's not an integer constant, it's an error */
    if (val_line.get_type() != TC_CVT_INT)
    {
        log_error(TCERR_LINE_REQ_INT);
        return;
    }

    /* evaluate the filename expression */
    if (pp_parse_expr(&val_fname, FALSE, TRUE, TRUE))
        return;

    /* the filename must be a string expression */
    if (val_fname.get_type() != TC_CVT_SSTR)
    {
        log_error(TCERR_LINE_FILE_REQ_STR);
        return;
    }

    /* find or create a descriptor for the filename */
    desc = get_file_desc(val_fname.get_val_str(),
                         val_fname.get_val_str_len(), FALSE, 0, 0);

    /* set the new line number and descriptor in the current stream */
    if (str_ != 0)
    {
        str_->set_next_linenum(val_line.get_val_int());
        str_->set_desc(desc);
    }

    /* 
     *   retain the pragma in the result if we're in preprocess-only mode,
     *   otherwise remove it 
     */
    if (!pp_only_mode_)
        clear_linebuf();
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up a symbol in the #define symbol table 
 */
CTcHashEntryPp *CTcTokenizer::find_define(const char *sym, size_t len) const
{
    /* look it up in the #define symbol table and return the result */
    return (CTcHashEntryPp *)defines_->find(sym, len);
}

/*
 *   Look up a symbol in the #undef table 
 */
CTcHashEntryPp *CTcTokenizer::find_undef(const char *sym, size_t len) const
{
    /* look it up in the #define symbol table and return the result */
    return (CTcHashEntryPp *)undefs_->find(sym, len);
}

/*
 *   Add a preprocessor macro definition 
 */
void CTcTokenizer::add_define(const char *sym, size_t len,
                              const char *expansion, size_t expan_len)
{
    CTcHashEntryPp *entry;
    
    /* create an entry for the macro, with no argument list */
    entry = new CTcHashEntryPpDefine(sym, len, TRUE, FALSE, 0, FALSE, 0, 0,
                                     expansion, expan_len);

    /* add the new entry to the table */
    defines_->add(entry);
}

/*
 *   Add a preprocessor macro definition 
 */
void CTcTokenizer::add_define(CTcHashEntryPp *entry)
{
    /* add the entry to our symbol table */
    defines_->add(entry);
}

/*
 *   parse an expression 
 */
int CTcTokenizer::pp_parse_expr(CTcConstVal *val, int read_first,
                                int last_on_line, int add_line_ending)
{
    CTcPrsNode *expr_tree;
    char ch;
    
    /* add the line ending marker if required */
    if (add_line_ending)
    {
        /* 
         *   append the special end-of-preprocess-line to the macro
         *   expansion buffer 
         */
        ch = TOK_END_PP_LINE;
        expbuf_.append(&ch, 1);
    }

    /* 
     *   note that we're pasing a preprocessor expression; this affects
     *   error logging in certain cases 
     */
    in_pp_expr_ = TRUE;

    /* 
     *   parse the expression in preprocessor mode, so that double-quoted
     *   strings can be concatenated and compared 
     */
    G_prs->set_pp_expr_mode(TRUE);

    /* get the first token on the line if desired */
    if (read_first)
        next();

    /* parse the expression */
    expr_tree = G_prs->parse_expr();

    /* make sure we're at the end of the line if desired */
    if (last_on_line && next() != TOKT_EOF)
        log_error(TCERR_PP_EXPR_EXTRA);

    /* if we added the special pp-line-ending marker, remove it */
    if (add_line_ending)
    {
        /* 
         *   the marker is always the last character - remove it simply by
         *   shortening the buffer by a character 
         */
        expbuf_.set_text_len(expbuf_.get_text_len() - 1);
    }

    /* return to normal expression mode */
    G_prs->set_pp_expr_mode(FALSE);

    /* return to normal tokenizing mode */
    in_pp_expr_ = FALSE;

    /* if we didn't get a valid expression, return failure */
    if (expr_tree == 0)
        return 1;

    /* make sure we got a constant */
    if (!expr_tree->is_const())
    {
        log_error(TCERR_PP_EXPR_NOT_CONST);
        return 1;
    }

    /* fill in the caller's value */
    *val = *expr_tree->get_const_val();

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   #define enumeration callback context 
 */
struct def_enum_cb_t
{
    /* original callback function */
    void (*cb)(void *, CTcHashEntryPp *);

    /* original callback context */
    void *ctx;
};

/* 
 *   #define enumeration callback.  This is a simple impedence matcher on the
 *   way to the real callbac; we cast the generic hash entry type to the
 *   CTcHashEntryPp subclass for the benefit of the real callback.  
 */
static void enum_defines_cb(void *ctx0, CVmHashEntry *entry)
{
    def_enum_cb_t *ctx;

    /* get our real context */
    ctx = (def_enum_cb_t *)ctx0;

    /* invoke the real callback, casting the entry reference appropriately */
    (*ctx->cb)(ctx->ctx, (CTcHashEntryPp *)entry);
}

/*
 *   Enumerate the entries in the #define table through a callback 
 */
void CTcTokenizer::enum_defines(void (*cb)(void *, CTcHashEntryPp *),
                                void *ctx)
{
    def_enum_cb_t myctx;

    /* set up our impedence-matcher context with the real callback info */
    myctx.cb = cb;
    myctx.ctx = ctx;

    /* enumerate through our impedence-matcher callback */
    defines_->enum_entries(&enum_defines_cb, &myctx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a lone identifier for a preprocessor directive.  The identifier
 *   must be the only thing left on the line; we'll generate an error if
 *   extra characters follow on the line.
 *   
 *   If there's no identifier on the line, or there's more information
 *   after the identifier, logs an error and returns non-zero; returns
 *   zero on success.  
 */
int CTcTokenizer::pp_get_lone_ident(char *buf, size_t bufl)
{
    /* get the next token, and make sure it's a symbol */
    if (next_on_line() != TOKT_SYM)
    {
        log_error_curtok(TCERR_BAD_DEFINE_SYM);
        return 1;
    }

    /* return an error if it doesn't fit */
    if (curtok_.get_text_len() > bufl)
        return 1;

    /* copy the text */
    memcpy(buf, curtok_.get_text(), curtok_.get_text_len());
    buf[curtok_.get_text_len()] = '\0';

    /* make sure there's nothing else on the line but whitespace */
    if (next_on_line() != TOKT_EOF)
    {
        log_error(TCERR_PP_EXTRA);
        return 1;
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Push a new #if level
 */
void CTcTokenizer::push_if(tok_if_t state)
{
    /* if we're out of space in the stack, throw a fatal error */
    if (if_sp_ == TOK_MAX_IF_NESTING)
        throw_fatal_error(TCERR_IF_NESTING_OVERFLOW);

    /* 
     *   if we're in a nested #if in a false #if, increase the nested
     *   false #if level 
     */
    if (in_false_if())
        ++if_false_level_;

    /* push the state, remembering where the #if was defined */
    if_stack_[if_sp_].desc = last_desc_;
    if_stack_[if_sp_].linenum = last_linenum_;
    if_stack_[if_sp_++].state = state;
}

/*
 *   Pop a #if level 
 */
void CTcTokenizer::pop_if()
{
    /* if we're in a nested #if in a false #if, pop the nesting level */
    if (if_false_level_ != 0)
        --if_false_level_;

    /* pop the main if level */
    if (if_sp_ != 0)
        --if_sp_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Log an error 
 */
void CTcTokenizer::log_error(int errnum, ...)
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(G_tok->get_last_desc(), G_tok->get_last_linenum(),
                          TC_SEV_ERROR, errnum, marker);
    va_end(marker);
}

/*
 *   Log an error with the current token's text as the parameter data,
 *   suitable for use with a "%.*s" display format entry 
 */
void CTcTokenizer::log_error_curtok(int errnum)
{
    /* 
     *   display the message, passing "%.*s" parameter data for the
     *   current token text: an integer giving the length of the token
     *   text, and a pointer to the token text 
     */
    log_error_or_warning_curtok(TC_SEV_ERROR, errnum);
}

/*
 *   Log an error or warning for the current token 
 */
void CTcTokenizer::log_error_or_warning_curtok(tc_severity_t sev, int errnum)
{
    /* log the error with our current token */
    log_error_or_warning_with_tok(sev, errnum, getcur());
}

/*
 *   Log an error or warning with the given token 
 */
void CTcTokenizer::log_error_or_warning_with_tok(
    tc_severity_t sev, int errnum, const CTcToken *tok)
{
    const char *tok_txt;
    size_t tok_len;
    char buf[128];
    const char *prefix;
    const char *suffix;
    utf8_ptr src;
    utf8_ptr dst;
    size_t rem;
    size_t outchars;

    /* see what we have */
    switch(tok->gettyp())
    {
    case TOKT_SSTR:
        /* show the string in quotes, but limit the length */
        prefix = "'";
        suffix = "'";
        goto format_string;

    case TOKT_DSTR:
        prefix = "\"";
        suffix = "\"";
        goto format_string;
        
    case TOKT_DSTR_START:
        prefix = "\"";
        suffix = "<<";
        goto format_string;
        
    case TOKT_DSTR_MID:
        prefix = ">>";
        suffix = "<<";
        goto format_string;

    case TOKT_DSTR_END:
        prefix = ">>";
        suffix = "\"";
        goto format_string;

    case TOKT_RESTR:
        prefix = "R'";
        suffix = "'";
        goto format_string;

    format_string:
        /* set the prefix */
        strcpy(buf, prefix);
        
        /* 
         *   show the string, but limit the length, and convert control
         *   characters to escaped representation 
         */
        src.set((char *)tok->get_text());
        rem = tok->get_text_len();
        for (dst.set(buf  + strlen(buf)), outchars = 0 ;
             rem != 0 && outchars < 20 ; src.inc(&rem), ++outchars)
        {
            /* if this is a control character, escape it */
            if (src.getch() < 32)
            {
                dst.setch('\\');
                
                switch(src.getch())
                {
                case 10:
                    dst.setch('n');
                    break;

                case 13:
                    dst.setch('r');
                    break;

                case 0x000F:
                    dst.setch('^');
                    break;

                case 0x000E:
                    dst.setch('v');
                    break;

                case 0x000B:
                    dst.setch('b');
                    break;

                case 0x0015:
                    dst.setch(' ');
                    break;

                case 9:
                    dst.setch('t');
                    break;

                default:
                    dst.setch('x');
                    dst.setch('0' + int_to_xdigit((src.getch() >> 12) & 0xf));
                    dst.setch('0' + int_to_xdigit((src.getch() >> 8) & 0xf));
                    dst.setch('0' + int_to_xdigit((src.getch() >> 4) & 0xf));
                    dst.setch('0' + int_to_xdigit((src.getch()) & 0xf));
                    break;
                }
            }
            else
            {
                /* put this character as-is */
                dst.setch(src.getch());
            }
        }

        /* if there's more string left, add "..." */
        if (rem != 0)
        {
            dst.setch('.');
            dst.setch('.');
            dst.setch('.');
        }

        /* add the suffix */
        strcpy(dst.getptr(), suffix);

        /* use this buffer as the token string to display */
        tok_txt = buf;
        tok_len = strlen(tok_txt);
        break;

    case TOKT_EOF:
        /* show a special "<End Of File>" marker */
        tok_txt = "<End Of File>";
        tok_len = strlen(tok_txt);
        break;
        
    default:
        /* just show the current token text */
        tok_txt = tok->get_text();
        tok_len = tok->get_text_len();
        break;
    }
    
    /* log the error */
    G_tcmain->log_error(get_last_desc(), get_last_linenum(),
                        sev, errnum, tok_len, tok_txt);
}

/*
 *   Log a warning 
 */
void CTcTokenizer::log_warning(int errnum, ...)
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(G_tok->get_last_desc(), G_tok->get_last_linenum(),
                          TC_SEV_WARNING, errnum, marker);
    va_end(marker);
}

/*
 *   Log a pedantic warning 
 */
void CTcTokenizer::log_pedantic(int errnum, ...)
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(G_tok->get_last_desc(), G_tok->get_last_linenum(),
                          TC_SEV_PEDANTIC, errnum, marker);
    va_end(marker);
}

/*
 *   Log a warning with the current token's text as the parameter data,
 *   suitable for use with a "%.*s" display format entry 
 */
void CTcTokenizer::log_warning_curtok(int errnum)
{
    /* 
     *   display the warning message, passing "%.*s" parameter data for
     *   the current token text: an integer giving the length of the token
     *   text, and a pointer to the token text 
     */
    log_error_or_warning_curtok(TC_SEV_WARNING, errnum);
}

/*
 *   Log and throw an internal error 
 */
void CTcTokenizer::throw_internal_error(int errnum, ...)
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(G_tok->get_last_desc(), G_tok->get_last_linenum(),
                          TC_SEV_INTERNAL, errnum, marker);
    va_end(marker);

    /* throw the generic internal error, since we've logged this */
    err_throw(TCERR_INTERNAL_ERROR);
}

/*
 *   Log and throw a fatal error 
 */
void CTcTokenizer::throw_fatal_error(int errnum, ...)
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(G_tok->get_last_desc(), G_tok->get_last_linenum(),
                          TC_SEV_FATAL, errnum, marker);
    va_end(marker);

    /* throw the generic fatal error, since we've logged this */
    err_throw(TCERR_FATAL_ERROR);
}

/*
 *   display a string value 
 */
void CTcTokenizer::msg_str(const char *str, size_t len) const
{
    /* display the string through the host interface */
    G_hostifc->print_msg("%.*s", (int)len, str);
}

/*
 *   display a numeric value 
 */
void CTcTokenizer::msg_long(long val) const
{
    /* display the number through the host interface */
    G_hostifc->print_msg("%ld", val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer Input Stream implementation 
 */

/* 
 *   create a token input stream 
 */
CTcTokStream::CTcTokStream(CTcTokFileDesc *desc, CTcSrcObject *src,
                           CTcTokStream *parent, int charset_error,
                           int init_if_level)
{
    /* remember the underlying source file */
    src_ = src;

    /* remember the file descriptor */
    desc_ = desc;

    /* remember the containing stream */
    parent_ = parent;

    /* the next line to read is line number 1 */
    next_linenum_ = 1;

    /* remember if there was a #charset error */
    charset_error_ = charset_error;

    /* we're not in a comment yet */
    in_comment_ = FALSE;

    /* remember the starting #if level */
    init_if_level_ = init_if_level;

#if 0 // #pragma C is not currently used
    /* 
     *   start out in parent's pragma C mode, or in non-C mode if we have
     *   no parent 
     */
    if (parent != 0)
        pragma_c_ = parent->is_pragma_c();
    else
        pragma_c_ = TRUE;
#endif
}

/*
 *   delete a token input stream 
 */
CTcTokStream::~CTcTokStream()
{
    /* we own the underlying file, so delete it */
    if (src_ != 0)
        delete src_;
}

/* ------------------------------------------------------------------------ */
/*
 *   File Descriptor 
 */

/*
 *   Get the length of a string with each instance of the given quote
 *   character escaped with a backslash.  We'll also count the escapes we
 *   need for each backslash.  
 */
static size_t get_quoted_len(const char *str, wchar_t qu)
{
    utf8_ptr p;
    size_t len;

    /* 
     *   scan the string for instances of the quote mark; each one adds an
     *   extra byte to the length needed, since each one requires a
     *   backslash character to escape the quote mark 
     */
    for (p.set((char *)str), len = strlen(str) ; p.getch() != '\0' ; p.inc())
    {
        wchar_t ch;
        
        /* 
         *   check to see if this character is quotable - it is quotable if
         *   it's a backslash or it's the quote character we're escaping 
         */
        ch = p.getch();
        if (ch == qu || ch == '\\')
        {
            /* 
             *   we need to escape this character, so add a byte for the
             *   backslash we'll need to insert 
             */
            ++len;
        }
    }

    /* return the length we calculated */
    return len;
}

/*
 *   Build a quoted string.  Fills in dst with the source string with each
 *   of the given quote marks and each backslash escaped with a backslash.
 *   Use get_quoted_len() to determine how much space to allocate for the
 *   destination buffer.  
 */
static void build_quoted_str(char *dstbuf, const char *src, wchar_t qu)
{
    utf8_ptr p;
    utf8_ptr dst;

    /* scan the source string for escapable characters */
    for (p.set((char *)src), dst.set(dstbuf), dst.setch(qu) ;
         p.getch() != '\0' ; p.inc())
    {
        wchar_t ch;

        /* get this source character */
        ch = p.getch();

        /* add a quote if we have a backslash or the quote character */
        if (ch == '\\' || ch == qu)
        {
            /* add a backslash to escape the character */
            dst.setch('\\');
        }

        /* add the character */
        dst.setch(ch);
    }

    /* add the close quote and trailing null */
    dst.setch(qu);
    dst.setch('\0');
}

/*
 *   create a file descriptor 
 */
CTcTokFileDesc::CTcTokFileDesc(const char *fname, size_t fname_len,
                               int index, CTcTokFileDesc *orig_desc,
                               const char *orig_fname, size_t orig_fname_len)
{
    const char *rootname;

    /* no source pages are allocated yet */
    src_pages_ = 0;
    src_pages_alo_ = 0;

    /* remember the first instance of this filename in the list */
    orig_ = orig_desc;

    /* there's nothing else in our chain yet */
    next_ = 0;

    /* remember my index in the master list */
    index_ = index;

    /* if there's a filename, save a copy of the name */
    fname_ = lib_copy_str(fname, fname_len);

    /* if there's an original filename save it as well */
    orig_fname_ = lib_copy_str(orig_fname, orig_fname_len);

    /* 
     *   get the root filename, since we need to build a quoted version of
     *   that as well as of the basic filename 
     */
    rootname = os_get_root_name(fname_);

    /* 
     *   Allocate space for the quoted versions of the filename - make room
     *   for the filename plus the quotes (one on each end) and a null
     *   terminator byte.
     */
    dquoted_fname_ = (char *)t3malloc(get_quoted_len(fname_, '"') + 3);
    squoted_fname_ = (char *)t3malloc(get_quoted_len(fname_, '\'') + 3);
    dquoted_rootname_ = (char *)t3malloc(get_quoted_len(rootname, '"') + 3);
    squoted_rootname_ = (char *)t3malloc(get_quoted_len(rootname, '\'') + 3);

    /* build the quoted version of the name */
    build_quoted_str(dquoted_fname_, fname_, '"');
    build_quoted_str(squoted_fname_, fname_, '\'');
    build_quoted_str(dquoted_rootname_, rootname, '"');
    build_quoted_str(squoted_rootname_, rootname, '\'');
}

/*
 *   delete the descriptor 
 */
CTcTokFileDesc::~CTcTokFileDesc()
{
    /* delete the filename and original filename strings */
    lib_free_str(fname_);
    lib_free_str(orig_fname_);

    /* delete the quotable filename strings */
    t3free(dquoted_fname_);
    t3free(squoted_fname_);
    t3free(dquoted_rootname_);
    t3free(squoted_rootname_);

    /* delete each source page we've allocated */
    if (src_pages_ != 0)
    {
        size_t i;

        /* go through the index array and delete each allocated page */
        for (i = 0 ; i < src_pages_alo_ ; ++i)
        {
            /* if this page was allocated, delete it */
            if (src_pages_[i] != 0)
                t3free(src_pages_[i]);
        }
        
        /* delete the source page index array */
        t3free(src_pages_);
    }
}

/*
 *   Source page structure.  Each page tracks a block of source lines.
 */
const size_t TCTOK_SRC_PAGE_CNT = 1024;
struct CTcTokSrcPage
{
    /* 
     *   Array of line entries on this page.  Each entry is zero if it
     *   hasn't been assigned yet, and contains the absolute image file
     *   address of the generated code for the source line if it has been
     *   assigned. 
     */
    ulong ofs[TCTOK_SRC_PAGE_CNT];
};


/*
 *   Add a source line 
 */
void CTcTokFileDesc::add_source_line(ulong linenum, ulong line_addr)
{
    size_t page_idx;
    size_t idx;

    /* get the index of the page containing this source line */
    page_idx = linenum / TCTOK_SRC_PAGE_CNT;

    /* get the index of the entry within the page */
    idx = linenum % TCTOK_SRC_PAGE_CNT;

    /* 
     *   determine if our page index table is large enough, and expand it
     *   if not 
     */
    if (page_idx >= src_pages_alo_)
    {
        size_t siz;
        size_t new_alo;
        
        /* allocate or expand the source pages array */
        new_alo = page_idx + 16;
        siz = new_alo * sizeof(src_pages_[0]);
        if (src_pages_ == 0)
            src_pages_ = (CTcTokSrcPage **)t3malloc(siz);
        else
            src_pages_ = (CTcTokSrcPage **)t3realloc(src_pages_, siz);

        /* clear the new part */
        memset(src_pages_ + src_pages_alo_, 0,
               (new_alo - src_pages_alo_) * sizeof(src_pages_[0]));

        /* remember the new allocation size */
        src_pages_alo_ = new_alo;
    }

    /* if this page isn't allocated, do so now */
    if (src_pages_[page_idx] == 0)
    {
        /* allocate the new page */
        src_pages_[page_idx] = (CTcTokSrcPage *)
                               t3malloc(sizeof(CTcTokSrcPage));

        /* clear it */
        memset(src_pages_[page_idx], 0, sizeof(CTcTokSrcPage));
    }

    /* 
     *   if this source line entry has been previously set, don't change
     *   it; otherwise, store the new setting 
     */
    if (src_pages_[page_idx]->ofs[idx] == 0)
        src_pages_[page_idx]->ofs[idx] = line_addr;
}

/*
 *   Enumerate source lines 
 */
void CTcTokFileDesc::enum_source_lines(void (*cbfunc)(void *, ulong, ulong),
                                       void *cbctx)
{
    size_t page_idx;
    CTcTokSrcPage **pg;

    /* loop over all of the pages */
    for (page_idx = 0, pg = src_pages_ ; page_idx < src_pages_alo_ ;
         ++page_idx, ++pg)
    {
        size_t i;
        ulong linenum;
        ulong *p;

        /* if this page is not populated, skip it */
        if (*pg == 0)
            continue;

        /* calculate the starting line number for this page */
        linenum = page_idx * TCTOK_SRC_PAGE_CNT;
        
        /* loop over the entries on this page */
        for (i = 0, p = (*pg)->ofs ; i < TCTOK_SRC_PAGE_CNT ;
             ++i, ++p, ++linenum)
        {
            /* if this entry has been set, call the callback */
            if (*p != 0)
                (*cbfunc)(cbctx, linenum, *p);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   #define symbol table hash entry 
 */

/*
 *   create an entry 
 */
CTcHashEntryPpDefine::CTcHashEntryPpDefine(const textchar_t *str, size_t len,
                                           int copy, int has_args, int argc,
                                           int has_varargs,
                                           const char **argv,
                                           const size_t *argvlen,
                                           const char *expansion,
                                           size_t expan_len)
    : CTcHashEntryPp(str, len, copy)
{
    /* copy the argument list if necessary */
    has_args_ = has_args;
    has_varargs_ = has_varargs;
    argc_ = argc;
    if (argc != 0)
    {
        int i;
        
        /* allocate the argument list */
        argv_ = (char **)t3malloc(argc * sizeof(*argv_));

        /* allocate the parameters hash table */
        params_table_ = new CVmHashTable(16, new CVmHashFuncCS(), TRUE);

        /* allocate the entry list */
        arg_entry_ = (CTcHashEntryPpArg **)
                     t3malloc(argc * sizeof(arg_entry_[0]));

        /* copy the arguments */
        for (i = 0 ; i < argc ; ++i)
        {
            CTcHashEntryPpArg *entry;
            
            /* copy the argument name */
            argv_[i] = lib_copy_str(argv[i], argvlen[i]);

            /* 
             *   Create the hash entries for this parameters.  We'll use
             *   this entry to look up tokens in the expansion text for
             *   matches to the formal names when expanding the macro.
             *   
             *   Note that we'll refer directly to our local copy of the
             *   argument name, so we don't need to make another copy in
             *   the hash entry.  
             */
            entry = new CTcHashEntryPpArg(argv_[i], argvlen[i], FALSE, i);
            params_table_->add(entry);

            /* add it to our by-index list */
            arg_entry_[i] = entry;
        }
    }
    else
    {
        /* no arguments */
        argv_ = 0;
        params_table_ = 0;
        arg_entry_ = 0;
    }

    /* save the original version of the expansion */
    orig_expan_ = lib_copy_str(expansion, expan_len);
    orig_expan_len_ = expan_len;

    /* parse the expansion, and save the parsed result */
    parse_expansion(argvlen);
}

/*
 *   Parse the expansion text.
 */
void CTcHashEntryPpDefine::parse_expansion(const size_t *argvlen)
{
    /*
     *   If there are arguments, scan the expansion for formal parameter
     *   names.  For each one we find, replace it with the special
     *   TOK_MACRO_FORMAL_FLAG character followed by a one-byte value giving
     *   the argument index.  This special sequence is less costly to find
     *   when we're expanding the macros - by doing the search here, we only
     *   need to do it once, rather than each time we expand the macro.  
     */
    if (argc_ != 0)
    {
        utf8_ptr src;
        size_t dstofs;
        tc_toktyp_t typ;
        CTcToken tok;
        const char *start;
        tok_embed_ctx ec;
        size_t expan_len = orig_expan_len_;

        /* 
         *   Generate our modified expansion text in the tokenizer's macro
         *   definition scratch buffer.  Initially, make sure we have room
         *   for a copy of the text; we'll resize the buffer later if we find
         *   we need even more.  
         */
        CTcTokString *defbuf = &G_tok->defbuf_;
        defbuf->ensure_space(expan_len);

        /* scan for argument names, and replace them */
        for (start = orig_expan_, dstofs = 0, src.set((char *)orig_expan_) ;; )
        {
            /* get the next token */
            typ = CTcTokenizer::next_on_line(&src, &tok, &ec, FALSE);

            /* if we've reached the end of the expansion, we're done */
            if (typ == TOKT_EOF)
                break;

            /* 
             *   If this is a formal parameter name, we'll replace it with a
             *   special two-byte sequence; otherwise, we'll keep it
             *   unchanged.  
             */
            if (typ == TOKT_SYM)
            {
                /* look up the symbol in our argument table */
                CTcHashEntryPpArg *entry =
                    (CTcHashEntryPpArg *)params_table_->find(
                        tok.get_text(), tok.get_text_len());

                /* check if we found it */
                if (entry != 0)
                {
                    /* get the argument number from the entry */
                    int i = entry->get_argnum();
                    
                    /* get the length of the formal name */
                    size_t arg_len = argvlen[i];

                    /* 
                     *   the normal replacement length for a formal parameter
                     *   is two bytes - one byte for the flag, and one for
                     *   the formal parameter index 
                     */
                    size_t repl_len = 2;

                    /* by default, the flag byte is the formal flag */
                    char flag_byte = TOK_MACRO_FORMAL_FLAG;

                    /*
                     *   Check for special varargs control suffixes.  If we
                     *   matched the last argument name, and this is a
                     *   varargs macro, we might have a suffix.  
                     */
                    if (has_varargs_
                        && i == argc_ - 1
                        && src.getch() == '#')
                    {
                        /* check for the various suffixes */
                        if (memcmp(src.getptr() + 1, "foreach", 7) == 0
                            && !is_sym(src.getch_at(8)))
                        {
                            /* 
                             *   include the suffix length in the token
                             *   length 
                             */
                            arg_len += 8;
                            
                            /* 
                             *   the flag byte is the #foreach flag, which is
                             *   a one-byte sequence 
                             */
                            flag_byte = TOK_MACRO_FOREACH_FLAG;
                            repl_len = 1;
                        }
                        else if (memcmp(src.getptr() + 1,
                                        "argcount", 8) == 0
                                 && !is_sym(src.getch_at(9)))
                        {
                            /* 
                             *   include the suffix length in the token
                             *   length 
                             */
                            arg_len += 9;
                            
                            /* 
                             *   the flag byte is the #argcount flag, which
                             *   is a one-byte sequence 
                             */
                            flag_byte = TOK_MACRO_ARGCOUNT_FLAG;
                            repl_len = 1;
                        }
                        else if (memcmp(src.getptr() + 1,
                                        "ifempty", 7) == 0
                                 && !is_sym(src.getch_at(8)))
                        {
                            /* include the length */
                            arg_len += 8;
                            
                            /* set the one-byte flag */
                            flag_byte = TOK_MACRO_IFEMPTY_FLAG;
                            repl_len = 1;
                        }
                        else if (memcmp(src.getptr() + 1,
                                        "ifnempty", 8) == 0
                                 && !is_sym(src.getch_at(9)))
                        {
                            /* include the length */
                            arg_len += 9;
                            
                            /* set the one-byte flag */
                            flag_byte = TOK_MACRO_IFNEMPTY_FLAG;
                            repl_len = 1;
                        }
                    }
                    
                    /* 
                     *   calculate the new length - we're removing the
                     *   argument name and adding the replacement string in
                     *   its place 
                     */
                    size_t new_len = expan_len + repl_len - arg_len;
                    
                    /* 
                     *   we need two bytes for the replacement - if this is
                     *   more than we're replacing, make sure we have room
                     *   for the extra 
                     */
                    if (new_len > expan_len)
                        defbuf->ensure_space(new_len);
                    
                    /* 
                     *   copy everything up to but not including the formal
                     *   name 
                     */
                    if (tok.get_text() > start)
                    {
                        /* store the text */
                        memcpy(defbuf->get_buf() + dstofs,
                               start, tok.get_text() - start);
                        
                        /* move past the stored text in the output */
                        dstofs += tok.get_text() - start;
                    }
                    
                    /* the next segment starts after this token */
                    start = tok.get_text() + arg_len;
                    
                    /* store the flag byte */
                    defbuf->get_buf()[dstofs++] = flag_byte;
                    
                    /* 
                     *   If appropriate, store the argument index - this
                     *   always fits in one byte because our hard limit on
                     *   formal parameters is less than 128 per macro.  Note
                     *   that we add one to the index so that we never store
                     *   a zero byte, to avoid any potential confusion with a
                     *   null terminator byte.  
                     */
                    if (repl_len > 1)
                        defbuf->get_buf()[dstofs++] = (char)(i + 1);
                    
                    /* remember the new length */
                    expan_len = new_len;
                }
            }
        }

        /* copy the last segment */
        if (tok.get_text() > start)
            memcpy(defbuf->get_buf() + dstofs, start, tok.get_text() - start);

        /* set the new length */
        defbuf->set_text_len(expan_len);

        /* save the parsed expansion */
        expan_ = lib_copy_str(defbuf->get_text());
        expan_len_ = expan_len;
    }
    else
    {
        /* 
         *   There are no arguments, so there's no parsing we need to do.
         *   Just use the original as the parsed expansion.  
         */
        expan_ = orig_expan_;
        expan_len_ = orig_expan_len_;
    }
}

/*
 *   delete 
 */
CTcHashEntryPpDefine::~CTcHashEntryPpDefine()
{
    int i;
    
    /* delete the argument list */
    if (argv_ != 0)
    {
        /* delete each argument string */
        for (i = 0 ; i < argc_ ; ++i)
            lib_free_str(argv_[i]);

        /* delete the argument vector */
        t3free(argv_);

        /* delete the argument entry list */
        t3free(arg_entry_);

        /* delete the hash table */
        delete params_table_;
    }

    /* delete the expansion */
    lib_free_str(expan_);
    if (orig_expan_ != expan_)
        lib_free_str(orig_expan_);
}

/*
 *   __LINE__ static buffer 
 */
char CTcHashEntryPpLINE::buf_[20];


/* ------------------------------------------------------------------------ */
/*
 *   Load macro definitions from a file.  
 */
int CTcTokenizer::load_macros_from_file(CVmStream *fp,
                                        CTcTokLoadMacErr *err_handler)
{
    long cnt;
    long i;
    size_t curarg;
    char *argv[TOK_MAX_MACRO_ARGS];
    size_t argvlen[TOK_MAX_MACRO_ARGS];
    size_t maxarg;
    int result;
    char *expan;
    size_t expmaxlen;

    /* we haven't allocated any argument buffers yet */
    maxarg = 0;

    /* allocate an initial expansion buffer */
    expmaxlen = 1024;
    expan = (char *)t3malloc(expmaxlen);

    /* presume success */
    result = 0;

    /* read the number of macros */
    cnt = fp->read_uint4();

    /* read each macro */
    for (i = 0 ; i < cnt ; ++i)
    {
        char namebuf[TOK_SYM_MAX_LEN];
        size_t namelen;
        int flags;
        size_t argc;
        size_t explen;
        CTcHashEntryPp *entry;
        int has_args;
        int has_varargs;

        /* read the name's length */
        namelen = fp->read_uint2();
        if (namelen > sizeof(namebuf))
        {
            /* log an error through the handler */
            err_handler->log_error(1);

            /* give up - we can't read any more of the file */
            result = 1;
            goto done;
        }

        /* read the name */
        fp->read_bytes(namebuf, namelen);

        /* read and decode the flags */
        flags = fp->read_uint2();
        has_args = ((flags & 1) != 0);
        has_varargs = ((flags & 2) != 0);

        /* read the number of arguments, and read each argument */
        argc = fp->read_uint2();
        for (curarg = 0 ; curarg < argc ; ++curarg)
        {
            /* read the length, and make sure it's valid */
            argvlen[curarg] = fp->read_uint2();
            if (argvlen[curarg] > TOK_SYM_MAX_LEN)
            {
                /* log an error */
                err_handler->log_error(2);

                /* give up - we can't read any more of the file */
                result = 2;
                goto done;
            }

            /* 
             *   if we haven't allocated a buffer for this argument slot yet,
             *   allocate it now; allocate the buffer at the maximum symbol
             *   size, so we can reuse the same buffer for an argument of
             *   other macros we read later 
             */
            while (curarg >= maxarg)
                argv[maxarg++] = (char *)t3malloc(TOK_SYM_MAX_LEN);

            /* read the argument text */
            fp->read_bytes(argv[curarg], argvlen[curarg]);
        }

        /* read the expansion size */
        explen = (size_t)fp->read_uint4();

        /* expand the expansion buffer if necessary */
        if (explen > expmaxlen)
        {
            /* 
             *   overshoot a bit, so that we won't have to reallocate again
             *   if we find a slightly larger expansion for a future macro 
             */
            expmaxlen = explen + 512;

            /* allocate the new buffer */
            expan = (char *)t3realloc(expan, expmaxlen);
        }

        /* read the expansion */
        fp->read_bytes(expan, explen);

        /*
         *   Before we create the entry, check to see if there's an existing
         *   entry with the same name.  
         */
        entry = find_define(namebuf, namelen);
        if (entry != 0)
        {
            /*
             *   We have another entry.  If the entry is exactly the same,
             *   then we can simply skip the current entry, because we simply
             *   want to keep one copy of each macro that's defined
             *   identically in mutiple compilation macros.  If the entry is
             *   different from the new one, delete both - a macro which
             *   appears in two or more compilation units with different
             *   meanings is NOT a global macro, and thus we can't include it
             *   in the debugging records.  
             */
            if (entry->is_pseudo()
                || entry->has_args() != has_args
                || entry->has_varargs() != has_varargs
                || entry->get_argc() != (int)argc
                || entry->get_orig_expan_len() != explen
                || memcmp(entry->get_orig_expansion(), expan, explen) != 0)
            {
                /*
                 *   The existing entry is different from the new entry, so
                 *   the macro has different meanings in different
                 *   compilation units, hence we cannot keep *either*
                 *   definition in the debug records.  Delete the existing
                 *   macro, and do not create the new macro.  If the existing
                 *   macro is a pseudo-macro, keep the old one (since it's
                 *   provided by the compiler itself), but still discard the
                 *   new one.  
                 */
                if (!entry->is_pseudo())
                    undefine(namebuf, namelen);
            }
            else
            {
                /*
                 *   The new entry is identical to the old one, so keep it.
                 *   We only need one copy of the entry, though, so simply
                 *   keep the old one - there's no need to create a new entry
                 *   for the object file data.  
                 */
            }
        }
        else
        {
            /*
             *   There's no existing macro with the same name, so create a
             *   new entry based on the object file data.  
             */
            entry = new CTcHashEntryPpDefine(namebuf, namelen, TRUE,
                                             has_args, argc, has_varargs,
                                             (const char **)argv, argvlen,
                                             expan, explen);

            /* add it to the preprocessor's macro symbol table */
            add_define(entry);
        }
    }

done:
    /* free the argument buffers we allocated */
    for (curarg = 0 ; curarg < maxarg ; ++curarg)
        t3free(argv[curarg]);

    /* free the expansion buffer */
    t3free(expan);

    /* success */
    return result;
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context for writing enumerated #define symbols to a file 
 */
struct write_macro_ctx_t
{
    /* object file we're writing to */
    CVmFile *fp;

    /* number of symbols written so far */
    unsigned long cnt;
};

/*
 *   Enumeration callback for writing the #define symbols to a file 
 */
static void write_macros_cb(void *ctx0, CTcHashEntryPp *entry)
{
    write_macro_ctx_t *ctx = (write_macro_ctx_t *)ctx0;
    int flags;
    int i;
    CVmFile *fp = ctx->fp;

    /* 
     *   if this is a pseudo-macro (such as __LINE__ or __FILE__), ignore it
     *   - these macros do not have permanent global definitions, so they're
     *   not usable in the debugger 
     */
    if (entry->is_pseudo())
        return;

    /* 
     *   If the macro was ever redefined or undefined, ignore it - the
     *   debugger can only use truly global macros, which are macros that
     *   have stable meanings throughout the compilation units where they
     *   appear (and which do not have different meanings in different
     *   compilation units, but that's not our concern at the moment).  The
     *   preprocessor keeps an "undef" table of everything undefined
     *   (explicitly, or implicitly via redefinition), so look up this macro
     *   in the undef table, and ignore the macro if it we find it.  
     */
    if (G_tok->find_undef(entry->getstr(), entry->getlen()) != 0)
        return;

    /* count this macro */
    ctx->cnt++;

    /* write the macro's name */
    fp->write_uint2(entry->getlen());
    fp->write_bytes(entry->getstr(), entry->getlen());

    /* write the flag bits */
    flags = 0;
    if (entry->has_args()) flags |= 1;
    if (entry->has_varargs()) flags |= 2;
    fp->write_uint2(flags);

    /* write the number of arguments, and write each argument */
    fp->write_uint2(entry->get_argc());
    for (i = 0 ; i < entry->get_argc() ; ++i)
    {
        CTcHashEntryPpArg *arg;

        /* get the argument */
        arg = entry->get_arg_entry(i);

        /* write the parameter name */
        fp->write_uint2(arg->getlen());
        fp->write_bytes(arg->getstr(), arg->getlen());
    }

    /* write the expansion */
    fp->write_uint4(entry->get_orig_expan_len());
    fp->write_bytes(entry->get_orig_expansion(), entry->get_orig_expan_len());
}

/*
 *   Write all #define symbols to a file, for debugging purposes.  Writes
 *   only symbols that have never been undefined or redefined, since the
 *   debugger can only make use of global symbols (i.e., symbols with
 *   consistent meanings through all compilation units in which they
 *   appear).  
 */
void CTcTokenizer::write_macros_to_file_for_debug(CVmFile *fp)
{
    long pos;
    long endpos;
    write_macro_ctx_t ctx;

    /* write a placeholder for the symbol count */
    pos = fp->get_pos();
    fp->write_uint4(0);

    /* write the symbols */
    ctx.fp = fp;
    ctx.cnt = 0;
    enum_defines(&write_macros_cb, &ctx);

    /* go back and fix up the symbol count */
    endpos = fp->get_pos();
    fp->set_pos(pos);
    fp->write_uint4(ctx.cnt);

    /* seek back to where we left off */
    fp->set_pos(endpos);
}
