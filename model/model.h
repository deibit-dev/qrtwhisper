#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <list>
#include <string>

#include <QObject>
#include <QString>
#include <QThread>

class Worker;

class Model : public QObject {
    Q_OBJECT

public:
    Model();
    bool start(int mic_dev, const std::string &modelPath);
    QString get_last_transcription() { return last_transcription; }
    QString lastError() const { return m_lastError; }
    std::list<std::pair<int, std::string>> get_mic_devices();
    int find_capture_device(const std::string &token);
    void stop_transcription();

signals:
    void update();

private slots:
    void handleMessage(const QString &msg) {
        std::cout << msg.toStdString() << std::endl;
        last_transcription = msg;
        emit update();
    }

private:
    Worker* worker = nullptr;
    QThread workerThread;
    QString last_transcription;
    QString m_lastError;
    std::list<std::pair<int, std::string>> mic_devices;
};

#endif // MODEL_H
