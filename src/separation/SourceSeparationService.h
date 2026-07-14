#pragma once

#include <QObject>
#include <QThread>
#include <QAtomicInt>
#include <memory>
#include "SeparationTypes.h"
#include "backends/SeparationBackendFactory.h"

namespace LAStudio {

class SeparationWorker;

class SourceSeparationService final : public QObject {
    Q_OBJECT
public:
    explicit SourceSeparationService(QObject *parent = nullptr);
    explicit SourceSeparationService(std::shared_ptr<SeparationBackendFactory> factory, QObject *parent = nullptr);
    ~SourceSeparationService() override;

    bool isolate(const SeparationRequest &request, QString *error = nullptr);
    void cancel();
    bool processing() const { return m_processing; }
    
    std::shared_ptr<SeparationBackendFactory> factory() const { return m_factory; }

signals:
    void progress(int percent, const QString &stage);
    void finished(const SeparationResult &result);

private slots:
    void onWorkerFinished(const SeparationResult &result);

private:
    void init();

    std::shared_ptr<SeparationBackendFactory> m_factory;
    QThread *m_thread = nullptr;
    SeparationWorker *m_worker = nullptr;
    bool m_processing = false;
    QAtomicInt m_cancelRequested{0};
};

} // namespace LAStudio
