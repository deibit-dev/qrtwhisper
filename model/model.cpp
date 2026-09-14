#include "model.h"

#include "TranscriptionWorker.h"
#include "audio/SdlAudioStream.h"
#include "engine/WhisperEngine.h"

#include "common-sdl.h"

#include <iostream>
#include <memory>

Model::Model() : QObject(nullptr) {
}

bool Model::startTranscription(int mic_dev, const std::string &modelPath) {
    auto stream = std::make_unique<SdlAudioStream>(m_params);
    Error error;
    if (!stream->start(mic_dev, &error)) {
        m_lastError = error;
        return false;
    }

    auto engine = std::make_unique<WhisperEngine>(m_params);
    if (!engine->load(QString::fromStdString(modelPath), &error)) {
        m_lastError = error;
        return false;
    }

    m_worker = new TranscriptionWorker(std::move(stream), std::move(engine));

    m_worker->moveToThread(&m_workerThread);
    connect(m_worker, &TranscriptionWorker::segmentTranscribed, this, &Model::onSegmentTranscribed);
    connect(&m_workerThread, &QThread::started, m_worker, &TranscriptionWorker::run);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &TranscriptionWorker::finished, &m_workerThread, &QThread::quit);

    m_workerThread.start();
    return true;
}

void Model::onSegmentTranscribed(const QString &text) {
    std::cout << text.toStdString() << std::endl;
    m_lastTranscription = text;
    emit transcriptionReady();
}

std::list<std::pair<int, std::string>> Model::micDevices() {
    m_micDevices.clear();
    SDL_Init(SDL_INIT_AUDIO);
    int num_devices = SDL_GetNumAudioDevices(SDL_TRUE);
    for (int i = 0; i < num_devices; i++) {
        m_micDevices.push_back(std::make_pair(i, SDL_GetAudioDeviceName(i, SDL_TRUE)));
    }
    SDL_Quit();
    return m_micDevices;
}

int Model::findCaptureDevice(const std::string &token) {
    for (auto &dev : micDevices()) {
        if (dev.second.find(token) != std::string::npos) {
            return dev.first;
        }
    }
    return -1;
}

void Model::stopTranscription() {
    if (m_worker == nullptr) {
        return;
    }
    m_worker->stop();
    m_workerThread.quit();
    m_workerThread.wait();
}
