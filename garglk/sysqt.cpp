/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 * Copyright (C) 2010-2021 by Chris Spiegel.                                  *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <QApplication>
#include <QChar>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QResizeEvent>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include "sysqt.h"
#include "moc_sysqt.cpp"

#include "glk.h"
#include "garglk.h"

/* buffer for clipboard text */
static QString cliptext;

/* filters and extensions for file dialogs */
static const std::map<FILEFILTERS, std::pair<QString, QString>> filters = {
    { FILTER_SAVE, std::make_pair("Saved game files (*.glksave *.sav)", "glksave") },
    { FILTER_TEXT, std::make_pair("Text files (*.txt)", "txt") },
    { FILTER_DATA, std::make_pair("Data files (*.glkdata)", "glkdata") },
};

static QApplication *app;
static Window *window;
static QElapsedTimer last_tick;
#define TICK_PERIOD_MILLIS 10

static void handle_input(const QString &input)
{
    for (const uint &c : input.toUcs4())
    {
        if (c == '\r' || c == '\n')
            gli_input_handle_key(keycode_Return);
        else if (QChar::isPrint(c))
            gli_input_handle_key(c);
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
    std::fprintf(stderr, "fatal: %s\n", msg.c_str());
    QMessageBox::critical(nullptr, "Error", msg.c_str());
    std::exit(EXIT_FAILURE);
}

void winexit()
{
    std::exit(0);
}

enum class Action { Open, Save };

static std::string winchoosefile(const QString &prompt, FILEFILTERS filter, Action action)
{
    QString filename;
    QFileDialog::Options options;
#ifdef GARGLK_NO_NATIVE_FILE_DIALOGS
    options |= QFileDialog::DontUseNativeDialog;
#endif

    if (action == Action::Open)
    {
        QString filterstring = QString("%1;;All files (*)").arg(filters.at(filter).first);
        filename = QFileDialog::getOpenFileName(window, prompt, "", filterstring, nullptr, options);
    }
    else
    {
        QString dir = QString("./Untitled.%1").arg(filters.at(filter).second);
        filename = QFileDialog::getSaveFileName(window, prompt, dir, filters.at(filter).first, nullptr, options);
    }

    return filename.toStdString();
}

std::string garglk::winopenfile(const char *prompt, FILEFILTERS filter)
{
    QString realprompt = QString("Open: %1").arg(prompt);
    return winchoosefile(realprompt, filter, Action::Open);
}

std::string garglk::winsavefile(const char *prompt, FILEFILTERS filter)
{
    QString realprompt = QString("Save: %1").arg(prompt);
    return winchoosefile(realprompt, filter, Action::Save);
}

void winclipstore(glui32 *text, int len)
{
    cliptext = QString::fromUcs4(reinterpret_cast<const char32_t *>(text), len);
}

static void winclipsend(QClipboard::Mode mode)
{
    if (cliptext.isEmpty())
        return;

    QClipboard *clipboard = QGuiApplication::clipboard();

    clipboard->setText(cliptext, mode);
}

static void winclipreceive(QClipboard::Mode mode)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text(mode);

    handle_input(text);
}

Window::Window() :
    m_view(new View(this)),
    m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, [&]() { m_timed_out = true; });
}

Window::~Window()
{
    delete m_view;
    delete m_timer;
}

void Window::closeEvent(QCloseEvent *)
{
    std::exit(0);
}

void Window::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    m_view->resize(event->size());

    int newwid = event->size().width();
    int newhgt = event->size().height();

    if (newwid == gli_image_w && newhgt == gli_image_h)
        return;

    gli_image_w = newwid;
    gli_image_h = newhgt;

    gli_resize_mask(gli_image_w, gli_image_h);

    gli_image_s = ((gli_image_w * 4 + 3) / 4) * 4;
    delete [] gli_image_rgb;
    gli_image_rgb = new unsigned char[gli_image_s * gli_image_h];

    gli_force_redraw = true;

    gli_windows_size_change();

    event->accept();
}

QVariant View::inputMethodQuery(Qt::InputMethodQuery query) const {
    switch (query)
    {
    case Qt::ImEnabled:
        return QVariant(true);
    default:
        return QVariant();
    }
}

// Handle compose key events (probably other input method events too).
// See https://stackoverflow.com/questions/28793356/qt-and-dead-keys-in-a-custom-widget
void View::inputMethodEvent(QInputMethodEvent *event) {
    if (!event->commitString().isEmpty())
    {
        QKeyEvent key_event(QEvent::KeyPress, 0, Qt::NoModifier, event->commitString());
        keyPressEvent(&key_event);
    }
    event->accept();
}

void View::paintEvent(QPaintEvent *event)
{
    if (!gli_drawselect)
        gli_windows_redraw();
    else
        gli_drawselect = false;

    QImage image(gli_image_rgb, gli_image_w, gli_image_h, QImage::Format_RGB32);
    QPainter painter(this);
    painter.drawImage(QPoint(0, 0), image);
    event->accept();
}

void Window::start_timer(long ms)
{
    if (m_timer->isActive())
        m_timer->stop();

    if (ms != 0)
    {
        m_timer->setInterval(ms);
        m_timer->start();
    }
}

static void edit_config()
{
    try
    {
        auto config = garglk::user_config();
        QDesktopServices::openUrl(QUrl::fromLocalFile(config.c_str()));
    }
    catch (std::runtime_error &e)
    {
        QMessageBox::warning(nullptr, "Warning", e.what());
    }
}

void View::keyPressEvent(QKeyEvent *event)
{
    Qt::KeyboardModifiers modmasked = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);

    static const std::map<std::pair<decltype(modmasked), decltype(event->key())>, std::function<void()>> keys = {
        {{Qt::ControlModifier, Qt::Key_A},     []{ gli_input_handle_key(keycode_Home); }},
        {{Qt::ControlModifier, Qt::Key_B},     []{ gli_input_handle_key(keycode_Left); }},
        {{Qt::ControlModifier, Qt::Key_C},     []{ winclipsend(QClipboard::Clipboard); }},
        {{Qt::ControlModifier, Qt::Key_D},     []{ gli_input_handle_key(keycode_Erase); }},
        {{Qt::ControlModifier, Qt::Key_E},     []{ gli_input_handle_key(keycode_End); }},
        {{Qt::ControlModifier, Qt::Key_F},     []{ gli_input_handle_key(keycode_Right); }},
        {{Qt::ControlModifier, Qt::Key_N},     []{ gli_input_handle_key(keycode_Down); }},
        {{Qt::ControlModifier, Qt::Key_P},     []{ gli_input_handle_key(keycode_Up); }},
        {{Qt::ControlModifier, Qt::Key_U},     []{ gli_input_handle_key(keycode_Escape); }},
        {{Qt::ControlModifier, Qt::Key_V},     []{ winclipreceive(QClipboard::Clipboard); }},
        {{Qt::ControlModifier, Qt::Key_X},     []{ winclipsend(QClipboard::Clipboard); }},
        {{Qt::ControlModifier, Qt::Key_Left},  []{ gli_input_handle_key(keycode_SkipWordLeft); }},
        {{Qt::ControlModifier, Qt::Key_Right}, []{ gli_input_handle_key(keycode_SkipWordRight); }},

        {{Qt::ControlModifier, Qt::Key_Comma}, edit_config},

        {{Qt::ShiftModifier, Qt::Key_Backspace}, []{ gli_input_handle_key(keycode_Delete); }},

        {{Qt::NoModifier, Qt::Key_Escape},    []{ gli_input_handle_key(keycode_Escape); }},
        {{Qt::NoModifier, Qt::Key_Tab},       []{ gli_input_handle_key(keycode_Tab); }},
        {{Qt::NoModifier, Qt::Key_Backspace}, []{ gli_input_handle_key(keycode_Delete); }},
        {{Qt::NoModifier, Qt::Key_Delete},    []{ gli_input_handle_key(keycode_Erase); }},
        {{Qt::NoModifier, Qt::Key_Return},    []{ gli_input_handle_key(keycode_Return); }},
        {{Qt::NoModifier, Qt::Key_Enter},     []{ gli_input_handle_key(keycode_Return); }},
        {{Qt::NoModifier, Qt::Key_Home},      []{ gli_input_handle_key(keycode_Home); }},
        {{Qt::NoModifier, Qt::Key_End},       []{ gli_input_handle_key(keycode_End); }},
        {{Qt::NoModifier, Qt::Key_Left},      []{ gli_input_handle_key(keycode_Left); }},
        {{Qt::NoModifier, Qt::Key_Up},        []{ gli_input_handle_key(keycode_Up); }},
        {{Qt::NoModifier, Qt::Key_Right},     []{ gli_input_handle_key(keycode_Right); }},
        {{Qt::NoModifier, Qt::Key_Down},      []{ gli_input_handle_key(keycode_Down); }},
        {{Qt::NoModifier, Qt::Key_PageUp},    []{ gli_input_handle_key(keycode_PageUp); }},
        {{Qt::NoModifier, Qt::Key_PageDown},  []{ gli_input_handle_key(keycode_PageDown); }},
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
    };

    try
    {
        keys.at(std::make_pair(modmasked, event->key()))();
        return;
    }
    catch (const std::out_of_range &)
    {
    }

    handle_input(event->text());
}

void View::mouseMoveEvent(QMouseEvent *event)
{
    /* hyperlinks and selection */
    if (gli_copyselect)
    {
        setCursor(Qt::IBeamCursor);
        gli_move_selection(event->pos().x(), event->pos().y());
    }
    else
    {
        if (gli_get_hyperlink(event->pos().x(), event->pos().y()))
            setCursor(Qt::PointingHandCursor);
        else
            unsetCursor();
    }

    event->accept();
}

void View::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        gli_input_handle_click(event->pos().x(), event->pos().y());
    else if (event->button() == Qt::MiddleButton)
        winclipreceive(QClipboard::Selection);

    event->accept();
}

void View::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        gli_copyselect = false;
        unsetCursor();
        winclipsend(QClipboard::Selection);
    }

    event->accept();
}

void View::wheelEvent(QWheelEvent *event)
{
    QPoint pixels = event->pixelDelta();
    QPoint degrees = event->angleDelta() / 8;
    int change = 0;
    bool page = event->modifiers() == Qt::ShiftModifier;

    if (!pixels.isNull())
        change = pixels.y();
    else if (!degrees.isNull())
        change = degrees.y();

    if (change == 0)
        return;

    if (change > 0)
    {
        if (page)
            gli_input_handle_key(keycode_PageUp);
        else
            gli_input_handle_key(keycode_MouseWheelUp);

    }
    else if (change < 0)
    {
        if (page)
            gli_input_handle_key(keycode_PageDown);
        else
            gli_input_handle_key(keycode_MouseWheelDown);
    }

    event->accept();
}

void wininit(int *, char **)
{
    // QApplication takes argc by reference (because it might modify
    // it), and thus requires it to live at least as long as the
    // QApplication instance. However, the caller to this function is
    // passing argc as the address of a function parameter which will no
    // longer exist as soon as that function exits. This isn't really a
    // Qt program in the normal sense, so doesn't need to provide Qt
    // command-line arguments (such as -style). Create static dummy
    // argument data here so it will live the entire life of the
    // program and thus fulfull QApplication's requirements.
    static int argc = 1;
    static char *argv[] = { const_cast<char *>("gargoyle"), nullptr };
    app = new QApplication(argc, argv);
    last_tick.start();
}

void winopen()
{
    int defw;
    int defh;

    defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    defh = gli_wmarginy * 2 + gli_cellh * gli_rows;

    window = new Window();
    window->resize(defw, defh);
    wintitle();

    if (gli_conf_fullscreen)
        window->showFullScreen();
    else
        window->show();
}

void wintitle()
{
    QString title;

    if (std::strlen(gli_story_title) != 0)
        title = gli_story_title;
    else if (std::strlen(gli_story_name) != 0)
        title = QString("%1 - %2").arg(gli_story_name, gli_program_name);
    else
        title = gli_program_name;

    window->setWindowTitle(title);
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    window->update(x0, y0, x1 - x0, y1 - y0);
}

std::string garglk::winfontpath(const std::string &filename)
{
    return QCoreApplication::applicationDirPath().toStdString() + "/" + filename;
}

void gli_tick()
{
    if (last_tick.elapsed() > TICK_PERIOD_MILLIS)
    {
        app->processEvents(QEventLoop::ExcludeUserInputEvents);
        last_tick.start();
    }
}

void gli_select(event_t *event, int polled)
{
    gli_event_clearevent(event);

    app->processEvents();

    gli_dispatch_event(event, polled);

    if (!polled)
    {
        while (event->type == evtype_None && !window->timed_out())
        {
            app->processEvents(QEventLoop::WaitForMoreEvents);
            gli_dispatch_event(event, polled);
        }
    }

    if (event->type == evtype_None && window->timed_out())
    {
        gli_event_store(evtype_Timer, nullptr, 0, 0);
        gli_dispatch_event(event, polled);
        window->reset_timeout();
    }
}
