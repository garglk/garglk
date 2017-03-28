#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines the tads-gen intrinsic function set.  This function
 *   set provides some miscellaneous functions, including data conversions,
 *   object iteration, regular expressions, and state persistence operations.
 */


/*
 *   The tads-gen function set.  This set contains basic data manipulation
 *   and miscellaneous utility functions.
 */
intrinsic 'tads-gen/030008'
{
    /*
     *   Get the type of the given value.  This returns a TypeXxx value.
     */
    dataType(val);

    /*
     *   Get the given parameter to the current function.  'idx' is 1 for the
     *   first argument in left-to-right order, 2 for the second, and so on. 
     */
    getArg(idx);

    /*
     *   Get the first object in memory.  If 'cls' is provided, we return the
     *   first object of the given class; otherwise we return the first
     *   object of any kind.  'flags' is an optional bitwise combination of
     *   ObjXxx values, specifying whether classes, instances, or both are
     *   desired.  If this isn't specified, ObjAll is assumed.  This is used
     *   in conjunction with nextObj() to iterate over all objects in memory,
     *   or all objects of a given class.
     *   
     *   Note that firstObj-nextObj loops can retrieve objects that aren't
     *   otherwise reachable, because unreachable objects are only removed
     *   from memory when the garbage collector runs, which happens
     *   intermittently.  If you want to ensure that any currently
     *   unreachable objects are removed from memory just before a
     *   firstObj-nextObj loop, you can do so by calling t3RunGC().
     */
    firstObj(cls?, flags?);

    /*
     *   Get the next object in memory after the given object, optionally of
     *   the given class and optionally limiting to instances, classes, or
     *   both.  This is used to continue an iteration started with
     *   firstObj().  
     */
    nextObj(obj, cls?, flags?);

    /*
     *   Random number generator (RNG) initialization.  This selects and
     *   initializes the random number generator algorithm used by rand().
     *   TADS provides several different RNG algorithms; each RNG has
     *   different properties, so some applications might have reasons to
     *   prefer a particular algorithm.  For general purposes, any of them
     *   should produce good results.
     *   
     *   The interpreter automatically makes a call to randomize() (the
     *   no-arguments version) when it starts up, unless the user specifies
     *   the "-norand" option when launching the interpreter.  For most
     *   programs, this means that you'll never have to make your own call to
     *   randomize() - you can just call rand() when you need random numbers.
     *   
     *   This function performs several tasks, depending on how you invoke
     *   it:
     *   
     *.    randomize() - selects the default RNG algorithm (ISAAC), and seeds
     *.          the RNG with truly random data from the operating system.
     *   
     *.    randomize(nil) - retrieves the current state of the RNG.  Returns
     *.          a list: [id, state], where 'id' is the ID of the currently
     *.          selected RNG algorithm, and 'state' is a value containing
     *.          its internal state, which can be used in a later call to
     *.          randomize([id, state]) to restore the RNG state.  The 'state'
     *.          value is opaque, meaning that it's not meant to be used
     *.          directly; the only thing you should do with it it save it
     *.          for later if you should want to restore this state later.
     *   
     *.    randomize([id, state]) - if you pass in a list that obtained from
     *.          an earlier call to randomize(nil), the RNG will be returned
     *.          to its state at the time of the randomize(nil) call.  This
     *.          selects the same RNG algorithm that was in effect then and
     *.          restores the internal state of the generator.  After calling
     *.          this, a series of calls to rand() will return the same
     *.          sequence of numbers that were returned after the call to
     *.          randomize(nil).
     *   
     *.    randomize(id) - selects the RNG algorithm identified by 'id'
     *.          (an RNG_Xxx value).  This doesn't change the state of
     *.          the generator; it simply selects the algorithm.
     *   
     *.    randomize(id, nil) - selects the RNG algorithm identified by
     *.          'id' (an RNG_Xxx value), and seeds the generator with
     *.          truly random data obtained from the operating system.
     *   
     *.    randomize(id, val) - selects the RNG algorithm identified by
     *.          'id' (an RNG_Xxx value), and seeds the generator with
     *.          the initial value 'val'.  This can be either an integer
     *.          value or a string; the preferred format varies by
     *.          algorithm, but they'll all accept either format.  After
     *.          you set a given seed value, rand() will return a sequence
     *.          of numbers that's repeatable every time you set the same
     *.          seed value.  This can be useful for things like testing,
     *.          where you want a sequence of numbers that's statistically
     *.          random, but which can be reproduced on demand.
     *   
     *   Most programs that require random numbers want a truly unpredictable
     *   series of numbers - that is, numbers that have a statistically
     *   random distribution, with no discernible patterns, and which will be
     *   different every time the program is run.  There are two separate
     *   parts to this proposition, and they're handled by separate
     *   functions.  rand() fulfills the first part: it uses a mathematical
     *   formula to generate a series of numbers that are statistically
     *   distributed in a random fashion (for example, so that "1" occurs as
     *   often as "10" or any other number, any given sequence of 2, 3, or
     *   more numbers is equally likely, and so on: mathematicians have many
     *   formal tests that RNGs must satisfy to be considered random.)
     *   randomize() fulfills the second part, which is making sure that the
     *   sequence of numbers is different every time you run the program.
     *   The reason this second part is important is that rand() is by its
     *   nature deterministic: it's defined entirely in terms of a formula,
     *   so given the same initial conditions, it'll always crank out the
     *   same sequence of numbers.  The trick is to randomize its initial
     *   conditions - and what makes it tricky is that we can't just turn to
     *   rand(), since it's the thing we're trying to randomize!
     *   
     *   This is where the "seed" values come in.  randomize() and
     *   randomize(id, nil) ask the operating system for truly random data to
     *   use for the initial conditions.  The degree of entropy in this OS
     *   seed data varies by system; some systems have better entropy sources
     *   than others.  But whatever the source, the seed data should be
     *   different each time you run the program.  randomize() feeds this
     *   seed data into the RNG to set its initial conditions, so each time
     *   you run, rand() will be starting from a different initial state.
     *   This makes for a different series of numbers from rand() on each
     *   run.
     *   
     *   Note that it's not necessary (or desirable) to call randomize()
     *   every time you want a random number.  Once you seed the RNG, rand()
     *   is all you need to call.  It can be slow to gather the true random
     *   data that randomize() uses, since this sometimes requires
     *   interacting with hardware devices or scanning large amounts of
     *   system data.  rand() is quite fast, since it just calculates a
     *   number using a formula.  Depending on the system, rand() might also
     *   be more reliable at producing high volumes of statistically random
     *   data than the OS sources.  The operating system sources of true
     *   entropy don't always change quickly, so it's better to use them
     *   infrequently, such as just once at the start of program execution,
     *   than to use them as a routine source of random numbers.
     *   
     *   The fixed seed values, with randomize(id, val), do something a
     *   little different.  Rather than making the RNG produce different
     *   sequences on each run, a fixed seed makes rand() generate the same
     *   series of numbers every time.  The numbers will still be
     *   statistically random, but each time you run the program, you'll get
     *   the same seaquence.  (The sequence is a function of the seed value.
     *   You'll get a different sequence for each different seed value.)
     *   
     *   Why would you want a fixed series of rand() results?  One big reason
     *   is testing.  One popular way to test software is regression testing,
     *   where you run the program and compare its output to a reference
     *   version that you know is correct.  If there are no differences, you
     *   know that changes you've made to the program since haven't broken
     *   anything in the test script.  Randomization interferes with this
     *   kind of testing, because it makes the output different on each run -
     *   it's useless to do a simple mechanical comparison of the new and old
     *   output because they'll always differ.  Fixed seeds to the rescue.
     *   Using a fixed seed, you can still exercise the program's random
     *   behavior, but the sequence of random behavior will repeat on every
     *   run, so you run those regression comparisons after all.  The really
     *   great thing is that you don't have to make big changes to the
     *   program if you want to switch between test mode and real randomness
     *   - all you have to do is change the one call to randomize().
     */
    randomize(...);

    /*
     *   Select a random number or a random value.  This uses the current
     *   random number algorithm as selected via randomize().
     *   
     *   If no arguments are supplied, the result is a random integer
     *   distributed evenly over the full range of the 32-bit integer type.
     *   The result can be positive or negative.
     *   
     *   If exactly one argument is supplied, the result depends on the type
     *   of the argument:
     *   
     *   - Integer: the function returns an integer from 0 to one less than
     *   the argument value.  For example, rand(10) returns a number from 0
     *   to 9 inclusive.
     *   
     *   - List: the function randomly selects one of the values from the
     *   list and returns it.
     *   
     *   - String: the function generates a random string by replacing each
     *   character of the argument string with a randomly chosen character,
     *   selected from a specific range specified by the argument character.
     *   For example, each 'a' in the input string is replaced by a random
     *   lower-case letter from a to z, each 'A' is replaced by a capital
     *   letter, and each 'd' is replaced by a random digit 0 to 9.  See the
     *   System Manual for the full list of the character codes.
     *   
     *   If more than one argument is supplied, the function randomly selects
     *   one of the arguments and returns it.  Note that this is an ordinary
     *   function call, so all of the arguments are evaluated, triggering any
     *   side effects of those evaluations.
     *   
     *   In all cases, the random numbers are uniformly distributed, meaning
     *   that each possible return value has equal probability.  
     */
    rand(...);

    /*
     *   Convert the given value to a string representation.  'val' can be an
     *   integer, in which case it's converted to a string representation in
     *   the numeric base given by 'radix' (which can be any value from 2 to
     *   36), or base 10 (decimal) if 'radix' is omitted; nil or true, in
     *   which case the string 'nil' or 'true' is returned; a string, which
     *   is returned unchanged; or a BigNumber, in which case the number is
     *   converted to a string representation in the given radix; a list or
     *   vector, in which case the individual elements are converted
     *   recursively, then the results concatenated together into a string
     *   with commas separating elements; or any of the built-in object types
     *   with default string conversions (ByteArray, StringBuffer, FileName,
     *   Date, TimeZone, FileName, etc).
     *   
     *   Note that when working with BigNumber values, you might prefer to
     *   use BigNumber.formatString(), as that gives you more control over
     *   the formatting style.
     *   
     *   'radix' is only meaningful with numeric values (integers and
     *   BigNumbers).  For BigNumbers, only whole integer values can be
     *   displayed in a non-decimal radix; if the number has a fractional
     *   part, the radix will be ignored and the number will be shown in
     *   decimal.
     *   
     *   'isSigned' indicates whether or not the value should be treated as
     *   "signed", meaning that negative values are represented with a "-"
     *   sign followed by the absolute value.  If 'isSigned' is nil, a
     *   negative value won't be converted to its absolute value before being
     *   displayed, but will instead be re-interpreted within its type system
     *   as an unsigned value.  For regular integers, this means that the
     *   result depends on the native hardware storage format for negative
     *   integers.  Most modern hardware uses two's complement notation,
     *   which represents -1 as 0xFFFFFFFF, -2 as 0xFFFFFFFE, etc.  Most
     *   types other than integer don't have distinct signed and unsigned
     *   interpretations, so 'isSigned' isn't meaningful with most other
     *   types.  With BigNumber in particular, the only effect is to omit the
     *   "-" sign for negative values.  
     */
    toString(val, radix?, isSigned?);

    /*
     *   Convert the given value to an integer.
     *   
     *   If 'val' is a string, the function parses the string's contents as
     *   an integer in the numeric base given by 'radix, which can be any
     *   integer from 2 to 36.  If 'radix' is omitted or nil, the default is
     *   base 10 (decimal).  The value is returned as an integer.  If the
     *   number represented by the string is too large for a 32-bit integer,
     *   a numeric overflow error occurs.
     *   
     *   If 'val' is true, or the string 'true', the return value is 1.  If
     *   'val' is nil, or the string 'nil', the return value is 0.  Leading
     *   and trailing spaces are ignored for these strings.
     *   
     *   If 'val' is a BigNumber value, the value is rounded to the whole
     *   number, and returned as an integer value.  A numeric overflow error
     *   occurs if the number is out of range for a 32-bit integer.  (If you
     *   want to round a BigNumber to the nearest integer and get the result
     *   as another BigNumber value, use the getWhole() method of the
     *   BigNumber.)
     *   
     *   See also toNumber(), which can also parse floating point values and
     *   whole numbers too large for the ordinary integer type.  
     */
    toInteger(val, radix?);

    /* 
     *   Get the current local time.
     *   
     *   If timeType is GetTimeDateAndTime (or is omitted), this returns the
     *   calendar date and wall-clock time, as a list: [year, month,
     *   dayOfMonth, dayOfWeek, dayOfYear, hour, minute, second, timer].
     *   Year is the year AD (for example, 2006); month is the current month,
     *   from 1 (January) to 12 (December); dayOfMonth is the calendar day of
     *   the month, from 1 to 31; dayOfWeek is the day of the week, from 1
     *   (Sunday) to 7 (Saturday); dayOfYear is the current day of the year,
     *   from 1 (January 1) to 366 (December 31 in a leap year); hour is the
     *   hour on a 24-hour clock, ranging from 0 (midnight) to 23 (11pm);
     *   minute is the minute of the hour, from 0 to 59; second is the second
     *   of the minute, from 0 to 59; and timer is the number of seconds
     *   elapsed since the "Epoch," defined as midnight, January 1, 1970,
     *   midnight UTC.  (This is the Epoch that Unix-like systems use, so it
     *   appears frequently in computer timekeeping systems.)  See the Date
     *   class for more comprehensive date/time handling.
     *   
     *   If timeType is GetTimeTicks, this return the number of milliseconds
     *   since an arbitrary starting time.  The first call to get this
     *   information sets the starting time, so it will return zero;
     *   subsequent calls will return the amount of time elapsed from that
     *   starting time.  Note that because a signed 32-bit integer can only
     *   hold values up to about 2 billion, the maximum elapsed time that
     *   this value can represent is about 24.8 days; so, if your program
     *   runs continuously for more than this, the timer value will roll
     *   around to zero at each 24.8 day multiple.  So, it's possible for
     *   this function to return a smaller value than on a previous
     *   invocation, if the two invocations straddle a 24.8-day boundary.  
     */
    getTime(timeType?);

    /*
     *   Match a string to a regular expression pattern.  'pat' can be either
     *   a string giving the regular expression, or can be a RexPattern
     *   object.  'str' is the string to match, and 'index' is the starting
     *   character index (the first character is at index 1) at which to
     *   start matching.  Returns the length in characters of the match, or
     *   nil if the string doesn't match the pattern.  (Note that a return
     *   value of zero doesn't indicate failure - rather, it indicates a
     *   successful match of the pattern to zero characters.  This is
     *   possible for a pattern with a zero-or-more closure, such as 'x*' or
     *   'x?'.)  
     */
    rexMatch(pat, str, index?);

    /*
     *   Search the given string for the given regular expression pattern.
     *   'pat' is a string giving the regular expression to find, or a
     *   RexPattern object.  'str' is the string to search, and 'index' is
     *   the optional starting index (the first character is at index 1;
     *   negative indices are from the end of the string, so -1 is the last
     *   character, -2 is the second to last, and so on).  If a match to the
     *   pattern isn't found, returns nil.  If a match is found, the return
     *   value is a list: [index, length, string], where index is the
     *   starting character index of the match, length is the length in
     *   characters of the match, and string is the text of the match.  
     */
    rexSearch(pat, str, index?);

    /*
     *   Get the given regular expression group.  This can be called after a
     *   successful rexMatch() or rexSearch() call to retrieve information on
     *   the substring that matched the given "group" within the regular
     *   expression.  A group is a parenthesized sub-pattern within the
     *   regular expression; groups are numbered left to right by the open
     *   parenthesis, starting at group 1.  If there is no such group in the
     *   last regular expression searched or matched, or the group wasn't
     *   part of the match (for example, because it was part of an
     *   alternation that wasn't matched), the return value is nil.  If the
     *   group is valid and was part of the match, the return value is a
     *   list: [index, length, string], where index is the character index
     *   within the matched or searched string of the start of the group
     *   match, length is the character length of the group match, and string
     *   is the text of the group match.  
     */
    rexGroup(groupNum);

    /*
     *   Search for the given regular expression pattern (which can be given
     *   as a regular expression string or as a RexPattern object) within the
     *   given string, and replace one or more occurrences of the pattern
     *   with the given replacement text.
     *   
     *   The search pattern can also be given as a *list* of search patterns.
     *   In this case, we'll search for each of the patterns and replace each
     *   one with the corresponding replacement text.  If the replacement is
     *   itself given a list in this case, each element of the pattern list
     *   is replaced by the corresponding element of the replacement list.
     *   If there are more patterns than replacements, the extra patterns are
     *   replaced by empty strings; any extra replacements are simply
     *   ignored.  If the replacement is a single value rather than a list,
     *   each pattern is replaced by that single replacement value.
     *   
     *   'flags' is a combination of the ReplaceXxx bit flags, using '|'.  If
     *   the flags include ReplaceAll, all occurrences of the pattern are
     *   replaced; otherwise only the first occurrence is replaced.
     *   
     *   ReplaceOnce and ReplaceAll are mutually exclusive; they mean,
     *   respectively, that only the first occurrence of the match should be
     *   replaced, or that every occurrence should be replaced.  ReplaceOnce
     *   and ReplaceAll are ignored if a 'limit' value is specified (this is
     *   true even if 'limit' is nil, which means that all occurrences are
     *   replaced).
     *   
     *   If ReplaceIgnoreCase is included, the capitalization of the match
     *   pattern is ignored, so letters in the pattern match both their
     *   upper- and lower-case equivalents.  Otherwise the case will be
     *   matched exactly. If ReplaceFollowCase AND ReplaceIgnoreCase are
     *   included, lower-case letters in the replacement text are capitalized
     *   as needed to follow the capitalization pattern of the actual text
     *   matched: if all the letters in the match are lower-case, the
     *   replacement is lower case; if all are upper-case, the replacement is
     *   changed to all upper-case; if there's a mix of cases in the match,
     *   the first letter of the replacement is capitalized and the rest are
     *   left in lower-case.
     *   
     *   The ReplaceSerial flag controls how the search proceeds when
     *   multiple patterns are specified.  By default, we search for each one
     *   of the patterns, and replace the leftmost match.  If ReplaceOnce is
     *   specified, we're done; otherwise we continue by searching again for
     *   all of the patterns, this time in the remainder of the string (after
     *   that first replacement), and again we replace the leftmost match.
     *   This proceeds until we can't find any more matches for any of the
     *   patterns.  If ReplaceSerial is included in the flags, we start by
     *   searching only for the first pattern, replacing one or all
     *   occurrences depending on the ReplaceOnce or ReplaceAll flag.  Next,
     *   if ReplaceAll is specified OR we didn't find any matches for the
     *   first pattern, we start over with the result and search for the
     *   second pattern, replacing one or all occurrences of it.  We repeat
     *   this for each pattern.
     *   
     *   If the flags are omitted entirely, the default is ReplaceAll (which
     *   means replace all occurrences, exact case matches only, parallel
     *   searching).
     *   
     *   'index', if provided, is the starting character index of the search;
     *   instances of the pattern before this index will be ignored.  Returns
     *   the result string with all of the desired replacements.  When an
     *   instance of the pattern is found and then replaced, the replacement
     *   string is not rescanned for further occurrences of the text, so
     *   there's no danger of infinite recursion; instead, scanning proceeds
     *   from the next character after the replacement text.
     *   
     *   'limit', if specified, is an integer indicating the maximum number
     *   of matches to replace, or nil to replace all matches.  If the limit
     *   is reached before all matches have been replaced, no further
     *   replacements are performed.  If this parameter is specified, it
     *   overrides any ReplaceOnce or ReplaceAll flag.
     *   
     *   The replacement text can use "%n" sequences to substitute group
     *   matches from the input into the output.  %1 is replaced by the match
     *   to the first group, %2 the second, and so on.  %* is replaced by the
     *   entire matched input.  (Because of the special meaning of "%", you
     *   must use "%%" to include a percent sign in the replacement text.)  
     */
    rexReplace(pat, str, replacement, flags?, index?, limit?);

    /*
     *   Create an UNDO savepoint.  This adds a marker to the VM's internal
     *   UNDO log, establishing a point in time for a future UNDO operation.
     */
    savepoint();

    /*
     *   UNDO to the most recent savepoint.  This uses the VM's internal UNDO
     *   log to undo all changes to persistent objects, up to the most recent
     *   savepoint.  Returns true if the operation succeeded, nil if not.  A
     *   nil return means that there's no further UNDO information recorded,
     *   which could be because the program has already undone everything
     *   back to the start of the session, or because the UNDO log was
     *   truncated due to memory size such that no savepoints are recorded.
     *   (The system automatically limits the UNDO log's total memory
     *   consumption, according to local system parameters.  This function
     *   requires at least one savepoint to be present, because otherwise it
     *   could create an inconsistent state.)  
     */
    undo();

    /*
     *   Save the current system state into the given file.  This uses the
     *   VM's internal state-save mechanism to store the current state of all
     *   persistent objects in the given file.  Any existing file is
     *   overwritten.
     *   
     *   'metatab' is an optional LookupTable containing string key/value
     *   pairs to be saved with the file as descriptive metadata.  The
     *   interpreter and other tools can display this information to the user
     *   when browsing a collection of saved game files, to help the user
     *   remember the details of each saved position.  It's up to the game to
     *   determine what to include; the list can include any information
     *   relevant to the game that would be helpful when reviewing saved
     *   position files, such as the room name, score, turn count, chapter
     *   name, etc.  
     */
    saveGame(filename, metatab?);

    /*
     *   Restore a previously saved state file.  This loads the states of all
     *   persistent objects stored in the given file.  The file must have
     *   been saved by the current version of the current running program; if
     *   not, an exception is thrown.  
     */
    restoreGame(filename);

    /*
     *   Restart the program from the beginning.  This resets all persistent
     *   objects to their initial state, as they were when the program was
     *   first started.  
     */
    restartGame();

    /*
     *   Get the maximum of the given arguments.  The values must be
     *   comparable with the ordinary "<" and ">" operators.  Note that
     *   because this is an ordinary function call, all of the arguments are
     *   evaluated (which means any side effects of these evaluations will be
     *   triggered).  
     */
    max(val1, ...);

    /*
     *   Get the minimum of the given arguments.  The values must be
     *   comparable with the ordinary "<" and ">" operators.  Note that
     *   because this is an ordinary function call, all of the arguments are
     *   evaluated (which means any side effects of these evaluations will be
     *   triggered).  
     */
    min(val1, ...);

    /*
     *   Create a string by repeating the given value the given number of
     *   times.  If the repeat count isn't specified, the default is 1; a
     *   repeat count less than zero throws an error.  'val' can be a string,
     *   in which case the string is simply repeated the given number of
     *   times; an integer, in which case the given Unicode character is
     *   repeated; or a list of integers, in which case the given Unicode
     *   characters are repeated, in the order of the list.  The list format
     *   can be used to create a string from a list of Unicode characters
     *   that you've been manipulating as a character array, which is
     *   sometimes a more convenient or efficient way to do certain types of
     *   string handling than using the actual string type.  
     */
    makeString(val, repeatCount?);

    /*
     *   Get a description of the parameters to the given function.  'func'
     *   is a function pointer.  This function returns a list: [minArgs,
     *   optionalArgs, isVarargs], where minArgs is the minimum number of
     *   arguments required by the function, optionalArgs is the additional
     *   number of arguments that can be optionally provided to the function,
     *   and isVarargs is true if the function takes any number of additional
     *   ("varying") arguments, nil if not.  
     */
    getFuncParams(func);

    /*
     *   Convert the given value to a number.  This is similar to
     *   toInteger(), but can parse strings containing floating point numbers
     *   and whole numbers too large for ordinary integers.
     *   
     *   If 'val' is an integer or BigNumber value, the return value is
     *   simply 'val'.
     *   
     *   If 'val' is a string, the function parses the string's contents as a
     *   number in the given 'radix', which can be any integer from 2 to 36.
     *   If 'radix' is omitted, the default is 10 for decimal.  If the radix
     *   is decimal, and the number contains a decimal point (a period, '.')
     *   or an exponent (which consists of the letter 'e' or 'E', an optional
     *   '+' or '-' sign, and one or more digits), the value is parsed as a
     *   floating point number, and a BigNumber value is returned.  For any
     *   other radix, decimal points and exponents are considered non-number
     *   characters.  For an integral value, the result will be an integer if
     *   the number is within the range that fits in an integer, otherwise
     *   the result is a BigNumber.  The routine will simply stop parsing at
     *   the first non-number character it encounters, so no error will occur
     *   if the string contains text following the number.  If the text
     *   doesn't contain any number characters at all, the result is zero.
     *   
     *   If val is true or the string 'true', return 1; if nil or the string
     *   'nil', returns 0.  Leading and trailing spaces are ignored in the
     *   string versions of these values.  
     */
    toNumber(val, radix?);

    /*
     *   Format values into a string.  This is similar to the traditional C
     *   language "printf" family of functions: it takes a "format string"
     *   containing a mix of plain text and substitution parameters, and a
     *   set of values to plug in to the substitution parameters, and returns
     *   a new string containing the formatted result.
     *   
     *   'format' is the format string.  Most characters of the format string
     *   are simply copied verbatim to the result.  However, each '%' in the
     *   format string begins a substitution parameter; the '%' is followed
     *   by one or more optional qualifiers, then by a type code letter.  The
     *   corresponding value from the argument list is formatted into a
     *   string according to the type code, and then replaces the entire '%'
     *   sequence in the result string.  By default, the first '%' parameter
     *   corresponds to the first additional argument after 'format', the
     *   second '%' corresponds to the second additional argument, and so on.
     *   You can override the default argument position of a '%' using the
     *   '$' qualifier - see below.
     *   
     *   The arguments following 'format' are the values to be substituted
     *   for the '%' parameters in the format string.
     *   
     *   The return value is a string containing the formatted result.
     *   
     *   See the System Manual for the list of '%' codes.  
     */
    sprintf(format, ...);

    /*
     *   Create a list by repeating the given value the given number of
     *   times.  If the repeat count isn't specified, the default is 1; a
     *   repeat count less than zero throws an error.  'val' can be any
     *   value; it's simply repeated in each element of the list.
     */
    makeList(val, repeatCount?);

    /*
     *   Get the absolute value of a number.  The argument can be an integer
     *   or a BigNumber; the return value is the absolute value of the
     *   argument, and has the same type as the argument.  (The absolute
     *   value of a positive number X (or zero) is X; the absolute value of a
     *   negative number X is -X.) 
     */
    abs(num);

    /*
     *   Get the sign of a number.  The argument can be an integer or a
     *   BigNumber.  The return value is an integer: 1 if the argument is
     *   positive, 0 if the argument is zero, -1 if the argument is negative.
     */
    sgn(num);

    /*
     *   Concatenate the arguments together into a single string.  The
     *   arguments can be strings or any types that can be automatically
     *   converted to string for the regular "+" operator; non-strings are
     *   first converted to strings using the same rules that "+" uses when
     *   combining a string with a non-string.  If there are no arguments,
     *   the result is an empty string.
     *   
     *   This function is essentially the same as concatenating a series of
     *   values with the "+" operator, but it's more efficient with three or
     *   more values, since the "+" operator has to be applied successively
     *   in pairs; that creates and copies an extra intermediate result
     *   string at each step.  This function only creates one result string
     *   and only has to copy each input string once.
     */
    concat(...);

    /*
     *   Search backwards in the given string for the given regular
     *   expression pattern.  'pat' is a string giving the regular expression
     *   to find, or a RexPattern object.  'str' is the string to search, and
     *   'index' is the optional starting index (the first character is at
     *   index 1; negative indices are from the end of the string, so -1 is
     *   the last character, -2 is the second to last, and so on; 0 means the
     *   position just after the last character of the string).  If 'index'
     *   is omitted, the default is to search the entire string from the end,
     *   which is equivalent to passing 0 or str.length()+1 for 'index'.
     *   
     *   If a match is found, the return value is a list: [index, length,
     *   string], where index is the starting character index of the match,
     *   length is the length in characters of the match, and string is the
     *   text of the match.
     *   
     *   This does the same work as rexSearch(), but searches the string
     *   backwards, from the end to the start.  The match must end before the
     *   starting index, which allows for repeated searches: simply pass the
     *   match index from the previous search as the 'index' value for the
     *   next search to find the next earlier match that doesn't overlap the
     *   previous match.
     *   
     *   The meanings of the <FirstBegin> and <FirstEnd> flags for a reverse
     *   search are essentially the mirror image of their meanings in a
     *   regular forward search.  This is easiest to understand by thinking
     *   about the flags in the abstract.  <FirstBegin> means that the
     *   winning match is the one whose "near" endpoint is closest to the
     *   starting index; <FirstEnd> means that the winner is the match whose
     *   "far" endpoint is closest to the starting index.  The near endpoint
     *   in a forward search is the start of the match, whereas it's the end
     *   of the match in a reverse search.  Similarly, the far endpoint is
     *   the end of the match in a forward search and the start of the match
     *   in a reverse search.  So in a reverse search, <FirstBegin> means
     *   that the winner is the match whose ending index is highest, and
     *   <FirstEnd> means that the winner is the one whose starting index is
     *   highest.
     */
    rexSearchLast(pat, str, index?);
}

/*
 *   flags for firstObj() and nextObj()
 */
#define ObjInstances  0x0001
#define ObjClasses    0x0002
#define ObjAll        (ObjInstances | ObjClasses)

/*
 *   rexReplace() flags 
 */
#define ReplaceAll         0x0001
#define ReplaceIgnoreCase  0x0002
#define ReplaceFollowCase  0x0004
#define ReplaceSerial      0x0008
#define ReplaceOnce        0x0010

/*
 *   getTime() flags 
 */
#define GetTimeDateAndTime  1
#define GetTimeTicks        2

/*
 *   Random number generator IDs for randomize().
 */

/*
 *   
 *   RNG_ISAAC - ISAAC is the default generator.  It's designed for both
 *   cryptographic application and more mainstream uses.  Crypto applications
 *   generally have more stringent requirements for an RNG than ordinary
 *   applications.  ISAAC does well with the usual statistical tests for RNGs
 *   and is reasonably fast.
 *   
 *   ISAAC's preferred format for a fixed seed is a string value.
 */
#define RNG_ISAAC    1


/*   
 *   RNG_LCG - LCG stands for Linear Congruential Generator, which generates
 *   numbers using a simple linear formula.  This is the old standby of
 *   computer RNGs; it's the sort that comes standard with C compilers.
 *   These generators have been extensively studied and have reasonably good
 *   statistical properties and a number of known weaknesses.  The TADS LCG
 *   uses the method described in Knuth, The Art of Computer Programming,
 *   volume 2, p170.
 *   
 *   The LCG's preferred format for a fixed seed is an integer value.
 */
#define RNG_LCG      2

/*
 *   RNG_MT19937 - Mersenne Twister MT19937 algorithm.  This is a widely used
 *   RNG algorithm that's become especially popular for scientific
 *   simulations.  It's relatively new, dating from 1997, and was designed to
 *   remedy most of the known shortcomings of linear congruential generators
 *   and other earlier RNGs.  It's fast and does well on standard statistical
 *   tests.
 *   
 *   MT's preferred format for a fixed seed is a string value.
 */
#define RNG_MT19937  3

