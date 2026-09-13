#include <QCoreApplication>
#include "PactlVirtualMic.h"

#include <QProcess>

namespace {
const char *kSourceName = "qrtwhisper_virtmic";
const char *kSourceDescription = "QRTWhisper_Virtual_Mic";
const int kTimeoutMs = 3000;
}

QString PactlVirtualMic::runCommand(const QStringList &args, bool *ok, QString *stderrOut) const {
    QProcess process;
    process.start(QStringLiteral("pactl"), args);

    if (!process.waitForStarted(kTimeoutMs)) {
        if (ok) *ok = false;
        if (stderrOut) *stderrOut = QCoreApplication::translate("PactlVirtualMic", "Failed to start 'pactl'.");
        return {};
    }

    if (!process.waitForFinished(kTimeoutMs)) {
        process.kill();
        process.waitForFinished(kTimeoutMs);
        if (ok) *ok = false;
        if (stderrOut) *stderrOut = QCoreApplication::translate("PactlVirtualMic", "'pactl' did not respond in time.");
        return {};
    }

    const QString out = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(process.readAllStandardError());

    if (ok) *ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (stderrOut) *stderrOut = err;
    return out;
}

QStringList PactlVirtualMic::outputSinks() const {
    bool ok = false;
    const QString out = runCommand({QStringLiteral("list"), QStringLiteral("short"),
                                    QStringLiteral("sinks")}, &ok, nullptr);
    if (!ok) {
        return {};
    }

    QStringList sinks;
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() >= 2 && !fields.at(1).isEmpty()) {
            sinks << fields.at(1);
        }
    }
    return sinks;
}

QString PactlVirtualMic::defaultOutputSink() const {
    bool ok = false;
    const QString out = runCommand({QStringLiteral("get-default-sink")}, &ok, nullptr);
    return ok ? out.trimmed() : QString();
}

bool PactlVirtualMic::create(const QString &outputSink, QString *errorOut) {
    m_moduleIndex = -1;

    bool ok = false;
    QString err;

    const QString modules = runCommand({QStringLiteral("list"), QStringLiteral("short"),
                                        QStringLiteral("modules")}, &ok, &err);
    if (!ok) {
        if (errorOut) *errorOut = err;
        return false;
    }

    for (const QString &line : modules.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() < 2 || fields.at(1) != QLatin1String("module-remap-source")) {
            continue;
        }
        const QString argument = fields.size() >= 3 ? fields.at(2) : QString();
        if (argument.contains(QStringLiteral("source_name=") + QLatin1String(kSourceName))) {
            runCommand({QStringLiteral("unload-module"), fields.at(0)}, nullptr, nullptr);
        }
    }

    const QString monitor = outputSink + QLatin1String(".monitor");
    const QString out = runCommand({
        QStringLiteral("load-module"),
        QStringLiteral("module-remap-source"),
        QStringLiteral("master=") + monitor,
        QStringLiteral("source_name=") + QLatin1String(kSourceName),
        QStringLiteral("source_properties=device.description=") + QLatin1String(kSourceDescription),
    }, &ok, &err);

    if (!ok) {
        if (errorOut) *errorOut = err.isEmpty() ? QCoreApplication::translate("PactlVirtualMic", "Failed to load the module.") : err;
        return false;
    }

    bool parsed = false;
    m_moduleIndex = out.trimmed().toInt(&parsed);
    if (!parsed) {
        if (errorOut) *errorOut = QCoreApplication::translate("PactlVirtualMic", "Could not read the loaded module index.");
        return false;
    }

    return true;
}

bool PactlVirtualMic::destroy() {
    if (m_moduleIndex < 0) {
        return true;
    }

    bool ok = false;
    runCommand({QStringLiteral("unload-module"), QString::number(m_moduleIndex)}, &ok, nullptr);
    m_moduleIndex = -1;
    return ok;
}

QString PactlVirtualMic::sourceToken() const {
    return QLatin1String(kSourceDescription);
}
