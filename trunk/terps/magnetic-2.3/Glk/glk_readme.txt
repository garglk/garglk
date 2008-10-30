Magnetic Scrolls Interpreter, Glk Notes
---------------------------------------


Introduction
------------

This is a port of the Magnetic Scrolls interpreter to Glk.  The complete
interface lives in the single file

	glk.c

The main test and development system for the port is Linux, with Xglk as the
Glk library.


Acknowledgments
---------------

Thanks to David Kinder for hints and tips on how to go about the port, and of
course to Niclas Karlsson and his collaborators for the underlying Magnetic
Scrolls interpreter, which looks like it probably wasn't a lot of fun to work
on at times.

Thanks also to Ben Hines <bhines@alumni.ucsd.edu> for the Mac code.  I've
copied in from AGiliTy with, I think, the appropriate modifications, but it's
untested at present.


Running Games
-------------

The interpreter understands game files that use the "Magnetic Scrolls" data
format.  A file in this format usually has the extension ".MAG" or ".mag", and
has "MaSc" in its first four data bytes.

Give the name of the game file to run at the system prompt.  For example

	glkmagnetic GUILD.MAG

If the game has an associated graphics file, normally a file that has the
extension ".GFX" or ".gfx", make sure that it is in the same directory as the
main data file, and has the same base name.  For example, the matching graphics
file for "GUILD.MAG" is "GUILD.GFX".  The Glk interpreter will find this file
automatically, if it exists, and use it to create game pictures.

The game may also have an associated hints file, with the extension ".HNT" or
".hnt".  Like the graphics file, make sure it is in the same directory as other
game files, and with the same base name, and the Glk interpreter will find the
file automatically.  Hints files apply only to the later Magnetic Windows
games, so not all the games you find will have them.  Also, hints for several
games may be combined into a single file, so you might need to copy, or link,
a file such as "COLL.HNT" to "CCORRUPT.HNT" to match with a "CCORRUPT.MAG"
file.

If there is no graphics file, the interpreter will run in text only mode.  It
will also do this if it cannot open the graphics file, or if the file does not
seem to contain game graphics.

If there is no hints file, the interpreter will not be able to display game
hints, but will otherwise run normally.

As a short cut, you can omit the game extension, so for example

	xmagnetic GUILD

will run GUILD.MAG, and also use pictures from GUILD.GFX, and hints from
GUILD.HNT, if either or both are available.

Because of the way the Glk interpreter looks for files, you should make sure
that the games are held in files with ".MAG" or ".mag" extensions, and any
associated graphics and hints are held in files with ".GFX" or ".gfx" and
".HNT" or ".hnt" extensions respectively.  Games held in files with other
extensions may not work.

There are a few command line options that you can add to vary the way that
the game looks:

	-np	Don't display game pictures initially
	-ng	Don't automatically gamma correct game picture colours
	-nx	Don't animate game pictures
	-ne	Don't add an extra prompt on line input
	-na	Don't expand single-letter abbreviations
	-nc	Don't attempt to interpret selected commands locally

See below for further information about what these options mean.


Compiling
---------

To compile Glk Magnetic for Linux, first unpack the source files.  You might
need to use the -a argument to unzip in order to convert text files for your
system.

Edit Makefile.glk so that it has the right path to the Glk library you wish to
build with.  If you want to build the IFP plugin, also edit the parts of
Makefile.glk that have paths to IFP components.

To build standalone binary version of Glk Magnetic Scrolls, change directory
to the Glk subdirectory, and use

	make -f Makefile.glk glkmagnetic

To build the IFP plugin, use

	make -f Makefile.glk magnetic-2.3.so

To clean up and delete everything in the case of a build error, use

	make -f Makefile.glk clean


Extra Prompting
---------------

Magnetic Scrolls games always prompt with ">" when they are expecting input.
However, Magnetic Windows ones are sometimes strangely silent, for example
after receiving an empty line, they don't issue a second prompt.

To try to make it a little clearer to see what's going on, Glk Magnetic will
add its own ">" prompt when the game is expecting line input, but only when it
determines that the game has not already issued its own prompt.

You can turn this off with the '-ne' option.


Displaying Pictures
-------------------

Magnetic Scrolls games generally come with attractive graphics, and while not
essential to the game, they do enhance the experience of playing.

Unfortunately, Glk does not provide a method to directly display an image
bitmap, so Glk Magnetic needs to adopt some tricks to get a picture to display.
The end result is that, on Linux Xglk at least, it can take several seconds to
render a complete picture.

To reduce the problems caused by a game pausing for several seconds for a
picture,  Glk Magnetic does its picture rendering using a background "thread".
This draws in the complete picture over a series of smaller image updates,
while at the same time still allowing game play.  To speed up picture
rendering, Glk Magnetic also goes to considerable effort to try to minimize
the amount of Glk graphics operations it uses.

If you move to a location with a new picture before the current one has been
fully rendered, Glk Magnetic will simply start working on the newer picture.

Most Magnetic Windows and Magnetic Scrolls games accept the "graphics" command,
which toggles pictures on and off.  Magnetic Scrolls games usually start with
graphics enabled, but for some reason Magnetic Windows games tend to start
with graphics disabled.  To get pictures, type "graphics" as more or less the
first game command.

You can use the "-np" option to turn off pictures.  Glk Magnetic also will not
show pictures if the Glk library does not support both graphics and timers, if
there is no suitable ".GFX" or ".gfx" graphics file, or if the interpreter
cannot open the graphics file successfully.  See below for further notes about
displaying pictures.


Animated Pictures
-----------------

Some of the pictures in later Magnetic Windows games are animated, and Glk
Magnetic will animate those pictures.

Animations stretch the bitmap display capabilities of Glk even further than
static pictures do.  Glk Magnetic uses several optimizations to try to reduce
the graphics load on the system, and in particular animates only the portions
of a picture that change between frames.  As a result, pictures with modest
animations work well; pictures with large animated areas may however not
display as effectively.  Fortunately, most of the animated pictures in
Magnetic Windows games don't require large graphics repaints between frames.

The speed and smoothness of animations will probably also depend on the CPU
speed of your system, and on the graphics card, in particular any graphics
acceleration, available.

As with static pictures, Glk Magnetic interleaves animation frames with game
play using a background "thread", if necessary breaking individual animation
frames into pieces so that that game play is not interrupted for too long.

You can use the "-nx" option to turn off picture animation.  Glk Magnetic will
then display just the static portions of a game's pictures.


Gamma Colour Corrections
------------------------

Some of the Magnetic Scrolls pictures look fine without gamma corrections.  On
the other hand, others, in particular those in "Corruption", and several from
"Wonderland", can look very dark if not corrected.

To try to adjust for this, Glk Magnetic will apply an automatic gamma
correction to each picture.  It selects a suitable gamma based on analyzing the
colours that the picture uses, and choosing one that gives reasonable colour
contrast.

There are two levels of automatic gamma correction: 'normal' and 'high'.  In
normal mode, the gamma correction is half of that needed to completely even
out the contrast between picture colours, offering a reasonable balance between
the amount of picture brightening and garish or faded colours.  In high mode,
the gamma correction is that needed to fully even out the contrast between
picture colours.  Use "glk gamma" when in a game to switch between these modes,
or to turn gamma correction off altogether.

You can use the "-ng" option to turn off gamma colour corrections.  In this
case, all pictures are displayed using their original colour palette.


Game Hints
----------

Where a game comes with hints (Wonderland, and the "collection" games), Glk
Magnetic will try to display them if possible.

For Glk libraries that support separate windows, Glk Magnetic shows hints in a
pair of windows which overlay the main game text window.  The upper hint
window gives a menu of hint topics.  The lower one shows the game hints one at
a time.

For Glk libraries that don't support separate windows, Glk Magnetic shows hints
in the main game window, using the best formatting it can arrange.

Glk Magnetic will not show hints if there is no suitable ".HNT" or ".hnt" hints
file, or if the interpreter cannot open the hints file successfully.


Status Line
-----------

Magnetic Scrolls games print a status line, and Glk Magnetic will display this
in a separate window above the main window, for Glk libraries that support
separate windows.

For other Glk libraries, Glk Magnetic will print the status line in "[...]"
brackets above the standard game output following each turn, but only when the
status line has changed since the last turn.

Note that Magnetic Windows games generally do not print a status line.  For
these games, Glk Magnetic will instead try to identify the game's title, and
display it, if identifiable, on the status line.


Expanding Abbreviations
-----------------------

Many IF games systems allow a player to use single character abbreviations for
selected common commands, for example, 'x' for 'examine', 'l' for look, and so
on.  Not all Magnetic Scrolls games offer this feature, however.

To try to help the player, Glk Magnetic will automatically expand a selection
of single character commands, before passing the expanded string to the game
as input.  It expands a command only if the first word of the command is a
single letter, and one of the following:

	'c' -> "close"		'g' -> "again"		'i' -> "inventory"
	'k' -> "attack"		'l' -> "look"		'p' -> "open"
	'q' -> "quit"		'r' -> "drop"		't' -> "take"
	'x' -> "examine" 	'y' -> "yes"		'z' -> "wait"

If you want to suppress abbreviation expansion, you can prefix your input with
a single quote character (like putting literal strings into a spreadsheet).
If you do this, the Glk interface will strip the quote, then pass the rest of
the string to the main interpreter without any more changes.  So for example,

	'x something

will pass the string "x something" back to the game, whereas

	x something

will pass "examine something" back to the game.

You can turn off abbreviation expansions with the command line option '-na'.


Interpreting Commands Locally
-----------------------------

Glk Magnetic will handle special commands if they are prefixed with the string
'glk'.  It understands the following special commands:

	help		  Prints help on Glk special commands
	summary		  Prints all current Glk settings
	version		  Prints the Glk library and Glk port version numbers
	license		  Prints the Glk port license

	undo		  Undoes the prior game move
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
	graphics on	  Turn on Glk graphics
	graphics off	  Turn off Glk graphics (see below)
	gamma normal	  Set automatic gamma colour correction mode to normal
	gamma high	  Set automatic gamma colour correction mode to high
	gamma off	  Turn automatic gamma colour correction off
	animations off	  Turn picture animation off
	animations on	  Turn picture animation on
	prompts on	  Turn extra "> " prompting on
	prompts off	  Turn extra "> " prompting off
	commands off	  Turn of Glk special commands; once off, there is no
			  way to turn them back on

You can abbreviate these commands, as long as the abbreviation you use is
unambiguous.  To improve compatibility with other IF games systems, Glk
Magnetic also recognizes the single word command "undo" as being equivalent
to "glk undo".

If for some reason you need to pass the string "glk" to the interpreter, you
can, as with abbreviations above, prefix it with a single quote character.

You can turn off local command handling with the command line option '-nc'.

If both abbreviation expansion and local command handling are turned off, there
is no need to use single quotes to suppress special interpreter features.


A Few Last Words on Graphics
----------------------------

When turning graphics on and off with Glk commands, you should pay special
attention to whether graphics are turned on or off in the game.  In order for
graphics to be displayed, they must be turned on both in the game AND in Glk.
Turning off graphics in Glk is a little more permanent than turning them off
in a game.  In some games, turning graphics off seems to mean do not display
new pictures, but leave the current picture on the display; turning off
graphics in Glk closes the graphics display window completely.

What can also be confusing is the tendency of Magnetic Windows games to display
a picture automatically only on your first visit to a game location, unless in
'verbose' mode.  On subsequent visits, a "look" command is needed to redisplay
the picture.  This appears not to happen with Magnetic Scrolls games.

Why an additional Glk way to turn off pictures?  Well, the Magnetic Windows and
Magnetic Scrolls games don't always behave the same way with the "graphics"
command, so a control inside Glk Magnetic itself gives a more consistent way
to turn graphics on and off.  The command

	glk graphics

with no on/off argument, prints out a brief information about the current state
of a game's graphics.  In practice, it's probably best to stick to just one
method of turning graphics on and off -- either the game's own way, or the
'glk graphics' command.


--

Simon Baldwin, simon_baldwin@yahoo.com
