/*
** os_glk.c - Definitions which are specific to the Glk 'platform'
**
** Notes:
**   10 Jan 99: SRG Initial creation
**    3 Dec 01: SRG Added os_build_full_path
**   20 Dec 01: SRG Added os_get_exe_filename and os_get_special_path
**   10 Mar 02: SRG Added os_more_prompt routine (thanks to D.J. Picton)
*/

#include "run.h"
#include "os_glk.h"
#include "oss_glk.h"
#include <unistd.h>
#include <stdlib.h>


/* ------------------------------------------------------------------------ */
/*
** Some machines are missing memmove, so we use our own memcpy/memmove
** routine instead.
*/
void *our_memcpy(void *destp, const void *srcp, size_t size)
{
    char *dest = (char *)destp;
    char *src = (char *)srcp;
    size_t n;
    
    n = size;
    if (dest < src) {
        while (n > 0) {
            *dest = *src;
            dest++;
            src++;
        n--;
        }
    }
    else if (dest > src) {
        for (src += n, dest += n; n > 0; n--) {
            dest--;
            src--;
            *dest = *src;
        }
    }
    
    return destp;
}


/* ------------------------------------------------------------------------ */
/*
** File handling
*/

/* Ask the user for a filename. Return 0 if successful, non-zero otherwise */
int os_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
               int prompt_type, int file_type)
{
    frefid_t fileref;
    glui32 glk_prompt_type, glk_file_type;

    glk_prompt_type = oss_convert_prompt_type(prompt_type);
    glk_file_type = oss_convert_file_type(file_type) |
    fileusage_TextMode;

    fileref = glk_fileref_create_by_prompt(glk_file_type, glk_prompt_type, 0);
    if (fileref == NULL)
        return OS_AFE_CANCEL;
    return (oss_convert_fileref_to_string(fileref, fname_buf, fname_buf_len)
            == FALSE ? OS_AFE_FAILURE : OS_AFE_SUCCESS);
}

/* Create and open a temporary file. Theoretically I should use the
 filename in swapname or generate my own and put it in buf */
osfildef *os_create_tempfile(const char *swapname, char *buf)
{
    frefid_t fileref;
    strid_t  stream;

    fileref = glk_fileref_create_temp(fileusage_Data | fileusage_BinaryMode,
                                      0);
    stream = glk_stream_open_file(fileref, filemode_ReadWrite, 0);
    glk_fileref_destroy(fileref);
    return stream;
}

/* Delete a temp file that was created by os_create_tempfile. Since
   Glk automatically deletes temporary files, I have no need of this */
int osfdel_temp(const char *fname)
{
    return TRUE;
}

/* Get a path to put a temp file--too bad we don't use paths...most of
   the time */
void os_get_tmp_path(char *buf)
{
    buf[0] = 0;
}

/* Locate a file and, if the search is successful, store the resulting
   name in the given buffer. Return TRUE if the file was found, FALSE
   otherwise. Since Glk doesn't concern itself with paths, this is an
   easy check */
int os_locate(const char *fname, int flen, const char *arg0, char *buf,
          size_t bufsiz)
{
    if (!osfacc(fname)) {
        memcpy(buf, fname, flen);
        buf[flen] = 0;
        return TRUE;
    }
    return FALSE;
}

/* open text file for reading; returns NULL on error */
osfildef *osfoprt(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_TextMode,
                            filemode_Read, 0));
}

/* open text file for writing; returns NULL on error */
osfildef *osfopwt(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_TextMode,
                            filemode_Write, 0));
}

/* open text file for reading and writing; returns NULL on error */
osfildef *osfoprwt(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_TextMode,
                            filemode_ReadWrite, 0));
}

/* open text file for reading/writing; truncate; returns NULL on error */
osfildef *osfoprwtt(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_TextMode,
                            filemode_ReadWrite, 0));
}

/* open binary file for writing; returns NULL on error */
osfildef *osfopwb(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_BinaryMode,
                            filemode_Write, 0));
}

/* open source file for reading */
osfildef *osfoprs(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_BinaryMode,
                            filemode_Read, 0));
}

/* open binary file for reading; returns NULL on erorr */
osfildef *osfoprb(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_BinaryMode,
                            filemode_Read, 0));
}

/* open binary file for reading/writing; don't truncate */
osfildef *osfoprwb(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_BinaryMode,
                            filemode_ReadWrite, 0));
}

/* open binary file for reading/writing; truncate; returns NULL on error */
osfildef *osfoprwtb(char *fname, glui32 typ)
{
    return (oss_open_stream(fname, typ, fileusage_BinaryMode,
                            filemode_ReadWrite, 0));
}

/* delete a file - TRUE if error */
int osfdel(char *fname)
{
    frefid_t fileref;
    int changed_dirs;

    fileref = oss_convert_string_to_fileref(fname, fileusage_BinaryMode);
    changed_dirs = oss_check_path(fname);
    glk_fileref_delete_file(fileref);
    glk_fileref_destroy(fileref);
    if (changed_dirs)
        oss_revert_path();
    return FALSE;
}

/* access a file - 0 if file exists */
int osfacc(const char *fname)
{
    frefid_t fileref;
    int      result, changed_dirs;

    fileref = oss_convert_string_to_fileref(fname, fileusage_BinaryMode);
    changed_dirs = oss_check_path(fname);
    result = (int)glk_fileref_does_file_exist(fileref);
    if (changed_dirs)
        oss_revert_path();
    return (result != TRUE);
}


/* build a full path name given a path and a filename */
/* (NB much of this comes from osnoui.c) */
void os_build_full_path(char *fullpathbuf, size_t fullpathbuflen,
                        const char *path, const char *filename)
{
    size_t plen, flen;
    int add_sep;

    /* I'm defining this just for the Unix version of GLK */
#ifdef GLKUNIX

    /*
     *   Note whether we need to add a separator.  If the path prefix ends
     *   in a separator, don't add another; otherwise, add the standard
     *   system separator character.
     *   
     *   Do not add a separator if the path is completely empty, since this
     *   simply means that we want to use the current directory.  
     */
    plen = strlen(path);
    add_sep = (plen != 0
               && path[plen-1] != OSPATHCHAR
               && strchr(OSPATHALT, path[plen-1]) == 0);

    /* copy the path to the full path buffer, limiting to the buffer length */
    if (plen > fullpathbuflen - 1)
        plen = fullpathbuflen - 1;
    memcpy(fullpathbuf, path, plen);

    /* add the path separator if necessary (and if there's room) */
    if (add_sep && plen + 2 < fullpathbuflen)
        fullpathbuf[plen++] = OSPATHCHAR;

    /* add the filename after the path, if there's room */
    flen = strlen(filename);
    if (flen > fullpathbuflen - plen - 1)
        flen = fullpathbuflen - plen - 1;
    memcpy(fullpathbuf + plen, filename, flen);

    /* add a null terminator */
    fullpathbuf[plen + flen] = '\0';
#endif /* GLKUNIX */
}

/* 
 *   Given argv[0], what's the executable's full pathname? The problem under
 *   Unix is that we don't necessarily know. If the interpreter is invoked
 *   using an alias, argv[0] will (under many shells) contain that alias. So
 *   we're going to return failure regardless. The good news is that we only
 *   lose the ability to create bound games, which I don't expect to be an
 *   issue under Unix. If you port Glk TADS to another platform where this
 *   is not the case, feel free to adjust this routine appropriately.  
 */
int os_get_exe_filename(char *buf, size_t buflen, const char *argv0)
{
    buf[0] = 0;
    return FALSE;
}

/* 
 *   Get a special path (e.g. path to standard include files or libraries).
 *   Valid id values include OS_GSP_T3_RES, OS_GSP_T3_LIB, and
 *   OS_GSP_T3_INC. If certain compiler variables are set, use those
 *   hardcoded paths. Otherwise, check for environment variables; if those
 *   aren't set, return argv0's path. Note that this is only for Unix Glk 
 */
void os_get_special_path(char *buf, size_t buflen, const char *argv0, int id)
{
    const char *str;
    char *p;

#ifdef GLKUNIX
    switch(id) {
#ifdef DEFINEDIRS
    case OS_GSP_T3_RES:
        str = RESDIR;
        break;
    case OS_GSP_T3_LIB:
        str = LIBDIR;
        break;
    case OS_GSP_T3_INC:
        str = INCDIR;
        break;
#else /* DEFINEDIRS */
    case OS_GSP_T3_RES:
        str = getenv("TADS3_RESDIR");
        break;
    case OS_GSP_T3_LIB:
        str = getenv("TADS3_LIBDIR");
        break;
    case OS_GSP_T3_INC:
        str = getenv("TADS3_INCLUDEDIR");
        break;
#endif /* DEFINEDIRS */

    case OS_GSP_T3_APP_DATA:
        /* for application data, use the executable directory */
        os_get_path_name(buf, buflen, argv0);
        break;
        
    default:
        /*
         *   If we're called with another identifier, it must mean that
         *   we're out of date.  Fail with an assertion.  
         */
        assert(FALSE);
    }

#ifndef DEFINEDIRS
    if (str == NULL) {
        strcpy(buf, argv0);
        p = buf + strlen(argv0) - 1;
                /* Move backwards until we find a slash in argv0 or its beginning */
        while (p != buf && *p != '/') {
            p--;
        }
        if (*p == '/' && p != buf) {
            *p = (char)NULL;
        }
    }
#endif /* DEFINEDIRS */

    if (strlen(str) >= buflen)
        assert(FALSE);
    strcpy(buf, str);
#endif /* GLKUNIX */
}


/* ------------------------------------------------------------------------ */
/*
** File extension and path fiddling
*/

/* I'm defining these functions for the Unix Glk libraries. Your port may
   not need them at all */

void os_defext(char *fname, const char *ext)
{
#ifdef GLKUNIX
    os_addext(fname, ext);
#endif /* GLKUNIX */
}

void os_addext(char *fname, const char *ext)
{
#ifdef GLKUNIX
    char buf[5], *p;

    /* Don't do any fiddling if the passed string is a hashed fileref or
       if there is already an extension on the file */
    if (oss_is_string_a_fileref(fname))
        return;
    p = fname + strlen(fname) - 1;
    while (p > fname && *p != '.' && *p != '/')
        p--;
    if (*p == '.')
        return;
    
    strcat(fname, ".");                   /* Append a dot and the extension */
    strcpy(buf, ext);           /* Make the extension lower-case by default */
    os_strlwr(buf);
    strcat(fname, buf);
#endif /* GLKUNIX */
}

void os_remext(char *fname)
{
#ifdef GLKUNIX
    char *p;

    /* Don't do any fiddling if the passed string is a hashed fileref */
    if (oss_is_string_a_fileref(fname))
        return;
    
    p = fname + strlen(fname);
    while (p != fname) {
        p--;
        if (*p == '.') {
            *p = 0;
            return;
        }
        /* If we run into a directory separator, return */
        if (*p == '/' || *p == '\\' || *p == ':')
            return;
    }
#endif /* GLKUNIX */
}

/* Get a pointer to the root name portion of a filename */
char *os_get_root_name(char *buf)
{
    char *p = buf;

#ifdef GLKUNIX
    p += strlen(buf) - 1;
    while (*p != '/' && p > buf)
        p--;
    if (p != buf) p++;
#endif /* GLKUNIX */
    return p;
}


/* ------------------------------------------------------------------------ */
/*
** Text handling and I/O
*/

/*
** The main text area print routines
*/
void os_printz(const char *str)
{
    /* print the string through the base counted-length print routine */
    os_print(str, strlen(str));
}

void os_print(const char *str, size_t len)
{
    int current_status_mode;

    /* Decide what to do based on our status mode */
    current_status_mode = os_get_status();
    if (current_status_mode == OSS_STATUS_MODE_STORY) {
        oss_put_string_with_hilite(story_win, str, len);
    }
    else if (current_status_mode == OSS_STATUS_MODE_STATUS) {
        const char *p;
        size_t      rem;
        
        /* The string requires some fiddling for the status window */
        for (p = str, rem = len ; rem != 0 && *p == '\n'; p++, --rem) ;
        if (rem != 0 && p[rem-1] == '\n')
            --rem;

        /* if that leaves anything, update the statusline */
        if (rem != 0)
            oss_change_status_left(p, rem);
    }
}

void os_fprintz(osfildef *fp, const char *str)
{
    glk_put_string_stream(fp, str);
}

void os_fprint(osfildef *fp, const char *str, size_t len)
{
    glk_put_buffer_stream(fp, str, len);
}

/* Convert a string to all-lowercase */
char *os_strlwr(char *s)
{
    char *sptr;

    sptr = s;
    while (*sptr != 0) {
        *sptr = glk_char_to_lower((unsigned char)*sptr);
        sptr++;
    }
    return s;
}

/* Show a [MORE] prompt */
void os_more_prompt(void)
{
    int done;

    /* display the "MORE" prompt */
    os_printz("[More]");
    os_flush();

    /* wait for a keystroke */
    for (done = FALSE; !done; )
    {
        os_event_info_t evt;

        /* get an event */
        switch(os_get_event(0, FALSE, &evt))
        {
        case OS_EVT_KEY:
            /* stop waiting, show one page */
            done = TRUE;
            break;

        case OS_EVT_EOF:
            /* end of file - there's nothing to wait for now */
            done = TRUE;
            break;

        default:
            /* ignore other events */
            break;
        }
    }
    os_printz("\n");
    os_flush();
}

/* ------------------------------------------------------------------------ */
/*
** Keyboard I/O
*/

/* Read in a line of text from the keyboard */
unsigned char *os_gets(unsigned char *buf, size_t bufl)
{
    event_t ev;

    glk_request_line_event(story_win, buf, (glui32) bufl - 1, 0);
    do {
        glk_select(&ev);
        if (ev.type == evtype_Arrange)
            oss_draw_status_line();
    } while (ev.type != evtype_LineInput);
    buf[ev.val1] = 0;                     /* Don't forget the trailing NULL */
    
    return buf;
}

/* Get a character from the keyboard. For extended characters, return 0,
   then return the extended key at the next call to this function */
int os_getc(void)
{
    return (oss_getc_from_window(story_win));
}

/* Get a character from the keyboard, returning low-level, untranslated
   key codes. Since we don't deal with anything but low-level codes,
   this is exactly the same as os_getc. */
int os_getc_raw(void)
{
    return os_getc();
}

/* Wait for a key to be hit */
void os_waitc(void)
{
    os_getc();
}

/* Here's the biggie. Get an input event, which may or may not be timed
   out. timeout is in milliseconds */
int os_get_event(unsigned long timeout, int use_timeout,
         os_event_info_t *info)
{
    event_t ev;

    glk_request_char_event(story_win);
    if (flag_timer_supported && use_timeout)
        glk_request_timer_events((glui32)timeout);
    do {
        glk_select(&ev);
        if (ev.type == evtype_Arrange)
            oss_draw_status_line();
    } while (ev.type != evtype_Timer && ev.type != evtype_CharInput);
    glk_cancel_char_event(story_win);                       /* Just in case */
    if (flag_timer_supported)             /* If we support timers, stop 'em */
        glk_request_timer_events(0);
    if (ev.type == evtype_Timer)
        return OS_EVT_TIMEOUT;
    if (ev.val1 == keycode_Return)
        ev.val1 = '\n';
    else if (ev.val1 == keycode_Tab)
        ev.val1 = '\t';
    if (ev.val1 <= 255)
        info->key[0] = (int)(ev.val1);
    else {
        info->key[0] = 0;
        info->key[1] = (int)(oss_convert_keystroke_to_tads(ev.val1));
    }
    return OS_EVT_KEY;
}


/* ------------------------------------------------------------------------ */
/*
** Status line functions
*/

/* Set the status mode */
void os_status(int stat)
{
    status_mode = stat;
}

/* Query the status mode */
int os_get_status()
{
    return status_mode;
}

/* Display a string in the score area (rightmost) of the status line */
void os_strsc(const char *p)
{
    if (p == NULL)              /* NULL means simply refresh the status */
        oss_draw_status_line();
    else oss_change_status_right(p);
}

/* Set the score. If score == -1, use the last score */
void os_score(int cur, int turncount)
{
    char buf[20];

    if (turncount == -1)       /* -1 means simply refresh the status */
        oss_draw_status_line();
    else {
        sprintf(buf, "%d/%d", cur, turncount);
        oss_change_status_right(buf);
    }
}


/* ------------------------------------------------------------------------ */
/*
** Other misc functions
*/

void os_rand(long *seed)
{
    time_t t;

    time(&t);
    *seed = (long)t;
}

long os_get_sys_clock_ms(void)
{
    if (time_zero == 0)
        time_zero = time(0);
    return ((time(0) - time_zero) * 1000);
}

void os_sleep_ms(long delay_in_ms)
{
    glk_tick();
    usleep(delay_in_ms);
    glk_tick();
}

void os_set_text_attr(int attr)
{
    /* ensure that we are acting on the story window */
    glk_set_window(story_win);

    /* map highlighting/bold/emphasized to "emphasized" style */
    if ((attr & (OS_ATTR_HILITE | OS_ATTR_BOLD | OS_ATTR_EM)) != 0)
        glk_set_style(style_Emphasized);
    else
        glk_set_style(style_Normal);
}

void os_set_text_color(os_color_t fg, os_color_t bg)
{
    /* glk does not have a way to set colors explicitly */
}

void oscls(void)
{
    glk_window_clear(story_win);
}

/* Flush the output */
void os_flush(void)
{
    glk_tick();
}

/* update the display - process any painting events immediately */
void os_update_display()
{
    glk_tick();
}

int os_yield(void)
{
    glk_tick();
    return FALSE;
}

void os_expause(void)
{
#ifdef USE_EXPAUSE
    os_printz("(Strike any key to exit...)");
    os_flush();
    os_waitc();
#endif /* USE_EXPAUSE */
}

int os_get_sysinfo(int code, void *parm, long *result)
{
    switch(code) {
    case SYSINFO_TEXT_HILITE:
        /* we do support text highlighting */
        *result = 1;
        return TRUE;

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
    case SYSINFO_BANNERS:
        /* Since we support none of these, set result to 0 */
        *result = 0;
        return TRUE;                              /* We recognized the code */
    case SYSINFO_INTERP_CLASS:
        /* we're a text-only character-mode interpreter */
        /* 
         *   $$$ we might want to be more specific: if it's possible to
         *   determine whether we're running on a character-mode or GUI
         *   platform, we should indicate type TEXT or TEXTGUI as
         *   appropriate.  There's no practical difference between these
         *   classes at the moment, though, so it's not very important to
         *   distinguish them. 
         */
        *result = SYSINFO_ICLASS_TEXT;
        return TRUE;
    default:
        return FALSE;
    }
}

void os_xlat_html4(unsigned int html4_char, char *result, size_t result_len)
{
    /* Return all standard Latin-1 characters as-is */
    if (html4_char <= 128 || (html4_char >= 160 && html4_char <= 255))
        result[0] = (unsigned char)html4_char;
    else {
        switch (html4_char) {
        case 130:                                      /* single back quote */
            result[0] = '`'; break;
        case 132:                                      /* double back quote */
            result[0] = '\"'; break;
        case 153:                                             /* trade mark */
            strcpy(result, "(tm)"); return;
        case 140:                                            /* OE ligature */
        case 338:                                            /* OE ligature */
            strcpy(result, "OE"); return;
        case 339:                                            /* oe ligature */
            strcpy(result, "oe"); return;
        case 159:                                                   /* Yuml */
            result[0] = 255;
        case 376:                                        /* Y with diaresis */
            result[0] = 'Y'; break;
        case 352:                                           /* S with caron */
            result[0] = 'S'; break;
        case 353:                                           /* s with caron */
            result[0] = 's'; break;
        case 150:                                                /* en dash */
        case 8211:                                               /* en dash */
            result[0] = '-'; break;
        case 151:                                                /* em dash */
        case 8212:                                               /* em dash */
            strcpy(result, "--"); return;
        case 145:                                      /* left single quote */
        case 8216:                                     /* left single quote */
            result[0] = '`'; break;
        case 146:                                     /* right single quote */
        case 8217:                                    /* right single quote */
        case 8218:                                    /* single low-9 quote */
            result[0] = '\''; break;
        case 147:                                      /* left double quote */
        case 148:                                     /* right double quote */
        case 8220:                                     /* left double quote */
        case 8221:                                    /* right double quote */
        case 8222:                                    /* double low-9 quote */
            result[0] = '\"'; break;
        case 8224:                                                /* dagger */
        case 8225:                                         /* double dagger */
        case 8240:                                        /* per mille sign */
            result[0] = ' '; break;
        case 139:                       /* single left-pointing angle quote */
        case 8249:                      /* single left-pointing angle quote */
            result[0] = '<'; break;
        case 155:                      /* single right-pointing angle quote */
        case 8250:                     /* single right-pointing angle quote */
            result[0] = '>'; break;
        case 8482:                                           /* small tilde */
            result[0] = '~'; break;
            
        default:
            /* unmappable character - return space */
            result[0] = (unsigned char)' ';
        }
    }
    result[1] = 0;
}

void os_gen_charmap_filename(char *filename, char *internal_id, char *argv0)
{
    filename[0] = 0;
}

/* ------------------------------------------------------------------------ */
/*
** Some empty routines that we have to have just because
*/
/* Set the title of the story window */
void os_set_title(const char *title) {}

/* Seek to the game file embedded in the given executable file */
osfildef *os_exeseek(const char *exefile, const char *typ) { return NULL; }

/* Load an external function from a file, given the name of the file */
int (*os_exfil(const char *name))(void *) { return (int (*)(void *))NULL; }

/* Load an external function from an open file */
int (*os_exfld(osfildef *fp, unsigned len))(void *)
{
    return (int (*)(void *))NULL;
}

/* Call an external function */
int os_excall(int (*extfn)(void *), void *arg) { return 0; }

/* Check for user break */
int os_break(void) { return FALSE; }

/* Get a filename from a startup parameter, if possible. Which it isn't */
int os_paramfile(char *buf) { return FALSE; }

/* Set the terminal into "plain" mode */
void os_plain(void) {}

/* Set the saved game extension. Sha, as if. */
void os_set_save_ext(const char *ext) {}

/* Set a file's filetype */
void os_settype(const char *f, int t) {}

/* Find the first file in a directory, given a wildcard pattern */
void *os_find_first_file(const char *dir, const char *pattern,
                         char *outbuf, size_t outbufsiz, int *isdir,
                         char *outpathbuf, size_t outpathbufsiz) {}

/* Find the next file */
void *os_find_next_file(void *ctx0, char *outbuf, size_t outbufsize,
        int *isdir, char *outpathbuf, size_t outpathbufsiz) {}

/* Cancel a search */
void os_find_close(void *ctx0) {}

/* Character map loading */
void os_advise_load_charmap(char *id, char *ldesc, char *sysinfo) {}

/* TK I should be able to remove these eventually, when Glk is updated */
int osfwb(osfildef *fp, unsigned char *buf, int bufl)
{
    glk_put_buffer_stream(fp, buf, (glui32)bufl);
    return FALSE;
}
int osfseek(osfildef *fp, long pos, int mode)
{
    glk_stream_set_position(fp, (glsi32)pos, mode);
    return FALSE;
}
int osfputs(char *buf, osfildef *fp)
{
    glk_put_string_stream(fp, buf);
    return 1;
}
