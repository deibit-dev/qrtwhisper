#include "TranscriptionWorker.h"

#include "audio/IAudioStream.h"
#include "engine/ITranscriptionEngine.h"

#include <QDebug>
#include <vector>

TranscriptionWorker::TranscriptionWorker(std::unique_ptr<IAudioStream> audio,
                                         std::unique_ptr<ITranscriptionEngine> engine)
    : QObject(nullptr), m_audio(std::move(audio)), m_engine(std::move(engine)) {
}

void TranscriptionWorker::run() {
    while (!m_stop) {
        std::vector<float> samples;
        if (!m_audio->nextWindow(samples)) {
            break;
        }

        QVector<QString> segments;
        Error error;
        if (!m_engine->transcribe(samples, segments, &error)) {
            qWarning() << error.detail;
            break;
        }

        for (const QString &text : segments) {
            emit segmentTranscribed(text);
        }
    }

    m_audio->stop();
    m_engine->shutdown();

    emit finished();
}

void TranscriptionWorker::stop() {
    m_stop = true;
}
