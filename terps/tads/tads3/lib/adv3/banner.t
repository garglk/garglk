#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: banner manager
 *   
 *   This module defines the banner manager, which provides high-level
 *   services to create and manipulate banner windows.
 *   
 *   A "banner" is an independent window shown within the interpreter's
 *   main application display frame (which might be the entire screen on a
 *   character-mode terminal, or could be a window in a GUI system).  The
 *   game can control the creation and destruction of banner windows, and
 *   can control their placement and size.
 *   
 *   This implementation is based in part on Steve Breslin's banner
 *   manager, used by permission.  
 */

#include "adv3.h"

/* ------------------------------------------------------------------------ */
/*
 *   A BannerWindow corresponds to an on-screen banner.  For each banner
 *   window a game wants to display, the game must create an object of this
 *   class.
 *   
 *   Note that merely creating a BannerWindow object doesn't actually
 *   display a banner window.  Once a BannerWindow is created, the game
 *   must call the object's showBanner() method to create the on-screen
 *   window for the banner.
 *   
 *   BannerWindow instances are intended to be persistent (not transient).
 *   The banner manager keeps track of each banner window that's actually
 *   being displayed separately via an internal transient object; the game
 *   doesn't need to worry about these tracking objects, since the banner
 *   manager automatically handles them.  
 */
class BannerWindow: OutputStreamWindow
    /*
     *   Construct the object.
     *   
     *   'id' is a globally unique identifying string for the banner.  When
     *   we dynamically create a banner object, we have to provide a unique
     *   identifying string, so that we can correlate transient on-screen
     *   banners with the banners in a saved state when restoring the saved
     *   state.
     *   
     *   Note that no ID string is needed for BannerWindow objects defined
     *   statically at compile-time, because the object itself ('self') is
     *   a suitably unique and stable identifier.  
     */
    construct(id)
    {
        /* remember my unique identifier */
        id_ = id;
    }

    /*
     *   Show the banner.  The game should call this method when it first
     *   wants to display the banner.
     *   
     *   'parent' is the parent banner; this is an existing BannerWindow
     *   object.  If 'parent' is nil, then the parent is the main game
     *   text window.  The new window's display space is obtained by
     *   carving space out of the parent's area, according to the
     *   alignment and size values specified.
     *   
     *   'where' and 'other' give the position of the banner among the
     *   children of the given parent.  'where' is one of the constants
     *   BannerFirst, BannerLast, BannerBefore, or BannerAfter.  If
     *   'where' is BannerBefore or BannerAfter, 'other' gives the
     *   BannerWindow object to be used as the reference point in the
     *   parent's child list; 'other' is ignored in other cases.  Note
     *   that 'other' must always be another child of the same parent; if
     *   it's not, then we act as though 'where' were given as BannerLast.
     *   
     *   'windowType' is a BannerTypeXxx constant giving the new window's
     *   type.
     *   
     *   'align' is a BannerAlignXxx constant giving the alignment of the
     *   new window.  'size' is an integer giving the size of the banner,
     *   in units specified by 'sizeUnits', which is a BannerSizeXxx
     *   constant.  If 'size' is nil, it indicates that the caller doesn't
     *   care about the size, usually because the caller will be resizing
     *   the banner soon anyway; the banner will initially have zero size
     *   in this case if we create a new window, or will retain the
     *   existing size if there's already a system window.
     *   
     *   'styleFlags' is a combination of BannerStyleXxx constants
     *   (combined with the bitwise OR operator, '|'), giving the requested
     *   display style of the new banner window.
     *   
     *   Note that if we already have a system banner window, and the
     *   existing banner window has the same characteristics as the new
     *   creation parameters, we'll simply re-use the existing window
     *   rather than closing and re-creating it; this reduces unnecessary
     *   redrawing in cases where the window isn't changing.  If the caller
     *   explicitly wants to create a new window even if we already have a
     *   window, the caller should simply call removeBanner() before
     *   calling this routine.  
     */
    showBanner(parent, where, other, windowType,
               align, size, sizeUnits, styleFlags)
    {
        local parentID;
        local otherID;

        /* note the ID's of the parent window and the insertion point */
        parentID = (parent != nil ? parent.getBannerID() : nil);
        otherID = (other != nil ? other.getBannerID() : nil);

        /* 
         *   if we have an 'other' specified, its parent must match our
         *   proposed parent; otherwise, ignore 'other' and insert at the
         *   end of the parent list 
         */
        if (other != nil && other.parentID_ != parentID)
        {
            other = nil;
            where = BannerLast;
        }

        /* if we already have an existing banner window, check for a match */
        if (handle_ != nil)
        {
            local t;
            local match;

            /* presume we won't find an exact match */
            match = nil;

            /* we already have a window - get the UI tracker object */
            if ((t = bannerUITracker.getTracker(self)) != nil)
            {
                /* check the placement, window type, alignment, and style */
                match = (t.windowType_ == windowType
                         && t.parentID_ == parentID
                         && t.align_ == align
                         && t.styleFlags_ == styleFlags
                         && bannerUITracker.orderMatches(t, where, otherID));
            }

            /* 
             *   if it doesn't match the existing window, close it, so that
             *   we will open a brand new window with the new
             *   characteristics 
             */
            if (!match)
                removeBanner();
        }

        /* if the system-level banner doesn't already exist, create it */
        if (handle_ == nil)
        {
            /* create my system-level banner window */
            if (!createSystemBanner(parent, where, other, windowType, align,
                                    size, sizeUnits, styleFlags))
            {
                /* we couldn't create the system banner - give up */
                return nil;
            }

            /* create our output stream */
            createOutputStream();

            /* add myself to the UI tracker's active banner list */
            bannerUITracker.addBanner(handle_, outputStream_, getBannerID(),
                                      parentID, where, other, windowType,
                                      align, styleFlags);
        }
        else
        {
            /* 
             *   Our system-level window already exists, so we don't need
             *   to create a new one.  However, our size could be
             *   different, so explicitly set the requested size if it
             *   doesn't match our recorded size.  If the size is given as
             *   nil, leave the size as it is; a nil size indicates that
             *   the caller doesn't care about the size (probably because
             *   the caller is going to change the size shortly anyway),
             *   so we can avoid unnecessary redrawing by leaving the size
             *   as it is for now.  
             */
            if (size != nil && (size != size_ || sizeUnits != sizeUnits_))
                bannerSetSize(handle_, size, sizeUnits, nil);
        }

        /* 
         *   remember the creation parameters, so that we can re-create the
         *   banner with the same characteristics in the future if we
         *   should need to restore the banner from a saved position 
         */
        parentID_ = parentID;
        windowType_ = windowType;
        align_ = align;
        size_ = size;
        sizeUnits_ = sizeUnits;
        styleFlags_ = styleFlags;

        /* 
         *   Add myself to the persistent banner tracker's active list.  Do
         *   this even if we already had a system handle, since we might be
         *   initializing the window as part of a persistent restore
         *   operation, in which case the persistent tracking object might
         *   not yet exist.  (This seems backwards: if we're restoring a
         *   persistent state, surely the persistent tracker would already
         *   exist.  In fact, the case we're really handling is where the
         *   window is open in the transient UI, because it was already
         *   open in the ongoing session; but the persistent state we're
         *   restoring doesn't include the window.  This is most likely to
         *   occur after a RESTART, since we could have a window that is
         *   always opened immediately at start-up and thus will be in the
         *   transient state up to and through the RESTART, but is only
         *   created as part of the initialization process.)  
         */
        bannerTracker.addBanner(self, parent, where, other);

        /* indicate success */
        return true;
    }

    /*
     *   Remove the banner.  This removes the banner's on-screen window.
     *   The BannerWindow object itself remains valid, but after this
     *   method returns, the BannerWindow no longer has an associated
     *   display window.
     *   
     *   Note that any child banners of ours will become undisplayable
     *   after we're gone.  A child banner depends upon its parent to
     *   obtain display space, so once the parent is gone, its children no
     *   longer have any way to obtain any display space.  Our children
     *   remain valid objects even after we're closed, but they won't be
     *   visible on the display.    
     */
    removeBanner()
    {
        /* if I don't have a system-level handle, there's nothing to do */
        if (handle_ == nil)
            return;

        /* remove my system-level banner window */
        bannerDelete(handle_);

        /* our system-level window is gone, so forget its handle */
        handle_ = nil;

        /* we only need an output stream when we're active */
        outputStream_ = nil;

        /* remove myself from the UI trackers's active list */
        bannerUITracker.removeBanner(getBannerID());

        /* remove myself from the persistent banner tracker's active list */
        bannerTracker.removeBanner(self);
    }

    /* write the given text to the banner */
    writeToBanner(txt)
    {
        /* write the text to our underlying output stream */
        outputStream_.writeToStream(txt);
    }

    /* flush any pending output to the banner */
    flushBanner() { bannerFlush(handle_); }

    /*
     *   Set the banner window to a specific size.  'size' is the new
     *   size, in units given by 'sizeUnits', which is a BannerSizeXxx
     *   constant.
     *   
     *   'isAdvisory' is true or nil; if true, it indicates that the size
     *   setting is purely advisory, and that a sizeToContents() call will
     *   eventually follow to set the actual size.  When 'isAdvisory is
     *   true, the interpreter is free to ignore the request if
     *   sizeToContents() 
     */
    setSize(size, sizeUnits, isAdvisory)
    {
        /* set the underlying system window size */
        bannerSetSize(handle_, size, sizeUnits, isAdvisory);

        /* 
         *   remember my new size in case we have to re-create the banner
         *   from a saved state 
         */
        size_ = size;
        sizeUnits_ = sizeUnits;
    }

    /*
     *   Size the banner to its current contents.  Note that some systems
     *   do not support this operation, so callers should always make an
     *   advisory call to setSize() first to set a size based on the
     *   expected content size.  
     */
    sizeToContents()
    {
        /* size our system-level window to our contents */
        bannerSizeToContents(handle_);
    }

    /*
     *   Clear my banner window.  This clears out all of the contents of
     *   our on-screen display area.  
     */
    clearWindow()
    {
        /* clear our system-level window */
        bannerClear(handle_);
    }

    /* set the text color in the banner */
    setTextColor(fg, bg) { bannerSetTextColor(handle_, fg, bg); }

    /* set the screen color in the banner window */
    setScreenColor(color) { bannerSetScreenColor(handle_, color); }

    /* 
     *   Move the cursor to the given row/column position.  This can only
     *   be used with text-grid banners; for ordinary text banners, this
     *   has no effect. 
     */
    cursorTo(row, col) { bannerGoTo(handle_, row, col); }

    /*
     *   Get the banner identifier.  If our 'id_' property is set to nil,
     *   we'll assume that we're a statically-defined object, in which case
     *   'self' is a suitable identifier.  Otherwise, we'll return the
     *   identifier string. 
     */
    getBannerID() { return id_ != nil ? id_ : self; }

    /*
     *   Restore this banner.  This is called after a RESTORE or UNDO
     *   operation that finds that this banner was being displayed at the
     *   time the state was saved but is not currently displayed in the
     *   active UI.  We'll show the banner using the characteristics saved
     *   persistently.
     */
    showForRestore(parent, where, other)
    {
        /* show myself, using my saved characteristics */
        showBanner(parent, where, other, windowType_, align_,
                   size_, sizeUnits_, styleFlags_);

        /* update my contents */
        updateForRestore();
    }

    /*
     *   Create the system-level banner window.  This can be customized as
     *   needed, although this default implementation should be suitable
     *   for most instances.
     *   
     *   Returns true if we are successful in creating the system window,
     *   nil if we fail.  
     */
    createSystemBanner(parent, where, other, windowType, align,
                       size, sizeUnits, styleFlags)
    {
        /* create the system-level window */
        handle_ = bannerCreate(parent != nil ? parent.handle_ : nil,
                               where, other != nil ? other.handle_ : nil,
                               windowType, align, size, sizeUnits,
                               styleFlags);

        /* if we got a valid handle, we succeeded */
        return (handle_ != nil);
    }

    /* create our banner output stream */
    createOutputStreamObj()
    {
        return new transient BannerOutputStream(handle_);
    }

    /*
     *   Update my contents after being restored.  By default, this does
     *   nothing; instances might want to override this to refresh the
     *   contents of the banner if the banner is normally updated only in
     *   response to specific events.  Note that it's not necessary to do
     *   anything here if the banner will soon be updated automatically as
     *   part of normal processing; for example, the status line banner is
     *   updated at each new command line via a prompt-daemon, so there's
     *   no need for the status line banner to do anything here.  
     */
    updateForRestore()
    {
        /* do nothing by default; subclasses can override as needed */
    }

    /*
     *   Initialize the banner window.  This is called during
     *   initialization (when first starting the game, or when resetting
     *   with RESTART).  If the banner is to be displayed from the start of
     *   the game, this can set up the on-screen display.
     *   
     *   Note that we might already have an on-screen handle when this is
     *   called.  This indicates that we're restarting an ongoing session,
     *   and that this banner already existed in the session before the
     *   RESTART operation.  If desired, we can attach ourselves to the
     *   existing on-screen banner, avoiding the redrawing that would occur
     *   if we created a new window.
     *   
     *   If this window depends upon another window for its layout order
     *   placement (i.e., we'll call showBanner() with another BannerWindow
     *   given as the 'other' parameter), then this routine should call the
     *   other window's initBannerWindow() method before creating its own
     *   window, to ensure that the other window has a system window and
     *   thus will be meaningful to establish the layout order.
     *   
     *   Overriding implementations should check the 'inited_' property.
     *   If this property is true, then it can be assumed that we've
     *   already been initialized and don't require further initialization.
     *   This routine can be called multiple times because dependent
     *   windows might call us directly, before we're called for our
     *   regular initialization.  
     */
    initBannerWindow()
    {
        /* by default, simply note that we've been initialized */
        inited_ = true;
    }

    /* flag: this banner has been initialized with initBannerWindow() */
    inited_ = nil

    /* 
     *   The creator-assigned ID string to identify the banner
     *   persistently.  This is only needed for banners created
     *   dynamically; for BannerWindow objects defined statically at
     *   compile time, simply leave this value as nil, and we'll use the
     *   object itself as the identifier.  
     */
    id_ = nil

    /* the handle to my system-level banner window */
    handle_ = nil

    /* 
     *   Creation parameters.  We store these when we create the banner,
     *   and update them as needed when the banner's display attributes
     *   are changed.  
     */
    parentID_ = nil
    windowType_ = nil
    align_ = nil
    size_ = nil
    sizeUnits_ = nil
    styleFlags_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Banner Output Stream.  This is a specialization of OutputStream that
 *   writes to a banner window.  
 */
class BannerOutputStream: OutputStream
    /* construct */
    construct(handle)
    {
        /* inherit base class constructor */
        inherited();
        
        /* remember my banner window handle */
        handle_ = handle;
    }

    /* execute preinitialization */
    execute()
    {
        /*
         *   We shouldn't need to do anything during pre-initialization,
         *   since we should always be constructed dynamically by a
         *   BannerWindow.  Don't even inherit the base class
         *   initialization, since it could clear out state that we want to
         *   keep through a restart, restore, etc.  
         */
    }

    /* write text from the stream to the interpreter I/O system */
    writeFromStream(txt)
    {
        /* write the text to the underlying system banner window */
        bannerSay(handle_, txt);
    }

    /* our system-level banner window handle */
    handle_ = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   The banner UI tracker.  This object keeps track of the current user
 *   interface display state; this object is transient because the
 *   interpreter's user interface is not part of the persistence
 *   mechanism.  
 */
transient bannerUITracker: object
    /* add a banner to the active display list */
    addBanner(handle, ostr, id, parentID, where, other,
              windowType, align, styleFlags)
    {
        local uiWin;
        local parIdx;
        local idx;

        /* create a transient BannerUIWindow object to track the banner */
        uiWin = new transient BannerUIWindow(handle, ostr, id, parentID,
                                             windowType, align, styleFlags);

        /* 
         *   Find the parent in the list.  If there's no parent, the
         *   parent is the main window; consider it to be at imaginary
         *   index zero in the list. 
         */
        parIdx = (parentID == nil
                  ? 0 : activeBanners_.indexWhich({x: x.id_ == parentID}));

        /* insert the banner at the proper point in our list */
        switch(where)
        {
        case BannerFirst:
            /* 
             *   insert as the first child of the parent - put it
             *   immediately after the parent in the list 
             */
            activeBanners_.insertAt(parIdx + 1, uiWin);
            break;

        case BannerLast:
        ins_last:
            /* 
             *   Insert as the last child of the parent: insert
             *   immediately after the last window that descends from the
             *   parent.  
             */
            activeBanners_.insertAt(skipDescendants(parIdx), uiWin);
            break;

        case BannerBefore:
        case BannerAfter:
            /* find the reference point ID in our list */
            idx = activeBanners_.indexWhich(
                {x: x.id_ == other.getBannerID()});

            /* 
             *   if we didn't find the reference point, or the reference
             *   point item doesn't have the same parent as the new item,
             *   then ignore the reference point and instead insert at the
             *   end of the parent's child list 
             */
            if (idx == nil || activeBanners_[idx].parentID_ != parentID)
                goto ins_last;

            /* 
             *   if inserting after, skip the reference item and all
             *   of its descendants 
             */
            if (where == BannerAfter)
                idx = skipDescendants(idx);

            /* insert at the position we found */
            activeBanners_.insertAt(idx, uiWin);
            break;
        }
    }

    /*
     *   Given an index in our list of active windows, skip the given item
     *   and all items whose windows are descended from this window.
     *   We'll leave the index positioned on the next entry in the list
     *   that isn't a descendant of the window at the given index.  Note
     *   that this skips not only children but grandchildren (and so on)
     *   as well.  
     */
    skipDescendants(idx)
    {
        local parentID;

        /* 
         *   if the index is zero, it's the main window; all windows are
         *   children of the root window, so return the next index after
         *   the last item 
         */
        if (idx == 0)
            return activeBanners_.length() + 1;

        /* note ID of the parent item */
        parentID = activeBanners_[idx].id_;
        
        /* skip the parent item */
        ++idx;

        /* keep going as long as we see children of the parent */
        while (idx <= activeBanners_.length()
               && activeBanners_[idx].parentID_ == parentID)
        {
            /* 
             *   This is a child of the given parent, so we must skip it;
             *   we must also skip its descendants, since they're all
             *   indirectly descendants of the original parent.  So,
             *   simply skip this item and its descendants with a
             *   recursive call to this routine.
             */
            idx = skipDescendants(idx);
        }

        /* return the new index */
        return idx;
    }

    /* remove a banner from the active display list */
    removeBanner(id)
    {
        local idx;

        /* find the entry with the given ID, and remove it */
        if ((idx = activeBanners_.indexWhich({x: x.id_ == id})) != nil)
        {
            local lastIdx;
            
            /* 
             *   After removing an item, its children are no longer
             *   displayable, because a child obtains display space from
             *   its parent.  So, we must remove any children of this item
             *   at the same time we remove the item itself.  Find the
             *   index of the next item after all of our descendants, so
             *   that we can remove the item and its children all at once.
             *   An item and its descendants are always contiguous in our
             *   list, since we store children immediately after their
             *   parents, so we can simply remove the range of items from
             *   the specified item to its last descendant.
             *   
             *   Note that skipDescendants() returns the index of the
             *   first item that is NOT a descendant; so, decrement the
             *   result so that we end up with the index of the last
             *   descendant.  
             */
            lastIdx = skipDescendants(idx) - 1;

            /* remove the item and all of its children */
            activeBanners_.removeRange(idx, lastIdx);
        }
    }

    /* get the BannerUIWindow tracker object for a given BannerWindow */
    getTracker(win)
    {
        local id;

        /* get the window's ID */
        id = win.getBannerID();
        
        /* return the tracker with the same ID as the given BannerWindow */
        return activeBanners_.valWhich({x: x.id_ == id});
    }

    /* check a BannerUIWindow to see if it matches the given layout order */
    orderMatches(uiWin, where, otherID)
    {
        local idx;
        local otherIdx;
        local parentID;
        local parIdx;

        /* get the list index of the given window */
        idx = activeBanners_.indexOf(uiWin);

        /* get the list index of the reference point window */
        otherIdx = (otherID != nil
                    ? activeBanners_.indexWhich({x: x.id_ == otherID}) : nil);

        /* 
         *   find the parent item (using imaginary index zero for the
         *   root, which we can think of as being just before the first
         *   item in the list)
         */
        parentID = uiWin.parentID_;
        parIdx = (parentID == nil
                  ? 0 : activeBanners_.indexWhich({x: x.id_ == parentID}));

        /* 
         *   if 'other' is specified, it has to have our same parent; if
         *   it has a different parent, it's not a match 
         */
        if (otherID != nil && parentID != activeBanners_[otherIdx].parentID_)
            return nil;

        /* 
         *   if there's no such window in the list, it can't match the
         *   given placement no matter what the given placement is, as it
         *   has no placement 
         */
        if (idx == nil)
            return nil;
        
        /* check the requested layout order */
        switch (where)
        {
        case BannerFirst:
            /* make sure it's immediately after the parent */
            return idx == parIdx + 1;

        case BannerLast:
            /* 
             *   Make sure it's the last child of the parent.  To do this,
             *   make sure that the next item after this item's last
             *   descendant is the same as the next item after the
             *   parent's last descendant. 
             */
            return skipDescendants(idx) == skipDescendants(parIdx);

        case BannerBefore:
            /* 
             *   we want this item to come before 'other', so make sure
             *   the next item after all of this item's descendants is
             *   'other' 
             */
            return skipDescendants(idx) == otherIdx;

        case BannerAfter:
            /* 
             *   we want this item to come just after 'other', so make
             *   sure that the next item after all of the descendants of
             *   'other' is this item 
             */
            return skipDescendants(otherIdx) == idx;

        default:
            /* other layout orders are invalid */
            return nil;
        }
    }

    /*
     *   The vector of banners currently on the screen.  This is a list of
     *   transient BannerUIWindow objects, stored in the same order as the
     *   banner layout list.  
     */
    activeBanners_ = static new transient Vector(32)
;

/*
 *   A BannerUIWindow object.  This keeps track of the transient UI state
 *   of a banner window while it appears on the screen.  We create only
 *   transient instances of this class, since it tracks what's actually
 *   displayed at any given time.  
 */
class BannerUIWindow: object
    /* construct */
    construct(handle, ostr, id, parentID, windowType, align, styleFlags)
    {
        /* remember the banner's data */
        handle_ = handle;
        outputStream_ = ostr;
        id_ = id;
        parentID_ = parentID;
        windowType_ = windowType;
        align_ = align;
        styleFlags_ = styleFlags;
    }

    /* the system-level banner handle */
    handle_ = nil

    /* the banner's ID */
    id_ = nil

    /* the parent banner's ID (nil if this is a top-level banner) */
    parentID_ = nil

    /* 
     *   The banner's output stream.  Output streams are always transient,
     *   so hang on to each active banner's stream so that we can plug it
     *   back in on restore. 
     */
    outputStream_ = nil

    /* creation parameters of the banner */
    windowType_ = nil
    align_ = nil
    styleFlags_ = nil

    /* 
     *   Scratch-pad for our association to our BannerWindow object.  We
     *   only use this during the RESTORE process, to tie the transient
     *   object back to the proper persistent object. 
     */
    win_ = nil
;

/*
 *   The persistent banner tracker.  This keeps track of the active banner
 *   windows persistently.  Whenever we save or restore the game's state,
 *   this object will be saved or restored along with the state.  When we
 *   restore a previously saved state, we can look at this object to
 *   determine which banners were active at the time the state was saved,
 *   and use this information to restore the same active banners in the
 *   user interface.
 *   
 *   This is a post-restore and post-undo object, so we're notified via our
 *   execute() method whenever we restore a saved state using RESTORE or
 *   UNDO.  When we restore a saved state, we'll restore the banner display
 *   conditions as they existed in the saved state.  
 */
bannerTracker: PostRestoreObject, PostUndoObject
    /* add a banner to the active display list */
    addBanner(win, parent, where, other)
    {
        local parIdx;
        local otherIdx;
        
        /* 
         *   Don't add it if it's already in the list.  If we're restoring
         *   the banner from persistent state, it'll already be in the
         *   active list, since the active list is the set of windows
         *   we're restoring in the first place. 
         */
        if (activeBanners_.indexOf(win) != nil)
            return;

        /* find the parent among the existing windows */
        parIdx = (parent == nil ? 0 : activeBanners_.indexOf(parent));

        /* note the index of 'other' */
        otherIdx = (other == nil ? nil : activeBanners_.indexOf(other));

        /* insert the banner at the proper point in our list */
        switch(where)
        {
        case BannerFirst:
            /* insert immediately after the parent */
            activeBanners_.insertAt(parIdx + 1, win);
            break;

        case BannerLast:
        ins_last:
            /* insert after the parent's last descendant */
            activeBanners_.insertAt(skipDescendants(parIdx), win);
            break;

        case BannerBefore:
        case BannerAfter:
            /* 
             *   if we didn't find the reference point, insert at the end
             *   of the parent's child list 
             */
            if (otherIdx == nil)
                goto ins_last;

            /* 
             *   if inserting after, skip the reference item and all of
             *   its descendants 
             */
            if (where == BannerAfter)
                otherIdx = skipDescendants(otherIdx);

            /* insert at the position we found */
            activeBanners_.insertAt(otherIdx, win);
            break;
        }
    }

    /*
     *   Skip all descendants of the window at the given index. 
     */
    skipDescendants(idx)
    {
        local parentID;

        /* index zero is the root item, so skip the entire list */
        if (idx == 0)
            return activeBanners_.length() + 1;
        
        /* note the parent item */
        parentID = activeBanners_[idx].getBannerID();

        /* skip the parent item */
        ++idx;

        /* keep going as long as we see children of the parent */
        while (idx < activeBanners_.length()
               && activeBanners_[idx].parentID_ == parentID)
        {
            /* this is a child, so skip it and all of its descendants */
            idx = skipDescendants(idx);
        }

        /* return the new index */
        return idx;
    }

    /* remove a banner from the active list */
    removeBanner(win)
    {
        local idx;
        local lastIdx;

        /* get the index of the item to remove */
        idx = activeBanners_.indexOf(win);

        /* if we didn't find it, ignore the request */
        if (idx == nil)
            return;

        /* find the index of its last descendant */
        lastIdx = skipDescendants(idx) - 1;

        /* 
         *   remove the item and all of its descendants - child items
         *   cannot be displayed once their parents are gone, so we can
         *   remove all of this item's children, all of their children,
         *   and so on, as they are becoming undisplayable 
         */
        activeBanners_.removeRange(idx, lastIdx);
    }

    /*
     *   The list of active banners.  This is a list of BannerWindow
     *   objects, stored in banner layout list order. 
     */
    activeBanners_ = static new Vector(32)

    /* receive RESTORE/UNDO notification */
    execute()
    {
        /* restore the display state for a non-initial state */
        restoreDisplayState(nil);
    }

    /*
     *   Restore the saved banner display state, so that the banner layout
     *   looks the same as it did when we saved the persistent state.  This
     *   should be called after restoring a saved state, undoing to a
     *   savepoint, or initializing (when first starting the game or when
     *   restarting).
     */
    restoreDisplayState(initing)
    {
        local uiVec;
        local uiIdx;
        local origActive;

        /* get the list of banners active in the UI */
        uiVec = bannerUITracker.activeBanners_;

        /*
         *   First, go through all of the persistent BannerWindow objects.
         *   For each one whose ID shows up in the active UI display list,
         *   tell the BannerWindow object its current UI handle.  
         */
        forEachInstance(BannerWindow, function(cur)
        {
            local uiCur;
            
            /* find this banner in the active UI list */
            uiCur = uiVec.valWhich({x: x.id_ == cur.getBannerID()});

            /* 
             *   if the window exists in the active UI list, note the
             *   current system handle for the window; otherwise, we have
             *   no system window, so set the handle to nil 
             */
            if (uiCur != nil)
            {
                /* note the current system banner handle */
                cur.handle_ = uiCur.handle_;

                /* re-establish the banner's active output stream */
                cur.outputStream_ = uiCur.outputStream_;

                /* tie the transient record to the current 'cur' */
                uiCur.win_ = cur;
            }
            else
            {
                /* it's not shown, so it has no system banner handle */
                cur.handle_ = nil;

                /* it has no output stream */
                cur.outputStream_ = nil;
            }
        });

        /* 
         *   
         *   'initing' indicates whether we're initializing (startup or
         *   RESTART) or doing something else (RESTORE, UNDO).  When
         *   initializing, if there are any banners on-screen, we'll give
         *   their associated BannerWindow objects (if any) a chance to set
         *   up their initial conditions; this allows us to avoid
         *   unnecessary redrawing if we have banners that we'd immediately
         *   set up to the same conditions anyway, since we can just keep
         *   the existing banners rather than removing and re-creating
         *   them.
         *   
         *   So, if we're initializing, tell each banner that it's time to
         *   set up its initial display.  
         */
        if (initing)
            forEachInstance(BannerWindow, {cur: cur.initBannerWindow()});

        /* 
         *   scan the active UI list, and close each window that isn't
         *   still open in the saved state 
         */
        foreach (local uiCur in uiVec)
        {
            /* if this window isn't in the active list, close it */
            if (activeBanners_.indexWhich(
                {x: x.getBannerID() == uiCur.id_}) == nil)
            {
                /*
                 *   There's no banner in the persistent list with this
                 *   ID, so this window is not part of the state we're
                 *   restoring.  Close the window.  If we have an
                 *   associated BannerWindow object, close through the
                 *   window object; otherwise, close the system handle
                 *   directly.  
                 */
                if (uiCur.win_ != nil)
                {
                    /* we have a BannerWindow - close it */
                    uiCur.win_.removeBanner();
                }
                else
                {
                    /* there's no BannerWindow - close the system window */
                    bannerDelete(uiCur.handle_);

                    /* remove the UI tracker object */
                    uiVec.removeElement(uiCur);
                }
            }
        }

        /* start at the first banner actually displayed right now */
        uiIdx = 1;

        /* 
         *   make a copy of the original active list - we might modify the
         *   actual active list in the course of restoring things, so make
         *   a copy that we can refer to as we reconstruct the original
         *   list 
         */
        origActive = activeBanners_.toList();

        /* 
         *   Scan the saved list of banners, and restore each one.  Note
         *   that by restoring windows in the order in which they appear
         *   in the list, we ensure that we always restore a parent before
         *   restoring any of its children, since a child always follows
         *   its parent in the list.  
         */
        for (local curIdx = 1, local aLen = origActive.length() ;
             curIdx <= aLen ; ++curIdx)
        {
            local redisp;
            local cur;

            /* get the current item */
            cur = origActive[curIdx];
                
            /* presume we will have to redisplay this banner */
            redisp = true;
            
            /*
             *   If this banner matches the current banner in the active
             *   UI display list, and the characteristics match, we need
             *   do nothing, as we're already displaying this banner
             *   properly.  If the current active UI banner doesn't match,
             *   then we need to insert this saved banner at the current
             *   active UI position. 
             */
            if (uiVec.length() >= uiIdx)
            {
                local uiCur;

                /* get this current UI display item (a BannerUIWindow) */
                uiCur = uiVec[uiIdx];

                /* check for a match to 'cur' */
                if (uiCur.id_ == cur.getBannerID()
                    && uiCur.parentID_ == cur.parentID_
                    && uiCur.windowType_ == cur.windowType_
                    && uiCur.align_ == cur.align_
                    && uiCur.styleFlags_ == cur.styleFlags_)
                {
                    /*
                     *   This saved banner ('cur') exactly matches the
                     *   active UI banner ('uiCur') at the same position
                     *   in the layout list.  Therefore, we do not need to
                     *   redisplay 'cur'.
                     */
                    redisp = nil;
                }
            }

            /* if we need to redisplay 'cur', do so */
            if (redisp)
            {
                local prvIdx;
                local where;
                local other;
                local parent;

                /*   
                 *   If 'cur' is already being displayed, we must remove
                 *   it before showing it anew.  This is the only way to
                 *   ensure that we display it with the proper
                 *   characteristics, since the characteristics of the
                 *   current instance of its window don't match up to what
                 *   we want to restore.  
                 */
                if (cur.handle_ != nil)
                    cur.removeBanner();

                /*
                 *   Figure out how to specify this window's display list
                 *   position.  A display list position is always
                 *   specified relative to the parent's child list, so
                 *   figure out where we go in our parent's list.  Scan
                 *   backwards in the active list for the nearest previous
                 *   window with the same parent.  If we find one, insert
                 *   the new window after that prior sibling; otherwise,
                 *   insert as the first child of our parent.  Presume
                 *   that we'll fail to find a prior sibling, then search
                 *   for it and search for our parent.  
                 */
                where = BannerFirst;
                other = nil;
                for (prvIdx = curIdx - 1 ; prvIdx > 0 ; --prvIdx)
                {
                    local prv;

                    /* note this item */
                    prv = origActive[prvIdx];
                    
                    /* 
                     *   If this item has our same parent, and we haven't
                     *   already found a prior sibling, this is our most
                     *   recent prior sibling, so note it.  
                     */
                    if (where == BannerFirst
                        && prv.parentID_ == cur.parentID_)
                    {
                        /* insert after this prior sibling */
                        where = BannerAfter;
                        other = prv;
                    }

                    /* if this is our parent, note it */
                    if (prv.getBannerID() == cur.parentID_)
                    {
                        /* 
                         *   note the parent BannerWindow object - we'll
                         *   need it to specify our window display
                         *   position 
                         */
                        parent = prv;

                        /* 
                         *   Children of a given parent always come after
                         *   the parent in the display list, so there's no
                         *   possibility of finding another sibling.
                         *   There's also obviously no possibility of
                         *   finding another parent.  So, our work here is
                         *   done; we can stop scanning.
                         */
                        break;
                    }
                }
                    
                /* show the window */
                cur.showForRestore(parent, where, other);
            }
            else
            {
                /* 
                 *   the banner is already showing, so we don't need to
                 *   redisplay it; simply notify it that a Restore
                 *   operation has taken place so that it can do any
                 *   necessary updates 
                 */
                cur.updateForRestore();
            }

            /*
             *   'cur' should now be displayed, but we might have failed
             *   to re-create it.  If we did show the window, we can
             *   advance to the next slot in the UI list, since this
             *   window will necessarily be at the current spot in the UI
             *   list.  
             */
            if (cur.handle_ != nil)
            {
                /* 
                 *   We know that this window is now the entry in the
                 *   active UI list at the current index we're looking at.
                 *   Move on to the next position in the active list for
                 *   the next saved window.  
                 */
                ++uiIdx;
            }
        }
    }
;

/*
 *   Initialization object - this will be called when we start the game the
 *   first time or RESTART within a session.  We'll restore the display
 *   state to the initial conditions. 
 */
bannerInit: InitObject
    execute()
    {
        /* restore banner displays to their initial conditions */
        bannerTracker.restoreDisplayState(true);
    }
;
