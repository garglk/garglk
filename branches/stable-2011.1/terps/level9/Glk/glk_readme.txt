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
the porting requirements, and to David Kinder for a complete reality check on
the port progress, and also for the Level 9 interpreter.

Thanks also to Ben Hines <bhines@alumni.ucsd.edu> for the Mac code, which, if
I have transcribed it correctly from earlier work with AGiliTy, should permit
this port to run on Mac platforms with Glk.


Running Games
-------------

The interpreter understands game files that use the "Spectrum snapshot" format.
A file in this format usually has the extension ".sna" or ".SNA".

Give the name of the game file to run at the system prompt.  For example

	glklevel9 colossal.sna

If the game has an external line drawing graphics file, make sure that the file
lives in the same directory as the game file, and has the same base name with
the extension ".pic", or is named "picture.dat".  The Glk interpreter will
find this file automatically, if it exists, and use it to create game pictures.
Line drawn graphics are not always held in an external graphics file; a game
may l hold its graphics internally, in which case no addtional files are
required.

If the game has external bitmap pictures, these will normally appear as
multiple files with ".pic", ".squ", ".picN", or ".title" extensions.  As with
line drawn graphics, ensure that these files are in the same directory as the
main game file, so that the Glk interpreter can find them.

If there are no graphics files, the interpreter will run in text only mode.  It
will also do this if it cannot open the graphics files, or if the files do not
seem to contain usable game graphics.

There are a few command line options that you can also add, to vary the way
that the game looks:

	-np     Don't display game pictures initially
	-ni	Don't intercept 'quit', 'restart', 'save', and 'restore'
	-ne	Don't add an extra prompt on line input
	-nl	Don't attempt to detect infinite loops in games
	-na	Don't expand single-letter abbreviations
	-nc	Don't attempt to interpret selected commands locally

See below for further information about what these options mean.


Compiling
---------

To compile Glk Level 9 for Linux, first unpack the source files.  You might
need to use the -a argument to unzip in order to convert text files for your
system.

Edit Makefile.glk so that it has the right path to the Glk library you wish to
build with.  If you want to build the IFP plugin, also edit the parts of
Makefile.glk that have paths to IFP components.

To build standalone binary version of Glk Level 9, use

	make -f Makefile.glk glklevel9

To build the IFP plugin, use

	make -f Makefile.glk level9-5.1.so

To clean up and delete everything in the case of a build error, use

	make -f Makefile.glk clean


Intercepting Game Commands
--------------------------

Some Level 9 games are written for cassette tape based microprocessor systems,
and the way in which they save, restore, and restart games generally reflects
this.  Additionally, there's often no straightforward way to quit from a game.

To try to make things a bit more convenient for a player, Glk Level 9 traps the
following command words in its interface:

	quit, restart, save, restore, and load (synonym for restore)

When it sees one of these entered on an input line, it tries to handle the
command itself.  In the case of "quit" and "restart", it will confirm that you
want to end the current game.

In the case of "save" and "restore", Glk Level 9 will try to handle saving or
loading game state to or from system files using the Level 9 interpreter
internal routines, rather than the ones built into the game itself.  This
bypasses the inconvenient "Lenslok" check built into some games, and also works
round a couple of possible Level 9 interpreter bugs.

If you prefix a command with a single quote, Glk Level 9 will not try to handle
the command itself, but will remove the quote, and pass the remaining string
to the game without change.

You can turn off command interception with the command line option '-ni'.


Extra Prompting
---------------

Early Level 9 games generally output a prompt something like

	What now?

However, it's not always printed when the game is expecting line input.  Simply
leaning on the Return key will scroll the game with no prompting, and the
Adrian Mole games offer no prompt at all when not in menu modes.

Some later Level 9 games begin with the "What now?" (or "What gnow?"), then
switch to issuing a "> " prompt later on.

To try to make it a little clearer to see what's going on, the Glk Level 9 will
add it's own "> " prompt when the game is expecting line input, but only when
it determines that the game has not already issued its own prompt.

You can turn this off with the '-ne' option.


Displaying Pictures
-------------------

Level 9 games may contain graphics, and while not essential to the game, they
can enhance the experience of playing.

Unfortunately, Glk does not provide a method to directly display an image
bitmap, so Glk Level 9 needs to adopt some tricks to get a picture to display.
The end result is that, on Linux Xglk at least, it can take several seconds to
render a complete picture.

To reduce the problems caused by a game pausing for several seconds for a
picture,  Glk Level 9 does its picture rendering using a background "thread".
This draws in the complete picture over a series of smaller image updates,
while at the same time still allowing game play.  To speed up picture
rendering, Glk Level 9 also goes to considerable effort to try to minimize the
amount of Glk graphics operations it uses.

If you move to a location with a new picture before the current one has been
fully rendered, Glk Level 9 will simply start working on the newer picture.

Most Level 9 games accept the "graphics" and "text" commands, which turn
pictures on and off respectively.  It is also possible to turn off pictures
directly in Glk, for games that don't support these two commands.

You can use the "-np" option to turn off pictures.  Glk Level 9 also will not
show pictures if the Glk library does not support both graphics and timers, if
there are no graphics files, built in graphics, or bitmaps, or if the
interpreter cannot open graphics files successfully.  See below for further
notes about displaying pictures.


Game Infinite Loop Checking
---------------------------

Some Level 9 games can enter an infinite loop if they have nothing better to
do.  For example, at then end of Adrian Mole, you have the option to play the
last part again.  If you decline, the game will spin in a tight loop which
does nothing.

Because it's hard to break into this loop without killing the interpreter
completely, Glk Level 9 notes how much work the interpreter is doing over time.
If it notices that the interpreter has neither printed anything nor asked for
input for five seconds or so, it assumes that the game is in an infinite loop,
and offers you the chance to stop the game.

In the unlikely event that this check triggers with a game that is not in an
infinite loop, you can turn off the option with "-nl".


Status Line
-----------

Level 9 games do not print a status line, so Glk Level 9 will use the status
window, a separate window above the main window created by Glk libraries that
support separate windows, to display the game's name and version details.

For other Glk libraries, Glk Level 9 will print the game's name in "[...]"
brackets above the standard game output following each turn, but only when the
game name has changed since the last turn (that is, in multi-file games).


Expanding Abbreviations
-----------------------

Many IF games systems allow a player to use single character abbreviations for
selected common commands, for example, 'x' for 'examine', 'l' for look, and so
on.  Not all Level 9 games offer this feature, however.

To try to help the player, Glk Level 9 will automatically expand a selection
of single character commands, before passing the expanded string to the game as
input.  It expands a command only if the first word of the command is a single
letter, and one of the following:

	'c' -> "close"		'g' -> "again"		'i' -> "inventory"
	'k' -> "attack"		'l' -> "look"		'p' -> "open"
	'q' -> "quit"		'r' -> "drop"		't' -> "take"
	'x' -> "examine" 	'y' -> "yes"		'z' -> "wait"

If you want to suppress abbreviation expansion, you can prefix your input with
a single quote character (like putting literal strings into a spreadsheet).
If you do this, Glk Level 9 will strip the quote, then pass the rest of the
string to the main interpreter without any more changes.  So for example,

	'x something

will pass the string "x something" back to the game, whereas

	x something

will pass "examine something" back to the game.

You can turn off abbreviation expansions with the command line option '-na'.


Interpreting Commands Locally
-----------------------------

Glk Level 9 will handle special commands if they are prefixed with the string
'glk'.  It understands the following special commands:

	help		  Prints help on Glk special commands
	summary		  Prints all current Glk settings
	version		  Prints the Glk library and Glk port version numbers
	license		  Prints the Glk port license

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
	graphics on       Turn on Glk graphics
	graphics off      Turn off Glk graphics (see below)
	loopchecks on	  Turn game infinite loop checking on
	loopchecks off	  Turn game infinite loop checking off
	locals on	  Turn local interception of 'quit', 'restart',
			  'save' and 'restore' on
	locals off	  Turn local interception of 'quit', etc. off
	prompts on	  Turn extra "> " prompting on
	prompts off	  Turn extra "> " prompting off
	commands off	  Turn of Glk special commands; once off, there is no
			  way to turn them back on

You can abbreviate these commands, as long as the abbreviation you use is
unambiguous.

If for some reason you need to pass the string "glk" to the interpreter, you
can, as with abbreviations above, prefix it with a single quote character.

You can turn off local command handling with the command line option '-nc'.

If all of abbreviation expansion, local command handling, and game command
interceptions are turned off, there is no need to use single quotes to suppress
special interpreter features.


Multi-file Games
----------------

Some Level 9 games come in multiple files, for example

	gamedat1.sna
	gamedat2.sna
	...

and so on.  Once a game part is finished, the Level 9 interpreter queries the
Glk interface for the next file.  When this happens, both for disk and for
tape based games, the interface will try to generate the appropriate file name
for the next game part, by incrementing the last digit found in the file's
name.  The interpreter will then look in the same directory for the new file
name.

If this method fails, then you will need to run the separate game parts
manually.  Alternatively, rename the files so that they match this pattern.


Keyboard Polling
----------------

Some Level 9 games use the standard home microprocessor system method of
waiting for a keypress, using a tight spin loop that samples the keyboard until
a key is detected.  The Adrian Mole games use this keyboard sampling both for
scroll pausing, and for their main menu selections, with something like:

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

This means that os_readchar() _must_ be implemented according to the notes in
Level 9's porting.txt.  That is, it must poll the keyboard for key presses,
and return the key code if a key is pressed, or zero if no key is currently
pressed.

If os_readchar() waits for a key press on each call, Adrian Mole games will
wait for a key press after each output line, which is not what we want.  On the
other hand, if it doesn't wait for a key press at the menu selection point, the
interpreter will consume all the available system CPU time, which is also very
undesirable on a multi-tasking operating system.

To solve this, Glk Level 9 uses two different techniques, and which it chooses
depends on whether the Glk library it is linked with has timers.

If the Glk library has timers, then on each call to os_readchar(), Glk Level 9
starts a timeout for a short period, then waits for either a key press event,
or a timeout.  If it receives a key press, it returns the code, otherwise it
returns zero.  This lets games run correctly.  Continually cycling through the
Glk timeout is fast enough to be acceptable to a player, but the timeout
provides good protection against spinning wildly in a tight loop.

If the Glk library does not have timers, then the interface will return zero
for some large number of consecutive calls, then wait until a character is
entered.  Since the initial number is large, this allows the game to progress,
generally, to the point where it needs a key press to continue at all.  The
only thing that might show slightly odd behaviour is the DEMO mode of Adrian
Mole games.

Also, this type of character polling cannot be used with reading input logs.
So the Glk "readlog" command does not function with Adrian Mole games.

To pretend that no key was pressed, when the interface is in fact waiting for
a key press, you can type Control-U.  The Glk interface will return this to
the game as a zero.

Games that rarely, or never, use line input, but continually poll for input
using os_readchar(), are difficult to stop.  To help with this, Glk Level 9
offers Control-C as as way to quit the game, with a confirmation as for "quit"
above.  Note that Control-C will only work when the game is using os_readchar()
(that is, most of the time for Adrian Mole games, and occasionally, if ever,
in other games).  For normal line input, use "quit" as described above.

Control-C is turned off by the command line option "-nc".


A Few Last Words on Graphics
----------------------------

When turning graphics on and off with Glk commands, you should pay special
attention to whether graphics are turned on or off in the game.  In order for
graphics to be displayed, they must be turned on both in the game AND in Glk.
Turning off graphics in Glk is a little more permanent than turning them off
in a game; in fact, some games don't seem to allow you to turn off graphics
within the game at all.

Why a secondary Glk way to turn off pictures?  Well, Level 9 games don't always
behave the same way with the "graphics" and "text" commands, so a control
inside Glk Level 9 itself gives a more consistent way to turn graphics on and
off.  The command

	glk graphics

with no on/off argument, prints out a brief information about the current state
of a game's graphics.  In practice, it's probably best to stick to just one
method of turning graphics on and off -- either the game's own way, or the
'glk graphics' command if the former is not available.

Glk Level 9 does not display all of the line drawn graphics correctly in a
handful of games.  Games known to not display all graphics fully correctly are
"Erik the Viking" (Spectrum, v2), "Return to Eden" (Spectrum48, v3), and
"Price of Magik" (Spectrum48, v3).


--

Simon Baldwin, simon_baldwin@yahoo.com
