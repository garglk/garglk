# Gargoyle

[![AppImage](https://github.com/garglk/garglk/actions/workflows/appimage.yml/badge.svg)](https://github.com/garglk/garglk/actions/workflows/appimage.yml)
[![MinGW](https://github.com/garglk/garglk/actions/workflows/mingw.yml/badge.svg)](https://github.com/garglk/garglk/actions/workflows/mingw.yml)
[![macOS](https://github.com/garglk/garglk/actions/workflows/macos-dmg.yml/badge.svg)](https://github.com/garglk/garglk/actions/workflows/macos-dmg.yml)
[![Ubuntu](https://github.com/garglk/garglk/actions/workflows/ubuntu-deb.yml/badge.svg)](https://github.com/garglk/garglk/actions/workflows/ubuntu-deb.yml)

## An interactive fiction player

Gargoyle is an IF player that supports all the major interactive fiction
formats.

Most interactive fiction is distributed as portable game files. These portable
game files come in many formats. In the past, you used to have to download a
separate player (interpreter) for each format of IF you wanted to play.

Gargoyle is based on the standard interpreters for the formats it supports.
Gargoyle is free software released under the terms of the GNU General Public
License.

Gargoyle can be downloaded from the [releases page](https://github.com/garglk/garglk/releases).

## Typography in interactive fiction

Gargoyle cares about typography! In this computer age of typographical poverty,
where horrible fonts, dazzling colors, and inadequate white space is God,
Gargoyle dares to rebel!

* Subpixel font rendering for LCD screens.
* Unhinted anti-aliased fonts: beautiful, the way they were designed.
* Adjustable gamma correction: tune the rendering for your screen.
* Floating point text layout for even spacing.
* Kerning for even more even spacing.
* Smart quotes and other punctuation formatting.
* Ligatures for “fi”, “fl”, “ff”, “ffi”, and “ffl”.
* Plenty o' margins.
* Plenty o' line spacing.
* Integrated scrollback.

The default fonts for Gargoyle are Charis SIL (as Gargoyle Serif) and Go Mono
(as Gargoyle Mono). They are included, so there is no need to install anything
on your system.

Gargoyle does not use any operating system functions for drawing text, so it can
use any TrueType, OpenType or Postscript font file you specify in the
configuration file.

## Many many formats

There are many interpreters that have been Glk-enabled. All of the following
interpreters have been compiled for Gargoyle and are included in the package:

* AdvSys
* Agility
* Alan 2 and 3
* Bocfel
* Git
* Glulxe
* Hugo
* JACL
* Level 9
* Magnetic
* Plus
* Scare
* Scottfree
* TADS 2 and 3
* TaylorMade

These interpreters are freely distributable, but are still copyrighted by their
authors and covered by their own licenses.

Gargoyle was originally developed by Tor Andersson.

## Configuration

Gargoyle is highly configurable, although its configuration is done entirely
through a text file rather than a graphical tool.

Several methods are provided to aid with configuration. The preferred method is
to start a game and then use the shortcut `Ctrl`+`,` (`Command`+`,` on Mac).
This will open Gargoyle's configuration file in a text editor. If you already
have a config file, it will use that; otherwise, a default config file will be
copied to an appropriate location first. This should be enough for the vast
majority of users, as they will not care where the configuration file is, only
that it can be edited. The configuration file itself contains a lot of
documentation, so should be fairly easy to understand.

It's also possible to open the config file in an editor by passing the
--edit-config flag to the main Gargoyle binary:

    $ gargoyle --edit-config

For modern Unix desktops, Gargoyle includes a desktop entry called "Gargoyle
Config Editor" which should be accessible by any desktop/launcher which supports
the Freedesktop.org Desktop Entry Specification. This will launch Gargoyle with
the --edit-config flag.

Alternatively, load a game and hit control-period (command-period on Mac). This
will bring up a message box listing all configuration files in the order they
will be searched.

Similarly, on non-Mac platforms, the --paths flag can be passed to Gargoyle to
list all config file paths:

    $ gargoyle --paths
    Configuration file paths:

    /home/user/.config/garglk.ini [user, non-existent]
    /home/user/.garglkrc [user]
    /etc/garglk.ini [system]

The way configuration files is loaded is somewhat complicated. There are three
"classes" of configuration file: system, user, and game-specific.

System configuration files are those which are installed as part of the system
and are not meant to be edited by the user. On Unix, this is typically something
like /etc/garglk.ini.

User configuration files are those which are meant to be edited by the user, and
are where any custom configuration should go. Ideally there would be a single
user configuration file, but for historical reasons there usually are multiple
possible locations.

Finally, game-specific configuration files are those which apply to a single
game (or a game directory). The intent is for game authors to be able to tweak
Gargoyle's behavior for their specific games.

Note that in the above list of paths, no game-specific configurations were
listed. That's because no game was provided. If a game is provided, the output
looks something like this:

    $ gargoyle --paths $HOME/if/infocom/zork1.z3
    Configuration file paths:

    /home/user/if/infocom/zork1.ini [game specific, non-existent]
    /home/user/if/infocom/garglk.ini [game specific, non-existent]
    /home/user/.config/garglk.ini [user, non-existent]
    /home/user/.garglkrc [user]
    /etc/garglk.ini [system]

Notice how the paths include information on both the type of config file as well
as whether the file exists, giving a good view of what will apply when Gargoyle
is run.

Paths are listed in order of priority, highest to lowest. Game-specific
configuration overrides user configuration, which overrides system
configuration. It's important to note that _all_ configuration files are
consulted. Gargoyle does not just stop at the first one it finds. Instead, it
goes "bottom up", loading system configuration first. Then, user configurations
are loaded, overriding any previous settings from the system configuration.
Finally, game-specific configurations are loaded, overriding any previous
settings from user and system configurations.

The upshot is that game-specific configurations can include just a few settings
(e.g. enforcing a minimim column count), allowing the majority of the user's
settings to still be applied.

## Runtime options

Gargoyle generally gets out of the way when running a game, presenting just the
interpreter. However, there are a few shortcuts available (on Mac, use
`Command` in place of `Ctrl`):

* `Ctrl`+`,`: Open Gargoyle's configuration file in a text editor.
* `Ctrl`+`.`: List the paths to all configuration files Gargoyle consults, as
  well as all theme paths Gargoyle consults (see THEMES.md for more information
  on color themes).
* `Ctrl`+`Shift`+`t`: Display a list of all available color themes (see
  THEMES.md for more information on color themes).
* `Ctrl`+`Shift`+`s`: Save the scrollback buffer to a file, effectively
  creating an ad hoc transcript.
* `Alt`+`Enter` (or `Command`-`Ctrl`-`f` on Mac): Toggle fullscreen.

In addition, Gargoyle supports many readline/Emacs-style line-editor bindings:

* `Ctrl`+`a`: Go to the beginning of the line.
* `Ctrl`+`b`: Move the cursor to the left.
* `Ctrl`+`d`: Erase the character under the cursor.
* `Ctrl`+`e`: Go to the end of the line.
* `Ctrl`+`f`: Move the cursor to the right.
* `Ctrl`+`h`: Erase the character to the left of the cursor.
* `Ctrl`+`n`: Next history entry.
* `Ctrl`+`p`: Previous history entry.
* `Ctrl`+`u`: Erase entire line.
