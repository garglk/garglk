# Gargoyle themes

## Overview

Gargoyle's colors can be configured through its configuration file, but the
syntax is verbose and it's not easy to switch between color configurations.
Gargoyle now has a way to specify entire color themes and switch between them
with a single configuration option.

Color themes are JSON files which specify all possible colors. Gargoyle comes
with some color themes, and you are able to create your own as well. Where to
place these color themes depends on your operating system. To see all theme
paths, hit control-period (or command-period on Mac) while Gargoyle is running.
The first listed theme path should be your personal path which you can install
new themes into.

## Definitions

### Colors

Colors are given in the usual *#RRGGBB* notation. Shorthand is not allowed (for
example *#f0f* is not interpreted as *#ff00ff*).

### Color pairs

A color pair is a pair of colors representing a foreground color and a
background color. It is a simple JSON object:

    {"fg": "#333333", "bg": "#ffffff"}

## Theme format

Example theme:

    {
        "name": "Lectrote Slate",
        "window": "#4d4d4f",
        "border": "#000000",
        "caret": "#ffffff",
        "link": "#000060",
        "more": "#ffddaa",
        "scrollbar": {"fg": "#585868", "bg": "#4d4d4f"},
        "text_buffer": {
            "default": {"fg": "#ffffff", "bg": "#4d4d4f"},
            "input": {"fg": "#ddffdd", "bg": "#4d4d4f"}
        },
        "text_grid": {
            "default": {"fg": "#333333", "bg": "#ffffff"}
        }
    }

All top-level keys are required.

*name* is the name of the theme, and is how it is referred to in the
configuration file.

The following keys are all colors:

* *window*: The overall color of the window background
* *border*: The color of borders (if any) between windows
* *caret*: The color of the input cursor
* *link*: The color of hyperlinks
* *more*: The color of the MORE prompt

*scrollbar* is a color pair which gives the color of the scrollbar. By default,
no scrollbar is shown in Gargoyle. The *scrollwidth* Gargoyle configuration
option can be used to enable the scrollbar.

The final two keys:

* *text_buffer*: The color of text styles for text buffer windows
* *text_grid*: The color of text styles for text grid windows

### Text styles

Glk text can be styled in many different ways (see [Styles in the Glk
specification](https://www.eblong.com/zarf/glk/Glk-Spec-075.html#stream_style)
for more information). For text buffers and text grids, you must specify color
pairs for all styles, although you can set a default which will be used as a
fallback for styles which are not explicitly set.

*text_buffer* and *text_grid* are objects mapping a style to a pair of colors.
The key is either *default* to set the default pair, or one of the following,
which correspond to Glk styles as mentioned above:

* *normal*
* *emphasized*
* *preformatted*
* *header*
* *subheader*
* *alert*
* *note*
* *blockquote*
* *input*
* *user1*
* *user2*

These must be spelled exactly as above (which matches the Glk specification);
for example, you cannot use *emphasised* in place of *emphasized*. Case also
matters: it is *emphasized*, not *Emphasized* or *EMPHASIZED*.

## Selecthing themes

Selecting a theme is as easy as putting the following in your configuration file:

    theme Lectrote Slate

This must come *after*, or replace, all other color-related configuration
options, or else the colors specified by the theme will be overridden by the
following entries. The theme name is case sensitive.

Gargoyle also includes a pseudo theme called *system* which attempts to follow
the system's color theme, adapting to whatever your current operating system
color theme is. It has built-in light and dark themes that are used, but you can
override these by providing your own themes named *light* and/or *dark*.
Gargoyle will then use your provided theme as the system theme instead of one of
its internal themes.
