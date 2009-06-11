#charset "us-ascii"

/* Copyright 2002 Michael J. Roberts */
/*
 *   This is an interactive test for the new persistent banner manager
 *   module, banner.t.  You can add this into any adv3-based game; we'll
 *   simply add a few new verbs to the game that let the player exercise
 *   banners interactively:
 *   
 *   addbanner left|right|top|bottom id - adds a new banner, aligned as
 *   requested and with the given ID.
 *   
 *   writebanner id <any text> - writes the given text to the banner.  The
 *   new text is appended to any text currently in the banner, followed by
 *   a newline.
 *   
 *   delbanner id - removes the banner with the given ID.  
 */

#include <adv3.h>
#include <en_us.h>

/*
 *   An object to track our interactively-created banners. 
 */
addedBannerTracker: object
    /* a table of our added banners, indexed by ID */
    bannerTab = static new LookupTable(16, 32)
;

DefineIAction(AddBanner)
    execAction()
    {
        /* add the banner at the top level (i.e., no banner parent) */
        doAddBanner(nil, id_, align_.bannerAlign, 10);
    }

    doAddBanner(parentID, id, align, pct)
    {
        local parent;
        
        /* make sure the name isn't already in use */
        if (addedBannerTracker.bannerTab[id] != nil)
        {
            "That banner ID is already in use. ";
            return;
        }

        /* create a new banner window object */
        local win = new BannerWindow('user:' + id);

        /* if a parent was specified, find it by ID */
        if (parentID != nil)
        {
            /* look up the parent by ID */
            parent = addedBannerTracker.bannerTab[parentID];

            /* make sure we found it */
            if (parent == nil)
            {
                "The specified parent doesn't exist. ";
                return;
            }
        }
        else
        {
            /* there's no parent */
            parent = nil;
        }

        /* show the banner */
        if (!win.showBanner(parent, BannerLast, nil, BannerTypeText,
                            align, pct, BannerSizePercent,
                            BannerStyleBorder | BannerStyleVScroll
                            | BannerStyleAutoVScroll))
        {
            "Unable to create new banner window. ";
            return;
        }

        /* write some initial text */
        win.writeToBanner('This is banner <q>' + id + '</q>\n');
        win.flushBanner();

        /* add it to the table of active banners */
        addedBannerTracker.bannerTab[id] = win;

        /* indicate success */
        "Banner created. ";
    }
;

DefineIAction(AddSubBanner)
    execAction()
    {
        /* add the banner with the given parent */
        AddBannerAction.doAddBanner(parid_, id_, align_.bannerAlign, 40);
    }
;

VerbRule(AddBanner)
    'addbanner' bannerAlignType->align_ tokWord->id_
     : AddBannerAction
    verbPhrase = 'addbanner/adding banner'
;

VerbRule(AddBannerEmpty) 'addbanner' : IAction
    execAction() { "usage: addbanner left|right|top|bottom <i>id</i> "; }
;

VerbRule(AddSubBanner)
    'addsub' tokWord->parid_ bannerAlignType->align_ tokWord->id_
     : AddSubBannerAction
     verbPhrase = 'addsub/adding sub-banner'
;

VerbRule(AddSubBannerEmpty) 'addsub' : IAction
    execAction() { "usage: addsub <i>parent_id</i> left|right|top|bottom
                    <i>id</i> "; }
;

grammar bannerAlignType(left): 'left' : BasicProd
    bannerAlign = BannerAlignLeft
;
grammar bannerAlignType(right): 'right' : BasicProd
    bannerAlign = BannerAlignRight
;
grammar bannerAlignType(top): 'top' : BasicProd
    bannerAlign = BannerAlignTop
;
grammar bannerAlignType(bottom): 'bottom' : BasicProd
    bannerAlign = BannerAlignBottom
;

DefineLiteralAction(WriteBanner)
    execAction()
    {
        local win;
        
        /* make sure the banner actually exists */
        win = addedBannerTracker.bannerTab[id_];
        if (win == nil)
        {
            "There is no such banner. ";
            return;
        }

        /* write the literal text to the banner */
        win.writeToBanner(getLiteral() + '\n');
        win.flushBanner();

        /* indicate success */
        "Done. ";
    }
;

VerbRule(WriteBanner) 'writebanner' tokWord->id_ singleLiteral
    : WriteBannerAction
    verbPhrase = 'write/writing to banner (what)'
;

DefineIAction(DelBanner)
    execAction()
    {
        local win;
        
        /* make sure the banner actually exists */
        win = addedBannerTracker.bannerTab[id_];
        if (win == nil)
        {
            "There is no such banner. ";
            return;
        }

        /* remove the banner window */
        win.removeBanner();

        /* stop tracking the banner window object */
        addedBannerTracker.bannerTab.removeElement(id_);

        /* indicate success */
        "Done. ";
    }
;

VerbRule(DelBanner) 'delbanner' tokWord->id_ : DelBannerAction
    verbPhrase = 'delbanner/deleting banner'
;
