#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>

class QEvent;

enum class SourceMode { VirtualOutput, RealMic };
enum class DisplayMethod { SystemNotification, Subtitles };

class MainWidget : public QWidget {
    Q_OBJECT

public:
    MainWidget();
    void add_output_dev(const QString &devName);
    void add_mic_dev(const QString &devName);
    void select_output(const QString &name);
    void setStartEnabled(bool enabled);

    void setDownloadedModels(const QStringList &ids, const QString &selected);
    QString selectedModelId() const;
    void setLanguageCode(const QString &code);

    SourceMode sourceMode() const;
    QString selectedOutputName() const;
    int selectedMicId() const;
    DisplayMethod displayMethod() const;

signals:
    void startClicked();
    void manageModelsClicked();
    void languageChangeRequested(const QString &code);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();
    void updateEnabledState();

    QVBoxLayout v_layout;
    QLabel *m_modelLabel;
    QLabel *m_sourceLabel;
    QLabel *m_outputLabel;
    QLabel *m_micLabel;
    QLabel *m_displayLabel;
    QComboBox q_model_selection;
    QPushButton manage_models_button;
    QRadioButton virtual_output_radio;
    QRadioButton real_mic_radio;
    QComboBox q_output_selection;
    QComboBox q_device_selection;
    QComboBox q_display_selection;
    QComboBox q_language_selection;
    QPushButton start_button;
};

#endif // MAINWIDGET_H
