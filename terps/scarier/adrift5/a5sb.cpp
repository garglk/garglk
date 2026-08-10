/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- growable output buffer.  See a5sb.h.
 */

#include <stdlib.h>
#include <string.h>

#include "a5sb.h"
#include "a5text.h"   /* A5_CLS_MARK */

void
sb_init (sb_t *b) { b->p = NULL; b->len = b->cap = 0; }

/* Grow the backing store to at least `cap` bytes up front (a size hint from
   the caller -- e.g. the previous turn's snapshot length), sparing the
   double-and-copy ladder sb_putn would otherwise climb. */
void
sb_reserve (sb_t *b, size_t cap)
{
  if (cap <= b->cap)
    return;
  b->p = (char *) realloc (b->p, cap);
  b->cap = cap;
  if (b->p != NULL)
    b->p[b->len] = '\0';
}

/* Append the n-byte span [s, s+n) verbatim.  The one place the buffer grows;
   sb_puts/sb_putc are length-computing wrappers around it. */
void
sb_putn (sb_t *b, const char *s, size_t n)
{
  if (s == NULL) return;
  if (b->len + n + 1 > b->cap)
    {
      size_t cap = b->cap ? b->cap : 128;
      while (cap < b->len + n + 1) cap *= 2;
      b->p = (char *) realloc (b->p, cap);
      b->cap = cap;
    }
  if (b->p == NULL) return;
  memcpy (b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}

void
sb_puts (sb_t *b, const char *s)
{
  if (s != NULL) sb_putn (b, s, strlen (s));
}

void
sb_putc (sb_t *b, char c) { sb_putn (b, &c, 1); }

char *
sb_finish (sb_t *b) { return b->p ? b->p : strdup (""); }

void
sb_resolve_cls (sb_t *b, size_t floor)
{
  size_t last = (size_t) -1, i;
  if (b->p == NULL || floor > b->len) return;
  /* Interactive hosts present the pre-<cls> text themselves and clear their
     window at the mark, so the wipe must not happen here (see a5text.h).
     But the commit boundary still closes every open span: the Runner renders
     each Display commit through its own Source2HTML parse, so a <center> or
     <b> the commit never closes dies with it -- Death Shack's Introduction
     opens <center> without closing it, yet the first room description (the
     next commit) shows left-aligned.  When this commit dangles a span, leave
     an A5_COMMIT_MARK for the host to reset its span state at, placed before
     the commit's trailing whitespace so the buffer keeps the tail shape that
     sb_pspace and the finish_turn trim inspect. */
  if (a5text_interactive ())
    {
      int center = 0, bold = 0;
      for (i = floor; i < b->len; i++)
        {
          char c = b->p[i];
          if (c == A5_CENTER_MARK) center++;
          else if (c == A5_ENDCENTER_MARK) { if (center > 0) center--; }
          else if (c == A5_BOLD_MARK) bold++;
          else if (c == A5_ENDBOLD_MARK) { if (bold > 0) bold--; }
        }
      if (center > 0 || bold > 0)
        {
          char mark[2] = { A5_COMMIT_MARK, '\0' };
          size_t at = b->len;
          /* Zero-width sentinels (pSpace/stripped-tag marks) count as tail
             here too, so finish_turn's strip and trailing-whitespace trim see
             the same byte run they did before the insertion. */
          while (at > floor
                 && (b->p[at - 1] == '\n' || b->p[at - 1] == '\r'
                     || b->p[at - 1] == ' ' || b->p[at - 1] == '\t'
                     || b->p[at - 1] == A5_PS_MARK
                     || b->p[at - 1] == A5_ALR_MARK))
            at--;
          sb_splice (b, at, 0, mark);
        }
      return;
    }
  for (i = floor; i < b->len; i++)
    if (b->p[i] == A5_CLS_MARK) last = i;
  if (last == (size_t) -1) return;
  memmove (b->p + floor, b->p + last + 1, b->len - (last + 1) + 1);
  b->len -= (last + 1 - floor);
}

void
sb_splice (sb_t *b, size_t off, size_t oldn, const char *s)
{
  size_t n = s != NULL ? strlen (s) : 0;
  if (b->p == NULL || off > b->len || off + oldn > b->len)
    return;
  if (n > oldn)
    {
      size_t grow = n - oldn;
      if (b->len + grow + 1 > b->cap)
        {
          size_t cap = b->cap ? b->cap : 128;
          while (cap < b->len + grow + 1) cap *= 2;
          b->p = (char *) realloc (b->p, cap);
          b->cap = cap;
        }
    }
  memmove (b->p + off + n, b->p + off + oldn, b->len - (off + oldn) + 1);
  if (n > 0)
    memcpy (b->p + off, s, n);
  b->len += n - oldn;
}

void
sb_pspace (sb_t *b)
{
  /* A trailing <cls> marker is treated like a trailing newline: the join spaces
     would otherwise be stranded before the marker and reappear as spurious
     leading whitespace once finish_turn drops everything up to the marker. */
  if (b->len > 0 && b->p[b->len - 1] != '\n' && b->p[b->len - 1] != A5_CLS_MARK)
    sb_puts (b, "  ");
}
