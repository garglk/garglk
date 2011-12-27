#charset "us-ascii"

/*
 *   TADS 3 Library - Menu System, console edition
 *   
 *   This implements the menusys user interface for the traditional
 *   console-mode interpreters.  
 */
#include "adv3.h"

/* ------------------------------------------------------------------------ */
/*
 *   Banner windows.  For the console version, we display the menu
 *   components in banner windows.  
 */


/* 
 *   The very top banner of the menu, which holds its title and
 *   instructions.
 */
topMenuBanner: BannerWindow
;

/* 
 *   The actual menu contents banner window.  This displays the list of
 *   menu items to choose from.  
 */
contentsMenuBanner: BannerWindow
;

/*
 *   The long topic banner.  This takes over the screen when we're
 *   displaying a long topic item.  
 */
longTopicBanner: BannerWindow
;


/* ------------------------------------------------------------------------ */
/*
 *   Menu Item - user interface implementation for the console 
 */
modify MenuItem
    /* 
     *   Call menu.display when you're ready to show the menu.  This
     *   should be called on the top-level menu; we run the entire menu
     *   display process, and return when the user exits from the menu
     *   tree.  
     */
    display()
    {
        local oldStr;
        local flags;

        /* make sure the main window is flushed before we get going */
        flushOutput();

        /* set up with the top menu banner in place of the status line */
        removeStatusLine();
        showTopMenuBanner(self);

        /* 
         *   display the menu using the same mode that the statusline
         *   has decided to use 
         */
        switch (statusLine.statusDispMode)
        {
        case StatusModeApi:
            /* use a border, unless we're taking over the whole screen */
            flags = (fullScreenMode ? 0 : BannerStyleBorder);

            /* 
             *   use a scrollbar if possible; keep the text scrolled into
             *   view as we show it 
             */
            flags |= BannerStyleVScroll | BannerStyleAutoVScroll;

            /* banner API mode - show our banner window */
            contentsMenuBanner.showBanner(nil, BannerLast, nil,
                                          BannerTypeText, BannerAlignTop,
                                          nil, nil, flags);

            /* make the banner window the default output stream */
            oldStr = contentsMenuBanner.setOutputStream();

            /* make sure we restore the default output stream when done */
            try
            {
                /* display and run our menu in HTML mode */
                showMenuHtml(self);
            }
            finally
            {
                /* restore the original default output stream */
                outputManager.setOutputStream(oldStr);

                /* remove the menu banner */
                contentsMenuBanner.removeBanner();
            }
            break;
            
        case StatusModeTag:
            /* HTML <banner> tag mode - just show our HTML contents */
            showMenuHtml(self);

            /* remove the banner for the menu display */
            "<banner remove id=MenuTitle>";
            break;
            
        case StatusModeText:
            /* display and run our menu in text mode */
            showMenuText(self);
            break;
        }

        /* we're done, so remove the top menu banner */
        removeTopMenuBanner();
    }
    
    /*
     *   Display the menu in plain text mode.  This is used when the
     *   interpreter only supports the old tads2-style text-mode
     *   single-line status area.
     *   
     *   Returns true if we should return to the parent menu, nil if the
     *   user selected QUIT to exit the menu system entirely.  
     */
    showMenuText(topMenu)
    {
        local i, selection, len, key = '', loc;

        /* remember the key list */
        curKeyList = topMenu.keyList;

        /* bring our contents up to date, as needed */
        updateContents();

        /* keep going until the player exits this menu level */
        do
        {
            /* 
             *   For text mode, print the title, then show the menu
             *   options as a numbered list, then ask the player to make a
             *   selection.  
             */
            
            /* get the number of items in the menu */
            len = contents.length();              

            /* show the menu heading */
            "\n<b><<heading>></b>\b";

            /* show the contents as a numbered list */
            for (i = 1; i <= len; i++)
            {
                /* leave room for two-digit numeric labels if needed */
                if (len > 9 && i <= 10) "\ ";

                /* show the item's number and title */
                "<<i>>.\ <<contents[i].title>>\n";
            }

            /* show the main prompt */
            gLibMessages.textMenuMainPrompt(topMenu.keyList);

            /* main input loop */
            do
            {
                /* 
                 *   Get a key, and convert any alphabetics to lower-case.
                 *   Do not allow real-time interruptions, as menus are
                 *   meta-game interactions. 
                 */
                key = inputManager.getKey(nil, nil).toLower();

                /* check for a command key */
                loc = topMenu.keyList.indexWhich({x: x.indexOf(key) != nil});

                /* also check for a numeric selection */
                selection = toInteger(key);
            } while ((selection < 1 || selection > len)
                     && loc != M_QUIT && loc != M_PREV);

            /* 
             *   show the selection if it's an ordinary key (an ordinary
             *   key is represented by a single character; if we have more
             *   than one character, it's one of the '[xxx]' special key
             *   representations) 
             */
            if (key.length() == 1)
                "<<key>>";

            /* add a blank line */
            "\b";
            
            /* 
             *   If the selection is a number, then the player selected
             *   that menu option.  Call that submenu or topic's display
             *   routine.  If the routine returns nil, the player selected
             *   QUIT, so we should quit as well. 
             */
            while (selection != 0 && selection <= contents.length())
            {
                /* invoke the child menu */
                loc = contents[selection].showMenuText(topMenu);

                /*   
                 *   Check the result.  If it's nil, it means QUIT; if it's
                 *   'next', it means we're to proceed directly to our next
                 *   sub-menu.  If the user didn't select QUIT, then
                 *   refresh our menu contents, as we'll be displaying our
                 *   menu again and its contents could have been affected
                 *   by the sub-menu invocation.  
                 */
                switch(loc)
                {
                case M_QUIT:
                    /* they want to quit - leave the submenu loop */
                    selection = 0;
                    break;

                case M_UP:
                    /* they want to go to the previous menu directly */
                    --selection;
                    break;

                case M_DOWN:
                    /* they want to go to the next menu directly */
                    ++selection;
                    break;

                case M_PREV:
                    /* 
                     *   they want to show this menu again - update our
                     *   contents so that we account for any changes made
                     *   while running the submenu, then leave the submenu
                     *   loop 
                     */
                    updateContents();
                    selection = 0;

                    /* 
                     *   forget the 'prev' command - we don't want to back
                     *   up any further just yet, since the submenu just
                     *   wanted to get back to this point 
                     */
                    loc = nil;
                    break;
                }
            }
        } while (loc != M_QUIT && loc != M_PREV);

        /* return the desired next action */
        return loc;
    }

    /*
     *   Show the menu using HTML.  Return nil when the user selects QUIT
     *   to exit the menu entirely.  
     */
    showMenuHtml(topMenu)
    {
        local len, selection = 1, loc;
        local refreshTitle = true;
        
        /* remember the key list */
        curKeyList = topMenu.keyList;

        /* update the menu contents, as needed */
        updateContents();

        /* keep going until the user exits this menu level */
        do
        {
            /* refresh our title in the instructions area if necessary */
            if (refreshTitle)
            {
                refreshTopMenuBanner(topMenu);
                refreshTitle = nil;
            }

            /* get the number of items in the menu */
            len = contents.length();
              
            /* check whether we're in banner API or <banner> tag mode */
            if (statusLine.statusDispMode == StatusModeApi)
            {
                /* banner API mode - clear our window */
                contentsMenuBanner.clearWindow();

                /* advise the interpreter of our best guess for our size */
                if (fullScreenMode)
                    contentsMenuBanner.setSize(100, BannerSizePercent, nil);
                else
                    contentsMenuBanner.setSize(len + 1, BannerSizeAbsolute,
                                               true);

                /* set up our desired color scheme */
                "<body bgcolor=<<bgcolor>> text=<<fgcolor>> >";
            }
            else
            {
                /* 
                 *   <banner> tag mode - set up our tag.  In full-screen
                 *   mode, set our height to 100% immediately; otherwise,
                 *   leave the height unspecified so that we'll use the
                 *   size of our contents.  Use a border only if we're not
                 *   taking up the full screen. 
                 */
                "<banner id=MenuBody align=top
                <<fullScreenMode ? 'height=100%' : 'border'>>
                ><body bgcolor=<<bgcolor>> text=<<fgcolor>> >";
            }

            /* display our contents as a table */
            "<table><tr><td width=<<indent>> > </td><td>";
            for (local i = 1; i <= len; i++)
            {
                /* 
                 *   To get the alignment right, we have to print '>' on
                 *   each and every line. However, we print it in the
                 *   background color to make it invisible everywhere but
                 *   in front of the current selection.
                 */
                if (selection != i)
                    "<font color=<<bgcolor>> >&gt;</font>";
                else
                    "&gt;";
                
                /* make each selection a plain (i.e. unhilighted) HREF */
                "<a plain href=<<i>> ><<contents[i].title>></a><br>";
            }

            /* end the table */
            "</td></tr></table>";

            /* finish our display as appropriate */
            if (statusLine.statusDispMode == StatusModeApi)
            {
                /* banner API - size the window to its contents */
                if (!fullScreenMode)
                    contentsMenuBanner.sizeToContents();
            }
            else
            {
                /* <banner> tag - just close the tag */
                "</banner>";
            }

            /* main input loop */
            do
            {
                local key, events;

                /* 
                 *   Read an event - don't allow real-time interruptions,
                 *   since menus are meta-game interactions.  Read an
                 *   event rather than just a keystroke, because we want
                 *   to let the user click on a menu item's HREF.  
                 */
                events = inputManager.getEvent(nil, nil);

                /* check the event type */
                switch (events[1])
                {
                case InEvtHref:
                    /* 
                     *   the HREF's value is the selection number, or a
                     *   'previous' command 
                     */
                    if (events[2] == 'previous')
                        loc = M_PREV;
                    else
                    {
                        selection = toInteger(events[2]);
                        loc = M_SEL;
                    }
                    break;

                case InEvtKey:
                    /* keystroke - convert any alphabetic to lower case */
                    key = events[2].toLower();

                    /* scan for a valid command key */
                    loc = topMenu.keyList.indexWhich(
                        {x: x.indexOf(key) != nil});
                    break;
                }

                /* handle arrow keys */
                if (loc == M_UP)
                {
                    selection--;
                    if (selection < 1)
                        selection = len;
                }
                else if (loc == M_DOWN)
                {
                    selection++;
                    if (selection > len)
                        selection = 1;
                }
            } while (loc == nil);

            /* if the player selected a sub-menu, invoke the selection */
            while (loc == M_SEL
                   && selection != 0
                   && selection <= contents.length())
            {
                /* 
                 *   Invoke the sub-menu, checking for a QUIT result.  If
                 *   the user isn't quitting, we'll display our own menu
                 *   again; in this case, update it now, in case something
                 *   in the sub-menu changed our own contents. 
                 */
                loc = contents[selection].showMenuHtml(topMenu);
                
                /* see what we have */
                switch (loc)
                {
                case M_UP:
                    /* they want to go directly to the previous menu */
                    loc = M_SEL;
                    --selection;
                    break;

                case M_DOWN:
                    /* they want to go directly to the next menu */
                    loc = M_SEL;
                    ++selection;
                    break;

                case M_PREV:
                    /* they want to return to this menu level */
                    loc = nil;

                    /* update our contents */
                    updateContents();

                    /* make sure we refresh the title area */
                    refreshTitle = true;
                    break;
                }
            }
        } while (loc != M_QUIT && loc != M_PREV);

        /* return the next status */
        return loc;
    }

    /* 
     *   showTopMenuBanner creates the banner for the menu using the
     *   banner API.  The banner contains the title of the menu on the
     *   left and the navigation keys on the right. 
     */
    showTopMenuBanner(topMenu)
    {
        /* do not show the top banner if we're in text mode */
        if (statusLine.statusDispMode == StatusModeText)
            return;

        /* 
         *   Since the status line has already figured out the terp's
         *   capabilities, piggyback off of what it learned.  If we're
         *   using banner API mode, show our banner window.  
         */
        if (statusLine.statusDispMode == StatusModeApi)
        {
            /* banner API mode - show our banner window */
            topMenuBanner.showBanner(nil, BannerFirst, nil, BannerTypeText,
                                     BannerAlignTop, nil, nil,
                                     BannerStyleBorder | BannerStyleTabAlign);
            
            /* advise the terp that we need two lines */
            topMenuBanner.setSize(2, BannerSizeAbsolute, true);
        }

        /* show our contents */
        refreshTopMenuBanner(topMenu);
    }

    /*
     *   Refresh the contents of the top bar with the instructions 
     */
    refreshTopMenuBanner(topMenu)
    {
        local oldStr;

        /* clear our old contents using the appropriate mode */
        switch (statusLine.statusDispMode)
        {
        case StatusModeApi:
            /* clear the window */
            topMenuBanner.clearWindow();

            /* set the default output stream to our menu window */
            oldStr = topMenuBanner.setOutputStream();

            /* set our color scheme */
            "<body bgcolor=<<topbarbg>> text=<<topbarfg>> >";
            break;
            
        case StatusModeTag:
            /* start a new <banner> tag */
            "<banner id=MenuTitle align=top><body bgcolor=<<topbarbg>>
            text=<<topbarfg>> >";
            break;
        }

        /* show our heading */
        say(heading);

        /* show our keyboard assignments */
        gLibMessages.menuInstructions(topMenu.keyList, prevMenuLink);
        
        /* finish up according to our mode */
        switch (statusLine.statusDispMode)
        {
        case StatusModeApi:
            /* banner API mode - restore the old output stream */
            outputManager.setOutputStream(oldStr);
            
            /* size the window to the actual content size */
            topMenuBanner.sizeToContents();
            break;
            
        case StatusModeTag:
            /* close the <banner> tag */
            "</banner>";
            break;
        }
    }

    /*
     *   Remove the top banner window
     */
    removeTopMenuBanner()
    {
        /* remove the window according to the banner mode */
        switch (statusLine.statusDispMode)
        {
        case StatusModeApi:
            /* banner API mode - remove the banner window */
            topMenuBanner.removeBanner();
            break;

        case StatusModeTag:
            /* banner tag mode - remove our banner tag */
            "<banner remove id=MenuTitle>";
        }
    }
    
    /*
     *   Remove the status line banner prior to displaying the menu
     */
    removeStatusLine()
    {
        local oldStr;

        /* remove the banner according to our banner display mode */
        switch (statusLine.statusDispMode)
        {
        case StatusModeApi:
            /* 
             *   banner API mode - simply set the banner window to zero
             *   size, which will effectively make it invisible 
             */
            statuslineBanner.setSize(0, BannerSizeAbsolute, nil);
            break;

        case StatusModeTag:
            /* <banner> tag mode - remove the statusline banner */
            oldStr = outputManager.setOutputStream(statusTagOutputStream);
            "<banner remove id=StatusLine>";
            outputManager.setOutputStream(oldStr);
            break;

        case StatusModeText:
            /* tads2-style statusline - there's no way to remove it */
            break;
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Menu topic item - console UI implementation 
 */
modify MenuTopicItem
    /*
     *   Display and run our menu in text mode.
     */
    showMenuText(topMenu)
    {
        local i, len, loc;
        
        /* remember the key list */
        curKeyList = topMenu.keyList;

        /* update our contents, as needed */
        updateContents();

        /* get the number of items in our list */
        len = menuContents.length();

        /* show our heading and instructions */
        "\n<b><<heading>></b>";
        gLibMessages.textMenuTopicPrompt();

        /* 
         *   Show all of the items up to and including the last one we
         *   displayed on any past invocation.  Append "[#/#]" to each
         *   item to show where we are in the overall list. 
         */
        for (i = 1 ; i <= lastDisplayed ; ++i)
        {
            /* display this item */
            displaySubItem(i, i == lastDisplayed, '\b');
        }

        /* main input loop */
        for (;;)
        {
            local key;
            
            /* read a keystroke */
            key = inputManager.getKey(nil, nil).toLower();

            /* look it up in the key list */
            loc = topMenu.keyList.indexWhich({x: x.indexOf(key) != nil});

            /* check to see if they want to quit the menu system */
            if (loc == M_QUIT)
                return M_QUIT;

            /* 
             *   check to see if they want to return to the previous menu;
             *   if we're out of items to show, return to the previous
             *   menu on any other keystrok as well 
             */
            if (loc == M_PREV || self.lastDisplayed == len)
                 return M_PREV;

            /* for any other keystroke, just show the next item */
            lastDisplayed++;
            displaySubItem(lastDisplayed, lastDisplayed == len, '\b');
        }
    }

    /*
     *   Display and run our menu in HTML mode.
     */
    showMenuHtml(topMenu)
    {
        local len;
        local topIdx;
        
        /* remember the key list */
        curKeyList = topMenu.keyList;

        /* refresh the top instructions bar with our heading */
        refreshTopMenuBanner(topMenu);

        /* update our contents, as needed */
        updateContents();

        /* get the number of items in our list */
        len = menuContents.length();

        /* 
         *   initially show the first item at the top of the window (we
         *   might scroll the list later to show a later item at the top,
         *   if we're limiting the number of items we can show at once) 
         */
        topIdx = 1;

        /* main interaction loop */
        for (;;)
        {
            local lastIdx;

            /* redraw the window with the current top item */
            lastIdx = redrawWinHtml(topIdx);

            /* process input */
            for (;;)
            {
                local events;
                local loc;
                local key;
                
                /* read an event */
                events = inputManager.getEvent(nil, nil);
                switch(events[1])
                {
                case InEvtHref:
                    /* check for a 'next' or 'previous' command */
                    switch(events[2])
                    {
                    case 'next':
                        /* we want to go to the next item */
                        loc = M_SEL;
                        break;
                        
                    case 'previous':
                        /* we want to go to the previous menu */
                        loc = M_PREV;
                        break;

                    default:
                        /* ignore other hyperlinks */
                        loc = nil;
                    }
                    break;

                case InEvtKey:
                    /* get the key, converting alphabetic to lower case */
                    key = events[2].toLower();

                    /* look up the keystroke in our key mappings */
                    loc = topMenu.keyList.indexWhich(
                        {x: x.indexOf(key) != nil});
                    break;
                }

                /* 
                 *   if they're quitting or returning to the previous
                 *   menu, we're done 
                 */
                if (loc == M_QUIT || loc == M_PREV)
                    return loc;
                
                /* advance to the next item if desired */
                if (loc == M_SEL)
                {
                    /* 
                     *   if the last item we showed is the last item in
                     *   our entire list, then the normal selection keys
                     *   simply return to the previous menu 
                     */
                    if (lastIdx == len)
                        return M_PREV;

                    /* 
                     *   If we haven't yet reached the last revealed item,
                     *   it means we're limited by the chunk size, so show
                     *   the next chunk.  Otherwise, reveal the next item.
                     */
                    if (lastIdx < lastDisplayed)
                    {
                        /* advance to the next chunk */
                        topIdx += chunkSize;
                    }
                    else
                    {
                        /* reveal the next item */
                        ++lastDisplayed;

                        /* 
                         *   if we're not in full-screen mode, and we've
                         *   already filled the window, scroll down a line
                         *   by advancing the index of the item at the top
                         *   of the window 
                         */
                        if (!fullScreenMode
                            && lastIdx == topIdx + chunkSize - 1)
                            ++topIdx;
                    }

                    /* done processing input */
                    break;
                }
            }
        }
    }

    /*
     *   redraw the window in HTML mode, starting with the given item at
     *   the top of the window 
     */
    redrawWinHtml(topIdx)
    {
        local len;
        local idx;
        
        /* get the number of items in our list */
        len = menuContents.length();

        /* check the banner mode (based on the statusline mode) */
        if (statusLine.statusDispMode == StatusModeApi)
        {
            /* banner API mode - clear the window */
            contentsMenuBanner.clearWindow();
            
            /* 
             *   Advise the terp of our best guess at our size: assume one
             *   line per item, and max out at either our actual number of
             *   items or our maximum chunk size, whichever is lower.  If
             *   we're in full-screen mode, though, simply size to 100% of
             *   the available space.  
             */
            if (fullScreenMode)
                contentsMenuBanner.setSize(100, BannerSizePercent, nil);
            else
                contentsMenuBanner.setSize(chunkSize < len ? chunkSize : len,
                                           BannerSizeAbsolute, true);

            /* set up our color scheme */
            "<body bgcolor=<<bgcolor>> text=<<fgcolor>> >";
        }
        else
        {
            /* <banner> tag mode - open our tag */
            "<banner id=MenuBody align=top
            <<fullScreenMode ? 'height=100%' : 'border'>>
            ><body bgcolor=<<bgcolor>> text=<<fgcolor>>  >";
        }

        /* start a table to show the items */
        "<table><tr><td width=<<self.indent>> > </td><td>";

        /* show the items */
        for (idx = topIdx ; ; ++idx)
        {
            local isLast;

            /* 
             *   Note if this is the last item we're going to show just
             *   now.  It's the last item we're showing if it's the last
             *   item in the list, or it's the 'lastDisplayed' item, or
             *   we've filled out the chunk size.  
             */
            isLast = (idx == len
                      || (!fullScreenMode && idx == topIdx + chunkSize - 1)
                      || idx == lastDisplayed);
            
            /* display the next item */
            displaySubItem(idx, isLast, '<br>');

            /* if that was the last item, we're done */
            if (isLast)
                break;
        }

        /* finish the table */
        "</td></tr></table>";

        /* finish the window */
        switch(statusLine.statusDispMode)
        {
        case StatusModeApi:
            /* if we're not in full-screen mode, set the final size */
            if (!fullScreenMode)
                contentsMenuBanner.sizeToContents();
            break;

        case StatusModeTag:
            /* end the banner tag */
            "</banner>";
            break;
        }

        /* return the index of the last item displayed */
        return idx;
    }

    /* 
     *   Display an item from our list.  'idx' is the index in our list of
     *   the item to display.  'lastBeforeInput' indicates whether or not
     *   this is the last item we're going to show before pausing for user
     *   input.  'eol' gives the newline sequence to display at the end of
     *   the line.  
     */
    displaySubItem(idx, lastBeforeInput, eol)
    {
        local item;

        /* get the item from our list */
        item = menuContents[idx];

        /* 
         *   show the item: if it's a simple string, just display it;
         *   otherwise, assume it's an object, and call its getItemText
         *   method to get its text (and possibly trigger any needed
         *   side-effects) 
         */
        say(dataType(item) == TypeSString ? item : item.getItemText());

        /* add the [n/m] indicator */
        gLibMessages.menuTopicProgress(idx, menuContents.length());

        /* 
         *   if this is the last item we're going to display before asking
         *   for input, and it's not the last item in the list overall,
         *   and we're in HTML mode, show a hyperlink for advancing to the
         *   next item 
         */
        if (lastBeforeInput && idx != menuContents.length())
            "&emsp;<<aHrefAlt('next', nextMenuTopicLink, '')>>";

        /* show the desired line-ending separator */
        say(eol);

        /* if it's the last item, add the end-of-list marker */
        if (idx == menuContents.length())
            "<<menuTopicListEnd>>\n";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Long topic item 
 */
modify MenuLongTopicItem
    /* display and run our menu in text mode */
    showMenuText(topMenu)
    {
        local ret;
        
        /* remember the key list */
        curKeyList = topMenu.keyList;

        /* take over the entire screen */
        cls();

        /* use the common handling */
        ret = showMenuCommon(topMenu);

        /* we're done, so clear the screen again */
        cls();

        /* return the result from the common handler */
        return ret;
    }

    /* display and run our menu in HTML mode */
    showMenuHtml(topMenu)
    {
        local ret;
        local oldStr;

        /* remember the key list */
        curKeyList = topMenu.keyList;

        /* update our contents, as needed */
        updateContents();

        /* hide the two menu system banners */
        if (statusLine.statusDispMode == StatusModeApi)
        {
            local flags;

            /* 
             *   Our banner window might already be showing, because we
             *   could be coming here directly from a prior chapter.  If it
             *   is, we don't need to show it again.  If it isn't showing,
             *   show it now.  
             */
            if (longTopicBanner.handle_ != nil)
            {
                /* simply clear our existing window */
                longTopicBanner.clearWindow();
            }
            else
            {
                /* hide the top menu banner */
                topMenuBanner.setSize(0, BannerSizeAbsolute, nil);

                /* figure our flags */
                flags = (fullScreenMode ? 0 : BannerStyleBorder)
                    | BannerStyleVScroll
                    | BannerStyleMoreMode
                    | BannerStyleAutoVScroll;
            
                /* banner API mode - show the long-topic banner */
                longTopicBanner.showBanner(contentsMenuBanner, BannerLast,
                                           nil, BannerTypeText,
                                           BannerAlignTop,
                                           100, BannerSizePercent, flags);
            }

            /* use its output stream */
            oldStr = longTopicBanner.setOutputStream();
                
            /* set up our color scheme in the new banner */
            "<body bgcolor=<<bgcolor>> text=<<fgcolor>> >";
        }
        else
        {
            /* 
             *   use the main game window output stream for printing this
             *   text (we need to switch back to it explicitly, because
             *   HTML-mode menus normally run in the context of the menu's
             *   banner output stream) 
             */
            oldStr = outputManager.setOutputStream(mainOutputStream);

            /* we're using the main window, so clear out the game text */
            cls();
        }

        try
        {
            /* show our contents using the normal text display */
            ret = showMenuCommon(topMenu);
        }
        finally
        {
            local chapter;
            
            /* restore the original output stream */
            outputManager.setOutputStream(oldStr);

            /*
             *   If we're going directly to the next or previous "chapter,"
             *   and the next menu is itself a long-topic item, don't clean
             *   up the screen: simply leave it in place for the next item.
             *   First, check for a next/previous chapter return code, and
             *   get the menu object for the next/previous chapter.  
             */
            if (ret == M_DOWN)
                chapter = location.getNextMenu(self);
            else if (ret == M_UP)
                chapter = location.getPrevMenu(self);

            /* 
             *   if we have a next/previous chapter, and it's a long-topic
             *   menu, we don't need cleanup; otherwise we do 
             */
            if (isChapterMenu
                && chapter != nil && chapter.ofKind(MenuLongTopicItem))
            {
                /* we don't need any cleanup */
            }
            else
            {
                /* clean up the window */
                if (statusLine.statusDispMode == StatusModeApi)
                {
                    /* API mode - remove our long-topic banner */
                    longTopicBanner.removeBanner();
                }
                else
                {
                    /* tag mode - we used the main game window, so clear it */
                    cls();
                }

                /* restore the top menu banner window */
                topMenu.showTopMenuBanner(topMenu);
            }
        }

        /* return the quit/continue indication */
        return ret;
    }

    /* show our contents - common handler for text and HTML modes */
    showMenuCommon(topMenu)
    {
        local evt, key, loc, nxt;

        /* update our contents, as needed */
        updateContents();

        /* show our heading, centered */
        "<CENTER><b><<heading>></b></CENTER>\b";

        /* show our contents */
        "<<menuContents>>\b";

        /* check to see if we should offer chapter navigation */
        nxt = (isChapterMenu ? location.getNextMenu(self) : nil);

        /* if there's a next chapter, show how we can navigate to it */
        if (nxt != nil)
        {
            /* show the navigation */
            gLibMessages.menuNextChapter(topMenu.keyList, nxt.title,
                                         'next', 'menu');
        }
        else
        {
            /* no chaptering - just print the ending message */
            "<<menuLongTopicEnd>>";
        }

        /* wait for an event */
        for (;;)
        {
            evt = inputManager.getEvent(nil, nil);
            switch(evt[1])
            {
            case InEvtHref:
                /* check for a 'next' or 'prev' command */
                if (evt[2] == 'next')
                    return M_DOWN;
                else if (evt[2] == 'prev')
                    return M_UP;
                else if (evt[2] == 'menu')
                    return M_PREV;
                break;

            case InEvtKey:
                /* get the key */
                key = evt[2].toLower();

                /* 
                 *   if we're in plain text mode, add a blank line after
                 *   the key input 
                 */
                if (statusLine.statusDispMode == StatusModeText)
                    "\b";

                /* look up the command key */
                loc = topMenu.keyList.indexWhich({x: x.indexOf(key) != nil});

                /* 
                 *   if it's 'next', either proceed to the next menu or
                 *   return to the previous menu, depending on whether
                 *   we're in chapter mode or not 
                 */
                if (loc == M_SEL)
                    return (nxt == nil ? M_PREV : M_DOWN);

                /* if it's 'prev', return to the previous menu */
                if (loc == M_PREV || loc == M_QUIT)
                    return loc;

                /* ignore other keys */
                break;
            }
        }
    }
;
