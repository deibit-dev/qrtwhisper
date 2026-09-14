#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QVector>

#include "MainWidget.h"
#include "ModelManager.h"

class Model;
class View;
class VirtualMic;
class ModelManagerDialog;
class QTranslator;
class QAction;
struct Error;

class Controller : public QObject {
    Q_OBJECT

public:
    Controller(Model* model, View* view, VirtualMic* virtualMic, ModelManager* modelManager, QObject* parent = nullptr);
    void start_main();
    void start_transcription();
    void quit();
    void setLanguage(const QString &code);

private slots:
    void onTranscriptionReady();
    void openModelManager();
    void onModelDownloadRequested(const QString &id);
    void onModelDeleteRequested(const QString &id);
    void onModelDownloadProgress(const QString &id, qint64 received, qint64 total);
    void onModelDownloadFinished(const QString &id, bool success, const QString &error);
    void onModelDownloadCanceled(const QString &id);
    void onMetadataReady();
    void onMetadataFailed(const QString &error);
    void onModelDialogFinished(int result);
    void onChangeModelsDirRequested();
    void onResetModelsDirRequested();

private:
    void refreshModelCombo();
    QVector<ModelItem> buildModelItems() const;
    void refreshAfterModelsDirChange();
    QString errorMessage(const Error &error) const;

    Model* m_model;
    View*  m_view;
    VirtualMic* m_virtualMic;
    ModelManager* m_modelManager;
    ModelManagerDialog* m_modelDialog = nullptr;
    DisplayMethod m_displayMethod = DisplayMethod::SystemNotification;
    bool m_virtualMicActive = false;
    QAction* m_quitAction = nullptr;
    QTranslator* m_translator = nullptr;
};

#endif // CONTROLLER_H
