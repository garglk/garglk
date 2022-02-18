// vim: set ft=cpp:

#ifndef ZTERP_OBJECTS_H
#define ZTERP_OBJECTS_H

#include "types.h"

void print_object(uint16_t obj, void (*outc)(uint8_t));

void zget_sibling();
void zget_child();
void zget_parent();
void zremove_obj();
void ztest_attr();
void zset_attr();
void zclear_attr();
void zinsert_obj();
void zget_prop_len();
void zget_prop_addr();
void zget_next_prop();
void zput_prop();
void zget_prop();
void zjin();
void zprint_obj();

#endif
