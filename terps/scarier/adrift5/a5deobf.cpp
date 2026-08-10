/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- de-obfuscation + zlib inflate.
 * See a5deobf.h.
 */

#include <stdlib.h>
#include <string.h>

#include "zlib.h"

#include "a5deobf.h"

/*
 * The fixed 1024-byte XOR key used to obfuscate ADRIFT 5 payloads, copied
 * verbatim from the Adrift 5 runner's FileIO.ObfuscateByteArray (iRandomKey).
 */
static const uint8_t A5_KEY[1024] = {
  41, 236, 221, 117, 23, 189, 44, 187, 161, 96, 4, 147, 90, 91, 172, 159,
  244, 50, 249, 140, 190, 244, 82, 111, 170, 217, 13, 207, 25, 177, 18, 4,
  3, 221, 160, 209, 253, 69, 131, 37, 132, 244, 21, 4, 39, 87, 56, 203,
  119, 139, 231, 180, 190, 13, 213, 53, 153, 109, 202, 62, 175, 93, 161, 239,
  77, 0, 143, 124, 186, 219, 161, 175, 175, 212, 7, 202, 223, 77, 72, 83,
  160, 66, 88, 142, 202, 93, 70, 246, 8, 107, 55, 144, 122, 68, 117, 39,
  83, 37, 183, 39, 199, 188, 16, 155, 233, 55, 5, 234, 6, 11, 86, 76,
  36, 118, 158, 109, 5, 19, 36, 239, 185, 153, 115, 79, 164, 17, 52, 106,
  94, 224, 118, 185, 150, 33, 139, 228, 49, 188, 164, 146, 88, 91, 240, 253,
  21, 234, 107, 3, 166, 7, 33, 63, 0, 199, 109, 46, 72, 193, 246, 216,
  3, 154, 139, 37, 148, 156, 182, 3, 235, 185, 60, 73, 111, 145, 151, 94,
  169, 118, 57, 186, 165, 48, 195, 86, 190, 55, 184, 206, 180, 93, 155, 111,
  197, 203, 143, 189, 208, 202, 105, 121, 51, 104, 24, 237, 203, 216, 208, 111,
  48, 15, 132, 210, 136, 60, 51, 211, 215, 52, 102, 92, 227, 232, 79, 142,
  29, 204, 131, 163, 2, 217, 141, 223, 12, 192, 134, 61, 23, 214, 139, 230,
  102, 73, 158, 165, 216, 201, 231, 137, 152, 187, 230, 155, 99, 12, 149, 75,
  25, 138, 207, 254, 85, 44, 108, 86, 129, 165, 197, 200, 182, 245, 187, 1,
  169, 128, 245, 153, 74, 170, 181, 83, 229, 250, 11, 70, 243, 242, 123, 0,
  42, 58, 35, 141, 6, 140, 145, 58, 221, 71, 35, 51, 4, 30, 210, 162,
  0, 229, 241, 227, 22, 252, 1, 110, 212, 123, 24, 90, 32, 37, 99, 142,
  42, 196, 158, 123, 209, 45, 250, 28, 238, 187, 188, 3, 134, 130, 79, 199,
  39, 105, 70, 14, 0, 151, 234, 46, 56, 181, 185, 138, 115, 54, 25, 183,
  227, 149, 9, 63, 128, 87, 208, 210, 234, 213, 244, 91, 63, 254, 232, 81,
  44, 81, 51, 183, 222, 85, 142, 146, 218, 112, 66, 28, 116, 111, 168, 184,
  161, 4, 31, 241, 121, 15, 70, 208, 152, 116, 35, 43, 163, 142, 238, 58,
  204, 103, 94, 34, 2, 97, 217, 142, 6, 119, 100, 16, 20, 179, 94, 122,
  44, 59, 185, 58, 223, 247, 216, 28, 11, 99, 31, 105, 49, 98, 238, 75,
  129, 8, 80, 12, 17, 134, 181, 63, 43, 145, 234, 2, 170, 54, 188, 228,
  22, 168, 255, 103, 213, 180, 91, 213, 143, 65, 23, 159, 66, 111, 92, 164,
  136, 25, 143, 11, 99, 81, 105, 165, 133, 121, 14, 77, 12, 213, 114, 213,
  166, 58, 83, 136, 99, 135, 118, 205, 173, 123, 124, 207, 111, 22, 253, 188,
  52, 70, 122, 145, 167, 176, 129, 196, 63, 89, 225, 91, 165, 13, 200, 185,
  207, 65, 248, 8, 27, 211, 64, 1, 162, 193, 94, 231, 213, 153, 53, 111,
  124, 81, 25, 198, 91, 224, 45, 246, 184, 142, 73, 9, 165, 26, 39, 159,
  178, 194, 0, 45, 29, 245, 161, 97, 5, 120, 238, 229, 81, 153, 239, 165,
  35, 114, 223, 83, 244, 1, 94, 238, 20, 2, 79, 140, 137, 54, 91, 136,
  153, 190, 53, 18, 153, 8, 81, 135, 176, 184, 193, 226, 242, 72, 164, 30,
  159, 164, 230, 51, 58, 212, 171, 176, 100, 17, 25, 27, 165, 20, 215, 206,
  29, 102, 75, 147, 100, 221, 11, 27, 32, 88, 162, 59, 64, 123, 252, 203,
  93, 48, 237, 229, 80, 40, 77, 197, 18, 132, 173, 136, 238, 54, 225, 156,
  225, 242, 197, 140, 252, 17, 185, 193, 153, 202, 19, 226, 49, 112, 111, 232,
  20, 78, 190, 117, 38, 242, 125, 244, 24, 134, 128, 224, 47, 130, 45, 234,
  119, 6, 90, 78, 182, 112, 206, 76, 118, 43, 75, 134, 20, 107, 147, 162,
  20, 197, 116, 160, 119, 107, 117, 238, 116, 208, 115, 118, 144, 217, 146, 22,
  156, 41, 107, 43, 21, 33, 50, 163, 127, 114, 254, 251, 166, 247, 223, 173,
  242, 222, 203, 106, 14, 141, 114, 11, 145, 107, 217, 229, 253, 88, 187, 156,
  153, 53, 233, 235, 255, 104, 141, 243, 146, 209, 33, 5, 109, 122, 72, 125,
  240, 198, 131, 178, 14, 40, 8, 15, 182, 95, 153, 169, 71, 77, 166, 38,
  182, 97, 97, 113, 13, 244, 173, 138, 80, 215, 215, 61, 107, 108, 157, 22,
  35, 91, 244, 55, 213, 8, 142, 113, 44, 217, 52, 159, 206, 228, 171, 68,
  42, 250, 78, 11, 24, 215, 112, 252, 24, 249, 97, 54, 80, 202, 164, 74,
  194, 131, 133, 235, 88, 110, 81, 173, 211, 240, 68, 51, 191, 13, 187, 108,
  44, 147, 18, 113, 30, 146, 253, 76, 235, 247, 30, 219, 167, 88, 32, 97,
  53, 234, 221, 75, 94, 192, 236, 188, 169, 160, 56, 40, 146, 60, 61, 10,
  62, 245, 10, 189, 184, 50, 43, 47, 133, 57, 0, 97, 80, 117, 6, 122,
  207, 226, 253, 212, 48, 112, 14, 108, 166, 86, 199, 125, 89, 213, 185, 174,
  186, 20, 157, 178, 78, 99, 169, 2, 191, 173, 197, 36, 191, 139, 107, 52,
  154, 190, 88, 175, 63, 105, 218, 206, 230, 157, 22, 98, 107, 174, 214, 175,
  127, 81, 166, 60, 215, 84, 44, 107, 57, 251, 21, 130, 170, 233, 172, 27,
  234, 147, 227, 155, 125, 10, 111, 80, 57, 207, 203, 176, 77, 71, 151, 16,
  215, 22, 165, 110, 228, 47, 92, 69, 145, 236, 118, 68, 84, 88, 35, 252,
  241, 250, 119, 215, 203, 59, 50, 117, 225, 86, 2, 8, 137, 124, 30, 242,
  99, 4, 171, 148, 68, 61, 55, 186, 55, 157, 9, 144, 147, 43, 252, 225,
  171, 206, 190, 83, 207, 191, 68, 155, 227, 47, 140, 142, 45, 84, 188, 20
};

void
a5_deobfuscate (uint8_t *data, uint32_t offset, uint32_t length)
{
  uint32_t i;

  if (data == NULL)
    return;
  for (i = 0; i < length; i++)
    data[offset + i] ^= A5_KEY[i % 1024];
}

uint8_t *
a5_inflate (const uint8_t *data, uint32_t length, uint32_t *out_size)
{
  z_stream stream;
  uint8_t *out;
  size_t cap;
  int status;

  /* The result is handed back through a uint32_t *out_size, so a decompressed
     payload cannot usefully exceed 4 GB; cap growth here uses size_t and is
     bounded so the doubling can never overflow to 0 and hand inflate a bogus
     avail_out. */
  const size_t A5_INFLATE_MAX = 0xFFFFFFFFu;

  memset (&stream, 0, sizeof stream);
  if (inflateInit (&stream) != Z_OK)
    return NULL;

  /* Start with a generous guess and grow as needed (XML compresses well). */
  cap = (size_t) length * 8 + 1024;
  if (cap > A5_INFLATE_MAX)
    cap = A5_INFLATE_MAX;
  out = (uint8_t *) malloc (cap);
  if (out == NULL)
    {
      inflateEnd (&stream);
      return NULL;
    }

  stream.next_in = (Bytef *) data;
  stream.avail_in = length;
  stream.next_out = out;
  stream.avail_out = (uInt) cap;

  do
    {
      if (stream.avail_out == 0)
        {
          size_t used = cap;
          uint8_t *grown;

          if (cap >= A5_INFLATE_MAX)
            {
              /* Refuse to grow past the representable size instead of
                 wrapping the capacity and overflowing the buffer. */
              free (out);
              inflateEnd (&stream);
              return NULL;
            }
          cap = (cap > A5_INFLATE_MAX / 2) ? A5_INFLATE_MAX : cap * 2;
          grown = (uint8_t *) realloc (out, cap);
          if (grown == NULL)
            {
              free (out);
              inflateEnd (&stream);
              return NULL;
            }
          out = grown;
          stream.next_out = out + used;
          stream.avail_out = (uInt) (cap - used);
        }
      status = inflate (&stream, Z_NO_FLUSH);
    }
  while (status == Z_OK);

  if (status != Z_STREAM_END)
    {
      free (out);
      inflateEnd (&stream);
      return NULL;
    }

  /* NUL-terminate (grow by one if the buffer is exactly full). */
  if (stream.avail_out == 0)
    {
      uint8_t *grown = (uint8_t *) realloc (out, cap + 1);
      if (grown == NULL)
        {
          free (out);
          inflateEnd (&stream);
          return NULL;
        }
      out = grown;
    }
  out[stream.total_out] = '\0';

  *out_size = (uint32_t) stream.total_out;
  inflateEnd (&stream);
  return out;
}

uint8_t *
a5_deflate (const uint8_t *data, uint32_t length, uint32_t *out_size)
{
  z_stream stream;
  uint8_t *out;
  uint32_t cap;
  int status;

  memset (&stream, 0, sizeof stream);
  /* Default windowBits (15) => a plain zlib stream (0x78 header), matching
     .NET's ZLibStream; CompressionLevel.Optimal maps to Z_BEST_COMPRESSION. */
  if (deflateInit (&stream, Z_BEST_COMPRESSION) != Z_OK)
    return NULL;

  cap = length + (length >> 8) + 64;   /* deflateBound-ish generous guess */
  out = (uint8_t *) malloc (cap);
  if (out == NULL)
    {
      deflateEnd (&stream);
      return NULL;
    }

  stream.next_in = (Bytef *) data;
  stream.avail_in = length;
  stream.next_out = out;
  stream.avail_out = cap;

  do
    {
      if (stream.avail_out == 0)
        {
          uint32_t used = cap;
          uint8_t *grown;

          cap *= 2;
          grown = (uint8_t *) realloc (out, cap);
          if (grown == NULL)
            {
              free (out);
              deflateEnd (&stream);
              return NULL;
            }
          out = grown;
          stream.next_out = out + used;
          stream.avail_out = cap - used;
        }
      status = deflate (&stream, Z_FINISH);
    }
  while (status == Z_OK);

  if (status != Z_STREAM_END)
    {
      free (out);
      deflateEnd (&stream);
      return NULL;
    }

  *out_size = (uint32_t) stream.total_out;
  deflateEnd (&stream);
  return out;
}
