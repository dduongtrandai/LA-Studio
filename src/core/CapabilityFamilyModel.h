#pragma once
#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

namespace LAStudio {

class ModelManager;
class RuntimeManager;
class RegistryManager;
class Settings;
class StudioSelectionRepository;

class CapabilityFamilyModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY availableLanguagesChanged)
    Q_PROPERTY(QString languageFilter READ languageFilter WRITE setLanguageFilter NOTIFY languageFilterChanged)
public:
    enum FamilyRoles {
        FamilyIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        SubtitleRole,
        DescriptionRole,
        AccentRole,
        SupportedRole,
        InstalledRole,
        SelectedRole,
        ReadyRole,
        StatusReasonRole,
        SelectedRuntimeIdRole,
        SelectedRuntimeVersionRole,
        RuntimeOptionsRole,
        MissingRequirementsRole,
        RequiredFilesRole,
        SelectedFilesRole,
        RawMetadataRole,
        ModelCardUrlRole,
        ReadmeContentRole,
        ThumbnailSourceRole,
        IconNameRole,
        FamilyCapabilityRole,
        StatusKindRole,
        StatusTitleRole,
        InfoBadgesRole,
        CapabilityBadgesRole,
        StatsBadgesRole,
        PreferredRuntimeIdRole,
        PreferredRuntimeVersionRole,
        IsLastudioPickRole,
        PickLabelRole,
        PickReasonRole
    };

    explicit CapabilityFamilyModel(ModelManager *models, RuntimeManager *runtimes, RegistryManager *registry, Settings *settings, QObject *parent = nullptr);
    ~CapabilityFamilyModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setCapability(const QString &capabilityId);
    Q_INVOKABLE void setSearchText(const QString &searchText);
    Q_INVOKABLE void setStatusFilter(const QString &filterType);
    Q_INVOKABLE void setLanguageFilter(const QString &languageCode);
    Q_INVOKABLE void setSelectedFamilyId(const QString &familyId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap itemForFamily(const QString &familyId) const;
    Q_INVOKABLE QString firstFamilyId() const;
    Q_INVOKABLE QVariantMap recommendedConfiguration() const;
    Q_INVOKABLE QVariantMap configurationForFamily(const QString &familyId) const;
    QVariantMap recommendedConfigurationExcluding(const QStringList &excludedFamilyIds) const;
    Q_INVOKABLE bool isFileInstalled(const QVariantMap &family, const QString &fileName, const QVariantMap &req) const;
    Q_INVOKABLE bool isModelSuitable(const QString &filename, const QVariantMap &family, const QVariantMap &requirement = QVariantMap()) const;
    Q_INVOKABLE QString estimateSize(const QString &filename, const QString &defaultFile, const QString &defaultSizeStr) const;
    Q_INVOKABLE void selectFileForRequirement(const QString &familyId, const QString &reqFile, const QString &selectedFile);
    Q_INVOKABLE void setInitialSelectedFiles(const QString &familyId, const QVariantMap &initialSelected);
    Q_INVOKABLE void saveSelectionForFamily(const QString &familyId,
                                            const QString &runtimeId,
                                            const QString &runtimeVersion,
                                            const QVariantMap &selectedFiles);

    int revision() const { return m_revision; }
    QVariantList availableLanguages() const { return m_availableLanguages; }
    QString languageFilter() const { return m_languageFilter; }

signals:
    void revisionChanged();
    void availableLanguagesChanged();
    void languageFilterChanged();

private:
    struct FamilyItem {
        QVariantMap rawMap;
        QString id;
        QString displayName;
        QString subtitle;
        QString description;
        QString accent;
        bool supported = false;
        bool installed = false;
        bool selected = false;
        bool ready = false;
        QString statusReason;
        QString selectedRuntimeId;
        QString selectedRuntimeVersion;
        QVariantList runtimeOptions;
        QVariantList missingRequirements;
        QVariantList requiredFiles;
        QVariantMap selectedFiles;
        QString modelCardUrl;
        QString readmeContent;
        QString thumbnailSource;
        QString iconName;
        QString familyCapability;
        QString statusKind;
        QString statusTitle;
        QVariantList infoBadges;
        QVariantList capabilityBadges;
        QVariantList statsBadges;
        QString preferredRuntimeId;
        QString preferredRuntimeVersion;
        bool isLastudioPick = false;
        QString pickLabel;
        QString pickReason;
    };

    QVariantMap toVariantMap(const FamilyItem &item) const;
    void updateItems();
    bool hasFamilyFile(const QVariantMap &family, const QString &modelId, const QString &fileName) const;
    bool requirementRequiredForCapability(const QVariantMap &req, const QString &capability) const;
    QString capabilityForFamily(const QVariantMap &family) const;
    QString recommendedFileForRequirement(const QVariantMap &family,
                                          const QVariantMap &requirement,
                                          const QVariantList &candidates) const;
    QVariantMap storedFilesByRequirement(const QVariantMap &family,
                                         const QString &capabilityId,
                                         const QString &familyId) const;

    QString m_capabilityId;
    QString m_searchText;
    QString m_statusFilter;
    QString m_languageFilter = QStringLiteral("all");
    QString m_selectedFamilyId;
    QList<FamilyItem> m_items;
    QVariantList m_availableLanguages;
    QHash<QString, QVariantMap> m_userSelectedFiles;
    int m_revision = 0;

    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;
    RegistryManager *m_registry = nullptr;
    Settings *m_settings = nullptr;
    StudioSelectionRepository *m_selectionRepository = nullptr;
};

} // namespace LAStudio
