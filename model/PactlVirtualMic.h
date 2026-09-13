#ifndef PACTLVIRTUALMIC_H
#define PACTLVIRTUALMIC_H

#include "VirtualMic.h"

class PactlVirtualMic : public VirtualMic {
public:
    QStringList outputSinks() const override;
    QString defaultOutputSink() const override;
    bool create(const QString &outputSink, QString *errorOut = nullptr) override;
    bool destroy() override;
    QString sourceToken() const override;

private:
    QString runCommand(const QStringList &args, bool *ok, QString *stderrOut) const;

    int m_moduleIndex = -1;
};

#endif // PACTLVIRTUALMIC_H
