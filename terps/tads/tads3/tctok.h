/* $Header: d:/cvsroot/tads/tads3/tctok.h,v 1.5 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tctok.h - TADS3 compiler tokenizer and preprocessor
Function
  
Notes
  The tokenizer is layered with the preprocessor, so that the preprocessor
  can deal with include files, macro expansion, and preprocessor directives.
Modified
  04/12/99 MJRoberts  - Creation
*/

#ifndef TCTOK_H
#define TCTOK_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "os.h"
#include "t3std.h"
#include "utf8.h"
#include "vmhash.h"
#include "vmerr.h"
#include "tcerr.h"
#include "tcerrnum.h"


/* ------------------------------------------------------------------------ */
/*
 *   Constants 
 */

/* maximum length of a symbol name, in characters */
const size_t TOK_SYM_MAX_LEN = 80;

/*
 *   Maximum buffer required to hold a symbol, in bytes.  Each UTF-8
 *   character may take up three bytes, plus we need a null terminator
 *   byte. 
 */
const size_t TOK_SYM_MAX_BUFFER = (3*TOK_SYM_MAX_LEN + 1);

/* maximum #if nesting level */
const size_t TOK_MAX_IF_NESTING = 100;

/* maximum number of parameters per macro */
const int TOK_MAX_MACRO_ARGS = 128;

/* 
 *   Special token flag characters - these are a characters that can't
 *   occur in an input file (we guarantee this by converting any
 *   occurrences of this character to a space on reading input).  We use
 *   these to flag certain special properties of tokens in the input
 *   buffer.
 *   
 *   We use ASCII characters in the control range (0x01 (^A) through 0x1A
 *   (^Z), excluding 0x09 (tab), 0x0A (LF), 0x0D (CR), and 0x0C (Page
 *   Feed); a well-formed source file would never use any of these
 *   characters in input.  Even if it does, we won't get confused, since
 *   we'll always translate these to a space if we find them in input; but
 *   choosing characters that *should* never occur in valid input will
 *   ensure that we never alter the meaning of valid source by this
 *   translation.  
 */

/* 
 *   macro parameter flag - we use this in the internal storage of a
 *   #define expansion to flag where the formal parameters are mentioned,
 *   so that we can substitute the actuals when expanding the macro 
 */
const char TOK_MACRO_FORMAL_FLAG = 0x01;

/*
 *   Token fully expanded flag.  Whenever we detect that a particular
 *   token has been fully expanded in the course of a particular macro
 *   expansion, we'll insert this byte before the token; on subsequent
 *   re-scans, whenever we see this flag, we'll realize that the token
 *   needs no further consideration of expansion. 
 */
const char TOK_FULLY_EXPANDED_FLAG = 0x02;

/*
 *   Macro substitution end marker.  Each time we expand a macro, we'll
 *   insert immediately after the macro expansion a special pseudo-token,
 *   consisting of this flag followed by a pointer to the symbol table
 *   entry for the symbol expanded.  As we expand macros, we'll check to
 *   see if any of these special flags appear in the buffer after the
 *   macro about to be expanded.  If we find such a flag matching the
 *   symbol about to be expanded, we'll know the symbol has already been
 *   fully expanded on a previous scan and thus must not be expanded
 *   again.  
 */
const char TOK_MACRO_EXP_END = 0x03;

/*
 *   End-of-line flag.  This serves as a local end-of-file marker for
 *   preprocessor lines.  Because preprocessor lines must be considered in
 *   isolation, we need some way when parsing one to tell the tokenizer
 *   not to try to read another line when it reaches the end of the
 *   current line.  This flag serves this purpose: when the tokenizer
 *   encounters one of these flags, it will simply return end-of-file
 *   until the caller explicitly reads a new source line. 
 */
const char TOK_END_PP_LINE = 0x04;

/*
 *   "#foreach" marker flag.  This marks the presence of a #foreach token in
 *   a macro's expansion.  We leave the text of the expansion area intact,
 *   but we replace the #foreach token with this marker character.  
 */
const char TOK_MACRO_FOREACH_FLAG = 0x05;

/* 
 *   "#argcount" marker flag.  This marks the presence of a #argcount token
 *   in a macro's expansion.  
 */
const char TOK_MACRO_ARGCOUNT_FLAG = 0x06;

/*
 *   "#ifempty" and #ifnempty" marker flags 
 */
const char TOK_MACRO_IFEMPTY_FLAG = 0x07;
const char TOK_MACRO_IFNEMPTY_FLAG = 0x08;

/*
 *   Macro format version number.  The compiler sets up a predefined macro
 *   (__TADS_MACRO_FORMAT_VERSION) with this information.  Since 3.1, the
 *   macro table is visible to user code via t3GetGlobalSymbols() (using the
 *   T3PreprocMacros selector), and this information includes the parsed
 *   format with the embedded flag codes.  The macro information can be used
 *   in DynamicFunc compilation at run-time.  If we ever make incompatible
 *   changes to the internal format, future interpreters will have to
 *   recognize older versions so that they can make the necessary
 *   translations.  By embedding the version information in the table, we
 *   make this recognition possible.  
 */
#define TCTOK_MACRO_FORMAT_VERSION   1


/* ------------------------------------------------------------------------ */
/*
 *   Macro table.  This is a virtualized version of our basic hash table, to
 *   allow specialized versions that provide views on top of other table
 *   structures.  
 */
class CTcMacroTable
{
public:
    virtual ~CTcMacroTable() { }
    
    /* add an entry */
    virtual void add(CVmHashEntry *entry) = 0;

    /* remove an entry */
    virtual void remove(CVmHashEntry *entry) = 0;

    /* find an entry */
    virtual class CVmHashEntry *find(const char *str, size_t len) = 0;

    /* enumerate entries */
    virtual void enum_entries(
        void (*func)(void *ctx, class CVmHashEntry *entry), void *ctx) = 0;

    /* dump the hash table for debugging purposes */
    virtual void debug_dump() = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   #if state 
 */
enum tok_if_t
{
    TOKIF_NONE,                                /* not in a #if block at all */
    TOKIF_IF_YES,                           /* processing a true #if branch */
    TOKIF_IF_NO,                           /* processing a false #if branch */
    TOKIF_IF_DONE,      /* done with true #if/#elif; skip #elif's and #else */
    TOKIF_ELSE_YES,                       /* processing a true #else branch */
    TOKIF_ELSE_NO                        /* processing a false #else branch */
};

/*
 *   #if stack entry 
 */
struct tok_if_info_t
{
    /* state */
    tok_if_t state;

    /* file descriptor and line number of starting #if */
    class CTcTokFileDesc *desc;
    long linenum;
};

/* ------------------------------------------------------------------------ */
/*
 *   Token Types 
 */

enum tc_toktyp_t
{
    TOKT_INVALID,                                          /* invalid token */
    TOKT_NULLTOK,          /* null token - caller should read another token */
    TOKT_EOF,                                                /* end of file */
    TOKT_MACRO_FORMAL,          /* formal parameter replacement placeholder */
    TOKT_MACRO_FOREACH,               /* macro varargs #foreach placeholder */
    TOKT_MACRO_ARGCOUNT,             /* macro varargs #argcount placeholder */
    TOKT_MACRO_IFEMPTY,                       /* #ifempty macro placeholder */
    TOKT_MACRO_IFNEMPTY,                     /* #ifnempty macro placeholder */
    TOKT_SYM,                                              /* symbolic name */
    TOKT_INT,                                                    /* integer */
    TOKT_SSTR,                                      /* single-quoted string */
    TOKT_SSTR_START,         /* start of an sstring with embedding - '...<< */
    TOKT_SSTR_MID,         /* middle of an sstring with embedding - >>...<< */
    TOKT_SSTR_END,             /* end of an sstring with embedding - >>...' */
    TOKT_DSTR,                                      /* double-quoted string */
    TOKT_DSTR_START,          /* start of a dstring with embedding - "...<< */
    TOKT_DSTR_MID,          /* middle of a dstring with embedding - >>...<< */
    TOKT_DSTR_END,              /* end of a dstring with embedding - >>..." */
    TOKT_RESTR,             /* regular expression string - R'...' or R"..." */
    TOKT_LPAR,                                            /* left paren '(' */
    TOKT_RPAR,                                           /* right paren ')' */
    TOKT_COMMA,                                                /* comma ',' */
    TOKT_DOT,                                                 /* period '.' */
    TOKT_LBRACE,                                          /* left brace '{' */
    TOKT_RBRACE,                                         /* right brace '}' */
    TOKT_LBRACK,                                 /* left square bracket '[' */
    TOKT_RBRACK,                                /* right square bracket ']' */
    TOKT_EQ,                                             /* equals sign '=' */
    TOKT_EQEQ,                                   /* double-equals sign '==' */
    TOKT_ASI,                      /* colon-equals assignment operator ':=' */
    TOKT_PLUS,                                             /* plus sign '+' */
    TOKT_MINUS,                                           /* minus sign '-' */
    TOKT_TIMES,                                /* multiplication symbol '*' */
    TOKT_DIV,                                        /* division symbol '/' */
    TOKT_MOD,                                                 /* modulo '%' */
    TOKT_GT,                                       /* greater-than sign '>' */
    TOKT_LT,                                          /* less-than sign '<' */
    TOKT_GE,                                  /* greater-or-equal sign '>=' */
    TOKT_LE,                                     /* less-or-equal sign '<=' */
    TOKT_NE,                                /* not-equals sign '!=' or '<>' */
    TOKT_ARROW,                                        /* arrow symbol '->' */
    TOKT_COLON,                                                /* colon ':' */
    TOKT_SEM,                                              /* semicolon ';' */
    TOKT_AND,                                            /* bitwise AND '&' */
    TOKT_ANDAND,                                        /* logical AND '&&' */
    TOKT_OR,                                              /* bitwise OR '|' */
    TOKT_OROR,                                           /* logical OR '||' */
    TOKT_XOR,                                            /* bitwise XOR '^' */
    TOKT_SHL,                                            /* shift left '<<' */
    TOKT_ASHR,                               /* arithmetic shift right '>>' */
    TOKT_LSHR,                                 /* logical shift right '>>>' */
    TOKT_INC,                                             /* increment '++' */
    TOKT_DEC,                                             /* decrement '--' */
    TOKT_PLUSEQ,                                        /* plus-equals '+=' */
    TOKT_MINEQ,                                        /* minus-equals '-=' */
    TOKT_TIMESEQ,                                      /* times-equals '*=' */
    TOKT_DIVEQ,                                       /* divide-equals '/=' */
    TOKT_MODEQ,                                          /* mod-equals '%=' */
    TOKT_ANDEQ,                                          /* and-equals '&=' */
    TOKT_OREQ,                                            /* or-equals '|=' */
    TOKT_XOREQ,                                          /* xor-equals '^=' */
    TOKT_SHLEQ,                              /* shift-left-and-assign '<<=' */
    TOKT_ASHREQ,                 /* arithmetic shift-right-and-assign '>>=' */
    TOKT_LSHREQ,                   /* logical shift-right-and-assign '>>>=' */
    TOKT_NOT,                                            /* logical not '!' */
    TOKT_BNOT,                                           /* bitwise not '~' */
    TOKT_POUND,                                                /* pound '#' */
    TOKT_POUNDPOUND,                                   /* double-pound '##' */
    TOKT_POUNDAT,                                          /* pound-at '#@' */
    TOKT_ELLIPSIS,                                        /* ellipsis '...' */
    TOKT_QUESTION,                                     /* question mark '?' */
    TOKT_QQ,                                   /* double question mark '??' */
    TOKT_COLONCOLON,                                   /* double-colon '::' */
    TOKT_FLOAT,                                    /* floating-point number */
    TOKT_BIGINT,          /* an integer promoted to a float due to overflow */
    TOKT_AT,                                                     /* at-sign */
    TOKT_DOTDOT,                                       /* range marker '..' */
    TOKT_FMTSPEC,                  /* sprintf format spec for <<%fmt expr>> */

    /* keywords */
    TOKT_SELF,
    TOKT_INHERITED,
    TOKT_ARGCOUNT,
    TOKT_IF,
    TOKT_ELSE,
    TOKT_FOR,
    TOKT_WHILE,
    TOKT_DO,
    TOKT_SWITCH,
    TOKT_CASE,
    TOKT_DEFAULT,
    TOKT_GOTO,
    TOKT_BREAK,
    TOKT_CONTINUE,
    TOKT_FUNCTION,
    TOKT_RETURN,
    TOKT_LOCAL,
    TOKT_OBJECT,
    TOKT_NIL,
    TOKT_TRUE,
    TOKT_PASS,
    TOKT_EXTERNAL,
    TOKT_EXTERN,
    TOKT_FORMATSTRING,
    TOKT_CLASS,
    TOKT_REPLACE,
    TOKT_MODIFY,
    TOKT_NEW,
    TOKT_DELETE,
    TOKT_THROW,
    TOKT_TRY,
    TOKT_CATCH,
    TOKT_FINALLY,
    TOKT_INTRINSIC,
    TOKT_DICTIONARY,
    TOKT_GRAMMAR,
    TOKT_ENUM,
    TOKT_TEMPLATE,
    TOKT_STATIC,
    TOKT_FOREACH,
    TOKT_EXPORT,
    TOKT_DELEGATED,
    TOKT_TARGETPROP,
    TOKT_PROPERTYSET,
    TOKT_TARGETOBJ,
    TOKT_DEFININGOBJ,
    TOKT_TRANSIENT,
    TOKT_REPLACED,
    TOKT_PROPERTY,
    TOKT_OPERATOR,
    TOKT_METHOD,
    TOKT_INVOKEE

    /* type names - formerly reserved but later withdrawn */
//  TOKT_VOID,
//  TOKT_INTKW,
//  TOKT_STRING,
//  TOKT_LIST,
//  TOKT_BOOLEAN,
//  TOKT_ANY
};

/* ------------------------------------------------------------------------ */
/*
 *   Source Block.  As we read the source file, we need to keep quoted
 *   strings and symbol names around for later reference, in case they're
 *   needed after reading more tokens and flushing the line buffer.  We'll
 *   copy needed text into our source blocks, which we keep in memory
 *   throughout the compilation, so that we can be certain we can
 *   reference these strings at any time.  
 */

/* size of a source block */
const size_t TCTOK_SRC_BLOCK_SIZE = 50000;

/* source block class */
class CTcTokSrcBlock
{
public:
    CTcTokSrcBlock()
    {
        /* no next block yet */
        nxt_ = 0;
    }

    ~CTcTokSrcBlock()
    {
        /* delete the next block in line */
        if (nxt_ != 0)
            delete nxt_;
    }

    /* get/set the next block */
    CTcTokSrcBlock *get_next() const { return nxt_; }
    void set_next(CTcTokSrcBlock *blk) { nxt_ = blk; }

    /* get a pointer to the block's buffer */
    char *get_buf() { return buf_; }

private:
    /* the next block in the list */
    CTcTokSrcBlock *nxt_;

    /* bytes of the list entry */
    char buf_[TCTOK_SRC_BLOCK_SIZE];
};


/* ------------------------------------------------------------------------ */
/*
 *   String Buffer.  We use these buffers for reading input lines and
 *   expanding macros.  
 */
class CTcTokString
{
public:
    CTcTokString()
    {
        /* no buffer yet */
        buf_ = 0;
        buf_len_ = 0;
        buf_size_ = 0;
    }

    virtual ~CTcTokString()
    {
        /* delete our buffer */
        if (buf_ != 0)
            t3free(buf_);
    }

    /* ensure that a given amount of space if available */
    virtual void ensure_space(size_t siz)
    {
        /* make sure there's room for the requested size plus a null byte */
        if (buf_size_ < siz + 1)
        {
            /* increase to the next 4k increment */
            buf_size_ = (siz + 4095 + 1) & ~4095;
            
            /* allocate or re-allocate the buffer */
            if (buf_ == 0)
                buf_ = (char *)t3malloc(buf_size_);
            else
                buf_ = (char *)t3realloc(buf_, buf_size_);

            /* throw an error if that failed */
            if (buf_ == 0)
                err_throw(TCERR_NO_STRBUF_MEM);
        }
    }

    /* expand the buffer */
    void expand()
    {
        /* expand to the next 4k increment */
        ensure_space(buf_size_ + 4096);
    }

    /* get the text and the length of the text */
    const char *get_text() const { return buf_; }
    size_t get_text_len() const { return buf_len_; }

    /* get the end of the text */
    const char *get_text_end() const { return buf_ + buf_len_; }

    /* append text to the buffer */
    virtual void append(const char *p) { append(p, strlen(p)); }
    virtual void append(const char *p, size_t len)
    {
        /* make sure we have space available */
        ensure_space(buf_len_ + len);

        /* copy the text onto the end of our buffer */
        memcpy(buf_ + buf_len_, p, len);

        /* add it to the length of the text */
        buf_len_ += len;

        /* null-terminte it */
        buf_[buf_len_] = '\0';
    }

    /* prepend text */
    virtual void prepend(const char *p) { prepend(p, strlen(p)); }
    virtual void prepend(const char *p, size_t len)
    {
        /* make sure we have enough space */
        ensure_space(buf_len_ + len);

        /* 
         *   move the existing text (including the null terminator) up in the
         *   buffer to make room for the prepended text 
         */
        memmove(buf_ + len, buf_, buf_len_ + 1);

        /* copy the new text to the start of the buffer */
        memcpy(buf_, p, len);

        /* count the new size */
        buf_len_ += len;
    }

    /* 
     *   Append a string to the buffer, enclosing the text in single or
     *   double quote (as given by 'qu', which must be either '"' or '\'')
     *   and backslash-escaping any occurrences of the same quote character
     *   found within the string.  
     */
    void append_qu(char qu, const char *p) { append_qu(qu, p, strlen(p)); }
    void append_qu(char qu, const char *p, size_t len)
    {
        const char *start;
        
        /* append the open quote */
        append(&qu, 1);

        /* scan for quotes we'll need to escape */
        while (len != 0)
        {
            size_t rem;
            
            /* skip to the next quote */
            for (start = p, rem = len ; rem != 0 && *p != qu ; ++p, --rem) ;

            /* insert the chunk up to the quote */
            if (p != start)
                append(start, p - start);

            /* if we did find a quote, append it with a backslash escape */
            if (rem != 0)
            {
                /* append the backslash and the quote */
                append("\\", 1);
                append(&qu, 1);

                /* skip the quote in the source */
                ++p;
                --rem;
            }

            /* we now only have 'rem' left to consider */
            len = rem;
        }

        /* finally, append the closing quote */
        append(&qu, 1);
    }

    /* insert text into the buffer at the given offset */
    virtual void insert(int ofs, const char *p, size_t len)
    {
        /* check to see if there's anything after the insertion point */
        if ((size_t)ofs >= buf_len_)
        {
            /* 
             *   there's nothing after the insertion point, so this is simply
             *   equivalent to 'append' - go do the append, and we're done 
             */
            append(p, len);
            return;
        }

        /* ensure there's space for the added text */
        ensure_space(buf_len_ + len);

        /* 
         *   Move the existing text after the insertion point just far enough
         *   to make room for the new text.  Include the null terminator.  
         */
        memmove(buf_ + ofs + len, buf_ + ofs, buf_len_ - ofs + 1);

        /* copy the new text in at the given offset */
        memcpy(buf_ + ofs, p, len);

        /* include the new text in our length */
        buf_len_ += len;
    }

    /* copy text into the buffer, replacing existing text */
    virtual void copy(const char *p, size_t len)
    {
        /* ensure we have enough space */
        ensure_space(len);

        /* copy the text */
        memcpy(buf_, p, len);

        /* set our length */
        buf_len_ = len;

        /* null-terminate it */
        buf_[buf_len_] = '\0';
    }

    /* clear any existing text */
    virtual void clear_text()
    {
        /* zero the length */
        buf_len_ = 0;

        /* put a null terminator at the start of the buffer if possible */
        if (buf_size_ > 0)
            buf_[0] = '\0';
    }

    /* get the buffer, for copying text directly into it */
    virtual char *get_buf() const { return buf_; }
    size_t get_buf_size() const { return buf_size_; }

    /* 
     *   Set the text length - use this after copying directly into the
     *   buffer to set the length, excluding the null terminator.  We'll
     *   add a null terminator at the given length.  
     */
    virtual void set_text_len(size_t len)
    {
        /* set the new length */
        buf_len_ = len;

        /* add a null terminator after the new length */
        if (len < buf_size_)
            buf_[len] = '\0';
    }

protected:
    /* buffer */
    char *buf_;

    /* size of the buffer */
    size_t buf_size_;

    /* length of the text in the buffer (excluding trailing null) */
    size_t buf_len_;
};


/*
 *   String buffer subclass for a non-allocated string that merely
 *   references another buffer.  This can be used anywhere a CTcString is
 *   required, but does not require any allocation.
 *   
 *   These objects can only be used in 'const' contexts: the underlying
 *   buffer cannot be changed or expanded, since we do not own the
 *   underlying buffer.  
 */
class CTcTokStringRef: public CTcTokString
{
public:
    CTcTokStringRef()
    {
        /* we have no referenced buffer yet */
        buf_ = 0;
        buf_size_ = 0;
        buf_len_ = 0;
    }

    ~CTcTokStringRef()
    {
        /* we don't own the underlying buffer, so simply forget about it */
        buf_ = 0;
    }

    /* we can't make any changes to the underlying buffer */
    void ensure_space(size_t) { }
    void append(const char *) { assert(FALSE); }
    void append(const char *, size_t) { assert(FALSE); }
    void prepend(const char *) { assert(FALSE); }
    void prepend(const char *, size_t) { assert(FALSE); }
    void insert(int, const char *, size_t) { assert(FALSE); }
    void copy(const char *, size_t) { assert(FALSE); }
    void clear_text() { assert(FALSE); }
    char *get_buf() const { assert(FALSE); return 0; }
    void set_text_len(size_t) { assert(FALSE); }

    /* set my underlying buffer */
    void set_buffer(const char *buf, size_t len)
    {
        buf_ = (char *)buf;
        buf_size_ = len + 1;
        buf_len_ = len;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   String embedding context.  This keeps track of the token structure for
 *   embedded expressions within strings using << >>. 
 */
struct tok_embed_level
{
    /* parenthesis depth within the expression */
    int parens;

    /* token type to switch back to on ending the string */
    tc_toktyp_t endtok;

    /* the quote character for the enclosing string */
    wchar_t qu;

    /* true -> the enclosing string is a triple-quoted string */
    int triple;

    void enter(wchar_t qu, int triple)
    {
        this->parens = 0;
        this->endtok = (qu == '"' ? TOKT_DSTR_END : TOKT_SSTR_END);
        this->qu = qu;
        this->triple = triple;
    }
};
struct tok_embed_ctx
{
    tok_embed_ctx() { reset(); }

    void reset()
    {
        level = 0;
        s = 0;
    }
    
    void start_expr(wchar_t qu, int triple, int report);

    void end_expr()
    {
        if (level > 0)
            --level;
        if (level == 0)
            s = 0;
        else if (level < countof(stk))
            s = stk + level - 1;
    }

    /* are we in an embedded expression? */
    int in_expr() const { return level != 0; }

    /* paren nesting at current level */
    int parens() const { return in_expr() ? s->parens : 0; }

    /* inc/dec paren nesting level */
    void parens(int inc)
    {
        if (in_expr())
        {
            if ((s->parens += inc) < 0)
                s->parens = 0;
        }
    }

    /* ending token type at current level */
    tc_toktyp_t endtok() const { return in_expr() ? s->endtok : TOKT_INVALID; }

    /* ending quote at current level */
    wchar_t qu() const { return in_expr() ? s->qu : 0; }

    /* ending quote is triple quote at current level */
    int triple() const { return in_expr() ? s->triple : FALSE; }

    /* nesting level */
    int level;

    /* stack pointer */
    tok_embed_level *s;

    /* stack */
    tok_embed_level stk[10];
};

/* ------------------------------------------------------------------------ */
/*
 *   Token 
 */
class CTcToken
{
public:
    CTcToken() { }
    CTcToken(tc_toktyp_t typ) : typ_(typ) { }
    
    /* get/set the token type */
    tc_toktyp_t gettyp() const { return typ_; }
    void settyp(tc_toktyp_t typ) { typ_ = typ; }

    /* get/set the fully-expanded flag */
    int get_fully_expanded() const { return fully_expanded_; }
    void set_fully_expanded(int flag) { fully_expanded_ = flag; }
    
    /* get/set the text pointer */
    const char *get_text() const { return text_; }
    size_t get_text_len() const { return text_len_; }
    void set_text(const char *txt, size_t len)
    {
        text_ = txt;
        text_len_ = len;
    }

    /* get/set the integer value */
    ulong get_int_val() const { return int_val_; }
    void set_int_val(ulong val)
    {
        typ_ = TOKT_INT;
        int_val_ = val;
    }

    /* 
     *   compare the text to the given string - returns true if the text
     *   matches, false if not 
     */
    int text_matches(const char *txt, size_t len) const
    {
        return (len == text_len_ && memcmp(txt, text_, len) == 0);
    }
    int text_matches(const char *txt) const
    {
        return text_matches(txt, txt != 0 ? strlen(txt) : 0);
    }

    /* copy from a another token */
    void set(const CTcToken &tok)
    {
        typ_ = tok.typ_;
        text_ = tok.text_;
        text_len_ = tok.text_len_;
        int_val_ = tok.int_val_;
        fully_expanded_ = tok.fully_expanded_;
    }

private:
    /* token type */
    tc_toktyp_t typ_;
    
    /* 
     *   Pointer to the token's text.  This is a pointer into the
     *   tokenizer's symbol table or into the token list itself, so this
     *   pointer is valid as long as the tokenizer and its token list are
     *   valid.
     */
    const char *text_;
    size_t text_len_;

    /* integer value - valid when the token type is TOKT_INT */
    ulong int_val_;

    /* 
     *   flag: the token has been fully expanded, and should not be
     *   expanded further on any subsequent rescan for macros 
     */
    uint fully_expanded_ : 1;
};

/* 
 *   Token list entry.  This is a generic linked list element containing a
 *   token.  
 */
class CTcTokenEle: public CTcToken
{
public:
    CTcTokenEle() { nxt_ = prv_ = 0; }

    /* get/set the next element */
    CTcTokenEle *getnxt() const { return nxt_; }
    void setnxt(CTcTokenEle *nxt) { nxt_ = nxt; }

    /* get/set the previous element */
    CTcTokenEle *getprv() const { return prv_; }
    void setprv(CTcTokenEle *prv) { prv_ = prv; }

protected:
    /* next/previous token in list */
    CTcTokenEle *nxt_;
    CTcTokenEle *prv_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Macro Expansion Resource object.  This object is a collection of
 *   resources that are needed for a macro expansion.  To avoid frequent
 *   allocating and freeing of these resources, we keep a pool of these
 *   objects around so that we can re-use them as needed.  We'll
 *   dynamically expand the pool as necessary, so this doesn't impose any
 *   pre-set limits; it simply avoids lots of memory allocation activity. 
 */
class CTcMacroRsc
{
public:
    CTcMacroRsc()
    {
        /* we're not in any lists yet */
        next_avail_ = 0;
        next_ = 0;
    }
    
    /* buffer for expansion of the whole line */
    CTcTokString line_exp_;

    /* buffer for expansion of current macro on line */
    CTcTokString macro_exp_;

    /* buffer for expansion of an actual parameter value */
    CTcTokString actual_exp_buf_;

    /* next resource object in the "available" list */
    CTcMacroRsc *next_avail_;

    /* next resource object in the master list */
    CTcMacroRsc *next_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Abstract token source interface.  This is used to allow external code
 *   to inject their own substreams into the main token stream.  
 */
class CTcTokenSource
{
public:
    virtual ~CTcTokenSource() { }

    /* 
     *   Get the next token from the source.  Returns null if there are no
     *   more tokens.  
     */
    virtual const CTcToken *get_next_token() = 0;

    /* set the enclosing external token source and current token */
    void set_enclosing_source(CTcTokenSource *src, const CTcToken *tok)
    {
        /* remember the enclosing source */
        enclosing_src_ = src;

        /* remember the current token */
        enclosing_curtok_ = *tok;
    }

    /* get the enclosing external token source */
    CTcTokenSource *get_enclosing_source() const
        { return enclosing_src_; }

    /* get the token that was current when this source was inserted */
    const CTcToken *get_enclosing_curtok() const
        { return &enclosing_curtok_; }

protected:
    /* the enclosing external token source */
    CTcTokenSource *enclosing_src_;

    /* 
     *   the current token in effect enclosing this source - this is the
     *   token that comes immediately after the source's tokens, because a
     *   source is inserted before the current token 
     */
    CTcToken enclosing_curtok_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Newline spacing modes.  The newline spacing mode controls how line
 *   breaks are handled within strings.
 */

enum newline_spacing_mode_t
{
    /* delete: a newline and immediately following whitespace are deleted */
    NEWLINE_SPACING_DELETE = 0,

    /* 
     *   collapse: a newline and immediately following whitespace are
     *   replaced with a single space character 
     */
    NEWLINE_SPACING_COLLAPSE = 1,

    /* 
     *   preserve: newlines and subsequent whitespace are preserved exactly
     *   as written in the source code 
     */
    NEWLINE_SPACING_PRESERVE = 2
};


/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer.  This object reads a file and constructs a representation
 *   of the file as a token list in memory.  The tokenizer interprets
 *   preprocessor directives and expands macros.  
 */
class CTcTokenizer
{
    friend class CTcHashEntryPpDefine;
    
public:
    /*
     *   Create the tokenizer and start reading from the given file.  The
     *   default character set is generally specified by the user (on the
     *   compiler command line, for example), or obtained from the
     *   operating system.
     */
    CTcTokenizer(class CResLoader *res_loader, const char *default_charset);

    /* destroy the tokenizer */
    ~CTcTokenizer();

    /*
     *   Reset the tokenizer.  Deletes the current source object and all
     *   saved token text.  This can be used after compilation of a unit
     *   is completed and the intermediate parser state can be completely
     *   discarded. 
     */
    void reset();

    /* 
     *   Set the source file.  'src_filename' is the fully-resolved local
     *   filename of the source file; 'orig_name' is the original name as
     *   given on the command line, in the makefile, or wherever it came
     *   from.  We keep track of the original name so that we can pass
     *   information to the debugger indicating the name as it was originally
     *   given; this is more useful than the resolved filename, because we
     *   might want to run the debugger on another machine with a different
     *   local directory structure.  
     */
    int set_source(const char *src_filename, const char *orig_name);

    /* set the source to a memory buffer */
    void set_source_buf(const char *buf)
        { set_source_buf(buf, buf != 0 ? strlen(buf) : 0); }

    void set_source_buf(const char *buf, size_t len);

    /* 
     *   Add a #include directory to the include path.  We search the
     *   include path in the order in which they were defined.
     */
    void add_inc_path(const char *path);

    /*
     *   Set preprocess-only mode.  In this mode, we'll retain
     *   preprocessor directives that will be needed if the preprocessed
     *   result is itself compiled; for example, we'll retain #line,
     *   #pragma C, #error, and #pragma message directives. 
     */
    void set_mode_pp_only(int flag) { pp_only_mode_ = flag; }

    /*
     *   Set list-includes mode.  In this mode, we'll simply scan source
     *   files and write to the standard output a list of the names of all
     *   of the #include files.  
     */
    void set_list_includes_mode(int flag) { list_includes_mode_ = flag; }

    /* 
     *   Get/set the test-report mode.  In this mode, we'll expand __FILE__
     *   macros with the root name only.  
     */
    int get_test_report_mode() const { return test_report_mode_; }
    void set_test_report_mode(int flag) { test_report_mode_ = flag; }

    /* enable or disable preprocessing directives */
    void enable_pp(int enable) { allow_pp_ = enable; }

    /* get the type of the current token */
    tc_toktyp_t cur() const { return curtok_.gettyp(); }

    /* get the next token, reading a new line of source if necessary */
    tc_toktyp_t next();

    /* 
     *   Un-get the current token and back up to the previous token.  The
     *   previous token becomes the current token, and the current token is
     *   saved on an internal stack to be fetched again after the returned
     *   token.
     *   
     *   With no arguments, this backs up to the internally stored previous
     *   token.  This allows callers to look ahead by one token without
     *   making a list of saved tokens.
     *   
     *   With an argument, this backs up to a prior token saved by the
     *   caller.  This can be used to back up arbitrarily many tokens - it's
     *   up to the caller to save ungettable tokens and push them back into
     *   the stream.  Tokens must be ungotten in reverse order (e.g., get A,
     *   B, C, D, unget D, C, B, A).  Note that the argument is the PREVIOUS
     *   token to restore, not the current token.
     *   
     *   Tokens un-got with this routine are accessible only to next(), not
     *   to any of the lower-level token readers.  
     */
    void unget();
    void unget(const CTcToken *prv);

    /* 
     *   Push a token into the token stream.  The given token becomes the
     *   next token to be retrieved.  This doesn't affect the current token.
     */
    void push(const CTcToken *tok);

    /* get the current token */
    const class CTcToken *getcur() const { return &curtok_; }

    /* 
     *   Copy the current token.  This makes a copy of the token's text in
     *   tokenizer source memory, to ensure that the reference to the text
     *   buffer the caller is keeping will remain valid forever. 
     */
    const class CTcToken *copycur();

    /* make a safely storable copy of a given token */
    void copytok(class CTcToken *dst, const class CTcToken *src);

    /* check to see if the current token matches the given text */
    int cur_tok_matches(const char *txt, size_t len);
    int cur_tok_matches(const char *txt)
        { return cur_tok_matches(txt, strlen(txt)); }

    /* 
     *   Try looking ahead to match a pair of symbols.  If the current token
     *   is a TOKT_SYM token matching sym1, read the next token to see if
     *   it's another TOKT_SYM matching sym2.  If so, skip the second token
     *   and return true.  If not, unget tthe second (so the original token
     *   is current again) and return false.  
     */
    int look_ahead(const char *sym1, const char *sym2);

    /* 
     *   peek ahead - same as look_ahead, but doesn't skip anything even on
     *   successfully matching the token pair
     */
    int peek_ahead(const char *sym1, const char *sym2);

    /*
     *   Set an external token source.  We'll read tokens from this source
     *   until it is exhausted, at which point we'll revert to the enclosing
     *   source.
     *   
     *   The new source is inserted before the current token, so the current
     *   token will become current once again when this source is exhausted.
     *   We'll automatically advance to the next token, which (unless we
     *   have an ungotten token stashed) will go to the first token in the
     *   new source.  
     */
    void set_external_source(CTcTokenSource *src)
    {
        /* 
         *   store the old source in the new source, so we can restore the
         *   old source when we have exhausted the new source 
         */
        src->set_enclosing_source(ext_src_, &curtok_);

        /* set the new external source */
        ext_src_ = src;

        /* skip to the next token */
        next();
    }

    /* clear all external sources, returning to the real token stream */
    void clear_external_sources();

    /* 
     *   assume that we should have found '>>' sequence after an embedded
     *   expression in a string - used by parsers to resynchronize after
     *   an apparent syntax error 
     */
    void assume_missing_str_cont();

    /* define a macro */
    void add_define(const char *sym, size_t len, const char *expansion,
                    size_t expan_len);

    void add_define(const char *sym, const char *expansion, size_t expan_len)
        { add_define(sym, strlen(sym), expansion, expan_len); }

    void add_define(const char *sym, const char *expansion)
        { add_define(sym, strlen(sym), expansion, strlen(expansion)); }

    /* add a macro, given the symbol entry */
    void add_define(class CTcHashEntryPp *entry);

    /* undefine a previously defined macro */
    void undefine(const char *sym, size_t len);
    void undefine(const char *sym) { undefine(sym, strlen(sym)); }

    /* find a #define symbol */
    class CTcHashEntryPp *find_define(const char *sym, size_t len) const;

    /* find an #undef symbol */
    class CTcHashEntryPp *find_undef(const char *sym, size_t len) const;

    /* enumerate all of the #define symbols through a callback */
    void enum_defines(void (*func)(void *ctx, class CTcHashEntryPp *entry),
                      void *ctx);

    /* read the next line and handle preprocessor directives */
    int read_line_pp();

    /* get the file descriptor and line number of the last line read */
    class CTcTokFileDesc *get_last_desc() const { return last_desc_; }
    long get_last_linenum() const { return last_linenum_; }
    void get_last_pos(class CTcTokFileDesc **desc, long *linenum) const
    {
        *desc = last_desc_;
        *linenum = last_linenum_;
    }

    /* 
     *   set the current file descriptor and line number -- this can be
     *   used to force the line position to a previously-saved value
     *   (during code generation, for example) for error-reporting and
     *   debug-record purposes 
     */
    void set_line_info(class CTcTokFileDesc *desc, long linenum)
    {
        last_desc_ = desc;
        last_linenum_ = linenum;
    }

    /* 
     *   Parse a preprocessor constant expression.  We always parse out of
     *   the macro expansion buffer (expbuf_), but the caller must set p_
     *   to point to the starting point on the expansion line prior to
     *   calling this routine.
     *   
     *   If 'read_first' is true, we'll read a token into curtok_ before
     *   parsing; otherwise, we'll assume the caller has already primed
     *   the pump by reading the first token.
     *   
     *   If 'last_on_line' is true, we'll flag an error if anything is
     *   left on the line after we finish parsing the expression.
     *   
     *   If 'add_line_ending' is true, we'll add an end-of-line marker to
     *   the expansion buffer, so that the tokenizer won't attempt to read
     *   past the end of the line.  Since a preprocessor expression must
     *   be contained entirely on a single logical line, we must never try
     *   to read past the end of the current line when parsing a
     *   preprocessor expression.  
     */
    int pp_parse_expr(class CTcConstVal *result,
                      int read_first, int last_on_line, int add_line_ending);

    /* log an error, optionally with parameters */
    static void log_error(int errnum, ...);

    /* 
     *   log an error with the current token text as the parameter,
     *   suitable for a "%.*s" format list entry (hence we'll provide two
     *   parameters: an integer with the length of the token text, and a
     *   pointer to the token text string) 
     */
    void log_error_curtok(int errnum);

    /* log a warning, optionally with parameters */
    static void log_warning(int errnum, ...);

    /* log a pedantic warning */
    static void log_pedantic(int errnum, ...);

    /* log a warning with the current token as the parameter */
    void log_warning_curtok(int errnum);

    /* log a warning or error for the current token */
    void log_error_or_warning_curtok(tc_severity_t sev, int errnum);

    /* log a warning or error for a given token */
    void log_error_or_warning_with_tok(tc_severity_t sev, int errnum,
                                       const CTcToken *tok);

    /* 
     *   log then throw a fatal error (this is different from an internal
     *   error in that it indicates an unrecoverable error in the input;
     *   an internal error indicates that something is wrong with the
     *   compiler itself)
     */
    static void throw_fatal_error(int errnum, ...);

    /* 
     *   log then throw an internal error (internal errors are always
     *   fatal: these indicate that something has gone wrong in the
     *   compiler, and are equivalent to an assert failure) 
     */
    static void throw_internal_error(int errnum, ...);

    /* display a string/number value */
    void msg_str(const char *str, size_t len) const;
    void msg_long(long val) const;

    /* get the current line */
    const char *get_cur_line() const { return linebuf_.get_text(); }
    size_t get_cur_line_len() const { return linebuf_.get_text_len(); }

    /* get/set the #define hash table */
    class CTcMacroTable *get_defines_table() const { return defines_; }
    class CTcMacroTable *set_defines_table(class CTcMacroTable *tab)
    {
        CTcMacroTable *old_tab = defines_;
        defines_ = tab;
        return old_tab;
    }

    /* 
     *   look up a token as a keyword; returns true and fills in 'kw' with
     *   the keyword token ID if the token is in fact a keyword, or
     *   returns false if it's not a keyword 
     */
    int look_up_keyword(const CTcToken *tok, tc_toktyp_t *kw);

    /* 
     *   Get the next token on the line, filling in the token object.
     *   Advances the pointer to the character immediately following the
     *   token.
     *   
     *   If the token is a string, and the string contains backslash
     *   sequences, we'll modify the source string by translating each
     *   backslash sequences; for example, a "\n" sequence is changed into an
     *   ASCII 10.
     *   
     *   'expanding' indicates whether or not we're in the initial macro
     *   expansion pass.  If this is true, we'll suppress error messages
     *   during this pass, as we'll encounter the same tokens again when we
     *   parse the expanded form of the line.  
     */
    static tc_toktyp_t next_on_line(utf8_ptr *p, CTcToken *tok,
                                    tok_embed_ctx *ec, int expanding);

    /*
     *   Get the text of an operator token.  Returns a pointer to a
     *   constant, static, null-terminated string, suitable for use in
     *   error messages.  
     */
    static const char *get_op_text(tc_toktyp_t op);

    /* 
     *   Store text in the source list.  Text stored here is available
     *   throughout compilation.  This routine automatically reserves the
     *   space needed, so do not call 'reserve' or 'commit' separately.  
     */
    const char *store_source(const char *txt, size_t len);

    /* reserve space for text in the source list */
    void reserve_source(size_t len);

    /* 
     *   Store a piece of text into pre-reserved space in the source list.
     *   This can be used to build up a string from several pieces.  You must
     *   call 'reserve' first to allocate the space, and you must explicitly
     *   add a null terminator at the end of the string.  Do not call
     *   'commit'; this automatically commits the space as each substring is
     *   added. 
     */
    const char *store_source_partial(const char *txt, size_t len);

    /*
     *   Get the index of the next source file descriptor that will be
     *   created.  The linker can use this information to fix up
     *   references to file descriptors in an object file when loading
     *   multiple object files.  
     */
    int get_next_filedesc_index() const { return next_filedesc_id_; }

    /* get the number of source file descriptors in the master list */
    int get_filedesc_count() const { return next_filedesc_id_; }

    /* get the file descriptor at the given (0-based) index */
    class CTcTokFileDesc *get_filedesc(size_t idx) const
    {
        /* return the array entry at the index, if the index is valid */
        return (idx < desc_list_cnt_ ? desc_list_[idx] : 0);
    }

    /* get the head of the master source file descriptor list */
    class CTcTokFileDesc *get_first_filedesc() const { return desc_head_; }

    /* 
     *   Create a new file descriptor and add it to the master list.  This
     *   creates the new descriptor unconditionally, even if a descriptor
     *   for the same source file already exists. 
     */
    class CTcTokFileDesc *create_file_desc(const char *fname, size_t len)
        { return get_file_desc(fname, len, TRUE, fname, len); }

    /*
     *   Set the string capture file.  Once this is set, we'll write the
     *   contents of each string token that we encounter to this file,
     *   with a newline after each token.  
     */
    void set_string_capture(osfildef *fp);

    /* write macros to a file, for debugger use */
    void write_macros_to_file_for_debug(class CVmFile *fp);

    /* 
     *   Load macros from a file.  If any errors occur, we'll flag them
     *   through the error handler object and return a non-zero value.
     *   Returns zero on success.  
     */
    int load_macros_from_file(class CVmStream *fp,
                              class CTcTokLoadMacErr *err_handler);

    /* receive notification that the compiler is done with all parsing */
    void parsing_done()
    {
        /* forget any input file position */
        set_line_info(0, 0);
    }

    /*
     *   Stuff text into the tokenizer source stream.  The new text is
     *   inserted at the current read pointer, so that the next token we
     *   fetch will come from the start of the inserted text.  If 'expand' is
     *   true, we'll expand macros in the text; if not, we'll insert the text
     *   exactly as is with no macro expansion.  
     */
    void stuff_text(const char *txt, size_t len, int expand);

private:
    /* skip whitespace and token markers */
    static void skip_ws_and_markers(utf8_ptr *p);
    
    /* 
     *   get the next token on the line; if we go past the end of the
     *   string buffer, we'll return EOF 
     */
    static tc_toktyp_t next_on_line(const CTcTokString *srcbuf, utf8_ptr *p,
                                    CTcToken *tok, tok_embed_ctx *ec,
                                    int expanding);

    /* 
     *   get the next token on the current line, updating the internal
     *   character position pointer to point just past the token, and filling
     *   in the internal current token object with the toen data 
     */
    tc_toktyp_t next_on_line()
        { return next_on_line(&p_, &curtok_, 0, FALSE); }

    /* get the next token on the line, with string translation */
    tc_toktyp_t next_on_line_xlat(tok_embed_ctx *ec)
        { return next_on_line_xlat(&p_, &curtok_, ec); }

    /* 
     *   get the next token, translating strings and storing string and
     *   symbol text in the source block list 
     */
    tc_toktyp_t next_on_line_xlat_keep();

    /* 
     *   get the next token on the line, translating strings to internal
     *   format 
     */
    tc_toktyp_t next_on_line_xlat(utf8_ptr *p, CTcToken *tok,
                                  tok_embed_ctx *ec);

    /* 
     *   translate a string to internal format by converting escape
     *   sequences; overwrites the original buffer 
     */
    tc_toktyp_t xlat_string(utf8_ptr *p, CTcToken *tok, tok_embed_ctx *ec);

    /* 
     *   translate a string into a given buffer; if 'force_embed_end' is
     *   true, we'll act as though we're continuing the string after the
     *   '>>' after an embedded expression, no matter what the actual
     *   input looks like 
     */
    tc_toktyp_t xlat_string_to(char *dst, utf8_ptr *p, CTcToken *tok,
                               tok_embed_ctx *ec, int force_embed_end);

    /* 
     *   Translate a string, saving the translated version in the source
     *   block list.  If 'force_end_embed' is true, we'll act as though we
     *   were looking at '>>' (or, more precisely, we'll act as though
     *   '>>' immediately preceded the current input), regardless of what
     *   the actual input looks like.  
     */
    tc_toktyp_t xlat_string_to_src(tok_embed_ctx *ec, int force_end_embed);

    /* 
     *   is the given token type "safe" (i.e., stored in the tokenizer source
     *   list, so that its text pointer is permanently valid; non-safe tokens
     *   have valid text pointers only until the next token fetch) 
     */
    int is_tok_safe(tc_toktyp_t typ);

    /* initialize the source block list */
    void init_src_block_list();

    /* delete current source file, including all including parents */
    void delete_source();

    /* 
     *   Read the next line; processes comments, but does not expand macros
     *   or parse preprocessor directives.  This always reads into linebuf_;
     *   the return value is the offset within linebuf_ of the new text.  A
     *   return value of -1 indicates that we're at end of file.  
     */
    int read_line(int append);

    /* 
     *   Set the source read pointer to the start of a new line, given the
     *   CTcTokString object containing the buffer, and the offset within
     *   that buffer. 
     */
    void start_new_line(CTcTokString *str, int ofs)
    {
        /* remember the buffer we're reading out of */
        curbuf_ = str;

        /* set the read pointer to the start of the new line's text */
        p_.set((char *)str->get_text() + ofs);
    }

    /* unsplice text from the current line and make it the next line */
    void unsplice_line(const char *new_line_start);

    /* 
     *   Commit space in the source list - this is used when text is directly
     *   stored after reserving space.  The size reserved may be greater than
     *   the size committed, because it is sometimes more efficient to make a
     *   guess that may overestimate the amount we actually end up needing.  
     */
    void commit_source(size_t len);

    /* parse a string */
    static tc_toktyp_t tokenize_string(
        utf8_ptr *p, CTcToken *tok, tok_embed_ctx *ec, int expanding_macros);

    /* process comments */
    void process_comments(size_t start_ofs);

    /* splice lines for a string that runs across multiple lines */
    void splice_string();

    /* scan a sprintf format spec */
    void scan_sprintf_spec(utf8_ptr *p);

    /* translate a \ escape sequence */
    void xlat_escape(utf8_ptr *dst, utf8_ptr *p, wchar_t qu, int triple);

    /* skip an ordinary character or \ escape sequence */
    void skip_escape(utf8_ptr *p);

    /* translate escape sequences in a string segment */
    void xlat_escapes(utf8_ptr *dstp, const utf8_ptr *srcp,
                      const utf8_ptr *endp);

    /* expand macros in the current line */
    int expand_macros_curline(int read_more, int allow_defined,
                              int append_to_expbuf);

    /* 
     *   Expand the macros in the given text, filling in the given
     *   CTcTokString with the results.  The expansion will clear out any
     *   existing text in the result buffer.  Returns zero on success, or
     *   non-zero on error.  
     */
    int expand_macros(class CTcTokString *dest, const char *str, size_t len)
    {
        CTcTokStringRef srcbuf;

        /* set up a CTcTokString for the source */
        srcbuf.set_buffer(str, len);

        /* go expand macros */
        return expand_macros(&srcbuf, 0, dest, FALSE, FALSE, FALSE);
    }

    /* expand all of the macros in the given text */
    int expand_macros(class CTcTokString *srcbuf, utf8_ptr *src,
                      class CTcTokString *expbuf, int read_more,
                      int allow_defined, int append);

    /* expand the macro at the current token on the current line */
    int expand_macro(class CTcMacroRsc *res, class CTcTokString *expbuf,
                     const class CTcTokString *srcbuf, utf8_ptr *src,
                     size_t macro_srcbuf_ofs, CTcHashEntryPp *entry,
                     int read_more, int allow_defined, int *expanded);

    /* 
     *   Remove our special expansion flags from an expanded macro buffer.
     *   This can be called after all expansion has been completed to clean
     *   up the buffer for human consumption. 
     */
    void remove_expansion_flags(CTcTokString *buf);

    /* scan for a prior expansion of a macro within the current context */
    static int scan_for_prior_expansion(utf8_ptr src, const char *src_end,
                                        const class CTcHashEntryPp *entry);

    /* remove end-of-macro-expansion flags from a buffer */
    static void remove_end_markers(class CTcTokString *buf);

    /* change a buffer to use individual token full-expansion markers */
    void mark_full_exp_tokens(CTcTokString *dstbuf,
                              const class CTcTokString *srcbuf,
                              int append) const;

    /* allocate a macro expansion resource */
    class CTcMacroRsc *alloc_macro_rsc();

    /* release a macro expansion resource */
    void release_macro_rsc(class CTcMacroRsc *rsc);

    /* 
     *   Parse the actual parameters to a macro.  Fills in argofs[] and
     *   arglen[] with the offsets (from srcbuf->get_buf()) and lengths,
     *   respectively, of each actual parameter's text. 
     */
    int parse_macro_actuals(const class CTcTokString *srcbuf, utf8_ptr *src,
                            const CTcHashEntryPp *macro_entry,
                            size_t argofs[TOK_MAX_MACRO_ARGS],
                            size_t arglen[TOK_MAX_MACRO_ARGS],
                            int read_more, int *found_actuals);

    /* splice the next line for reading more macro actuals */
    tc_toktyp_t actual_splice_next_line(const CTcTokString *srcbuf,
                                        utf8_ptr *src, CTcToken *tok);

    /* substitute the actual parameters in a macro's expansion */
    int substitute_macro_actuals(class CTcMacroRsc *rsc,
                                 class CTcTokString *subexp,
                                 CTcHashEntryPp *macro_entry,
                                 const class CTcTokString *srcbuf,
                                 const size_t *argofs, const size_t *arglen,
                                 int allow_defined);

    /* stringize a macro actual parameter into an expansion buffer */
    void stringize_macro_actual(class CTcTokString *expbuf,
                                const char *actual_val, size_t actual_len,
                                char quote_char, int add_open_quote,
                                int add_close_quote);

    /* skip a delimited macro expansion area (#foreach, #ifempty, etc) */
    void skip_delimited_group(utf8_ptr *p, int parts_to_skip);

    /* expand a defined() preprocessor operator */
    int expand_defined(class CTcTokString *subexp,
                       const class CTcTokString *srcbuf, utf8_ptr *src);

    /* add a file to the list of files to be included only once */
    void add_include_once(const char *fname);

    /* find a file in the list of files to be included only once */
    int find_include_once(const char *fname);

    /* process a #pragma directive */
    void pp_pragma();

    /* process a #charset directive */
    void pp_charset();

    /* process a #include directive */
    void pp_include();

    /* process a #define directive */
    void pp_define();

    /* process a #if directive */
    void pp_if();

    /* process a #ifdef directive */
    void pp_ifdef();

    /* process a #ifdef directive */
    void pp_ifndef();

    /* process a #ifdef or #ifndef */
    void pp_ifdef_or_ifndef(int sense);

    /* process a #else directive */
    void pp_else();

    /* process a #elif directive */
    void pp_elif();

    /* process a #endif directive */
    void pp_endif();

    /* process a #error directive */
    void pp_error();

    /* process a #undef directive */
    void pp_undef();

    /* process a #line directive */
    void pp_line();

    /* get a lone identifier for a preprocessor directive */
    int pp_get_lone_ident(char *buf, size_t bufl);

    /* process a #pragma C directive */
    // void pragma_c(); - not currently used

    /* process a #pragma once directive */
    void pragma_once();

    /* process a #pragma all_once directive */
    void pragma_all_once();

    /* process a #pragma message directive */
    void pragma_message();

    /* process a #pragma newline_spacing(on/off) directive */
    void pragma_newline_spacing();

    /* process a #pragma sourceTextGroup directive */
    void pragma_source_text_group();

    /* 
     *   Determine if we're in a false #if branch.  If we're inside a #if
     *   block, and the state is either IF_NO, IF_DONE, or ELSE_NO, or
     *   we're inside a #if nested within any negative branch, we're in a
     *   not-taken branch of a #if block.  
     */
    int in_false_if() const
    {
        return (if_sp_ != 0
                && (if_false_level_ != 0
                    || if_stack_[if_sp_ - 1].state == TOKIF_IF_NO
                    || if_stack_[if_sp_ - 1].state == TOKIF_IF_DONE
                    || if_stack_[if_sp_ - 1].state == TOKIF_ELSE_NO));
    }

    /* push a new #if level with the given state */
    void push_if(tok_if_t state);

    /* get the current #if state */
    tok_if_t get_if_state() const
    {
        if (if_sp_ == 0)
            return TOKIF_NONE;
        else
            return if_stack_[if_sp_ - 1].state;
    }

    /* switch the current #if level to the given state */
    void change_if_state(tok_if_t state)
    {
        if (if_sp_ != 0)
            if_stack_[if_sp_ - 1].state = state;
    }

    /* pop the current #if level */
    void pop_if();

    /* 
     *   Find or create a descriptor for the given filename.  'fname' is
     *   the full file system path specifying the file.  'orig_fname' is
     *   the filename as originally specified by the user, if different;
     *   in the case of #include files, this indicates the name that was
     *   specified in the directive itself, whereas 'fname' is the actual
     *   filename that resulted from searching the include path for the
     *   given name. 
     */
    class CTcTokFileDesc *get_file_desc(const char *fname, size_t fname_len,
                                        int always_create,
                                        const char *orig_fname,
                                        size_t orig_fname_len);

    /* clear the line buffer */
    void clear_linebuf();

    /* flag: ALL_ONCE mode - we include each file only once */
    unsigned int all_once_ : 1;

    /* flag: warn on ignoring a redundant #include file */
    unsigned int warn_on_ignore_incl_ : 1;

    /*
     *   Flag: in preprocess-only mode.  In this mode, we'll leave certain
     *   preprocessor directives intact in the source, since they'll be
     *   needed in a subsequent compilation of the preprocessed source.
     *   For example, we'll leave #line directives, #pragma C, #error, and
     *   #pragma message directives in the preprocessed result.  
     */
    unsigned int pp_only_mode_ : 1;

    /* 
     *   Flag: in test reporting mode.  In this mode, we'll expand __FILE__
     *   macros with the root name only. 
     */
    unsigned int test_report_mode_ : 1;

    /*
     *   Flag: in preprocess-for-includes mode.  In this mode, we'll do
     *   nothing except run the preprocessor and generate a list of the
     *   header files that are included, along with header files they
     *   include, and so on.  
     */
    unsigned int list_includes_mode_ : 1;

    /* 
     *   flag: we're parsing a preprocessor constant expression (for a
     *   #if, for example; this doesn't apply to simple macro expansion) 
     */
    unsigned int in_pp_expr_ : 1;

    /* mode for handling newlines in strings */
    newline_spacing_mode_t string_newline_spacing_;

    /* resource loader */
    class CResLoader *res_loader_;

    /* 
     *   name of our default character set - this is generally specified
     *   by the user (on the compiler command line, for example), or
     *   obtained from the operating system 
     */
    char *default_charset_;

    /* input (to unicode) character mapper for the default character set */
    class CCharmapToUni *default_mapper_;

    /* head of list of previously-included files */
    struct tctok_incfile_t *prev_includes_;

    /* head and tail of include path list */
    struct tctok_incpath_t *incpath_head_;
    struct tctok_incpath_t *incpath_tail_;

    /* file descriptor and line number of last line read */
    class CTcTokFileDesc *last_desc_;
    long last_linenum_;

    /* file descriptor and line number of last line appended */
    class CTcTokFileDesc *appended_desc_;
    long appended_linenum_;

    /* current input stream */
    class CTcTokStream *str_;

    /* master list of file descriptors */
    class CTcTokFileDesc *desc_head_;
    class CTcTokFileDesc *desc_tail_;

    /* 
     *   array of file descriptors (we keep the list in both an array and
     *   a linked list, since we need both sequential and indexed access;
     *   this isn't a lot of trouble since we never need to remove an
     *   entry from the list) 
     */
    class CTcTokFileDesc **desc_list_;

    /* number of entries in desc_list_ */
    size_t desc_list_cnt_;

    /* number of slots allocated in desc_list_ array */
    size_t desc_list_alo_;

    /* next file descriptor ID to be assigned */
    int next_filedesc_id_;

    /* pointer to current position in current line */
    utf8_ptr p_;

    /* 
     *   The CTcTokString object containing the current line.  This is the
     *   buffer object we're currently reading from, and will be either
     *   linebuf_ or expbuf_.  p_ always points into this buffer.  
     */
    CTcTokString *curbuf_;

    /* raw file input buffer */
    CTcTokString linebuf_;

    /* 
     *   unsplice buffer - we'll put any unspliced text into this buffer,
     *   then read it back at the next read_line() 
     */
    CTcTokString unsplicebuf_;

    /* macro expansion buffer */
    CTcTokString expbuf_;

    /* macro definition parsing buffer */
    CTcTokString defbuf_;

    /* 
     *   Flag: in a string.  If this is '\0', we're not in a string;
     *   otherwise, this is the quote character that ends the string.
     */
    wchar_t in_quote_;

    /* flag: the in_quote_ string we're in is triple quoted */
    int in_triple_;

    /* embedded expression context in comment */
    tok_embed_ctx comment_in_embedding_;

    /* embedded expression context within a macro expansion */
    tok_embed_ctx macro_in_embedding_;

    /* embedded expression context for the main token stream */
    tok_embed_ctx main_in_embedding_;

    /* 
     *   #if state stack.  if_sp_ is the index of the next nesting slot;
     *   if if_sp_ is zero, it means that we're not in a #if at all.
     *   
     *   Separately, the if_false_level_ is the level of #if's contained
     *   within a false #if branch.  This is separate because, once we're
     *   in a false #if branch, everything within it is false.
     */
    int if_sp_;
    tok_if_info_t if_stack_[TOK_MAX_IF_NESTING];
    int if_false_level_;

    /* source block list head */
    CTcTokSrcBlock *src_head_;

    /* current (and last) source block */
    CTcTokSrcBlock *src_cur_;

    /* pointer to next available byte in the current source block */
    char *src_ptr_;

    /* number of bytes remaining in the current source block */
    size_t src_rem_;

    /* current token */
    CTcToken curtok_;

    /* previous token (for unget) */
    CTcToken prvtok_;

    /* head of allocated list of unget token slots */
    CTcTokenEle *unget_head_;

    /* 
     *   Last unget token.  This is a pointer into the unget list, to the
     *   element containing the last ungotten token.  The next next() will
     *   reinstate this token and move the pointer back into the list; the
     *   next unget() will move the pointer forward and fill in the next
     *   element.  This is null if there are no ungotten tokens.  
     */
    CTcTokenEle *unget_cur_;

    /* the external token source, if any */
    CTcTokenSource *ext_src_;

    /* symbol table for #define symbols */
    class CTcMacroTable *defines_;

    /* 
     *   symbol table for symbols explicitly undefined; we keep track of
     *   these so that we can exclude anything ever undefined from the debug
     *   macro records, since only static global macros can be handled in the
     *   debug records 
     */
    class CVmHashTable *undefs_;

    /* symbol table for TADS keywords */
    class CVmHashTable *kw_;

    /* head of macro resource pool list */
    class CTcMacroRsc *macro_res_head_;

    /* head of list of available macro resources */
    class CTcMacroRsc *macro_res_avail_;

    /* 
     *   string capture file - if this is non-null, we'll capture all of
     *   the strings we read to this file, one string per line 
     */
    class CVmDataSource *string_fp_;

    /* character mapper for writing to the string capture file */
    class CCharmapToLocal *string_fp_map_;

    /* true -> allow preprocessor directives */
    unsigned int allow_pp_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Error handler interface.  Callers of load_macros_from_file() in
 *   CTcTokenizer must provide an implementation of this interface to handle
 *   errors that occur while loading macros.  
 */
class CTcTokLoadMacErr
{
public:
    /* 
     *   Flag an error.  The error codes are taken from the following list:
     *   
     *   1 - a macro name symbol in the file is too long (it exceeds the
     *   maximum symbol length for the preprocessor)
     *   
     *   2 - a formal parameter name is too long 
     */
    virtual void log_error(int err) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer File Descriptor.  Each unique source file has a separate
 *   file descriptor, which keeps track of the file's name. 
 */
class CTcTokFileDesc
{
public:
    /* create a file descriptor */
    CTcTokFileDesc(const char *fname, size_t fname_len, int index,
                   CTcTokFileDesc *orig_desc,
                   const char *orig_fname, size_t orig_fname_len);

    /* delete the descriptor */
    ~CTcTokFileDesc();

    /* get the filename */
    const char *get_fname() const { return fname_; }

    /* get the original filename string */
    const char *get_orig_fname() const { return orig_fname_; }

    /* 
     *   get the filename as a double-quoted string (backslashes and
     *   double-quotes will be escaped with backslashes) 
     */
    const char *get_dquoted_fname() const { return dquoted_fname_; }

    /* 
     *   get the root filename (i.e., with no path prefix) as a
     *   double-quoted string 
     */
    const char *get_dquoted_rootname() const { return dquoted_rootname_; }

    /* get the filename as a single-quoted string */
    const char *get_squoted_fname() const { return squoted_fname_; }

    /* get the root filename as a single-quoted string */
    const char *get_squoted_rootname() const { return squoted_rootname_; }

    /* get/set the next file descriptor in the descriptor chain */
    CTcTokFileDesc *get_next() const { return next_; }
    void set_next(CTcTokFileDesc *nxt) { next_ = nxt; }

    /* get my index in the master list */
    int get_index() const { return index_; }

    /* get the original descriptor for this file in the list */
    CTcTokFileDesc *get_orig() const { return orig_; }

    /* 
     *   get the list index of the original entry (returns my own list
     *   index if I am the original entry) 
     */
    int get_orig_index() const
        { return orig_ == 0 ? index_ : orig_->get_index(); }

    /* 
     *   Add a source line position to our list.  We keep an index of the
     *   byte-code address for each executable source line, so that
     *   debuggers can find the compiled code corresponding to a source
     *   location.  The image builder gives us this information during the
     *   linking process.  The address is the absolute location in the
     *   image file of the executable code for the given source line (the
     *   first line in the file is numbered 1).  
     */
    void add_source_line(ulong linenum, ulong line_addr);

    /* 
     *   Enumerate the source lines, calling the callback for each one.
     *   We will only enumerate source lines which actually have an
     *   associated code location - source lines that generated no
     *   executable code are skipped.  We'll enumerate the lines in
     *   ascending order of line number, and each line number will appear
     *   only once.  
     */
    void enum_source_lines(void (*cbfunc)(void *ctx, ulong linenum,
                                          ulong byte_code_addr),
                           void *cbctx);
    
private:
    /* index in the master list */
    int index_;
    
    /* filename string - this is the actual file system filename */
    char *fname_;

    /* 
     *   original filename string, if different from fname_ - this is the
     *   filename as specified by the user, before it was adjusted with
     *   include paths or other extra location information 
     */
    char *orig_fname_;

    /* double-quoted version of the filename */
    char *dquoted_fname_;

    /* single-quoted version of the filename */
    char *squoted_fname_;

    /* single-quoted version of the root filename */
    char *squoted_rootname_;

    /* double-quoted version of the root filename */
    char *dquoted_rootname_;

    /* next descriptor in the master descriptor list */
    CTcTokFileDesc *next_;

    /* 
     *   The original file descriptor with the same filename.  If we
     *   create multiple descriptors for the same filename (because, for
     *   example, the same header is included in several different object
     *   files), we'll keep track of the original descriptor for the file
     *   in all of the copies. 
     */
    CTcTokFileDesc *orig_;

    /* source line pages */
    struct CTcTokSrcPage **src_pages_;

    /* number of source line page slots allocated */
    size_t src_pages_alo_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer Input Stream 
 */
class CTcTokStream
{
public:
    /* create a token stream */
    CTcTokStream(class CTcTokFileDesc *desc, class CTcSrcObject *src,
                 CTcTokStream *parent, int charset_error,
                 int init_if_level);

    /* delete the stream */
    ~CTcTokStream();
    
    /* get/set the associated file descriptor */
    class CTcTokFileDesc *get_desc() const { return desc_; }
    void set_desc(class CTcTokFileDesc *desc) { desc_ = desc; }

    /* get the underlying source file */
    class CTcSrcObject *get_src() const { return src_; }

    /* get the line number of the next line to be read */
    long get_next_linenum() const { return next_linenum_; }

    /* set the next line number */
    void set_next_linenum(long l) { next_linenum_ = l; }

    /* get the enclosing stream */
    CTcTokStream *get_parent() const { return parent_; }

    /* count having read a line */
    void count_line() { ++next_linenum_; }

    /* was there a #charset error when opening the file? */
    int get_charset_error() const { return charset_error_; }

    /* get/set the in-comment status */
    int is_in_comment() const { return in_comment_; }
    void set_in_comment(int f) { in_comment_ = f; }

    /* get/set the pragma C mode */
    // int is_pragma_c() const { return pragma_c_; }
    // void set_pragma_c(int f) { pragma_c_ = f; }

    /* get/set if nesting level at the start of the file */
    int get_init_if_level() const { return init_if_level_; }
    void set_init_if_level(int level) { init_if_level_ = level; }

    /* get/set the newline spacing mode */
    newline_spacing_mode_t get_newline_spacing() const
        { return newline_spacing_; }
    void set_newline_spacing(newline_spacing_mode_t f)
        { newline_spacing_ = f; }

private:
    /* file descriptor associated with this file */
    class CTcTokFileDesc *desc_;
    
    /* the underlying source reader */
    class CTcSrcObject *src_;

    /* 
     *   the enclosing stream - this is the stream that #include'd the
     *   current stream 
     */
    CTcTokStream *parent_;

    /* line number of next line to be read */
    ulong next_linenum_;

    /* #if nesting level at the start of the file */
    int init_if_level_;

    /* newline_spacing mode when the stream was stacked */
    newline_spacing_mode_t newline_spacing_;

    /* flag: we were unable to load the map in the #charset directive */
    uint charset_error_ : 1;

    /* the stream is in a multi-line comment */
    uint in_comment_ : 1;

    /* flag: we're in #pragma C+ mode */
    // uint pragma_c_ : 1; - #pragma C is not currently used
};

/* ------------------------------------------------------------------------ */
/*
 *   Keyword Hash Table Entry 
 */
class CTcHashEntryKw: public CVmHashEntryCS
{
public:
    CTcHashEntryKw(const textchar_t *str, tc_toktyp_t tokid)
        : CVmHashEntryCS(str, strlen(str), FALSE)
    {
        /* save the token ID for the keyword */
        tokid_ = tokid;
    }

    /* get the token ID */
    tc_toktyp_t get_tok_id() const { return tokid_; }

private:
    /* our token ID */
    tc_toktyp_t tokid_;
};

/* ------------------------------------------------------------------------ */
/*
 *   basic #define symbol table entry 
 */
class CTcHashEntryPp: public CVmHashEntryCS
{
public:
    CTcHashEntryPp(const textchar_t *str, size_t len, int copy)
        : CVmHashEntryCS(str, len, copy)
    {
        /* by default, we have no arguments */
        has_args_ = FALSE;
        has_varargs_ = FALSE;
        argc_ = 0;
        argv_ = 0;
        params_table_ = 0;
    }

    /* get the expansion text */
    virtual const char *get_expansion() const = 0;
    virtual size_t get_expan_len() const = 0;

    /* get the original expansion text, before parsing */
    virtual const char *get_orig_expansion() const { return get_expansion(); }
    virtual size_t get_orig_expan_len() const { return get_expan_len(); }

    /* certain special macros (__LINE__, __FILE__) aren't undef'able */
    virtual int is_undefable() const { return TRUE; }

    /* 
     *   most macros are real symbols, created by #define's, but some are
     *   special pseudo-macros, like __LINE__ and __FILE__, that the
     *   preprocessor provides 
     */
    virtual int is_pseudo() const { return FALSE; }

    /* does the macro have an argument list? */
    int has_args() const { return has_args_; }

    /* get the number of arguments */
    int get_argc() const { return argc_; }

    /* do we have a variable number of arguments? */
    int has_varargs() const { return has_varargs_; }

    /* 
     *   get the minimum number of allowed arguments - if we have varargs,
     *   this is one less than the number of formals listed, since the last
     *   formal can correspond to any number of actuals, including zero 
     */
    int get_min_argc() const { return has_varargs_ ? argc_ - 1 : argc_; }

    /* get the name of an argument by position (0 = first argument) */
    const char *get_arg_name(int idx) const { return argv_[idx]; }

    /* get the parameter hash table entry for the parameter */
    class CTcHashEntryPpArg *get_arg_entry(int idx) const
        { return arg_entry_[idx]; }

    /* get the parameters hash table */
    const CVmHashTable *get_params_table() const { return params_table_; }

protected:
    /* argument list */
    char **argv_;

    /* list of parameter hash entries */
    class CTcHashEntryPpArg **arg_entry_;

    /* parameter hash table */
    CVmHashTable *params_table_;

    /* argument count */
    int argc_;

    /* flag: the macro has a parameter list */
    uint has_args_ : 1;

    /* 
     *   flag: the parameter list takes a variable number of arguments; if
     *   this is set, then argc_ is one greater than the minimum number of
     *   arguments required, and the last formal receives the varying part
     *   of the actual parameter list, which can contain zero or more
     *   actuals 
     */
    uint has_varargs_ : 1;
};

/*
 *   #define symbol hash table entry
 */
class CTcHashEntryPpDefine: public CTcHashEntryPp
{
public:
    /* 
     *   Create the hash entry.  argc is the number of arguments to the
     *   macro, and argv is an array of pointers to null-terminated
     *   strings with the argument names, in the order defined in the
     *   macro.
     *   
     *   If has_args is false, the macro does not take a parameter list at
     *   all.  Note that it is possible for has_args to be true and argc
     *   to be zero, because a macro can be defined to take an argument
     *   list with no arguments (i.e., empty parens).  A macro with an
     *   empty argument list is distinct from a macro with no argument
     *   list: in the former case, the empty parens are required, and are
     *   removed from the input stream and replaced with the macro's
     *   expansion.
     *   
     *   We'll make a copy of the argument list vector, strings, and
     *   expansion text, so the caller is free to forget all of that after
     *   creating the entry instance.  
     */
    CTcHashEntryPpDefine(const textchar_t *str, size_t len, int copy,
                         int has_args, int argc, int has_varargs,
                         const char **argv, const size_t *argvlen,
                         const char *expansion, size_t expan_len);

    ~CTcHashEntryPpDefine();

    /* 
     *   Get the expansion text and its length.  This is the parsed version
     *   of the expansion, with occurrences of the formal parameters replaced
     *   by TOK_MACRO_FORMAL_FLAG sequences, and the various '#' sequences
     *   replaced by their corresponding flags. 
     */
    const char *get_expansion() const { return expan_; }
    size_t get_expan_len() const { return expan_len_; }

    /* get the original expansion - the original text before parsing */
    const char *get_orig_expansion() const { return orig_expan_; }
    size_t get_orig_expan_len() const { return orig_expan_len_; }

private:
    /* parse the expansion */
    void parse_expansion(const size_t *argvlen);

    /* expansion (parsed version) */
    char *expan_;
    size_t expan_len_;

    /* original expansion (before parsing) */
    char *orig_expan_;
    size_t orig_expan_len_;
};


/*
 *   Hash table entry for __FILE__ and __LINE__
 */
class CTcHashEntryPpSpecial: public CTcHashEntryPp
{
public:
    CTcHashEntryPpSpecial(CTcTokenizer *tok, const char *str)
        : CTcHashEntryPp(str, strlen(str), FALSE)
    {
        /* remember my tokenizer */
        tok_ = tok;
    }

    /* these special macros are not undef'able */
    virtual int is_undefable() const { return FALSE; }

    /* special macros are pseudo-macros provided by the preprocessor */
    virtual int is_pseudo() const { return TRUE; }

protected:
    /* my tokenizer */
    CTcTokenizer *tok_;
};

class CTcHashEntryPpFILE: public CTcHashEntryPpSpecial
{
public:
    CTcHashEntryPpFILE(CTcTokenizer *tok)
        : CTcHashEntryPpSpecial(tok, "__FILE__") { }

    /* our expansion is the current filename, in single quotes */
    const char *get_expansion() const { return get_base_text(); }
    size_t get_expan_len() const { return strlen(get_base_text()); }

private:
    /* get our expansion base text */
    const char *get_base_text() const
    {
        /* 
         *   if we're in test-report mode, use the root name only;
         *   otherwise, use the full name with path 
         */
        if (tok_->get_last_desc() == 0)
            return "";
        else if (tok_->get_test_report_mode())
            return tok_->get_last_desc()->get_squoted_rootname();
        else
            return tok_->get_last_desc()->get_squoted_fname();
    }
};

class CTcHashEntryPpLINE: public CTcHashEntryPpSpecial
{
public:
    CTcHashEntryPpLINE(CTcTokenizer *tok)
        : CTcHashEntryPpSpecial(tok, "__LINE__") { }

    /* our expansion is the line number as a decimal string */
    const char *get_expansion() const
        { gen_expansion(tok_); return buf_; }
    size_t get_expan_len() const
        { gen_expansion(tok_); return strlen(buf_); }

private:
    /* generate the expansion text into our internal buffer */
    static void gen_expansion(CTcTokenizer *tok)
        { sprintf(buf_, "%ld", tok->get_last_linenum()); }

    /* internal buffer */
    static char buf_[20];
};


/*
 *   Hash entry for preprocessor arguments 
 */
class CTcHashEntryPpArg: public CVmHashEntryCS
{
public:
    CTcHashEntryPpArg(const char *str, size_t len, int copy, int argnum)
        : CVmHashEntryCS(str, len, copy)
    {
        /* remember the argument number */
        argnum_ = argnum;
    }

    /* get my argument number */
    int get_argnum() const { return argnum_; }

private:
    /* argument number */
    int argnum_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Previously-included file list entry.  Each time we include a file,
 *   we'll add an entry to a list of files; in the future, we'll consult
 *   this list to ensure that we don't include the same file again. 
 */
struct tctok_incfile_t
{
    /* next entry in the list of previously-included files */
    tctok_incfile_t *nxt;

    /* name of this file (we'll allocate memory to hold the name) */
    char fname[1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Include path list entry.  This structure defines one include path; we
 *   maintain a list of these structures.  
 */
struct tctok_incpath_t
{
    /* next entry in the list */
    tctok_incpath_t *nxt;

    /* path */
    char path[1];
};

#endif /* TCTOK_H */

