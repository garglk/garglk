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

