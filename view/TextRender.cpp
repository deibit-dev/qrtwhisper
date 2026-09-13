#include "TextRender.h"

TextRender::TextRender(QWidget* parent) : TransparentWindow(parent) {
    label = new QLabel(this);
    label->setStyleSheet(
    "QLabel {"
    "color: white;"
    "font-size: 24px;"
    "background-color: rgba(0, 0, 0, 150);"
    "padding: 10px;"
    "border-radius: 5px;"
    "}"
    );
    label->setAlignment(Qt::AlignCenter);
    label->resize(300, 100);
    label->move(100, 700);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);



}

void TextRender::bringToFront() {
    if (isMinimized()) {
        showNormal();
    }
    raise();
    activateWindow();

    if (QWindow* window = windowHandle()) {
        window->requestActivate();
    }

    hide();
    show();

}

void TextRender::updateLabel(const QString& str) {
    label->setText(str);
    label->adjustSize();
    bringToFront();
}
