// Copyright (C) 2026 by the Gargoyle developers.
//
// This file is part of Gargoyle.

#include "sessionqt.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "garglk.h"
#include "ipc.h"
#include "menubarqt.h"

namespace garglk {
namespace session_detail {

const std::unordered_map<FileFilter, std::pair<QString, QString>> dialog_filters = {
    {FileFilter::Save, {"Saved game files (*.glksave *.sav)", "glksave"}},
    {FileFilter::Text, {"Text files (*.txt)", "txt"}},
    {FileFilter::Data, {"Data files (*.glkdata)", "glkdata"}},
};

class GameWindow;

class GameView : public QWidget {
    Q_OBJECT

public:
    explicit GameView(GameWindow *window);

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    GameWindow *m_window;
};

class GameWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit GameWindow(QLocalSocket *socket, QWidget *parent = nullptr);

    void send(ipc::Msg type, const QByteArray &payload = {});
    void handle_message(const ipc::Message &msg);
    void feed_bytes(const QByteArray &bytes);

    QImage &frame() { return m_frame; }

public slots:
    void consumeLeftover();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void send_backing_info();
    void apply_frame(int width, int height, const QByteArray &rgb);
    QString run_file_dialog(bool save, const QString &prompt, FileFilter filter, const QString &savedir);
    void on_ready_read();

    QLocalSocket *m_socket;
    GameView *m_view;
    QSettings *m_settings;
    QImage m_frame;
    QByteArray m_readbuf;
    bool m_closing = false;
};

class PendingConnection;

class Session : public QObject {
    Q_OBJECT

public:
    static Session &instance();

    bool is_parent() const { return m_parent; }
    bool try_handoff(const QString &game);
    bool init_parent();
    void set_launcher(LaunchGameFn fn);
    bool open_game(const QString &game);
    int exec();
    void note_window_closed(GameWindow *window);
    void adopt_window(std::unique_ptr<GameWindow> window);

private:
    Session();
    void on_new_connection();

    QLocalServer *m_server = nullptr;
    bool m_parent = false;
    LaunchGameFn m_launch;
    QSettings m_settings;
    std::unordered_map<GameWindow *, std::unique_ptr<GameWindow>> m_windows;
    std::vector<PendingConnection *> m_pending;

    friend class PendingConnection;
    friend class GameWindow;
};

class PendingConnection : public QObject {
    Q_OBJECT

public:
    PendingConnection(QLocalSocket *socket, Session *session);

signals:
    void finished();

private:
    void on_ready_read();

    QLocalSocket *m_socket;
    Session *m_session;
    QByteArray m_buf;
};

GameView::GameView(GameWindow *window) : QWidget(window), m_window(window)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_InputMethodEnabled, true);
}

void GameView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (!m_window->frame().isNull()) {
        QImage image = m_window->frame();
        image.setDevicePixelRatio(devicePixelRatioF());
        painter.drawImage(QPoint(0, 0), image);
    } else {
        painter.fillRect(rect(), QColor(0, 0, 0));
    }
}

void GameView::keyPressEvent(QKeyEvent *event)
{
    m_window->send(ipc::Msg::EventKey,
            ipc::make_event_key(
                static_cast<std::uint32_t>(event->modifiers()),
                static_cast<std::int32_t>(event->key()),
                event->text()));
    event->accept();
}

void GameView::mouseMoveEvent(QMouseEvent *event)
{
    double dpr = devicePixelRatioF();
    int x = std::round(event->pos().x() * dpr);
    int y = std::round(event->pos().y() * dpr);
    m_window->send(ipc::Msg::EventMouse, ipc::make_event_mouse(ipc::MouseKind::Move, x, y, 0));
    event->accept();
}

void GameView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    double dpr = devicePixelRatioF();
    int x = std::round(event->pos().x() * dpr);
    int y = std::round(event->pos().y() * dpr);
    m_window->send(ipc::Msg::EventMouse, ipc::make_event_mouse(ipc::MouseKind::Press, x, y, 0));
    event->accept();
}

void GameView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    double dpr = devicePixelRatioF();
    int x = std::round(event->pos().x() * dpr);
    int y = std::round(event->pos().y() * dpr);
    m_window->send(ipc::Msg::EventMouse, ipc::make_event_mouse(ipc::MouseKind::Release, x, y, 0));
    event->accept();
}

void GameView::wheelEvent(QWheelEvent *event)
{
    m_window->send(ipc::Msg::EventMouse,
            ipc::make_event_mouse(ipc::MouseKind::Wheel, 0, 0, event->angleDelta().y()));
    event->accept();
}

GameWindow::GameWindow(QLocalSocket *socket, QWidget *parent) :
    QMainWindow(parent),
    m_socket(socket),
    m_view(new GameView(this)),
    m_settings(new QSettings("io.github.garglk", "Gargoyle", this))
{
    setCentralWidget(m_view);
    m_socket->setParent(this);
    if (gli_conf_menu_bar) {
        setup_file_menu(this, m_settings,
                [](const QString &game) { Session::instance().open_game(game); });
    } else {
        menuBar()->hide();
    }

    connect(m_socket, &QLocalSocket::readyRead, this, &GameWindow::on_ready_read);
    connect(m_socket, &QLocalSocket::disconnected, this, [this]() {
        if (!m_closing) {
            close();
        }
    });
}

void GameWindow::consumeLeftover()
{
    auto leftover = m_socket->property("garglk_leftover").toByteArray();
    m_socket->setProperty("garglk_leftover", QVariant());
    feed_bytes(leftover);
}

void GameWindow::feed_bytes(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return;
    }
    m_readbuf.append(bytes);
    ipc::Message msg;
    while (ipc::try_decode(m_readbuf, msg)) {
        handle_message(msg);
    }
}

void GameWindow::on_ready_read()
{
    feed_bytes(m_socket->readAll());
}

void GameWindow::send(ipc::Msg type, const QByteArray &payload)
{
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState) {
        m_socket->write(ipc::encode(type, payload));
    }
}

void GameWindow::send_backing_info()
{
    double dpr = m_view->devicePixelRatioF();
    int width = std::round(m_view->width() * dpr);
    int height = std::round(m_view->height() * dpr);
    send(ipc::Msg::BackingInfo, ipc::make_backing_info(width, height, dpr));
    send(ipc::Msg::EventArrange, ipc::make_event_arrange(width, height, dpr));
}

void GameWindow::apply_frame(int width, int height, const QByteArray &rgb)
{
    if (width <= 0 || height <= 0 || rgb.size() < width * height * 3) {
        return;
    }

    m_frame = QImage(width, height, QImage::Format_RGB888);
    std::memcpy(m_frame.bits(), rgb.constData(), static_cast<std::size_t>(width * height * 3));
    // QImage may require bytesPerLine alignment; copy line-by-line if needed.
    if (m_frame.bytesPerLine() != width * 3) {
        for (int y = 0; y < height; y++) {
            std::memcpy(m_frame.scanLine(y), rgb.constData() + y * width * 3,
                    static_cast<std::size_t>(width * 3));
        }
    }
    m_view->update();
}

QString GameWindow::run_file_dialog(bool save, const QString &prompt, FileFilter filter, const QString &savedir)
{
    QFileDialog::Options options;
#ifdef GARGLK_CONFIG_NO_NATIVE_FILE_DIALOGS
    options |= QFileDialog::DontUseNativeDialog;
#endif

    QString dir = savedir;
    auto it = dialog_filters.find(filter);
    QString filterstring = it != dialog_filters.end() ? it->second.first : "All files (*)";

    if (save) {
        if (dir.isEmpty()) {
            dir = ".";
        }
        if (it != dialog_filters.end()) {
            dir += QString("/Untitled.%1").arg(it->second.second);
        }
        return QFileDialog::getSaveFileName(this, prompt, dir, filterstring, nullptr, options);
    }

    QString full = QString("%1;;All files (*)").arg(filterstring);
    return QFileDialog::getOpenFileName(this, prompt, dir, full, nullptr, options);
}

void GameWindow::handle_message(const ipc::Message &msg)
{
    int off = 0;

    switch (msg.type) {
    case ipc::Msg::InitWindow: {
        std::uint32_t do_move = 0, fullscreen = 0, r = 0, g = 0, b = 0, pid = 0;
        std::int32_t x = 0, y = 0, width = 0, height = 0;
        ipc::read_u32(msg.payload, off, do_move);
        ipc::read_i32(msg.payload, off, x);
        ipc::read_i32(msg.payload, off, y);
        ipc::read_i32(msg.payload, off, width);
        ipc::read_i32(msg.payload, off, height);
        ipc::read_u32(msg.payload, off, fullscreen);
        ipc::read_u32(msg.payload, off, r);
        ipc::read_u32(msg.payload, off, g);
        ipc::read_u32(msg.payload, off, b);
        ipc::read_u32(msg.payload, off, pid);
        Q_UNUSED(pid);

        setMinimumSize(40, 40);
        int menu_h = gli_conf_menu_bar ? menuBar()->sizeHint().height() : 0;
        resize(width, height + menu_h);
        if (do_move) {
            QWidget::move(x, y);
        }

        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)));
        setPalette(pal);

        if (fullscreen) {
            showFullScreen();
        } else {
            show();
        }
        send_backing_info();
        break;
    }

    case ipc::Msg::SetTitle: {
        QString title;
        if (ipc::read_string(msg.payload, off, title)) {
            setWindowTitle(title);
        }
        break;
    }

    case ipc::Msg::SetContents: {
        std::int32_t width = 0, height = 0;
        ipc::read_i32(msg.payload, off, width);
        ipc::read_i32(msg.payload, off, height);
        apply_frame(width, height, msg.payload.mid(off));
        break;
    }

    case ipc::Msg::CloseWindow:
        close();
        break;

    case ipc::Msg::OpenDialog:
    case ipc::Msg::SaveDialog: {
        QString prompt, savedir;
        std::uint32_t filter_u = 0;
        ipc::read_string(msg.payload, off, prompt);
        ipc::read_u32(msg.payload, off, filter_u);
        ipc::read_string(msg.payload, off, savedir);
        auto path = run_file_dialog(msg.type == ipc::Msg::SaveDialog, prompt,
                static_cast<FileFilter>(filter_u), savedir);
        send(ipc::Msg::DialogResult, ipc::make_dialog_result(path));
        break;
    }

    case ipc::Msg::AbortDialog: {
        QString prompt;
        ipc::read_string(msg.payload, off, prompt);
        QMessageBox::critical(this, "Error", prompt);
        break;
    }

    case ipc::Msg::SetCursor: {
        std::uint32_t cursor = 0;
        ipc::read_u32(msg.payload, off, cursor);
        if (cursor == 1) {
            m_view->setCursor(Qt::IBeamCursor);
        } else if (cursor == 2) {
            m_view->setCursor(Qt::PointingHandCursor);
        } else {
            m_view->unsetCursor();
        }
        break;
    }

    case ipc::Msg::ToggleFullScreen:
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
        send(ipc::Msg::FullScreenResult, ipc::make_fullscreen_result(isFullScreen()));
        break;

    case ipc::Msg::QueryFullScreen:
        send(ipc::Msg::FullScreenResult, ipc::make_fullscreen_result(isFullScreen()));
        break;

    case ipc::Msg::RequestBacking:
        send_backing_info();
        break;

    default:
        break;
    }
}

void GameWindow::closeEvent(QCloseEvent *event)
{
    m_closing = true;
    send(ipc::Msg::EventQuit);
    if (m_socket) {
        m_socket->disconnectFromServer();
    }
    Session::instance().note_window_closed(this);
    event->accept();
    // Owned by Session's unique_ptr map; release and deleteLater to avoid
    // destroying this object while closeEvent is still on the stack.
}

void GameWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    send_backing_info();
}

void GameWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, [this]() { send_backing_info(); });
}

PendingConnection::PendingConnection(QLocalSocket *socket, Session *session) :
    QObject(session),
    m_socket(socket),
    m_session(session)
{
    m_socket->setParent(this);
    connect(m_socket, &QLocalSocket::readyRead, this, &PendingConnection::on_ready_read);
    connect(m_socket, &QLocalSocket::disconnected, this, &PendingConnection::finished);
}

void PendingConnection::on_ready_read()
{
    m_buf.append(m_socket->readAll());
    ipc::Message msg;
    while (ipc::try_decode(m_buf, msg)) {
        if (msg.type == ipc::Msg::OpenGame) {
            int off = 0;
            QString path;
            if (ipc::read_string(msg.payload, off, path)) {
                m_socket->write(ipc::encode(ipc::Msg::HandoffAck));
                m_socket->flush();
                m_session->open_game(path);
            }
            emit finished();
            return;
        }

        if (msg.type == ipc::Msg::InitWindow) {
            m_socket->setParent(nullptr);
            disconnect(m_socket, nullptr, this, nullptr);

            auto window = std::make_unique<GameWindow>(m_socket);
            auto *raw = window.get();
            if (!m_buf.isEmpty()) {
                m_socket->setProperty("garglk_leftover", m_buf);
                m_buf.clear();
            }
            m_session->adopt_window(std::move(window));
            raw->handle_message(msg);
            QMetaObject::invokeMethod(raw, "consumeLeftover", Qt::QueuedConnection);
            emit finished();
            return;
        }
    }
}

Session &Session::instance()
{
    static Session session;
    return session;
}

Session::Session() : m_settings("io.github.garglk", "Gargoyle")
{
}

bool Session::try_handoff(const QString &game)
{
    if (!gli_conf_ipc || game.isEmpty()) {
        return false;
    }

    QLocalSocket sock;
    sock.connectToServer(QString::fromStdString(ipc::server_name_from_config()));
    if (!sock.waitForConnected(500)) {
        return false;
    }

    sock.write(ipc::encode(ipc::Msg::OpenGame, ipc::make_open_game(game)));
    sock.flush();

    if (!sock.waitForReadyRead(2000)) {
        return false;
    }

    QByteArray buf = sock.readAll();
    ipc::Message reply;
    return ipc::try_decode(buf, reply) && reply.type == ipc::Msg::HandoffAck;
}

bool Session::init_parent()
{
    if (!gli_conf_ipc) {
        return false;
    }

    m_server = new QLocalServer(this);
    auto name = QString::fromStdString(ipc::server_name_from_config());
    QLocalServer::removeServer(name);
    if (!m_server->listen(name)) {
        QMessageBox::critical(nullptr, "Error",
                QString("Unable to start Gargoyle IPC server:\n%1").arg(m_server->errorString()));
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, &Session::on_new_connection);
    m_parent = true;
    return true;
}

void Session::set_launcher(LaunchGameFn fn)
{
    m_launch = std::move(fn);
}

bool Session::open_game(const QString &game)
{
    if (game.isEmpty() || !m_launch) {
        return false;
    }

    note_recent_file(&m_settings, game);
    return m_launch(game.toStdString());
}

int Session::exec()
{
    if (m_windows.empty()) {
        QTimer::singleShot(30000, this, [this]() {
            if (m_windows.empty()) {
                QApplication::quit();
            }
        });
    }

    return QApplication::exec();
}

void Session::note_window_closed(GameWindow *window)
{
    auto it = m_windows.find(window);
    if (it != m_windows.end()) {
        it->second.release();
        m_windows.erase(it);
        window->deleteLater();
    }
    if (m_windows.empty()) {
        QApplication::quit();
    }
}

void Session::adopt_window(std::unique_ptr<GameWindow> window)
{
    auto *raw = window.get();
    m_windows.emplace(raw, std::move(window));
}

void Session::on_new_connection()
{
    while (m_server->hasPendingConnections()) {
        auto *socket = m_server->nextPendingConnection();
        auto *pending = new PendingConnection(socket, this);
        m_pending.push_back(pending);
        connect(pending, &PendingConnection::finished, this, [this, pending]() {
            m_pending.erase(std::remove(m_pending.begin(), m_pending.end(), pending), m_pending.end());
            pending->deleteLater();
        });
    }
}

} // namespace session_detail

bool session_is_parent()
{
    return session_detail::Session::instance().is_parent();
}

bool session_try_handoff(const QString &game)
{
    return session_detail::Session::instance().try_handoff(game);
}

bool session_init_parent()
{
    return session_detail::Session::instance().init_parent();
}

void session_set_launcher(LaunchGameFn fn)
{
    session_detail::Session::instance().set_launcher(std::move(fn));
}

bool session_open_game(const QString &game)
{
    return session_detail::Session::instance().open_game(game);
}

int session_exec()
{
    return session_detail::Session::instance().exec();
}

}

#include "sessionqt.moc"
