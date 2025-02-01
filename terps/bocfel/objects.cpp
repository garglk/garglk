// Copyright 2009-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include "objects.h"
#include "branch.h"
#include "memory.h"
#include "process.h"
#include "screen.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

static uint16_t find_object(uint16_t n)
{
    // Use 32-bit arithmetic to detect 16-bit overflow.
    uint32_t base = header.objects, object = n, addr;
    int objsize;

    if (zversion <= 3) {
        ZASSERT(n <= 255, "illegal object %u referenced", static_cast<unsigned int>(n));
        addr = base + (31 * 2) + (9 * (object - 1));
        objsize = 9;
    } else {
        addr = base + (63 * 2) + (14 * (object - 1));
        objsize = 14;
    }

    ZASSERT(addr + objsize < header.static_start, "object %u out of range", static_cast<unsigned int>(n));

    return addr;
}

#define OFFSET_PARENT	(zversion <= 3 ? 4 :  6)
#define OFFSET_SIBLING	(zversion <= 3 ? 5 :  8)
#define OFFSET_CHILD	(zversion <= 3 ? 6 : 10)
#define OFFSET_PROP	(zversion <= 3 ? 7 : 12)

#define parent_of(object)		relation(object, OFFSET_PARENT)
#define sibling_of(object)		relation(object, OFFSET_SIBLING)
#define child_of(object)		relation(object, OFFSET_CHILD)

#define set_parent(obj1, obj2)		set_relation(obj1, obj2, OFFSET_PARENT)
#define set_sibling(obj1, obj2)		set_relation(obj1, obj2, OFFSET_SIBLING)
#define set_child(obj1, obj2)		set_relation(obj1, obj2, OFFSET_CHILD)

static uint16_t property_address(uint16_t n)
{
    return word(find_object(n) + OFFSET_PROP);
}

static uint16_t relation(uint16_t object, int offset)
{
    return zversion <= 3 ? byte(find_object(object) + offset) : word(find_object(object) + offset);
}

// the 32 attribute flags     parent     sibling     child   properties
// ---32 bits in 4 bytes---   ---3 bytes------------------  ---2 bytes--
//
// the 48 attribute flags     parent    sibling   child     properties
// ---48 bits in 6 bytes---   ---3 words, i.e. 6 bytes----  ---2 bytes--
static void set_relation(uint16_t obj1, uint16_t obj2, int offset)
{
    if (zversion <= 3) {
        store_byte(find_object(obj1) + offset, obj2);
    } else {
        store_word(find_object(obj1) + offset, obj2);
    }
}

static void remove_object(uint16_t object)
{
    uint16_t parent = parent_of(object);

    if (parent != 0) {
        uint16_t child = child_of(parent);
        ZASSERT(child != 0, "malformed object table: parent has no children");

        // Direct child
        if (child == object) {
            // parent->child = parent->child->sibling
            set_child(parent, sibling_of(child));
        } else {
            while (sibling_of(child) != object) {
                // child = child->sibling
                child = sibling_of(child);
                ZASSERT(child != 0, "malformed object table: object to remove is not a child of its parent");
            }

            // Now the sibling of child is the object to remove.

            // child->sibling = child->sibling->sibling
            set_sibling(child, sibling_of(sibling_of(child)));
        }

        // object->parent = 0
        set_parent(object, 0);

        // object->sibling = 0
        set_sibling(object, 0);
    }
}

static uint16_t property_length(uint16_t propaddr)
{
    uint16_t length;
    // The address is to the data; the size byte is right before.
    uint8_t b = user_byte(propaddr - 1);

    if (zversion <= 3) {
        length = (b >> 5) + 1;
    } else {
        if ((b & 0x80) == 0x80) {
            length = b & 0x3f;
            if (length == 0) {
                length = 64;
            }
        } else {
            length = ((b & 0x40) == 0x40) ? 2 : 1;
        }
    }

    return length;
}

static uint8_t property_number(uint16_t propaddr)
{
    uint8_t propnum;

    if (zversion <= 3) {
        propnum = user_byte(propaddr - 1) & 0x1f;
    } else {
        if ((user_byte(propaddr - 1) & 0x80) == 0x80) {
            propnum = user_byte(propaddr - 2) & 0x3f;
        } else {
            propnum = user_byte(propaddr - 1) & 0x3f;
        }
    }

    return propnum;
}

static uint16_t advance_prop_addr(uint16_t propaddr)
{
    uint8_t size;

    size = user_byte(propaddr++);

    if (size == 0) {
        return 0;
    }

    if (zversion >= 4 && (size & 0x80) == 0x80) {
        propaddr++;
    }

    return propaddr;
}

static uint16_t first_property(uint16_t object)
{
    uint16_t propaddr = property_address(object);

    propaddr += (2 * user_byte(propaddr)) + 1;

    return advance_prop_addr(propaddr);
}

static uint16_t next_property(uint16_t propaddr)
{
    propaddr += property_length(propaddr);

    return advance_prop_addr(propaddr);
}

#define FOR_EACH_PROPERTY(object, addr) for (uint16_t addr = first_property(object); addr != 0; addr = next_property(addr))

static bool find_property(uint16_t object, uint16_t propnum, uint16_t &propaddr, uint16_t &proplen)
{
    FOR_EACH_PROPERTY(object, addr) {
        if (property_number(addr) == propnum) {
            propaddr = addr;
            proplen = property_length(addr);
            return true;
        }
    }

    return false;
}

static void check_attr(uint16_t attr)
{
    ZASSERT(attr <= (zversion <= 3 ? 31 : 47), "invalid attribute: %u", static_cast<unsigned int>(attr));
}

static bool is_zero(bool is_store, bool is_jump)
{
    if (zargs[0] == 0) {
        if (is_store) {
            store(0);
        }
        if (is_jump) {
            branch_if(false);
        }

        return true;
    }

    return false;
}

#define check_zero(store, jump)	do { if (is_zero(store, jump)) return; } while (false)

static void check_propnum(uint16_t propnum)
{
    ZASSERT(propnum > 0 && propnum < (zversion <= 3 ? 32 : 64), "invalid property: %u", static_cast<unsigned int>(propnum));
}

// Attributes are stored at the very beginning of an object, so the
// address find_object() returns refers directly to the attributes. The
// leftmost bit is attribute 0. Thus these attribute functions need to
// find out first which byte of the attributes to look at; this is done
// by dividing by 8. Attributes 0-7 will be in byte 0, 8-15 in byte 1,
// and so on. Then the particular bit is found. Attributes 0..7 are
// bits 7..0, attributes 8..15 are 7..0, and so on. Taking the
// remainder of the attribute divided by 8 gives the bit position,
// counting from the left, of the attribute.
#define ATTR_BIT(num)		(0x80U >> ((num) % 8))
void ztest_attr()
{
    check_zero(false, true);
    check_attr(zargs[1]);

    uint16_t addr = find_object(zargs[0]) + (zargs[1] / 8);

    branch_if((byte(addr) & ATTR_BIT(zargs[1])) != 0);
}

void zset_attr()
{
    check_zero(false, false);
    check_attr(zargs[1]);

    uint16_t addr = find_object(zargs[0]) + (zargs[1] / 8);

    store_byte(addr, byte(addr) | ATTR_BIT(zargs[1]));
}

void zclear_attr()
{
    check_zero(false, false);
    check_attr(zargs[1]);

    uint16_t addr = find_object(zargs[0]) + (zargs[1] / 8);

    store_byte(addr, byte(addr) & ~ATTR_BIT(zargs[1]));
}
#undef ATTR_BIT

void zremove_obj()
{
    check_zero(false, false);

    remove_object(zargs[0]);
}

void zinsert_obj()
{
    check_zero(false, false);

    remove_object(zargs[0]);

    set_sibling(zargs[0], child_of(zargs[1]));
    set_child(zargs[1], zargs[0]);
    set_parent(zargs[0], zargs[1]);
}

void zget_sibling()
{
    check_zero(true, true);

    uint16_t sibling = sibling_of(zargs[0]);

    store(sibling);
    branch_if(sibling != 0);
}

void zget_child()
{
    check_zero(true, true);

    uint16_t child = child_of(zargs[0]);

    store(child);
    branch_if(child != 0);
}

void zget_parent()
{
    check_zero(true, false);

    store(parent_of(zargs[0]));
}

void zput_prop()
{
    check_zero(false, false);
    check_propnum(zargs[1]);

    uint16_t propaddr, proplen;
    bool found;

    found = find_property(zargs[0], zargs[1], propaddr, proplen);

    ZASSERT(found, "broken story: no prop");

    if (proplen == 1) {
        user_store_byte(propaddr, zargs[2] & 0xff);
    } else if (proplen == 2) {
        user_store_word(propaddr, zargs[2]);
    } else {
        // As with @get_prop below, this is an invalid length, but
        // Photograph does this. It’s a bug in the game, but it’s pretty
        // easy to accidentally do in Inform. Modern Inform versions
        // detect it, but older ones didn’t, so it’s probably a safe
        // assumption that there are some other older games out there
        // which will trigger this as well. As with some other
        // interpreters, store a word for all invalid values.
        user_store_word(propaddr, zargs[2]);
    }
}

void zget_prop()
{
    check_zero(true, false);
    check_propnum(zargs[1]);

    uint16_t propaddr, proplen;

    if (find_property(zargs[0], zargs[1], propaddr, proplen)) {
        if (proplen == 1) {
            store(user_byte(propaddr));
        } else if (proplen == 2) {
            store(user_word(propaddr));
        } else {
            // If the proplen is > 2, the story file is misbehaving. At
            // least Christminster does this, and Frotz and Nitfol allow
            // it, reading a word, so do that here.
            store(user_word(propaddr));
        }
    } else {
        uint16_t i;

        i = header.objects + (2 * (zargs[1] - 1));
        store(word(i));
    }
}

void zget_prop_len()
{
    // Z-spec 1.1 says @get_prop_len 0 must yield 0.
    if (zargs[0] == 0) {
        store(0);
    } else {
        store(property_length(zargs[0]));
    }
}

void zget_prop_addr()
{
    check_zero(true, false);

    // Theoretically this should check whether the requested property is
    // valid (i.e. is within the proper range for the current story type).
    // However, at least one game (Mingsheng) attempts to get the address
    // of an invalid property (115); the standard could be read to
    // indicate that zero should be returned for *any* invalid property,
    // so do that.

    uint16_t propaddr, length;

    if (find_property(zargs[0], zargs[1], propaddr, length)) {
        store(propaddr);
    } else {
        store(0);
    }
}

void zget_next_prop()
{
    check_zero(true, false);

    uint16_t object = zargs[0], propnum = zargs[1], found_propnum = 0;
    bool next = false;

    FOR_EACH_PROPERTY(object, propaddr) {
        uint8_t current_propnum = property_number(propaddr);

        if (propnum == 0 || next) {
            found_propnum = current_propnum;
            break;
        }

        if (current_propnum == propnum) {
            next = true;
        }
    }

    store(found_propnum);
}

void zjin()
{
    // @jin 0 0 is not defined, since @jin requires an object (§15) and
    // object 0 is not actually an object (§12.3). However, many
    // interpreters yield a true value for this, and Torbjorn Andersson’s
    // strictz tester expects it to be true, so go with the flow.
    if (zargs[0] == 0 && zargs[1] == 0) {
        branch_if(true);
        return;
    }

    check_zero(false, true);

    branch_if(parent_of(zargs[0]) == zargs[1]);
}

void print_object(uint16_t obj, void (*outc)(uint8_t))
{
    if (obj == 0) {
        return;
    }

    print_handler(property_address(obj) + 1, outc);
}

void zprint_obj()
{
    check_zero(false, false);

    print_object(zargs[0], nullptr);
}
