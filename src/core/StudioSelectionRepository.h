#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QSqlDatabase>

namespace LAStudio {

struct StudioConfiguration {
    QString capabilityId;
    QString familyId;
    QString runtimeId;
    QString runtimeVersion;
    QVariantMap selectedFiles; // maps role -> filename

    bool isValid() const {
        return !capabilityId.isEmpty() && !familyId.isEmpty();
    }

    QString signature(const QString &runtimePath, const QVariantMap &resolvedPaths) const {
        // stable signature calculation
        QStringList keys = resolvedPaths.keys();
        std::sort(keys.begin(), keys.end());
        QStringList parts;
        parts.append(QStringLiteral("capabilityId=") + capabilityId);
        QString cleanedPath = runtimePath;
        cleanedPath.replace(QStringLiteral("\\"), QStringLiteral("/"));
        parts.append(QStringLiteral("runtimePath=") + cleanedPath);
        for (const QString &key : keys) {
            QString val = resolvedPaths.value(key).toString();
            val.replace(QStringLiteral("\\"), QStringLiteral("/"));
            parts.append(key + QStringLiteral("=") + val);
        }
        return parts.join(QStringLiteral("|"));
    }
};

class Settings;

class StudioSelectionRepository : public QObject {
    Q_OBJECT
public:
    explicit StudioSelectionRepository(const QString &connectionName, QObject *parent = nullptr);
    ~StudioSelectionRepository() override = default;

    StudioConfiguration selectionFor(const QString &capabilityId) const;
    void saveActiveSelection(const StudioConfiguration &selection);
    void clearActiveSelection(const QString &capabilityId);
    void migrateLegacySelectionsIfNeeded(Settings *settings);

private:
    QSqlDatabase db() const;

    QString m_connectionName;
};

} // namespace LAStudio
