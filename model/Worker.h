#ifndef WORKER_H
#define WORKER_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#include <QObject>
#include <QDebug>
#include <QString>

#include "common.h"
#include "common-sdl.h"
#include "whisper.h"

#include "WhisperParams.h"

class Worker : public QObject
{
    Q_OBJECT

public:
    Worker(int mic_dev, const std::string &modelPath);
    int setup_capture(int mic_dev, const std::string &modelPath);

    bool isReady() const { return m_ready; }
    QString errorMessage() const { return m_error; }

public slots:
    void doWork();
    void stopWork() { m_stop = true; }

signals:
    void conditionMessage(const QString &message);
    void finished();

private:
    std::atomic<bool> m_stop;
    bool m_ready = false;
    QString m_error;

    whisper_params params;
    wav_writer wavWriter;

    int n_samples_step;
    int n_samples_len;
    int n_samples_keep;
    int n_samples_30s;
    bool use_vad;
    int n_new_line;

    std::vector<float> pcmf32;
    std::vector<float> pcmf32_old;
    std::vector<float> pcmf32_new;

    bool is_running;
    audio_async* audio;
    std::chrono::time_point<std::chrono::system_clock> t_last, t_start;

    std::vector<whisper_token> prompt_tokens;
    int n_iter;
    std::ofstream fout;

    struct whisper_context *ctx;
};

#endif // WORKER_H
