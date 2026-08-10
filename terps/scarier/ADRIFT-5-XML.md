# ADRIFT-5 Adventure XML Unofficial Specification

This document describes the ADRIFT-5 adventure document format and related save-state XML. There is no official XSD or DTD; the format is defined by the code in https://github.com/jcwild/ADRIFT-5/

`ADRIFT-5` builds a `dev500.exe` "Generator" app and a `run500.exe` "Runner" app.

## Contents

- [1. File containers](#1-file-containers)
  - [1.1 Adventure XML (logical document)](#11-adventure-xml-logical-document)
    - [1.1.1 Adventure XML file extensions](#111-adventure-xml-file-extensions)
  - [1.2 `.taf` binary layout (v5)](#12-taf-binary-layout-v5)
    - [1.2.1 Tools that work with `.taf` files](#121-tools-that-work-with-taf-files)
  - [1.3 `.amf` library modules](#13-amf-library-modules)
  - [1.4 Save games (`.tas`)](#14-save-games-tas)
- [2. General conventions](#2-general-conventions)
  - [2.1 No attributes](#21-no-attributes)
  - [2.2 Booleans](#22-booleans)
  - [2.3 Enumerations](#23-enumerations)
  - [2.4 Dates](#24-dates)
  - [2.5 Version string](#25-version-string)
  - [2.6 Colours](#26-colours-backgroundcolour-inputcolour-outputcolour-linkcolour)
  - [2.7 Omitted defaults](#27-omitted-defaults)
  - [2.8 Flat item list](#28-flat-item-list)
  - [2.9 Library flag](#29-library-flag)
  - [2.10 Expressions (overview)](#210-expressions-overview)
  - [2.11 `FromTo` Inclusive Ranges](#211-fromto-inclusive-ranges)
  - [2.12 `Control`](#212-control)
  - [2.13 Inline HTML-like tags](#213-inline-html-like-tags)
- [3. Adventure shape](#3-adventure-shape)
- [4. Shared structures](#4-shared-structures)
  - [4.1 Keys (IDs)](#41-keys-ids)
    - [4.1.1 Runtime reference tokens](#411-runtime-reference-tokens)
    - [4.1.2 Key collision on library merge](#412-key-collision-on-library-merge)
  - [4.2 Description blocks](#42-description-blocks)
    - [4.2.1 `DisplayWhen`](#421-displaywhen)
    - [4.2.2 Common containers using description blocks](#422-common-containers-using-description-blocks)
  - [4.3 Expressions](#43-expressions)
    - [Where expressions appear](#where-expressions-appear)
    - [Language outline](#language-outline)
    - [Property paths](#property-paths)
  - [4.4 Restrictions](#44-restrictions)
    - [4.4.1 `BracketSequence`](#441-bracketsequence)
  - [4.5 Actions](#45-actions)
    - [4.5.1 MoveObject and object groups](#451-moveobject-and-object-groups)
    - [4.5.2 MoveCharacter and character groups](#452-movecharacter-and-character-groups)
    - [4.5.3 Location groups](#453-location-groups)
  - [4.6 Percent-functions and special tokens](#46-percent-functions-and-special-tokens)
    - [Interactive pop-ups](#interactive-pop-ups)
    - [Session / environment tokens](#session--environment-tokens)
    - [Listing and display helpers](#listing-and-display-helpers)
    - [Text utilities](#text-utilities)
- [5. Adventure header](#5-adventure-header)
- [6. Adventure item types](#6-adventure-item-types)
  - [6.1 Folder (Generator UI only)](#61-folder-generator-ui-only)
  - [6.2 Property (definition)](#62-property-definition)
  - [6.3 Location](#63-location)
  - [6.4 Object](#64-object)
  - [6.5 Task](#65-task)
    - [Specific tasks](#specific-tasks)
    - [System tasks](#system-tasks)
  - [6.6 Event](#66-event)
    - [Type and lifespan](#type-and-lifespan)
    - [WhenStart](#whenstart)
    - [Control](#control)
    - [SubEvent](#subevent)
  - [6.7 Character](#67-character)
    - [Character location](#character-location)
    - [Walk](#walk)
    - [Topic](#topic)
  - [6.8 Variable](#68-variable)
  - [6.9 Group](#69-group)
  - [6.10 TextOverride (ALR)](#610-textoverride-alr)
  - [6.11 Hint](#611-hint)
  - [6.12 Synonym](#612-synonym)
  - [6.13 Function (user-defined)](#613-function-user-defined)
  - [6.14 Exclude](#614-exclude)
  - [6.15 Map](#615-map)
    - [Page](#page)
    - [Node](#node)
    - [Link](#link)
    - [Runner drawing rules](#runner-drawing-rules-for-ui-implementors)
  - [6.16 FileMappings (Blorb export)](#616-filemappings-blorb-export)
- [7. Save-game XML (`<Game>`)](#7-save-game-xml-game)
  - [7.1 Location](#71-location)
  - [7.2 Object](#72-object)
  - [7.3 Task](#73-task)
  - [7.4 Event](#74-event)
  - [7.5 Character](#75-character)
  - [7.6 Variable](#76-variable)
  - [7.7 Group](#77-group)
  - [7.8 Turns](#78-turns)
- [8. Minimal adventure skeleton](#8-minimal-adventure-skeleton)

---

## 1. File containers

### 1.1 Adventure XML (logical document)

Root element: **`<Adventure>`**.

Declaration typically:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Adventure>
  …
</Adventure>
```

Within the `<Adventure>` element you'll find "header" child elements and then "item" child elements. See [§3 Adventure shape](#3-adventure-shape) for an overview. Full lists are in [§5 Adventure header](#5-adventure-header) and [§6 Adventure item types](#6-adventure-item-types).

#### 1.1.1 Adventure XML file extensions

- When you develop a game with `ADRIFT-5`, the Generator app generates Adventure XML and then compresses it into a `.taf` file for players to download and enjoy.
  - `.taf` files can also be added to ADRIFT `.blorb` files. (See the [Blorb Spec](https://eblong.com/zarf/blorb/Blorb-Spec.html) for more information about blorbs. `.taf` files appear as executable `Exec` resource chunks of type `ADRI`.)
- `ADRIFT-5` source has ADRIFT Module Files with `.amf` extensions, which are reusable libraries of ADRIFT code. The most important AMF is `StandardLibrary.amf`.

### 1.2 `.taf` binary layout (v5)

A `.taf` file compresses Adventure XML and adds some extra metadata, like this:

1. **12-byte header** — always obfuscated, never plaintext. For v5 files it is the magic bytes  
   `3C 42 3F C9 6A 87 C2 CF 92 45 3E 61`. The loader (`FileIO.vb`) compares against the known v5/v4/v3.9 magic sequences; anything else is run through the de-obfuscator and must then read `"Version X.XX"`, so a file whose header is literal ASCII text is rejected.
2. **Optional Babel / IFID section** (5.0.20+) — the loader reads bytes 12–15 as a 4-hex-digit length and bytes 16–23 as a marker; the Babel block is recognized when the length reads `"0000"` **or** the marker reads `<ifindex` (an OR test, `FileIO.vb`). When present, a Babel block of that length follows. Pre-5.0.20 files (and extracted `.taf`s) have no Babel section and their payload is *not* obfuscated.
3. **zlib-compressed Adventure XML**, usually **XOR-obfuscated** with the fixed ADRIFT key
4. **14-byte trailer** after the compressed payload — the loader seeks to `fileLength - 14` to read it. (The `26` sometimes seen in loader arithmetic is the 12-byte header plus this 14-byte trailer combined, not the trailer size.)

### 1.3 `.amf` library modules

Plain Adventure XML (not compressed) for reusable item packs (properties, tasks, ALRs, …). The Generator merges selected `.amf` files into an adventure; the most important is `StandardLibrary.amf`.

When loading an `.amf` as a library (`bLibrary`), every imported item is stamped library-owned: `IsLibrary` / `<Library>1</Library>` and an in-memory `LoadedFromLibrary` flag. See [§2.9](#29-library-flag). Header fields like `Title` / `Author` from the `.amf` are **not** applied to the adventure (except library-only metadata such as Babel `Description`). Maps and `<Exclude>` lists from libraries are skipped; exclusions come from the **adventure**.

### 1.4 Save games (`.tas`)

Root element: **`<Game>`** (not `<Adventure>`). Contains runtime deltas only. See [§7 Save-game XML](#7-save-game-xml-game).

---

## 2. General conventions

### 2.1 No attributes

ADRIFT-5 almost never uses XML attributes. Structure is **nested elements with text content**.

### 2.2 Booleans

Usually `"1"` / `"0"`. The `ADRIFT-5` writer normally **omits** default-false flags; readers treat absence as the default. Presence of `"1"` means true. The loader's boolean parse accepts `-1` / `1` / `TRUE` / `VRAI` as true (`VRAI` is a French-locale artifact of VB serialization); anything else is false.

### 2.3 Enumerations

Written as Visual Basic enum member names (PascalCase), e.g. `North`, `Specific`, `Immediately`, `ContinueAlways`, `HighestPriorityPassingTask`.

### 2.4 Dates

`yyyy-MM-dd HH:mm:ss` in `LastUpdated` / `Created`. No timezone is included; these datetimes are in whatever timezone the `ADRIFT-5` Generator app was using at the time the XML was created.

### 2.5 Version string

Version strings are used for upgrade paths when loading older files.

It's formatted as a string like `5.0000366` in this format: `Major.MMMBBBR`

- Major: Always 5 for ADRIFT 5
- Minor (MMM): Minor version number, zero-padded to 3 digits
  - This has always been `000` in practice
- Build (BBB): Build number, zero-padded to 3 digits
  - The latest build number as of July 2026 is `036`
- Revision (R): The revision number
  - The latest revision number as of July 2026 is `6`
  - The revision digit is only present in newer builds — older files use the 6-digit form (e.g. `5.000020`)

Thus, the latest version string is `5.0000366`. Readers compare version strings numerically (parsed as a double), so both forms order correctly.

### 2.6 Colours (`BackgroundColour`, `InputColour`, `OutputColour`, `LinkColour`)

Colors are stored in a counterintuitive format, as Windows OLE (`COLORREF`) color integers.

OLE stores colors as three bytes of data, but unlike almost all other color formats, OLE stores the bytes in the order Blue, Green, and then Red, instead of the more common Red, Green, Blue.

For example, in OLE, pure Red would be `0x0000FF`, which would be `#FF0000` almost anywhere else.

But then, for a double whammy, Adventure XML converts those three Blue, Green, Red and bytes into an integer, and stores it as a decimal string.

For example, cyan is 100% Blue, 100% Green, 0% Red: `0xFFFF00` in OLE order is 16,776,960, so you'd see it written in Adventure XML as `<OutputColour>16776960</OutputColour>`. (The actual Runner default output colour, for reference, is the teal RGB 25,165,138 — `DEFAULT_OUTPUTCOLOUR` — and colour elements are only written when they differ from the defaults.)

### 2.7 Omitted defaults

`dev500.exe` skips many default values (e.g. font Arial/12, default direction regexes, `Aggregate` when true). Parsers must supply the same defaults.

### 2.8 Flat item list

Items are **siblings under `<Adventure>`**, not nested under `<Locations>` / `<Objects>` wrappers. Element names are singular: `Location`, `Object`, `Task`, …

### 2.9 Library flag

```xml
<Library>1</Library>
```

Written on an item (Location, Object, Task, Property, …) when that item is treated as coming from a **library module** (`.amf`). Omitted when false.

**Meaning**

| Context | Behaviour |
|---------|-----------|
| Item appears only inside an `.amf` | Generators stamp every item library-owned on load; writers emit `<Library>1</Library>` when saving the module |
| Item was merged into an adventure from an `.amf` and not customised | Adventure XML keeps `<Library>1</Library>` so later library updates know it is still a stock library copy |
| Author edits the item in the Generator | Next save should omit `<Library>` (the item is now part of the adventure). But there's a bug in `<Group>` `<Member>`s, see below. |
| Runner loading a `.taf` / adventure | Flag is loaded but does not change play rules; it is Generator / merge bookkeeping |

Bug: When adding `<Member>` elements to `<Group>`s provided by a library, the Generator does _not_ remove `<Library>1</Library>` from the group.

**Merge / exclude**

- Adventure-level [`<Exclude>`](#614-exclude) keys: do not load matching library items when merging.
- Duplicate keys: library load may update, skip, or keep both depending on Generator overwrite settings and whether the existing item still has `<Library>1</Library>`.
- Tasks loaded from a library get a large priority bump so they sort after author tasks ([§6.5](#65-task)).
- Task [`ReplaceTask`](#65-task) can force a library task to replace an existing key.

### 2.10 Expressions (overview)

An **expression** is a small runtime formula language (arithmetic, comparisons, string ops, builtins like `IF` / `RAND`). It is not its own XML element type — it shows up as text inside other fields.

You will mainly spot expressions as:

- `<# … #>` inside output / description text
- `'…'` (single-quoted) as a Variable restriction value
- the body of an `Expression` restriction
- the right-hand side of `SetVariable` / `IncVariable` / `DecVariable`

Full operators, builtins, and embedding rules: [§4.3 Expressions](#43-expressions).

### 2.11 `FromTo` Inclusive Ranges

A turn/count range written as a single integer or an inclusive random range:

```text
N
N to M
```

Examples: `3`, `1 to 2`, `0 to 5`. When the runner needs a concrete value, it picks uniformly from `N`…`M` inclusive (or just `N` if no `to` clause).

Used in event `Length` / `StartDelay` / SubEvent timing, walk step durations, and similar duration fields.

### 2.12 `Control`

Task-triggered start/stop lines on events and walks:

```text
Start|Stop|Suspend|Resume  Completion|UnCompletion  TaskKey
```

Example: `Start Completion TaskBeginStorm` — when `TaskBeginStorm` completes, start this event/walk. `UnCompletion` fires when that (completed) task is unset by a `SetTasks Unset` **action**; an event's/walk's own `UnsetTask` sub-action only clears the completed flag and fires no controls. See [§6.6 Event](#66-event) and [§6.7 Character](#67-character) (walks).

### 2.13 Inline HTML-like tags

Adventure text (description `<Text>`, completion messages, ALRs, etc.) may embed a small **HTML-like** dialect. These are **not** Adventure XML elements — they are author markup inside string content after expressions and substitutions.

Tags are case-insensitive. Unknown tags are usually ignored (unless an ALR rewrites the whole string). They may nest with style tags; pairing is last-in/first-out by tag name.

#### Style and layout

| Tag | Meaning |
|-----|---------|
| `<b>…</b>` | Bold |
| `<i>…</i>` | Italic |
| `<u>…</u>` | Underline |
| `<c>…</c>` | Colour text using the adventure **input** colour (`InputColour`) |
| `<center>…</center>` or `<centre>…</centre>` | Centre-align |
| `<left>…</left>` | Left-align |
| `<right>…</right>` | Right-align |
| `<font …>…</font>` | Face / size / colour (attributes below) |
| `<br>` | Line break (void) |

**`<font>` attributes** (any combination):

- `color="#RRGGBB"` or `color="RRGGBB"` — text colour (note: RGB _not_ OLE BGR)
- `face="Font Name"` — typeface
- `size="N"` — absolute point size (ADRIFT treats body default as 12pt)
- `size="+N"` / `size="-N"` — relative to the current font size

Example: `<font face="Courier New" size=14 color="#00FFFF">mono cyan</font>`

#### Side-effect tags (void / self-contained)

| Tag | Meaning |
|-----|---------|
| `<cls>` | Clear the main output window |
| `<del>` | Delete the previous character of output (backspace one glyph) |
| `<wait N>` | Pause **N** seconds (N may be fractional), then continue |
| `<waitkey>` | Pause until the player presses a key |
| `<img src="path">` | Show an image. `src` is a filename/path as in the adventure (resolved via [§6.16 FileMappings](#616-filemappings-blorb-export) inside a Blorb, else as an absolute path to a file on disk). Official Runner often paints the Graphics pane / pastes into the transcript |
| `<audio play src="…" [channel=N] [loop=Y\|N]>` | Start sound on channel 1–8 (default 1) |
| `<audio pause [channel=N]>` / `<audio stop [channel=N]>` | Pause or stop that channel |
| `<window Name>` | Official Windows Runner only: open/route following text into a named dockable output pane |

Channels outside 1–8 are ignored. `loop=Y` repeats playback.

#### Character entities

Literal special characters in output text:

| Entity | Character |
|--------|-----------|
| `&lt;` | `<` |
| `&gt;` | `>` |
| `&amp;` | `&` |
| `&perc;` | `%` |
| `&quot;` | `"` |

`&perc;` is used so percent-functions and `%Player%`-style tokens are not re-parsed after formatting.

#### Ordering with expressions

Within a description string, substitution typically expands `%…%` / `<#…#>` **before** HTML tags are applied. Authors can therefore generate markup from expressions, e.g. `<# IF(…) #>` emitting `<b>…</b>`.

---

## 3. Adventure shape

Under `<Adventure>`, children fall into two groups:

1. **Header** fields — title, colours, introduction, runner settings, and so on. Full list in [§5 Adventure header](#5-adventure-header).
2. **Items** — a flat sibling list of game entities (not nested under `<Locations>` / `<Objects>` wrappers). Full schemas in [§6 Adventure item types](#6-adventure-item-types).

Item element names (singular):

| Element | Role |
|---------|------|
| [`Folder`](#61-folder-generator-ui-only) | Generator UI folders only; no effect at runtime |
| [`Location`](#63-location) | Rooms |
| [`Object`](#64-object) | Objects players can interact with |
| [`Property`](#62-property-definition) | Defines properties of `<Object>`s |
| [`Task`](#65-task) | Commands / system tasks |
| [`Event`](#66-event) | Timed / turn events |
| [`Character`](#67-character) | Player and NPCs |
| [`Variable`](#68-variable) | Numeric / text variables |
| [`Group`](#69-group) | Groups of locations, objects, or characters |
| [`TextOverride`](#610-textoverride-alr) | ALRs (output text replacements) |
| [`Hint`](#611-hint) | Hint system (never implemented at runtime) |
| [`Synonym`](#612-synonym) | Command synonyms |
| [`Function`](#613-function-user-defined) | User-defined functions |
| [`Exclude`](#614-exclude) | Keys skipped on library merge |
| [`Map`](#615-map) | Automap layout |
| [`FileMappings`](#616-filemappings-blorb-export) | Blorb resource id ↔ filename |

Shared nesting used across header fields and items (defined next in [§4](#4-shared-structures)):

- Almost every item has a `<Key>` ([§4.1](#41-keys-ids)); the structural exceptions are `Exclude`, `Map`, and `FileMappings`, which are not keyed items.
- Many text fields are **description blocks** ([§4.2](#42-description-blocks)).
- Text and logic can embed **expressions** ([§2.10](#210-expressions-overview), [§4.3](#43-expressions)), **percent-functions** ([§4.6](#46-percent-functions-and-special-tokens)), and [HTML-like tags](#213-inline-html-like-tags).
- Tasks, topics, hints, and location movements carry **restrictions** ([§4.4](#44-restrictions)) and often **actions** ([§4.5](#45-actions)).

```xml
<Adventure>
  <!-- header: Title, Introduction, … -->
  <Location>
    <Key>LocationStart</Key>
    <LongDescription>…</LongDescription>   <!-- description block -->
    <Movement>
      <Direction>North</Direction>
      <Destination>LocationHall</Destination>
      <Restrictions>…</Restrictions>
    </Movement>
  </Location>
  <Task>
    <Key>TaskTake</Key>
    <Command>take {the} %object%</Command>
    <CompletionMessage>…</CompletionMessage>  <!-- description block -->
    <Restrictions>…</Restrictions>
    <Actions>…</Actions>
  </Task>
</Adventure>
```

---

## 4. Shared structures

These forms recur inside header fields and item elements. This section defines them fully so later sections can assume the helpers exist.

### 4.1 Keys (IDs)

Every adventure item has a unique `<Key>` string.

Examples: `Location1`, `Object2`, `Task15`, `Character3`, `Variable1`, `Group1`, `Property1`, `Event1`, `Hint1`, `Synonym1`, `Folder1`.

Keys are normally generated by `dev500.exe` like this: `[KeyPrefix][TypeName][N]`. (The `<KeyPrefix>` is an adventure header element; it's empty by default.)

Newer Generator “nice key” mode builds PascalCase names from the item’s common name (truncated). Libraries and authored games freely use semantic keys: `Player`, `StaticOrDynamic`, `SynLook`, `FunEcho`, `AllLocations`.

#### 4.1.1 Runtime reference tokens

A **runtime reference token** is a key-shaped placeholder that stands in for a value bound when the player’s command (or a nested task call) is matched. It is not the `<Key>` of any adventure item; the runner resolves it to a real key, direction, number, or text for the current turn.

Commands use **percent-tokens**. Restrictions and actions usually use the **expanded** forms. Same binding either way. Bare `%object%` is treated like `%object1%` (and likewise for the other types); both expand to the unnumbered `Referenced…` form.

##### Tokens by type

| Type | Percent-tokens (commands) | Expanded forms (restrictions / actions) |
|------|---------------------------|-----------------------------------------|
| Object | `%object%`, `%object1%`…`%object5%`, `%objects%` | `ReferencedObject`, `ReferencedObject1`…`ReferencedObject5`, `ReferencedObjects` |
| Character | `%character%`, `%character1%`…`%character5%`, `%characters%` | `ReferencedCharacter`, `ReferencedCharacter1`…`ReferencedCharacter5`, `ReferencedCharacters` |
| Location | `%location%`, `%location1%`…`%location5%` | `ReferencedLocation`, `ReferencedLocation1`…`ReferencedLocation5` |
| Direction | `%direction%`, `%direction1%`…`%direction5%` | `ReferencedDirection`, `ReferencedDirection1`…`ReferencedDirection5` |
| Number | `%number%`, `%number1%`…`%number5%` | `ReferencedNumber`, `ReferencedNumber1`…`ReferencedNumber5` |
| Text | `%text%`, `%text1%`…`%text5%` | `ReferencedText`, `ReferencedText1`…`ReferencedText5` |
| Item (object or character) | `%item%`, `%item1%`…`%item5%` | `ReferencedItem`, `ReferencedItem1`…`ReferencedItem5` |

There is no `%locations%` / `%directions%` / `%numbers%` / `%texts%` / `%items%` in `ReferenceNames()`. Plurals exist only for objects and characters.

Mapping: `%object%` / `%object1%` → `ReferencedObject` / `ReferencedObject1`; `%objects%` → `ReferencedObjects`; and so on for the other types (`%character%` → `ReferencedCharacter`, …).

##### Special

| Token | Meaning |
|-------|---------|
| `%Player%` | The current player character (not a command capture). Context-dependent: in restrictions/actions/paths it resolves to the player's **key**; in plain display text the `%Player%` percent-function prints the player's **name**. |

#### 4.1.2 Key collision on library merge

When merging `.amf` libraries into an adventure, a library item whose key already exists is handled by Generator overwrite policy ([§2.9](#29-library-flag)): update the existing library item, keep a customised adventure item, or keep both (renaming — trailing digits stripped/incremented until unique).

Keys listed in [`<Exclude>`](#614-exclude) are skipped entirely during merge.

### 4.2 Description blocks

Many text fields are **not** plain strings. They use a container element holding one or more `<Description>` child elements:

```xml
<LongDescription>
  <Description>
    <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
    <Text>A dusty attic.</Text>
  </Description>
  <Description>
    <Restrictions>…</Restrictions>          <!-- optional -->
    <DisplayWhen>AppendToPreviousDescription</DisplayWhen>
    <Text> A hatch in the floor stands open.</Text>
    <DisplayOnce>1</DisplayOnce>             <!-- optional -->
    <ReturnToDefault>1</ReturnToDefault>     <!-- optional -->
    <TabLabel>Hatch open</TabLabel>          <!-- optional; Generator UI label -->
  </Description>
</LongDescription>
```

#### 4.2.1 `DisplayWhen`

| Value | Meaning |
|-------|---------|
| `StartDescriptionWithThis` | Replace / start with this description |
| `AppendToPreviousDescription` | Append to previous output |
| `StartAfterDefaultDescription` | After the default description |

#### 4.2.2 Common containers using description blocks

- Header elements: `Introduction`, `EndGameText`
- Location: `ShortDescription`, `LongDescription`
- Object / character: `Description`
- Task: `CompletionMessage`, `FailOverride`
- Property (text type): `Value`
- ALR (`TextOverride`): `NewText`
- Hint: `Subtle`, `Sledgehammer`
- Character topic: `Description`
- User-defined function: `Output`
- Restriction: `Message`
- Event / walk activity: `Action` (when displaying a message)

Description `<Text>` may contain inline expressions (`<# … #>`; see [§4.3](#43-expressions)), [percent-functions](#46-percent-functions-and-special-tokens) (`%PopUpChoice[…]%`, `%Turns%`, …), and [HTML-like tags](#213-inline-html-like-tags) (`<b>`, `<c>`, `<br>`, …).

**Legacy:** Files older than ~5.000015 may store short descriptions as plain text. Modern writers always use nested blocks.

### 4.3 Expressions

Expressions use a small expression language evaluated at runtime. They are **not** a separate XML element type; they appear as text payloads inside other forms.

#### Where expressions appear

| Context | How the expression is marked |
|---------|------------------------------|
| Output / description text | `<# … #>` inline (evaluated during text substitution) |
| Variable restriction value | `'…'` single quotes around the expression |
| Restriction type `Expression` | Whole body is a boolean expression (optional surrounding `<#…#>` is stripped) |
| `SetVariable` / `IncVariable` / `DecVariable` | RHS, often written as `"…"` in the action payload |
| `Time` action | Turns count in `Skip "N" turns` may be an expression |
| Array indices, UDF args, `UserStatus`, etc. | Bare or `%…%`-containing expression text |

#### Language outline

After [percent-functions](#46-percent-functions-and-special-tokens) / `%variables%` / reference tokens are expanded, the evaluator tokenizes and resolves:

**Arithmetic:** `+` `-` `*` `/` `^` `mod`, parentheses. Integer division **rounds to nearest, halves away from zero** (`Math.Round(a/b, MidpointRounding.AwayFromZero)`): `-5/2` = `-3`. Note this differs from ADRIFT 4, whose epsilon-biased rounding gives `-5/2` = `-2` — a portability trap when converting games.

**Comparisons:** symbolic only — `=` / `==`, `<>` / `!=`, `<`, `<=`, `>`, `>=`. The word forms (`EqualTo`, `GreaterThan`, …) belong to the *restriction* language, not expressions; a bare word like `EQ` inside an expression parses as a string literal, not an operator.

**Logic:** `AND` `OR`

**Strings:** `"…"` literals; `&` concatenation

**Booleans:** `True` / `False` (case-insensitive) as whole expressions

**Selected builtins** (names are case-insensitive):

| Kind | Examples |
|------|----------|
| Conditionals / choice | `IF(cond, then, else)`, `EITHER(a, b)`, `ONEOF(a, b, …)` |
| Random | `RAND(lo, hi)` / `RAND(n)` — with replacement; `URAND(…)` — unique (no-repeat) |
| Numeric | `ABS(n)`, `MIN(a, b)`, `MAX(a, b)`, `VAL(s)`, `STR(n)` |
| String | `LEN(s)`, `LEFT`/`LFT`, `RIGHT`/`RGT`, `MID`, `INSTR`, `REPLACE` |
| Case | `UPPER`/`UCASE`/`UPR`, `LOWER`/`LCASE`/`LWR`, `PROPER`/`PCASE`/`PPR` |

**Variables** may appear by name or via `%VarName%` / `%VarName[index]%` (index may itself be an expression). Runtime tokens such as `%text%` / `%number%` are substituted first; in expression mode, text refs are typically auto-quoted.

`RAND` vs `URAND`: both take `RAND(n)` → `0..n` or `RAND(lo, hi)` → inclusive range. **`RAND`** draws independently each time (duplicates allowed). **`URAND`** (“unique rand”) draws without replacement from that range until every value has been used, then reshuffles and starts over (per min–max pair, adventure-wide).

Examples:

```text
<# %Score% + 1 #>
<# IF(%Player%.Held.Count = 0, "empty-handed", "carrying something") #>
VariableTurns Must BeEqualTo 'VariableScore * 2'
SetVariable: VariableFlag = IF(%object% = "ObjectKey", 1, 0)
```

This is an outline, not a full grammar: edge cases (short-circuiting, type coercion, bad expressions) follow ADRIFT-5 `SetToExpression`.

#### Property paths

Dot-paths navigate from an item (or list of items) to related items, lists, counts, names, or [instance properties](#62-property-definition).

**Shape**

```text
Root.Segment.Segment…
Root.Method(args).Segment…
```

- **Root** — an item key (`Player`, `ObjectHat`, `LocationLab`, `CharacterBob`), a runtime ref after expansion (`%Player%` → key, `%object1%` → key or `KeyA|KeyB`), or a pipe-separated multi-key list.
- **Segments** — built-ins below, or an adventure **property key** (e.g. `OpenStatus`, `Weight`).
- **`(args)`** — optional method filters / list options (comma-separated keywords).
- Intermediate steps that produce **lists** can continue with `.Count`, `.List` / `.Name`, `.Parent`, `.Children…`, or further property filters.

Bare keys without `%…%` also work in text that goes through `ReplaceFunctions` (e.g. `LocationLab.Exits.Count`).

**Built-ins by root kind**

| On | Segment | Result |
|----|---------|--------|
| Any item / list | `Count` | Integer size (`1` for a single item); common after lists |
| Any list | `List` / `Name` | Human-readable names of the items (see below) |
| Any list | `Sum` | Sum of a prior integer/value-list property step |
| Any list | `Description` | Joined long descriptions / location views |
| Any list | `Parent` | Immediate parents (container / holder / room) |
| Object | `Children(…)` | Contained items. Recognized args (space-stripped, lowercased): empty/`all`/`onandin`/`all,onandin` (everything), `in`, `objects`, `characters`, and `objects,in` / `objects,on` / `objects,onandin` / `characters,in` / `characters,on` / `characters,onandin`. **Bare `on` is not recognized** — an unrecognized arg combination yields the empty set (the runner's `Select Case` falls through) |
| Object | `Contents(…)` | Contents **inside** the object only (never on it). Args: empty/`all`, `objects`, `characters`. Objects only — locations have no `Contents` |
| Object | `Objects` | Objects related to this object (children context) |
| Character | `Held` / `Worn` / `WornAndHeld` | Inventory subsets. `Held(true)` (default) vs `Held(false)` — include/exclude nested carried containers as FrankenDrift does |
| Character | `Location` | Character’s **room** (walks through on/in to ultimate location) |
| Character | `Exits` | Directions **that character** can currently take from its room (route restrictions are checked against that character, not the player) |
| Character | `Descriptor` / `Description` / `Name(…)` / `Parent` | Naming and parent |
| Location | `Name` | Short description |
| Location | `Description` | View / long text |
| Location | `Objects` / `Characters` | Items in the room (no `Contents` on locations) |
| Location | `Exits` | Directions with a destination set (not necessarily currently passable) |
| Location | `LocationTo(South)` / `LocationTo(South\|East)` | Neighbouring location(s) by direction name(s) |
| Event | `Length` / `Position` | Event length / current position within it (when rooted on an event) |

If a path **stops on a list** with no further segment (for example `%Player%.Held` rather than `%Player%.Held.Count` or `%Player%.Held.List`), the result is the item keys joined with `|`, e.g. `ObjectLamp|ObjectKey`. That string can feed another path or comparison that expects keys.

**`List` and `Name`:** both turn a collection into printed text (empty → `"nothing"`). Multiple items become English lists such as `the lamp, the key and the box`, or direction names such as `north, south and west`.

Optional args in parentheses, comma-separated, may mix any of these:

| Concern | Keywords | Effect |
|---------|----------|--------|
| Joiner | *(default)* / `or` / `rows` | `" and "` between the last two items; `" or "` instead; or one item per line |
| Article (objects) | *(default)* / `indefinite` / `none` | `the lamp` vs `a lamp` vs `lamp` |
| Article (characters) | *(default)* / `definite` | Characters default to **indefinite** (`a tall man`); `definite` → `the tall man` |
| Pronouns (characters) | `force`, `objective`, `possessive`, `reflective`, `none` | Pronoun form when listing characters |
| Nested contents | *(default for `List`)* / `false` / `0` | In the desktop Runner, `List` may append what’s in/on each object (e.g. `the box.  On the box is a coin.`); `Name` never does; `false`/`0` disables nesting on `List`. (Reimplementations render the flat list; treat the nested append as Runner-specific display behavior rather than a guaranteed value) |

Examples: `%Player%.Held.List` → `the lamp and the key`; `%Player%.Held.List(Indefinite, False)` → `a lamp and a key` with no in/on append; `%objects%.Name(Indefinite)` → names only, indefinite articles.

**Adventure properties as segments:** any property key defined for that item type becomes a path step. Integer / value-list / state / selection-only properties filter or expose values; key-typed properties (`ObjectKey`, `LocationKey`, …) replace the current list with the referenced item(s). Optional `(value)` filters (e.g. `OpenStatus(Open)`).

**Examples**

```text
%Player%.Held.Count
%Player%.Held.List(Indefinite, False)
%Player%.Location.Name
%Player%.Location.Exits.Count
ObjectBox.Children(objects).Count
ObjectGem.Parent.Name
LocationLab.LocationTo(North).Name
CharacterBob.WornAndHeld.Count
%objects%.Name(Indefinite)
LocationLab.Objects.Weight.Sum          # if Weight is an Integer property
```

Multi-key roots share one path: `LocationLab|LocationCloset.Objects.Count` sums/lists across both.

See also [§4.6](#46-percent-functions-and-special-tokens) (`%PropertyValue[…]%` and friends) for non-dot helpers that overlap this space.

### 4.4 Restrictions

Shared by tasks, topics, hints, and location movements.

```xml
<Restrictions>
  <Restriction>
    <Object>ObjectHat Must BeWornByCharacter CharacterBob</Object>
    <Message>
      <Description>
        <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
        <Text>Bob isn't wearing the hat.</Text>
      </Description>
    </Message>
  </Restriction>
  <Restriction>
    <Task>TaskOpenDoor Must BeComplete</Task>
  </Restriction>
  <BracketSequence>#A#</BracketSequence>
</Restrictions>
```

The **first child element name** in the `<Restriction>` element is the restriction type. Body is whitespace-separated tokens with `Must` / `MustNot`.

| Tag | Pattern (conceptual) |
|-----|----------------------|
| `Location` | `key1 Must\|MustNot LocationOp key2` |
| `Object` | `key1 Must\|MustNot ObjectOp key2` |
| `Character` | `key1 Must\|MustNot CharacterOp key2` |
| `Item` | `key1 Must\|MustNot ItemOp key2` |
| `Task` | `taskKey Must\|MustNot BeComplete` |
| `Variable` | `varKey[index]? Must\|MustNot BeEqualTo\|… value` |
| `Property` | `propKey itemKey Must\|MustNot [EqualTo…] value` — note the **property key comes first**, then the item reference |
| `Direction` | `Must\|MustNot BeNorth` (etc.; direction after `Be`) |
| `Expression` | boolean [expression](#43-expressions) |

**Location ops:** `BeInGroup`, `HaveBeenSeenByCharacter`, `HaveProperty`, `BeLocation`, `Exist`

**Object ops:** `BeAtLocation`, `BeHeldByCharacter`, `BeInGroup`, `BeInsideObject`, `BeInState`, `BeOnObject`, `BePartOfCharacter`, `BePartOfObject`, `BeVisibleToCharacter`, `BeWornByCharacter`, `Exist`, `HaveBeenSeenByCharacter`, `HaveProperty`, `BeExactText`, `BeHidden`, `BeObject`

**Character ops:** `BeAlone`, `BeAloneWith`, `BeAtLocation`, `BeCharacter`, `BeInConversationWith`, `Exist`, `HaveRouteInDirection`, `HaveSeenCharacter`, `HaveSeenLocation`, `HaveSeenObject`, `BeHoldingObject`, `BeInSameLocationAsCharacter`, `BeInSameLocationAsObject`, `BeLyingOnObject`, `BeInGroup`, `BeOfGender`, `BeSittingOnObject`, `BeStandingOnObject`, `BeWearingObject`, `BeWithinLocationGroup`, `HaveProperty`, `BeInPosition`, `BeInsideObject`, `BeOnObject`, `BeOnCharacter`, `BeVisibleToCharacter`

**Variable ops** (written after `Be`): `EqualTo`, `GreaterThan`, `GreaterThanOrEqualTo`, `LessThan`, `LessThanOrEqualTo`, `Contain`

**Variable values:**

- Bare integer → numeric constant
- `"quoted"` → string constant
- `'expr'` → [expression](#43-expressions) (single quotes mark “evaluate this”)
- Bare key → another variable’s value
- Array index: `Variable1[%IndexVar%]` or `Variable1[2]`

#### 4.4.1 `BracketSequence`

`<BracketSequence>` says how the sibling `<Restriction>` elements combine. It is a compact boolean formula over those restrictions, in list order.

| Symbol | Meaning |
|--------|---------|
| `#` | The next unused restriction (left to right among `<Restriction>` siblings) |
| `A` | AND |
| `O` | OR |
| `(` `)` | Grouping |

Square brackets `[` `]` may appear in older / Generator UI forms; loaders rewrite `[`→`((` and `]`→`))`.

Each `#` consumes one restriction in document order (a cursor advances through the `<Restriction>` list as `#`s are evaluated).

**Too few `#`s:** Restrictions with no matching `#` in the sequence are **never evaluated** — they do not affect the result. For example, with three restrictions and `<BracketSequence>#A#</BracketSequence>`, only R0 AND R1 matter; R2 is ignored. Short-circuiting is similar: if an early `A` fails (or an early `O` succeeds), remaining `#`s in the unread tail are skipped without evaluating those restrictions.

**Too many `#`s:** An out-of-range `#` throws internally, but the Runner catches the exception in `GetGeneralTask` and reports "I didn't understand that command" — the command fails soft; the app does not crash. Still, do not author sequences with more `#`s than restrictions.

**Malformed sequences:** Things like consecutive hashes (`#A#A##`) or other junk between `#` slots fail the whole list (treated as false).

Generator normally writes one `#` per restriction, joined with `A`/`O` and parentheses as needed. An empty `BracketSequence` with restrictions present is unusual; prefer an explicit sequence.

**5.0.26 upgrade path (`CorrectBracketSequence`):** ADRIFT 5.0.26 changed how mixed sequences bind, and when loading a file older than 5.0.26 the loader offers to *rewrite* stored sequences: the longest run of the form `#A#O#O#…` becomes `#A(#O#O#…)`. A spec-compliant reader of pre-5.0.26 files should be aware the on-disk sequence may predate this correction (the desktop Runner asks the user via an "Adventure Upgrade" dialog before mutating).

Examples (three restrictions R0, R1, R2):

| Sequence | Meaning |
|----------|---------|
| `#` | Only evaluate the first restriction |
| `#A#` | R0 AND R1 (as in the example above) |
| `#O#` | R0 OR R1 |
| `#A#A#` | R0 AND R1 AND R2 |
| `#A(#O#)` | R0 AND (R1 OR R2) |
| `(#O#)A#` | (R0 OR R1) AND R2 |

**AND / OR associativity:** Evaluation rewrites chains like `#A#O#` into `(#A#)O#` before combining, so AND is applied before OR when they are mixed without parentheses (`#A#A#O#` → `(#A#A#)O#`). Prefer explicit parentheses for clarity.

### 4.5 Actions

```xml
<Actions>
  <MoveObject>Object Hat ToCarriedBy Player</MoveObject>
  <SetProperty>Hat WornByWho CharacterBob</SetProperty>
  <SetTasks>Execute TaskOpenDoor</SetTasks>
  <SetVariable>VariableScore = "10"</SetVariable>
  <EndGame>Win</EndGame>
</Actions>
```

The **tag name is the action type**; text is the payload.

| Tag | Payload summary |
|-----|-----------------|
| `MoveObject` / `AddObjectToGroup` / `RemoveObjectFromGroup` | see [§4.5.1](#451-moveobject-and-object-groups) |
| `MoveCharacter` / `AddCharacterToGroup` / `RemoveCharacterFromGroup` | see [§4.5.2](#452-movecharacter-and-character-groups) |
| `AddLocationToGroup` / `RemoveLocationFromGroup` | see [§4.5.3](#453-location-groups) |
| `SetProperty` | `itemKey propKey value…` |
| `SetTasks` | `Execute\|Unset\|Clear taskKey (params)?` or FOR-loop form (`Clear` is a synonym for `Unset`) |
| `SetVariable` / `IncVariable` / `DecVariable` | `key[index]? = "expr"` or FOR-loop form ([§4.3](#43-expressions)) |
| `Conversation` | `Greet\|Ask\|Tell\|Say\|Farewell\|EnterWith\|LeaveWith …` |
| `Time` | `Skip "N" turns` |
| `EndGame` | `Win` \| `Lose` \| `Neutral` |

(There are no `WalkTo` or `Score` action types — the Runner's action enum (`clsTask.vb ItemEnum`) contains only the tags above. `WalkTo` exists only as a runtime player field driving map-click auto-walk, never as an authored `<Actions>` child.)

Keys in these payloads are adventure keys or runtime placeholders (`ReferencedObject`, `Player`, `%Player%`, …).

#### 4.5.1 MoveObject and object groups

```xml
<MoveObject>Object Hat ToCarriedBy Player</MoveObject>
<MoveObject>EverythingHeldBy Player ToLocation LocationYard</MoveObject>
<AddObjectToGroup>Object Hat ToGroup GroupInventoryItems</AddObjectToGroup>
<RemoveObjectFromGroup>Object Hat FromGroup GroupInventoryItems</RemoveObjectFromGroup>
```

`ObjectSelector key1 [prop] Destination key2`

- `ObjectSelector` — which object(s) to act on: `Object`, `EverythingHeldBy`, `EverythingWornBy`, `EverythingInside`, `EverythingOn`, `EverythingWithProperty`, `EverythingInGroup`, `EverythingAtLocation`
- `key1` — object key when selector is `Object`; otherwise the scope (holder, container, group, location, or property key)
- `prop` — optional property value; only when selector is `EverythingWithProperty`
- `Destination` — where they go: `InsideObject`, `OntoObject`, `ToCarriedBy`, `ToLocation`, `ToLocationGroup`, `ToPartOfCharacter`, `ToPartOfObject`, `ToSameLocationAs`, `ToWornBy`, `ToGroup`, `FromGroup` (`AddObjectToGroup` / `RemoveObjectFromGroup` fix this to `ToGroup` / `FromGroup`)
- `key2` — destination / group key

#### 4.5.2 MoveCharacter and character groups

```xml
<MoveCharacter>Character Player ToLocation LocationYard</MoveCharacter>
<MoveCharacter>Character Player ToParentLocation </MoveCharacter>
<MoveCharacter>EveryoneAtLocation LocationYard ToLocation LocationHall</MoveCharacter>
<AddCharacterToGroup>Character CharacterBob ToGroup GroupGuests</AddCharacterToGroup>
<RemoveCharacterFromGroup>Character CharacterBob FromGroup GroupGuests</RemoveCharacterFromGroup>
```

`CharacterSelector key1 [prop] Destination key2`

- `CharacterSelector` — which character(s) to act on: `Character`, `EveryoneInside`, `EveryoneOn`, `EveryoneWithProperty`, `EveryoneInGroup`, `EveryoneAtLocation`
- `key1` — character key when selector is `Character`; otherwise the scope
- `prop` — optional; only for `EveryoneWithProperty`
- `Destination` — where they go: `InDirection`, `ToLocation`, `ToLocationGroup`, `ToLyingOn`, `ToSameLocationAs`, `ToSittingOn`, `ToStandingOn`, `ToSwitchWith`, `InsideObject`, `OntoCharacter`, `ToParentLocation`, `ToGroup`, `FromGroup` (`AddCharacterToGroup` / `RemoveCharacterFromGroup` fix this to `ToGroup` / `FromGroup`)
- `key2` — destination / group key; omitted when Destination is `ToParentLocation`

#### 4.5.3 Location groups

```xml
<AddLocationToGroup>Location LocationYard ToGroup GroupRooms</AddLocationToGroup>
<AddLocationToGroup>LocationOf CharacterBob ToGroup GroupRooms</AddLocationToGroup>
<RemoveLocationFromGroup>EverywhereInGroup GroupRooms FromGroup GroupRooms</RemoveLocationFromGroup>
```

`LocationSelector key1 [prop] ToGroup|FromGroup key2`

- `LocationSelector` — which location(s) to act on: `Location`, `LocationOf`, `EverywhereInGroup`, `EverywhereWithProperty`
- `key1` — location key when selector is `Location`; otherwise the scope (character for `LocationOf`, group for `EverywhereInGroup`, property key for `EverywhereWithProperty`)
- `prop` — optional; only for `EverywhereWithProperty`
- `ToGroup` / `FromGroup` — add vs remove (matches the tag)
- `key2` — group key

### 4.6 Percent-functions and special tokens

Besides [command reference tokens](#411-runtime-reference-tokens) (`%object%`, …) and [variable](#68-variable) / [expression](#43-expressions) forms (`%Score%`, `<# … #>`), ADRIFT expands **percent-functions** during text substitution:

```text
%FunctionName%
%FunctionName[arg1, arg2, …]%
```

Names are case-insensitive. Arguments are comma-separated; nested `%…%` inside args is expanded first. These run in the Runner’s replace pipeline (before or with expressions, depending on the call site) and can appear in description text, action RHS strings, restriction messages, ALR `NewText`, etc.

Also related, but separate:

| Form | Role |
|------|------|
| `%Player%`, `%object1%`, … | Keys / refs ([§4.1.1](#411-runtime-reference-tokens)) |
| `%VarName%` / `%VarName[i]%` | Variable values |
| `%Player%.Held.Count`, `%object%.Name`, … | [Property paths](#property-paths) |
| `<# … #>` | Expression evaluation ([§4.3](#43-expressions)) |
| ALR / [`TextOverride`](#610-textoverride-alr) | Author string→string replacements after display text is built |
| [HTML-like tags](#213-inline-html-like-tags) | Output formatting, not `%…%` |

#### Interactive pop-ups

These **block** for player input. `run500.exe` displays a modal dialog. (It seems OK to just use a special prompt instead.) Typical use is inside a `SetVariable` action:

```xml
<SetVariable>ChoiceResult = %PopUpChoice["Pick a colour", "red", "blue"]%</SetVariable>
<SetVariable>InputResult = %PopUpInput["Please enter your name", "Anonymous"]%</SetVariable>
```

| Function | Arguments | Result |
|----------|-----------|--------|
| `%PopUpChoice[prompt, choiceYes, choiceNo]%` | Exactly three (often quoted strings) | Yes → `choiceYes`, No → `choiceNo`. Dialog text is `prompt` plus “Yes for …, No for …” |
| `%PopUpInput[prompt]%` | Prompt only | Player’s typed string, wrapped in `"…"` for expression/display use |
| `%PopUpInput[prompt, default]%` | Prompt + default | Same; empty input keeps `default`. Always returned as `"…"` |

Arguments may themselves be expressions / nested functions; Runner evaluates them when building the dialog. Do not rely on pop-ups in pure output text unless you intend to interrupt mid-message — store the result in a variable, then print `%ChoiceResult%`.

#### Session / environment tokens

| Token | Expansion |
|-------|-----------|
| `%Turns%` | Turn count **before** the current command (the Runner increments `Adventure.Turns` only after processing the input, so text printed during a turn sees the pre-command value) |
| `%Version%` | Runner/engine version (compact digit string) |
| `%Release%` | Babel / IFID release version from metadata when present |
| `%AloneWithChar%` | Key of the single NPC alone with the player, or a no-character sentinel |
| `%ConvCharacter%` | Key of the character currently in conversation |
| `%CharacterName%` | Short for `%CharacterName[%Player%]%` |
| `%CharacterName[subject\|object\|possessive|…]%` | Pronoun helper for the player (rewritten to an explicit `%CharacterName[%Player%, …]%` call) |

#### Listing and display helpers

Common keyed functions (arg is usually an item key, sometimes a pipe-separated list):

| Function | Typical use |
|----------|-------------|
| `DisplayLocation[loc]`, `LocationName[loc]` | Room name / description snippets |
| `DisplayObject[ob]`, `ObjectName[…]`, `TheObject[…]`, `TheObjects[…]` | Object naming |
| `DisplayCharacter[ch]`, `CharacterName[…]`, `CharacterDescriptor[…]`, `CharacterProper[…]`, `ProperName[…]` | Character naming |
| `LocationOf[ch]`, `ParentOf[…]`, `PrevParentOf[…]` | Containment / previous parent |
| `Held[ch]`, `Worn[ch]`, `ListHeld[ch]`, `ListWorn[ch]` | Inventory lists |
| `ListObjectsAtLocation[loc]`, `ListObjectsOn[ob]`, `ListObjectsIn[ob]`, `ListObjectsOnAndIn[ob]`, `ObjectsIn[ob]` | Object lists |
| `ListCharactersOn[ob]`, `ListCharactersIn[ob]`, `ListCharactersOnAndIn[ob]` | Character lists |
| `ListExits[ch]` | Exits from that character’s location |
| `PropertyValue[itemKey, propKey]` | Property string value |
| `TaskCompleted[taskKey]` | `True` / `False` |
| `PrevListObjectsOn[…]` | Same as list helpers, evaluated against **previous-turn** state |

#### Text utilities

| Function | Meaning |
|----------|---------|
| `LCase[s]` / `UCase[s]` / `PCase[s]` | Lower / upper / proper case |
| `NumberAsText[n]` | Spell out a number |
| `Replace[haystack, old, new]` | Substring replace |
| `Sum[…]` | Sum numbers found in the argument text |

Author **user-defined functions** ([§6.13](#613-function-user-defined)) use the same `%Name[args]%` calling convention once defined.

---

## 5. Adventure header

Children of `<Adventure>` (all optional except where noted by practice):

| Element | Meaning | Notes |
|---------|---------|--------|
| `Version` | File/product version | Required for load |
| `LastUpdated` | Adventure timestamp | |
| `Title` | Game title | |
| `Author` | Author | |
| `Description` | Babel bibliographic description | Only on library `.amf` modules |
| `FontName`, `FontSize` | Defaults (Arial, 12) | |
| `BackgroundColour`, `InputColour`, `OutputColour`, `LinkColour` | OLE colours | |
| `UserStatus` | Status-bar expression / text | May contain [expressions](#43-expressions) / `%…%` |
| `Introduction` | Opening text | Description block |
| `ShowFirstLocation` | Show room after intro | `"1"`/`"0"`; compressed saves |
| `ShowExits` | Boolean, whether or not to display the list of exits | |
| `EnableMenu`, `EnableDebugger` | Runner UI | |
| `Elapsed` | Developer time spent | |
| `Cover` | Cover image filename | |
| `EndGameText` | Win/end text | Description block |
| `TaskExecution` | Matching strategy, an enum | **Version-gated default:** when the tag is absent, files with version ≥ 5.0.22 default to `HighestPriorityTask`, but older files (< 5.000022) must default to `HighestPriorityPassingTask` — the element and the new default were introduced together in 5.0.22. Getting this wrong makes some pre-5.0.22 games (e.g. *Return to Camelot*, v5.000020) unwinnable. |
| `WaitTurns` | Number of turns for “wait” (default 3) | |
| `KeyPrefix` | Prefix for new keys | |
| `DirectionNorth` … `DirectionOut` | Direction command regexes | Slash-separated synonyms; only written if non-default |
| `NotUnderstood` | Fallback message | Loaded by some runners |
| `ifindex` | Legacy Babel | Rare |

Default direction regexes (when tags omitted):

| Direction | Default |
|-----------|---------|
| North | `North/N` |
| NorthEast | `NorthEast/NE/North-East/N-E` |
| East | `East/E` |
| SouthEast | `SouthEast/SE/South-East/S-E` |
| South | `South/S` |
| SouthWest | `SouthWest/SW/South-West/S-W` |
| West | `West/W` |
| NorthWest | `NorthWest/NW/North-West/N-W` |
| Up | `Up/U` |
| Down | `Down/D` |
| In | `In/Inside` |
| Out | `Out/O/Outside` |

---

## 6. Adventure item types

### 6.1 Folder (Generator UI only)

```xml
<Folder>
  <Key>ROOT</Key>
  <Name>Desktop</Name>
  <Member>Locations</Member>
  <Expanded>0</Expanded>
  <Height>344</Height>
  <Width>300</Width>
  <Visible>1</Visible>
  <X>176</X>
  <Y>152</Y>
  <Type>…</Type>
  <SortColumn>…</SortColumn>
  <Library>1</Library>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Folder>
```

The Generator `dev500.exe` loads/saves folders, but they have no effect at runtime.

### 6.2 Property (definition)

Defines schema for instance properties on locations, objects, characters, or any item.

```xml
<Property>
  <Key>StaticOrDynamic</Key>
  <Description>Static or Dynamic</Description>
  <Mandatory>1</Mandatory>
  <PropertyOf>Objects</PropertyOf>
  <Type>StateList</Type>
  <State>Static</State>
  <State>Dynamic</State>
  <AppendTo>…</AppendTo>
  <DependentKey>…</DependentKey>
  <DependentValue>…</DependentValue>
  <RestrictProperty>…</RestrictProperty>
  <RestrictValue>…</RestrictValue>
  <PrivateTo>…</PrivateTo>
  <Tooltip>…</Tooltip>
  <Library>1</Library>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Property>
```

**PropertyOf:** `Objects`, `Characters`, `Locations`, `AnyItem`

**Type:**

| Type | Instance value form |
|------|---------------------|
| `SelectionOnly` | Key only (no `<Value>`) — presence means selected |
| `Integer` | `<Value>n</Value>` |
| `Text` | `<Value>` with nested Description block |
| `StateList` | `<Value>stateName</Value>`; definition lists `<State>` |
| `ValueList` | Labelled ints; definition uses `<ValueList><Label/><Value/></ValueList>` |
| `ObjectKey` / `CharacterKey` / `LocationKey` / `LocationGroupKey` | Key string in `<Value>` |

Common Standard Library property keys (non-exhaustive): `StaticOrDynamic`, `DynamicLocation`, `InLocation`, `OnWhat`, `InsideWhat`, `HeldByWho`, `WornByWho`, `Wearable`, `Openable`, `Container`, `Surface`, `CharacterLocation`, `CharacterAtLocation`, `CharOnWhat`, `CharInsideWhat`, `CharOnWho`, `Gender`, `Known`, `Weight`, `Size`. (Note the surface/container keys are `OnWhat` / `InsideWhat` — `OnObject` / `InObject` are save-file *enum values*, not property keys.)

### 6.3 Location

```xml
<Location>
  <Key>LocationLab</Key>
  <ShortDescription>…</ShortDescription>
  <LongDescription>…</LongDescription>
  <Movement>
    <Direction>West</Direction>
    <Destination>LocationHall</Destination>
    <Restrictions>…</Restrictions>
  </Movement>
  <Property>
    <Key>SomeProp</Key>
    <Value>…</Value>
  </Property>
  <Hide>1</Hide>
  <Library>1</Library>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Location>
```

**Directions:** `North`, `NorthEast`, `East`, `SouthEast`, `South`, `SouthWest`, `West`, `NorthWest`, `Up`, `Down`, `In`, `Out`

**Hide:** When `1`, the Runner omits this location from the map UI (Generator still shows it, dashed). Does not hide the room from play or descriptions. See [§6.15 Map](#615-map).

### 6.4 Object

```xml
<Object>
  <Key>ObjectHat</Key>
  <Article>a</Article>
  <Prefix>red</Prefix>
  <Name>hat</Name>
  <Name>cap</Name>
  <Description>…</Description>
  <Property>
    <Key>StaticOrDynamic</Key>
    <Value>Dynamic</Value>
  </Property>
  <Property>
    <Key>DynamicLocation</Key>
    <Value>In Location</Value>
  </Property>
  <Property>
    <Key>InLocation</Key>
    <Value>LocationLab</Value>
  </Property>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Object>
```

Placement is **via properties**, not a dedicated location child. Typical dynamic placement:

| Property | Example values |
|----------|----------------|
| `StaticOrDynamic` | `Static`, `Dynamic` |
| `DynamicLocation` | `Hidden`, `In Location`, `Inside Object`, `On Object`, `Held By Character`, `Worn By Character` |
| `InLocation` / related | location or object/character key |

Static placement uses `StaticLocation`-family properties. The property **values** are adventure wording: `Nowhere` (or `Hidden`), `Single Location`, `Location Group`, `Everywhere`, `Part of Character`, `Part of Object` — these map to the `StaticExistsWhereEnum` names `NoRooms`, `SingleLocation`, `LocationGroup`, `AllRooms`, `PartOfCharacter`, `PartOfObject` (`clsObject.vb`). Don't confuse the enum names with the property values.

### 6.5 Task

A task runs its [`<Actions>`](#45-actions) when executed. When tasks are completed, they are "set." Actions can mark a task as uncompleted by "unsetting" the task.

```xml
<Task>
  <Key>TaskTakeHat</Key>
  <Priority>5</Priority>
  <AutoFillPriority>10</AutoFillPriority>
  <Type>General</Type>
  <Command>take {the} %object%</Command>
  <Description>Take something</Description>
  <CompletionMessage>…</CompletionMessage>
  <Repeatable>1</Repeatable>
  <Continue>ContinueAlways</Continue>
  <LowPriority>1</LowPriority>
  <PreventOverriding>1</PreventOverriding>
  <ReplaceTask>1</ReplaceTask>
  <MessageBeforeOrAfter>After</MessageBeforeOrAfter>
  <FailOverride>…</FailOverride>
  <Restrictions>…</Restrictions>
  <Actions>…</Actions>
  <Aggregate>0</Aggregate>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Task>
```

- `Priority` — matching order among General tasks: **lower number is tried first**. When multiple tasks match (or Continue keeps searching), the runner walks rising `Priority` values. Equal `Priority` values have **no defined order** (do not rely on which tied task runs first).
- `AutoFillPriority` — The `run500.exe` runner can autocomplete commands. Lower values prefer that task’s commands in suggestions; `0` disables auto-fill for the task. Default `10`; writers omit the tag when it is still the default. General tasks only.
- `LowPriority` — if a better-priority matching task has already failed with restriction/fail text, skip this task rather than letting it override that failure. Typical for library fallbacks. Written as `<LowPriority>1</LowPriority>` when set.
- `PreventOverriding` — Generator authoring: do not offer this task as a parent (`GeneralTask`) for new Specific tasks (“Prevent this task from being inherited”). Not used at play time by runners.
- `ReplaceTask` — load/merge: if another task already has this key, replace it with this one (library/module load). Without it, duplicate keys are skipped or renamed. Generator checkbox only enabled for library items.

**Type:** The type of a task determines when the task is executed.

* For `<General>` tasks, the task is executed when the player types a command that matches the task's `<Command>` list.
* `<Specific>` tasks execute when their parent `<General>` task matches, specialized for particular values. See below.
* `<System>` tasks can be executed at the start of the adventure (`<RunImmediately>`), when entering a specific location (`<LocationTrigger>`), or have no execution trigger of its own. See below.

**Continue:** After a matching task runs, the runner may try other tasks that also match the same input but have a worse (higher) `Priority`.

- `<Continue>ContinueAlways</Continue>` — always keep searching for lower-priority matches (“multiple matching”).
- Tag omitted (or older values `ContinueNever` / `ContinueOnFail` / `ContinueOnNoOutput`, all treated as false) — stop once a matching task **passes and produces output**. The runner still continues if the task fails and/or produces no output (so a later task can handle the command), subject to adventure `TaskExecution`.

When it does not continue, command matching stops: later tasks are not tried for that input.

**Aggregate:** Default true; writers emit `<Aggregate>0</Aggregate>` only when false.

**MessageBeforeOrAfter:** `Before` (default) or `After`.

#### Specific tasks

A Specific task specializes a General parent for particular referenced values. It has no `<Command>` of its own; it matches when the parent’s command matched and the player’s references satisfy each `<Specific>` slot.

```xml
<Task>
  <Key>TaskTakeHat</Key>
  <Type>Specific</Type>
  <GeneralTask>TakeObjects</GeneralTask>
  <Specific>
    <Type>Object</Type>
    <Multiple>0</Multiple>
    <Key>Hat</Key>
  </Specific>
  <CompletionMessage>…</CompletionMessage>
  <SpecificOverrideType>Override</SpecificOverrideType>
  <Restrictions>…</Restrictions>
  <Actions>…</Actions>
</Task>
```

- `GeneralTask` — key of the parent General task (e.g. `TakeObjects`)
- One `<Specific>` per reference slot in the parent’s first command, in that order
  - `Type` — reference kind: `Object`, `Character`, `Number`, `Text`, `Direction`, `Location`, `Item`
  - `Multiple` — `1` if that slot may match multiple items (as with `%objects%`); else `0`
  - `Key` — required match for that slot; empty (`<Key />` or `<Key></Key>`) means any value of that type; may use placeholders such as `%Player%`
  - Multiple `<Key>` children are allowed when several concrete values should match the same slot
- `SpecificOverrideType` — how this task’s text/actions combine with the parent’s. The specific task itself always runs fully; the `TextOnly` / `ActionsOnly` suffix selects **which halves of the parent still run**:
  - `Override` — run only this task; the parent is suppressed (default)
  - `BeforeTextAndActions` — this task first, then the parent's text *and* actions
  - `BeforeTextOnly` — this task first, then only the parent's *text* (parent actions suppressed)
  - `BeforeActionsOnly` — this task first, then only the parent's *actions* (parent text suppressed)
  - `AfterTextAndActions` — the parent first, then this task. (`AfterTextOnly` / `AfterActionsOnly` are accepted but unimplemented in the Runner — an explicit TODO in the source — and behave like `AfterTextAndActions`)

Example with two slots (`say %text% to %character%`):

```xml
<Specific>
  <Type>Text</Type>
  <Multiple>0</Multiple>
  <Key>virtue</Key>
</Specific>
<Specific>
  <Type>Character</Type>
  <Multiple>0</Multiple>
  <Key>Boethius</Key>
</Specific>
```

#### System tasks

System tasks have no player command patterns. They run only when triggered. They can be triggered by an `<Action>` (`SetTasks`), at adventure start (`RunImmediately`), or when the player enters a location (`LocationTrigger`).

```xml
<Task>
  <Key>TaskEnterOutside</Key>
  <Type>System</Type>
  <Description>Escape (Front Door)</Description>
  <LocationTrigger>Outside</LocationTrigger>
  <CompletionMessage>…</CompletionMessage>
  <Repeatable>1</Repeatable>
  <Restrictions>…</Restrictions>
  <Actions>…</Actions>
</Task>
```

##### `LocationTrigger`

Value is a **location key**. When the player character moves to a location whose key equals that value, the runner queues this System task (if it is still runnable: `Repeatable` or not yet completed). Same-location “moves” do not fire it. Non-player character movement does not.

Omit `LocationTrigger` (and omit `RunImmediately`) for tasks that should run only when explicitly executed.

`RunImmediately` (`1`) runs the System task once at adventure load instead; it is mutually exclusive with a location trigger in the Generator UI.

### 6.6 Event

Timed or turn-based scripting. An event starts, runs for a `Length`, optionally repeats, can be started/stopped by task completion, and fires zero or more `SubEvent`s at absolute or relative times.

```xml
<Event>
  <Key>EventStorm</Key>
  <Description>Storm</Description>
  <Type>TurnBased</Type>
  <WhenStart>AfterATask</WhenStart>
  <StartDelay>1 to 3</StartDelay> <!-- for <WhenStart>BetweenXandYTurns</WhenStart>; also re-drawn before each repeat when <RepeatCountdown>1 -->
  <Length>5 to 10</Length>
  <Repeating>1</Repeating>
  <RepeatCountdown>1</RepeatCountdown>
  <Control>Start Completion TaskBeginStorm</Control>
  <SubEvent>
    <When>1 FromStartOfEvent</When>
    <What>DisplayMessage</What>
    <Measure>Turns</Measure>
    <Action>
      <Description>
        <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
        <Text>Rain begins to fall.</Text>
      </Description>
    </Action>
    <OnlyApplyAt>LocationOutside</OnlyApplyAt>
  </SubEvent>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Event>
```

#### Type and lifespan

- `Type` — `TurnBased` (advances with turns) or `TimeBased` (wall-clock; SubEvents may use real timers).
- `Length` — how long the event runs once started. `FromTo` form: `"N"` or `"N to M"` (a value in that inclusive range is chosen each start/reset). Required.
- `Repeating` (`1`) — when the event finishes after `Length`, it may start again (not restarted if `Length` resolves to 0).
- `RepeatCountdown` (`1`) — if repeating, wait out `StartDelay` again before each restart; otherwise restart immediately.

#### WhenStart

How the event begins:

* `Immediately`: Start the event when play begins
* `BetweenXandYTurns`: Wait a random delay from start. Requires a `<StartDelay>1 to 3</StartDelay>` to specify how long to delay. The delay starts when play begins. (`StartDelay` never applies to `<SubEvent>`s — their timing is always relative to their **own** event via `FromStartOfEvent` / `FromLastSubEvent` / `BeforeEndOfEvent`; see below.)
* `AfterATask` (or `0`): Starts when a `<Control>` starts the task. See below

#### Control

Zero or more `<Control>` elements. Body format is [§2.12](#212-control):

```text
Start|Stop|Suspend|Resume  Completion|UnCompletion  TaskKey
```

Example: `Start Completion TaskBeginStorm` — start this event when `TaskBeginStorm` completes. `UnCompletion` fires when that task, having completed, is unset by a `SetTasks Unset` **action**. (An event's or walk's `UnsetTask` sub-action does *not* fire UnCompletion controls — it only clears the completed flag.) Controls also Stop / Suspend / Resume an event already in progress.

#### SubEvent

Zero or more `<SubEvent>` children. Timing is checked each tick while the event is `Running`.

```text
<When>N WhenEnum</When>     e.g. 1 FromStartOfEvent   or   0 to 2 FromLastSubEvent
<What>WhatEnum</What>    see below
<Measure>Turns|Seconds</Measure>
<Action>    may be a description block, or a task to execute/"unset" (mark uncompleted)
<OnlyApplyAt>    see below
```

- `When` — fire after a randomized [`FromTo`](#211-fromto-inclusive-ranges) inclusive range of "units" (turns or seconds), measured as:
  - `FromStartOfEvent` — from when the event started
  - `FromLastSubEvent` — since the previous SubEvent ran (the first uses start-of-event if nothing has run yet)
  - `BeforeEndOfEvent` — when only that many units remain until `Length` elapses
- `Measure` — the unit for the number in `<When>`:
  - On a **TurnBased** event: `Turns` means player turns; `Seconds` means real-world wall-clock seconds (a timer, independent of turns). Example: `<When>5 FromStartOfEvent</When>` with `<Measure>Seconds</Measure>` fires five seconds after the event starts, even if the player has not taken a turn.
  - On a **TimeBased** event: the event itself already advances once per real second, so `Turns` and `Seconds` both count those seconds. Prefer `Seconds` for clarity.
- `What` / `Action` / `OnlyApplyAt`:
  - `DisplayMessage` — `<Action>` holds a description block. `OnlyApplyAt` is a location or location-group key; display runs only if the player is there. Use `AllLocations` for everywhere. Without `OnlyApplyAt`, the message is not shown.
  - `SetLook` — `<Action>` holds a description block that displays when the player runs the `look` command while the event is running. `OnlyApplyAt` adds the description block only in the matching locations.
  - `ExecuteTask` — `<Action>` text is `ExecuteTask TaskKey` (no `OnlyApplyAt`).
  - `UnsetTask` — `<Action>` text is `UnsetTask TaskKey` (marks that task incomplete; no `OnlyApplyAt`).

### 6.7 Character

```xml
<Character>
  <Key>CharacterBob</Key>
  <Name>Bob</Name>
  <Article>the</Article>
  <Prefix />
  <Descriptor>guard</Descriptor>
  <Type>NonPlayer</Type>
  <Description>…</Description>
  <Property>…</Property>
  <Walk>
    <Description>Patrol</Description>
    <Loops>1</Loops>
    <StartActive>0</StartActive>
    <Step>LocationWest 1 to 2</Step>
    <Control>Start Completion TaskStartPatrol</Control>
    <Activity>
      <When>ComesAcross %Player%</When>
      <Action>…</Action>
      <OnlyApplyAt>…</OnlyApplyAt>
    </Activity>
  </Walk>
  <Topic>
    <Key>TopicWeather</Key>
    <ParentKey>…</ParentKey>
    <Summary>Weather</Summary>
    <Keywords>weather rain</Keywords>
    <Description>…</Description>
    <IsAsk>1</IsAsk>
    <IsTell>1</IsTell>
    <IsCommand>1</IsCommand>
    <IsFarewell>1</IsFarewell>
    <IsIntro>1</IsIntro>
    <StayInNode>1</StayInNode>
    <Restrictions>…</Restrictions>
    <Actions>…</Actions>
  </Topic>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Character>
```

**Name** is the proper name (e.g. `Bob`). **Descriptor** is the common-noun form used in command matching and display (like an object’s `<Name>`). With `<Article>` and `<Prefix>`, the first descriptor makes phrases such as “a tall guard”. Multiple `<Descriptor>`s are aliases.

**Type:** `Player` or `NonPlayer`.

**`<Perspective>`:** omitted when `ThirdPerson`; otherwise e.g. `FirstPerson`, `SecondPerson`. Usually only set on the player character. The default when omitted is version-gated: files ≥ 5.00002 default to `ThirdPerson`, but the loader forces `SecondPerson` for older files. (On a `MoveCharacter … SwitchWith` player swap, the new player character **inherits the previous player's perspective**, overwriting its own authored value.)

#### Character location

Initial placement is stored as **instance properties** on the character (not as dedicated top-level children). Common Standard Library keys:

| Property | Value |
|----------|--------|
| `CharacterLocation` | `At Location`, `Hidden`, `In Object`, `On Object`, `On Character`, or `In Character` |
| `CharacterAtLocation` | location key (when at a location) |
| `CharInsideWhat` | object key (when inside an object) |
| `CharOnWhat` | object key (when on an object) |
| `CharOnWho` | character key (when on a character) |
| `CharInsideWho` | character key (when in a character) |

Example — Bob standing in a room:

```xml
<Property>
  <Key>CharacterLocation</Key>
  <Value>At Location</Value>
</Property>
<Property>
  <Key>CharacterAtLocation</Key>
  <Value>LocationYard</Value>
</Property>
```

(Save-game XML uses compact location fields instead; see [§7.5](#75-character).)

#### Walk

NPC itineraries. Zero or more `<Walk>` children per character.

```xml
<Walk>
  <Description>Patrol</Description>
  <Loops>1</Loops>
  <StartActive>0</StartActive>
  <Step>LocationWest 1 to 2</Step>
  <Step>LocationHall 1</Step>
  <Control>Start Completion TaskStartPatrol</Control>
  <Activity>
    <When>ComesAcross %Player%</When>
    <Action>
      <Description>
        <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
        <Text>The guard notices you.</Text>
      </Description>
    </Action>
    <OnlyApplyAt>AllLocations</OnlyApplyAt>
  </Activity>
  <Activity>
    <When>1 FromStartOfWalk</When>
    <Action>ExecuteTask TaskFromWalk</Action>
  </Activity>
</Walk>
```

- `Description` — label for the Generator / debugger
- `Loops` — `1` to repeat the walk when it finishes; `0` otherwise
- `StartActive` — `1` to start the walk when the adventure begins.
  - To start an inactive walk later, use a `<Control>` like this: `<Control>Start Completion TaskKey</Control>`. Then complete that task (for example `<SetTasks>Execute TaskKey</SetTasks>`) to start the walk.
- `Step` — `Destination FromTo` (duration is `"N"` or `"N to M"` turns). One or more steps define the route. The destination may be a location key, `Hidden`, a location-**group** key (a random member is drawn each time, re-rolling until one adjacent to the walker is found — the re-rolls consume RNG draws), or a character key / `%Player%` (step toward that character, taken only when adjacent).
- `Control` — same compact form as events: `Start|Stop|Suspend|Resume Completion|UnCompletion TaskKey` (drives the walk when that task completes or is unset)
- `Activity` (SubWalk) — optional side effects while walking:
  - `When` — either `ComesAcross Key`, or `FromTo FromStartOfWalk|FromLastSubWalk|BeforeEndOfWalk`. In practice `ComesAcross` fires only on meeting the **player**: the desktop Runner ignores the key entirely and tests walker-location == player-location, so author it as `ComesAcross %Player%` (object keys never fire)
  - `Action` — description block (display message), or `ExecuteTask TaskKey` / `UnsetTask TaskKey`
  - `OnlyApplyAt` — location or location-group key restricting when a display message is shown (`AllLocations` for everywhere)

#### Topic

Conversation nodes for this character. Zero or more `<Topic>` children.

```xml
<Topic>
  <Key>TopicWeather</Key>
  <ParentKey>TopicRoot</ParentKey>
  <Summary>Weather</Summary>
  <Keywords>weather rain</Keywords>
  <Description>…</Description>
  <IsAsk>1</IsAsk>
  <IsTell>1</IsTell>
  <IsCommand>1</IsCommand>
  <IsFarewell>1</IsFarewell>
  <IsIntro>1</IsIntro>
  <StayInNode>1</StayInNode>
  <Restrictions>…</Restrictions>
  <Actions>…</Actions>
</Topic>
```

- `Key` — topic id (conversation “node”)
- `ParentKey` — optional tree link. The runner keeps a single current-node key for whoever you’re talking to. A topic with `ParentKey` set is only considered when that current node equals the parent; topics with no `ParentKey` are always eligible for that character. There is no adventure property, variable, or restriction that exposes the current node — the only way to branch is by giving child topics the right `ParentKey`. (A Character restriction like `%Player% Must BeInConversationWith CharacterBob` only checks *which character* you are talking to, not which topic.)
- `Summary` — short label (Generator / UI)
- `Keywords` — match text for Ask/Tell/Command (comma-separated for Ask/Tell keyword lists; command patterns when `IsCommand`)
- `Description` — reply text (description block)
- `IsAsk` / `IsTell` / `IsCommand` / `IsFarewell` / `IsIntro` — which kind of conversation attempt can match this topic (`1` when set; omitted when false):
  - `IsIntro` — opening/greeting someone (Conversation `Greet` / first talk)
  - `IsAsk` — asking them about something (`Ask …`)
  - `IsTell` — telling them about something (`Tell …`)
  - `IsCommand` — free-form “say …” / command patterns toward them (Conversation `Say`)
  - `IsFarewell` — ending the conversation (`Farewell` / goodbye)
  A topic may set several of `IsAsk` / `IsTell` / `IsIntro` / `IsFarewell` to respond in more than one of those situations. `IsCommand` is different: matching compares the flag for **equality**, so setting `IsCommand` removes the topic from Ask/Tell/Greet/Farewell matching — it then matches only Say/command attempts.
- `StayInNode` — controls the current-node key after this topic runs. If the topic **has child topics**, the node is set to this topic's key regardless of `StayInNode`. If it has no children: `1` keeps the node at its **previous** value (you stay in whatever node you were already in); omitted/false clears the node.
- `Restrictions` / `Actions` — same shared forms as tasks ([§4.4](#44-restrictions), [§4.5](#45-actions))

### 6.8 Variable

Set variables wih a `SetVariable` action; read variables with expressions.

```xml
<Variable>
  <Key>VariableScore</Key>
  <Name>Score</Name>
  <Type>Numeric</Type>
  <InitialValue>0</InitialValue>
  <ArrayLength>10</ArrayLength>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Variable>
```

**Type** is only `Numeric` or `Text`. A variable becomes an array when `<ArrayLength>` is present and greater than 1 (omit it, or treat as 1, for a scalar). That gives four useful forms: numeric, text, numeric array, text array.

- Scalar: `<InitialValue>` holds one number or one string.
- Numeric array: initials are usually comma-separated (`1,2,3`); if there is a single value, every slot is filled with it.
- Text array: initials are newline-separated (one entry per line in the saved string).

ADRIFT arrays are 1-indexed. Some older files write array size as `<Length>` instead of `<ArrayLength>`; loaders may accept both.

### 6.9 Group

```xml
<Group>
  <Key>AllLocations</Key>
  <Type>Locations</Type>
  <Name>All Locations</Name>
  <Member>LocationLab</Member>
  <Property>…</Property>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Group>
```

**Type:** `Locations`, `Objects`, `Characters`.

Special key `AllLocations` / `ALLROOMS` may expand to all locations at runtime.

### 6.10 TextOverride (ALR)

These used to be called "ADRIFT Language Resources" (ALR). "ALR" appears throughout the code.

```xml
<TextOverride>
  <Key>ALR1</Key>
  <OldText>you can't</OldText>
  <NewText>…description…</NewText>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</TextOverride>
```

Replaces matching output text.

### 6.11 Hint

Hints were never implemented in the ADRIFT 5 `run500.exe` runner, but some Adventure XML files include them anyway.

```xml
<Hint>
  <Key>Hint1</Key>
  <Question>How do I open the door?</Question>
  <Subtle>…</Subtle>
  <Sledgehammer>…</Sledgehammer>
  <Restrictions>…</Restrictions>
</Hint>
```

### 6.12 Synonym

Creates synonyms for any whole word in the players input before command matching.

```xml
<Synonym>
  <Key>SynLook</Key>
  <From>x</From>
  <From>examine</From>
  <To>look</To>
</Synonym>
```

### 6.13 Function (user-defined)

Functions accept a number of arguments and return values in expressions.

```xml
<Function>
  <Key>FunEcho</Key>
  <Name>Echo</Name>
  <Output>…</Output> <!-- description block -->
  <Argument>
    <Name>text</Name>
    <Type>Text</Type>
  </Argument>
  <LastUpdated>…</LastUpdated>
  <Created>…</Created>
</Function>
```

Argument **Type** values include `Text`, `Numeric`, `Object`, `Character`, `Location`, etc.

### 6.14 Exclude

```xml
<Exclude>SomeLibraryKey</Exclude>
```

Zero or more direct children of `<Adventure>`. Each element’s text is an item **key**. When merging [`.amf` libraries](#13-amf-library-modules), items with those keys are **not** loaded (`ShouldWeLoadLibraryItem` → no).

Used to opt out of specific pieces of Standard Library (or other modules) without editing the `.amf`. Survives in the adventure XML. See also [§2.9 `<Library>`](#29-library-flag).

### 6.15 Map

```xml
<Map>
  <Page>
    <Key>0</Key>
    <Selected>1</Selected>
    <Label>Main</Label>
    <Node>
      <Key>LocationLab</Key>
      <X>0</X>
      <Y>0</Y>
      <Z>0</Z>
      <Height>4</Height>
      <Width>6</Width>
      <Link>
        <SourceAnchor>West</SourceAnchor>
        <DestinationAnchor>East</DestinationAnchor>
        <Anchor>
          <X>-2</X>
          <Y>2</Y>
          <Z>0</Z>
        </Anchor>
      </Link>
    </Node>
  </Page>
</Map>
```

The `run500.exe` Runner displays an automatic map using the `<Map>` element.

The `<Map>` is for _displaying_ the map; it doesn't control character movement or apply restrictions. That's defined on each location's [`Movement`](#63-location) children.

The runner can generate a map even if the `<Map>` element is missing, empty, or has a single page with no nodes, building it from location `<Movement>` elements.

#### Page

Zero or more `<Page>` children under `<Map>`.

| Child | Required | Notes |
|-------|----------|-------|
| `Key` | Yes | Integer page id. Default page is `0`. |
| `Selected` | No | `"1"` marks the page the Generator UI had selected. At most one page is selected. |
| `Label` | No | Display name for the page tab. Labels default to `Page N` where `N = Key + 1` if `Label` is omitted. |
| `Node` | Zero+ | One node per location shown on this page. |

Each page is a separate canvas. Only the active page’s nodes are drawn. In the Generator, tabs are visible and authors can rename them (`Label`) and switch between them. In the `run500.exe` Runner, a page’s tab appears once any of its nodes has been seen (`MapPage.Seen`). The Runner switches the active page automatically when focusing the player’s current location.

Pages partition the location graph: a location appears on at most one page.

#### Node

Each `<Node>` is one location rectangle on a page.

| Child | Required | Notes |
|-------|----------|-------|
| `Key` | Yes | Location key. Must exist in `<Location>`; unknown keys are skipped on load. |
| `X`, `Y`, `Z` coordinates | Yes | Integer top-left of the node box in abstract map units. |
| `Height` | No | Default `4`. Written only when ≠ 4. |
| `Width` | No | Default `6`. Written only when ≠ 6. |
| `Link` | Zero+ | Outgoing drawn connectors from this node (keyed by source direction). |

#### Link

Each `<Link>` is stored under the source node. Destination location key, duplex (two-way vs one-way), and line style are **not** serialized in `<Map>`; load reconstructs them from location movements. A movement with restrictions makes the link *eligible* for dotted rendering, but the Runner’s draw rules decide solid / dotted / omitted (see below).

**Duplex (two-way):** On load, the link is duplex when the destination location’s movement for `DestinationAnchor` points back to the source location. Otherwise it is one-way.

| Child | Required | Notes |
|-------|----------|-------|
| `SourceAnchor` | Yes | `DirectionsEnum` name: which face of **this** node the line leaves. Same names as location directions (`North` … `NorthWest`, `Up`, `Down`, `In`, `Out`). |
| `DestinationAnchor` | Yes | Face of the destination node the line arrives at. |
| `Anchor` | Zero+ | Optional midpoints for a bent polyline: each has `X` / `Y` / `Z`. Order is the path from source to destination. |

A link whose destination is on another `<Page>` does **not** get a full connector in the Runner (geometry is resolved with the current page’s nodes only). Whether anything is drawn instead depends on whether the destination has been seen — see below.

Only one link is allowed per `SourceAnchor` per node.

#### Runner drawing rules (for UI implementors)

Node/link visibility ("fog of war") is determined at runtime, controlled by the runner.

**Nodes**

- Hide nodes the player has not seen, and nodes whose location has `<Hide>1</Hide>`.
  - Authors can auto-reveal unseen rooms by teleporting the player, moving the player through every room that the authors want to reveal. (Grandpa's Ranch does this.)
- Hide nodes that aren't on the current map page.
- Dim or fade nodes whose `Z` differs from the focused node’s `Z`.

**Exits / links**

For each outgoing link, the Runner chooses one of: a full connector, a "stub" out-arrow, or nothing.

A "stub" is a short arrow from the node edge into empty space (or an In/Out icon with no arrow).

In/Out exits are drawn as labeled green ("IN") / pink ("OUT") discs on the node edge.

| Destination | Route allowed | Same page as source | Other page / no map node |
|-------------|---------------|---------------------|---------------------------|
| **Unseen** | Yes | Stub | Stub |
| **Unseen** | No | Nothing | Nothing |
| **Seen** | Yes | Full connector from Map `Link` (if present) | **Nothing** |
| **Seen** | No | Nothing | Nothing |

Details:

- **Unseen destination:** draw a stub when the route is allowed. This does not require a Map `<Link>` — stubs follow location `<Movement>`.
- **Seen destination on the same page:** draw the Map `Link` connector when one exists for that `SourceAnchor`.
  - No movement restrictions: solid line.
  - Movement has restrictions and the route currently fails: omit the link
  - Movement has restrictions and the route currently passes: link style is dotted.
    - The `run500.exe` Runner forces the link to be solid until that exit's restricton tests have failed at least once. For example, if route is passable at the start of the game, and then later becomes restricted, and then the player unblocks the route, the route would change to dotted. This state is not saved/recorded in the save-game XML state. (It seems unnecessarily complicated to me.)
  - **Arrow heads (one-way vs two-way):** One-way links (`Not Duplex`) draw an arrow head at the destination end of the connector. Duplex (two-way) links have no arrow head. Stubs to unseen destinations always use an arrow head. The Runner draws each undirected pair once (via node sort order), so a duplex exit appears as a single line without arrows rather than two opposing arrows.
  - Self-links (destination key equals source) are drawn as out-arrows, not a loop through empty space.
- **Seen destination on another page (or no node on this page):** The map shows no exit glyph for that direction. (An exit back to a seen starting room on another page is a common case of this.)


**Pages**

- Show a tab strip; the Runner shows a tab for a page once any node on that page has been seen.
- Switch to the page that contains the player’s current location when focusing on them.
- Pages with no seen nodes stay hidden in the tab strip.

### 6.16 FileMappings (Blorb export)

```xml
<FileMappings>
  <Mapping>
    <Resource>1</Resource>
    <File>C:\Games\mygame\cover.jpg</File>
  </Mapping>
</FileMappings>
```

Present only when the Generator packages a game as a Blorb (`.blorb`). Not written for plain `.taf` / `.xml` saves.

Blorb files can include images and sound as numbered resources, but Adventure XML refers to them by absolute paths on the author's machine, e.g. `C:\Users\authorname\mygame\cover.jpg`.

When building a blorb, the Generator embeds the image into the blorb and adds a `<Mapping>`, allowing the runner to convert the `cover.jpg` file path into a Blorb resource ID number, extract the image from the blorb, and display the image.

---

## 7. Save-game XML (`<Game>`)

Typically zlib-compressed in `.tas` files. Root is **`<Game>`**, not `<Adventure>` — runtime deltas only (keys must already exist in the adventure).

Booleans in this file are usually VB `.ToString` (`True` / `False`), not Adventure’s `1` / `0`.

Several item children may include **`<Displayed>`** elements: save-only markers for which `DisplayOnce` description fragments have already been shown ([§4.2](#42-description-blocks)). Each value is `descriptionIndex-singleIndex` (1-based): which description list on the item, and which `<Description>` child within it. Example: `<Displayed>1-2</Displayed>`.

### 7.1 Location

```xml
<Location>
  <Key>LocationLab</Key>
  <Property><Key>…</Key><Value>…</Value></Property>
  <Displayed>1-2</Displayed>
</Location>
```

| Child | Notes |
|-------|-------|
| `Key` | Location key |
| `Property` | Zero or more current instance property values |
| `Displayed` | Zero or more; see above |

Does **not** store fog-of-war location seen state (that lives on [Character `Seen`](#75-character)).

### 7.2 Object

```xml
<Object>
  <Key>ObjectHat</Key>
  <DynamicExistWhere>HeldByCharacter</DynamicExistWhere>
  <StaticExistWhere>NoRooms</StaticExistWhere>
  <LocationKey>Player</LocationKey>
  <Property>…</Property>
  <Displayed>…</Displayed>
</Object>
```

| Child | Notes |
|-------|-------|
| `Key` | Object key |
| `DynamicExistWhere` | Omitted when `Hidden`. Values: `Hidden`, `InLocation`, `InObject`, `OnObject`, `HeldByCharacter`, `WornByCharacter` |
| `StaticExistWhere` | Omitted when `NoRooms`. Values: `NoRooms`, `SingleLocation`, `LocationGroup`, `AllRooms`, `PartOfCharacter`, `PartOfObject` |
| `LocationKey` | Key of the place/holder implied by those where-enums (location, object, character, group, …) |
| `Property` | Zero or more instance properties. **Must stay consistent with the location triple**: FrankenDrift applies `DynamicExistWhere`/`LocationKey` first, then overwrites from these properties, and object `Location` GET is derived from `DynamicLocation` / `HeldByWho` / `WornByWho` / `InLocation` / `InsideWhat` / `OnWhat` (or the static equivalents). Values use adventure wording (`Held By Character`, `Inside Object`, …), not the enum names |
| `Displayed` | Zero or more |

Adventure XML places objects via properties ([§6.4](#64-object)); the save repeats both the compact location triple and the matching location properties.

### 7.3 Task

```xml
<Task>
  <Key>Task1</Key>
  <Completed>True</Completed>
  <Scored>False</Scored>
  <Displayed>…</Displayed>
</Task>
```

| Child | Notes |
|-------|-------|
| `Key` | Task key |
| `Completed` | Whether the task has successfully run. Non-`Repeatable` tasks with `Completed` true are not runnable again until unset ([§6.5](#65-task)) |
| `Scored` | Whether this task has already applied its score change (first time completing updates `Score`; prevents double-counting on later runs) |
| `Displayed` | Zero or more |

### 7.4 Event

```xml
<Event>
  <Key>Event1</Key>
  <Status>Running</Status>
  <Timer>3</Timer>
  <SubEventTime>1</SubEventTime>
  <SubEventIndex>0</SubEventIndex>
  <Displayed>…</Displayed>
</Event>
```

| Child | Notes |
|-------|-------|
| `Key` | Event key |
| `Status` | `NotYetStarted`, `Running`, `CountingDownToStart`, `Paused`, `Finished` |
| `Timer` | Remaining time to end of event (`TimerToEndOfEvent`) |
| `SubEventTime` | Event timer when the last SubEvent fired (`iLastSubEventTime` / `TimerFromStartOfEvent` then). Keeps `FromLastSubEvent` timing correct after load ([§6.6](#66-event)) |
| `SubEventIndex` | 0-based index of that last SubEvent in the event’s `SubEvents` list |
| `Displayed` | Zero or more |

### 7.5 Character

```xml
<Character>
  <Key>Player</Key>
  <ExistWhere>AtLocation</ExistWhere>
  <Position>Standing</Position>
  <LocationKey>LocationLab</LocationKey>
  <Walk>
    <Status>Running</Status>
    <Timer>5</Timer>
  </Walk>
  <Seen>LocationLab</Seen>
  <Property>…</Property>
  <Displayed>…</Displayed>
</Character>
```

| Child | Notes |
|-------|-------|
| `Key` | Character key |
| `ExistWhere` | Omitted when `Hidden`. Values: `Hidden`, `AtLocation`, `OnObject`, `InObject`, `OnCharacter` |
| `Position` | Omitted when `Standing`. Values: `Standing`, `Sitting`, `Lying` |
| `LocationKey` | Key of location / object / character for `ExistWhere` (omitted if empty) |
| `Walk` | Zero or more, **in adventure walk order**. Each has `Status` (`NotYetStarted`, `Running`, `Paused`, `Finished`) and `Timer` (turns left in the current walk / `iTimerToEndOfWalk`) |
| `Seen` | Zero or more keys this character has seen — **locations, objects, and characters** in one flat list. On load, membership is tested against each keyspace (`HasSeenLocation` / `HasSeenObject` / `HasSeenCharacter`). Automap fog of war uses the **Player**’s seen locations |
| `Property` | Zero or more instance properties (may include `ProperName`). Location-related properties (`CharacterLocation`, `CharacterAtLocation`, `CharInsideWhat`, `CharOnWhat`, `CharOnWho`) must match `ExistWhere` / `LocationKey` |
| `Displayed` | Zero or more |

Adventure placement uses properties ([§6.7](#67-character)); the save uses the compact location fields above.

### 7.6 Variable

```xml
<Variable>
  <Key>VariableScore</Key>
  <Value_0>10</Value_0>
  <Displayed>…</Displayed>
</Variable>
```

| Child | Notes |
|-------|-------|
| `Key` | Variable key |
| `Value_N` | Current value of array slot `N` (0-based). Numeric slots equal to `0` and empty text slots are **omitted**. Older saves may use a single `<Value>` for slot 0 |
| `Displayed` | Zero or more |

### 7.7 Group

```xml
<Group>
  <Key>GroupEnemies</Key>
  <Member>CharacterGuard</Member>
</Group>
```

| Child | Notes |
|-------|-------|
| `Key` | Group key |
| `Member` | Zero or more member keys (full current membership after add/remove actions) |

### 7.8 Turns

```xml
<Turns>42</Turns>
```

Integer turn counter for the session (displayed in the status bar).

---

## 8. Minimal adventure skeleton

```xml
<?xml version="1.0" encoding="utf-8"?>
<Adventure>
  <Version>5.000036</Version>
  <Title>Example</Title>
  <Author>Author</Author>
  <Introduction>
    <Description>
      <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
      <Text>You wake up.</Text>
    </Description>
  </Introduction>
  <ShowFirstLocation>1</ShowFirstLocation>

  <Property>
    <Key>StaticOrDynamic</Key>
    <Description>Static or Dynamic</Description>
    <PropertyOf>Objects</PropertyOf>
    <Type>StateList</Type>
    <State>Static</State>
    <State>Dynamic</State>
  </Property>
  <!-- … further property definitions as needed … -->

  <Location>
    <Key>LocationStart</Key>
    <ShortDescription>
      <Description>
        <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
        <Text>Start</Text>
      </Description>
    </ShortDescription>
    <LongDescription>
      <Description>
        <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
        <Text>A small room.</Text>
      </Description>
    </LongDescription>
  </Location>

  <Character>
    <Key>Player</Key>
    <Name>You</Name>
    <Article />
    <Prefix />
    <Type>Player</Type>
    <Perspective>SecondPerson</Perspective>
    <Description>
      <Description>
        <DisplayWhen>StartDescriptionWithThis</DisplayWhen>
        <Text>As good-looking as ever.</Text>
      </Description>
    </Description>
    <Property>
      <Key>CharacterLocation</Key>
      <Value>At Location</Value>
    </Property>
    <Property>
      <Key>CharacterAtLocation</Key>
      <Value>LocationStart</Value>
    </Property>
  </Character>
</Adventure>
```

Real games also need Standard Library tasks/properties (movement, take/drop, etc.), merged from `StandardLibrary.amf`.
