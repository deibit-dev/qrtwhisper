#include "ModelManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

namespace {
const QString kHfBase = QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp");
const QString kMetadataUrl = QStringLiteral("https://huggingface.co/api/models/ggerganov/whisper.cpp?blobs=true");
}

ModelManager::ModelManager(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);

    m_catalog = {
        QStringLiteral("tiny.en"),
        QStringLiteral("base.en"),
        QStringLiteral("small.en"),
        QStringLiteral("medium.en"),
        QStringLiteral("tiny"),
        QStringLiteral("base"),
        QStringLiteral("small"),
        QStringLiteral("medium"),
        QStringLiteral("large-v3"),
        QStringLiteral("large-v3-turbo"),
    };
    for (const QString &id : m_catalog) {
        m_catalogSet.insert(id);
    }
}

QString ModelManager::configuredModelsDir() const {
    return QSettings().value(QStringLiteral("models/dir")).toString();
}

QString ModelManager::defaultModelsDir() const {
    QString base = QCoreApplication::applicationDirPath();
    // AppImage: use the writable folder next to the AppImage file instead of the
    // read-only mount point.
    const QByteArray appImage = qgetenv("APPIMAGE");
    if (!appImage.isEmpty()) {
        const QString dir = QFileInfo(QString::fromLocal8Bit(appImage)).absolutePath();
        if (!dir.isEmpty()) {
            base = dir;
        }
    }
    return base + QStringLiteral("/models");
}

QString ModelManager::modelsDir() const {
    const QString configured = configuredModelsDir();
    return configured.isEmpty() ? defaultModelsDir() : configured;
}

bool ModelManager::ensureModelsDir(QString *errorOut) {
    // If the stored directory does not exist (or none is stored), fall back to
    // the default location (./models relative to the executable / AppImage),
    // persist that value and make sure the directory is created.
    const QString saved = configuredModelsDir();
    QString dir = saved;
    if (dir.isEmpty() || !QDir(dir).exists()) {
        dir = defaultModelsDir();
        QSettings settings;
        settings.setValue(QStringLiteral("models/dir"), QDir(dir).absolutePath());
    }

    QDir target(dir);
    if (target.exists()) {
        return true;
    }
    if (!target.mkpath(QStringLiteral("."))) {
        if (errorOut) {
            *errorOut = tr("Failed to create the models directory: ") + target.absolutePath();
        }
        return false;
    }
    return true;
}

bool ModelManager::setModelsDir(const QString &dir, QString *errorOut) {
    QDir target(dir);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        if (errorOut) {
            *errorOut = tr("Failed to create the models directory: ") + target.absolutePath();
        }
        return false;
    }
    if (!QFileInfo(target.absolutePath()).isWritable()) {
        if (errorOut) {
            *errorOut = tr("The models directory is not writable: ") + target.absolutePath();
        }
        return false;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("models/dir"), target.absolutePath());
    refreshLocal();
    return true;
}

void ModelManager::resetModelsDir() {
    // Restore the default location and persist it, so a valid directory is
    // always stored (re-creating it if necessary).
    const QString def = defaultModelsDir();
    QDir().mkpath(def);
    QSettings settings;
    settings.setValue(QStringLiteral("models/dir"), QDir(def).absolutePath());
    refreshLocal();
}

QStringList ModelManager::catalogIds() const {
    return m_catalog;
}

QString ModelManager::fileName(const QString &id) const {
    return QStringLiteral("ggml-") + id + QStringLiteral(".bin");
}

QString ModelManager::modelPath(const QString &id) const {
    return modelsDir() + QLatin1Char('/') + fileName(id);
}

qint64 ModelManager::sizeOf(const QString &id) const {
    if (m_metadata.contains(id) && m_metadata.value(id).size > 0) {
        return m_metadata.value(id).size;
    }
    return fallbackSize(id);
}

qint64 ModelManager::fallbackSize(const QString &id) const {
    static const QMap<QString, qint64> sizes = {
        {QStringLiteral("tiny.en"),        75LL * 1024 * 1024},
        {QStringLiteral("base.en"),       142LL * 1024 * 1024},
        {QStringLiteral("small.en"),      466LL * 1024 * 1024},
        {QStringLiteral("medium.en"),    1500LL * 1024 * 1024},
        {QStringLiteral("tiny"),           75LL * 1024 * 1024},
        {QStringLiteral("base"),          142LL * 1024 * 1024},
        {QStringLiteral("small"),         466LL * 1024 * 1024},
        {QStringLiteral("medium"),       1500LL * 1024 * 1024},
        {QStringLiteral("large-v3"),     3100LL * 1024 * 1024},
        {QStringLiteral("large-v3-turbo"),1600LL * 1024 * 1024},
    };
    return sizes.value(id, 0);
}

QString ModelManager::idFromFileName(const QString &fileName) const {
    const QString prefix = QStringLiteral("ggml-");
    const QString suffix = QStringLiteral(".bin");
    if (fileName.startsWith(prefix) && fileName.endsWith(suffix)) {
        return fileName.mid(prefix.length(), fileName.length() - prefix.length() - suffix.length());
    }
    return QString();
}

bool ModelManager::catalogContains(const QString &id) const {
    return m_catalogSet.contains(id);
}

QString ModelManager::downloadUrl(const QString &id) const {
    return kHfBase + QStringLiteral("/resolve/main/") + fileName(id);
}

void ModelManager::refreshLocal() {
    m_downloaded.clear();
    QDir dir(modelsDir());
    const QStringList files = dir.entryList({QStringLiteral("ggml-*.bin")}, QDir::Files, QDir::Name);
    for (const QString &f : files) {
        const QString id = idFromFileName(f);
        if (catalogContains(id)) {
            m_downloaded.insert(id);
        }
    }
    emit localModelsChanged();
}

QStringList ModelManager::downloadedIds() const {
    QStringList result;
    for (const QString &id : m_catalog) {
        if (m_downloaded.contains(id)) {
            result << id;
        }
    }
    return result;
}

bool ModelManager::isDownloaded(const QString &id) const {
    return m_downloaded.contains(id);
}

void ModelManager::fetchMetadata() {
    if (m_metadataLoading) {
        return;
    }
    m_metadataLoading = true;

    QNetworkRequest request((QUrl(kMetadataUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QRTWhisper/0.1"));

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_metadataLoading = false;

        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            reply->deleteLater();
            emit metadataFailed(error);
            return;
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            emit metadataFailed(tr("Invalid server response."));
            return;
        }

        m_metadata.clear();
        const QJsonArray siblings = doc.object().value(QStringLiteral("siblings")).toArray();
        for (const QJsonValue &value : siblings) {
            const QJsonObject obj = value.toObject();
            const QString rfilename = obj.value(QStringLiteral("rfilename")).toString();
            const QString id = idFromFileName(rfilename);
            if (id.isEmpty() || !catalogContains(id)) {
                continue;
            }

            Meta meta;
            const QJsonObject lfs = obj.value(QStringLiteral("lfs")).toObject();
            meta.sha256 = lfs.value(QStringLiteral("sha256")).toString();
            meta.size = static_cast<qint64>(lfs.value(QStringLiteral("size")).toDouble());
            if (meta.size <= 0) {
                meta.size = static_cast<qint64>(obj.value(QStringLiteral("size")).toDouble());
            }
            m_metadata.insert(id, meta);
        }

        m_metadataLoaded = true;
        emit metadataReady();
    });
}

bool ModelManager::downloadModel(const QString &id, QString *errorOut) {
    if (m_downloading) {
        if (errorOut) *errorOut = tr("A download is already in progress.");
        return false;
    }
    if (!m_metadata.contains(id)) {
        if (errorOut) *errorOut = tr("No metadata available for this model. Try again.");
        return false;
    }
    const QString sha256 = m_metadata.value(id).sha256;
    if (sha256.isEmpty()) {
        if (errorOut) *errorOut = tr("No checksum available for this model.");
        return false;
    }

    const QString partPath = modelPath(id) + QStringLiteral(".part");
    m_partFile = new QFile(partPath, this);
    if (!m_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = m_partFile->errorString();
        delete m_partFile;
        m_partFile = nullptr;
        if (errorOut) *errorOut = error;
        return false;
    }

    m_hash = new QCryptographicHash(QCryptographicHash::Sha256);
    m_activeId = id;
    m_downloading = true;
    m_canceled = false;

    QNetworkRequest request((QUrl(downloadUrl(id))));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QRTWhisper/0.1"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        const QByteArray chunk = m_reply->readAll();
        if (m_partFile) m_partFile->write(chunk);
        if (m_hash) m_hash->addData(chunk);
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit downloadProgress(m_activeId, received, total);
    });
    connect(m_reply, &QNetworkReply::finished, this, &ModelManager::onReplyFinished);

    return true;
}

void ModelManager::cancelDownload() {
    if (m_reply) {
        m_canceled = true;
        m_reply->abort();
    }
}

void ModelManager::onReplyFinished() {
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }

    const QString id = m_activeId;
    const bool canceled = m_canceled;
    m_canceled = false;

    const QByteArray chunk = reply->readAll();
    if (!chunk.isEmpty() && m_partFile) m_partFile->write(chunk);
    if (!chunk.isEmpty() && m_hash) m_hash->addData(chunk);

    if (m_partFile) m_partFile->close();
    m_partFile->deleteLater();
    m_partFile = nullptr;

    const QNetworkReply::NetworkError error = reply->error();
    const bool httpOk = (error == QNetworkReply::NoError);

    QString errorText;
    bool success = false;

    if (canceled) {
        // nothing; handled below
    } else if (httpOk) {
        const QString expected = m_metadata.value(id).sha256;
        const QString actual = m_hash ? QString::fromLatin1(m_hash->result().toHex()) : QString();
        if (expected.isEmpty()) {
            errorText = tr("Could not verify the checksum.");
        } else if (actual.compare(expected, Qt::CaseInsensitive) != 0) {
            errorText = tr("Checksum mismatch. The file was discarded.");
        } else if (QFile::rename(modelPath(id) + QStringLiteral(".part"), modelPath(id))) {
            success = true;
        } else {
            errorText = tr("Could not move the downloaded file.");
        }
    } else {
        errorText = reply->errorString();
    }

    if (!success) {
        QFile::remove(modelPath(id) + QStringLiteral(".part"));
    }

    delete m_hash;
    m_hash = nullptr;
    reply->deleteLater();

    m_downloading = false;
    m_activeId.clear();

    if (success) {
        refreshLocal();
    }

    if (canceled) {
        emit downloadCanceled(id);
    } else {
        emit downloadFinished(id, success, errorText);
    }
}

bool ModelManager::deleteModel(const QString &id, QString *errorOut) {
    const QString path = modelPath(id);
    QFile file(path);
    if (!file.exists()) {
        if (errorOut) *errorOut = tr("The model does not exist.");
        return false;
    }
    if (!file.remove()) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }
    m_downloaded.remove(id);
    emit localModelsChanged();
    return true;
}
