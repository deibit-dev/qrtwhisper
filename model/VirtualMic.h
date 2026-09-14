#ifndef VIRTUALMIC_H
#define VIRTUALMIC_H

#include <QString>
#include <QStringList>

#include "Error.h"

class VirtualMic {
public:
    virtual ~VirtualMic() = default;

    virtual QStringList outputSinks() const = 0;
    virtual QString defaultOutputSink() const = 0;
    virtual Error create(const QString &outputSink) = 0;
    virtual bool destroy() = 0;
    virtual QString sourceToken() const = 0;
};

#endif // VIRTUALMIC_H
