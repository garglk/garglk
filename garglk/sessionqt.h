// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.

#ifndef GARGLK_SESSIONQT_H
#define GARGLK_SESSIONQT_H

#include <functional>
#include <string>

#include <QString>

namespace garglk {

// True when this process is the IPC session parent (owns game windows).
bool session_is_parent();

// If another Gargoyle parent is already running, hand off `game` to it
// and return true (caller should exit). No-op / false when ipc is off.
bool session_try_handoff(const QString &game);

// Start the local server. Returns false on failure.
bool session_init_parent();

// Called by the launcher to open/launch a game via winterp (non-blocking).
using LaunchGameFn = std::function<bool(const std::string &game)>;
void session_set_launcher(LaunchGameFn fn);

// Open a game from the parent (File → Open / handoff / initial story).
bool session_open_game(const QString &game);

// Run the Qt event loop until the last game window closes.
int session_exec();

}

#endif
