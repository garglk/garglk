#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - main header
 *   
 *   This file provides definitions of macros, properties, and other
 *   identifiers used throughout the library and in game source.
 *   
 *   Each source code file in the library and in a game should generally
 *   #include this header near the top of the source file.  
 */

#ifndef ADV3_H
#define ADV3_H

/* ------------------------------------------------------------------------ */
/*
 *   Include the system headers that we depend upon.  We include these here
 *   so that each game source file will pick up the same set of system
 *   headers in the same order, which is important for intrinsic function
 *   set definitions.  
 */
#include <tads.h>
#include <tok.h>
#include <t3.h>
#include <vector.h>
#include <strbuf.h>
#include <file.h>
#include <dict.h>


/* ------------------------------------------------------------------------ */
/*
 *   Establish the default dictionary for the game's player command parser
 *   vocabulary.
 */
dictionary cmdDict;

/* ------------------------------------------------------------------------ */
/*
 *   canInherit - determine if there's anything to inherit from the current
 *   method.  Returns true if there's a method to inherit, nil if
 *   'inherited' in the current context would not invoke any code.  
 */
#define canInherit() \
    propInherited(targetprop, targetobj, definingobj, PropDefAny)


/* ------------------------------------------------------------------------ */
/*
 *   Generic part of speech for "miscellaneous word."  We use this to
 *   classify words in unstructured phrases; it can apply to any token.
 *   Note that dictionary entries are never made with this word type, so
 *   it's not specific to any language; this is merely for flagging words in
 *   unstructured phrases in grammar matches.  
 */
dictionary property miscWord;

/*
 *   Generic part of speech for SpecialTopic words.  We enter special topic
 *   keywords into the parser dictionary so that they're not flagged as
 *   unknown words if they're used out of context.  
 */
dictionary property specialTopicWord;


/* ------------------------------------------------------------------------ */
/*
 *   If we're compiling for debugging, automatically include the parser
 *   debug code, which allows certain information on the parsing process
 *   (such as grammar match trees) to be displayed each time a command is
 *   typed.
 *   
 *   Note that you can turn on parser debugging independently of full
 *   compiler debug information simply by explicitly defining PARSER_DEBUG
 *   (with the t3make -D option, for example).  
 */
#ifdef __DEBUG
#define PARSER_DEBUG
#endif

/*
 *   Define some convenience macros for parser debug operations.  When
 *   PARSER_DEBUG isn't defined, these macros expand out to nothing.  
 */
#ifdef PARSER_DEBUG

#define dbgShowGrammarList(lst) showGrammarList(lst)
#define dbgShowGrammarWithCaption(headline, match) \
    showGrammarWithCaption(headline, match)

#else /* PARSER_DEBUG */

#define dbgShowGrammarList(lst)
#define dbgShowGrammarWithCaption(headline, match)

#endif /* PARSER_DEBUG */


/* ------------------------------------------------------------------------ */
/*
 *   Parser global variables giving information on the command currently
 *   being performed.  These are valid through doAction processing.  These
 *   should never be changed except by the parser.
 */

/* the actor performing the current command */
#define gActor (libGlobal.curActor)

/*
 *   For convenience, define some macros that return the current direct and
 *   indirect objects from the current action.  The library only uses direct
 *   and indirect objects, so games that define additional command objects
 *   will have to add their own similar macros for those.  
 */
#define gDobj (gAction.getDobj())
#define gIobj (gAction.getIobj())

/* 
 *   Get the current ResolvedTopic, and the literal text of the topic phrase
 *   as the user typed it (but converted to lower case).  These are
 *   applicable when the current action has a topic phrase.  
 */
#define gTopic (gAction.getTopic())
#define gTopicText (gTopic.getTopicText.toLower())

/* get the current literal phrase text, when the command has one */
#define gLiteral (gAction.getLiteral())

/*
 *   The tentative pre-resolution lists for the direct and indirect objects.
 *   When we're resolving an object of a multi-object command, these
 *   pre-resolution lists are available for the later-resolved objects.
 *   
 *   Note that these values are list of ResolveInfo objects.  The obj_
 *   property of a list entry gives the entry's game-world object.
 *   
 *   These lists do not provide the final resolution lists for the objects;
 *   rather, they provide a tentative set of possibilities based on the
 *   information that's available without knowing the results of resolving
 *   the earlier-resolved objects yet.
 *   
 *   These are not meaningful when resolving single-object actions.  
 */
#define gTentativeDobj (gAction.getTentativeDobj())
#define gTentativeIobj (gAction.getTentativeIobj())

/* 
 *   the actor who *issued* the current command (this is usually the player
 *   character, because most commands are initiated by the player on the
 *   command line, but it is also possible for one actor to send a command
 *   to another programmatically) 
 */
#define gIssuingActor (libGlobal.curIssuingActor)

/* the Action object of the command being executed */
#define gAction (libGlobal.curAction)

/*
 *   Determine if the current global action is the specified action.  Only
 *   the action prefix is needed - so use "Take" rather than "TakeAction"
 *   here.
 *   
 *   This tests to see if the current global action is an instance of the
 *   given action class - we test that it's an instance rather than the
 *   action class itself because the parser creates an instance of the
 *   action when it matches the action's syntax.  
 */
#define gActionIs(action) \
    (gAction != nil && gAction.actionOfKind(action##Action))

/* is the current global action ANY of the specified actions? */
#define gActionIn(action...) \
    (gAction != nil \
     && (action#foreach/gAction.actionOfKind(action##Action)/||/))

/* the verification results object - this is valid during verification */
#define gVerifyResults (libGlobal.curVerifyResults)

/* the command transcript object - this is valid during command execution */
#define gTranscript (mainOutputStream.curTranscript)

/*
 *   Some message processors add their own special parameters to messages,
 *   because they want to use expansion parameters (in the "{the dobj/him}"
 *   format) outside of the set of objects directly involved in the command.
 *   
 *   The Action method setMessageParam() lets you define such a parameter,
 *   but for convenience, we define this macro for setting one or more
 *   parameters whose names exactly match their local variable names.  In
 *   other words, if you call this macro like this:
 *   
 *   gMessageParams(obj, cont)
 *   
 *   then you'll get one parameter with the text name 'obj' whose expansion
 *   will be the value of the local variable obj, and another with text name
 *   'cont' whose expansion is the value of the local variable cont.  
 */
#define gMessageParams(var...) \
    (gAction.setMessageParams(var#foreach/#@var, var/,/))

/*
 *   Synthesize a global message parameter name for the given object and
 *   return the synthesized name.  This is useful in cases where there might
 *   be repeated instances of global message parameters in the same action,
 *   so it's not safe to use a fixed name string for the object.  We'll
 *   create a unique global message parameter name, associate the object with
 *   the name, and return the name string.  
 */
#define gSynthMessageParam(var) (gAction.synthMessageParam(var))


/* ------------------------------------------------------------------------ */
/*
 *   Miscellaneous macros
 */

/* get the current player character Actor object */
#define gPlayerChar (libGlobal.playerChar)

/* 
 *   the exit lister object - if the exits module isn't included in the
 *   game, this will be nil 
 */
#define gExitLister (libGlobal.exitListerObj)

/*
 *   the hint manager object - if the hints module isn't included in the
 *   game, this will be nil 
 */
#define gHintManager (libGlobal.hintManagerObj)


/* ------------------------------------------------------------------------ */
/*
 *   The current library messages object.  This is the source object for
 *   messages that don't logically relate to the actor carrying out the
 *   comamand.  It's mostly used for meta-command replies, and for text
 *   fragments that are used to construct descriptions.
 *   
 *   This message object isn't generally used for parser messages or action
 *   replies - most of those come from the objects given by the current
 *   actor's getParserMessageObj() or getActionMessageObj(), respectively.
 *   
 *   By default, this is set to libMessages.  The library never changes this
 *   itself, but a game can change this if it wants to switch to a new set of
 *   messages during a game.  (If you don't need to change messages during a
 *   game, but simply want to customize some of the default messages, you
 *   don't need to set this variable - you can simply use 'modify
 *   libMessages' instead.  This variable is designed for cases where you
 *   want to *dynamically* change the standard messages during the game.)  
 */
#define gLibMessages (libGlobal.libMessageObj)


/* ------------------------------------------------------------------------ */
/*
 *   readMainCommandTokens() phase identifiers.  We define a separate code
 *   for each kind of call to readMainCommandTokens() so that we can do any
 *   special token processing that depends on the type of command we're
 *   reading.
 *   
 *   The library doesn't use the phase information itself for anything.
 *   These phase codes are purely for the author's use in writing
 *   pre-parsing functions and for differentiating prompts for the different
 *   types of input, as needed.
 *   
 *   Games that read additional response types of their own are free to add
 *   their own enums to identify the additional phases.  Since the library
 *   doesn't need the phase information for anything internally, it won't
 *   confuse the library at all to add new game-specific phase codes.  
 */

/* reading a normal command line */
enum rmcCommand;

/* reading an unknown word response, to check for an "oops" command */
enum rmcOops;

/* reading a response to a prompt for a missing object phrase */
enum rmcAskObject;

/* reading a response to a prompt for a missing literal phrase */
enum rmcAskLiteral;

/* reading a response to an interactive disambiguation prompt */
enum rmcDisambig;


/* ------------------------------------------------------------------------ */
/*
 *   Property set definitions 
 */

/* in debug mode, flag objFor definitions for non-existent actions */
#ifdef __DEBUG
# define objForCheck(which, action) \
    sentinel##which##action = __objref(action##Action, warn)
#else
# define objForCheck(which, action)
#endif

#define objFor(which, action) \
    objForCheck(which, action) \
    propertyset '*' ## #@which ## #@action

#define dobjFor(action) objFor(Dobj, action)
#define iobjFor(action) objFor(Iobj, action)

/*
 *   Treat an object definition as equivalent to another object definition.
 *   These can be used immediately after a dobjFor() or iobjFor() to treat
 *   the first action as though it were the second.  So, if the player types
 *   "search box", and we want to treat the direct object the same as for
 *   "look in box", we could make this definition for the box:
 *   
 *   dobjFor(Search) asDobjFor(LookIn)
 *   
 *   Note that no semicolon is needed after this definition, and that this
 *   definition is completely in lieu of a regular property set for the
 *   object action.
 *   
 *   In general, a mapping should NOT change the role of an object:
 *   dobjFor(X) should not usually be mapped using asIobjFor(Y), and
 *   iobjFor(X) shouldn't be mapped using asDobjFor(Y).  The problem with
 *   changing the role is that the handler routines often assume that the
 *   object is actually in the role for which the handler was written; a
 *   verify handler might refer to '{dobj}' in generating a message, for
 *   example, so reversing the roles would give the wrong object in the role.
 *   
 *   Role reversals should always be avoided, but can be used if necessary
 *   under conditions where all of the code involved in the TARGET of the
 *   mapping can be carefully controlled to ensure that it doesn't make
 *   assumptions about object roles, but only references 'self'.  Reversing
 *   roles in a mapping should never be attempted in general-purpose library
 *   code, because code based on the library could override the target of the
 *   role-reversing mapping, and the override could fail to observe the
 *   restrictions on object role references.
 *   
 *   Note that role reversals can almost always be handled with other
 *   mechanisms that handle reversals cleanly.  Always consider remapTo()
 *   first when confronted with a situation that seems to call for a
 *   role-reversing asObjFor() mapping, as remapTo() specifically allows for
 *   object role changes.  
 */
#define asObjFor(obj, Action) \
    { \
        preCond { return preCond##obj##Action; } \
        verify() { verify##obj##Action; } \
        remap() { return remap##obj##Action; } \
        check() { check##obj##Action; } \
        action() { action##obj##Action; } \
    }

#define asDobjFor(action) asObjFor(Dobj, action)
#define asIobjFor(action) asObjFor(Iobj, action)

/* 
 *   Define mappings of everything except the action.  This can be used in
 *   cases where we want to pick up the verification, preconditions, and
 *   check routines from another handler, but not the action.  This is often
 *   useful for two-object verbs where the action processing is entirely
 *   provided by one or the other object, so applying it to both would be
 *   redundant.  
 */
#define asObjWithoutActionFor(obj, Action) \
    { \
        preCond { return preCond##obj##Action; } \
        verify() { verify##obj##Action; } \
        remap() { return remap##obj##Action(); } \
        check() { check##obj##Action; } \
        action() {  } \
    }

#define asDobjWithoutActionFor(action) asObjWithoutActionFor(Dobj, action)
#define asIobjWithoutActionFor(action) asObjWithoutActionFor(Iobj, action)

/*
 *   "Remap" an action.  This effectively rewrites the action in the given
 *   form.  Each of the object slots can be filled either with a specific
 *   object, or with a noun phrase role name (DirectObject, IndirectObject);
 *   in the latter case, the object or objects from the named noun phrase
 *   role in the *current* action (i.e., before the rewrite) will be used.
 *   
 *   If the new action has two or more objects (for example, if it's a
 *   TIAction), then EXACTLY ONE of the slots must be filled with a specific
 *   object, and all of the other slots must be filled with role names.  The
 *   specific object is the one that corresponds to the original object
 *   that's doing the remapping in the first place - this can simply be
 *   'self' if the new action will operate on the same object, or it can be
 *   a different object.  The important thing is that the 'verify' method
 *   for the defining object will be forwarded to the corresponding 'verify'
 *   method on the corresponding object for the new action.
 *   
 *   This macro must be used as the ENTIRE definition block for a dobjFor()
 *   or iobjFor().  For example, to remap a "put in" command directed to a
 *   desk so that the command is instead applied to a drawer in the desk, we
 *   could define the following on the desk object:
 *   
 *   iobjFor(PutIn) remapTo(PutIn, DirectObject, deskDrawer) 
 */
#define remapTo(action, objs...) { remap = [action##Action, ##objs] }

/*
 *   Conditionally remap an action.  If 'cond' (a conditional expression)
 *   evaluated to true, we'll remap the action as directed; otherwise, we'll
 *   inherit the default handling 
 */
#define maybeRemapTo(cond, action, objs...) \
    { remap = ((cond) ? [action##Action, ##objs] : inherited()) }


/*
 *   For two-object push-travel actions, such as "push sled into cave",
 *   define a special mapping for both the direct and indirect objects:
 *   
 *   - Map the direct object (the object being pushed) to a simple
 *   PushTravel action.  So, for "push sled into cave," map the direct
 *   object handling to PushTravel for the sled.  This makes the handling of
 *   the command equivalent to "push sled north" and the like.
 *   
 *   - Map the indirect object (the travel connector) to use the PushTravel
 *   action's verify remapper.  This is handled specially by the PushTravel
 *   action object to handle the verification as though it were verifying
 *   the corresponding ordinary (non-push) travel action on the indirect
 *   object.  Beyond verification, we do nothing, since the direct object of
 *   a pushable object will handle the whole action using a nested travel
 *   action.
 *   
 *   This effectively decomposes the two-object action into two coupled
 *   single-object actions: a regular PushTravel action on the object being
 *   pushed, and a regular whatever-kind-of-travel on the connector being
 *   traversed.  This handling has the appeal that it means that we don't
 *   need a separate PUSH version of every kind of allowed travel on a
 *   connector, and we don't need a special handler version for each kind of
 *   travel on a pushable object; instead, we just use the basic PushTravel
 *   and kind-of-travel handlers to form the combined form.  Note that this
 *   still allows separate treatment of the combined form wherever desired,
 *   just by overriding these default handlers for the two-object action.  
 */
#define mapPushTravelHandlers(pushAction, travelAction) \
    dobjFor(pushAction) asDobjFor(PushTravel) \
    mapPushTravelIobj(pushAction, travelAction)

#define mapPushTravelIobj(pushAction, travelAction) \
    iobjFor(pushAction) \
    { \
        verify() \
            { gAction.verifyPushTravelIobj(self, travelAction##Action); } \
    }


/* ------------------------------------------------------------------------ */
/*
 *   For an Actor, delegate an action handler to the ActorState object for
 *   processing.  You can use this any time you want to write the handlers
 *   for a particular action in the ActorState rather than in the Actor
 *   itself.  This would be desirable if the actor's response for a
 *   particular action varies considerably according to the actor's state.
 *   For example, if you want an actor's response to being attacked to be
 *   handled in the actor's current ActorState object, you could put this
 *   code in the Actor object:
 *   
 *   dobjFor(AttackWith) actorStateDobjFor(AttackWith)
 *   
 *   Once you've done this, you'd just write a normal dobjFor(AttackWith)
 *   handler in each of the ActorState objects associated with the actor.  
 */
#define actorStateObjFor(obj, Action) \
    { \
        preCond { return curState.preCond##obj##Action; } \
        verify() { curState.verify##obj##Action; } \
        remap() { return curState.remap##obj##Action; } \
        check() { curState.check##obj##Action; } \
        action() { curState.action##obj##Action; } \
    }

#define actorStateDobjFor(action) actorStateObjFor(Dobj, action)
#define actorStateIobjFor(action) actorStateObjFor(Iobj, action)


/* ------------------------------------------------------------------------ */
/*
 *   Object role identifiers.  These are used to identify the role of a noun
 *   phrase in a command.
 *   
 *   The library provides base classes for actions of zero, one, and two noun
 *   phrases in their grammars: "look", "take book", "put book on shelf".  We
 *   thus define role identifiers for direct and indirect objects.  Note that
 *   even though we stop there, this doesn't preclude games or library
 *   extensions from adding actions that take more than two noun phrases
 *   ("put coin in slot with tongs"); any such extensions must simply define
 *   their own additional role identifiers for the third or fourth (etc) noun
 *   phrase.
 */
enum ActorObject, DirectObject, IndirectObject;

/*
 *   A special role for the "other" object of a two-object command.  This
 *   can be used in certain contexts (such as remapTo) where a particular
 *   object role is implied by the context, and where the action involved
 *   has exactly two objects; OtherObject in such contexts means
 *   DirectObject when the implied role is IndirectObject, and vice versa. 
 */
enum OtherObject;


/* ------------------------------------------------------------------------ */
/*
 *   Pronoun types.  These are used to identify pronoun antecedents when
 *   resolving noun phrases involving pronouns.
 *   
 *   We define a basic set of pronouns here that are common to most
 *   languages.  Language-specific modules are free to add their own pronoun
 *   types as needed.
 *   
 *   Our basic set is:
 *   
 *   'it' - the neuter singular
 *.  'him' - the masculine singular
 *.  'her' - the feminine singular
 *.  'them' - the ungendered plural
 *.  'you' - second person singular
 *.  'me' - first person singular
 *   
 *   Note that the first-person and second-person pronouns are assigned
 *   meanings that can vary by context.  When a command is issued by the
 *   player character to the player character (i.e., the command comes from
 *   the player and no target actor is specified), these refer to the player
 *   character when the PC is in the appropriate referral person - if the
 *   game calls the PC "you", then the player calls the PC "me", and vice
 *   versa.  When a command is targeted to or issued by an actor other than
 *   the PC, then "you" refers to the command's target and "me" refers to
 *   the command's issuer.  
 */
enum PronounIt, PronounThem, PronounHim, PronounHer, PronounYou, PronounMe;


/* ------------------------------------------------------------------------ */
/* 
 *   set the location property 
 */
+ property location;


/* ------------------------------------------------------------------------ */
/*
 *   Alternative exit definition.  This can be used to define a secondary
 *   direction that links to the same destination, via the same connector, as
 *   another direction.  It's frequently desirable to link multiple
 *   directions to the same exit; for example, a door leading north might
 *   also lead out, or a stairway to the north could lead up as well.
 *   
 *   Use this as follows in a room's property list:
 *   
 *.     out asExit(north)
 *   
 *   (Note that there's no '=' sign.)
 *   
 *   It's not necessary to use this macro to declare an alternative exit,
 *   since the alternatives can all point directly to the same connector as
 *   the original.  The only thing this macro does is to make the alternative
 *   exit unlisted - it won't be shown in the list of exits in the status
 *   line, and it won't be shown in "you can't go that way" messages.
 *   
 *   Note that there's one common case where you should be careful with
 *   asExit(): if you have a room that has an exit in some compass direction,
 *   you might be tempted to make OUT the primary "direction" for the exit,
 *   and treat the equivalent compass as a synonym, with a line such as
 *   "south asExit(out)".  You should avoid doing this - do it the other way
 *   instead, with the compass direction as the primary direction and OUT as
 *   the synonym: "out asExit(south)".  The reason this is important is that
 *   if there's a nested room inside the location (such as a chair), OUT
 *   while in the nested room will mean to get out of the nested room.  If
 *   you make the compass direction primary and make OUT the synonym, the
 *   compass direction will be listed as an available exit both in the
 *   location and in any nested rooms within it.  
 */
#define asExit(dir) = static ((dir).createUnlistedProxy())


/* ------------------------------------------------------------------------ */
/*
 *   "Person" indices.  We define these as numbers rather than enums so that
 *   we can easily use these as list indices.  
 */
#define FirstPerson   1
#define SecondPerson  2
#define ThirdPerson   3


/* ------------------------------------------------------------------------ */
/*
 *   Transparency levels 
 */

/* the sense is passed without loss of detail */
enum transparent;

/* the sense is passed, but with a loss of detail associated with distance */
enum distant;

/*
 *   The sense is passed, but with attenuation of energy level.  No other
 *   obscuration of detail occurs; this is something like tinted glass that
 *   doesn't distort the transmitted sense but reduces the amount of energy. 
 */
enum attenuated;

/* 
 *   The sense is passed, but with a loss of detail due to an obscuring
 *   layer of material.  The energy level is also attenuated.  This is
 *   something like dirty or wavy glass that distorts an image transmitted
 *   through it but doesn't completely block out light.  
 */
enum obscured;

/* the sense is not passed at all */
enum opaque;


/* ------------------------------------------------------------------------ */
/*
 *   Size classes.  An object is large, medium, or small with respect to
 *   each sense; the size is used to determine how well the object can be
 *   sensed at a distance or when obscured.
 *   
 *   What "size" means depends on the sense.  For sight, the size
 *   indicates the visual size of the object.  For hearing, the size
 *   indicates the loudness of the object.  
 */

/* 
 *   Large - the object is large enough that its details can be sensed
 *   from a distance or through an obscuring medium.
 */
enum large;

/* 
 *   Medium - the object can be sensed at a distance or when obscured, but
 *   not in any detail.  Most objects fall into this category.  Note that
 *   things that are parts of large objects should normally be medium.  
 */
enum medium;

/*
 *   Small - the object cannot be sensed at a distance at all.  This is
 *   appropriate for detailed parts of medium-class objects.  
 */
enum small;


/* ------------------------------------------------------------------------ */
/*
 *   Path traversal operations. 
 */

/* traverse from the starting point of the path */
enum PathFrom;

/* traverse into the contents */
enum PathIn;

/* traverse out to the container */
enum PathOut;

/* traverse from an object to a peer at the same containment level */
enum PathPeer;

/* 
 *   traverse through an object with no common container on either side of
 *   the traversal - this is used when we are traversing an object, such as a
 *   SenseConnector, that connects unrelated locations 
 */
enum PathThrough;

/* traverse to the ending point of the path */
enum PathTo;


/* ------------------------------------------------------------------------ */
/*
 *   Listing Options 
 */

/* 
 *   use "tall" notation, which lists objects in a single column, one item
 *   per line (the default is "wide" notation, which creates a sentence
 *   with the object listing) 
 */
#define ListTall      0x0001

/*
 *   Recursively list the contents of each item we list.  
 *   
 *   For a 'tall' list, this indicates that we'll show the listed contents
 *   of each item that we list, and the listed contents of those items, and
 *   so on, indenting each level to indicate the containment relationship.
 *   
 *   For a 'wide' list, this indicates that we'll show the listed contents
 *   of each item in-line in the listing, as a parenthetic note.
 *   
 *   For both types of listings, when this flag is set and the indent level
 *   is zero (indicating a top-level listing), after the main list, we'll
 *   show a separate list for the contents of each item in our list that
 *   isn't itself listable but has listed contents, or has contents with
 *   listed contents, and so on to any level.  For example, if we're showing
 *   a room description, and the room contains a desk that isn't listed
 *   because it's a fixed part of the room, we'll show a separate list of
 *   the desk's listed contents.  
 */
#define ListRecurse   0x0002

/* 
 *   use "long list" notation - separates items that contain sublists with
 *   special punctuation, to set off the individual items in the longer
 *   listing from the items in the sublists (for example, separates items
 *   with semicolons rather than commas) 
 */
#define ListLong      0x0004

/* 
 *   This is a recursive listing of the contents of an item.  This is set by
 *   showList() in calls it makes to recursive listing levels.
 */
#define ListContents  0x0008

/*
 *   Custom option bits.  Flag bits with this value and higher are reserved
 *   for use by individual lister subclasses.
 *   
 *   To ensure compatibility with any future changes that involve adding
 *   more base lister flags, subclasses are encouraged to use the following
 *   mechanism.  DO NOT use #define to define your own custom subclass
 *   flags.  Instead, define a property of your lister subclass for each
 *   flag you need as follows:
 *   
 *   myCustomFlag1 = ListerCustomFlag(1) // use 1 for the first flag
 *.  myCustomFlag2 = ListerCustomFlag(2) // etc
 *.  nextCustomFlag = ListerCustomFlag(3)
 *   
 *   You DO NOT have to use the name 'myCustomFlag1' - use whatever name you
 *   like that describes the nature of the flag.  However, the last item
 *   MUST be called 'nextCustomFlag' - this ensures that any subclasses of
 *   your class will allocate their own flags with new values that don't
 *   conflict with any of yours.
 *   
 *   Then, when a client of your Lister subclass needs to pass one of your
 *   flag to the Lister, it should simply evaluate your 'myCustomFlagN'
 *   property of your lister.  If you'd like, you can even #define a ListXxx
 *   macro that retrieves the value, for the convenience of your callers:
 *   
 *   #define ListMyclassMyCustomFlag1 (Myclass.myCustomFlag1) 
 */
#define ListCustomFlag 0x0100

#define ListerCustomFlag(n) static ((inherited.nextCustomFlag) << ((n) - 1))


/* ------------------------------------------------------------------------ */
/*
 *   Spelled-out number options, for spellInt() and related functions.
 *   
 *   Note that the interfaces to the number-spelling functions can vary by
 *   language, and that variation can include these flags.  Some language
 *   modules might ignore some of these generic flags or define additional
 *   language-specific flags.  
 */

/* 
 *   Use tens of hundreds rather than thousands if possible - 1950 is
 *   'nineteen hundred fifty' rather than 'one thousand nine hundred
 *   fifty'.  This only works if the number (not including the millions
 *   and billions) is in the range 1,100 to 9,999, because we don't want
 *   to say something like 'one hundred twenty hundred' for 12,000.  
 */
#define SpellIntTeenHundreds    0x0001

/*
 *   use 'and' before the tens - 125 is 'one hundred and twenty-five'
 *   rather than 'one hundred twenty-five' 
 */
#define SpellIntAndTens         0x0002

/*
 *   put a comma after each power group - 123456 is 'one hundred
 *   twenty-three thousand, four hundred fifty-six' 
 */
#define SpellIntCommas          0x0004


/* ------------------------------------------------------------------------ */
/*
 *   Decimal number format options.  These are used with the number
 *   formatting functions to control the formatting of numbers displayed in
 *   decimal digit format.  
 */

/*
 *   Use a group separator character between digit groups, using the
 *   default setting in languageGlobals.
 */
#define DigitFormatGroupSep     0x0001

/* 
 *   Explicitly use a comma/period to separate digit groups, overriding
 *   the current languageGlobals setting.
 */
#define DigitFormatGroupComma   0x0002
#define DigitFormatGroupPeriod  0x0004


/* ------------------------------------------------------------------------ */
/*
 *   aHref() flags 
 */
#define AHREF_Plain  0x0001    /* plain text hyperlink (no underline/color) */


/* ------------------------------------------------------------------------ */
/*
 *   ResolveInfo flags 
 */

/* the noun phrase ends with an adjective */
#define EndsWithAdj      0x0001

/* 
 *   one of the words in the noun phrase was truncated from its full
 *   dictionary spelling 
 */
#define VocabTruncated   0x0002

/*
 *   One or more plurals was truncated from its full dictionary spelling.
 *   (We specially distinguish plurals that are truncated, because in
 *   English a plural is usually formed by adding "s" or "es" to the end of
 *   the singular form of a noun, meaning that a given singular form is
 *   usually a leading substring of its plural.  When a singular noun is
 *   longer than the truncation limit, which is conventionally six
 *   characters, the singular will always match as a truncated version of
 *   the plural, so every time someone types in a singular it'll be treated
 *   as ambiguous between the singular and plural form.  So, in the English
 *   parser, we have a preference to ignore a truncated plural any time the
 *   word could also be interpreted as an untruncated singular, hence we
 *   note when we have a truncated plural.)  
 */
#define PluralTruncated  0x0004

/*
 *   The object came from an 'all' phrase.  Normally, the only time this
 *   makes any difference is when deciding whether or not to mention which
 *   object we're acting upon; an 'all' object should normally be mentioned
 *   explicitly, as though the command had involved multiple objects,
 *   because otherwise it might not be clear to the user what object had
 *   actually matched 'all'.  
 */
#define MatchedAll       0x0008

/*
 *   Always announce the object before executing the command on it.  This
 *   flag can be set for objects that match phrases whose meaning isn't
 *   necessarily known to the player, such as "all" (which selects objects
 *   based on the simulation state, which might not exactly match what the
 *   player had in mind) or "any book" (which might select arbitrarily from
 *   several possibilities, so the player can't know which we'll choose).  
 */
#define AlwaysAnnounce   0x0010

/*
 *   The noun phrase describing this object was ambiguous, and the object
 *   was selected by automatic disambiguation in a context where it was
 *   clear which object was indicated.  This is used in cases where the
 *   objects not selected were all illogical for the action context.  
 */
#define ClearDisambig    0x0020

/*
 *   The noun phase describing this object was ambiguous, and the object was
 *   selected by automatic disambiguation in a context where it was not
 *   perfectly clear which object was indicated.  This is used for cases
 *   where the objects selected were more logical than the objects not
 *   selected, but some of the unselected objects were still logical.
 *   
 *   This flag doesn't mean that we chose arbitrarily, but rather that we
 *   chose the best object or objects from a field that included additional
 *   objects that, though not quite as good, were still valid.  We flag this
 *   case because the user *could* have meant to use one of the other valid
 *   objects, even though we consider it most likely that the user meant to
 *   use the one(s) we selected; so, we want to flag this so we can call the
 *   user's attention to our choice, to make it more likely that the user
 *   will immediately notice if we made the wrong choice.
 *   
 *   Note that we can't have both ClearDisambig and UnclearDisambig at the
 *   same time, but we *can* have neither of these.  If neither flag is set
 *   for an object, it simply means that the object wasn't ambiguous to
 *   start with.  When the user explicitly picks an object interactively,
 *   the selected object is effectively unambiguous, so it won't have either
 *   flag set; even though it started off ambiguous, the user did all of the
 *   work of selecting the appropriate object, leaving things unambiguous in
 *   the end.  
 */
#define UnclearDisambig  0x0040

/*
 *   The noun phrase was missing from the command and this object was
 *   supplied as an implicit default.
 */
#define DefaultObject    0x0080

/*
 *   We've announced this as a defaulted object.  We use this to ensure that
 *   we only make this type of announcement once, even if the opportunity to
 *   make the announcement comes up more than once; this can happen when
 *   we're asking for missing objects interactively in a multi-object
 *   command, since we might want to announce a default before prompting as
 *   well as before execution.  
 */
#define AnnouncedDefaultObject  0x0100


/* ------------------------------------------------------------------------ */
/*
 *   Announcement styles for disambiguated objects.  These are used in the
 *   gameMain object (see GameMainDef) to select which type of announcement
 *   is used when the parser disambiguates a noun phrase using the
 *   logicalness rules.  
 */

/* 
 *   Announce unclear disambiguation results only.  When this setting is
 *   selected, the parser makes a parenthetical announcement (e.g., "(the red
 *   door)") when it selects an object based on likelihood rankings from
 *   among more than one logical match.  The parser makes no announcement
 *   when exactly one logical object is in scope, even if other objects match
 *   the noun phrase by name. 
 */
enum AnnounceUnclear;

/*
 *   Announce clear and unclear disambiguation results, both using
 *   parenthetical announcement ("(the red door)").  When this setting is
 *   selected, the parser makes these announcements every time it applies the
 *   logicalness rules or likelihood rankings to disambiguate a noun phrase.
 *   There's no announcement when no disambiguation is needed (because the
 *   noun phrase matches only one in-scope object).  
 */
enum AnnounceClear;

/*
 *   Describe clear disambiguation results, rather than announcing them.  The
 *   parser makes the parenthetical announcement, as usual, for unclear
 *   disambiguation picks, but not for clear picks (a clear pick is one where
 *   there's only one logical object, even though the noun phrase matches
 *   more than one object).  For clear picks, however, the parser uses a
 *   verbose version of the action reply in lieu of one of the terse default
 *   messages.  For example, rather than saying just "Taken", the parser
 *   would reply "You take the red book."  The longer messages mention the
 *   object by name, to make it clear exactly which one was chosen.  
 */
enum DescribeClear;


/* ------------------------------------------------------------------------ */
/*
 *   Inventory modes.  "Wide" mode displays the inventory in paragraph form;
 *   "tall" mode displays as a list, with one item per line, indenting items
 *   to reflect containment.  
 */
enum InventoryWide, InventoryTall;


/* ------------------------------------------------------------------------ */
/*
 *   Define an action with the given base class.  This adds the *Action
 *   suffix to the given root name, and defines a class with the given base
 *   class.  We also define the baseActionClass property to refer to myself;
 *   this is the canonical class representing the action for all subclasses.
 *   This information is useful because a language module might define
 *   several grammar rule subclasses for the given class; this lets us
 *   relate any instances of those various subclasses back to this same
 *   canonical class for the action if necessary.  
 */
#define DefineAction(name, baseClass...) \
    class name##Action: ##baseClass \
    baseActionClass = name##Action

/*
 *   Define a "system" action.  System actions are meta-game commands, such
 *   as SAVE and QUIT, that generally operate the user interface and are not
 *   part of the game world.  
 */
#define DefineSystemAction(name) \
    DefineAction(name, SystemAction)

/*
 *   Define a concrete IAction, given the root name for the action.  We'll
 *   automatically generate a class with name XxxAction. 
 */
#define DefineIAction(name) \
    DefineAction(name, IAction)

/* define a conversational IAction, such as Hello, Goodbye, Yes, No */
#define DefineConvIAction(name) \
    DefineAction(name, ConvIAction)

/*
 *   Define a concrete TAction, given the root name for the action.  We'll
 *   automatically generate a class with name XxxAction, a verProp with name
 *   verXxx, a checkProp with name checkXxx, and an actionProp with name
 *   actionDobjXxx.  
 */
#define DefineTAction(name) \
    DefineTActionSub(name, TAction)

/*
 *   Define a concrete TAction with a specific base class.  
 */
#define DefineTActionSub(name, cls) \
    DefineAction(name, cls) \
    verDobjProp = &verifyDobj##name \
    remapDobjProp = &remapDobj##name \
    preCondDobjProp = &preCondDobj##name \
    checkDobjProp = &checkDobj##name \
    actionDobjProp  = &actionDobj##name \

/*
 *   Define a concrete TIAction, given the root name for the action.  We'll
 *   automatically generate a class with name XxxAction, a verDobjProp with
 *   name verDobjXxx, a verIobjProp with name verIobjxxx, a checkDobjProp
 *   with name checkDobjXxx, a checkIobjProp with name checkIobjXxx, an
 *   actionDobjProp with name actionDobjXxx, and an actionIobjProp with name
 *   actionIobjXxx.  
 */
#define DefineTIAction(name) \
    DefineTIActionSub(name, TIAction)

/*
 *   Define a concrete TIAction with a specific base class.  
 */
#define DefineTIActionSub(name, cls) \
    DefineAction(name, cls) \
    verDobjProp = &verifyDobj##name \
    verIobjProp = &verifyIobj##name \
    remapDobjProp = &remapDobj##name \
    remapIobjProp = &remapIobj##name \
    preCondDobjProp = &preCondDobj##name \
    preCondIobjProp = &preCondIobj##name \
    checkDobjProp = &checkDobj##name \
    checkIobjProp = &checkIobj##name \
    actionDobjProp  = &actionDobj##name \
    actionIobjProp = &actionIobj##name

/*
 *   Define a concrete TopicAction, given the root name for the action. 
 */
#define DefineTopicAction(name) \
    DefineAction(name, TopicAction)

/*
 *   Define a concrete TopicTAction, given the root name for the action.
 *   'which' gives the role the topic serves, for message generation purposes
 *   - this should be one of the object role enums (DirectObject,
 *   IndirectObject, etc) indicating which role the topic plays in the
 *   action's grammar.  
 */
#define BaseDefineTopicTAction(name, which, cls) \
    DefineAction(name, cls) \
    verDobjProp = &verifyDobj##name \
    remapDobjProp = &remapDobj##name \
    preCondDobjProp = &preCondDobj##name \
    checkDobjProp = &checkDobj##name \
    actionDobjProp = &actionDobj##name \
    whichMessageTopic = which

#define DefineTopicTAction(name, which) \
    BaseDefineTopicTAction(name, which, TopicTAction)

/*
 *   Define a concrete ConvTopicTAction.  This is just like defining a
 *   TopicTAction, but defines the action using the ConvTopicTAction
 *   subclass.  
 */
#define DefineConvTopicTAction(name, which) \
    BaseDefineTopicTAction(name, which, ConvTopicTAction)

/*
 *   Define a concrete LiteralAction, given the root name for the action.  
 */
#define DefineLiteralAction(name) \
    DefineAction(name, LiteralAction)

/*
 *   Define a concrete LiteralTAction, given the root name for the action.
 *   'which' gives the role the literal phrase serves, for message generation
 *   purposes - this should be one of the object role enums (DirectObject,
 *   IndirectObject, etc) indicating which role the topic plays in the
 *   action's grammar.  
 */
#define DefineLiteralTAction(name, which) \
    DefineAction(name, LiteralTAction) \
    verDobjProp = &verifyDobj##name \
    remapDobjProp = &remapDobj##name \
    preCondDobjProp = &preCondDobj##name \
    checkDobjProp = &checkDobj##name \
    actionDobjProp = &actionDobj##name \
    whichMessageLiteral = which

/* ------------------------------------------------------------------------ */
/*
 *   Convenience macros for setting verify results.
 *   
 *   A verify routine can use these macros to set any number of verify
 *   results.  The VerifyResultList will keep only the result that gives the
 *   strongest disapproval of the action, since the verification process is
 *   by its nature only interested in the most negative result.
 *   
 *   These macros take advantage of the fact that we have a global
 *   VerifyResultList object, which gathers the results of the verification,
 *   so they can be used only in verify routines.  The global verification
 *   results object is valid during each verification invocation.  
 */

/* 
 *   Command is logical.  There's generally no need to add a logical result
 *   explicitly, since a command is logical unless disapproved, but we
 *   include this for completeness.
 *   
 *   We use 100 as the default likelihood, to leave plenty of room for
 *   specific likelihood rankings both above and below the default level.  
 */
#define logical \
    (gVerifyResults.addResult(new LogicalVerifyResult(100, '', 100)))

/* 
 *   Command is logical, and is ranked as indicated among logical results.
 *   The 'rank' value is the likelihood rank; the higher the rank, the more
 *   logical the command is.  The rank is only used to establish an ordering
 *   of the logical results; if a command also has illogical results, all of
 *   the illogical results rank as less logical than the logical result with
 *   the lowest likelihood.
 *   
 *   The 'key' value is an arbitrary string value associated with the
 *   ranking.  When two result lists both have a logical result object, and
 *   both logical result objects have the same likelihood level, we'll check
 *   the keys; if the keys match, we'll treat the two results as equivalent
 *   and thus not distinguishing for disambiguation.  This is useful because
 *   it creates a crude multivariate space for ranking items for
 *   disambiguation.
 *   
 *   For example, suppose we have a "put in" command, and we have two
 *   possibilities for the target container.  Neither is being held by the
 *   actor, so they both have a result with a logical rank of 70 with a key
 *   value of 'not held'.  In addition, both are openable, and one is open
 *   and the other is closed; the closed one has an additional result with a
 *   logical rank of 80 and a key of 'not open'.  Which do we choose?  If we
 *   looked only at the logical rankings, both would be equivalent, since
 *   both have 70's as their most disapproving results.  However, we see
 *   that the two 70's were applied for the same reason - because they share
 *   a common key - so we know this information isn't helpful for
 *   disambiguation and can be ignored.  So, we find that the closed one has
 *   an 80, and the other has no other results (hence is by default logical
 *   with rank 100), thus we take the '80' as the better one.
 *   
 *   Throughout the library, we use the following conventions:
 *   
 *   150 = especially good fit: a good candidate for the action that is
 *   especially likely to be used with the command.  For example, a book is
 *   especially suitable for a "read" command.
 *   
 *   140 = similar to 150, but slightly less ideal a fit.  We use this for
 *   objects that are excellent fits, but for which we know certain other
 *   objects might be better fits.
 *   
 *   100 = default: a perfectly good candidate for the action, with nothing
 *   that would make it illogical, but nothing that makes it especially
 *   likely, either
 *   
 *   80 = slightly less than perfect: a good candidate, but with some
 *   temporary and correctable attribute that may make it less likely than
 *   others.  This is used for attributes that can be corrected: a container
 *   needs to be opened for the action to succeed, but isn't currently open,
 *   or an article of clothing cannot be worn for the action to proceeds,
 *   but is currently being worn.
 *   
 *   60/70 = slightly less than perfect, but with some attributes that can't
 *   be readily corrected and which make the candidate potentially less
 *   likely.  These are used to make guesses about which might object might
 *   be intended when several are logical but some might be more readily
 *   used than others; for example, if putting an object into a container, a
 *   container being held might rank higher than one not being held, so the
 *   one not being held might be ranked a "70" likelihood.
 *   
 *   50 = logical but not especially likely: an acceptable candidate for the
 *   action, but probably not the best choice for the action.  This is used
 *   when an object can be used for the action, but would not be expected to
 *   do anything special with the action.  
 */
#define logicalRank(rank, key)  \
    (gVerifyResults.addResult(new LogicalVerifyResult(rank, key, 100)))

/*
 *   Logical ranking with specific list ordering.  This is the same as a
 *   regular logicalRank, but uses the given list ordering rather than the
 *   default list ordering (100).
 */
#define logicalRankOrd(rank, key, ord) \
    (gVerifyResults.addResult(new LogicalVerifyResult(rank, key, ord)))

/* command is logical but dangerous */
#define dangerous \
    (gVerifyResults.addResult(new DangerousVerifyResult('')))

/* 
 *   command is logical but non-obvious: the object should never be taken as
 *   a default 
 */
#define nonObvious \
    (gVerifyResults.addResult(new NonObviousVerifyResult('')))

/* command is currently (but not always) illogical, for the given reason */
#define illogicalNow(msg, params...) \
    (gVerifyResults.addResult(new IllogicalNowVerifyResult(msg, ##params)))

/* illogical because things are already as the command would make them */
#define illogicalAlready(msg, params...) \
    (gVerifyResults.addResult( \
        new IllogicalAlreadyVerifyResult(msg, ##params)))

/* command is always illogical */
#define illogical(msg, params...) \
    (gVerifyResults.addResult(new IllogicalVerifyResult(msg, ##params)))

/* illogical since we're trying to use something on itself (eg, PUT X IN X) */
#define illogicalSelf(msg, params...) \
    (gVerifyResults.addResult(new IllogicalSelfVerifyResult(msg, ##params)))

/* command is being performed on an inaccessible object */
#define inaccessible(msg, params...) \
    (gVerifyResults.addResult(new InaccessibleVerifyResult(msg, ##params)))


/* ------------------------------------------------------------------------ */
/*
 *   Convenience macros for setting command results.
 */

/*
 *   Set a default report for the current command.  This report will be
 *   shown unless a non-default report is issued, or if the default report
 *   is to be suppressed (for example, because the command is being
 *   performed implicitly as part of another command).
 *   
 *   Default reports should be used only for simple acknowledgments of the
 *   command's successful completion - things like "Taken" or "Dropped" or
 *   "Done."
 *   
 *   Default responses are suppressed for implicit commands because they are
 *   redundant.  When a command is performed implicitly, it is conventional
 *   to mention the command being performed with a parenthetical: "(First
 *   taking the book)".  In such cases, a simple acknowledgment that the
 *   command was successfully performed would add nothing of use but would
 *   merely make the output more verbose, so we omit it.  
 */
#define defaultReport(msg, params...) \
    (gTranscript.addReport(new DefaultCommandReport(msg, ##params)))

/*
 *   Set a default descriptive report for the current command.  This report
 *   will be shown unless any other report is shown for the same command.
 *   This differs from defaultReport in that we don't suppress a default
 *   description for an implied command: we only suppress a default
 *   description when there are other reports for the same command.
 *   
 *   The purpose of the default descriptive report is to generate reports
 *   that say things along the lines that there's nothing special to
 *   describe.  For example:
 *   
 *   >x desk
 *.  You see nothing special about it.
 *   
 *   >look in alcove
 *.  There's nothing in the alcove.
 *   
 *   When there's nothing else to report, these default descriptions are
 *   suitable as the full response to the command.  However, they become
 *   undesirable when we have other "status" information or related special
 *   descriptions to display; consider:
 *   
 *   >x desk
 *.  You see nothing special about it.
 *.  Someone has jabbed a dagger into the top of the desk.
 *   
 *   >look in alcove
 *.  There's nothing in the alcove.
 *.  A vase is displayed in the alcove.
 *   
 *   >x bag
 *.  You see nothing special about it.  It's open, and it contains
 *.  a red book, an iron key, and a brass key.
 *   
 *   In the first two examples above, we have special descriptions for
 *   objects contained in the objects being described.  The special
 *   descriptions essentially contradict the default descriptions' claims
 *   that there's nothing special to mention, and also render the default
 *   descriptions unnecessary, in that it would be enough to show just the
 *   special descriptions.  The third example above is similar, but the
 *   extra information is status information for the object being described
 *   rather than a special description of a contained item; as with the
 *   other examples, the generic default description is both contradictory
 *   and unnecessary.
 *   
 *   Default description reports should ONLY be used for messages that have
 *   the character of the examples above: generic descriptions that indicate
 *   explicitly that there's nothing special to report.  Messages that offer
 *   any sort of descriptive detail should NOT be generated as default
 *   description reports, because it is suitable and desirable to retain an
 *   actual descriptive message even when other status information or
 *   related special descriptions are also shown.  
 */
#define defaultDescReport(msg, params...) \
    (gTranscript.addReport(new DefaultDescCommandReport(msg, ##params)))

/*
 *   Add an cosmetic internal spacing report.  This type of report is used
 *   to show spacing (usually a paragraph break) within command output.
 *   
 *   The important thing about this report is that it doesn't trigger
 *   suppression of any default reports.  This is useful when internal
 *   separation is added on speculation that there might be some reports to
 *   separate, but without certainty that there will actually be any reports
 *   shown; for example, when preparing to show a list of special
 *   descriptions, we might add some spacing just in case some special
 *   descriptions will be shown, saving the trouble of checking to see if
 *   anything actually needs to be shown.  
 */
#define cosmeticSpacingReport(msg, params...) \
    (gTranscript.addReport(new CosmeticSpacingCommandReport(msg, ##params)))

/*
 *   Add an "extra" report.  This is an incidental message that doesn't
 *   affect the display of a default report. 
 */
#define extraReport(msg, params...) \
    (gTranscript.addReport(new ExtraCommandReport(msg, ##params)))

/*
 *   Set a main report for the current command.  This report will be shown
 *   as the main report from the command, overriding any default report for
 *   the command.  
 */
#define mainReport(msg, params...) \
    (gTranscript.addReport(new MainCommandReport(msg, ##params)))

/*
 *   Set a "before" report for the current command.  This report will be
 *   shown before any main report, but will override any default report for
 *   the command.  
 */
#define reportBefore(msg, params...) \
    (gTranscript.addReport(new BeforeCommandReport(msg, ##params)))

/*
 *   Set an "after" report for the current command.  This report will be
 *   shown after any main report, but will override any default report for
 *   the command.  
 */
#define reportAfter(msg, params...) \
    (gTranscript.addReport(new AfterCommandReport(msg, ##params)))

/*
 *   Report failure.  This overrides any default report, and marks the
 *   command as having failed.
 *   
 *   A failure report should NOT indicate any state change - this is
 *   important because failure reports are suppressed under some conditions
 *   (for example, when an NPC is performing an implied command, and the
 *   implied command fails, we don't show the failure report).  If a failure
 *   is accompanied by a state change, then a mainReport() should be made in
 *   addition to the failure report - the main report should indicate the
 *   state change.  
 */
#define reportFailure(msg, params...) \
    (gTranscript.addReport(new FailCommandReport(msg, ##params)))

/*
 *   Report a question.  This shows a report that's really an interactive
 *   prompt for more information, such as a prompt for a missing object. 
 */
#define reportQuestion(msg, params...) \
    (gTranscript.addReport(new QuestionCommandReport(msg, ##params)))


/* ------------------------------------------------------------------------ */
/*
 *   Thing message property overrides sometimes need to be selective about
 *   the role of the object.  These macros let you specify that a Thing
 *   message override is only in effect when the Thing is the direct or
 *   indirect object.  When the object isn't in the specified role, the
 *   message override will be ignored.
 *   
 *   For example, suppose you want to override an object's response to PUT
 *   IN, but *only* when it's the indirect object of PUT IN - *not* when the
 *   object is itself being put somewhere.  To do this, you could give the
 *   object a property like this:
 *   
 *.    notAContainerMsg = iobjMsg('The vase\'s opening is too small. ')
 *   
 *   This specifies that when the object is involved in a PUT IN command that
 *   fails with the 'notAContainerMsg' message, the given message should be
 *   used - but *only* when the object is the indirect object.  
 */
#define dobjMsg(msg) (gDobj == self ? msg : nil)
#define iobjMsg(msg) (gIobj == self ? msg : nil)


/* ------------------------------------------------------------------------ */
/*
 *   Try performing a command implicitly.  The action is the root name of
 *   the action, without the 'Action' suffix - we'll automatically add the
 *   suffix.  'objs' is a varying-length list of the resolved objects in the
 *   new action - the direct object, indirect object, and any others needed
 *   for the action.  
 */
#define tryImplicitAction(action, objs...) \
    _tryImplicitAction(gIssuingActor, gActor, &announceImplicitAction, \
    action##Action, ##objs)

/*
 *   Try performing a command implicitly, with a special descriptive
 *   message.  'msgProp' gives the libMessages method to invoke the announce
 *   the action, if the action is performed.  If 'msgProp' is nil, no
 *   message is displayed at all.
 *   
 *   'action' is the root name of the action, without the 'Action' suffix
 *   (we'll automatically add the suffix).  'objs' is a varying-length list
 *   of the resolved objects - direct object, indirect object, and any
 *   others needed.  
 */
#define tryImplicitActionMsg(msgProp, action, objs...) \
    _tryImplicitAction(gIssuingActor, gActor, msgProp, \
                       action##Action, ##objs)

/*
 *   Replace the current action with a new action.  The new action will be
 *   performed, and the original action will be terminated with 'exit'.
 *   
 *   'action' is the root name of the action, without the 'Action' suffix
 *   (we'll add the suffix automatically).  'objs' is a varying-length list
 *   of the resolved objects - direct object, indirect object, etc.  
 */
#define replaceAction(action, objs...) \
    _replaceAction(gActor, action##Action, ##objs)

/*
 *   Replace the current action with a new action directed to a different
 *   actor (but from the same issuing actor).  
 */
#define replaceActorAction(actor, action, objs...) \
    _replaceAction(actor, action##Action, ##objs)

/*
 *   Run a nested action.
 */
#define nestedAction(action, objs...) \
    _nestedAction(nil, gActor, action##Action, ##objs)

/*
 *   Run a nested action targeted to a given actor.
 */
#define nestedActorAction(actor, action, objs...) \
    _nestedAction(nil, actor, action##Action, ##objs)

/*
 *   Run a new action.  This is a brand new action run as a separate turn,
 *   not as a nested action.  This doesn't replace any current action, but is
 *   simply a separate action.
 *   
 *   This is normally used only for internal actions that are run between
 *   other actions.  This should not normally be used while another action is
 *   being processed - use nestedAction for that instead.  This should also
 *   not normally be used to replace the current action with another - use
 *   replaceAction for that.
 *   
 *   Returns a CommandTranscript object, which provides information on the
 *   results of the action.  
 */
#define newAction(action, objs...) \
    _newAction(CommandTranscript, nil, gActor, action##Action, ##objs)

/* run a new action with a specific actor */
#define newActorAction(actor, action, objs...) \
    _newAction(CommandTranscript, nil, actor, action##Action, ##objs)

/*
 *   Ask for a direct object and retry the command using the single-object
 *   phrasing.  This can be used in the action() routine for a no-object
 *   command to ask for the missing direct object.  
 *   
 *   In many cases, there is simply no grammar rule for a zero-object form
 *   of a verb; in such cases, this macro is not needed, since the missing
 *   object is handled via the grammar.  However, for some actions, it is
 *   desirable to allow the zero-object phrasing some of the time, but
 *   require the direct-object phrasing other times.  This macro exists for
 *   these cases, because it allows the intransitive version of the action
 *   to decide, on a case-by-case basis, whether to process the no-object
 *   form of the command or to prompt for a direct object.
 *   
 *   newAction is the root name (without the Action suffix) of the
 *   transitive action to execute.  For example, if we're processing a plain
 *   "in" command, we could use askForDobj(Enter) to ask for a direct object
 *   for the transitive "enter" phrasing. 
 */
#define askForDobj(newAction) \
    (newAction##Action.retryWithMissingDobj(gAction, ResolveAsker))

/*
 *   Ask for an indirect object and retry the command using the two-object
 *   phrasing.  This can be used in the action() routine of a single-object
 *   command to ask for the missing indirect object.
 *   
 *   In many cases, there is simply no grammar rule for a single-object form
 *   of a verb; in such cases, this macro is not needed, since the missing
 *   object is handled via the grammar.  However, for some actions, it is
 *   desirable to allow the single-object phrasing some of the time, but
 *   require the two-object phrasing other times.  This macro exists for
 *   these cases, because it allows the action() routine to decide, on a
 *   case-by-case basis, whether to process the single-object form of the
 *   command or to prompt for an indirect object.
 *   
 *   newAction is the root name (without the Action suffix) of the
 *   two-object form of the action.  For example, if we're processing a
 *   single-object "unlock" command, we would use askForIobj(UnlockWith) to
 *   ask for an indirect object for the "unlock with" two-object phrasing.  
 */
#define askForIobj(newAction) \
    (newAction##Action.retryWithMissingIobj(gAction, ResolveAsker))

/*
 *   Ask for a literal phrase and retry the command using the two-object
 *   phrasing.  This is analogous to askForDobj() and askForIobj(), but for
 *   literal phrases; we effectively convert a TAction into a
 *   LiteralTAction.  
 */
#define askForLiteral(newAction) \
    (newAction##Action.retryWithMissingLiteral(gAction))

/*
 *   Ask for a topic phrase and retry the command using the two-object
 *   phrasing. 
 */
#define askForTopic(newAction) \
    (newAction##Action.retryWithMissingTopic(gAction))

/* ------------------------------------------------------------------------ */
/*
 *   Command interruption signal macros.  
 */

/* a concise macro to throw an ExitSignal */
#define exit throw new ExitSignal()

/* a concise macro to throw an ExitActionSignal */
#define exitAction throw new ExitActionSignal()

/* a concise macro to throw an AbortImplicitSignal */
#define abortImplicit throw new AbortImplicitSignal()


/* ------------------------------------------------------------------------ */
/*
 *   Flags for LOOK AROUND styles 
 */

/* show the room name as part of the description */
#define LookRoomName       0x0001

/* show the room's long desription (the roomDesc) */
#define LookRoomDesc       0x0002

/* show the non-portable items (the specialDesc's) */
#define LookListSpecials   0x0004

/* show the portable items */
#define LookListPortables  0x0008


/* ------------------------------------------------------------------------ */
/*
 *   Template for multi-location objects.  To put a MultiLoc object in
 *   several initial locations, simply use a template giving the list of
 *   locations.  
 */
MultiLoc template [locationList];


/* ------------------------------------------------------------------------ */
/*
 *   Templates for style tags 
 */
StyleTag template 'tagName' 'openText'? 'closeText'?;


/* ------------------------------------------------------------------------ */
/*
 *   A template for footnotes - all we usually need to define in a footnote
 *   is its descriptive text, so this makes it easy to define one.  
 */
Footnote template "desc";

/* footnote status levels */
enum FootnotesOff, FootnotesMedium, FootnotesFull;


/* ------------------------------------------------------------------------ */
/*
 *   An achievement defines its descriptive text.  It can also optionally
 *   define the number of points it awards.  
 */
Achievement template +points? "desc";


/* ------------------------------------------------------------------------ */
/* 
 *   An event list takes a list of strings, objects, and/or functions.
 */
EventList template [eventList];

/* 
 *   A shuffled event list with two lists - the first list is the sequential
 *   initial list, fired in the exact order specified; and the second is the
 *   random list, with the events that occur in shuffled order after we
 *   exhaust the initial list.  
 */
ShuffledEventList template [firstEvents] [eventList];

/* a synchronized event list takes its state from another list */
SyncEventList template ->masterObject inherited;

/* low-level shuffled list */
ShuffledList template [valueList];


/* ------------------------------------------------------------------------ */
/*
 *   Define a template for the Tip class.
 */
Tip template "desc";


/* ------------------------------------------------------------------------ */
/*
 *   Definitions for the menu system
 */

/* 
 *   The indices for the key values used to navigate menus, which are held
 *   in the keyList array of MenuItems.  
 */
#define M_QUIT      1
#define M_PREV      2
#define M_UP        3
#define M_DOWN      4
#define M_SEL       5

/* some templates for defining menu items */
MenuItem template 'title' 'heading'?;
MenuTopicItem template 'title' 'heading'? [menuContents];
MenuLongTopicItem template 'title' 'heading'? 'menuContents';

/* templates for hint system objects */
Goal template ->closeWhenAchieved? 'title' 'heading'? [menuContents];
Hint template 'hintText' [referencedGoals]?;

/* ------------------------------------------------------------------------ */
/*
 *   Templates for topic database entries.  
 */

/*
 *   A TopicEntry can be defined with an optional score, followed by the
 *   match criteria (which can be either a single matching object, a list of
 *   matching objects, or a regular expression pattern string), followed by
 *   the optional response text (which can be given either as a double-quoted
 *   string or as a list of single-quoted strings to use as an EventList).
 */
TopicEntry template
   +matchScore?
   @matchObj | [matchObj] | 'matchPattern'
   "topicResponse" | [eventList] ?;

/* a ShuffledEventList version of the above */
TopicEntry template
   +matchScore?
   @matchObj | [matchObj] | 'matchPattern'
   [firstEvents] [eventList];

/* we can also include *both* the match object/list *and* pattern */
TopicEntry template
   +matchScore?
   @matchObj | [matchObj]
   'matchPattern'
   "topicResponse" | [eventList] ?;

/* a ShuffledEventList version of the above */
TopicEntry template
   +matchScore?
   @matchObj | [matchObj]
   'matchPattern'
   [firstEvents] [eventList];

/* miscellanous topics just specify the response text or list */
MiscTopic template "topicResponse" | [eventList];
MiscTopic template [firstEvents] [eventList];

/* 
 *   A SpecialTopic takes a keyword list or a regular expression instead of
 *   the regular match criteria.  It also takes a suggestion name string and
 *   the normal response text.  There's no need for a score in a special
 *   topic, since these are unique.  
 */
SpecialTopic template
   'name'
   [keywordList] | 'matchPat'
   "topicResponse" | [eventList] ?;

/* a ShuffledEventList version of the above */
SpecialTopic template
   'name'
   [keywordList] | 'matchPat'
   [firstEvents] [eventList];

/* default topics just specify the response text */
DefaultTopic template "topicResponse" | [eventList];
DefaultTopic template [firstEvents] [eventList];

/* alternative topics just specify the response string or strings */
AltTopic template "topicResponse" | [eventList];
AltTopic template [firstEvents] [eventList];

/* a TopicGroup can specify its score adjustment */
TopicGroup template +matchScoreAdjustment;

/* a conversation node need a name */
ConvNode template 'name';

/*
 *   End-of-conversation reason codes 
 */
enum endConvBye;                                    /* player typed GOODBYE */
enum endConvTravel;         /* the other character is trying to travel away */
enum endConvBoredom;                /* our attentionSpan has been exhausted */
enum endConvActor;     /* the NPC itself (not the player) is saying GOODBYE */

/*
 *   Special result code for Actor.canEndConversation() - this indicates that
 *   the other actor said something to force the conversation to keep going. 
 */
enum blockEndConv;

/* ------------------------------------------------------------------------ */
/*
 *   Conversation manager macros
 */

/* has a topic key been revealed through <.reveal>? */
#define gRevealed(key) (conversationManager.revealedNameTab[key] != nil)

/* reveal a topic key, as though through <.reveal> */
#define gReveal(key) (conversationManager.setRevealed(key))

/* mark a Topic/Thing as known/seen by the player character */
#define gSetKnown(obj) (gPlayerChar.setKnowsAbout(obj))
#define gSetSeen(obj) (gPlayerChar.setHasSeen(obj))


/* ------------------------------------------------------------------------ */
/*
 *   For compatibility with versions before 3.1.1, define
 *   openableContentsLister as a synonym for openableDescContentsLister.  The
 *   former was renamed to the latter in 3.1.1 because the original name was
 *   inconsistent with the corresponding listers for other classes.  In
 *   principle, openableContentsLister is meant to be the 'contentsLister'
 *   (for displaying the openable's contents in room descriptions, etc) for
 *   an Openable, while openableDescContentsLister is its
 *   'descContentsLister' (for displaying the openable's contents in its own
 *   EXAMINE description).  Fortunately we don't have a need for a special
 *   contentsLister for Openable, so we can avoid breaking existing code by
 *   mapping the old name to the new name.
 */
#define openableContentsLister openableDescContentsLister


#endif /* ADV3_H */
