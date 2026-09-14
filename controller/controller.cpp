#include "controller.h"

#include "MainWidget.h"
#include "model.h"
#include "Error.h"
#include "ModelManagerDialog.h"
#include "TextRender.h"
#include "Tray.h"
#include "View.h"
#include "VirtualMic.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QTranslator>

Controller::Controller(Model* model, View* view, VirtualMic* virtualMic, ModelManager* modelManager, QObject* parent)
    : QObject(parent), m_model(model), m_view(view), m_virtualMic(virtualMic), m_modelManager(modelManager)
{
    connect(m_model, &Model::transcriptionReady, this, &Controller::onTranscriptionReady);

    auto main_widget = m_view->getMainWidget();
    connect(main_widget, &MainWidget::startClicked, this, &Controller::start_transcription);
    connect(main_widget, &MainWidget::manageModelsClicked, this, &Controller::openModelManager);
    connect(main_widget, &MainWidget::languageChangeRequested, this, &Controller::setLanguage);

    connect(m_modelManager, &ModelManager::metadataReady, this, &Controller::onMetadataReady);
    connect(m_modelManager, &ModelManager::metadataFailed, this, &Controller::onMetadataFailed);
    connect(m_modelManager, &ModelManager::downloadProgress, this, &Controller::onModelDownloadProgress);
    connect(m_modelManager, &ModelManager::downloadFinished, this, &Controller::onModelDownloadFinished);
    connect(m_modelManager, &ModelManager::downloadCanceled, this, &Controller::onModelDownloadCanceled);

    m_quitAction = m_view->getTray()->addMenuAction(tr("Quit"), [this]() {
        quit();
    });
}

void Controller::setLanguage(const QString &code) {
    const QString effective = code.isEmpty() ? QStringLiteral("en") : code;
    QSettings settings;
    settings.setValue(QStringLiteral("ui/language"), effective);

    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    if (effective != QLatin1String("en")) {
        auto *translator = new QTranslator(this);
        const QString path = QStringLiteral(":/translations/qrtwhisper_") + effective + QStringLiteral(".qm");
        if (translator->load(path)) {
            m_translator = translator;
            QCoreApplication::installTranslator(m_translator);
        } else {
            delete translator;
        }
    }

    if (m_quitAction) {
        m_quitAction->setText(tr("Quit"));
    }

    m_view->getMainWidget()->setLanguageCode(effective);
}

void Controller::start_main() {
    auto main_widget = m_view->getMainWidget();

    QString error;
    if (!m_modelManager->ensureModelsDir(&error)) {
        QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"), error);
    }
    m_modelManager->refreshLocal();
    refreshModelCombo();

    for (const auto &sink : m_virtualMic->outputSinks()) {
        main_widget->add_output_dev(sink);
    }
    main_widget->select_output(m_virtualMic->defaultOutputSink());

    for (const auto &dev : m_model->micDevices()) {
        main_widget->add_mic_dev(QString::fromStdString(dev.second));
    }

    main_widget->show();
}

void Controller::refreshModelCombo() {
    auto main_widget = m_view->getMainWidget();
    const QStringList downloaded = m_modelManager->downloadedIds();

    QString selected;
    if (downloaded.contains(QStringLiteral("medium.en"))) {
        selected = QStringLiteral("medium.en");
    } else if (!downloaded.isEmpty()) {
        selected = downloaded.first();
    }

    main_widget->setDownloadedModels(downloaded, selected);
    main_widget->setStartEnabled(!downloaded.isEmpty());
}

QVector<ModelItem> Controller::buildModelItems() const {
    QVector<ModelItem> items;
    for (const QString &id : m_modelManager->catalogIds()) {
        ModelItem item;
        item.id = id;
        item.fileName = m_modelManager->fileName(id);
        item.sizeBytes = m_modelManager->sizeOf(id);
        item.downloaded = m_modelManager->isDownloaded(id);
        items.append(item);
    }
    return items;
}

void Controller::openModelManager() {
    auto main_widget = m_view->getMainWidget();
    if (m_modelDialog) {
        m_modelDialog->raise();
        m_modelDialog->activateWindow();
        return;
    }

    m_modelDialog = new ModelManagerDialog(main_widget);
    m_modelDialog->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_modelDialog, &ModelManagerDialog::downloadRequested, this, &Controller::onModelDownloadRequested);
    connect(m_modelDialog, &ModelManagerDialog::deleteRequested, this, &Controller::onModelDeleteRequested);
    connect(m_modelDialog, &ModelManagerDialog::cancelRequested, this, [this]() {
        m_modelManager->cancelDownload();
    });
    connect(m_modelDialog, &QDialog::finished, this, &Controller::onModelDialogFinished);
    connect(m_modelDialog, &ModelManagerDialog::changeModelsDirRequested, this, &Controller::onChangeModelsDirRequested);
    connect(m_modelDialog, &ModelManagerDialog::resetModelsDirRequested, this, &Controller::onResetModelsDirRequested);

    m_modelDialog->setDownloadAllowed(m_modelManager->isMetadataLoaded());
    m_modelDialog->setModelsDir(m_modelManager->modelsDir());
    m_modelDialog->setItems(buildModelItems());

    if (!m_modelManager->isMetadataLoaded()) {
        m_modelDialog->setStatus(tr("Fetching model information…"));
        m_modelManager->fetchMetadata();
    }

    m_modelDialog->show();
}

void Controller::onModelDialogFinished(int) {
    if (m_modelDialog) {
        if (m_modelManager->isDownloading()) {
            m_modelManager->cancelDownload();
        }
        m_modelDialog->deleteLater();
        m_modelDialog = nullptr;
    }
    refreshModelCombo();
}

void Controller::onChangeModelsDirRequested() {
    auto main_widget = m_view->getMainWidget();
    const QString dir = QFileDialog::getExistingDirectory(
            main_widget, tr("Select models directory"), m_modelManager->modelsDir());
    if (dir.isEmpty()) {
        return;
    }

    QString error;
    if (!m_modelManager->setModelsDir(dir, &error)) {
        QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"), error);
        return;
    }
    refreshAfterModelsDirChange();
}

void Controller::onResetModelsDirRequested() {
    m_modelManager->resetModelsDir();
    refreshAfterModelsDirChange();
}

void Controller::refreshAfterModelsDirChange() {
    if (m_modelDialog) {
        m_modelDialog->setModelsDir(m_modelManager->modelsDir());
        m_modelDialog->setItems(buildModelItems());
    }
    refreshModelCombo();
}

void Controller::onModelDownloadRequested(const QString &id) {
    QString error;
    if (!m_modelManager->downloadModel(id, &error)) {
        if (m_modelDialog) {
            m_modelDialog->setStatus(error);
        }
        return;
    }
    if (m_modelDialog) {
        m_modelDialog->setDownloading(true);
        m_modelDialog->setStatus(tr("Downloading %1…").arg(id));
    }
}

void Controller::onModelDeleteRequested(const QString &id) {
    auto main_widget = m_view->getMainWidget();
    const auto answer = QMessageBox::question(main_widget, QStringLiteral("QRTWhisper"),
                                              tr("Delete model %1?").arg(id),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_modelManager->deleteModel(id, &error)) {
        if (m_modelDialog) {
            m_modelDialog->setStatus(tr("Error: ") + error);
        }
        return;
    }
    refreshModelCombo();
    if (m_modelDialog) {
        m_modelDialog->setItems(buildModelItems());
    }
}

void Controller::onModelDownloadProgress(const QString &, qint64 received, qint64 total) {
    if (m_modelDialog) {
        m_modelDialog->setProgress(received, total);
    }
}

void Controller::onModelDownloadFinished(const QString &, bool success, const QString &error) {
    if (success) {
        refreshModelCombo();
    }
    if (!m_modelDialog) {
        return;
    }
    m_modelDialog->setDownloading(false);
    m_modelDialog->setStatus(success ? tr("Model downloaded successfully.")
                                     : tr("Error: ") + error);
    m_modelDialog->setItems(buildModelItems());
}

void Controller::onModelDownloadCanceled(const QString &) {
    if (!m_modelDialog) {
        return;
    }
    m_modelDialog->setDownloading(false);
    m_modelDialog->setStatus(tr("Download cancelled."));
    m_modelDialog->setItems(buildModelItems());
}

void Controller::onMetadataReady() {
    if (!m_modelDialog) {
        return;
    }
    m_modelDialog->setDownloadAllowed(true);
    m_modelDialog->setItems(buildModelItems());
    m_modelDialog->setStatus(QString());
}

void Controller::onMetadataFailed(const QString &error) {
    if (!m_modelDialog) {
        return;
    }
    m_modelDialog->setDownloadAllowed(false);
    m_modelDialog->setStatus(tr("Failed to fetch model information: ") + error);
}

QString Controller::errorMessage(const Error &error) const {
    switch (error.code) {
    case ErrorCode::AudioCaptureFailed:
        return tr("Failed to initialize audio capture (SDL).");
    case ErrorCode::ModelLoadFailed:
        return tr("Failed to load model: %1").arg(error.detail);
    case ErrorCode::UnknownLanguage:
        return tr("Unknown language: %1").arg(error.detail);
    case ErrorCode::TranscriptionFailed:
        return tr("Transcription failed.");
    case ErrorCode::VirtualMicFailed:
        return tr("Failed to create the virtual microphone:\n") + error.detail;
    case ErrorCode::None:
        break;
    }
    return {};
}

void Controller::start_transcription() {
    auto main_widget = m_view->getMainWidget();
    m_displayMethod = main_widget->displayMethod();

    const QString modelId = main_widget->selectedModelId();
    if (modelId.isEmpty()) {
        main_widget->setStartEnabled(true);
        QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"),
                             tr("No downloaded model. Manage models first."));
        return;
    }

    const QString modelPath = m_modelManager->modelPath(modelId);
    if (!QFileInfo::exists(modelPath)) {
        main_widget->setStartEnabled(true);
        QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"),
                             tr("The model does not exist:\n") + modelPath);
        return;
    }

    int capture_id = -1;

    if (main_widget->sourceMode() == SourceMode::VirtualOutput) {
        Error error = m_virtualMic->create(main_widget->selectedOutputName());
        if (!error.ok()) {
            main_widget->setStartEnabled(true);
            QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"), errorMessage(error));
            return;
        }
        m_virtualMicActive = true;

        const std::string token = m_virtualMic->sourceToken().toStdString();
        for (int attempt = 0; attempt < 12 && capture_id < 0; ++attempt) {
            capture_id = m_model->findCaptureDevice(token);
            if (capture_id < 0) {
                QThread::msleep(250);
            }
        }

        if (capture_id < 0) {
            m_virtualMic->destroy();
            m_virtualMicActive = false;
            main_widget->setStartEnabled(true);
            QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"),
                                 tr("The virtual microphone was not found among the capture devices."));
            return;
        }
    } else {
        capture_id = main_widget->selectedMicId();
    }

    if (!m_model->startTranscription(capture_id, modelPath.toStdString())) {
        if (m_virtualMicActive) {
            m_virtualMic->destroy();
            m_virtualMicActive = false;
        }
        main_widget->setStartEnabled(true);
        QMessageBox::warning(main_widget, QStringLiteral("QRTWhisper"), errorMessage(m_model->lastError()));
        return;
    }

    main_widget->hide();
    m_view->getTray()->show();

    // Whisper startup finished: notify readiness (always via notification, never subtitle).
    m_view->getTray()->showMessage(QStringLiteral("QRTWhisper"), tr("Ready"));

    if (m_displayMethod == DisplayMethod::Subtitles) {
        m_view->getTextRender()->show();
    }
}

void Controller::quit() {
    m_modelManager->cancelDownload();
    m_model->stopTranscription();
    if (m_virtualMicActive) {
        m_virtualMic->destroy();
        m_virtualMicActive = false;
    }
    QApplication::quit();
}

void Controller::onTranscriptionReady() {
    const QString str = m_model->lastTranscription();
    if (m_displayMethod == DisplayMethod::Subtitles) {
        m_view->getTextRender()->updateLabel(str);
    } else {
        m_view->getTray()->showMessage(QStringLiteral("QRTWhisper"), str);
    }
}
