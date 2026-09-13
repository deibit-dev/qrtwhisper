#include "ModelManagerDialog.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ModelManagerDialog::ModelManagerDialog(QWidget *parent) : QDialog(parent) {
    resize(520, 380);

    m_list = new QListWidget(this);

    m_download = new QPushButton(this);
    m_delete   = new QPushButton(this);
    m_cancel   = new QPushButton(this);
    m_close    = new QPushButton(this);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);

    m_status = new QLabel(this);

    m_modelsDir = new QLabel(this);
    m_modelsDir->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_dirLabel  = new QLabel(this);
    m_changeDir = new QPushButton(this);
    m_resetDir  = new QPushButton(this);

    auto *buttons = new QHBoxLayout();
    buttons->addWidget(m_download);
    buttons->addWidget(m_delete);
    buttons->addWidget(m_cancel);
    buttons->addStretch();
    buttons->addWidget(m_close);

    auto *dirRow = new QHBoxLayout();
    dirRow->addWidget(m_dirLabel);
    dirRow->addWidget(m_modelsDir, 1);
    dirRow->addWidget(m_changeDir);
    dirRow->addWidget(m_resetDir);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(dirRow);
    layout->addWidget(m_list);
    layout->addWidget(m_progress);
    layout->addWidget(m_status);
    layout->addLayout(buttons);

    retranslateUi();

    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *, QListWidgetItem *) { updateButtons(); });
    connect(m_download, &QPushButton::clicked, this, [this]() {
        const QString id = selectedId();
        if (!id.isEmpty()) {
            emit downloadRequested(id);
        }
    });
    connect(m_delete, &QPushButton::clicked, this, [this]() {
        const QString id = selectedId();
        if (!id.isEmpty()) {
            emit deleteRequested(id);
        }
    });
    connect(m_cancel, &QPushButton::clicked, this, &ModelManagerDialog::cancelRequested);
    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_changeDir, &QPushButton::clicked, this, &ModelManagerDialog::changeModelsDirRequested);
    connect(m_resetDir, &QPushButton::clicked, this, &ModelManagerDialog::resetModelsDirRequested);

    updateButtons();
}

void ModelManagerDialog::retranslateUi() {
    setWindowTitle(tr("Model Management"));
    m_download->setText(tr("Download"));
    m_delete->setText(tr("Delete"));
    m_cancel->setText(tr("Cancel"));
    m_close->setText(tr("Close"));
    m_dirLabel->setText(tr("Models directory:"));
    m_changeDir->setText(tr("Change…"));
    m_resetDir->setText(tr("Reset to default"));
    setItems(m_items);
}

void ModelManagerDialog::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void ModelManagerDialog::setItems(const QVector<ModelItem> &items) {
    const QString previous = selectedId();
    m_items = items;

    m_list->clear();
    for (const ModelItem &item : m_items) {
        QString label = item.fileName;
        if (item.downloaded) {
            label += tr("  —  Downloaded");
        } else if (item.sizeBytes > 0) {
            label += QStringLiteral("  —  ") + humanSize(item.sizeBytes);
        }
        auto *entry = new QListWidgetItem(label, m_list);
        entry->setData(Qt::UserRole, item.id);
        if (item.id == previous) {
            m_list->setCurrentItem(entry);
        }
    }

    if (!m_list->currentItem() && m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    updateButtons();
}

QString ModelManagerDialog::selectedId() const {
    QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ModelManagerDialog::setDownloading(bool downloading) {
    m_downloading = downloading;
    m_progress->setVisible(downloading);
    if (!downloading) {
        m_progress->setValue(0);
    }
    updateButtons();
}

void ModelManagerDialog::setProgress(qint64 received, qint64 total) {
    if (total > 0) {
        m_progress->setValue(static_cast<int>(received * 100 / total));
    } else {
        m_progress->setValue(0);
    }
}

void ModelManagerDialog::setStatus(const QString &text) {
    m_status->setText(text);
}

void ModelManagerDialog::setDownloadAllowed(bool allowed) {
    m_downloadAllowed = allowed;
    updateButtons();
}

void ModelManagerDialog::setModelsDir(const QString &dir) {
    m_modelsDir->setText(dir);
}

void ModelManagerDialog::updateButtons() {
    const QString id = selectedId();
    const bool hasSelection = !id.isEmpty();

    bool downloaded = false;
    for (const ModelItem &item : m_items) {
        if (item.id == id) {
            downloaded = item.downloaded;
            break;
        }
    }

    m_download->setEnabled(m_downloadAllowed && hasSelection && !downloaded && !m_downloading);
    m_delete->setEnabled(hasSelection && downloaded && !m_downloading);
    m_cancel->setEnabled(m_downloading);
}

QString ModelManagerDialog::humanSize(qint64 bytes) {
    if (bytes <= 0) {
        return QString();
    }
    const double kb = 1024.0;
    if (bytes < kb) {
        return QString::number(bytes) + QStringLiteral(" B");
    }
    const double mb = bytes / (kb * kb);
    if (mb < 1024.0) {
        return QString::number(mb, 'f', 0) + QStringLiteral(" MB");
    }
    const double gb = mb / 1024.0;
    return QString::number(gb, 'f', 1) + QStringLiteral(" GB");
}
