/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os_stdio.cpp - OS implementation for standard I/O
Function
  This is a simple stdio-based implementation of the OS display functions.
  This is a portable implementation (at least, it's portable to any
  platform where stdio can be used).

  The main purpose of this implementation is to make it easy to build
  command-line versions of tools that incorporate osifc-layer functions
  without having to drag in the full-blown OS-specific implementation.
  For example, test tools might find this useful.
Notes
  
Modified
  09/04/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "os.h"
#include "t3std.h"

/* 
 *   status-line display mode 
 */
static int S_status_mode = 0;

/*
 *   set non-stop mode 
 */
void os_nonstop_mode(int flag)
{
}

/*
 *   flush output 
 */
void os_flush()
{
}

/*
 *   update the display 
 */
void os_update_display()
{
}

/*
 *   get highlighting character - indicate that highlighting is to be
 *   ignored 
 */
/* set text attributes */
void os_set_text_attr(int /*attr*/)
{
    /* not supported - ignore it */
}

/* set text color */
void os_set_text_color(os_color_t /*fg*/, os_color_t /*bg*/)
{
    /* not supported - ignore it */
}

/* set the screen color */
void os_set_screen_color(os_color_t /*color*/)
{
    /* not supported - ignore it */
}

/*
 *   display text 
 */
void os_printz(const char *str)
{
    /* suppress output if we're not in plain text mode */
    if (S_status_mode == 0)
        fputs(str, stdout);
}

/*
 *   get an event 
 */
int os_get_event(unsigned long timeout, int use_timeout,
                 os_event_info_t *info)
{
    /* we can't handle timeouts */
    if (use_timeout)
        return OS_EVT_NOTIMEOUT;

    /* get a key */
    info->key[0] = os_getc_raw();
    if (info->key[0] == 0)
        info->key[1] = os_getc_raw();

    /* return the keyboard event */
    return OS_EVT_KEY;
}

/*
 *   clear the screen 
 */
void oscls()
{
    /* 
     *   print a ^L to clear the screen, if such sequences are interpreted
     *   by the display driver or terminal 
     */
    printf("\014\n");
}

/*
 *   read from the keyboard 
 */
unsigned char *os_gets(unsigned char *buf, size_t buflen)
{
    size_t len;
    
    /* read from stdin; return failure if fgets does */
    if (fgets((char *)buf, buflen, stdin) == 0)
        return 0;

    /* remove the trailing newline in the result, if there is one */
    if ((len = strlen((char *)buf)) != 0 && buf[len-1] == '\n')
        buf[len-1] = '\0';

    /* return the buffer pointer */
    return buf;
}

/*
 *   read from keyboard with timeout 
 */
int os_gets_timeout(unsigned char *buf, size_t bufl,
                    unsigned long timeout_in_milliseconds, int use_timeout)
{
    /* 
     *   if we've been asked to read with a timeout, we can't comply, so
     *   just return the no-timeout-available error code; if we're reading
     *   without a timeout, just use the ordinary input reader 
     */
    if (use_timeout)
    {
        /* timeout requested, no can do - return the no-timeout error code */
        return OS_EVT_NOTIMEOUT;
    }
    else
    {
        /* 
         *   no timeout requested, so use the ordinary reader, and translate
         *   its returns codes to the appropriate equivalents for this
         *   routine (null from os_gets -> error -> OS_EVT_EOF as our return
         *   code; non-null -> success -> OS_EVT_LINE) 
         */
        return (os_gets(buf, bufl) == 0 ? OS_EVT_EOF : OS_EVT_LINE);
    }
}

/*
 *   Cancel interrupted input.  We don't handle input with timeout in the
 *   first place, so interrupted input is impossible, so we need do nothing
 *   here. 
 */
void os_gets_cancel(int reset)
{
    /* there's nothing for us to do */
}

/*
 *   prompt for information through a dialog
 */
int os_input_dialog(int /*icon_id*/,
                    const char *prompt, int standard_button_set,
                    const char **buttons, int button_count,
                    int /*default_index*/, int /*cancel_index*/)
{
    /* keep going until we get a valid response */
    for (;;)
    {
        int i;
        char buf[256];
        const char *p;
        const char *cur;
        char *resp;
        int match_cnt;
        int last_found;
        static const struct
        {
            const char *buttons[3];
            int button_count;
        } std_btns[] =
        {
            { { "&OK" },                    1 },
            { { "&OK", "&Cancel" },         2 },
            { { "&Yes", "&No" },            2 },
            { { "&Yes", "&No", "&Cancel" }, 3 }
        };

        /* 
         *   if we have a standard button set selected, get our button
         *   labels 
         */
        switch(standard_button_set)
        {
        case 0:
            /* use the explicit buttons provided */
            break;

        case OS_INDLG_OK:
            i = 0;

        use_std_btns:
            /* use the selected standard button set */
            buttons = (const char **)std_btns[i].buttons;
            button_count = std_btns[i].button_count;
            break;

        case OS_INDLG_OKCANCEL:
            i = 1;
            goto use_std_btns;

        case OS_INDLG_YESNO:
            i = 2;
            goto use_std_btns;

        case OS_INDLG_YESNOCANCEL:
            i = 3;
            goto use_std_btns;

        default:
            /* 
             *   we don't recognize other standard button sets - return an
             *   error 
             */
            return 0;
        }

        /* 
         *   if there are no buttons defined, they'll never be able to
         *   respond, so we'd just loop forever - rather than let that
         *   happen, return failure 
         */
        if (button_count == 0)
            return 0;

        /* display a newline and the prompt string */
        os_printz("\n");
        os_printz(prompt);

        /* display the response */
        for (i = 0 ; i < button_count ; ++i)
        {
            /* 
             *   display a slash to separate responses, if this isn't the
             *   first one 
             */
            if (i != 0)
                os_printz("/");

            /* get the current button */
            cur = buttons[i];

            /* 
             *   Look for a "&" in the response string.  If we find it,
             *   remove the "&" and enclose the shortcut key in parens.  
             */
            for (p = cur ; *p != '&' && *p != '\0' ; ++p) ;

            /* if we found the "&", put the next character in parens */
            if (*p == '&')
            {
                size_t pre_len;
                size_t post_len;

                /* limit the prefix length to avoid overflowing the buffer */
                pre_len = p - cur;
                if (pre_len > sizeof(buf) - 5)
                    pre_len = sizeof(buf) - 5;

                /* limit the postfix length to avoid buffer overflow, too */
                post_len = strlen(p + 2);
                if (post_len > sizeof(buf) - 5 - pre_len)
                    post_len = sizeof(buf) - 5 - pre_len;

                /* reformat the response string */
                sprintf(buf, "%.*s(%c)%.*s",
                        (int)pre_len, cur, *(p + 1), (int)post_len, p + 2);

                /* show it */
                os_printz(buf);
            }
            else
            {
                /* no '&' - just display the response string as-is */
                os_printz(cur);
            }
        }

        /* read the response */
        os_printz(" > ");
        os_gets((unsigned char *)buf, sizeof(buf));

        /* skip any leading spaces in the reply */
        for (resp = buf ; isspace(*resp) ; ++resp) ;

        /* if it's one character, check it against the shortcut keys */
        if (strlen(resp) == 1)
        {
            /* scan the responses */
            for (i = 0 ; i < button_count ; ++i)
            {
                /* look for a '&' in this button */
                for (p = buttons[i] ; *p != '&' && *p != '\0' ; ++p) ;

                /* if we found the '&', check the shortcut */
                if (*p == '&' && toupper(*(p+1)) == toupper(*resp))
                {
                    /* 
                     *   this is the one - return the current index
                     *   (bumping it by one to get a 1-based value) 
                     */
                    return i + 1;
                }
            }
        }

        /* 
         *   Either it's not a one-character reply, or it didn't match a
         *   short-cut - check it against the leading substrings of the
         *   responses.  If it matches exactly one of the responses in its
         *   leading substring, use that response.  
         */
        for (i = 0, match_cnt = 0 ; i < button_count ; ++i)
        {
            const char *p1;
            const char *p2;

            /* 
             *   compare this response to the user's response; skip any
             *   '&' in the button label 
             */
            for (p1 = resp, p2 = buttons[i] ; *p1 != '\0' && *p2 != '\0' ;
                 ++p1, ++p2)
            {
                /* if this is a '&' in the button label, skip it */
                if (*p2 == '&')
                    ++p2;

                /* if these characters don't match, it's no match */
                if (toupper(*p1) != toupper(*p2))
                    break;
            }

            /* 
             *   if we reached the end of the user's response, we have a
             *   match in the leading substring - count it and remember
             *   this as the last one, but keep looking, since we need to
             *   make sure we don't have any other matches 
             */
            if (*p1 == '\0')
            {
                ++match_cnt;
                last_found = i;
            }
        }

        /* 
         *   if we found exactly one match, return it (adjusting to a
         *   1-based index); if we found more or less than one match, it's
         *   not a valid response, so start over with a new prompt 
         */
        if (match_cnt == 1)
            return last_found + 1;
    }
}

/*
 *   ask for a file 
 */
int os_askfile(const char *prompt, char *reply, int replen,
               int /*dialog_type*/, os_filetype_t /*file_type*/)
{
    /* show the prompt */
    os_printz(prompt);
    os_printz(" >");

    /* ask for the filename */
    os_gets((unsigned char *)reply, replen);

    /* 
     *   if they entered an empty line, return "cancel"; otherwise, return
     *   success 
     */
    return (reply[0] == '\0' ? OS_AFE_CANCEL : OS_AFE_SUCCESS);
}

/*
 *   get system information 
 */
int os_get_sysinfo(int code, void *param, long *result)
{
    switch(code)
    {
    case SYSINFO_HTML:
    case SYSINFO_JPEG:
    case SYSINFO_PNG:
    case SYSINFO_WAV:
    case SYSINFO_MIDI:
    case SYSINFO_WAV_MIDI_OVL:
    case SYSINFO_WAV_OVL:
    case SYSINFO_PREF_IMAGES:
    case SYSINFO_PREF_SOUNDS:
    case SYSINFO_PREF_MUSIC:
    case SYSINFO_PREF_LINKS:
    case SYSINFO_MPEG:
    case SYSINFO_MPEG1:
    case SYSINFO_MPEG2:
    case SYSINFO_MPEG3:
    case SYSINFO_LINKS_HTTP:
    case SYSINFO_LINKS_FTP:
    case SYSINFO_LINKS_NEWS:
    case SYSINFO_LINKS_MAILTO:
    case SYSINFO_LINKS_TELNET:
    case SYSINFO_PNG_TRANS:
    case SYSINFO_PNG_ALPHA:
    case SYSINFO_OGG:
    case SYSINFO_TEXT_HILITE:
    case SYSINFO_TEXT_COLORS:
    case SYSINFO_BANNERS:
        /* 
         *   we don't support any of these features - set the result to 0
         *   to indicate this 
         */
        *result = 0;

        /* return true to indicate that we recognized the code */
        return TRUE;

    case SYSINFO_INTERP_CLASS:
        /* indicate that we're a text-only interpreter */
        *result = SYSINFO_ICLASS_TEXT;
        return TRUE;

    default:
        /* we don't recognize other codes */
        return FALSE;
    }
}

/*
 *   set status mode
 */
void os_status(int mode)
{
    /* remember the new mode */
    S_status_mode = mode;
}

/*
 *   display a string in the right half of the status line 
 */
void os_strsc(const char *)
{
    /* ignore it - we don't have a status line in this UI */
}

void os_plain()
{
    /* 
     *   this implementation is always in plain mode, so there's nothing
     *   we need to do here 
     */
}

void os_expause()
{
    /* do nothing */
}

/* ------------------------------------------------------------------------ */
/*
 *   none of the banner functions are useful in plain stdio mode 
 */
void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units, unsigned long style)
{
    return 0;
}

void os_banner_delete(void *banner_handle)
{
}

void os_banner_orphan(void *banner_handle)
{
}

void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
}

void os_banner_flush(void *banner_handle)
{
}

void os_banner_set_size(void *banner_handle, int siz, int siz_units,
                        int is_advisory)
{
}

void os_banner_size_to_contents(void *banner_handle)
{
}

void os_banner_start_html(void *banner_handle)
{
}

void os_banner_end_html(void *banner_handle)
{
}

void os_banner_set_attr(void *banner_handle, int attr)
{
}

void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg)
{
}

void os_banner_set_screen_color(void *banner_handle, os_color_t color)
{
}

void os_banner_clear(void *banner_handle)
{
}

int os_banner_get_charwidth(void *banner_handle)
{
    return 0;
}

int os_banner_get_charheight(void *banner_handle)
{
    return 0;
}

int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
    return FALSE;
}

void os_banner_goto(void *banner_handle, int row, int col)
{
}

void os_init_ui_after_load(class CVmBifTable *, class CVmMetaTable *)
{
}
