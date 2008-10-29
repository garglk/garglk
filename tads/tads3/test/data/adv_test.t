#include "adv3.h"

object template "sDesc";
object template 'sDescStr';
object template 'sDescStr' "lDesc";
object template "sDesc" "lDesc";

/*
 *   List group for encyclopedia volumes.  We want the listing to come out
 *   like this: volumes I, III, and VII of the Encyclopedia Tadsica.  Note
 *   that we want the volumes in ascending order of volume number.  
 */
EncycListGroup: ListGroup
    showGroupList(pov, lister, lst, options, indent, infoList)
    {
        /* put the list in sorted order of volume */
        lst = lst.sort(nil, {a, b: a.volumeNum - b.volumeNum});

        /* check for wide/tall mode */
        if ((options & LIST_TALL) != 0)
        {
            "<<spellInt(lst.length())>> volumes of the Encyclopedia
            Tadsica\n";
            showListSimple(pov, lister, lst, options | LIST_GROUP,
                           indent + 1, 0, infoList);
        }
        else
        {
            "volumes ";
            showListSimple(pov, lister, lst, options | LIST_GROUP,
                           indent + 1, 0, infoList);
            " of the Encyclopedia Tadsica";
        }
    }

    /* 
     *   because of our phrasing, we don't need to set off our group
     *   sublist from items in the enclosing list 
     */
    groupDisplaysSublist = nil
;

class EncycVolume: Thing
    /* my volume number, as an integer */
    volumeNum = 0

    /* list with other volumes */
    listWith = EncycListGroup

    /* use a roman numeral to display my volume number */
    sDescStr
    {
        return 'Encyclopedia Tadsica volume '
             + intToRoman(volumeNum).toUpper();
    }

    /* don't display any articles */
    aDesc { sDesc; }
    theDesc { sDesc; }

    /* when I'm listed in a group, just show my volume number */
    groupListDesc(options, pov, senseInfo)
    {
        say(intToRoman(volumeNum).toUpper());
    }
;

/*
 *   Define a group lister for coins.  This one's a bit complicated: when
 *   we list the group, we want to add up the dollars-and-cents value of
 *   the coins, and then we want to sort the coins in descending order of
 *   value (more valuable coins first).  
 */
CoinListGroup: ListGroup
    showGroupList(pov, lister, lst, options, indent, infoList)
    {
        local total;
        
        /* count up the amount of money we have */
        total = 0;
        foreach (local cur in lst)
            total += cur.value;

        /* show the monetary value */
        if (total < 100)
        {
            /* it's less than a dollar - spell out the number of cents */
            "<<spellInt(total)>> cents in coins";
        }
        else
        {
            local cents;
            
            /* it's over a dollar - show like $1.25 */
            "$<<total/100>>.";

            /* get the cents */
            cents = toString(total % 100);

            /* make sure we show a leading zero if it's only one digit */
            if (cents.length() == 1)
                cents = '0' + cents;

            /* show it */
            "<<cents>> in change";
        }

        /* sort the list by descending monetary value */
        lst = lst.sort(true, {coin1, coin2: coin1.value - coin2.value});

        /* show the appropriate wide/tall separator */
        if ((options & LIST_TALL) != 0) "\n"; else " (";

        /* show the sublist */
        showListSimple(pov, lister, lst, options | LIST_GROUP, indent + 1,
                       0, infoList);

        /* end the sublist */
        if ((options & LIST_TALL) == 0) ")";
    }

    /* we parenthesize our list, so we don't introduce a sublist */
    groupDisplaysSublist = nil
;

class Coin: Thing
    isEquivalent = true
    sDescStr = 'coin'
    listWith = CoinListGroup

    /* the monetary value of the coin, in cents */
    value = 0
;

class DollarCoin: Coin
    sDescStr = 'dollar coin'
    value = 100
;

class QuarterCoin: Coin
    sDescStr = 'quarter'
    value = 25
;

class DimeCoin: Coin
    sDescStr = 'dime'
    value = 10
;

class NickelCoin: Coin
    sDescStr = 'nickel'
    value = 5
;

class PennyCoin: Coin
    sDescStr = 'penny'
    value = 1
;

dirtyGlass: Material
    seeThru = obscured
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

window: Connector, Thing 'window'
    locationList = [room1, myCar]
    connectorMaterial = glass
    initDesc = "A window lets you see into a nearby car. "
;

dirtyWindow: Connector, Thing 'dirty window'
    locationList = [room1, room3]
    connectorMaterial = dirtyGlass
;

room1: DarkRoom "Room A"
    "This room was obviously constructed in a hurry and without much
    forethought: the walls are at odd angles and don't quite meet at
    the seams; pieces of debris are scattered all about; many of the
    nails haven't been pounded down all the way. "
;

+ Container 'dark glass sphere'
    isOpen = nil
    material = dirtyGlass
;

++ Thing
    sDescStr
    {
        local info;

        /* get my visual sense information */
        info = getVisualSenseInfo();

        /* change the name depending on how well lit we are */
        if (info == nil || info.ambient < 3)
            return 'shiny object';
        else
            return 'silver key';
    }
;

+ chair: Thing 'chair';

++ me: Actor 'me';

+++ torch: LightSource 'torch'
    isLit = nil
    brightnessOn = 4
;

+++ QuarterCoin;
+++ quarter3: QuarterCoin;
+++ quarter4: QuarterCoin;
+++ QuarterCoin;
+++ QuarterCoin;
+++ DimeCoin;
+++ NickelCoin;
+++ NickelCoin;
+++ penny1: PennyCoin;
+++ penny2: PennyCoin;
+++ PennyCoin;

+++ candle: LightSource 'candle'
    isLit = nil
    brightnessOn = 2
;

+++ EncycVolume volumeNum = 4;

+ quarter1: QuarterCoin;
+ PennyCoin;
+ quarter2: QuarterCoin;

+ Surface 'desk';

++ EncycVolume volumeNum = 3;
++ EncycVolume volumeNum = 7;
++ EncycVolume volumeNum = 2;
++ EncycVolume volumeNum = 6;
++ EncycVolume volumeNum = 5;

+ PennyCoin;
+ PennyCoin;

room2: DarkRoom "Room B";

+ myCar: Container 'car'
    isOpen = nil
;

++ seatbelt: Thing 'seatbelt';

++ clock: Thing 'LED clock' aDesc = "an LED clock" brightness=1;

+ box: OpenableContainer 'box'
    isOpen = true
;

++ book: Thing 'book';

room3: DarkRoom "Room C";

+ abacus: Thing 'abacus' brightness=1;

+ flashlight: LightSource 'flashlight'
    isLit = nil
;

+ cabinet: Container 'cabinet'
    isOpen = nil
    material = dirtyGlass
;

++ calculator: Thing 'calculator';

++ dime: Thing 'dime' seeSize = small;

/*
 *   An Achievement object for earning money 
 */
moneyAchievement: Achievement
    achieve(points, item)
    {
        /* add the item to our money list */
        moneyList += item;

        /* add myself to the score */
        addToScore(points, self);
    }
    
    lDesc
    {
        "finding ";
        showListAll(plainLister, moneyList, 0, 0);
    }

    /* the list of items for which I've been achieved */
    moneyList = []
;

main(args)
{
    /* set the player character */
    setPlayer(me);
   
    "<.roomname>Room Name Test<./roomname>\b";

    /* look around in the dark */
    room1.lookAround(me, true);
    "\b";

    /* do some point-to-point tests */
    "me->chair: <<sayTrans(me.senseObj(sight, chair))>>\n";
    "me->box: <<sayTrans(me.senseObj(sight, box))>>\n";
    "me->book: <<sayTrans(me.senseObj(sight, book))>>\n";
    "me->car: <<sayTrans(me.senseObj(sight, myCar))>>\n";
    "me->seatbelt: <<sayTrans(me.senseObj(sight, seatbelt))>>\n";
    "me->clock: <<sayTrans(me.senseObj(sight, clock))>>\n";
    "me->abacus: <<sayTrans(me.senseObj(sight, abacus))>>\n";
    "me->calculator: <<sayTrans(me.senseObj(sight, calculator))>>\n";
    "me->dime: <<sayTrans(me.senseObj(sight, dime))>>\n";
    "abacus->calculator: <<sayTrans(abacus.senseObj(sight, calculator))>>\n";

    "\b";

    /* construct some lists of everything visible from a given point */
    "visible from me: <<sayList(me.senseList(sight))>>\n";
    "visible from Room C: <<sayList(room3.senseList(sight))>>\n";
    "visible from calculator: <<sayList(calculator.senseList(sight))>>\n";

    /* construct a list of everything in scope for me */
    "in scope for me: <<sayList(me.scopeList())>>\n";

    "\b";

    /* turn on the flashlight and see what we can see */
    flashlight.isLit = true;
    "After lighting the flashlight:\n
    visible from me: <<sayList(me.senseList(sight))>>\n";

    "\b";

    /* turn on the candle and see what we can see */
    candle.isLit = true;
    "After lighting the candle:\n
    visible from me: <<sayList(me.senseList(sight))>>\n";

    "\b";

    /* turn off the flashlight and see what we can see */
    flashlight.isLit = nil;
    "After turning the flashlight off again:\n
    visible from me: <<sayList(me.senseList(sight))>>\n";

    "\b";

    /* show the room description */
    room1.lookAround(me, true);
    "\b";

    /* show the player character inventory */
    me.showInventory(nil);
    "\b";

    /* show a tall inventory */
    me.showInventory(true);
    "\b";

    /* turn on the torch */
    torch.isLit = true;
    "After lighting the torch:\n";
    
    /* show the room description */
    room1.lookAround(me, true);
    "\b";

    /* make the torch brighter */
    torch.brightnessOn++;
    "After increasing the torch's brightness:\n";
    
    /* show the room description */
    room1.lookAround(me, true);
    "\b";

    /* show the score */
    showFullScore();
    "\b";

    /* add some points, then show the score again */
    moneyAchievement.achieve(2, quarter1);
    addToScore(10, 'turning on the flashlight');
    addToScore(3, 'finding various treasures');
    addToScore(5, 'climbing the ladder');
    addToScore(1, 'lighting the torch');
    addToScore(9, 'finding various treasures');
    moneyAchievement.achieve(2, quarter2);
    moneyAchievement.achieve(1, penny2);

    /* while we're at it, set up a ranking table */
    libMessages.scoreRankTable =
        [
            [0, 'a novice adventurer'],
            [30, 'an intermediate adventurer'],
            [70, 'an advanced adventurer'],
            [100, 'a master adventurer']
        ];

    /* show the full score */
    showFullScore();
    "\b";
}

sayTrans(trans)
{
    /* note the transparency level */
    switch(trans[1])
    {
    case opaque:
        "opaque";
        break;

    case transparent:
        "transparent";
        break;

    case obscured:
        "obscured";
        break;

    case distant:
        "distant";
        break;

    default:
        "???";
        break;
    }

    /* note the obstructor */
    if (trans[2] != nil)
        " - obstructed by <<trans[2].sDesc>>";

    if (trans[3] != nil)
        " (light level <<trans[3]>>)";
}

sayList(lst)
{
    local i;
    
    "[";
    i = 0;
    foreach (local cur in lst)
    {
        if (i++ != 0)
            ", ";
        cur.sDesc;
    }
    "]";
}

