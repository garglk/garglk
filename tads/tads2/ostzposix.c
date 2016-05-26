#include <ctype.h>
#include <string.h>
#include <time.h>

#include "os.h"
#include "lib.h"
#include "osstzprs.h"

extern void safe_strcpy(char *dst, size_t dstlen, const char *src);

/* Get the local time zone name, as a zoneinfo database key.  Many
 * modern Unix systems use zoneinfo keys as they native timezone
 * setting, but there are several different ways of storing this on a
 * per-process and system-wide basis.  We'll try looking for the common
 * mechanisms.
 */
int
os_get_zoneinfo_key( char *buf, size_t buflen )
{
    /* First, try the TZ environment variable.  This is used on nearly
     * all Unix-alikes for a per-process timezone setting, although
     * it will only contain a zoneinfo key in newer versions.  There
     * are several possible formats for specifying a zoneinfo key:
     *
     *  TZ=/usr/share/zoneinfo/America/Los_Angeles
     *    - A full absolute path name to a tzinfo file.  We'll sense
     *      this by looking for "/zoneinfo/" in the string, and if we
     *      find it, we'll return the portion after /zoneinfo/.
     *
     *  TZ=America/Los_Angeles
     *    - Just the zoneinfo key, without a path.  If we find a string
     *      that contains all alphabetics, undersores, and slashes, and
     *      has at least one internal slash but doesn't start with a
     *      slash, we probably have a zoneinfo key.  We'll see if we
     *      can find a matching file in the usual zoneinfo database
     *      locations: /etc/zoneinfo, /usr/share/zoneinfo; if we can,
     *      we'll return the key name, otherwise we'll assume this
     *      isn't actually a zoneinfo key but just happens to look like
     *      one in terms of format.
     *
     *  TZ=:America/Los_Angeles
     *  TZ=:/etc/zoneinfo/America/Los_Angeles
     *    - POSIX systems generally use the ":" prefix to signify that
     *      this is a zoneinfo path rather than the old-style "EST5EDT"
     *      type of self-contained zone description.  If we see a colon
     *      prefix with a relative path (properly formed in terms of
     *      its character content), we'll simply assume this is a
     *      zoneinfo key without even checking for an existing file,
     *      since there's not much else it could be.  If we see an
     *      absolute path, we'll search it for /zoneinfo/ and return
     *      the portion after this, again without checking for an
     *      existing file.
     */
    const char *tz = getenv("TZ");
    if (tz != 0 && tz[0] != '\0')
    {
        /* check that the string is formatted like a zoneinfo key */
#define tzcharok(c) (isalpha(c) != 0 || (c) == '/' || (c) == '_')
        int fmt_ok = TRUE;
        const char *p;
        fmt_ok &= (tz[0] == ':' || tzcharok(tz[0]));
        for (p = tz + 1 ; *p != '\0' ; ++p)
            fmt_ok &= tzcharok(*p);

        /* proceed only if it has the right format */
        if (fmt_ok)
        {
            /* check for a leading ':', per POSIX */
            if (tz[0] == ':')
            {
                /* yes, we have a leading ':', so it's almost certainly
                 * a zoneinfo key; if it's an absolute path, find the
                 * part after /zoneinfo/
                 */
                if (tz[1] == '/')
                {
                    /* absolute form - look for /zoneinfo/ */
                    const char *z = strstr(tz, "/zoneinfo/");
                    if (z != 0)
                    {
                        /* found it - return the part after /zoneinfo/ */
                        safe_strcpy(buf, buflen, z + 10);
                        return TRUE;
                    }
                }
                else
                {
                    /* relative path - return as-is minus the colon */
                    safe_strcpy(buf, buflen, tz + 1);
                    return TRUE;
                }
            }
            else
            {
                /* There's no colon, so it *might* be a zoneinfo key.
                 * If it's an absolute path containing /zoneinfo/, it's
                 * a solid bet.  If it's a relative path, look to see
                 * if we can find a file in one of the usual zoneinfo
                 * database locations.
                 */
                if (tz[0] == '/')
                {
                    /* absolute path - check for /zoneinfo/ */
                    const char *z = strstr(tz, "/zoneinfo/");
                    if (z != 0)
                    {
                        /* found it - return the part after /zoneinfo/ */
                        safe_strcpy(buf, buflen, z + 10);
                        return TRUE;
                    }
                }
                else
                {
                    /* relative path - look for a tzinfo file in the
                     * usual locations
                     */
                    static const char *dirs[] = {
                        "/etc/zoneinfo",
                        "/usr/share/zoneinfo",
                        0
                    };
                    const char **dir;
                    for (dir = dirs ; *dir != 0 ; ++dir)
                    {
                        /* build this full path */
                        char fbuf[OSFNMAX];
                        os_build_full_path(fbuf, sizeof(fbuf), *dir, tz);

                        /* check for a file at this location */
                        if (!osfacc(fbuf))
                        {
                            /* got it - looks like a good zoneinfo key */
                            safe_strcpy(buf, buflen, tz);
                            return TRUE;
                        }
                    }
                }
            }
        }
    }

    /* No luck with TZ, so try the system-wide settings next.
     *
     * If a file called /etc/timezone exists, it's usually a one-line
     * text file containing the zoneinfo key.  Read and return its
     * contents.
     */
    FILE *fp;
    if ((fp = fopen("/etc/timezone", "r")) != 0)
    {
        /* read the one-liner */
        char lbuf[256];
        int ok = FALSE;
        if (fgets(lbuf, sizeof(lbuf), fp) != 0)
        {
            /* strip any trailing newline */
            size_t l = strlen(lbuf);
            if (l != 0 && lbuf[l-1] == '\n')
                lbuf[l-1] = '\0';

            /* if it's in absolute format, return the part after
             * /zoneinfo/; otherwise just return the string
             */
            if (lbuf[0] == '/')
            {
                /* absoltue path - find /zoneinfo/ */
                const char *z = strstr(lbuf, "/zoneinfo/");
                if (z != 0)
                {
                    safe_strcpy(buf, buflen, z + 10);
                    ok = TRUE;
                }
            }
            else
            {
                /* relative notation - return it as-is */
                safe_strcpy(buf, buflen, lbuf);
                ok = TRUE;
            }
        }

        /* we're done with the file */
        fclose(fp);

        /* if we got our result, return success */
        if (ok)
            return TRUE;
    }

    /* If /etc/sysconfig/clock exists, read it and look for a line
     * starting with ZONE=.  This contains the zoneinfo key.
     */
    if ((fp = fopen("/etc/sysconfig/clock", "r")) != 0)
    {
        /* scan the file for ZONE=... */
        int ok = FALSE;
        for (;;)
        {
            /* read the next line */
            char lbuf[256];
            if (fgets(lbuf, sizeof(lbuf), fp) == 0)
                break;

            /* skip leading spaces */
            const char *p;
            for (p = lbuf ; isspace(*p) ; ++p) ;

            /* check for ZONE */
            if (memicmp(p, "zone", 4) != 0)
                continue;

            /* skip spaces after ZONE */
            for (p += 4 ; isspace(*p) ; ++p) ;

            /* check for '=' */
            if (*p != '=')
                continue;

            /* skip spaces after the '=' */
            for (++p ; isspace(*p) ; ++p) ;

            /* if it's in absolute form, look for /zoneinfo/ */
            if (*p == '/')
            {
                const char *z = strstr(p, "/zoneinfo/");
                if (z != 0)
                {
                    safe_strcpy(buf, buflen, z + 10);
                    ok = TRUE;
                }
            }
            else
            {
                /* relative notation - it's the zoneinfo key */
                safe_strcpy(buf, buflen, p);
                ok = TRUE;
            }

            /* that's our ZONE line, so we're done scanning the file */
            break;
        }

        /* done with the file */
        fclose(fp);

        /* if we got our result, return success */
        if (ok)
            return TRUE;
    }

    /* If /etc/localtime is a symbolic link, the linked file is the
     * actual zoneinfo file.  Resolve the link and return the portion
     * of the path after "/zoneinfo/".
     */
    static const char *elt = "/etc/localtime";
    unsigned long mode;
    char linkbuf[OSFNMAX];
    const char *zi;
    if (osfmode(elt, FALSE, &mode, NULL)
        && (mode & OSFMODE_LINK) != 0
        && os_resolve_symlink(elt, linkbuf, sizeof(linkbuf))
        && (zi = strstr(linkbuf, "/zoneinfo/")) != 0)
    {
        /* it's a link containing /zoneinfo/, so return the portion
         * after /zoneinfo/
         */
        safe_strcpy(buf, buflen, zi + 10);
        return TRUE;
    }

    /* well, we're out of ideas - return failure */
    return FALSE;
}

/* Get timezone information. This can be used as a fallback if the a
 * zoneinfo key isn't available, or if the caller doesn't use zoneinfo
 * data.
 */
int
os_get_timezone_info( struct os_tzinfo_t *info )
{
    /* try parsing environment variable TZ as a POSIX timezone string */
    const char *tz = getenv("TZ");
    if (tz != 0 && oss_parse_posix_tz(info, tz, strlen(tz), TRUE))
        return TRUE;

#ifndef _WIN32
    /* fall back on localtime() - that'll at least give us the current
     * timezone name and GMT offset in most cases
     */
    time_t timer = time(0);
    const struct tm *tm = localtime(&timer);
    if (tm != 0)
    {
        memset(info, 0, sizeof(*info));
        info->is_dst = tm->tm_isdst;
        if (tm->tm_isdst)
        {
            info->dst_ofs = tm->tm_gmtoff;
            safe_strcpy(info->dst_abbr, sizeof(info->dst_abbr), tm->tm_zone);
        }
        else
        {
            info->std_ofs = tm->tm_gmtoff;
            safe_strcpy(info->std_abbr, sizeof(info->std_abbr), tm->tm_zone);
        }
        return TRUE;
    }
#endif

    /* no information is available */
    return FALSE;
}
