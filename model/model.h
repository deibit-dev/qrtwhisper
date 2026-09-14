#ifndef MODEL_H
#define MODEL_H

#include <list>
#include <string>

#include <QObject>
#include <QString>
#include <QThread>

#include "Error.h"
#include "WhisperParams.h"

class TranscriptionWorker;

class Model : public QObject {
    Q_OBJECT

public:
    Model();
    bool startTranscription(int mic_dev, const std::string &modelPath);
    QString lastTranscription() const { return m_lastTranscription; }
    Error lastError() const { return m_lastError; }
    std::list<std::pair<int, std::string>> micDevices();
    int findCaptureDevice(const std::string &token);
    void stopTranscription();

signals:
    void transcriptionReady();

private slots:
    void onSegmentTranscribed(const QString &text);

private:
    whisper_params m_params;
    TranscriptionWorker* m_worker = nullptr;
    QThread m_workerThread;
    QString m_lastTranscription;
    Error m_lastError;
    std::list<std::pair<int, std::string>> m_micDevices;
};

#endif // MODEL_H
