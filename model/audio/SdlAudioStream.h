#ifndef SDLAUDIOSTREAM_H
#define SDLAUDIOSTREAM_H

#include "IAudioStream.h"
#include "WhisperParams.h"

#include <chrono>
#include <vector>

#include "common-sdl.h"
#include "common.h"

class SdlAudioStream : public IAudioStream {
public:
    explicit SdlAudioStream(const whisper_params &params);

    bool start(int captureId, Error *errorOut = nullptr) override;
    bool nextWindow(std::vector<float> &samples) override;
    void stop() override;

private:
    whisper_params m_params;

    audio_async *m_audio = nullptr;
    wav_writer m_wavWriter;

    int m_n_samples_step = 0;
    int m_n_samples_len = 0;
    int m_n_samples_keep = 0;
    int m_n_samples_30s = 0;
    bool m_use_vad = false;
    int m_n_new_line = 1;
    int m_n_iter = 0;

    bool m_is_running = false;

    std::vector<float> m_pcm;
    std::vector<float> m_pcmOld;
    std::vector<float> m_pcmNew;

    std::chrono::time_point<std::chrono::high_resolution_clock> m_tLast;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_tStart;
};

#endif // SDLAUDIOSTREAM_H
