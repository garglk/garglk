#charset "us-ascii"

/*
 *   TADS 3 Tips, by Krister Fundin (fundin@yahoo.com). Provides a uniform
 *   way of providing one-time only tips to the player (and especially to
 *   inexperienced players) when certain things happen in the game.  
 */

#include <adv3.h>

/* ---------------------------------------------------------------------- */
/*
 *   The tip manager keeps track of which tips we have shown. Since we don't
 *   want to unnecessarily show any tips more than once, we store this
 *   information both transiently (in the tipManager) and persistently (in
 *   the tip objects themselves). This should make sure that we at least
 *   cover these two types of cases:
 *
 *   - The player sees a tip, then restarts, undos or restores to an earlier
 *     position.
 *   - The player sees a tip, saves, then resumes play at some later time.
 */
transient tipManager: InitObject, PostRestoreObject, PostUndoObject
    /*
     *   Show pending tips. This is called by a PromptDaemon before each new
     *   round of input.
     */
    showTips()
    {
        /*
         *   Go through our vector of pending tips. Use a 'for' loop instead
         *   a 'foreach' loop, in case showing one tip triggers another one.
         */
        for (local i = 1 ; i <= pendingTips.length ; i++)
        {
            /* show the description of the current tip */
            pendingTips[i].showTipDesc();
        }

        /* clear the vector of pending tips */
        pendingTips.setLength(0);
    }

    /* update tip information after a restore, restart or undo */
    execute()
    {
        /* go through all tips */
        forEachInstance(Tip, function(tip)
        {
            /*
             *   see if this one has been shown, according to its own
             *   persistent memory
             */
            if (tip.shown)
            {
                /*
                 *   It says that it has been shown. If it's not in our list
                 *   of shown tips, then add it.
                 */
                if (shownTips.indexOf(tip) == nil)
                    shownTips += tip;
            }
            else
            {
                /*
                 *   It says that it hasn't been shown. If it's in our list
                 *   of shown tips, then it must have been shown after all.
                 */
                if (shownTips.indexOf(tip) != nil)
                    tip.shown = true;
            }
        });
    }

    /* a vector of tips to be displayed before the next prompt */
    pendingTips = static new Vector(2)

    /*
     *   A transient list of shown tips. Note that this must be a list and
     *   not a vector. When updating a list, we actually replace it with a
     *   new list, since lists are immutable. This is a transient change -
     *   it affects only the value of the shownTips property. Updating a
     *   vector, however, modifies the vector itself and leaves the property
     *   with the same reference. A vector itself is always persistent, so
     *   this change would be lost after E.G. a restore.
     */
    shownTips = []
;

/*
 *   The Tip class. Each actual tip should be represented by an instance of
 *   this class. To show the tip, just call tipName.showTip(). If the tip
 *   has already been shown, or if the tips have been turned off completely,
 *   then nothing will be displayed.
 */
class Tip: object
    /*
     *   The actual text to display when this tip is shown. We'll wrap it in
     *   <.tip> tags automatically, and also add a paragraph break before
     *   it.
     */
    desc = ""

    /* show this tip */
    showTip()
    {
        /* see if we should be shown */
        if (!shouldShowTip)
            return;

        /*
         *   defer the actual displaying of the tip until just before the
         *   next command prompt
         */
        tipManager.pendingTips.append(self);

        /* note that we have been shown */
        makeShown();
    }

    /* display our tip description, I.E. the actual tip */
    showTipDesc()
    {
        /* display a pargraph break and an opening <.tip> style tag */
        "<.p><.tip>";

        /* show our description */
        desc();

        /* close the <.tip> tag */
        "<./tip>";
    }

    /* should we show this tip when asked to? */
    shouldShowTip()
    {
        /*
         *   We'll show it as long as it hasn't been shown before and we
         *   haven't turned the tips off completely. Certain tips might want
         *   to be displayed even when all tips are turned off, if they
         *   contain information specific to a certain story or an extension
         *   that it uses. If so, then this method could be overridden.
         */
        return (!shown && tipMode.isOn);
    }

    /*
     *   Mark this tip as shown. This method can be called by outside code
     *   before the tip has been triggered. If the tip informs the player of
     *   a certain command, for instance, then it would become redundant if
     *   the player has already used that command.
     */
    makeShown()
    {
        /* set our shown flag */
        shown = true;

        /* also add us to the transient list of shown tips */
        tipManager.shownTips += self;
    }

    /* flag: has this tip been shown before? */
    shown = nil
;

/*
 *   A style tag that we enclose tips with. By default, we just use plain
 *   parentheses, just like for notifications and parser messages, but this
 *   could be overridden if we wanted to display something fancier.
 */
tipStyleTag: StyleTag 'tip'
    openText = '('
    closeText = ')'
;

/*
 *   During pre-init, create a PromptDaemon for displaying tips. We don't
 *   want to display them directly when the showTip() method is called, to
 *   allow tips to be triggered from pretty much anywhere without having to
 *   worry about them showing up in the middle of some text.
 */
PreinitObject
    execute()
    {
        /*
         *   Give this daemon a higher-than-average eventOrder, in case
         *   another PromptDaemon wants to display a tip. The standard tip
         *   about turning of score notification is triggered this way.
         */
        new PromptDaemon(tipManager, &showTips).eventOrder = 200;
    }
;

/* ---------------------------------------------------------------------- */
/*
 *   Next, we want to allow turning all tips on and off during the game. It
 *   should also be possible to turn the tips off for ALL games that use
 *   them, and thus we define a SettingsItem for this purpose. This means
 *   that the player can turn the tips off and then save this setting as the
 *   default.
 */
tipMode: BinarySettingsItem
    /* we show tips by default */
    isOn = true

    /* the ID string to use in the configuration file */
    settingID = 'tips.showtips'

    /* show our description */
    settingDesc = (libMessages.tipStatusShort(isOn))
;

/*
 *   Define a system action for turning tip mode on/off.
 */
DefineSystemAction(TipMode)
    execSystemAction()
    {
        /* set the new tip mode */
        tipMode.isOn = stat_;

        /* acknowledge the change */
        libMessages.acknowledgeTipStatus(stat_);
    }
;

