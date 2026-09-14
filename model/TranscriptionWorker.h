#ifndef TRANSCRIPTIONWORKER_H
#define TRANSCRIPTIONWORKER_H

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

#include "audio/IAudioStream.h"
#include "engine/ITranscriptionEngine.h"

class TranscriptionWorker : public QObject {
    Q_OBJECT

public:
    TranscriptionWorker(std::unique_ptr<IAudioStream> audio,
                        std::unique_ptr<ITranscriptionEngine> engine);

public slots:
    void run();
    void stop();

signals:
    void segmentTranscribed(const QString &text);
    void finished();

private:
    std::atomic<bool> m_stop{false};
    std::unique_ptr<IAudioStream> m_audio;
    std::unique_ptr<ITranscriptionEngine> m_engine;
};

#endif // TRANSCRIPTIONWORKER_H
