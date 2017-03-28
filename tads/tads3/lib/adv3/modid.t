#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - module ID's
 *   
 *   This module defines a framework for "module ID's."  The module ID
 *   mechanism allows library modules to identify themselves.  The module
 *   ID framework is modular, in that several libraries can be linked
 *   together without any prior knowledge of one another, and the module ID
 *   framework will be able to find all of their ID's.  
 */

/* include the library header */
#include "adv3.h"

/* we also require file access */
#include <file.h>


/* ------------------------------------------------------------------------ */
/*
 *   Module ID.  Each library add-in can define one of these, so that the
 *   "credits" command and the like can automatically show the version of
 *   each library module included in the finished game, without the game's
 *   author having to compile a list of the module versions manually.
 *   
 *   An easy way to implement a CREDITS command is to define a ModuleID
 *   object for the game itself, and override the showCredit() method to
 *   display the text of the game's credits.  The module object for the
 *   game itself should usually have a listingOrder of 1, because the
 *   author usually will want the game's information to be displayed first
 *   in any listing that shows each included library module (such as the
 *   VERSION command's output).  
 */
class ModuleID: object
    /* my name */
    name = ''

    /* the "byline" for the module, in plain text and HTML versions */
    byline = ''
    htmlByline = ''

    /* my version number string */
    version = ''

    /*
     *   Show my library credit.  By default won't show anything.
     *   Libraries should generally not override this, because we want to
     *   leave it up to the author to determine how the credits are
     *   displayed.  If a library overrides this, then the author won't be
     *   able to control the formatting of the library credit, which is
     *   undesirable.  
     */
    showCredit() { }

    /*
     *   Show version information.  By default, we show our name and
     *   version number, then start a new line.  The main game's module ID
     *   should generally override this to show an appropriate version
     *   message for the game, and any library add-ins that want to
     *   display their version information can override this to do so.  
     */
    showVersion()
    {
        gLibMessages.showVersion(name, version);
        "\n";
    }

    /*
     *   Show the "about this game" information.  By default, we show
     *   nothing here.  Typically, only the game's module ID object will
     *   override this; in the game's module ID object, this method should
     *   display any desired background information about the game that
     *   the author wants the player to see on typing the ABOUT command.
     *   
     *   The ABOUT command conventionally displays information about the
     *   game and its author - the kind of thing you'd find in an author's
     *   notes section in a book - along with any special instructions to
     *   the player, such as notes on unusual command syntax.  Information
     *   that players will find especially helpful include:
     *   
     *   - A list of any unusual command phrasings that the game uses.
     *   Ideally, you will disclose here every verb that's required to
     *   complete the game, beyond the basic set common to most games
     *   (LOOK, INVENTORY, NORTH, SOUTH, TAKE, DROP, PUT IN, etc).  By
     *   disclosing every necessary verb and phrasing, you can be certain
     *   to avoid "guess the verb" puzzles.  (Note that it's possible to
     *   disclose every *required* verb without disclosing every
     *   *accepted* verb - some verbs might be so suggestive of a
     *   particular puzzle solution that you wouldn't want to disclose
     *   them, but as long as you disclose less suggestive alternatives
     *   that can be used to solve the same puzzles, you have a valid
     *   defense against accusations of using "guess the verb" puzzles.)
     *   
     *   - A quick overview of the NPC conversation system, if any.
     *   Conversation systems have been slowly evolving as authors
     *   experiment with different styles, and at least three or four
     *   different conventions have emerged.  The default that experienced
     *   players will expect is the traditional ASK/TELL system, so it's
     *   especially important to mention your system if you're using
     *   something else.
     *   
     *   - An indication of the "cruelty" level of the game.  In
     *   particular, many experienced players find it helpful to know from
     *   the outset how careful they have to be about saving positions
     *   throughout play, so it's helpful to point out whether or not it's
     *   possible for the player character to be killed; whether it's
     *   possible to get into situations where the game becomes
     *   "unwinnable"; and if the game can become unwinnable, whether or
     *   not this will become immediately clear.  The kindest games never
     *   kill the PC and are always winnable, no matter what actions the
     *   player takes; it's never necessary to save these games except to
     *   suspend a session for later resumption.  The cruelest games kill
     *   the PC without warning (although if they offer an UNDO command
     *   from a "death" prompt, then even this doesn't constitute true
     *   cruelty), and can become unwinnable in ways that aren't readily
     *   and immediately apparent to the player, which means that the
     *   player could proceed for quite some time (and thus invest
     *   substantial effort) after the game is already lost.
     *   
     *   - A description of any special status line displays or other
     *   on-screen information whose meaning might not be immediately
     *   apparent.  
     */
    showAbout() { }

    /* 
     *   My listing order.  When we compile a list of modules, we'll sort
     *   the modules first by ascending listing order; any modules with
     *   the same listing order will be sorted alphabetically by name with
     *   respect to the other modules with the same listing order.
     *   
     *   The value 1 is reserved for the game's own ID object.  Note that
     *   the TADS 3 library defines a module ID with listing order 50,
     *   which is chosen so that the main library credit will appear after
     *   the game credits but before any extension credits using the
     *   default order value 100 that we define here.  Extensions are
     *   free, however, to use a number lower than 5 if they wish to
     *   appear before the main library credit.  
     */
    listingOrder = 100

    /* 
     *   get a list of all of the modules that are part of the game,
     *   sorted in listing order 
     */
    getModuleList()
    {
        local lst;
        
        /* compile a list of all of the modules */
        lst = new Vector(10);
        forEachInstance(ModuleID, { obj: lst.append(obj) });
        lst = lst.toList();

        /* 
         *   sort the list by listing order (and alphabetically by name
         *   where listing orders are the same) 
         */
        lst = lst.sort(SortAsc, function(a, b) {

            /* if the listings order differ, sort by listing order */
            if (a.listingOrder != b.listingOrder)
                return a.listingOrder - b.listingOrder;

            /* the listing orders are the same; sort by name */
            if (a.name < b.name)
                return -1;
            else if (a.name > b.name)
                return 1;
            else
                return 0;
        });

        /* return the sorted list */
        return lst;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A module ID with metadata.  During pre-initialization, we'll
 *   automatically write out a file with the metadata for each of these
 *   objects.  This is an abstract base class; a subclass must be created
 *   for each specific metadata format.  
 */
class MetadataModuleID: ModuleID, PreinitObject
    /* execute pre-initialization */
    execute()
    {
        /* write out our metadata */
        writeMetadataFile();
    }

    /* 
     *   write our metadata file - this must be overridden by each
     *   subclass to carry out the specific steps needed to create and
     *   write the metadata file in the appropriate format for the
     *   subclass 
     */
    writeMetadataFile() { }
;

/*
 *   A module ID with GameInfo metadata.  The GameInfo metadata format is
 *   the standard TADS format for descriptive data about the game.  The
 *   usual way to use GameInfo metadata is to create a file called
 *   "gameinfo.txt" for a game, then embed this file directly in the
 *   game's .t3 file using the TADS 3 resource bundler (t3res).  Once the
 *   gameinfo.txt is embedded in the .t3 file, tools will be able to read
 *   the game's descriptive data directly from the .t3 file.  For example,
 *   HTML TADS on Windows can read the information into its Game Chest,
 *   which allows the interpreter to show the full name of the game, the
 *   author, and a blurb describing the game, among other things.
 */
class GameInfoModuleID: MetadataModuleID
    /* 
     *   The IFID - this is a UUID uniquely identifying the game, using the
     *   standard UUID format (xxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, where
     *   each 'x' is a hexadecimal digit).  You should pick an IFID when
     *   you start each game project, and then keep the same IFID
     *   throughout the game's entire existence, *including* version
     *   updates.  Each new version release of the same game - even major
     *   new versions - should use the same IFID, so that the versions can
     *   all be related to one another as the same game.
     *   
     *   If the game has multiple IFIDs, list them here, separated by
     *   commas.  You should NOT *intentionally* create multiple IFIDs for
     *   your game; once you've created an IFID, it should be the unique
     *   and permanent identifier for the game.  In particular, do NOT
     *   create a new IFID for a new version: the whole series of releases
     *   throughout a game's lifetime should be identified by a single
     *   IFID, so that archivists will know that the versions are all
     *   incarnations of the same work.
     *   
     *   The reason that multiple IFIDs are allowed at all is that many
     *   older games were not assigned explicit UUID-style IFIDs when
     *   released.  In such cases, the game has an "implied" IFID based on
     *   an MD5 hash of the compiled game file's contents.  Every release
     *   that doesn't contain an explicit IFID will therefore have a
     *   different implied IFID.  So, for example, if you've already
     *   released versions 1, 2, and 3 of your game, and you didn't assign
     *   explicit IFID values to those releases, each version will have a
     *   different implied IFID.  When you release version 4, you should
     *   NOT assign a new UUID-style IFID.  Instead, in the IFID string
     *   here, list ALL THREE of the implied IFIDs from the past releases.
     *   Each of the three IFIDs counts from now on as an IFID for the
     *   work, for all versions collectively.  (By placing the list of
     *   IFIDs in version 4, you prevent version 4 from adding yet another
     *   implied IFID of its own: the explicit IFID list supersedes the
     *   implied IFID.)  See the Babel spec for more information, and for
     *   instructions on how to calculate the implied IFID for a TADS game
     *   that was released without a UUID-style IFID.  
     */
    IFID = ''

    /*
     *   The game's headline.  It's become an IF tradition to use a
     *   quasi-subtitle of the sort that Infocom used, of the form "An
     *   Interactive Mystery."  This can be used to define that subtitle.  
     */
    headline = ''

    /*
     *   If this game is part of a series, such as a trilogy, these can be
     *   used to identify the name of the series and the position in the
     *   series.  The series name should be something like "The Enchanter
     *   Trilogy"; the series number, if provided, should be a simple
     *   integer string ('1', '2', etc) giving the position in the series.
     *   Note that the series number isn't required even if a series name
     *   is specified, since some series are just groups of works without
     *   any particular ordering. 
     */
    seriesName = ''
    seriesNumber = ''

    /*
     *   The genre of the game.  Some games don't fit any particular genre,
     *   and some authors just don't like the idea of having to pigeonhole
     *   their games, so feel free to leave it out.  If there's a good fit
     *   to a well-established genre, though, you can specify it here.  We
     *   recommend you keep this short - one word, maybe two - and use a
     *   genre name that's generally recognized as such.  You might want to
     *   use Baf's Guide as a reference (http://www.wurb.com/if/genre).  
     */
    genreName = ''

    /*
     *   The forgiveness level, according to the Zarfian scale propounded
     *   by Andrew Plotkin on rec.arts.int-fiction.  This must be one of
     *   these terms, using the exact capitalization shown: Merciful,
     *   Polite, Tough, Nasty, Cruel.
     */
    forgivenessLevel = ''
    
    /* 
     *   The names and email addresses of the authors, in GameInfo format.
     *   This list must use the following format:
     *   
     *   author one <email>; author two <email> <email>; ...
     *   
     *   In other words, list the first author's name, followed by one or
     *   more email addresses, in angle brackets, for the first author.
     *   If more than one author is to be listed, add a semicolon,
     *   followed by the name of the second author, followed by the second
     *   author's email address or addresses, enclosing each in angle
     *   brackets.  Repeat as needed for additional authors.  The list
     *   does not need to end with a semicolon; semicolons are merely used
     *   to separate entries.  
     */
    authorEmail = ''

    /*
     *   The game's web site, if any.  If specified, this must be an
     *   absolute URL with http protocol - that is, it must be of the form
     *   "http://mydomain.com/...".  
     */
    gameUrl = ''

    /*
     *   Descriptive text for the game, in plain text format.  This is a
     *   short description that can be used, for example, in a catalog of
     *   games.  This should be a couple of sentences or so.
     */
    desc = ''

    /*
     *   Descriptive text for the game, as an HTML fragment.  This should
     *   have the same information as the 'desc', but this version can use
     *   HTML markups (including tags and character entities) to embellish
     *   the display of the text.  Any HTML markups should be "in-line"
     *   body elements only, not "block" or head elements, so that this
     *   text can be inserted into a larger HTML document.  For example,
     *   markups like <i> and <b> are fine, but <p> and <table> should not
     *   be used.  
     */
    htmlDesc = ''

    /*
     *   The release date.  By default, we compute this statically to be
     *   today's date.  This means this will be set to the date of
     *   compilation.  If the game wishes to override this, note that the
     *   GameInfo format requires this to be of the form YYYY-MM-DD.  For
     *   example, December 9, 2001 would be '2001-12-09'.  
     */
    releaseDate = static getGameInfoToday()

    /*
     *   The date of first publication.  This can be just a year in YYYY
     *   format, or a full YYYY-MM-DD date.  This is the original release
     *   date of the original version of the game, which is often of
     *   interest to archivists.  This should *not* be updated when a new
     *   release is made - it's always the date of *original* publication.
     */
    firstPublished = ''

    /*
     *   The language in which this game's text is written.  This is the
     *   RFC3066 language code for the main language of the work.  For
     *   example, games written in US English would use 'en-US', while
     *   games written in British English would use 'en-GB'.  Note that
     *   each language-specific library module should use 'modify' to set
     *   this to the default for the library.  
     */
    languageCode = ''

    /*
     *   The license type for this game.  Most text IF games these days
     *   are released as freeware, so we use this as the default.  The
     *   GameInfo metadata format defines several other standard license
     *   types, including Public Domain, Shareware, Commercial Demo,
     *   Commercial, and Other.  Authors should change this if they plan
     *   to release under a licensing model other than freeware.
     *   
     *   Note that the GameInfo metadata format documentation explicitly
     *   states that the license type indicated here is advisory only and
     *   cannot be considered definitive.  This means that this setting
     *   does not take away any of the author's rights to set specific
     *   license terms.  Even so, we recommend that you pick an
     *   appropriate value here to avoid any confusion.  
     */
    licenseType = 'Freeware'

    /*
     *   The copying rules for this game.  Most text games these days are
     *   released as freeware with minimal restrictions on copying, so we
     *   use a default of "nominal cost only." Other values defined in the
     *   GameInfo format include Prohibited, No Restrictions, No-Cost Only,
     *   At-Cost Only, and Other.  A modifier indicates whether or not the
     *   game may be included in compilations (such as those "10,001 great
     *   games" CD-R's that people like to sell on auction sites); we
     *   indicate that inclusion in compilations is allowed by default.
     *   You can change this to "Compilations Prohibited" if you prefer not
     *   to allow your game to be distributed in that fashion.
     *   
     *   Note that the restrictions specified here aren't enforced by any
     *   sort of copy-protection or DRM (digital rights management)
     *   technology.  This information is entirely for the benefit of
     *   conscientious users who want to abide by your wishes and thus need
     *   to know what your wishes are.
     *   
     *   The GameInfo bundle is mostly for the benefit of software that can
     *   extract the information from the compiled game.  So, we recommend
     *   that you also put a full notice and explanation of your license
     *   restrictions somewhere that users can easily find it, such as in a
     *   separate LICENSE.TXT file that you distribute with your game, or
     *   in the text of the game itself (displayed by a LICENSE or
     *   COPYRIGHT command, for example).  
     */
    copyingRules = 'Nominal cost only; compilations allowed'

    /*
     *   The recommended "presentation profile" for the game.  'Default'
     *   means that the interpreter's default profile should be used.
     *   (Some interpreters let the user select which profile to use as the
     *   default, in which case 'Default' means we'll use that profile.)  
     */
    presentationProfile = 'Default'

    /* write our metadata file */
    writeMetadataFile()
    {
        local f;

        /* 
         *   open the file - note that the GameInfo.txt resource is
         *   required to be encoded in UTF-8, so open the file with the
         *   UTF-8 character set 
         */
        f = File.openTextFile(gameInfoFilename, FileAccessWrite, 'utf-8');
    
        /* scan our list of metadata keys and write each one */
        for (local i = 1, local len = metadataKeys.length() ;
             i + 1 <= len ; i += 2)
        {
            local key;
            local prop;
            local val;
            
            /* get the key name and value property for this entry */
            key = metadataKeys[i];
            prop = metadataKeys[i+1];
            val = self.(prop);

            /* turn any '\ ' characters into ' ' characters */
            val = val.findReplace('\ ', ' ', ReplaceAll);
            
            /* write out this key if there's a value defined for it */
            if (val != nil && val != '')
                f.writeFile(key + ': ' + val + '\n');
        }

        /* done with the file - close it */
        f.closeFile();

        /* remember the primary IFID in the globals */
        libGlobal.IFID = rexReplace([',.*$', '<space>+'], IFID, '');
    }

    /* 
     *   the GameInfo filename - by default, we write the standard
     *   gameinfo.txt file 
     */
    gameInfoFilename = 'GameInfo.txt'

    /*
     *   The metadata key mappings.  This is a list of key/property pairs.
     *   The key in each pair is a string giving a standard GameInfo key
     *   name, and the property gives the property (of self) that we
     *   evaluate to get the string value for that key. 
     */
    metadataKeys =
    [
        'IFID', &IFID,
        'Name', &name,
        'Headline', &headline,
        'Byline', &byline,
        'HtmlByline', &htmlByline,
        'AuthorEmail', &authorEmail,
        'Url', &gameUrl,
        'Desc', &desc,
        'HtmlDesc', &htmlDesc,
        'Version', &version,
        'ReleaseDate', &releaseDate,
        'FirstPublished', &firstPublished,
        'Series', &seriesName,
        'SeriesNumber', &seriesNumber,
        'Genre', &genreName,
        'Forgiveness', &forgivenessLevel,
        'Language', &languageCode,
        'LicenseType', &licenseType,
        'CopyingRules', &copyingRules,
        'PresentationProfile', &presentationProfile
    ]

    /* 
     *   get today's date, using the GameInfo standard date format
     *   (YYYY-MM-DD) 
     */
    getGameInfoToday()
    {
        local dt;
        local mm, dd;

        /* get the current date */
        dt = getTime(GetTimeDateAndTime);
        
        /* get the month, and add a leading zero if it's only one digit */
        mm = (dt[2] < 10 ? '0' : '') + dt[2];
        
        /* get the day, and add a leading zero if it's only one digit */
        dd = (dt[3] < 10 ? '0' : '') + dt[3];
        
        /* build and return the full YYYY-MM-DD date string */
        return toString(dt[1]) + '-' + mm + '-' + dd;
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Base class for the game's module ID.  This merely sets the listing
 *   order to 1 so that the game's credit is listed first.  Normally,
 *   exactly one GameID object, called 'versionInfo', is defined in a game,
 *   to provide the game's identifying information.  
 *   
 *   Note that this class is based on GameInfoModuleID, so the library will
 *   automatically write out a gameinfo.txt file based on this object's
 *   settings.  For full GameInfo data, the game should minimally define the
 *   following properties (see GameInfoModuleID and ModuleID for details on
 *   these properties):
 *
 *.  IFID - a random 32-digit hex number to uniquely identify the game;
 *.    you can generate one at http://www.tads.org/ifidgen/ifidgen
 *.  name - the name of the game
 *.  byline - the main author credit: "by so and so"
 *.  htmlByline - the main author credit as an HTML fragment
 *.  authorEmail - the authors' names and email addresses (in GameInfo format)
 *.  desc - a short blurb describing the game, in plain text format
 *.  htmlDesc - the descriptive blurb as an HTML ragment
 *.  version - the game's version string
 *   
 *   In addition, you can override the following settings if you don't like
 *   the defaults inherited from GameInfoModuleID:
 *   
 *.  releaseDate - the release date string (YYYY-MM-DD)
 *.  licenseType - freeware, shareware, etc.
 *.  copyingRules - summary rules on copying
 *.  presentationProfile - Multimedia, Plain Text
 */
class GameID: GameInfoModuleID
    /* always list the game's credits before any library credits */
    listingOrder = 1

    /*
     *   Show the game's credits.  By default, we'll just show our name and
     *   by-line.
     *   
     *   Typically, authors will want to override this to display the full
     *   credits for the game.  Most authors like to show the author or
     *   authors, along with notes of thanks to important contributors.
     *   
     *   Note that libraries generally will not show anything automatically
     *   in the credits, to allow the author full control over the
     *   formatting of the credits.  Authors are encouraged to give credit
     *   where it's due for any libraries they use.  
     */
    showCredit()
    {
        /* by default, just show the game's name and by-line */
        gLibMessages.showCredit(name, htmlByline);
    }

    /* 
     *   show a blank line after the game's version information, to make
     *   it stand apart from the list of library and VM version numbers 
     */
    showVersion()
    {
        inherited();
        "\b";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   The main TADS 3 library ID.
 */
moduleAdv3: ModuleID
    name = 'TADS 3 Library'
    byline = 'by Michael J.\ Roberts'
    htmlByline = 'by <a href="mailto:mjr_@hotmail.com">'
                 + 'Michael J.\ Roberts</a>'
    version = '3.1.3'

    /*
     *   We use a listing order of 50 so that, if all of the other credits
     *   use the defaults, we appear after the game's own credits
     *   (conventionally at listing order 1) and before any extension
     *   credits (which inherit the default order 100), but so that
     *   there's room for extensions that want to appear before us, or
     *   after us but before any default-ordered extensions.  
     */
    listingOrder = 50
;


/* ------------------------------------------------------------------------ */
/*
 *   An ID module not for the library but for the T3 VM itself.  This
 *   doesn't display any credit information, but displays version number
 *   information for the VM so that the "version" command shows what
 *   version of the interpreter is in use.  
 */
ModuleID
    showVersion()
    {
        local vsn = t3GetVMVsn();

        /* 
         *   show the version information - note that we must decompose
         *   the version number into the standard 3-part dotted string 
         */
        gLibMessages.showVersion('T3 VM (' + t3GetVMID() + ')',
                                 '' + (vsn >> 16) + '.'
                                 + ((vsn >> 8) & 0xFF) + '.'
                                 + (vsn & 0xFF));
        "\n";
    }

    /*
     *   Use a very high listing order so that we're the last thing shown.
     */
    listingOrder = 10000
;

