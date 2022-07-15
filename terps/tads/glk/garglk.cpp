// garglk's (private) API is C++, but osglkban.c requires access to it.
// It shouldn't be using the private API, but for now, use a wrapper to
// expose what osglkban.c needs, with a C API.

#include "garglk.h"

extern "C" {

int os_banner_size_garglk(winid_t win)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int size = dwin->scrollmax;
    if (dwin->numchars)
        size ++;
    return size;
}

}
