#include "SdlAudioStream.h"

#include "whisper.h"

#include <algorithm>
#include <cstring>
#include <thread>

SdlAudioStream::SdlAudioStream(const whisper_params &params) : m_params(params) {
}

bool SdlAudioStream::start(int captureId, Error *errorOut) {
    m_n_samples_step = static_cast<int>(1e-3 * m_params.step_ms * WHISPER_SAMPLE_RATE);
    m_n_samples_len  = static_cast<int>(1e-3 * m_params.length_ms * WHISPER_SAMPLE_RATE);
    m_n_samples_keep = static_cast<int>(1e-3 * m_params.keep_ms * WHISPER_SAMPLE_RATE);
    m_n_samples_30s  = static_cast<int>(1e-3 * 30000.0 * WHISPER_SAMPLE_RATE);

    m_use_vad = m_n_samples_step <= 0;
    m_n_new_line = !m_use_vad ? std::max(1, m_params.length_ms / m_params.step_ms - 1) : 1;

    m_audio = new audio_async(m_params.length_ms);
    if (!m_audio->init(captureId, WHISPER_SAMPLE_RATE)) {
        if (errorOut) {
            *errorOut = Error{ErrorCode::AudioCaptureFailed, {}};
        }
        return false;
    }
    m_audio->resume();

    m_pcm    = std::vector<float>(m_n_samples_30s, 0.0f);
    m_pcmNew = std::vector<float>(m_n_samples_30s, 0.0f);

    if (m_params.save_audio) {
        time_t now = time(nullptr);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", localtime(&now));
        m_wavWriter.open(std::string(buffer) + ".wav", WHISPER_SAMPLE_RATE, 16, 1);
    }

    m_n_iter = 0;
    m_is_running = true;
    m_tLast  = std::chrono::high_resolution_clock::now();
    m_tStart = m_tLast;

    return true;
}

bool SdlAudioStream::nextWindow(std::vector<float> &samples) {
    if (!m_is_running) {
        return false;
    }

    if (m_params.save_audio) {
        m_wavWriter.write(m_pcmNew.data(), m_pcmNew.size());
    }

    if (!m_use_vad) {
        m_is_running = sdl_poll_events();
        if (!m_is_running) {
            return false;
        }

        while (true) {
            m_is_running = sdl_poll_events();
            if (!m_is_running) {
                return false;
            }
            m_audio->get(m_params.step_ms, m_pcmNew);
            if (static_cast<int>(m_pcmNew.size()) > 2 * m_n_samples_step) {
                m_audio->clear();
                continue;
            }
            if (static_cast<int>(m_pcmNew.size()) >= m_n_samples_step) {
                m_audio->clear();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const int n_new = static_cast<int>(m_pcmNew.size());
        const int n_take = std::min(static_cast<int>(m_pcmOld.size()),
                                    std::max(0, m_n_samples_keep + m_n_samples_len - n_new));

        m_pcm.resize(n_new + n_take);

        for (int i = 0; i < n_take; i++) {
            m_pcm[i] = m_pcmOld[m_pcmOld.size() - n_take + i];
        }
        memcpy(m_pcm.data() + n_take, m_pcmNew.data(), n_new * sizeof(float));

        samples = m_pcm;
        m_pcmOld = m_pcm;

        ++m_n_iter;
        if ((m_n_iter % m_n_new_line) == 0) {
            m_pcmOld = std::vector<float>(m_pcm.end() - m_n_samples_keep, m_pcm.end());
        }
    } else {
        while (true) {
            m_is_running = sdl_poll_events();
            if (!m_is_running) {
                return false;
            }

            const auto t_now  = std::chrono::high_resolution_clock::now();
            const auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - m_tLast).count();

            if (t_diff < 2000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            m_audio->get(2000, m_pcmNew);

            if (::vad_simple(m_pcmNew, WHISPER_SAMPLE_RATE, 1000, m_params.vad_thold, m_params.freq_thold, false)) {
                m_audio->get(m_params.length_ms, m_pcm);
                samples = m_pcm;
                m_tLast = t_now;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return true;
}

void SdlAudioStream::stop() {
    m_is_running = false;
    if (m_audio != nullptr) {
        m_audio->pause();
    }
}
