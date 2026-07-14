#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio {

class ExampleManager : public QObject {
    Q_OBJECT

public:
    explicit ExampleManager(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList examplesForTask(const QString &task, const QVariantMap &family) const;
    Q_INVOKABLE QVariantMap resolveExample(const QVariantMap &example) const;

private:
    QVariantList loadExamples() const;
    void scanDirectory(const QString &rootPath, QVariantList &examples) const;
    bool matchesFamily(const QVariantMap &example, const QVariantMap &family) const;
    QString resolveLocalPath(const QString &path) const;
};

} // namespace LAStudio

