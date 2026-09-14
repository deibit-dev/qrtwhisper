#ifndef IAUDIOSTREAM_H
#define IAUDIOSTREAM_H

#include <QString>
#include <vector>

#include "Error.h"

class IAudioStream {
public:
    virtual ~IAudioStream() = default;

    virtual bool start(int captureId, Error *errorOut = nullptr) = 0;
    virtual bool nextWindow(std::vector<float> &samples) = 0;
    virtual void stop() = 0;
};

#endif // IAUDIOSTREAM_H
