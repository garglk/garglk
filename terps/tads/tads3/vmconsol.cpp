#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1987, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmconsol.cpp - TADS 3 console input reader and output formatter
Function
  Provides console input and output for the TADS 3 built-in function set
  for the T3 VM, including the output formatter.

  T3 uses the UTF-8 character set to represent character strings.  The OS
  functions use the local character set.  We perform the mapping between
  UTF-8 and the local character set within this module, so that OS routines
  see local characters only, not UTF-8.

  This code is based on the TADS 2 output formatter, but has been
  substantially reworked for C++, Unicode, and the slightly different
  TADS 3 formatting model.
Notes

Returns
  None
Modified
  08/25/99 MJRoberts  - created from TADS 2 output formatter
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "wchar.h"

#include "os.h"
#include "t3std.h"
#include "utf8.h"
#include "charmap.h"
#include "vmuni.h"
#include "vmconsol.h"
#include "vmglob.h"
#include "vmhash.h"
#include "vmdatasrc.h"
#include "vmnetfil.h"
#include "vmfilobj.h"
#include "vmerr.h"
#include "vmobj.h"


/* ------------------------------------------------------------------------ */
/*
 *   Log-file formatter subclass implementation 
 */

/*
 *   Open a new log file 
 */
int CVmFormatterLog::open_log_file(VMG_ const char *fname)
{
    CVmNetFile *nf = 0;
    err_try
    {
        /* create the network file descriptor */
        nf = CVmNetFile::open(
            vmg_ fname, 0, NETF_NEW, OSFTLOG, "text/plain");
    }
    err_catch_disc
    {
        nf = 0;
    }
    err_end;

    /* open the log file using the network file descriptor */
    return open_log_file(vmg_ nf);
}

int CVmFormatterLog::open_log_file(VMG_ const vm_val_t *filespec,
                                   const struct vm_rcdesc *rc)
{
    CVmNetFile *nf = 0;
    err_try
    {
        /* create the network file descriptor */
        nf = CVmNetFile::open(vmg_ filespec, rc,
                              NETF_NEW, OSFTLOG, "text/plain");

        /* validate file safety */
        CVmObjFile::check_safety_for_open(vmg_ nf, VMOBJFILE_ACCESS_WRITE);
    }
    err_catch_disc
    {
        /* if we got a file, it must be a safety exception - rethrow it */
        if (nf != 0)
        {
            nf->abandon(vmg0_);
            err_rethrow();
        }
    }
    err_end;

    /* open the log file using the network file descriptor */
    int err = open_log_file(vmg_ nf);

    /* if that succeeded, remember the file spec in our global variable */
    if (!err)
    {
        /* create our VM global if we don't have one already */
        if (logglob_ == 0)
            logglob_ = G_obj_table->create_global_var();

        /* remember the file spec */
        logglob_->val = *filespec;
    }

    /* return the result */
    return err;
}

int CVmFormatterLog::open_log_file(VMG_ CVmNetFile *nf)
{
    /* close any existing log file */
    if (close_log_file(vmg0_))
    {
        if (nf != 0)
            nf->abandon(vmg0_);
        return 1;
    }

    /* if there's no network file spec, return failure */
    if (nf == 0)
        return 1;

    /* reinitialize */
    init();

    /* remember the network file descriptor */
    lognf_ = nf;

    /* open the local file */
    logfp_ = osfopwt(lognf_->lclfname, OSFTLOG);

    /* if we couldn't open the file, abandon the network file descriptor */
    if (logfp_ == 0)
    {
        lognf_->abandon(vmg0_);
        lognf_ = 0;
    }

    /* return success if we successfully opened the file, failure otherwise */
    return (logfp_ == 0);
}

/*
 *   Set the log file to a file opened by the caller 
 */
int CVmFormatterLog::set_log_file(VMG_ CVmNetFile *nf, osfildef *fp)
{
    /* close any existing log file */
    if (close_log_file(vmg0_))
        return 1;

    /* reinitialize */
    init();

    /* remember the file */
    logfp_ = fp;
    lognf_ = nf;

    /* success */
    return 0;
}

/*
 *   Close the log file 
 */
int CVmFormatterLog::close_log_file(VMG0_)
{
    /* presume success */
    int err = FALSE;
    
    /* if we have a file, close it */
    if (logfp_ != 0)
    {
        /* close and forget the handle */
        osfcls(logfp_);
        logfp_ = 0;
    }

    /* forget the log network file descriptor, if we have one */
    if (lognf_ != 0)
    {
        err_try
        {
            lognf_->close(vmg0_);
        }
        err_catch_disc
        {
            /* flag the error, but otherwise discard the exception */
            err = TRUE;
        }
        err_end;

        /* forget the descriptor, as we've just deleted it */
        lognf_ = 0;
    }

    /* clear our global for the file spec */
    if (logglob_ != 0)
        logglob_->val.set_nil();

    /* success */
    return err;
}


/* ------------------------------------------------------------------------ */
/*
 *   Base Formatter 
 */

/*
 *   deletion
 */
CVmFormatter::~CVmFormatter()
{
    /* if we have a table of horizontal tabs, delete it */
    if (tabs_ != 0)
        delete tabs_;

    /* forget the character mapper */
    set_charmap(0);
}

/*
 *   set a new character mapper 
 */
void CVmFormatter::set_charmap(CCharmapToLocal *cmap)
{
    /* add a reference to the new mapper, if we have one */
    if (cmap != 0)
        cmap->add_ref();

    /* release our reference on any old mapper */
    if (cmap_ != 0)
        cmap_->release_ref();

    /* remember the new mapper */
    cmap_ = cmap;
}

/*
 *   Write out a line.  Text we receive is in the UTF-8 character set.
 */
void CVmFormatter::write_text(VMG_ const wchar_t *txt, size_t cnt,
                              const vmcon_color_t *colors, vm_nl_type nl)
{
    /* 
     *   Check the "script quiet" mode - this indicates that we're reading
     *   a script and not echoing output to the display.  If this mode is
     *   on, and we're writing to the display, suppress this write.  If
     *   the mode is off, or we're writing to a non-display stream (such
     *   as a log file stream), show the output as normal.  
     */
    if (!console_->is_quiet_script() || !is_disp_stream_)
    {
        char local_buf[128];
        char *dst;
        size_t rem;

        /*
         *   Check to see if we've reached the end of the screen, and if so
         *   run the MORE prompt.  Note that we don't show a MORE prompt
         *   unless we're in "formatter more mode," since if we're not, then
         *   the OS layer code is taking responsibility for pagination
         *   issues.
         *   
         *   Note that we suppress the MORE prompt if we're showing a
         *   continuation of a line already partially shown.  We only want to
         *   show a MORE prompt at the start of a new line.
         *   
         *   Skip the MORE prompt if this stream doesn't use it.  
         */
        if (formatter_more_mode()
            && console_->is_more_mode()
            && !is_continuation_
            && linecnt_ + 1 >= console_->get_page_length())
        {
            /* set the standard text color */
            set_os_text_color(OS_COLOR_P_TEXT, OS_COLOR_P_TEXTBG);
            set_os_text_attr(0);

            /* display the MORE prompt */
            console_->show_more_prompt(vmg0_);

            /* restore the current color scheme */
            set_os_text_color(os_color_.fg, os_color_.bg);
            set_os_text_attr(os_color_.attr);
        }

        /* count the line if a newline follows */
        if (nl != VM_NL_NONE && nl != VM_NL_NONE_INTERNAL)
            ++linecnt_;

        /* convert and display the text */
        for (dst = local_buf, rem = sizeof(local_buf) - 1 ; cnt != 0 ; )
        {
            size_t cur;
            size_t old_rem;
            wchar_t c;
            
            /* 
             *   if this character is in a new color, write out the OS-level
             *   color switch code 
             */
            if (colors != 0 && !colors->equals(&os_color_))
            {
                /* 
                 *   null-terminate and display what's in the buffer so far,
                 *   so that we close out all of the remaining text in the
                 *   old color and attributes
                 */
                *dst = '\0';
                print_to_os(local_buf);

                /* reset to the start of the local output buffer */
                dst = local_buf;
                rem = sizeof(local_buf) - 1;

                /* set the text attributes, if they changed */
                if (colors->attr != os_color_.attr)
                    set_os_text_attr(colors->attr);

                /* set the color, if it changed */
                if (colors->fg != os_color_.fg
                    || colors->bg != os_color_.bg)
                    set_os_text_color(colors->fg, colors->bg);

                /* 
                 *   Whatever happened, set our new color internally as the
                 *   last color we sent to the OS.  Even if we didn't
                 *   actually do anything, we'll at least know we won't have
                 *   to do anything more until we find another new color. 
                 */
                os_color_ = *colors;
            }

            /* get this character */
            c = *txt;

            /* 
             *   translate non-breaking spaces into ordinary spaces if the
             *   underlying target isn't HTML-based 
             */
            if (!html_target_ && c == 0x00A0)
                c = ' ';

            /* try storing another character */
            old_rem = rem;
            cur = (cmap_ != 0 ? cmap_ : G_cmap_to_ui)->map(c, &dst, &rem);

            /* if that failed, flush the buffer and try again */
            if (cur > old_rem)
            {
                /* null-terminate the buffer */
                *dst = '\0';
                
                /* display the text */
                print_to_os(local_buf);

                /* reset to the start of the local output buffer */
                dst = local_buf;
                rem = sizeof(local_buf) - 1;
            }
            else
            {
                /* we've now consumed this character of input */
                ++txt;
                --cnt;
                if (colors != 0)
                    ++colors;
            }
        }

        /* if we have a partially-filled buffer, display it */
        if (dst > local_buf)
        {
            /* null-terminate and display the buffer */
            *dst = '\0';
            print_to_os(local_buf);
        }

        /* write the appropriate type of line termination */
        switch(nl)
        {
        case VM_NL_NONE:
        case VM_NL_INPUT:
        case VM_NL_NONE_INTERNAL:
            /* no line termination is needed */
            break;

        case VM_NL_NEWLINE:
            /* write a newline */
            print_to_os(html_target_ && html_pre_level_ == 0 ?
                        "<BR HEIGHT=0>\n" : "\n");
            break;

        case VM_NL_OSNEWLINE:
            /* 
             *   the OS will provide a newline, but add a space to make it
             *   explicit that we can break the line here 
             */
            print_to_os(" ");
            break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Flush the current line to the display, using the given type of line
 *   termination.
 *   
 *   VM_NL_NONE: flush the current line but do not start a new line; more
 *   text will follow on the current line.  This is used, for example, to
 *   flush text after displaying a prompt and before waiting for user
 *   input.
 *   
 *   VM_NL_INPUT: acts like VM_NL_NONE, except that we flush everything,
 *   including trailing spaces.
 *   
 *   VM_NL_NONE_INTERNAL: same as VM_NL_NONE, but doesn't flush at the OS
 *   level.  This is used when we're only flushing our buffers in order to
 *   clear out space internally, not because we want the underlying OS
 *   renderer to display things immediately.  This distinction is
 *   important in HTML mode, since it ensures that the HTML parser only
 *   sees well-formed strings when flushing.
 *   
 *   VM_NL_NEWLINE: flush the line and start a new line by writing out a
 *   newline character.
 *   
 *   VM_NL_OSNEWLINE: flush the line as though starting a new line, but
 *   don't add an actual newline character to the output, since the
 *   underlying OS display code will handle this.  Instead, add a space
 *   after the line to indicate to the OS code that a line break is
 *   possible there.  (This differs from VM_NL_NONE in that VM_NL_NONE
 *   doesn't add anything at all after the line.)  
 */
void CVmFormatter::flush(VMG_ vm_nl_type nl)
{
    int cnt;
    vm_nl_type write_nl;

    /* null-terminate the current output line buffer */
    linebuf_[linepos_] = '\0';

    /* 
     *   Expand any pending tab.  Allow "anonymous" tabs only if we're
     *   flushing because we're ending the line normally; if we're not
     *   ending the line, we can't handle tabs that depend on the line
     *   ending. 
     */
    expand_pending_tab(vmg_ nl == VM_NL_NEWLINE);

    /* 
     *   note number of characters to display - assume we'll display all of
     *   the characters in the buffer 
     */
    cnt = wcslen(linebuf_);

    /* 
     *   Trim trailing spaces, unless we're about to read input or are doing
     *   an internal flush.  (Show trailing spaces when reading input, since
     *   we won't be able to revise the layout after this point.  Don't trim
     *   on an internal flush either, as this kind of flushing simply empties
     *   out our buffer exactly as it is.)  
     */
    if (nl != VM_NL_INPUT && nl != VM_NL_NONE_INTERNAL)
    {
        /* 
         *   look for last non-space character, but keep any spaces that come
         *   before an explicit non-breaking flag 
         */
        for ( ; cnt > 0 && linebuf_[cnt-1] == ' ' ; --cnt)
        {
            /* don't remove this character if it's marked as non-breaking */
            if ((flagbuf_[cnt-1] & VMCON_OBF_NOBREAK) != 0)
                break;
        }

        /* 
         *   if we're actually doing a newline, discard the trailing spaces
         *   for good - we don't want them at the start of the next line 
         */
        if (nl == VM_NL_NEWLINE)
            linepos_ = cnt;
    }

    /* check the newline mode */
    switch(nl)
    {
    case VM_NL_NONE:
    case VM_NL_NONE_INTERNAL:
        /* no newline - just flush out what we have */
        write_nl = VM_NL_NONE;
        break;

    case VM_NL_INPUT:
        /* no newline - flush out what we have */
        write_nl = VM_NL_NONE;

        /* on input, reset the HTML parsing state */
        html_passthru_state_ = VMCON_HPS_NORMAL;
        break;

    case VM_NL_NEWLINE:
        /* 
         *   We're adding a newline.  We want to suppress redundant
         *   newlines -- we reduce any run of consecutive vertical
         *   whitespace to a single newline.  So, if we have anything in
         *   this line, or we didn't already just write a newline, write
         *   out a newline now; otherwise, write nothing.  
         */
        if (linecol_ != 0 || !just_did_nl_ || html_pre_level_ > 0)
        {
            /* add the newline */
            write_nl = VM_NL_NEWLINE;
        }
        else
        {
            /* 
             *   Don't write out a newline after all - the line buffer is
             *   empty, and we just wrote a newline, so this is a
             *   redundant newline that we wish to suppress (so that we
             *   collapse a run of vertical whitespace down to a single
             *   newline).  
             */
            write_nl = VM_NL_NONE;
        }
        break;

    case VM_NL_OSNEWLINE:
        /* 
         *   we're going to depend on the underlying OS output layer to do
         *   line breaking, so we won't add a newline, but we will add a
         *   space, so that the underlying OS layer knows we have a word
         *   break here 
         */
        write_nl = VM_NL_OSNEWLINE;
        break;
    }

    /* 
     *   display the line, as long as we have something buffered to
     *   display; even if we don't, display it if our column is non-zero
     *   and we didn't just do a newline, since this must mean that we've
     *   flushed a partial line and are just now doing the newline 
     */
    if (cnt != 0 || (linecol_ != 0 && !just_did_nl_)
        || html_pre_level_ > 0)
    {
        /* write it out */
        write_text(vmg_ linebuf_, cnt, colorbuf_, write_nl);
    }

    /* check the line ending */
    switch (nl)
    {
    case VM_NL_NONE:
    case VM_NL_INPUT:
        /* we're not displaying a newline, so flush what we have */
        flush_to_os();

        /* 
         *   the subsequent buffer will be a continuation of the current
         *   text, if we've displayed anything at all here 
         */
        is_continuation_ = (linecol_ != 0);
        break;

    case VM_NL_NONE_INTERNAL:
        /* 
         *   internal buffer flush only - subsequent text will be a
         *   continuation of the current line, if there's anything on the
         *   current line 
         */
        is_continuation_ = (linecol_ != 0);
        break;

    default:
        /* we displayed a newline, so reset the column position */
        linecol_ = 0;

        /* the next buffer starts a new line on the display */
        is_continuation_ = FALSE;
        break;
    }

    /* 
     *   Move any trailing characters we didn't write in this go to the start
     *   of the buffer.  
     */
    if (cnt < linepos_)
    {
        size_t movecnt;

        /* calculate how many trailing characters we didn't write */
        movecnt = linepos_ - cnt;

        /* move the characters, colors, and flags */
        memmove(linebuf_, linebuf_ + cnt, movecnt * sizeof(linebuf_[0]));
        memmove(colorbuf_, colorbuf_ + cnt, movecnt * sizeof(colorbuf_[0]));
        memmove(flagbuf_, flagbuf_ + cnt, movecnt * sizeof(flagbuf_[0]));
    }

    /* move the line output position to follow the preserved characters */
    linepos_ -= cnt;

    /* 
     *   If we just output a newline, note it.  If we didn't just output a
     *   newline, but we did write out anything else, note that we're no
     *   longer at the start of a line on the underlying output device.  
     */
    if (nl == VM_NL_NEWLINE)
        just_did_nl_ = TRUE;
    else if (cnt != 0)
        just_did_nl_ = FALSE;

    /* 
     *   if the current buffering color doesn't match the current osifc-layer
     *   color, then we must need to flush just the new color/attribute
     *   settings (this can happen when we have changed the attributes in
     *   preparation for reading input, since we won't have any actual text
     *   to write after the color change) 
     */
    if (!cur_color_.equals(&os_color_))
    {
        /* set the text attributes in the OS window, if they changed */
        if (cur_color_.attr != os_color_.attr)
            set_os_text_attr(cur_color_.attr);

        /* set the color in the OS window, if it changed */
        if (cur_color_.fg != os_color_.fg
            || cur_color_.bg != os_color_.bg)
            set_os_text_color(cur_color_.fg, cur_color_.bg);

        /* set the new osifc color */
        os_color_ = cur_color_;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear out our buffers 
 */
void CVmFormatter::empty_buffers(VMG0_)
{
    /* reset our buffer pointers */
    linepos_ = 0;
    linecol_ = 0;
    linebuf_[0] = '\0';
    just_did_nl_ = FALSE;
    is_continuation_ = FALSE;

    /* there's no pending tab now */
    pending_tab_align_ = VMFMT_TAB_NONE;

    /* start out at the first line */
    linecnt_ = 0;

    /* reset the HTML lexical state */
    html_passthru_state_ = VMCON_HPS_NORMAL;
}

/* ------------------------------------------------------------------------ */
/*
 *   Immediately update the display window 
 */
void CVmFormatter::update_display(VMG0_)
{
    /* update the display window at the OS layer */
    os_update_display();
}

/* ------------------------------------------------------------------------ */
/*
 *   Display a blank line to the stream
 */
void CVmFormatter::write_blank_line(VMG0_)
{
    /* flush the stream */
    flush(vmg_ VM_NL_NEWLINE);

    /* if generating for an HTML display target, add an HTML line break */
    if (html_target_)
        write_text(vmg_ L"<BR>", 4, 0, VM_NL_NONE);

    /* write out a blank line */
    write_text(vmg_ L"", 0, 0, VM_NL_NEWLINE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate a tab for a "\t" sequence in the game text, or a <TAB
 *   MULTIPLE> or <TAB INDENT> sequence parsed in our mini-parser.
 *   
 *   Standard (non-HTML) version: we'll generate enough spaces to take us to
 *   the next tab stop.
 *   
 *   HTML version: if we're in native HTML mode, we'll just generate the
 *   equivalent HTML; if we're not in HTML mode, we'll generate a hard tab
 *   character, which the HTML formatter will interpret as a <TAB
 *   MULTIPLE=4>.  
 */
void CVmFormatter::write_tab(VMG_ int indent, int multiple)
{
    int maxcol;

    /* check to see what the underlying system is expecting */
    if (html_target_)
    {
        char buf[40];

        /* 
         *   the underlying system is HTML - generate an appropriate <TAB>
         *   sequence to produce the desired effect 
         */
        sprintf(buf, "<TAB %s=%d>",
                indent != 0 ? "INDENT" : "MULTIPLE",
                indent != 0 ? indent : multiple);
            
        /* write it out */
        buffer_string(vmg_ buf);
    }
    else if (multiple != 0)
    {
        /* get the maximum column */
        maxcol = get_buffer_maxcol();

        /*
         *   We don't have an HTML target, and we have a tab to an every-N
         *   stop: expand the tab with spaces.  Keep going until we reach
         *   the next tab stop of the given multiple.  
         */
        do
        {
            /* stop if we've reached the maximum column */
            if (linecol_ >= maxcol)
                break;

            /* add another space */
            linebuf_[linepos_] = ' ';
            flagbuf_[linepos_] = cur_flags_;
            colorbuf_[linepos_] = cur_color_;

            /* advance one character in the buffer */
            ++linepos_;

            /* advance the column counter */
            ++linecol_;
        } while ((linecol_ + 1) % multiple != 0);
    }
    else if (indent != 0)
    {
        /* 
         *   We don't have an HTML target, and we just want to add a given
         *   number of spaces.  Simply write out the given number of spaces,
         *   up to our maximum column limit.  
         */
        for (maxcol = get_buffer_maxcol() ;
             indent != 0 && linecol_ < maxcol ; --indent)
        {
            /* add another space */
            linebuf_[linepos_] = ' ';
            flagbuf_[linepos_] = cur_flags_;
            colorbuf_[linepos_] = cur_color_;

            /* advance one character in the buffer and one column */
            ++linepos_;
            ++linecol_;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Flush a line 
 */
void CVmFormatter::flush_line(VMG_ int padding)
{
    /* 
     *   check to see if we're using the underlying display layer's line
     *   wrapping 
     */
    if (os_line_wrap_)
    {
        /*
         *   In the HTML version, we don't need the normal *MORE*
         *   processing, since the HTML layer will handle that.
         *   Furthermore, we don't need to provide actual newline breaks
         *   -- that happens after the HTML is parsed, so we don't have
         *   enough information here to figure out actual line breaks.
         *   So, we'll just flush out our buffer whenever it fills up, and
         *   suppress newlines.
         *   
         *   Similarly, if we have OS-level line wrapping, don't try to
         *   figure out where the line breaks go -- just flush our buffer
         *   without a trailing newline whenever the buffer is full, and
         *   let the OS layer worry about formatting lines and paragraphs.
         *   
         *   If we're using padding, use newline mode VM_NL_OSNEWLINE.  If
         *   we don't want padding (which is the case if we completely
         *   fill up the buffer without finding any word breaks), write
         *   out in mode VM_NL_NONE, which just flushes the buffer exactly
         *   like it is.  
         */
        flush(vmg_ padding ? VM_NL_OSNEWLINE : VM_NL_NONE_INTERNAL);
    }
    else
    {
        /*
         *   Normal mode - we process the *MORE* prompt ourselves, and we
         *   are responsible for figuring out where the actual line breaks
         *   go.  Use flush() to generate an actual newline whenever we
         *   flush out our buffer.  
         */
        flush(vmg_ VM_NL_NEWLINE);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Write a character to an output stream.  The character is provided to us
 *   as a wide Unicode character.  
 */
void CVmFormatter::buffer_char(VMG_ wchar_t c)
{
    const wchar_t *exp;
    size_t exp_len;

    /* check for a display expansion */
    exp = (cmap_ != 0 ? cmap_ : G_cmap_to_ui)->get_expansion(c, &exp_len);
    if (exp != 0)
    {
        /* write each character of the expansion */
        for ( ; exp_len != 0 ; ++exp, --exp_len)
            buffer_expchar(vmg_ *exp);
    }
    else
    {
        /* there's no expansion - buffer the character as-is */
        buffer_expchar(vmg_ c);
    }
}

/*
 *   Write an expanded character to an output stream.  
 */
void CVmFormatter::buffer_expchar(VMG_ wchar_t c)
{
    /* presume the character takes up only one column */
    int cwid = 1;

    /* presume we'll use the current flags for the new character */
    unsigned char cflags = cur_flags_;

    /* assume it's not a quoted space */
    int qspace = FALSE;

    /* 
     *   Check for some special characters.
     *   
     *   If we have an underlying HTML renderer, keep track of the HTML
     *   lexical state, so we know if we're in a tag or in ordinary text.  We
     *   can pass through all of the special line-breaking and spacing
     *   characters to the underlying HTML renderer.
     *   
     *   If our underlying renderer is a plain text renderer, we actually
     *   parse the HTML ourselves, so HTML tags will never make it this far -
     *   the caller will already have interpreted any HTML tags and removed
     *   them from the text stream, passing only the final plain text to us.
     *   However, with a plain text renderer, we have to do all of the work
     *   of line breaking, so we must look at the special spacing and
     *   line-break control characters.  
     */
    if (html_target_)
    {
        /* 
         *   track the lexical state of the HTML stream going to the
         *   underlying renderer 
         */
        switch (html_passthru_state_)
        {
        case VMCON_HPS_MARKUP_END:
        case VMCON_HPS_NORMAL:
            /* check to see if we're starting a markup */
            if (c == '&')
                html_passthru_state_ = VMCON_HPS_ENTITY_1ST;
            else if (c == '<')
            {
                html_passthru_tagp_ = html_passthru_tag_;
                html_passthru_state_ = VMCON_HPS_TAG_START;
            }
            else
                html_passthru_state_ = VMCON_HPS_NORMAL;
            break;

        case VMCON_HPS_ENTITY_1ST:
            /* check to see what kind of entity we have */
            if (c == '#')
                html_passthru_state_ = VMCON_HPS_ENTITY_NUM_1ST;
            else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
                html_passthru_state_ = VMCON_HPS_ENTITY_NAME;
            else
                html_passthru_state_ = VMCON_HPS_NORMAL;
            break;

        case VMCON_HPS_ENTITY_NUM_1ST:
            /* check to see what kind of number we have */
            if (c == 'x' || c == 'X')
                html_passthru_state_ = VMCON_HPS_ENTITY_HEX;
            else if (c >= '0' && c <= '9')
                html_passthru_state_ = VMCON_HPS_ENTITY_DEC;
            else
                html_passthru_state_ = VMCON_HPS_NORMAL;
            break;

        case VMCON_HPS_ENTITY_HEX:
            /* see if we're done with hex digits */
            if (c == ';')
                html_passthru_state_ = VMCON_HPS_MARKUP_END;
            else if ((c < '0' || c > '9')
                     && (c < 'a' || c > 'f')
                     && (c < 'A' || c > 'F'))
                html_passthru_state_ = VMCON_HPS_NORMAL;
            break;

        case VMCON_HPS_ENTITY_DEC:
            /* see if we're done with decimal digits */
            if (c == ';')
                html_passthru_state_ = VMCON_HPS_MARKUP_END;
            else if (c < '0' || c > '9')
                html_passthru_state_ = VMCON_HPS_NORMAL;
            break;

        case VMCON_HPS_ENTITY_NAME:
            /* see if we're done with alphanumerics */
            if (c == ';')
                html_passthru_state_ = VMCON_HPS_MARKUP_END;
            else if ((c < 'a' || c > 'z')
                     && (c < 'A' || c > 'Z')
                     && (c < '0' || c > '9'))
                html_passthru_state_ = VMCON_HPS_NORMAL;
            break;

        case VMCON_HPS_TAG_START:
            /* start of a tag, before the name - check for the name start */
            if (c == '/' && html_passthru_tagp_ == html_passthru_tag_)
            {
                /* 
                 *   note the initial '/', but stay in TAG_START state, so
                 *   that we skip any spaces between the '/' and the tag name
                 */
                *html_passthru_tagp_++ = '/';
            }
            else if ((c >= 'a' && c <= 'z')
                     || (c >= 'A' && c <= 'Z'))
            {
                /* start of the tag name */
                html_passthru_state_ = VMCON_HPS_TAG_NAME;
                *html_passthru_tagp_++ = (char)c;
            }
            else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                /* 
                 *   ignore whitespace between '<' and the tag name - simply
                 *   stay in TAG_START mode 
                 */
            }
            else
            {
                /* anything else is invalid - must not be a tag after all */
                html_passthru_state_ = VMCON_HPS_NORMAL;
            }
            break;

        case VMCON_HPS_TAG_NAME:
            /* tag name - check for continuation */
            if ((c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z'))
            {
                /* gather the tag name if it fits */
                if (html_passthru_tagp_ - html_passthru_tag_ + 1
                    < sizeof(html_passthru_tag_))
                    *html_passthru_tagp_++ = (char)c;
            }
            else
            {
                /* end of the tag name */
                *html_passthru_tagp_ = '\0';
                html_passthru_state_ = VMCON_HPS_TAG;
                goto do_tag;
            }
            break;

        case VMCON_HPS_TAG:
            do_tag:
            /* see if we're done with the tag, or entering quoted material */
            if (c == '>')
            {
                /* switch to end of markup mode */
                html_passthru_state_ = VMCON_HPS_MARKUP_END;

                /* note if entering or exiting a PRE tag */
                if (stricmp(html_passthru_tag_, "pre") == 0)
                    ++html_pre_level_;
                else if (stricmp(html_passthru_tag_, "/pre") == 0
                         && html_pre_level_ != 0)
                    --html_pre_level_;
            }
            else if (c == '/'
                     && html_passthru_tagp_ != html_passthru_tag_
                     && *(html_passthru_tagp_ - 1) != '/')
            {
                /* add the '/' to the end of the tag name */
                if (html_passthru_tagp_ - html_passthru_tag_ + 1
                    < sizeof(html_passthru_tag_))
                {
                    *html_passthru_tagp_++ = (char)c;
                    *html_passthru_tagp_ = '\0';
                }
            }
            else if (c == '"')
                html_passthru_state_ = VMCON_HPS_DQUOTE;
            else if (c == '\'')
                html_passthru_state_ = VMCON_HPS_SQUOTE;
            break;

        case VMCON_HPS_SQUOTE:
            /* see if we're done with the quoted material */
            if (c == '\'')
                html_passthru_state_ = VMCON_HPS_TAG;
            break;

        case VMCON_HPS_DQUOTE:
            /* see if we're done with the quoted material */
            if (c == '"')
                html_passthru_state_ = VMCON_HPS_TAG;
            break;

        default:
            /* ignore other states */
            break;
        }
    }
    else
    {
        /* check for special characters */
        switch(c)
        {
        case 0x00AD:
            /*
             *   The Unicode "soft hyphen" character.  This indicates a
             *   point at which we can insert a hyphen followed by a soft
             *   line break, if it's a convenient point to break the line;
             *   if we don't choose to break the line here, the soft hyphen
             *   is invisible.  
             *   
             *   Don't buffer anything at all; instead, just flag the
             *   preceding character as being a soft hyphenation point, so
             *   that we can insert a hyphen there when we get around to
             *   breaking the line.  
             */
            if (linepos_ != 0)
                flagbuf_[linepos_ - 1] |= VMCON_OBF_SHY;

            /* we don't need to buffer anything, so we're done */
            return;

        case 0xFEFF:
            /*
             *   The Unicode zero-width non-breaking space.  This indicates
             *   a point at which we cannot break the line, even if we could
             *   normally break here.  Flag the preceding character as a
             *   non-breaking point.  Don't buffer anything for this
             *   character, as it's not rendered; it merely controls line
             *   breaking.  
             */
            if (linepos_ != 0)
                flagbuf_[linepos_ - 1] |= VMCON_OBF_NOBREAK;

            /* we don't buffer anything, so we're done */
            return;

        case 0x200B:                                    /* zero-width space */
        case 0x200a:                                          /* hair space */
        case 0x2008:                                   /* punctuation space */
            /* 
             *   Zero-width space: This indicates an explicitly allowed
             *   line-breaking point, but is rendered as invisible.  Flag the
             *   preceding character as an OK-to-break point, but don't
             *   buffer anything, as the zero-width space isn't rendered.  
             *   
             *   Hair and punctuation spaces: Treat these very thin spaces as
             *   invisible in a fixed-width font.  These are normally used
             *   for subtle typographical effects in proportionally-spaced
             *   fonts; for example, for separating a right single quote from
             *   an immediately following right double quote (as in a
             *   quotation within a quotation: I said, "type 'quote'").  When
             *   translating to fixed-pitch type, these special spacing
             *   effects aren't usually necessary or desirable because of the
             *   built-in space in every character cell.
             *   
             *   These spaces cancel any explicit non-breaking flag that
             *   precedes them, since they cause the flag to act on the
             *   space's left edge, while leaving the right edge open for
             *   breaking.  Since we don't actually take up any buffer space,
             *   push our right edge's breakability back to the preceding
             *   character.  
             */
            if (linepos_ != 0)
            {
                flagbuf_[linepos_ - 1] &= ~VMCON_OBF_NOBREAK;
                flagbuf_[linepos_ - 1] |= VMCON_OBF_OKBREAK;
            }

            /* we don't buffer anything, so we're done */
            return;

        case 0x00A0:
            /* non-breaking space - buffer it as given */
            break;
            
        case 0x0015:             /* special internal quoted space character */
        case 0x2005:                                   /* four-per-em space */
        case 0x2006:                                    /* six-per-em space */
        case 0x2007:                                        /* figure space */
        case 0x2009:                                          /* thin space */
            /* 
             *   Treat all of these as non-combining spaces, and render them
             *   all as single ordinary spaces.  In text mode, we are
             *   limited to a monospaced font, so we can't render any
             *   differences among these various thinner-than-normal spaces.
             */
            qspace = TRUE;
            c = ' ';
            break;

        case 0x2002:                                            /* en space */
        case 0x2004:                                  /* three-per-em space */
            /* 
             *   En space, three-per-em space - mark these as non-combining,
             *   and render them as a two ordinary spaces.  In the case of
             *   an en space, we really do want to take up the space of two
             *   ordinary spaces; for a three-per-em space, we want about a
             *   space and a half, but since we're dealing with a monospaced
             *   font, we have to round up to a full two spaces.  
             */
            qspace = TRUE;
            cwid = 2;
            c = ' ';
            break;
            
        case 0x2003:
            /* em space - mark it as non-combining */
            qspace = TRUE;
            
            /* render this as three ordinary spaces */
            cwid = 3;
            c = ' ';
            break;

        default:
            /* 
             *   Translate any horizontal whitespace character to a regular
             *   space character.  Note that, once this is done, we don't
             *   need to worry about calling t3_is_space() any more - we can
             *   just check that we have a regular ' ' character.  
             */
            if (t3_is_space(c))
            {
                /* convert it to an ordinary space */
                c = ' ';
                
                /* if we're in obey-whitespace mode, quote the space */
                qspace = obey_whitespace_ || html_pre_level_ > 0;
            }
            break;
        }
    }

    /* if it's a quoted space, mark it as such in the buffer flags */
    if (qspace)
        cflags |= VMCON_OBF_QSPACE;

    /* 
     *   Check for the caps/nocaps flags - but only if our HTML lexical state
     *   in the underlying text stream is plain text, because we don't want
     *   to apply these flags to alphabetic characters that are inside tag or
     *   entity text.  
     */
    if (html_passthru_state_ == VMCON_HPS_NORMAL)
    {
        if ((capsflag_ || allcapsflag_) && t3_is_alpha(c))
        {
            /* 
             *   capsflag or allcapsflag is set, so render this character in
             *   title case or upper case, respectively.  For the ordinary
             *   capsflag, use title case rather than upper case, since
             *   capsflag conceptually only applies to a single letter, and
             *   some Unicode characters represent ligatures of multiple
             *   letters.  In such cases we only want to capitalize the first
             *   letter in the ligature, which is exactly what we get when we
             *   convert it to the title case.
             *   
             *   Start by consuming the capsflag. 
             */
            capsflag_ = FALSE;

            /* get the appropriate expansion */
            const wchar_t *u = allcapsflag_ ? t3_to_upper(c) : t3_to_title(c);

            /*
             *   If there's no expansion, continue with the original
             *   character.  If it's a single-character expansion, continue
             *   with the replacement character.  If it's a 1:N expansion,
             *   recursively buffer the expansion. 
             */
            if (u != 0 && u[0] != 0)
            {
                if (u[1] != 0)
                {
                    /* 1:N expansion - handle it recursively */
                    buffer_wstring(vmg_ u);
                    return;
                }
                else
                {
                    /* 1:1 expansion - continue with the new character */
                    c = u[0];
                }
            }
        }
        else if (nocapsflag_ && t3_is_alpha(c))
        {
            /* lower-casing the character - consume the flag */
            nocapsflag_ = FALSE;

            /* get the expansion */
            const wchar_t *l = t3_to_lower(c);

            /* 
             *   recursively handle 1:N expansions; otherwise continue with
             *   the replacement character, if there is one 
             */
            if (l != 0 && l[0] != 0)
            {
                if (l[1] != 0)
                {
                    /* 1:N expansion - handle it recursively */
                    buffer_wstring(vmg_ l);
                    return;
                }
                else
                {
                    /* 1:1 expansion - continue with the new character */
                    c = l[0];
                }
            }
        }
    }

    /*
     *   If this is a space of some kind, we might be able to consolidate it
     *   with a preceding character.  If the display layer is an HTML
     *   renderer, pass spaces through intact, and let the HTML parser handle
     *   whitespace compression.
     */
    if (c == ' ' && html_pre_level_ == 0)
    {
        /* ignore ordinary whitespace at the start of a line */
        if (linecol_ == 0 && !qspace)
            return;

        /* 
         *   Consolidate runs of whitespace.  Ordinary whitespace is
         *   subsumed into any type of quoted spaces, but quoted spaces do
         *   not combine.  
         */
        if (linepos_ > 0)
        {
            wchar_t prv;

            /* get the previous character */
            prv = linebuf_[linepos_ - 1];
            
            /* 
             *   if the new character is an ordinary (combining) whitespace
             *   character, subsume it into any preceding space character 
             */
            if (!qspace && prv == ' ')
                return;

            /* 
             *   if the new character is a quoted space, and the preceding
             *   character is a non-quoted space, subsume the preceding
             *   space into the new character 
             */
            if (qspace
                && prv == ' '
                && !(flagbuf_[linepos_ - 1] & VMCON_OBF_QSPACE))
            {
                /* remove the preceding ordinary whitespace */
                --linepos_;
                --linecol_;
            }
        }
    }

    /* if the new character fits in the line, add it */
    if (linecol_ + cwid < get_buffer_maxcol())
    {
        /* buffer this character */
        buffer_rendered(c, cflags, cwid);

        /* we're finished processing the character */
        return;
    }

    /*
     *   The line would overflow if this character were added.
     *   
     *   If we're trying to output any kind of breakable space, just add it
     *   to the line buffer for now; we'll come back later and figure out
     *   where to break upon buffering the next non-space character.  This
     *   ensures that we don't carry trailing space (even trailing en or em
     *   spaces) to the start of the next line if we have an explicit
     *   newline before the next non-space character.  
     */
    if (c == ' ')
    {
        /* 
         *   We're adding a space, so we'll figure out the breaking later,
         *   when we output the next non-space character.  If the preceding
         *   character is any kind of space, don't bother adding the new
         *   one, since any contiguous whitespace at the end of the line has
         *   no effect on the line's appearance.  
         */
        if (linebuf_[linepos_ - 1] == ' ')
        {
            /* 
             *   We're adding a space to a line that already ends in a
             *   space, so we don't really need to add the character.
             *   However, reflect the virtual addition in the output column
             *   position, since the space does affect our column position.
             *   We know that we're adding the new space even though we have
             *   a space preceding, since we wouldn't have gotten this far
             *   if we were going to collapse the space with a run of
             *   whitespace. 
             */
        }
        else
        {
            /* the line doesn't already end in space, so add the space */
            linebuf_[linepos_] = ' ';
            flagbuf_[linepos_] = cflags;
            colorbuf_[linepos_] = cur_color_;

            /* advance one character in the buffer */
            ++linepos_;
        }

        /* 
         *   Adjust the column position for the added space.  Note that we
         *   adjust by the rendered width of the new character even though
         *   we actually added only one character; we only add one character
         *   to the buffer to avoid buffer overflow, but the column position
         *   needs adjustment by the full rendered width.  The fact that the
         *   actual buffer size and rendered width no longer match isn't
         *   important because the difference is entirely in invisible
         *   whitespace at the right end of the line.  
         */
        linecol_ += cwid;

        /* done for now */
        return;
    }
    
    /*
     *   We're adding something other than an ordinary space to the line,
     *   and the new character won't fit, so we must find an appropriate
     *   point to break the line. 
     *   
     *   First, add the new character to the buffer - it could be
     *   significant in how we calculate the break position.  (Note that we
     *   allocate the buffer with space for one extra character after
     *   reaching the maximum line width, so we know we have room for this.)
     */
    linebuf_[linepos_] = c;
    flagbuf_[linepos_] = cur_flags_;

    /* 
     *   if the underlying OS layer is doing the line wrapping, just flush
     *   out the buffer; don't bother trying to do any line wrapping
     *   ourselves, since this work would just be redundant with what the OS
     *   layer has to do anyway 
     */
    if (os_line_wrap_)
    {
        /* flush the line, adding no padding after it */
        flush_line(vmg_ FALSE);

        /* 
         *   we've completely cleared out the line buffer, so reset all of
         *   the line buffer counters 
         */
        linepos_ = 0;
        linecol_ = 0;
        linebuf_[0] = '\0';
        is_continuation_ = FALSE;

        /* we're done */
        goto done_with_wrapping;
    }

    /*
     *   Scan backwards, looking for a break position.  Start at the current
     *   column: we know we can fit everything up to this point on a line on
     *   the underlying display, so this is the rightmost possible position
     *   at which we could break the line.  Keep going until we find a
     *   breaking point or reach the left edge of the line.  
     */
    int shy, i;
    for (i = linepos_, shy = FALSE ; i >= 0 ; --i)
    {
        unsigned char f;
        unsigned char prvf;
        
        /* 
         *   There are two break modes: word-break mode and break-anywhere
         *   mode.  The modes are applied to each character, via the buffer
         *   flags.
         *   
         *   In word-break mode, we can break at any ordinary space, at a
         *   soft hyphen, just after a regular hyphen, or at any explicit
         *   ok-to-break point; but we can't break after any character
         *   marked as a no-break point.
         *   
         *   In break-anywhere mode, we can break between any two
         *   characters, except that we can't break after any character
         *   marked as a no-break point.  
         */

        /* get the current character's flags */
        f = flagbuf_[i];

        /* get the preceding character's flags */
        prvf = (i > 0 ? flagbuf_[i-1] : 0);

        /* 
         *   if the preceding character is marked as a no-break point, we
         *   definitely can't break here, so keep looking 
         */
        if ((prvf & VMCON_OBF_NOBREAK) != 0)
            continue;

        /* 
         *   if the preceding character is marked as an explicit ok-to-break
         *   point, we definitely can break here 
         */
        if ((prvf & VMCON_OBF_OKBREAK) != 0)
            break;

        /* 
         *   If the current character is in a run of break-anywhere text,
         *   then we can insert a break just before the current character.
         *   Likewise, if the preceding character is in a run of
         *   break-anywhere text, we can break just after the preceding
         *   character, which is the same as breaking just before the
         *   current character.
         *   
         *   Note that we must test for both cases to properly handle
         *   boundaries between break-anywhere and word-break text.  If
         *   we're switching from word-break to break-anywhere text, the
         *   current character will be marked as break-anywhere, so if we
         *   only tested the previous character, we'd miss this transition.
         *   If we're switching from break-anywhere to word-break text, the
         *   previous character will be marked as break-anywhere, so we'd
         *   miss the fact that we could break right here (rather than
         *   before the previous character) if we didn't test it explicitly.
         */
        if ((f & VMCON_OBF_BREAK_ANY) != 0
            || (i > 0 && (prvf & VMCON_OBF_BREAK_ANY) != 0))
            break;

        /* 
         *   If the preceding character is marked as a soft hyphenation
         *   point, and we're not at the rightmost position, we can break
         *   here with hyphenation.  We can't break with hyphenation at the
         *   last position because hyphenation requires us to actually
         *   insert a hyphen character, and we know that at the last
         *   position we don't have room for inserting another character.  
         */
        if (i > 0 && i < linepos_ && (prvf & VMCON_OBF_SHY) != 0)
        {
            /* note that we're breaking at a soft hyphen */
            shy = TRUE;

            /* we can break here */
            break;
        }

        /* 
         *   we can break to the left of a space (i.e., we can break before
         *   the current character if the current character is a space) 
         */
        if (linebuf_[i] == ' ')
            break;

        /* 
         *   We can also break to the right of a space.  We need to check
         *   for this case separately from checking that the current
         *   charatcer is a space (which breaks to the left of the space),
         *   because we could have a no-break marker on one side of the
         *   space but not on the other side.  
         */
        if (i > 0 && linebuf_[i-1] == ' ')
            break;

        /* 
         *   If we're to the right of a hyphen, we can break here.  However,
         *   don't break in the middle of a set of consecutive hyphens
         *   (i.e., we don't want to break up "--" sequences).
         */
        if (i > 0 && linebuf_[i-1] == '-' && linebuf_[i] != '-')
            break;
    }
    
    /* check to see if we found a good place to break */
    if (i < 0)
    {
        /*
         *   We didn't find a good place to break.  If the underlying
         *   console allows overrunning the line width, simply add the
         *   character, even though it overflows; otherwise, force a break
         *   at the line width, even though it doesn't occur at a natural
         *   breaking point.
         *   
         *   In any case, don't let our buffer fill up beyond its maximum
         *   size.  
         */
        if (!console_->allow_overrun() || linepos_ + 1 >= OS_MAXWIDTH)
        {
            /* 
             *   we didn't find any good place to break, and the console
             *   doesn't allow us to overrun the terminal width - flush the
             *   entire line as-is, breaking arbitrarily in the middle of a
             *   word 
             */
            flush_line(vmg_ FALSE);
            
            /* 
             *   we've completely cleared out the line buffer, so reset all
             *   of the line buffer counters 
             */
            linepos_ = 0;
            linecol_ = 0;
            linebuf_[0] = '\0';
            is_continuation_ = FALSE;
        }
    }
    else
    {
        wchar_t tmpbuf[OS_MAXWIDTH];
        vmcon_color_t tmpcolor[OS_MAXWIDTH];
        unsigned char tmpflags[OS_MAXWIDTH];
        size_t tmpchars;
        int nxti;

        /* null-terminate the line buffer */        
        linebuf_[linepos_] = '\0';

        /* trim off leading spaces on the next line after the break */
        for (nxti = i ; linebuf_[nxti] == ' ' ; ++nxti) ;

        /* 
         *   The next line starts after the break - save a copy.  We actually
         *   have to save a copy of the trailing material outside the buffer,
         *   since we might have to overwrite the trailing part of the buffer
         *   to expand tabs.  
         */
        tmpchars = wcslen(&linebuf_[nxti]);
        memcpy(tmpbuf, &linebuf_[nxti], tmpchars*sizeof(tmpbuf[0]));
        memcpy(tmpcolor, &colorbuf_[nxti], tmpchars*sizeof(tmpcolor[0]));
        memcpy(tmpflags, &flagbuf_[nxti], tmpchars*sizeof(tmpflags[0]));

        /* if we're breaking at a soft hyphen, insert a real hyphen */
        if (shy)
            linebuf_[i++] = '-';
        
        /* trim off trailing spaces */
        for ( ; i > 0 && linebuf_[i-1] == ' ' ; --i)
        {
            /* stop if we've reached a non-breaking point */
            if ((flagbuf_[i-1] & VMCON_OBF_NOBREAK) != 0)
                break;
        }

        /* terminate the buffer after the break point */
        linebuf_[i] = '\0';
        
        /* write out everything up to the break point */
        flush_line(vmg_ TRUE);

        /* move the saved start of the next line into the line buffer */
        memcpy(linebuf_, tmpbuf, tmpchars*sizeof(tmpbuf[0]));
        memcpy(colorbuf_, tmpcolor, tmpchars*sizeof(tmpcolor[0]));
        memcpy(flagbuf_, tmpflags, tmpchars*sizeof(tmpflags[0]));
        linecol_ = linepos_ = tmpchars;
    }
    
done_with_wrapping:
    /* add the new character to buffer */
    buffer_rendered(c, cflags, cwid);
}

/*
 *   Write a rendered character to an output stream buffer.  This is a
 *   low-level internal routine that we call from buffer_expchar() to put
 *   the final rendition of a character into a buffer.
 *   
 *   Some characters render as multiple copies of a single character; 'wid'
 *   gives the number of copies to store.  The caller is responsible for
 *   ensuring that the rendered representation fits in the buffer and in the
 *   available line width.  
 */
void CVmFormatter::buffer_rendered(wchar_t c, unsigned char flags, int wid)
{
    unsigned char flags_before;

    /* note whether or not we have a break before us */
    flags_before = (linepos_ > 0
                    ? flagbuf_[linepos_-1] & VMCON_OBF_NOBREAK
                    : 0);

    /* add the character the given number of times */
    for ( ; wid != 0 ; --wid)
    {
        /* buffer the character */
        linebuf_[linepos_] = c;
        flagbuf_[linepos_] = flags;
        colorbuf_[linepos_] = cur_color_;

        /* 
         *   if this isn't the last part of the character, carry forward any
         *   no-break flag from the previous part of the character; this will
         *   ensure that a no-break to the left of the sequence applies to
         *   the entire sequence 
         */
        if (wid > 1)
            flagbuf_[linepos_] |= flags_before;

        /* advance one character in the buffer */
        ++linepos_;

        /* adjust our column counter */
        ++linecol_;
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   write out a UTF-8 string
 */
void CVmFormatter::buffer_string(VMG_ const char *txt)
{
    /* write out each character in the string */
    for ( ; utf8_ptr::s_getch(txt) != 0 ; txt += utf8_ptr::s_charsize(*txt))
        buffer_char(vmg_ utf8_ptr::s_getch(txt));
}

/*
 *   write out a wide unicode string 
 */
void CVmFormatter::buffer_wstring(VMG_ const wchar_t *txt)
{
    /* write out each wide character */
    for ( ; *txt != L'\0' ; ++txt)
        buffer_char(vmg_ *txt);
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the next wide unicode character in a UTF8-encoded string, and
 *   update the string pointer and remaining length.  Returns zero if no
 *   more characters are available in the string.  
 */
wchar_t CVmFormatter::next_wchar(const char **s, size_t *len)
{
    wchar_t ret;
    size_t charsize;

    /* if there's nothing left, return a null terminator */
    if (*len == 0)
        return 0;

    /* get this character */
    ret = utf8_ptr::s_getch(*s);

    /* advance the string pointer and length counter */
    charsize = utf8_ptr::s_charsize(**s);
    *len -= charsize;
    *s += charsize;

    /* render embedded null bytes as spaces */
    if (ret == 0)
        ret = ' ';

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Display a string of a given length.  The text is encoded as UTF-8
 *   characters.  
 */
int CVmFormatter::format_text(VMG_ const char *s, size_t slen)
{
    /* get the first character */
    wchar_t c = next_wchar(&s, &slen);

    /* if we have anything to show, show it */
    while (c != '\0')
    {
        /* 
         *   first, process the character through our built-in text-only HTML
         *   mini-parser, if our HTML mini-parser state indicates that we're
         *   in the midst of parsing a tag 
         */
        if (html_parse_state_ != VMCON_HPS_NORMAL
            || (html_in_ignore_ && c != '&' && c != '<'))
        {
            /* run our HTML parsing until we finish the tag */
            c = resume_html_parsing(vmg_ c, &s, &slen);

            /* proceed with the next character */
            continue;
        }

        /* check for special characters */
        switch(c)
        {
        case 10:
            /* newline */
            flush(vmg_ VM_NL_NEWLINE);
            break;
                    
        case 9:
            /* tab - write an ordinary every-4-columns tab */
            write_tab(vmg_ 0, 4);
            break;

        case 0x000B:
            /* \b - blank line */
            write_blank_line(vmg0_);
            break;
                    
        case 0x000F:
            /* capitalize next character */
            capsflag_ = TRUE;
            nocapsflag_ = FALSE;
            break;

        case 0x000E:
            /* un-capitalize next character */
            nocapsflag_ = TRUE;
            capsflag_ = FALSE;
            break;

        case '<':
        case '&':
            /* HTML markup-start character - process it */
            if (html_target_ || literal_mode_)
            {
                /* 
                 *   The underlying OS renderer interprets HTML mark-up
                 *   sequences, OR we're processing all text literally; in
                 *   either case, we don't need to perform any
                 *   interpretation.  Simply pass through the character as
                 *   though it were any other.  
                 */
                goto normal_char;
            }
            else
            {
                /*
                 *   The underlying target does not accept HTML sequences.
                 *   It appears we're at the start of an "&" entity or a tag
                 *   sequence, so parse it, remove it, and replace it (if
                 *   possible) with a text-only equivalent.  
                 */
                c = parse_html_markup(vmg_ c, &s, &slen);

                /* go back and process the next character */
                continue;
            }
            break;

        case 0x0015:                      /* our own quoted space character */
        case 0x00A0:                                  /* non-breaking space */
        case 0x00AD:                                         /* soft hyphen */
        case 0xFEFF:                       /* non-breaking zero-width space */
        case 0x2002:                                            /* en space */
        case 0x2003:                                            /* em space */
        case 0x2004:                                  /* three-per-em space */
        case 0x2005:                                   /* four-per-em space */
        case 0x2006:                                    /* six-per-em space */
        case 0x2007:                                        /* figure space */
        case 0x2008:                                   /* punctuation space */
        case 0x2009:                                          /* thin space */
        case 0x200a:                                          /* hair space */
        case 0x200b:                                    /* zero-width space */
            /* 
             *   Special Unicode characters.  For HTML targets, write these
             *   as &# sequences - this bypasses character set translation
             *   and ensures that the HTML parser will see them as intended.
             */
            if (html_target_)
            {
                char buf[15];
                char *p;
                
                /* 
                 *   it's an HTML target - render these as &# sequences;
                 *   generate the decimal representation of 'c' (in reverse
                 *   order, hence start with the terminating null byte and
                 *   the semicolon) 
                 */
                p = buf + sizeof(buf) - 1;
                *p-- = '\0';
                *p-- = ';';

                /* generate the decimal representation of 'c' */
                for ( ; c != 0 ; c /= 10)
                    *p-- = (c % 10) + '0';

                /* add the '&#' sequence */
                *p-- = '#';
                *p = '&';

                /* write out the sequence */
                buffer_string(vmg_ p);
            }
            else
            {
                /* for non-HTML targets, treat these as normal */
                goto normal_char;
            }
            break;

        default:
        normal_char:
            /* normal character - write it out */
            buffer_char(vmg_ c);
            break;
        }

        /* move on to the next character */
        c = next_wchar(&s, &slen);
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the display object 
 */
CVmConsole::CVmConsole()
{
    /* no script file yet */
    script_sp_ = 0;

    /* no command log file yet */
    command_fp_ = 0;
    command_nf_ = 0;
    command_glob_ = 0;

    /* assume we'll double-space after each period */
    doublespace_ = TRUE;

    /* presume we'll have no log stream */
    log_str_ = 0;
    log_enabled_ = FALSE;
}

/*
 *   Delete the display object 
 */
void CVmConsole::delete_obj(VMG0_)
{
    /* close any active script file(s) */
    while (script_sp_ != 0)
        close_script_file(vmg0_);

    /* close any active command log file */
    close_command_log(vmg0_);

    /* delete the log stream if we have one */
    if (log_str_ != 0)
        log_str_->delete_obj(vmg0_);

    /* delete this object */
    delete this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Display a string of a given byte length 
 */
int CVmConsole::format_text(VMG_ const char *p, size_t len)
{
    /* display the string */
    disp_str_->format_text(vmg_ p, len);

    /* if there's a log file, write to the log file as well */
    if (log_enabled_)
        log_str_->format_text(vmg_ p, len);

    /* indicate success */
    return 0;
}

/*
 *   Display a string on the log stream only 
 */
int CVmConsole::format_text_to_log(VMG_ const char *p, size_t len)
{
    /* if there's a log file, write to it; otherwise ignore the whole thing */
    if (log_enabled_)
        log_str_->format_text(vmg_ p, len);

    /* indicate success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the text color 
 */
void CVmConsole::set_text_color(VMG_ os_color_t fg, os_color_t bg)
{
    /* set the color in our main display stream */
    disp_str_->set_text_color(vmg_ fg, bg);
}

/*
 *   Set the body color 
 */
void CVmConsole::set_body_color(VMG_ os_color_t color)
{
    /* set the color in the main display stream */
    disp_str_->set_os_body_color(color);
}

/* ------------------------------------------------------------------------ */
/*
 *   Display a blank line 
 */
void CVmConsole::write_blank_line(VMG0_)
{
    /* generate the newline to the standard display */
    disp_str_->write_blank_line(vmg0_);

    /* if we're logging, generate the newline to the log file as well */
    if (log_enabled_)
        log_str_->write_blank_line(vmg0_);
}


/* ------------------------------------------------------------------------ */
/*
 *   outcaps() - sets an internal flag which makes the next letter output
 *   a capital, whether it came in that way or not.  Set the same state in
 *   both formatters (standard and log).  
 */
void CVmConsole::caps()
{
    disp_str_->caps();
    if (log_enabled_)
        log_str_->caps();
}

/*
 *   outnocaps() - sets the next letter to a miniscule, whether it came in
 *   that way or not.  
 */
void CVmConsole::nocaps()
{
    disp_str_->nocaps();
    if (log_enabled_)
        log_str_->nocaps();
}

/*
 *   obey_whitespace() - sets the obey-whitespace mode 
 */
int CVmConsole::set_obey_whitespace(int f)
{
    int ret;

    /* note the original display stream status */
    ret = disp_str_->get_obey_whitespace();

    /* set the stream status */
    disp_str_->set_obey_whitespace(f);
    if (log_enabled_)
        log_str_->set_obey_whitespace(f);

    /* return the original status of the display stream */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Open a log file 
 */
int CVmConsole::open_log_file(VMG_ const char *fname)
{
    /* pass the file to the log stream, if we have one */
    return log_str_ != 0 ? log_str_->open_log_file(vmg_ fname) : 1;
}

int CVmConsole::open_log_file(VMG_ const vm_val_t *filespec,
                              const struct vm_rcdesc *rc)
{
    /* pass the file to the log stream, if we have one */
    return log_str_ != 0 ? log_str_->open_log_file(vmg_ filespec, rc) : 1;
}

/*
 *   Close the log file 
 */
int CVmConsole::close_log_file(VMG0_)
{
    /* if there's no log stream, there's obviously no file open */
    if (log_str_ == 0)
        return 1;

    /* tell the log stream to close its file */
    return log_str_->close_log_file(vmg0_);
}

#if 0 //$$$
/*
 *   This code is currently unused.  However, I'm leaving it in for now -
 *   the algorithm takes a little thought, so it would be nicer to be able
 *   to uncomment the existing code should we ever need it in the future.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   Write UTF-8 text explicitly to the log file.  This can be used to add
 *   special text (such as prompt text) that would normally be suppressed
 *   from the log file.  When more mode is turned off, we don't
 *   automatically copy text to the log file; any text that the caller
 *   knows should be in the log file during times when more mode is turned
 *   off can be explicitly added with this function.
 *   
 *   If nl is true, we'll add a newline at the end of this text.  The
 *   caller should not include any newlines in the text being displayed
 *   here.  
 */
void CVmConsole::write_to_logfile(VMG_ const char *txt, int nl)
{
    /* if there's no log file, there's nothing to do */
    if (logfp_ == 0)
        return;

    /* write the text in the log file character set */
    write_to_file(logfp_, txt, G_cmap_to_log);

    /* add a newline if desired */
    if (nl)
    {
        /* add a normal newline */
        os_fprintz(logfp_, "\n");

        /* if the logfile is an html target, write an HTML line break */
        if (log_str_ != 0 && log_str_->is_html_target())
            os_fprintz(logfp_, "<BR HEIGHT=0>\n");
    }

    /* flush the output */
    osfflush(logfp_);
}

/*
 *   Write text to a file in the given character set 
 */
void CVmConsole::write_to_file(osfildef *fp, const char *txt,
                               CCharmapToLocal *map)
{
    size_t txtlen = strlen(txt);
    
    /* 
     *   convert the text from UTF-8 to the local character set and write the
     *   converted text to the log file 
     */
    while (txtlen != 0)
    {
        char local_buf[128];
        size_t src_bytes_used;
        size_t out_bytes;
        
        /* convert as much as we can (leaving room for a null terminator) */
        out_bytes = map->map_utf8(local_buf, sizeof(local_buf),
                                  txt, txtlen, &src_bytes_used);

        /* null-terminate the result */
        local_buf[out_bytes] = '\0';

        /* write the converted text */
        os_fprintz(fp, local_buf);

        /* skip the text we were able to convert */
        txt += src_bytes_used;
        txtlen -= src_bytes_used;
    }
}
#endif /* 0 */


/* ------------------------------------------------------------------------ */
/*
 *   Reset the MORE line counter.  This should be called whenever user
 *   input is read, since stopping to read user input makes it unnecessary
 *   to show another MORE prompt until the point at which input was
 *   solicited scrolls off the screen.  
 */
void CVmConsole::reset_line_count(int clearing)
{
    /* reset the MORE counter in the display stream */
    disp_str_->reset_line_count(clearing);
}

/* ------------------------------------------------------------------------ */
/*
 *   Flush the output line.  We'll write to both the standard display and
 *   the log file, as needed.  
 */
void CVmConsole::flush(VMG_ vm_nl_type nl)
{
    /* flush the display stream */
    disp_str_->flush(vmg_ nl);

    /* flush the log stream, if we have an open log file */
    if (log_enabled_)
        log_str_->flush(vmg_ nl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear our buffers
 */
void CVmConsole::empty_buffers(VMG0_)
{
    /* tell the formatter to clear its buffer */
    disp_str_->empty_buffers(vmg0_);

    /* same with the log stream, if applicable */
    if (log_enabled_)
        log_str_->empty_buffers(vmg0_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Immediately update the display 
 */
void CVmConsole::update_display(VMG0_)
{
    /* update the display for the main display stream */
    disp_str_->update_display(vmg0_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Open a script file 
 */
int CVmConsole::open_script_file(VMG_ const char *fname,
                                 int quiet, int script_more_mode)
{
    CVmNetFile *nf = 0;
    err_try
    {
        /* create the network file descriptor */
        nf = CVmNetFile::open(
            vmg_ fname, 0, NETF_READ, OSFTCMD, "text/plain");
    }
    err_catch_disc
    {
        /* failed - no network file */
        nf = 0;
    }
    err_end;

    /* open the script on the network file */
    return open_script_file(vmg_ nf, 0, quiet, script_more_mode);
}

int CVmConsole::open_script_file(VMG_ const vm_val_t *filespec,
                                 const struct vm_rcdesc *rc,
                                 int quiet, int script_more_mode)
{
    CVmNetFile *nf = 0;
    err_try
    {
        /* create the network file descriptor */
        nf = CVmNetFile::open(
            vmg_ filespec, rc, NETF_READ, OSFTCMD, "text/plain");

        /* validate file safety */
        CVmObjFile::check_safety_for_open(vmg_ nf, VMOBJFILE_ACCESS_READ);
    }
    err_catch_disc
    {
        /* if we got a file, it must be a safety exception - rethrow it */
        if (nf != 0)
        {
            nf->abandon(vmg0_);
            err_rethrow();
        }
    }
    err_end;

    /* open the script on the network file */
    return open_script_file(vmg_ nf, filespec, quiet, script_more_mode);
}

int CVmConsole::open_script_file(VMG_ CVmNetFile *nf, const vm_val_t *filespec,
                                 int quiet, int script_more_mode)
{
    int evt;
    char buf[50];
    
    /* if the network file open failed, return an error */
    if (nf == 0)
        return 1;

    /* open the local file */
    osfildef *fp = osfoprt(nf->lclfname, OSFTCMD);

    /* if that failed, silently ignore the request */
    if (fp == 0)
    {
        /* abandon the network file descriptor and return failure */
        nf->abandon(vmg0_);
        return 1;
    }

    /* read the first line to see if it looks like an event script */
    if (osfgets(buf, sizeof(buf), fp) != 0
        && strcmp(buf, "<eventscript>\n") == 0)
    {
        /* remember that it's an event script */
        evt = TRUE;
    }
    else
    {
        /* 
         *   it's not an event script, so it must be a regular command-line
         *   script - rewind it so we read the first line again as a regular
         *   input line 
         */
        evt = FALSE;
        osfseek(fp, 0, OSFSK_SET);
    }

    /* if there's an enclosing script, inherit its modes */
    if (script_sp_ != 0)
    {
        /* 
         *   if the enclosing script is quiet, force the nested script to be
         *   quiet as well 
         */
        if (script_sp_->quiet)
            quiet = TRUE;

        /* 
         *   if the enclosing script is nonstop, force the nested script to
         *   be nonstop as well
         */
        if (!script_sp_->more_mode)
            script_more_mode = FALSE;
    }

    /* push the new script file onto the stack */
    script_sp_ = new script_stack_entry(
        vmg_ script_sp_, set_more_state(script_more_mode), nf, fp, filespec,
        script_more_mode, quiet, evt);
    
    /* turn on NONSTOP mode in the OS layer if applicable */
    if (!script_more_mode)
        os_nonstop_mode(TRUE);

    /* success */
    return 0;
}

/*
 *   Close the current script file 
 */
int CVmConsole::close_script_file(VMG0_)
{
    script_stack_entry *e;
    
    /* if we have a file, close it */
    if ((e = script_sp_) != 0)
    {
        int ret;
        
        /* close the file */
        osfcls(e->fp);

        err_try
        {
            /* close the network file */
            e->netfile->close(vmg0_);
        }
        err_catch_disc
        {
            /* 
             *   Ignore any error - since we're reading the file, the chances
             *   of anything going wrong are pretty minimal anyway; but even
             *   if an error did occur, our interface just isn't set up to do
             *   anything meaningful with it.  
             */
        }
        err_end;

        /* pop the stack */
        script_sp_ = e->enc;

        /* restore the enclosing level's MORE mode */
        os_nonstop_mode(!e->old_more_mode);

        /* 
         *   return the MORE mode in effect before we started reading the
         *   script file 
         */
        ret = e->old_more_mode;

        /* delete the stack level */
        e->delobj(vmg0_);

        /* return the result */
        return ret;
    }
    else
    {
        /* 
         *   there's no script file - just return the current MORE mode,
         *   since we're not making any changes 
         */
        return is_more_mode();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Script stack element 
 */

script_stack_entry::script_stack_entry(
    VMG_ script_stack_entry *encp, int old_more,
    class CVmNetFile *netfile, osfildef *outfp, const vm_val_t *filespec,
    int new_more, int is_quiet, int is_event_script)
{
    /* remember the stack level settings */
    this->enc = encp;
    this->old_more_mode = old_more;
    this->netfile = netfile;
    this->fp = outfp;
    this->more_mode = new_more;
    this->quiet = is_quiet;
    this->event_script = is_event_script;

    /* if we have a file spec, create a global for it for gc protection */
    this->filespec = 0;
    if (filespec != 0)
    {
        this->filespec = G_obj_table->create_global_var();
        this->filespec->val = *filespec;
    }
}

void script_stack_entry::delobj(VMG0_)
{
    /* we're done with our file spec global, if we had one */
    if (filespec != 0)
        G_obj_table->delete_global_var(filespec);

    /* delete myself */
    delete this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Open a command log file 
 */
int CVmConsole::open_command_log(VMG_ const char *fname, int event_script)
{
    CVmNetFile *nf = 0;
    err_try
    {
        /* create the network file descriptor */
        nf = CVmNetFile::open(vmg_ fname, 0, NETF_NEW, OSFTCMD, "text/plain");
    }
    err_catch_disc
    {
        /* failed - no network file */
        nf = 0;
    }
    err_end;

    /* create the command log */
    return open_command_log(vmg_ nf, event_script);
}

int CVmConsole::open_command_log(VMG_ const vm_val_t *filespec,
                                 const struct vm_rcdesc *rc,
                                 int event_script)
{
    CVmNetFile *nf = 0;
    err_try
    {
        /* create the network file descriptor */
        nf = CVmNetFile::open(vmg_ filespec, rc,
                              NETF_NEW, OSFTCMD, "text/plain");

        /* validate file safety */
        CVmObjFile::check_safety_for_open(vmg_ nf, VMOBJFILE_ACCESS_WRITE);
    }
    err_catch_disc
    {
        /* if we got a file, it must be a safety exception - rethrow it */
        if (nf != 0)
        {
            nf->abandon(vmg0_);
            err_rethrow();
        }
    }
    err_end;

    /* create the command log */
    int err = open_command_log(vmg_ nf, event_script);

    /* if that succeeded, save the filespec in our VM global */
    if (!err)
    {
        /* create our VM global if we don't have one already */
        if (command_glob_ == 0)
            command_glob_ = G_obj_table->create_global_var();

        /* remember the file spec */
        command_glob_->val = *filespec;
    }

    /* return the result */
    return err;
}

int CVmConsole::open_command_log(VMG_ CVmNetFile *nf, int event_script)
{
    /* close any existing command log file */
    close_command_log(vmg0_);

    /* if the network file open failed, return failure */
    if (nf == 0)
        return 1;
    
    /* open the file */
    osfildef *fp = osfopwt(nf->lclfname, OSFTCMD);
    if (fp != 0)
    {
        /* success - remember the file handle and net descriptor */
        command_nf_ = nf;
        command_fp_ = new CVmFileSource(fp);
    }
    else
    {
        /* failed - abandon the network file */
        nf->abandon(vmg0_);
    }

    /* note the type */
    command_eventscript_ = event_script;

    /* if it's an event script, write the file type tag */
    if (event_script && command_fp_ != 0)
    {
        command_fp_->writez("<eventscript>\n");
        command_fp_->flush();
    }

    /* return success if we successfully opened the file */
    return (command_fp_ == 0);
}

/* 
 *   close the active command log file 
 */
int CVmConsole::close_command_log(VMG0_)
{
    /* if there's a command log file, close it */
    if (command_fp_ != 0)
    {
        /* close the file */
        delete command_fp_;

        /* close the network file */
        err_try
        {
            command_nf_->close(vmg0_);
        }
        err_catch_disc
        {
            /* 
             *   ignore any errors - our interface doesn't give us any
             *   meaningful way to return error information
             */
        }
        err_end;

        /* forget the file */
        command_fp_ = 0;
    }

    /* clear out our file spec global, if we have one */
    if (command_glob_ != 0)
        command_glob_->val.set_nil();

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Read a line of input from the console.  Fills in the buffer with a
 *   null-terminated string in the UTF-8 character set.  Returns zero on
 *   success, non-zero on end-of-file.  
 */
int CVmConsole::read_line(VMG_ char *buf, size_t buflen, int bypass_script)
{
    /* cancel any previous interrupted input */
    read_line_cancel(vmg_ TRUE);

try_again:
    /* use the timeout version, with no timeout specified */
    switch(read_line_timeout(vmg_ buf, buflen, 0, FALSE, bypass_script))
    {
    case OS_EVT_LINE:
        /* success */
        return 0;

    case VMCON_EVT_END_QUIET_SCRIPT:
        /* 
         *   end of script - we have no way to communicate this result back
         *   to our caller, so simply ignore the result and ask for another
         *   line 
         */
        goto try_again;

    default:
        /* anything else is an error */
        return 1;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   translate an OS_EVT_XXX code to an event script file tag 
 */
static const char *evt_to_tag(int evt)
{
    switch (evt)
    {
    case OS_EVT_KEY:
        return "key";

    case OS_EVT_TIMEOUT:
        return "timeout";

    case OS_EVT_HREF:
        return "href";

    case OS_EVT_NOTIMEOUT:
        return "notimeout";

    case OS_EVT_EOF:
        return "eof";

    case OS_EVT_LINE:
        return "line";

    case OS_EVT_COMMAND:
        return "command";

    case VMCON_EVT_END_QUIET_SCRIPT:
        return "endqs";

    case VMCON_EVT_DIALOG:
        return "dialog";

    case VMCON_EVT_FILE:
        return "file";

    default:
        return "";
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Log an event to the output script.  The parameter is in the UI character
 *   set.  
 */
int CVmConsole::log_event(VMG_ int evt,
                          const char *param, size_t paramlen,
                          int param_is_utf8)
{
    /* translate the event code to a tag, and log the event */
    log_event(vmg_ evt_to_tag(evt), param, paramlen, param_is_utf8);

    /* return the event type code */
    return evt;
}

void CVmConsole::log_event(VMG_ const char *tag,
                           const char *param, size_t paramlen,
                           int param_is_utf8)
{
    /* if there's a script file, log the event */
    if (command_fp_ != 0)
    {
        /* if the tag has < > delimiters, remove them */
        size_t taglen = strlen(tag);
        if (tag[0] == '<')
            ++tag, --taglen;
        if (taglen != 0 && tag[taglen-1] == '>')
            --taglen;

        /* write the event in the proper format for the script type */
        if (command_eventscript_)
        {
            /* 
             *   It's an event script, so we write all event types.  Check
             *   for certain special parameter translations.  
             */
            if (taglen == 3 && memicmp(tag, "key", 3) == 0)
            {
                /* 
                 *   key event - for characters that would look like
                 *   whitespace characters if we wrote them as-is, translate
                 *   to [xxx] key names 
                 */
                if (param != 0 && paramlen == 1)
                {
                    switch (param[0])
                    {
                    case '\n':
                        param = "[enter]";
                        paramlen = 7;
                        break;

                    case '\t':
                        param = "[tab]";
                        paramlen = 5;
                        break;

                    case ' ':
                        param = "[space]";
                        paramlen = 7;
                        break;
                    }
                }
            }

            /* 
             *   if the event doesn't have parameters, ignore any parameter
             *   provided by the caller 
             */
            if ((taglen == 9 && memicmp(tag, "notimeout", 9) == 0)
                || (taglen == 3 && memicmp(tag, "eof", 3) == 0))
                param = 0;

            /* if we have a non-empty tag, write the event */
            if (taglen != 0)
            {
                /* write the tag, in the local character set */
                G_cmap_to_ui->write_file(command_fp_, "<", 1);
                G_cmap_to_ui->write_file(command_fp_, tag, strlen(tag));
                G_cmap_to_ui->write_file(command_fp_, ">", 1);
                
                /* add the parameter, if present */
                if (param != 0)
                {
                    if (param_is_utf8)
                        G_cmap_to_ui->write_file(command_fp_, param, paramlen);
                    else
                        command_fp_->write(param, paramlen);
                }
                
                /* add the newline */
                G_cmap_to_ui->write_file(command_fp_, "\n", 1);
                
                /* flush the output */
                command_fp_->flush();
            }
        }
        else
        {
            /*
             *   It's a plain old command-line script.  If the event is an
             *   input-line event, record it; otherwise leave it out, as this
             *   script file format can't represent any other event types.  
             */
            if (taglen == 4 && memicmp(tag, "line", 4) == 0 && param != 0)
            {
                /* write the ">" prefix */
                G_cmap_to_ui->write_file(command_fp_, ">", 1);

                /* add the command line */
                if (param_is_utf8)
                    G_cmap_to_ui->write_file(command_fp_, param, paramlen);
                else
                    command_fp_->write(param, paramlen);

                /* add the newline */
                G_cmap_to_ui->write_file(command_fp_, "\n", 1);

                /* flush the output */
                command_fp_->flush();
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Static variables for input state.  We keep these statically, because we
 *   might need to use the values across a series of read_line_timeout calls
 *   if timeouts occur. 
 */

/* original 'more' mode, before input began */
static int S_old_more_mode;

/* flag: input is pending from an interrupted read_line_timeout invocation */
static int S_read_in_progress;

/* local buffer for reading input lines */
static char S_read_buf[256];


/*
 *   Read a line of input from the console, with an optional timeout value. 
 */
int CVmConsole::read_line_timeout(VMG_ char *buf, size_t buflen,
                                  unsigned long timeout, int use_timeout,
                                  int bypass_script)
{
    /* no event yet */
    int evt = OS_EVT_NONE;

    /* we haven't received any script input yet */
    int got_script_input = FALSE;

    /* 
     *   presume we won't echo the text to the display; in most cases, it
     *   will be echoed to the display in the course of reading it from
     *   the keyboard 
     */
    int echo_text = FALSE;

    /* remember the initial MORE mode */
    S_old_more_mode = is_more_mode();

    /*
     *   If we're not resuming an interrupted read already in progress,
     *   initialize some display settings. 
     */
    if (!S_read_in_progress)
    {
        /* 
         *   Turn off MORE mode if it's on - we don't want a MORE prompt
         *   showing up in the midst of user input.  
         */
        S_old_more_mode = set_more_state(FALSE);

        /* 
         *   flush the output; don't start a new line, since we might have
         *   displayed a prompt that is to be on the same line with the user
         *   input 
         */
        flush_all(vmg_ VM_NL_INPUT);
    }

    /* 
     *   if there's a script file, read from it, unless the caller has
     *   specifically asked us to bypass it 
     */
    if (script_sp_ != 0 && !bypass_script)
    {
    read_script:
        /* note whether we're in quiet mode */
        int was_quiet = script_sp_->quiet;
        
        /* try reading a line from the script file */
        if (read_line_from_script(S_read_buf, sizeof(S_read_buf), &evt))
        {
            /* we successfully read input from the script */
            got_script_input = TRUE;

            /*
             *   if we're not in quiet mode, make a note to echo the text to
             *   the display 
             */
            if (!script_sp_->quiet)
                echo_text = TRUE;
        }
        else
        {
            /* 
             *   End of script file - return to reading from the enclosing
             *   level (i.e., the enclosing script, or the keyboard if this
             *   is the outermost script).  The return value from
             *   close_script_file() is the MORE mode that was in effect
             *   before we started reading the script file; we'll use this
             *   when we restore the enclosing MORE mode so that we restore
             *   the pre-script MORE mode when we return.  
             */
            S_old_more_mode = close_script_file(vmg0_);
            
            /* note the new 'quiet' mode */
            int is_quiet = (script_sp_ != 0 && script_sp_->quiet);
            
            /* 
             *   if we're still reading from a script (which means we closed
             *   the old script and popped out to an enclosing script), and
             *   the 'quiet' mode hasn't changed, simply go back for another
             *   read 
             */
            if (script_sp_ != 0 && is_quiet == was_quiet)
                goto read_script;
            
            /* 
             *   temporarily turn off MORE mode, in case we read from the
             *   keyboard 
             */
            set_more_state(FALSE);
            
            /* flush any output we generated while reading the script */
            flush(vmg_ VM_NL_NONE);
            
            /* 
             *   If we were in quiet mode but no longer are, let the caller
             *   know we've finished reading a script, so that the caller can
             *   set up the display properly for reading from the keyboard.
             *   
             *   If we weren't in quiet mode, we'll simply proceed to the
             *   normal keyboard reading; when not in quiet mode, no special
             *   display fixup is needed.  
             */
            if (was_quiet && !is_quiet)
            {
                /* return to the old MORE mode */
                set_more_state(S_old_more_mode);
                
                /* add a blank line to the log file, if necessary */
                if (log_enabled_)
                    log_str_->print_to_os("\n");
                
                /* note in the streams that we've read an input line */
                disp_str_->note_input_line();
                if (log_str_ != 0)
                    log_str_->note_input_line();
                
                /* 
                 *   generate a synthetic "end of script" event to let the
                 *   caller know we're switching back to regular keyboard
                 *   reading 
                 */
                return log_event(vmg_ VMCON_EVT_END_QUIET_SCRIPT);
            }

            /*
             *   Note that we do not have an event yet - we've merely closed
             *   the script file, and now we're going to continue by reading
             *   a line from the keyboard instead.  The call to
             *   close_script_file() above will have left script_sp_ == 0, so
             *   we'll shortly read an event from the keyboard.  Thus 'evt'
             *   is still not set to any value, because we do not yet have an
             *   event - this is intentional.  
             */
        }
    }

    /* 
     *   if we're not reading from a scripot, reset the MORE line counter,
     *   since we're reading user input at the current point and shouldn't
     *   pause for a MORE prompt until the text we're reading has scrolled
     *   off the screen 
     */
    if (script_sp_ == 0)
        reset_line_count(FALSE);
    
    /* reading is now in progress */
    S_read_in_progress = TRUE;

    /* if we didn't get input from a script, read from the keyboard */
    if (!got_script_input)
    {
        /* 
         *   If we're in network mode, return EOF to indicate that no console
         *   input is available.  Network programs can only call this routine
         *   to read script input, and aren't allowed to read from the
         *   regular keyboard.  
         */
        if (G_net_config != 0)
        {
            read_line_done(vmg0_);
            return OS_EVT_EOF;
        }

        /* read a line from the keyboard */
        evt = os_gets_timeout((uchar *)S_read_buf, sizeof(S_read_buf),
                              timeout, use_timeout);

        /*
         *   If that failed because timeout is not supported on this
         *   platform, and the caller didn't actually want to use a timeout,
         *   try again with an ordinary os_gets().  If they wanted to use a
         *   timeout, simply return the NOTIMEOUT indication to our caller.  
         */
        if (evt == OS_EVT_NOTIMEOUT && !use_timeout)
        {
            /* perform an ordinary untimed input */
            if (os_gets((uchar *)S_read_buf, sizeof(S_read_buf)) != 0)
            {
                /* success */
                evt = OS_EVT_LINE;
            }
            else
            {
                /* error reading input */
                evt = OS_EVT_EOF;
            }
        }

        /* 
         *   If we actually read a line, notify the display stream that we
         *   read text from the console - it might need to make some
         *   internal bookkeeping adjustments to account for the fact that
         *   we moved the write position around on the display.
         *   
         *   Don't note the input if we timed out, since we haven't finished
         *   reading the line yet in this case.  
         */
        if (evt == OS_EVT_LINE)
        {
            disp_str_->note_input_line();
            if (log_str_ != 0)
                log_str_->note_input_line();
        }
    }

    /* if we got an error, return it */
    if (evt == OS_EVT_EOF)
    {
        set_more_state(S_old_more_mode);
        read_line_done(vmg0_);
        return log_event(vmg_ evt);
    }

    /* 
     *   Convert the text from the local UI character set to UTF-8.  Reserve
     *   space in the output buffer for the null terminator.  
     */
    char *outp = buf;
    size_t outlen = buflen - 1;
    G_cmap_from_ui->map(&outp, &outlen, S_read_buf, strlen(S_read_buf));

    /* add the null terminator */
    *outp = '\0';

    /* 
     *   If we need to echo the text (because we read it from a script file),
     *   do so now.  Never echo text in the network configuration, since we
     *   don't use the local console UI in this mode.  
     */
    if (echo_text && G_net_config == 0)
    {
        /* show the text */
        format_text(vmg_ buf);

        /* add a newline */
        format_text(vmg_ "\n");
    }

    /* if we finished reading the line, do our line-finishing work */
    if (evt == OS_EVT_LINE)
        read_line_done(vmg0_);

    /* 
     *   Log and return the event.  Note that we log events in the UI
     *   character set, so we want to simply use the original, untranslated
     *   input buffer. 
     */
    return log_event(vmg_ evt, S_read_buf, strlen(S_read_buf), FALSE);
}

/*
 *   Cancel an interrupted input. 
 */
void CVmConsole::read_line_cancel(VMG_ int reset)
{
    /* reset the underling OS layer */
    os_gets_cancel(reset);

    /* do our line-ending work */
    read_line_done(vmg0_);
}

/*
 *   Perform line-ending work.  This is used when we finish reading a line
 *   in read_line_timeout(), or when we cancel an interrupted line, thus
 *   finishing the line, in read_line_cancel(). 
 */
void CVmConsole::read_line_done(VMG0_)
{
    /* if we have a line in progress, finish it off */
    if (S_read_in_progress)
    {
        /* set the original 'more' mode */
        set_more_state(S_old_more_mode);

        /* 
         *   Write the input line, followed by a newline, to the log file.
         *   Note that the text is still in the local character set, so we
         *   can write it directly to the log file.
         *   
         *   If we're reading from a script file in "echo" mode, skip this.
         *   When reading from a script file in "echo" mode, we will manually
         *   copy the input commands to the main console, which will
         *   automatically copy to the main log file.  If we're in quiet
         *   scripting mode, though, we won't do that, so we do need to
         *   capture the input explicitly here.  
         */
        if (log_enabled_ && (script_sp_ == 0 || script_sp_->quiet))
        {
            log_str_->print_to_os(S_read_buf);
            log_str_->print_to_os("\n");
        }
        
        /* note in the streams that we've read an input line */
        disp_str_->note_input_line();
        if (log_str_ != 0)
            log_str_->note_input_line();

        /* clear the in-progress flag */
        S_read_in_progress = FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Read an input event from the script file.  If we're reading an event
 *   script file, we'll read the next event and return TRUE; if we're not
 *   reading a script file, or the script file is a command-line script
 *   rather than an event script, we'll simply return FALSE.
 *   
 *   If the event takes a parameter, we'll read the parameter into 'buf'.
 *   The value is returned in the local character set, so the caller will
 *   need to translate it to UTF-8.
 *   
 *   If 'filter' is non-null, we'll only return events of the types in the
 *   filter list.  
 */
int CVmConsole::read_event_script(VMG_ int *evt, char *buf, size_t buflen,
                                  const int *filter, int filter_cnt,
                                  unsigned long *attrs)
{
    /* 
     *   if we're not reading a script, or it's not an event script, skip
     *   this 
     */
    if (script_sp_ == 0 || !script_sp_->event_script)
        return FALSE;

    /* get the script file */
    osfildef *fp = script_sp_->fp;

    /* keep going until we find something */
    for (;;)
    {
        /* read the next event */
        if (!read_script_event_type(evt, attrs))
        {
            /* end of the script - close it */
            set_more_state(close_script_file(vmg0_));

            /* if there's no more script file, there's no event */
            if (script_sp_ == 0)
                return FALSE;

            /* go back for the next event */
            fp = script_sp_->fp;
            continue;
        }

        /* if it's not in the filter list, skip it */
        if (filter != 0)
        {
            int i, found;

            /* look for a match in our filter list */
            for (i = 0, found = FALSE ; i < filter_cnt ; ++i)
            {
                if (filter[i] == *evt)
                {
                    found = TRUE;
                    break;
                }
            }

            /* if we didn't find it, skip this line */
            if (!found)
            {
                skip_script_line(fp);
                continue;
            }
        }

        /* if there's a buffer, read the rest of the line */
        if (buf != 0)
        {
            /* read the parameter into the buffer, and return the result */
            if (!read_script_param(buf, buflen, fp))
                return FALSE;

            /* if this is an OS_EVT_KEY event, translate special keys */
            if (*evt == OS_EVT_KEY)
            {
                if (strcmp(buf, "[enter]") == 0)
                    strcpy(buf, "\n");
                else if (strcmp(buf, "[tab]") == 0)
                    strcpy(buf, "\t");
                else if (strcmp(buf, "[space]") == 0)
                    strcpy(buf, " ");
            }
        }
        else
        {
            /* no result buffer - just skip anything left on the line */
            skip_script_line(fp);
        }

        /* success */
        return TRUE;
    }
}

/*
 *   Read a <tag> or attribute token from a script file.  Returns the
 *   character after the end of the token.  
 */
static int read_script_token(char *buf, size_t buflen, osfildef *fp)
{
    char *p;
    int c;

    /* skip leading whitespace */
    for (c = osfgetc(fp) ; isspace(c) ; c = osfgetc(fp)) ;

    /* read from the file until we reach the end of the token */
    for (p = buf ;
         p < buf + buflen - 1
             && c != '>' && !isspace(c)
             && c != '\n' && c != '\r' && c != EOF ; )
    {
        /* store this character */
        *p++ = (char)c;

        /* get the next one */
        c = osfgetc(fp);
    }

    /* null-terminate the token */
    *p = '\0';

    /* return the character that ended the token */
    return c;
}

/*
 *   Read the next event type from current event script file.  This leaves
 *   the file positioned at the parameter data for the event, if any.
 *   Returns FALSE if we reach end of file without finding an event.  
 */
int CVmConsole::read_script_event_type(int *evt, unsigned long *attrs)
{
    /* clear the caller's attribute flags, if provided */
    if (attrs != 0)
        *attrs = 0;

    /* if there's no script, there's no event */
    if (script_sp_ == 0)
        return FALSE;

    /* get the file */
    osfildef *fp = script_sp_->fp;

    /* if it's a command-line script, there are only line input events */
    if (!script_sp_->event_script)
    {
        /* keep going until we find an input line */
        for (;;)
        {
            /* read the first character of the line */
            int c = osfgetc(fp);
            if (c == '>')
            {
                /* we found a line input event */
                *evt = OS_EVT_LINE;
                return TRUE;
            }
            else if (c == EOF)
            {
                /* end of file - give up */
                return FALSE;
            }
            else
            {
                /* 
                 *   anything else is just a comment line - just skip it and
                 *   keep looking 
                 */
                skip_script_line(fp);
            }
        }
    }

    /* keep going until we find an event tag */
    for (;;)
    {
        int c;
        char tag[32];
        static const struct
        {
            const char *tag;
            int evt;
        }
        *tp, tags[] =
        {
            { "key", OS_EVT_KEY },
            { "timeout", OS_EVT_TIMEOUT },
            { "notimeout", OS_EVT_NOTIMEOUT },
            { "eof", OS_EVT_EOF },
            { "line", OS_EVT_LINE },
            { "command", OS_EVT_COMMAND },
            
            { "endqs", VMCON_EVT_END_QUIET_SCRIPT },
            { "dialog", VMCON_EVT_DIALOG },
            { "file", VMCON_EVT_FILE },

            { 0, 0 }
        };

        /* check the start of the line to make sure it's an event */
        c = osfgetc(fp);

        /* if at EOF, return failure */
        if (c == EOF)
            return FALSE;

        /* if it's not an event code line, skip the line */
        if (c != '<')
        {
            skip_script_line(fp);
            continue;
        }

        /* read the event type (up to the '>') */
        c = read_script_token(tag, sizeof(tag), fp);

        /* check for attributes */
        while (isspace(c))
        {
            char attr[32];
            static const struct
            {
                const char *name;
                unsigned long flag;
            }
            *ap, attrlist[] =
            {
                { "overwrite", VMCON_EVTATTR_OVERWRITE },
                { 0, 0 }
            };

            /* read the attribute name */
            c = read_script_token(attr, sizeof(attr), fp);

            /* if the name is empty, stop */
            if (attr[0] == '\0')
                break;

            /* look up the token */
            for (ap = attrlist ; ap->name != 0 ; ++ap)
            {
                /* check for a match */
                if (stricmp(attr, ap->name) == 0)
                {
                    /* if the caller wants the flag, set it */
                    if (attrs != 0)
                        *attrs |= ap->flag;

                    /* no need to look any further */
                    break;
                }
            }

            /* if we're at the '>' or at end of line or file, stop */
            if (c == '>' || c == '\n' || c == '\r' || c == EOF)
                break;
        }

        /* if it's not a well-formed tag, ignore it */
        if (c != '>')
        {
            skip_script_line(fp);
            continue;
        }

        /* look up the tag */
        for (tp = tags ; tp->tag != 0 ; ++tp)
        {
            /* check for a match to this tag name */
            if (stricmp(tp->tag, tag) == 0)
            {
                /* got it - return the event type */
                *evt = tp->evt;
                return TRUE;
            }
        }

        /* we don't recognize the tag name; skip the line and keep looking */
        skip_script_line(fp);
    }
}

/*
 *   Skip to the next script line 
 */
void CVmConsole::skip_script_line(osfildef *fp)
{
    int c;

    /* read until we find the end of the current line */
    for (c = osfgetc(fp) ; c != EOF && c != '\n' && c != '\r' ;
         c = osfgetc(fp)) ;
}

/*
 *   read the rest of the current script line into the given buffer 
 */
int CVmConsole::read_script_param(char *buf, size_t buflen, osfildef *fp)
{
    /* ignore zero-size buffer requests */
    if (buflen == 0)
        return FALSE;

    /* read characters until we run out of buffer or reach a newline */
    for (;;)
    {
        /* get the next character */
        int c = osfgetc(fp);

        /* if it's a newline or end of file, we're done */
        if (c == '\n' || c == EOF)
        {
            /* null-terminate the buffer */
            *buf = '\0';

            /* indicate success */
            return TRUE;
        }

        /* 
         *   if there's room in the buffer, add the character - always leave
         *   one byte for the null terminator 
         */
        if (buflen > 1)
        {
            *buf++ = (char)c;
            --buflen;
        }
    }
}


/*
 *   Read a line of text from the script file, if there is one.  Returns TRUE
 *   on success, FALSE if we reach the end of the script file or encounter
 *   any other error.  
 */
int CVmConsole::read_line_from_script(char *buf, size_t buflen, int *evt)
{
    /* if there's no script file, return failure */
    if (script_sp_ == 0)
        return FALSE;

    /* get the file from the script stack */
    osfildef *fp = script_sp_->fp;

    /* keep going until we find a line that we like */
    for (;;)
    {
        /* read the script according to its type ('event' or 'line input') */
        if (script_sp_->event_script)
        {
            /* read to the next event */
            if (!read_script_event_type(evt, 0))
                return FALSE;

            /* check the event code */
            switch (*evt)
            {
            case OS_EVT_LINE:
            case OS_EVT_TIMEOUT:
                /* 
                 *   it's one of our line input events - read the line (or
                 *   partial line, in the case of TIMEOUT) 
                 */
                return read_script_param(buf, buflen, fp);

            default:
                /* 
                 *   it's not our type of event - skip the rest of the line
                 *   and keep looking 
                 */
                skip_script_line(fp);
                break;
            }
        }
        else
        {
            /* 
             *   We have a basic line-input script rather than an event
             *   script.  Each input line starts with a '>'; everything else
             *   is a comment.
             *   
             *   Read the first character on the line.  If it's not a
             *   newline, there's more text on the same line, so read the
             *   rest and determine what to do.  
             */
            int c = osfgetc(fp);
            if (c == '>')
            {
                /* it's a command line - read it */
                *evt = OS_EVT_LINE;
                return read_script_param(buf, buflen, fp);
            }
            else if (c == EOF)
            {
                /* end of file */
                return FALSE;
            }
            else if (c == '\n' || c == '\r')
            {
                /* blank line - continue on to the next line */
            }
            else
            {
                /* it's not a command line - just skip it and keep looking */
                skip_script_line(fp);
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Main System Console 
 */

/*
 *   create 
 */
CVmConsoleMain::CVmConsoleMain(VMG0_)
{
    /* create the system banner manager */
    banner_manager_ = new CVmBannerManager();

    /* create the log console manager */
    log_console_manager_ = new CVmLogConsoleManager();

    /* create and initialize our display stream */
    main_disp_str_ = new CVmFormatterMain(this, 256);
    main_disp_str_->init();

    /* initially send text to the main display stream */
    disp_str_ = main_disp_str_;

    /* 
     *   Create our log stream.  The main console always has a log stream,
     *   even when it's not in use, so that we can keep the log stream's
     *   state synchronized with the display stream in preparation for
     *   activation.  
     */
    log_str_ = new CVmFormatterLog(this, 80);

    /* 
     *   use the default log file character mapper - on some systems, files
     *   don't use the same character set as the display 
     */
    log_str_->set_charmap(G_cmap_to_log);

    /* initialize the log stream */
    log_str_->init();

    /* 
     *   the log stream is initially enabled (this is separate from the log
     *   file being opened; it merely indicates that we send output
     *   operations to the log stream for processing) 
     */
    log_enabled_ = TRUE;

    /* we don't have a statusline formatter until asked for one */
    statline_str_ = 0;

    /* reset statics */
    S_read_in_progress = FALSE;
    S_read_buf[0] = '\0';
}

/*
 *   delete 
 */
void CVmConsoleMain::delete_obj(VMG0_)
{
    /* delete the system banner manager */
    banner_manager_->delete_obj(vmg0_);

    /* delete the system log console manager */
    log_console_manager_->delete_obj(vmg0_);

    /* delete the display stream */
    main_disp_str_->delete_obj(vmg0_);

    /* delete the statusline stream, if we have one */
    if (statline_str_ != 0)
        statline_str_->delete_obj(vmg0_);

    /* do the inherited work */
    CVmConsole::delete_obj(vmg0_);
}

/*
 *   Clear the window 
 */
void CVmConsoleMain::clear_window(VMG0_)
{
    /* flush and empty our output buffer */
    flush(vmg_ VM_NL_NONE);
    empty_buffers(vmg0_);
    
    /* clear the main window */
    oscls();

    /* reset the MORE line counter in the display stream */
    disp_str_->reset_line_count(TRUE);
}

/*
 *   Set statusline mode 
 */
void CVmConsoleMain::set_statusline_mode(VMG_ int mode)
{
    CVmFormatterDisp *str;

    /* 
     *   if we're switching into statusline mode, and we don't have a
     *   statusline stream yet, create one 
     */
    if (mode && statline_str_ == 0)
    {
        /* create and initialize the statusline stream */
        statline_str_ = new CVmFormatterStatline(this);
        statline_str_->init();
    }

    /* get the stream selected by the new mode */
    if (mode)
        str = statline_str_;
    else
        str = main_disp_str_;

    /* if this is already the active stream, we have nothing more to do */
    if (str == disp_str_)
        return;

    /* make the new stream current */
    disp_str_ = str;

    /* 
     *   check which mode we're switching to, so we can do some extra work
     *   specific to each mode 
     */
    if (mode)
    {
        /* 
         *   we're switching to the status line, so disable the log stream -
         *   statusline text is never sent to the log, since the log reflects
         *   only what was displayed in the main text area 
         */
        log_enabled_ = FALSE;
    }
    else
    {
        /*
         *   we're switching back to the main stream, so flush the statusline
         *   so we're sure the statusline text is displayed 
         */

        /* end the line */
        statline_str_->format_text(vmg_ "\n", 1);

        /* flush output */
        statline_str_->flush(vmg_ VM_NL_NONE);

        /* re-enable the log stream, if we have one */
        if (log_str_ != 0)
            log_enabled_ = TRUE;
    }

    /* switch at the OS layer */
    os_status(mode);
}

/*
 *   Flush everything 
 */
void CVmConsoleMain::flush_all(VMG_ vm_nl_type nl)
{
    /* flush our primary console */
    flush(vmg_ nl);

    /* 
     *   Flush each banner we're controlling.  Note that we explicitly flush
     *   the banners with newline mode 'NONE', regardless of the newline mode
     *   passed in by the caller: the caller's mode is for the primary
     *   console, but for the banners we just want to make sure they're
     *   flushed out normally, since whatever we're doing in the primary
     *   console that requires flushing doesn't concern the banners. 
     */
    banner_manager_->flush_all(vmg_ VM_NL_NONE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Handle manager 
 */

/* initialize */
CVmHandleManager::CVmHandleManager()
{
    size_t i;

    /* allocate an initial array of handle slots */
    handles_max_ = 32;
    handles_ = (void **)t3malloc(handles_max_ * sizeof(*handles_));

    /* all slots are initially empty */
    for (i = 0 ; i < handles_max_ ; ++i)
        handles_[i] = 0;
}

/* delete the object - this is the public destructor interface */
void CVmHandleManager::delete_obj(VMG0_)
{
    size_t i;

    /* 
     *   Delete each remaining object.  Note that we need to call the virtual
     *   delete_handle_object routine, so we must do this before reaching the
     *   destructor (once in the base class destructor, we no longer have
     *   access to the subclass virtuals).  
     */
    for (i = 0 ; i < handles_max_ ; ++i)
    {
        /* if this banner is still valid, delete it */
        if (handles_[i] != 0)
            delete_handle_object(vmg_ i + 1, handles_[i]);
    }

    /* delete the object */
    delete this;
}

/* destructor */
CVmHandleManager::~CVmHandleManager()
{
    /* delete the handle pointer array */
    t3free(handles_);
}

/* 
 *   Allocate a new handle 
 */
int CVmHandleManager::alloc_handle(void *item)
{
    size_t slot;

    /* scan for a free slot */
    for (slot = 0 ; slot < handles_max_ ; ++slot)
    {
        /* if this one is free, use it */
        if (handles_[slot] == 0)
            break;
    }

    /* if we didn't find a free slot, extend the array */
    if (slot == handles_max_)
    {
        size_t i;

        /* allocate a larger array */
        handles_max_ += 32;
        handles_ = (void **)
                   t3realloc(handles_, handles_max_ * sizeof(*handles_));

        /* clear out the newly-allocated slots */
        for (i = slot ; i < handles_max_ ; ++i)
            handles_[i] = 0;
    }

    /* store the new item in our pointer array */
    handles_[slot] = item;

    /* 
     *   convert the slot number to a handle by adjusting it to a 1-based
     *   index, and return the result 
     */
    return slot + 1;
}


/* ------------------------------------------------------------------------ */
/*
 *   Banner manager 
 */

/*
 *   Create a banner 
 */
int CVmBannerManager::create_banner(VMG_ int parent_id,
                                    int where, int other_id,
                                    int wintype, int align,
                                    int siz, int siz_units,
                                    unsigned long style)
{
    void *handle;
    void *parent_handle;
    void *other_handle;
    CVmConsoleBanner *item;

    /* get the parent handle, if provided */
    parent_handle = get_os_handle(parent_id);

    /* get the 'other' handle, if we need it for the 'where' */
    switch(where)
    {
    case OS_BANNER_BEFORE:
    case OS_BANNER_AFTER:
        /* retrieve the handle for the other_id */
        other_handle = get_os_handle(other_id);
        break;

    default:
        /* we don't need 'other' for other 'where' modes */
        other_handle = 0;
        break;
    }

    /* try creating the OS-level banner window */
    handle = os_banner_create(parent_handle, where, other_handle, wintype,
                              align, siz, siz_units, style);

    /* if we couldn't create the OS-level window, return failure */
    if (handle == 0)
        return 0;

    /* create the new console */
    item = new CVmConsoleBanner(handle, wintype, style);

    /* allocate a handle for the new banner, and return the handle */
    return alloc_handle(item);
}

/*
 *   Delete or orphan a banner window.  Deleting and orphaning both sever
 *   all ties from the banner manager (and thus from the T3 program) to the
 *   banner.  Deleting a banner actually gets deletes it at the OS level;
 *   orphaning the banner severs our ties, but hands the banner over to the
 *   OS to do with as it pleases.  On some implementations, the OS will
 *   continue to display the banner after it's orphaned to allow the final
 *   display configuration to remain visible even after the program has
 *   terminated.  
 */
void CVmBannerManager::delete_or_orphan_banner(VMG_ int banner, int orphan)
{
    CVmConsoleBanner *item;
    void *handle;

    /* if the banner is invalid, ignore the request */
    if ((item = (CVmConsoleBanner *)get_object(banner)) == 0)
        return;

    /* get the OS-level banner handle */
    handle = item->get_os_handle();

    /* delete the banner item */
    item->delete_obj(vmg0_);

    /* clear the slot */
    clear_handle(banner);

    /* delete the OS-level banner */
    if (orphan)
        os_banner_orphan(handle);
    else
        os_banner_delete(handle);
}

/*
 *   Get the OS-level handle for the given banner 
 */
void *CVmBannerManager::get_os_handle(int banner)
{
    CVmConsoleBanner *item;

    /* if the banner is invalid, return failure */
    if ((item = (CVmConsoleBanner *)get_object(banner)) == 0)
        return 0;

    /* return the handle from the slot */
    return item->get_os_handle();
}

/*
 *   Flush all banners 
 */
void CVmBannerManager::flush_all(VMG_ vm_nl_type nl)
{
    size_t slot;

    /* flush each banner */
    for (slot = 0 ; slot < handles_max_ ; ++slot)
    {
        /* if this slot has a valid banner, flush it */
        if (handles_[slot] != 0)
            ((CVmConsoleBanner *)handles_[slot])->flush(vmg_ nl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Banner Window Console 
 */
CVmConsoleBanner::CVmConsoleBanner(void *banner_handle, int win_type,
                                   unsigned long style)
{
    CVmFormatterBanner *str;
    os_banner_info_t info;
    int obey_whitespace = FALSE;
    int literal_mode = FALSE;

    /* remember our OS-level banner handle */
    banner_ = banner_handle;

    /* get osifc-level information on the banner */
    if (!os_banner_getinfo(banner_, &info))
        info.os_line_wrap = FALSE;

    /* 
     *   If it's a text grid window, don't do any line wrapping.  Text grids
     *   simply don't have any line wrapping, so we don't want to impose any
     *   at the formatter level.  Set the formatter to "os line wrap" mode,
     *   to indicate that the formatter doesn't do wrapping - even though
     *   the underlying OS banner window won't do any wrapping either, the
     *   lack of line wrapping counts as OS handling of line wrapping.  
     */
    if (win_type == OS_BANNER_TYPE_TEXTGRID)
    {
        /* do not wrap lines in the formatter */
        info.os_line_wrap = TRUE;

        /* use literal mode, and obey whitespace literally */
        literal_mode = TRUE;
        obey_whitespace = TRUE;
    }

    /* create and initialize our display stream */
    disp_str_ = str = new CVmFormatterBanner(banner_handle, this,
                                             win_type, style);
    str->init_banner(info.os_line_wrap, obey_whitespace, literal_mode);

    /* remember our window type */
    win_type_ = win_type;
}

/*
 *   Deletion 
 */
void CVmConsoleBanner::delete_obj(VMG0_)
{
    /* delete our display stream */
    disp_str_->delete_obj(vmg0_);

    /* do the inherited work */
    CVmConsole::delete_obj(vmg0_);
}

/*
 *   Clear the banner window 
 */
void CVmConsoleBanner::clear_window(VMG0_)
{
    /* flush and empty our output buffer */
    flush(vmg_ VM_NL_NONE);
    empty_buffers(vmg0_);
    
    /* clear our underlying system banner */
    os_banner_clear(banner_);
    
    /* tell our display stream to zero its line counter */
    disp_str_->reset_line_count(TRUE);
}

/*
 *   Get banner information 
 */
int CVmConsoleBanner::get_banner_info(os_banner_info_t *info)
{
    int ret;
    
    /* get the OS-level information */
    ret = os_banner_getinfo(banner_, info);

    /* make some adjustments if we got valid information back */
    if (ret)
    {
        /* 
         *   check the window type for further adjustments we might need to
         *   make to the data returned from the OS layer 
         */
        switch(win_type_)
        {
        case OS_BANNER_TYPE_TEXTGRID:
            /* 
             *   text grids don't support <TAB> alignment, even if the
             *   underlying OS banner says we do, because we simply don't
             *   support <TAB> (or any other HTML markups) in a text grid
             *   window 
             */
            info->style &= ~OS_BANNER_STYLE_TAB_ALIGN;
            break;

        default:
            /* other types don't require any adjustments */
            break;
        }
    }

    /* return the success indication */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Log file console manager 
 */

/*
 *   create a log console 
 */
int CVmLogConsoleManager::create_log_console(
    VMG_ CVmNetFile *nf, osfildef *fp, CCharmapToLocal *cmap, int width)
{
    CVmConsoleLog *con;
    
    /* create the new console */
    con = new CVmConsoleLog(vmg_ nf, fp, cmap, width);

    /* allocate a handle for the new console and return the handle */
    return alloc_handle(con);
}

/*
 *   delete log a console 
 */
void CVmLogConsoleManager::delete_log_console(VMG_ int handle)
{
    CVmConsoleLog *con;
    
    /* if the handle is invalid, ignore the request */
    if ((con = (CVmConsoleLog *)get_object(handle)) == 0)
        return;

    /* delete the console */
    con->delete_obj(vmg0_);

    /* clear the slot */
    clear_handle(handle);
}

/* ------------------------------------------------------------------------ */
/*
 *   Log file console 
 */
CVmConsoleLog::CVmConsoleLog(VMG_ CVmNetFile *nf, osfildef *fp,
                             class CCharmapToLocal *cmap, int width)
{
    CVmFormatterLog *str;

    /* create our display stream */
    disp_str_ = str = new CVmFormatterLog(this, width);

    /* set the file */
    str->set_log_file(vmg_ nf, fp);

    /* set the character mapper */
    str->set_charmap(cmap);
}

/*
 *   destroy 
 */
void CVmConsoleLog::delete_obj(VMG0_)
{
    /* delete our display stream */
    disp_str_->delete_obj(vmg0_);

    /* do the inherited work */
    CVmConsole::delete_obj(vmg0_);
}
