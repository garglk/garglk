# Gargoyle for Kindle Paperwhite 3

Changes to upstream gargoyle (https://github.com/garglk/garglk): <br/>
Kindle Port based on patches from: http://www.fabiszewski.net/kindle-gargoyle/ <br/>
Kindle Port incorporates TADS 3.1.3 branch from: https://github.com/cspiegel/garglk/tree/203-tads-upgrade <br/>
Kindle Port incorporates "double click/double tap on word to insert" patch from: https://github.com/RedHatter/granite <br/>
Kindle Port maps swipe up / down on touchscreen to page up / page down in the text window.

# Gargoyle [![Build Status](https://travis-ci.org/garglk/garglk.svg?branch=master)](https://travis-ci.org/garglk/garglk)

## An interactive fiction player

Gargoyle is an IF player that supports all the major interactive fiction formats.

Most interactive fiction is distributed as portable game files. These portable game files come in many formats. In the past, you used to have to download a separate player (interpreter) for each format of IF you wanted to play.

Gargoyle is based on the standard interpreters for the formats it supports. Gargoyle is free software released under the terms of the GNU General Public License.

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

The default fonts for Gargoyle are Bitstream Charter and Luxi Mono. They are included, so there is no need to install anything on your system.

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
