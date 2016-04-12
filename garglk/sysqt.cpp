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
#include <QClipboard>
#include <QCursor>
#include <QFileDialog>
#include <QGraphicsView>
#include <QMainWindow>
#include <QPainter>
#include <QResizeEvent>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

#include "sysqt.h"
#include "moc_sysqt.h"

extern "C" {
#include "glk.h"
#include "garglk.h"
}

/* buffer for clipboard text */
static QString cliptext;

/* filters for file dialogs */
static const char *filternames[] = {
    "Saved game files (*.sav)",
    "Text files (*.txt)",
    "All files (*)",
};

static QApplication *app;
static Window *window;

static void handle_input(const QString &input)
{
    for (const uint &c : input.toUcs4())
    {
        switch (c)
        {
            case '\r': case '\n': case '\b': case '\t': case 27:
                break;
            default:
                gli_input_handle_key(c);
                break;
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

void winabort(const char *fmt, ...)
{
    std::va_list ap;

    std::fprintf(stderr, "fatal: ");
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fflush(stderr);
    std::abort();
}

void winexit()
{
    std::exit(0);
}

enum class Action { Open, Save };

static void winchoosefile(const QString &prompt, char *buf, int len, int filter, Action action)
{
    QString filename;
    QString dir = "";

    if (buf[0] != 0)
        dir = QString("./") + buf;

    if (action == Action::Open)
        filename = QFileDialog::getOpenFileName(window, prompt, dir, filternames[filter]);
    else
        filename = QFileDialog::getSaveFileName(window, prompt, dir, filternames[filter]);

    snprintf(buf, len, "%s", filename.toStdString().c_str());
}

void winopenfile(const char *prompt, char *buf, int len, int filter)
{
    QString realprompt = QString("Open: %1").arg(prompt);
    winchoosefile(realprompt, buf, len, filter, Action::Open);
}

void winsavefile(const char *prompt, char *buf, int len, int filter)
{
    QString realprompt = QString("Save: %1").arg(prompt);
    winchoosefile(realprompt, buf, len, filter, Action::Save);
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
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, [&]() { m_timed_out = true; });
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

    gli_force_redraw = 1;

    gli_windows_size_change();

    event->accept();
}

void View::paintEvent(QPaintEvent *event)
{
    if (!gli_drawselect)
        gli_windows_redraw();
    else
        gli_drawselect = FALSE;

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

void View::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier)
    {
        switch (event->key())
        {
            case Qt::Key_A: gli_input_handle_key(keycode_Home); break;
            case Qt::Key_B: gli_input_handle_key(keycode_Left); break;
            case Qt::Key_C: winclipsend(QClipboard::Clipboard); break;
            case Qt::Key_D: gli_input_handle_key(keycode_Erase); break;
            case Qt::Key_E: gli_input_handle_key(keycode_End); break;
            case Qt::Key_F: gli_input_handle_key(keycode_Right); break;
            case Qt::Key_N: gli_input_handle_key(keycode_Down); break;
            case Qt::Key_P: gli_input_handle_key(keycode_Up); break;
            case Qt::Key_U: gli_input_handle_key(keycode_Escape); break;
            case Qt::Key_V: winclipreceive(QClipboard::Clipboard); break;
            case Qt::Key_X: winclipsend(QClipboard::Clipboard); break;
            case Qt::Key_Left: gli_input_handle_key(keycode_SkipWordLeft); break;
            case Qt::Key_Right: gli_input_handle_key(keycode_SkipWordRight); break;
        }
    }
    else
    {
        switch (event->key())
        {
            case Qt::Key_Escape: gli_input_handle_key(keycode_Escape); break;
            case Qt::Key_Tab: gli_input_handle_key(keycode_Tab); break;
            case Qt::Key_Backspace: gli_input_handle_key(keycode_Delete); break;
            case Qt::Key_Return: gli_input_handle_key(keycode_Return); break;
            case Qt::Key_Enter: gli_input_handle_key(keycode_Return); break;
            case Qt::Key_Home: gli_input_handle_key(keycode_Home); break;
            case Qt::Key_End: gli_input_handle_key(keycode_End); break;
            case Qt::Key_Left: gli_input_handle_key(keycode_Left); break;
            case Qt::Key_Up: gli_input_handle_key(keycode_Up); break;
            case Qt::Key_Right: gli_input_handle_key(keycode_Right); break;
            case Qt::Key_Down: gli_input_handle_key(keycode_Down); break;
            case Qt::Key_PageUp: gli_input_handle_key(keycode_PageUp); break;
            case Qt::Key_PageDown: gli_input_handle_key(keycode_PageDown); break;
            case Qt::Key_F1: gli_input_handle_key(keycode_Func1); break;
            case Qt::Key_F2: gli_input_handle_key(keycode_Func2); break;
            case Qt::Key_F3: gli_input_handle_key(keycode_Func3); break;
            case Qt::Key_F4: gli_input_handle_key(keycode_Func4); break;
            case Qt::Key_F5: gli_input_handle_key(keycode_Func5); break;
            case Qt::Key_F6: gli_input_handle_key(keycode_Func6); break;
            case Qt::Key_F7: gli_input_handle_key(keycode_Func7); break;
            case Qt::Key_F8: gli_input_handle_key(keycode_Func8); break;
            case Qt::Key_F9: gli_input_handle_key(keycode_Func9); break;
            case Qt::Key_F10: gli_input_handle_key(keycode_Func10); break;
            case Qt::Key_F11: gli_input_handle_key(keycode_Func11); break;
            case Qt::Key_F12: gli_input_handle_key(keycode_Func12); break;

            default: handle_input(event->text()); break;
        }
    }
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
        gli_copyselect = FALSE;
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
    bool page = (event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier;

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

void wininit(int *argc, char **argv)
{
    app = new QApplication(*argc, argv);
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

void gli_select(event_t *event, int polled)
{
    gli_curevent = event;
    gli_event_clearevent(event);

    app->processEvents();

    gli_dispatch_event(gli_curevent, polled);

    if (!polled)
    {
        while (gli_curevent->type == evtype_None && !window->timed_out())
        {
            app->processEvents(QEventLoop::WaitForMoreEvents);
            gli_dispatch_event(gli_curevent, polled);
        }
    }

    if (gli_curevent->type == evtype_None && window->timed_out())
    {
        gli_event_store(evtype_Timer, nullptr, 0, 0);
        gli_dispatch_event(gli_curevent, polled);
        window->reset_timeout();
    }

    gli_curevent = nullptr;
}
