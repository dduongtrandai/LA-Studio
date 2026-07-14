#pragma once

#include "WorkflowGraph.h"

#include <QObject>
#include <QHash>
#include <QVariantMap>
#include <functional>

namespace LAStudio {

class WorkflowNodeExecutor : public QObject
{
    Q_OBJECT
public:
    explicit WorkflowNodeExecutor(QObject *parent = nullptr) : QObject(parent) {}
    ~WorkflowNodeExecutor() override = default;

    virtual void start(const QVariantMap &inputs, const QVariantMap &parameters) = 0;
    virtual void cancel() = 0;
    virtual void resume(const QVariantMap &decision) { Q_UNUSED(decision); }

signals:
    void progress(int percent, const QString &status);
    void completed(const QVariantMap &outputs);
    void failed(const QString &error);
    void waitingForInput(const QVariantMap &request);
};

struct WorkflowPropertyDefinition {
    QString id;
    QString title;
    QVariant defaultValue;
    bool required = false;
    bool connectable = false;
    bool affectsCache = true;
};

struct WorkflowNodeDefinition {
    QString typeId;
    int contractVersion = 1;
    QString title;
    QString category;
    QList<WorkflowPort> inputs;
    QList<WorkflowPort> outputs;
    std::function<WorkflowNodeExecutor *(QObject *)> createExecutor;
    QString description;
    QList<WorkflowPropertyDefinition> properties;
};

class NodeRegistry final : public QObject
{
    Q_OBJECT
public:
    explicit NodeRegistry(QObject *parent = nullptr);
    bool registerNode(const WorkflowNodeDefinition &definition);
    bool contains(const QString &typeId) const;
    bool contains(const QString &typeId, int contractVersion) const;
    WorkflowNodeDefinition definition(const QString &typeId) const;
    WorkflowNodeDefinition definition(const QString &typeId, int contractVersion) const;
    QList<WorkflowNodeDefinition> definitions() const;
    WorkflowNodeExecutor *createExecutor(const QString &typeId, QObject *parent) const;
    WorkflowNodeExecutor *createExecutor(const QString &typeId, int contractVersion, QObject *parent) const;

private:
    QHash<QString, WorkflowNodeDefinition> m_definitions;
};

} // namespace LAStudio
