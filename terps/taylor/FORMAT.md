# TaylorMade Game Data Unofficial Specification

This document describes the game data of the later Adventure International UK / Adventure Soft UK adventures, the ones run by the TaylorMade interpreter in this directory: *Questprobe featuring The Human Torch and The Thing*, *Rebel Planet*, *Blizzard Pass*, *Masters of the Universe: The Super Adventure* (a.k.a. *Terraquake*), *Temple of Terror* and *Kayleth*. There is no official specification; the format is defined by the original Z80 and 6502 engines.

The design is superficially similar to that of the earlier Adventure International UK (Scott Adams engine) games. The tile-based graphics, font, and split-screen layout look familiar. They hide a number of inventions that squeeze more into the 48 kilobyte memory of the ZX Spectrum: an efficient token-based text compressor and room pictures drawn with a picture script, an expansion of the system first seen in *Seas of Blood*, combining and mirroring several smaller images.

This document started as Alan Cox's working notes. It has been brought in line with what this interpreter does; where the two disagreed, the interpreter (which is tested against the original games) wins. Statements the interpreter does not depend on are marked **(unverified)**. Memory addresses were checked in 48K and 128K snapshots of the Spectrum releases.

The document describes the games and their original engines. Where it describes what this interpreter does instead, it says so ([§2.4](#24-the-original-engine-and-this-interpreter)).

## Contents

- [1. Scope](#1-scope)
  - [1.1 Engine variants](#11-engine-variants)
  - [1.2 Game files](#12-game-files)
  - [1.3 Locating the tables](#13-locating-the-tables)
- [2. General conventions](#2-general-conventions)
  - [2.1 Notation](#21-notation)
  - [2.2 Implicit indexing](#22-implicit-indexing)
  - [2.3 Operand types](#23-operand-types)
  - [2.4 The original engine and this interpreter](#24-the-original-engine-and-this-interpreter)
- [3. Game state](#3-game-state)
  - [3.1 Flags](#31-flags)
  - [3.2 Object locations](#32-object-locations)
  - [3.3 Initial state](#33-initial-state)
- [4. Text](#4-text)
  - [4.1 Token table](#41-token-table)
  - [4.2 Strings](#42-strings)
  - [4.3 Rebel Planet strings](#43-rebel-planet-strings)
  - [4.4 Capitalisation and spacing](#44-capitalisation-and-spacing)
- [5. Messages](#5-messages)
  - [5.1 System messages](#51-system-messages)
  - [5.2 Second message table](#52-second-message-table)
- [6. Rooms](#6-rooms)
- [7. Objects](#7-objects)
  - [7.1 Low and ordinary objects](#71-low-and-ordinary-objects)
- [8. Dictionary](#8-dictionary)
  - [8.1 Assigning words to slots](#81-assigning-words-to-slots)
- [9. Exits](#9-exits)
- [10. Engine tables](#10-engine-tables)
  - [10.1 Table layout](#101-table-layout)
  - [10.2 Opcode byte](#102-opcode-byte)
  - [10.3 Execution](#103-execution)
  - [10.4 Conditions](#104-conditions)
  - [10.5 Actions](#105-actions)
- [11. Pictures](#11-pictures)
  - [11.1 Picture model](#111-picture-model)
  - [11.2 Tiles](#112-tiles)
  - [11.3 Block pictures](#113-block-pictures)
  - [11.4 Picture scripts](#114-picture-scripts)
  - [11.5 Kayleth animations](#115-kayleth-animations)
  - [11.6 Rebel Planet animations](#116-rebel-planet-animations)
- [12. Questprobe 3](#12-questprobe-3)
  - [12.1 Game state](#121-game-state)
  - [12.2 Text](#122-text)
  - [12.3 Engine tables](#123-engine-tables)
  - [12.4 Pictures](#124-pictures)
- [13. Saved games](#13-saved-games)
  - [13.1 What is saved](#131-what-is-saved)
  - [13.2 Save file](#132-save-file)
  - [13.3 Undo and memory saves](#133-undo-and-memory-saves)
  - [13.4 Original Spectrum releases](#134-original-spectrum-releases)
  - [13.5 Original Commodore 64 releases](#135-original-commodore-64-releases)
- [Appendix A. The BBC Micro source](#appendix-a-the-bbc-micro-source)
- [Appendix B. Comparison with the Quill](#appendix-b-comparison-with-the-quill)
  - [B.1 Blizzard Pass in both systems](#b1-blizzard-pass-in-both-systems)

---

## 1. Scope

### 1.1 Engine variants

This interpreter tells four variants of the original engine apart (`GameVersion` in `taylor.h`). Each game has a separate entry in its registry (`gameinfo.c`) for every release (Spectrum, Commodore 64, and the text-only and hybrid editions of *Temple of Terror*), but all releases of a game share a variant.

| Variant | Games | Distinguishing features |
|---------|-------|-------------------------|
| Questprobe 3 | *Questprobe featuring The Human Torch and The Thing* | The oldest. Its own opcode numbering and text encoding, and the picture format of the earlier Adventure International UK games, drawn by a `PICTURE` action. See [§12](#12-questprobe-3). |
| Rebel Planet | *Rebel Planet* | String end markers live inside the token table ([§4.3](#43-rebel-planet-strings)). |
| Blizzard Pass | *Blizzard Pass* | Spectrum 128 only. Action 12 is `GOBY`. |
| Terraquake | *Terraquake*, *Temple of Terror*, *Kayleth* | Second message table, block pictures compressed with a pattern table (Spectrum only), the newer picture-script operands, actions 31–35. |

Everything outside [§12](#12-questprobe-3) describes the three later variants unless it says otherwise. Where the Rebel Planet and Blizzard Pass variants differ together from the Terraquake variant, they are called the **older** variants.

### 1.2 Game files

The games exist only as memory images of the original machines (Spectrum snapshots, Commodore 64 images), not as files written by an authoring system, and that is what this interpreter reads. There is **no header**: nothing in the image says where a table is or how long it is. The original engine has the addresses built into its code; this interpreter keeps the table offsets of every supported release in its registry, `gameinfo.c`.

A 48K Spectrum snapshot holds the memory from `4000` up, so a table at memory address *A* of a Spectrum game is at file offset *A* − `4000`. The registry stores offsets in this form, and this interpreter adds a per-file baseline to them when a dump is laid out differently.

*Blizzard Pass* is the only game for the Spectrum 128. Its image holds the eight 16 KB RAM pages in the order 5, 2, 0, 1, 3, 4, 6, 7, so page 6 starts at offset `6 × 4000`. The game contains two room description tables: the short descriptions of the graphical game, and the long descriptions of the text-only game from "the other side of the tape", both placed in one setup with the long ones in the higher banks.

Memory addresses of the *Blizzard Pass* tables:

| Table | Address | Notes |
|-------|---------|-------|
| Room text | `75CE` | Short descriptions, of the graphical game |
| Messages | `7E26` | |
| Object text | `8CE6` | |
| Status table | `96D9` | |
| Event table | `9A54` | |
| Words | `A720` | |
| Room exits | `AB67` | |
| Tokens | `B1BC` | |
| Object starts | `B6A8` | Initial object locations |
| Pictures | `B798` | Picture scripts, one per room |
| Room text | `C000` in RAM page 6 | Long descriptions, of the text game |

### 1.3 Locating the tables

This interpreter finds some tables by searching for known bytes rather than taking them from the registry, so that different dumps of the same game work:

| Table | Search |
|-------|--------|
| Dictionary | The bytes `NORT 01 N`: the first entry, `NORT` with value 1, followed by the first letter of the next entry, `N`. |
| Initial flags | The Z80 block copy `01 06 00 ED B0 C9 00 FD`; the flags start 5 bytes in. In Questprobe 3, `E7 97 51 95 5B 7E 5D 7E 76 93`; the flags start 11 bytes in. Otherwise the registry offset. |
| Token table | A per-game signature, if the registry has one. Otherwise the Z80 code `47 B7 28 0B 2B 23 CB 7E`, which is preceded by the table's address (little-endian); a match is accepted when 300 or more of the 512 bytes at that address are lowercase letters (ignoring bit 7). Otherwise, in Questprobe 3, 6 bytes past the dictionary end `XXXX FF`. As a last resort, one byte before the text `You are in `. Otherwise the registry offset. |
| Tiles | The 24 bytes `00`×8 `80`×8 `40`×8 (the first three tiles). Otherwise the registry offset. |
| Kayleth animations | `FF 00 00 00 0F 00 5D 0F 00 61` or `FF 00 00 15 0F 00 5D 0F 00 61`. |

This interpreter takes all other tables (rooms, objects, messages, engine tables, exits, object locations, block pictures, picture scripts) from the registry.

---

## 2. General conventions

### 2.1 Notation

Byte values and addresses are hexadecimal (`7E`, `0x7E`). Opcode and table numbers are decimal unless written in hex. Multi-byte numbers are little-endian. "Bit 7" is the top bit of a byte.

### 2.2 Implicit indexing

No table of strings or entries has an index of pointers. String *n* of a table is found by skipping the *n* strings before it ([§4.2](#42-strings)), and the exits and picture script of a room are found by scanning. Tables are numbered from 0.

### 2.3 Operand types

The opcode tables use these letters for operand bytes:

| Letter | Meaning |
|--------|---------|
| `L` | Location (room number, or a special location of [§3.2](#32-object-locations)) |
| `O` | Object number |
| `F` | Flag number |
| `N` | Number (0–255) |
| `W` | Word value from the dictionary |
| `V` | Verb value |
| `M` | Message number |
| `%` | Percentage |
| `T` | Time |
| `D`, `P` | Duration and pitch of a beep |
| `0` | An operand byte that is read but not used |

### 2.4 The original engine and this interpreter

Two programs run this data, and the document keeps them apart:

- **The original engine** is the Z80 or 6502 code that shipped with each game (the *original*, for short). Unqualified statements about what happens when a game runs, such as how the parser fills its slots or how a table is scanned, describe the original engine.
- **This interpreter** is TaylorMade, the C reimplementation in this directory. Statements about it always say "this interpreter" or name its source files. They cover what the original has no counterpart for (the registry of releases, finding tables in a file), and the places where this interpreter deliberately differs from the original.

---

## 3. Game state

### 3.1 Flags

There are 128 flags of one byte each, holding 0–255. Several have fixed roles:

| Flag | Role | Initial value in the games |
|------|------|----------------------------|
| 0 | Current location | Set to 0 at start ([§3.3](#33-initial-state)) |
| 1 | Dark when nonzero | 0 |
| 2 | Carried location | 253 (`FD`) |
| 3 | Worn location | 254 (`FE`) |
| 4 | Maximum number of objects carried | Per game |
| 5 | Number of objects carried | 0 |
| 6 | Number of objects | Per game |
| 7 | Wait counter (*Rebel Planet* and *Kayleth* only) | 0 |

Flags 0 to 6 are loaded from the initial flag table of the file; the rest start at 0.

The later engine has no score or turn-count flags; the tables keep any counters themselves.

For *Kayleth*, this interpreter ignores flag 6 and takes the number of objects to be 120, which avoids printing an empty "You notice." line.

### 3.2 Object locations

There are 256 object location slots of one byte each. A location is a room number or one of:

| Value | Meaning |
|-------|---------|
| `FC` (252) | Destroyed |
| `FD` (253) | Carried (by default) |
| `FE` (254) | Worn (by default) |

The carried and worn locations are whatever flags 2 and 3 hold; `FD` and `FE` are only their initial values.

### 3.3 Initial state

When a game starts:

1. All flags are set to 0.
2. Flags 0 to 6 are copied from the initial flag table (seven bytes).
3. Flag 0 is set to 0. Room 0 is the introduction or title room, which has no picture. (So is room 91 in *Kayleth*.)
4. The location of each object, for as many objects as the game has, is copied from the object location table (one byte per object).
5. The wait counter, if the game has one, is set to 0.

---

## 4. Text

All text is stored tokenised.

### 4.1 Token table

The token table is a list of up to 256 tokens. Each token is one or more ASCII characters, one per byte, in bits 0–6. Bit 7 is set on the last character of a token. Token *n* is found by skipping *n* tokens from the start of the table.

### 4.2 Strings

A string is a list of token numbers, one byte each, ending in a terminator byte:

| Terminator | Meaning |
|------------|---------|
| `5E` | End of string; a space is pending after it |
| `7E` | End of string; no space is pending |

Because the terminators are ordinary byte values, tokens `5E` and `7E` cannot appear in a string. A string table is a sequence of strings; string *n* begins after the *n*-th terminator.

### 4.3 Rebel Planet strings

In the Rebel Planet variant, the terminators are **not** bytes of the string. They are characters (`^` = `5E`, `~` = `7E`) inside the token expansions. A string ends at the first such character in its expanded text, which may fall in the middle of a token; the rest of that token belongs to the next string.

Finding string *n* therefore means expanding every token of the strings before it.

### 4.4 Capitalisation and spacing

The first letter after a `.` is capitalised automatically. A pending space ([§4.2](#42-strings)) is written before the next character printed.

This interpreter also prints a space after every message ([§5](#5-messages)), except in *Blizzard Pass*.

---

## 5. Messages

Messages are a string table ([§4.2](#42-strings)).

### 5.1 System messages

The first 33 messages are used by the engine itself:

| Message | Use |
|---------|-----|
| 0 | `. You can see` (before the list of ordinary objects) |
| 1–10 | The directions North, South, East, West, Up, Down, NorthEast, NorthWest, SouthEast, SouthWest |
| 11 | `That input contained no recognized words` |
| 12 | `It is beyond my power to do that.` |
| 13 | `. You can go` (before the list of exits) |
| 14 | `what do you want to do now` (not in the early games) |
| 15 | `you're carrying too much.` |
| 16 | `you are carrying the following:` |
| 17 | `nothing at all` |
| 18 | `game's now over. Do you want to play again (Y/N) ?` |
| 19 | `Ok.` |
| 20 | `press any key to continue` |
| 21 | `you already have it` |
| 22 | `sorry, I don't see it here.` |
| 23 | `you don't have it` |
| 24 | `you can't go that way` |
| 25 | `It is dark. You cannot see.` |
| 26 | `do you want to restore a saved game ?` |
| 27 | (empty) |
| 28 | `you are not wearing it` |
| 29 | `you are wearing it` |
| 30 | `(worn),` |
| 31 | `No help at this stage.` |
| 32 | `I don't understand that verb` |
| 33– | Game text |

The texts are those of a typical game; each game words them its own way.

### 5.2 Second message table

The Terraquake variant has a second message table, in the same format, printed by action 12 (`MESSAGE2`).

---

## 6. Rooms

Room descriptions are a string table, indexed by room number.

In *Blizzard Pass*, the original prints the short descriptions in the graphical game and the long ones in the text-only game ([§1.2](#12-game-files)). This interpreter always prints the long descriptions, which exist for rooms 0 to 101, and uses the short table only for rooms beyond that.

---

## 7. Objects

Object descriptions are a string table, one string per object. The initial location of each object is in the object location table, one byte per object ([§3.3](#33-initial-state)).

### 7.1 Low and ordinary objects

The first objects are **low** objects: their text is written as a continuation of the room description. The rest are **ordinary** objects, listed after message 0.

The file does not say where the low objects end. This interpreter takes the first object whose description ends in a comma as the first ordinary one. In the Rebel Planet variant, only an object past number 20 counts. In *Blizzard Pass* the rule does not work, and the number of low objects is fixed at 70.

---

## 8. Dictionary

The dictionary (word table) is a single list of 5-byte entries:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 4 | The first four letters of the word, upper case ASCII, padded with spaces |
| 4 | 1 | The word's value |

The list ends with the entry `XXXX FF`. Several words may share a value (synonyms). Input words are matched on their first four letters.

| Value | Meaning |
|-------|---------|
| 0 | None (noise words such as AND) |
| 1–10 | The directions, in the order of system messages 1–10 |
| `7E` | Reserved: "any word" in the engine tables |

Alan Cox's notes say bit 7 of the value is set for nouns and clear for verbs. That holds in *Blizzard Pass* and *Terraquake* only: all their verbs are below 128 and all their nouns 128 or above. The other games number their nouns on from the verbs, and some nouns stay below 128 (from 50 in *Questprobe 3*, which has no values of 128 or above at all; from 85 in *Rebel Planet*, 67 in *Kayleth* and 120 in *Temple of Terror*). The original parser does not look at bit 7 but compares the value with a per-game threshold; see [§8.1](#81-assigning-words-to-slots).

### 8.1 Assigning words to slots

The original parser reads the words of a command it recognises into the slots *verb*, *noun*, W3, W4 and W5 (Questprobe 3 has no W5). It does not fill the slots in the order the words were typed. Each of the first two words it recognises goes to the verb slot if its value is below a per-game threshold, and to the noun slot otherwise, so `FLAME OUT` is matched as `OUT FLAME`; if both are of the same kind, the second replaces the first. Later words go to W3, W4 and W5 in order, and words beyond the last slot are ignored.

| Game | Verb threshold | Words that keep their position |
|------|----------------|--------------------------------|
| Questprobe 3 | 50 | 16 and 44 |
| Rebel Planet | 85 | 16 and 44 |
| Blizzard Pass | 85 | — |
| Terraquake | 66 | — |
| Temple of Terror, Kayleth | 63 | — |

The words that keep their position stay in the slot of the order they were typed in, as the first or second word.

The verb is kept for a command after a comma (`GET LAMP, SWORD`). In *Questprobe 3* a command with no verb is answered "I don't know how to do that" without looking at the tables; the later games give up only when no word is recognised at all.

In *Blizzard Pass*, the original engine changes a noun typed before any verb to a verb: SKIN (249 → 43), POLISH (175 → 44) and DRAW(BRIDGE) (134 → 49). See [§B.1](#b1-blizzard-pass-in-both-systems) for why.

The Spectrum and C64 engines of all the games have this parser, and this interpreter works the same way.

---

## 9. Exits

The exit table is one stream of room blocks:

```text
exit-table  = { room-block } FF
room-block  = (80 + room) { verb destination }
```

- A room block starts with the room number plus `80`.
- Each exit is two bytes: a verb (1–10) and the destination room.
- The block ends at the next byte with bit 7 set: the start of the next block, or the final `FF`.

The verb number is also the number of the system message that names the direction ([§5.1](#51-system-messages)). Only verbs 1 to 10 are exits. Exits are tried before the event table.

---

## 10. Engine tables

A game has two engine tables:

- The **status** table is run every turn.
- The **event** table is run against the player's input.

The names are those of the BBC source ([Appendix A](#appendix-a-the-bbc-micro-source)), where the tables are called `STATUS` and `EVENT`.

### 10.1 Table layout

```text
status-table = { entry } 7F
event-table  = { verb noun entry } 7F
entry        = { condition } action { action }
condition    = opcode-byte operand [ operand ]
action       = opcode-byte [ operand [ operand ] ]
```

- An event table entry is preceded by two bytes, the verb and the noun it matches. `7E` matches any word.
- An entry has no end marker: its conditions end at the first byte with bit 7 set, and its actions end at the first byte with bit 7 clear (the next entry's verb or first condition) or at `7F`.
- An entry must have at least one action.

### 10.2 Opcode byte

| Bit | Meaning |
|-----|---------|
| 7 | Set for an action, clear for a condition |
| 6 | (Actions only) Stop: end the table scan after this action |
| 5–0 | Opcode number |

Bit 6 is never set on a condition, so a condition cannot be the last element of an entry.

The number of operand bytes follows from the opcode number alone, whether or not the opcode uses them:

| Opcode kind | No operands | One byte | Two bytes |
|-------------|-------------|----------|-----------|
| Condition | — | 1–20 | 21 and up |
| Action | 1–8 | 9–21 | 22 and up |

### 10.3 Execution

The table is scanned from the start. For the event table, an entry is tried only if its verb and noun match the verb and noun slots ([§8.1](#81-assigning-words-to-slots)).

1. The conditions of the entry are tested in order. If one fails, the rest of the entry is skipped and the scan goes on with the next entry.
2. If all hold, the actions are performed in order.
3. If an action has bit 6 set, the scan stops right after that action; the actions after it in the entry are not performed.
4. A failed `GET` or `DROP` also stops the scan.
5. Otherwise the scan goes on with the next entry.

### 10.4 Conditions

| Code | Name | Operands | True when |
|------|------|----------|-----------|
| 1 | `AT` | L | The player is in L |
| 2 | `NOTAT` | L | The player is not in L |
| 3 | `ATGT` | L | The player's room number is greater than L |
| 4 | `ATLT` | L | The player's room number is less than L |
| 5 | `PRESENT` | O | O is in the room, carried or worn |
| 6 | `HERE` | O | O is in the room |
| 7 | `ABSENT` | O | O is not present |
| 8 | `NOTHERE` | O | O is not in the room |
| 9 | `CARRIED` | O | O is carried or worn |
| 10 | `NOTCARR` | O | O is neither carried nor worn |
| 11 | `WORN` | O | O is worn |
| 12 | `NOTWORN` | O | O is not worn |
| 13 | `CREATED` | O | O is not destroyed |
| 14 | `NOTCREATED` | O | O is destroyed |
| 15 | `ZERO` | F | Flag F is 0 |
| 16 | `NOTZERO` | F | Flag F is not 0 |
| 17 | `W3` | W | The third word of the input is W |
| 18 | `W4` | W | The fourth word is W |
| 19 | `W5` | W | The fifth word is W |
| 20 | `CHANCE` | % | A random percentage is below % |
| 21 | `LT` | F N | Flag F < N |
| 22 | `GT` | F N | Flag F > N |
| 23 | `EQ` | F N | Flag F = N |
| 24 | `NE` | F N | Flag F ≠ N |
| 25 | `OBJECT` | O L | O is in L |

### 10.5 Actions

| Code | Name | Operands | Effect |
|------|------|----------|--------|
| 1 | `RESTORE` | | Ask whether to restore a saved game |
| 2 | `END` | | The game is over; ask whether to play again |
| 3 | `INVENT` | | List the objects carried and worn |
| 4 | `ANYKEY` | | Wait for a key |
| 5 | `SAVE` | | Save the game |
| 6 | `DROPALL` | | Drop everything carried |
| 7 | `LOOK` | | Describe the room |
| 8 | `OK` | | Print `Ok.` |
| 9 | `GET` | O | Pick up O |
| 10 | `DROP` | O | Drop O |
| 11 | `GOTO` | L | Move the player to L |
| 12 | `GOBY` | O | *Blizzard Pass*: move the player to the room of O |
| 12 | `MESSAGE2` | M | Terraquak variant: print message M of the second message table |
| 13 | `SET` | F | Set flag F to 255 |
| 14 | `CLEAR` | F | Set flag F to 0 |
| 15 | `MESSPRINT` | M | Print message M |
| 16 | `CREATE` | O | Put O in the current room |
| 17 | `DESTROY` | O | Put O in the destroyed location |
| 18 | `PRINT` | F | Print the value of flag F |
| 19 | `DELAY` | T | Pause |
| 20 | `WEAR` | O | Wear O |
| 21 | `REMOVE` | O | Take off O |
| 22 | `LET` | F N | Set flag F to N |
| 23 | `PLUS` | F N | Add N to flag F, stopping at 255 |
| 24 | `MINUS` | F N | Subtract N from flag F, stopping at 0 |
| 25 | `PLACE` | O L | Put O in L |
| 26 | `SWAP` | O O | Exchange the locations of two objects |
| 27 | `SWAP` | F F | Exchange the values of two flags |
| 28 | `MEANS` | V N | Replace the verb and noun of the input |
| 29 | `PUTWITH` | O O | Put the first object where the second is |
| 30 | `BEEP` | D P | (*Rebel Planet*) Beep with duration D and pitch P |
| 31 | `REFRESH` | 0 0 | Redraw the room. *Terraquake* and *Kayleth*: first "get all", for objects from 45 (*Terraquake*) or 78 (*Kayleth*) up |
| 32 | `QSAVE` | 0 0 | (*Terraquake*, *Temple of Terror*, *Kayleth*) Save to memory |
| 33 | `QLOAD` | 0 0 | (*Terraquake*, *Temple of Terror*, *Kayleth*) Restore from memory |
| 34 | Clear lower | 0 0 | (*Kayleth*) Clear the lower window |
| 35 | `OOPS` | 0 0 | (*Temple of Terror*) Undo the last move |

---

## 11. Pictures

### 11.1 Picture model

A room picture is 32 by 12 character cells of 8 × 8 pixels, with one Spectrum-style colour attribute (ink, paper, bright, flash) per cell. A picture is composed by running the room's **picture script** ([§11.4](#114-picture-scripts)), which draws **block pictures** ([§11.3](#113-block-pictures)) into the cells and then transforms them. Block pictures are made of **tiles** ([§11.2](#112-tiles)).

A block picture is a picture of the earlier Adventure International UK games, such as *Gremlins* (see http://aimemorial.if-legends.org/gfxbdp.html and `irmak.c` in `../aiukgraphics`), with a shorter header: the size is packed into one byte, and the position, which the earlier games store in the picture, is given by the script that draws it. In the earlier games a room picture is one such picture, drawn whole (*Robin of Sherwood* is special, using strips for the forest and special rules). The new thing is the picture script, which combines block pictures, and copies and transforms parts of the result. Picture scripts first appear in *Seas of Blood*. In Spatterlight, the ScottFree interpreter draws the *Seas of Blood* pictures with this interpreter's script code (`taylordraw.c` in `../aiukgraphics`).

### 11.2 Tiles

The tile table holds 8 bytes per tile, one byte per pixel row, top row first, bit 7 leftmost. Tile 0 is empty, tile 1 is a vertical line at the left edge, and tile 2 a line in the next column ([§1.3](#13-locating-the-tables)).

### 11.3 Block pictures

The block picture table is a sequence of block pictures, numbered from 0:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 1 | Bits 7–4: width − 1. Bits 3–0: height − 1 (in cells) |
| 1 | — | The tile stream, compressed, ending in `FE` |

The tile stream is the earlier games' format: a sequence of tile numbers and command bytes (bit 7 set) carrying flip, rotation, overlay, a repeat count and "add 128 to the tile number", followed by the run-length coded colour attributes.

The width field can hold only 1–16, half the width of a room picture. *Blizzard Pass* gets around this in code: after reading the size of a block picture, its Spectrum engine compares the block number with 13, 34, 111, 15, 17 and 85, in that order, and adds 16 to the width if it is one of them (`71BC`–`71D8` in the Spectrum release). Blocks 13, 15, 17, 34 and 85 are stored as 16 wide and so become 32, the full width of the picture; block 111 is stored as 1 wide and becomes 17. None of the other games has this code.

In some releases the tile stream has an extra layer of compression, of one of two kinds. Decompression replaces each token with the bytes it stands for and leaves every other byte as it is; it stops at `FE`. Compressed or not, the same picture unpacks to the same tile stream on the Spectrum and the Commodore 64, apart from a few pictures that differ between the releases.

**Rebel Planet (Spectrum and C64).** *Rebel Planet* uses these tokens on both platforms; *Blizzard Pass* is decoded the same way but contains none. Common forms of the repeat command `82` *count* *tile* are replaced with single-byte tokens:

| Token | Expansion |
|-------|-----------|
| `EF` | `82 01 00` |
| `EE` | `82 02 00` |
| `EB` | `82 03 00` |
| `F3` *n* | `82` *n* `00` |
| `FA` *len* *copies* | The next *len* − 1 bytes, decompressed in turn, repeated *copies* times |

**Terraquake variant (Spectrum only).** The C64 releases of *Terraquake*, *Temple of Terror* and *Kayleth* store the tile stream uncompressed. In the Spectrum releases, the game has a pattern table of *k* marker bytes, followed by *k* two-byte addresses, followed by the patterns. A byte equal to marker *i* is replaced with the pattern at address *i*, up to a per-game end marker byte. The table address, *k* and the end marker are in the registry.

### 11.4 Picture scripts

The picture script table is a sequence of scripts, each ending in `FF`. Script *n* is the picture of room *n*.

A script is a sequence of instructions. A byte that is not one of the codes below is the number of a block picture, and is followed by the cell position *x*, *y* at which to draw it.

The second column gives the address of each routine in the Spectrum release of *Terraquake*, where the dispatcher is at `6F09` and a script is run by calling `6EF9`. Drawing a block picture is `6F84`, which calls `7234` to find the block.

| Code | Address | Operands | Effect |
|------|---------|----------|--------|
| `FF` | | | End of script |
| `FE` | `733C` | | Mirror the left half into the right half |
| `FD` | `6FF5` | *before* *after* | Replace colour *before* with *after*, in both ink and paper |
| `FC` | `76D4` | *y* *x* *height* *attribute* *width* | Set the attribute of a rectangle of cells |
| `FC` (older) | | *x* *y* *attribute* *length* | Set the attribute of a run of *length* cells |
| `FB` | `700D` | | Make every cell bright |
| `FA` | `7512` | | Flip the picture horizontally |
| `F9` | `6F26` | *room* | Draw the picture of *room* as a part of this one (handled in the dispatcher: calls `6EF9`) |
| `F8` | `729D` | *object* | Stop drawing if *object* is not in the room |
| `F8` (older) | | *object* *block* *x* *y* | Draw *block* at *x*, *y* if *object* is in the room |
| `F7` | `743A` | *block* *x* *y* | Draw *block* at *x*, *y* |
| `F6` | `744E` | *block* *x* *y* | Draw *block* at *x*, *y* |
| `F5` | `7444` | *block* *x* *y* | Draw *block* at *x*, *y* |
| `F4` | `7458` | *object* | Stop drawing if *object* is in the room |
| `F3` | `7409` | | Mirror the top half into the bottom half |
| `F2` | `7331` | *y* *x* *y2* *width* | Mirror an area horizontally |
| `F1` | `73FE` | *x* *y* *y2* *width* | Mirror an area vertically |
| `EE` | `7507` | *y* *x* *y2* *width* | Flip an area horizontally |
| `ED` | `7654` | | Flip the picture vertically |
| `EC` | `7649` | *x* *y* *y2* *width* | Flip an area vertically |
| `EB`, `EA` | `0000` | | Crash: the original jumps to the ROM reset. This interpreter stops drawing instead. |
| `E9` | `7678` | *before* *after* | Replace colour *before* with *after* (paper or ink), brightness included |
| `E8` | `747A` | | Clear the picture |

Notes:

- `F0` and `EF` are block picture numbers, not instructions.
- "In the room" for `F8` and `F4` means that the object's location is the room the player is in, even while `F9` draws another room's script.
- The original `F7`, `F6` and `F5` store `0C`, `04` or `08` at `6FB7` (an ordinary block draw stores 0). Nothing reads that byte.
- Area operands are not clipped to a row: a column past the right edge is a cell of the next row, as in the original.

### 11.5 Kayleth animations

*Kayleth* has a table of room animations ([§1.3](#13-locating-the-tables)), one entry per room:

```text
animation-table = { FF | animation }
animation       = loop-to object skip { delay skip image } FF
```

An `FF` alone is a room without an animation. *loop-to* is the frame to go back to, in units of 3 bytes, *object* is an object that must be present, and each 3-byte frame record gives a delay and a block picture.

### 11.6 Rebel Planet animations

*Rebel Planet* has no animation data. Its six animations are Z80 code, and an interpreter has to reimplement them. The addresses below are in the Spectrum release.

The routine at `71D4` does one step of animation each time it is called. There is no interrupt or timer: the key wait loop (`6525`) and the wait at a full screen (`646E`) call it on every pass while they wait for a key. Animation speed therefore depends on how fast the loop runs, and nothing moves while the game is busy. Each call does the following:

1. Increment a frame counter (`5BCE`). The animations use its low bits to run at a fraction of the call rate.
2. If the picture on screen (`611A`) is not that of the player's room (`5D87`), clear the serpent state (`5BCF`) and return.
3. Go through the object list at `72E3`: 36, 92, 58, 107, 50, 42, 0. The first object that is in the player's room picks the animation. If none is there, clear the serpent state and return.

The routine doesn't check which room it is in, only where the object is. In the shipped game each of these objects only shows up in the rooms given below.

| Object | Room | Code | Animation |
|---|---|---|---|
| 50, unfolding space | 1 | `72FD` | Stars. Runs when bit 1 of the counter is set. Pixel lines 24–48 rotate one pixel, wrapping: cells 6–15 rotate left (`7174`), cells 16–25 rotate right (`719D`). |
| 42, force fields | 86 | `72EA` | Force field. Runs on every call. Pixel lines 16–55, cells 13–18, rotate one pixel right. |
| 107, holograph head | 88 | `721E` | Queen's computer. Runs on every call; see below. |
| 92, Sycane Serpent | 29–33 | `7270` | Serpent. Runs when bit 0 of the counter is set. Draws the next of blocks 63–67 at *x* 14 and *y* 9, 8, 6, 5, 4, then stops. |
| 58, security robot | 50 | `72AF` | Robot. Draws block 138 at 13,2 when bit 1 of the counter is set, otherwise block 139. |
| 36, crag snapper | 71 | `72C1` | Snapper. Draws block 142 at 17,6 when bit 1 of the counter is set, otherwise block 133 at 14,4. |

Positions are in character cells, *x* first, and blocks are drawn with the ordinary block routine (`7061`). The stars and the force field share one routine: `72EA` changes its start position, width and line count for the force field, and `72FD` restores them. The rotation works on screen memory, so whatever is drawn in those cells (the stars, the field) moves. The attributes stay where they are.

The serpent stores the last block it drew in `5BCF`, and 0 means none yet. That variable is cleared whenever the serpent is not in the room, so it grows again from the start every time it comes back. The original doesn't animate the serpent going back down; this interpreter does ([§11.6.2](#1162-this-interpreter)).

The Queen's computer does two things on each call:

- **Head.** It draws block 18, the head, at 18,1 when bit 3 of the counter is set. Otherwise it draws block 166, an empty 3×5 block in the same place. So the head blinks on and off every 8 calls.
- **Lights.** It recolours 15 cells straight in attribute memory, using the 15 low bytes of the table at `7260`–`726E`:
  - row 3, cells 3, 4, 10, 11, 27, 28
  - row 6, the same cells
  - row 7, cells 18–20

  Each cell gets the next paper colour from a counter kept at `726F`, which steps by 8, so the paper colour cycles through 0–7. The ink is kept and BRIGHT is set. Each call starts where the previous one stopped, so the colours appear to run around the lights.

#### 11.6.1 The C64 release

The C64 release has the same structure. At startup it copies its animation code to `CC00`–`CFFF`, and the routine is at `CC5F` (runtime addresses). The key-wait loops call it before each `GETIN` (`FFE4`). It returns at once if `5C` is non-zero. Its frame counter is 16 bits wide, with the low byte at `5B` and the high byte at `64`. The object list is at `CE99` and holds the same objects in the same order.

The Queen's computer (`CCB1`) works differently from the Spectrum version:

- **Head.** The head is never redrawn. When bit 2 of the high counter byte is set, the routine writes the head's 15 screen-matrix colour bytes from the table at `CEB2`. Otherwise it writes zeros, which leaves the head black on black. The head therefore changes every 1024 calls. Measured in VICE, that is about every 8 seconds.
- **Lights.** These are only updated when bit 1 of the low counter byte is clear. The cells are the same, in the same order, from the table at `CE89`–`CE97`, offset from the screen matrix at `607C`. Each cell's screen-matrix byte is set to `F0` OR the counter at `CE98`, which gives light grey set pixels on a background of the counter's colour. The counter starts at 15 and is then decremented, wrapping from 0 to 15. So the background colours count down through all 16 C64 colours, where the Spectrum counts up through 8.

#### 11.6.2 This interpreter

This interpreter reimplements the animations in `animations.c`, with these differences from the original:

- **Timing.** The animations run on a Glk timer at a fixed rate for each animation, not from the key-wait loop. Each tick does one step, without the original's frame-counter tests. So the robot and the snapper change on every tick.
- **Stars.** Only pixel lines 24–47 are rotated, because line 48 is blank in every release.
- **Serpent.** When the serpent leaves the room, it is drawn going back down, block by block.
- **Queen's computer.** The lights are recoloured as in the original: in the Spectrum order and colours, or in the C64's when a C64 palette is in use. The head is shown for 8 ticks and hidden for 8, or for 64 and 64 in the C64 release. The original's period of 1024 calls would be too long at this interpreter's tick rate.

---

## 12. Questprobe 3

Questprobe 3 differs from the later games in most of its encodings. Anything not described here is as in the later variants.

### 12.1 Game state

The first four flag slots are the location of the current character, the location of the other character, the carried location and the other character's carried location. Flag numbers in the engine tables are counted from the slot after those, so **flag *n* of the game is slot *n* + 4**. Only the four slots are copied from the initial flag table ([§3.3](#33-initial-state)).

| Game flag | Slot | Role |
|-----------|------|------|
| 1 | 5 | Wait count |
| 22, 23 | 26, 27 | Turn count (the low byte wraps at 100) |
| 27 | 31 | Which character is being played |
| 39 | 43 | Dark flag of the current character |

There are 49 objects and nothing can be worn. Every object is a low object ([§7.1](#71-low-and-ordinary-objects)).

| Location | Meaning |
|----------|---------|
| `FC` | Destroyed |
| `FD` | Carried by the Human Torch |
| `FE` | Carried by the Thing |

### 12.2 Text

- Bytes below `7B` are literal characters. Bytes from `7B` up are token numbers, offset by `7B` (byte `7B` is token 0).
- A string ends in `1F`, and a string table ends in `18`.
- The system messages are messages 210 and up (message 210 + *n* is system message *n*), except that the exits message is 217.
- The first letter after `.`, `!`, `?` or `:` is capitalised.

### 12.3 Engine tables

The table layout and opcode byte are as in [§10.1](#101-table-layout) and [§10.2](#102-opcode-byte). The status table may have `7E` padding bytes between its entries. The operand counts differ:

| Opcode kind | No operands | One byte | Two bytes |
|-------------|-------------|----------|-----------|
| Condition | — | 1–15 | 16 and up |
| Action | 1–8 | 9–17 | 18 and up |

Conditions:

| Code | Name | Operands |
|------|------|----------|
| 1 | `AT` | L |
| 2 | `NOTAT` | L |
| 3 | `ATGT` | L |
| 4 | `ATLT` | L |
| 5 | `PRESENT` | O |
| 6 | `ABSENT` | O |
| 7 | `CARRIED` | O |
| 8 | `NOTCARR` | O |
| 9 | `CREATED` | O |
| 10 | `NOTCREATED` | O |
| 11 | `ZERO` | F |
| 12 | `NOTZERO` | F |
| 13 | `W3` | W |
| 14 | `W4` | W |
| 15 | `CHANCE` | % |
| 16 | `LT` | F N |
| 17 | `GT` | F N |
| 18 | `EQ` | F N |
| 19 | `OBJECT` | O L |

Actions:

| Code | Name | Operands | Effect |
|------|------|----------|--------|
| 1 | `SWITCH` | | Swap the two characters' inventories and dark flags |
| 2 | `WATCH` | | Print the watch message: turns and restedness |
| 3 | `RESTORE` | | |
| 4 | `END` | | Ask "play again?" |
| 5 | `INVENT` | | |
| 6 | `ANYKEY` | | |
| 7 | `SAVE` | | |
| 8 | `OK` | | Prints nothing; stops the table scan, like the Quill's `DONE` |
| 9 | `GET` | O | |
| 10 | `DROP` | O | |
| 11 | `GOTO` | L | |
| 12 | `GOTOH` | O | Go to where object O (the other character) is |
| 13 | `SET` | F | |
| 14 | `CLEAR` | F | |
| 15 | `MESSPRINT` | M | |
| 16 | `CREATE` | O | |
| 17 | `DESTROY` | O | |
| 18 | `LET` | F N | |
| 19 | `PLUS` | F N | |
| 20 | `MINUS` | F N | |
| 21 | `PLACE` | O L | |
| 22 | `SWAP` | O O | |
| 23 | `PICTURE` | N 0 | Draw picture N on top of the room picture; 0 = the room picture itself |

### 12.4 Pictures

Questprobe 3 pictures are in the format of the earlier Adventure International UK games, such as *Gremlins*, *Savage Island* and *Supergran* (picture format version 3 in the Scott interpreter): each picture is drawn whole, with no picture script to combine it with others. A picture is the same as a block picture ([§11.3](#113-block-pictures)) except for its header, which is 4 bytes and includes the position, and the tile stream has none of the extra compression:

| Offset | Content |
|--------|---------|
| 0 | Width in cells |
| 1 | Height in cells |
| 2 | X offset |
| 3 | Y offset |

What is unique to Questprobe 3 is how pictures are drawn. The original engine never draws a room picture on its own; the game draws every picture with the `PICTURE` action ([§12.3](#123-engine-tables)), usually from the status table. `PICTURE 0` clears the picture area and draws the picture of the current room, and `PICTURE` *n* draws picture *n* − 1 on top of what is already there.

The pictures are found through two tables, located by searching:

1. A picture index table of one byte per picture (starting `00 01 01 02 03 04 05 06 02 02`). Bits 6–0 select an entry of the offset table.
2. An offset table of two-byte offsets (starting `00 00 A7 02 A7 03 B9 08 D7 0B`), relative to the start of the picture data (which starts `20 0C 00 00 8A 01 44 A0 17 8A`).

There are 246 tiles. Two pictures also use tile 255, for which the original draws the bytes after the tile table as speckles.

---

## 13. Saved games

### 13.1 What is saved

The whole mutable state of a game is the 128 flags ([§3.1](#31-flags)) and the 256 object location slots ([§3.2](#32-object-locations)). Everything else is read from the game image and does not change:

- the room, object and message text, the dictionary and the engine tables,
- the room exits ([§9](#9-exits)), which no action modifies,
- the initial state ([§3.3](#33-initial-state)).

The player's room is flag 0, the carried and worn location codes are flags 2 and 3, and the carried count is flag 5, so those are saved with the flags. The picture on screen, the pending input, the wait counter's display and the number of objects (flag 6, fixed per game) need no separate record. Nothing in a saved game identifies the game or the release; one game's file loads into another and is read as that game's state.

This interpreter keeps the numbers of flags and object slots fixed at 128 and 256 whatever the game's own counts, so a game with fewer objects saves the unused slots too. The original engines save only 67 flag slots (69 in *Questprobe 3*) followed by as many object slots as the game has.

**The original engines** keep fewer flags and objects and save them in their own ways, described in [§13.4](#134-original-spectrum-releases) and [§13.5](#135-original-commodore-64-releases). This interpreter cannot read their saved games, and they cannot read its files.

### 13.2 Save file

`SAVE` ([§10.5](#105-actions), action 5, or the typed commands `SAVE` and `#SAVE`) asks the player for a file through the Glk file dialog and writes 384 bytes, with no header:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 128 | Flags 0–127, one byte each |
| 128 | 256 | Object locations 0–255, one byte each |

If the file cannot be opened, it prints `Save failed.`; otherwise `Saved.`. The game goes on.

`RESTORE` (action 1, or `#RESTORE`) asks for a file, and prints `Unable to open file.` if it does not exist. The file is read straight into the live arrays and must be exactly 384 bytes long. If it is shorter or longer, it prints `BAD DATA! Invalid save file.` and puts back the state that was current before the attempt. On success the lower window is cleared and the room is described again; the picture is redrawn by that description. A restore leaves the Kayleth preview mode, if the game is in it.

Nothing else in the file is checked: any 384 bytes restore, including a room number that has no description.

### 13.3 Undo and memory saves

Undo, the memory save slot (`QSAVE`/`QLOAD`, actions 32 and 33 of the Terraquake variant, and `#QUICKSAVE`/`#QUICKLOAD` for every game) and restarting use the same 384 bytes of state, kept in memory as a record of flags and object locations ([§13.1](#131-what-is-saved)).

- **Undo.** One state is pushed before each command that the player types, up to 100; the oldest is dropped beyond that. No state is pushed on the turn after a restore (stop-time is set), nor during the *Kayleth* preview. Undoing pops one and restores the one before it; there is nothing to undo on the first turn. The *Temple of Terror* `OOPS` action (35) does the same without the `Move undone.` message, and *Terraquake* uses it after sending the player to the "By the power of Grayskull" room (action `GOTO 83` pushes a state first).
- **Memory save.** One slot. Saving again replaces it (`Previous state deleted.`). Restoring the slot pushes an undo state afterwards, so it can be undone.
- **Restart.** The initial state is recorded when the game starts, and is restored by `#RESTART`.

Every restore, from file or memory, sets the stop-time flag so that the rest of the current turn's table scan does not run on the restored state.

### 13.4 Original Spectrum releases

All the Spectrum games save to tape through the 48K ROM routines `SA-BYTES` ($04C2) and `LD-BYTES` ($0556), and keep the state in one contiguous RAM block: flag slots first (flag 0, the player's room, is the first byte), then the object locations. Only 67 flag slots exist in the games below, except *Questprobe 3*, which has 69 (four character slots and 65 game flags).

| Game | Block start | Length | Flags | Objects | Format |
|------|-------------|--------|-------|---------|--------|
| *Rebel Planet* | $5D87 | 323 | 67 | 256 | CODE file |
| *Questprobe 3* | $5B70 | 118 | 69 | 49 | CODE file |
| *Blizzard Pass* (128K) | $5D87 | 321 | 67 | 254 | raw block |
| *Terraquake* | $5D5C | 159 | 67 | 92 | raw block |
| *Kayleth* | $5D5C | 191 | 67 | 124 | raw block |
| *Temple of Terror* | $5D5C | 259 | 67 | 192 | raw block |

The object count is the initial value of flag 6 plus one for *Terraquake*, *Kayleth* and *Blizzard Pass*. *Kayleth*'s own table has 124 slots although this interpreter forces 120 objects.

**CODE file** (*Rebel Planet*, *Questprobe 3*). The game calls the ROM's `SAVE "SAVED GAME" CODE` entry (`$0970`) with the block address in HL and a 17-byte header prepared in RAM, so the ROM prints `Start tape, then press any key.` and writes a standard header (type 3, name `SAVED GAME`, the length above) followed by a data block with flag byte $FF. A save is therefore an ordinary tape file that a BASIC `LOAD "" CODE` can read. Restoring prints the game's message 26 (*do you want to restore a saved game ?*) and takes `Y` or `N` (`N` returns to the game). It then calls the ROM's header search (`LD-LOOK-H`, $0761) with the expected header, which skips tape files of a different type or name, rejects a file longer than expected, and loads the data to a buffer at $4014 in the screen memory; after a key press the game copies the block to the state arrays and carries on.

**Raw block** (*Terraquake*, *Kayleth*, *Temple of Terror*, *Blizzard Pass*). The game prints message 20 (*press any key to continue*) and calls `SA-BYTES` directly with IX = block start, DE = length and A = $2A, so the tape holds just a data block with flag byte $2A and the ROM's XOR parity byte, with no header and no name. Restoring asks message 26 (`Y`/`N`) and calls `LD-BYTES` with the same flag byte and length. A block of another length or flag, or a tape error, makes the ROM return with carry clear, and the game then restarts from the beginning, as it does after `play again? Y`.

Neither variant checks the contents.

### 13.5 Original Commodore 64 releases

The C64 games call the KERNAL `SAVE` ($FFD8) and `LOAD` ($FFD5) directly. The block always starts with the flag slots at $0400, with the object locations from $0443 (67 flag slots), and ends before $0543, so it is 323 bytes in *Rebel Planet*, *Kayleth* and *Terraquake* (*Temple of Terror* is the same). *Questprobe 3* keeps its state at $CA00 and saves 119 bytes, up to but not including $CA77. That is one byte more than the Spectrum version's 118; the extra byte follows the 49 object slots.

| Game | File name | Device | Secondary address | Prompt on save | Prompt on load |
|------|-----------|--------|-------------------|----------------|----------------|
| *Rebel Planet* | `RPSAVED` | 1 (tape) | 0 | `Press PLAY and RECORD on tape` | `Press PLAY on tape` |
| *Questprobe 3* | `SAVED GAME.` | 1 (tape) | $FF | `PRESS PLAY AND RECORD ON TAPE.` | `PRESS PLAY ON TAPE.` |
| *Kayleth* | `KLSAVED` | 8 (disk) | 1 | `PRESS PLAY AND RECORD ON TAPE` | `PRESS PLAY ON TAPE` |
| *Terraquake* (tape) | `MASTERS` | 1 (tape) | 1 | `PRESS PLAY AND RECORD ON TAPE` | `PRESS PLAY ON TAPE` |
| *Temple of Terror* (disk) | `MASTERS` | 8 (disk) | 1 | none | none |

The file name is passed to `SETNAM` by a small routine that sits next to `SETLFS`, with the name bytes right after it. *Kayleth* passes length 9 for the 8-byte string `KLSAVED` + carriage return, so one byte of code follows it into the name. In the *Terraquake* and *Temple of Terror* images that were examined the string is present but no `SETNAM` call is, so those saves are made with whatever name the KERNAL last had (none).

**Saving** prints the prompt, waits for a key, calls `SAVE` with the range above, and sets flag $56 to redraw the screen. There is no error check; a failed save is not noticed. **Restoring** asks message 26 (`Y`/`N`, `N` returns), prints the prompt, calls `LOAD` with X/Y = $0400 (or $CA00) and secondary address 0 where the file's own address is not used. It fails with `LOAD ERROR TRY AGAIN` if the KERNAL reports an error or if the end address's low byte differs from the expected one ($43, or $77 in *Questprobe 3*), and asks again. On success the screen is redrawn.

The files are therefore plain KERNAL program files with a two-byte load address ($0400 or $CA00) followed by the block. The Spectrum and C64 states have the same layout in *Rebel Planet* (67 flags, 256 object slots) but the files are different containers and the tape formats do not interchange.

*Questprobe 3* on the BBC Micro and the Amiga and other releases were not examined.

---

## Appendix A. The BBC Micro source

The 6502 source of the BBC Micro Questprobe 3 survives. Its opcode names, which partly overlap with those of the Quill, are the source of the names used by this interpreter and this document.

| Kind | Names, in opcode order |
|------|------------------------|
| Conditions | `AT NOTAT ATGT ATLT PRESENT ABSENT CARRIED NOTCARR CREATED NOTCREATED ZERO NOTZERO W3 W4 CHANCE LT GT EQ OBJECT` |
| Actions | `SWITCH WATCH RESTORE END INVENT ANYKEY SAVE OK GET DROP GOTO GOTOH SET CLEAR MESSPRINT CREATE DESTROY LET PLUS MINUS PLACE SWAP PICTURE` |

`PICTURE` does nothing on the BBC. The later games add `W5` for the fifth word.

The source confirms the following:

- The flags start 4 bytes into the data block ([§12.1](#121-game-state)).
- The operand count comes from the opcode number, as in the Quill.
- Bit 6 of an action byte stops the table scan.
- An entry matches verb then noun only, with `7E` for any word, and the parser fills the slots by value ([§8.1](#81-assigning-words-to-slots)).
- Exits use the room+`80` block format of the later games ([§9](#9-exits)), and are tried before the `EVENT` table, as in the Quill.

---

## Appendix B. Comparison with the Quill

The original engine comes from a Quill-like system written by Roger Taylor and James Byrne of Ashminster Computing. They first wrote it to port a Quill game, *Hampstead*, to the BBC Micro in 1984, a year before Gilsoft's own BBC Micro Quill. Adventure International UK licensed it for its version of Questprobe 3 and improved it for each game after that (according to IFWiki). The data fits: the engine works like the Quill, but none of its tables can be read as Quill data.

The Quill side below is the Spectrum Quill, serial C, the last and largest of Gilsoft's releases (https://www.ifwiki.org/The_Quill lists them all). The earlier serial A has fewer opcodes, and the other machines' versions differ in their details.

| Aspect | Quill | Original engine |
|--------|-------|------------|
| Header | A block of counts and table pointers, found by its signature `10 xx 11 xx 12 … 15`. | None. Table offsets per release, or found by searching ([§1.3](#13-locating-the-tables)). |
| Text | Each byte stored as `FF` minus the character; a string ends in `1F`. Later games mix literal characters with tokens 165 and up, from a dictionary whose entries end in a byte with bit 7 set. | Every byte of a string is a token, with the same bit-7 ending, and a string ends in `5E` or `7E`. Questprobe 3 is closer to the Quill: literal characters, tokens from `7B` up, strings ending in `1F`. |
| String tables | Rooms, objects and messages each have a table of two-byte pointers. | No pointers; string *n* is found by skipping the *n* strings before it. |
| Flags | 37 flags. 0 is dark, 1 the number of objects carried, 2–10 count down on their own, 30 is the score, 31–32 the turn count, 33–34 the verb and noun, 35 the location. | 128 flags, special ones at the bottom ([§3.1](#31-flags)). 0 is the location, 1 is dark. No score or turn flags, except Questprobe 3's turn count. |
| Locations | `FC` destroyed, `FE` carried, `FD` worn. | `FC` destroyed, `FD` carried, `FE` worn (swapped), with flags 2 and 3 holding those two numbers. |
| Words | 5 bytes per word: 4 letters, stored inverted like the text, and a value. The table ends at a 0 byte. Two words, the first taken as the verb. | 5 bytes per word in plain ASCII, ending `XXXX FF`. Up to five words; the first two are sorted into verb and noun by value ([§8.1](#81-assigning-words-to-slots)), so their order does not matter. `W3`–`W5` test the rest. |
| Exits | A pointer per room to a list of verb and room pairs, ending in `FF`. Any verb can be an exit. | One stream of room blocks ([§9](#9-exits)); only verbs 1–10 are exits. In both engines the exits are tried before the event table. |
| Tables | Process table (= status table) and Response table (= event table). Entry: verb, noun (`FF` = any word), then a pointer to the conditions, which end in `FF`, followed by the actions, also ending in `FF`. A verb of 0 ends the table. | Entry: verb, noun (`7E` = any word), then conditions and actions in line. Bit 7 tells an action from a condition, so there are no end markers. `7F` ends the table. |
| Opcodes | Conditions and actions numbered separately from 0. The action number sets the operand count: 0–16 none, 17–28 one, 29 and up two. | Numbered from 1, with the same layout for conditions as well (none, one, two bytes). Mostly the same names (Appendix A). Questprobe 3 keeps the Quill's order for the first six conditions: `AT NOTAT ATGT ATLT PRESENT ABSENT`. |
| Stopping | A matching entry runs and the scan goes on. Actions such as `DONE`, `OK`, `DESC`, `END` and a failed `GET` stop the scan. | The same, except that any action stops the scan when bit 6 of its byte is set. A failed `GET` or `DROP` stops it too. |
| Carrying | `CARRIED` is true only for an object carried and not worn. `GET` checks flag 1 against the limit. | `CARRIED` is true for carried or worn. `GET` checks flag 5 against flag 4. |
| Pictures | Gilsoft's separate The Illustrator: a vector format of lines, fills and shading. | The tile bitmaps and picture scripts of the earlier Adventure International UK games ([§11](#11-pictures)). |

### B.1 Blizzard Pass in both systems

The C64 *Blizzard Pass* is a Quill game, so the same game exists in both systems. The two have the same map and puzzles, but the rooms are numbered differently. About a quarter of the messages are the same word for word, and most of the rest are reworded.

| | C64 (Quill) | Spectrum (original engine) |
|---|---|---|
| Room descriptions | One set, closest to the Spectrum's short descriptions, with the exits written into them. | Short descriptions in the graphics game, long ones in the text-only game. |
| Objects with several states | One object for each state, such as a locked, an open and a smashed door. | Descriptions built from fragments. |
| Word numbers | GET is 100. | GET is 12. |
| SKIN, POLISH | Synonyms of the verbs CUT and RUB. | Nouns (SKIN 249, POLISH 175). The verbs `VSKI` 43 and `VPOL` 44 can't be typed. |
| DRAW | A single word: DRAW PENTAGRAM, GO DRAWBRIDGE. The Quill takes the first word as the verb. | Twice in the dictionary, as verb 49 and noun 134, because the parser slots words by value. |
| Fix-up | None needed. | Engine code changes SKIN, POLISH and DRAW to verbs when no verb comes before them ([§8.1](#81-assigning-words-to-slots)). |

Those workarounds suggest that the game was written with the Quill and then ported to this engine, as *Hampstead* was. The C64 title text says it was "transferred from Spectrum 128 with a few slight alterations", which fits the author's own Quill version having been written on a Spectrum 128.

#### Status and event tables

The two versions' tables were compared by mapping rooms (90 of 111 matched, from identical description text and then exit propagation, with no inconsistencies), messages (by text similarity) and words (by name). The room and message numbers below are therefore approximate. The Quill calls the tables "process" and "response". They correspond to the status and event tables here.

| | C64 (Quill) | Spectrum (TaylorMade) |
|---|---|---|
| Event / response entries | 342 | 431 |
| Status / process entries | 39 | 94 |
| Words (entries / distinct values) | 180 / 131 | 218 / 146 |
| Messages used | 115 | 172 or more |
| Verb and noun numbers | Verbs 1–48, nouns from 79. | Verbs below 85, nouns from 128. |
| Room, object, flag, message numbers | Unrelated to the Spectrum's. | Unrelated to the C64's. |
| Entry order | Event table grouped by verb (movement verbs first, many conditional). | Starts with puzzle entries (DOWN CREV, HURL WIND…). |
| Conditions | AT (191), PRESENT (150), CARRIED (137). | PRESENT (187), AT (103), HERE (99), W3 (33), NOTCREATED (29), CARRIED (26). |
| Actions | MESSAGE 163, DESC 121, SCORE 37, TURNS 36, PAUSE 23. No MEANS. | MESSPRINT 207, MEANS 58, SWAP 58, PLACE 25. No score or turn counter. |

* **Words.** Only the directions map one to one (1–4 are the same; C64 5–8 are Spectrum 7–10; C64 up and down 9 and 10 are Spectrum 5 and 6). Other values map to several values, in no monotone order. 24 C64 words are absent from the Spectrum dictionary and 44 Spectrum words (AND, AT, IT, ON, OFF…) from the C64's.
* **Order.** The longest common subsequence of the verb/noun sequences of the two event tables is 21 entries. With the conditions included, it is 4, and for the status tables 2.
* **Messages.** Of the 115 C64 messages, 20 are identical to a Spectrum message and 41 are at least 85% similar. Some have no Spectrum counterpart.
* **Entry by entry.** Of 43 C64 event entries with a confidently matched message, 24 have a Spectrum entry with the same verb and noun class that prints the matched message. Even those differ in their conditions and actions. The Spectrum tests whether an object is present or created where the C64 tests AT a room plus CARRIED. The C64 adds SCORE and TURNS before END. The C64 uses PAUSE then DESC where the Spectrum uses GOTO with the stop bit. The Spectrum uses MEANS and SWAP of objects.
* **Verbs.** Some puzzles use different verbs (THRO and HURL, LOOK and READ, SMAS and FORC, CROS HOLE and CROS CREV).
* **Take and drop.** The C64 has 65 TAKE and 54 DROP entries, the Spectrum 83 and 58. The per-noun counts agree for only 18 of 57 TAKE nouns and 24 of 54 DROP nouns. The Spectrum has an unconditional `TAKE x → GET` entry for each object, and the C64 does not.
* **Status tables.** C64: starts with `AT 0 → SET 5, ANYKEY, GOTO 78, DESC` and keeps a counter in flag 5. Spectrum: starts with `NOTZERO 14, PRESENT 70 → DESTROY, MESSPRINT`, and uses `AT 0 → RESTORE, GOTO 68`.

The two versions implement the same puzzles, with the same deaths and the same messages. They are not renumbered copies of one another. Numbers, order, conditions and actions all differ, and the Spectrum uses engine features that the Quill lacks. That rules out a mechanical conversion of the tables. The tables look like a re-implementation by hand, and the matched room, object and message texts suggest that the author worked from the same game design and text. The exact method is not recoverable from the files.
