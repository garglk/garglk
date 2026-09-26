// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.

#include "menubarqt.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStringList>
#include <QVector>

#include "garglk.h"

namespace garglk {

namespace {

constexpr int MAX_RECENT_FILES = 10;
const char *RECENT_FILES_KEY = "recent/files";
const char *SETTINGS_ORG = "io.github.garglk";
const char *SETTINGS_APP = "Gargoyle";

// Prefer the folder of the most recently opened game; otherwise Downloads.
QString default_browse_directory(QSettings *settings)
{
    auto recent = settings->value(RECENT_FILES_KEY).toStringList();
    for (const auto &path : recent) {
        QFileInfo info(path);
        auto dir = info.absolutePath();
        if (!dir.isEmpty() && QDir(dir).exists()) {
            return dir;
        }
    }

    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

void populate_recent_menu(QMenu *recent_menu, QSettings *settings,
        const std::function<void(const QString &)> &open_game, QWidget *parent)
{
    recent_menu->clear();

    auto recent = settings->value(RECENT_FILES_KEY).toStringList();
    if (recent.isEmpty()) {
        auto *empty = recent_menu->addAction("No Recent Files");
        empty->setEnabled(false);
        return;
    }

    for (const auto &path : recent) {
        auto *action = recent_menu->addAction(QFileInfo(path).fileName());
        action->setData(path);
        action->setToolTip(path);
        QObject::connect(action, &QAction::triggered, parent, [parent, settings, open_game, path] {
            if (!QFileInfo::exists(path)) {
                QMessageBox::warning(parent, "Warning", QString("File not found:\n%1").arg(path));
                auto recent = settings->value(RECENT_FILES_KEY).toStringList();
                recent.removeAll(path);
                settings->setValue(RECENT_FILES_KEY, recent);
                return;
            }
            open_game(path);
        });
    }
}

void show_about(QWidget *parent)
{
    auto version = QApplication::applicationVersion();
    if (version.isEmpty()) {
        version = "unknown";
    }
    QMessageBox::about(parent, "About Gargoyle",
            QString("<h3>Gargoyle</h3>"
                    "<p>Version %1</p>"
                    "<p>An interactive fiction player</p>").arg(version));
}

}

void note_recent_file(QSettings *settings, const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    auto absolute = QFileInfo(path).absoluteFilePath();
    auto recent = settings->value(RECENT_FILES_KEY).toStringList();
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while (recent.size() > MAX_RECENT_FILES) {
        recent.removeLast();
    }
    settings->setValue(RECENT_FILES_KEY, recent);
}

QString browse_for_game(QWidget *parent)
{
    struct Filter {
        QString name;
        QStringList extensions;
    };

    const QVector<Filter> game_filters = {
        {"Adrift", {"taf"}},
        {"AdvSys", {"dat"}},
        {"AGT", {"agx", "d$$"}},
        {"Alan", {"acd", "a3c"}},
        {"Glulx", {"ulx", "blb", "blorb", "glb", "gblorb"}},
        {"Hugo", {"hex"}},
        {"JACL", {"jacl", "j2"}},
        {"Level 9", {"l9", "sna"}},
        {"Magnetic Scrolls", {"mag"}},
        {"TADS", {"gam", "t3"}},
        {"Z-code", {"z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "zlb", "zblorb"}},
    };

    QStringList mapped_filters;
    QStringList all_extensions;
    for (const auto &filter : game_filters) {
        QStringList exts;
        for (const auto &ext : filter.extensions) {
            exts << QString("*.%1").arg(ext);
        }
        all_extensions << exts;
        mapped_filters << QString("%1 Games (%2)").arg(filter.name, exts.join(" "));
    }

    // Hide filter details because the sheer number in "All Games" makes
    // the dialog ridiculously wide (Qt probably should cut it off, but
    // it doesn't, so try to compensate here).
    QString filter_string = QString("All Games (%1);;All Files (*);;%2")
        .arg(all_extensions.join(" "), mapped_filters.join(";;"));

    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    auto start_dir = default_browse_directory(&settings);

    QFileDialog::Options options(QFileDialog::HideNameFilterDetails);
#ifdef GARGLK_CONFIG_NO_NATIVE_FILE_DIALOGS
    options |= QFileDialog::DontUseNativeDialog;
#endif
    return QFileDialog::getOpenFileName(parent, "Open", start_dir, filter_string, nullptr, options);
}

void setup_menus(QMainWindow *window, QSettings *settings,
        const std::function<void(const QString &)> &open_game,
        const std::function<void()> &on_close,
        const std::function<void()> &on_exit,
        const std::function<void()> &on_cut,
        const std::function<void()> &on_copy,
        const std::function<void()> &on_paste)
{
    auto *file_menu = window->menuBar()->addMenu("&File");

    auto *open_action = file_menu->addAction("&Open…");
    open_action->setShortcut(QKeySequence::Open);
    QObject::connect(open_action, &QAction::triggered, window, [window, open_game] {
        auto game = browse_for_game(window);
        if (!game.isEmpty()) {
            open_game(game);
        }
    });

    auto *recent_menu = file_menu->addMenu("Open &Recent");
    auto populate = [recent_menu, settings, open_game, window] {
        populate_recent_menu(recent_menu, settings, open_game, window);
    };
    QObject::connect(recent_menu, &QMenu::aboutToShow, window, populate);
    populate();

    file_menu->addSeparator();

    auto *close_action = file_menu->addAction("&Close", window, on_close);
    close_action->setShortcut(QKeySequence::Close);

    file_menu->addSeparator();

    auto *settings_action = file_menu->addAction("&Settings", window, [] {
        gli_edit_config();
    });
    // QKeySequence::Preferences is empty on Windows; Ctrl+, matches the
    // existing key binding in keyPressEvent.
    auto prefs = QKeySequence::keyBindings(QKeySequence::Preferences);
    if (prefs.isEmpty()) {
        settings_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    } else {
        settings_action->setShortcuts(prefs);
    }

    file_menu->addSeparator();

    auto *exit_action = file_menu->addAction("E&xit", window, on_exit);
    // QKeySequence::Quit is empty on Windows; Ctrl+Q matches the
    // existing key binding in keyPressEvent.
    auto quit = QKeySequence::keyBindings(QKeySequence::Quit);
    if (quit.isEmpty()) {
        exit_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    } else {
        exit_action->setShortcuts(quit);
    }

    auto *edit_menu = window->menuBar()->addMenu("&Edit");
    auto *cut_action = edit_menu->addAction("Cu&t", window, on_cut);
    cut_action->setShortcut(QKeySequence::Cut);
    auto *copy_action = edit_menu->addAction("&Copy", window, on_copy);
    copy_action->setShortcut(QKeySequence::Copy);
    auto *paste_action = edit_menu->addAction("&Paste", window, on_paste);
    paste_action->setShortcut(QKeySequence::Paste);

    auto *help_menu = window->menuBar()->addMenu("&Help");
    auto *about_action = help_menu->addAction("&About Gargoyle", window, [window] {
        show_about(window);
    });
    about_action->setMenuRole(QAction::AboutRole);
}

}
