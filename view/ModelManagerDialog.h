#ifndef MODELMANAGERDIALOG_H
#define MODELMANAGERDIALOG_H

#include <QDialog>
#include <QVector>

#include "ModelManager.h"

class QListWidget;
class QPushButton;
class QProgressBar;
class QLabel;
class QEvent;

class ModelManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModelManagerDialog(QWidget *parent = nullptr);

    void setItems(const QVector<ModelItem> &items);
    QString selectedId() const;
    void setDownloading(bool downloading);
    void setProgress(qint64 received, qint64 total);
    void setStatus(const QString &text);
    void setDownloadAllowed(bool allowed);
    void setModelsDir(const QString &dir);

signals:
    void downloadRequested(const QString &id);
    void deleteRequested(const QString &id);
    void cancelRequested();
    void changeModelsDirRequested();
    void resetModelsDirRequested();

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void updateButtons();

private:
    void retranslateUi();
    static QString humanSize(qint64 bytes);

    QListWidget *m_list;
    QPushButton *m_download;
    QPushButton *m_delete;
    QPushButton *m_cancel;
    QPushButton *m_close;
    QProgressBar *m_progress;
    QLabel *m_status;
    QLabel *m_dirLabel;
    QLabel *m_modelsDir;
    QPushButton *m_changeDir;
    QPushButton *m_resetDir;

    QVector<ModelItem> m_items;
    bool m_downloading = false;
    bool m_downloadAllowed = true;
};

#endif // MODELMANAGERDIALOG_H
