#include "converter.h"

#include <errno.h>

#include "options.h"
#include "syserr.h"
#include "msg.h"

#ifndef HAVE_GARGLK

/* Cross-building sets iconv args to const using WINICONV_CONST, so
 * nullify that to remove warnings... */
#define WINICONV_CONST
#include <iconv.h>

/* Outgoing - will always return an alloc'd string that needs to be freed */
char *ensureExternalEncoding(char input[]) {
    if (encodingOption == ENCODING_UTF) {
        iconv_t cd = iconv_open("UTF-8", "ISO_8859-1");
        if (cd == (iconv_t) -1)
            syserr("iconv_open() failed!");

        /* UTF-8 might be longer than ISO8859-1 encoding so double */
        char *in_p = input;
        size_t in_left = strlen(input);
        size_t out_left = strlen(input)*2;
        char *converted = (char *)malloc(out_left+1);
        char *out_p = converted;

        if (iconv(cd, &in_p, &in_left, &out_p, &out_left) == (size_t) -1) {
            /* Internal code (ISO8859-1) to UTF should never fail */
            syserr("Conversion of command output to UTF-8 failed");
        }
        *out_p = '\0';

        iconv_close(cd);
        return converted;
    } else
        return strdup(input);
}


/* Incoming -- will always return an alloc'd string that needs to be freed */
char *ensureInternalEncoding(char input[]) {
    if (encodingOption == ENCODING_UTF) {
        iconv_t cd = iconv_open("ISO_8859-1", "UTF-8");
        if (cd == (iconv_t) -1)
            syserr("iconv_open() failed!");

        /* ISO8859-1 encoding is always shorter than UTF-8, so this is enough */
        char *in_p = input;
        size_t in_left = strlen(input);
        size_t out_left = in_left;
        char *converted = (char *)malloc(strlen(input)+1);
        char *out_p = converted;

        if (iconv(cd, &in_p, &in_left, &out_p, &out_left) == (size_t) -1) {
            /* TODO: We need a message for illegal characters in input */
            printf("Conversion of command input from UTF-8 failed, are you sure about the input encoding? ('%s')\n", strerror(errno));
            error(M_WHAT);
        }
        *out_p = '\0';

        iconv_close(cd);
        return converted;
    } else
        return strdup(input);
}

#else

/* For Gargoyle/GARGLK we just return the original string to avoid extra dependency on iconv */

char *ensureExternalEncoding(char input[]) {
    return strdup(input);
}

char *ensureInternalEncoding(char input[]) {
    return strdup(input);
}

#endif
