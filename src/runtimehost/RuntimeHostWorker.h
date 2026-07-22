#pragma once

#include "RuntimeHostAdapter.h"

#include <QObject>
#include <QCborMap>
#include <QCborValue>
#include <QVector>

#include <memory>
#include <atomic>

namespace LAStudio {

class RuntimeHostWorker final : public QObject {
    Q_OBJECT
public:
    explicit RuntimeHostWorker(QObject *parent = nullptr);
    ~RuntimeHostWorker() override;

public slots:
    void load(const QString &adapterId, const QCborMap &configuration);
    void infer(const QCborMap &request, const QVector<float> &referenceSamples);
    void cancel();
    // Cancellation is intentionally safe to call from the server thread while
    // infer() occupies the worker thread. Adapters must make cancel() a
    // lock-free/atomic signal (OmniVoice does).
    void requestCancel();
    void unload();

signals:
    void loadFinished(bool ok, QCborValue schema, QString error);
    void inferFinished(bool ok, RuntimeHostAdapter::Result result, QString error);
    void progress(int current, int total, QString stage, int chunkIndex, int chunkCount);
    void cancelled();

private:
    std::unique_ptr<RuntimeHostAdapter> m_adapter;
    std::atomic<bool> m_cancelRequested{false};
};

} // namespace LAStudio
