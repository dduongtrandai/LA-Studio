#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>
#include <QString>
#include <QtQml/qqml.h>
#include <QVariantList>

namespace LAStudio {

struct ModelInfo {
    QString id;
    QString author;
    QString task;       // "stt", "tts", "voice-clone"
    QString format;     // "gguf", "onnx", "bin"
    QString path;       // local directory
    QStringList files;
    QString size;       // formatted string e.g. "1.2 GB"
    qint64 totalSize = 0;
    QString arch;
    QString params;
    QString publisher;
    QString quant;
    QString modified;
};

class ModelManager : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ModelManager is managed by AppController")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int version READ version NOTIFY registryUpdated)
    Q_PROPERTY(QString modelsRoot READ modelsRoot NOTIFY modelsRootChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TaskRole,
        FormatRole,
        PathRole,
        FilesRole,
        SizeRole,
        TotalSizeRole,
        ArchRole,
        ParamsRole,
        PublisherRole,
        QuantRole,
        ModifiedRole
    };
    Q_ENUM(Roles)

    explicit ModelManager(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_models.size()); }
    int version() const { return m_version; }

    Q_INVOKABLE void addModel(const QString &id, const QString &task,
                               const QString &format, const QString &path,
                               const QStringList &files, const QString &size);
    Q_INVOKABLE void removeModel(const QString &id);
    Q_INVOKABLE QVariantMap findModel(const QString &id) const;
    Q_INVOKABLE QVariantList modelsForTask(const QString &task) const;
    Q_INVOKABLE QVariantList filteredSttModels(const QString &searchText = QString()) const;
    Q_INVOKABLE QVariantList filteredVoiceCloneModels(const QString &searchText = QString(),
                                                       const QString &runtimeId = QString());
    Q_INVOKABLE bool hasFile(const QString &modelId, const QString &filename) const;
    Q_INVOKABLE QString filePath(const QString &modelId, const QString &filename) const;
    QString firstAvailableFilePath(const QString &modelId, const QStringList &filenames) const;
    Q_INVOKABLE QVariantMap fileMetadata(const QString &modelId, const QString &filename) const;
    Q_INVOKABLE bool setFileMetadata(const QString &modelId, const QString &filename, const QVariantMap &metadata);
    Q_INVOKABLE QString concreteModelDir(const QString &modelId) const;
    Q_INVOKABLE QString virtualModelDir(const QString &modelId) const;
    Q_INVOKABLE void scanLocalModels();

    QString modelsRoot() const;
    void setModelsRoot(const QString &root);

    void loadRegistry();
    void saveRegistry();

signals:
    void countChanged();
    void registryUpdated();
    void modelsRootChanged();

private:
    void refreshVoiceCloneCache();

    QString registryPath() const;
    QString m_modelsRoot;
    QVector<ModelInfo> m_models;
    int m_version = 0;
    QVariantList m_cachedVoiceCloneModels;
    bool m_voiceCloneCacheDirty = true;
};

} // namespace LAStudio
