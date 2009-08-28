/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i objects.c' */
#ifndef CFH_OBJECTS_H
#define CFH_OBJECTS_H

/* From `objects.c': */
zword LOWO (zword p );
void LOWOcopy (zword a , zword b );
void LOWOwrite (zword p , zword n );
void objects_init (void);
void op_get_child (void);
void op_get_parent (void);
void op_get_sibling (void);
void op_insert_obj (void);
void op_jin (void);
void op_remove_obj (void);
offset object_name (zword object );
void op_print_obj (void);
void op_clear_attr (void);
void op_set_attr (void);
void op_test_attr (void);
void op_get_next_prop (void);
void op_get_prop_addr (void);
void op_get_prop_len (void);
void op_get_prop (void);
void op_put_prop (void);

#ifdef DEBUGGING
BOOL infix_property_loop (zword object , zword *propnum , zword *location , zword *len , zword *nonindivloc , zword *nonindivlen );
void infix_move (zword dest , zword object );
void infix_remove (zword object );
zword infix_parent (zword object );
zword infix_child (zword object );
zword infix_sibling (zword object );
void infix_set_attrib (zword object , zword attrib );
void infix_clear_attrib (zword object , zword attrib );
zword infix_get_proptable (zword object , zword prop , zword *length );
zword infix_get_prop (zword object , zword prop );
void infix_put_prop (zword object , zword prop , zword val );
BOOL infix_test_attrib (zword object , zword attrib );
void infix_object_tree (zword object );
void infix_object_find (const char *description );
void infix_object_display (zword object );

#endif /* DEBUGGING */

#endif /* CFH_OBJECTS_H */
