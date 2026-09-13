#ifndef TEXTRENDER_H
#define TEXTRENDER_H

#include "TransparentWindow.h"

#include <QLabel>

class TextRender : public TransparentWindow {
    Q_OBJECT

public:
    TextRender(QWidget* parent = nullptr);

    void updateLabel(const QString&);
    void bringToFront();

private:
    QLabel* label;
};

#endif // TEXTRENDER_H
