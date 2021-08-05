#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  indlg_tx.cpp - formatted text implementation of input_dialog
Function
  Implements the input dialog using formatted text
Notes
  Only one of indlg_tx.c or indlg_os.c should be included in a given
  executable.  For a text-only version, include indlg_tx.  For a version
  where os_input_dialog() provides a system dialog, use indlg_os instead.

  We provide a choice of input_dialog() implementations in the portable
  code (rather than only through the OS code) so that we can call
  the formatted text output routines in this version.  An OS-layer
  implementation could not call the formatted output routines (it would
  have to call os_printf directly), which would result in poor prompt
  formatting any time a prompt exceeded a single line of text.
Modified
  09/27/99 MJRoberts  - Creation
*/


#include "os.h"
#include "t3std.h"
#include "vmglob.h"
#include "vmconsol.h"
#include "charmap.h"

/*
 *   formatted text-only file prompt 
 */
int CVmConsole::input_dialog(VMG_ int /*icon_id*/,
                             const char *prompt, int standard_button_set,
                             const char **buttons, int button_count,
                             int default_index, int cancel_index,
                             int bypass_script)
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
        format_text(vmg_ "\n");
        format_text(vmg_ prompt);
        format_text(vmg_ " ");

        /* display the response */
        for (i = 0 ; i < button_count ; ++i)
        {
            /* 
             *   display a slash to separate responses, if this isn't the
             *   first one 
             */
            if (i != 0)
                format_text(vmg_ "/");

            /* get the current button */
            cur = buttons[i];

            /* 
             *   Look for a "&" in the response string.  If we find it,
             *   remove the "&" and enclose the shortcut key in parens.  
             */
            for (p = cur ; *p != '\0' && *p != '&' ;
                 p = utf8_ptr::s_inc((char *)p)) ;

            /* if we found the "&", put the next character in parens */
            if (*p != '\0')
            {
                size_t pre_len;
                size_t post_len;

                /* 
                 *   note the length of the part up to the '&', but limit it
                 *   to avoid overflowing the buffer 
                 */
                pre_len = p - cur;
                if (pre_len > sizeof(buf) - 5)
                    pre_len = sizeof(buf) - 5;

                /* 
                 *   note the length of the part after the '&', limiting it
                 *   as well 
                 */
                post_len = strlen(p+2);
                if (post_len > sizeof(buf) - 5 - pre_len)
                    post_len = sizeof(buf) - 5 - pre_len;

                /* reformat the response string */
                sprintf(buf, "%.*s(%c)%.*s",
                        (int)pre_len, cur, *(p + 1), (int)post_len, p + 2);

                /* display it */
                format_text(vmg_ buf);
            }
            else
            {
                /* no '&' - just display the response string as-is */
                format_text(vmg_ cur);
            }
        }

        /* switch to input font */
        format_text(vmg_ "<font face='TADS-Input'>");

        /* read the response */
        format_text(vmg_ " >");
        read_line(vmg_ buf, sizeof(buf));

        /* close the input font tag */
        format_text(vmg_ "</font>");

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

