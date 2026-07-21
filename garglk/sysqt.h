#ifndef GARGLK_SYSQT_H
#define GARGLK_SYSQT_H

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

namespace garglk {

class View : public QWidget
{
    Q_OBJECT

public:
    explicit View(QWidget *parent) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setAttribute(Qt::WA_InputMethodEnabled, true);
    }

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void refresh();

protected:
    void inputMethodEvent(QInputMethodEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    bool m_fullscreen_from_maximized = false;
};

class Window : public QMainWindow {
    Q_OBJECT
public:
    Window();

    void refresh() { m_view->refresh(); }

    void start_timer(unsigned long);
    bool timed_out() const { return m_timed_out; }
    void reset_timeout() { m_timed_out = false; }

    const QSettings *settings() { return m_settings; }

protected:
    void showEvent(QShowEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void moveEvent(QMoveEvent *) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    bool event(QEvent *) override;
#endif

private:
    // Resizes gli_image_rgb to the physical-pixel size for the given
    // logical size at the view's *current* effective_dpr(). Shared by
    // resizeEvent() and the delayed call in showEvent() - see the
    // comment there for why a second call is needed.
    void updateBufferSize(const QSize &logicalSize);

    View *const m_view;
    QTimer *const m_timer;
    QSettings *const m_settings;
    bool m_timed_out = false;
};


}
#endif
