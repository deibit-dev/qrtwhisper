#ifndef WHISPERPARAMS_H
#define WHISPERPARAMS_H

#include <cstdint>
#include <string>
#include <thread>
#include <algorithm>

struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t step_ms    = 3000;
    int32_t length_ms  = 10000;
    int32_t keep_ms    = 200;
    int32_t capture_id = 1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;
    int32_t beam_size  = -1;

    float vad_thold    = 0.6f;
    float freq_thold   = 100.0f;

    bool translate     = false;
    bool no_fallback   = false;
    bool print_special = false;
    bool no_context    = true;
    bool no_timestamps = false;
    bool tinydiarize   = false;
    bool save_audio    = false;
    bool use_gpu       = true;
    bool flash_attn    = false;

    std::string language  = "en";
    std::string model;
    std::string fname_out;
};

#endif // WHISPERPARAMS_H
