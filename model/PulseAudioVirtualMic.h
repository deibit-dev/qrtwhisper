#ifndef PULSEAUDIOVIRTUALMIC_H
#define PULSEAUDIOVIRTUALMIC_H

#include "VirtualMic.h"

#include <pulse/pulseaudio.h>

class PulseAudioVirtualMic : public VirtualMic {
public:
    QStringList outputSinks() const override;
    QString defaultOutputSink() const override;
    bool create(const QString &outputSink, QString *errorOut = nullptr) override;
    bool destroy() override;
    QString sourceToken() const override;

private:
    int m_moduleIndex = -1;
};

#endif // PULSEAUDIOVIRTUALMIC_H
