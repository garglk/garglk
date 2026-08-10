/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Blorb / IFF container reader.
 * See a5blorb.h.  Modelled on Bocfel's iff.cpp/blorb.cpp.
 */

#include <string.h>

#include "a5blorb.h"

/* Read a big-endian 32-bit word. */
static uint32_t
a5_be32 (const uint8_t *p)
{
  return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16)
       | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}

int
a5blorb_find (const uint8_t *buf, uint32_t length,
              uint32_t usage, uint32_t number, a5_blorb_chunk_t *out)
{
  uint32_t pos, ridx_off, ridx_size, nresources, entry, index_;

  if (buf == NULL || length < 12)
    return 0;

  /* Outer wrapper must be FORM ... IFRS. */
  if (a5_be32 (buf) != A5_FOURCC ('F', 'O', 'R', 'M')
      || a5_be32 (buf + 8) != A5_FOURCC ('I', 'F', 'R', 'S'))
    return 0;

  /* Walk the top-level chunks looking for the resource index, RIdx. */
  ridx_off = 0;
  ridx_size = 0;
  pos = 12;
  while (pos + 8 <= length)
    {
      uint32_t ctype = a5_be32 (buf + pos);
      uint32_t csize = a5_be32 (buf + pos + 4);
      uint32_t body = pos + 8;

      if (ctype == A5_FOURCC ('R', 'I', 'd', 'x'))
        {
          ridx_off = body;
          ridx_size = csize;
          break;
        }
      /* Chunks are padded to an even length.  Guard the advance against a
         garbled csize that would overflow uint32 and wrap pos backwards
         (which would re-scan the same bytes forever). */
      if (csize > length - body)
        break;
      pos = body + csize + (csize & 1);
      if (pos < body)
        break;
    }

  if (ridx_off == 0 || ridx_size < 4 || ridx_off + 4 > length)
    return 0;

  /* RIdx body: count, then count * (usage, number, start) 12-byte entries. */
  nresources = a5_be32 (buf + ridx_off);
  entry = ridx_off + 4;
  for (index_ = 0; index_ < nresources; index_++, entry += 12)
    {
      uint32_t res_usage, res_number, start, ctype, csize;

      if (entry + 12 > length)
        break;

      res_usage = a5_be32 (buf + entry);
      res_number = a5_be32 (buf + entry + 4);
      start = a5_be32 (buf + entry + 8);

      if (res_usage != usage || res_number != number)
        continue;

      /* start is an untrusted 32-bit offset; test with subtraction so the
         bounds check cannot be bypassed by unsigned overflow of start + 8.
         (length >= 12 is guaranteed above, so length - 8 does not wrap.) */
      if (start > length - 8)
        return 0;

      ctype = a5_be32 (buf + start);
      csize = a5_be32 (buf + start + 4);

      /* Defensive clamp against a truncated/garbled file.  start + 8 <= length
         holds here, so length - (start + 8) does not wrap. */
      if (csize > length - (start + 8))
        csize = length - (start + 8);

      out->type = ctype;
      out->data = buf + start + 8;
      out->size = csize;
      return 1;
    }

  return 0;
}

int
a5blorb_find_exec (const uint8_t *buf, uint32_t length, a5_blorb_chunk_t *out)
{
  return a5blorb_find (buf, length, A5_USAGE_EXEC, 0, out);
}

int
a5blorb_find_layout (const uint8_t *buf, uint32_t length, a5_blorb_chunk_t *out)
{
  uint32_t pos;

  if (buf == NULL || length < 12)
    return 0;

  if (a5_be32 (buf) != A5_FOURCC ('F', 'O', 'R', 'M')
      || a5_be32 (buf + 8) != A5_FOURCC ('I', 'F', 'R', 'S'))
    return 0;

  pos = 12;
  while (pos + 8 <= length)
    {
      uint32_t ctype = a5_be32 (buf + pos);
      uint32_t csize = a5_be32 (buf + pos + 4);
      uint32_t body = pos + 8;

      if (csize > length - body)
        break;

      /* The Generator writes the layout through a data chunk, which is 'TEXT'
         unless something set it to 'BINA' (Blorb.vb, DataChunk.ID); either
         way the payload names its own type in its first four bytes. */
      if ((ctype == A5_FOURCC ('T', 'E', 'X', 'T')
           || ctype == A5_FOURCC ('B', 'I', 'N', 'A'))
          && csize > 4
          && a5_be32 (buf + body) == A5_FOURCC ('R', 'L', 'A', 'Y'))
        {
          out->type = ctype;
          out->data = buf + body + 4;
          out->size = csize - 4;
          return 1;
        }

      pos = body + csize + (csize & 1);
      if (pos < body)
        break;
    }

  return 0;
}

/* ------------------------------------------------ Runner layout (RLAY) XML */

/*
 * The layout is .NET SOAP, not the XmlTextWriter output a5xml.cpp reads: it
 * carries attributes, namespace prefixes and href back-references, none of
 * which that parser handles.  Only two facts are wanted from it, so scan for
 * them textually rather than build a second DOM.
 */

/* strstr over a buffer that need not be NUL-terminated. */
static const char *
a5_scan (const char *hay, size_t n, const char *needle)
{
  size_t nl = strlen (needle);
  size_t i;

  if (nl == 0 || n < nl)
    return NULL;
  for (i = 0; i + nl <= n; i++)
    {
      if (hay[i] == needle[0] && memcmp (hay + i, needle, nl) == 0)
        return hay + i;
    }
  return NULL;
}

/*
 * Inner text of the element starting at `el` (which points at its '<'), as a
 * pointer/length pair into the buffer.  Returns 0 for an empty or self-closing
 * element.
 */
static int
a5_elem_text (const char *el, const char *end,
              const char **text, size_t *text_len)
{
  const char *gt, *lt;

  gt = a5_scan (el, (size_t) (end - el), ">");
  if (gt == NULL || gt == el || gt[-1] == '/')
    return 0;
  lt = a5_scan (gt + 1, (size_t) (end - gt - 1), "<");
  if (lt == NULL)
    return 0;
  *text = gt + 1;
  *text_len = (size_t) (lt - gt - 1);
  return 1;
}

/*
 * The caption of the pane whose element body is [span, span_end), resolved
 * through the SOAP id/href indirection when the string is shared.  Returns 0
 * when the pane has no caption.
 */
static int
a5_pane_caption (const char *span, const char *span_end,
                 const char *doc, const char *doc_end,
                 const char **text, size_t *text_len)
{
  const char *el, *gt, *href, *quote, *target;
  char idref[80];
  size_t reflen;

  el = a5_scan (span, (size_t) (span_end - span), "<Text");
  if (el == NULL || span_end - el < 6
      || (el[5] != ' ' && el[5] != '>' && el[5] != '/'))
    return 0;

  gt = a5_scan (el, (size_t) (span_end - el), ">");
  if (gt == NULL)
    return 0;

  /* <Text href="#ref-24"/> -- the caption lives in the element with that id. */
  href = a5_scan (el, (size_t) (gt - el), "href=\"#");
  if (href == NULL)
    return a5_elem_text (el, span_end, text, text_len);

  href += strlen ("href=\"#");
  quote = a5_scan (href, (size_t) (gt - href), "\"");
  if (quote == NULL)
    return 0;
  reflen = (size_t) (quote - href);
  if (reflen == 0 || reflen + strlen ("id=\"\"") >= sizeof idref)
    return 0;
  memcpy (idref, "id=\"", 4);
  memcpy (idref + 4, href, reflen);
  idref[4 + reflen] = '"';
  idref[5 + reflen] = '\0';

  target = a5_scan (doc, (size_t) (doc_end - doc), idref);
  if (target == NULL)
    return 0;
  /* Back up to the '<' that opens the element carrying the id. */
  while (target > doc && *target != '<')
    target--;
  if (*target != '<')
    return 0;
  return a5_elem_text (target, doc_end, text, text_len);
}

int
a5blorb_layout_pane_open (const uint8_t *xml, uint32_t length,
                          const char *pane)
{
  static const char PANE_TAG[] = "DockableControlPane";
  const char *doc = (const char *) xml;
  const char *doc_end = doc + length;
  const char *at;
  size_t pane_len;

  if (xml == NULL || length == 0 || pane == NULL)
    return -1;
  pane_len = strlen (pane);

  at = doc;
  for (;;)
    {
      const char *span, *span_end, *caption;
      size_t caption_len;
      const char *lt;

      at = a5_scan (at, (size_t) (doc_end - at), PANE_TAG);
      if (at == NULL)
        return -1;
      span = at;
      at += sizeof PANE_TAG - 1;

      /* Skip the matching close tag: the name is preceded by "</", possibly
         through a namespace prefix ("</a1:DockableControlPane>"). */
      for (lt = span; lt > doc && *lt != '<'; lt--)
        ;
      if (*lt == '<' && lt[1] == '/')
        continue;

      /* Panes do not nest, so the element body runs to the next mention of the
         tag -- its own close tag, or the next pane if the file is truncated. */
      span_end = a5_scan (at, (size_t) (doc_end - at), PANE_TAG);
      if (span_end == NULL)
        span_end = doc_end;

      if (!a5_pane_caption (span, span_end, doc, doc_end,
                            &caption, &caption_len))
        continue;
      if (caption_len != pane_len || memcmp (caption, pane, pane_len) != 0)
        continue;

      /* Infragistics writes <Closed> only for a closed pane, but do not rely
         on the omission alone. */
      if (a5_scan (span, (size_t) (span_end - span), "<Closed>true<") != NULL)
        return 0;
      return 1;
    }
}
