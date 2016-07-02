/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i stack.c' */
#ifndef CFH_STACK_H
#define CFH_STACK_H

/* From `stack.c': */
void init_stack (offset initialstack_stack_size , zword initialframe_size );
void kill_stack (void);
BOOL verify_stack (void);
void add_stack_frame (offset return_PC , unsigned num_locals , zword *locals , unsigned num_args , int result_var );
void remove_stack_frame (void);
void check_set_var (int var , zword val );
void mop_func_return (zword ret_val );
void op_catch (void);
unsigned stack_get_numlocals (int frame );
void op_throw (void);
void op_check_arg_count (void);
void op_push (void);
void op_pop (void);
void op_pull (void);
void op_pop_stack (void);
void op_push_stack (void);
void mop_store_result (zword val );
void op_ret_popped (void);
unsigned stack_get_depth (void);
BOOL frame_is_valid (unsigned frame );
offset frame_get_PC (unsigned frame );
zword frame_get_var (unsigned frame , int var );
void frame_set_var (unsigned frame , int var , zword val );
zword get_var (int var );
void set_var (int var , zword val );
extern const unsigned qstackframe[];
BOOL quetzal_stack_restore (strid_t stream , glui32 qsize );
glui32 get_quetzal_stack_size (void);
BOOL quetzal_stack_save (strid_t stream );

#endif /* CFH_STACK_H */
