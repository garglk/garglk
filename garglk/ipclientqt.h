// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.

#ifndef GARGLK_IPCLIENTQT_H
#define GARGLK_IPCLIENTQT_H

#include <cstdint>
#include <string>

#include <QString>

#include "garglk.h"

namespace garglk::ipc_client {

// Attempt to connect to the parent session when gli_conf_ipc is set.
// Returns true if connected (headless IPC mode).
bool connect_to_parent();

bool active();

void init_window(bool move, int x, int y, int width, int height, bool fullscreen,
        unsigned char r, unsigned char g, unsigned char b);

void set_title(const QString &title);
bool set_contents(int width, int height, const unsigned char *rgb, std::size_t nbytes);
void request_close();

std::string open_dialog(const QString &prompt, FileFilter filter, const QString &savedir);
std::string save_dialog(const QString &prompt, FileFilter filter, const QString &savedir);
void abort_dialog(const QString &prompt);

void set_cursor(std::uint32_t cursor);
void toggle_fullscreen();
bool is_fullscreen();

// Process inbound IPC messages (keys, mouse, arrange, quit).
void poll();

// True after parent sent EventQuit or the socket disconnected.
bool window_dead();

// Last known backing store size from parent (physical pixels).
void backing_size(int &width, int &height, double &dpr);

}

#endif
