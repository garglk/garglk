#include <stdio.h>
#include "glk.h"
#include "garglk.h"

#ifdef GLK_MODULE_HYPERLINKS

void glk_set_hyperlink(glui32 linkval)
{
    gli_strict_warning("set_hyperlink: hyperlinks not supported.");
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
    gli_strict_warning("set_hyperlink_stream: hyperlinks not supported.");
}

void glk_request_hyperlink_event(winid_t win)
{
    gli_strict_warning("request_hyperlink_event: hyperlinks not supported.");
}

void glk_cancel_hyperlink_event(winid_t win)
{
    gli_strict_warning("cancel_hyperlink_event: hyperlinks not supported.");
}

#endif /* GLK_MODULE_HYPERLINKS */

