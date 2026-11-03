# common_utils: refactoring / cleanup / deduplication TODO

Findings from a read-through of `terps/common_utils/` and its callers. No code
was changed. Items are grouped by theme and roughly ordered by value within each
group. "Risk" is the chance of breaking a build or a terp, not the effort.

Files reviewed: `common_defines.h`, `common_file_utils.[ch]`, `common_utils.[ch]`,
`companion_search.[ch]`, `debugprint.h`, `layouttext.[ch]`, `memory_allocation.[ch]`,
`minmax.h`, `randomness.[ch]`, `read_le16.h`, `rgbsurface.h`.

---

## 1. Duplication with code elsewhere in the tree

### 1.1 `READ_LE_UINT16` / `READ_LE_UINT32` redefined in `unp64/exo_util.h`
- `terps/unp64/exo_util.h:39,52` defines its own non-static `inline` copies of
  `READ_LE_UINT16` and `READ_LE_UINT32`. In C99 a plain `inline` definition emits
  no external symbol, which is the exact problem `read_le16.h`'s comment says it
  avoids with `static inline`.
- They also differ subtly: the unp64 `READ_LE_UINT32` doesn't cast `b[2] << 16`
  to `uint32_t` (harmless for these byte values, but inconsistent).
- **TODO:** make `exo_util.h` include `read_le16.h` and delete its copies. Check
  that no TU includes both headers (currently a redefinition error if so, which
  is probably why they haven't been combined).
- Risk: low. Check `unp64` is built in the same include path as `common_utils`.

### 1.2 `ReadLE(p, nbytes)` in `decompressz80/decompressz80_internal.h`
- A third little-endian reader with a variable byte count. Probably not worth
  merging, but if `read_le16.h` gets a generic `READ_LE_UINT(ptr, n)` (see 3.2)
  it could use it.
- Risk: low; value: small. Optional.

### 1.3 `GetWord()` in `scott/ti994a/load_ti99_4a.c:76`
- A bounds-checked big-endian wrapper over `READ_BE_UINT16`. A bounds-checked
  read helper (`READ_BE_UINT16_CHECKED(base, size, ptr)` or similar) would let
  other detectors share it. Check `ti99_disk.c`, `apple2detect.c`,
  `atari8detect.c` for similar hand-rolled checks before deciding.

### 1.4 `GLK_*_ROCK` redefined in `comprehend/comprehend.h:22-24`
- Same values as `common_defines.h` (1, 1010, 1020). Comprehend is a C++/ScummVM
  derived terp so it may deliberately not use `common_utils`, but the numbers
  must stay in sync because `FindGlkWindowWithRock()` and the Swift/ObjC side
  rely on them.
- **TODO:** either include `common_defines.h` from comprehend, or split the three
  rock constants into their own header (`glk_rocks.h`) so they can be included
  without dragging in `MAX_LENGTH` etc.

### 1.5 `MIN`/`MAX` redefined in several terps
- `archetype/archetype_compat.h`, `comprehend/comprehend_compat.h`,
  `hugo/heglk/hecugel.c`, `tads/htmltads/win32/mpegamp/mpegamp.h` all define
  their own, with correct parenthesisation. Only `plus`, `taylor`, `scott`,
  `common_sagadraw` use `minmax.h`. Leave the third-party ones alone, but
  archetype and comprehend could use the shared header once it is fixed (2.1).

### 1.6 Glulxe's own copy of the xoshiro RNG
- `terps/glulxe/osdepend.c` contains the original of what `randomness.c` was
  adapted from (same `xo_*` functions). `randomness.c` is "Glulxe osdepend.c with
  the Glk bits stripped". Consider having glulxe use `randomness.[ch]` so there is
  one implementation, or at least note in `randomness.c` which Glulxe version it
  was taken from, to make future upstream merges easier.
- Risk: medium (autorestore state format, `erkyrath_random_{get,set}_detstate`
  must stay compatible with what glulxe/git serialise).

### 1.7 Vendored copies in `terps/scarier/common_utils/`
- `randomness.[ch]` and `rgbsurface.h` are byte-identical copies, checked by
  `make -f Makefile.headless vendorcheck` in scarier (see
  `scarier/common_utils/README`). That is fine as a deliberate Gargoyle-drop-in
  decision; nothing to change in the copy, but:
  - **TODO:** make `common_utils/rgbsurface.h` and `randomness.[ch]` header
    comments say "a copy lives in scarier/common_utils; run vendorcheck after
    editing". The comment in `rgbsurface.h` already does this; `randomness.[ch]`
    do not.
  - **TODO:** consider making the copies symlinks or generating them from a
    single source in the build instead of by hand, if Gargoyle's CMake can
    tolerate it (probably not, hence the copy).

### 1.8 Four hand-written `Display()` and two `CleanupAndExit()` implementations
- `Display()` is defined separately in `taylor/ui.c`, `scott/scott.c`,
  `plus/plusmain.c` (near-identical vsnprintf + glk_put_string). `CleanupAndExit`
  in `taylor/extracommands.c`, `scott/scott.c`, `plus/plusmain.c`.
- `common_utils.c` calls both through hand-written local prototypes and the
  `extern winid_t Bottom` (see 2.3), so `Fatal()` only links in terps that supply
  all three symbols. Moving `Display()` into `common_utils` (it only needs
  `glk_put_string_stream` and `vsnprintf`) would make `Fatal` self-contained
  apart from `CleanupAndExit`. Must check each variant: `scott` and `plus` may
  differ in how they handle style or the buffer size.

### 1.9 Window-resize idiom repeated in four places
- `glk_window_get_parent(Graphics)` + `glk_window_set_arrangement(parent,
  winmethod_Above | winmethod_Fixed, n, Graphics)` appears in `scott_display.c`
  (x2), `plusmain.c`, `taylor/ui.c`, plus once inside `ShowTextInTopWindow`.
  A tiny `SetTopWindowHeight(winid_t top, glui32 rows)` helper next to
  `ShowTextInTopWindow` would remove them.
- Similarly the "find Top / Graphics / Bottom by rock, open if missing" snippets in
  `title_image.c`, `loading_screen.c`, `scott_display.c`, `plusmain.c` and
  `taylor/ui.c` look like the same code. Worth a proper look; a
  `FindOrOpenGlkWindow()` could join `FindGlkWindowWithRock()` in `common_utils.c`.

### 1.10 Test stubs that mirror common_utils
- `scott/saga/test/c64test.c`, `extract_stubs.c`, `scenetest.c`, `filltest.c`,
  `zxvectortest.c`, `apple2test.c` each define their own `Fatal`, `MemAlloc`,
  `MemCalloc`, `FindGlkWindowWithRock`, `ShowTextInTopWindow`. There is also
  `scott/test/headless_stubs.c`. A single `common_utils/test_stubs.c` (Glk-free
  `Fatal` + allocators, headless) would remove a lot of copy-paste. Because
  `memory_allocation.c` only needs `Fatal`, it could be compiled as-is in all of
  them if `Fatal` were stubbed once.

---

## 2. Correctness / robustness cleanups

### 2.1 `minmax.h` macros are unparenthesised
```c
#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)
```
- `MIN(x & 1, y)` or `MAX(a + b, c)` with a lower-precedence operator, or any
  argument containing `?:`, `=`, `,` or a shift, expands wrongly. Compare the
  fully parenthesised versions in `archetype_compat.h` and `comprehend_compat.h`.
- **TODO:** `#define MIN(a,b) (((a) < (b)) ? (a) : (b))`, same for MAX. Grep
  callers in `plus`, `taylor`, `scott`, `common_sagadraw` for arguments with side
  effects (the double evaluation is unavoidable with a macro). Consider
  `static inline int min_int(int,int)` if any caller mixes signedness.
- Risk: low, but compiling the callers with `-Wsign-compare` first would show
  whether any argument type was accidentally relying on the bug.

### 2.2 `memory_allocation.h` has no includes
- Uses `size_t` without `#include <stddef.h>`; it only compiles because every
  includer happens to include `<stdlib.h>`/`<stdio.h>` first. Add
  `#include <stddef.h>`.
- `memory_allocation.c` forward-declares `__attribute__((noreturn)) void
  Fatal(const char *)` itself instead of including `common_utils.h` (which pulls
  in `glk.h`). The GCC-specific attribute breaks MSVC. Options: include
  `common_utils.h` (it already uses `GLK_ATTRIBUTE_NORETURN`), or move the
  `Fatal` prototype to a Glk-free header (e.g. `fatal.h`) so allocators and
  `common_file_utils.c` can use it in Glk-free tools such as `readwoz`.
- `MemAlloc(0)` / `MemCalloc(0)` / `MemRealloc(p, 0)` can return NULL on some
  libcs, which would call `Fatal("Out of memory")` for a legitimate request. Either
  document "size must be > 0" or treat 0 as 1.
- No overflow-checking variant (`MemCallocN(n, size)`); several callers likely
  multiply by hand. Grep for `MemAlloc(.* \* .*)` and add one if there are many.
- `MemStrndup(string, len)` does `memcpy(t, string, len)` without checking that
  `string` is at least `len` long (unlike `strndup`, which stops at NUL). Name is
  misleading; either use `memchr` for the length or rename to `MemCopyString`.
- There is no `MemStrdup`. Callers probably use `strdup` directly and ignore NULL.
  Check.

### 2.3 `Fatal()` in `common_utils.c`
- Declares `extern winid_t Bottom;`, `Display()` and `CleanupAndExit()` inline in
  the .c instead of in a header. `Bottom` is a global every terp must define
  with this exact name; `common_utils.h` should document that contract (or take
  the window via a setter/callback: `SetFatalWindow(winid_t)`).
- Prints `"%s!\n"` to stderr and the Glk window. `Display(Bottom, "%s!\n", x)`
  treats `x` as trusted; fine, but if x contains `%` it is not an issue since it
  is passed as an argument. OK. Just note that `Fatal` could be re-entered if
  `glk_window_open` itself calls `Fatal` (e.g. out of memory in a Glk
  implementation) - low risk.
- The file has no `#include "common_utils.h"`, so the compiler never checks that
  the definition matches the declaration. Add the include.
- The file header says "Created by Administrator" — see 4.3.

### 2.4 `common_file_utils.c`
- `ReadFileIfExists` opens with `"r"` instead of `"rb"`. Harmless on POSIX and
  macOS but corrupts binary data (`\r\n` translation, `0x1A` EOF) on Windows/MinGW
  builds, which this code seems to support (see the MSVC notes elsewhere).
  **TODO:** use `"rb"`.
- `GetFileLength` returns 0 for both "error" and "empty file", and leaves the file
  at offset 0 on success but at EOF on `ftell` failure. Return a signed type or a
  bool+out-param; `ReadFileIfExists` and callers (`scott/detect_game.c:1517`,
  `plus/plusmain.c:2533`) can then tell "empty" from "failed".
- `ReadFileIfExists` does `fseek(fptr, 0, SEEK_SET)` right after `GetFileLength`
  already did, so the second one is redundant.
- Doesn't check `*size == 0` result vs. `bytesread < *size` (short read): returns
  a buffer with uninitialised tail bytes while `*size` still holds the full length.
  Set `*size = bytesread` or fail.
- `size` is only written on success paths; on `fopen` failure the caller's value is
  unchanged. Set `*size = 0` at the top so callers can't act on stale values.
- No upper bound: a multi-GB file is slurped whole. `MAX_LENGTH` from
  `common_defines.h` is the usual cap in callers; consider a `ReadFileIfExistsMax`.
- `#include <stdlib.h>`/`<stdint.h>` are present, `<stdio.h>` comes from the header.
  OK. But `common_file_utils.h` uses `size_t` and `FILE`: fine because `<stdio.h>`.

### 2.5 `companion_search.c`
- `LookForCompanionFilename` uses a VLA: `char buffer[stringlen + FILENAME_BUFFER_EXTRA]`.
  VLAs are unavailable in MSVC and unbounded on the stack for a long path from
  user input. Replace with `MemAlloc`/`free`, or a fixed `PATH_MAX`-sized buffer
  with a length check.
- `tolower(gamefile[i])` etc. pass plain `char` (possibly negative for UTF-8
  filenames) to `<ctype.h>` functions: undefined behaviour. Cast to
  `unsigned char` everywhere (`is_companion_name`, `determine_type`,
  `FindCompanionFile`). `match_case` already does this correctly.
- `is_companion_name` re-implements "side"/"disk" matching with four hand-written
  char compares each. Replace with a small `matches_word_ci(filename, i - 3, "side")`
  helper (`strncasecmp` is not portable to MSVC; a 3-line static helper is).
- Magic numbers: `adjusted_index = i - 3`, `+ 2`, `i + 3`, `i + 4`, `i <= 3`. A
  short comment table or named offsets would make `FindCompanionFile` readable.
  The function mixes "find the pattern", "classify digit/letter" and "read the
  file"; consider splitting the first two into
  `ParseCompanionName(const char *name, size_t *index, CompanionNameType *type)`
  so it can be unit tested without touching the file system.
- `determine_type` for `'t'`/`'o'` checks `length > i + 4` but is called with
  `adjusted_index` which for the `before_extension` branch is `i - 3`; reasoning
  about it requires careful reading. Needs tests (see 5.1).
- In the loop, `c` is reused for three different meanings (lower-cased char,
  separator char, then compared against ' ', '_', '.'). Use separate variables.
- Mixed `//` and `/* */` comment styles, while the rest of the directory uses
  `/* */`. Pick one.
- `CompanionInsertBeforeExtension`: the `while (buffer[ppos] != '.' && ppos > 0)`
  scan starts at `namelen - 1` and can underflow if `namelen == 0` (callers
  guard, but the function is public). It searches backwards across directory
  separators, so `dir.v2/file` would insert into the directory name. Stop at
  path separator. Use `memmove` instead of the manual copy loop (the loop also
  relies on `size_t i >= ppos` with `ppos >= 1` to avoid wrap — fragile).
- `CompanionStripEnclosed`: uses `strrchr` for the open/close characters across
  the whole path, so `[` in a directory name can be matched; limit the search to
  the file-name part (after the last separator). `namelen` is only used for the
  `< MIN_FILENAME_LENGTH` test.
- `CompanionBracketFallback` hard-codes the string `"[cr CSS]"` (a particular
  cracker's tag, see Plus/ScottFree Atari loaders). Keep, but move to a
  `#define COMPANION_CRACK_TAG` in the .h so the comment there and the code agree.
- `CompanionFallback` has 8 parameters. Passing a `struct CompanionName {char
  *buffer; size_t bufsize, namelen; int index; CompanionNameType type;}` would
  halve it and let a future change add fields without touching every callback.
- Public enum values `TYPE_A`, `TYPE_B`, `TYPE_1`, … are very generic global names
  in a header that is included by C files in three terps. Prefix them
  (`COMPANION_TYPE_A` …); callers in `plus/companionfile.c`, `scott/saga/*detect.c`
  need a mechanical rename.
- `FILENAME_BUFFER_EXTRA` and `MIN_FILENAME_LENGTH` live in the .c, but fallbacks in
  other files (which insert text) need to know the extra room. Document in the
  header that `bufsize` is the only valid bound.
- `plus/companionfile.c` still exists next to the shared module. Check what is
  left in it; it may now be a thin wrapper that can be deleted.

### 2.6 `randomness.c`
- `#elif UNIX_RAND_GETRANDOM` should be `#elif defined(UNIX_RAND_GETRANDOM)`;
  `-Wundef` would flag it, and `-DUNIX_RAND_GETRANDOM` (no value) is an error.
- `#ifdef __APPLE__` block is closed with `/* OS_MAC */` — stale label.
- `time.h`/`stddef.h` are included twice (once at the top, again inside the
  `#ifndef RAND_SET_SEED` fallback). Keep the top one and drop the fallback's.
- Defines `TRUE`/`FALSE` locally; replace with `<stdbool.h>` (and `bool
  rand_use_native`). The detstate API uses `int usenative`, which should stay `int`
  for compatibility with glulxe/git callers.
- `xo_seed_random(time(NULL))` truncates `time_t` to `uint32_t`; fine, but two
  launches in the same second get the same sequence when `OS_STDC` is used.
  `arc4random`/`getrandom` paths avoid this. Mention in comment, or mix in
  `clock()`.
- `erkyrath_random_set_detstate` sets `rand_use_native` and silently ignores
  `count != 4`; `arr` is not null-checked.
- The header comment references `OS_UNIX`, `OS_WINDOWS`, `OS_STDC` which no longer
  mean anything to the Spatterlight build (the Mac build has `__APPLE__`). A short
  table in the header of which macro selects which path in which build system
  would help the Gargoyle/Windows maintainers.
- `randomness.h` guards with `extern "C"` (good) — `layouttext.h`,
  `companion_search.h`, `common_file_utils.h`, `common_utils.h`,
  `memory_allocation.h` don't. Add them in all headers for consistency
  (comprehend and other C++ terps may want them).
- Function names `erkyrath_random`, `set_erkyrath_random` are historical; a
  rename (`sl_random`, …) would be churn across twelve terps. Not recommended.

### 2.7 `layouttext.c`
- `LineBreakText` is dense; the inner break logic (`brk`/`skip` handling, two
  `if (brk == 0)` fallbacks) deserves an extracted `static size_t find_break(const
  char *line, size_t linelen, size_t width, size_t *skip)` with its own tests.
- `IsLineBreak` handles `\r`, `\n`, `\r\n`; `memcpy(result + out, line, brk)`
  trusts the `2 * len + 1` bound. A comment proves it; an `assert(out <= 2*len)`
  in debug builds would catch regressions.
- Strips leading `' '` and `'.'`: the `.` is specific to the Scott Adams room
  description convention and surprising in a "common" function. Make it a
  parameter or move to a caller wrapper.
- `ShowTextInTopWindow` calls `strchr` per line, then temporarily writes into the
  buffer it owns (`end[1] = '\0'`). Fine, but passing `print(top, line, len)` or a
  `glk_put_buffer`-style callback would remove the in-place edit.
- `glk_window_get_parent(top)` is not NULL-checked; callers guarantee a pair
  window, but a defensive check costs one line.
- `MIN_BOTTOM_ROWS` is a policy constant defined only in the .c; consider a
  header parameter if Taylor/Plus ever want a different value.
- No tests exist for either function (see 5.1) even though the text-wrapping
  rules are intricate.

### 2.8 `debugprint.h`
- `debug_print` only exists when `DEBUG` is defined, and then is further gated by
  `DEBUG_PRINT` (default 0). Two levels of switches that no longer match: with
  `DEBUG` defined and `DEBUG_PRINT 0`, the arguments are still compiled, but when
  `DEBUG` is undefined the macro vanishes and arguments are never type-checked
  (unused-variable warnings; stale format strings rot). Standard fix:
  `#define debug_print(...) do { if (DEBUG_PRINT) fprintf(stderr, __VA_ARGS__); } while (0)`
  with no `#ifdef DEBUG`.
- Uses `##__VA_ARGS__`, a GNU extension (MSVC accepts it as of 2019+ with
  `/Zc:preprocessor`). Use `__VA_ARGS__` with a mandatory format string, which is
  what every call site does anyway.
- The header comment says "Part of ScottFree"; it's shared by `readwoz`, `bocfel`,
  `plus`, `taylor`, `common_sagadraw`. Update.
- `#ifndef debug_print` guards a macro name (works only if the macro was
  `#define`d before); fine, but the file will `#define DEBUG_PRINT` itself only
  `#ifndef`. Document that `DEBUG_PRINT` is overridable per translation unit.

### 2.9 `read_le16.h`
- File name says `le16` but now holds LE32, BE16 and the `AND_ADVANCE` variants.
  Rename to `byteorder.h` (leaving a one-line `read_le16.h` forwarding header
  to avoid touching ~25 includers at once).
- Missing the natural counterparts: `READ_BE_UINT32`, `READ_LE_UINT16_AND_ADVANCE`'s
  32-bit versions, `WRITE_LE_UINT16/32`, `WRITE_BE_*`. Several callers
  (`plus/loaddatabase.c`, `scott/saga/saga.c`) probably open-code the BE32 read.
  Grep for `<< 24` in terps that include this header.
- `READ_LE_UINT16` computes `b[1] << 8` as `int`, then casts; OK. `READ_BE_UINT16_AND_ADVANCE`
  is written `(b[1] | (b[0] << 8))` while the others use `(b[x] << 8) | b[y]`:
  harmless but inconsistent. Implement the `AND_ADVANCE` ones in terms of the
  non-advancing ones.
- The `AND_ADVANCE` helpers take `uint8_t **` and can't be used with a `const
  uint8_t *` cursor; add `const` variants or make them macros-with-typeof (no).
- Macro-style names for inline functions (`READ_LE_UINT16`) are kept for
  ScummVM compatibility, fine.

### 2.10 `rgbsurface.h`
- 636 lines, of which ~200 are the two font tables. Splitting into
  `rgbsurface.h` (raster ops), `rgbsurface_fonts.h` (tables), and
  `rgbsurface_send.h` would help readers, but **the scarier copy must follow** and
  vendorcheck covers a single file today. Only do this if the copy list is
  updated in the same change.
- Every user includes the whole thing (including both font tables) as
  `static const` data; a TU that only uses `rgbsurf_fill_rect` still gets two
  unused 700-byte tables and `-Wunused-const-variable` noise. Wrap the tables in
  `#ifdef RGBSURF_WITH_FONTS`, or make the font accessors the only users and mark
  the tables `__attribute__((unused))`.
- `<math.h>` is needed only for `lround`/`sqrt` in `rgbsurf_line` /
  `rgbsurf_fill_circle`; builds must link `-lm` on some platforms. Both could be
  done with integer arithmetic (`isqrt` is already done manually in
  `fill_circle`).
- `rgbsurf_send` and `rgbsurf_send_rects` duplicate the run-scanning and the
  "shown matches" test. Implement `rgbsurf_send` as `rgbsurf_send_rects` with an
  adaptor, or factor a `rgbsurf__row_unchanged()` helper.
- `rgbsurf_send_rects` allocates two `malloc` buffers per call (every map redraw).
  Cache in the surface or take caller-provided scratch.
- `rgbsurf_nib` for `RGBSURF_NIB_ROUND` mixes `long` casts and `int` offsets;
  `rgbsurf_fill_circle` is the careful version with `long long`. Make `nib`'s round
  case call `rgbsurf_fill_circle(s, x, y, r, ...)` if the `r*r+1` fudge can be
  expressed there (it differs: nib uses `<= r*r + 1`). Would need a visual diff of
  the map output; the Scarier headless map dump tools can produce one.
- C++ callers get implicit `void*` conversions fixed by explicit casts on the
  `malloc`s (already done). Good. File is documented "C and C++".
- The ASCII tables are hard to audit; add a one-off generator or at least a
  self-check unit test that every glyph row fits in `w` bits (for the 5x7 font
  `bits >> 5 == 0`).

---

## 3. API / structure improvements

### 3.1 Split by dependency instead of by name
Today `common_utils.h` needs `glk.h`; `memory_allocation.c` needs `Fatal` (defined in
a Glk-dependent file); `common_file_utils.c` needs `MemAlloc`. Headless tools
(`readwoz`, `question/test`, `scott/saga/test`, `agility`, `archetype`) therefore
need stubs (1.10). Suggested layering:
1. **No dependencies:** `byteorder.h`, `minmax.h`, `debugprint.h`, `rgbsurface.h`,
   `randomness.[ch]`.
2. **Needs only `Fatal(const char*)`:** `memory_allocation.[ch]`,
   `common_file_utils.[ch]`, `companion_search.[ch]`. Put the `Fatal` prototype in
   `fatal.h` (no Glk).
3. **Needs Glk:** `common_utils.[ch]` (the Glk-backed `Fatal` and window finder),
   `layouttext.[ch]`.

### 3.2 `common_defines.h` mixes unrelated things
- `MAX_LENGTH` / `MIN_LENGTH` are limits on a game file/extent size (used by
  decrunchers and disk detectors); `MAX_WORDLENGTH` / `MAX_WORDS` are parser
  limits used by `scott/parser.c`; the three `GLK_*_ROCK`s are window ids.
  Names are far too generic for a header included by three terps
  (`jacl/constants.h` and `alan2/glkmedia.c` already define their own `MAX_WORDS`,
  so including `common_defines.h` there would break).
- **TODO:** split (`glk_rocks.h`, `game_limits.h`) and prefix
  (`GAMEFILE_MAX_LENGTH`, `GAMEFILE_MIN_LENGTH`, `PARSER_MAX_WORDS`…), or at
  least check where `MAX_WORDS` is actually used (grep found no users in
  `scott`/`taylor`/`plus`, which may mean it is dead).
- Document why `MAX_LENGTH` is 300000 (largest known disk image?), and why
  `MIN_LENGTH` is 24.

### 3.3 Includers pull `common_utils` headers through umbrella headers
`scott.h`, `plus/common.h`, `taylor/utility.h` each re-include several
`common_utils` headers, so individual `.c` files depend on them transitively.
Switching each .c to include what it uses (IWYU) would let the umbrella headers
shrink and make 3.1 possible. Low priority, high churn.

### 3.4 Build integration
- Only `scott/saga/test/Makefile` and `question/test/Makefile` reference
  `common_utils/*.c` by path; the Xcode project, CMake and the other Makefiles
  list them separately. Check all consumers list the same files, and consider a
  `common_utils/sources.mk` / CMake `common_utils.cmake` snippet that
  enumerates the `.c` files so adding a file doesn't need N edits.
- Add `-I` conventions: `scarier` includes `"common_utils/randomness.h"` but
  everyone else includes bare `"randomness.h"`. Pick one (the prefixed form is
  clearer and avoids a clash with `scarier`'s copy).

---

## 4. Style / hygiene

1. 4.1 Mixed indentation and brace styles: `rgbsurface.h` uses GNU style (2-space,
   space before `(`, braces on own line); the rest of the directory is 4-space
   K&R. Pick one for new code; reformatting the vendored header means editing the
   scarier copy too (vendorcheck).
2. 4.2 Add header guards that match the file name consistently:
   `RGBSURFACE_H` (upper) vs `layouttext_h` (lower) vs `DEBUG_PRINT`.
3. 4.3 Boilerplate Xcode headers ("Created by Administrator on 2025-12-20")
   carry no information; replace with a one-line description of the file's
   purpose (as `companion_search.h` and `rgbsurface.h` already have).
4. 4.4 `common_utils.c` has no `#include "common_utils.h"` and no
   `<stdlib.h>`; relies on `glk.h`.
5. 4.5 Public function names mix CamelCase (`MemAlloc`, `GetFileLength`,
   `LineBreakText`, `Companion*`), snake_case (`erkyrath_random`) and
   `rgbsurf_*`. Do not rename; just document the convention in a README.
6. 4.6 Add a `README.md` for the directory listing each module, its
   dependencies (3.1) and which terps use it, plus the vendored-copy rule. The
   only such note currently is in `scarier/common_utils/README`.
7. 4.7 `companion_search.h` says "Shared by Plus and ScottFree" but Taylor's
   `gameload.c:429` also reads a companion file by hand (`secondfile`). Check
   whether Taylor can use `FindCompanionFile` with a custom reader.

---

## 5. Tests

1. **5.1 Unit tests for the pure functions.** None of these touch Glk or the file
   system and they are the most intricate code in the directory:
   - `LineBreakText` (long words, runs of spaces, `\r\n`, leading `.`/space,
     `columns` <= 2, empty/NULL input, result size bound).
   - `CompanionStripEnclosed`, `CompanionInsertBeforeExtension`, and
     `FindCompanionFile` with a fake `CompanionReader` that records names
     requested: `Side 1`/`Side 2`, `disk_a`, `side one`/`two`, mixed case,
     `game1.dsk`, UTF-8 names, name with no extension, name at buffer limit, path
     with `[` in a directory.
   - `rgbsurf_line` clipping (far endpoints, dash phase) and `rgbsurf_send_rects`
     (rects cover exactly the same pixels as `rgbsurf_send`; a randomised
     comparison would be cheap).
   - `erkyrath_random` known-answer vector for a fixed seed, and
     `get_detstate`/`set_detstate` round trip.
2. 5.2 A `terps/common_utils/test/Makefile` running the above headless, wired
   into the same CI target as scarier's `vendorcheck`.
3. 5.3 Compile with `-Wall -Wextra -Wconversion -Wundef -fsanitize=address,undefined`
   once; the `tolower(char)` and `#elif UNIX_RAND_GETRANDOM` findings above
   come from reading, not from the compiler, so there are probably a few more.

---

## Suggested order of work

1. Mechanical, near-zero risk: 2.1 (MIN/MAX parens), 2.2 (`<stddef.h>`), 2.3/2.4
   (`#include` own header, `"rb"`, `*size = 0`), 2.5 `unsigned char` casts and
   VLA, 2.6 `#elif defined`, 2.8 `debug_print` shape, 4.x hygiene.
2. Dedup that removes real copies: 1.1 (`exo_util.h`), 1.4 (rocks), 1.8
   (`Display`), 1.9 (resize idiom), 1.10 (test stubs).
3. Structural: 3.1 layering (`fatal.h`), 3.2 split `common_defines.h`,
   `read_le16.h` -> `byteorder.h`.
4. Tests (5.x) — ideally before step 3, so the refactors are covered.
5. Riskier / optional: 1.6 (glulxe RNG), 2.10 (splitting `rgbsurface.h`), 2.5
   `CompanionFallback` signature and `TYPE_*` renames.
