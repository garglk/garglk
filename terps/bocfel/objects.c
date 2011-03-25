/*-
 * Copyright 2009-2011 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>

#include "objects.h"
#include "branch.h"
#include "memory.h"
#include "process.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

static uint16_t OBJECT(uint16_t n)
{
  /* Use 32-bit arithmetic to detect 16-bit overflow. */
  uint32_t base = header.objects, obj = n, addr;
  int objsize;

  if(zversion <= 3)
  {
    ZASSERT(n <= 255, "illegal object %u referenced", (unsigned)n);
    addr = base + (31 * 2) + (9 * (obj - 1));
    objsize = 9;
  }
  else
  {
    addr = base + (63 * 2) + (14 * (obj - 1));
    objsize = 14;
  }

  ZASSERT(addr + objsize < header.static_start, "object %u out of range", (unsigned)n);

  return addr;
}

#if 0
#define ZERO_OBJ()	warning("attempted operation on object zero");
#else
#define ZERO_OBJ()	((void)0)
#endif

#define OFFSET_PARENT	(zversion <= 3 ? 4 :  6)
#define OFFSET_SIBLING	(zversion <= 3 ? 5 :  8)
#define OFFSET_CHILD	(zversion <= 3 ? 6 : 10)
#define OFFSET_PROP	(zversion <= 3 ? 7 : 12)

#define PARENT(object)		RELATION(object, OFFSET_PARENT)
#define SIBLING(object)		RELATION(object, OFFSET_SIBLING)
#define CHILD(object)		RELATION(object, OFFSET_CHILD)

#define SET_PARENT(obj1, obj2)	SET_OBJECT(obj1, obj2, OFFSET_PARENT)
#define SET_SIBLING(obj1, obj2)	SET_OBJECT(obj1, obj2, OFFSET_SIBLING)
#define SET_CHILD(obj1, obj2)	SET_OBJECT(obj1, obj2, OFFSET_CHILD)

static uint16_t PROPADDR(uint16_t n)
{
  return WORD(OBJECT(n) + OFFSET_PROP);
}

static uint16_t RELATION(uint16_t object, int offset)
{
  return zversion <= 3 ? BYTE(OBJECT(object) + offset) : WORD(OBJECT(object) + offset);
}

/*
 * the 32 attribute flags     parent     sibling     child   properties
 * ---32 bits in 4 bytes---   ---3 bytes------------------  ---2 bytes--
 *
 * the 48 attribute flags     parent    sibling   child     properties
 * ---48 bits in 6 bytes---   ---3 words, i.e. 6 bytes----  ---2 bytes--
 */
static void SET_OBJECT(uint16_t obj1, uint16_t obj2, int offset)
{
  if(zversion <= 3) STORE_BYTE(OBJECT(obj1) + offset, obj2);
  else              STORE_WORD(OBJECT(obj1) + offset, obj2);
}

static void remove_obj(uint16_t object)
{
  uint16_t parent = PARENT(object);

  if(parent != 0)
  {
    uint16_t child = CHILD(parent);

    /* Direct child */
    if(child == object)
    {
      /* parent->child = parent->child->sibling */
      SET_CHILD(parent, SIBLING(child));
    }
    else
    {
      while(SIBLING(child) != object)
      {
        /* child = child->sibling */
        child = SIBLING(child);
      }

      /* Now the sibling of child is the object to remove. */

      /* child->sibling = child->sibling->sibling */
      SET_SIBLING(child, SIBLING(SIBLING(child)));
    }

    /* object->parent = 0 */
    SET_PARENT(object, 0);

    /* object->sibling = 0 */
    SET_SIBLING(object, 0);
  }
}

static uint16_t property_length(uint16_t propaddr)
{
  uint16_t length;
  /* The address is to the data; the size byte is right before. */
  uint8_t byte = user_byte(propaddr - 1);

  if(zversion <= 3)
  {
    length = (byte >> 5) + 1;
  }
  else
  {
    if(byte & 0x80)
    {
      length = byte & 0x3f;
      if(length == 0) length = 64;
    }
    else
    {
      length = (byte & 0x40) ? 2 : 1;
    }
  }

  return length;
}

static uint8_t PROPERTY(uint16_t addr)
{
  uint8_t propnum;

  if(zversion <= 3)
  {
    propnum = user_byte(addr - 1) & 0x1f;
  }
  else
  {
    if(user_byte(addr - 1) & 0x80) propnum = user_byte(addr - 2) & 0x3f;
    else                           propnum = user_byte(addr - 1) & 0x3f;
  }

  return propnum;
}

static uint16_t advance_prop_addr(uint16_t propaddr)
{
  uint8_t size;

  size = user_byte(propaddr++);

  if(size == 0) return 0;

  if(zversion >= 4 && (size & 0x80)) propaddr++;

  return propaddr;
}

static uint16_t first_property(uint16_t object)
{
  uint16_t propaddr = PROPADDR(object);

  propaddr += (2 * user_byte(propaddr)) + 1;

  return advance_prop_addr(propaddr);
}

static uint16_t next_property(uint16_t propaddr)
{
  propaddr += property_length(propaddr);

  return advance_prop_addr(propaddr);
}

#define FOR_EACH_PROPERTY(object, addr) for(uint16_t addr = first_property(object); addr != 0; addr = next_property(addr))

static int find_prop(uint16_t object, uint16_t property, uint16_t *propaddr, uint16_t *length)
{
  FOR_EACH_PROPERTY(object, addr)
  {
    if(PROPERTY(addr) == property)
    {
      *propaddr = addr;
      *length = property_length(addr);
      return 1;
    }
  }

  return 0;
}

static void check_attr(uint16_t attr)
{
  ZASSERT(attr <= (zversion <= 3 ? 31 : 47), "invalid attribute: %u", (unsigned)attr);
}

static int is_zero(int is_store, int is_jump)
{
  if(zargs[0] == 0)
  {
    ZERO_OBJ();

    if(is_store) store(0);
    if(is_jump)  branch_if(0);

    return 1;
  }

  return 0;
}

#define check_zero(store, jump)	do { if(is_zero(store, jump)) return; } while(0)

/* Attributes are stored at the very beginning of an object, so the
 * address OBJECT() returns refers directly to the attributes.  The
 * leftmost bit is attribute 0.  Thus these attribute functions need to
 * find out first which byte of the attributes to look at; this is done
 * by dividing by 8.  Attributes 0-7 will be in byte 0, 8-15 in byte 1,
 * and so on.  Then the particular bit is found.  Attributes 0..7 are
 * bits 7..0, attributes 8..15 are 7..0, and so on.  Taking the
 * remainder of the attribute divided by 8 gives the bit position,
 * counting from the left, of the attribute.
 */
#define ATTR_BIT(num)		(0x80U >> ((num) % 8))
void ztest_attr(void)
{
  check_zero(0, 1);
  check_attr(zargs[1]);

  uint16_t addr = OBJECT(zargs[0]) + (zargs[1] / 8);

  branch_if(BYTE(addr) & ATTR_BIT(zargs[1]));
}

void zset_attr(void)
{
  check_zero(0, 0);
  check_attr(zargs[1]);

  uint16_t addr = OBJECT(zargs[0]) + (zargs[1] / 8);

  STORE_BYTE(addr, BYTE(addr) | ATTR_BIT(zargs[1]));
}

void zclear_attr(void)
{
  check_zero(0, 0);
  check_attr(zargs[1]);

  uint16_t addr = OBJECT(zargs[0]) + (zargs[1] / 8);

  STORE_BYTE(addr, BYTE(addr) & ~ATTR_BIT(zargs[1]));
}
#undef ATTR_BIT

void zremove_obj(void)
{
  check_zero(0, 0);

  remove_obj(zargs[0]);
}

void zinsert_obj(void)
{
  check_zero(0, 0);

  remove_obj(zargs[0]);

  SET_SIBLING(zargs[0], CHILD(zargs[1]));
  SET_CHILD(zargs[1], zargs[0]);
  SET_PARENT(zargs[0], zargs[1]);
}

void zget_sibling(void)
{
  check_zero(1, 1);

  uint16_t sibling = SIBLING(zargs[0]);

  store(sibling);
  branch_if(sibling != 0);
}

void zget_child(void)
{
  check_zero(1, 1);

  uint16_t child = CHILD(zargs[0]);

  store(child);
  branch_if(child != 0);
}

void zget_parent(void)
{
  check_zero(1, 0);

  store(PARENT(zargs[0]));
}

void zput_prop(void)
{
  check_zero(0, 0);

  uint16_t propaddr, length;
  int found;

  found = find_prop(zargs[0], zargs[1], &propaddr, &length);

  ZASSERT(found, "broken story: no prop");
  ZASSERT(length == 1 || length == 2, "broken story: property too long: %u", (unsigned)length);

  if(length == 1) user_store_byte(propaddr, zargs[2] & 0xff);
  else            user_store_word(propaddr, zargs[2]);
}

void zget_prop(void)
{
  check_zero(1, 0);

  uint16_t propaddr, length;

  if(find_prop(zargs[0], zargs[1], &propaddr, &length))
  {
    if     (length == 1) store(user_byte(propaddr));
    else if(length == 2) store(user_word(propaddr));

    /* If the length is > 2, the story file is misbehaving.  At least
     * Christminster does this, and Frotz and Nitfol allow it, reading a
     * word, so do that here.
     */
    else                 store(user_word(propaddr));
  }
  else
  {
    uint32_t i;

    ZASSERT(zargs[1] < (zversion <= 3 ? 32 : 64), "invalid property: %u", zargs[1]);

    i = header.objects + (2 * (zargs[1] - 1));
    store(WORD(i));
  }
}

void zget_prop_len(void)
{
  /* Z-spec 1.1 says @get_prop_len 0 must yield 0. */
  if(zargs[0] == 0) store(0);
  else              store(property_length(zargs[0]));
}

void zget_prop_addr(void)
{
  check_zero(1, 0);

  uint16_t propaddr, length;

  if(find_prop(zargs[0], zargs[1], &propaddr, &length)) store(propaddr);
  else store(0);
}

void zget_next_prop(void)
{
  check_zero(1, 0);

  uint16_t object = zargs[0], property = zargs[1];
  int next = 0;
  int found_prop = 0;

  FOR_EACH_PROPERTY(object, propaddr)
  {
    uint8_t propnum = PROPERTY(propaddr);

    if(property == 0 || next)
    {
      found_prop = propnum;
      break;
    }

    if(propnum == property) next = 1;
  }

  store(found_prop);
}

void zjin(void)
{
  /* @jin 0 0 is not defined, since @jin requires an object (§15) and
   * object 0 is not actually an object (§12.3).  However, many
   * interpreters yield a true value for this, and Torbjorn Andersson’s
   * strictz tester expects it to be true, so go with the flow.
   */
  if(zargs[0] == 0 && zargs[1] == 0)
  {
    branch_if(1);
    return;
  }

  check_zero(0, 1);

  branch_if(PARENT(zargs[0]) == zargs[1]);
}

void print_object(uint16_t obj, void (*outc)(uint8_t))
{
  if(obj == 0) return;

  print_handler(PROPADDR(obj) + 1, outc);
}

void zprint_obj(void)
{
  check_zero(0, 0);

  print_object(zargs[0], NULL);
}
