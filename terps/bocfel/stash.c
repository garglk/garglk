// Copyright 2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <stdbool.h>
#include <stddef.h>

#include "stash.h"
#include "util.h"

// Restoring (as in @restore) is done in-place, meaning that data
// structures used by the interpreter are updated during the restore. If
// any error is encountered during restore, the system could thus be in
// a state of partial update, preventing things from working properly.
// To avoid this, any structures which are updated in-place are first
// backed up. On restore failure, these will be restored. Anything can
// register functions to back up, restore, and free any particular piece
// of state that needs to be protected. The restore code will ensure
// these are all called appropriately.
//
// Required functions:
//
// void backup(void)  - Stash away the current state for future restore
// bool restore(void) - Restore the stashed state, returning true if it
//                      could successfully be restored, false otherwise.
// void free(void)    - Free any resources that were allocated by
//                      backup(). This can be called at any time,
//                      including before any calls to backup(), and
//                      including multiple times in a row without any
//                      intervening call to backup().

struct stash {
    void (*backup)(void);
    bool (*restore)(void);
    void (*free)(void);
};

static struct stash stashes[16];
static size_t nstashes = 0;

static bool have_stash = false;

void stash_register(void (*backup)(void), bool (*restore)(void), void (*free)(void))
{
    if (nstashes == ASIZE(stashes)) {
        die("internal error: too many stashes");
    }

    struct stash *stash = &stashes[nstashes++];

    stash->backup = backup;
    stash->restore = restore;
    stash->free = free;
}

void stash_backup(void)
{
    for (size_t i = 0; i < nstashes; i++) {
        stashes[i].backup();
    }

    have_stash = true;
}

bool stash_restore(void)
{
    bool success = true;

    for (size_t i = 0; i < nstashes; i++) {
        if (!stashes[i].restore()) {
            success = false;
            break;
        }
    }

    stash_free();

    return success;
}

void stash_free(void)
{
    for (size_t i = 0; i < nstashes; i++) {
        stashes[i].free();
    }

    have_stash = false;
}

bool stash_exists(void)
{
    return have_stash;
}
