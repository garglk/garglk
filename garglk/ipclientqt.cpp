// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.

#include "ipclientqt.h"

#include <QLocalSocket>
#include <QtGlobal>

#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <utility>

#ifdef _WIN32
#include <process.h>
#define garglk_getpid() _getpid()
#else
#include <unistd.h>
#define garglk_getpid() getpid()
#endif

#include "garglk.h"
#include "ipc.h"

namespace garglk::ipc_client {
namespace {

QLocalSocket *g_socket = nullptr;
QByteArray g_readbuf;
bool g_active = false;
std::atomic<bool> g_dead{false};
bool g_fullscreen = false;
int g_back_w = 0;
int g_back_h = 0;
double g_back_dpr = 1.0;
bool g_arrange_pending = false;
std::optional<QString> g_dialog_result;

#ifdef Q_OS_MAC
constexpr Qt::KeyboardModifier kRealCtrl = Qt::MetaModifier;
#else
constexpr Qt::KeyboardModifier kRealCtrl = Qt::ControlModifier;
#endif

void send_msg(ipc::Msg type, const QByteArray &payload = {})
{
    if (g_socket && g_socket->state() == QLocalSocket::ConnectedState) {
        g_socket->write(ipc::encode(type, payload));
        g_socket->flush();
    }
}

void handle_key(std::uint32_t modifiers, std::int32_t key, const QString &text);
void handle_mouse(ipc::MouseKind kind, std::int32_t x, std::int32_t y, std::int32_t wheel_delta);

void handle_message(const ipc::Message &msg)
{
    int off = 0;
    switch (msg.type) {
    case ipc::Msg::EventKey: {
        std::uint32_t modifiers = 0;
        std::int32_t key = 0;
        QString text;
        ipc::read_u32(msg.payload, off, modifiers);
        ipc::read_i32(msg.payload, off, key);
        ipc::read_string(msg.payload, off, text);
        handle_key(modifiers, key, text);
        break;
    }
    case ipc::Msg::EventMouse: {
        std::uint32_t kind = 0;
        std::int32_t x = 0, y = 0, wheel = 0;
        ipc::read_u32(msg.payload, off, kind);
        ipc::read_i32(msg.payload, off, x);
        ipc::read_i32(msg.payload, off, y);
        ipc::read_i32(msg.payload, off, wheel);
        handle_mouse(static_cast<ipc::MouseKind>(kind), x, y, wheel);
        break;
    }
    case ipc::Msg::EventArrange:
    case ipc::Msg::BackingInfo: {
        std::int32_t w = 0, h = 0;
        double dpr = 1.0;
        ipc::read_i32(msg.payload, off, w);
        ipc::read_i32(msg.payload, off, h);
        ipc::read_f64(msg.payload, off, dpr);
        if (w > 0 && h > 0 && (w != g_back_w || h != g_back_h || dpr != g_back_dpr)) {
            g_back_w = w;
            g_back_h = h;
            g_back_dpr = dpr;
            g_arrange_pending = true;
        }
        break;
    }
    case ipc::Msg::EventQuit:
        g_dead = true;
        break;
    case ipc::Msg::DialogResult: {
        QString path;
        ipc::read_string(msg.payload, off, path);
        g_dialog_result = path;
        break;
    }
    case ipc::Msg::FullScreenResult: {
        std::uint32_t v = 0;
        ipc::read_u32(msg.payload, off, v);
        g_fullscreen = v != 0;
        break;
    }
    default:
        break;
    }
}

void handle_key(std::uint32_t modifiers, std::int32_t key, const QString &text)
{
    Qt::KeyboardModifiers modmasked = static_cast<Qt::KeyboardModifiers>(modifiers)
            & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);

    static const std::map<std::pair<Qt::KeyboardModifiers, int>, std::function<void()>> keys = {
        {{kRealCtrl, Qt::Key_A}, []{ gli_input_handle_key(keycode_Home); }},
        {{kRealCtrl, Qt::Key_B}, []{ gli_input_handle_key(keycode_Left); }},
        {{kRealCtrl, Qt::Key_D}, []{ gli_input_handle_key(keycode_Erase); }},
        {{kRealCtrl, Qt::Key_E}, []{ gli_input_handle_key(keycode_End); }},
        {{kRealCtrl, Qt::Key_F}, []{ gli_input_handle_key(keycode_Right); }},
        {{kRealCtrl, Qt::Key_H}, []{ gli_input_handle_key(keycode_Delete); }},
        {{kRealCtrl, Qt::Key_N}, []{ gli_input_handle_key(keycode_Down); }},
        {{kRealCtrl, Qt::Key_P}, []{ gli_input_handle_key(keycode_Up); }},
        {{kRealCtrl, Qt::Key_U}, []{ gli_input_handle_key(keycode_Escape); }},
#ifdef Q_OS_WIN
        {{Qt::ControlModifier, Qt::Key_Q}, []{ gli_exit(0); }},
#endif
        {{Qt::ControlModifier, Qt::Key_Comma}, gli_edit_config},
        {{Qt::ShiftModifier, Qt::Key_Backspace}, []{ gli_input_handle_key(keycode_Delete); }},
        {{Qt::NoModifier, Qt::Key_Escape},    []{ gli_input_handle_key(keycode_Escape); }},
        {{Qt::NoModifier, Qt::Key_Tab},       []{ gli_input_handle_key(keycode_Tab); }},
        {{Qt::NoModifier, Qt::Key_Backspace}, []{ gli_input_handle_key(keycode_Delete); }},
        {{Qt::NoModifier, Qt::Key_Return},    []{ gli_input_handle_key(keycode_Return); }},
        {{Qt::NoModifier, Qt::Key_Enter},     []{ gli_input_handle_key(keycode_Return); }},
        {{Qt::NoModifier, Qt::Key_Left},      []{ gli_input_handle_key(keycode_Left); }},
        {{Qt::NoModifier, Qt::Key_Up},        []{ gli_input_handle_key(keycode_Up); }},
        {{Qt::NoModifier, Qt::Key_Right},     []{ gli_input_handle_key(keycode_Right); }},
        {{Qt::NoModifier, Qt::Key_Down},      []{ gli_input_handle_key(keycode_Down); }},
        {{Qt::NoModifier, Qt::Key_PageUp},    []{ gli_input_handle_key(keycode_PageUp); }},
        {{Qt::NoModifier, Qt::Key_PageDown},  []{ gli_input_handle_key(keycode_PageDown); }},
        {{Qt::NoModifier, Qt::Key_Home},      []{ gli_input_handle_key(keycode_Home); }},
        {{Qt::NoModifier, Qt::Key_End},       []{ gli_input_handle_key(keycode_End); }},
        {{Qt::NoModifier, Qt::Key_Delete},    []{ gli_input_handle_key(keycode_Erase); }},
        {{Qt::NoModifier, Qt::Key_F1},        []{ gli_input_handle_key(keycode_Func1); }},
        {{Qt::NoModifier, Qt::Key_F2},        []{ gli_input_handle_key(keycode_Func2); }},
        {{Qt::NoModifier, Qt::Key_F3},        []{ gli_input_handle_key(keycode_Func3); }},
        {{Qt::NoModifier, Qt::Key_F4},        []{ gli_input_handle_key(keycode_Func4); }},
        {{Qt::NoModifier, Qt::Key_F5},        []{ gli_input_handle_key(keycode_Func5); }},
        {{Qt::NoModifier, Qt::Key_F6},        []{ gli_input_handle_key(keycode_Func6); }},
        {{Qt::NoModifier, Qt::Key_F7},        []{ gli_input_handle_key(keycode_Func7); }},
        {{Qt::NoModifier, Qt::Key_F8},        []{ gli_input_handle_key(keycode_Func8); }},
        {{Qt::NoModifier, Qt::Key_F9},        []{ gli_input_handle_key(keycode_Func9); }},
        {{Qt::NoModifier, Qt::Key_F10},       []{ gli_input_handle_key(keycode_Func10); }},
        {{Qt::NoModifier, Qt::Key_F11},       []{ gli_input_handle_key(keycode_Func11); }},
        {{Qt::NoModifier, Qt::Key_F12},       []{ gli_input_handle_key(keycode_Func12); }},
#ifdef Q_OS_MAC
        {{Qt::MetaModifier | Qt::ControlModifier, Qt::Key_F}, []{ toggle_fullscreen(); }},
#else
        {{Qt::AltModifier, Qt::Key_Return}, []{ toggle_fullscreen(); }},
#endif
    };

    auto it = keys.find({modmasked, key});
    if (it != keys.end()) {
        it->second();
        return;
    }

    for (const auto &ch : text) {
        if (ch.isPrint() || ch == '\n' || ch == '\r' || ch == '\t') {
            gli_input_handle_key(ch.unicode());
        }
    }
}

void handle_mouse(ipc::MouseKind kind, std::int32_t x, std::int32_t y, std::int32_t wheel_delta)
{
    switch (kind) {
    case ipc::MouseKind::Move:
        if (gli_copyselect) {
            send_msg(ipc::Msg::SetCursor, ipc::make_set_cursor(1));
            gli_move_selection(x, y);
        } else if (gli_get_hyperlink(x, y) != 0) {
            send_msg(ipc::Msg::SetCursor, ipc::make_set_cursor(2));
        } else {
            send_msg(ipc::Msg::SetCursor, ipc::make_set_cursor(0));
        }
        break;
    case ipc::MouseKind::Press:
        gli_input_handle_click(x, y);
        send_msg(ipc::Msg::SetCursor, ipc::make_set_cursor(0));
        break;
    case ipc::MouseKind::Release:
        gli_copyselect = false;
        send_msg(ipc::Msg::SetCursor, ipc::make_set_cursor(0));
        break;
    case ipc::MouseKind::Wheel:
        if (wheel_delta > 0) {
            gli_input_handle_key(keycode_MouseWheelUp);
        } else if (wheel_delta < 0) {
            gli_input_handle_key(keycode_MouseWheelDown);
        }
        break;
    }
}

bool wait_for(ipc::Msg expected, int timeout_ms = 60000)
{
    while (g_socket && g_socket->state() == QLocalSocket::ConnectedState) {
        ipc::Message msg;
        while (ipc::try_decode(g_readbuf, msg)) {
            if (msg.type == expected) {
                handle_message(msg);
                return true;
            }
            handle_message(msg);
        }
        if (!g_socket->waitForReadyRead(timeout_ms)) {
            return false;
        }
        g_readbuf.append(g_socket->readAll());
    }
    return false;
}

std::string dialog_common(bool save, const QString &prompt, FileFilter filter, const QString &savedir)
{
    g_dialog_result.reset();
    send_msg(save ? ipc::Msg::SaveDialog : ipc::Msg::OpenDialog,
            ipc::make_dialog(prompt, filter, savedir));
    if (!wait_for(ipc::Msg::DialogResult)) {
        return {};
    }
#ifdef _WIN32
    return g_dialog_result->toLocal8Bit().toStdString();
#else
    return g_dialog_result->toStdString();
#endif
}

} // namespace

bool connect_to_parent()
{
    if (!gli_conf_ipc) {
        return false;
    }

    g_socket = new QLocalSocket;
    g_socket->connectToServer(QString::fromStdString(ipc::server_name_from_config()));
    if (!g_socket->waitForConnected(1000)) {
        delete g_socket;
        g_socket = nullptr;
        return false;
    }

    QObject::connect(g_socket, &QLocalSocket::disconnected, []() {
        g_dead = true;
    });

    g_active = true;
    return true;
}

bool active()
{
    return g_active;
}

void init_window(bool move, int x, int y, int width, int height, bool fullscreen,
        unsigned char r, unsigned char g, unsigned char b)
{
    send_msg(ipc::Msg::InitWindow,
            ipc::make_init_window(move, x, y, width, height, fullscreen, r, g, b,
                static_cast<std::uint32_t>(garglk_getpid())));
    wait_for(ipc::Msg::BackingInfo, 5000);
    if (g_arrange_pending && g_back_w > 0 && g_back_h > 0) {
        gli_windows_size_change(g_back_w, g_back_h, false);
        g_arrange_pending = false;
    }
}

void set_title(const QString &title)
{
    send_msg(ipc::Msg::SetTitle, ipc::make_set_title(title));
}

bool set_contents(int width, int height, const unsigned char *rgb, std::size_t nbytes)
{
    send_msg(ipc::Msg::SetContents, ipc::make_set_contents(width, height, rgb, nbytes));
    return true;
}

void request_close()
{
    send_msg(ipc::Msg::CloseWindow);
}

std::string open_dialog(const QString &prompt, FileFilter filter, const QString &savedir)
{
    return dialog_common(false, prompt, filter, savedir);
}

std::string save_dialog(const QString &prompt, FileFilter filter, const QString &savedir)
{
    return dialog_common(true, prompt, filter, savedir);
}

void abort_dialog(const QString &prompt)
{
    send_msg(ipc::Msg::AbortDialog, ipc::make_abort_dialog(prompt));
}

void set_cursor(std::uint32_t cursor)
{
    send_msg(ipc::Msg::SetCursor, ipc::make_set_cursor(cursor));
}

void toggle_fullscreen()
{
    send_msg(ipc::Msg::ToggleFullScreen);
    wait_for(ipc::Msg::FullScreenResult, 2000);
}

bool is_fullscreen()
{
    send_msg(ipc::Msg::QueryFullScreen);
    wait_for(ipc::Msg::FullScreenResult, 1000);
    return g_fullscreen;
}

void poll()
{
    if (!g_socket) {
        return;
    }

    if (g_socket->bytesAvailable() > 0) {
        g_readbuf.append(g_socket->readAll());
    }

    ipc::Message msg;
    while (ipc::try_decode(g_readbuf, msg)) {
        handle_message(msg);
    }

    if (g_arrange_pending && g_back_w > 0 && g_back_h > 0) {
        gli_windows_size_change(g_back_w, g_back_h, true);
        g_arrange_pending = false;
    }

    if (g_dead) {
        gli_exit(0);
    }
}

bool window_dead()
{
    return g_dead;
}

void backing_size(int &width, int &height, double &dpr)
{
    width = g_back_w;
    height = g_back_h;
    dpr = g_back_dpr;
}

}
