/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  gameinfo.cpp - extract and parse tads 2/3 game file information
Function
  Searches a compiled tads 2 game file or a compiled tads 3 image file
  for a game information resource.  If the resource is found, extracts
  and parses the resource.

  A game information resource is a set of name/value pairs giving
  information on the game, such as its name, author, and description.
  Each value is simply a text string.  The game information is supplied
  by the author, and is intended to help archives and users by providing
  a way to obtain specific information about the game in a structured
  format directly from the game file itself.

  This implementation is independent of any tads 2 or tads 3 subsystem
  except osifc, which it uses for portable file I/O and portable byte
  format conversions.  We do depend upon the 'resfind' utility module,
  which extracts multimedia resources from tads game files.

  For an example of how to use this module, please refer to the TEST
  definitions at the bottom of this file - this gives a small sample
  program that loads game information from a file and displays each
  field on the standard output.  For sample build commands, refer to
  mk_gameinfo.bat, which builds the test program for Win32 console
  mode using MS Visual C++.
Notes

Modified
  09/24/01 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>

#include <os.h>
#include "t3std.h"
#include "utf8.h"

#include "resfind.h"
#include "gameinfo.h"

/* ------------------------------------------------------------------------ */
/*
 *   Name/value pair list entry 
 */
struct tads_valinfo
{
    const char *name;
    size_t name_len;

    /* value string */
    const char *val;

    /* next entry in the list */
    tads_valinfo *nxt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Create 
 */
CTadsGameInfo::CTadsGameInfo()
{
    /* no buffer yet */
    buf_ = 0;

    /* no name/value pairs yet */
    first_val_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete 
 */
CTadsGameInfo::~CTadsGameInfo()
{
    /* if we have a buffer, delete it */
    if (buf_ != 0)
        osfree(buf_);

    /* free the name/value pair list */
    free_value_list();
}

/*
 *   Delete the list.  If a subclass overrides free_value(), it should call
 *   this routine from its overridden destructor, to ensure that the vtable
 *   still has a reference to the subclass version of free_value() when this
 *   is called.
 *   
 *   Since this routine deletes each entry and leaves us with an empty list,
 *   it's harmless to call this more than once; each subclass can thus
 *   safely call this from its own destructor, even if it thinks its own
 *   superclass or subclasses might call it as well.  
 */
void CTadsGameInfo::free_value_list()
{
    while (first_val_ != 0)
    {
        /* remember the next one */
        tads_valinfo *nxt = first_val_->nxt;

        /* 
         *   delete this one - delete its value string and then the
         *   structure itself 
         */
        free_value(first_val_->val);
        osfree(first_val_);

        /* move on to the next */
        first_val_ = nxt;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a game file and retrieve game information
 */
int CTadsGameInfo::read_from_fp(osfildef *fp)
{
    /* find the GameInfo.txt resource */
    tads_resinfo resinfo;
    if (!tads_find_resource_fp(fp, "GameInfo.txt", &resinfo))
    {
        /* no GameInfo.txt resource - there's no game information */
        return FALSE;
    }

    /* parse the file and return the result code */
    return parse_file(fp, resinfo.seek_pos, resinfo.siz);
}

/*
 *   Parse a game file and retrieve game information
 */
int CTadsGameInfo::read_from_file(const char *fname)
{
    /* open the file */
    osfildef *fp;
    if ((fp = osfoprb(fname, OSFTGAME)) == 0
        && (fp = osfoprb(fname, OSFTT3IMG)) == 0)
    {
        /* 
         *   we can't open the file, so we obviously can't parse it to find
         *   game information 
         */
        return 0;
    }

    /* parse the file and find the game information */
    int ret = read_from_fp(fp);

    /* we're done with the file - close it */
    osfcls(fp);

    /* return the results from the parser */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Is this a vertical whitespace character?  
 */
static int is_vspace(wchar_t ch)
{
    return ch == '\n' || ch == '\r' || ch == 0x2028;
}

/*
 *   Is this a horizontal whitespace character (i.e., any whitespace
 *   character other than a newline character of some kind)?  
 */
static int is_hspace(wchar_t ch)
{
    /* 
     *   it's a horizontal whitespace character if it's a whitespace
     *   character and it's not a vertical whitespace character 
     */
    return is_space(ch) && !is_vspace(ch);
}

/* ------------------------------------------------------------------------ */
/*
 *   Skip a newline sequence.  We'll skip the longest of the following
 *   sequences we find: CR, LF, CR-LF, LF-CR, 0x2028. 
 */
static void skip_newline(utf8_ptr *p, size_t *rem)
{
    /* if there's nothing left, there's nothing to do */
    if (*rem == 0)
        return;

    /* check what we have and skip accordingly */
    switch(p->getch())
    {
    case '\n':
        /* 
         *   it's an LF, so skip it and check for a CR; if a CR immediately
         *   follows, consider the entire LF-CR to be a single newline, so
         *   skip the CR as well 
         */
        p->inc(rem);
        if (*rem != 0 && p->getch() == '\r')
            p->inc(rem);
        break;

    case '\r':
        /* 
         *   it's a CR, so skip it and check for an LF; if an LF immediately
         *   follows, consider the entire CR-LF to be a single newline, so
         *   skip the LF as well 
         */
        p->inc(rem);
        if (*rem != 0 && p->getch() == '\n')
            p->inc(rem);
        break;

    case 0x2028:
        /* 
         *   it's a Unicode line separator character - this indicates a
         *   newline but cannot be used in combination with any other
         *   characters to form multi-character newline sequences, so simply
         *   skip this one and we're done 
         */
        p->inc(rem);
        break;

    default:
        /* we're not on a newline at all, so there's nothing for us to do */
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Skip to the start of the next line 
 */
static void skip_to_next_line(utf8_ptr *p, size_t *rem)
{
    /* find the next line-ending sequence */
    for ( ; *rem != 0 ; p->inc(rem))
    {
        /* get the current character */
        wchar_t ch = p->getch();

        /* check to see if we've reached a newline yet */
        if (is_vspace(ch))
        {
            /* skip the entire newline sequence */
            skip_newline(p, rem);

            /* we're done looking for the end of the line */
            break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a file 
 */
int CTadsGameInfo::parse_file(osfildef *fp, unsigned long res_seek_pos,
                              unsigned long res_size)
{
    /* find the tail of the existing list */
    tads_valinfo *last_val;
    for (last_val = first_val_ ; last_val != 0 && last_val->nxt != 0 ;
         last_val = last_val->nxt) ;
    
    /* we found the resource - seek to it in the file */
    if (osfseek(fp, res_seek_pos, OSFSK_SET))
        return FALSE;

    /* 
     *   Allocate a block of memory for loading the game information.  The
     *   game information resource is typically fairly small, so for
     *   simplicity we'll just allocate a single block of memory and load
     *   the whole resource into the block.  Allocate space for one extra
     *   byte, so that we can ensure we have a newline at the end of the
     *   buffer.  
     */
    buf_ = (char *)osmalloc(res_size + 1);
    if (buf_ == 0)
        return FALSE;

    /* read the data */
    if (osfrb(fp, buf_, res_size))
        return FALSE;

    /* 
     *   store an extra newline at the end of the buffer, so that we can be
     *   certain that the last line ends in a newline - at worst, this will
     *   add an extra blank line to the end, but since we ignore blank lines
     *   this will do no harm 
     */
    buf_[res_size++] = '\n';

    /* parse the data */
    utf8_ptr p(buf_);
    for (size_t rem = res_size ; rem != 0 ; )
    {
        /* skip any leading whitespace */
        while (rem != 0 && is_space(p.getch()))
            p.inc(&rem);

        /* if the line starts with '#', it's a comment, so skip it */
        if (rem != 0 && p.getch() == '#')
        {
            /* skip the entire line, and go back for the next one */
            skip_to_next_line(&p, &rem);
            continue;
        }

        /* we must have the start of a name - note it */
        utf8_ptr name_start = p;

        /* skip ahead to a space or colon */
        while (rem != 0 && p.getch() != ':' && !is_hspace(p.getch()))
            p.inc(&rem);

        /* note the length of the name */
        size_t name_len = p.getptr() - name_start.getptr();

        /* skip any whitespace before the presumed colon */
        while (rem != 0 && is_hspace(p.getch()))
            p.inc(&rem);

        /* if we're not at a colon, the line is ill-formed, so skip it */
        if (rem == 0 || p.getch() != ':')
        {
            /* skip the entire line, and go back for the next one */
            skip_to_next_line(&p, &rem);
            continue;
        }

        /* skip the colon and any whitespace immediately after it */
        for (p.inc(&rem) ; rem != 0 && is_hspace(p.getch()) ; p.inc(&rem)) ;

        /* 
         *   Whatever terminated the name, replace it with a null character
         *   - this is safe, since we at least have a colon character to
         *   replace, and we've already skipped it so we can overwrite it
         *   now.  A null character in utf-8 is simply a single zero byte,
         *   so we can store our byte directly without worrying about any
         *   utf-8 multi-byte strangeness.  
         */
        *(name_start.getptr() + name_len) = '\0';

        /* note where the value starts */
        utf8_ptr val_start = p;

        /* set up to write to the buffer at the current point */
        utf8_ptr dst = p;

        /*
         *   Now find the end of the value.  The value can span multiple
         *   lines; if we find a newline followed by a space, it means that
         *   the next line is a continuation of the current value, and that
         *   the entire sequence of newlines and immediately following
         *   whitespace should be converted to a single space in the value.
         *   
         *   Note that we copy the transformed value directly over the old
         *   version of the value in the buffer.  This is safe because the
         *   transformation can only remove characters - we merely collapse
         *   each newline-whitespace sequence into a single space.  
         */
        while (rem != 0)
        {
            /* get this character */
            wchar_t ch = p.getch();
            
            /* check for a newline */
            if (is_vspace(ch))
            {
                /* 
                 *   it's a newline - skip it (and any other characters in
                 *   the newline sequence) 
                 */
                skip_newline(&p, &rem);

                /* 
                 *   if there's no leading whitespace on the next line,
                 *   we've reached the end of this value
                 */
                if (rem == 0 || !is_hspace(p.getch()))
                {
                    /* 
                     *   no whitespace -> end of the value - stop scanning
                     *   the value 
                     */
                    break;
                }

                /* skip leading whitespace on the line */
                while (rem != 0 && is_hspace(p.getch()))
                    p.inc(&rem);

                /* 
                 *   add a single whitespace character to the output for the
                 *   entire sequence of the newline plus the leading
                 *   whitespace on the line 
                 */
                dst.setch(' ');
            }
            else if (ch == 0)
            {
                /* change null bytes to spaces */
                dst.setch(' ');

                /* skip this character */
                p.inc(&rem);
            }
            else
            {
                /* it's not a newline - simply copy it out and keep going */
                dst.setch(ch);

                /* skip this character */
                p.inc(&rem);
            }
        }

        /* 
         *   Store a null terminator at the end of the value (it's safe to
         *   write this to the buffer because at worst it'll overwrite the
         *   newline at the end of the last line, which we've already
         *   skipped in the source).  Note that we obtain the length of the
         *   value string before storing the null terminator, because we
         *   don't want to count the null byte in the value's length.  
         */
        dst.setch('\0');

        /* 
         *   Create a new value list entry.  Point the entry directly into
         *   our buffer, since we're keeping the buffer around as long as
         *   the value list is around. 
         */
        tads_valinfo *val_info =
            (tads_valinfo *)osmalloc(sizeof(tads_valinfo));
        val_info->name = name_start.getptr();
        val_info->name_len = name_len;
        val_info->val = store_value(val_start.getptr(),
                                    dst.getptr() - val_start.getptr());
        val_info->nxt = 0;

        /* link the new value at the end of our list */
        if (last_val != 0)
            last_val->nxt = val_info;
        else
            first_val_ = val_info;
        last_val = val_info;
    }

    /* success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up a a value by name, returning a pointer to the value string.
 */
const char *CTadsGameInfo::get_val(const char *name) const
{
    /* for efficiency, note the length of the name up front */
    size_t name_len = strlen(name);

    /* scan our list of value entries for the given name */
    for (tads_valinfo *cur = first_val_ ; cur != 0 ; cur = cur->nxt)
    {
        /* 
         *   If the name matches, return the value for this entry.  Note
         *   that, since the name is always a plain ASCII string, we can
         *   perform a simple byte-by-byte case-insensitive comparison using
         *   memicmp. 
         */
        if (cur->name_len == name_len
            && memicmp(cur->name, name, name_len) == 0)
        {
            /* it's the one - return its value */
            return cur->val;
        }
    }

    /* didn't find it - return null */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Enumerate all name/value pairs 
 */
void CTadsGameInfo::enum_values(CTadsGameInfo_enum *cb)
{
    /* invoke the callback for each value in our list */
    for (tads_valinfo *cur = first_val_ ; cur != 0 ; cur = cur->nxt)
        cb->tads_enum_game_info(cur->name, cur->val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Testing routine.  We'll find the game information in a given file, then
 *   display all of the strings.  We'll convert the values to the display
 *   character set before showing them.  
 */
#ifdef TEST
#include "charmap.h"
#include "resload.h"
#include "vmimage.h" // $$$ for our vmimage substitution

/*
 *   name/value pair enumeration interface implementation - this is an
 *   implementation for the testing program that merely displays the
 *   name/value pair on the standard output 
 */
class CTestEnum: public CTadsGameInfo_enum
{
public:
    /* 
     *   CTadsGameInfo_enum implementation 
     */

    /* receive a name/value pair in the enumeration */
    virtual void tads_enum_game_info(const char *name, const char *val)
    {
        /* display the name/value pair */
        printf("%s = %s\n", name, val);
    }
};

/*
 *   main test program entrypoint 
 */
int main(int argc, char **argv)
{
    /* check usage */
    if (argc != 2)
    {
        printf("usage: gameinfo <gamefile>\n");
        exit(2);
    }

    /* get the arguments */
    const char *fname = argv[1];

    /* 
     *   set up a game information reader that translates to the default
     *   local display character set 
     */
    CTadsGameInfo *info = new CTadsGameInfoLocal(argv[0]);

    /* load the game information object, if possible */
    if (!info->read_from_file(fname))
    {
        printf("unable to load game information from file \"%s\"\n", fname);
        delete info;
        exit(2);
    }

    /* enumerate the name/value pairs through our callback implementation */
    CTestEnum cb;
    info->enum_values(&cb);

    /* we're done with our game information reader - delete it */
    delete info;

    /* success */
    exit(0);
    return 0;
}

#endif /* TEST */
