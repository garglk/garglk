#charset "us-ascii"

/*
 *   Sample game using the TADS 3 library.
 *   
 *   This is an extremely contrived game, and is not intended to be the
 *   least bit interesting to a player but rather serves as a test of
 *   various library features during library development.  
 */

#include "adv3.h"
#include "tok.h"
#include "en_us.h"

/*
 *   This is how we'd change a library grammar definition for a verb, if
 *   we wanted to replace library grammar with our own.  This doesn't
 *   change the rest of the VerbRule definition; it merely changes the
 *   grammar.  If we wanted to change the rest of the VerbRule, we'd use
 *   'replace' rather than 'modify'.  Note that 'replace' and' modify'
 *   both replace the grammar rule itself, though; they differ in how they
 *   handle the *rest* of the properties of the VerbRule object.
 *   
 *   To delete a library verb rule grammar, modify the rule and use an
 *   unmatchable token as the replacement grammar rule (' ' will work,
 *   since the standard tokenizer won't return a space character as a
 *   token).  
 */
//modify VerbRule(Examine)
//    ('ex' | 'lkat') DobjList :
//;


/* ------------------------------------------------------------------------ */
/*
 *   Game credits and version information
 */
versionInfo: GameID
    IFID = 'fa555579-c107-f533-d2a8-d2c93ca87ea0'
    name = 'TADS 3 Library Sampler'
    version = '1.0'
    byline = 'by M.J.Roberts'
    htmlByline =
        'by <a href="mailto:mjr_@hotmail.com">M.&nbsp;J.&nbsp;Roberts</a>'
    authorEmail = 'M.J. Roberts <mjr_@hotmail.com>'
    desc = 'A random sample and test of features of
               the tads 3 library.'
    htmlDesc = 'A random sample and test of features of the
                <b><font size=-1>TADS</font> 3 library</b>.'

    /* add some special text of our own for the credits */
    showCredit()
    {
        "<b><<name>></b>\n
        <<htmlByline>>\b
        Thanks to everyone who has participated in the TADS 3 library
        design process for contributing so many great ideas.
        \b
        <center>
        * * *
        \n
        </center>
        \b";
    }

    /* show "about" information */
    showAbout()
    {
        "This is just a boring sample game to test features of
        the library under construction.  Everything in this game
        is contrived to exercise a particular set of library
        features, and we make no attempt to make a coherent
        game out of this hodge-podge of tests.  A more interesting
        demonstration game will have to wait until the library
        is further along. ";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Set up a barrier object so that we can conveniently prevent the
 *   tricycle from going past certain points.  This is just a generic
 *   vehicle barrier, but we customize the failure message.  
 */
tricycleBarrier: VehicleBarrier
    construct(msg) { msg_ = msg; }
    explainTravelBarrier(traveler)
    {
        if (traveler == tricycle)
            reportFailure(msg_);
        else
            inherited(traveler);
    }
    msg_ = '{You/he}\'d best stay indoors with the tricycle; it would
            probably break if you rode it outside. '
; 

/* set up a barrier against pushing the television out of the house */
tvBarrier: PushTravelBarrier
    construct(msg) { msg_ = msg; }
    canPushedObjectPass(obj) { return obj != television; }
    explainTravelBarrier(traveler) { reportFailure(msg_); }
    msg_ = 'The television\'s casters are too small to
            allow the TV to go beyond this point. '
;

/* ------------------------------------------------------------------------ */
/*
 *   Trivial class for rooms in the house.  We use this mostly for
 *   identification of house rooms.  
 */
class HouseRoom: Room
;

class DarkHouseRoom: DarkRoom, HouseRoom
;

/* ------------------------------------------------------------------------ */
/*
 *   Basic Coin class 
 */
class Coin: Thing
    vocabWords = 'coin*coins'
    listWith = [coinGroup]

    /* 
     *   The base name for the coin as it appears in groups.  We leave off
     *   the word "coin" because the coin group prefix uses it.  
     */
    coinGroupBaseName = ''

    /* use the coinCollective for selected actions on coins */
    collectiveGroups = [coinCollective]

    /* 
     *   coin group name and counted name - we synthesize these from the
     *   group base name 
     */
    coinGroupName = ('one ' + coinGroupBaseName)
    countedCoinGroupName(cnt)
        { return spellIntBelow(cnt, 100) + ' ' + coinGroupBaseName; }
;

/*
 *   copper coins 
 */
class CopperCoin: Coin 'copper -' 'copper coin' @livingRoom
    isEquivalent = true
    coinValue = 1
    coinGroupBaseName = 'copper'
;

/* 
 *   silver coins 
 */
class SilverCoin: Coin 'silver -' 'silver coin' @livingRoom
    isEquivalent = true
    coinValue = 5
    coinGroupBaseName = 'silver'
;

/*
 *   gold coins 
 */
class GoldCoin: Coin 'gold -' 'gold coin' @livingRoom
    isEquivalent = true
    coinValue = 10
    coinGroupBaseName = 'gold'

    /* flag: I've awarded points for being taken by the player character */
    takeAwarded = nil

    /* award points when the item is first taken */
    afterAction()
    {
        /* inherit the default handling */
        inherited();

        /* 
         *   if I'm now being held directly or indirectly, and I haven't
         *   awarded my points for being taken before, award points now 
         */
        if (!takeAwarded && isIn(libGlobal.playerChar))
        {
            /* award my points */
            goldCoinAchievement.awardPoints();

            /* only award this achievement once per coin */
            takeAwarded = true;
        }
    }
;

/*
 *   Coin group - this is a listing group we use to group coins in room
 *   contents lists, object contents lists, and inventory lists.  
 */
coinGroup: ListGroupParen
    showGroupCountName(lst)
    {
        "<<spellIntBelowExt(lst.length(), 100, 0,
           DigitFormatGroupSep)>> coins";
    }

    /* 
     *   Just for the heck of it, order coins in a group in descending
     *   order of monetary value.  Since we want descending order, return
     *   the negative of the value comparison.  
     */
    compareGroupItems(a, b) { return b.coinValue - a.coinValue; }

    /*
     *   Customize the display of the individual coins listed in a coin
     *   group, so that we leave off the word "coin" from the name.  This
     *   will make our group list look like so:
     *   
     *   five coins (two gold, two silver, one copper) 
     */
    showGroupItem(lister, obj, options, pov, info)
        { say(obj.coinGroupName); }
    showGroupItemCounted(lister, lst, options, pov, infoTab)
        { say(lst[1].countedCoinGroupName(lst.length())); }
;

/*
 *   Coin Collective - this is a collective group object that we use to
 *   perform certain actions on coins collectively, rather than iteratively
 *   on the individuals.  This is an "itemizing" collective group, because
 *   we want "examine coins" to show a message listing the individual
 *   coins.  
 */
coinCollective: ItemizingCollectiveGroup '*coins' 'coins'
;


/* ------------------------------------------------------------------------ */
/*
 *   Balloons 
 */
class Balloon: Thing 'inflated uninflated popped balloon'
    /* balloons are by default equivalent to one another */
    isEquivalent = true

    /* list balloons in one place as a group */
    listWith = [balloonGroup]

    /* our state index - this is an index into our allStates list */
    state = nil

    /* get our current state - translates our index to a state */
    getState = (allStates[state])

    /* our list of all of our possible states */
    allStates = [balloonStateInflated,
                 balloonStateUninflated,
                 balloonStatePopped]
;

class RedBalloon: Balloon 'red -' 'red balloon';
class BlueBalloon: Balloon 'blue -' 'blue balloon';
class GreenBalloon: Balloon 'green -' 'green balloon';

/* use a simple unadorned grouper to keep all the balloons together */
balloonGroup: ListGroupSorted;

/* balloon state objects */
balloonStateInflated: ThingState 'inflated' +1
    stateTokens = ['inflated']
;
balloonStateUninflated: ThingState 'uninflated' +2
    stateTokens = ['uninflated']
;
balloonStatePopped: ThingState 'popped' +3
    stateTokens = ['popped']
;


sun: MultiFaceted
    initialLocationClass = OutdoorRoom
    instanceObject: Distant { 'sun' 'sun'
        "It's the yellow star around which the Earth orbits. "
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Living Room 
 */

redBook: Thing 'red book*books' 'red book' @livingRoom;
blueBook: Thing 'blue test book/booklet*books booklets'
    'blue test booklet' @livingRoom;
bigRedBall: Thing 'big large red ball*balls' 'large red ball' @livingRoom;
smallRedBall: Thing 'small little red ball*balls' 'small red ball' @livingRoom;
greenBall: Thing 'green ball*balls' 'green ball' @livingRoom;
box: Container 'brown cardboard box' 'brown box' @livingRoom;
poBox: Thing 'p. o. p.o. po post office # box' 'PO Box' @livingRoom;
eightball: Thing '8 ball/8-ball*balls' '8-ball' @livingRoom;

greenJar: OpenableContainer 'green glass jar*jars' 'green jar' @livingRoom
    "It's made of transparent green glass. "
    material = glass
    initiallyOpen = nil
;

clearJar: OpenableContainer 'clear glass jar*jars' 'clear jar' @livingRoom
    "It's made of clear glass. "
    material = glass
    initiallyOpen = true
    isEquivalent = true
;

CopperCoin location=greenJar;
CopperCoin location=greenJar;

SilverCoin;
SilverCoin;
SilverCoin;

GoldCoin;
GoldCoin;
GoldCoin;
GoldCoin;
GoldCoin;

/* achievement for obtaining a gold coin */
goldCoinAchievement: Achievement
    "obtaining <<spellInt(scoreCount)>> gold coin<<
      scoreCount > 1 ? 's' : ''>>"

    /* 
     *   we're worth two points, and we can be awarded up to five times
     *   (one time per gold coin), for a total of ten points 
     */
    points = 2
    maxPoints = 10
;

ironKey: Key 'iron key*keys' 'iron key' @livingRoom
    "It's an ordinary iron key. "
;
brassKey: Key 'brass key*keys' 'brass key' @livingRoom
    "It's an ordinary brass key. "
;
rustyKey: Key 'rusty key*keys' 'rusty key' @livingRoom
    "It's an ordinary rusty key. "
;

/*
 *   Note that we don't have to define any vocabulary for the player
 *   character, since the library automatically defines the appropriate
 *   pronouns for any actor being used as the player character.  The
 *   pronoun selection is automatic and dynamic, so if we change to
 *   another player character later, the pronouns will all switch
 *   automatically at the same time.  
 */
me: Person
    location = livingRoom
    bulkCapacity = 5 // to force lots of bag-of-holding shuffling, for testing
//  referralPerson = firstPerson

    /* 
     *   When we issue a series of commands to another actor, we can wait
     *   until the entire series finishes before we take another turn.
     *   Set this flag to true to wait, or nil to allow us to take turns
     *   while the other actor carries out our orders. 
     */
    issueCommandsSynchronously = true
;

+ duffel: BagOfHolding, Openable, Container 'duffel bag' 'duffel bag'
    "It's a really capacious bag. "
;

+ keyring: Keyring 'key ring/keyring' 'keyring'
;

bob: Person 'bob' 'Bob' @livingRoom
    isProperName = true
    isHim = true

    /* obey any command from any other character */
    obeyCommand(issuer, action) { return true; }

    /* 
     *   We want to be able to follow other actors if requested, so keep
     *   track of departure data.  NPC's don't track departure information
     *   by default, because most NPC's never need it, and tracking it
     *   consumes a little extra time and memory.  
     */
    wantsFollowInfo(obj) { return true; }
;

/*
 *   Respond to any ASK ABOUT topic with a generic message incorporating
 *   the tokens from the topic.  
 */
+ DefaultAskTopic
    "<q>Ah, yes, <<gTopic.getTopicText()>>, very interesting...</q> "
;

+ GiveTopic
    matchTopic(fromActor, obj)
    {
        /* we match any coin */
        return obj.ofKind(Coin) ? matchScore : nil;
    }

    handleTopic(fromActor, obj)
    {
        /* accept the coin into my actor's inventory */
        obj.moveInto(getActor());

        /* add our special report */
        gTranscript.addReport(new AcceptCoinReport(obj));

        /* register for collective handling at the end of the command */
        gAction.callAfterActionMain(self);
    }

    afterActionMain()
    {
        /*
         *   adjust the transcript by summarizing consecutive coin
         *   acceptance reports 
         */
        gTranscript.summarizeAction(
            {x: x.ofKind(AcceptCoinReport)},
            {vec: '\^' + getActor().theName + ' accepts the '
                  + spellInt(vec.length()) + ' coins. ' });
    }
;

+ AskForTopic [ironKey, brassKey, rustyKey]
    handleTopic(fromActor, topic)
    {
        "<q>So, you want a key, do you? Let's see... ";
        SimpleLister.showSimpleList(topic.inScopeList + topic.likelyList);
        "...</q>";
    }
;

class AcceptCoinReport: MainCommandReport
    construct(obj)
    {
        /* remember the coin we accepted */
        coinObj = obj;

        /* inherit the default handling */
        gMessageParams(obj);
        inherited('Bob accepts {the obj/him}. ');
    }

    /* my coin object */
    coinObj = nil
;

bill: Person 'bill' 'Bill' @livingRoom
    isProperName = true
    isHim = true
    obeyCommand(issuer, action) { return true; }
    wantsFollowInfo(obj) { return true; }

    /* 
     *   Make Bill go after Bob.  This normally wouldn't be important,
     *   since the relative order of actor turns is usually arbitrary, but
     *   we want our test scripts to be stable so we establish a fixed
     *   order.  We accomplish the order adjustment simply by giving Bill a
     *   slightly higher schedule order than the default.  
     */
    calcScheduleOrder()
    {
        inherited();
        scheduleOrder += 1;
    }
;

livingRoom: HouseRoom
    roomName = 'Living Room'
    destName = 'the living room'
    desc = "It's a nice, big room.  A potted plant is the only attempt
            at decoration.  Passages lead north, east, and west.
            To the south, the front door of the house
            leads outside. "
    north = diningRoom
    east = den
    south = frontDoor
    out asExit(south)
    west = platformRoom
;

+ tricycle: Vehicle, BasicChair 'tricycle/trike' 'tricycle'
    "It's a child's three-wheeled bike. "

    gaveInstructions = nil

    allowedPostures = [sitting]

    /*
     *   Customize the travel arrival/departure messages for certain types
     *   of connectors.  The default message is of the form "The tricycle
     *   (carrying whoever) arrives from the east."  This isn't great for
     *   the tricycle, which only carries one rider, and which only goes
     *   when the rider makes it go; a better message would be "Bob rides
     *   the tricycle in from the east," which better captures that the
     *   rider is the one making the tricycle go somewhere.  
     */
    sayArrivingDir(dir, conn)
    {
        local actor = getTravelerActors()[1];
        
        "<<actor.name>> ride<<actor.verbEndingS>> in from the
        <<dir.name>> on a tricycle. ";
    }
    sayArrivingThroughPassage(conn)
    {
        local actor = getTravelerActors()[1];
        
        "<<actor.name>> ride<<actor.verbEndingS>> in through
        <<conn.theName>> on a tricycle. ";
    }
    sayDepartingDir(dir, conn)
    {
        local actor = getTravelerActors()[1];
        
        "<<actor.name>> ride<<actor.verbEndingS>> the tricycle off
        to the <<dir.name>>. ";
    }
    sayDepartingThroughPassage(conn)
    {
        local actor = getTravelerActors()[1];
        
        "<<actor.name>> ride<<actor.verbEndingS>> off
        through <<conn.theName>> on the tricycle. ";
    }

    dobjFor(Ride) asDobjFor(SitOn)

    dobjFor(SitOn)
    {
        check()
        {
            /* don't allow riding the tricycle except in the house */
            if (!gActor.location.getOutermostRoom().ofKind(HouseRoom))
            {
                reportFailure('{You/he} shouldn\'t ride {the dobj/him}
                    except in the house; {it dobj/he} might break. ');
                exit;
            }
        }

        action()
        {
            /* inherit the default handling */
            inherited();

            /* if we haven't given instructions previously, do so now */
            if (!gaveInstructions)
            {
                mainReport('Okay, {you\'re} now on the tricycle.  {You/he}
                    can probably manage to ride around on it if
                    {it/he} tr{ies}; just say the direction you\'d like '
                    + (gActor.isPlayerChar ? '' : '{it/him}') + ' to go. ');
                gaveInstructions = true;
            }
        }
    }

    dobjFor(Eat) { verify() { } action() { "{You/he} eat{s} {your} tricycle. "; } }
;

+ frontDoor: Door 'front door' 'front door'
    "It leads outside to the south. "
    travelBarrier = [tricycleBarrier, tvBarrier]
;

+ Decoration
    'potted plant small green clay plant/tree/pot/leaf/leaves/dirt/soil'
    'potted plant'
    "It's a tree with small green leaves, about six feet tall and
    growing in a clay pot filled with soil. "
;

+ television: TravelPushable
    'old huge boxy model/unit/television/tv' 'television'
    "It's an old, huge, boxy model, certainly black-and-white.  The
    bulbous picture tube shows no sign of life.  Two dials control
    the channel selection (it looks like it's currently <<curChannel>>),
    but the channel setting hardly matters when the set doesn't work
    in the first place.  It seems to be on small casters, which is
    a good thing given how big it is and how heavy it looks. "

    specialDesc = "An old television, a huge, boxy, massive-looking thing,
                   sits on the floor. "
    specialNominalRoomPartLocation = defaultFloor

    curChannel()
    {
        if (upperDial.curSetting == 'UHF')
            return lowerDial.curSetting;
        else
            return upperDial.curSetting;
    }
;

++ Decoration 'casters' 'casters'
    "Small wheels, intended to make it easier to move the unit's
    huge bulk around the house. "
;

++ Decoration 'bulbous picture tube/screen' 'picture tube'
    "It shows no pictures. ";

++ upperDial: NumberedDial, Component
    'upper tv television channel dial*dials' 'upper dial'
    "This is the VHF selector dial; it can be set to a channel from 2
    through 13, or to the additional stop marked \"UHF\". It's
    currently set to <<curSetting>>. "

   minSetting = 2
   maxSetting = 13
   curSetting = 8
   isValidSetting(val)
   {
       /* allow the special 'uhf' setting in addition to the numbers */
       if (val.toLower() == 'uhf')
           return true;

       /* inherit the default handling to check the numbered settings */
       return inherited(val);
   }
   makeSetting(val)
   {
       /* if the setting is 'uhf', change it to all caps */
       if (val.toLower() == 'uhf')
           val = 'UHF';

       /* inherit default handling with the (possibly) adjusted value */
       inherited(val);
   }
;

++ lowerDial: NumberedDial, Component
    'lower tv television channel dial*dials' 'lower dial'
    "This is the UHF selector dial; it can be set to a channel from
    14 through 82.  It's currently set to <<curSetting>>.  You probably
    have to turn the upper dial to \"UHF\" for this dial's setting
    to have any effect. "
    minSetting = 14
    maxSetting = 82
    curSetting = 44
;


+ Consultable 'phone book' 'phone book'
    "You can probably look up people you know in here. "
;
++ ConsultTopic @bob "Bob's number is (415)555-1212. ";
++ ConsultTopic @bill "Bill's number is (650)555-1212. ";
++ ConsultTopic @salesman "Ron's number is (510)555-1212. ";
++ ConsultTopic @insCompany
    "<q>Actuarial Life Insurance Company!  Call for best rates!
    (800)555-1212!</q> "
;
++ DefaultConsultTopic "You can't find any such listing. ";

/* ------------------------------------------------------------------------ */
/*
 *   Platform room 
 */
platformRoom: HouseRoom 'Platform Room' 'the platform room'
    "This room is obviously highly contrived.  Filling one end of the
    room is a platform, almost like a theater stage, raised a couple of
    feet above the floor.  At opposite ends of the 
    platform are two smaller platforms, one red and one blue; 
    atop each smaller platform is a chair of the same color as its platform.
    In the center of the main platform is a raised dais, upon which
    <<stool.isDirectlyIn(dais) ? "are a wooden stool and" : "is">> a
    leather armchair.
    <.p>The only exit is east. "

    east = livingRoom
    down = (whiteBox.isOpen ? whiteBox.down : nil)
;

+ Fixture, Platform
    'main theater theatre platform/stage*platforms' 'main platform'
;

++whiteBox: Openable, Booth 'large white cardboard box' 'white box'
    useInitSpecialDesc()
    {
        /* show our initial description only when the actor isn't in me */
        return inherited() && !gActor.isIn(self);
    }
    initSpecialDesc = "On the main platform is a large white box,
        about waist high and quite roomy. "

    /* don't mention our contents if we're using our initial description */
    contentsListed = (!useSpecialDesc())

    interiorDesc = "The box is roomy, but not enough that you can
                    stand up in it when it's closed. "

    down = whiteBoxTrapDoor

    /* make it paper, so we can hear through the box */
    material = paper

    /* 
     *   we can't stand up in the box when it's closed, so make the
     *   default posture sitting 
     */
    defaultPosture = (isOpen ? standing : sitting)

    /* we can't close the box when someone's standing in it */
    makeOpen(stat)
    {
        /* if we're closing the box, check to make sure we're allowed to */
        if (!stat)
        {
            /* 
             *   We're trying to close the box; if anyone's standing in
             *   the box, don't allow it. 
             */
            foreach (local cur in allContents())
            {
                /* if this is a standing actor, disallow closure */
                if (cur.isActor && cur.posture == standing)
                {
                    /* 
                     *   we can't close - issue a failure report and
                     *   terminate the command 
                     */
                    reportFailure('{You/he} cannot close the box while
                        anyone is standing in it. ');
                    exit;
                }
            }
        }

        /* no problems - inherit default handling */
        inherited(stat);
    }

    /* 
     *   if they try to stand within a nested room within us, mention that
     *   they can only sit 
     */
    roomBeforeAction()
    {
        /* 
         *   If we're closed, and the action is 'stand', and they're
         *   something nested within me, mention after the fact that
         *   there's no room to stand up.  We need to mention this, but we
         *   do not need to enforce the constraint here because the nested
         *   room will set our default sitting posture when moving an
         *   actor from an interior location.  
         */
        if (!isOpen
            && gActionIs(Stand)
            && gActor.isIn(self)
            && !gActor.isDirectlyIn(self))
            reportAfter('There isn\'t room to stand up in here, so
                {you/he} sit{s} in the box. ');
    }

    /* enforce the low headroom when the box is closed */
    makeStandingUp()
    {
        if (isOpen)
        {
            /* we're open, so proceed as normal */
            inherited();
        }
        else
        {
            /* the box is closed, so they can't stand up */
            reportFailure('There\'s not enough room to stand up in
                the box while it\'s closed. ');
        }
    }
;

+++ whiteBoxTrapDoor: Fixture, Door -> tunnelTrapDoor
    'small trap door' 'trap door'
    "It's set into the floor of the box; it looks just large enough
    for you to fit through. "

    /* 
     *   list me *after* the room's contents, since the box is sometimes
     *   listed with the room's contents 
     */
    specialDescBeforeContents = nil

    initSpecialDesc = "A small trap door is set into the floor of the box. "

    /* don't require standing up before entering the trap door */
    actorTravelPreCond(actor) { return []; }

    enteredBefore = nil
    dobjFor(TravelVia)
    {
        action()
        {
            /* mention the ladder below */
            if (enteredBefore)
                "You find the ladder below and climb down. ";
            else
            {
                "You tentatively lower yourself through the door, and
                manage to find what feels like a ladder below, so you
                carefully climb down. ";
                enteredBefore = true;
            }

            /* inherit the default handling */
            inherited();
        }
    }
;

+++ Switch 'small black plastic beeper pager box/beeper/pager/switch' 'beeper'
    "It's a small black plastic box with a single red light
    (which is currently <<isOn ? 'lit' : 'off'>>) and a
    switch (currently <<isOn ? 'on' : 'off'>>). "

    /* 
     *   if they know about us, they'll recognize our beeping, so they'll
     *   want to be able to refer to the beeper itself; thus, give the
     *   beeper a sound presence once it's been seen 
     */
    soundPresence = (isOn && seen)

    isOn = true
    makeOn(val)
    {
        /* inherit the default handling */
        inherited(val);

        /* add/remove our beeping sound, as appropriate */
        beeperNoise.moveInto(val ? self : nil);
    }
;

++++ Component 'tiny small red beeper light' 'beeper light'
    "It's a tiny red light, currently <<location.isOn ? 'lit' : 'off'>>. "
;

++++ beeperNoise: Noise
    'piercing high-pitched high pitched beeping sound/noise/beep'
    'beeping sound'
    sourceDesc = "The beeper is making a piercing, high-pitched beeping
                  noise. "
    descWithSource = "The beeper is beeping, which is what they do. "
    descWithoutSource = "It's a piercing, high-pitched beeping noise. "

    hereWithSource = "The beeper is making a piercing beeping noise. "
    hereWithoutSource = "You can hear a high-pitched beeping noise. "

    displaySchedule = [2, 4]
;

++ Fixture, Platform 'small smaller red platform*platforms' 'red platform'
;

class PlatformChair: Chair
    dobjFor(SitOn)
    {
        verify()
        {
            /* inherit default */
            inherited();

            /* 
             *   if they're directly in my location, and my location is a
             *   nested room of some kind, boost likelihood that this is
             *   where they want to sit 
             */
            if (gActor.isDirectlyIn(location) && location.ofKind(NestedRoom))
                logicalRank(150, 'adjacent');
        }
    }
;

+++ Fixture, PlatformChair 'red chair' 'red chair'
;

++ Fixture, Platform 'small smaller blue platform*platforms' 'blue platform'
;

+++ Fixture, PlatformChair 'blue chair' 'blue chair'
;

++ dais: Fixture, Platform 'raised dais' 'dais'
;

+++ stool: PlatformChair 'wooden wood stool' 'stool'
    /* 
     *   since we're part of the main room description if we're in our
     *   original location, don't list me if I'm on the dais 
     */
    isListed = (location == dais ? nil : inherited)
;

+++ Fixture, PlatformChair 'leather arm chair/armchair' 'armchair'
    actorInPrep = 'in'
;

#if 0 // disabled by default
/* 
 *   enable this to test multi-location sense connections - this is a bit
 *   too contrived an example, so we disable it by default 
 */
whiteBoxHole: SenseConnector, Fixture 'small hole' 'small hole'
    "It's a small hole in the box. "
    locationList = [whiteBox, backYard]
    connectorMaterial = glass
;
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Front yard 
 */
frontYard: OutdoorRoom 'Front Yard' 'the front yard'
    "This is a small yard in front of a modest house.  The front
    door to the house leads in to the north.  To the south is
    the street. "
    in asExit(north)
    north = frontDoorOutside
    south: NoTravelMessage { "You'd rather not venture beyond the
               immediate vicinity of the house right now. " }
    atmosphereList: RandomEventList {
        eventList = [
            nil,
            'A car drives past. ',
            'Someone honks a horn in the distance. ',
            nil,
            'A few birds pass overhead. ',
            'A couple of bicyclists pedal past on the street. ',
            nil,
            nil ]
    }
;

+ frontDoorOutside: Door ->frontDoor 'front door' 'front door'
    "It leads north, into the house. "
;

+ Enterable, Decoration 'narrow residential street/road' 'street'
    "It's a narrow residential street. "
    connector = (frontYard.south)
;

+ Enterable, Decoration ->frontDoorOutside 'house' 'house'
    "It's a modest house.  A door leads in to the north. "
;

+ RedBalloon state = 3;
+ RedBalloon state = 3;
+ RedBalloon state = 3;
+ RedBalloon state = 3;
+ RedBalloon state = 1;
+ RedBalloon state = 1;
+ RedBalloon state = 2;

+ BlueBalloon state = 2;
+ BlueBalloon state = 1;
+ BlueBalloon state = 1;

+ GreenBalloon state = 3;
+ GreenBalloon state = 3;
+ GreenBalloon state = 3;
+ GreenBalloon state = 3;
+ GreenBalloon state = 3;

+ salesman: Person 'salesman/man/ron' 'salesman'
    "He's a thin, youngish man in a garish suit that's rather comically
    oversized on him. "
    isHim = true
;
++ Wearable 'oversized garish suit' 'suit'
    "It's an oddly bright shade of blue, and is way too large for its
    present wearer. "
   wornBy = salesman
;
++ Thing 'huge black briefcase' 'briefcase'
    "It's a huge black briefcase. "
;

/* the salesman's initial state */
++ ActorState
    isInitState = true
    specialDesc = "A youngish man in a garish suit is standing in
                   the yard near the front door. "

    takeTurn()
    {
        if (gPlayerChar.location == frontYard)
            salesman.initiateConversation(salesConv, 'sales-hello');
        inherited();
    }
;

++ salesConv: InConversationState
    attentionSpan = 10000
    nextState = salesWaiting
    stateDesc = "He's smiling eagerly at you. "
    specialDesc = "The salesman is standing a little too close,
                  smiling a bit too much. "
;

++ salesWaiting: ConversationReadyState
    specialDesc = "The salesman is standing in the yard. "

    inConvState = salesConv

    takeTurn()
    {
        /* re-arm for conversation only when the PC is gone */
        if (gPlayerChar.location != frontYard)
            regreet = true;

        /* greet again randomly if we're re-armed and the PC is present */
        if (regreet && gPlayerChar.location == frontYard && rand(100) < 33)
            salesman.initiateConversation(salesConv, 'sales-1');

        inherited();
    }

    /* flag: we're ready to re-greet the player */
    regreet = nil

    /* on activation, set the re-greet flag to nil by default */
    activateState(actor, oldState)
    {
        inherited(actor, oldState);
        regreet = nil;
    }
;
+++ HelloTopic
    "You tap the salesman on the shoulder.  <q>Oh, hi!</q> he says.
    <q>I\'d sure like to talk to you about life insurance.</q>
    <.convnode sales-1> "
;
+++ ByeTopic, ShuffledEventList
    ['<q>I have to go,</q> you say.
    <.p><q>Thanks for your time!</q> the salesman says. ',
    
     '<q>Thanks, but not right now,</q> you say.
     <.p><q>I\'ll be here when you\'re ready!</q> the salesman says. ']
;
+++ ImpByeTopic, ShuffledEventList
    ['The salesman waves. <q>Okay, I\'ll wait here!</q> he says. ',
     '<q>Thanks for your time!</q> the salesman says. ',
     'The salesman calls after you, <q>I\'ll be here if you need me!</q> ']
;

++ ConvNode 'sales-hello'
    npcGreetingMsg = "<.p>The man in the bad suit walks up to you and
                      extends his hand; by habit you shake his hand.
                      <q>Hello, friend!  My name is Ron, and I represent the
                      Acturial Life Insurance Company.</q> He releases
                      your hand after a vigorous shaking. <q>You know, a lot
                      of people I talk to don't know just how important life
                      insurance is to proper financial planning.  Let me
                      ask you this: do you have all the life insurance
                      coverage you and your family need?</q> "

    npcContinueList: CyclicEventList {
    [
        'The salesman says eagerly, <q>Really, do you have enough
        insurance?</q> ',
        
        'Ron says, <q>The sad fact is, most people <i>don\'t know</i>
        how much insurance they really need.  I\'d really like to
        tell you about our policies...</q><.convnode sales-1> '
    ]}
;
+++ YesTopic
    "<q>I'm covered,</q> you say.
    <.p>Ron smiles and nods. <q>You know, a lot of people <i>think</i>
    they're covered. But have you really <i>read</i> your insurance
    policy?  Most insurance policies have so many exclusions and
    limitations that you just can't rely on them.  That's why
    Acturial Life created a new kind of insurance policy.  Let me
    tell you about it...</q><.convnode sales-1> "
;
+++ NoTopic
    "<q>Why, no,</q> you say.
    <.p>Ron smiles eagerly. <q>Well, then it's a good thing I was in
    your neighborhood today! Let me tell you about our policies...</q>
    <.convnode sales-1> "
;

++ ConvNode 'sales-1'
    npcGreetingMsg = "<.p>Ron walks up to you. <q>Hello again, friend!
                      It would be my pleasure to help you with your
                      insurance needs.</q> "

    npcContinueList: ShuffledEventList {
    ['The salesman looks serious for a moment. <q>Most people don\'t know
     this, but death is our number one killer,</q> he says earnestly.
     He goes back to smiling. <q>Fortunately, death is covered under
     our policy\'s loss-of-life section.</q> ',

     'Ron says, <q>You know, life insurance is a lot more interesting
     than most people think.</q> ',

     'The salesman says, <q>I\'m really glad I was in the neighborhood
     today.  Everyone needs more insurance than they think.</q> '
    ]}
;

++ AskTellTopic, StopEventList @insPolicy
    [
        '<q>Okay, tell me about your policy,</q> you say.
        <.p><q>Our policy is the best in the business,</q> Ron says.
        <q>For starters, it\'s guaranteed go pay, which most policies
        aren\'t.  There\'s so much more I can tell you.</q> ',

        '<q>Tell me more about your policy,</q> you say.
        <.p><q>I could go on all day,</q> the salesman says. '
    ]
;

++ AskTellTopic, StopEventList @insCompany
    ['<q>I\'ve never heard of your company,</q> you say.
     <.p><q>Most people haven\'t,</q> Ron says. <q>We\'re the best-kept
     secret in the business.</q>',

     '<q>Why would you want to keep your company a secret?</q>
     <.p><q>One word: savings.  We save on marketing expenses, and
     pass the low cost on to you.</q> ',

     '<q>What else can you tell me about your company?</q>
     <.p><q>I\'d rather tell you about the policy.</q> ']
;

/* some topics for the salesman to talk about */
insPolicy: Topic 'life insurance policy/policies';
insCompany: Topic 'acturial life insurance company';

/* ------------------------------------------------------------------------ */
/*
 *   Den 
 */
den: HouseRoom
    roomName = 'Den'
    destName = 'the den'
    desc = "This small room is dominated by a desk, a massive steel
            edifice painted a drab gray, something that would be
            more at home in a government office during the cold war.
            Behind the desk, built in to the north wall, is a
            bookcase.  A passage leads west. "
    west = livingRoom
    north = bookcasePassage
    roomParts = static (inherited() - defaultNorthWall)
;

+ Decoration 'north wall*walls' 'north wall'
    "A floor-to ceiling bookcase<<
      bookcasePassage.isOpen ? " (which has swung aside to expose
      a passage to the north)" : "">>
    is built into the wall. "
;

+ bookcase: Fixture 'book case/bookcase' 'bookcase'
    desc
    {
        "It's a floor-to-ceiling bookcase built in to the north
        wall, behind the desk, filled with hundreds of dusty tomes. ";
        if (bookcasePassage.isOpen)
            "The bookcase is evidently a secret door, because it
            has moved sideways to expose a passage to the north. ";
    }
;

+ bookcasePassage: BasicOpenable, HiddenDoor
    'passage' 'passage' "It leads north. "
    initSpecialDesc
    {
        if (isOpen)
            "The bookcase has moved aside to reveal a passage
            to the north. ";
    }
;

+ Decoration 'dusty book/books/tome/tomes' 'books'
    "There must be hundreds of books, all with incomprehensible
    titles and authors you've never heard of. "
    isPlural = true
    dobjFor(Read)
    {
        verify() { }
        action()
        {
            "Much as you enjoy reading, you can't find any book
            whose title even means anything to you, let alone
            holds any interest. ";
        }
    }
;

+ desk: Immovable, Surface
    'massive big huge edifice drab gray grey steel metal desk' 'desk'
    desc
    {
        "It's a big desk with a single drawer (currently
        <<deskDrawer.openDesc>>).  A small button is set inconspicuously
        into the edge of the desktop";
        if (hiddenPanel.isOpen)
            ", and in the side is a small compartment containing a lever";
        ". ";
    }

    dobjFor(Open) remapTo(Open, deskDrawer)
    dobjFor(Close) remapTo(Close, deskDrawer)
    dobjFor(LookIn) remapTo(LookIn, deskDrawer)
    iobjFor(PutIn) remapTo(PutIn, DirectObject, deskDrawer)
;

++ Thing 'bob\'s brand premium fish cleaner jar/cleaner'
    'jar of Bob\'s Brand Premium Fish Cleaner'
    "It presumably once held fish cleaner (whatever that is),
    but it's just an empty jar now. "
;

++penCup: Container 'pen cup' 'pen cup' 
   "It's a uneven clay cup, currently employed as a pen holder. " 

    /* just for fun, show my contents out-of-line in listings I'm in */
    contentsListedSeparately = true
; 

class pen: Thing 'pen*pens' 'pen' 
   "A simple Bic. " 
   isEquivalent = true 
; 

+++pen; 
+++pen; 
+++pen; 
+++pen; 

++ deskDrawer: Component, OpenableContainer 'desk drawer' 'desk drawer'
;

++ Immovable 'old-fashioned rotary phone/telephone/dial/receiver/handset'
    'phone'
    "It's an old-fashioned rotary phone, very sturdy-looking but
    rather scuffed from long use. "

    dobjFor(Take)
    {
        verify() { }
        check() { }
        action()
        {
            "You pick up the handset, but there's nothing but static
            on the line.  At least it stops ringing.  You put the
            handset back, and the phone starts ringing again. ";
        }
    }

    dobjFor(Answer) asDobjFor(Take)
;

+++ Noise 'ring/ringing' 'ringing/bell'
    sourceDesc = "The phone is ringing loudly. "
    descWithSource = "It's an actual mechanical bell, not the ubiquitous
                      electronic warble of modern phones. "
    hereWithSource()
    {
        switch (displayCount)
        {
        case 1:
            "The phone is ringing. ";
            break;

        default:
            "The phone is still ringing. ";
            break;
        }
    }

    displaySchedule = [2, 4, 8]
;


++ Button, Component 'small button' 'small button'
    dobjFor(Push)
    {
        action()
        {
            /* open/close the hidden panel */
            hiddenPanel.makeOpen(!hiddenPanel.isOpen);

            /* show the appropriate message */
            if (hiddenPanel.isOpen)
                "A previously hidden panel in the side of the desk opens,
                revealing a small compartment containing a lever. ";
            else
                "The panel in the desk closes, hiding the lever.  Now
                that the panel is closed, it's completely seamless -
                you can't see any sign of it. ";
        }
    }
;

++ hiddenPanel: BasicOpenable, Component, Container
    'hidden panel/compartment' 'compartment'
    "It's a small compartment, just large enough to contain the
    lever it enclosed. "

    bulkCapacity = 0
    initiallyOpen = nil

    /* the panel is only visible when it's open */
    sightPresence = (self.isOpen)
;

+++ SpringLever, Component 'lever' 'lever'
    "It's mounted in a small compartment in the desk. "
    dobjFor(Pull)
    {
        action()
        {
            /* open/close the secret door */
            bookcasePassage.makeOpen(!bookcasePassage.isOpen);

            /* show the appropriate message */
            if (bookcasePassage.isOpen)
                "The lever is surprisingly heavy, but you manage to
                pull it all the way out.  At the end of its travel,
                something under the floor clicks, and the bookcase
                behind the desk slides sideways far enough to open
                a passage to the north.  As soon as you release the
                lever, it springs back to its original position. ";
            else
                "You pull the lever out all the way.  Something
                under the floor clicks, and the bookcase slides
                sideways until it covers the passage, leaving
                no trace of a doorway. ";
        }
    }
;

++ typewriter: Immovable
    'big old old-fashioned black manual typewriter' 'typewriter'
    initSpecialDesc = "A big, old-fashioned manual typewriter is sitting
        on the desk. "
    desc = "It's big, black, manual typewriter, like something out
        of an old black-and-white movie about newspapermen or private
        detectives.  There's a piece of paper in it. "
    dobjFor(TypeOn) { verify() { } }
    dobjFor(TypeLiteralOn)
    {
        verify() { }
        action()
        {
            /* add the literal text to the paper */
            typewriterPaper.addText(gAction.getLiteral());
            mainReport('With some effort, {you/he} work{s} the keys of the
                ancient machine, noisily impressing <q>'
                + gAction.text_ + '</q> on the paper. ');
        }
    }
;

+++ typewriterPaper: Thing 'piece/paper' 'piece of paper'
    isListedInContents = nil
    desc
    {
        if (textList.length() == 0)
            "The paper is blank. ";
        else
        {
            "The typewritten letters on the page have that uneven
            darkness and wandering alignment characteristic of
            the work a well-worn manual typewriter:<tt>\b";

            foreach (local cur in textList)
                "\t<<cur>>\n";

            "</tt>";
        }
    }
    addText(txt) { textList.append(txt); }
    textList = static new Vector(10)
    moveInto(obj)
    {
        reportFailure('On second thought, {you/he}\'d rather leave the
            paper in the typewriter in case someone needs to do
            some typing. ');
        exit;
    }
    dobjFor(TypeOn) { verify() { } }
    dobjFor(TypeLiteralOn)
    {
        verify()
        {
            /* 
             *   allow it but reduce the likelihood - we want the
             *   typewriter to win in a default case 
             */
            logicalRank(50, 'nondefault');
        }
        action()
        {
            /* treat 'type on paper' as 'type on typewriter' */
            replaceAction(TypeLiteralOn, typewriter, gAction.text_);
        }
    }
;

++ dagger: Thing 'dagger' 'dagger'
    initSpecialDesc = "Someone has jabbed a dagger into the top of
        the desk, leaving the dagger sticking up almost vertically. "
    initExamineDesc = "Its point is stuck into the top of the desk. "
;

++ watch: Wearable 'watch' 'watch' "It has a minute hand and an hour hand. ";
+++ Component 'minute big hand' 'minute hand';
+++ Component 'hour little hand' 'hour hand';

++ Container 'glass bottle' 'bottle'
    bulkCapacity = 2
    canFitObjThruOpening(obj)
    {
        /* we can only fit small objects through the opening */
        return obj.bulk < 2;
    }
;

+++ Thing 'minature model sailing ship/boat' 'model ship'
    "It's a miniature model of a sailing ship. "
    bulk = 2
;

#if 0 // for testing distant viewing of sightSize=large objects
viewScreen: SenseConnector, Fixture 'view screen/viewscreen' 'viewscreen'
    "It's a viewscreen, displaying a close-up shot of a floating sphere. "
    locationList = [den, backYard]
    connectorMaterial: Material
    {
        seeThru = distant
        hearThru = distant
        smellThru = opaque
        touchThru = opaque
    }
;
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Top of stairs 
 */
class SpiralStairway: Fixture
    'perforated central black metal/stair/stairs/stairway/pole' 'stairs'
    "The stairs are narrow triangles of perforated black metal arranged
    in a helix around a central pole. "
    isPlural = true
;

topOfStairs: HouseRoom
    roomName = 'Top of Stairs'
    destName = 'the top of the stairs'
    desc = "This is the top of a narrow spiral staircase that
            leads down a dark shaft walled in rough brown bricks.
            A passage leads south. "
    south = stairPassage
    down = spiralStairsTop
;

+ Decoration 'dark rough brown shaft/brick/bricks' 'dark shaft'
    "The shaft is just large enough for the staircase. "
;

+ stairPassage: ThroughPassage ->bookcasePassage 'south passage' 'passage'
;

+ spiralStairsTop: StairwayDown, SpiralStairway
    moveActor(actor)
    {
        if (actor.isPlayerChar)
            "{You/he} carefully descend{s} the steep, narrow stairs. ";
        inherited(actor);
    }

    travelBarrier = static [
        new tvBarrier('The television is far too heavy to take down
                      the stairs. '),
        new tricycleBarrier('It would make a nice stunt, but it\'s far
                            too dangerous to ride the tricycle down
                            the stairs. ')
    ]
;

/* ------------------------------------------------------------------------ */
/*
 *   Bottom of stairs 
 */
bottomOfStairs: DarkHouseRoom
    roomName = 'Bottom of Stairs'
    destName = 'the bottom of the stairs'
    desc = "This is the bottom of a narrow spiral staircase.
            Apart from the stairs, the only exit is the door
            to the south. "
    up = spiralStairsBottom
    south = stairDoor

    /* no illumination here */
    brightness = 0

    /* 
     *   even in the dark, keep the stairs in scope, because we're
     *   standing on them 
     */
    getExtraScopeItems(actor)
    {
        return inherited(actor) + spiralStairsBottom;
    }
;

class IronDoor: LockableWithKey, Door
    'iron door/rivet/rivets/plate/plates' 'iron door'
    "It's a formidable-looking mass of iron plate and rivets. "

    keyList = [ironKey]
;

+ stairDoor: IronDoor;

+ spiralStairsBottom: StairwayUp, SpiralStairway -> spiralStairsTop
    travelBarrier: tricycleBarrier
    {
        msg_ = 'Riding the tricycle up the stairs is
            simply out of the question. '
    }
;

/* 
 *   make the washing machine a complex containe so that we can have
 *   components as well as contents within 
 */
+ ComplexContainer 'washing machine/washer' 'washing machine'
    "It's a beat-up old washing machine.  A big dial is the only
    obvious control. "

    subContainer: ComplexComponent, OpenableContainer { }
;

++ Component, LabeledDial
    'big washer washing machine control dial' 'control dial'
    "The dial has stops labeled Wash, Rinse, Spin, and Stop.  It's currently
    set to <<curSetting>>. "

    curSetting = 'Wash'
    validSettings = ['Wash', 'Rinse', 'Spin', 'Stop']
;

/* ------------------------------------------------------------------------ */
/*
 *   Dining Room
 */
diningRoom: HouseRoom
    roomName = 'Dining Room'
    destName = 'the dining room'
    desc = "This medium-sized room<<myNote.noteRef>> has a dining table
            and a couple of chairs, one upholstered in dark fabric and
            other in light fabric.
            A door (<<diningDoor.openDesc>>) leads north. "
    south = livingRoom
    north = diningDoor
    out asExit(north)
    myNote: Footnote { "This is just a sample note for the dining room. " }
;

+ alcove: OutOfReach, Fixture, Container 'arched small alcove' 'small alcove'
    "It's a small, arched alcove, set high up on the wall, near the
    ceiling. "

    useSpecialDesc = (contents.length() != 1 || contents[1] != trophy)
    specialDesc = "A small alcove is set near the top of one wall,
                   up near the ceiling. "

    canObjReachContents(obj)
    {
        /* if the actor is standing on a chair, allow it */
        if (obj.posture == standing && obj.location == diningTable)
            return true;

        /* inherit the default */
        return inherited(obj);
    }

    cannotReachFromOutsideMsg(obj)
    {
        return '{You/he} can\'t reach ' + obj.theNameObj
            + ' from here; the alcove is too high up.  {You/he} might
            be able to reach the alcove if {you/he} were standing
            on something. ';
    }
;

++ trophy: Thing 'silver big trophy/award/cup' 'trophy'
    "It's shaped like a big cup, and looks to be made of silver. "

    useInitSpecialDesc = (location == alcove)
    initSpecialDesc = "A trophy is visible in a small alcove set
        into the top of one wall. "
;

+ diningDoor: Door 'door' 'door'
    "It leads north. "
    travelBarrier = [tricycleBarrier, tvBarrier]
;

class DiningChair: Chair, Immovable 'chair*chairs' 'chair'
    "Its style matches that of the table. "
;

+ DiningChair 'light lighter fabric' 'lighter chair';
+ DiningChair 'dark darker fabric' 'darker chair';

+ diningTable: Heavy, Platform 'dining large oval table' 'table'
    "It's a large<<myNote.noteRef>> oval table. "
    myNote: Footnote { "Okay, it's not all that large.  Medium-sized
                        sounds so weak, though. " }
;

class MyCandle: FireSource, Candle
    fuelLevel = 32
    desc()
    {
        /* show a description based on how far we've burned down */
        if (fuelLevel > 30)
            "The candle has barely been used. ";
        else if (fuelLevel > 24)
            "The candle looks only slightly burned down. ";
        else if (fuelLevel > 16)
            "The candle looks about half burned down. ";
        else if (fuelLevel > 8)
            "The candle is considerably burned down. ";
        else if (fuelLevel > 0)
            "The candle is mostly burned down. ";
        else
            "The candle is completely burned down. ";

        /* if we're burning, mention it */
        if (isLit)
            "It's lit. ";
    }
;

++ MyCandle 'red candle' 'red candle';
++ MyCandle 'white candle' 'white candle';

++ vase: Container 'vase' 'vase'
    "It's a simple glass cylinder, open at the top. "
    bulkCapacity = 1
    initSpecialDesc = "A bouquet of flowers is arranged in a vase
        in the center of the table. "
    useInitSpecialDesc()
    {
        /* don't use the initial description if the flowers have been moved */
        return flowers.isIn(self) ? inherited() : nil;
    }
    verifyInsert(obj, newCont)
    {
        if (obj != flowers)
            illogical('The vase is only designed to hold flowers. ');
    }

    /* 
     *   in room and inventory lists, show the vase with its flowers, if
     *   it has the flowers 
     */
    listName = (flowers.isIn(self)
                ? 'a bouquet of flowers in a vase'
                : inherited())
;

+++ flowers: Thing 'flower/flowers/arrangement/bouquet' 'bouquet of flowers'
    "It's a nice arrangement of flowers. "

    /* 
     *   don't separately list the flowers if they're in the vase, because
     *   the vase shows the flowers as part of its list name; but don't
     *   hide the flowers from an explicit "look in vase" 
     */
    isListed = (!isIn(vase))
    isListedIn = true
;

++++ Odor 'floral flowery pleasant fragrance/odor/smell' 'floral fragrance'
    /* the description when they smell the flowers directly */
    sourceDesc = "The flowers have a strong, well, flowery fragrance. "

    /* the description when they smell us and the flowers are visible */
    descWithSource = "The pleasant fragrance is coming from the flowers. "

    /* the description when they smell us and the flowers aren't seen */
    descWithoutSource = "It's a pleasant floral fragrance. "

    /* room description with and without being able to see the flowers */
    hereWithSource = "The flowers have a pleasant fragrance. "
    hereWithoutSource = "A flowery fragrance is in the air. "
;

++ StretchyContainer 'stretchy bag' 'stretchy bag'
    "It's made of some very elastic material.  It seems
    to take on the size and shape of whatever's inside. "

   /* 
    *   we have no bulk contribution of our own when we have any contents,
    *   but we do have a minimum empty bulk of 1 
    */
   bulk = 0
   minBulk = 1
;

++ Container 'cookie tin' 'cookie tin'
    "It's a round metal cylinder about three inches tall, with
    no lid, decorated with pictures of cookies. "

    bulkCapacity = 3
;

++ Thing 'yellow pile inflatable rubber raft' 'inflatable rubber raft'
    desc
    {
        if (inflated)
            "It's fully inflated. ";
        else
            "In its current deflated state, it's just a pile of yellow
            rubber, bunched up messily.  It's large, but you could
            probably inflate it if you had to. ";
    }

    inflated = nil
    bulk { return inflated ? 10 : 2; }

    makeInflated(flag)
    {
        /* check the effect of the inflation on my bulk */
        whatIf({: checkBulkChange()}, &inflated, flag);

        /* set the new status */
        inflated = flag;
    }

    dobjFor(Inflate)
    {
        verify()
        {
            if (inflated)
                illogicalAlready('It\'s already fully inflated. ');
        }
        action()
        {
            makeInflated(true);
            mainReport('It takes all your strength, but you manage to
                blow up the raft.  You might feel light-headed for
                a bit. ');
        }
    }

    dobjFor(Deflate)
    {
        verify()
        {
            if (!inflated)
                illogicalAlready('It\'s already fully deflated. ');
        }
        action()
        {
            makeInflated(nil);
            mainReport('You let the air out of the raft, which flattens
                into a bunched-up pile of yellow rubber. ');
        }
    }
;

/* ------------------------------------------------------------------------ */
/* 
 *   Back Yard 
 */
backYard: OutdoorRoom
    roomName = 'Back Yard'
    destName = 'the back yard'
    vocabWords = 'back yard'
    desc = "This small yard has a well-kept lawn.  The edges of the yard
            are defined by thick, tall hedges.  An old garden shed occupies
            the northwest corner of the yard.  A door into the house
            is to the south. "
    south = yardDoor
    in asExit(south)
    northwest = shedOuterDoor
;
+ yardDoor: Door ->diningDoor 'house back door' 'back door of the house'
    "It leads south, into the house.  "
;

+ Decoration 'shrub/shrubs/hedge/hedges/plant/plants' 'hedges'
    "The hedges are tall and thick, and completely close in the
    boundaries of the yard. "
    isPlural = true
    dobjFor(LookOver)
    {
        verify() { }
        action() { "The hedges are too tall. "; }
    }
;

/*
 *   This object tests and demonstrates using '*' and matchName to parse
 *   non-dictionary words as object names.
 *   
 *   This isn't the kind of place you'd really use '*' in practice; for
 *   this case, you'd be much better off defining all of the possible
 *   color names as adjectives for the object and then using matchName()
 *   to ignore matches to anything but the current one.  This works better
 *   because the color names would then exist as dictionary words, and
 *   hence the parser would not complain about them being unknown when
 *   this object is not in scope.
 */
+ Sphere: Immovable
    noun = '*' 'sphere'
    name = (colors[curColor] + ' sphere')
    desc = "The <<colors[curColor]>> sphere floats unnervingly in mid-air,
            with no apparent mechanical means of suspension.  It does not
            hover like a balloon, but seems absolutely still, as though
            firmly attached to an invisible pole. "
    cannotTakeMsg = 'The sphere is strangely fixed in place; it resists
                     your attempts to move it.  It does seem to be able to
                     rotate freely on its vertical axis, though, so you
                     could probably turn it if you wanted to. '
    cannotMoveMsg = (cannotTakeMsg)
    cannotPutInMsg = (cannotTakeMsg)

    initSpecialDesc = "In the center of the yard, floating in
        mid-air, is a <<colors[curColor]>> sphere. "

    matchNameCommon(origTokens, adjustedTokens)
    {
        /* check each token */
        foreach (local cur in origTokens)
        {
            /* 
             *   if it's 'sphere' or our current color name, match it;
             *   otherwise, we don't have a match 
             */
            if (getTokVal(cur) not in ('sphere', colors[curColor]))
                return nil;
        }

        /* all of our tokens match */
        return self;
    }

    dobjFor(Turn)
    {
        verify() { }
        action()
        {
            ++curColor;
            if (curColor > colors.length())
                curColor = 1;

            "Although the sphere is immovably fixed in space, it rotates
            so freely that it feels weightless.  You give it the lightest
            touch, and it starts spinning wildly, so fast that it becomes
            a featureless, glowing blur.  After a few moments, it abruptly
            comes to a stop, and you see that its color has changed
            to <<colors[curColor]>>. ";
        }
    }

    /* current index in color list */
    curColor = 1

    /* list of possible colors */
    colors = ['red', 'blue', 'green', 'orange', 'yellow', 'white']
;

+ Enterable, Decoration ->yardDoor 'house' 'house'
    "It's a modest one-story house.  A door leads in to the south. "
;

+ Enterable ->shedOuterDoor 'old garden shed' 'shed'
    "The shed's wood siding looks well weathered; only the thinnest
    layer of white paint remains.  Its flimsy-looking door
    is <<shedOuterDoor.openDesc>>. "
;

+ Decoration 'shed\'s wood siding' 'wood siding'
    "It looks weathered. "
;

+ shedOuterDoor: Door 'shed door' 'door of the shed'
    "It leads into the shed.  "
;

/* ------------------------------------------------------------------------ */
/*
 *   Shed 
 */
shed: Room 'Shed' 'the shed'
    "There's barely room to stand up or turn around in this tiny
    storage space.  The north wall has a few deep shelves, but
    otherwise the structure is featureless.  A door leads out
    to the southeast. "
    southeast = shedInnerDoor
    out asExit(southeast)
;

+ shedInnerDoor: Door ->shedOuterDoor 'shed door' 'door'
    "It leads out of the shed. "
;

+ shedShelves: Fixture, Surface 'deep shelf/shelves' 'shelf'
    "The shelves are about a yard deep and look sturdy. "
;

++ Flashlight 'flash light/flashlight' 'flashlight'
    "It's a big lantern-type flashlight.  It's currently <<onDesc>>. "
;

++ Thing 'rat poison/box' 'box of rat poison'
    "It's a large box, missing its top, full of white powder. The
    box is labeled RAT POISON and is marked with a prominent
    skull-and-crossbones symbol, along with a legend in tiny
    type reading <q>Use of this product in a manner inconsistent
    with its labeling is a violation of Adventure Game Law.</q> "

    dobjFor(Eat) remapTo(Eat, ratPoison)
    lookInDesc = "It's full of white powder. "
;

+++ ratPoison: Component, Food 'white powder' 'white powder'
    "The box is full of white powder. "
    cannotTakeMsg = 'You\'d best leave the poison in the box. '
    cannotMoveMsg = 'You\'d best leave the poison in the box. '
    cannotPutMsg = 'Best to leave the poison in the box. '

    dobjFor(Pour)
    {
        verify() { }
        action() { "You'd best leave the poison in its box. "; }
    }
    dobjFor(PourInto) asDobjFor(Pour)
    dobjFor(PourOnto) asDobjFor(Pour)

    dobjFor(Eat)
    {
        preCond = [touchObj]
        verify() { dangerous; }
        action()
        {
            "It's not very tasty, but it sure is deadly. ";

            /* terminate the game */
            finishGameMsg(ftDeath, [finishOptionUndo, finishOptionCredits]);
        }
    }
;

class MyMatch: Matchstick 'match*matches' 'match'
;

++ Matchbook 'match book/matches/matchbook*matches' 'book of matches'
    matchName(origTokens, adjustedTokens)
    {
        /* 
         *   if the name is just 'match', ignore it - we want to allow
         *   'match' as an adjective because we want to match 'match
         *   book', but just plain 'match' is very unlikely to mean us 
         */
        if (origTokens.length() == 1
            && getTokVal(origTokens[1]) == 'match')
            return nil;

        /* inherit default handling */
        return inherited(origTokens, adjustedTokens);
    }

    /* put this after the individual matches when we match a plural */
    pluralOrder = 110
;

+++ MyMatch;
+++ MyMatch;
+++ MyMatch;
+++ MyMatch;
+++ MyMatch;

/* ------------------------------------------------------------------------ */
/*
 *   Additional verbs
 */

DefineTAction(Inflate);
VerbRule(Inflate)
    ('inflate' | 'blow' 'up') dobjList
    | 'blow' dobjList 'up'
    : InflateAction
    verbPhrase = 'inflate/inflating (what)'
;

DefineTAction(Deflate);
VerbRule(Deflate)
    'deflate' dobjList
    : DeflateAction
    verbPhrase = 'deflate/deflating (what)'
;

DefineTAction(LookOver);
VerbRule(LookOver)
    'look' 'over' dobjList
    : LookOverAction
    verbPhrase = 'look/looking (over what)'
;

DefineTAction(Ride);
VerbRule(Ride)
    'ride' singleDobj : RideAction
    verbPhrase = 'ride/riding (what)'
;

DefineTAction(Answer);
VerbRule(Answer)
    'answer' dobjList
    : AnswerAction
    verbPhrase = 'answer/answering (what)'
;

modify Thing
    dobjFor(Inflate)
    {
        preCond = [touchObj]
        verify()
        {
            illogical('{That dobj/he} {is}n\'t something {you/he} can
                inflate. ');
        }
    }
    dobjFor(Deflate)
    {
        preCond = [touchObj]
        verify()
        {
            illogical('{That dobj/he} {is}n\'t something {you/he} can
                deflate. ');
        }
    }
    dobjFor(LookOver)
    {
        preCond = [objVisible]
        verify() { illogical('{You/he} see{s} nothing unusual. '); }
    }
    dobjFor(Ride)
    {
        verify()
        {
            illogical('{That dobj/he} {is}n\'t something {you/he}
                can ride. ');
        }
    }
    dobjFor(Answer)
    {
        preCond = [objVisible]
        verify()
        {
            illogical('{The dobj/he} didn\'t ask {you/him} anything. ');
        }
    }
;

DefineTopicAction(ThinkAbout)
    execAction()
    {
        "You think about <<gTopic.getTopicText()>>. ";
    }
;
VerbRule(ThinkAbout) 'think' 'about' singleTopic : ThinkAboutAction
    verbPhrase = 'think/thinking (about what)'
;

/* ------------------------------------------------------------------------ */
/*
 *   Basement tunnel 
 */
tunnel: DarkRoom 'Tunnel' 'the tunnel'
    "This low, narrow tunnel is walled in old, dark gray concrete
    or something similar.  The walls curve inward at shoulder level to
    form an arched ceiling just barely high enough at the center
    to allow walking without bending your neck.  The passage ends
    to the north at an iron door, and ends at the south in an
    earthen wall. A narrow side passage leads west. "

    north = tunnelDoor
    west = sideTunnel

    /* 
     *   since we have special walls and ceiling that are specifically
     *   included as normal decoration objects in this location, keep only
     *   the default floor among our room parts 
     */
    roomParts = [defaultFloor]
;

+ tunnelCeiling: Decoration
    'arched concrete cement ceiling roof' 'ceiling'
    "It's very low, and arches in from the walls. "
;

+ tunnelWalls: Decoration
    'east west tunnel concrete cement wall/walls' 'tunnel wall'
    "The walls are made of some kind of dark gray concrete. "
;

+ earthenWall: Decoration
    'earthen dirt south wall' 'earthen wall'
    "It's just a wall of dirt, as though construction on the
    tunnel had been abruptly halted here. "
;

+ tunnelDoor: IronDoor ->stairDoor;

/* ------------------------------------------------------------------------ */
/*
 *   Side tunnel 
 */
sideTunnel: DarkRoom 'Jagged Tunnel' 'the jagged tunnel'
    "This is an extremely narrow tunnel; it looks like construction
    was abandoned before it was completed. The passage is jagged,
    but continues a little way to the east.  A rough ladder has
    been improvised from wood scraps and fastened to the wall,
    leading up to a small trap door above. "

    up = tunnelTrapDoor
    east = tunnel

    roomParts = [defaultFloor]

    getExtraScopeItems(actor)
    {
        /* 
         *   if they arrived from above, make sure the trap door and
         *   ladder are in scope 
         */
        if (lastArrivedVia[actor] == tunnelTrapDoor)
            return inherited(actor) + tunnelTrapDoor + tunnelLadder;
        else
            return inherited(actor);
    }

    /* table giving last arrival connector for each actor present */
    lastArrivedVia = static new LookupTable(4, 4)

    travelerArriving(traveler, origin, connector, backConnector)
    {
        /* note the connector by which they're arriving */
        foreach (local actor in traveler.getTravelerActors())
            lastArrivedVia[actor] = backConnector;

        /* inherit default handling */
        inherited(traveler, origin, connector, backConnector);
    }
;

+ tunnelTrapDoor: Fixture, Door
    'small trap door' 'trap door'
;

+ tunnelLadder: StairwayUp 'rough improvised wood ladder/scraps' 'ladder'
    dobjFor(TravelVia) remapTo(TravelVia, tunnelTrapDoor)
;

+ Decoration
    'west north south tunnel dirt jagged wall/walls' 'tunnel wall'
    "The walls are just bare dirt. "
;

/* ------------------------------------------------------------------------ */
/*
 *   A generic USE verb of two objects.  We'll remap this to a more
 *   specific verb when possible, based on the direct object, using the
 *   direct object's remapDobjUse() method.  
 */
DefineTIAction(UseWith)
    /* we want to remap on the direct object, so we must resolve it first */
    resolveFirst = DirectObject
;
VerbRule(UseWith) 'use' singleDobj ('on' | 'with') singleIobj: UseWithAction
    verbPhrase = 'use/using (what) (on what)'
;
VerbRule(UseWithWhat) 'use' singleDobj: UseWithAction
    verbPhrase = 'use/using (what) (on what)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new EmptyNounPhraseProd();
        iobjMatch.responseProd = withSingleNoun;
    }
;

modify Thing
    dobjFor(UseWith) { verify() { } }
    iobjFor(UseWith) { verify() { illogical('Please be more specific. '); }}
;

/*
 *   USE <key> WITH <obj> maps to LOCK/UNLOCK <obj> WITH <key> 
 */
modify Key
    dobjFor(UseWith)
    {
        verify() { verifyIobjLockWith(); }
        preCond() { return preCondIobjLockWith(); }
        remap()
        {
            /* 
             *   If we have a single tentative indirect object, check to
             *   see if it's locked or unlocked, and use the appropriate
             *   verb (LOCK or UNLOCK) to reverse its state.  If we can't
             *   yet identify the indirect object, guess that it's UNLOCK.
             */
            if (gTentativeIobj.length() == 1)
            {
                /* 
                 *   If the object is locked, unlock it; otherwise lock
                 *   it.  Note that we must reverse the dobj/iobj roles in
                 *   the remapped verb, because the key is the direct
                 *   object of USE but the indirect object of LOCK/UNLOCK. 
                 */
                if (gTentativeIobj[1].obj_.isLocked)
                    return [UnlockWithAction, OtherObject, self];
                else
                    return [LockWithAction, OtherObject, self];
            }
            else
            {
                /* guess that it's UNLOCK */
                return [UnlockWithAction, OtherObject, self];
            }
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   "fill x with y" - treat this as "put y in x".  This verb isn't really
 *   useful in this sample game; it's here only as a demonstration of
 *   using the TIAction remapping macros.  
 */
DefineTIAction(FillWith)
    resolveFirst = DirectObject
;

VerbRule(FillWith)
    'fill' singleDobj 'with' iobjList
    : FillWithAction
    verbPhrase = 'fill/filling (what) (with what)'
;

modify Thing
    dobjFor(FillWith) remapTo(PutIn, IndirectObject, self);
;

/* ------------------------------------------------------------------------ */
/*
 *   Main startup object.  This lets us customize the introductory text,
 *   set the player character object, and control the game's startup
 *   procedure.  
 */
gameMain: GameMainDef
    /* the initial (and only) player character is 'me' */
    initialPlayerChar = me

    /* show our introduction message */
    showIntro()
    {
        /* show our simple introduction message */
        "Welcome to the TADS 3 Library Sample Game!\b";
    }

    /* 
     *   Create an "about box".
     *   
     *   The <ABOUTBOX> tag has to be re-displayed each time we clear the
     *   screen, since the tag is part of the main text window's output
     *   stream and thus only lasts as long as the rest of the text on the
     *   main game screen.  Fortunately, the library will take care of this
     *   for us automatically, as long as we do two things.  First, we have
     *   to put our <ABOUTBOX> tag here, in this method, so that the
     *   library knows how to re-display the tag when necessary.  Second,
     *   we must always use the cls() function (NOT the low-level
     *   clearScreen() function) whenever we want to clear the game window.
     */
    setAboutBox()
    {
        "<aboutbox><body bgcolor=black text=white>
        <table height='100%' width='100%' border=0>
        <tr valign=middle align=center><td>
        <font size=5 face=tads-sans>T3 Library Sampler</font><br><br>
        <font face=tads-sans><b>This is the T3 Library Sample Game.  It's
        just an example of how to create a game using the T3 standard
        library (also known as <q>adv3</q>).</b></font>
        </table></aboutbox>";
    }

    /* show our end-of-game message */
    showGoodbye()
    {
        "<.p>Thanks for playing the Library Sample Game!\b";
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   random test verbs
 */

VerbRule(rtfuse) 'test-rtfuse' : IAction
    execAction()
    {
        "Starting a real-time fuse for 10 seconds... ";
        new RealTimeFuse(testFuse, &execEvent, 10000);
    }
;

VerbRule(rtdaemon) 'test-rtdaemon' : IAction
    execAction()
    {
        "Starting a real-time daemon for 10 second... ";
        new RealTimeDaemon(testDaemon, &execEvent, 10000);
    }
;

VerbRule(moreprompt) 'moreprompt' : IAction
    execAction()
    {
        "Pausing for a MORE prompt, leaving the real-time clock running.\n";
        inputManager.pauseForMore(nil);
    }
;

VerbRule(morefreeze) 'morefreeze' : IAction
    execAction()
    {
        "Pausing for a MORE prompt, freezing the real-time clock.\n";
        inputManager.pauseForMore(true);
    }
;

VerbRule(cls) 'cls' : IAction
    execAction()
    {
        "Clearing the screen...";
        cls();
    }
;

VerbRule(dialog) 'dialog' : IAction
    execAction()
    {
        local btn = inputManager.getInputDialog(
            InDlgIconQuestion,
            'What did you have for breakfast this morning?',
            ['&Eggs', '&Cereal', '&Fruit', '&Other'],
            1, 4);
        "You selected button <<btn>>! ";
    }
;

VerbRule(dialog2) 'ldialog' : IAction
    execAction()
    {
        local btn = inputManager.getInputDialog(
            InDlgIconQuestion,
            'Here\'s a much taller dialog.  This one will go on for
            several lines, to make sure that the layout looks right
            with long text contents as well as a short one-liner.
            \n\n
            And this just goes on and on and on, doesn\'t it? Well,
            that\'s the whole idea.  This dialog also uses standard
            buttons instead of the explicitly labeled buttons of the
            dialog you get with the "dialog" command, to test that
            the standard button setup is working properly.  Well,
            that should probably be long enough!!!',
            InDlgOkCancel, 1, 2);
        "You selected button <<btn>>! ";
    }
;

VerbRule(fuse) 'test-fuse' : IAction
    execAction()
    {
        "Starting a fuse for three turns... ";
        new Fuse(testFuse, &execEvent, 3);
    }
;

testFuse: object
    execEvent()
    {
        "<.commandsep>This is the test fuse!<.commandsep>";
    }
;

VerbRule(daemon) 'test-daemon': IAction
    execAction()
    {
        "Starting daemon that will run three times, every other turn... ";
        new Daemon(testDaemon, &execEvent, 2);
        testDaemon.counter = 0;
    }
;

VerbRule(getkey) 'getkey': IAction
    execAction()
    {
        local key;

        /* get a key */
        key = inputManager.getKey(true, {: "<.p>Enter a key:\ " });

        /* show the key */
        "\nThanks!  You typed '<<key>>'!\n";

        nestedActorAction(bob, Take, bigRedBall);
    }
;

VerbRule(showlinks) 'showlinks': IAction
    execAction()
    {
        "Here are some links:
        <ul>
        <li><font color=green>color <<
          aHref('outside', 'outside')>> the link</font>
        <li><font color=purple>color <<
          aHref('inside', '<font color=red>inside</font>')>> the link</font>
        <li><font color=navy>a mix of <<
          aHref('mix', 'outside <font color=#ff8000>and inside</font> the link')
          >> for extra completeness</font>
        </ul> ";
    }
;

testDaemon: object
    execEvent()
    {
        "\bThis is the test daemon! ";

#if 0
        "<.p>*** You have died ***\n";
        finishGame([finishOptionUndo, finishOptionCredits,
                    finishOptionFullScore]);
#endif

        /* if I've run three times now, cancel myself */
        if (++counter == 3)
            eventManager.removeCurrentEvent();
    }
    counter = 0
;

VerbRule(nbspTest) 'nbsptest' : IAction
    execAction()
    {
        "(This test is designed for 80-column displays.)
        \b
This is a long line intended to wrap after a bit.  The trick is that
here&nbsp;to&nbsp;here (i.e., the parts between the first and second 'here')
should be non-breaking: 'here&nbsp;to&nbsp;here' should all be on one line,
even though there are spaces between the words.  Anyway, so much for
the test. ";
    }
;

VerbRule(wrapTest) 'wraptest' : IAction
    execAction()
    {
        "(This test is designed for 80-column displays.)
        \b
This is a long line intended to wrap after.\u2002First, let's test soft
hy&shy;phen&shy;ation.
Okay, we should have hyphenated somewhere in there.
\b
Next, let us make sure that regular hyphen wrapping still works.  What we need
is for this line to wrap <b>after</b> 'need'.\n
Next, let us make sure that regular hyphen wrapping still works.  What we
need&nbsp;-- and this is key&nbsp;-- is to make sure this line wraps
<b>before</b> 'need'.\n
Next, let us make sure that regular hyphens still work.  This time use
hard-hyphen-wrapping, where we break at a hyphen in the middle of a word.
\b
Now we can test some new stuff.  First, test the explicit break flag:
We\u200bCan\u200bBreak\u200bAt\u200bAny\u200bOf\u200bThese";
"\u200bInter\u200bCaps\u200bCharacters\u200bAnd\u200bWe\u200bCan\u200bGo";
"\u200bOn\u200bLike\u200bThis\u200bFor\u200bQuite\u200bSome\u200bTime";
"\u200bEven\u200bTo\u200bThe\u200bExtent\u200bOf\u200bWrapping\u200bAnother";
"\u200bLine\u200bOf\u200bText!
\b
Next, let's test that explicit non-break points are working.  We'll want
to\ufeff \ufeffbreak just before the previous word 'break', but we have a
non-breaking zero-width space after the 'to' and another before the 'break',
which should prevent breaking there.\n
Likewise, we'll want to break this line at a hyphen, coming up right
here-\ufeffabsolutely.  But we have a non-break right after the hyphen,
which should tell us not to break there.\n
For comparison, this line we do want to break at the hyphen, which is
here-absolutely once again.  This time we should break after the hyphen.
\b
The next test is for East Asian language wrapping.<wrap char>WeAreNowIn";
"CharacterWrapMode,WhichMeansThatWeCanWrapBetweenAnyTwoCharactersExcept";
"WhereOtherwiseNotedWithAnExplicitNonBreakingIndicator\ufeff.<wrap word>This
is back in English-style text, which means we should wrap on word
boundaries.<wrap char>AndNowWe'reBackInCharacterWrappingMode.TheTrickIsThat";
"WeWantToCheckTransitionsBetweenTheTwoModes.<wrap word>Such as here, where
we are back in word mode.<wrap char>OrHereWithCharInTheMiddleOfALine.";
"<wrap word>We're back in word-wrap mode here.\u2002The remaining test is
to put word-wrap mode in the middle of a line of<wrap char>CharacterWrapped";
"Text,SuchAsThis<wrap word>(here is some word-wrapped text)<wrap char>AndNow";
"WeAreBackToCharacterWrappedTextToTheEndOfTheLine.ATestWeHaveYetToTryWith";
"CharWrappedTextIs\ufeff.\ufeff.\ufeff.ExplicitNonBreakingIndicators.
<wrap word>(That ellipsis should be all together, not broken across lines,
and should furthermore stick to the letter just before it, so the ellipsis
doesn't start a new line.)\n
\b
This is just a test of <u>underlining some typographical spaces</u>:
let's try <u>an en space:\u2002an em space:\u2003a thin space:\u2009a
punctuation space:\u2008a figure space:\u2007a hair space:\u200a...and
that</u> should do it.\n
<font color=black bgcolor=yellow>
And now let's try something similar:\u2002
ending the line with an en plus an em:\u2002\u2003This is to demonstrate
that we trim trailing whitespace, even if the whitespace consists of
typographical spaces.</font>\n
<font color=black bgcolor=aqua>
Another similar test:\u2002This time, some non-breaking
spaces:\uFEFF\u2002\uFEFF\u2003\uFEFF\n
That ended with an en plus em again, but these were quoted with FEFF's.
</font>\n
\b
<p><table border width=50% align=center>
<tr><td><wrap char>ThisIsSomeTextInATableInCharacterWrapMode.TheIdeaIsTo";
"SeeHowTablesDealWithThisSortOfThing.TablesUseTheBasicWrappingMechanism";
"LikeEverythingElse,ButHaveSomeSpecialCodeToMeasureTheSizeOfTheTextThat";
"WillGoInThem.<wrap word>
<td>This is a second column that uses character-based wrapping instead
of word-based wrapping.  The two columns should more or less balance
out, although the word-based column will want a bigger share because
of its larger minimum width.
</table>\n";
    }
;

VerbRule(EventTest) 'inputevent' : IAction
    execAction()
    {
        local evt = inputManager.getEvent(true, {: "Waiting for event..." });

        " Got it!\n";
        local name = [InEvtKey -> 'key', InEvtHref -> 'href',
                      InEvtEof -> 'eof'][evt[1]];
        "Event type=<<name>> (<<evt[1]>>), param=<<
          evt.length() > 1 ? evt[2] : ''>> ";
    }
;


VerbRule(logAbout) 'logabout' : IAction
    execAction()
    {
        "Logging the ABOUT command to About.txt...\n";
        LogConsole.captureToFile('About.txt',
                                 getLocalCharSet(CharsetDisplay), 80,
                                 {: AboutAction.execSystemAction() });
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A rather contrived preparser.
 *   
 *   This preparser allows the player to set "aliases" using syntax like
 *   this:
 *   
 *   alias $xxx yyy
 *   
 *   where xxx is the name of an alias variable, and yyy is the
 *   replacement text for the variable.  Once an alias has been created,
 *   it can be used in any command simply by naming it with the '$ prefix.
 */
StringPreParser
    /* pre-compile our search patterns */
    patDefAlias = static new RexPattern(
        '<space>*alias<space>+<dollar>(<alphanum>+)<space>+(.+)')
    patUseAlias = static new RexPattern(
        '(<^dollar>*)<dollar>(<alphanum>+)(.*)')

    doParsing(str, which)
    {
        local ret;
        local aliasName;
        local aliasText;
        
        /* check for the alias definition syntax */
        if (rexMatch(patDefAlias, str) != nil)
        {
            /* extract the alias name and the replacement text */
            aliasName = rexGroup(1)[3];
            aliasText = rexGroup(2)[3];

            /* add the alias to our table */
            aliasTable[aliasName] = aliasText;

            /* mention that the alias has been defined */
            "Okay, $<<aliasName>> has been defined. ";

            /* this command has been fully handled */
            return nil;
        }

        /* we're not defining an alias, so look for aliases to replace */
        ret = nil;
        for (;;)
        {
            /* look for a '$' in the balance of the input string */
            if (rexMatch(patUseAlias, str) != nil)
            {
                local prv;
                local nxt;
                
                /* 
                 *   extract the text before the alias, the name of the
                 *   alias, and the rest of the text after the alias 
                 */
                prv = rexGroup(1)[3];
                aliasName = rexGroup(2)[3];
                nxt = rexGroup(3)[3];

                /* add the previous text to the output string so far */
                if (ret != nil)
                    ret += prv;
                else
                    ret = prv;

                /* 
                 *   translate the alias; if it has no translation, then
                 *   use the original text of the alias, including the
                 *   '$', unchanged 
                 */
                aliasText = aliasTable[aliasName];
                if (aliasText == nil)
                    aliasText = '$' + aliasName;

                /* add the translated alias to the output string so far */
                ret += aliasText;

                /* continue parsing the balance of the input string */
                str = nxt;
            }
            else
            {
                /* 
                 *   We have no more aliases in the string - the balance
                 *   of the string is simply to be returned unchanged.  If
                 *   we've already built some output text, add the balance
                 *   of the input to the output so far; otherwise, the
                 *   balance of the input is the entirety of the output.  
                 */
                if (ret != nil)
                    ret += str;
                else
                    ret = str;

                /* we're done with the text */
                break;
            }
        }

        /* return the output string */
        return ret;
    }

    /* our lookup table of defined aliases */
    aliasTable = static new LookupTable()
;

