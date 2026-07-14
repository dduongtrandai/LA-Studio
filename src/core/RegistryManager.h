#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>

namespace LAStudio {

class CatalogManager;

class RegistryManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList modelCategories READ modelCategories NOTIFY registryUpdated)
    Q_PROPERTY(QVariantList ttsFamilies READ ttsFamilies NOTIFY registryUpdated)
    Q_PROPERTY(QVariantList sttFamilies READ sttFamilies NOTIFY registryUpdated)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)

public:
    explicit RegistryManager(QObject *parent = nullptr);

    QVariantList modelCategories() const { return m_modelCategories; }
    QVariantList ttsFamilies() const { return m_ttsFamilies; }
    QVariantList sttFamilies() const { return m_sttFamilies; }
    QString databasePath() const { return m_databasePath; }

    void initializeFromCatalog(CatalogManager *catalog);
    Q_INVOKABLE void refreshFromCatalog();
    QString connectionName() const;

signals:
    void registryUpdated();
    void errorOccurred(const QString &error);

private:
    bool openDatabase();
    bool ensureSchema();
    bool importCatalog(const QVariantList &modelCategories,
                       const QVariantList &ttsFamilies,
                       const QVariantList &sttFamilies,
                       const QString &catalogVersion);
    bool reloadCachedViews();

    QString m_databasePath;
    QString m_connectionName;
    CatalogManager *m_catalog = nullptr;
    QVariantList m_modelCategories;
    QVariantList m_ttsFamilies;
    QVariantList m_sttFamilies;
};

} // namespace LAStudio
