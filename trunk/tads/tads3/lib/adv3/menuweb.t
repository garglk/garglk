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
        /* save the top-level key list */
        MenuItem.curKeyList = keyList;

        /* set myself as the current menu and the top-level menu */
        MenuItem.topMenu = self;
        MenuItem.curMenu = self;

        /* display the menu in the javascript client */
        showMenu(nil);

        /* process network events until the user closes the menu */
        MenuItem.isOpen = true;
        processNetRequests({: !MenuItem.isOpen });
    }

    /* current menu, and current top-level menu */
    curMenu = nil
    topMenu = nil

    /* is the menu open? */
    isOpen = nil

    /* show this menu as a submenu */
    showMenu(from)
    {
        /* get the XML representation of the menu item list */
        local xml = '<menusys><<getXML(from)>></menusys>';
        
        /* tell the javascript client to display the menu */
        webMainWin.sendWinEvent(xml);

        /* save this in the main window as the current menu state */
        webMainWin.menuSysState = xml;
    }

    /* navigate into a submenu */
    enterSubMenu(idx)
    {
        /* validate the index and select the new menu */
        if (idx >= 1 && idx <= contents.length())
        {
            /* get the new menu */
            local m = contents[idx];
            
            /* note the new menu location */
            MenuItem.curMenu = m;
            
            /* show the new menu */
            m.showMenu(self);
        }
    }

    /*
     *   Package my menu items as XML, to send to the javascript API.
     *   'from' is the menu we just navigated from, if any.  This is nil
     *   when we enter the top level menu, since we're not navigating from
     *   another menu; when we navigate from a parent to a child, this is
     *   the parent; when we return from a child to a parent, this is the
     *   child; and when we move directly from sibling to sibling (via a
     *   next/previous chapter command), this is the sibling.  When we
     *   display a new topic in a topic list menu, this is simply 'self'.  
     */
    getXML(from)
    {
        /* set up a string buffer for the xml */
        local s = new StringBuffer();
        
        /* update the menu contents */
        updateContents();

        /* start with the menu title */
        s.append('<title><<title.htmlify()>></title>');
    
        /* note if we're the top-level menu */
        if (location == nil || MenuItem.topMenu == self)
            s.append('<isTop/>');

        /* run through the contents */
        for (local item in contents)
            s.append('<menuItem><<item.title.htmlify()>></menuItem>');

        /* if the 'from' menu is a child, initially select it */
        local idx = contents.indexOf(from);
        if (idx != nil)
            s.append('<fromChild><<idx>></fromChild>');

        /* add the keys */
        getKeysXML(s);

        /* return the string */
        return toString(s);
    }

    /* get the XML description of the top-level key list */
    getKeysXML(buf)
    {
        buf.append('<keylists>');
        for (local kl in curKeyList)
        {
            buf.append('<keylist>');
            for (local k in kl)
            {
                if (k == ' ')
                    k = 'U+0020';
                buf.append('<key><<k>></key>');
            }
            buf.append('</keylist>');
        }
        buf.append('</keylists>');
    }

    /* 
     *   Prepare a title or content string for our XML output.  If 'val' is
     *   a string, we'll run it through the output formatter to expand any
     *   special <.xxx> sequences.  If 'val' is a property, we'll evaluate
     *   the property of self, capturing the output if it generates any or
     *   capturing the string if it returns one.  In all cases, we take the
     *   result string and convert TADS special characters to HTML, and
     *   finally html-escape the result for inclusion in XML output, and
     *   return the resulting string.  
     */
    formatXML(func)
    {
        /* call the function and process it through the menu stream filters */
        local txt = menuOutputStream.captureOutput(func);

        /* convert special characters and html-escape the result */
        return txt.specialsToHtml().htmlify();
    }
;

/*
 *   Menu system UI request processor.  This receives requests from the
 *   javascript client in response to user actions: selecting a menu item,
 *   navigating to the parent menu, closing the menu. 
 */
menuSysEventPage: WebResource
    vpath = '/webui/menusys'
    processRequest(req, query)
    {
        /* check the action type */
        if (query.isKeyPresent('close'))
        {
            /* close the menu */
            MenuItem.isOpen = nil;
        }
        else if (query.isKeyPresent('prev'))
        {
            /* go to the parent menu, or close the menu if at the top */
            local cur = MenuItem.curMenu;
            if (cur != nil && cur != MenuItem.topMenu && cur.location != nil)
            {
                /* go to the parent */
                local par = cur.location;
                MenuItem.curMenu = par;
                par.showMenu(cur);
            }
            else
            {
                /* there's no parent - close the menu */
                MenuItem.isOpen = nil;

                /* tell the UI to close its menu dialog */
                webMainWin.sendWinEvent('<menusys><close/></menusys>');
                webMainWin.menuSysState = '';
            }
        }
        else if (query.isKeyPresent('select'))
        {
            /* 
             *   Select a child menu.  The 'select=n' parameter is the
             *   index in the current menu's child list of the new item to
             *   select.  Retrieve the index.
             */
            MenuItem.curMenu.enterSubMenu(toInteger(query['select']));
        }
        else if (query.isKeyPresent('nextTopic'))
        {
            /* get the next topic in the current topic menu */
            sendAck(req, MenuItem.curMenu.getNextTopicXML());

            /* rebuild the menu state for the change */
            webMainWin.menuSysState =
                '<menusys><<MenuItem.curMenu.getXML(MenuItem.curMenu)
                  >></menusys>';
            
            /* we've sent our reply, so we're done */
            return;
        }
        else if (query.isKeyPresent('chapter'))
        {
            /* get the next or previous chapter, as applicable */
            local dir = query['chapter'];
            local m = MenuItem.curMenu;
            local par = m.location;
            local nxt = (dir == 'next'
                         ? par.getNextMenu(m) : par.getPrevMenu(m));

            /* enter this chapter */
            if (nxt != nil)
            {
                MenuItem.curMenu = nxt;
                nxt.showMenu(m);
            }
        }

        /* acknowledge the request */
        sendAck(req);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Menu topic item - console UI implementation 
 */
modify MenuTopicItem
    /* get the XML description of my menu list */
    getXML(from)
    {
        /* start with an empty result buffer */
        local s = new StringBuffer();

        /* update our contents, as needed */
        updateContents();

        /* add the title and total number of items in the menu */
        s.append('<title><<title.htmlify()>></title>'
                 + '<numItems><<menuContents.length()>></numItems>');

        /* note if we're the top-level menu */
        if (location == nil || MenuItem.topMenu == self)
            s.append('<isTop/>');

        /* add each item in our list */
        for (local i in 1..lastDisplayed)
            s.append(getTopicXML(i));

        /* add the keys */
        getKeysXML(s);

        /* return the XML string */
        return toString(s);
    }

    /* get the next topic, in XML format */
    getNextTopicXML()
    {
        /* if we're not already at the last item, advance the counter */
        if (lastDisplayed < menuContents.length())
            ++lastDisplayed;

        /* format the last item */
        return getTopicXML(lastDisplayed);
    }

    /* get the XML formatted description of the item at the given index */
    getTopicXML(i)
    {
        /* get the item */
        local item = menuContents[i];

        /* get the item's text, and format as XML */
        item = formatXML(dataType(item) == TypeObject
                         ? {: item.getItemText() } : item);

        /* format the item text */
        return '<topicItem><<item>></topicItem>';
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Long topic item 
 */
modify MenuLongTopicItem
    /* get my XML description */
    getXML(from)
    {
        /* start with an empty result buffer */
        local s = new StringBuffer();

        /* update our contents, as needed */
        updateContents();

        /* add my title (heading) */
        local t = heading != nil && heading != '' ? heading : title;
        s.append('<title><<t.htmlify()>></title>');

        /* add our contents */
        s.append('<longTopic><<formatXML({: menuContents })>></longTopic>');

        /* 
         *   if this is a chapter menu, note if we have links for the next
         *   and previous chapters 
         */
        if (isChapterMenu)
        {
            local m;
            
            if ((m = location.getNextMenu(self)) != nil)
                s.append('<nextChapter><<m.title.htmlify()>></nextChapter>');

            if ((m = location.getPrevMenu(self)) != nil)
                s.append('<prevChapter><<m.title.htmlify()>></prevChapter>');
        }

        /* add the keys */
        getKeysXML(s);

        /* return the XML string */
        return toString(s);
    }
;
