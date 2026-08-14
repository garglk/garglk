/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- growable output buffer.  See a5sb.h.
 */

#include <stdlib.h>
#include <string.h>

#include "a5sb.h"
#include "a5text.h"   /* A5_CLS_MARK, A5_DEL_MARK */

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

/* True when c is a zero-width presentation / bookkeeping sentinel: <del> walks
   back over these without removing them, because they mark where something was
   rather than something the player can see. */
static int
a5_is_single_pres_mark (unsigned char c)
{
  return c == A5_ALR_MARK || c == A5_WAITKEY_MARK
      || c == A5_CENTER_MARK || c == A5_ENDCENTER_MARK
      || c == A5_BOLD_MARK || c == A5_ENDBOLD_MARK
      || c == A5_ITALIC_MARK || c == A5_ENDITALIC_MARK
      || c == A5_UNDERLINE_MARK || c == A5_ENDUNDERLINE_MARK
      || c == A5_RIGHT_MARK || c == A5_ENDRIGHT_MARK
      || c == A5_ENDCOLOUR_MARK || c == A5_ENDFONT_MARK
      || c == A5_ENDWINDOW_MARK
      || c == A5_CLS_MARK || c == A5_PS_MARK || c == A5_COMMIT_MARK
      || c == A5_DEL_MARK;
}

/* True when c opens/closes a payload span: \xNN…\xNN. */
static int
a5_is_spanning_pres_mark (unsigned char c)
{
  return c == A5_IMG_MARK || c == A5_WINDOW_MARK || c == A5_SOUND_MARK
      || c == A5_WAIT_MARK || c == A5_COLOUR_MARK || c == A5_FONT_MARK;
}

size_t
sb_del_glyph (sb_t *b, size_t from)
{
  size_t i = from;

  if (b->p == NULL || from > b->len)
    return 0;

  while (i > 0)
    {
      unsigned char c = (unsigned char) b->p[i - 1];

      if (a5_is_single_pres_mark (c))
        {
          i--;
          continue;
        }
      if (a5_is_spanning_pres_mark (c))
        {
          /* Skip the whole \xNN<payload>\xNN span; do not delete it. */
          size_t end = i - 1;
          size_t j = end;
          int found = 0;
          while (j > 0)
            {
              j--;
              if ((unsigned char) b->p[j] == c)
                {
                  i = j;
                  found = 1;
                  break;
                }
            }
          if (!found)
            i = end;
          continue;
        }
      /* Glyph ends at i (exclusive): drop one UTF-8 codepoint, keep any marks
         that trailed it in the buffer. */
      {
        size_t end = i;
        size_t n;
        while (i > 0
               && ((unsigned char) b->p[i - 1] & 0xC0) == 0x80)
          i--;
        if (i > 0)
          i--;
        n = b->len - end;
        if (n > 0)
          memmove (b->p + i, b->p + end, n);
        b->len = i + n;
        b->p[b->len] = '\0';
        return end - i;
      }
    }
  return 0;
}

void
sb_resolve_del (sb_t *b)
{
  size_t k = 0;

  if (b->p == NULL)
    return;
  while (k < b->len)
    {
      if ((unsigned char) b->p[k] != (unsigned char) A5_DEL_MARK)
        {
          k++;
          continue;
        }
      /* Everything the turn has emitted before this marker is in reach --
         including the pSpace join the previous message's tail was given.
         The delete shrinks the buffer ahead of k, so track the marker. */
      k -= sb_del_glyph (b, k);
      memmove (b->p + k, b->p + k + 1, b->len - k - 1);
      b->len--;
      b->p[b->len] = '\0';
    }
}

void
sb_resolve_cls (sb_t *b, size_t floor)
{
  size_t last = (size_t) -1, i;
  if (b->p == NULL || floor > b->len) return;
  /* Interactive hosts present the pre-<cls> text themselves and clear their
     window at the mark, so the wipe must not happen here (see a5text.h).
     But the commit boundary still closes every open span: the Runner renders
     each Display commit through its own Source2HTML parse, so a <center>,
     <right>, <b>, <i>, or colour tag the commit never closes dies with it --
     Death Shack's Introduction opens <center> without closing it, yet the
     first room description (the next commit) shows left-aligned.  When this
     commit dangles a span, leave an A5_COMMIT_MARK for the host to reset its
     span state at, placed before the commit's trailing whitespace so the
     buffer keeps the tail shape that sb_pspace and the finish_turn trim
     inspect. */
  if (a5text_interactive ())
    {
      int center = 0, bold = 0, italic = 0, underline = 0, right = 0, colour = 0;
      int font = 0;

      for (i = floor; i < b->len; i++)
        {
          char c = b->p[i];
          if (c == A5_CENTER_MARK) center++;
          else if (c == A5_ENDCENTER_MARK) { if (center > 0) center--; }
          else if (c == A5_BOLD_MARK) bold++;
          else if (c == A5_ENDBOLD_MARK) { if (bold > 0) bold--; }
          else if (c == A5_ITALIC_MARK) italic++;
          else if (c == A5_ENDITALIC_MARK) { if (italic > 0) italic--; }
          else if (c == A5_UNDERLINE_MARK) underline++;
          else if (c == A5_ENDUNDERLINE_MARK) { if (underline > 0) underline--; }
          else if (c == A5_RIGHT_MARK) right++;
          else if (c == A5_ENDRIGHT_MARK) { if (right > 0) right--; }
          else if (c == A5_COLOUR_MARK)
            {
              /* \027<value>\027 -- one span, two marks; skip to its close so
                 the value cannot be miscounted, then count it as open. */
              size_t j = i + 1;
              while (j < b->len && b->p[j] != A5_COLOUR_MARK) j++;
              if (j >= b->len) break;
              i = j;
              colour++;
            }
          else if (c == A5_ENDCOLOUR_MARK) { if (colour > 0) colour--; }
          else if (c == A5_FONT_MARK)
            {
              /* \010face\177size\010 -- same delimited shape as colour. */
              size_t j = i + 1;
              while (j < b->len && b->p[j] != A5_FONT_MARK) j++;
              if (j >= b->len) break;
              i = j;
              font++;
            }
          else if (c == A5_ENDFONT_MARK) { if (font > 0) font--; }

        }
      if (center > 0 || bold > 0 || italic > 0 || underline > 0
          || right > 0 || colour > 0 || font > 0)

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
  /* Test ONLY the final byte -- do not "improve" this by walking back over
     zero-width marks to find a buried newline.  Global.pSpace runs on the
     runner's RAW accumulated text, tags included, so a message whose source
     ends in a tag's '>' gets the join spaces even when the character the
     player sees last is a newline.  A trailing mark here means exactly
     "the raw text ended in a tag" (the del arm's A5_ALR_MARK, a stripped
     tag's ps_mark_trailing A5_PS_MARK), so mark != '\n' -> spaces is the
     runner's own answer.  Measured in run500.exe (delrt probe cases 9-11):
     "9[x\n\n<del>" + "]END9" really does render "9[x\n  ]END9" -- the join
     spaces are added even though a newline survives the <del>.

     A trailing <cls> marker is the one exception, treated like a trailing
     newline: the join spaces would otherwise be stranded before the marker
     and reappear as spurious leading whitespace once finish_turn drops
     everything up to the marker. */
  if (b->len > 0 && b->p[b->len - 1] != '\n' && b->p[b->len - 1] != A5_CLS_MARK)
    sb_puts (b, "  ");
}
