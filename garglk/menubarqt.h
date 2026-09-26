// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.

#ifndef GARGLK_MENUBARQT_H
#define GARGLK_MENUBARQT_H

#include <functional>

#include <QMainWindow>
#include <QSettings>
#include <QString>
#include <QWidget>

#include "garglk.h"

namespace garglk {

// Shared menus used by the IPC session host and the non-IPC interpreter
// window fallback.
GARGLK_API void note_recent_file(QSettings *settings, const QString &path);
GARGLK_API QString browse_for_game(QWidget *parent = nullptr);
GARGLK_API void setup_menus(QMainWindow *window, QSettings *settings,
        const std::function<void(const QString &)> &open_game,
        const std::function<void()> &on_close,
        const std::function<void()> &on_exit,
        const std::function<void()> &on_cut,
        const std::function<void()> &on_copy,
        const std::function<void()> &on_paste);

}

#endif
