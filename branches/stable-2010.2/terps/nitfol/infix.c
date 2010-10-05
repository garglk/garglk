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
#ifdef STDOUT_DEBUG
#include <stdio.h>
#endif


#ifdef DEBUGGING


#ifdef HEADER

typedef enum { Z_UNKNOWN, Z_BOOLEAN, Z_NUMBER, Z_OBJECT, Z_ROUTINE, Z_STRING, Z_GLOBAL, Z_LOCAL, Z_BYTEARRAY, Z_WORDARRAY, Z_OBJPROP, Z_ATTR, Z_PROP, Z_ARRAY } z_type;

typedef struct z_typed z_typed;
struct z_typed {
  zword v;      /* The value stored */
  z_type t;

  zword o, p;   /* location of value (if t is appropriate) */
};


typedef struct {
  const char *filename;
  strid_t stream;
  int num_lines;
  glui32 *line_locations;
} infix_file;

typedef struct {
  infix_file *file;
  int line_num;
  int line_x;
  const char *func_name;
  unsigned func_num;
  offset thisPC;
} infix_location;

#endif


#define EOF_DBR          0
#define FILE_DBR         1
#define CLASS_DBR        2
#define OBJECT_DBR       3
#define GLOBAL_DBR       4
#define ATTR_DBR         5
#define PROP_DBR         6
#define FAKE_ACTION_DBR  7
#define ACTION_DBR       8
#define HEADER_DBR       9
#define LINEREF_DBR     10
#define ROUTINE_DBR     11
#define ARRAY_DBR       12
#define MAP_DBR         13
#define ROUTINE_END_DBR 14
#define MAX_DBR 14

static unsigned record_info[][9] = {
  {      0,      0,      0, 0, 0,      0, 0, 0, 0 }, /* eof_dbr */
  {      1, 0x8000, 0x8000, 0, 0,      0, 0, 0, 0 }, /* file_dbr */
  { 0x8000,      1,      2, 1, 1,      2, 1, 0, 0 }, /* class_dbr */
  {      2, 0x8000,      1, 2, 1,      1, 2, 1, 0 }, /* object_dbr */
  {      1, 0x8000,      0, 0, 0,      0, 0, 0, 0 }, /* global_dbr */
  {      2, 0x8000,      0, 0, 0,      0, 0, 0, 0 }, /* attr_dbr */
  {      2, 0x8000,      0, 0, 0,      0, 0, 0, 0 }, /* prop_dbr */
  {      2, 0x8000,      0, 0, 0,      0, 0, 0, 0 }, /* fake_action_dbr */
  {      2, 0x8000,      0, 0, 0,      0, 0, 0, 0 }, /* action_dbr */
  {     64,      0,      0, 0, 0,      0, 0, 0, 0 }, /* header_dbr */
  {      2,      2,      0, 0, 0,      0, 0, 0, 0 }, /* lineref_dbr */
  {      2,      1,      2, 1, 3, 0x8000, 0, 0, 0 }, /* routine_dbr */
  {      2, 0x8000,      0, 0, 0,      0, 0, 0, 0 }, /* array_dbr */
  {      0,      0,      0, 0, 0,      0, 0, 0, 0 }, /* map_dbr */
  {      2,      1,      2, 1, 3,      0, 0, 0, 0 }  /* routine_end_dbr */
};


offset infix_get_routine_PC(zword routine)
{
  offset destPC = UNPACKR(routine);
  unsigned num_local = HIBYTE(destPC);
  destPC++;
  if(zversion < 5)
    destPC += num_local * ZWORD_SIZE;
  return destPC;
}


static infix_file infix_load_file_info(const char *filename)
{
  infix_file r;
  glsi32 c;
  int lines_allocated = 0;
					  
  r.filename = filename;
  r.num_lines = 0;
  r.line_locations = NULL;

  r.stream = n_file_name(fileusage_Data | fileusage_TextMode,
			 filemode_Read, filename);

  if(!r.stream)
    return r;

  lines_allocated = 256;
  r.line_locations = (glui32 *) n_malloc(lines_allocated * sizeof(*r.line_locations));
  r.line_locations[r.num_lines] = 0;
  r.num_lines++;

  while((c = glk_get_char_stream(r.stream)) != GLK_EOF) {
    if(c == 10) {
      if(r.num_lines >= lines_allocated) {
	glui32 *new_locations;

	lines_allocated *= 2;
	new_locations = (glui32 *) n_realloc(r.line_locations,
				  lines_allocated * sizeof(*r.line_locations));
	r.line_locations = new_locations;
      }
      r.line_locations[r.num_lines] = glk_stream_get_position(r.stream);
      r.num_lines++;
    }
  }
  return r;
}


static void infix_unload_file_info(infix_file *f)
{
  if(f->line_locations)
    n_free(f->line_locations);
  f->line_locations = 0;
  return;
}


void infix_file_print_line(infix_file *f, int line)
{
  glsi32 c;

  if(!(f && f->stream && f->num_lines && f->line_locations))
    return;

  if(line <= 0)
    line = 1;
  if(line > f->num_lines)
    line = f->num_lines;

  if(fullname) {  /* Running as a subprocess under emacs */
    infix_print_char(26);
    infix_print_char(26);
    infix_print_string(f->filename);
    infix_print_char(':');
    infix_print_number(line);
    infix_print_char(':');
    infix_print_number(f->line_locations[line-1] + 0);
    infix_print_string(":beg:0x00000000");
  } else {
    infix_print_number(line);
    infix_print_char('\t');
    
    glk_stream_set_position(f->stream, f->line_locations[line-1], seekmode_Start);

    while((c = glk_get_char_stream(f->stream)) != GLK_EOF) {
      if(c == 10)
	break;
      infix_print_char(c);
    }
  }

  infix_print_char(10);
}


static unsigned local_names_info[] = { 0x8000, 0 };
static unsigned sequence_point_info[] ={ 1, 2, 1, 2, 0 };
static unsigned map_info[] = { 0x8000, 3, 0 };

static glui32 infix_strlen;
static char *infix_stringdata;

static glui32 infix_add_string(strid_t infix)
{
  glui32 ch;
  glui32 return_offset = infix_strlen;

  while((ch = glk_get_char_stream(infix)) != 0) {
    if(infix_stringdata)
      infix_stringdata[infix_strlen] = ch;
    infix_strlen++;
  }

  if(infix_stringdata)
    infix_stringdata[infix_strlen] = 0;
  infix_strlen++;

  return return_offset;
}

static void infix_add_stringchar(int c)
{
  /* Really inefficient to call realloc for every character, but oh well... */
  infix_stringdata = n_realloc(infix_stringdata, infix_strlen+1);
  infix_stringdata[infix_strlen++] = c;
}

static glui32 infix_add_zstring(zword paddr)
{
  if(paddr) {
    glui32 return_offset = infix_strlen;
    offset addr = UNPACKS(paddr);
    if(addr+2 < total_size) {
      decodezscii(addr, infix_add_stringchar);
      infix_add_stringchar(0);
      return return_offset;
    }
  }
  return 0xffffffffL;
}


static infix_file *infix_files;
static unsigned infix_filescount;

static char **infix_objects;
static unsigned infix_objectscount;

static char **infix_globals;
static unsigned infix_globalscount;

typedef struct {
  zword address;
  const char *name;
} infix_arrayref;

static infix_arrayref *infix_arrays;
static unsigned infix_arrayscount;

static char **infix_attrs;
static unsigned infix_attrscount;

static char **infix_props;
static unsigned infix_propscount;

typedef struct {
  const char *name;
  unsigned filenum;
  unsigned startline;
  unsigned start_x;
  offset start_PC;
  const char *localnames[15];
  unsigned end_line;
  unsigned end_x;
  offset end_PC;
} routineref;

static routineref *infix_routines;
static unsigned infix_routinescount;

typedef struct {
  unsigned routine;
  unsigned filenum;
  unsigned line;
  unsigned x;
  offset PC;
} infix_sequence;

static infix_sequence *infix_linerefs;
static unsigned infix_linerefscount;


static offset code_start = 0;
static offset string_start = 0;

static int infix_compare_linerefs(const void *a, const void *b)
{
  const infix_sequence *A = (const infix_sequence *) a;
  const infix_sequence *B = (const infix_sequence *) b;
  if(A->PC < B->PC)
    return -1;
  if(A->PC > B->PC)
    return 1;

  if(A->line < B->line)
    return -1;
  if(A->line > B->line)
    return 1;
  return 0;
}

static BOOL infix_load_records(strid_t infix)
{
  unsigned n;

  for(;;) {
    glui32 record_type = glk_get_char_stream(infix);
    glui32 record_data[64];
    glui32 more_data[4];
    if(record_type > MAX_DBR) {
      glk_stream_close(infix, NULL);
      n_show_error(E_DEBUG, "unknown record type", record_type);
      return FALSE;
    }

    fillstruct(infix, record_info[record_type], record_data, infix_add_string);
    
    switch(record_type) {
    case EOF_DBR:
      if(infix_linerefs && infix_routines && code_start) {
	for(n = 0; n < infix_routinescount; n++) {
	  infix_routines[n].start_PC += code_start;
	  infix_routines[n].end_PC += code_start;
	}

	n_qsort(infix_linerefs, infix_linerefscount, sizeof(*infix_linerefs),
		infix_compare_linerefs);

	for(n = 0; n < infix_linerefscount; n++)
	  infix_linerefs[n].PC += code_start;
      }
      return TRUE;
    case FILE_DBR:
      if(infix_files) {
	infix_files[record_data[0]] = infix_load_file_info(infix_stringdata + record_data[2]);
      }
      if(record_data[0] >= infix_filescount)
	infix_filescount = record_data[0] + 1;
      break;
    case CLASS_DBR:
      break;
    case OBJECT_DBR:
      if(infix_objects) {
	infix_objects[record_data[0]] = infix_stringdata + record_data[1];
      }
      if(record_data[0] >= infix_objectscount)
	infix_objectscount = record_data[0] + 1;
      break;
    case GLOBAL_DBR:
      if(infix_globals) {
	infix_globals[record_data[0]] = infix_stringdata + record_data[1];
      }
      if(record_data[0] >= infix_globalscount)
	infix_globalscount = record_data[0] + 1;
      break;
    case ARRAY_DBR:
      if(infix_arrays) {
      }
      infix_arrayscount++;
      break;
    case ATTR_DBR:
      if(infix_attrs) {
	infix_attrs[record_data[0]] = infix_stringdata + record_data[1];
      }
      if(record_data[0] >= infix_attrscount)
	infix_attrscount = record_data[0] + 1;
      break;
    case PROP_DBR:
      if(infix_props) {
	infix_props[record_data[0]] = infix_stringdata + record_data[1];
      }
      if(record_data[0] >= infix_propscount)
	infix_propscount = record_data[0] + 1;
      break;
    case FAKE_ACTION_DBR:
      break;
    case ACTION_DBR:
      break;
    case HEADER_DBR:
      glk_stream_set_position(current_zfile, 0, seekmode_Start);
      for(n = 0; n < 64; n++) {
	unsigned c = (unsigned char) glk_get_char_stream(current_zfile);
	if(record_data[n] != c)  {
	  n_show_error(E_DEBUG, "infix file does not match current file", n);
	  return FALSE;
	}
      }
      break;
    case ROUTINE_DBR:
      if(infix_routines) {
	infix_routines[record_data[0]].filenum   = record_data[1];
	infix_routines[record_data[0]].startline = record_data[2];
	infix_routines[record_data[0]].start_x   = record_data[3];
	infix_routines[record_data[0]].start_PC  = record_data[4];
	infix_routines[record_data[0]].name = infix_stringdata +
	                                                        record_data[5];
      }

      if(infix_routines)
	for(n = 0; n < 15; n++)
	  infix_routines[record_data[0]].localnames[n] = NULL;

      for(n = 0; n < 16; n++) {
	if(!glk_get_char_stream(infix))
	  break;
	if(n == 15)
	  return FALSE;
	glk_stream_set_position(infix, -1, seekmode_Current);
	fillstruct(infix, local_names_info, more_data, infix_add_string);
	if(infix_routines) {
	  infix_routines[record_data[0]].localnames[n] = infix_stringdata +
	                                                          more_data[0];
	}
      }
      if(record_data[0] >= infix_routinescount)
	infix_routinescount = record_data[0] + 1;
      break;
    case LINEREF_DBR:
      for(n = 0; n < record_data[1]; n++) {
	fillstruct(infix, sequence_point_info, more_data, NULL);
	if(infix_linerefs) {
	  infix_linerefs[infix_linerefscount].routine = record_data[0];
	  infix_linerefs[infix_linerefscount].filenum = more_data[0];
	  infix_linerefs[infix_linerefscount].line    = more_data[1];
	  infix_linerefs[infix_linerefscount].x       = more_data[2];
	  infix_linerefs[infix_linerefscount].PC      = more_data[3] +
	                               infix_routines[record_data[0]].start_PC;
	}
	infix_linerefscount++;
      }
      break;
    case ROUTINE_END_DBR:
      if(infix_routines) {
	infix_routines[record_data[0]].end_line = record_data[2];
	infix_routines[record_data[0]].end_x    = record_data[3];
	infix_routines[record_data[0]].end_PC   = record_data[4];
      }
      if(record_data[0] >= infix_routinescount)
	infix_routinescount = record_data[0] + 1;
      break;
    case MAP_DBR:
      for(;;) {
	if(!glk_get_char_stream(infix))
	  break;
	glk_stream_set_position(infix, -1, seekmode_Current);
	fillstruct(infix, map_info, more_data, infix_add_string);
	if(infix_stringdata) {
	  if(n_strcmp(infix_stringdata + more_data[0], "code area") == 0)
	    code_start = more_data[1];
	  if(n_strcmp(infix_stringdata + more_data[0], "strings area") == 0)
	    string_start = more_data[1];
	}
      }
      break;
    }     
  }
}


BOOL init_infix(strid_t infix)
{
  if(!infix && (infix_props || infix_attrs))
    return FALSE;

  kill_infix();

  /* Inform 6.10+ has run-time symbols (techman 9.6) */
  if(!infix && z_memory && prop_table_end
     && (z_memory[HD_INFORMVER] == '6' && z_memory[HD_INFORMVER + 2] >= '1')) {
    zword addr;
    unsigned i;
    glui32 *prop_names, *attr_names;

    /* Assume no strings before end of property table */
    string_start = prop_table_end;
    
    for(addr = prop_table_end+1; LOWORD(addr); addr+=ZWORD_SIZE)
      ;
    addr+=ZWORD_SIZE;

    infix_propscount = LOWORD(addr) - 1; addr+=ZWORD_SIZE;
    prop_names = (glui32 *) n_calloc(sizeof(*prop_names), infix_propscount+1);
    for(i = 1; i <= infix_propscount; i++) {
      prop_names[i] = infix_add_zstring(LOWORD(addr));
      addr += ZWORD_SIZE;
    }
    
    infix_attrscount = 48;
    attr_names = (glui32 *) n_calloc(sizeof(*prop_names), infix_propscount);
    for(i = 0; i <= 47; i++) {
      attr_names[i] = infix_add_zstring(LOWORD(addr));
      addr += ZWORD_SIZE;
    }

    infix_props = (char **) n_calloc(sizeof(*infix_props), infix_propscount+1);
    infix_attrs = (char **) n_calloc(sizeof(*infix_attrs), infix_attrscount);
    for(i = 1; i <= infix_propscount; i++) {
      if(prop_names[i] != 0xffffffffL)
	infix_props[i] = infix_stringdata + prop_names[i];
    }
    for(i = 0; i <= 47; i++) {
      if(attr_names[i] != 0xffffffffL)
	infix_attrs[i] = infix_stringdata + attr_names[i];
    }

    n_free(prop_names);
    n_free(attr_names);
    
    return TRUE;
  }

  if(!infix)
    return FALSE;
  
  glk_stream_set_position(infix, 0, seekmode_Start);
  if((glk_get_char_stream(infix) != 0xDE) ||
     (glk_get_char_stream(infix) != 0xBF) ||
     (glk_get_char_stream(infix) != 0x00) ||
     (glk_get_char_stream(infix) != 0x00)) {
    glk_stream_close(infix, NULL);
    n_show_error(E_DEBUG, "unknown version or not an infix file", 0);
    return FALSE;
  }

  /* ignore inform version number */
  glk_stream_set_position(infix, 6, seekmode_Start);

  /* Calculate space requirements */
  infix_load_records(infix);

  /* Malloc required memory */
  infix_stringdata   = (char *) n_calloc(sizeof(*infix_stringdata),
					 infix_strlen);
  infix_strlen       = 0;
  infix_files        = (infix_file *) n_calloc(sizeof(*infix_files),
					       infix_filescount);
  infix_filescount   = 0;
  infix_objects      = (char **) n_calloc(sizeof(*infix_objects),
					  infix_objectscount);
  infix_objectscount = 0;
  infix_globals      = (char **) n_calloc(sizeof(*infix_globals),
					  infix_globalscount);
  infix_globalscount = 0;
  infix_attrs        = (char **) n_calloc(sizeof(*infix_attrs),
					  infix_attrscount);
  infix_attrscount   = 0;
  infix_props        = (char **) n_calloc(sizeof(*infix_props),
					  infix_propscount);
  infix_propscount   = 0;
  infix_routines     = (routineref *) n_calloc(sizeof(*infix_routines),
					       infix_routinescount);
  infix_routinescount= 0;
  infix_linerefs     = (infix_sequence *) n_calloc(sizeof(*infix_linerefs),
						   infix_linerefscount);
  infix_linerefscount= 0;

  glk_stream_set_position(infix, 6, seekmode_Start);
  infix_load_records(infix);
  
  return TRUE;
}


void kill_infix(void)
{
  unsigned i;

  n_free(infix_stringdata);
  infix_stringdata = 0;
  infix_strlen = 0;

  if(infix_files) {
    for(i = 0; i < infix_filescount; i++)
      infix_unload_file_info(&infix_files[i]);
    n_free(infix_files);
  }
  infix_files = 0;
  infix_filescount = 0;

  n_free(infix_objects);
  infix_objects = 0;
  infix_objectscount = 0;

  n_free(infix_globals);
  infix_globals = 0;
  infix_globalscount = 0;

  n_free(infix_attrs);
  infix_attrs = 0;
  infix_attrscount = 0;

  n_free(infix_props);
  infix_props = 0;
  infix_propscount = 0;

  n_free(infix_routines);
  infix_routines = 0;
  infix_routinescount = 0;

  n_free(infix_linerefs);
  infix_linerefs = 0;
  infix_linerefscount = 0;
}


void infix_print_znumber(zword blah)
{
  set_glk_stream_current();
  g_print_znumber(blah);
}

void infix_print_offset(zword blah)
{
  set_glk_stream_current();
  g_print_number(blah);
}

void infix_print_number(zword blah)
{
  set_glk_stream_current();
  g_print_number(blah);
}

void infix_print_char(int blah)
{
  if(blah == 13)
    blah = 10;
#ifdef STDOUT_DEBUG
  fputc(blah, stdout);
#else
  set_glk_stream_current();
  /* We don't do a style-set because of bugs in zarf Glks */
  glk_put_char((unsigned char) blah);
#endif
}

void infix_print_fixed_char(int blah)
{
  if(blah == 13)
    blah = 10;
#ifdef STDOUT_DEBUG
  fputc(blah, stdout);
#else
  set_glk_stream_current();
  glk_set_style(style_Preformatted);
  glk_put_char((unsigned char) blah);
  glk_set_style(style_Normal);
#endif
}

void infix_print_string(const char *blah)
{
  while(*blah)
    infix_print_char(*blah++);
}

void infix_print_fixed_string(const char *blah)
{
  while(*blah)
    infix_print_fixed_char(*blah++);
}

void infix_get_string(char *dest, int maxlen)
{
#ifdef STDOUT_DEBUG
  fgets(dest, maxlen, stdin);
#else
  unsigned char t;
  int len;
  len = z_read(0, dest, maxlen - 1, 0, 0, 0, 0, &t);
  dest[len] = 0;
#endif
}

void infix_get_val(z_typed *val)
{
  switch(val->t) {
  case Z_LOCAL:     val->v = frame_get_var(val->o, val->p + 1); break;
  case Z_GLOBAL:    val->v = get_var(val->o + 0x10); break;
  case Z_BYTEARRAY: val->v = LOBYTE(val->o + val->p); break;
  case Z_WORDARRAY: val->v = LOWORD(val->o + ZWORD_SIZE * val->p); break;
  case Z_OBJPROP:   val->v = infix_get_prop(val->o, val->p); break;
  default: ;
  }
}

void infix_assign(z_typed *dest, zword val)
{
  switch(dest->t) {
  case Z_LOCAL:     frame_set_var(dest->o, dest->p + 1, val); break;
  case Z_BYTEARRAY: LOBYTEwrite(dest->o + dest->p, val); break;
  case Z_WORDARRAY: LOWORDwrite(dest->o + ZWORD_SIZE * dest->p, val); break;
  case Z_OBJPROP:   infix_put_prop(dest->o, dest->p, val); break;
  default:          infix_print_string("assigning to non-lvalue\n"); break;
  }
  dest->v = val;
}

void infix_display(z_typed val)
{
  unsigned n, i;
  const char *name;

  switch(val.t) {
  default:
    infix_print_znumber(val.v);
    break;

  case Z_BOOLEAN:
    if(val.v)
      infix_print_string("true");
    else
      infix_print_string("false");
    break;

  case Z_UNKNOWN:
    val.t = Z_NUMBER;
    infix_display(val);

    name = debug_decode_number(val.v);

    if(name) {
      infix_print_char(' ');
      infix_print_char('(');
      infix_print_string(name);
      infix_print_char(')');
    }
    break;

  case Z_OBJECT:
    infix_object_display(val.v);
    break;

  case Z_STRING:
    infix_print_char('\"');
    decodezscii(UNPACKS(val.v), infix_print_char);
    infix_print_char('\"');
    break;

  case Z_ROUTINE:
    if(!infix_routines)
      return;

    for(i = 0; i < infix_routinescount; i++) {
      if(infix_routines[i].start_PC == UNPACKR(val.v)) {

	infix_print_char('{');
	for(n = 0; n < 15; n++) {
	  if(infix_routines[i].localnames[n]) {
	    infix_print_string(infix_routines[i].localnames[n]);
	    if(n < 14 && infix_routines[i].localnames[n+1])
	      infix_print_string(", ");
	  }
	}
	infix_print_string("} ");

	infix_print_offset(infix_routines[i].start_PC);
	infix_print_string(" <");
	infix_print_string(infix_routines[i].name);
	infix_print_char('>');
	break;
      }
    }
  }
  infix_print_char('\n');
}


int infix_find_file(infix_file **dest, const char *name)
{
  unsigned n;
  if(infix_files) {
    for(n = 0; n < infix_filescount; n++) {
      if(infix_files[n].filename) {
	unsigned len = n_strlen(infix_files[n].filename);
	if(len &&
	   n_strncasecmp(infix_files[n].filename, name, len) == 0 &&
	   (name[len] == ' ' || name[len] == ':' || name[len] == 0)) {
	  *dest = &infix_files[n];
	  return len;
	}
      }
    }
  }
  return 0;
}


BOOL infix_find_symbol(z_typed *val, const char *name, int len)
{
  unsigned n;
  if(infix_routines) {
    infix_location location;
    if(infix_decode_PC(&location, frame_get_PC(infix_selected_frame))) {
      for(n = 0; n < 15; n++) {
	if(n_strmatch(infix_routines[location.func_num].localnames[n], name, len)) {
	  val->t = Z_LOCAL;
	  val->o = infix_selected_frame;
	  val->p = n;
	  infix_get_val(val);
	  return TRUE;
	}
      }
    }
  }
  if(infix_objects)
    for(n = 0; n < infix_objectscount; n++) {
      if(n_strmatch(infix_objects[n], name, len)) {
	val->t = Z_OBJECT;
	val->v = n;
	return TRUE;
      }
    }
  if(infix_globals) 
    for(n = 0; n < infix_globalscount; n++) {
      if(n_strmatch(infix_globals[n], name, len)) {
	val->t = Z_WORDARRAY;
	val->o = z_globaltable;
	val->p = n;
	infix_get_val(val);
	return TRUE;
      }
  }
  if(infix_arrays)
    for(n = 0; n < infix_arrayscount; n++) {
      if(n_strmatch(infix_arrays[n].name, name, len)) {
	val->t = Z_NUMBER;
	val->v = n;
	return TRUE;
      }
    }
  if(infix_attrs)
    for(n = 0; n < infix_attrscount; n++) {
      if(n_strmatch(infix_attrs[n], name, len)) {
	val->t = Z_NUMBER;
	val->v = n;
	return TRUE;
      }
    }
  if(infix_props)
    for(n = 0; n < infix_propscount; n++) {
      if(n_strmatch(infix_props[n], name, len)) {
	val->t = Z_NUMBER;
	val->v = n;
	return TRUE;
      }
    }
  if(infix_routines)
    for(n = 0; n < infix_routinescount; n++) {
      if(n_strmatch(infix_routines[n].name, name, len)) {
	val->t = Z_ROUTINE;
	val->v = PACKR(infix_routines[n].start_PC);
	return TRUE;
      }
    }
  return FALSE;
}

static char infix_temp_string_buffer[45];
static unsigned infix_temp_strlen;

static void infix_temp_string_build(int ch)
{
  infix_temp_string_buffer[infix_temp_strlen] = ch;
  infix_temp_strlen++;
  if(infix_temp_strlen > 40) {
    infix_temp_strlen = 43;
    infix_temp_string_buffer[40] = '.';
    infix_temp_string_buffer[41] = '.';
    infix_temp_string_buffer[42] = '.';
  }
}


const char *infix_get_name(z_typed val)
{
  unsigned i;
  if(!infix_stringdata)
    return NULL;
  switch(val.t) {
  case Z_GLOBAL:
    if(val.o < infix_globalscount)
      return infix_globals[val.o];
    break;
  case Z_OBJECT:
    if(val.v < infix_objectscount)
      return infix_objects[val.v];
    break;
  case Z_ATTR:
    if(val.v < infix_attrscount)
      return infix_attrs[val.v];
    break;
  case Z_PROP:
    if(val.v < infix_propscount)
      return infix_props[val.v];
    break;
  case Z_ROUTINE:
    for(i = 0; i < infix_routinescount; i++) {
      if(infix_routines[i].start_PC == UNPACKR(val.v))
	return infix_routines[i].name;
    }
    break;
  case Z_STRING:
    if(UNPACKS(val.v) < string_start)
      break;
    if(string_start < code_start && UNPACKS(val.v) >= code_start)
      break;
    if(UNPACKS(val.v) >= total_size)
      break;

    /* Assume every string except the first is preceded by zeroed padding until
       an end-of-string marker */
    if(UNPACKS(val.v) != string_start) {
      offset addr = UNPACKS(val.v) - 2;
      zword s;
      while((s = HIWORD(addr)) == 0)
	addr -= 2;
      if(!(s & 0x8000))
	break;
    }

    infix_temp_string_buffer[0] = '\"';
    infix_temp_strlen = 1;
    testing_string = TRUE; string_bad = FALSE;
    decodezscii(UNPACKS(val.v), infix_temp_string_build);
    testing_string = FALSE;
    if(string_bad)
      break;
    infix_temp_string_buffer[infix_temp_strlen] = '\"';
    infix_temp_string_buffer[infix_temp_strlen + 1] = 0;
    return infix_temp_string_buffer;
  case Z_ARRAY:
    if(val.v < infix_arrayscount)
      return infix_arrays[val.v].name;
    break;

  default: ;
  }
  return NULL;
}


/* We search through linerefs very often so use binary search for speed */

static infix_sequence *infix_search_linerefs(offset thisPC)
{
  unsigned n;
  int top = 0;
  int bottom = infix_linerefscount-1;
  int middle = (top + bottom) / 2;

  if(!infix_linerefs)
    return NULL;

  do {
    middle = (top + bottom) / 2;
    if(thisPC < infix_linerefs[middle].PC)
      bottom = middle - 1;
    else if(thisPC > infix_linerefs[middle].PC)
      top = middle + 1;
    else
      break;
  } while(top <= bottom);

  /* If the PC is in the middle of a line, we want to display that line. In
     this case, PC will be greater than infix_linerefs[middle].PC, so just let
     it go. Otherwise, we want to look at the previous lineref, so subtract
     one (or more)
  */

  while(middle && thisPC < infix_linerefs[middle].PC)
    middle--;


  /* Make sure PC is inside the function the lineref says it is; if so, then
     we're done */

  n = infix_linerefs[middle].routine;
  if(thisPC >= infix_routines[n].start_PC &&
     thisPC <= infix_routines[n].end_PC)
    return &infix_linerefs[middle];

  return NULL;
}


BOOL infix_decode_PC(infix_location *dest, offset thisPC)
{
  infix_sequence *n = infix_search_linerefs(thisPC);

  if(!n) {     /* No sequence point found - return a function */
    unsigned i;

    if(!infix_routines)
      return FALSE;

    for(i = 0; i < infix_routinescount; i++) {
      if(thisPC >= infix_routines[i].start_PC &&
	 thisPC <= infix_routines[i].end_PC) {
	
	routineref *r = &infix_routines[i];
	dest->file = &infix_files[r->filenum];
	dest->line_num = r->startline;
	dest->line_x = r->start_x;
	dest->func_name = r->name;
	dest->func_num = i;
	dest->thisPC = r->start_PC;

	return TRUE;
      }
    }

    /* Not in a function. Give up. */
    return FALSE;
  }


  dest->file      = &infix_files[n->filenum];
  dest->line_num  = n->line;
  dest->line_x    = n->x;
  dest->func_name = infix_routines[n->routine].name;
  dest->func_num  = n->routine;
  dest->thisPC    = n->PC;

  return TRUE;
}


BOOL infix_decode_fileloc(infix_location *dest, const char *filename,
			  unsigned line_num)
{
  unsigned n;
  if(!infix_linerefs)
    return FALSE;
  for(n = 0; n < infix_linerefscount; n++) {
    if(infix_linerefs[n].line == line_num &&
       n_strcmp(infix_files[infix_linerefs[n].filenum].filename, filename) == 0) {

      dest->file      = &infix_files[infix_linerefs[n].filenum];
      dest->line_num  = infix_linerefs[n].line;
      dest->line_x    = infix_linerefs[n].x;
      dest->func_name = infix_routines[infix_linerefs[n].routine].name;
      dest->func_num  = infix_linerefs[n].routine;
      dest->thisPC    = infix_linerefs[n].PC;
      return TRUE;
    }
  }
  dest->thisPC = 0;
  return FALSE;
}


BOOL infix_decode_func_name(infix_location *dest, const char *file_name,
			    const char *func_name)
{
  unsigned n;
  if(!infix_linerefs)
    return FALSE;
  for(n = 0; n < infix_linerefscount; n++) {
    if(n_strcmp(infix_files[infix_linerefs[n].filenum].filename, file_name) == 0) {
      if(!file_name || n_strcmp(infix_routines[infix_linerefs[n].filenum].name,
				func_name) == 0) {

	dest->file      = &infix_files[infix_linerefs[n].filenum];
	dest->line_num  = infix_linerefs[n].line;
	dest->line_x    = infix_linerefs[n].x;
	dest->func_name = infix_routines[infix_linerefs[n].routine].name;
	dest->func_num  = infix_linerefs[n].routine;
	dest->thisPC    = infix_linerefs[n].PC;
	return TRUE;
      }
    }
  }
  return FALSE;
}


void infix_gprint_loc(int frame, offset thisPC)
{
  infix_location boo;
  offset loc;
  BOOL found_frame;
  unsigned numlocals;
  unsigned n;

  if(!thisPC) {
    loc = frame_get_PC(frame);
    numlocals = stack_get_numlocals(frame);
  } else {
    loc = thisPC;
    numlocals = 0;
  }

  found_frame = infix_decode_PC(&boo, loc);

  infix_print_offset(loc);

  if(found_frame) {
    infix_print_string(" in ");
    infix_print_string(boo.func_name);
  }

  if(!thisPC) {
    infix_print_string(" (");
    
    for(n = 0; n < numlocals; n++) {
      const char *name;
      if(n)
	infix_print_string(", ");
      if(found_frame) {
	infix_print_string(infix_routines[boo.func_num].localnames[n]);
	infix_print_char('=');
      }
      infix_print_znumber(frame_get_var(frame, n + 1));
      name = debug_decode_number(frame_get_var(frame, n + 1));
      if(name) {
	infix_print_char(' ');
	infix_print_string(name);
      }
    }

    infix_print_string(")");
  }

  if(found_frame) {
    infix_print_string(" at ");
    if(boo.file->filename)
      infix_print_string(boo.file->filename);
    else
      infix_print_string("<No file>");
    infix_print_char(':');
    infix_print_number(boo.line_num);
  }
  
  infix_print_char('\n');

  if(found_frame && !thisPC)
    infix_file_print_line(boo.file, boo.line_num);
}


void infix_list_files(void)
{
  unsigned i;
  for(i = 0; i < infix_filescount; i++) {
    if(i)
      infix_print_string(", ");
    infix_print_string(infix_files[i].filename);
  }
}

#else

BOOL init_infix(strid_t unused)
{
  n_show_error(E_DEBUG, "debugging code not compiled in", 0);
  return FALSE;
}


#endif

