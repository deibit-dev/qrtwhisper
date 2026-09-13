#ifndef VIEW_H
#define VIEW_H

#include <QObject>

class MainWidget;
class TextRender;
class Tray;

class View : public QObject {

public:
    View();

    MainWidget* getMainWidget()  { return mainWidget; }
    TextRender* getTextRender()  { return textRender; }
    Tray*       getTray()        { return tray; }

private:
    MainWidget* mainWidget;
    TextRender* textRender;
    Tray*       tray;
};

#endif // VIEW_H
