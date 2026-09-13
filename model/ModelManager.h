#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QCryptographicHash;

struct ModelItem {
    QString id;
    QString fileName;
    qint64  sizeBytes = 0;
    bool    downloaded = false;
};

class ModelManager : public QObject {
    Q_OBJECT

public:
    explicit ModelManager(QObject *parent = nullptr);

    QString modelsDir() const;
    QString defaultModelsDir() const;
    bool setModelsDir(const QString &dir, QString *errorOut = nullptr);
    void resetModelsDir();
    bool ensureModelsDir(QString *errorOut = nullptr);

    QStringList catalogIds() const;
    QString fileName(const QString &id) const;
    QString modelPath(const QString &id) const;
    qint64 sizeOf(const QString &id) const;

    void refreshLocal();
    QStringList downloadedIds() const;
    bool isDownloaded(const QString &id) const;

    bool isMetadataLoaded() const { return m_metadataLoaded; }
    void fetchMetadata();

    bool isDownloading() const { return m_downloading; }
    bool downloadModel(const QString &id, QString *errorOut = nullptr);
    void cancelDownload();
    bool deleteModel(const QString &id, QString *errorOut = nullptr);

signals:
    void metadataReady();
    void metadataFailed(const QString &error);
    void downloadProgress(const QString &id, qint64 received, qint64 total);
    void downloadFinished(const QString &id, bool success, const QString &error);
    void downloadCanceled(const QString &id);
    void localModelsChanged();

private slots:
    void onReplyFinished();

private:
    struct Meta {
        qint64 size = 0;
        QString sha256;
    };

    QString downloadUrl(const QString &id) const;
    QString idFromFileName(const QString &fileName) const;
    bool catalogContains(const QString &id) const;
    qint64 fallbackSize(const QString &id) const;
    QString configuredModelsDir() const;

    QNetworkAccessManager *m_nam;
    QStringList m_catalog;
    QSet<QString> m_catalogSet;
    QMap<QString, Meta> m_metadata;
    bool m_metadataLoaded = false;
    bool m_metadataLoading = false;
    QSet<QString> m_downloaded;

    QNetworkReply *m_reply = nullptr;
    QFile *m_partFile = nullptr;
    QCryptographicHash *m_hash = nullptr;
    QString m_activeId;
    bool m_downloading = false;
    bool m_canceled = false;
};

#endif // MODELMANAGER_H
