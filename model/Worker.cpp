#include "Worker.h"

#include "common-sdl.h"
#include "common.h"
#include "common-whisper.h"
#include "whisper.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

Worker::Worker(int mic_dev, const std::string &modelPath) : QObject(nullptr), m_stop(false) {
    m_ready = (setup_capture(mic_dev, modelPath) == 0);
}

void Worker::doWork() {
    if (!m_ready) {
        emit finished();
        return;
    }

    while (is_running && !m_stop) {
        if (params.save_audio) {
            wavWriter.write(pcmf32_new.data(), pcmf32_new.size());
        }

        is_running = sdl_poll_events();
        if (!is_running) {
            break;
        }

        // process new audio
        if (!use_vad) {
            while (true) {
                is_running = sdl_poll_events();
                if (!is_running) {
                    break;
                }
                audio->get(params.step_ms, pcmf32_new);
                if ((int) pcmf32_new.size() > 2 * n_samples_step) {
                    audio->clear();
                    continue;
                }
                if ((int) pcmf32_new.size() >= n_samples_step) {
                    audio->clear();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            const int n_samples_new = pcmf32_new.size();
            const int n_samples_take = std::min((int) pcmf32_old.size(),
                                                std::max(0, n_samples_keep + n_samples_len - n_samples_new));

            pcmf32.resize(n_samples_new + n_samples_take);

            for (int i = 0; i < n_samples_take; i++) {
                pcmf32[i] = pcmf32_old[pcmf32_old.size() - n_samples_take + i];
            }

            memcpy(pcmf32.data() + n_samples_take, pcmf32_new.data(), n_samples_new * sizeof(float));

            pcmf32_old = pcmf32;
        } else {
            const auto t_now  = std::chrono::high_resolution_clock::now();
            const auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_last).count();

            if (t_diff < 2000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            audio->get(2000, pcmf32_new);

            if (::vad_simple(pcmf32_new, WHISPER_SAMPLE_RATE, 1000, params.vad_thold, params.freq_thold, false)) {
                audio->get(params.length_ms, pcmf32);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            t_last = t_now;
        }

        // run the inference
        {
            whisper_full_params wparams = whisper_full_default_params(
                params.beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY);

            wparams.print_progress   = false;
            wparams.print_special    = params.print_special;
            wparams.print_realtime   = false;
            wparams.print_timestamps = !params.no_timestamps;
            wparams.translate        = params.translate;
            wparams.single_segment   = !use_vad;
            wparams.max_tokens       = params.max_tokens;
            wparams.language         = params.language.c_str();
            wparams.n_threads        = params.n_threads;
            wparams.beam_search.beam_size = params.beam_size;
            wparams.audio_ctx        = params.audio_ctx;
            wparams.tdrz_enable      = params.tinydiarize;
            wparams.temperature_inc  = params.no_fallback ? 0.0f : wparams.temperature_inc;
            wparams.prompt_tokens    = params.no_context ? nullptr : prompt_tokens.data();
            wparams.prompt_n_tokens  = params.no_context ? 0       : prompt_tokens.size();

            if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
                return;
            }

            {
                if (use_vad) {
                    const int64_t t1 = (t_last - t_start).count() / 1000000;
                    const int64_t t0 = std::max(0.0, t1 - pcmf32.size() * 1000.0 / WHISPER_SAMPLE_RATE);
                    (void)t0; // suppress unused warning
                }

                const int n_segments = whisper_full_n_segments(ctx);
                for (int i = 0; i < n_segments; ++i) {
                    const char * text = whisper_full_get_segment_text(ctx, i);

                    if (params.no_timestamps) {
                        qDebug() << text;
                        emit conditionMessage(text);
                    } else {
                        QString output = text;

                        if (whisper_full_get_segment_speaker_turn_next(ctx, i)) {
                            output += " [SPEAKER_TURN]";
                        }
                        output += "\n";

                        qDebug() << output;
                        emit conditionMessage(output);
                    }
                }
            }

            ++n_iter;

            if (!use_vad && (n_iter % n_new_line) == 0) {
                pcmf32_old = std::vector<float>(pcmf32.end() - n_samples_keep, pcmf32.end());

                if (!params.no_context) {
                    prompt_tokens.clear();
                    const int n_segments = whisper_full_n_segments(ctx);
                    for (int i = 0; i < n_segments; ++i) {
                        const int token_count = whisper_full_n_tokens(ctx, i);
                        for (int j = 0; j < token_count; ++j) {
                            prompt_tokens.push_back(whisper_full_get_token_id(ctx, i, j));
                        }
                    }
                }
            }
        }
    }

    audio->pause();
    whisper_print_timings(ctx);
    whisper_free(ctx);

    emit finished();
}

int Worker::setup_capture(int mic_dev, const std::string &modelPath)
{
    params.capture_id = mic_dev;
    params.model      = modelPath;

    params.keep_ms   = std::min(params.keep_ms,   params.step_ms);
    params.length_ms = std::max(params.length_ms, params.step_ms);

    n_samples_step = (1e-3 * params.step_ms)   * WHISPER_SAMPLE_RATE;
    n_samples_len  = (1e-3 * params.length_ms)  * WHISPER_SAMPLE_RATE;
    n_samples_keep = (1e-3 * params.keep_ms)    * WHISPER_SAMPLE_RATE;
    n_samples_30s  = (1e-3 * 30000.0)           * WHISPER_SAMPLE_RATE;

    use_vad = n_samples_step <= 0;
    n_new_line = !use_vad ? std::max(1, params.length_ms / params.step_ms - 1) : 1;

    params.no_timestamps  = !use_vad;
    params.no_context    |= use_vad;
    params.max_tokens     = 0;

    // init audio
    audio = new audio_async(params.length_ms);
    if (!audio->init(params.capture_id, WHISPER_SAMPLE_RATE)) {
        m_error = tr("Failed to initialize audio capture (SDL).");
        return 1;
    }
    audio->resume();

    // whisper init
    if (params.language != "auto" && whisper_lang_id(params.language.c_str()) == -1) {
        m_error = tr("Unknown language: ") + QString::fromStdString(params.language);
        return 1;
    }

    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu    = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);
    if (ctx == nullptr) {
        m_error = tr("Failed to load model: ") + QString::fromStdString(params.model);
        return 1;
    }

    pcmf32     = std::vector<float>(n_samples_30s, 0.0f);
    pcmf32_new = std::vector<float>(n_samples_30s, 0.0f);

    {
        fprintf(stderr, "\n");
        if (!whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        fprintf(stderr, "%s: processing %d samples (step = %.1f sec / len = %.1f sec / keep = %.1f sec), %d threads, lang = %s, task = %s, timestamps = %d ...\n",
                __func__,
                n_samples_step,
                float(n_samples_step) / WHISPER_SAMPLE_RATE,
                float(n_samples_len)  / WHISPER_SAMPLE_RATE,
                float(n_samples_keep) / WHISPER_SAMPLE_RATE,
                params.n_threads,
                params.language.c_str(),
                params.translate ? "translate" : "transcribe",
                params.no_timestamps ? 0 : 1);

        if (!use_vad) {
            fprintf(stderr, "%s: n_new_line = %d, no_context = %d\n", __func__, n_new_line, params.no_context);
        } else {
            fprintf(stderr, "%s: using VAD, will transcribe on speech activity\n", __func__);
        }
        fprintf(stderr, "\n");
    }

    n_iter = 0;
    is_running = true;

    if (params.fname_out.length() > 0) {
        fout.open(params.fname_out);
        if (!fout.is_open()) {
            m_error = tr("Failed to open the output file: ") + QString::fromStdString(params.fname_out);
            return 1;
        }
    }

    if (params.save_audio) {
        time_t now = time(0);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", localtime(&now));
        std::string filename = std::string(buffer) + ".wav";
        wavWriter.open(filename, WHISPER_SAMPLE_RATE, 16, 1);
    }

    t_last  = std::chrono::high_resolution_clock::now();
    t_start = t_last;

    return 0;
}
