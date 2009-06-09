/*----------------------------------------------------------------------*\

  exe.c

  Amachine instruction execution unit of Alan interpreter

\*----------------------------------------------------------------------*/

#include "sysdep.h"
#include "types.h"
#include "act.h"
#include "set.h"
#include "state.h"
#include "debug.h"
#include "params.h"
#include "parse.h"
#include "syserr.h"

#include "exe.h"

#ifdef USE_READLINE
#include "readline.h"
#endif


#ifdef HAVE_GLK
#include "glk.h"
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif

#include "main.h"
#include "parse.h"
#include "inter.h"
#include "act.h"
#include "stack.h"
#include "decode.h"

#include "exe.h"

#define WIDTH 80


/* Forward: */
void describeInstances(void);


/*----------------------------------------------------------------------*/
static void printMessageUsingParameter(MsgKind message, int i) {
  setupParameterForInstance(1, i);
  printMessage(message);
  restoreParameters();
}

/*----------------------------------------------------------------------*/
static void printMessageUsing2Parameters(MsgKind message, int instance1,
					 int instance2) {
  setupParameterForInstance(1, instance1);
  setupParameterForInstance(2, instance2);
  printMessage(message);
  restoreParameters();
}


/*======================================================================*/
void setStyle(Aint style)
{
#ifdef HAVE_GLK
  switch (style) {
  case NORMAL_STYLE: glk_set_style(style_Normal); break;
  case EMPHASIZED_STYLE: glk_set_style(style_Emphasized); break;
  case PREFORMATTED_STYLE: glk_set_style(style_Preformatted); break;
  case ALERT_STYLE: glk_set_style(style_Alert); break;
  case QUOTE_STYLE: glk_set_style(style_BlockQuote); break;
  }
#endif
}

/*======================================================================*/
void print(Aword fpos, Aword len)
{
  char str[2*WIDTH];            /* String buffer */
  int outlen = 0;               /* Current output length */
  int ch = 0;
  int i;
  long savfp = 0;		/* Temporary saved text file position */
  static Bool printFlag = FALSE; /* Printing already? */
  Bool savedPrintFlag = printFlag;
  void *info = NULL;		/* Saved decoding info */


  if (len == 0) return;

  if (isHere(HERO, TRUE)) {	/* Check if the player will see it */
    if (printFlag) {            /* Already printing? */
      /* Save current text file position and/or decoding info */
      if (header->pack)
	info = pushDecode();
      else
	savfp = ftell(textFile);
    }
    printFlag = TRUE;           /* We're printing now! */

    /* Position to start of text */
    fseek(textFile, fpos+header->stringOffset, 0);

    if (header->pack)
      startDecoding();
    for (outlen = 0; outlen != len; outlen = outlen + strlen(str)) {
      /* Fill the buffer from the beginning */
      for (i = 0; i <= WIDTH || (i > WIDTH && ch != ' '); i++) {
	if (outlen + i == len)  /* No more characters? */
	  break;
	if (header->pack)
	  ch = decodeChar();
	else
	  ch = getc(textFile);
	if (ch == EOFChar)      /* Or end of text? */
	  break;
	str[i] = ch;
      }
      str[i] = '\0';
#if ISO == 0
      fromIso(str, str);
#endif
      output(str);
    }

    /* And restore */
    printFlag = savedPrintFlag;
    if (printFlag) {
      if (header->pack)
	popDecode(info);
      else
	fseek(textFile, savfp, 0);
    }
  }
}


/*======================================================================*/
void sys(Aword fpos, Aword len)
{
  char *command;

  command = getStringFromFile(fpos, len);
  int tmp = system(command);
  free(command);
}


/*======================================================================*/
char *getStringFromFile(Aword fpos, Aword len)
{
  char *buf = allocate(len+1);
  char *bufp = buf;

  /* Position to start of text */
  fseek(textFile, fpos+header->stringOffset, 0);

  if (header->pack)
    startDecoding();
  while (len--)
    if (header->pack)
      *(bufp++) = decodeChar();
    else
      *(bufp++) = getc(textFile);

  /* Terminate string with zero */
  *bufp = '\0';

  return buf;
}



/*======================================================================*/
void score(Aword sc)
{
  if (sc == 0) {
    setupParameterForInteger(1, current.score);
    setupParameterForInteger(2, header->maximumScore);
    printMessage(M_SCORE);
  } else {
    current.score += scores[sc-1];
    scores[sc-1] = 0;
    gameStateChanged = TRUE;
  }
}


/*======================================================================*/
void visits(Aword v)
{
  current.visits = v;
}


/*======================================================================*/
Bool confirm(MsgKind msgno)
{
  char buf[80];

  /* This is a bit of a hack since we really want to compare the input,
     it could be affirmative, but for now any input is NOT! */
  printMessage(msgno);

#ifdef USE_READLINE
  if (!readline(buf)) return TRUE;
#else
  if (gets(buf) == NULL) return TRUE;
#endif
  col = 1;

  return (buf[0] == '\0');
}


/*======================================================================*/
void quitGame(void)
{
  char buf[80];

  if (gameStateChanged) {
    rememberCommands();
    pushGameState();
  }

  current.location = where(HERO, TRUE);
  para();
  while (TRUE) {
    col = 1;
    statusline();
    printMessage(M_QUITACTION);
#ifdef USE_READLINE
    if (!readline(buf)) terminate(0);
#else
    if (gets(buf) == NULL) terminate(0);
#endif
    if (strcmp(buf, "restart") == 0)
      longjmp(restartLabel, TRUE);
    else if (strcmp(buf, "restore") == 0) {
      restore();
      return;
    } else if (strcmp(buf, "quit") == 0) {
      terminate(0);
    } else if (strcmp(buf, "undo") == 0) {
      undo();
    }
  }
  syserr("Fallthrough in QUIT");
}



/*======================================================================*/
void restartGame(void)
{
  Aint previousLocation = current.location;

  current.location = where(HERO, TRUE);
  para();
  if (confirm(M_REALLY)) {
    longjmp(restartLabel, TRUE);
  }
  current.location = previousLocation;
}



/*======================================================================*/
void cancelEvent(Aword evt)
{
  int i;

  for(i = eventQueueTop-1; i>=0; i--)
    if (eventQueue[i].event == evt) {
      while (i < eventQueueTop-1) {
	eventQueue[i].event = eventQueue[i+1].event;
	eventQueue[i].after = eventQueue[i+1].after;
	eventQueue[i].where = eventQueue[i+1].where;
	i++;
      }
      eventQueueTop--;
      return;
    }
}


/*======================================================================*/
void increaseEventQueue(void)
{
  eventQueue = realloc(eventQueue, (eventQueueTop+2)*sizeof(EventQueueEntry));
  if (eventQueue == NULL) syserr("Out of memory in increaseEventQueue()");

  eventQueueSize = eventQueueTop + 2;
}


/*======================================================================*/
void schedule(Aword event, Aword where, Aword after)
{  int i;

   if (event == 0) syserr("NULL event");
  
   cancelEvent(event);
   /* Check for overflow */
   if (eventQueue == NULL || eventQueueTop == eventQueueSize)
     increaseEventQueue();
  
   /* Bubble this event down */
   for (i = eventQueueTop; i >= 1 && eventQueue[i-1].after <= after; i--) {
     eventQueue[i].event = eventQueue[i-1].event;
     eventQueue[i].after = eventQueue[i-1].after;
     eventQueue[i].where = eventQueue[i-1].where;
   }
  
   eventQueue[i].after = after;
   eventQueue[i].where = where;
   eventQueue[i].event = event;
   eventQueueTop++;
}



/*======================================================================*/
AttributeEntry *findAttribute(AttributeEntry *attributeTable,
			      Aint attributeCode)
{
  AttributeEntry *attribute = attributeTable;
  while (attribute->code != attributeCode) {
    attribute++;
    if (endOfTable(attribute))
      syserr("Attribute not found.");
  }
  return attribute;
}




/*======================================================================*/
Aword getAttribute(AttributeEntry *attributeTable, Aint attributeCode)
{
  AttributeEntry *attribute = findAttribute(attributeTable, attributeCode);

  return attribute->value;
}
  

/*======================================================================*/
void setAttribute(AttributeEntry *attributeTable,
		  Aint attributeCode,
		  Aword newValue)
{
  AttributeEntry *attribute = findAttribute(attributeTable, attributeCode);

  attribute->value = newValue;
  gameStateChanged = TRUE;
}


/*======================================================================*/
void setValue(Aint id, Aint atr, Aword val)
{
  char str[80];

  if (id > 0 && id <= header->instanceMax) {
    setAttribute(admin[id].attributes, atr, val);
    if (isLocation(id))	/* May have changed so describe next time */
      admin[id].visitsCount = 0;
  } else {
    sprintf(str, "Can't SET/MAKE instance (%ld).", (long) id);
    syserr(str);
  }
}


/*======================================================================*/
void setStringAttribute(Aint id, Aint atr, char *str)
{
  free((char *)attributeOf(id, atr));
  setValue(id, atr, (Aword)str);
}


/*======================================================================*/
void setSetAttribute(Aint id, Aint atr, Aword set)
{
  freeSet((Set *)attributeOf(id, atr));
  setValue(id, atr, (Aword)set);
}


/*----------------------------------------------------------------------*/
static Aword literalAttribute(Aword lit, Aint atr)
{
  char str[80];

  if (atr == 1)
    return literal[literalFromInstance(lit)].value;
  else {
    sprintf(str, "Unknown attribute for literal (%ld).", (long) atr);
    syserr(str);
  }
  return(EOF);
}


/*======================================================================*/
Aword attributeOf(Aint id, Aint atr)
{
  char str[80];

  if (isLiteral(id))
    return literalAttribute(id, atr);
  else {
    if (id > 0 && id <= header->instanceMax)
      return getAttribute(admin[id].attributes, atr);
    else {
      sprintf(str, "Can't ATTRIBUTE item (%ld).", (long) id);
      syserr(str);
    }
  }
  return(EOF);
}


/*======================================================================*/
Aword getStringAttribute(Aint id, Aint atr)
{
  return (Aword) strdup((char *)attributeOf(id, atr));
}


/*======================================================================*/
Aword getSetAttribute(Aint id, Aint atr)
{
  return (Aword) copySet((Set *)attributeOf(id, atr));
}


/*======================================================================*/
Aword concat(Aword s1, Aword s2)
{
  char *result = allocate(strlen((char*)s1)+strlen((char*)s2)+1);
  strcpy(result, (char*)s1);
  strcat(result, (char*)s2);
  free((char*)s1);
  free((char*)s2);
  return (Aword)result;
}


/*----------------------------------------------------------------------*/
static char *stripCharsFromStringForwards(int count, char *initialString, char **theRest)
{
  int stripPosition;
  char *strippedString;
  char *rest;

  if (count > strlen(initialString))
    stripPosition = strlen(initialString);
  else
    stripPosition = count;
  rest = strdup(&initialString[stripPosition]);
  strippedString = strdup(initialString);
  strippedString[stripPosition] = '\0';
  *theRest = rest;
  return strippedString;
}

/*----------------------------------------------------------------------*/
static char *stripCharsFromStringBackwards(Aint count, char *initialString, char **theRest) {
  int stripPosition;
  char *strippedString;
  char *rest;

  if (count > strlen(initialString))
    stripPosition = 0;
  else
    stripPosition = strlen(initialString)-count;
  strippedString = strdup(&initialString[stripPosition]);
  rest = strdup(initialString);
  rest[stripPosition] = '\0';
  *theRest = rest;
  return strippedString;
}


/*----------------------------------------------------------------------*/
static int countLeadingBlanks(char *string, int position) {
  static char blanks[] = " ";
  return strspn(&string[position], blanks);
}

/*----------------------------------------------------------------------*/
static int skipWordForwards(char *string, int position)
{
  char separators[] = " .,?";

  int i;

  for (i = position; i<=strlen(string) && strchr(separators, string[i]) == NULL; i++)
    ;
  return i;
}


/*----------------------------------------------------------------------*/
static char *stripWordsFromStringForwards(Aint count, char *initialString, char **theRest) {
  int skippedChars;
  int position = 0;
  char *stripped;
  int i;

  for (i = count; i>0; i--) {
    /* Ignore any initial blanks */
    skippedChars = countLeadingBlanks(initialString, position);
    position += skippedChars;
    position = skipWordForwards(initialString, position);
  }

  stripped = (char *)allocate(position+1);
  strncpy(stripped, initialString, position);
  stripped[position] = '\0';

  skippedChars = countLeadingBlanks(initialString, position);
  *theRest = strdup(&initialString[position+skippedChars]);

  return(stripped);
}


/*----------------------------------------------------------------------*/
static int skipWordBackwards(char *string, int position)
{
  char separators[] = " .,?";
  int i;

  for (i = position; i>0 && strchr(separators, string[i-1]) == NULL; i--)
    ;
  return i;
}


/*----------------------------------------------------------------------*/
static int countTrailingBlanks(char *string, int position) {
  int skippedChars, i;
  skippedChars = 0;

  if (position > strlen(string)-1)
    syserr("position > length in countTrailingBlanks");
  for (i = position; i >= 0 && string[i] == ' '; i--)
    skippedChars++;
  return(skippedChars);
}


/*----------------------------------------------------------------------*/
static char *stripWordsFromStringBackwards(Aint count, char *initialString, char **theRest) {
  int skippedChars;
  char *stripped;
  int strippedLength;
  int position = strlen(initialString);
  int i;

  for (i = count; i>0 && position>0; i--) {
    position -= 1;
    /* Ignore trailing blanks */
    skippedChars = countTrailingBlanks(initialString, position);
    if (position - skippedChars < 0) break; /* No more words to strip */
    position -= skippedChars;
    position = skipWordBackwards(initialString, position);
  }

  skippedChars = countLeadingBlanks(initialString, 0);
  strippedLength = strlen(initialString)-position-skippedChars;
  stripped = (char *)allocate(strippedLength+1);
  strncpy(stripped, &initialString[position+skippedChars], strippedLength);
  stripped[strippedLength] = '\0';

  if (position > 0) {
    skippedChars = countTrailingBlanks(initialString, position-1);
    position -= skippedChars;
  }
  *theRest = strdup(initialString);
  (*theRest)[position] = '\0';
  return(stripped);
}



/*======================================================================*/
Aword strip(Abool stripFromBeginningNotEnd, Aint count, Abool stripWordsNotChars, Aint id, Aint atr)
{
  char *initialString = (char *)attributeOf(id, atr);
  char *theStripped;
  char *theRest;

  if (stripFromBeginningNotEnd) {
    if (stripWordsNotChars)
      theStripped = stripWordsFromStringForwards(count, initialString, &theRest);
    else
      theStripped = stripCharsFromStringForwards(count, initialString, &theRest);
  } else {
    if (stripWordsNotChars)
      theStripped = stripWordsFromStringBackwards(count, initialString, &theRest);
    else
      theStripped = stripCharsFromStringBackwards(count, initialString, &theRest);
  }
  setStringAttribute(id, atr, theRest);
  return (Aword)theStripped;
}


/*----------------------------------------------------------------------*/
static void verifyId(Aint id, char action[]) {
  char message[200];

  if (id == 0) {
    sprintf(message, "Can't %s instance (%ld).", action, (long) id);
    syserr(message);
  } else if (id > header->instanceMax) {
    sprintf(message, "Can't %s instance (%ld > instanceMax).", action, (long) id);
    syserr(message);
  }
}


/*======================================================================*/
/* Return the current position of an instance, directly or not */
Aword where(Aint id, Abool directly)
{
  verifyId(id, "WHERE");

  if (isLocation(id))
    return 0;
  else if (directly)
    return admin[id].location;
  else
    return location(id);
}



/*======================================================================*/
/* Return the *location* of an instance, transitively, i.e. not directly */
Aword location(Aint id)
{
  int loc;
  int container = 0;

  verifyId(id, "LOCATION");

  loc = admin[id].location;
  while (loc != 0 && !isLocation(loc)) {
    container = loc;
    loc = admin[loc].location;
  }
  if (loc != 0)
    return loc;
  else {
    if (container == 0)
      if (!isA(id, THING) && !isLocation(id))
	return location(HERO);
      else
	return 0;		/* Nowhere */
    else
      if (!isA(container, THING) && !isLocation(container))
	return location(HERO);
      else
	return 0;		/* Nowhere */
  }
}


/*======================================================================*/
Aint containerSize(Aint container, Abool directly) {
  Aint i;
  Aint count = 0;

  for (i = 1; i <= header->instanceMax; i++) {
    if (in(i, container, directly))
      count++;
  }
  return(count);
}


/*======================================================================*/
Aint getContainerMember(Aint container, Aint index, Abool directly) {
  Aint i;
  Aint count = 0;

  for (i = 1; i <= header->instanceMax; i++) {
    if (in(i, container, directly)) {
      count++;
      if (count == index)
	return i;
    }
  }
  syserr("Index not in container in 'containerMember()'");
  return 0;
}

/*----------------------------------------------------------------------*/
static void locateIntoContainer(Aword theInstance, Aword theContainer) {
  if (!isA(theInstance, container[instance[theContainer].container].class))
    printMessageUsing2Parameters(M_CANNOTCONTAIN, theContainer, theInstance);
  else if (checklim(theContainer, theInstance))
    error(MSGMAX);		/* Return to player without any message */
  else
    admin[theInstance].location = theContainer;
}


/*----------------------------------------------------------------------*/
static void locateLocation(Aword loc, Aword whr)
{
  Aint l = whr;

  /* Ensure this does not create a recursive location chain */
  while (l != 0) {
    if (admin[l].location == loc)
      apperr("Locating a location would create a recursive loop of locations containing each other.");
    else
      l = admin[l].location;
  }
  admin[loc].location = whr;
}


/*----------------------------------------------------------------------*/
static void locateObject(Aword obj, Aword whr)
{
  if (isContainer(whr)) { /* Into a container */
    locateIntoContainer(obj, whr);
  } else {
    admin[obj].location = whr;
    /* Make sure the location is described since it's changed */
    admin[whr].visitsCount = 0;
  }
}


/*----------------------------------------------------------------------*/
static void executeInheritedEntered(Aint theClass) {
  if (theClass == 0) return;
  if (class[theClass].entered)
    interpret(class[theClass].entered);
  else
    executeInheritedEntered(class[theClass].parent);
}


/*----------------------------------------------------------------------*/
static void locateActor(Aword movingActor, Aword whr)
{
  Aint previousCurrentLocation = current.location;
  Aint previousActorLocation = admin[movingActor].location;
  Aint previousActor = current.actor;
  Aint previousInstance = current.instance;

  /* FIXME: Actors locating into containers is dubious, anyway as it
   is now it allows the hero to be located into a container. And what
   happens with current location if so... */
  if (isContainer(whr))
    locateIntoContainer(movingActor, whr);
  else {
    current.location = whr;
    admin[movingActor].location = whr;
  }

  /* Now we have moved so show what is needed... */
  current.instance = current.location;

  if (movingActor == HERO) {
    if (admin[admin[movingActor].location].visitsCount % (current.visits+1) == 0)
      look();
    else {
      if (anyOutput)
	para();
      say(where(HERO, TRUE));
      printMessage(M_AGAIN);
      newline();
      describeInstances();
    }
    admin[where(HERO, TRUE)].visitsCount++;
    admin[where(HERO, TRUE)].visitsCount %= (current.visits+1);
  } else
    admin[whr].visitsCount = 0;

  /* And execute possible entered */
  current.actor = movingActor;
  if (instance[current.location].entered != 0) {
    if (previousActorLocation != current.location) {
      interpret(instance[current.location].entered);
      current.instance = previousInstance;
    }
  } else
    executeInheritedEntered(instance[current.location].parent);
  current.actor = previousActor;

  if (current.actor != movingActor)
    current.location = previousCurrentLocation;

  current.instance = previousInstance;
}


/*======================================================================*/
void locate(Aint id, Aword whr)
{
  Aword containerId;
  ContainerEntry *theContainer;
  Aword previousInstance = current.instance;

  verifyId(id, "LOCATE");
  verifyId(whr, "LOCATE AT");

  /* First check if the instance is in a container, if so run extract checks */
  if (isContainer(admin[id].location)) {    /* In something? */
    current.instance = admin[id].location;
    containerId = instance[admin[id].location].container;
    theContainer = &container[containerId];

    if (theContainer->extractChecks != 0) {
      if (sectionTraceOption) {
	printf("\n<EXTRACT from ");
	traceSay(id);
	printf("(%ld, container %ld), Checking:>\n", id, containerId);
      }
      if (!trycheck(theContainer->extractChecks, EXECUTE)) {
	fail = TRUE;
	return;
      }
      current.instance = previousInstance;
    }
    if (theContainer->extractStatements != 0) {
      if (sectionTraceOption) {
	printf("\n<EXTRACT from ");
	traceSay(id);
	printf("(%ld, container %ld), Executing:>\n", id, containerId);
      }
      interpret(theContainer->extractStatements);
    }
  }
    
  if (isActor(id))
    locateActor(id, whr);
  else if (isLocation(id))
    locateLocation(id, whr);
  else
    locateObject(id, whr);

  gameStateChanged = TRUE;
}


/*----------------------------------------------------------------------*/


/*======================================================================*/
Abool isHere(Aint id, Abool directly)
{
  verifyId(id, "HERE");

  if (directly)
    return(admin[id].location == current.location);
  else
    return at(id, current.location, directly);
}


/*======================================================================*/
Abool isNearby(Aint instance, Abool directly)
{
  verifyId(instance, "NEARBY");

  if (isLocation(instance))
    return exitto(current.location, instance);
  else
    return exitto(current.location, where(instance, directly));
}


/*======================================================================*/
Abool isNear(Aint id, Aint other, Abool directly)
{
  Aint l1, l2;

  verifyId(id, "NEAR");

  if (isLocation(id))
    l1 = id;
  else
    l1 = where(id, directly);
  if (isLocation(other))
    l2 = other;
  else
    l2 = where(other, directly);
  return exitto(l2, l1);
}


/*======================================================================*/
Abool isA(Aint instanceId, Aint ancestor)
{
  int parent;

  if (isLiteral(instanceId))
    parent = literal[instanceId-header->instanceMax].class;
  else
    parent = instance[instanceId].parent;
  while (parent != 0 && parent != ancestor)
    parent = class[parent].parent;

  return (parent != 0);
}



/*======================================================================*/
/* Look in a container to see if the instance is in it. */
Abool in(Aint theInstance, Aint container, Abool directly)
{
  int loc;

  if (!isContainer(container))
    syserr("IN in a non-container.");

  if (directly)
    return admin[theInstance].location == container;
  else {
    loc = admin[theInstance].location;
    while (loc != 0)
      if (loc == container)
	return TRUE;
      else
	loc = admin[loc].location;
    return FALSE;
  }
}



/*======================================================================*/
/* Look see if an instance is AT another. */
Abool at(Aint theInstance, Aint other, Abool directly)
{
  if (theInstance == 0 || other == 0) return FALSE;

  if (directly) {
    if (isLocation(other))
      return admin[theInstance].location == other;
    else
      return admin[theInstance].location == admin[other].location;
  } else {
    if (!isLocation(other))
      /* If the other is not a location, compare their locations */
      return location(theInstance) == location(other);
    else if (location(theInstance) == other)
      return TRUE;
    else if (location(other) != 0)
      return at(theInstance, location(other), FALSE);
    else
      return FALSE;
  }
}


/*----------------------------------------------------------------------*/
static Abool executeInheritedMentioned(Aword theClass) {
  if (theClass == 0) return FALSE;

  if (class[theClass].mentioned) {
    interpret(class[theClass].mentioned);
    return TRUE;
  } else
    return executeInheritedMentioned(class[theClass].parent);
}


/*----------------------------------------------------------------------*/
static Abool mention(Aint id) {
  if (instance[id].mentioned) {
    interpret(instance[id].mentioned);
    return TRUE;
  } else
    return executeInheritedMentioned(instance[id].parent);
}


/*----------------------------------------------------------------------*/
static void sayLiteral(Aword lit)
{
  char *str;

  if (isNumeric(lit))
    sayInteger(literal[lit-header->instanceMax].value);
  else {
    str = (char *)strdup((char *)literal[lit-header->instanceMax].value);
    sayString(str);
  }    
}

	
/*======================================================================*/
void sayInstance(Aint id)
{
#ifdef SAY_INSTANCE_WITH_PLAYER_WORDS_IF_PARAMETER
  int p, i;

  /* Find the id in the parameters... */
  if (params != NULL)
    for (p = 0; params[p].code != EOF; p++)
      if (params[p].code == id) {
	/* Found it so.. */
	if (params[p].firstWord == EOF) /* Any words he used? */
	  break;		/* No... */
	else {				/* Yes, so use them... */ 
	  char *capitalized;
	  /* Assuming the noun is the last word we can simply output the adjectives... */
	  for (i = params[p].firstWord; i <= params[p].lastWord-1; i++)
	    output((char *)pointerTo(dict[wrds[i]].wrd));
	  /* ... and then the noun, capitalized if necessary */
	  if (header->capitalizeNouns) {
	    capitalized = strdup((char *)pointerTo(dict[wrds[params[p].lastWord]].wrd));
	    capitalized[0] = IsoToUpperCase(capitalized[0]);
	    output(capitalized);
	    free(capitalized);
	  } else
	    output((char *)pointerTo(dict[wrds[params[p].lastWord]].wrd));
	}
	return;
      }
#endif
  if (!mention(id))
    interpret(instance[id].name);
}


/*======================================================================*/
void sayInteger(Aword val)
{
  char buf[25];

  if (isHere(HERO, FALSE)) {
    sprintf(buf, "%ld", (long) val);
    output(buf);
  }
}


/*======================================================================*/
void sayString(char *str)
{
  if (isHere(HERO, FALSE))
    output(str);
  free(str);
}


/*----------------------------------------------------------------------*/
static char *wordWithCode(Aint classBit, Aint code) {
  int w;
  char str[50];

  for (w = 0; w < dictsize; w++)
    if (dictionary[w].code == code && ((classBit&dictionary[w].classBits) != 0))
      return pointerTo(dictionary[w].string);
  sprintf(str, "Could not find word of class %ld with code %ld.", (long) classBit, (long) code);
  syserr(str);
  return NULL;
}


/*----------------------------------------------------------------------*/
static Bool sayInheritedDefiniteForm(Aword theClass) {
  if (theClass == 0) {
    syserr("No default definite article");
    return FALSE;
  } else {
    if (class[theClass].definite.address) {
      interpret(class[theClass].definite.address);
      return class[theClass].definite.isForm;
    } else
      return sayInheritedDefiniteForm(class[theClass].parent);
  }
}


/*----------------------------------------------------------------------*/
static void sayDefinite(Aint id) {
  if (instance[id].definite.address) {
    interpret(instance[id].definite.address);
    if (!instance[id].definite.isForm)
      sayInstance(id);
  } else
    if (!sayInheritedDefiniteForm(instance[id].parent))
      sayInstance(id);
}


/*----------------------------------------------------------------------*/
static Bool sayInheritedIndefiniteForm(Aword theClass) {
  if (theClass == 0) {
    syserr("No default indefinite article");
    return FALSE;
  } else {
    if (class[theClass].indefinite.address) {
      interpret(class[theClass].indefinite.address);
      return class[theClass].indefinite.isForm;
    } else
      return sayInheritedIndefiniteForm(class[theClass].parent);
  }
}


/*----------------------------------------------------------------------*/
static void sayIndefinite(Aint id) {
  if (instance[id].indefinite.address) {
    interpret(instance[id].indefinite.address);
    if (!instance[id].indefinite.isForm)
      sayInstance(id);
  } else
    if (!sayInheritedIndefiniteForm(instance[id].parent))
      sayInstance(id);
}


/*----------------------------------------------------------------------*/
static Bool sayInheritedNegativeForm(Aword theClass) {
  if (theClass == 0) {
    syserr("No default negative form");
    return FALSE;
  } else {
    if (class[theClass].negative.address) {
      interpret(class[theClass].negative.address);
      return class[theClass].negative.isForm;
    } else
      return sayInheritedNegativeForm(class[theClass].parent);
  }
}


/*----------------------------------------------------------------------*/
static void sayNegative(Aint id) {
  if (instance[id].negative.address) {
    interpret(instance[id].negative.address);
    if (!instance[id].negative.isForm)
      sayInstance(id);
  } else
    if (!sayInheritedNegativeForm(instance[id].parent))
      sayInstance(id);
}


/*----------------------------------------------------------------------*/
static void sayInheritedPronoun(Aint id) {
  if (id == 0)
    syserr("No default pronoun");
  else {
    if (class[id].pronoun != 0)
      output(wordWithCode(PRONOUN_BIT, class[id].pronoun));
    else
      sayInheritedPronoun(class[id].parent);
  }
}


/*----------------------------------------------------------------------*/
static void sayPronoun(Aint id) {
  if (instance[id].pronoun != 0)
      output(wordWithCode(PRONOUN_BIT, instance[id].pronoun));
  else
    sayInheritedPronoun(instance[id].parent);
}


/*----------------------------------------------------------------------*/
static void sayArticleOrForm(Aint id, SayForm form)
{
  if (!isLiteral(id))
    switch (form) {
    case SAY_DEFINITE:
      sayDefinite(id);
      break;
    case SAY_INDEFINITE:
      sayIndefinite(id);
      break;
    case SAY_NEGATIVE:
      sayNegative(id);
      break;
    case SAY_PRONOUN:
      sayPronoun(id);
      break;
    case SAY_SIMPLE:
      say(id);
      break;
    default:
      syserr("Unexpected form in 'sayArticleOrForm()'");
    }
  else
    say(id);
}


/*======================================================================*/
void say(Aint id)
{
  Aword previousInstance = current.instance;
  current.instance = id;

  if (isHere(HERO, FALSE)) {
    if (isLiteral(id))
      sayLiteral(id);
    else {
      verifyId(id, "SAY");
      sayInstance(id);
    }
  }
  current.instance = previousInstance;
}


/*======================================================================*/
void sayForm(Aint id, SayForm form)
{
  Aword previousInstance = current.instance;
  current.instance = id;

  sayArticleOrForm(id, form);

  current.instance = previousInstance;
}


/***********************************************************************\

  Description Handling

\***********************************************************************/


FORWARD void list(Aword cnt);

/*----------------------------------------------------------------------*/
static Bool inheritedDescriptionCheck(Aint classId)
{
  if (classId == 0) return TRUE;
  if (!inheritedDescriptionCheck(class[classId].parent)) return FALSE;
  if (class[classId].descriptionChecks == 0) return TRUE;
  return trycheck(class[classId].descriptionChecks, TRUE);
}

/*----------------------------------------------------------------------*/
static Bool descriptionCheck(Aint instanceId)
{
  if (inheritedDescriptionCheck(instance[instanceId].parent)) {
    if (instance[instanceId].checks == 0) return TRUE;
    return trycheck(instance[instanceId].checks, TRUE);
  } else
    return FALSE;
}

/*----------------------------------------------------------------------*/
static Abool inheritsDescriptionFrom(Aword classId)
{
  if (class[classId].description != 0)
    return TRUE;
  else if (class[classId].parent != 0)
    return inheritsDescriptionFrom(class[classId].parent);
  else
    return FALSE;
}  

/*----------------------------------------------------------------------*/
static Abool hasDescription(Aword instanceId)
{
  if (instance[instanceId].description != 0)
    return TRUE;
  else if (instance[instanceId].parent != 0)
    return inheritsDescriptionFrom(instance[instanceId].parent);
  else
    return FALSE;
}

/*----------------------------------------------------------------------*/
static void describeClass(Aint id)
{
  if (class[id].description != 0) {
    /* This class has a description, run it */
    interpret(class[id].description);
  } else {
    /* Search up the inheritance tree, if any, to find a description */
    if (class[id].parent != 0)
      describeClass(class[id].parent);
  }
}


/*----------------------------------------------------------------------*/
static void describeAnything(Aint id)
{
  if (instance[id].description != 0) {
    /* This instance has its own description, run it */
    interpret(instance[id].description);
  } else {
    /* Search up the inheritance tree to find a description */
    if (instance[id].parent != 0)
      describeClass(instance[id].parent);
  }
  admin[id].alreadyDescribed = TRUE;
}


/*----------------------------------------------------------------------*/
static Bool describeable(Aint i) {
  return isObject(i) || isActor(i);
}


/*----------------------------------------------------------------------*/
static Bool containerIsEmpty(Aword cnt)
{
  int i;

  for (i = 1; i <= header->instanceMax; i++)
    if (describeable(i) && in(i, cnt, FALSE))
      return FALSE;
  return TRUE;
}


/*----------------------------------------------------------------------*/
static void describeContainer(Aint id)
{
  if (!containerIsEmpty(id) && !attributeOf(id, OPAQUEATTRIBUTE))
    list(id);
}    


/*----------------------------------------------------------------------*/
static void describeObject(Aword obj)
{
  if (hasDescription(obj))
    describeAnything(obj);
  else {
    printMessageUsingParameter(M_SEE_START, obj);
    printMessage(M_SEE_END);
    if (instance[obj].container != 0)
      describeContainer(obj);
  }
  admin[obj].alreadyDescribed = TRUE;
}


/*----------------------------------------------------------------------*/
static ScriptEntry *scriptOf(Aint act) {
  ScriptEntry *scr;

  if (admin[act].script != 0) {
    for (scr = (ScriptEntry *) pointerTo(header->scriptTableAddress); !endOfTable(scr); scr++)
      if (scr->code == admin[act].script)
	break;
    if (!endOfTable(scr))
      return scr;
  }
  return NULL;
}


/*----------------------------------------------------------------------*/
static StepEntry *stepOf(Aint act) {
  StepEntry *step;
  ScriptEntry *scr = scriptOf(act);

  if (scr == NULL) return NULL;

  step = (StepEntry*)pointerTo(scr->steps);
  step = &step[admin[act].step];

  return step;
}


/*----------------------------------------------------------------------*/
static void describeActor(Aword act)
{
  ScriptEntry *scr = scriptOf(act);

  if (scr != NULL && scr->description != 0)
    interpret(scr->description);
  else if (hasDescription(act))
    describeAnything(act);
  else {
    printMessageUsingParameter(M_SEE_START, act);
    printMessage(M_SEE_END);
    if (instance[act].container != 0)
      describeContainer(act);
  }
  admin[act].alreadyDescribed = TRUE;
}



static Bool descriptionOk;

/*======================================================================*/
void describe(Aint id)
{
  Aword previousInstance = current.instance;

  current.instance = id;
  verifyId(id, "DESCRIBE");
  if (descriptionCheck(id)) {
    descriptionOk = TRUE;
    if (isObject(id)) {
      describeObject(id);
    } else if (isActor(id)) {
      describeActor(id);
    } else
      describeAnything(id);
  } else
    descriptionOk = FALSE;
  current.instance = previousInstance;
}


/*======================================================================*/
void describeInstances(void)
{
  int i;
  int lastInstanceFound = 0;
  int found = 0;

  /* First describe every object here with its own description */
  for (i = 1; i <= header->instanceMax; i++)
    if (admin[i].location == current.location && isObject(i) &&
	!admin[i].alreadyDescribed && hasDescription(i))
      describe(i);

  /* Then list all things without a description */
  for (i = 1; i <= header->instanceMax; i++)
    if (admin[i].location == current.location
	&& !admin[i].alreadyDescribed
	&& isObject(i)) {
      if (found == 0)
	printMessageUsingParameter(M_SEE_START, i);
      else if (found > 1)
	printMessageUsingParameter(M_SEE_COMMA, lastInstanceFound);
      admin[i].alreadyDescribed = TRUE;

      if (instance[i].container && containerSize(i, TRUE) > 0 && !attributeOf(i, OPAQUEATTRIBUTE)) {
	if (found > 0)
	  printMessageUsingParameter(M_SEE_AND, i);
	printMessage(M_SEE_END);
	describeContainer(i);
	found = 0;
	continue;		/* Actually start another list. */
      }
      found++;
      lastInstanceFound = i;
    }

  if (found > 0) {
    if (found > 1) {
      printMessageUsingParameter(M_SEE_AND, lastInstanceFound);
    }
    printMessage(M_SEE_END);
  }
  
  /* Finally all actors with a separate description */
  for (i = 1; i <= header->instanceMax; i++)
    if (admin[i].location == current.location && i != HERO && isActor(i)
	&& !admin[i].alreadyDescribed)
      describe(i);

  /* Clear the describe flag for all instances */
  for (i = 1; i <= header->instanceMax; i++)
    admin[i].alreadyDescribed = FALSE;
}


/*======================================================================*/
void look(void)
{
  int i;

  /* Set describe flag for all objects and actors */
  for (i = 1; i <= header->instanceMax; i++)
    admin[i].alreadyDescribed = FALSE;

  if (anyOutput)
    para();

#ifdef HAVE_GLK
  glk_set_style(style_Subheader);
#endif

  sayInstance(current.location);

#ifdef HAVE_GLK
  glk_set_style(style_Normal);
#endif

  newline();
  capitalize = TRUE;
  describe(current.location);
  if (descriptionOk)
    describeInstances();
}


/*======================================================================*/
void list(Aword cnt)
{
  int i;
  Aword props;
  Aword foundInstance[2] = {0,0};
  int found = 0;
  Aint previousThis = current.instance;

  current.instance = cnt;

  /* Find container table entry */
  props = instance[cnt].container;
  if (props == 0) syserr("Trying to list something not a container.");

  for (i = 1; i <= header->instanceMax; i++) {
    if (describeable(i)) {
      /* We can only see objects and actors directly in this container... */
      if (admin[i].location == cnt) { /* Yes, it's in this container */
	if (found == 0) {
	  if (container[props].header != 0)
	    interpret(container[props].header);
	  else {
	    if (isActor(container[props].owner))
	      printMessageUsingParameter(M_CARRIES, container[props].owner);
	    else
	      printMessageUsingParameter(M_CONTAINS, container[props].owner);
	  }
	  foundInstance[0] = i;
	} else if (found == 1)
	  foundInstance[1] = i;
	else {
	  printMessageUsingParameter(M_CONTAINS_COMMA, i);
	}
	found++;
      }
    }
  }

  if (found > 0) {
    if (found > 1)
      printMessageUsingParameter(M_CONTAINS_AND, foundInstance[1]);
    printMessageUsingParameter(M_CONTAINS_END, foundInstance[0]);
  } else {
    if (container[props].empty != 0)
      interpret(container[props].empty);
    else {
      if (isActor(container[props].owner))
	printMessageUsingParameter(M_EMPTYHANDED, container[props].owner);
      else
	printMessageUsingParameter(M_EMPTY, container[props].owner);
    }
  }
  needSpace = TRUE;
  current.instance = previousThis;
}


/*======================================================================*/
void showImage(Aword image, Aword align)
{
#ifdef HAVE_GLK
  glui32 ecode;

  if ((glk_gestalt(gestalt_Graphics, 0) == 1) &&
      (glk_gestalt(gestalt_DrawImage, wintype_TextBuffer) == 1)) {
    glk_window_flow_break(glkMainWin);
    printf("\n");
    ecode = glk_image_draw(glkMainWin, image, imagealign_MarginLeft, 0);
  }
#endif
}    
 

/*======================================================================*/
void playSound(Aword sound)
{
#ifdef HAVE_GLK
#ifdef GLK_MODULE_SOUND
  static schanid_t soundChannel = NULL;
  glui32 ecode;

  if (glk_gestalt(gestalt_Sound, 0) == 1) {
    if (soundChannel == NULL)
      soundChannel = glk_schannel_create(0);
    if (soundChannel != NULL) {
      glk_schannel_stop(soundChannel);
      ecode = glk_schannel_play(soundChannel, sound);
    }
  }
#endif
#endif
}    
 


/*======================================================================*/
void empty(Aword cnt, Aword whr)
{
  int i;

  for (i = 1; i <= header->instanceMax; i++)
    if (in(i, cnt, TRUE))
      locate(i, whr);
}



/*======================================================================*/
void use(Aword act, Aword scr)
{
  char str[80];
  StepEntry *step;

  if (!isActor(act)) {
    sprintf(str, "Instance is not an Actor (%ld).", (long) act);
    syserr(str);
  }

  admin[act].script = scr;
  admin[act].step = 0;
  step = stepOf(act);
  if (step != NULL && step->after != 0) {
    interpret(step->after);
    admin[act].waitCount = pop();
  }

  gameStateChanged = TRUE;
}

/*======================================================================*/
void stop(Aword act)
{
  char str[80];

  if (!isActor(act)) {
    sprintf(str, "Instance is not an Actor (%ld).", (long) act);
    syserr(str);
  }

  admin[act].script = 0;
  admin[act].step = 0;

  gameStateChanged = TRUE;
}




/*----------------------------------------------------------------------*/
Aword randomInteger(Aword from, Aword to)
{
  if (to == from)
    return to;
  else if (to > from)
    return (rand()/10)%(to-from+1)+from;
  else
    return (rand()/10)%(from-to+1)+to;
}



/*----------------------------------------------------------------------*/
Abool btw(Aint val, Aint low, Aint high)
{
  if (high > low)
    return low <= val && val <= high;
  else
    return high <= val && val <= low;
}



/*======================================================================*/
Aword contains(Aword string, Aword substring)
{
  Abool found;

  strlow((char *)string);
  strlow((char *)substring);

  found = (strstr((char *)string, (char *)substring) != 0);

  free((char *)string);
  free((char *)substring);

  return(found);
}


/*======================================================================*/
Abool streq(char a[], char b[])
{
  Bool eq;

  strlow(a);
  strlow(b);

  eq = (strcmp(a, b) == 0);

  free(a);
  free(b);

  return(eq);
}
