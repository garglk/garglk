/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Blorb / IFF container reader.
 * See a5blorb.h.  Modelled on Bocfel's iff.cpp/blorb.cpp.
 */

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
