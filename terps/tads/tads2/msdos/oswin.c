#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/OSWIN.C,v 1.4 1999/07/11 00:46:37 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name

  oswin.c - os implementation for Windows
Function
  
Notes
  
Modified
  11/05/97 MJRoberts  - Creation
*/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <windows.h>
#include <Wincrypt.h>

#include "std.h"
#include "os.h"
#include "trd.h"

/* saved game file extension */
static char default_saved_game_ext[OSFNMAX] = "";

/* initial directory for os_askfile dialogs */
static char S_open_file_dir[OSFNMAX];

/* ------------------------------------------------------------------------ */
/*
 *   Application instance handle - the WinMain procedure (defined
 *   elsewhere) must define this variable and initialize it to the
 *   application instance handle.  
 */
extern HINSTANCE oss_G_hinstance;


/* ------------------------------------------------------------------------ */
/*
 *   Set the initial directory for os_askfile dialogs 
 */
void oss_set_open_file_dir(const char *dir)
{
    /* 
     *   if there's a string, copy it to our buffer; otherwise, clear our
     *   buffer 
     */
    if (dir != 0)
        strcpy(S_open_file_dir, dir);
    else
        S_open_file_dir[0] = '\0';
}

/* ------------------------------------------------------------------------ */
/* 
 *   dialog hook for standard file dialog - centers it on the screen 
 */
static UINT APIENTRY filedlg_hook(HWND dlg, UINT msg,
                                  WPARAM wpar, LPARAM lpar)
{
    /* if this is the post-initialization message, center the dialog */
    if (msg == WM_NOTIFY
        && ((NMHDR *)lpar)->code == CDN_INITDONE)
    {
        RECT deskrc;
        RECT dlgrc;
        int deskwid, deskht;
        int dlgwid, dlght;

        /* get the desktop area */
        GetWindowRect(GetDesktopWindow(), &deskrc);
        deskwid = deskrc.right - deskrc.left;
        deskht = deskrc.bottom - deskrc.top;

        /* get the dialog box area */
        GetWindowRect(((NMHDR *)lpar)->hwndFrom, &dlgrc);
        dlgwid = dlgrc.right - dlgrc.left;
        dlght = dlgrc.bottom - dlgrc.top;

        /* center the dialog on the screen */
        MoveWindow(((NMHDR *)lpar)->hwndFrom,
                   deskrc.left + (deskwid - dlgwid)/2,
                   deskrc.top + (deskht - dlght)/2, dlgwid, dlght, TRUE);

        /* message handled */
        return 1;
    }

    /* not handled */
    return 0;
}

/*
 *   Ask the user for a filename, using a system-dependent dialog or other
 *   mechanism.  Returns zero on success, non-zero on error.  
 */
int os_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
               int prompt_type, int file_type)
{
    OPENFILENAME info;
    int ret;
    char filter[256];
    const char *def_ext;
    const char *save_def_ext;
    static char lastsave[OSFNMAX] = "";

    /* presume we won't need a default extension */
    def_ext = 0;
    save_def_ext = 0;

    /* 
     *   Build the appropriate filter.  Most of the filters are boilerplate,
     *   but the "saved game" filter (for both TADS 2 and TADS 3 saved game
     *   files) is special because we need to use the dynamically-selected
     *   default saved game extension for our filter.  This means that the
     *   "unknown" type is equally complicated, because it needs to include
     *   the dynamic saved game extension as well.  
     */
    if (file_type == OSFTSAVE
        || file_type == OSFTT3SAV
        || file_type == OSFTUNK)
    {
        static const char filter_start[] =
            "Saved Game Positions\0*.";
        static const char filter_end_sav[] =
            "All Files\0*.*\0\0";
        static const char filter_end_unk[] =
            "TADS Games\0*.GAM\0"
            "Text Files\0*.TXT\0"
            "Log Files\0*.LOG\0"
            "All Files\0*.*\0\0";
        size_t totlen;
        size_t extlen;
        const char *save_filter;

        /* 
         *   Get the default saved game extension.  If the game program has
         *   explicitly set a custom extension, use that.  Otherwise, use the
         *   appropriate extension for the file type.  
         */
        if (default_saved_game_ext[0] != '\0')
        {
            /* 
             *   there's an explicit filter, so use it as the filter and as
             *   the default extension 
             */
            save_filter = default_saved_game_ext;
            save_def_ext = default_saved_game_ext;
        }
        else if (file_type == OSFTSAVE)
        {
            /* tads 2 saved game, so use the ".sav" suffix */
            save_filter = "sav";
            save_def_ext = "sav";
        }
        else if (file_type == OSFTT3SAV)
        {
            /* tads 3 saved game, so use the ".t3v" suffix */
            save_filter = "t3v";
            save_def_ext = "t3v";
        }
        else
        {
            /* 
             *   Unknown file type - use BOTH tads 2 and tads 3 saved game
             *   extensions for the filter, but do not apply any default
             *   extension to files we're creating.
             *   
             *   Note that we will append this filter to "*.", so the first
             *   extension in our list doesn't need the "*.", but the second
             *   one does.  
             */
            save_filter = "sav;*.t3v";
            save_def_ext = 0;
        }

        /* 
         *   Build the filter string for saved game files.  Note that we
         *   need to use memcpy rather than strcpy, because we need to
         *   include a bunch of embedded null characters within the
         *   string.  First, copy the prefix up to the saved game
         *   extension...  
         */
        memcpy(filter, filter_start, sizeof(filter_start) - 1);
        totlen = sizeof(filter_start) - 1;

        /* now add the saved game extension that we're to use... */
        extlen = strlen(save_filter) + 1;
        memcpy(filter + totlen, save_filter, extlen);
        totlen += extlen;
        
        /* finish with the whole rest of it */
        if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
        {
            /* saved game: use the saved game extra filter list */
            memcpy(filter + totlen, filter_end_sav, sizeof(filter_end_sav));

            /* 
             *   if we're creating a new saved game file, apply the default
             *   suffix to the name the user enters for the new file 
             */
            if (prompt_type == OS_AFP_SAVE)
                def_ext = save_def_ext;
        }
        else
        {
            /* not a saved game: use the unknown file type filter list */
            memcpy(filter + totlen, filter_end_unk, sizeof(filter_end_unk));
        }
    }
    else
    {
        static const struct
        {
            const char *disp;
            const char *pat;
            const char *def_ext;
        }
        filters[] =
        {
            { "TADS Games",      "*.gam", "gam" },              /* OSFTGAME */
            { 0, 0 },                                  /* unused - OSFTSAVE */
            { "Transcripts",     "*.log", "log" },               /* OSFTLOG */
            { "Swap Files",      "*.dat", "dat" },              /* OSFTSWAP */
            { "Data Files",      "*.dat", "dat" },              /* OSFTDATA */
            { "Command Scripts", "*.cmd", "cmd" },               /* OSFTCMD */
            { "Message Files",   "*.msg", "msg" },              /* OSFTERRS */
            { "Text Files",      "*.txt", "txt" },              /* OSFTTEXT */
            { "Binary Files",    "*.dat", "dat" },               /* OSFTBIN */
            { "Character Maps",  "*.tcp", "tcp" },              /* OSFTCMAP */
            { "Preference Files","*.dat", "dat" },              /* OSFTPREF */
            { 0, 0, 0 },                                /* unused - OSFTUNK */
            { "T3 Applications", "*.t3" /* was t3x */, "t3" }, /* OSFTT3IMG */
            { "T3 Object Files", "*.t3o", "t3o" },             /* OSFTT3OBJ */
            { "T3 Symbol Files", "*.t3s", "t3s" },             /* OSFTT3SYM */
            { 0, 0, 0 }                               /* unused - OSFTT3SAV */
        };
        static const char filter_end[] = "All Files\0*.*\0\0";
        char *p;
        
        /* 
         *   We have an ordinary, static filter -- choose the appropriate
         *   filter from our list.  Start with the display name...
         */
        strcpy(filter, filters[file_type].disp);

        /* add the file filter pattern after the null character */
        p = filter + strlen(filter) + 1;
        strcpy(p, filters[file_type].pat);

        /* add the filter terminator after the null */
        p += strlen(p) + 1;
        memcpy(p, filter_end, sizeof(filter_end));

        /* remember the default extension */
        save_def_ext = def_ext = filters[file_type].def_ext;
    }

    /* set up the open-file structure */
    info.lStructSize = sizeof(info);
    info.hwndOwner = GetActiveWindow();
    info.hInstance = oss_G_hinstance;
    info.lpstrFilter = filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname_buf;
    fname_buf[0] = '\0';
    info.nMaxFile = fname_buf_len;
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = (S_open_file_dir[0] != '\0' ? S_open_file_dir : 0);
    info.lpstrTitle = prompt;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = def_ext;
    info.lCustData = 0;
    info.lpfnHook = filedlg_hook;
    info.Flags = OFN_ENABLEHOOK | OFN_EXPLORER | OFN_NOCHANGEDIR;
    info.lpTemplateName = 0;

    /*
     *   If we're asking for a saved game file, pick a default.  If we've
     *   asked before, use the same default as last time; if not, use the
     *   game name as the default.  If we're opening a saved game rather then
     *   saving one, don't apply the default if the file we'd get by applying
     *   the default doesn't exist.  
     */
    if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
    {
        /* try using the last saved game name */
        strncpy(fname_buf, lastsave, fname_buf_len);
        fname_buf[fname_buf_len - 1] = '\0';

        /* if that's empty, generate a default name based on the game name */
        if (fname_buf[0] == '\0' && G_os_gamename[0] != '\0')
        {
            /* copy the game name as the base of the save-file name */
            strncpy(fname_buf, G_os_gamename, fname_buf_len);
            fname_buf[fname_buf_len - 1] = '\0';

            /* remove the old extension */
            os_remext(fname_buf);

            /* ... and replace it with the saved-game extension */
            if (save_def_ext != 0
                && strlen(fname_buf) + strlen(save_def_ext) + 1
                   < (size_t)fname_buf_len)
                os_defext(fname_buf, save_def_ext);
        }

        /* 
         *   if we're asking about a file to open, make sure our default
         *   exists - if not, don't offer it as the default, since it's a
         *   waste of time and attention for the user to have to change it 
         */
        if (prompt_type == OS_AFP_OPEN && osfacc(fname_buf))
            fname_buf[0] = '\0';
    }

    /* display the dialog */
    if (prompt_type == OS_AFP_SAVE)
    {
        info.Flags |= OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
        ret = GetSaveFileName(&info);
    }
    else
    {
        /* ask for a file to open */
        ret = GetOpenFileName(&info);
    }

    /* translate the result code to an OS_AFE_xxx value */
    if (ret == 0)
    {
        /* 
         *   an error occurred - check to see what happened: if the extended
         *   error is zero, it means that the user simply canceled the
         *   dialog, otherwise it means that an error occurred 
         */
        ret = (CommDlgExtendedError() == 0 ? OS_AFE_CANCEL : OS_AFE_FAILURE);
    }
    else
    {
        char path[OSFNMAX];

        /* success */
        ret = OS_AFE_SUCCESS;

        /* 
         *   remember the file's directory for the next file dialog (we need
         *   to do this explicitly because we're not allowing the dialog to
         *   set the working directory) 
         */
        os_get_path_name(path, sizeof(path), fname_buf);
        oss_set_open_file_dir(path);

        /* 
         *   if this is a saved game prompt, remember the response as the
         *   default for next time 
         */
        if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
            strcpy(lastsave, fname_buf);
    }

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   get filename from startup parameter, if possible; returns true and
 *   fills in the buffer with the parameter filename on success, false if
 *   no parameter file could be found 
 */
int os_paramfile(char *buf)
{
    /* this operation is not meaningful on windows */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Switch to a working directory 
 */
void os_set_pwd(const char *dir)
{
    SetCurrentDirectory(dir);
}

/*
 *   Switch the working directory to the path containing the given file 
 */
void os_set_pwd_file(const char *fname)
{
    char buf[OSFNMAX];
    char *p;
    char *last_sep;

    /* make a copy of the filename */
    strcpy(buf, fname);

    /* find the last path separator */
    for (p = buf, last_sep = 0 ; *p != '\0' ; ++p)
    {
        switch(*p)
        {
        case '/':
        case '\\':
        case ':':
            /* remember the location of this separator character */
            last_sep = p;
            break;
        }
    }

    /* 
     *   if we found a separator, the part up to the last separator is the
     *   directory, and the rest is the file; terminate the buffer at the
     *   last separator and switch to that directory 
     */
    if (last_sep != 0)
    {
        *last_sep = '\0';
        os_set_pwd(buf);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Service routine: set a registry key 
 */
static void set_reg_key(char *keyname, char *valname, char *val)
{
    DWORD disposition;
    HKEY  hkey;
    
    /* create or open the key */
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, keyname, 0, 0, 0, KEY_ALL_ACCESS,
                       0, &hkey, &disposition) == ERROR_SUCCESS)
    {
        /* set the value */
        RegSetValueEx(hkey, valname, 0, REG_SZ, (BYTE *)val, strlen(val) + 1);

        /* done with the key */
        RegCloseKey(hkey);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the saved-game extension 
 */
void os_set_save_ext(const char *ext)
{
    HKEY hkey;
    char keyname[MAX_PATH];
    
    /* copy the saved game extension into our buffer */
    strcpy(default_saved_game_ext, ext);

    /* 
     *   check to see if the registry keys for the saved game association
     *   are present 
     */
    sprintf(keyname, ".%s", ext);
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, keyname, 0, KEY_ALL_ACCESS, &hkey)
        == ERROR_SUCCESS)
    {
        /* 
         *   the suffix key exists, so assume the association is all set
         *   up -- all we need to do is close the key 
         */
        RegCloseKey(hkey);
    }
    else
    {
        char modname[MAX_PATH];
        char val[MAX_PATH];
        char classname[MAX_PATH];
        char cmd[MAX_PATH + 30];
        char *p;
        char *rootname;
        char *lastdot;

        /* 
         *   The suffix key doesn't exit, so we must create the
         *   associations.  
         */

        /*
         *   Generate a class name.  Use the name of our executable as the
         *   basis of the class name, and add ".SavedGame" to it.  Note
         *   that we must remove the directory prefix from the executable
         *   filename, since we want it to be a valid key value (i.e., no
         *   backslashes).  
         */
        GetModuleFileName(0, modname, sizeof(modname));
        for (p = rootname = modname ; *p != '\0' ; ++p)
        {
            /* 
             *   if this is a path separator, assume the root starts on
             *   the next character; if we find another path separator
             *   later we'll ignore the current one 
             */
            if (*p == '\\' || *p == '/' || *p == ':')
                rootname = p + 1;
        }

        /* generate the class name -- module name + ".SavedGame" */
        sprintf(classname, "%s.SavedGame", rootname);

        /*
         *   Set the suffix key -- this associates the suffix with the
         *   class name (for example, ".DeepSave" -> "Deep.exe.SavedGame") 
         */
        set_reg_key(keyname, "", classname);

        /*
         *   Set the class key's default value -- this gives a display
         *   name for files with our suffix.  We don't know the display
         *   name, so we'll just call these "<exe-name> Saved Game" files,
         *   with the ".EXE" removed from the executable's name.  
         */
        for (p = rootname, lastdot = p + strlen(p) ; *p != '\0' ; ++p)
        {
            /* if this is a dot, note it */
            if (*p == '.')
                lastdot = p;
        }
        sprintf(val, "\"%.*s\" Saved Game", lastdot - rootname, rootname);
        set_reg_key(classname, "", val);

        /*
         *   Now create the shell command for restoring these games.  For
         *   this, we need the name of the executable (in quotes), plus
         *   the -r option, plus the parameters ("%1"). 
         */
        sprintf(cmd, "\"%s\" -r \"%%1\"", modname);
        sprintf(keyname, "%s\\Shell\\Open\\Command", classname);
        set_reg_key(keyname, "", cmd);

        /*
         *   Set the icon for our file suffix - saved games use icon #2 in
         *   the executable.  
         */
        sprintf(cmd, "%s,2", modname);
        sprintf(keyname, "%s\\DefaultIcon", classname);
        set_reg_key(keyname, "", cmd);
    }
}

const char *os_get_save_ext()
{
    return (default_saved_game_ext[0] != '\0' ? default_saved_game_ext : 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Generate a filename for a character mapping table.  On Windows, the
 *   filename is always simply "win" plus the internal ID plus ".tcp".  
 */
void os_gen_charmap_filename(char *filename, char *internal_id,
                             char *argv0)
{
    char *p;
    char *rootname;
    size_t pathlen;
    
    /* find the path prefix of the original executable filename */
    for (p = rootname = argv0 ; *p != '\0' ; ++p)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
            rootname = p + 1;
    }

    /* copy the path prefix */
    pathlen = rootname - argv0;
    memcpy(filename, argv0, pathlen);

    /* if there's no trailing backslash, add one */
    if (pathlen == 0 || filename[pathlen - 1] != '\\')
        filename[pathlen++] = '\\';

    /* add "win_", plus the character set ID, plus the extension */
    strcpy(filename + pathlen, "win_");
    strcat(filename + pathlen, internal_id);
    strcat(filename + pathlen, ".tcp");
}

/*
 *   Get the filename character mapping for Unicode characters.  We use
 *   the current ANSI character set for all mappings.  
 */
void os_get_charmap(char *mapname, int charmap_id)
{
    /* build the name as "CPnnnn", where nnnn is the ANSI code page */
    sprintf(mapname, "CP%d", GetACP());
}


/* ------------------------------------------------------------------------ */
/*
 *   Sleep until the specified time.  To avoid locking up our window,
 *   rather than using the system Sleep() API, we'll instead process and
 *   discard events until the delay is finished.  Fortunately, we have the
 *   handy os_get_event() routine to process events with a timeout; we'll
 *   just call this repeatedly, discarding the events, until the timeout
 *   expires.  
 */
void os_sleep_ms(long delay_in_milliseconds)
{
    long done_time;

    /* calculate when we'll be done */
    done_time = os_get_sys_clock_ms() + delay_in_milliseconds;

    /* discard events until the timeout expires */
    for (;;)
    {
        long cur_time;
        os_event_info_t info;

        /* get the current time */
        cur_time = GetTickCount();

        /* if we've reached the expiration point, delay no longer */
        if (cur_time >= done_time)
            break;

        /* 
         *   Read an event, timing out after what's left of our delay
         *   interval.  If we get an end-of-file event, abort the wait.  
         */
        if (os_get_event(done_time - cur_time, TRUE, &info) == OS_EVT_EOF)
            break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get user input through a dialog 
 */

/*
 *   dialog parameters object - we can show only one dialog at a time, so
 *   we use a static instance of this structure for simplicity 
 */
static struct 
{
    /* default button ID - zero if no default */
    int default_id;
    
    /* cancel button ID - zero if none */
    int cancel_id;
} S_dlg_params;

/* 
 *   dialog box procedure 
 */
static BOOL CALLBACK oss_input_dialog_proc(HWND hwnd, UINT msg,
                                           WPARAM wpar, LPARAM lpar)
{
    switch(msg)
    {
    case WM_INITDIALOG:
        /* if there's a defined default button, set initial focus there */
        if (S_dlg_params.default_id != 0)
        {
            /* set the focus */
            SetFocus(GetDlgItem(hwnd, S_dlg_params.default_id));

            /* return zero to indicate that we've set focus */
            return 0;
        }
        
        /* return non-zero to use the default button */
        return 1;
        
    case WM_COMMAND:
        /* 
         *   if it's a cancel or close command, allow it only if we have a
         *   cancel button defined 
         */
        if (wpar == IDCANCEL || wpar == IDCLOSE)
        {
            /* 
             *   if it's the cancel button, dismiss the dialog with the
             *   defined cancel button, not with the generic cancel
             *   message 
             */
            if (S_dlg_params.cancel_id != 0)
                EndDialog(hwnd, S_dlg_params.cancel_id);

            /* handled */
            return 1;
        }

        /* 
         *   if it's the a return key, translate it to the default key, if
         *   there is one 
         */
        if (wpar == IDOK)
        {
            /* use the default button, if defined */
            if (S_dlg_params.default_id != 0)
                EndDialog(hwnd, S_dlg_params.default_id);

            /* handled */
            return 1;
        }
        
        /* 
         *   it's a command, so they must have pushed one of our buttons;
         *   all of our buttons are dismissal buttons, so simply dismiss
         *   the dialog with the command 
         */
        EndDialog(hwnd, wpar);
        return 1;

    default:
        /* we don't process other messages */
        return 0;
    }
}

/* base of our command ID's */
#define OS_ID_CMD_BASE  100

/* size we allocate to our icon */
#define OS_ID_ICON_X    32
#define OS_ID_ICON_Y    32


/*
 *   show the dialog 
 */
int os_input_dialog(int icon_id, const char *prompt, int standard_button_set,
                    const char **buttons, int button_count,
                    int default_index, int cancel_index)
{
    /* check for standard dialogs */
    if (standard_button_set != 0)
    {
        DWORD flags;
        int ret;

        /* select the dialog style based on the button set */
        switch(standard_button_set)
        {
        case OS_INDLG_OK:
            flags = MB_OK;
            break;

        case OS_INDLG_OKCANCEL:
            flags = MB_OKCANCEL;
            break;

        case OS_INDLG_YESNO:
            flags = MB_YESNO;
            break;

        case OS_INDLG_YESNOCANCEL:
            flags = MB_YESNOCANCEL;
            break;

        default:
            /* we don't recognize other button sets - return failure */
            return 0;
        }

        /* select the icon ID */
        switch(icon_id)
        {
        case OS_INDLG_ICON_NONE:
            break;

        case OS_INDLG_ICON_WARNING:
            flags |= MB_ICONWARNING;
            break;

        case OS_INDLG_ICON_INFO:
            flags |= MB_ICONINFORMATION;
            break;

        case OS_INDLG_ICON_QUESTION:
            flags |= MB_ICONQUESTION;
            break;

        case OS_INDLG_ICON_ERROR:
            flags |= MB_ICONERROR;
            break;
        }

        /* show the message box modally to the task */
        flags |= MB_TASKMODAL;

        /* select the appropriate default button */
        switch(default_index)
        {
        case 1:
            flags |= MB_DEFBUTTON1;
            break;

        case 2:
            flags |= MB_DEFBUTTON2;
            break;

        case 3:
            flags |= MB_DEFBUTTON3;
            break;

        case 4:
            flags |= MB_DEFBUTTON4;
            break;
        }

        /* show the dialog */
        ret = MessageBox(0, prompt, "TADS", flags);

        /* determine which button they pushed */
        switch(ret)
        {
        case 0:
        default:
            /* failure */
            return 0;

        case IDYES:
            /* They clicked yes - this is always at button index 1 */
            return 1;

        case IDNO:
            /* they clicked no - this is always at button index 2 */
            return 2;

        case IDCANCEL:
            /* 
             *   they clicked cancel - this could be at 2 or 3, depending
             *   on the dialog 
             */
            switch(standard_button_set)
            {
            case OS_INDLG_OKCANCEL:
                return 2;

            case OS_INDLG_YESNOCANCEL:
                return 3;
            }

            /* we shouldn't have gotten cancel in other cases */
            return 0;

        case IDOK:
            /* this is always at index 1 */
            return 1;
        }
    }
    else
    {
#define MAX_BUTTONS  16
#define MAX_LABEL_CHARS  1024
        struct
        {
            DLGTEMPLATE hdr;
            WORD menu_arr[1];
            WORD class_arr[1];

            /* space for the title and font information */
            wchar_t title_arr[128];

            /* 
             *   Space for at least 16 dialog item templates, and 1k for the
             *   prompt text and button label text.  Also leave space for
             *   internal structure padding to 4-byte boundaries - we need
             *   padding for each button.  
             */
            WORD buf[sizeof(DLGITEMTEMPLATE)*MAX_BUTTONS/2       /* buttons */
                     + MAX_LABEL_CHARS            /* prompt & button labels */
                     + 2*MAX_BUTTONS];                /* per-button padding */
        } tpl;
        DLGITEMTEMPLATE *itm;
        int chrem = MAX_LABEL_CHARS;
        int chcur;
        wchar_t *p;
        int i;
        HDC dc = GetDC(GetDesktopWindow());
        SIZE txtsiz;
        TEXTMETRIC tm;
        LONG dlg_base_units;
        int dlgx, dlgy;
        int btn_total_wid;
        int curx;

        /* 
         *   to ensure we don't overflow our dialog template memory, limit
         *   the button count to the maximum 
         */
        if (button_count > MAX_BUTTONS)
            button_count = MAX_BUTTONS;

        /* get the dialog-box-unit conversion factors */
        dlg_base_units = GetDialogBaseUnits();
        dlgx = LOWORD(dlg_base_units);
        dlgy = HIWORD(dlg_base_units);

        /* set up the template header */
        tpl.hdr.style = DS_3DLOOK | DS_CENTER | DS_MODALFRAME | DS_SETFONT;
        tpl.hdr.dwExtendedStyle = 0;
        tpl.hdr.x = 0;
        tpl.hdr.y = 0;
        tpl.hdr.cx = 300;
        tpl.hdr.cy = 94;

        /* 
         *   measure the height of some sample text, to see how tall the
         *   buttons will be - if we need more space than the 14 dialog units
         *   we've assumed in our fixed dialog height, add more height to the
         *   dialog to make room 
         */
        GetTextMetrics(dc, &tm);
        i = MulDiv(tm.tmHeight, 8, dlgy) + 8;
        if (i > 14)
            tpl.hdr.cy += 14 - i;

        /* 
         *   we have one control for the prompt text, another for the icon
         *   if specified, and then one for each button 
         */
        tpl.hdr.cdit = 1 + (icon_id != 0 ? 1 : 0) + button_count;

        /* no menu or class entries */
        tpl.menu_arr[0] = 0;
        tpl.class_arr[0] = 0;

        /* set up the title */
        wcscpy(tpl.title_arr, (const wchar_t *)L"TADS");

        /* get the next available entry after the title string */
        p = tpl.title_arr + wcslen(tpl.title_arr) + 1;

        /* set the point size and font name */
        *p++ = 8;
        wcscpy(p, (const wchar_t *)L"MS Sans Serif");

        /* skip past the font name */
        p += wcslen(p) + 1;

        /* 
         *   we're now pointing to the first item template slot - align it
         *   on a DWORD boundary 
         */
        itm = (DLGITEMTEMPLATE *)((((unsigned long)(p)) + 3) & ~3);

        /* adjust to a system icon ID */
        switch(icon_id)
        {
        case OS_INDLG_ICON_WARNING:
            icon_id = (int)IDI_EXCLAMATION;
            break;

        case OS_INDLG_ICON_INFO:
            icon_id = (int)IDI_ASTERISK;
            break;

        case OS_INDLG_ICON_QUESTION:
            icon_id = (int)IDI_QUESTION;
            break;

        case OS_INDLG_ICON_ERROR:
            icon_id = (int)IDI_HAND;
            break;

        default:
            /* no icon */
            icon_id = 0;
            break;
        }

        /* add the icon */
        if (icon_id != 0)
        {
            /* add the icon */
            itm->style = SS_ICON | WS_VISIBLE | WS_CHILD;
            itm->dwExtendedStyle = 0;
            itm->x = 10;
            itm->y = 10;
            itm->cx = OS_ID_ICON_X;
            itm->cy = OS_ID_ICON_Y;
            itm->id = (WORD)-1;

            /* set the STATIC class */
            p = (WORD *)(itm + 1);
            *p++ = 0xffff;
            *p++ = 0x0082;

            /* set the icon ID */
            *p++ = 0xffff;
            *p++ = icon_id;
            
            /* no creation data */
            *p++ = 0;

            /* get the next item */
            itm = (DLGITEMTEMPLATE *)((((unsigned long)(p)) + 3) & ~3);
        }

        /* add the text string */
        itm->style = WS_VISIBLE | WS_CHILD;
        itm->dwExtendedStyle = 0;
        itm->x = 10 + (icon_id == 0 ? 0 : OS_ID_ICON_X + 2);
        itm->y = 10;
        itm->cx = tpl.hdr.cx - 14 - (icon_id == 0 ? 0 : OS_ID_ICON_X + 2);
        itm->cy = 32;
        itm->id = (WORD)-1;

        /* set the STATIC class */
        p = (WORD *)(itm + 1);
        *p++ = 0xffff;
        *p++ = 0x0082;

        /* copy the prompt text, translating to unicode */
        chcur = MultiByteToWideChar(CP_ACP, 0, prompt, -1, p, chrem);
        chrem -= chcur;
        p += chcur;

        /* no creation data */
        *p++ = 0;

        /*
         *   Before we set the button positions, run through the buttons and
         *   calculate their widths.  This will let us figure out how to
         *   center the line of buttons. 
         */
        for (btn_total_wid = 0, i = 0 ; i < button_count ; ++i)
        {
            short wid;
            
            /* measure the button text */
            GetTextExtentPoint32(dc, buttons[i], strlen(buttons[i]), &txtsiz);
            wid = (short)MulDiv(txtsiz.cx, 4, dlgx);

            /* add in some padding at either end of the text */
            wid += 8;

            /* 
             *   for a better appearance for short button names, use a
             *   minimum width of 64 dialog units for each button
             */
            if (wid < 64)
                wid = 64;

            /* count this in the total width */
            btn_total_wid += wid;

            /* if this isn't the first button, add inter-button spacing */
            if (i != 0)
                btn_total_wid += 4;
        }

        /*
         *   If the total of the button widths, plus 4 units on either side
         *   for spacing, exceeds our dialog width, increase the dialog width
         *   accordingly.  
         */
        if (tpl.hdr.cx < btn_total_wid + 8)
            tpl.hdr.cx = btn_total_wid + 8;

        /* get the next item */
        itm = (DLGITEMTEMPLATE *)((((unsigned long)(p)) + 3) & ~3);

        /*
         *   Figure the position of the left edge of the buttons such that we
         *   center the overall line of buttons in the dialog.  
         */
        curx = (tpl.hdr.cx - btn_total_wid) / 2;

        /* add the buttons */
        for (i = 0 ; i < button_count ; ++i)
        {
            /* add the button */
            itm->style = BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP
                         | WS_CHILD;
            itm->dwExtendedStyle = 0;
            itm->id = i + 1 + OS_ID_CMD_BASE;

            /* measure the button text */
            GetTextExtentPoint32(dc, buttons[i], strlen(buttons[i]), &txtsiz);
            GetTextMetrics(dc, &tm);
            itm->cx = (short)MulDiv(txtsiz.cx, 4, dlgx) + 8;
            itm->cy = (short)MulDiv(tm.tmHeight, 8, dlgy) + 8;

            /* apply a minimum button width of 64 dialog units */
            if (itm->cx < 64)
                itm->cx = 64;

            /* set the position of the button */
            itm->x = curx;
            itm->y = tpl.hdr.cy - itm->cy - 18;

            /* 
             *   advance to the next position (include the button width plus
             *   inter-button spacing) 
             */
            curx += itm->cx + 4;

            /* set the BUTTON class */
            p = (WORD *)(itm + 1);
            *p++ = 0xffff;
            *p++ = 0x0080;

            /* copy the button label text */
            chcur = MultiByteToWideChar(CP_ACP, 0, buttons[i], -1, p, chrem);
            chrem -= chcur;
            p += chcur;

            /* no creation data */
            *p++ = 0;

            /* get the next item, DWORD-aligned */
            itm = (DLGITEMTEMPLATE *)((((unsigned long)(p)) + 3) & ~3);
        }

        /* 
         *   set up the default and cancel buttons in the dialog
         *   parameters struture, so that the dialog procedure has access
         *   to them; adjust our indices by our command offset 
         */
        S_dlg_params.default_id = (default_index == 0
                                   ? 0 : default_index + OS_ID_CMD_BASE);
        S_dlg_params.cancel_id = (cancel_index == 0
                                  ? 0 : cancel_index + OS_ID_CMD_BASE);

        /* we're done with the desktop DC */
        ReleaseDC(GetDesktopWindow(), dc);

        /* make sure the text output is flushed */
        os_flush();

        /* show the dialog */
        i = DialogBoxIndirect(oss_G_hinstance, &tpl.hdr, GetActiveWindow(),
                              (DLGPROC)oss_input_dialog_proc);

        /* check the result */
        if (i >= OS_ID_CMD_BASE)
        {
            /* they selected one of the buttons - adjust to a 1-base */
            i -= OS_ID_CMD_BASE;
        }
        else
        {
            /* unrecognized result */
            i = 0;
        }

        /* return the result */
        return i;

#undef MAX_LABEL_CHARS
#undef MAX_BUTTONS
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   String Resource loader.  We'll go to the executable's resource list
 *   to load these strings, using a base resource ID of 61000.  An
 *   application (HTML TADS, for example) must provide these resources in
 *   the .res file it links in to its executable.  
 */
int os_get_str_rsc(int id, char *buf, size_t buflen)
{
    /* 
     *   load the string - adjust the resource ID by adding our base value
     *   of 61000 
     */
    return (LoadString(oss_G_hinstance, id + 61000, buf, buflen) == 0);
}

/*
 *   Use the system strlwr() implementation for os_strlwr() 
 */
char *os_strlwr(char *s)
{
    return strlwr(s);
}

