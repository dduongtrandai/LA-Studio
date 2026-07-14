#pragma once

#include <QObject>
#include <QVariantMap>

namespace LAStudio {

// Capability boundary used by workflow node executors. Generic node code talks
// to this adapter instead of depending on a product-specific job runner.
class WorkflowExecutionAdapter : public QObject
{
    Q_OBJECT
public:
    explicit WorkflowExecutionAdapter(QObject *parent = nullptr) : QObject(parent) {}
    ~WorkflowExecutionAdapter() override = default;

    virtual void start(const QString &nodeType, const QVariantMap &inputs,
                       const QVariantMap &parameters) = 0;
    virtual void cancel() = 0;
    virtual void resume(const QVariantMap &decision) = 0;

signals:
    void stageCompleted(const QString &nodeId, const QVariantMap &outputs);
    void failed(const QString &message);
};

} // namespace LAStudio
