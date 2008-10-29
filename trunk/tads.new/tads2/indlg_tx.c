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
  indlg_tx.c - input dialog - formatted text-only version
Function
  Implements the input dialog function using formatted text
Notes
  Only one of indlg_tx or indlg_os should be included in a given
  executable.  The choice depends on whether a system-level dialog
  is available or not.  If no system-level dialog is available,
  include indlg_tx, which provides an implementation using formatted
  text.  If a system-level dialog is available, use indlg_os, which
  calls os_input_dialog().

  This file exists in the portable layer, rather than in the OS layer,
  so that we can provide an implementation using formatted text.  An
  OS-layer implementation could not provide formatted text.
Modified
  09/27/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "std.h"
#include "os.h"
#include "tio.h"

/* ------------------------------------------------------------------------ */
/*
 *   Text-mode os_input_dialog implementation 
 */
int tio_input_dialog(int icon_id, const char *prompt,
                     int standard_button_set,
                     const char **buttons, int button_count,
                     int default_index, int cancel_index)
{
    /* ignore the icon ID - we can't display an icon in text mode */
    VARUSED(icon_id);

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
        outformat("\\n");
        outformat((char *)prompt);
        outformat(" ");

        /* display the response */
        for (i = 0 ; i < button_count ; ++i)
        {
            /* 
             *   display a slash to separate responses, if this isn't the
             *   first one 
             */
            if (i != 0)
                outformat("/");

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
                /* reformat the response string */
                sprintf(buf, "%.*s(%c)%s", (int)(p - cur), cur, *(p+1), p+2);

                /* display it */
                outformat(buf);
            }
            else
            {
                /* no '&' - just display the response string as-is */
                outformat((char *)cur);
            }
        }

        /* if we're in HTML mode, switch to input font */
        if (tio_is_html_mode())
            outformat("<font face='TADS-Input'>");

        /* read the response */
        getstring(" >", buf, sizeof(buf));

        /* if we're in HTML mode, close the input font tag */
        if (tio_is_html_mode())
            outformat("</font>");

        /* skip any leading spaces in the reply */
        for (resp = buf ; t_isspace(*resp) ; ++resp) ;

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

