#ifndef ALREADY_INCLUDED_MEMBUF
#define ALREADY_INCLUDED_MEMBUF

/*
 * Copyright (c) 2002 - 2005 Magnus Lind.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software, alter it and re-
 * distribute it freely for any non-commercial, non-profit purpose subject to
 * the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software in a
 *   product, an acknowledgment in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not
 *   be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any distribution.
 *
 *   4. The names of this software and/or it's copyright holders may not be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 */

#define STATIC_MEMBUF_INIT                                                     \
  { 0, 0, 0 }

struct membuf {
  void *buf;
  int len;
  int size;
};

void membuf_init(struct membuf *sb);
void membuf_clear(struct membuf *sb);
void membuf_free(struct membuf *sb);
void membuf_new(struct membuf **sbp);
void membuf_delete(struct membuf **sbp);
int membuf_memlen(const struct membuf *sb);
void membuf_truncate(struct membuf *sb, int len);

/* returns the new len or < 0 if failure */
int membuf_trim(struct membuf *sb, int pos);

void *membuf_memcpy(struct membuf *sb, int offset, const void *mem, int len);
void *membuf_append(struct membuf *sb, const void *mem, int len);
void *membuf_append_char(struct membuf *sb, char c);
void *membuf_insert(struct membuf *sb, int offset, const void *mem, int len);
void membuf_remove(struct membuf *sb, int offset, int len);
void membuf_atleast(struct membuf *sb, int size);
void membuf_atmost(struct membuf *sb, int size);
int membuf_get_size(const struct membuf *sb);
void *membuf_get(const struct membuf *sb);

#endif
