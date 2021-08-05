/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmconhmp.cpp - T3 VM Console - HTML mini-parser
Function
  This is the console HTML "mini-parser," which provides some minimal
  interpretation of HTML markups for text-only underlying output streams.

  Note that the mini-parser is included even in full HTML-enabled versions
  of TADS (such as HTML TADS or HyperTADS), because we still need text-only
  interpretation of HTML markups for log-file targets.
Notes
  
Modified
  06/08/02 MJRoberts  - Creation
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "wchar.h"

#include "os.h"
#include "t3std.h"
#include "utf8.h"
#include "vmuni.h"
#include "vmconsol.h"
#include "vmglob.h"
#include "charmap.h"
#include "vmhash.h"


/* ------------------------------------------------------------------------ */
/*
 *   Horizontal Tab object.  We use these objects to record tab stops
 *   defined with <TAB ID=xxx> in our HTML mini-parser.
 *   
 *   This is a hash table entry subclass because we use a hash table of tab
 *   entries, keyed by ID strings.  Note that we use a case-sensitive hash
 *   table entry - clients must convert ID's to lower-case if they don't
 *   want case sensitivity.
 *   
 *   Note that this code looks a bit cavalier with unicode vs ASCII, but
 *   this is actually safe.  The underlying hash table code uses
 *   counted-length strings, so it doesn't care about embedded null bytes.
 *   We therefore can pass a wchar_t buffer to the underlying hash table,
 *   even though the underlying hash table uses char's, because we multiply
 *   our length by the size of a wchar_t to get the byte length.  
 */
class CVmFmtTabStop: public CVmHashEntryCS
{
public:
    CVmFmtTabStop(const char *id, size_t idlen)
        : CVmHashEntryCS(id, idlen, TRUE)
    {
        /* we don't know our column yet */
        column_ = 0;
    }

    /* the column where the tab is defined */
    int column_;

    /*
     *   Find a tabstop object with a given ID, optionally creating a new
     *   object if an existing one isn't found.  
     */
    static CVmFmtTabStop *find(CVmHashTable *hashtab, wchar_t *id, int create)
    {
        /* 
         *   Get the length - in characters AND in bytes - of the new ID.
         *   We mostly work with the new ID in bytes rather than wchar_t's,
         *   because the underlying hash table does everything in bytes.  
         */
        
        /* 
         *   Convert the new ID to folded case.  The hash table doesn't deal
         *   with unicode case conversions, so we use a case-sensitive hash
         *   table and simply make sure we only give it lower-case
         *   characters.  
         */
        size_t flen = fold_case(0, id);
        wchar_t *fid = 0;
        if (flen != 0)
            fid = new wchar_t[flen+1];

        CVmFmtTabStop *entry = 0;
        err_try
        {
            /* do the folding if applicable */
            if (fid != 0)
            {
                fold_case(fid, id);
                id = fid;
            }

            /* get the folded ID length in characters and bytes */
            size_t id_chars = wcslen(id);
            size_t id_bytes = id_chars * sizeof(wchar_t);
            
            /* look for an existing tab-stop entry with the same ID */
            entry = (CVmFmtTabStop *)hashtab->find((char *)id, id_bytes);
            
            /* if we didn't find it, and they want to create it, do so */
            if (entry == 0 && create)
                hashtab->add(entry = new CVmFmtTabStop((char *)id, id_bytes));
        }
        err_finally
        {
            /* if we allocated the ID string, release it */
            if (fid != 0)
                delete [] fid;
        }
        err_end;

        /* return what we found */
        return entry;
    }

    /* get the case folding of a string */
    static size_t fold_case(wchar_t *dst, const wchar_t *str)
    {
        size_t dstlen = 0;
        for ( ; *str != 0 ; ++str)
        {
            /* 
             *   get the case folding of the current character, defaulting to
             *   an identity mapping if there's no folding defined 
             */
            const wchar_t *f = t3_to_fold(*str);
            wchar_t fi[2] = { *str, 0 };
            if (f == 0)
                f = fi;

            /* copy it to the output if there's an output buffer */
            for ( ; *f != 0 ; ++f, ++dstlen)
            {
                if (dst != 0)
                    *dst++ = *f;
            }
        }

        /* add the null */
        if (dst != 0)
            *dst = 0;
        
        /* return the length (excluding the null) */
        return dstlen;
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   HTML entity mapping table.  When we're in non-HTML mode, we keep our own
 *   expansion table so that we can map HTML entity names into the local
 *   character set.
 *   
 *   The entries in this table must be in sorted order (by HTML entity name),
 *   because we use a binary search to find an entity name in the table.  
 */

/*
 *   '&' character name definition structure 
 */
struct amp_tbl_t
{
    /* entity name */
    const char *cname;

    /* HTML Unicode character value */
    wchar_t html_cval;
};

/* 
 *   table of '&' character name sequences 
 */
static const struct amp_tbl_t amp_tbl[] =
{
    { "AElig", 198 },
    { "Aacute", 193 },
    { "Abreve", 258 },
    { "Acirc", 194 },
    { "Agrave", 192 },
    { "Alpha", 913 },
    { "Aogon", 260 },
    { "Aring", 197 },
    { "Atilde", 195 },
    { "Auml", 196 },
    { "Beta", 914 },
    { "Cacute", 262 },
    { "Ccaron", 268 },
    { "Ccedil", 199 },
    { "Chi", 935 },
    { "Dagger", 8225 },
    { "Dcaron", 270 },
    { "Delta", 916 },
    { "Dstrok", 272 },
    { "ETH", 208 },
    { "Eacute", 201 },
    { "Ecaron", 282 },
    { "Ecirc", 202 },
    { "Egrave", 200 },
    { "Eogon", 280 },
    { "Epsilon", 917 },
    { "Eta", 919 },
    { "Euml", 203 },
    { "Gamma", 915 },
    { "Iacute", 205 },
    { "Icirc", 206 },
    { "Igrave", 204 },
    { "Iota", 921 },
    { "Iuml", 207 },
    { "Kappa", 922 },
    { "Lacute", 313 },
    { "Lambda", 923 },
    { "Lcaron", 317 },
    { "Lstrok", 321 },
    { "Mu", 924 },
    { "Nacute", 323 },
    { "Ncaron", 327 },
    { "Ntilde", 209 },
    { "Nu", 925 },
    { "OElig", 338 },
    { "Oacute", 211 },
    { "Ocirc", 212 },
    { "Odblac", 336 },
    { "Ograve", 210 },
    { "Omega", 937 },
    { "Omicron", 927 },
    { "Oslash", 216 },
    { "Otilde", 213 },
    { "Ouml", 214 },
    { "Phi", 934 },
    { "Pi", 928 },
    { "Prime", 8243 },
    { "Psi", 936 },
    { "Racute", 340 },
    { "Rcaron", 344 },
    { "Rho", 929 },
    { "Sacute", 346 },
    { "Scaron", 352 },
    { "Scedil", 350 },
    { "Sigma", 931 },
    { "THORN", 222 },
    { "Tau", 932 },
    { "Tcaron", 356 },
    { "Tcedil", 354 },
    { "Theta", 920 },
    { "Uacute", 218 },
    { "Ucirc", 219 },
    { "Udblac", 368 },
    { "Ugrave", 217 },
    { "Upsilon", 933 },
    { "Uring", 366 },
    { "Uuml", 220 },
    { "Xi", 926 },
    { "Yacute", 221 },
    { "Yuml", 376 },
    { "Zacute", 377 },
    { "Zcaron", 381 },
    { "Zdot", 379 },
    { "Zeta", 918 },
    { "aacute", 225 },
    { "abreve", 259 },
    { "acirc", 226 },
    { "acute", 180 },
    { "aelig", 230 },
    { "agrave", 224 },
    { "alefsym", 8501 },
    { "alpha", 945 },
    { "amp", '&' },
    { "and", 8743 },
    { "ang", 8736 },
    { "aogon", 261 },
    { "aring", 229 },
    { "asymp", 8776 },
    { "atilde", 227 },
    { "auml", 228 },
    { "bdquo", 8222 },
    { "beta", 946 },
    { "breve", 728 },
    { "brvbar", 166 },
    { "bull", 8226 },
    { "cacute", 263 },
    { "cap", 8745 },
    { "caron", 711 },
    { "ccaron", 269 },
    { "ccedil", 231 },
    { "cedil", 184 },
    { "cent", 162 },
    { "chi", 967 },
    { "circ", 710 },
    { "clubs", 9827 },
    { "cong", 8773 },
    { "copy", 169 },
    { "crarr", 8629 },
    { "cup", 8746 },
    { "curren", 164 },
    { "dArr", 8659 },
    { "dagger", 8224 },
    { "darr", 8595 },
    { "dblac", 733 },
    { "dcaron", 271 },
    { "deg", 176 },
    { "delta", 948 },
    { "diams", 9830 },
    { "divide", 247 },
    { "dot", 729 },
    { "dstrok", 273 },
    { "eacute", 233 },
    { "ecaron", 283 },
    { "ecirc", 234 },
    { "egrave", 232 },
    { "emdash", 8212 },
    { "empty", 8709 },
    { "emsp", 0x2003 },
    { "endash", 8211 },
    { "ensp", 0x2002 },
    { "eogon", 281 },
    { "epsilon", 949 },
    { "equiv", 8801 },
    { "eta", 951 },
    { "eth", 240 },
    { "euml", 235 },
    { "exist", 8707 },
    { "figsp", 0x2007 },
    { "fnof", 402 },
    { "forall", 8704 },
    { "fpmsp", 0x2005 },
    { "frac12", 189 },
    { "frac14", 188 },
    { "frac34", 190 },
    { "frasl", 8260 },
    { "gamma", 947 },
    { "ge", 8805 },
    { "gt", '>' },
    { "hArr", 8660 },
    { "hairsp", 0x200a },
    { "harr", 8596 },
    { "hearts", 9829 },
    { "hellip", 8230 },
    { "iacute", 237 },
    { "icirc", 238 },
    { "iexcl", 161 },
    { "igrave", 236 },
    { "image", 8465 },
    { "infin", 8734 },
    { "int", 8747 },
    { "iota", 953 },
    { "iquest", 191 },
    { "isin", 8712 },
    { "iuml", 239 },
    { "kappa", 954 },
    { "lArr", 8656 },
    { "lacute", 314 },
    { "lambda", 955 },
    { "lang", 9001 },
    { "laquo", 171 },
    { "larr", 8592 },
    { "lcaron", 318 },
    { "lceil", 8968 },
    { "ldq", 8220 },
    { "ldquo", 8220 },
    { "le", 8804 },
    { "lfloor", 8970 },
    { "lowast", 8727 },
    { "loz", 9674 },
    { "lsaquo", 8249 },
    { "lsq", 8216 },
    { "lsquo", 8216 },
    { "lstrok", 322 },
    { "lt", '<' },
    { "macr", 175 },
    { "mdash", 8212 },
    { "micro", 181 },
    { "middot", 183 },
    { "minus", 8722 },
    { "mu", 956 },
    { "nabla", 8711 },
    { "nacute", 324 },
    { "nbsp", 0x00A0 },                                   /* == 160 decimal */
    { "ncaron", 328 },
    { "ndash", 8211 },
    { "ne", 8800 },
    { "ni", 8715 },
    { "not", 172 },
    { "notin", 8713 },
    { "nsub", 8836 },
    { "ntilde", 241 },
    { "nu", 957 },
    { "oacute", 243 },
    { "ocirc", 244 },
    { "odblac", 337 },
    { "oelig", 339 },
    { "ogon", 731 },
    { "ograve", 242 },
    { "oline", 8254 },
    { "omega", 969 },
    { "omicron", 959 },
    { "oplus", 8853 },
    { "or", 8744 },
    { "ordf", 170 },
    { "ordm", 186 },
    { "oslash", 248 },
    { "otilde", 245 },
    { "otimes", 8855 },
    { "ouml", 246 },
    { "para", 182 },
    { "part", 8706 },
    { "permil", 8240 },
    { "perp", 8869 },
    { "phi", 966 },
    { "pi", 960 },
    { "piv", 982 },
    { "plusmn", 177 },
    { "pound", 163 },
    { "prime", 8242 },
    { "prod", 8719 },
    { "prop", 8733 },
    { "psi", 968 },
    { "puncsp", 0x2008 },
    { "quot", '"' },
    { "rArr", 8658 },
    { "racute", 341 },
    { "radic", 8730 },
    { "rang", 9002 },
    { "raquo", 187 },
    { "rarr", 8594 },
    { "rcaron", 345 },
    { "rceil", 8969 },
    { "rdq", 8221 },
    { "rdquo", 8221 },
    { "real", 8476 },
    { "reg", 174 },
    { "rfloor", 8971 },
    { "rho", 961 },
    { "rsaquo", 8250 },
    { "rsq", 8217 },
    { "rsquo", 8217 },
    { "sacute", 347 },
    { "sbquo", 8218 },
    { "scaron", 353 },
    { "scedil", 351 },
    { "sdot", 8901 },
    { "sect", 167 },
    { "shy", 173 },
    { "sigma", 963 },
    { "sigmaf", 962 },
    { "sim", 8764 },
    { "spades", 9824 },
    { "spmsp", 0x2006 },
    { "sub", 8834 },
    { "sube", 8838 },
    { "sum", 8721 },
    { "sup", 8835 },
    { "sup1", 185 },
    { "sup2", 178 },
    { "sup3", 179 },
    { "supe", 8839 },
    { "szlig", 223 },
    { "tau", 964 },
    { "tcaron", 357 },
    { "tcedil", 355 },
    { "there4", 8756 },
    { "theta", 952 },
    { "thetasym", 977 },
    { "thinsp", 0x2009 },
    { "thorn", 254 },
    { "tilde", 732 },
    { "times", 215 },
    { "tpmsp", 0x2004 },
    { "trade", 8482 },
    { "uArr", 8657 },
    { "uacute", 250 },
    { "uarr", 8593 },
    { "ucirc", 251 },
    { "udblac", 369 },
    { "ugrave", 249 },
    { "uml", 168 },
    { "upsih", 978 },
    { "upsilon", 965 },
    { "uring", 367 },
    { "uuml", 252 },
    { "weierp", 8472 },
    { "xi", 958 },
    { "yacute", 253 },
    { "yen", 165 },
    { "yuml", 255 },
    { "zacute", 378 },
    { "zcaron", 382 },
    { "zdot", 380 },
    { "zeta", 950 },
    { "zwnbsp", 0xfeff },
    { "zwsp", 0x200b }
};

/*
 *   Color names.  For text-mode interpreters, we parse certain tag
 *   attributes that can specify colors, so we must recognize the basic set
 *   of HTML color names for these attributes.  
 */
struct color_tbl_t
{
    /* name of the color */
    const wchar_t *name;

    /* osifc color code */
    os_color_t color_code;
};

static const color_tbl_t color_tbl[] =
{
    { L"black",   os_rgb_color(0x00, 0x00, 0x00) },
    { L"white",   os_rgb_color(0xFF, 0xFF, 0xFF) },
    { L"red",     os_rgb_color(0xFF, 0x00, 0x00) },
    { L"blue",    os_rgb_color(0x00, 0x00, 0xFF) },
    { L"green",   os_rgb_color(0x00, 0x80, 0x00) },
    { L"yellow",  os_rgb_color(0xFF, 0xFF, 0x00) },
    { L"cyan",    os_rgb_color(0x00, 0xFF, 0xFF) },
    { L"aqua",    os_rgb_color(0x00, 0xFF, 0xFF) },
    { L"magenta", os_rgb_color(0xFF, 0x00, 0xFF) },
    { L"silver",  os_rgb_color(0xC0, 0xC0, 0xC0) },
    { L"gray",    os_rgb_color(0x80, 0x80, 0x80) },
    { L"maroon",  os_rgb_color(0x80, 0x00, 0x00) },
    { L"purple",  os_rgb_color(0x80, 0x00, 0x80) },
    { L"fuchsia", os_rgb_color(0xFF, 0x00, 0xFF) },
    { L"lime",    os_rgb_color(0x00, 0xFF, 0x00) },
    { L"olive",   os_rgb_color(0x80, 0x80, 0x00) },
    { L"navy",    os_rgb_color(0x00, 0x00, 0x80) },
    { L"teal",    os_rgb_color(0x00, 0x80, 0x80) },

    /* the parameterized colors */
    { L"text",       OS_COLOR_P_TEXT },
    { L"bgcolor",    OS_COLOR_P_TEXTBG },
    { L"statustext", OS_COLOR_P_STATUSLINE },
    { L"statusbg",   OS_COLOR_P_STATUSBG },
    { L"input",      OS_COLOR_P_INPUT },

    /* end-of-table marker */
    { 0,          0 }
};



/* ------------------------------------------------------------------------ */
/*
 *   Parse the COLOR attribute of a FONT tag.  Sets *result to the parsed
 *   color value.  Returns true if we found a valid value, false if not.  
 */
int CVmFormatter::parse_color_attr(VMG_ const wchar_t *val,
                                   os_color_t *result)
{
    const color_tbl_t *p;

    /* 
     *   if the value starts with '#', it's a hex RGB value; otherwise, it's
     *   a color name 
     */
    if (*val == '#')
    {
        unsigned long hexval;

        /* parse the value as a hex number */
        for (hexval = 0, ++val ; *val != '\0' && is_xdigit(*val) ; ++val)
        {
            /* add this digit */
            hexval <<= 4;
            hexval += (t3_is_digit(*val)
                       ? *val - '0'
                       : *val >= 'a' && *val <= 'f'
                       ? *val - 'a' + 0x0A
                       : *val - 'A' + 0x0A);
        }

        /* 
         *   os_color_t uses the same encoding as HTML, so just return the
         *   value as given (ensuring that it doesn't overflow the 24 bits
         *   assigned to our RGB encoding) 
         */
        *result = (os_color_t)(hexval & 0x00FFFFFF);
        return TRUE;
    }
    else
    {
        /* scan the color table for a match for the name */
        for (p = color_tbl ; p->name != 0 ; ++p)
        {
            /* if the name matches, use it */
            if (CVmCaseFoldStr::wstreq(val, p->name))
            {
                /* this is the color - make it the current buffer color */
                *result = p->color_code;

                /* indicate that we found a valid color value */
                return TRUE;
            }
        }
    }

    /* indicate that we didn't find a valid color value */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a tab 
 */
CVmFmtTabStop *CVmFormatter::find_tab(wchar_t *id, int create)
{
    /* if we don't have a hash table yet, create one */
    if (tabs_ == 0)
        tabs_ = new CVmHashTable(64, new CVmHashFuncCS(), TRUE);

    /* find the tab */
    return CVmFmtTabStop::find(tabs_, id, create);
}

/* ------------------------------------------------------------------------ */
/*
 *   Expand the pending tab. 
 */
void CVmFormatter::expand_pending_tab(VMG_ int allow_anon)
{
    int txtlen;
    int spaces;
    int i;
    int col;
    
    /* check the alignment */
    switch(pending_tab_align_)
    {
    case VMFMT_TAB_LEFT:
        /* 
         *   left-aligned tabs are never pending, so this should never
         *   happen 
         */
        assert(FALSE);
        return;

    case VMFMT_TAB_NONE:
        /* there's no pending tab, so there's nothing to do */
        return;

    case VMFMT_TAB_RIGHT:
        /*
         *   It's a right-aligned tab.  If we have a TO, expand the tab with
         *   enough spaces to push the text up to the defined tab's column;
         *   otherwise, insert spaces to push the text to the full line
         *   width.  
         */
        if (pending_tab_entry_ != 0)
        {
            /* 
             *   if we're not past the tab, insert enough spaces to get us
             *   there; if we're already past the tab, insert nothing 
             */
            if (pending_tab_entry_->column_ > linecol_)
                spaces = pending_tab_entry_->column_ - linecol_;
            else
                spaces = 0;
        }
        else if (allow_anon)
        {
            /* push the text to the full line width */
            spaces = console_->get_line_width() - 1 - linecol_;
        }
        else
        {
            /* it's anonymous, but these aren't allowed - ignore it */
            spaces = 0;
        }
        break;

    case VMFMT_TAB_CENTER:
        /*
         *   It's a center-aligned tab.  If we have a TO, expand the tab
         *   with enough spaces to center the text on the defined tab's
         *   column; otherwise, center the text between the tab's starting
         *   column and the full line width. 
         */
        txtlen = linecol_ - pending_tab_start_;
        if (pending_tab_entry_ != 0)
        {
            int startcol;
            
            /* 
             *   find the column where the text would have to start for the
             *   text to be centered over the tab's column 
             */
            startcol = pending_tab_entry_->column_ - (txtlen / 2);

            /* 
             *   if that's past the starting column, insert enough spaces to
             *   get us there; otherwise, do nothing, as we'd have to start
             *   to the left of the leftmost place we can put the text 
             */
            if (startcol > pending_tab_start_)
                spaces = startcol - pending_tab_start_;
            else
                spaces = 0;
        }
        else if (allow_anon)
        {
            /* center the text in the remaining space to the line width */
            spaces = ((console_->get_line_width() - pending_tab_start_)
                      - txtlen) / 2;
        }
        else
        {
            /* it's anonymous, but these aren't allowed - ignore it */
            spaces = 0;
        }
        break;

    case VMFMT_TAB_DECIMAL:
        /*
         *   Decimal tab.  Scan the text after the tab, looking for the
         *   first occurrence of the decimal point character. 
         */
        for (col = pending_tab_start_, i = linepos_ - (linecol_ - col) ;
             i < linepos_ ; ++i, ++col)
        {
            /* if this is the decimal point character, stop looking */
            if (linebuf_[i] == pending_tab_dp_)
                break;
        }

        /* 
         *   insert enough spaces to put the decimal point character in the
         *   defined tab's column; if the decimal point character is already
         *   past the tab's column, do nothing 
         */
        if (pending_tab_entry_->column_ > col)
            spaces = pending_tab_entry_->column_ - col;
        else
            spaces = 0;
        break;
    }

    /* insert the spaces into the buffer where the <TAB> appeared */
    if (spaces > 0)
    {
        /* 
         *   Calculate the index in the buffer of the insertion point.  To
         *   do this, start with linepos_, which the current output index in
         *   the buffer, and subtract the difference between linecol_ and
         *   the tab starting column, since this will give us the number of
         *   characters in the buffer since the tab insertion point. 
         */
        i = linepos_ - (linecol_ - pending_tab_start_);

        /* 
         *   open up the line buffer and line color buffer by the number of
         *   spaces we're adding 
         */
        memmove(linebuf_ + i + spaces, linebuf_ + i,
                (linepos_ - i + 1)*sizeof(linebuf_[0]));
        memmove(colorbuf_ + i + spaces, colorbuf_ + i,
                (linepos_ - i + 1)*sizeof(colorbuf_[0]));

        /* advance the line and column positions for the insertion */
        linecol_ += spaces;
        linepos_ += spaces;

        /* insert the spaces */
        for ( ; spaces != 0 ; --spaces, ++i)
        {
            /* insert a space of the same color active at the <TAB> */
            linebuf_[i] = ' ';
            colorbuf_[i] = pending_tab_color_;
        }
    }

    /* we've handled the pending tab, so we can forget about it */
    pending_tab_align_ = VMFMT_TAB_NONE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Resume text-only HTML mini-parser.  This is called when we start writing
 *   a new string and discover that we're parsing inside an HTML tag in our
 *   mini-parser.
 *   
 *   Returns the next character after the run of text we parse.  
 */
wchar_t CVmFormatter::resume_html_parsing(VMG_ wchar_t c,
                                          const char **sp, size_t *slenp)
{
    /* keep going until we run out of characters */
    while (c != L'\0')
    {
        /*
         *   If we're parsing HTML here, and we're inside a tag, skip
         *   characters until we reach the end of the tag.  
         */
        if (html_parse_state_ != VMCON_HPS_NORMAL)
        {
            wchar_t *dst;

            /* check our parsing mode */
            switch(html_parse_state_)
            {
            case VMCON_HPS_TAG:
                /* 
                 *   keep skipping up to the closing '>', but note when we
                 *   enter any quoted section 
                 */
                switch(c)
                {
                case '>':
                    /* we've reached the end of the tag */
                    html_parse_state_ = VMCON_HPS_NORMAL;

                    /* if we have a deferred <BR>, process it now */
                    switch(html_defer_br_)
                    {
                    case HTML_DEFER_BR_NONE:
                        /* no deferred <BR> */
                        break;

                    case HTML_DEFER_BR_FLUSH:
                        flush(vmg_ VM_NL_NEWLINE);
                        break;

                    case HTML_DEFER_BR_BLANK:
                        write_blank_line(vmg0_);
                        break;
                    }

                    /* no more deferred <BR> pending */
                    html_defer_br_ = HTML_DEFER_BR_NONE;

                    /* if we're parsing a <TAB> tag, finish it */
                    if (html_in_tab_)
                    {
                        /* check the attributes of the <TAB> */
                        if (new_tab_entry_ != 0
                            || new_tab_align_ != VMFMT_TAB_NONE)
                        {
                            /*   
                             *   We have either a <TAB TO>, maybe with an
                             *   ALIGN attribute, or at least a <TAB ALIGN>.
                             *   
                             *   If there's no TO, then we use the default
                             *   treatment based on the alignment: for
                             *   ALIGN=RIGHT, the text up to the end of the
                             *   line is aligned at the right edge of the
                             *   display; for ALIGN=CENTER, it's centered
                             *   within the margins.  For other alignments,
                             *   a TAB without a TO attribute has no effect.
                             *   
                             *   Check for ALIGN without TO, and ignore it
                             *   if it's not CENTER or RIGHT alignment.  
                             */
                            if (new_tab_entry_ == 0
                                && new_tab_align_ != VMFMT_TAB_RIGHT
                                && new_tab_align_ != VMFMT_TAB_CENTER)
                            {
                                /*
                                 *   meaningless alignment for <TAB> without
                                 *   a TO attribute - ignore the tab 
                                 */
                            }
                            else if (new_tab_align_ == VMFMT_TAB_LEFT
                                     || new_tab_align_ == VMFMT_TAB_NONE)
                            {
                                int delta;
                                
                                /*
                                 *   This is a left-aligned tab (explicitly
                                 *   or implicitly: if no alignment is
                                 *   specified, default to left alignment).
                                 *   This is easy: just insert spaces to the
                                 *   desired column.  
                                 */
                                delta = new_tab_entry_->column_ - linecol_;
                                if (delta > 0)
                                    write_tab(vmg_ delta, 0);
                            }
                            else
                            {
                                /*
                                 *   For other alignments, we won't know how
                                 *   much space to insert until we find the
                                 *   the next <TAB> or the end of the line.
                                 *   
                                 *   Remember the starting column of the
                                 *   pending tab.  When we reach another
                                 *   <TAB>, or we reach the end of the line,
                                 *   we'll insert spaces at this point as
                                 *   needed.  Remember the current color, so
                                 *   we can use the color active at the
                                 *   <TAB> tag when we insert the spaces
                                 *   expanding the tab.
                                 *   
                                 *   Also remember the characteristics
                                 *   specified in the <TAB> tag (alignment,
                                 *   decimal-point alignment character), so
                                 *   we'll be able to apply the desired
                                 *   characteristics when we expand the tab.
                                 */
                                pending_tab_start_ = linecol_;
                                pending_tab_color_ = cur_color_;
                                pending_tab_entry_ = new_tab_entry_;
                                pending_tab_align_ = new_tab_align_;
                                pending_tab_dp_ = new_tab_dp_;

                                /* default to '.' as the DP aligner */
                                if (pending_tab_align_ == VMFMT_TAB_DECIMAL
                                    && pending_tab_dp_ == L'\0')
                                    pending_tab_dp_ = L'.';
                            }
                        }
                        else
                        {
                            /* 
                             *   it's just a <TAB> with no attributes -
                             *   default to <TAB MULTIPLE=4>, so that it
                             *   behaves like a simple '\t' 
                             */
                            write_tab(vmg_ 0, 4);
                        }
                    }

                    /* we're not parsing inside a tag any more */
                    html_allow_alt_ = FALSE;
                    html_allow_color_ = FALSE;
                    html_in_body_ = FALSE;
                    html_in_tab_ = FALSE;
                    html_in_wrap_ = FALSE;

                    /* done */
                    break;

                case '"':
                    /* enter a double-quoted string */
                    html_parse_state_ = VMCON_HPS_DQUOTE;
                    break;

                case '\'':
                    /* enter a single-quoted string */
                    html_parse_state_ = VMCON_HPS_SQUOTE;
                    break;

                default:
                    /* if it's alphabetic, start an attribute name */
                    if (t3_is_alpha(c))
                    {
                        /* note that we're parsing an attribute name */
                        html_parse_state_ = VMCON_HPS_ATTR_NAME;

                        /* empty the buffer */
                        attrname_[0] = L'\0';

                        /* go back and re-read it in the new mode */
                        continue;
                    }
                    break;
                }
                break;

            case VMCON_HPS_DQUOTE:
                /* if we've reached the closing quote, return to tag state */
                if (c == '"')
                    html_parse_state_ = VMCON_HPS_TAG;
                break;

            case VMCON_HPS_SQUOTE:
                /* if we've reached the closing quote, return to tag state */
                if (c == '\'')
                    html_parse_state_ = VMCON_HPS_TAG;
                break;

            case VMCON_HPS_ATTR_NAME:
                /* 
                 *   parsing an attribute name; resume at the last character
                 *   of the buffer so far 
                 */
                dst = attrname_ + wcslen(attrname_);
                
                /* add characters to the attribute name */
                while (t3_is_alpha(c) || t3_is_digit(c))
                {
                    /* store this character if there's room */
                    if (dst + 1 < attrname_ + CVFMT_MAX_ATTR_NAME)
                        *dst++ = c;
                    
                    /* get the next character */
                    c = next_wchar(sp, slenp);
                }
                
                /* null-terminate the result so far */
                *dst++ = '\0';

                /* 
                 *   if we've reached the end of the string, stop here,
                 *   staying in the same mode 
                 */
                if (c == '\0')
                    break;
                
                /* skip any whitespace */
                while (t3_is_whitespace(c))
                    c = next_wchar(sp, slenp);
                
                /* if we found an '=', switch to parsing the value */
                if (c == '=')
                {
                    /* switch to value mode */
                    html_parse_state_ = VMCON_HPS_ATTR_VAL;
                    
                    /* empty the value buffer */
                    attrval_[0] = L'\0';
                    attr_qu_ = L'\0';

                    /* proceed to the next character */
                    break;
                }
                else
                {
                    /* 
                     *   There's no value explicitly given, so the value is
                     *   implicitly the same as the attribute name. 
                     */
                    wcscpy(attrval_, attrname_);
                    
                    /* this takes us back to tag mode */
                    html_parse_state_ = VMCON_HPS_TAG;

                    /* go process the implicit attribute value */
                    goto process_attr_val;
                }

            case VMCON_HPS_ATTR_VAL:
                /* reading attribute value - pick up where we left off */
                dst = attrval_ + wcslen(attrval_);

                /* if we haven't started the name yet, skip whitespace */
                if (attr_qu_ == 0 && attrval_[0] == L'\0')
                {
                    while (t3_is_whitespace(c))
                        c = next_wchar(sp, slenp);
                }

                /* if we're out of characters, we're done */
                if (c == 0)
                    break;

                /* if this is the first character, check for a quote */
                if (attrval_[0] == L'\0' && (c == L'"' || c == L'\''))
                {
                    /* note the quote */
                    attr_qu_ = c;

                    /* skip it */
                    c = next_wchar(sp, slenp);
                    if (c == 0)
                        break;
                }

                /* keep going until we reach the end of the attribute */
                while (c != 0)
                {
                    /* if this is our quote character, we're done */
                    if (attr_qu_ != 0 && c == attr_qu_)
                    {
                        /* skip the quote */
                        c = next_wchar(sp, slenp);

                        /* return to tag mode */
                        html_parse_state_ = VMCON_HPS_TAG;

                        /* stop scanning */
                        break;
                    }

                    /* 
                     *   if it's a space or a '>', and the attribute isn't
                     *   quoted, this marks the end of the attribute 
                     */
                    if (attr_qu_ == 0 && (t3_is_whitespace(c) || c == '>'))
                    {
                        /* return to tag mode */
                        html_parse_state_ = VMCON_HPS_TAG;

                        /* stop scanning */
                        break;
                    }

                    /* store the character if we have room  */
                    if (dst + 1 < attrval_ + CVFMT_MAX_ATTR_VAL)
                        *dst++ = c;

                    /* read the next character */
                    c = next_wchar(sp, slenp);
                }

                /* null-terminate the destination */
                *dst = L'\0';

                /* 
                 *   if we're still in value-parsing mode, it means that we
                 *   reached the end of the string without reaching a value
                 *   delimiter - simply stop scanning and wait for more 
                 */
                if (html_parse_state_ == VMCON_HPS_ATTR_VAL)
                    break;

            process_attr_val:
                /* 
                 *   We have our tag and value, so see if we recognize it,
                 *   and it's meaningful in the context of the current tag.  
                 */
                if (html_defer_br_ != HTML_DEFER_BR_NONE
                    && CVmCaseFoldStr::wstreq(attrname_, L"height"))
                {
                    int ht;
                    
                    /*
                     *   If the height is zero, always treat this as a
                     *   non-blanking flush.  If it's one, treat it as we
                     *   originally planned to.  If it's greater than one,
                     *   add n blank lines.  
                     */
                    ht = wtoi(attrval_);
                    if (ht == 0)
                    {
                        /* always use non-blanking flush */
                        html_defer_br_ = HTML_DEFER_BR_FLUSH;
                    }
                    else if (ht == 1)
                    {
                        /* keep original setting */
                    }
                    else
                    {
                        /* 
                         *   write out the desired number of blank lines 
                         */
                        for ( ; ht > 0 ; --ht)
                            write_blank_line(vmg0_);
                    }
                }
                else if (html_allow_alt_
                         && !html_in_ignore_
                         && CVmCaseFoldStr::wstreq(attrname_, L"alt"))
                {
                    /* write out the ALT string */
                    buffer_wstring(vmg_ attrval_);
                }
                else if (html_allow_color_
                         && CVmCaseFoldStr::wstreq(attrname_, L"color"))
                {
                    os_color_t new_color;
                    
                    /* parse the color, and use it as the new foreground */
                    if (parse_color_attr(vmg_ attrval_, &new_color))
                        cur_color_.fg = new_color;
                }
                else if (html_allow_color_
                         && CVmCaseFoldStr::wstreq(attrname_, L"bgcolor"))
                {
                    os_color_t new_color;

                    /* parse the color, and use it as the new foreground */
                    if (parse_color_attr(vmg_ attrval_, &new_color))
                        cur_color_.bg = new_color;
                }
                else if (html_in_body_
                         && CVmCaseFoldStr::wstreq(attrname_, L"text"))
                {
                    os_color_t new_color;

                    /* parse the color, and use it as the new foreground */
                    if (parse_color_attr(vmg_ attrval_, &new_color))
                        cur_color_.fg = new_color;
                }
                else if (html_in_body_
                         && CVmCaseFoldStr::wstreq(attrname_, L"bgcolor"))
                {
                    os_color_t new_color;

                    /* parse the color, and use it as the new background */
                    if (parse_color_attr(vmg_ attrval_, &new_color))
                        set_os_body_color(new_color);
                }
                else if (html_in_tab_)
                {
                    /* check the attribute name */
                    if (CVmCaseFoldStr::wstreq(attrname_, L"id"))
                    {
                        /*
                         *   We're defining a new tab at the current output
                         *   position.  Find or create a tab object with the
                         *   given ID, and set its position to the current
                         *   column position.  (If we already have an entry
                         *   with the same name, the new definition replaces
                         *   the old definition, so just change the existing
                         *   entry.)  
                         */
                        CVmFmtTabStop *entry = find_tab(attrval_, TRUE);

                        /* define the entry in the current column */
                        entry->column_ = linecol_;
                        
                        /* this finishes the <TAB> */
                        html_in_tab_ = FALSE;
                    }
                    else if (CVmCaseFoldStr::wstreq(attrname_, L"to"))
                    {
                        /* 
                         *   find the tab object with the given ID, and
                         *   remember it; when we're done parsing the tag,
                         *   we'll look at this to generate our tab output 
                         */
                        new_tab_entry_ = find_tab(attrval_, FALSE);

                        /* if we didn't find it, ignore this <TAB> */
                        if (new_tab_entry_ == 0)
                            html_in_tab_ = FALSE;
                    }
                    else if (CVmCaseFoldStr::wstreq(attrname_, L"indent"))
                    {
                        /* 
                         *   it's a simple <TAB INDENT=n> - this simply
                         *   inserts the given number of spaces 
                         */
                        write_tab(vmg_ wtoi(attrval_), 0);

                        /* this finishes the <TAB> */
                        html_in_tab_ = FALSE;
                    }
                    else if (CVmCaseFoldStr::wstreq(attrname_, L"multiple"))
                    {
                        /* 
                         *   it's a simple <TAB MULTIPLE=n> - this simply
                         *   inserts spaces to the given column multiple 
                         */
                        write_tab(vmg_ wtoi(attrval_), 0);

                        /* this finishes the <TAB> */
                        html_in_tab_ = FALSE;
                    }
                    else if (CVmCaseFoldStr::wstreq(attrname_, L"align"))
                    {
                        /* note the alignment */
                        if (CVmCaseFoldStr::wstreq(attrval_, L"left"))
                            new_tab_align_ = VMFMT_TAB_LEFT;
                        else if (CVmCaseFoldStr::wstreq(attrval_, L"right"))
                            new_tab_align_ = VMFMT_TAB_RIGHT;
                        else if (CVmCaseFoldStr::wstreq(attrval_, L"center"))
                            new_tab_align_ = VMFMT_TAB_CENTER;
                        else if (CVmCaseFoldStr::wstreq(attrval_, L"decimal"))
                            new_tab_align_ = VMFMT_TAB_DECIMAL;
                        else
                        {
                            /* invalid - ignore the <TAB> */
                            html_in_tab_ = FALSE;
                        }
                    }
                    else if (CVmCaseFoldStr::wstreq(attrname_, L"dp"))
                    {
                        /* note the decimal point character */
                        new_tab_dp_ = attrval_[0];
                    }
                }
                else if (html_in_wrap_)
                {
                    /* it's a WRAP tag - check the attribute */
                    if (CVmCaseFoldStr::wstreq(attrname_, L"word"))
                    {
                        /* word wrap mode - clear the 'any' wrap flag */
                        cur_flags_ &= ~VMCON_OBF_BREAK_ANY;
                    }
                    else if (CVmCaseFoldStr::wstreq(attrname_, L"char"))
                    {
                        /* character wrap mode - set the 'any' flag */
                        cur_flags_ |= VMCON_OBF_BREAK_ANY;
                    }
                }
                
                /* 
                 *   since we already read the next character, simply loop
                 *   back immediately 
                 */
                continue;

            default:
                /* ignore other states */
                break;
            }

            /* 
             *   move on to the next character, and start over with the
             *   new character 
             */
            c = next_wchar(sp, slenp);
            continue;
        }

        /*
         *   If we're in a title, and this isn't the start of a new tag,
         *   skip the character - we suppress all regular text output
         *   inside a <TITLE> ... </TITLE> sequence. 
         */
        if (html_in_ignore_)
        {
            /* check for the start of a new tag */
            if (c == '<')
            {
                /* stop skipping - return to normal parsing */
                return c;
            }
            else
            {
                wchar_t curCh;
                
                /* check for entities */
                if (c == '&')
                {
                    /* parse the entity (fetches the next character) */
                    c = parse_entity(vmg_ &curCh, sp, slenp);
                }
                else
                {
                    /* use this character literally */
                    curCh = c;

                    /* get the next character */
                    c = next_wchar(sp, slenp);
                }
                
                /* 
                 *   if we're gathering a title, and there's room in the
                 *   title buffer for more (always leaving room for a null
                 *   terminator), add this to the title buffer 
                 */
                if (html_in_title_ && html_title_ptr_ != 0)
                {
                    size_t rem, orig_rem;
                    
                    /* 
                     *   figure the remaining title buffer space, leaving
                     *   room for a terminating null byte 
                     */
                    orig_rem = rem = html_title_buf_size_ - 1;
                
                    /* 
                     *   map this character into the local character set and
                     *   add it to the buffer 
                     */
                    if (G_cmap_to_ui->map(curCh, &html_title_ptr_, &rem)
                        > orig_rem)
                    {
                        /* 
                         *   it didn't fit - add null termination and close
                         *   out the buffer (we might have to add more than
                         *   one byte, because a single character might take
                         *   up multiple bytes in the local character set; to
                         *   ensure that we don't skip a wide character then
                         *   later add a narrower character, close out the
                         *   entire buffer when first we encounter a
                         *   character that doesn't fit) 
                         */
                        while (html_title_ptr_
                               < html_title_buf_ + html_title_buf_size_)
                            *html_title_ptr_++ = '\0';
                    }
                }
            }

            /* don't display anything in an ignore section */
            continue;
        }

        /* 
         *   we didn't find any HTML parsing we need to do with this
         *   character, so return the character
         */
        return c;
    }

    /* return the character that made us stop parsing */
    return c;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse the beginning HTML markup.  This is called when we are scanning a
 *   '<' or '&' character in output text, and we're in HTML mode, and the
 *   underlying target doesn't support HTML parsing.  Returns the next
 *   character to process after we finish our initial parsing.  
 */
wchar_t CVmFormatter::parse_html_markup(VMG_ wchar_t c,
                                        const char **sp, size_t *slenp)
{
    /* check what we have */
    if (c == '<')
    {
        const size_t MAX_TAG_SIZE = 50;
        wchar_t tagbuf[MAX_TAG_SIZE];
        wchar_t *dst;
        int is_end_tag;
        
        /* skip the opening '<' */
        c = next_wchar(sp, slenp);
        
        /* note if this is a closing tag */
        if (c == '/' || c == '\\')
        {
            /* it's an end tag - note it and skip the slash */
            is_end_tag = TRUE;
            c = next_wchar(sp, slenp);
        }
        else
            is_end_tag = FALSE;
        
        /* 
         *   find the end of the tag name - the tag continues to the next
         *   space, '>', or end of line 
         */
        for (dst = tagbuf ; c != '\0' && c != ' ' && c != '>' ;
             c = next_wchar(sp, slenp))
        {
            /* add this to the tag buffer if it fits */
            if (dst + 1 < tagbuf + MAX_TAG_SIZE)
                *dst++ = c;
        }
        
        /* null-terminate the tag name */
        *dst = '\0';
        
        /*
         *   Check to see if we recognize the tag.  We only recognize a few
         *   simple tags that map easily to character mode.  
         */
        if (CVmCaseFoldStr::wstreq(tagbuf, L"br"))
        {
            /* 
             *   line break - if there's anything buffered up, just flush the
             *   current line, otherwise write out a blank line 
             */
            if (html_in_ignore_)
                /* suppress in ignore mode */;
            else if (linepos_ != 0)
                html_defer_br_ = HTML_DEFER_BR_FLUSH;
            else
                html_defer_br_ = HTML_DEFER_BR_BLANK;
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"b")
                 || CVmCaseFoldStr::wstreq(tagbuf, L"i")
                 || CVmCaseFoldStr::wstreq(tagbuf, L"em")
                 || CVmCaseFoldStr::wstreq(tagbuf, L"strong"))
        {
            /* suppress in ignore mode */
            if (!html_in_ignore_)
            {
                /* check to see if this is a start or end tag */
                if (!is_end_tag)
                {
                    int attr;

                    /* it's a start tag - push the current attributes */
                    push_color();

                    /* figure the attribute we're adding */
                    switch(tagbuf[0])
                    {
                    case 'b':
                    case 'B':
                        attr = OS_ATTR_BOLD;
                        break;

                    case 'i':
                    case 'I':
                        attr = OS_ATTR_ITALIC;
                        break;

                    case 'e':
                    case 'E':
                        attr = OS_ATTR_EM;
                        break;

                    case 's':
                    case 'S':
                        attr = OS_ATTR_STRONG;
                        break;
                    }

                    /* add the attribute */
                    cur_color_.attr |= attr;
                }
                else
                {
                    /* it's an end tag - just pop the saved attributes */
                    pop_color();
                }
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"p"))
        {
            /* paragraph - send out a blank line */
            if (!html_in_ignore_)
                write_blank_line(vmg0_);
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"tab") && !html_in_ignore_)
        {
            if (!html_in_ignore_)
            {
                /* if we have a pending tab, handle it */
                expand_pending_tab(vmg_ TRUE);

                /* start a tab definition */
                html_in_tab_ = TRUE;

                /* the new tab has no known characteristics yet */
                new_tab_entry_ = 0;
                new_tab_align_ = VMFMT_TAB_NONE;
                new_tab_dp_ = L'\0';
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"img")
                 || CVmCaseFoldStr::wstreq(tagbuf, L"sound"))
        {
            /* IMG and SOUND - allow ALT attributes */
            html_allow_alt_ = TRUE;
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"font"))
        {
            /* FONT - look for COLOR attribute */
            if (html_in_ignore_)
            {
                /* do nothing while ignoring tags */
            }
            else if (!is_end_tag)
            {
                /* enable the COLOR attribute */
                html_allow_color_ = TRUE;

                /* push the current color, in case we change it */
                push_color();
            }
            else
            {
                /* restore the color in the enclosing text */
                pop_color();
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"body"))
        {
            /* BODY - look for BGCOLOR and TEXT attributes */
            if (!html_in_ignore_ && !is_end_tag)
            {
                /* note that we're parsing a BODY tag */
                html_in_body_ = TRUE;
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"hr"))
        {
            int rem;

            if (!html_in_ignore_)
            {
                /* start a new line */
                flush(vmg_ VM_NL_NEWLINE);

                /* write out underscores to the display width */
                for (rem = console_->get_line_width() - 1 ; rem > 0 ; )
                {
                    const size_t DASHBUFLEN = 100;
                    wchar_t dashbuf[DASHBUFLEN];
                    wchar_t *p;
                    size_t cur;

                    /* do as much as we can on this pass */
                    cur = rem;
                    if (cur > DASHBUFLEN - 1)
                        cur = DASHBUFLEN - 1;

                    /* deduct this from the total */
                    rem -= cur;

                    /* fill the buffer with dashes */
                    for (p = dashbuf ; cur != 0 ; --cur)
                        *p++ = '-';
                    *p = '\0';

                    /* write it out */
                    buffer_wstring(vmg_ dashbuf);
                }

                /* put a blank line after the underscores */
                write_blank_line(vmg0_);
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"q"))
        {
            wchar_t htmlchar;

            if (!html_in_ignore_)
            {
                /* if it's an open quote, increment the level */
                if (!is_end_tag)
                    ++html_quote_level_;

                /* add the open quote */
                htmlchar =
                    (!is_end_tag
                     ? ((html_quote_level_ & 1) == 1
                        ? 8220 : 8216)
                     : ((html_quote_level_ & 1) == 1
                        ? 8221 : 8217));

                /* write out the HTML quote character */
                buffer_char(vmg_ htmlchar);

                /* if it's a close quote, decrement the level */
                if (is_end_tag)
                    --html_quote_level_;
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"title"))
        {
            /* 
             *   Turn ignore mode on or off as appropriate, and turn on or
             *   off title mode as well.  
             */
            if (is_end_tag)
            {
                /* 
                 *   note that we're leaving an ignore section and a title
                 *   section 
                 */
                --html_in_ignore_;
                --html_in_title_;

                /*  
                 *   If we're no longer in a title, and we have a buffer
                 *   where we've gathered up the title string, call the OS
                 *   layer to tell it the title string, in case it wants to
                 *   change the window title or otherwise make use of the
                 *   title.  (Note that some streams don't bother keeping
                 *   track of the title at all, so we might not have a title
                 *   buffer.)  
                 */
                if (html_in_title_ == 0 && html_title_ptr_ != 0)
                {
                    /* null-terminate the title string */
                    *html_title_ptr_ = '\0';
                    
                    /* tell the OS about the title */
                    if (!html_in_ignore_)
                        set_title_in_os(html_title_buf_);
                }
            }
            else
            {
                /* 
                 *   if we aren't already in a title, set up to capture the
                 *   title into the title buffer 
                 */
                if (!html_in_title_)
                    html_title_ptr_ = html_title_buf_;
                
                /* 
                 *   note that we're in a title and in an ignore section,
                 *   since nothing within gets displayed 
                 */
                ++html_in_ignore_;
                ++html_in_title_;
            }
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"aboutbox")
                 || CVmCaseFoldStr::wstreq(tagbuf, L"banner"))
        {
            /* turn ignore mode on or off as appropriate */
            if (is_end_tag)
                --html_in_ignore_;
            else
                ++html_in_ignore_;
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"log"))
        {
            /* handle the <log> tag according to our stream subclass */
            process_log_tag(is_end_tag);
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"nolog"))
        {
            /* handle the <nolog> tag according to our stream subclass */
            process_nolog_tag(is_end_tag);
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"wrap"))
        {
            /* note that we have a 'wrap' tag to process */
            if (!html_in_ignore_)
                html_in_wrap_ = TRUE;
        }
        else if (CVmCaseFoldStr::wstreq(tagbuf, L"pre"))
        {
            /* count the nesting level if starting PRE mode */
            if (!is_end_tag)
                ++html_pre_level_;

            /* surround the PRE block with blank lines */
            write_blank_line(vmg0_);

            /* count the nesting level if ending PRE mode */
            if (is_end_tag && html_pre_level_ != 0)
                --html_pre_level_;
        }

        /* suppress everything up to the next '>' */
        html_parse_state_ = VMCON_HPS_TAG;

        /* 
         *   return the next character - since we're now in TAG mode, we'll
         *   resume the HTML mini-parser to parse the contents of the tag 
         */
        return c;
    }
    else if (c == '&')
    {
        /* parse the HTML markup */
        wchar_t ent;
        c = parse_entity(vmg_ &ent, sp, slenp);

        /* translate it and write it out */
        buffer_char(vmg_ ent);

        /* proceed with the next character */
        return c;
    }
    else
    {
        /* 
         *   we have nothing special to do with this character - simply
         *   buffer it and move on to the next character 
         */
        buffer_char(vmg_ c);
        return next_wchar(sp, slenp);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an HTML entity markup.  Call this with the current character
 *   pointer at the '&'.  We'll parse the entity name and return it in *ent.
 *   The return value is the next character to be parsed after the entity.  
 */
wchar_t CVmFormatter::parse_entity(VMG_ wchar_t *ent, const char **sp,
                                   size_t *slenp)
{
    const size_t MAXAMPLEN = 10;
    char ampbuf[MAXAMPLEN];
    char *dst;
    const char *orig_s;
    size_t orig_slen;
    const struct amp_tbl_t *ampptr;
    size_t lo, hi, cur;
    wchar_t c;

    /* 
     *   remember where the part after the '&' begins, so we can come back
     *   here later if necessary 
     */
    orig_s = *sp;
    orig_slen = *slenp;
    
    /* get the character after the ampersand */
    c = next_wchar(sp, slenp);

    /* if it's numeric, parse the number */
    if (c == '#')
    {
        uint val;
        
        /* skip the '#' */
        c = next_wchar(sp, slenp);
        
        /* check for hex */
        if (c == 'x' || c == 'X')
        {
            /* skip the 'x' */
            c = next_wchar(sp, slenp);
            
            /* read the hex number */
            for (val = 0 ; is_xdigit(c) ; c = next_wchar(sp, slenp))
            {
                /* accumulate this digit into the value */
                val *= 16;
                if (is_digit(c))
                    val += c - '0';
                else if (c >= 'a' && c <= 'f')
                    val += c - 'a' + 10;
                else
                    val += c - 'A' + 10;
            }
        }
        else
        {
            /* read the number */
            for (val = 0 ; is_digit(c) ; c = next_wchar(sp, slenp))
            {
                /* accumulate this digit into the value */
                val *= 10;
                val += c - '0';
            }
        }
        
        /* if we found a ';' at the end, skip it */
        if (c == ';')
            c = next_wchar(sp, slenp);

        /* return the entity from the numeric character value */
        *ent = (wchar_t)val;
        
        /* we're done with this character */
        return c;
    }
    
    /*
     *   Parse the sequence after the '&'.  Parse up to the closing
     *   semicolon, or any non-alphanumeric, or until we fill up the buffer.
     */
    for (dst = ampbuf ;
         c != '\0' && (is_digit(c) || is_alpha(c))
             && dst < ampbuf + MAXAMPLEN - 1 ;
         c = next_wchar(sp, slenp))
    {
        /* copy this character to the name buffer */
        *dst++ = (char)c;
    }
    
    /* null-terminate the name */
    *dst = '\0';
    
    /* do a binary search for the name */
    lo = 0;
    hi = sizeof(amp_tbl)/sizeof(amp_tbl[0]) - 1;
    for (;;)
    {
        int diff;
        
        /* if we've converged, look no further */
        if (lo > hi
            || lo >= sizeof(amp_tbl)/sizeof(amp_tbl[0]))
        {
            ampptr = 0;
            break;
        }

        /* split the difference */
        cur = lo + (hi - lo)/2;
        ampptr = &amp_tbl[cur];
        
        /* see where we are relative to the target item */
        diff = strcmp(ampptr->cname, ampbuf);
        if (diff == 0)
        {
            /* this is it */
            break;
        }
        else if (lo >= hi)
        {
            /* failed to find it */
            ampptr = 0;
            break;
        }
        else if (diff > 0)
        {
            /* this one is too high - check the lower half */
            hi = (cur == hi ? hi - 1 : cur);
        }
        else
        {
            /* this one is too low - check the upper half */
            lo = (cur == lo ? lo + 1 : cur);
        }
    }

    /* skip to the appropriate next character */
    if (c == ';')
    {
        /* name ended with semicolon - skip the semicolon */
        c = next_wchar(sp, slenp);
    }
    else if (ampptr != 0)
    {
        int skipcnt;

        /* found the name - skip its exact length */
        skipcnt = strlen(ampptr->cname);
        for (*sp = orig_s, *slenp = orig_slen ; skipcnt != 0 ;
             c = next_wchar(sp, slenp), --skipcnt) ;

        /* 
         *   that positions us on the last character of the entity name; skip
         *   one more, so that we're on the character after the entity name 
         */
        c = next_wchar(sp, slenp);
    }
    
    /* if we found the entry, write out the character */
    if (ampptr != 0)
    {
        /* we found it - return the character value */
        *ent = ampptr->html_cval;
    }
    else
    {
        /* 
         *   we didn't find it - ignore the entire sequence, and just write
         *   it out verbatim; for our caller's purposes, the result of the
         *   parse is just the '&' itself 
         */
        *ent = '&';
        
        /* now go back and scan from the next character after the ampersand */
        *sp = orig_s;
        *slenp = orig_slen;
        c = next_wchar(sp, slenp);
    }

    /* return the next character */
    return c;
}


/* ------------------------------------------------------------------------ */
/*
 *   Service routine - read a number represented as a base-10 string of wide
 *   characters 
 */
int CVmFormatter::wtoi(const wchar_t *p)
{
    long acc;
    int neg = FALSE;

    /* skip any leading whitespace */
    while (t3_is_whitespace(*p))
        ++p;

    /* check for a leading sign */
    if (*p == '+')
    {
        /* positive sign - skip it */
        ++p;
    }
    else if (*p == '-')
    {
        /* negative sign - note it and skip it */
        neg = TRUE;
        ++p;
    }

    /* scan the digits */
    for (acc = 0 ; is_digit(*p) ; ++p)
    {
        /* shift the accumulator and add this digit */
        acc *= 10;
        acc += value_of_digit(*p);
    }

    /* apply the sign */
    if (neg)
        acc = -acc;

    /* return the result */
    return acc;
}

