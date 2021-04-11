// vim: set ft=c:

#ifndef ZTERP_OBJECTS_H
#define ZTERP_OBJECTS_H

#include <stdint.h>

void print_object(uint16_t obj, void (*outc)(uint8_t));

void zget_sibling(void);
void zget_child(void);
void zget_parent(void);
void zremove_obj(void);
void ztest_attr(void);
void zset_attr(void);
void zclear_attr(void);
void zinsert_obj(void);
void zget_prop_len(void);
void zget_prop_addr(void);
void zget_next_prop(void);
void zput_prop(void);
void zget_prop(void);
void zjin(void);
void zprint_obj(void);

#endif
