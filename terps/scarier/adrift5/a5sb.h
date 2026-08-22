/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- growable output buffer (string builder).
 *
 * The turn loop accumulates a turn's rendered text into an sb_t before handing
 * it back to the host.  sb_pspace / sb_resolve_cls implement the Adrift 5 runner's
 * per-commit join-spacing and <cls> wipe semantics (A5_CLS_MARK, a5text.h) that
 * the a5run_* files share.  Split out of a5run.cpp; see a5run.h.
 */

#ifndef SCARIER_A5SB_H
#define SCARIER_A5SB_H

#include <stddef.h>

typedef struct { char *p; size_t len, cap; } sb_t;

void  sb_init   (sb_t *b);
void  sb_reserve (sb_t *b, size_t cap);
void  sb_puts   (sb_t *b, const char *s);
void  sb_putc   (sb_t *b, char c);
void  sb_putn   (sb_t *b, const char *s, size_t n);
char *sb_finish (sb_t *b);

/* Resolve one commit's markers within b->p[floor .. len).  The runner's Display
   renders per commit (bCommit=True flushes the accumulated sOutputText through
   one Source2HTML parse, where <cls> clears that call's buffer); so a <cls>
   wipes everything back to the LAST commit boundary, not the whole turn, and a
   <center>/<b> span still open when the commit ends dies with it.  `floor` is
   the offset of the current commit's start.  Headless, drops everything from
   `floor` up to and including the last <cls> marker in the range, leaving the
   surviving tail; interactive, instead leaves an A5_COMMIT_MARK at the
   boundary when the commit dangles a span, for the host to reset span state
   at (see a5text.h). */
void  sb_resolve_cls (sb_t *b, size_t floor);

/* <del>: delete the last output glyph before offset `from` (pass b->len for
   "the end of the buffer").  Walks back over presentation / ALR marks and
   whole spanning-mark payloads without removing them, then pops one UTF-8
   codepoint -- newlines included, so a <del> can undo a <br> or a paragraph
   break.  Returns the number of bytes removed, 0 when the walk found no glyph
   at all (an empty or marks-only buffer). */
size_t sb_del_glyph (sb_t *b, size_t from);

/* Apply every A5_DEL_MARK in the buffer to the accumulated turn text, in
   source order, removing the markers themselves.  <del> is a whole-turn
   operator in the runner (see a5text.h): a message that opens with <del> has
   no glyph of its own to eat, so the plain renderer leaves a marker and this
   pass -- finish_turn, after sb_resolve_cls, so a <cls> that wipes the marker
   wins -- deletes back across the pSpace join into the previous message. */
void  sb_resolve_del (sb_t *b);

/* Replace the oldn-byte span at `off` with the NUL-terminated `s` (grows or
   shrinks the buffer).  Out-of-range arguments are ignored. */
void  sb_splice (sb_t *b, size_t off, size_t oldn, const char *s);

/* The Adrift 5 runner's Global.pSpace: before appending the next Display() chunk, the
   accumulator gets two trailing spaces -- UNLESS it is empty or already ends in
   a newline (vbLf).  This is what joins a task's completion message and a
   turn-based event's message onto the same line ("...milk.  The postman...")
   while a message that ends in a newline keeps the next one on a fresh line.
   Scarier formerly forced a '\n' after every message, so multi-message turns
   diverged from the runner; call sb_pspace before each message instead. */
void  sb_pspace (sb_t *b);

#endif
