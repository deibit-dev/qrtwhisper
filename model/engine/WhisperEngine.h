#ifndef WHISPERENGINE_H
#define WHISPERENGINE_H

#include "ITranscriptionEngine.h"
#include "WhisperParams.h"
#include "whisper.h"

#include <string>
#include <vector>

class WhisperEngine : public ITranscriptionEngine {
public:
    explicit WhisperEngine(const whisper_params &params);
    ~WhisperEngine() override;

    bool load(const QString &modelPath, Error *errorOut = nullptr) override;
    bool transcribe(const std::vector<float> &samples,
                    QVector<QString> &segments,
                    Error *errorOut = nullptr) override;
    bool isMultilingual() const override;
    void shutdown() override;

private:
    whisper_params m_params;
    whisper_context *m_ctx = nullptr;

    std::string m_language;
    bool m_translate = false;
    bool m_no_timestamps = false;
    bool m_no_context = true;
    bool m_single_segment = false;
    bool m_use_vad = false;
    int m_n_new_line = 1;
    int m_n_iter = 0;

    std::vector<whisper_token> m_promptTokens;
};

#endif // WHISPERENGINE_H
