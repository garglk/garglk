// vim: set ft=cpp:

#ifndef ZTERP_STACK_H
#define ZTERP_STACK_H

#include <stdexcept>
#include <string>
#include <vector>

#include "types.h"

static constexpr unsigned long DEFAULT_STACK_SIZE = 0x4000;
static constexpr unsigned long DEFAULT_CALL_DEPTH = 0x400;

class RestoreError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

extern bool seen_save_undo;

void init_stack(bool first_run);

uint16_t variable(uint16_t var);
void store_variable(uint16_t var, uint16_t n);
uint16_t *stack_top_element();

void start_v6();
uint16_t internal_call(uint16_t routine, std::vector<uint16_t> args = {});
void do_return(uint16_t retval);

enum class SaveType {
    Normal,
    Meta,
    Autosave,
};

enum class SaveOpcode {
    None = -1,
    Read = 0,
    ReadChar = 1,
};

bool do_save(SaveType savetype, SaveOpcode saveopcode);
bool do_restore(SaveType savetype, SaveOpcode &saveopcode);

enum class SaveStackType {
    Game,
    User
};

enum class SaveResult {
    Success,
    Failure,
    Unavailable,
};

SaveResult push_save(SaveStackType type, SaveType savetype, SaveOpcode saveopcode, const char *desc);
bool pop_save(SaveStackType type, size_t saveno, SaveOpcode &saveopcode);
bool drop_save(SaveStackType type, size_t i);
void list_saves(SaveStackType type);

void zpush();
void zpull();
void zload();
void zstore();
void zret_popped();
void zpop();
void zcatch();
void zthrow();
void zret();
void zrtrue();
void zrfalse();
void zcheck_arg_count();
void zpop_stack();
void zpush_stack();
void zsave_undo();
void zrestore_undo();
void zsave();
void zrestore();

void zcall_store();
void zcall_nostore();

#define zcall		zcall_store
#define zcall_1n	zcall_nostore
#define zcall_1s	zcall_store
#define zcall_2n	zcall_nostore
#define zcall_2s	zcall_store
#define zcall_vn	zcall_nostore
#define zcall_vn2	zcall_nostore
#define zcall_vs2	zcall_store

#endif
