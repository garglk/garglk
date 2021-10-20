// vim: set ft=c:

#ifndef ZTERP_STACK_H
#define ZTERP_STACK_H

#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_STACK_SIZE	0x4000
#define DEFAULT_CALL_DEPTH	0x400

extern bool seen_save_undo;

void init_stack(bool first_run);

uint16_t variable(uint16_t var);
void store_variable(uint16_t var, uint16_t n);
uint16_t *stack_top_element(void);

void start_v6(void);
#ifdef ZTERP_GLK
uint16_t internal_call(uint16_t routine);
#endif
void do_return(uint16_t retval);

enum SaveType {
    SaveTypeNormal,
    SaveTypeMeta,
    SaveTypeAutosave,
};

enum SaveOpcode {
    SaveOpcodeNone = -1,
    SaveOpcodeRead = 0,
    SaveOpcodeReadChar = 1,
};

bool do_save(enum SaveType savetype, enum SaveOpcode saveopcode);
bool do_restore(enum SaveType savetype, enum SaveOpcode *saveopcode);

enum SaveStackType {
    SaveStackGame,
    SaveStackUser
};

enum SaveResult {
    SaveResultSuccess,
    SaveResultFailure,
    SaveResultUnavailable,
};

enum SaveResult push_save(enum SaveStackType type, enum SaveType savetype, enum SaveOpcode saveopcode, const char *desc);
bool pop_save(enum SaveStackType type, long saveno, enum SaveOpcode *saveopcode);
bool drop_save(enum SaveStackType type, long i);
void list_saves(enum SaveStackType type, void (*printer)(const char *));

void zpush(void);
void zpull(void);
void zload(void);
void zstore(void);
void zret_popped(void);
void zpop(void);
void zcatch(void);
void zthrow(void);
void zret(void);
void zrtrue(void);
void zrfalse(void);
void zcheck_arg_count(void);
void zpop_stack(void);
void zpush_stack(void);
void zsave_undo(void);
void zrestore_undo(void);
void zsave(void);
void zrestore(void);

void zcall_store(void);
void zcall_nostore(void);

#define zcall		zcall_store
#define zcall_1n	zcall_nostore
#define zcall_1s	zcall_store
#define zcall_2n	zcall_nostore
#define zcall_2s	zcall_store
#define zcall_vn	zcall_nostore
#define zcall_vn2	zcall_nostore
#define zcall_vs2	zcall_store

#endif
