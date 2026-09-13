#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QSettings>

#include "model.h"
#include "View.h"
#include "controller.h"
#include "VirtualMic.h"
#include "VirtualMicFactory.h"
#include "ModelManager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("dei-eng"));
    app.setApplicationName(QStringLiteral("QRTWhisper"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    Model m_model;
    View view;
    auto virtualMic = makeVirtualMic();
    ModelManager modelManager;
    Controller controller(&m_model, &view, virtualMic.get(), &modelManager);

    QString language = QSettings().value(QStringLiteral("ui/language")).toString();
    if (language.isEmpty()) {
        language = QLocale::system().language() == QLocale::Spanish
                ? QStringLiteral("es") : QStringLiteral("en");
    }
    controller.setLanguage(language);

    controller.start_main();

    return app.exec();
}
