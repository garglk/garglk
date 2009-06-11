#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - footnotes
 *   
 *   This module defines objects related to footnotes.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Footnote - this allows footnote references to be generated in
 *   displayed text, and the user to retrieve the contents of the footnote
 *   on demand.
 *   
 *   Create an instance of Footnote for each footnote.  For each footnote
 *   object, define the "desc" property as a double-quoted string (or
 *   method) displaying the footnote's contents.
 *   
 *   To display a footnote reference in a passage of text, call
 *   <<x.noteRef>>, where x is the footnote object in question.  That's all
 *   you have to do - we'll automatically assign the footnote a sequential
 *   number (so that footnote references are always seen by the player in
 *   ascending order, regardless of the order in which the player
 *   encounters the sources of the footnotes), and the NOTE command will
 *   automatically figure out which footnote object is involved for a given
 *   footnote number.
 *   
 *   This class also serves as a daemon notification object to receive
 *   per-command daemon calls.  The first time we show a footnote
 *   reference, we'll show an explanation of how footnotes work.  
 */
class Footnote: object
    /* 
     *   Display the contents of the footnote - this will be called when
     *   the user asks to show the footnote with the "NOTE n" command.
     *   Each instance must provide suitable text here.  
     */
    desc = ""

    /*
     *   Get a reference to the footnote for use in a passage of text.
     *   This returns a single-quoted string to display as a reference to
     *   the footnote.  
     */
    noteRef
    {
        /* 
         *   If the sensory context is blocking output, do not consider
         *   this a reference to the footnote at all, since the player
         *   won't see it.  
         */
        if (senseContext.isBlocking)
            return '';
        
        /* 
         *   if we haven't already assigned a number to this footnote,
         *   assign one now 
         */
        if (footnoteNum == nil)
        {
            /* 
             *   Allocate a new footnote number and remember it as our
             *   own.  Note that we want the last footnote number for all
             *   footnotes, so use the Footnote class property
             *   lastFootnote. 
             */
            footnoteNum = ++(Footnote.lastFootnote);

            /* 
             *   add myself to the class's list of numbered notes, so we
             *   can find this footnote easily again given its number 
             */
            Footnote.numberedFootnotes.append(self);

            /* note that we've generated a footnote reference */
            Footnote.everShownFootnote = true;
        }

        /* 
         *   If we're allowed to show footnotes, return the library
         *   message text to display given the note number.  If all
         *   footnotes are being hidden, or if we're only showing new
         *   footnotes and we've already read this one, return an empty
         *   string.  
         */
        switch(footnoteSettings.showFootnotes)
        {
        case FootnotesFull:
            /* we're showing all footnotes unconditionally */
            return gLibMessages.footnoteRef(footnoteNum);

        case FootnotesMedium:
            /* we're only showing unread footnotes */
            return footnoteRead ? '' : gLibMessages.footnoteRef(footnoteNum);

        case FootnotesOff:
            /* we're hiding all footnotes unconditionally */
            return '';
        }

        /* 
         *   in case the status is invalid and we fall through, return an
         *   empty string as a last resort
         */
        return '';
    }

    /*
     *   Display a footnote given its number.  If there is no such
     *   footnote, we'll display an error message saying so.  (This is a
     *   class method, so it should be called directly on Footnote, not on
     *   instances of Footnote.)  
     */
    showFootnote(num)
    {
        /* 
         *   if there's a footnote for this number, display it; otherwise,
         *   display an error explaining that the footnote number is
         *   invalid 
         */
        if (num >= 1 && num <= lastFootnote)
        {
            local fn;

            /* 
             *   it's a valid footnote number - get the footnote object
             *   from our vector of footnotes, simply using the footnote
             *   number as an index into the vector
             */
            fn = numberedFootnotes[num];

            /* show its description by calling 'desc' method */
            fn.desc;

            /* note that this footnote text has been read */
            fn.footnoteRead = true;
        }
        else
        {
            /* there is no such footnote */
            gLibMessages.noSuchFootnote(num);
        }
    }

    /* SettingsItem tracking our current status */
    footnoteSettings = footnoteSettingsItem

    /* 
     *   my footnote number - this is assigned the first time I'm
     *   referenced; initially we have no number, since we don't want to
     *   assign a number until the note is first referenced 
     */
    footnoteNum = nil

    /* 
     *   Flag: this footnote's full text has been displayed.  This refers
     *   to the text of the footnote itself, not the reference, so this is
     *   only set when the "FOOTNOTE n" command is used to read this
     *   footnote.  
     */
    footnoteRead = nil

    /*
     *   Static property: the highest footnote number currently in use.
     *   We start this at zero, because zero is never a valid footnote
     *   number.  
     */
    lastFootnote = 0

    /*
     *   Static property: a vector of all footnotes which have had numbers
     *   assigned.  We use this to find a footnote object given its note
     *   number.  
     */
    numberedFootnotes = static new Vector(20)

    /* static property: we've never shown a footnote reference before */
    everShownFootnote = nil

    /* static property: per-command-prompt daemon entrypoint */
    checkNotification()
    {
        /*
         *   If we've ever shown a footnote, show the footnote
         *   notification now.  Note that we know we've never shown a
         *   notification before simply because we're still running - we
         *   remove this daemon as soon as it shows its notification.  
         */
        if (everShownFootnote)
        {
            /* show the first footnote notification */
            gLibMessages.firstFootnote();

            /* 
             *   We only want to show this notification once in the whole
             *   game, so we can cancel this daemon now.  Since we're the
             *   event that's running, we can just tell the event manager
             *   to remove the current event from receiving further
             *   notifications.  
             */
            eventManager.removeCurrentEvent();
        }
    }
;

/* our FOOTNOTES settings item */
footnoteSettingsItem: SettingsItem
    /* our current status - the factory default is "medium" */
    showFootnotes = FootnotesMedium

    /* our config file variable ID */
    settingID = 'adv3.footnotes'
    
    /* show our short description */
    settingDesc = (gLibMessages.shortFootnoteStatus(showFootnotes))

    /* get the setting's external file string representation */
    settingToText()
    {
        switch(showFootnotes)
        {
        case FootnotesMedium:
            return 'medium';
            
        case FootnotesFull:
            return 'full';
            
        default:
            return 'off';
        }
    }

    settingFromText(str)
    {
        /* convert to lower-case and strip off spaces */
        if (rexMatch('<space>*(<alpha>+)', str.toLower()) != nil)
            str = rexGroup(1)[3];
        
        /* check the keyword */
        switch (str)
        {
        case 'off':
            showFootnotes = FootnotesOff;
            break;
            
        case 'medium':
            showFootnotes = FootnotesMedium;
            break;
            
        case 'full':
            showFootnotes = FootnotesFull;
            break;
        }
    }
;

/* pre-initialization - set up the footnote explanation daemon */
PreinitObject
    execute()
    {
        /* since we're available, register as the global footnote handler */
        libGlobal.footnoteClass = Footnote;

        /* initialize the footnote notification daemon */
        new PromptDaemon(Footnote, &checkNotification);
    }
;

