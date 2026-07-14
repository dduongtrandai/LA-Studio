#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <memory>

namespace LAStudio {

class SttBackend;

class SttWorker : public QObject {
    Q_OBJECT

public:
    explicit SttWorker(QObject *parent = nullptr);
    ~SttWorker() override;

public slots:
    void loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath = QString());
    void unloadModel();
    void transcribe(const QVector<float> &samples, const QString &language, int threads, bool translate, const QVariantMap &settings);
    void cancelProcessing();

signals:
    void modelLoaded(bool success, const QString &error);
    void progress(int percent);
    void finished(const QString &text, const QVariantList &segments);
    void errorOccurred(const QString &error);

private:
    std::unique_ptr<SttBackend> m_backend;
};

} // namespace LAStudio

