#pragma once

#include <QObject>

class QThread;

namespace LAStudio {

// Owns the QThread/worker relationship; the capability instance remains
// responsible for invoking its worker's typed unload operation before stop().
class WorkerThreadHost final : public QObject
{
    Q_OBJECT
public:
    explicit WorkerThreadHost(QObject *parent = nullptr);
    ~WorkerThreadHost() override;

    bool start(QObject *worker);
    void stop();
    QThread *thread() const { return m_thread; }
    QObject *worker() const { return m_worker; }

private:
    QThread *m_thread = nullptr;
    QObject *m_worker = nullptr;
};

} // namespace LAStudio
