// Copyright 2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "stash.h"
#include "types.h"

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
// Required methods:
//
// void backup()  - Stash away the current state for future restore.
// bool restore() - Restore the stashed state, returning true if it
//                  could successfully be restored, false otherwise.
// void free()    - Free any resources that were allocated by
//                  backup(). This can be called at any time,
//                  including before any calls to backup(), and
//                  including multiple times in a row without any
//                  intervening call to backup().

static std::vector<std::unique_ptr<Stasher>> stashes;

void stash_register(std::unique_ptr<Stasher> stasher)
{
    stashes.push_back(std::move(stasher));
}

Stash::~Stash()
{
    free();
}

void Stash::backup()
{
    for (const auto &stash : stashes) {
        stash->backup();
    }

    m_have_stash = true;
}

bool Stash::restore()
{
    bool success = std::all_of(stashes.begin(), stashes.end(), [](std::unique_ptr<Stasher> &stash) { return stash->restore(); });

    free();

    return success;
}

void Stash::free()
{
    for (const auto &stash : stashes) {
        stash->free();
    }

    m_have_stash = false;
}

bool Stash::exists() const
{
    return m_have_stash;
}
