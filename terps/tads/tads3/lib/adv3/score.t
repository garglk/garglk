#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 by Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - scoring
 *   
 *   This module defines objects related to keeping track of the player's
 *   score, which indicates the player's progress through the game.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   An Achievement is an object used to award points in the score.  For
 *   most purposes, an achievement can be described simply by a string,
 *   but the Achievement object provides more flexibility in describing
 *   combined scores when a set of similar achievements are to be grouped.
 *   
 *   There are two ways to use the scoring system.
 *   
 *   1.  You can use a mix of string names and Achievement objects for
 *   scoring items; each time you award a scoring item, you call the
 *   function addToScore() to specify the achievement (by name or by
 *   Achievement object) and the number of points to award.  You can also
 *   call the method addToScoreOnce() on an Achievement object to award
 *   the scoring item, ensuring that the item is only awarded once in the
 *   entire game (saving you the trouble of checking to see if the event
 *   that triggered the scoring item has happened before already in the
 *   same game).  If you do this, you MUST set the property
 *   gameMain.maxScore to reflect the maximum score possible in the game.
 *   
 *   2.  You can use EXCLUSIVELY Achievement objects to represents scoring
 *   items, and give each Achievement object a 'points' property
 *   indicating the number of points it's worth.  To award a scoring item,
 *   you call the method awardPoints() on an Achievement object.  If you
 *   use this style of scoring, the library AUTOMATICALLY computes the
 *   gameMain.maxScore value, by adding up the 'points' values of all of
 *   the Achievement objects in the game.  For this to work properly, you
 *   have to obey the following rules:
 *   
 *.    - use ONLY Achievement objects (never strings) to award points;
 *.    - set the 'points' property of each Achievement to its score;
 *.    - define Achievement objects statically only (never use 'new' to
 *.      create an Achievement dynamically)
 *.    - if an Achievement can be awarded more than once, you must override
 *.      its 'maxPoints' property to reflect the total number of points it
 *.      will be worth when it is awarded the maximum number of times;
 *.    - always award an Achievement through its awardPoints() or
 *.      awardPointsOnce() method;
 *.    - there exists at least one solution of the game in which every
 *.      Achievement object is awarded
 */
class Achievement: object
    /*
     *   The number of points this Achievement scores individually.  By
     *   default, we set this to nil.  If you use the awardPoints() or
     *   awardPointsOnce() methods, you MUST set this to a non-nil value.
     *   
     *   If you set this to a non-nil value, the library will use it
     *   pre-compute the maximum possible score in the game, saving you the
     *   trouble of figuring out the maximum score by hand.  
     */
    points = nil

    /*
     *   The MAXIMUM number of points this Achievement can award.  This is
     *   by default just our 'points' value, on the assumption that the
     *   achievement is scored only once.  The library uses this value
     *   during pre-initialization to compute the maximum possible score in
     *   the game.  
     */
    maxPoints = (points)

    /* 
     *   Describe the achievement - this must display a string explaining
     *   the reason the points associated with this achievement were
     *   awarded.
     *   
     *   Note that this description can make use of the scoreCount
     *   information to show different descriptions depending on how many
     *   times the item has scored.  For example, an achievement for
     *   finding various treasure items might want to display "finding a
     *   treasure" if only one treasure was found and "finding five
     *   treasures" if five were found.
     *   
     *   In some cases, it might be desirable to keep track of additional
     *   custom information, and use that information in generating the
     *   description.  For example, the game might keep a list of
     *   treasures found with the achievement, adding to the list each
     *   time the achievement is scored, and displaying the contents of
     *   the list when the description is shown.
     */
    desc = ""

    /* show myself in a full-score listing */
    listFullScoreItem()
    {
        /* show the number of points I'm worth */
        gLibMessages.fullScoreItemPoints(totalPoints);

        /* show my description */
        desc;
    }

    /* 
     *   The number of times the achievement has been awarded.  Each time
     *   the achievement is passed to addToScore(), this is incremented.
     *   Note that this is distinct from the number of points.  
     */
    scoreCount = 0

    /* 
     *   the number of points awarded for the achievement; if this
     *   achievement has been accomplished multiple times, this reflects
     *   the aggregate number of points awarded for all of the times it
     *   has been accomplished 
     */
    totalPoints = 0

    /*
     *   Add this achievement to the score one time only, awarding the
     *   given number of points.  This can be used to score an achievement
     *   without separately tracking whether or not the achievement has
     *   been accomplished previously.  If the achievement has already been
     *   scored before, this will do nothing at all; otherwise, it'll score
     *   the achievement with the given number of points.  Returns true if
     *   we do award the points, nil if not (because we've awarded them
     *   before).  
     */
    addToScoreOnce(points)
    {
        /* if I've never been scored before, score me now */
        if (scoreCount == 0)
        {
            /* add the points to the score */
            addToScore(points, self);

            /* tell the caller we awarded the points as requested */
            return true;
        }
        else
        {
            /* tell the caller we didn't do anything */
            return nil;
        }
    }

    /*
     *   Award this Achievement's score, using the score value specified in
     *   my 'points' property.  
     */
    awardPoints()
    {
        /* add me to the score, using my 'points' property */
        addToScore(points, self);
    }

    /*
     *   Award this Achievement's score, but ensure that we're never
     *   awarded more than one time.  If this Achievement has already been
     *   awarded, this does nothing at all.  Returns true if we do award
     *   the points, nil if not (because we've awarded them before).  
     */
    awardPointsOnce()
    {
        /* award my 'points' value only if we haven't score before */
        return addToScoreOnce(points);
    }
;

/*
 *   Generic text achievement.  When we add an achievement to the full
 *   score list and the achievement is a simple string description, we'll
 *   create one of these to encapsulate the achievement.  
 */
class SimpleAchievement: Achievement
    /* create dynamically with a given string as our description */
    construct(str) { desc_ = str; }

    /* show my description */
    desc { say(desc_); }

    /* my description string */
    desc_ = ''
;

/*
 *   List interface for showing the full score list 
 */
fullScoreLister: Lister
    showListPrefixTall(itemCount, pov, parent)
    {
        /* showt he full score list intro message */
        gLibMessages.showFullScorePrefix;
    }

    /* every achievement is listed */
    isListed(obj) { return true; }

    /* each item counts as a singular object grammatically */
    listCardinality(obj) { return 1; }

    /* achievements have no containment hierarchy */
    getContents(obj) { return []; }
    getListedContents(obj, infoTab) { return []; }
    showContentsList(pov, obj, options, indent, infoTab) { }
    showInlineContentsList(pov, obj, options, indent, infoTab) { }

    /* show an item */
    showListItem(obj, options, pov, infoTab) { obj.listFullScoreItem(); }
;
    

/*
 *   Score notification daemon handler.  We'll receive a
 *   checkNotification() call each turn; we'll display a notification
 *   message each time the score has changed since the last time we ran.  
 */
scoreNotifier: PreinitObject
    /* the score as it was the last time we displayed a notification */
    lastScore = static (libScore.totalScore)

    /* we've never generated a notification about the score before */
    everNotified = nil

    /* daemon entrypoint */
    checkNotification()
    {
        /* 
         *   if the score has changed since the last time we checked,
         *   possibly generate a notification 
         */
        if (libScore.totalScore != lastScore)
        {
            /* only show a message if we're allowed to */
            if (libScore.scoreNotify.isOn)
            {
                local delta;
            
                /* calculate the change since the last notification */
                delta = libScore.totalScore - lastScore;
                
                /* 
                 *   generate the first or non-first notification, as
                 *   appropriate 
                 */
                if (everNotified)
                    gLibMessages.scoreChange(delta);
                else
                    gLibMessages.firstScoreChange(delta);
            
                /* 
                 *   note that we've ever generated a score change
                 *   notification, so that we don't generate the more
                 *   verbose first-time message on subsequent
                 *   notifications 
                 */
                everNotified = true;
            }

            /* 
             *   Remember the current score, so that we don't generate
             *   another notification until the score has changed again.
             *   Note that we note the new score even if we aren't
             *   displaying a message this time, because we don't want to
             *   generate a message upon re-enabling notifications.  
             */
            lastScore = libScore.totalScore;
        }
    }

    /* execute pre-initialization */
    execute()
    {
        /* initialize the score change notification daemon */
        new PromptDaemon(self, &checkNotification);
    }
;

/* 
 *   Add points to the total score.  This is a convenience function that
 *   simply calls libScore.addToScore_().
 */
addToScore(points, desc)
{
    /* simply call the libScore method to handle it */
    libScore.addToScore_(points, desc);
}

/*
 *   The main game score object.  
 */
libScore: PreinitObject
    /*
     *   Add to the score.  'points' is the number of points to add to the
     *   score, and 'desc' is a string describing the reason the points
     *   are being awarded, or an Achievement object describing the points.
     *   
     *   We keep a list of each unique achievement.  If 'desc' is already
     *   in this list, we'll simply add the given number of points to the
     *   existing entry for the same description.
     *   
     *   Note that, if 'desc' is an Achievement object, it will match a
     *   previous item only if it's exactly the same Achievement instance.
     */
    addToScore_(points, desc)
    {
        local idx;
        
        /* 
         *   if the description is a string, encapsulate it in a
         *   SimpleAchievement object 
         */
        if (dataType(desc) == TypeSString)
        {
            local newDesc;
            
            /* 
             *   look for an existing SimpleAchievement in our list with
             *   the same descriptive text - if we find one, reuse it,
             *   since this is another instance of the same group of
             *   achievements and thus can be combined into the same
             *   achievement object 
             */
            newDesc = fullScoreList.valWhich(
                { x: x.ofKind(SimpleAchievement) && x.desc_ == desc });

            /* 
             *   if we didn't find it, create a new simple achievement to
             *   wrap the descriptive text 
             */
            if (newDesc == nil)
                newDesc = new SimpleAchievement(desc);

            /* 
             *   for the rest of our processing, use the wrapper simple
             *   achievement object instead of the original text string 
             */
            desc = newDesc;
        }

        /* increase the use count for the achievement */
        desc.scoreCount++;
        
        /* add the points to the total */
        totalScore += points;
        
        /* try to find a match in our list of past achievements */
        idx = fullScoreList.indexOf(desc);
        
        /* if we didn't find it, add it to the list */
        if (idx == nil)
            fullScoreList.append(desc);

        /* 
         *   combine the points awarded this time into the total for this
         *   achievement 
         */
        desc.totalPoints += points;
    }

    /*
     *   Explicitly run the score notification daemon. 
     */
    runScoreNotifier()
    {
        /* explicitly run the notification */
        scoreNotifier.checkNotification();
    }

    /*
     *   Show the simple score 
     */
    showScore()
    {
        /* 
         *   Show the basic score statistics.  Use the appropriate form of
         *   the message, depending on whether or not there's a maximum
         *   score value. 
         */
        if (gameMain.maxScore != nil)
            gLibMessages.showScoreMessage(totalScore,
                                          gameMain.maxScore,
                                          libGlobal.totalTurns);
        else
            gLibMessages.showScoreNoMaxMessage(totalScore,
                                               libGlobal.totalTurns);

        /* show the score ranking */
        showScoreRank(totalScore);
    }

    /* 
     *   show the score rank message 
     */
    showScoreRank(points)
    {
        local idx;
        local tab = gameMain.scoreRankTable;
        
        /* if there's no rank table, skip the ranking */
        if (tab == nil)
            return;
        
        /*
         *   find the last item for which our score is at least the
         *   minimum - the table is in ascending order of minimum score,
         *   so we want the last item for which our score is sufficient 
         */
        idx = tab.lastIndexWhich({x: points >= x[1]});
    
        /* if we didn't find an item, use the first by default */
        if (idx == nil)
            idx = 1;
    
        /* show the description from the item we found */
        gLibMessages.showScoreRankMessage(tab[idx][2]);
    }

    /*
     *   Display the full score.  'explicit' is true if the player asked
     *   for the full score explicitly, as with a FULL SCORE command; if
     *   we're showing the full score automatically in the course of some
     *   other action, 'explicit' should be nil.  
     */
    showFullScore()
    {
        /* show the basic score statistics */
        showScore();
        
        /* list the achievements in 'tall' mode */
        fullScoreLister.showListAll(fullScoreList.toList(),
                                    ListTall, 0);
    }

    /*
     *   Vector for the full score achievement list.  This is a list of
     *   all of the Achievement objects awarded for accomplishments so
     *   far.  
     */
    fullScoreList = static new Vector(32)

    /* the total number of points scored so far */
    totalScore = 0

    /* 
     *   current score notification status - if on, we'll show a message at
     *   the end of each turn where the score changes, otherwise we won't
     *   mention anything 
     */
    scoreNotify = scoreNotifySettingsItem

    /*
     *   Compute the sum of the maximum point values of the Achievement
     *   objects in the game.  Point values are optional in Achievement
     *   objects; if there are no Achievement objects with non-nil point
     *   values, this will simply return nil.  
     */
    calcMaxScore()
    {
        local sum;
        local found;

        /* start with a running total of zero */
        sum = 0;

        /* we haven't found any non-nil point values yet */
        found = nil;

        /*
         *   Run through all of the Achievement objects to see if we can
         *   derive a maximum score for the game. 
         */
        forEachInstance(Achievement, function(obj) {
            local m;
            
            /*
             *   If this object has a non-nil maxPoints value, add it to
             *   the running total. 
             */
            if ((m = obj.maxPoints) != nil)
            {
                /* add this one to the sum */
                sum += m;

                /* note that we found one with a non-nil point value */
                found = true;
            }
        });

        /*
         *   If we found any Achievements with point values, return the sum
         *   of those point values; otherwise, return nil. 
         */
        return (found ? sum : nil);
    }

    /* execute pre-initialization */
    execute()
    {
        /* register as the global score handler */
        libGlobal.scoreObj = self;
    }
;

/* settings item for score notification mode */
scoreNotifySettingsItem: BinarySettingsItem
    /* the "factory setting" for NOTIFY is ON */
    isOn = true

    /* our configuration file variable ID */
    settingID = 'adv3.notify'

    /* show our description */
    settingDesc = (gLibMessages.shortNotifyStatus(isOn))
;
