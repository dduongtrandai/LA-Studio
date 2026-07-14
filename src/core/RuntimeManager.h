#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDir>
#include <runtimes/WhisperInterface.h>
#include <runtimes/OmnivoiceInterface.h>
#include <runtimes/VibevoiceInterface.h>
#include <runtimes/VieneuTtsInterface.h>
#include <QVector>

namespace LAStudio {

class CatalogManager;
class Settings;

struct RuntimeInfo {
    QString id;
    QString engineFamily; // e.g., "crispasr", "omnivoice.cpp"
    QString variant;      // e.g., "win-x86_64-cpu", "win-x86_64-cuda-12"
    QString name;
    QString version;
    QString type;        // e.g., "whisper.cpp"
    QString directory;   // Full path to the runtime folder
    QString libraryPath; // Path to the main DLL/SO
    QString kind = QStringLiteral("dynamic-library");
    QString executablePath; // Process runtime entrypoint
    QString protocolVersion;
    QStringList capabilities;
    QStringList modelFormats;
    QVariantMap metadata;

    bool isUsable() const
    {
        return kind == QStringLiteral("process") ? !executablePath.isEmpty()
                                                 : !libraryPath.isEmpty();
    }
};

class RuntimeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList allRuntimes READ allRuntimes NOTIFY registryUpdated)
    Q_PROPERTY(QString backendsPath READ backendsPath CONSTANT)

public:
    explicit RuntimeManager(CatalogManager *catalog, Settings *settings, QObject *parent = nullptr);

    QVariantList allRuntimes() const;
    QString backendsPath() const;
    
    Q_INVOKABLE void scanRuntimes();
    Q_INVOKABLE bool loadRuntime(const QString &id);
    Q_INVOKABLE bool loadTtsRuntime(const QString &id);
    Q_INVOKABLE QString getRuntimePath(const QString &id) const;
    Q_INVOKABLE QString getRuntimePathForVersion(const QString &id, const QString &version) const;
    Q_INVOKABLE QString getRuntimeExecutablePath(const QString &id) const;
    Q_INVOKABLE QString getRuntimeExecutablePathForVersion(const QString &id, const QString &version) const;
    Q_INVOKABLE QString getRuntimeKindForVersion(const QString &id, const QString &version) const;
    Q_INVOKABLE QVariantList runtimeVersions(const QString &id) const;
    Q_INVOKABLE bool removeRuntimeVersion(const QString &id, const QString &version);
    RuntimeInfo inspectRuntimeDirectory(const QString &directory) const;

signals:
    void registryUpdated();
    void runtimeVersionAboutToBeRemoved(const QString &id, const QString &version);

private:
    RuntimeInfo processRuntimeDir(const QDir &dir,
                                  const QString &familyHint,
                                  const QStringList &catalogCandidates,
                                  const QVariantList &catalogFamilies) const;

    QVector<RuntimeInfo> m_runtimes;
    CatalogManager *m_catalog = nullptr;
    Settings *m_settings = nullptr;
};

} // namespace LAStudio
