/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"

/* attribute/8 will be the byte offset into the object entry for attribute */
#define ATTRIBUTE(n, a) z_memory[z_objecttable + n * OBJSIZE + a / 8]

#define OBJ_ADDR(n)     (z_objecttable + (n) * OBJSIZE)

#define PARENTP(n)      (OBJ_ADDR(n) + oPARENT)
#define SIBLINGP(n)     (OBJ_ADDR(n) + oSIBLING)
#define CHILDP(n)       (OBJ_ADDR(n) + oCHILD)
#define PROPSP(n)       (OBJ_ADDR(n) + oPROPS)

#define PARENT(n)  LOWO(PARENTP(n))
#define SIBLING(n) LOWO(SIBLINGP(n))
#define CHILD(n)   LOWO(CHILDP(n))
#define PROPS(n)   LOWORD(PROPSP(n))


static zword OBJSIZE;
static zword oPARENT, oSIBLING, oCHILD, oPROPS;
static zword PROP_NUM_MASK, ATTR_COUNT;

static BOOL object_property_loop(zword object, zword *propnum,
				 zword *location, zword *len);


zword LOWO(zword p)
{
  if(zversion >= 4)
    return LOWORD(p);
  return LOBYTE(p);
}

void LOWOcopy(zword a, zword b)
{
  if(zversion >= 4) {
    LOWORDcopy(a, b);
  } else {
    LOBYTEcopy(a, b);
  }
}

void LOWOwrite(zword p, zword n)
{
  if(zversion >= 4) {
    LOWORDwrite(p, n);
  } else {
    LOBYTEwrite(p, n);
  }
}


#ifdef FAST
#define check_obj_valid(obj) TRUE
#define check_attr_valid(attr) TRUE
#else
static BOOL check_obj_valid(zword object)
{
  if(object > object_count) { /* Object past the first property table entry */
    if(object > maxobjs) {    /* Object past the end of dynamic memory */
      n_show_error(E_OBJECT, "object number too large", object);
      return FALSE;
    }
    n_show_warn(E_OBJECT, "object number probably too large", object);
  }
  if(object == 0) {
    n_show_error(E_OBJECT, "vile object 0 error from hell", object);
    return FALSE;
  }
  return TRUE;
}

static BOOL check_attr_valid(zword attr)
{
  if(attr > ATTR_COUNT) {
    n_show_error(E_OBJECT, "attempt to access illegal attribute", attr);
    return FALSE;
  }
  return TRUE;
}
#endif



void objects_init(void)
{
  zword object;

  if(zversion >= 4) {
    OBJSIZE = 14;
    oPARENT = 6;
    oSIBLING = 8;
    oCHILD = 10;
    oPROPS = 12;
    PROP_NUM_MASK = 0x3f;
    ATTR_COUNT = 47;
  } else {
    OBJSIZE = 9;
    oPARENT = 4;
    oSIBLING = 5;
    oCHILD = 6;
    oPROPS = 7;
    PROP_NUM_MASK = 0x1f;
    ATTR_COUNT = 31;
  }

  /* Address of objects; we want this to be one before the first object 
     because there's no object 0 */
  z_objecttable = z_propdefaults + PROP_NUM_MASK * ZWORD_SIZE - OBJSIZE;
  maxobjs = (dynamic_size - z_objecttable) / OBJSIZE;

  obj_first_prop_addr = ZWORD_MASK;
  obj_last_prop_addr = 0;

  prop_table_start = ZWORD_MASK;
  prop_table_end = 0;

  /* Count objects in tree, assuming objects end where proptables begin */
  for(object = 1; OBJ_ADDR(object) < prop_table_start; object++) {
    zword propnum, location, len;

    if(PROPS(object) < prop_table_start)
      prop_table_start = PROPS(object);

    location = 0;
    while(object_property_loop(object, &propnum, &location, &len)) {
      if(location < obj_first_prop_addr)
	obj_first_prop_addr = location;
      if(location > obj_last_prop_addr)
	obj_last_prop_addr = location;

      if(location + len > prop_table_end)
	prop_table_end = location + len;
    }
  }

  object_count = object - 1;

  if(object_count > maxobjs)
    object_count = maxobjs;
}




void op_get_child(void)
{
  zword child;
  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    mop_skip_branch();
    return;
  }

  debug_object(operand[0], OBJ_GET_INFO);

  child = CHILD(operand[0]);
  mop_store_result(child);
  mop_cond_branch(child != 0);
}


void op_get_parent(void)
{
  zword parent;
  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    return;
  }

  debug_object(operand[0], OBJ_GET_INFO);

  parent = PARENT(operand[0]);
  mop_store_result(parent);
}


void op_get_sibling(void)
{
  zword sibling;
  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    mop_skip_branch();
    return;
  }

  debug_object(operand[0], OBJ_GET_INFO);

  sibling = SIBLING(operand[0]);
  mop_store_result(sibling);
  mop_cond_branch(sibling != 0);
}



void op_insert_obj(void)
{
  zword object = operand[0], dest = operand[1];

  if(!check_obj_valid(object) || !check_obj_valid(dest))
    return;

#ifndef FAST
  {
    zword child = object;
    int depth = 0;
    while(child) {
      if(child == dest) {
	n_show_error(E_OBJECT, "attempt to place an object inside itself", object);
	return;
      }
      child = CHILD(child);
      depth++;
      if(depth > maxobjs) {
	n_show_error(E_OBJECT, "found objects inside themselves", child);
	break;
      }
    }
  }
#endif


  op_remove_obj(); /* Remove object operand[0] from object tree */

  debug_object(operand[1], OBJ_RECEIVE);

  LOWOwrite(PARENTP(object), dest);
  LOWOcopy(SIBLINGP(object), CHILDP(dest));
  LOWOwrite(CHILDP(dest), object);
}


void op_jin(void)
{
  if(!check_obj_valid(operand[0])) {
    mop_skip_branch();
    return;
  }

  debug_object(operand[0], OBJ_GET_INFO);
  debug_object(operand[1], OBJ_GET_INFO);

  mop_cond_branch(PARENT(operand[0]) == operand[1]);
}


void op_remove_obj(void)
{
  zword parent, sibling, nextsibling;
  zword object = operand[0];

  if(!check_obj_valid(object))
    return;
  parent = PARENT(object);

  if(!parent)   /* If no parent, do nothing with no error message */
    return;
  if(!check_obj_valid(parent))
    return;

  debug_object(operand[0], OBJ_MOVE);

  nextsibling = CHILD(parent);

  if(nextsibling == object) {  /* if it's the first child */
    LOWOcopy(CHILDP(parent), SIBLINGP(object));
  } else {
    unsigned width = 0;
    do { /* Loops until the SIBLING(sibling)==object so we can skip over it */
      sibling = nextsibling;
      
      if(!check_obj_valid(sibling))
	return;

      nextsibling = SIBLING(sibling);

#ifndef FAST
      if(width++ > maxobjs) {  /* If we've looped more times than reasonable */
	n_show_error(E_OBJECT, "looped sibling list", parent);
	return;
      }
#endif
    } while(nextsibling != object);
    
    LOWOcopy(SIBLINGP(sibling), SIBLINGP(object));/*make the list skip object*/
  }

  LOWOwrite(PARENTP(object), 0);  
  LOWOwrite(SIBLINGP(object), 0);
}

offset object_name(zword object)
{
  if(LOBYTE(PROPS(object)))
    return PROPS(object) + 1;
  else
    return 0;
}


void op_print_obj(void)
{
  offset short_name_off = object_name(operand[0]);
  if(short_name_off)
    decodezscii(short_name_off, output_char);
  else
    n_show_error(E_OBJECT, "attempt to print name of nameless object", operand[0]);
}


/* attribute opcodes: */

void op_clear_attr(void)
{
  if(!check_obj_valid(operand[0]) || !check_attr_valid(operand[1]))
    return;
  debug_attrib(operand[1], operand[0]);

  ATTRIBUTE(operand[0], operand[1]) &= ~(b10000000 >> (operand[1] & b0111));
  /* shift top bit right to select the appropriate bit and make
     a mask from the complement */
}


void op_set_attr(void)
{
  if(!check_obj_valid(operand[0]) || !check_attr_valid(operand[1]))
    return;
  debug_attrib(operand[1], operand[0]);
                                      /* select the bit to be set */
  ATTRIBUTE(operand[0], operand[1]) |= (b10000000 >> (operand[1] & b0111));
}


void op_test_attr(void)
{
  if(!check_obj_valid(operand[0]) || !check_attr_valid(operand[1])) {
    mop_skip_branch();
    return;
  }
  debug_attrib(operand[1], operand[0]);
                                       /* select the bit to be tested */
  if(ATTRIBUTE(operand[0], operand[1]) & (b10000000 >> (operand[1] & b0111)))
    mop_take_branch();
  else
    mop_skip_branch();
}


/* property table opcodes: */

/* Helper functions */

/*
 * Given the location of the sizebyte, returns the length of the following
 *  property and sets *size_length to the size of the sizebyte
 */
static zword get_prop_length(zword propoffset, int *size_length)
{
  zword prop_length;

  zbyte size_byte = LOBYTE(propoffset);
  if(zversion >= 4) {
    if(size_byte & b10000000) {  /* top bit set means two bytes of size info */
      *size_length = 2;
      prop_length = LOBYTE(propoffset + 1) & b00111111;
      if(prop_length == 0)
	prop_length = 64;
    } else {  /* one byte of size info */
      *size_length = 1;
      if(size_byte & b01000000)
	prop_length = 2;
      else
	prop_length = 1;
    }
  } else {
    prop_length = (size_byte >> 5) + 1;
    *size_length = 1;
  }
  return prop_length;
}


/*
 * Loops over all properties of an object, returning FALSE if no more
 * Before first call, set *location = 0;
 */
static BOOL object_property_loop(zword object, zword *propnum,
				 zword *location, zword *len)
{
  zword proptable;
  int size_byte, size_length;

  if(!*location) {
    *location = proptable = PROPS(object);
    *len = LOBYTE(proptable) * 2 + 1;   /* skip the header */
  }

  proptable  = *location;
  proptable += *len;
 
  size_byte  = LOBYTE(proptable);
  *propnum   = size_byte & PROP_NUM_MASK;
  if(*propnum) {
    *len       = get_prop_length(proptable, &size_length);
    proptable += size_length;

    *location  = proptable;
    return TRUE;
  }
  return FALSE;
}



static zword get_prop_offset(zword object, zword property, zword *length)
{
  zword propnum, location;
  location = 0;
  while(object_property_loop(object, &propnum, &location, length)) {
    if(propnum == property)
      return location;
  }
  return 0;
}


void op_get_next_prop(void)
{
  zword proptable = 0;
  zword prop_len;
  zword prop_num;

  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    return;
  }

  if(operand[1] == 0) {
    if(object_property_loop(operand[0], &prop_num, &proptable, &prop_len))
      mop_store_result(prop_num);
    else
      mop_store_result(0);
    return;
  }
  
  while(object_property_loop(operand[0], &prop_num, &proptable, &prop_len)) {
    if(prop_num == operand[1]) {
      if(object_property_loop(operand[0], &prop_num, &proptable, &prop_len))
	mop_store_result(prop_num);
      else
	mop_store_result(0);
      return;
    }
  }

  n_show_error(E_OBJECT, "get_next_prop on nonexistent property", operand[1]);
  mop_store_result(0);
  return;
}


void op_get_prop_addr(void)
{
  zword proptable;
  zword prop_len;
  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    return;
  }

  proptable = get_prop_offset(operand[0], operand[1], &prop_len);

  mop_store_result(proptable);
}


void op_get_prop_len(void)
{
  int size_length;
  zword prop_length;

#ifndef FAST
  if(operand[0] < obj_first_prop_addr ||
     operand[0] > obj_last_prop_addr) {
    if(operand[0] < 64) {
      n_show_error(E_OBJECT, "get property length in header", operand[0]);
      mop_store_result(0);
      return;
    }
    n_show_warn(E_OBJECT, "get property length at probably bad address", operand[0]);
  }
#endif

  operand[0]--;                    /* Skip back a byte for the size byte */

  if(zversion >= 4)
    if(LOBYTE(operand[0]) & 0x80) {  /* test top bit - two bytes of size info */
      operand[0]--;                  /* Skip back another byte */
    }

  prop_length = get_prop_length(operand[0], &size_length);

  mop_store_result(prop_length);
}


void op_get_prop(void)
{
  zword prop_length;
  zword proptable;
  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    return;
  }

  proptable = get_prop_offset(operand[0], operand[1], &prop_length);

  if(proptable == 0) {  /* property not provided; use property default */
    mop_store_result(LOWORD(z_propdefaults + (operand[1]-1) * ZWORD_SIZE));
    return;
  }

  switch(prop_length) {
  case 1:
    mop_store_result(LOBYTE(proptable)); break;
#ifndef FAST
  default:
    n_show_port(E_OBJECT, "get_prop on property with bad length", operand[1]);
#endif
  case ZWORD_SIZE:
    mop_store_result(LOWORD(proptable));
  }
}


void op_put_prop(void)
{
  zword prop_length;
  zword proptable;
  if(!check_obj_valid(operand[0])) {
    mop_store_result(0);
    return;
  }

  proptable = get_prop_offset(operand[0], operand[1], &prop_length);

#ifndef FAST
  if(proptable == 0) {
    n_show_error(E_OBJECT, "attempt to write to nonexistent property", operand[1]);
    return;
  }
#endif
  
  switch(prop_length) {
  case 1:
    LOBYTEwrite(proptable, operand[2]); break;
#ifndef FAST
  default:
    n_show_port(E_OBJECT, "put_prop on property with bad length", operand[1]);
#endif
  case ZWORD_SIZE:
    LOWORDwrite(proptable, operand[2]); break;
  }
}



#ifdef DEBUGGING


BOOL infix_property_loop(zword object, zword *propnum, zword *location, zword *len, zword *nonindivloc, zword *nonindivlen)
{
  BOOL status;

  if(*location && *propnum > PROP_NUM_MASK) {
    *location += *len;
    *propnum = LOWORD(*location);
    *location += ZWORD_SIZE;
    *len = LOBYTE(*location);
    (*location)++;

    if(*propnum)
      return TRUE;

    *propnum = 0;
    *location = *nonindivloc;
    *len = *nonindivlen;
    *nonindivloc = 0;
    *nonindivlen = 0;
  }

  status = object_property_loop(object, propnum, location, len);
  if(!status)
    return FALSE;

  if(*propnum == 3) { /* Individual property table */
    zword iproptable = LOWORD(*location);
    zword ilen;

    *propnum = LOWORD(iproptable);
    iproptable += ZWORD_SIZE;
    ilen = LOBYTE(iproptable);
    iproptable++;
    if(!*propnum)
      return infix_property_loop(object, propnum, location, len, nonindivloc, nonindivlen);

    *nonindivloc = *location;
    *nonindivlen = *len;
    *location = iproptable;
    *len = ilen;
  }

  return TRUE;
}


void infix_move(zword dest, zword object)
{
  zword to1 = operand[0], to2 = operand[1];
  operand[0] = object; operand[1] = dest;
  op_insert_obj();
  operand[0] = to1; operand[1] = to2;
}

void infix_remove(zword object)
{
  zword to1 = operand[0];
  operand[0] = object;
  op_remove_obj();
  operand[0] = to1;
}

zword infix_parent(zword object)
{
  return PARENT(object);
}

zword infix_child(zword object)
{
  return CHILD(object);
}

zword infix_sibling(zword object)
{
  return SIBLING(object);
}

void infix_set_attrib(zword object, zword attrib)
{
  zword to1 = operand[0], to2 = operand[1];
  operand[0] = object; operand[1] = attrib;
  op_set_attr();
  operand[0] = to1; operand[1] = to2;
}

void infix_clear_attrib(zword object, zword attrib)
{
  zword to1 = operand[0], to2 = operand[1];
  operand[0] = object; operand[1] = attrib;
  op_clear_attr();
  operand[0] = to1; operand[1] = to2;
}



static void infix_property_display(unsigned prop,
				   offset proptable, unsigned prop_length)
{
  BOOL do_number = TRUE;
  BOOL do_name = TRUE;

  unsigned i;

  /* things we know to be objects/strings/routines */
  static const char *decode_me_names[] = { 
    "n_to", "nw_to", "w_to", "sw_to", "s_to", "se_to", "e_to", "ne_to",
    "in_to", "out_to", "u_to", "d_to",
    "add_to_scope", "after", "article", "articles", "before", "cant_go",
    "daemon", "describe", "door_dir", "door_to", "each_turn", "found_in",
    "grammar", "initial", "inside_description", "invent", "life", "orders",
    "parse_name", "plural", "react_after", "react_before",
    "short_name", "short_name_indef", "time_out", "when_closed", "when_open",
    "when_on", "when_off", "with_key",
    "obj",
    "description",
    "ofclass"
  };

  /* things we know to be just plain numbers */
  static const char *dont_decode_names[] = {
    "capacity", "number", "time_left"
  };

  z_typed p;
  const char *propname;

  p.v = prop; p.t = Z_PROP;
  propname = infix_get_name(p);

  if(prop == 2)
    propname = "ofclass";
    
  infix_print_string(", ");
  
  if(propname)
    infix_print_string(propname);
  else {
    infix_print_string("P");
    infix_print_number(prop);
  }
  infix_print_string(" =");

  if(prop == 2) {
    for(i = 0; i < prop_length; i+=ZWORD_SIZE) {
      offset short_name_off = object_name(LOWORD(proptable + i));
      if(short_name_off) {
	infix_print_char(' ');
	decodezscii(short_name_off, infix_print_char);
      } else {
	infix_print_string(" <badclass>");
      }
    }
    return;
  }

  if(propname) {
    if(n_strcmp(propname, "name") == 0) {
      for(i = 0; i < prop_length; i+=ZWORD_SIZE) {
	infix_print_string(" '");
	decodezscii(LOWORD(proptable + i), infix_print_char);
	infix_print_char('\'');
      }
      return;
    }

    for(i = 0; i < sizeof(decode_me_names) / sizeof(*decode_me_names); i++)
      if(n_strcmp(decode_me_names[i], propname) == 0)
	do_number = FALSE;
      
    for(i = 0; i < sizeof(dont_decode_names) / sizeof(*dont_decode_names); i++)
      if(n_strcmp(dont_decode_names[i], propname) == 0)
	do_name = FALSE;
  }

  if(prop_length % ZWORD_SIZE || LOWORD(proptable) == 0) {
    do_number = TRUE;
    do_name = FALSE;
  }

  if(do_number) {
    switch(prop_length) {
    case 1:
      infix_print_char(' ');
      infix_print_znumber(LOBYTE(proptable));
      break;
    case ZWORD_SIZE:
      infix_print_char(' ');
      infix_print_znumber(LOWORD(proptable));
      break;
    default:
      for(i = 0; i < prop_length; i++) {
	infix_print_char(' ');
	infix_print_znumber(LOBYTE(proptable + i));
      }
    }
  }

  if(do_name) {
    for(i = 0; i < prop_length; i += ZWORD_SIZE) {
      zword val = LOWORD(proptable + i);
      const char *name = debug_decode_number(val);

      if(name) {
	infix_print_char(' ');
	if(do_number)
	  infix_print_char('(');
	infix_print_string(name);
	if(do_number)
	  infix_print_char(')');
      } else {
	if(!do_number) {
	  infix_print_char(' ');
	  infix_print_znumber(val);
	}
	if(val <= object_count) {
	  offset short_name_off = object_name(val);
	  if(short_name_off) {
	    infix_print_char(' ');
	    infix_print_char('(');
	    decodezscii(short_name_off, infix_print_char);
	    infix_print_char(')');
	  }
	}
      }
    }
  }
}


static void infix_show_object(zword object)
{
  const char *name;
  if(!object) {
    infix_print_string("0");
  } else {
    offset short_name_off;
    z_typed o;
    o.t = Z_OBJECT; o.v = object;
    name = infix_get_name(o);
    if(name) {
      infix_print_string(name);
    } else {
      infix_print_number(object);
    }

    short_name_off = object_name(object);
    if(short_name_off) {
      infix_print_string(" \"");
      decodezscii(short_name_off, infix_print_char);
      infix_print_char('"');
    } else if(!name) {
      infix_print_string(" <nameless>");
    }
  }
}

zword infix_get_proptable(zword object, zword prop, zword *length)
{
  zword propnum, location, nloc, nlen;

  location = 0;
  while(infix_property_loop(object, &propnum, &location, length, &nloc, &nlen)) {
    if(propnum == prop)
      return location;
  }

  return 0;
}


zword infix_get_prop(zword object, zword prop)
{
  zword prop_length;
  zword proptable = infix_get_proptable(object, prop, &prop_length);

  if(!proptable) {
    if(prop < PROP_NUM_MASK) { /* property defaults */
      proptable = z_propdefaults + (prop - 1) * ZWORD_SIZE;
      prop_length = ZWORD_SIZE;
    } else {
      return 0;
    }
  }

  switch(prop_length) {
  case 1:
    return LOBYTE(proptable);
  default:
  case ZWORD_SIZE: 
    return LOWORD(proptable);
  }
}


void infix_put_prop(zword object, zword prop, zword val)
{
  zword prop_length;
  zword proptable = infix_get_proptable(object, prop, &prop_length);

  if(!proptable)
    return;

  switch(prop_length) {
  case 1:
    LOBYTEwrite(proptable, val);
  default:
  case ZWORD_SIZE: 
    LOWORDwrite(proptable, val);
  }
}


BOOL infix_test_attrib(zword object, zword attrib)
{
  if(!check_obj_valid(object) || !check_attr_valid(attrib)) {
    return FALSE;
  }

                                       /* select the bit to be tested */
  if(ATTRIBUTE(object, attrib) & (b10000000 >> (attrib & b0111)))
    return TRUE;
  else
    return FALSE;
}


static char *trunk = NULL;
static int trunksize = 128;

static void infix_draw_trunk(int depth)
{
  int i;
  for(i = 1; i < depth; i++) {
    if(trunk[i])
      infix_print_fixed_string(" |  ");
    else
      infix_print_fixed_string("    ");
  }
}

static void infix_draw_branch(int depth)
{
  infix_draw_trunk(depth);
  if(depth)
    infix_print_fixed_string(" +->");
}


static void infix_draw_object(zword object, int depth)
{
  zword c;
  unsigned width;

  if(depth >= trunksize) {
    trunksize *= 2;
    trunk = (char *) n_realloc(trunk, trunksize);
  }

  infix_draw_branch(depth);
  infix_show_object(object);
  infix_print_char(10);

  /* Do a sanity check before we print anything to avoid screenfulls of junk */
  width = 0;
  for(c = CHILD(object); c; c = SIBLING(c)) {
    if(width++ > maxobjs) {
      infix_print_string("looped sibling list.\n");
      return;
    }
  }

  for(c = CHILD(object); c; c = SIBLING(c)) {
    if(PARENT(c) != object) { /* Section 12.5 (b) */
      infix_print_string("object ");
      infix_print_number(c);
      infix_print_string(" is a child of object ");
      infix_print_number(object);
      infix_print_string(" but has ");
      infix_print_number(PARENT(c));
      infix_print_string(" listed as its parent.\n");
      return;
    }

    trunk[depth+1] = (SIBLING(c) != 0);

    infix_draw_object(c, depth+1);
  }
}

void infix_object_tree(zword object)
{
  trunk = (char *) n_malloc(trunksize);

  if(object != 0) {
    infix_draw_object(object, 0);
    n_free(trunk);
    return;
  }

  for(object = 1; object <= object_count; object++) {
    if(!PARENT(object)) {
      if(SIBLING(object)) {  /* Section 12.5 (a) */
	infix_print_string("Object ");
	infix_print_number(object);
	infix_print_string(" has no parent, but has sibling ");
	infix_print_number(SIBLING(object));
	infix_print_string(".\n");
	return;
      }
      infix_draw_object(object, 0);
    }
  }  

  n_free(trunk);

}


/* Contrary to the zspec, short names may be arbitrarily long because of
   abbreviations, so use realloc */

static char *short_name;
static unsigned short_name_length;
static unsigned short_name_i;

static void infix_copy_short_name(int ch)
{
  if(short_name_i + 1 >= short_name_length ) {
    char *p;
    short_name_length *= 2;
    p = (char *) n_realloc(short_name, short_name_length);
    short_name = p;
  }
  short_name[short_name_i++] = ch;
}

void infix_object_find(const char *description)
{
  zword object;
  char *desc = n_strdup(description);
  n_strlower(desc);
  for(object = 1; object <= object_count; object++) {
    offset short_name_off = object_name(object);
    if(short_name_off) {
      short_name_length = 512;
      short_name = (char *) n_malloc(short_name_length);
      short_name_i = 0;
      decodezscii(short_name_off, infix_copy_short_name);
      short_name[short_name_i] = 0;
      n_strlower(short_name);
      if(n_strstr(short_name, desc)) {
	infix_show_object(object);
	if(PARENT(object)) {
	  infix_print_string(" in ");
	  infix_show_object(PARENT(object));
	}
	infix_print_char(10);
      }
      n_free(short_name);
    }
  }
}


void infix_object_display(zword object)
{
  offset short_name_off;
  zword propnum, location, length, nloc, nlen;
  unsigned n;
  BOOL did;

  if(object == 0) {
    infix_print_string("nothing");
    return;
  }

  if(!check_obj_valid(object)) {
    infix_print_string("invalid object");
    return;
  }

  infix_print_char('{');

  short_name_off = object_name(object);
  if(short_name_off) {
    infix_print_string("short_name = \"");
    decodezscii(short_name_off, infix_print_char);
    infix_print_string("\", attrib =");
  }

  did = FALSE;
  for(n = 0; n < ATTR_COUNT; n++) {
    if(infix_test_attrib(object, n)) {
      z_typed a;
      const char *attrname;
      a.t = Z_ATTR; a.v = n;
      attrname = infix_get_name(a);
      infix_print_char(' ');
      if(attrname)
	infix_print_string(attrname);
      else
	infix_print_number(n);
      did = TRUE;
    }
  }
  if(!did)
    infix_print_string(" <none>");

  infix_print_string(", parent = ");
  infix_show_object(PARENT(object));

  infix_print_string(", sibling = ");
  infix_show_object(SIBLING(object));

  infix_print_string(", child = ");
  infix_show_object(CHILD(object));


  location = 0;
  while(infix_property_loop(object, &propnum, &location, &length, &nloc, &nlen)) {
    infix_property_display(propnum, location, length);
  }

  infix_print_char('}');
}

#endif /* DEBUGGING */
