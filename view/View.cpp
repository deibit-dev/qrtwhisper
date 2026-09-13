#include "View.h"

#include "MainWidget.h"
#include "TextRender.h"
#include "Tray.h"

View::View() : QObject() {
    textRender = new TextRender();
    mainWidget = new MainWidget();
    tray       = new Tray();
}
