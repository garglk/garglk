#ifndef GARGLK_SYSQT_H
#define GARGLK_SYSQT_H

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

class View : public QWidget
{
    Q_OBJECT

public:
    View(QWidget *parent) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setAttribute(Qt::WA_InputMethodEnabled, true);
    }

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

protected:
    void inputMethodEvent(QInputMethodEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
};

class Window : public QMainWindow {
    Q_OBJECT
public:
    Window();
    ~Window();

    void start_timer(long);
    bool timed_out() { return m_timed_out; }
    void reset_timeout() { m_timed_out = false; }

protected:
    void closeEvent(QCloseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    View *m_view;
    QTimer *m_timer;
    bool m_timed_out = false;
};

#endif
