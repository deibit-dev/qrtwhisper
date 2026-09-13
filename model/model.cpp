#include "model.h"
#include "Worker.h"

#include "common-sdl.h"

#include <iostream>

Model::Model() : QObject(nullptr) {
}

bool Model::start(int mic_dev, const std::string &modelPath) {
    worker = new Worker(mic_dev, modelPath);
    if (!worker->isReady()) {
        m_lastError = worker->errorMessage().isEmpty()
                ? tr("Failed to initialize transcription.")
                : worker->errorMessage();
        delete worker;
        worker = nullptr;
        return false;
    }

    worker->moveToThread(&workerThread);
    connect(worker, &Worker::conditionMessage, this, &Model::handleMessage);
    connect(&workerThread, &QThread::started, worker, &Worker::doWork);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &Worker::finished, &workerThread, &QThread::quit);

    workerThread.start();
    return true;
}

std::list<std::pair<int, std::string>> Model::get_mic_devices() {
    mic_devices.clear();
    SDL_Init(SDL_INIT_AUDIO);
    int num_devices = SDL_GetNumAudioDevices(SDL_TRUE);
    for (int i = 0; i < num_devices; i++) {
        mic_devices.push_back(std::make_pair(i, SDL_GetAudioDeviceName(i, SDL_TRUE)));
    }
    SDL_Quit();
    return mic_devices;
}

int Model::find_capture_device(const std::string &token) {
    for (auto &dev : get_mic_devices()) {
        if (dev.second.find(token) != std::string::npos) {
            return dev.first;
        }
    }
    return -1;
}

void Model::stop_transcription() {
    if (worker == nullptr) {
        return;
    }
    worker->stopWork();
    workerThread.quit();
    workerThread.wait();
}