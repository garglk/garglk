# S.A.G.A. Plus Game Data Unofficial Specification

This document describes the game data of the S.A.G.A. Plus adventures (Scott Adams Graphic Adventure Plus) that the Plus interpreter in this directory runs, in the order they were written: *Questprobe featuring The Amazing Spider-Man*, *The Adventures of Buckaroo Banzai*, *Questprobe featuring The Fantastic Four*, *Sorcerer of Claymorgue Castle* (an earlier Scott Adams game remade for Plus), and *X-Men*, which was still unfinished when Adventure International closed. There is no official specification; the format is defined by the original interpreters of each system.

The design grows out of the earlier Scott Adams engine, which ScottFree runs. That engine has rooms, items, a verb–noun dictionary, an action table and messages, and Plus keeps all of them. What is new is a larger state (64 numbered counters and 64 flags), a parser that knows adverbs, prepositions and a second noun, conditions that combine with AND, OR, XOR, NOT and brackets, loops in the command list, numbers longer than one byte, a substitution table that rewrites the typed line, and pictures drawn per item.

This document is based on what this interpreter does, which has been tested against the original games, and on the MS-DOS interpreters, which were checked where this interpreter's behaviour was in doubt. Statements this interpreter does not depend on are marked **(unverified)**.

The document describes the games and their original engines. Where it describes what this interpreter does instead, it says so ([§2.5](#25-the-original-engine-and-this-interpreter)).

## Contents

- [1. Scope](#1-scope)
  - [1.1 Games](#11-games)
  - [1.2 Systems](#12-systems)
  - [1.3 Identifying a database](#13-identifying-a-database)
- [2. General conventions](#2-general-conventions)
  - [2.1 Notation](#21-notation)
  - [2.2 Counts are highest indices](#22-counts-are-highest-indices)
  - [2.3 Actions and commands](#23-actions-and-commands)
  - [2.4 Operand types](#24-operand-types)
  - [2.5 The original engine and this interpreter](#25-the-original-engine-and-this-interpreter)
- [3. Game state](#3-game-state)
  - [3.1 Counters](#31-counters)
  - [3.2 Flags](#32-flags)
  - [3.3 Locations](#33-locations)
  - [3.4 Initial state](#34-initial-state)
- [4. Text](#4-text)
  - [4.1 Strings](#41-strings)
  - [4.2 Messages](#42-messages)
  - [4.3 Spacing](#43-spacing)
  - [4.4 The room display](#44-the-room-display)
- [5. Dictionaries](#5-dictionaries)
  - [5.1 Dictionary strings](#51-dictionary-strings)
  - [5.2 Substitutions](#52-substitutions)
- [6. Rooms](#6-rooms)
- [7. Items](#7-items)
  - [7.1 Item pictures](#71-item-pictures)
- [8. Actions](#8-actions)
  - [8.1 Action layout](#81-action-layout)
  - [8.2 Extra words](#82-extra-words)
  - [8.3 Conditions](#83-conditions)
  - [8.4 Control conditions](#84-control-conditions)
  - [8.5 Command encoding](#85-command-encoding)
  - [8.6 Commands](#86-commands)
  - [8.7 System commands](#87-system-commands)
  - [8.8 Differences between the original engines](#88-differences-between-the-original-engines)
- [9. Execution](#9-execution)
  - [9.1 The turn](#91-the-turn)
  - [9.2 Automatic actions](#92-automatic-actions)
  - [9.3 Explicit actions](#93-explicit-actions)
  - [9.4 The parser](#94-the-parser)
- [10. Database layouts](#10-database-layouts)
  - [10.1 Header](#101-header)
  - [10.2 Plain text database](#102-plain-text-database)
  - [10.3 Binary database](#103-binary-database)
- [11. Containers](#11-containers)
  - [11.1 MS-DOS](#111-ms-dos)
  - [11.2 Commodore 64](#112-commodore-64)
  - [11.3 Atari 8-bit](#113-atari-8-bit)
  - [11.4 Apple II](#114-apple-ii)
  - [11.5 Atari ST](#115-atari-st)
- [12. Pictures](#12-pictures)
  - [12.1 Picture numbering](#121-picture-numbering)
  - [12.2 What is drawn](#122-what-is-drawn)
  - [12.3 Commodore 64 and Atari 8-bit](#123-commodore-64-and-atari-8-bit)
  - [12.4 Apple II](#124-apple-ii)
  - [12.5 MS-DOS](#125-ms-dos)
  - [12.6 Atari ST](#126-atari-st)
- [13. Animation](#13-animation)
  - [13.1 The original engine](#131-the-original-engine)
  - [13.2 This interpreter](#132-this-interpreter)
- [14. Saved games](#14-saved-games)
  - [14.1 What is saved](#141-what-is-saved)
  - [14.2 MS-DOS](#142-ms-dos)
  - [14.3 Commodore 64 and Plus/4](#143-commodore-64-and-plus4)
  - [14.4 Atari 8-bit](#144-atari-8-bit)
  - [14.5 Apple II](#145-apple-ii)
  - [14.6 Atari ST](#146-atari-st)
  - [14.7 This interpreter](#147-this-interpreter)
- [Appendix A. Command summary](#appendix-a-command-summary)
- [Appendix B. Condition summary](#appendix-b-condition-summary)

---

## 1. Scope

### 1.1 Games

| Game | Title string | Room / item / special pictures |
|------|--------------|-------------------------------|
| *Questprobe featuring The Amazing Spider-Man* | `SPIDER-MAN (tm)` | 18 / 26 / 26 |
| *The Adventures of Buckaroo Banzai* | `BUCKAROO` | 27 / 24 / 20 |
| *Questprobe featuring The Fantastic Four* | `FF #1 ` | 21 / 22 / 21 |
| *Sorcerer of Claymorgue Castle* | `Sorcerer of Claymorgue Castle. SAGA#13.` | 33 / 52 / 6 |
| *X-Men* (unfinished) | `X-MEN ` | none |

The title string is stored in the database ([§10](#10-database-layouts)); the trailing spaces of two of them are part of the string. The picture counts are the most pictures of each kind ([§12.1](#121-picture-numbering)) that this interpreter looks for in the MS-DOS files.

*X-Men* exists only as an MS-DOS plain text database (`SPL54.DAT`), with no pictures. Its interpreter reports itself as S.A.G.A.+ version 225.

### 1.2 Systems

The games were released for five systems, which store the same data in two layouts:

| System | Database | Pictures |
|--------|----------|----------|
| MS-DOS | Plain text ([§10.2](#102-plain-text-database)) | One `.PAK` file each ([§12.5](#125-ms-dos)) |
| Commodore 64 | Binary, little-endian ([§10.3](#103-binary-database)) | One file each on a D64 disk image ([§12.3](#123-commodore-64-and-atari-8-bit)) |
| Atari 8-bit | Binary, little-endian | A table of sectors on the disk ([§11.3](#113-atari-8-bit)) |
| Apple II | Binary, little-endian | Addresses on the disk, held in the database ([§11.4](#114-apple-ii)) |
| Atari ST | Binary, big-endian, sections padded | One file each on the disk ([§12.6](#126-atari-st)) |

Not every game exists on every system.

### 1.3 Identifying a database

A database is recognised by its title string, which must equal one of the strings of [§1.1](#11-games) exactly. This interpreter tries the plain text layout first; if the title does not match, it tries the binary layout on the disk image formats in the order Atari 8-bit, Atari ST, Apple II, Commodore 64. A binary database must also pass a header check ([§10.3](#103-binary-database)) before its title is read.

---

## 2. General conventions

### 2.1 Notation

- Hexadecimal values are written with a `0x` prefix, or in code font when they are byte strings (`96 02 80`).
- *n*, *a*, *b* stand for operands. `counter[n]` is counter *n*; `flag[n]` is flag *n*.
- LE16 and BE16 are 16-bit values, little-endian and big-endian.
- "Here" means in the player's current room (counter 32).

### 2.2 Counts are highest indices

Every count in the header is the **highest index**, not the number of entries: a header value of *N* means entries 0 to *N*, which is *N* + 1 entries. This holds for items, rooms, messages, verbs, nouns, prepositions, adverbs, substitutions, object pictures and actions alike. Entry 0 always exists and is usually a placeholder.

### 2.3 Actions and commands

An **action** is one entry of the action table: the words that trigger it, its conditions, and a list of **commands** to carry out ([§8](#8-actions)). A command is a number from 0 to 127, or a message number from 128 up, optionally followed by operands. Parts of the source code use "action" for a command.

### 2.4 Operand types

Commands and conditions take these operand types:

| Type | Meaning |
|------|---------|
| *item* | An item number, or one of the special values below |
| *room* | A room number |
| *counter* | A counter number 0–63 |
| *flag* | A flag number 0–63 |
| *num* | A plain number |
| *noun* | A noun group number, or one of the special values below |
| *msg* | A message number |

Three values stand for a word of the typed command rather than a fixed number:

| Value | As an *item* | As a *noun* |
|-------|--------------|-------------|
| 997 (`ARG_WITH_LIST`) | The item in counter 45 | The noun of the "with" list that counter 44 points at ([§9.4](#94-the-parser)) |
| 998 (`ARG_NOUN`) | The item in counter 30 | The noun typed |
| 999 (`ARG_NOUN2`) | The item in counter 31 | The second noun typed |

A *room* operand may also be one of these values, and then means the room of the item it stands for.

### 2.5 The original engine and this interpreter

Two kinds of program run this data, and the document keeps them apart:

- **The original engine** is the interpreter that shipped with each release, in machine code for its system (the *original*, for short). Unqualified statements about what happens when a game runs, such as how the action table is scanned or how a condition is evaluated, describe the original engine.
- **This interpreter** is Plus, the C reimplementation in this directory. Statements about it always say "this interpreter" or name its source files. They cover what the original has no counterpart for (finding the data in a disk image, the saved game format, undo), and the places where this interpreter deliberately differs from the original.

The original engines differ between systems mostly in how they read the data and draw pictures. Where they differ in how they run a game, this document describes the MS-DOS engine, and [§8.8](#88-differences-between-the-original-engines) lists the differences.

---

## 3. Game state

### 3.1 Counters

There are 64 counters, each a signed 16-bit number. Counters 0–29 and 54–59 belong to the game. The engine uses the others:

| Counter | Meaning |
|---------|---------|
| 30 | The item that the noun of the command stands for ([§9.3](#93-explicit-actions)) |
| 31 | The item that the second noun stands for |
| 32 | The current room |
| 33 | The number of items carried, as counted by the last `GET` (command 52) |
| 34 | The treasure room |
| 35 | The room the player goes to on death (command 61) |
| 36–41 | Free. Used for holding rooms by the earlier engine, and free for the game to use |
| 42 | The most items the player can carry |
| 43 | The number of the last item |
| 44 | Which entry of the "with" list `ARG_WITH_LIST` stands for |
| 45 | The item that `ARG_WITH_LIST` stands for |
| 46 | The number of nouns in the "with" list |
| 47 | The **current counter**, which commands 77–83 work on |
| 48 | The verb of the command |
| 49 | The noun of the command |
| 50 | The preposition of the command |
| 51 | The adverb of the command |
| 52 | The participle of the command |
| 53 | The second noun of the command |
| 60–62 | Reserved |
| 63 | The item register, used by system commands 9 and 12 ([§8.7](#87-system-commands)) |

Counters 48–53 are set from the parsed command before the explicit actions are scanned, and again by commands 55 and 120. Command 121 copies them back, so a game can change the command and search again ([§8.6](#86-commands)).

### 3.2 Flags

There are 64 flags, each on or off. Several have fixed roles:

| Flag | Meaning |
|------|---------|
| 0 | The flag that commands 67 and 68 set and clear |
| 15 | Dark. The room is dark when this flag is set and no light source (an item with flag value 2, [§7](#7-items)) is here or carried |
| 16 | "Light out" in the earlier engine. Free for the game here |
| 33 | **Match**: an action matched the words of the command, but its conditions failed ([§9.3](#93-explicit-actions)) |
| 34 | **Draw**: the room picture needs redrawing |
| 35 | Graphics on. Set at the start; when clear, no pictures are drawn |
| 60 | **Always match**: the first explicit action matches whatever is typed ([§9.3](#93-explicit-actions)) |
| 63 | **Stop time**: skip the automatic actions once ([§9.1](#91-the-turn)) |

Flag 60 is not used this way in *Spider-Man* or *Buckaroo Banzai*; *Buckaroo Banzai* uses flag 60 as an ordinary flag. *X-Men* uses it for commands addressed to other characters.

Game-specific flags that this interpreter reads:

| Game | Flag | Meaning |
|------|------|---------|
| *Spider-Man* | 9 | The web animation is loaded |
| *Spider-Man* | 10 | The Sandman animation is loaded |
| *Buckaroo Banzai* | 6 | Blackout: room 29 stays black ([§13.2](#132-this-interpreter)) |

### 3.3 Locations

An item's location is a room number or one of these values:

| Value | Meaning |
|-------|---------|
| 0 | Destroyed (out of play) |
| 255 | Carried by the player (−1 in the MS-DOS interpreters) |

No other value is special. A database can keep an item out of sight by putting it in a room number above the highest room: *Buckaroo Banzai* starts its jug of jet fuel in 50, and *The Fantastic Four* and *Claymorgue* start an item in 99. The engine treats these like any other room number; the MS-DOS *Fantastic Four* interpreter never compares a location with 50 or 99. In *X-Men*, which has 105 rooms, 50 and 99 are ordinary rooms.

A location is a 16-bit number in the running game. *X-Men* uses room 500 as a limbo that is not 0: when the player takes the plastic clock on the wall (item 63), it is put in room 500 and the portable clock (64) is taken instead. Its location must not be 0, because the continuation actions that hang the wall clocks (items 63 and 65) in the room do so while item 63 is not in play (condition 14), and would bring it back. The databases store the initial locations in one byte (binary) or as a number where −1 means carried (plain text).

### 3.4 Initial state

When a game starts:

- Every counter and flag is 0, except as below.
- Counter 32 (current room) is the header's start room.
- Counter 34 (treasure room) is the header's treasure room.
- Counter 35 (dead room) is the header's room count, which is the number of the last room.
- Counter 42 (most items carried) is the header's maximum carried.
- Counter 43 (last item) is the header's item count.
- Flag 35 (graphics on) is set.
- Every item is in its initial location.

---

## 4. Text

### 4.1 Strings

Text is stored as plain 8-bit ASCII with no compression. A backtick (`` ` ``) in the data stands for a double quote.

### 4.2 Messages

Messages are numbered from 0. A command prints a message by number ([§8.5](#85-command-encoding)). A room's description is also a message ([§6](#6-rooms)).

The system messages ("I didn't completely understand your command", "I can't do that yet", the inventory and look texts and so on) are not in the database: each original engine has its own, and this interpreter has its own too.

### 4.3 Spacing

The output routine joins messages with spaces, with these rules:

- A message that starts with `\` or a space is printed with no space before it. The `\` is not printed.
- A comma at the end of a message is held back, and printed only if more text follows.
- A single trailing space is removed.

### 4.4 The room display

The room description is "You are " followed by the room's text, unless the text starts with `*`; then the `*` is removed and nothing is added.

Items here with non-empty text follow as one sentence: "You can also see a *x*, a *y*, and a *z*". The article comes from the first letter of the item text: "an" before a vowel, nothing before a space (the text supplies its own words), "a" otherwise. Items with empty text are never listed.

Exits follow. With one exit: "Obvious exits: " and the direction. With several: " I see exits to the North, South, and East."

In the dark, only "I can't see. It is too dark!" is shown.

---

## 5. Dictionaries

There are four dictionaries: verbs, nouns, prepositions and adverbs. Participles (the word before a second noun, such as WITH) are in the preposition dictionary.

### 5.1 Dictionary strings

Each dictionary is a list of strings, one per **group**. A group is the number the engine uses for a word, and is its position in the list, counting from 0. The string lists the group's words, each preceded by a comma and the list ended by a comma:

```
,GET,TAKE,GRAB,
```

All the words of a string are synonyms. A `#` in a noun separates homonyms that need disambiguating ([§9.4](#94-the-parser)): `KEY#1` and `KEY#2` are both typed KEY.

Some groups have fixed meanings:

| Dictionary | Group | Meaning |
|------------|-------|---------|
| Noun | 0 | `ANY`: matches any noun |
| Preposition | 2 | WITH. As a participle it starts a "with" list. It cannot be the first word of a command |
| Preposition | 3 | AT. As a participle it starts a "with" list |

### 5.2 Substitutions

The substitution table rewrites the typed line before it is split into words. Each entry is a string of the form

```
,A,B,=C,
```

which says that A and B are both replaced by C. A pattern may hold more than one word.

Replacements happen in table order. A pattern matches only at word boundaries. Each pattern is replaced again and again until it no longer matches; this interpreter replaces a pattern only once if its replacement contains the pattern itself, as *X-Men*'s `,BRICK,BRICKED UP,BRICKED,BRICK WALL,=BRICK,` does, which would otherwise never end.

---

## 6. Rooms

Each room has eight values:

| Column | Meaning |
|--------|---------|
| 0–5 | The room to the north, south, east, west, up and down; 0 for no exit |
| 6 | The description, as the command that prints its message: message number + 76, or 0 for none |
| 7 | The room picture number, or 255 for none |

Room 0 is a placeholder.

---

## 7. Items

Each item has:

| Field | Meaning |
|-------|---------|
| Text | What the room display and inventory print. Empty for items that are never listed |
| Location | The initial location ([§3.3](#33-locations)) |
| Word | The noun group the player uses for it, or 0 for none |
| Flags | A 16-bit set of bits, tested by condition 30 |

Two flag values matter to the engine: value 1 marks an item the parser can match to a noun ([§9.3](#93-explicit-actions)), and value 2 marks a light source.

### 7.1 Item pictures

The object picture table says which picture shows which item in which room:

| Field | Meaning |
|-------|---------|
| Room | The room |
| Object | The item |
| Image | The picture number (an address on the Apple II) |

An item can have different pictures in different rooms, and an entry draws only when the item is in that room ([§12.2](#122-what-is-drawn)).

---

## 8. Actions

### 8.1 Action layout

Each action has:

| Field | Meaning |
|-------|---------|
| Length byte | High nibble: the number of extra words. Low nibble: the number of command bytes − 1 |
| Verb | A verb group, or 0 for an automatic action |
| Noun / chance | For an explicit action, a noun group (0 for any). For an automatic action, the chance in percent. Must be below 128 |
| Extra words | One byte each ([§8.2](#82-extra-words)) |
| Conditions | 16-bit values, up to one with bit 15 set ([§8.3](#83-conditions)) |
| Commands | The command bytes ([§8.5](#85-command-encoding)) |

An automatic action (verb 0) with noun 0 is a continuation, which runs only as part of a chain ([§9.2](#92-automatic-actions)).

### 8.2 Extra words

Each extra word byte is

| Bits | Meaning |
|------|---------|
| 7–6 | Type: 1 adverb, 2 participle, 3 preposition |
| 5–0 | The word group. 0 matches any word, and 1 matches only when the command has no word of this type |

A participle byte is followed by a second byte, the noun group of the second noun (0 for any).

An action matches only if every extra word matches the word of its type in the command. An action with no participle among its extra words does not match a command that has a participle.

When the command has no participle, an action that asks for one may take it from the words left on the line: a preposition and a noun, or (for participle 2, WITH) a bare noun. "GIVE KEY JOHN" then reads as "GIVE KEY WITH JOHN".

### 8.3 Conditions

A condition is a 16-bit value, big-endian on every system:

| Bits | Meaning |
|------|---------|
| 15 | Set on the last condition of the action |
| 14–5 | The argument, 0–1023 |
| 4–0 | The condition code |

A condition with code 0 holds an argument for the condition before it: a condition that needs two arguments takes its second argument from the next code-0 value. Ten bits are enough for the special values 997–999 of [§2.4](#24-operand-types).

| Code | Arguments | True when |
|------|-----------|-----------|
| 0 | *num* | (An argument of the previous condition) |
| 1 | *item* | The item is carried |
| 2 | *item* | The item is here |
| 3 | *item* | The item is carried or here |
| 4 | *room* | The player is in the room |
| 5 | *counter*, *num* | The room in the counter has the description command *num* |
| 8 | *flag* | The flag is set |
| 9 | *flag* | The flag is clear |
| 10 | *room* | Any item is in the room (0 means carried) |
| 13 | *item* | The item is in play: its location is valid and not 0 |
| 14 | *item* | The item is not in play |
| 15 | *num* | The current counter ≤ *num* |
| 16 | *num* | The current counter > *num* |
| 17 | *counter*, *counter* | The two counters are equal |
| 18 | *counter*, *counter* | The first counter is greater than the second |
| 19 | *num* | The current counter = *num* |
| 20 | *counter*, *num* | The counter = *num* |
| 21 | *counter*, *num* | The counter ≥ *num* |
| 22 | *counter* | The counter is 0 |
| 23 | *item*, *counter* | The item is in the room held in the counter |
| 26 | *item*, *room* | The item is in the room |
| 28 | *noun*, *noun* | The two nouns are the same group |
| 29 | *item*, *noun* | The item's word is the noun |
| 30 | *item*, *num* | The item's flags AND *num* is not 0 |
| 31 | *num* | A control condition ([§8.4](#84-control-conditions)) |

Codes 6, 7, 11, 12, 24, 25 and 27 have no handler in the MS-DOS *X-Men* interpreter, and are always true there and in this interpreter. Only *X-Men* uses one of them (code 27, once).

**Loader patch.** In *X-Men*, the action for the Aquarian has its first condition code 2 where code 21 is needed. This interpreter changes it when loading, recognising the action by its conditions.

### 8.4 Control conditions

Condition 31 does not test anything; it changes how the results of the following conditions combine:

| Argument | Meaning |
|----------|---------|
| 0 | AND: combine the next result by AND (the default) |
| 1 | OR: combine the next result by OR. Also turns on test at end |
| 2 | XOR: combine the next result by exclusive or |
| 3 | NOT the next condition |
| 4 | NOT all the following conditions |
| 5 | Turn off NOT |
| 6 | `(`: start a group. Up to 5 levels deep |
| 7 | `)`: end a group. Its result combines with the result before the group like a single condition, and NOT applies to it |
| 8 | Test at end ([below](#test-at-end)) |
| 9 | Fuzzy: compare nouns only up to a `#` |
| 10 | Strict: compare nouns exactly |

The first result, and the first result after `(`, replaces the running result instead of combining with it.

<a id="test-at-end"></a>**Test at end.** Normally the action fails as soon as the running result is false, without reading the remaining conditions. With test at end on, every condition is evaluated and only the final result counts. So "A, OR, B" works because OR turns on test at end before B is read, but "A, B, OR, C" stops at a false B. This is what the MS-DOS interpreters of *Spider-Man*, *The Fantastic Four* and *X-Men* do.

### 8.5 Command encoding

The command bytes of an action are a sequence of numbers. A command number and each of its operands is one number. A number is one byte, unless the byte has bit 7 set; then it is two bytes:

```
number = (first & 0x7F) × 256 + second
```

A command number 1–51 prints that message. A command number of 128 or more is read as one byte, not as the start of a two-byte number: it prints message *n* − 76 (messages 52 and up).

### 8.6 Commands

Operands are listed in the order they follow the command. Where a command combines two counters, the second operand is changed.

| Command | Operands | Effect |
|---------|----------|--------|
| 0 | | Nothing |
| 1–51 | | Print message 1–51 |
| 52 | *item* | `GET`: count the items carried, not counting item 0, into counter 33. If the count is at least counter 42, print "carrying too much" and stop the action at once, as a success. Otherwise the item is carried, and counter 33 is increased |
| 53 | *item* | `DROP`: the item is here |
| 54 | *room* | `GOTO`: the player goes to the room. Sets the draw flag |
| 55 | *verb*, *noun* | `MEANS`: replace the command by this verb and noun, with no other words and no "with" list, and search the action table again. Ends the action as done |
| 56 | | Dark: set flag 15 |
| 57 | | Light: clear flag 15 |
| 58 | *flag* | Set the flag |
| 59 | *item* | Destroy the item (location 0) |
| 60 | *flag* | Clear the flag |
| 61 | | Death: print "I'm dead", clear flag 15, go to the room in counter 35, look |
| 62 | *item*, *room* | Put the item in the room. The room may be a two-byte number |
| 63 | | Game over |
| 64 | *num*, *counter* | Set the counter to *num* |
| 65 | *counter*, *counter* | Multiply the second counter by the first |
| 66 | | Inventory. In *Claymorgue*, also shows the inventory picture |
| 67 | | Set flag 0 |
| 68 | | Clear flag 0 |
| 69 | | Done: stop searching after this action |
| 70 | | Clear the screen |
| 71 | | Save the game |
| 72 | *item*, *item* | Swap the locations of the two items |
| 73 | | Continue: run the continuation actions that follow ([§9.2](#92-automatic-actions)) |
| 74 | *item* | Take the item, with no limit |
| 75 | *item*, *item* | Put the first item where the second is |
| 76 | | Look |
| 77 | | Decrease the current counter by 1, with no floor |
| 78 | | Print the current counter |
| 79 | *num* | Set the current counter |
| 80 | *item*, *counter* | Set the counter to the item's location |
| 81 | *counter* | Swap the current counter with the counter |
| 82 | *num* | Add *num* to the current counter |
| 83 | *num* | Subtract *num* from the current counter |
| 84 | | Print the noun typed |
| 85 | *item* | Print a space and the item's text |
| 86 | *item* | Print the item's noun and a space |
| 87 | | Unused |
| 88 | | Wait, with an empty loop of 47,766 steps: about 0.9 s on a 4.77 MHz IBM PC, less on a faster one ([§8.8](#88-differences-between-the-original-engines)). This interpreter waits one second |
| 89 | | Redraw the room picture, and clear the draw flag |
| 90 | *num* | Show special picture *num*, set the draw flag, and wait for a key |
| 91 | *counter*, *counter*, *counter* | Divide: if the first counter is not 0, the third counter gets the second modulo the first, and the second counter gets the second divided by the first |
| 92 | | Stop the search after this action ([§9.3](#93-explicit-actions)) |
| 93 | *counter* | Wait, with an empty loop of 9,001 steps for each unit of the counter: about 0.16 s per unit on a 4.77 MHz IBM PC. This interpreter waits 250 ms per unit, and 125 ms in *Claymorgue*, which waits 50 units with nothing on screen when the lamp burns out |
| 94 | *counter*, *counter*, *counter* | Set the first counter to exit *c3* of room *c2*, where *c2* and *c3* are the values of the second and third counters |
| 95 | *counter*, *counter*, *counter* | Set exit *c2* of room *c1* to *c3* |
| 96 | | Unused |
| 97 | *counter*, *counter* | Swap the two counters. Sets the draw flag if the first is 32 and the room changes |
| 98 | *counter*, *counter* | Copy the first counter to the second |
| 99 | *num*, *counter* | Set the counter to *num* |
| 100 | *num*, *counter* | Add *num* to the counter |
| 101 | *num*, *counter* | Subtract *num* from the counter |
| 102 | *counter* | Print the counter |
| 103 | *counter* | Make the counter its absolute value |
| 104 | *counter* | Negate the counter |
| 105 | *counter*, *counter* | AND the first counter into the second |
| 106 | *counter*, *counter* | OR the first counter into the second |
| 107 | *counter*, *counter* | XOR the first counter into the second |
| 108 | *counter* | Set the counter to a random number 0–99 |
| 109 | *counter* | Set the counter to 0 |
| 110 | *num*, *num* | System command ([§8.7](#87-system-commands)) |
| 111 | | Start over: end this action as done, and scan the action table again from the first action |
| 112 | | Loop begin: remember this action, and run the continuations that follow, as command 73 |
| 113 | | Loop back to the last loop begin. After command 115, instead forget the loop and go on |
| 114 | | Forget the last loop |
| 115 | | Forget the last loop at the next command 113 |
| 116 | | Search on: search the action table again, and end this action as done |
| 117 | | Mark the command as matched, so that "I can't do that yet" is printed if nothing else happens |
| 118 | | Mark the command as handled |
| 119 | *msg* | Use the message as the name of the player character that starts the prompt. A number from 52 up is numbered like a command, so 76 is subtracted |
| 120 | | Copy the words of the command to counters 48–53 |
| 121 | | Copy counters 48–53 back to the words of the command, and search the action table again |
| 122 | *item* | Print "an" or "a" and the item's text |
| 123 | *counter*, *counter* | Add the first counter to the second |
| 124 | | Forget all loops |
| 125 | *flag*, *flag* | Copy the first flag to the second |
| 126 | *counter*, *counter* | Indirect read: set the first counter to the counter whose number is in the second |
| 127 | *counter*, *counter* | Indirect write: set the counter whose number is in the second counter to the first |
| 128–255 | | Print message *n* − 76 |

When an action's commands end, the action's result is the strongest that any command gave, in this order: game over, loop begin, continue, done, success.

### 8.7 System commands

Command 110 takes two numbers *a* and *b*, and does one of these, by *a*:

| *a* | Effect |
|-----|--------|
| 1 | Offer to load a saved game |
| 2 | Add the special picture in counter *b* to the animation store ([§13](#13-animation)) |
| 3 | Clear the animation store |
| 4 | Draw the stored picture whose index, counting from 0 in the order added, is in counter *b* |
| 5 | Draw special picture *b* |
| 6 | Beep, and wait for a key |
| 7 | Draw the item picture in counter *b* |
| 8 | Draw the room picture in counter *b* |
| 9 | Swap the flags of item *b* with counter 63 |
| 10 | Add the item picture in counter *b* to the animation store |
| 11 | Add the room picture in counter *b* to the animation store |
| 12 | Step to the next item ([below](#next-item)) |

<a id="next-item"></a>**Next item.** System command 12 lets a loop ([command 112](#86-commands)) visit items one by one, with the item in counter 30. Starting from the item already in counter 30, it puts the first item at the place that mode *b* chooses in counter 30:

| *b* | Place |
|-----|-------|
| 1 | The room in counter 63 |
| 2 | Here |
| 4 | Carried |

When there are no more items, counter 30 is set to 0 and the loop is left at its next loop back, as after command 115. Mode 3 (here or carried) looks only at the item in counter 30: if that item is here or carried, nothing happens; otherwise, unless it is the last item, the action table is scanned again from the first action.

This interpreter does not draw the pictures of system commands 4 and 5 at once, as the original does. It queues those of 4 as the frames of an animation, and keeps that of 5 as its background ([§13.2](#132-this-interpreter)).

### 8.8 Differences between the original engines

The original engines come in three families:

- **MS-DOS**: compiled BASIC, one program per game.
- **6502**: the Apple II, Commodore 64, Commodore Plus/4 and Atari 8-bit programs, builds of one source.
- **Atari ST**: the line-numbered source of the MS-DOS programs, in 68000 code.

The MS-DOS and 6502 programs of *Spider-Man* and *Buckaroo Banzai* are built from an *early* source, those of the games after them from a *later* one. Both Atari ST programs are of the later source. The commands and conditions that differ, in the programs that have been read:

| Number | Difference |
|--------|------------|
| Command 61 | In every 6502 build, it does not clear flag 15 (dark). It clears the two bytes 0x40 past the flag instead, which lie in the counters: the high byte of counter 7 and the low byte of counter 8. So the player can come back to life in the dark |
| Command 70 | Does nothing on the Apple II and the Atari 8-bit, in every build: it calls a routine that returns at once. It clears the screen on the C64, the Plus/4 and in MS-DOS |
| Command 86 | Does not work in any 6502 build: it starts with a call to the fixed address 0x292C, where none of these programs has a routine (on the Plus/4 it is the middle of the text of the direction words). It works on the Atari ST. No database uses it |
| Command 108 | 1 to 100 in the 6502 builds, 0 to 99 in MS-DOS |
| Command 120 | Copies the words in the early builds. Does nothing in the later ones, which copy the words to counters 48–53 elsewhere |
| Commands 105–107 | In the early Atari 8-bit builds all three exclusive-or, through a garbled test. No database uses them |
| Conditions 15, 16, 18, 21, command 65 | Unsigned in the early 6502 builds, signed in the later ones and on the Atari ST |
| Conditions 24, 25, 27 | Have code only on the Atari 8-bit, where 24 and 25 do not work; elsewhere they return at once. No Atari 8-bit database uses them |
| System command 12 | Only the MS-DOS programs of *X-Men* and *Claymorgue* have it |
| Commands 88, 93 | Every program waits with an empty loop, so the wait depends on the speed of the machine; none reads a clock. In the early MS-DOS programs, 88 counts 1,000 steps (about 0.02 s on a 4.77 MHz PC) and 93 counts the counter × 1,000 steps, a 16-bit product that overflows above 32. The early 6502 builds wait about 0.2–0.35 s for 88 and about 1.4 s per unit for 93 (0.8 s on the Atari 8-bit). The later 6502 builds wait about 0.2–0.5 s for 88 and 0.1–0.2 s per unit for 93. On the Atari ST, 88 counts 1,048,575 steps, about 3.5 s, and 93 about 0.03 s per unit |

This interpreter follows the later MS-DOS programs, except that it always copies the words in command 120, and that it times commands 88 and 93 by the clock.

---

## 9. Execution

### 9.1 The turn

A turn is:

1. The automatic actions ([§9.2](#92-automatic-actions)), unless the stop-time flag (63) is set. The flag is cleared either way.
2. The room is described ([§4.4](#44-the-room-display)), and its picture drawn if the draw flag (34) is set.
3. A line is read and parsed ([§9.4](#94-the-parser)), and the words are copied to counters 48–53.
4. The explicit actions ([§9.3](#93-explicit-actions)).
5. If no action matched, "I didn't completely understand your command." If one matched but none did anything, "I can't do that yet." The rest of the line is then thrown away.

Saving and the words this interpreter handles itself ([§9.4](#94-the-parser)) set the stop-time flag, so the next turn passes without automatic actions. This interpreter also skips them on the turn after a restore, and takes an undo snapshot after the automatic actions have run.

### 9.2 Automatic actions

The automatic actions are the actions with verb 0 at the start of the table. The scan stops at the first action with a verb.

An automatic action with noun *c* > 0 runs if a random number 0–99 is below *c* and its conditions hold. A continuation (noun 0) runs only as part of a chain: after an action whose commands included continue (73), the continuations that follow it run in turn, and the chain ends at the next action whose verb or noun is not 0.

### 9.3 Explicit actions

The command typed is held as six words: verb, noun, preposition, adverb, participle, second noun. Before the scan, the match flag (33) is cleared and counters 30 and 31 are set to the items that the noun and the second noun refer to. That is the first item, wherever it is, whose word is the noun and whose flags have bit 0 set; 0 if there is none.

The scan goes through the actions in order. An action **matches** when:

- its verb equals the verb typed,
- its noun is 0 or equals the noun typed, and
- its extra words match the words typed ([§8.2](#82-extra-words)).

During a chain, continuations (verb 0) also match.

When the always-match flag (60) is set, the first explicit action matches whatever is typed.

A matching action whose conditions fail sets the match flag, if its verb and noun are those typed (or its noun is 0). A matching action whose conditions hold runs its commands; if its result is anything but done, the match flag is cleared.

The scan stops after an action that ran, unless it is in a chain or has asked for the search to go on. It is also stopped by command 92. Commands 55, 116 and 121 start a new search; command 111 starts the scan over from the first action.

This interpreter stops a search after 1,000,000 actions, and keeps at most 32 loop points.

### 9.4 The parser

The typed line goes through these steps:

1. The characters `. , : ; ' " - !` become spaces.
2. Letters become upper case, and runs of spaces become one space.
3. The substitutions are applied ([§5.2](#52-substitutions)).
4. The line is split into words, and each word is looked up.

**Looking up a word.** The first word is tried as a verb first. Then, at any position, a word is tried as an adverb, a noun, a preposition, and (after the first word) a verb, in that order. `ANY` is noun group 0.

**Grammar.** A command is

```
[adverb] verb [noun | preposition noun] [adverb] [participle noun2]
```

A preposition with no noun after it becomes the participle. After the participle WITH (2) or AT (3), the second noun and any nouns that directly follow it form the **"with" list**, up to 20 nouns. Counter 46 holds their number, and `ARG_WITH_LIST` stands for the one that counter 44 points at.

**Homonyms.** When a noun is not found as typed, it is compared with the nouns of the items here or carried up to their `#`. If one item matches, it is used. If several match, the player is asked "When you say *x*, do you mean *y*?" about each in turn.

**Several commands on a line.** After one command has been carried out, the words left on the line are parsed as the next command. The verb carries over, so "GET LAMP, KEY" gets both.

**Words this interpreter adds.** This interpreter handles these words before the action table sees them:

| Words | Meaning |
|-------|---------|
| IT, HER, HIM, THEM | The last noun typed |
| WHERE, ? | "I'm only your puppet! You must figure things out for yourself!" |
| AGAIN, G, REPEAT, CONTINUE | Repeat the last command |
| UNDO, BOM (optionally followed by COMMAND, MOVE or TURN) | Undo the last turn |
| TRANSCRIPT, SCRIPT, PON (optionally followed by ON) | Transcript on |
| POFF, TRANSCRIPT OFF, SCRIPT OFF | Transcript off |
| RESTART (optionally followed by GAME, STORY or SESSION) | Restart the game, after asking |
| RAMSAVE, RAM SAVE | Save to one slot in memory |
| RAMLOAD, RAMRESTORE, RAM LOAD | Restore from that slot |

SAVE and LOAD are left to the game, whose actions save with command 71 and load with system command 1.

---

## 10. Database layouts

### 10.1 Header

The header has these fields, in this order, in the plain text layout:

| Field | Meaning |
|-------|---------|
| 0 | Items (highest index) |
| 1 | Action sum: the size of the action table in bytes |
| 2 | Nouns |
| 3 | Verbs |
| 4 | Rooms |
| 5 | Most items carried |
| 6 | Start room |
| 7 | Messages |
| 8 | Treasure room |
| 9 | Light time **(unused by this interpreter)** |
| 10 | Prepositions |
| 11 | Adverbs |
| 12 | Actions |
| 13 | Treasures **(unused by this interpreter)** |
| 14 | Substitutions |
| 15 | Unknown |
| 16 | Object pictures |
| 17 | The number of extra values after the object pictures, less one |

### 10.2 Plain text database

The MS-DOS database is a text file. Each value is on its own line, unless said otherwise. A quoted string is enclosed in double quotes, may span lines, and writes a double quote as `""`. This interpreter drops carriage returns from strings, replaces characters outside ASCII by `?`, and allows at most 1000 characters.

The file holds, in order:

1. The title, quoted, followed on the same line by the 18 header fields, each preceded by a comma:
   ```
   "FF #1 ",h0,h1,...,h17
   ```
2. The actions, field 12 + 1 of them. Each is a list of numbers:
   - the length byte ([§8.1](#81-action-layout)),
   - the verb and the noun or chance,
   - the extra words,
   - each condition as two numbers, its high and its low byte, up to the one with bit 15 set,
   - the command bytes, (low nibble of the length byte) + 1 of them.
3. The verbs, field 3 + 1 quoted strings.
4. The nouns, field 2 + 1 quoted strings.
5. The rooms, field 4 + 1 lines of eight numbers separated by commas: `n,s,e,w,u,d,desc,image`.
6. The messages, field 7 + 1 quoted strings.
7. The items, field 0 + 1 lines of the form `"text",location,word,flags`. A location of −1 means carried.
8. The adverbs, field 11 + 1 quoted strings.
9. The prepositions, field 10 + 1 quoted strings.
10. The substitutions, field 14 + 1 quoted strings.
11. The object pictures, field 16 + 1 lines of the form `room,object,image`.
12. Field 17 + 1 numbers of unknown purpose.
13. A quoted comment for each action, describing it.

Empty dictionary and substitution strings are skipped.

The pictures are in files next to the database ([§12.5](#125-ms-dos)).

### 10.3 Binary database

The binary database is an image of the data as the original engine has it in memory. It starts with two bytes, the load address on the 8-bit systems; this interpreter puts two bytes in front of an Atari ST database to match.

**Section table.** Offset 2 holds 24 16-bit pointers, one to the start of each section and the last to the end of the data:

| Entry | Section |
|-------|---------|
| 0 | Actions |
| 1 | Verbs |
| 2 | Nouns |
| 3–10 | Room columns 0–7 |
| 11 | Messages |
| 12 | Item texts |
| 13 | Item locations |
| 14 | Item words |
| 15 | Item flags |
| 16 | Adverbs |
| 17 | Prepositions |
| 18 | Substitutions |
| 19 | Title |
| 20 | Object picture rooms |
| 21 | Object picture items |
| 22 | Object picture images |
| 23 | End |

On the 8-bit systems the sections follow each other directly, and this interpreter reads them in order without the table. On the Atari ST every section starts at an even offset, with a pad byte after a section of odd length, so the table is needed there; its values are big-endian offsets from the start of the table.

**Header.** At offset 0x32 are 16 words (little-endian, big-endian on the Atari ST):

| Word | Meaning |
|------|---------|
| 0–11 | Header fields 0–11 ([§10.1](#101-header)) |
| 12 | Substitutions |
| 13 | Object pictures |
| 14 | Unknown |
| 15 | Header field 15 |

The binary header has no count of actions or treasures, and no extra values. The actions start at offset 0x50, which is two bytes before the end of the header: the last header word is also the start of the first action.

**Sections.** Each section holds:

| Section | Contents |
|---------|----------|
| Actions | As in [§10.2](#102-plain-text-database), one byte per number, with the conditions as big-endian 16-bit values. Actions are read until action sum bytes have been read; that gives the action count |
| Dictionaries, messages, item texts, substitutions, title | Strings, each a length byte and the characters. A length of 0 or 255 is an empty string |
| Room columns 0–5 | One byte per room |
| Room columns 6–7 | One word per room. On the Apple II, column 7 is two bytes, an address ([§11.4](#114-apple-ii)) |
| Item locations | One byte per item |
| Item words, item flags | One word per item |
| Object picture rooms, items | One byte per entry |
| Object picture images | One word per entry. On the Apple II, two bytes, an address |

Empty strings in a dictionary are padding, not entries. On the Atari ST, empty messages after message 0 are also padding. The first empty substitution string is skipped.

On the Apple II, the special picture table follows the object pictures ([§11.4](#114-apple-ii)).

**Header check.** This interpreter accepts a binary database only if it has 10–500 items, 50–190 nouns, 30–190 verbs and 10–100 rooms.

---

## 11. Containers

This section describes where the data is in the files that the games are distributed as today: loose files for MS-DOS, and disk images for the other systems. Except for the MS-DOS files, it describes this interpreter's way of finding the data in a disk image, and the offsets are those of the known images.

### 11.1 MS-DOS

The database is a text file ([§10.2](#102-plain-text-database)). The pictures are files named `R0nn.PAK`, `B0nn.PAK` and `S0nn.PAK` in the same directory ([§12.1](#121-picture-numbering)).

### 11.2 Commodore 64

The game is distributed as a D64 disk image. The database is the file `DATA`. Each picture is a file whose name is `R`, `B` or `S` followed by three digits.

**Patch.** In the C64 *Spider-Man*, item pictures B053 and B056 have byte 8 changed from 8 to 7 when loaded; they are drawn in the wrong place otherwise.

### 11.3 Atari 8-bit

The game is distributed as an ATR disk image, whose header starts `96 02 80 16 80 00`. The database is at file offset 0xFF8E, so that its header is at 0xFFC0. It may be on the companion disk; then this interpreter swaps the two disks.

**Picture table.** The boot disk holds a table of picture positions at offset 0x9593. It has 250 two-byte entries: 100 room pictures, then 100 item pictures, then 50 special pictures. Entry 0 is `00 00`. For an entry of bytes *lo*, *hi*:

```
sector = (hi & 3) << 8 | lo        counted from 1; 0 = no picture
offset = 16 + (sector − 1) × 128 + (hi >> 3) × 7
```

*offset* is the file offset of the picture's size word, which is 2 bytes into the picture. The picture is size + 4 bytes long, starting at *offset* − 2.

On the picture disk, the bytes 0xB390–0xB410 are not part of the picture data: a picture that spans them continues after them.

The *Spider-Man* title picture S000 is on the boot disk at 0x6D97. Without the picture disk, the game runs without pictures.

### 11.4 Apple II

The game is distributed as a 143,360-byte DSK disk image (this interpreter converts WOZ images first). This interpreter reverses the sector order of the whole image when loading, so the offsets here are of the reversed image.

The database is at offset 125,438. If the action sum at 0x1EA32 is not between 4000 and 7000, the database is on the other side, the companion disk.

**Picture addresses.** The database gives each picture as two bytes, a track and a sector, which make a memory address:

```
address = track × 0x1000 + sector × 0x100
offset  = 0x22F00 − address        (in the reversed image)
```

A picture starts with its size word. The size of an item picture does not include the size word itself, so 2 is added.

**Special picture table.** After the object picture images comes a table of special pictures: a track and a sector for each special picture from 0, up to the end of the sector the database ends in.

**Naming.** The Apple II database gives room and item pictures by address, not by number. This interpreter names a room picture R0*nn* after the first room *nn* that uses it. Item pictures are numbered by their index in the object picture table plus 1 in *The Fantastic Four*, and by the item number in *Spider-Man*. *Buckaroo Banzai* has no item pictures on the Apple II.

**Companion disks.** *Buckaroo Banzai* has two disk sides, `177a - Buckaroo Banzai - Side 1.dsk` and `177b - Buckaroo Banzai - Side 2.dsk`. This interpreter looks for the other side by that name.

### 11.5 Atari ST

The game is distributed as a disk image, either raw (`.st`) or in MSA format (starting `0E 0F`, with run-length escape byte 0xE5), of 213,610 to 368,640 bytes. The disk is FAT12, with subdirectories. The database is the file `DATABASE`. The pictures are files named `R`, `B` or `S` and three digits, up to 100 of each.

---

## 12. Pictures

### 12.1 Picture numbering

There are three kinds of picture, each numbered from 0:

| Kind | Letter | Use |
|------|--------|-----|
| Room | R | The picture of a room (room column 7) |
| Item | B | The picture of an item in a room (the object picture table) |
| Special | S | Pictures shown by command 90, the title (S000), backgrounds and animation frames |

A picture's name is its letter, a `0` and two digits: `R005`, `B012`, `S000`. The number is the last two digits. Number 255 means no picture.

### 12.2 What is drawn

When the room is shown, and graphics are on (flag 35):

1. The room's picture is drawn.
2. Then, for each object picture entry whose room is here and whose item is here, the item picture is drawn on top, in table order.

A room with picture 255 has no picture. A dark room is drawn black, except that *Claymorgue* draws its picture R000 for darkness, and the Atari ST versions draw the empty backdrop S999 ([§12.6](#126-atari-st)).

Game-specific rules:

- *Spider-Man*: when item 0 is here, the room picture is drawn upside down.
- *Claymorgue*: room 33 is the inventory screen.
- The title picture S000 is shown at the start.

### 12.3 Commodore 64 and Atari 8-bit

The C64 and Atari 8-bit pictures have the same format. All coordinates are in the 160-pixel-wide multicolour screen, where a byte column is 4 pixels wide.

| Offset | Size | Meaning |
|--------|------|---------|
| 0 | 2 | Unused |
| 2 | 2 | Size of the picture data, LE16 |
| 4 | 1 | Left, in byte columns, plus 3 |
| 5 | 1 | Top, in pixel rows |
| 6 | 1 | Right, in byte columns |
| 7 | 1 | Bottom, in pixel rows |
| 8 | 4 | Colours |
| 12 | | Picture data |

**Colours.** Each pixel is 2 bits. Pixel value 0 is black; values 1–3 take the colours from colour bytes 1–3. Colour byte 0 is not used. The colour bytes are Atari colour register values; the C64 engine maps them to its own palette through a table **(this interpreter's reading; the C64 table is in `c64a8draw.c`)**.

**Compression.** The data is run-length encoded in pairs of bytes. Each run starts with a count byte *c*:

- If bit 7 is set, the next two bytes are repeated (*c* & 0x7F) + 1 times.
- Otherwise, the next *c* + 1 pairs of bytes are copied.

**Order.** Each pair of bytes is two pixel rows of one byte column. Pairs fill a byte column from top to bottom, then the next byte column to the right. Each byte holds four pixels, two bits each, most significant first.

### 12.4 Apple II

The Apple II pictures use the same header as [§12.3](#123-commodore-64-and-atari-8-bit) after the size word, but no colour bytes:

| Offset | Size | Meaning |
|--------|------|---------|
| 0 | 2 | Size of the picture data, LE16 (§11.4) |
| 2 | 1 | Left, in byte columns |
| 3 | 1 | Top |
| 4 | 1 | Right, in byte columns |
| 5 | 1 | Bottom |
| 6 | | Picture data |

The data is compressed as in [§12.3](#123-commodore-64-and-atari-8-bit). The bytes are written straight into the high-resolution screen, two rows per pair, column by column from top to bottom. The address of byte column *x* of row *y* is

```
((y / 8) & 7) << 7  +  ((y / 8) & 0x18) × 5  +  (y & 7) << 10  +  x
```

The colours come from the Apple II's colour artefacts of the bit patterns. A room or special picture with top 0 sets the picture window to (bottom + 2) × 2 rows by (right + 1) × 14 pixels **(this interpreter)**.

### 12.5 MS-DOS

An MS-DOS `.PAK` picture is drawn in CGA's four-colour mode, with the palette black, cyan, magenta, white:

| Offset | Size | Meaning |
|--------|------|---------|
| 0x00 | 1 | 0xFD |
| 0x01 | 4 | Segment and offset, LE16 each; not used |
| 0x05 | 2 | Size, LE16 |
| 0x0D | 1 | 0xFF: double-width pixels |
| 0x0F | 2 | Start: the screen offset of the top left byte, LE16 |
| 0x11 | 2 | End: the screen offset where the picture ends, LE16 |
| 0x13 | 1 | Width in bytes; × 4 is the width in pixels |
| 0x17 | | Picture data |

The file is the memory image that BASIC's `BSAVE` writes: a header of 0xFD, segment, offset and size, then the data. The original `BLOAD`s it into an array of words.

The position is derived from the CGA screen offset of 80 bytes a line:

```
left = (start mod 80) × 4 − 24
top  = start / 40, rounded to even
rows = (end − start) / 80
```

**Compression.** Each run starts with a count byte *c*:

- If bit 7 is set, the next byte is repeated (*c* & 0x7F) + 1 times.
- Otherwise, the next *c* + 1 bytes are copied.

**Order.** Each byte holds four pixels, two bits each. Rows are interlaced as in CGA memory: first the even rows from *top* down, two rows apart, then the odd rows from *top* + 1.

### 12.6 Atari ST

An Atari ST picture is drawn in the 320 × 200 sixteen-colour mode:

| Offset | Size | Meaning |
|--------|------|---------|
| 0 | 2 | The size of the file in bytes, BE16 |
| 2 | 2 | Size of the picture data, BE16 |
| 4 | 2 | Start: a screen offset in words, BE16; × 2 is the byte offset in a 160-byte line |
| 6 | 1 | Width in bytes, each 4 pixels. Over 70 means the full width, and the window is cleared first |
| 7 | 1 | The number of colour cycling records |
| 8 | 32 | 16 palette entries, BE16, each `0x0RGB` |
| 40 | | Colour cycling records, then the picture data |

**Colour cycling.** Each record is:

| Size | Meaning |
|------|---------|
| 1 | High nibble: the slot, 0–13; palette index slot + 1 cycles. Low nibble: the number *n* of colours |
| 1 | The delay before the first step, less one |
| 2 | The rate, BE16: the time between steps, less one |
| 2 × *n* | The colours to cycle through, `0x0RGB` |

The original counts the delay and the rate in vertical blanks, in an interrupt routine that sets the colour register at each step. The colours wrap around after the *n*th. In all known pictures the delay is 0. This interpreter counts both in ticks of 25 ms, and uses the delay as the step to start from.

**Compression.** As MS-DOS ([§12.5](#125-ms-dos)): a count *c* below 0x80 copies *c* + 1 bytes; otherwise the next byte is repeated (*c* & 0x7F) + 1 times.

**Order.** Each data byte is two pixels, each two screen pixels wide: the high nibble first, then the low. A pixel's palette index is its nibble with the bits reversed: bit 4 of the byte is bit 3 of the first pixel's index, and so on. The *x* position of the byte at screen address *a* is 2 × (*a* mod 160) + 6 × (*a* & 1); after each pair of bytes the address advances by 1 + 6 × (*a* & 1).

**Rules.** Each picture sets all 16 colour registers, and replaces the colour cycling of the picture before it, even where the old picture is still visible. So that what is already on screen goes on cycling, an item picture repeats the cycling records of the room it is drawn in, and of the other items there. In *Fantastic Four*, items 11 and 14 repeat those of rooms 11 and 9, and items 3, 5, 13, 21 and 22 share those of room 4.

This interpreter cycles by repainting the pixels of each cycling colour, not by setting a colour register. When a new picture is drawn, it moves the pixels of the old picture's cycling colours that the new picture leaves uncovered to the new picture's colour with the same palette index; those with no such colour stop cycling. There is no cycling in the dark, except in *Claymorgue*, where the darkness picture R000 ([§12.2](#122-what-is-drawn)) cycles like any other picture.

The original unpacks S999 once, at the start, and restores the screen from that copy before a small room picture; S999's palette and colour cycling are not loaded again. A small room picture is drawn over the backdrop S999, except in *Claymorgue*, where the rooms 0, 5, 11, 16, 18 and 31 are small but full. In *Spider-Man*, the special pictures 6, 7, 8, 12 and 18 have a white background.

---

## 13. Animation

Animation is driven by the system commands of [§8.7](#87-system-commands): the game clears the animation store (3), adds pictures to it (2, 10, 11), and then shows them (4), each time naming one of those pictures by its index. A picture may be shown several times.

### 13.1 The original engine

This is how the MS-DOS and Atari ST programs of *The Fantastic Four* do it.

- The store is an array of about 12 KB (MS-DOS) or 24 KB (Atari ST), with a table of where each picture starts: 21 entries on MS-DOS, 20 on the Atari ST.
- Command 3 empties the store.
- Commands 2, 10 and 11 load the picture and append its packed data, as read from the file, to the store. Nothing is drawn. Commands 10 and 11 run the code of 7 and 8, which tell them apart from 7 and 8 by *a* ≥ 10.
- Command 4 copies the stored picture back to the picture buffer and draws it at once. There is no timer: nothing happens after the last picture, and the room is not redrawn.
- Command 5 loads a special picture and draws it at once.
- The pace comes from the game: it waits between pictures with commands 88 and 93, and for a key with command 110 6.
- Neither program has system command 12.

For example, the cannon in *The Fantastic Four*: one action draws special picture 7 (110 5) and sets counters to add the specials 8–17 to the store in a loop. Another beeps and waits for a key (110 6). Then a loop shows the stored pictures 0–9 (110 4), with a short delay (93 5) after each. A last action clears the store, adds the specials 18–20 and shows them in the same way.

### 13.2 This interpreter

This interpreter plays an animation on a timer instead:

- An animation holds up to 31 pictures and 31 frames. Command 4 queues a frame.
- The first frame starts the animation. A frame lasts 200 ms. The last frame is held for 2 seconds, and then the room is redrawn.
- If a background has been set (system command 5), it is drawn first.
- The delays of the game (commands 88 and 93) are skipped while an animation plays.

Game-specific rules:

| Game | Rule |
|------|------|
| *Spider-Man* | Room 5 redraws item 23 after an animation |
| *Spider-Man*, Atari ST | The web: special pictures 14–18, ending on 18 with its colour cycling |
| *Buckaroo Banzai* | Room 29 with the blackout flag (6) set stays black |
| *The Fantastic Four* | The cannon in room 5: 300 ms a frame (250 ms in the original), played in two halves with a pause of 2 seconds between, where the original loads the pictures of the second half |

---

## 14. Saved games

### 14.1 What is saved

Every original engine saves the same state:

- the 64 flags and the 64 counters,
- the protagonist, the message set by [command 119](#86-commands),
- the location and the flags of every item (system command 9 changes item flags, [§8.7](#87-system-commands)),
- room columns 0–6 of every room: the six exits, which commands 94 and 95 change, and the description.

No original saves the picture on screen, the animation, or anything that identifies the game or the version. The counts of items and rooms are not saved either: they come from the database in memory. A saved game of one game restores into another without complaint, and is read as that game's state.

The Apple II, Atari 8-bit and Atari ST engines save the whole database image, which holds the item and room state; the MS-DOS and Commodore engines write only the parts that change. A saved game always goes to one of several **areas**, named by a letter. After a restore, the room is shown again.

In the layouts below, *I* is the header's item count and *R* its room count ([§10.1](#101-header)), so there are *I* + 1 items and *R* + 1 rooms ([§2.2](#22-counts-are-highest-indices)).

### 14.2 MS-DOS

**Saving.** Command 71 prints "Saving game.", then asks `SAVE GAME AREA (A-D)` until a letter A–D is typed, and echoes it in upper case. *Spider-Man* does not ask: it always uses area A. The file is `SAVEG`*x*`.DAT`, where *x* is the area letter, in the game's directory.

**Restoring.** System command 1 asks `Want to use your old saved game ?` (*Claymorgue* leaves out the `?`). On Y, it asks `Which save game area (A-D)`; on N, the game goes on. If the file is missing, *Claymorgue* and *The Fantastic Four* print `Save area not found!`, while the *Spider-Man* interpreter stops with the BASIC error `File not found in line 760`. Nothing in the file is checked.

**Format.** The file is text, one number per line, as BASIC's `PRINT #` writes it: a space or a minus sign, the digits, a space, and CR LF (`" 24 \r\n"`, `"-1 \r\n"`). After the last number comes one 0x1A byte, then filler up to a multiple of 128 bytes, left over from the file buffer. A reader stops at the 0x1A.

| Numbers | Content |
|---------|---------|
| 1 | The protagonist |
| 2 × (*I* + 1) | For each item, its location (−1 for carried, [§3.3](#33-locations)) and its flags |
| 2 × 64 | For each *n* from 0 to 63, flag *n* (0 or −1) and counter *n* |
| 7 × (*R* + 1) | For each room, its exits n, s, e, w, u, d and its description, as in the database ([§10.2](#102-plain-text-database)). The picture column is not saved |

So *Spider-Man* writes 562 numbers, *Claymorgue* 529 and *The Fantastic Four* 472. The saved games of all five MS-DOS games have this layout.

### 14.3 Commodore 64 and Plus/4

**Saving.** Command 71 first makes sure that the program disk is not in the drive: if a file named `$A0` (one shifted space) loads, it prints `Mount save game disk!`, waits for a key and tries again. It then asks `Save area (A-Z or FORMAT)?`. `FORMAT` prints `Formatting!` and formats the disk with the drive command `N0:S,SG`. A letter names the file: the letter with bit 7 set, `.`, and 0xD3 (a shifted S), so area A is `$C1 $2E $D3`. The old file is scratched, and the new one is saved as a PRG file on device 8. On a disk error, it prints `DISK ERROR` and asks again. At the end it asks to `Mount program disk!`.

**Restoring.** System command 1 asks `Shall I restore your saved game?` until Y or N is typed. On Y, it checks the disk and asks for the area as above, and loads the file to the address stored in it. If the load fails, it prints `BAD SAVE FILE` and asks again. Nothing else is checked.

**Format.** The file is a PRG file with the load address 0x0800 (0x1000 on the Plus/4), followed by:

| Size | Content |
|------|---------|
| 8 × (*R* + 1) | Room columns 0–6 as in the database ([§10.3](#103-binary-database)): six columns of one byte per room, then the description column of one LE16 per room |
| 3 × (*I* + 1) | For each item, its flags (LE16) and its location (one byte) |
| 64 | Flags 0–63, one byte each, 0x00 or 0xFF |
| 128 | Counters 0–63, LE16 |
| 2 | The protagonist, LE16 |

*Buckaroo Banzai* (35 rooms, 60 items) gives 665 bytes after the load address, *Spider-Man* 741 and *The Fantastic Four* 621. All the C64 interpreters and the Plus/4 one (`SAGA.TED`) write this format.

### 14.4 Atari 8-bit

The Atari 8-bit engine has no file system. It writes sectors of a separate save disk directly, with the 128-byte sectors counted from 1.

**Saving.** Command 71 asks `Put save disk & hit key.`. It refuses the boot disk and the picture disk, then asks `Format disk (Y/N)?`, where only a lower-case `y` (0x79) means yes, then `Type game save area (A-D)`. On a disk error it prints `ERROR!`. At the end it asks `Put SAGA+ disk side 2 & hit key.`.

**Restoring.** System command 1 asks `Want to restore a` / `previously saved game?`, then for the disk and the area as above.

**Format.** The save consists of the database image, 0x1000–0x4FFF in memory (64 pages, starting with the section table, [§10.3](#103-binary-database)), and a state block:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 64 | Flags 0–63, one byte each, 0x00 or 0xFF |
| 64 | 128 | Counters 0–63, LE16 |
| 192 | 2 | The protagonist, LE16 |
| 194 | 2 | A scratch word: the room whose items were last listed. Not in the first *Buckaroo Banzai* release |

Area A starts at sector 8, B at 160, C at 320 and D at 480. Page *k* of the image is written to sectors *N* + 2*k* and *N* + 2*k* + 1, where *N* is the area's first sector, and the state block to sector *N* + 128 and the start of *N* + 129. Sector 360, the DOS directory, is skipped and the rest of the area moves up one sector. Sector 2 is a directory: formatting fills it with 0x33, saving sets byte 2 × *area* (A = 0) to 0, and restoring an area whose byte is not 0 prints `Bad file.`.

The first release of *Buckaroo Banzai* differs: it asks `Put disk & hit any key`, `Init disk (Y/N)?` and `Type game area (A-C):`, prints `I/O error`, has only the areas A–C, writes the 194-byte state block, and keeps no directory.

### 14.5 Apple II

The Apple II engine has no file system either. It writes logical DOS 3.3 sectors of a separate data disk, with its own copy of RWTS.

**Saving.** Command 71 asks for the data disk. It refuses the program disk's second side, which it recognises by the bytes 0x00, 0x01, … 0xFF in track 0, sector 1. *Spider-Man* then offers a menu whose first choice formats the disk, without a catalog. Then it asks for the area. At the end it asks for the program disk. The messages are:

| *Spider-Man* | *The Fantastic Four*, *Buckaroo Banzai* |
|--------------|------------------------------------------|
| `ENTER YOUR 'DATA DISK' AND TYPE 'Y'` | `PUT 'DATA DISK' & TYPE 'Y'` |
| `(1) INIT YOUR 'SAVE GAME' DISK (2) SAVE GAME (3) EXIT ENTER:` | |
| `Enter 'game save' area (A-D) :` | `Type 'GAME SAVE' area (A-D) :` |
| `READ/WRITE ERROR !!!` | `I/O ERROR !` |
| `Insert program disk (side #02) into drive #01 and 'RETURN'` | `Put SAGA+ disk (side#2) & 'RETURN'` |
| `Do you want to restore a previously saved game? (Yes,No)` | `Want to restore a previously saved game? (Yes,No)` |

**Restoring.** System command 1 asks the last question of the table, then for the disk and the area.

**Format.** The save consists of the database image, 0x4000–0x7FFF in memory (64 pages), and a state block laid out as on the Atari 8-bit: 194 bytes in *Spider-Man*, 196 in *The Fantastic Four* and *Buckaroo Banzai*. For an area whose first track is *T*, page *k* of the image goes to track *T* − ⌊*k* / 16⌋, sector 15 − (*k* mod 16), so the image fills four tracks downwards, and the state block goes to track *T* − 4, sector 15.

| Area | First track, *Spider-Man* | First track, the others |
|------|---------------------------|-------------------------|
| A | 5 | 6 |
| B | 15 | 15 |
| C | 22 | 22 |
| D | 32 | 32 |

*The Fantastic Four* and *Buckaroo Banzai* keep a directory in track 0, sector 1: saving sets byte *area* (A = 0) to 0x33, and restoring an area without it asks for the area again. On a disk formatted by DOS 3.3, that sector holds boot code.

### 14.6 Atari ST

**Saving.** Command 71 prints `Now saving game.`. If the file `WPROTECT` exists, the program disk is in the drive: it prints `Mount save game disk!` and waits for Return until it is gone. It then asks `Save area A-Z` and `? ` until one letter A–Z is typed. The file is named by the letter alone, without an extension. *The Fantastic Four* writes it in the root directory of the current drive; the known *Spider-Man* and *Claymorgue* disks, whose programs are identical and patched, write it to drive A:. If the file cannot be created, it prints `File open error.` and `DISK ERROR`, and a short write prints `DISK ERROR`. At the end it waits with `Mount program disk!` until `WPROTECT` can be found again. Only *The Fantastic Four* disk has a `WPROTECT` file; the others save to the program disk.

**Restoring.** System command 1 asks `Shall I restore your saved game?` until Y or N is typed. On Y, it asks for the area as above and reads the file straight into memory. If the file is missing, or is not exactly 16,834 bytes long, it prints `BAD SAVE FILE.` and asks again; a short file has already overwritten part of the state by then.

**Format.** The file is 16,834 (0x41C2) bytes, big-endian:

| Offset | Size | Content |
|--------|------|---------|
| 0x0000 | 0x4100 | The `DATABASE` file as loaded, starting with the section table, padded with zeros |
| 0x4100 | 64 | Flags 0–63, one byte each, 0x00 or 0xFF |
| 0x4140 | 128 | Counters 0–63, BE16 |
| 0x41C0 | 2 | The protagonist, BE16 |

### 14.7 This interpreter

This interpreter writes a text file of its own:

1. 64 lines, each a counter.
2. One line: `flags protagonist 0 imgtype imgindex`, where *flags* is all 64 flags as one 64-bit decimal number (flag *n* is bit *n*), and *imgtype* and *imgindex* are the picture last drawn.
3. One line per item, items 0 to the last, each its location.
4. One line per item, items 0 to the last, each its flags as an unsigned number.
5. One line per room, rooms 0 to the last, each its columns 0–6 (the six exits and the description) separated by spaces.

Older versions of this interpreter wrote only parts 1–3. Such a file still loads, and leaves the item flags and the room exits as they were. Undo states, the RAMSAVE slot and restarting keep the same state as the file, picture aside. Like the originals, it does not save the picture column. It cannot read the original formats.

Saving sets the stop-time flag. This interpreter keeps 100 undo states, and one RAMSAVE slot.

---

## Appendix A. Command summary

| | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| **0** | nop | msg | msg | msg | msg | msg | msg | msg | msg | msg |
| **10–49** | msg | | | | | | | | | |
| **50** | msg | msg | get | drop | goto | means | dark | light | set flag | destroy |
| **60** | clear flag | death | put in room | game over | set counter | multiply | inventory | set flag 0 | clear flag 0 | done |
| **70** | cls | save | swap items | continue | take | put with | look | dec current | print current | set current |
| **80** | counter = location | swap current | add current | sub current | print noun | print item | print item noun | — | pause | redraw |
| **90** | special picture | divide | stop search | delay | get exit | set exit | — | swap counters | copy counter | set counter |
| **100** | add | subtract | print counter | abs | neg | and | or | xor | random | clear counter |
| **110** | system | start over | loop | loop back | pop loop | pop at loop back | search on | matched | handled | prompt |
| **120** | words → counters | counters → words | a/an item | add counters | reset loops | copy flag | indirect read | indirect write | | |

## Appendix B. Condition summary

| Code | Condition | Code | Condition |
|------|-----------|------|-----------|
| 0 | argument | 16 | current > *n* |
| 1 | carried | 17 | counter = counter |
| 2 | here | 18 | counter > counter |
| 3 | present | 19 | current = *n* |
| 4 | in room | 20 | counter = *n* |
| 5 | room description | 21 | counter ≥ *n* |
| 6 | (always true) | 22 | counter = 0 |
| 7 | (always true) | 23 | item in counter's room |
| 8 | flag set | 24 | (always true) |
| 9 | flag clear | 25 | (always true) |
| 10 | room has items | 26 | item in room |
| 11 | (always true) | 27 | (always true) |
| 12 | (always true) | 28 | noun = noun |
| 13 | in play | 29 | item's noun = noun |
| 14 | not in play | 30 | item flags AND *n* |
| 15 | current ≤ *n* | 31 | control |
