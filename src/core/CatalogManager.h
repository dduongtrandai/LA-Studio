#pragma once

#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QHash>
#include <QString>
#include <QNetworkReply>

#include <QtQml/qqml.h>

namespace LAStudio {

class CatalogManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("CatalogManager is managed by AppController")
    Q_PROPERTY(QVariantList modelCategories READ modelCategories NOTIFY catalogUpdated)
    Q_PROPERTY(QVariantList ttsFamilies READ ttsFamilies NOTIFY catalogUpdated)
    Q_PROPERTY(QVariantList sttFamilies READ sttFamilies NOTIFY catalogUpdated)
    Q_PROPERTY(bool isUpdating READ isUpdating NOTIFY isUpdatingChanged)

public:
    explicit CatalogManager(QObject *parent = nullptr);

    QVariantList modelCategories() const { return m_modelCategories; }
    QVariantList ttsFamilies() const { return m_ttsFamilies; }
    QVariantList sttFamilies() const { return m_sttFamilies; }
    QString version() const { return m_version; }
    bool isUpdating() const { return m_isUpdating; }

    Q_INVOKABLE void fetchRemoteCatalog(const QString &url);
    Q_INVOKABLE QVariantList languageSet(const QString &setId);

signals:
    void catalogUpdated();
    void isUpdatingChanged();
    void errorOccurred(const QString &error);

private:
    void loadLocalCatalog();
    void parseCatalog(const QByteArray &data);
    void saveToCache(const QByteArray &data);

    QVariantList m_modelCategories;
    QVariantList m_ttsFamilies;
    QVariantList m_sttFamilies;
    QString m_version;
    bool m_isUpdating = false;
    QNetworkAccessManager *m_networkManager;
    QHash<QString, QVariantList> m_languageSetsCache;
};

} // namespace LAStudio
