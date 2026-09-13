#include "Tray.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

Tray::Tray() {
    setIcon(QIcon(QStringLiteral(":/icons/app.png")));

    menu = new QMenu();
    setContextMenu(menu);
}

QAction* Tray::addMenuAction(const QString &text, std::function<void()> callback) {
    return menu->addAction(text, callback);
}
