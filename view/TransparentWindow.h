#ifndef TRANSPARENTWINDOW_H
#define TRANSPARENTWINDOW_H

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QScreen>
#include <QWindow>

class TransparentWindow : public QWidget {
public:
    TransparentWindow(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Window);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);

        setGeometry(QApplication::primaryScreen()->geometry());
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);

        if (windowHandle()) {
            windowHandle()->setFlags(windowHandle()->flags() | Qt::WindowTransparentForInput);
        }
    }
};

#endif // TRANSPARENTWINDOW_H
