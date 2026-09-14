#ifndef PACTLVIRTUALMIC_H
#define PACTLVIRTUALMIC_H

#include "VirtualMic.h"

class PactlVirtualMic : public VirtualMic {
public:
    QStringList outputSinks() const override;
    QString defaultOutputSink() const override;
    Error create(const QString &outputSink) override;
    bool destroy() override;
    QString sourceToken() const override;

private:
    QString runCommand(const QStringList &args, bool *ok, QString *stderrOut) const;

    int m_moduleIndex = -1;
};

#endif // PACTLVIRTUALMIC_H
