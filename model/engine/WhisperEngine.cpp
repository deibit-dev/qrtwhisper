#include "WhisperEngine.h"

#include "whisper.h"

#include <algorithm>
#include <cstdio>

namespace {
bool isBlankAudio(const QString &text) {
    QString normalized = text;
    normalized.remove(QLatin1Char('[')).remove(QLatin1Char(']'))
              .remove(QLatin1Char(' ')).remove(QLatin1Char('_'))
              .remove(QLatin1Char('\n')).remove(QLatin1Char('\r'));
    return normalized.compare(QStringLiteral("BLANKAUDIO"), Qt::CaseInsensitive) == 0;
}
}

WhisperEngine::WhisperEngine(const whisper_params &params)
    : m_params(params), m_language(params.language), m_translate(params.translate) {
}

WhisperEngine::~WhisperEngine() {
    shutdown();
}

bool WhisperEngine::load(const QString &modelPath, Error *errorOut) {
    if (m_language != "auto" && whisper_lang_id(m_language.c_str()) == -1) {
        if (errorOut) {
            *errorOut = Error{ErrorCode::UnknownLanguage, QString::fromStdString(m_language)};
        }
        return false;
    }

    const int n_samples_step = static_cast<int>(1e-3 * m_params.step_ms * WHISPER_SAMPLE_RATE);
    m_use_vad = n_samples_step <= 0;
    m_no_timestamps = !m_use_vad;
    m_no_context = m_params.no_context || m_use_vad;
    m_single_segment = !m_use_vad;
    m_n_new_line = !m_use_vad ? std::max(1, m_params.length_ms / m_params.step_ms - 1) : 1;

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu    = m_params.use_gpu;
    cparams.flash_attn = m_params.flash_attn;

    m_ctx = whisper_init_from_file_with_params(modelPath.toUtf8().constData(), cparams);
    if (m_ctx == nullptr) {
        if (errorOut) {
            *errorOut = Error{ErrorCode::ModelLoadFailed, modelPath};
        }
        return false;
    }

    if (!whisper_is_multilingual(m_ctx)) {
        if (m_language != "en" || m_translate) {
            m_language = "en";
            m_translate = false;
            fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
        }
    }

    return true;
}

bool WhisperEngine::transcribe(const std::vector<float> &samples,
                               QVector<QString> &segments,
                               Error *errorOut) {
    if (m_ctx == nullptr) {
        if (errorOut) {
            *errorOut = Error{ErrorCode::TranscriptionFailed, {}};
        }
        return false;
    }

    whisper_full_params wparams = whisper_full_default_params(
        m_params.beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY);

    wparams.print_progress    = false;
    wparams.print_special     = m_params.print_special;
    wparams.print_realtime    = false;
    wparams.print_timestamps  = !m_no_timestamps;
    wparams.translate         = m_translate;
    wparams.single_segment    = m_single_segment;
    wparams.max_tokens        = 0;
    wparams.language          = m_language.c_str();
    wparams.n_threads         = m_params.n_threads;
    wparams.beam_search.beam_size = m_params.beam_size;
    wparams.audio_ctx         = m_params.audio_ctx;
    wparams.tdrz_enable       = m_params.tinydiarize;
    wparams.temperature_inc   = m_params.no_fallback ? 0.0f : wparams.temperature_inc;
    wparams.prompt_tokens     = m_no_context ? nullptr : m_promptTokens.data();
    wparams.prompt_n_tokens   = m_no_context ? 0 : static_cast<int>(m_promptTokens.size());

    if (whisper_full(m_ctx, wparams, samples.data(), static_cast<int>(samples.size())) != 0) {
        if (errorOut) {
            *errorOut = Error{ErrorCode::TranscriptionFailed, {}};
        }
        return false;
    }

    const int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(m_ctx, i);
        const QString segmentText = QString::fromUtf8(text);

        if (m_params.filter_blank_audio && isBlankAudio(segmentText)) {
            continue;
        }

        if (!m_no_timestamps) {
            QString output = segmentText;
            if (whisper_full_get_segment_speaker_turn_next(m_ctx, i)) {
                output += QStringLiteral(" [SPEAKER_TURN]");
            }
            output += QStringLiteral("\n");
            segments.append(output);
        } else {
            segments.append(segmentText);
        }
    }

    ++m_n_iter;
    if (!m_use_vad && (m_n_iter % m_n_new_line) == 0) {
        if (!m_no_context) {
            m_promptTokens.clear();
            for (int i = 0; i < n_segments; ++i) {
                const int token_count = whisper_full_n_tokens(m_ctx, i);
                for (int j = 0; j < token_count; ++j) {
                    m_promptTokens.push_back(whisper_full_get_token_id(m_ctx, i, j));
                }
            }
        }
    }

    return true;
}

bool WhisperEngine::isMultilingual() const {
    return m_ctx != nullptr && whisper_is_multilingual(m_ctx);
}

void WhisperEngine::shutdown() {
    if (m_ctx != nullptr) {
        whisper_print_timings(m_ctx);
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}
