#include "MainWidget.h"

#include <QEvent>
#include <QSignalBlocker>

MainWidget::MainWidget() {
    m_modelLabel = new QLabel(this);
    m_sourceLabel = new QLabel(this);
    m_outputLabel = new QLabel(this);
    m_micLabel = new QLabel(this);
    m_displayLabel = new QLabel(this);

    auto modelRow = new QHBoxLayout();
    modelRow->addWidget(m_modelLabel);
    modelRow->addWidget(&q_model_selection);
    modelRow->addWidget(&manage_models_button);
    v_layout.addLayout(modelRow);

    virtual_output_radio.setChecked(true);

    auto sourceRow = new QHBoxLayout();
    sourceRow->addWidget(m_sourceLabel);
    sourceRow->addWidget(&virtual_output_radio);
    sourceRow->addWidget(&real_mic_radio);
    sourceRow->addStretch();
    v_layout.addLayout(sourceRow);

    auto outputRow = new QHBoxLayout();
    outputRow->addWidget(m_outputLabel);
    outputRow->addWidget(&q_output_selection);
    v_layout.addLayout(outputRow);

    auto micRow = new QHBoxLayout();
    micRow->addWidget(m_micLabel);
    micRow->addWidget(&q_device_selection);
    v_layout.addLayout(micRow);

    auto displayRow = new QHBoxLayout();
    displayRow->addWidget(m_displayLabel);
    displayRow->addWidget(&q_display_selection);
    v_layout.addLayout(displayRow);

    v_layout.addWidget(&start_button);

    auto languageRow = new QHBoxLayout();
    languageRow->addStretch();
    languageRow->addWidget(&q_language_selection);
    v_layout.addLayout(languageRow);

    setLayout(&v_layout);

    retranslateUi();

    updateEnabledState();
    connect(&virtual_output_radio, &QRadioButton::toggled, this, [this](bool) {
        updateEnabledState();
    });

    connect(&manage_models_button, &QPushButton::clicked, this, &MainWidget::manageModelsClicked);

    connect(&start_button, &QPushButton::clicked, this, [this]() {
        start_button.setEnabled(false);
        emit startClicked();
    });

    connect(&q_language_selection, &QComboBox::currentIndexChanged, this, [this]() {
        const QString code = q_language_selection.currentData().toString();
        if (!code.isEmpty()) {
            emit languageChangeRequested(code);
        }
    });
}

void MainWidget::retranslateUi() {
    m_modelLabel->setText(tr("Model:"));
    manage_models_button.setText(tr("Manage models…"));
    virtual_output_radio.setText(tr("System output (virtual mic)"));
    real_mic_radio.setText(tr("Real microphone"));
    m_sourceLabel->setText(tr("Source:"));
    m_outputLabel->setText(tr("Output:"));
    m_micLabel->setText(tr("Microphone:"));
    m_displayLabel->setText(tr("Display method:"));
    start_button.setText(tr("Start"));

    const int displayIndex = q_display_selection.currentIndex();
    {
        const QSignalBlocker blocker(&q_display_selection);
        q_display_selection.clear();
        q_display_selection.addItem(tr("System notification"));
        q_display_selection.addItem(tr("Subtitle"));
        if (displayIndex >= 0 && displayIndex < q_display_selection.count()) {
            q_display_selection.setCurrentIndex(displayIndex);
        }
    }

    const int languageIndex = q_language_selection.currentIndex();
    {
        const QSignalBlocker blocker(&q_language_selection);
        q_language_selection.clear();
        q_language_selection.addItem(QStringLiteral("English"), QStringLiteral("en"));
        q_language_selection.addItem(QStringLiteral("Español"), QStringLiteral("es"));
        if (languageIndex >= 0 && languageIndex < q_language_selection.count()) {
            q_language_selection.setCurrentIndex(languageIndex);
        }
    }
}

void MainWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void MainWidget::setLanguageCode(const QString &code) {
    const int index = q_language_selection.findData(code);
    if (index >= 0) {
        const QSignalBlocker blocker(&q_language_selection);
        q_language_selection.setCurrentIndex(index);
    }
}

void MainWidget::updateEnabledState() {
    q_output_selection.setEnabled(virtual_output_radio.isChecked());
    q_device_selection.setEnabled(real_mic_radio.isChecked());
}

void MainWidget::add_output_dev(const QString &devName) {
    q_output_selection.addItem(devName);
}

void MainWidget::add_mic_dev(const QString &devName) {
    q_device_selection.addItem(devName);
}

void MainWidget::select_output(const QString &name) {
    const int index = q_output_selection.findText(name);
    if (index >= 0) {
        q_output_selection.setCurrentIndex(index);
    }
}

void MainWidget::setStartEnabled(bool enabled) {
    start_button.setEnabled(enabled);
}

void MainWidget::setDownloadedModels(const QStringList &ids, const QString &selected) {
    const QString previous = q_model_selection.currentText();
    q_model_selection.clear();
    q_model_selection.addItems(ids);

    int index = ids.indexOf(selected);
    if (index < 0 && !previous.isEmpty()) {
        index = ids.indexOf(previous);
    }
    if (index < 0 && !ids.isEmpty()) {
        index = 0;
    }
    if (index >= 0) {
        q_model_selection.setCurrentIndex(index);
    }
}

QString MainWidget::selectedModelId() const {
    return q_model_selection.currentText();
}

SourceMode MainWidget::sourceMode() const {
    return virtual_output_radio.isChecked() ? SourceMode::VirtualOutput : SourceMode::RealMic;
}

QString MainWidget::selectedOutputName() const {
    return q_output_selection.currentText();
}

int MainWidget::selectedMicId() const {
    return q_device_selection.currentIndex();
}

DisplayMethod MainWidget::displayMethod() const {
    return q_display_selection.currentIndex() == 1 ? DisplayMethod::Subtitles : DisplayMethod::SystemNotification;
}
