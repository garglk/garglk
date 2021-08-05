#charset "us-ascii"

/*
 *   TADS 3 Library - Menu System
 *   
 *   Copyright 2003 by Stephen Granade
 *.  Modifications copyright 2003, 2010 Michael J. Roberts
 *   
 *   This module is designed to make it easy to add on-screen menu trees to
 *   a game.  Note that we're not using the term "menu" in its modern GUI
 *   sense of a compact, mouse-driven pop-up list.  The style of menu we
 *   implement is more like the kind you'd find in old character-mode
 *   terminal programs, where a list of text items takes over the main
 *   window contents.
 *   
 *   Note that in plain-text mode (for interpreters without banner
 *   capabilities), a menu won't be fully usable if it exceeds nine
 *   subitems: each item in a menu is numbered, and the user selects an
 *   item by entering its number; but we only accept a single digit as
 *   input, so only items 1 through 9 can be selected on any given menu.
 *   In practice you probably wouldn't want to create larger menus anyway,
 *   for usability reasons, but this is something to keep in mind.  If you
 *   need more items, you can group some of them into a submenu.
 *   
 *   The user interface for the menu system is implemented in menucon.t for
 *   traditional console interpreter, and in menuweb.t for the Web UI.
 *   
 *   Stephen Granade adapted this module from his TADS 2 menu system, and
 *   Mike Roberts made some minor cosmetic changes to integrate it with the
 *   main TADS 3 library.  
 */

#include "adv3.h"


/* 
 *   General instructions:
 *   
 *   Menus consist of MenuItems, MenuTopicItems, and MenuLongTopicItems.
 *   
 *   - MenuItems are the menu (and sub-menu) items that the player will
 *   select.  Their "title" attribute is what will be shown in the menu,
 *   and the "heading" attribute is shown as the heading while the menu
 *   itself is active; by default, the heading simply uses the title.
 *   
 *   - MenuTopicItems are for lists of topic strings that the player will
 *   be shown, like hints. "title" is what will be shown in the menu;
 *   "menuContents" is a list of either strings to be displayed, one at a
 *   time, or objects which each must return a string via a "menuContents"
 *   method.
 *   
 *   - MenuLongTopicItems are for longer discourses. "title" is what will
 *   be shown in the menu; "menuContents" is either a string to be printed
 *   or a routine to be called.
 *   
 *   adv3.h contains templates for MenuItems, for your convenience.
 *   
 *   A simple example menu:
 *   
 *   FirstMenu: MenuItem 'Test menu';
 *.  + MenuItem 'Pets';
 *.  ++ MenuItem 'Chinchillas';
 *.  +++ MenuTopicItem 'About them'
 *.    menuContents = ['Furry', 'South American', 'Curious',
 *   'Note: Not a coat'];
 *.  +++ MenuTopicItem 'Benefits'
 *.    menuContents = ['Non-allergenic', 'Cute', 'Require little space'];
 *.  +++ MenuTopicItem 'Downsides'
 *.     menuContents = ['Require dust baths', 'Startle easily'];
 *.  ++ MenuItem 'Cats';
 *.  +++ MenuLongTopicItem 'Pure evil'
 *.     menuContents = 'Cats are, quite simply, pure evil. I would provide
 *.                     ample evidence were there room for it in this
 *.                     simple example.';
 *.  +++ MenuTopicItem 'Benefits'
 *.    menuContents = ['They, uh, well...', 'Okay, I can\'t think of any.'];
 */

/* ------------------------------------------------------------------------ */
/*
 *   Menu output stream.  We run topic contents through this output stream
 *   to allow topics to use the special paragraph and style tag markups.  
 */
transient menuOutputStream: OutputStream
    /* 
     *   Process a function call through the stream.  If the function
     *   generates any output, we capture it.  If the function simply
     *   returns text, we run it through the filters. 
     */
    captureOutput(val)
    {
        /* reset our buffer */
        buf_.deleteChars(1);

        /* call the function while capturing its output */
        outputManager.withOutputStream(menuOutputStream, function()
        {
            /* if it's a function, invoke it */
            if (dataType(val) != TypeSString)
                val = val();

            /* if we have a string, run it through my filters */
            if (dataType(val) == TypeSString)
                writeToStream(val);
        });

        /* return my captured output */
        return toString(buf_);
    }

    /* we capture our output to a string buffer */
    writeFromStream(txt) { buf_.append(txt); }

    /* initialize */
    execute()
    {
        inherited();
        buf_ = new StringBuffer();
        addOutputFilter(typographicalOutputFilter);
        addOutputFilter(menuParagraphManager);
        addOutputFilter(styleTagFilter);
    }

    /* our capture buffer (a StringBuffer object) */
    buf_ = nil
;

/*
 *   Paragraph manager for the menu output stream. 
 */
transient menuParagraphManager: ParagraphManager
;

/* ------------------------------------------------------------------------ */
/*
 *   A basic menu object.  This is an abstract base class that
 *   encapsulates some behavior common to different menu classes, and
 *   allows the use of the + syntax (like "+ MenuItem") to define
 *   containment.
 */
class MenuObject: object
    /* our contents list */
    contents = []

    /* 
     *   Since we're inheriting from object, but need to use the "+"
     *   syntax, we need to set up the contents appropriately
     */
    initializeLocation()
    {
        if (location != nil)
            location.addToContents(self);
    }

    /* add a menu item */
    addToContents(obj)
    {
        /* 
         *   If the menu has a nil menuOrder, and it inherits menuOrder
         *   from us, then it must be a dynamically-created object that
         *   doesn't provide a custom menuOrder.  Provide a suitable
         *   default of a value one higher than the highest menuOrder
         *   currently in our list, to ensure that the item always sorts
         *   after any items currently in the list. 
         */
        if (obj.menuOrder == nil && !overrides(obj, MenuObject, &menuOrder))
        {
            local maxVal;
            
            /* find the maximum current menuOrder value */
            maxVal = nil;
            foreach (local cur in contents)
            {
                /* 
                 *   if this one has a value, and it's the highest so far
                 *   (or the only one with a value we've found so far),
                 *   take it as the maximum so far 
                 */
                if (cur.menuOrder != nil
                    && (maxVal == nil || cur.menuOrder > maxVal))
                    maxVal = cur.menuOrder;
            }

            /* if we didn't find any values, use 0 as the arbitrary default */
            if (maxVal == nil)
                maxVal = 0;

            /* go one higher than the maximum of the existing items */
            obj.menuOrder = maxVal;
        }

        /* add the item to our contents list */
        contents += obj;
    }

    /*
     *   The menu order.  When we're about to show a list of menu items,
     *   we'll sort the list in ascending order of this property, then in
     *   ascending order of title.  By default, we set this order value to
     *   be equal to the menu item's sourceTextOrder. This makes the menu
     *   order default to the order of objects as defined in the source. If
     *   some other basis is desired, override topicOrder.  
     */
    menuOrder = (sourceTextOrder)

    /*
     *   Compare this menu object to another, for the purposes of sorting a
     *   list of menu items. Returns a positive number if this menu item
     *   sorts after the other one, a negative number if this menu item
     *   sorts before the other one, 0 if the relative order is arbitrary.
     *   
     *   By default, we'll sort by menuOrder if the menuOrder values are
     *   different, otherwise arbitrarily.  
     */
    compareForMenuSort(other)
    {
        /* 
         *   if one menuOrder value is nil, sort it earlier than the other;
         *   if they're both nil, they sort as equivalent 
         */
        if (menuOrder == nil && other.menuOrder == nil)
            return 0;
        else if (menuOrder == nil)
            return -1;
        else if (other.menuOrder == nil)
            return 1;

        /* return the difference of the sort order values */
        return menuOrder - other.menuOrder;
    }

    /* 
     *   Finish initializing our contents list.  This will be called on
     *   each MenuObject *after* we've called initializeLocation() on every
     *   object.  In other words, every menu will already have been added
     *   to its parent's contents; this can do anything else that's needed
     *   to initialize the contents list.  For example, some subclasses
     *   might want to sort their contents here, so that they list their
     *   menus in a defined order.  By default, we sort the menu items by
     *   menuOrder; subclasses can override this as needed.  
     */
    initializeContents()
    {
        /* sort our contents list in the object-defined sorting order */
        contents = contents.sort(
            SortAsc, {a, b: a.compareForMenuSort(b)});
    }
;

/* 
 *   This preinit object makes sure the MenuObjects all have their
 *   contents initialized properly.
 */
PreinitObject
    execute()
    {
        /* initialize each menu's location */
        forEachInstance(MenuObject, { menu: menu.initializeLocation() });

        /* do any extra work to initialize each menu's contents list */
        forEachInstance(MenuObject, { menu: menu.initializeContents() });
    }
;

/* ------------------------------------------------------------------------ */
/* 
 *   A MenuItem is a given item in the menu tree.  In general all you need
 *   to do to use menus is create a tree of MenuItems with titles.
 *   
 *   To display a menu tree, call displayMenu() on the top menu in the
 *   tree.  That routine displays the menu and processes user input until
 *   the user dismisses the menu, automatically displaying submenus as
 *   necessary.  
 */
class MenuItem: MenuObject
    /* the name of the menu; this is listed in the parent menu */
    title = ''

    /* 
     *   the heading - this is shown when this menu is active; by default,
     *   we simply use the title 
     */
    heading = (title)

    /*
     *   Display properties.  These properties control the way the menu
     *   appears on the screen.  By default, a menu looks to its parent
     *   menu for display properties; this makes it easy to customize an
     *   entire menu tree, since changes in the top-level menu will cascade
     *   to all children that don't override these settings.  However, each
     *   menu can customize its own appearance by overriding these
     *   properties itself.
     *   
     *   'fgcolor' and 'bgcolor' are the foreground (text) and background
     *   colors, expressed as HTML color names (so '#nnnnnn' values can be
     *   used to specify RGB colors).
     *   
     *   'indent' is the number of pixels to indent the menu's contents
     *   from the left margin.  This is used only in HTML mode.
     *   
     *   'fullScreenMode' indicates whether the menu should take over the
     *   entire screen, or limit itself to the space it actually requires.
     *   Full screen mode makes the menu block out any game window text.
     *   Limited mode leaves the game window partially uncovered, but can
     *   be a bit jumpy, since the window changes size as the user
     *   navigates through different menus.  
     */

    /* foreground (text) and background colors, as HTML color names */
    fgcolor = (location != nil ? location.fgcolor : 'text')
    bgcolor = (location != nil ? location.bgcolor : 'bgcolor')

    /* 
     *   Foreground and background colors for the top instructions bar.
     *   By default, we use the color scheme of the parent menu, or the
     *   inverse of our main menu color scheme if we're the top menu. 
     */
    topbarfg = (location != nil ? location.topbarfg : 'statustext')
    topbarbg = (location != nil ? location.topbarbg : 'statusbg')

    /* number of spaces to indent the menu's contents */
    indent = (location != nil ? location.indent : '10')
    
    /* 
     *   full-screen mode: make our menu take up the whole screen (apart
     *   from the instructions bar, of course) 
     */
    fullScreenMode = (location != nil ? location.fullScreenMode : true)
    
    /* 
     *   The keys used to navigate the menus, in order:
     *   
     *   [quit, previous, up, down, select]
     *   
     *   Since multiple keys can be used for the same navigation, the list
     *   is implemented as a List of Lists.  Keys must be given as
     *   lower-case in order to match input, since we convert all input
     *   keys to lower-case before matching them.
     *   
     *   In the sublist for each key, we use the first element as the key
     *   name we show in the instruction bar at the top of the screen.
     *   
     *   By default, we use our parent menu's key list, if we have a
     *   parent; if we have no parent, we use the standard keys from the
     *   library messages.
     */
    keyList = (location != nil ? location.keyList : gLibMessages.menuKeyList)

    /* 
     *   the current key list - we'll set this on entry to the start of
     *   each showMenuXxx method, so that we keep track of the actual key
     *   list in use, as inherited from the top-level menu 
     */
    curKeyList = nil

    /*
     *   Title for the link to the previous menu, if any.  If the menu has
     *   a parent menu, we'll display this link next to the menu title in
     *   the top instructions/title bar.  If this is nil, we won't display
     *   a link at all.  Note that this can contain an HTML fragment; for
     *   example, you could use an <IMG> tag to display an icon here.  
     */
    prevMenuLink = (location != nil ? gLibMessages.prevMenuLink : nil)

    /* 
     *   Update our contents.  By default, we'll do nothing; subclasses
     *   can override this to manage dynamic menus if desired.  This is
     *   called just before the menu is displayed, each time it's
     *   displayed. 
     */
    updateContents() { }


    /*
     *   Get the next menu in our list following the given menu.  Returns
     *   nil if we don't find the given menu, or the given menu is the last
     *   menu. 
     */
    getNextMenu(menu)
    {
        /* find the menu in our contents list */
        local idx = contents.indexOf(menu);

        /* 
         *   if we found it, and it's not the last, return the menu at the
         *   next index; otherwise return nil 
         */
        return (idx != nil && idx < contents.length()
                ? contents[idx + 1] : nil);
    }

    /*
     *   Get the menu previous tot he given menu.  Returns nil if we don't
     *   find the given menu or the given menu is the first one. 
     */
    getPrevMenu(menu)
    {
        /* find the menu in our contents list */
        local idx = contents.indexOf(menu);

        /* 
         *   if we found it, and it's not the first, return the menu at the
         *   prior index; otherwise return nil 
         */
        return (idx != nil && idx > 1 ? contents[idx - 1] : nil);
    }

    /* get the index in the parent of the given child menu */
    getChildIndex(child)
    {
        return contents.indexOf(child);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   MenuTopicItem displays a series of entries successively.  This is
 *   intended to be used for displaying something like a list of hints for
 *   a topic.  Set menuContents to be a list of strings to be displayed.  
 */
class MenuTopicItem: MenuItem
    /* the name of this topic, as it appears in our parent menu */
    title = ''

    /* heading, displayed while we're showing this topic list */
    heading = (title)

    /* hyperlink text for showing the next menu */
    nextMenuTopicLink = (gLibMessages.nextMenuTopicLink)

    /* 
     *   A list of strings and/or MenuTopicSubItem items.  Each one of
     *   these is meant to be something like a single hint on our topic.
     *   We display these items one at a time when our menu item is
     *   selected.  
     */
    menuContents = []

    /* the index of the last item we displayed from our menuContents list */
    lastDisplayed = 1

    /* 
     *   The maximum number of our sub-items that we'll display at once.
     *   This is only used on interpreters with banner capabilities, and is
     *   ignored in full-screen mode.  
     */
    chunkSize = 6

    /* we'll display this after we've shown all of our items */
    menuTopicListEnd = (gLibMessages.menuTopicListEnd)
;

/* ------------------------------------------------------------------------ */
/*
 *   A menu topic sub-item can be used to represent an item in a
 *   MenuTopicItem's list of display items.  This can be useful when
 *   displaying a topic must trigger a side-effect.  
 */
class MenuTopicSubItem: object
    /*
     *   Get the item's text.  By default, we just return an empty string.
     *   This should be overridden to return the appropriate text, and can
     *   also trigger any desired side-effects.  
     */
    getItemText() { return ''; }
;

/* ------------------------------------------------------------------------ */
/* 
 *   Long Topic Items are used to print out big long gobs of text on a
 *   subject.  Use it for printing long treatises on your design
 *   philosophy and the like.  
 */
class MenuLongTopicItem: MenuItem
    /* the title of the menu, shown in parent menus */
    title = ''

    /* the heading, shown while we're displaying our contents */
    heading = (title)

    /* either a string to be displayed, or a method returning a string */
    menuContents = ''

    /* 
     *   Flag - this is a "chapter" in a list of chapters.  If this is set
     *   to true, then we'll offer the options to proceed directly to the
     *   next and previous chapters.  If this is nil, we'll simply wait for
     *   acknowledgment and return to the parent menu. 
     */
    isChapterMenu = nil

    /* the message we display at the end of our text */
    menuLongTopicEnd = (gLibMessages.menuLongTopicEnd)
;

