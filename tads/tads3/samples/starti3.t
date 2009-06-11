#charset "us-ascii"

/*
 *   Copyright (c) 1999, 2002 by Michael J. Roberts.  Permission is
 *   granted to anyone to copy and use this file for any purpose.  
 *   
 *   This is a starter TADS 3 source file.  This is a complete TADS game
 *   that you can compile and run.
 *   
 *   To compile this game in TADS Workbench, open the "Build" menu and
 *   select "Compile for Debugging."  To run the game, after compiling it,
 *   open the "Debug" menu and select "Go."
 *   
 *   Please note that this file contains considerably more than the
 *   minimal set of definitions necessary to create a working game; this
 *   file has numerous examples meant to help you start making progress on
 *   your game more quickly, by giving you a few concrete examples that
 *   you can copy and modify.  As you flesh out your game, you should
 *   modify the objects we define here, or simply remove them when you no
 *   longer need them in your game.
 *   
 *   If you want a truly minimal set of definitions, create another new
 *   game in TADS Workbench, and choose the "advanced" version when asked
 *   for the type of starter game to create.  
 */

/* 
 *   Include the main header for the standard TADS 3 adventure library.
 *   Note that this does NOT include the entire source code for the
 *   library; this merely includes some definitions for our use here.  The
 *   main library must be "linked" into the finished program by including
 *   the file "adv3.tl" in the list of modules specified when compiling.
 *   In TADS Workbench, simply include adv3.tl in the "Source Files"
 *   section of the project.
 *   
 *   Also include the US English definitions, since this game is written
 *   in English.  
 */
#include <adv3.h>
#include <en_us.h>

/*
 *   Our game credits and version information.  This object isn't required
 *   by the system, but our GameInfo initialization above needs this for
 *   some of its information.
 *   
 *   IMPORTANT - You should customize some of the text below, as marked:
 *   the name of your game, your byline, and so on.  
 */
versionInfo: GameID
    IFID = '$IFID$'
    name = '$TITLE$'
    byline = 'by $AUTHOR$'
    htmlByline = 'by <a href="mailto:$EMAIL$">
                  $AUTHOR$</a>'
    version = '1'
    authorEmail = '$AUTHOR$ <$EMAIL$>'
    desc = '$DESC$'
    htmlDesc = '$HTMLDESC$'

    /* 
     *   other bibliographic tags you might want to set include:
     *
     *.    headline = 'An Interactive Sample'
     *.    seriesName = 'The Sample Trilogy'
     *.    seriesNumber = '1'
     *.    genreName = 'Sample Games'
     *.    forgivenessLevel = 'Polite'
     *.    gameUrl = 'http://mysite.com/mygame.htm'
     *.    firstPublished = '2006'
     *.    languageCode = 'en-US'
     *.    licenseType = 'Freeware'
     *.    copyingRules = 'Nominal cost only; compilations allowed'
     *.    presentationProfile = 'Default'
     */

    showCredit()
    {
        /* show our credits */
        "Put credits for the game here. ";

        /* 
         *   The game credits are displayed first, but the library will
         *   display additional credits for library modules.  It's a good
         *   idea to show a blank line after the game credits to separate
         *   them visually from the (usually one-liner) library credits
         *   that follow.  
         */
        "\b";
    }
    showAbout()
    {
        "Put information for players here.  Many authors like to mention
        any unusual commands here, along with background information on
        the game (for example, the author might mention that the game
        was created as an entry for a particular competition). ";
    }
;

/* 
 *   Entryway location.
 *   
 *   We use the class "Room" to define the location.  Room is a class,
 *   defined in the library, that can be used for most of the locations in
 *   the game.
 *   
 *   Our definition defines two strings.  The first string, which must be
 *   in single quotes, is the "name" of the room; the name is displayed on
 *   the status line and each time the player enters the room.  The second
 *   string, which must be in double quotes, is the "description" of the
 *   room, which is a full description of the room.  This is displayed
 *   when the player types "look around," when the player first enters the
 *   room, and any time the player enters the room when playing in VERBOSE
 *   mode.  
 */
entryway: Room 'Entryway'
    "This large, formal entryway is slightly intimidating:
    the walls are lined with somber portraits of gray-haired
    men from decades past; a medieval suit of armor<<describeAxe>>
    towers over a single straight-backed wooden chair.  The
    front door leads back outside to the south.  A hallway leads
    north. "

    /*
     *   In the description text above, we embedded the expression
     *   "describeAxe".  Whenever the description text is displayed, it
     *   will call evaluate that expression, which will in turn call this
     *   method, where we'll generate some additional text to describe the
     *   axe if it's still part of the suit of armor. 
     */
    describeAxe
    {
        if (axe.isIn(suitOfArmor))
            ", posed with a battle axe at the ready,";
    }

    /* 
     *   To the north is the hallway.  Set the "north" property to the
     *   destination room object.  Other direction properties that we
     *   could set: east, west, north, up, down, plus the diagonals:
     *   northeast, northwest, southeast, southwest.  We can also set "in"
     *   and "out", and the shipboard directions port, starboard, fore,
     *   and aft.  
     */
    north = hallway

    /*
     *   To the south is the front door.  A travel direction link can
     *   point directly to another room, but it can also point to
     *   something like a door.  
     */
    south = frontDoor

    /*
     *   The "out" direction is the same as south, since going south leads
     *   outside 
     */
    out = frontDoor
;

/*
 *   Define the front door.  The "+" sign in front of the definition means
 *   that the object is located within the most recently defined room,
 *   which in this case is 'entryway' as defined above.
 *   
 *   We start this object definition with two strings, both in single
 *   quotes, and a third in double quotes.  The first is the vocabulary
 *   list for the object, which tells us how the player can refer to this
 *   object.  The second string is the name, which is how the game refers
 *   to the object in generated messages.  The third is the full
 *   description of the object, which is displayed when the player
 *   examines this object.
 *   
 *   The vocabulary list consists of any number of words separated by
 *   spaces.  Every word is an adjective except the last, which is a noun.
 *   You can specify more than one noun by listing several nouns separated
 *   by slash characters ("/").  The player can use any of the words
 *   defined here to refer to the object - the player doesn't have to use
 *   all of the words, or use them in the same order that we define them
 *   here, except that adjectives and nouns must be in the grammatically
 *   correct order (in English, this means that adjectives must precede
 *   nouns).  
 */
+ frontDoor: Door 'front door' 'front door'
    "It's a heavy wooden door, currently closed. "

    /* the door is initially closed */
    initiallyOpen = nil

    /*
     *   Doors can usually be opened, but we don't want to allow this one
     *   to be opened.  The library by default allows a door to be opened
     *   and closed at will.  To change this, we must override the "direct
     *   object" handler for the Open action on this object.  Since we
     *   don't want anything to happen when the player tries to open the
     *   door, we can simply override the action handler and display a
     *   message indicating why we can't open the door.  
     */
    dobjFor(Open)
    {
        action() { "You'd rather stay in the house for now. "; }
    }
;

/*
 *   Define the chair.  We use the Chair class for this.  Note that the
 *   default Chair class defines a moveable object; we don't want our chair
 *   going anywhere, so make it use the Immovable class as well.
 *   
 *   We don't need to refer to the chair anywhere, so we don't bother
 *   giving it a name.  This saves us a little typing and saves us the
 *   trouble of thinking of a name for the object.  
 */
+ Chair, Immovable 'straight-backed wooden chair' 'wooden chair'
    "It looks like one of those formal chairs that looks elegant
    but is incredibly uncomfortable to sit on. "
;

/*
 *   Define the suit of armor.  It can't be moved because it's very heavy,
 *   so make it a Heavy object.  Note that we do need to refer to this
 *   object (in the 'entryway' object), so we need to give it an object
 *   name.
 *   
 *   Note that we define both "suit" and "armor" as nouns in our vocabulary
 *   list, because we want to be able to refer to it as "suit of armor"; in
 *   the phrasing "x of y", both x and y are noun phrases.  
 */
+ suitOfArmor: Heavy 'medieval plate-mail suit/armor' 'suit of armor'
    "It's a suit of plate-mail armor that looks suitable for
    a very tall knight. <<describeAxe>> "

    /* 
     *   as we did in entryway's description, we've embedded a call to our
     *   describeAxe method, so that we can add a description of the axe
     *   if appropriate 
     */
    describeAxe
    {
        if (axe.isIn(self))
            "The armor is posed with a huge battle-axe held
            at the ready. ";
    }
;

/*
 *   The battle axe, initially posed with the suit of armor.  We make this
 *   a Thing, because we want it to be something the player can pick up
 *   and manipulate.
 *   
 *   This definition starts with two "+" signs, to indicate that it is
 *   initially inside the last object defined with one "+" sign, which is
 *   the suit of armor.
 *   
 *   Note that we define a bunch of vocabulary words that aren't really
 *   synonyms for "axe," but are for things we describe as parts of the
 *   axe (the blade, the dried blood on the blade).  Those parts aren't
 *   worth defining as separate objects, but we can at least recognize
 *   them as vocabulary words that simply refer to the axe itself.  
 */
++ axe: Thing 'large steel battle dried ax/axe/blade/edge/blood' 'battle axe'
    "It's a large steel battle axe.  A little bit of dried blood on
    the edge of the blade makes the authenticity of the equipment
    quite credible. "

    /* 
     *   When we're located in the suit of armor, the suit of armor and
     *   the room containing the suit of armor describe us specially.
     *   This means that we do not want to display our name among the
     *   miscellaneous items listed in the room's description.  To prevent
     *   being listed in the ordinary description, indicate that we have a
     *   "special" description any time we're located in the suit of
     *   armor, and then make this special desription show nothing - it's
     *   not necessary to show anything because the room and suit of armor
     *   both already show something special for us.  
     */
    useSpecialDesc = (isIn(suitOfArmor))
    specialDesc = ""
;
    
/*
 *   Define the portraits.  We don't want to define several individual
 *   portraits, because they're not important enough, so define a single
 *   object that refers to the portraits collectively.
 *   
 *   Because the library normally allows the player to abbreviate any word
 *   to its first six or more letters, note that we don't have to provide
 *   separate vocabulary words for "portrait" and "portraits", or for
 *   "picture" and "pictures" - "portrait" is an acceptable abbreviation
 *   for "portraits".  
 */
+ Fixture 'somber gray-haired portraits/pictures/men/man' 'portraits'
    "The men in the portraits look like bankers or businessmen, all
    serious faces and old-fashioned suits. "

    /* 
     *   this object has a plural name, so we must set the isPlural flag
     *   to let the library know how to use its name in messages 
     */
    isPlural = true
;

/*
 *   The hallway, north of the entryway.  
 */
hallway: Room 'Hallway'
    "This broad, dimly-lit corridor runs north and south. "

    south = entryway
    north = kitchen
;

/*
 *   The kitchen.
 */
kitchen: Room 'Kitchen'
    "This is a surprisingly cramped kitchen, equipped with
    antique accoutrements: the stove is a huge black iron contraption,
    and in place of a refrigerator is an actual icebox.  A hallway
    lies to the south. "

    south = hallway
;

/*
 *   The stove is a Fixture, since we don't want the player to be able to
 *   move it.  It's also an OpenableContainer, because we want the player
 *   to be able to open and close it and put things in it.
 *   
 *   Note that we define 'stove' as both an adjective and as a noun,
 *   because we want the player to be able to refer to it not only as a
 *   "stove" but also as a "stove door".
 *   
 *   Because we're an OpenableContainer, the library will automatically add
 *   to our description text an open/closed indication and a listing of any
 *   contents when we're open.  
 */
+ Fixture, OpenableContainer
    'huge black iron stove stove/oven/contraption/door' 'stove'
    "It's a huge black iron cube, with a front door that swings
    open sideways. "

    /* it's initially closed */
    initiallyOpen = nil
;

/*
 *   Put a loaf of bread in the stove.  It's edible, so use the library
 *   class Food. 
 */
++ Food 'fresh golden-brown brown loaf/bread/crust' 'loaf of bread'
    "It's a fresh loaf with a golden-brown crust. "

    /* 
     *   we want to provide a special message when we eat the bread, so
     *   override the direct object action handler for the Eat action;
     *   inherit the default handling, but also display our special
     *   message, which will automatically override the default message
     *   that the base class produces 
     */
    dobjFor(Eat)
    {
        action()
        {
            /* inherit the default handling */
            inherited();

            /* show our special description */
            "You tear off a piece and eat it; it's delicious.  You tear off
            a little more, then a little more, and before long the whole loaf
            is gone. ";
        }
    }
;

/*
 *   The icebox is similar to the stove.
 */
+ Fixture, OpenableContainer 'ice box/icebox' 'icebox'
    "Before there were refrigerators, people had these: it's just
    a big insulated box, into which one would put perishables
    along with enough ice to keep the perishables chilled for a few
    days. "

    /* 
     *   when looking in the icebox, explicitly point out that it contains
     *   no ice; do this by overriding the LookIn action handler,
     *   inheriting the default handling and adding our own message 
     */
    dobjFor(LookIn)
    {
        action()
        {
            /* show the default description */
            inherited();

            /* add a note that there's no ice, after a paragraph break */
            "<.p>It's been a long time since any ice was in there. ";
        }
    }
;

/*
 *   Define the player character.  The name of this object is not
 *   important, but note that it has to match up with the name we use in
 *   the main() routine to initialize the game, below.
 *   
 *   Note that we aren't required to define any vocabulary or description
 *   for this object, because the class Actor, defined in the library,
 *   automatically provides the appropriate definitions for an Actor when
 *   the Actor is serving as the player character.  Note also that we
 *   don't have to do anything special in this object definition to make
 *   the Actor the player character; any Actor can serve as the player
 *   character, and we'll establish this one as the PC in main(), below.  
 */
me: Actor
    /* the initial location is the entryway */
    location = entryway
;

/*
 *   The "gameMain" object lets us set the initial player character and
 *   control the game's startup procedure.  Every game must define this
 *   object.  For convenience, we inherit from the library's GameMainDef
 *   class, which defines suitable defaults for most of this object's
 *   required methods and properties.  
 */
gameMain: GameMainDef
    /* the initial player character is 'me' */
    initialPlayerChar = me

    /* 
     *   Show our introductory message.  This is displayed just before the
     *   game starts.  Most games will want to show a prologue here,
     *   setting up the situation for the player, and show the title of the
     *   game.  
     */
    showIntro()
    {
        "Welcome to the TADS 3 Starter Game!\b";
    }

    /* 
     *   Show the "goodbye" message.  This is displayed on our way out,
     *   after the user quits the game.  You don't have to display anything
     *   here, but many games display something here to acknowledge that
     *   the player is ending the session.  
     */
    showGoodbye()
    {
        "<.p>Thanks for playing!\b";
    }
;
