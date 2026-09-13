#include <QCoreApplication>
#include "PulseAudioVirtualMic.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstring>

namespace {
const char *kSourceName = "qrtwhisper_virtmic";
const char *kSourceDescription = "QRTWhisper_Virtual_Mic";

struct OpData {
    pa_threaded_mainloop *ml = nullptr;
    int done = 0;
};

struct SinkListData : OpData {
    QStringList sinks;
};

struct ServerInfoData : OpData {
    QString defaultSink;
};

struct ModuleListData : OpData {
    QVector<uint32_t> modules;
};

struct LoadModuleData : OpData {
    uint32_t index = PA_INVALID_INDEX;
};

struct UnloadModuleData : OpData {
    int success = 0;
};

void sinkInfoCallback(pa_context *, const pa_sink_info *info, int eol, void *userdata) {
    auto *data = static_cast<SinkListData *>(userdata);
    if (eol > 0) {
        data->done = 1;
        pa_threaded_mainloop_signal(data->ml, 0);
        return;
    }
    if (info && info->name) {
        data->sinks << QString::fromUtf8(info->name);
    }
}

void serverInfoCallback(pa_context *, const pa_server_info *info, void *userdata) {
    auto *data = static_cast<ServerInfoData *>(userdata);
    if (info && info->default_sink_name) {
        data->defaultSink = QString::fromUtf8(info->default_sink_name);
    }
    data->done = 1;
    pa_threaded_mainloop_signal(data->ml, 0);
}

void moduleInfoCallback(pa_context *, const pa_module_info *info, int eol, void *userdata) {
    auto *data = static_cast<ModuleListData *>(userdata);
    if (eol > 0) {
        data->done = 1;
        pa_threaded_mainloop_signal(data->ml, 0);
        return;
    }
    if (info && info->name && info->argument) {
        const char *needle = "source_name=qrtwhisper_virtmic";
        if (qstrcmp(info->name, "module-remap-source") == 0
                && strstr(info->argument, needle) != nullptr) {
            data->modules << info->index;
        }
    }
}

void loadModuleCallback(pa_context *, uint32_t index, void *userdata) {
    auto *data = static_cast<LoadModuleData *>(userdata);
    data->index = index;
    data->done = 1;
    pa_threaded_mainloop_signal(data->ml, 0);
}

void unloadModuleCallback(pa_context *, int success, void *userdata) {
    auto *data = static_cast<UnloadModuleData *>(userdata);
    data->success = success;
    data->done = 1;
    pa_threaded_mainloop_signal(data->ml, 0);
}

void stateCallback(pa_context *, void *userdata) {
    auto *ml = static_cast<pa_threaded_mainloop *>(userdata);
    pa_threaded_mainloop_signal(ml, 0);
}

void waitDone(OpData *data, pa_operation *op) {
    pa_threaded_mainloop_lock(data->ml);
    while (!data->done) {
        pa_threaded_mainloop_wait(data->ml);
    }
    pa_threaded_mainloop_unlock(data->ml);
    if (op) {
        pa_operation_unref(op);
    }
}

class PulseSession {
public:
    explicit PulseSession(QString *errorOut) {
        m_mainloop = pa_threaded_mainloop_new();
        if (!m_mainloop) {
            fail(errorOut, QCoreApplication::translate("PulseAudioVirtualMic", "Failed to create the PulseAudio mainloop."));
            return;
        }

        m_context = pa_context_new(pa_threaded_mainloop_get_api(m_mainloop), "QRTWhisper");
        if (!m_context) {
            fail(errorOut, QCoreApplication::translate("PulseAudioVirtualMic", "Failed to create the PulseAudio context."));
            return;
        }

        pa_context_set_state_callback(m_context, stateCallback, m_mainloop);

        if (pa_threaded_mainloop_start(m_mainloop) < 0) {
            fail(errorOut, QCoreApplication::translate("PulseAudioVirtualMic", "Failed to start the PulseAudio mainloop."));
            return;
        }

        pa_threaded_mainloop_lock(m_mainloop);
        if (pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            const QString error = QString::fromUtf8(pa_strerror(pa_context_errno(m_context)));
            pa_threaded_mainloop_unlock(m_mainloop);
            fail(errorOut, error);
            return;
        }

        pa_context_state_t state;
        while ((state = pa_context_get_state(m_context)) == PA_CONTEXT_CONNECTING
               || state == PA_CONTEXT_AUTHORIZING
               || state == PA_CONTEXT_SETTING_NAME) {
            pa_threaded_mainloop_wait(m_mainloop);
        }

        const bool ready = (state == PA_CONTEXT_READY);
        QString error;
        if (!ready) {
            error = QString::fromUtf8(pa_strerror(pa_context_errno(m_context)));
        }
        pa_threaded_mainloop_unlock(m_mainloop);

        if (!ready) {
            fail(errorOut, error.isEmpty()
                     ? QCoreApplication::translate("PulseAudioVirtualMic", "Failed to connect to the PulseAudio server.")
                     : error);
            return;
        }

        m_ok = true;
    }

    ~PulseSession() {
        if (m_mainloop) {
            pa_threaded_mainloop_stop(m_mainloop);
        }
        if (m_context) {
            pa_context_disconnect(m_context);
            pa_context_unref(m_context);
        }
        if (m_mainloop) {
            pa_threaded_mainloop_free(m_mainloop);
        }
    }

    bool ok() const { return m_ok; }
    pa_context *context() const { return m_context; }
    pa_threaded_mainloop *mainloop() const { return m_mainloop; }

private:
    static void fail(QString *errorOut, const QString &error) {
        if (errorOut) {
            *errorOut = error;
        }
    }

    pa_threaded_mainloop *m_mainloop = nullptr;
    pa_context *m_context = nullptr;
    bool m_ok = false;
};
} // namespace

QStringList PulseAudioVirtualMic::outputSinks() const {
    PulseSession session(nullptr);
    if (!session.ok()) {
        return {};
    }

    SinkListData data;
    data.ml = session.mainloop();
    pa_operation *op = pa_context_get_sink_info_list(session.context(), sinkInfoCallback, &data);
    if (!op) {
        return {};
    }
    waitDone(&data, op);
    return data.sinks;
}

QString PulseAudioVirtualMic::defaultOutputSink() const {
    PulseSession session(nullptr);
    if (!session.ok()) {
        return {};
    }

    ServerInfoData data;
    data.ml = session.mainloop();
    pa_operation *op = pa_context_get_server_info(session.context(), serverInfoCallback, &data);
    if (!op) {
        return {};
    }
    waitDone(&data, op);
    return data.defaultSink;
}

bool PulseAudioVirtualMic::create(const QString &outputSink, QString *errorOut) {
    destroy();

    PulseSession session(errorOut);
    if (!session.ok()) {
        return false;
    }
    pa_context *context = session.context();
    pa_threaded_mainloop *mainloop = session.mainloop();

    ModuleListData moduleData;
    moduleData.ml = mainloop;
    pa_operation *listOp = pa_context_get_module_info_list(context, moduleInfoCallback, &moduleData);
    if (listOp) {
        waitDone(&moduleData, listOp);
    }

    for (uint32_t index : moduleData.modules) {
        UnloadModuleData unloadData;
        unloadData.ml = mainloop;
        pa_operation *unloadOp = pa_context_unload_module(context, index, unloadModuleCallback, &unloadData);
        if (unloadOp) {
            waitDone(&unloadData, unloadOp);
        }
    }

    const QByteArray argument = QStringLiteral("master=%1.monitor source_name=%2 source_properties=device.description=%3")
            .arg(outputSink, QLatin1String(kSourceName), QLatin1String(kSourceDescription))
            .toUtf8();

    LoadModuleData loadData;
    loadData.ml = mainloop;
    pa_operation *loadOp = pa_context_load_module(context, "module-remap-source", argument.constData(),
                                                  loadModuleCallback, &loadData);
    if (!loadOp) {
        if (errorOut) *errorOut = QCoreApplication::translate("PulseAudioVirtualMic", "Failed to load the PulseAudio module.");
        return false;
    }
    waitDone(&loadData, loadOp);

    if (loadData.index == PA_INVALID_INDEX) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(pa_strerror(pa_context_errno(context)));
        }
        return false;
    }

    m_moduleIndex = static_cast<int>(loadData.index);
    return true;
}

bool PulseAudioVirtualMic::destroy() {
    if (m_moduleIndex < 0) {
        return true;
    }

    PulseSession session(nullptr);
    if (!session.ok()) {
        m_moduleIndex = -1;
        return false;
    }

    UnloadModuleData data;
    data.ml = session.mainloop();
    pa_operation *op = pa_context_unload_module(session.context(), static_cast<uint32_t>(m_moduleIndex),
                                                unloadModuleCallback, &data);
    if (!op) {
        m_moduleIndex = -1;
        return false;
    }
    waitDone(&data, op);

    m_moduleIndex = -1;
    return data.success != 0;
}

QString PulseAudioVirtualMic::sourceToken() const {
    return QLatin1String(kSourceDescription);
}
