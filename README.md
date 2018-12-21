# Gargoyle for Kindle Touch Devices (Touch, Paperwhite 3, etc.)

Binaries are available here:
https://www.mobileread.com/forums/showpost.php?p=3526963&postcount=1

Changes to upstream gargoyle (https://github.com/garglk/garglk):
* Kindle port based on patches from: http://www.fabiszewski.net/kindle-gargoyle/
* Supported in-game touch screen gestures:
 * One finger single tap on input line: place cursor
 * One finger double tap on word in input line: place cursor after tapped word
 * One finger double tap on word in game text to insert it into input line at cursor position (based on patch from: [url]https://github.com/RedHatter/granite[/url]) 
 * One finger swipe up: page up
 * One finger swipe down: page down
 * One finger swipe left: move cursor left
 * One finger swipe right: move cursor right
 * Two finger single tap on top-left side of the game screen: clear input line
 * Two finger single tap on middle-left side of the game screen: delete characters left of cursor until first whitespace
 * Two finger single tap on bottom-left side of the game screen: move cursor to the beginning of the word to the left
 * Two finger single tap on top-right side of the game screen: delete character right of cursor (= DEL key)
 * Two finger single tap on middle-right side of the game screen: delete characters right of cursor until first whitespace
 * Two finger single tap on bottom-right side of the game screen: move cursor to the beginning of the next word
 * Two finger single tap on top-center of the game screen: command history: previous (= key "cursor up")
 * Two finger single tap on bottom-center of the game screen: command history: next (= key "cursor down")

Kindle specific fullscreen layout & save/restore dialogs can be activated by defining: _KINDLE at compile time.<br/>
Touch features/gestures can be activated by defining: _ALT_MOUSE_HANDLING at compile time.

# Gargoyle [![Build Status](https://travis-ci.org/garglk/garglk.svg?branch=master)](https://travis-ci.org/garglk/garglk)

## An interactive fiction player

Gargoyle is an IF player that supports all the major interactive fiction formats.

Most interactive fiction is distributed as portable game files. These portable game files come in many formats. In the past, you used to have to download a separate player (interpreter) for each format of IF you wanted to play.

Gargoyle is based on the standard interpreters for the formats it supports. Gargoyle is free software released under the terms of the GNU General Public License.

Gargoyle can be downloaded from the [releases page](https://github.com/garglk/garglk/releases).

## Typography in interactive fiction

Gargoyle cares about typography! In this computer age of typographical poverty, where horrible fonts, dazzling colors, and inadequate white space is God, Gargoyle dares to rebel!

* Subpixel font rendering for LCD screens.
* Unhinted anti-aliased fonts: beautiful, the way they were designed.
* Adjustable gamma correction: tune the rendering for your screen.
* Floating point text layout for even spacing.
* Kerning for even more even spacing.
* Smart quotes and other punctuation formatting.
* Ligatures for “fi” and “fl”.
* Plenty o' margins.
* Plenty o' line spacing.
* Integrated scrollback.

The default fonts for Gargoyle are Noto Serif and Go Mono. They are included, so there is no need to install anything on your system.

Gargoyle does not use any operating system functions for drawing text, so it can use any TrueType, OpenType or Postscript font file you specify in the configuration file.

## Many many formats

There are many interpreters that have been Glk-enabled. All of the following interpreters have been compiled for Gargoyle and are included in the package:

* AdvSys
* Agility
* Alan 2 and 3
* Bocfel
* Frotz
* Geas
* Git
* Glulxe
* Hugo
* JACL
* Level 9
* Magnetic
* Nitfol
* Scare
* Scottfree
* TADS 2 and 3

These interpreters are freely distributable, but are still copyrighted by their authors and covered by their own licenses.

Gargoyle was originally developed by Tor Andersson.
