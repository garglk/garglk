#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - US English header
 *   
 *   This file defines common properties, macros, and other identifiers used
 *   throughout the US English modules of the TADS 3 library.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Vocabulary properties for parts of speech we use in the English parser.
 *   
 *   These properties are defined in the English-specific header because
 *   other languages might classify their parts of speech differently; for
 *   example, a French parser might distinguish nouns according to gender,
 *   and thus require masculineNoun and feminineNoun properties instead of a
 *   single noun property.
 *   
 *   adjApostS is a special adjective form for adjectives that are
 *   explicitly defined in vocabulary with a "'s" suffix.  In most cases,
 *   it's not necessary to explicitly define possessive words explicitly in
 *   an object's vocabulary, because the parser's possessive qualifier
 *   phrase grammar allows any valid noun phrase to be used in a 's
 *   possessive form to indicate ownership.  However, this only works when
 *   ownership is actually modeled in the game; that is, this only works
 *   when one object has an ownership relation with another object, using
 *   the owner/isOwnedBy mechanism in Thing.  Sometimes, a possessive word
 *   is just a literal part of an object's name, and the nominal owner
 *   doesn't actually exist as an object in the game: "Mom's Old-Fashioned
 *   Robot Oil," for example.  In these cases, the object can include the
 *   possessive word *without* the 's suffix ("Mom") as a vocabulary word
 *   for the object, using the adjApostS property.
 *   
 *   A literalAdjective is like an adjective, but behaves a little
 *   differently in the grammar.  A literal adjective can be optionally
 *   enclosed in quotes (for example, we could refer to an elevator button
 *   labeled "G" as "'G' button" or simply "G button").  In addition, a
 *   literal adjective can be placed after the noun it modifies (hence
 *   "button 'G'" or "button G").    
 */
dictionary property noun, adjective, plural, adjApostS, literalAdjective;


/*
 *   Numeric parts of speech:
 *   
 *   digitWord - a name for a digit (zero through nine)
 *   
 *   teenWord - a name for a 'teen' (ten through nineteen) - we distinguish
 *   these because there's no general grammatical rule for constructing
 *   names for these numbers out of smaller lexical units.  We include ten
 *   in this set, rather than in the teenWord set, because ten cannot be
 *   combined with digit words to form other numbers.
 *   
 *   tensWord - a name for a multiple of ten (twenty, thirty, ..., ninety).
 *   The words in this set can be used to construct the numbers from 20 to
 *   99 by combining them with digit words using a hyphen (as in
 *   "twenty-two").
 *   
 *   ordinalWord - an ordinal, such as '1st' or 'first'.  
 */
dictionary property digitWord, teenWord, tensWord;
dictionary property ordinalWord;


/* ------------------------------------------------------------------------ */
/*
 *   Templates for the basic vocabulary object 
 */
VocabObject template 'vocabWords';

/*
 *   Define some templates that apply to ordinary objects (descendants of
 *   Thing). 
 */
Thing template 'vocabWords' 'name' @location? "desc"?;

/*
 *   For rooms, we normally have no vocabulary words, but we do have a name
 *   and description, and optionally a "destination name" to use to describe
 *   connectors from adjoining rooms.  
 */
Room template 'roomName' 'destName'? 'name'? "desc"?;

/*
 *   For actors, we generally override the npcDesc or pcDesc rather than the
 *   base desc.  For the standard templates, set the npcDesc, since most
 *   actors are NPC's rather than the player character.  
 */
Actor template 'vocabWords' 'name' @location? "npcDesc";

/*
 *   For passages, allow special syntax to point to the master side of the
 *   passage. 
 */
Passage template ->masterObject inherited;

/* 
 *   For one-way room connectors, provide special syntax to point to the
 *   destination room. 
 */
OneWayRoomConnector template ->destination;

/*
 *   For enterables, allow special syntax to point to the connector which an
 *   actor uses to traverse into the enterable. 
 */
Enterable template ->connector inherited;
Exitable template ->connector inherited;

/*
 *   For TravelMessage connectors, provide special syntax to specify the
 *   message and point to the destination. 
 */
TravelMessage template ->destination "travelDesc" | [eventList];

/*
 *   For connectors that don't go anywhere but do show a message on a travel
 *   attempt, we just need to specify the travel message. 
 */
NoTravelMessage template "travelDesc" | [eventList];
DeadEndConnector template 'apparentDestName'? "travelDesc" | [eventList];

/* 
 *   Unthings are defined pretty much like Things, except they have no use
 *   for a desc, and frequently want a custom notHereMsg.  
 */
Unthing template 'vocabWords' 'name' @location? 'notHereMsg'?;

/* ThingState objects */
ThingState template 'listName_' +listingOrder?;



/* ------------------------------------------------------------------------ */
/*
 *   Convenience macros for defining command grammar.
 *   
 *   A command's grammar is defined via a 'grammar' rule definition for the
 *   'predicate' production; the VerbRule macro can be used for better
 *   readability.
 *   
 *   Within a command grammar rule, there are several commonly-used object
 *   roles assigned to single-noun or noun-list phrases.  We provide the
 *   singleDobj, dobjList, singleIobj, and iobjList macros to make these
 *   assignments more readable.  In addition, number, topic, literal, and
 *   direction phrases can be assigned with singleNumber, singleTopic,
 *   singleLiteral, and singleDir, respectively.  
 */

#define VerbRule(tag)  grammar predicate(tag):
#define singleDobj     singleNoun->dobjMatch
#define singleIobj     singleNoun->iobjMatch
#define dobjList       nounList->dobjMatch
#define iobjList       nounList->iobjMatch
#define singleNumber   numberPhrase->numMatch
#define singleTopic    topicPhrase->topicMatch
#define singleLiteral  literalPhrase->literalMatch
#define singleDir      directionName->dirMatch


/* ------------------------------------------------------------------------ */
/*
 *   Convenience macros for controlling the narrative tense.
 */

/*
 *   Set the current narrative tense.  Use val = true for past and
 *   val = nil for present.
 */
#define setPastTense(val) (gameMain.usePastTense = (val))

/*
 *   Shorthand macro for selecting one of two values depending on the
 *   current narrative tense.
 */
#define tSel(presVal, pastVal) \
    (gameMain.usePastTense ? (pastVal) : (presVal))

/*
 *   Temporarily override the current narrative tense and invoke a callback
 *   function.
 */
#define withPresent(callback) (withTense(nil, (callback)))
#define withPast(callback)    (withTense(true, (callback)))
