#ifndef ERROR_H
#define ERROR_H

#include <QString>

enum class ErrorCode {
    None = 0,
    AudioCaptureFailed,
    ModelLoadFailed,
    UnknownLanguage,
    TranscriptionFailed,
    VirtualMicFailed,
};

struct Error {
    ErrorCode code = ErrorCode::None;
    QString detail;

    bool ok() const { return code == ErrorCode::None; }
};

#endif // ERROR_H
