#pragma once

#include <QObject>
#include <QAtomicInt>
#include <memory>
#include "SeparationTypes.h"
#include "backends/SeparationBackendFactory.h"

namespace LAStudio {

class SeparationWorker final : public QObject {
    Q_OBJECT
public:
    explicit SeparationWorker(std::shared_ptr<SeparationBackendFactory> factory, QObject *parent = nullptr);
    ~SeparationWorker() override = default;

public slots:
    void process(const SeparationRequest &request, QAtomicInt *cancelFlag);

signals:
    void progress(int percent, const QString &stage);
    void finished(const SeparationResult &result);

private:
    std::shared_ptr<SeparationBackendFactory> m_factory;
};

} // namespace LAStudio
