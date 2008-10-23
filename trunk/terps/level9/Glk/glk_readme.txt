Level 9 Interpreter, Glk Notes
------------------------------


Introduction
------------

This is a port of the Level 9 interpreter to Glk.  The complete interface
lives in the single file

	glk.c

The main test and development system for the port is Linux, with Xglk as the
Glk library.


Acknowledgements
----------------

Thanks to Alan Staniforth for considerable help with getting the some of the
source for the base interpreter together, and for his hints on how to cover
the porting requirements, and to David Kinder for a complete reality check
on the port progress, and also for the v3.0 Level9 interpreter.

Thanks also to Ben Hines <bhines@alumni.ucsd.edu> for the Mac code, which,
if I have transcribed it correctly from earlier work with AGiliTy, should
permit this port to run on Mac platforms with Glk.


Running Games
-------------

The interpreter understands game files that use the "Spectrum snapshot"
format.  A file in this format usually has the extension ".sna" or ".SNA".

Give the name of the game file to run at the system prompt.  For example

	glklevel9 colossal.sna

There are a few command line options that you can also add, to vary the way
that the game looks:

	-ni	Don't intercept 'quit', 'restart', 'save', and 'restore'
	-np	Don't add an extra prompt on line input
	-nl	Don't attempt to detect infinite loops in games
	-na	Don't expand single-letter abbreviations
	-nc	Don't attempt to interpret selected commands locally

See below for further information about what these options mean.

Glk Level9 does not attempt to display any game graphics.


Compiling
---------

To compile Glk Level 9 for Linux, first unpack the source files.  You might
need to use the -a argument to unzip in order to convert text files for your
system.

Edit Makefile.glk so that it has the right path to the Glk library you wish
to build with.  If you want to build the IFP plugin, also edit the parts of
Makefile.glk that have paths to IFP components.

To build standalone binary version of Glk Level 9, use

	make -f Makefile.glk glklevel9

To build the IFP plugin, use

	make -f Makefile.glk level9-3.0.so

To clean up and delete everything in the case of a build error, use

	make -f Makefile.glk clean


Intercepting Game Commands
--------------------------

Some Level 9 games are written for cassette tape based microprocessor
systems, and the way in which they save, restore, and restart games generally
reflects this.  Additionally, there's often no straightforward way to quit
from a game.

To try to make things a bit more convenient for a player, Glk Level9 traps
the following command words in its interface:

	quit, restart, save, restore, and load (synonym for restore)

When it sees one of these entered on an input line, it tries to handle the
command itself.  In the case of "quit" and "restart", it will confirm that
you want to end the current game.

In the case of "save" and "restore", Glk Level9 will try to handle saving or
loading game state to or from system files using the Level 9 interpreter
internal routines, rather than the ones built into the game itself.  This
bypasses the inconvenient "Lenslok" check built into some games, and also
works round a couple of possible Level 9 interpreter bugs.

If you prefix a command with a single quote, Glk Level9 will not try to
handle the command itself, but will remove the quote, and pass the remaining
string to the game without change.

You can turn off command interception with the command line option '-ni'.


Extra Prompting
---------------

Early Level 9 games generally output a prompt something like

	What now?

However, it's not always printed when the game is expecting line input.
Simply leaning on the Return key will scroll the game with no prompting, and
the Adrian Mole games offer no prompt at all when not in menu modes.

Some later Level 9 games begin with the "What now?" (or "What gnow?"), then
switch to issuing a "> " prompt later on.

To try to make it a little clearer to see what's going on, the Glk Level9
will add it's own "> " prompt when the game is expecting line input, but
only when it determines that the game has not already issued its own prompt.

You can turn this off with the '-np' option.


Game Infinite Loop Checking
---------------------------

Some Level9 games can enter an infinite loop if they have nothing better to
do.  For example, at then end of Adrian Mole, you have the option to play
the last part again.  If you decline, the game will spin in a tight loop
which does nothing.

Because it's hard to break into this loop without killing the interpreter
completely, Glk Level9 notes how much work the interpreter is doing over
time.  If it notices that the interpreter has neither printed anything nor
asked for input for five seconds or so, it assumes that the game is in an
infinite loop, and offers you the chance to stop the game.

In the unlikely event that this check triggers with a game that is not in an
infinite loop, you can turn off the option with "-nl".


Expanding Abbreviations
-----------------------

Many IF games systems allow a player to use single character abbreviations
for selected common commands, for example, 'x' for 'examine', 'l' for look,
and so on.  Not all Level 9 games offer this feature, however.

To try to help the player, Glk Level 9 will automatically expand a selection
of single character commands, before passing the expanded string to the game
as input.  It expands a command only if the first word of the command is a
single letter, and one of the following:

	'c' -> "close"		'g' -> "again"		'i' -> "inventory"
	'k' -> "attack"		'l' -> "look"		'p' -> "open"
	'q' -> "quit"		'r' -> "drop"		't' -> "take"
	'x' -> "examine" 	'y' -> "yes"		'z' -> "wait"

If you want to suppress abbreviation expansion, you can prefix your input
with a single quote character (like putting literal strings into a spread-
sheet).  If you do this, Glk Level9 will strip the quote, then pass the rest
of the string to the main interpreter without any more changes.  So for
example,

	'x something

will pass the string "x something" back to the game, whereas

	x something

will pass "examine something" back to the game.

You can turn off abbreviation expansions with the command line option '-na'.


Interpreting Commands Locally
-----------------------------

Glk Level9 will handle special commands if they are prefixed with the string
'glk'.  It understands the following special commands:

	script on	  Starts recording the game text output sent to the
			  main game window
	script off	  Turns off game text recording
	inputlog on	  Starts recording input lines typed by the player
	inputlog off	  Stops recording input lines
	readlog on	  Reads an input log file as if it had been typed by
			  a player; reading stops automatically at the end of
			  the file
	abbreviations on  Turn abbreviation expansion on
	abbreviations off Turn abbreviation expansion off
	loopchecks on	  Turn game infinite loop checking on
	loopchecks off	  Turn game infinite loop checking off
	locals on	  Turn local interception of 'quit', 'restart',
			  'save' and 'restore' on
	locals off	  Turn local interception of 'quit', etc. off
	prompts on	  Turn extra "> " prompting on
	prompts off	  Turn extra "> " prompting off
	version		  Prints the Glk library and Glk port version numbers
	commands off	  Turn of Glk special commands; once off, there is no
			  way to turn them back on

You can abbreviate these commands, as long as the abbreviation you use is
unambiguous.

If for some reason you need to pass the string "glk" to the interpreter, you
can, as with abbreviations above, prefix it with a single quote character.

You can turn off local command handling with the command line option '-nc'.

If all of abbreviation expansion, local command handling, and game command
interceptions are turned off, there is no need to use single quotes to
suppress special interpreter features.


Multi-file Games
----------------

Some Level 9 games come in multiple files, for example

	gamedat1.sna
	gamedat2.sna
	...

and so on.  Once a game part is finished, the Level 9 interpreter queries
the Glk interface for the next file.  When this happens, both for disk and
for tape based games, the interface will try to generate the appropriate
file name for the next game part, by incrementing the last digit found in
the file's name.  The interpreter will then look in the same directory for
the new file name.

If this method fails, then you will need to run the separate game parts
manually.  Alternatively, rename the files so that they match this pattern.


Keyboard Polling
----------------

Some Level 9 games use the standard home microprocessor system method of
waiting for a keypress, using a tight spin loop that samples the keyboard
until a key is detected.  The Adrian Mole games use this keyboard sampling
both for scroll pausing, and for their main menu selections, with something
like:

	for each line in the output paragraph
		print the line
		while (os_readchar() shows a key is pressed)
			;
		endwhile
	endfor
	selection = return value from os_readchar()
	while (selection shows no key is pressed)
		selection = return value from os_readchar()
	handle selection...

This means that os_readchar() _must_ be implemented according to the notes
in Level 9's porting.txt.  That is, it must poll the keyboard for key
presses, and return the key code if a key is pressed, or zero if no key is
currently pressed.

If os_readchar() waits for a key press on each call, Adrian Mole games will
wait for a key press after each output line, which is not what we want.  On
the other hand, if it doesn't wait for a key press at the menu selection
point, the interpreter will consume all the available system CPU time, which
is also very undesirable on a multi-tasking operating system.

To solve this, Glk Level9 uses two different techniques, and which it
chooses depends on whether the Glk library it is linked with has timers.

If the Glk library has timers, then on each call to os_readchar(), Glk
Level9 starts a timeout for a short period, then waits for either a key
press event, or a timeout.  If it receives a key press, it returns the code,
otherwise it returns zero.  This lets games run correctly.  Continually
cycling through the Glk timeout is fast enough to be acceptable to a player,
but the timeout provides good protection against spinning wildly in a tight
loop.

If the Glk library does not have timers, then the interface will return zero
for some large number of consecutive calls, then wait until a character is
entered.  Since the initial number is large, this allows the game to
progress, generally, to the point where it needs a key press to continue at
all.  The only thing that might show slightly odd behaviour is the DEMO mode
of Adrian Mole games.

Also, this type of character polling cannot be used with reading input logs.
So the Glk "readlog" command does not function with Adrian Mole games.

To pretend that no key was pressed, when the interface is in fact waiting
for a key press, you can type Control-U.  The Glk interface will return this
to the game as a zero.

Games that rarely, or never, use line input, but continually poll for input
using os_readchar(), are difficult to stop.  To help with this, Glk Level9
offers Control-C as as way to quit the game, with a confirmation as for
"quit" above.  Note that Control-C will only work when the game is using
os_readchar() (that is, most of the time for Adrian Mole games, and
occasionally, if ever, in other games).  For normal line input, use "quit"
as described above.

Control-C is turned off by the command line option "-nc".


--

Simon Baldwin, simon_baldwin@yahoo.com
