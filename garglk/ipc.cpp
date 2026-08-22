// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "ipc.h"

#include <cstring>

#include "garglk.h"

namespace garglk::ipc {
namespace {

constexpr int HEADER_SIZE = 8; // type + payload length

}

void append_u32(QByteArray &out, std::uint32_t v)
{
    char buf[4];
    std::memcpy(buf, &v, 4);
    out.append(buf, 4);
}

void append_i32(QByteArray &out, std::int32_t v)
{
    append_u32(out, static_cast<std::uint32_t>(v));
}

void append_f64(QByteArray &out, double v)
{
    char buf[8];
    std::memcpy(buf, &v, 8);
    out.append(buf, 8);
}

void append_string(QByteArray &out, const QString &s)
{
    auto utf8 = s.toUtf8();
    append_u32(out, static_cast<std::uint32_t>(utf8.size()));
    out.append(utf8);
}

void append_bytes(QByteArray &out, const void *data, std::size_t n)
{
    out.append(static_cast<const char *>(data), static_cast<int>(n));
}

bool read_u32(const QByteArray &in, int &off, std::uint32_t &v)
{
    if (off + 4 > in.size()) {
        return false;
    }
    std::memcpy(&v, in.constData() + off, 4);
    off += 4;
    return true;
}

bool read_i32(const QByteArray &in, int &off, std::int32_t &v)
{
    std::uint32_t u = 0;
    if (!read_u32(in, off, u)) {
        return false;
    }
    v = static_cast<std::int32_t>(u);
    return true;
}

bool read_f64(const QByteArray &in, int &off, double &v)
{
    if (off + 8 > in.size()) {
        return false;
    }
    std::memcpy(&v, in.constData() + off, 8);
    off += 8;
    return true;
}

bool read_string(const QByteArray &in, int &off, QString &s)
{
    std::uint32_t len = 0;
    if (!read_u32(in, off, len)) {
        return false;
    }
    if (off + static_cast<int>(len) > in.size()) {
        return false;
    }
    s = QString::fromUtf8(in.constData() + off, static_cast<int>(len));
    off += static_cast<int>(len);
    return true;
}

QByteArray encode(Msg type, const QByteArray &payload)
{
    QByteArray out;
    out.reserve(HEADER_SIZE + payload.size());
    append_u32(out, static_cast<std::uint32_t>(type));
    append_u32(out, static_cast<std::uint32_t>(payload.size()));
    out.append(payload);
    return out;
}

bool try_decode(QByteArray &buffer, Message &out)
{
    if (buffer.size() < HEADER_SIZE) {
        return false;
    }

    int off = 0;
    std::uint32_t type = 0;
    std::uint32_t len = 0;
    if (!read_u32(buffer, off, type) || !read_u32(buffer, off, len)) {
        return false;
    }

    if (buffer.size() < HEADER_SIZE + static_cast<int>(len)) {
        return false;
    }

    out.type = static_cast<Msg>(type);
    out.payload = buffer.mid(HEADER_SIZE, static_cast<int>(len));
    buffer.remove(0, HEADER_SIZE + static_cast<int>(len));
    return true;
}

QByteArray make_init_window(bool move, int x, int y, int width, int height,
        bool fullscreen, unsigned char r, unsigned char g, unsigned char b,
        std::uint32_t pid)
{
    QByteArray p;
    append_u32(p, move ? 1u : 0u);
    append_i32(p, x);
    append_i32(p, y);
    append_i32(p, width);
    append_i32(p, height);
    append_u32(p, fullscreen ? 1u : 0u);
    append_u32(p, r);
    append_u32(p, g);
    append_u32(p, b);
    append_u32(p, pid);
    return p;
}

QByteArray make_set_title(const QString &title)
{
    QByteArray p;
    append_string(p, title);
    return p;
}

QByteArray make_set_contents(int width, int height, const unsigned char *rgb, std::size_t nbytes)
{
    QByteArray p;
    append_i32(p, width);
    append_i32(p, height);
    append_bytes(p, rgb, nbytes);
    return p;
}

QByteArray make_dialog(const QString &prompt, FileFilter filter, const QString &savedir)
{
    QByteArray p;
    append_string(p, prompt);
    append_u32(p, static_cast<std::uint32_t>(filter));
    append_string(p, savedir);
    return p;
}

QByteArray make_abort_dialog(const QString &prompt)
{
    QByteArray p;
    append_string(p, prompt);
    return p;
}

QByteArray make_set_cursor(std::uint32_t cursor)
{
    QByteArray p;
    append_u32(p, cursor);
    return p;
}

QByteArray make_event_key(std::uint32_t modifiers, std::int32_t key, const QString &text)
{
    QByteArray p;
    append_u32(p, modifiers);
    append_i32(p, key);
    append_string(p, text);
    return p;
}

QByteArray make_event_mouse(MouseKind kind, std::int32_t x, std::int32_t y, std::int32_t wheel_delta)
{
    QByteArray p;
    append_u32(p, static_cast<std::uint32_t>(kind));
    append_i32(p, x);
    append_i32(p, y);
    append_i32(p, wheel_delta);
    return p;
}

QByteArray make_event_arrange(int width, int height, double dpr)
{
    QByteArray p;
    append_i32(p, width);
    append_i32(p, height);
    append_f64(p, dpr);
    return p;
}

QByteArray make_dialog_result(const QString &path)
{
    QByteArray p;
    append_string(p, path);
    return p;
}

QByteArray make_fullscreen_result(bool fullscreen)
{
    QByteArray p;
    append_u32(p, fullscreen ? 1u : 0u);
    return p;
}

QByteArray make_backing_info(int width, int height, double dpr)
{
    return make_event_arrange(width, height, dpr);
}

QByteArray make_open_game(const QString &path)
{
    QByteArray p;
    append_string(p, path);
    return p;
}

std::string server_name_from_config()
{
    if (!gli_conf_ipc_server.empty()) {
        return gli_conf_ipc_server;
    }
    return DEFAULT_SERVER_NAME;
}

}
