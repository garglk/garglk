// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2010 by Ben Cressey.
// Copyright (C) 2010-2021 by Chris Spiegel.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <QApplication>
#include <QChar>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QMessageBox>
#include <QMoveEvent>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QProcess>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

#if GARGLK_CONFIG_HAS_QDBUS
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#endif

#ifdef GARGLK_CONFIG_KDE
#include <kio_version.h>

#if KIO_VERSION >= ((5 << 16) | (69 << 8))
#define GARGLK_KDE_HAS_APPLICATION_LAUNCHER
#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#if KIO_VERSION >= ((5 << 16) | (98 << 8))
#include <KIO/JobUiDelegateFactory>
#define create_delegate(window) (KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window))
#else
#include <KIO/JobUiDelegate>
#define create_delegate(window) (new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window))
#endif
#else
#include <KRun>
#endif
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "format.h"
#include "optional.hpp"

#include "sysqt.h"
#include "moc_sysqt.cpp"

#include "garversion.h"
#include "glk.h"
#include "garglk.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define HAS_QT6
#endif

// buffer for clipboard text
static QString cliptext;

// filters and extensions for file dialogs
static const std::unordered_map<FileFilter, std::pair<QString, QString>> filters = {
    {FileFilter::Save, std::make_pair("Saved game files (*.glksave *.sav)", "glksave")},
    {FileFilter::Text, std::make_pair("Text files (*.txt)", "txt")},
    {FileFilter::Data, std::make_pair("Data files (*.glkdata)", "glkdata")},
};

static QApplication *app;
static garglk::Window *window;

static bool refresh_needed = true;

static constexpr int TICK_PERIOD_MILLIS = 10;
static std::atomic<bool> process_events(false);

static void handle_input(const QString &input, bool from_paste)
{
    auto fn = from_paste ? gli_input_handle_key_paste :
                           gli_input_handle_key;

    for (const uint &c : input.toUcs4()) {
        if (c == '\r' || c == '\n') {
            fn(keycode_Return);
        } else if (QChar::isPrint(c)) {
            fn(c);
        }
    }
}

void glk_request_timer_events(glui32 ms)
{
    window->start_timer(ms);
}

void gli_notification_waiting()
{
    QApplication::postEvent(window, new QEvent(QEvent::None));
}

void garglk::winabort(const std::string &msg)
{
    std::cerr << "fatal: " << msg << std::endl;
    QMessageBox::critical(nullptr, "Error", msg.c_str());
    gli_exit(EXIT_FAILURE);
}

void garglk::winwarning(const std::string &title, const std::string &msg)
{
    std::cerr << "warning: " << msg << std::endl;
    QMessageBox::warning(nullptr, title.c_str(), msg.c_str());
}

void winexit()
{
    gli_exit(0);
}

enum class Action { Open, Save };

static std::string winchoosefile(const QString &prompt, FileFilter filter, Action action)
{
    QString filename;
    QFileDialog::Options options;
#ifdef GARGLK_CONFIG_NO_NATIVE_FILE_DIALOGS
    options |= QFileDialog::DontUseNativeDialog;
#endif

    QString dir = "";

    if (gli_conf_gamedata_location == GamedataLocation::Dedicated && gli_workfile.has_value()) {
        auto path = QFileInfo(QString::fromStdString(*gli_workfile));
        if (!path.fileName().isEmpty()) {
            QDir basedir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir savepath = QDir(basedir.filePath("gamedata")).filePath(path.fileName());
            if (savepath.mkpath(savepath.absolutePath())) {
                dir = savepath.absolutePath();
            }
        }
    } else if (gli_conf_gamedata_location == GamedataLocation::Gamedir) {
        dir = QString::fromStdString(gli_workdir);
    }

    if (action == Action::Open) {
        QString filterstring = QString("%1;;All files (*)").arg(filters.at(filter).first);
        filename = QFileDialog::getOpenFileName(window, prompt, dir, filterstring, nullptr, options);
    } else {
        if (dir == "") {
            dir += ".";
        }
        dir += QString("/Untitled.%1").arg(filters.at(filter).second);
        filename = QFileDialog::getSaveFileName(window, prompt, dir, filters.at(filter).first, nullptr, options);
    }

    // toStdString() converts to UTF-8, which is not used by Windows (at
    // least not by default). toLocal8Bit() will use the current locale
    // to determine an encoding. This allows many non-ASCII characters
    // in filenames, although only those that are in the current
    // codepage. In the future, explore Windows' UTF-8 support to
    // hopefully fully support Unicode.
#ifdef _WIN32
    return filename.toLocal8Bit().toStdString();
#else
    return filename.toStdString();
#endif
}

std::string garglk::winopenfile(const char *prompt, FileFilter filter)
{
    QString realprompt = QString("Open: %1").arg(prompt);
    return winchoosefile(realprompt, filter, Action::Open);
}

std::string garglk::winsavefile(const char *prompt, FileFilter filter)
{
    QString realprompt = QString("Save: %1").arg(prompt);
    return winchoosefile(realprompt, filter, Action::Save);
}

void winclipstore(const std::vector<glui32> &text)
{
    cliptext = QString::fromUcs4(reinterpret_cast<const char32_t *>(text.data()), text.size());
}

static void winclipsend(QClipboard::Mode mode)
{
    if (cliptext.isEmpty()) {
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();

    clipboard->setText(cliptext, mode);
}

static void winclipreceive(QClipboard::Mode mode)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text(mode);

    handle_input(text, true);
}

garglk::Window::Window() :
    m_view(new View(this)),
    m_timer(new QTimer(this)),
    // Qt programs have an organization and name that can be set, and
    // Gargoyle used to set these to "io.github.garglk" and "Gargoyle".
    // The QSettings here followed that. However, Gargoyle now uses an
    // empty organization and the name "gargoyle" (on Unix) so that
    // directories are more conventionally-named, e.g. /usr/share/gargoyle
    // instead of /usr/share/io.github.garglk/Gargoyle. But QSettings
    // _requires_ an organization name. Given that this is a setting
    // users aren't ever supposed to see anyhow, and that these exact
    // names were used in the past, keep them the same so that older
    // configurations can be loaded. Ideally this would probably just be
    // "gargoyle" and "gargoyle" but aesthetics are nowhere near as
    // important as not losing settings; and since nobody is going to
    // see these names in the normal course of using Gargoyle, it
    // doesn't really matter anyway.
    m_settings(new QSettings("io.github.garglk", "Gargoyle", this))
{
    m_timer->setTimerType(Qt::TimerType::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, [&]() {
        m_timed_out = true;
    });
}

void garglk::Window::closeEvent(QCloseEvent *)
{
    gli_exit(0);
}

void garglk::Window::resizeEvent(QResizeEvent *event)
{
    static bool first_resize = true;

    QMainWindow::resizeEvent(event);

    m_view->resize(event->size());

    int newwid = event->size().width();
    int newhgt = event->size().height();

    if (newwid == gli_image_rgb.width() && newhgt == gli_image_rgb.height()) {
        return;
    }

    refresh_needed = true;

    // On startup, Qt posts a resize event as the window is created.
    // This resize occurs before the Glk program even starts, so
    // shouldn't create an arrange event.
    gli_windows_size_change(newwid, newhgt, !first_resize);

    if (gli_conf_save_window_size) {
        m_settings->setValue("window/size", event->size());
    }

    if (gli_conf_save_window_location || gli_conf_save_window_size) {
        m_settings->setValue("window/fullscreen", ::window->isFullScreen());
    }

    event->accept();

    first_resize = false;
}

void garglk::Window::moveEvent(QMoveEvent *event)
{
    if (gli_conf_save_window_location) {
        m_settings->setValue("window/position", event->pos());
    }

    event->accept();
}

QVariant garglk::View::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return QVariant(true);
    default:
        return QVariant();
    }
}

// Handle compose key events (probably other input method events too).
// See https://stackoverflow.com/questions/28793356/qt-and-dead-keys-in-a-custom-widget
void garglk::View::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty()) {
        QKeyEvent key_event(QEvent::KeyPress, 0, Qt::NoModifier, event->commitString());
        keyPressEvent(&key_event);
    }
    event->accept();
}

void garglk::View::refresh()
{
    if (!gli_drawselect) {
        gli_windows_redraw();
    } else {
        gli_drawselect = false;
    }

    update();
    refresh_needed = false;
}

void garglk::View::paintEvent(QPaintEvent *event)
{
    QImage image(gli_image_rgb.data(), gli_image_rgb.width(), gli_image_rgb.height(), gli_image_rgb.stride(), QImage::Format_RGB888);
    QPainter painter(this);
    painter.drawImage(QPoint(0, 0), image);
    event->accept();
}

void garglk::Window::start_timer(unsigned long ms)
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }

    // QTimer::setInterval() takes int, so limit to avoid wrapping.
    if (ms > std::numeric_limits<int>::max()) {
        ms = std::numeric_limits<int>::max();
    }

    if (ms != 0) {
        m_timer->setInterval(ms);
        m_timer->start();
    }
}

void gli_edit_config()
{
    try {
        auto config = garglk::user_config();
#ifdef __APPLE__
        // QDesktopServices::openUrl uses the default handler for the
        // specified file. One possible filename is garglk.ini, meaning
        // that the handler for INI files will be used. This at least
        // works by default under Plasma & Gnome on Unix, and on
        // Windows. But on macOS, INI files aren't associated with a
        // text editor, so use "open -t" to explicitly request a text
        // editor. This would actually be the best solution on all
        // platforms, but Qt doesn't appear to be able to look up the
        // default program for a specific MIME type, so finding a user's
        // text editor can't be done easily.
        QProcess proc;
        QStringList args;
        args << "-t" << config.c_str();
        proc.startDetached("open", args);
#elif defined(GARGLK_CONFIG_KDE)
        // If KDE is available, it provides a way to query the user's
        // preferred text editor, allowing an approximation of the Mac
        // behavior. Fall back to QDesktopServices::openUrl otherwise.
        QUrl url(QUrl::fromLocalFile(config.c_str()));
#ifdef GARGLK_KDE_HAS_APPLICATION_LAUNCHER
        auto service = KApplicationTrader::preferredService("text/plain");
        auto *job = new KIO::ApplicationLauncherJob(service);
        job->setUiDelegate(create_delegate(window));
        job->setUrls({url});
        job->start();
#else
        KRun::runUrl(url, "text/plain", window);
#endif
#else
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(config.c_str()))) {
            QMessageBox::warning(nullptr, "Warning", "Unable to find a text editor");
        }
#endif
    } catch (std::runtime_error &e) {
        QMessageBox::warning(nullptr, "Warning", e.what());
    }
}

static void show_paths()
{
    // Convert to native separators and return absolute path.
    auto canonicalize = [](const std::string &path) {
        auto qpath = QString::fromStdString(path);
        qpath = QDir(qpath).absolutePath();
        return QDir::toNativeSeparators(qpath);
    };

    QString text("<p>Configuration file paths:</p><pre>");

    for (const auto &config : garglk::all_configs) {
        auto path = canonicalize(config.path);
        auto type = QString::fromStdString(config.format_type());

        text += QString("%1 %2\n").arg(path).arg(type);
    }

    text += "</pre><p>Theme paths:</p><pre>";
    auto theme_paths = garglk::theme::paths();
    std::reverse(theme_paths.begin(), theme_paths.end());
    for (const auto &path : theme_paths) {
        text += canonicalize(path) + "\n";
    }
    text += "</pre>";

    QMessageBox box(QMessageBox::Icon::Information, "Paths", text);
    box.setTextFormat(Qt::TextFormat::RichText);
    box.exec();
}

static void show_themes()
{
    QString text("The following themes are available:\n\n");

    for (const auto &theme_name : garglk::theme::names()) {
        text += QString("• ") + QString::fromStdString(theme_name) + "\n";
    }

    QMessageBox box(QMessageBox::Icon::Information, "Themes", text);
    box.setTextFormat(Qt::TextFormat::PlainText);
    box.exec();
}

// On Mac, Qt::ControlModifier means the command key. But for Emacs
// keys, we need the actual control key.
#ifdef Q_OS_MAC
static constexpr Qt::KeyboardModifier RealCtrl = Qt::MetaModifier;
#else
static constexpr Qt::KeyboardModifier RealCtrl = Qt::ControlModifier;
#endif

void garglk::View::keyPressEvent(QKeyEvent *event)
{
    Qt::KeyboardModifiers modmasked = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);

    refresh_needed = true;

    static const std::map<QKeySequence::StandardKey, std::function<void()>> sequences = {
        {QKeySequence::Cut,                []{ winclipsend(QClipboard::Clipboard); }},
        {QKeySequence::Copy,               []{ winclipsend(QClipboard::Clipboard); }},
        {QKeySequence::Paste,              []{ winclipreceive(QClipboard::Clipboard); }},
        {QKeySequence::MoveToPreviousWord, []{ gli_input_handle_key(keycode_SkipWordLeft); }},
        {QKeySequence::MoveToNextWord,     []{ gli_input_handle_key(keycode_SkipWordRight); }},
        {QKeySequence::Quit,               []{ gli_exit(0); }},
        {QKeySequence::Delete,             []{ gli_input_handle_key(keycode_Erase); }},
        {QKeySequence::MoveToStartOfLine,  []{ gli_input_handle_key(keycode_Home); }},
        {QKeySequence::MoveToEndOfLine,    []{ gli_input_handle_key(keycode_End); }},
        {QKeySequence::MoveToPreviousPage, []{ gli_input_handle_key(keycode_PageUp); }},
        {QKeySequence::MoveToNextPage,     []{ gli_input_handle_key(keycode_PageDown); }},
    };

    static const std::map<std::pair<decltype(modmasked), decltype(event->key())>, std::function<void()>> keys = {
        // Emacs keys.
        {{RealCtrl, Qt::Key_A}, []{ gli_input_handle_key(keycode_Home); }},
        {{RealCtrl, Qt::Key_B}, []{ gli_input_handle_key(keycode_Left); }},
        {{RealCtrl, Qt::Key_D}, []{ gli_input_handle_key(keycode_Erase); }},
        {{RealCtrl, Qt::Key_E}, []{ gli_input_handle_key(keycode_End); }},
        {{RealCtrl, Qt::Key_F}, []{ gli_input_handle_key(keycode_Right); }},
        {{RealCtrl, Qt::Key_H}, []{ gli_input_handle_key(keycode_Delete); }},
        {{RealCtrl, Qt::Key_N}, []{ gli_input_handle_key(keycode_Down); }},
        {{RealCtrl, Qt::Key_P}, []{ gli_input_handle_key(keycode_Up); }},
        {{RealCtrl, Qt::Key_U}, []{ gli_input_handle_key(keycode_Escape); }},

#ifdef Q_OS_WIN
        // Qt doesn't assign Ctrl-Q to QKeySequence::Quit on Windows.
        {{Qt::ControlModifier, Qt::Key_Q}, []{ gli_exit(0); }},
#endif

#ifdef __HAIKU__
        // For some reason, on Haiku, the "shifted" versions of comma/period are
        // used, which, on US English keyboards, at least, are less-than/
        // greater-than signs. I assume this is a bug in translating
        // Haiku input to Qt input, but I'm also not completely sure it's wrong,
        // either; the QKeyEvent documentations says that key() "does not
        // distinguish between capital and non-capital letters", but these
        // aren't letters...
        {{Qt::ControlModifier, Qt::Key_Less}, gli_edit_config},
        {{Qt::ControlModifier, Qt::Key_Greater}, show_paths},
#else
        {{Qt::ControlModifier, Qt::Key_Comma}, gli_edit_config},
        {{Qt::ControlModifier, Qt::Key_Period}, show_paths},
#endif

        {{Qt::ShiftModifier | Qt::ControlModifier, Qt::Key_T}, [] { show_themes(); }},

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

        {{Qt::ShiftModifier | Qt::ControlModifier, Qt::Key_S}, []{
            auto text = gli_get_scrollback();
            if (text.has_value()) {
                auto filename = QFileDialog::getSaveFileName(::window, "Save transcript", "transcript.txt", "Text files (*.txt)");
                if (!filename.isNull()) {
                    QFile file(filename);
                    if (file.open(QIODevice::WriteOnly)) {
                        std::size_t n = file.write(text->data(), text->size());
                        if (n != text->size()) {
                            QMessageBox::critical(nullptr, "Error", "Error writing entire transcript.");
                        }
                    } else {
                        QMessageBox::critical(nullptr, "Error", "Unable to open file for writing.");
                    }
                }
            } else {
                QMessageBox::warning(nullptr, "Warning", "Could not find appropriate window for scrollback.");
            }
        }},

        // Don't use QKeySequence::FullScreen here, as that takes over
        // F11 on X, and F11 can be bound by Glk; and it doesn't bind
        // Alt-Enter on X, either, meaning the existing behavior would
        // change. For Mac, use Meta+Ctrl+F, and Alt-Enter everywhere
        // else.
#ifdef Q_OS_MAC
        {{Qt::MetaModifier | Qt::ControlModifier, Qt::Key_F}, [this]{
#else
        {{Qt::AltModifier, Qt::Key_Return}, [this]{
#endif
            if (::window->isFullScreen()) {
                if (m_fullscreen_from_maximized) {
                    ::window->showMaximized();
                } else {
                    ::window->showNormal();
                }
            } else {
                m_fullscreen_from_maximized = ::window->isMaximized();
                ::window->showFullScreen();
            }
        }},
    };

    for (const auto &pair : sequences) {
        if (event->matches(pair.first)) {
            pair.second();
            return;
        }
    }

    try {
        keys.at(std::make_pair(modmasked, event->key()))();
        return;
    } catch (const std::out_of_range &) {
    }

    handle_input(event->text(), false);
}

void garglk::View::mouseMoveEvent(QMouseEvent *event)
{
    // hyperlinks and selection
    if (gli_copyselect) {
        setCursor(Qt::IBeamCursor);
        gli_move_selection(event->pos().x(), event->pos().y());
    } else {
        if (gli_get_hyperlink(event->pos().x(), event->pos().y()) != 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }
    }

    event->accept();
}

void garglk::View::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        gli_input_handle_click(event->pos().x(), event->pos().y());
    } else if (event->button() == Qt::MiddleButton) {
        winclipreceive(QClipboard::Selection);
    }

    event->accept();
}

void garglk::View::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        gli_copyselect = false;
        unsetCursor();
        winclipsend(QClipboard::Selection);
    }

    event->accept();
}

void garglk::View::wheelEvent(QWheelEvent *event)
{
    QPoint pixels = event->pixelDelta();
    QPoint degrees = event->angleDelta() / 8;
    int change = 0;
    bool page = event->modifiers() == Qt::ShiftModifier;

    if (!pixels.isNull()) {
        change = pixels.y();
    } else if (!degrees.isNull()) {
        change = degrees.y();
    }

    if (change == 0) {
        return;
    }

    if (change > 0) {
        if (page) {
            gli_input_handle_key(keycode_PageUp);
        } else {
            gli_input_handle_key(keycode_MouseWheelUp);
        }

    } else if (change < 0) {
        if (page) {
            gli_input_handle_key(keycode_PageDown);
        } else {
            gli_input_handle_key(keycode_MouseWheelDown);
        }
    }

    event->accept();
}

void wininit()
{
    // QApplication takes argc by reference (because it might modify
    // it), and thus requires it to live at least as long as the
    // QApplication instance. This isn't really a Qt program in the
    // normal sense, so doesn't need to provide Qt command-line
    // arguments (such as -style). Create static dummy argument data
    // here so it will live the entire life of the program and thus
    // fulfill QApplication's requirements.
    static int argc = 1;
    static char *argv[] = {const_cast<char *>("gargoyle"), nullptr};
    app = new QApplication(argc, argv);
    QApplication::setApplicationVersion(GARGOYLE_VERSION);

    // The convention on Windows & Mac is capitalized directories,
    // whereas on Unix it's all lowercase. Qt uses the application name
    // when finding paths with QStandardDirectories, so set the expected
    // name based on operating system.
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QApplication::setApplicationName("Gargoyle");
#else
    QApplication::setApplicationName("gargoyle");
#endif

    std::thread([]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TICK_PERIOD_MILLIS));
            process_events.store(true, std::memory_order_relaxed);
        }
    })
    .detach();
}

void winopen()
{
    window = new garglk::Window();

    window->setMinimumSize(gli_wmarginx * 2, gli_wmarginy * 2);

    int defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    int defh = gli_wmarginy * 2 + gli_cellh * gli_rows;
    QSize size(defw, defh);
    if (gli_conf_save_window_size) {
        auto stored_size = window->settings()->value("window/size");
        if (stored_size.canConvert<QSize>()) {
            size = stored_size.toSize();
        }
    }
    window->resize(size);

    if (gli_conf_save_window_location) {
        auto position = window->settings()->value("window/position");
        if (position.canConvert<QPoint>()) {
            window->move(position.toPoint());
        }
    }

    wintitle();

    bool do_fullscreen = gli_conf_fullscreen;

    if (gli_conf_save_window_location || gli_conf_save_window_size) {
        auto fullscreen = window->settings()->value("window/fullscreen");
        if (fullscreen.canConvert<bool>()) {
            do_fullscreen = fullscreen.toBool();
        }
    }

    if (do_fullscreen) {
        window->showFullScreen();
    } else {
        window->show();
    }
}

void wintitle()
{
    QString title;

    if (!gli_story_title.empty()) {
        title = QString::fromStdString(gli_story_title);
    } else if (!gli_story_name.empty()) {
        title = QString("%1 - %2").arg(QString::fromStdString(gli_story_name), QString::fromStdString(gli_program_name));
    } else {
        title = QString::fromStdString(gli_program_name);
    }

    window->setWindowTitle(title);
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    refresh_needed = true;
}

bool windark()
{
#if GARGLK_CONFIG_HAS_QDBUS
    // https://flatpak.github.io/xdg-desktop-portal/
    QDBusInterface interface("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings");
    QDBusReply<QVariant> reply = interface.call("Read", "org.freedesktop.appearance", "color-scheme");
    if (reply.isValid()) {
        auto dbusvar = qvariant_cast<QDBusVariant>(reply.value());
        QVariant result = dbusvar.variant();
#ifdef HAS_QT6
        if (result.typeId() == QMetaType::Type::UInt) {
#else
        if (result.type() == QVariant::UInt) {
#endif
            return result.toUInt() == 1;
        }
    }
#endif

    // From https://stackoverflow.com/a/69705673/1017090
    // Is there really no builtin way in Qt to check this?
    QLabel label("");
    auto text_hsv_value = label.palette().color(QPalette::WindowText).value();
    auto bg_hsv_value = label.palette().color(QPalette::Window).value();

    return text_hsv_value > bg_hsv_value;
}

nonstd::optional<std::string> garglk::winfontpath(const std::string &filename)
{
    return Format("{}/{}", QCoreApplication::applicationDirPath().toStdString(), filename);
}

std::string garglk::windatadir()
{
#if defined(_WIN32)
    return garglk::winappdir().value_or(".");
#elif GARGLK_CONFIG_APPIMAGE
    // For AppImages, hard-code the "known" path to app data (in this
    // case that's <binary>/../share/gargoyle).
    auto dir = QCoreApplication::applicationDirPath().toStdString();
    return Format("{}/../share/gargoyle", dir);
#elif defined(GARGLK_CONFIG_DATADIR)
    return GARGLK_CONFIG_DATADIR;
#else
    return ".";
#endif
}

std::vector<std::string> garglk::winthemedirs()
{
    std::vector<std::string> paths;

    for (const auto &path : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)) {
        paths.push_back(path.toStdString() + "/themes");
    }

#ifdef _WIN32
    // On Windows, also search the executable's directory.
    paths.push_back(QCoreApplication::applicationDirPath().toStdString() + "/themes");
#endif

#if GARGLK_CONFIG_APPIMAGE
    paths.push_back(Format("{}/themes", garglk::windatadir()));
#endif

    // QStandardPaths returns higher priority directories first: reverse
    // so that higher priority directories come later, so entries there
    // can overwrite earlier entries.
    std::reverse(paths.begin(), paths.end());

    return paths;
}

nonstd::optional<std::string> garglk::winlegacythemedir() {
#ifdef _WIN32
    const char *appdata = std::getenv("APPDATA");
    if (appdata == nullptr) {
        return nonstd::nullopt;
    }

    return std::string(appdata) + "\\io.github.garglk\\Gargoyle\\themes";
#else
    QString user_share = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir legacy_themes = QDir(user_share).filePath("io.github.garglk/Gargoyle/themes");
    return legacy_themes.path().toStdString();
#endif
}

nonstd::optional<std::string> garglk::winappdir()
{
    return QCoreApplication::applicationDirPath().toStdString();
}

bool garglk::winisfullscreen()
{
    return window->isFullScreen();
}

void gli_tick()
{
    // Qt needs to keep processing events even in the absence of calls
    // to glk_select(). Processing Qt events is expensive, so should not
    // be done each tick (which generally happens each VM instruction).
    // Originally this waited at least 10ms between calls, but the mere
    // act of checking a timer each iteration was too expensive. Now a
    // separate thread sits and atomically updates "process_events"
    // every 10ms, since checking this atomic variable is much faster
    // than checking a timer.
    if (process_events.load(std::memory_order_relaxed)) {
        app->processEvents(QEventLoop::ExcludeUserInputEvents);
        process_events.store(false, std::memory_order_relaxed);
    }
}

void gli_select(event_t *event, bool polled)
{
    gli_event_clearevent(event);

    app->processEvents();

    gli_dispatch_event(event, polled);

    if (refresh_needed) {
        window->refresh();
    }

    if (!polled) {
        while (event->type == evtype_None && !window->timed_out()) {
            if (refresh_needed) {
                window->refresh();
            }

            app->processEvents(QEventLoop::WaitForMoreEvents);
            gli_dispatch_event(event, polled);
        }
    }

    if (event->type == evtype_None && window->timed_out()) {
        gli_event_store(evtype_Timer, nullptr, 0, 0);
        gli_dispatch_event(event, polled);
        window->reset_timeout();
    }

    process_events.store(false, std::memory_order_relaxed);
}

void garglk::show_game_info(const garglk::GameInfo &info, bool show_once)
{
    if (show_once && info.ifid.has_value()) {
        QSettings settings("io.github.garglk", "Gargoyle");
        auto seen = settings.value("games/info_shown").toStringList();
        auto qifid = QString::fromStdString(*info.ifid);
        if (seen.contains(qifid)) {
            return;
        }
        seen.append(qifid);
        settings.setValue("games/info_shown", seen);
    }

    auto title = QString::fromStdString(info.title);
    auto author = QString::fromStdString(info.author);

    QDialog dialog;
    dialog.setWindowTitle(title);

    auto *outer_layout = new QVBoxLayout(&dialog);
    auto *content_layout = new QHBoxLayout;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    auto screen_geometry = dialog.screen()->availableGeometry();
#else
    auto screen_geometry = QGuiApplication::primaryScreen()->availableGeometry();
#endif
    int third_screen = screen_geometry.width() / 3;

    // Cover art on the left, scaled to 1/3 screen width
    if (info.cover.has_value()) {
        QPixmap pixmap;
        if (pixmap.loadFromData(info.cover->data(), static_cast<uint>(info.cover->size()))) {
            double ratio = static_cast<double>(third_screen) / pixmap.width();
            auto *image_label = new QLabel;
            image_label->setPixmap(pixmap.scaled(
                pixmap.width() * ratio,
                pixmap.height() * ratio,
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
            image_label->setAlignment(Qt::AlignTop);
            content_layout->addWidget(image_label);
        }
    }

    QString html;
    html += QString("<b style='font-size: large;'>%1</b><br/>").arg(title.toHtmlEscaped());
    if (info.headline.has_value()) {
        html += QString("<i>%1</i><br/>").arg(QString::fromStdString(*info.headline).toHtmlEscaped());
    }
    html += QString("by %1").arg(author.toHtmlEscaped());
    for (const auto &paragraph : info.description) {
        html += QString("<p>%1</p>").arg(QString::fromStdString(paragraph).toHtmlEscaped());
    }

    auto *text_view = new QTextBrowser;
    text_view->setHtml(html);
    text_view->setReadOnly(true);
    text_view->setOpenExternalLinks(false);
    text_view->setFrameShape(QFrame::NoFrame);
    text_view->setFixedWidth(third_screen);
    content_layout->addWidget(text_view, 1);

    outer_layout->addLayout(content_layout, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    outer_layout->addWidget(buttons);

    int max_height = screen_geometry.height() * 2 / 3;
    dialog.setMaximumHeight(max_height);
    dialog.adjustSize();
    dialog.setFixedSize(dialog.size());
    dialog.exec();
}
