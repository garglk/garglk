// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef GARGLK_IPC_H
#define GARGLK_IPC_H

#include <cstdint>
#include <string>
#include <vector>

#include <QByteArray>
#include <QString>

#include "garglk.h"

namespace garglk::ipc {

// Default QLocalServer name; overridable via garglk.ini "ipc_server".
constexpr const char *DEFAULT_SERVER_NAME = "io.github.garglk.Gargoyle";

enum class Msg : std::uint32_t {
    // Child -> parent
    InitWindow = 1,
    SetTitle = 2,
    SetContents = 3,
    CloseWindow = 4,
    OpenDialog = 5,
    SaveDialog = 6,
    AbortDialog = 7,
    SetCursor = 8,
    ToggleFullScreen = 9,
    QueryFullScreen = 10,
    RequestBacking = 11,

    // Parent -> child
    EventKey = 100,
    EventMouse = 101,
    EventArrange = 102,
    EventQuit = 103,
    DialogResult = 104,
    FullScreenResult = 105,
    BackingInfo = 106,

    // Launcher -> existing parent (handoff)
    OpenGame = 200,
    HandoffAck = 201,
};

enum class MouseKind : std::uint32_t {
    Move = 0,
    Press = 1,
    Release = 2,
    Wheel = 3,
};

struct Message {
    Msg type = Msg::InitWindow;
    QByteArray payload;
};

QByteArray encode(Msg type, const QByteArray &payload = {});
bool try_decode(QByteArray &buffer, Message &out);

void append_u32(QByteArray &out, std::uint32_t v);
void append_i32(QByteArray &out, std::int32_t v);
void append_f64(QByteArray &out, double v);
void append_string(QByteArray &out, const QString &s);
void append_bytes(QByteArray &out, const void *data, std::size_t n);

bool read_u32(const QByteArray &in, int &off, std::uint32_t &v);
bool read_i32(const QByteArray &in, int &off, std::int32_t &v);
bool read_f64(const QByteArray &in, int &off, double &v);
bool read_string(const QByteArray &in, int &off, QString &s);

QByteArray make_init_window(bool move, int x, int y, int width, int height,
        bool fullscreen, unsigned char r, unsigned char g, unsigned char b,
        std::uint32_t pid);
QByteArray make_set_title(const QString &title);
QByteArray make_set_contents(int width, int height, const unsigned char *rgb, std::size_t nbytes);
QByteArray make_dialog(const QString &prompt, FileFilter filter, const QString &savedir);
QByteArray make_abort_dialog(const QString &prompt);
QByteArray make_set_cursor(std::uint32_t cursor);
QByteArray make_event_key(std::uint32_t modifiers, std::int32_t key, const QString &text);
QByteArray make_event_mouse(MouseKind kind, std::int32_t x, std::int32_t y, std::int32_t wheel_delta);
QByteArray make_event_arrange(int width, int height, double dpr);
QByteArray make_dialog_result(const QString &path);
QByteArray make_fullscreen_result(bool fullscreen);
QByteArray make_backing_info(int width, int height, double dpr);
QByteArray make_open_game(const QString &path);

std::string server_name_from_config();

}

#endif
