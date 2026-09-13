#ifndef TRAY_H
#define TRAY_H

#include <functional>
#include <QSystemTrayIcon>

class QMenu;
class QAction;

class Tray : public QSystemTrayIcon {
public:
    Tray();

    void showMessage(const QString &title, const QString &message,
                     QSystemTrayIcon::MessageIcon icon = Information,
                     int timeoutMs = 5000) {
        QSystemTrayIcon::showMessage(title, message, icon, timeoutMs);
        m_notificationVisible = true;
    }

    QAction* addMenuAction(const QString &text, std::function<void()> callback);

private:
    bool m_notificationVisible = false;
    QMenu* menu;
};

#endif // TRAY_H
