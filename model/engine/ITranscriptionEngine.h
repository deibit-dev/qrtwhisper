#ifndef ITRANSCRIPTIONENGINE_H
#define ITRANSCRIPTIONENGINE_H

#include <QString>
#include <QVector>
#include <vector>

#include "Error.h"

class ITranscriptionEngine {
public:
    virtual ~ITranscriptionEngine() = default;

    virtual bool load(const QString &modelPath, Error *errorOut = nullptr) = 0;
    virtual bool transcribe(const std::vector<float> &samples,
                            QVector<QString> &segments,
                            Error *errorOut = nullptr) = 0;
    virtual bool isMultilingual() const = 0;
    virtual void shutdown() = 0;
};

#endif // ITRANSCRIPTIONENGINE_H
