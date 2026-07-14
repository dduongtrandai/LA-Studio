#include "ModelsPathMigrationService.h"
#include "ModelsPathMigrator.h"
#include "controllers/AppController.h"
#include "controllers/ModelSessionRegistry.h"

#include "core/Settings.h"
#include "core/ModelManager.h"
#include "core/DownloadManager.h"
#include "core/PathUtils.h"
#include "stt/SttEngine.h"
#include "tts/TtsEngine.h"
#include "core/Logger.h"

#include <QDir>
#include <QThreadPool>
#include <QPointer>
#include <QCoreApplication>

namespace LAStudio {

ModelsPathMigrationService::ModelsPathMigrationService(Settings *settings,
                                                       ModelManager *models,
                                                       DownloadManager *downloads,
                                                       SttEngine *stt,
                                                       TtsEngine *tts,
                                                       QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_models(models)
    , m_downloads(downloads)
    , m_stt(stt)
    , m_tts(tts)
{
}

void ModelsPathMigrationService::changeDirectory(const QString &pathUrl)
{
    if (m_running) {
        return;
    }

    const QString localPath = PathUtils::urlToLocalPath(pathUrl);
    if (localPath.trimmed().isEmpty()) {
        return;
    }

    QString newPath = QDir(localPath).absolutePath();
    QString oldPath = QDir(m_models ? m_models->modelsRoot() : QString()).absolutePath();

    if (newPath.isEmpty() || newPath == oldPath) {
        return;
    }

    // Check if nested
    QString oldPathSlash = oldPath;
    if (!oldPathSlash.endsWith(u'/')) oldPathSlash += u'/';
    QString newPathSlash = newPath;
    if (!newPathSlash.endsWith(u'/')) newPathSlash += u'/';

    if (newPathSlash.startsWith(oldPathSlash, Qt::CaseInsensitive) || oldPathSlash.startsWith(newPathSlash, Qt::CaseInsensitive)) {
        m_error = QStringLiteral("Cannot move models directory to a nested directory.");
        emit errorChanged();
        return;
    }

    if (m_stt && m_stt->isProcessing() || m_tts && m_tts->isProcessing()) {
        m_error = QStringLiteral("Cannot change models directory while engines are processing.");
        emit errorChanged();
        return;
    }

    if (m_downloads && !m_downloads->activeDownloads().isEmpty()) {
        m_error = QStringLiteral("Cannot change models directory while downloading.");
        emit errorChanged();
        return;
    }

    m_error.clear();
    emit errorChanged();

    m_message = QStringLiteral("Starting migration...");
    emit messageChanged();

    m_running = true;
    emit runningChanged();

    AppController* appCtrl = AppController::instance();
    if (appCtrl && appCtrl->sessionRegistry()) {
        const QStringList capabilities = { QStringLiteral("stt"), QStringLiteral("tts") };
        for (const QString &fallbackCapability : capabilities) {
            IModelSession *session = appCtrl->sessionRegistry()->sessionForCapability(fallbackCapability);
            if (!session) continue;

            const QList<SessionConfiguration> loaded = session->loadedConfigurations();
            if (loaded.isEmpty()) {
                QString owner = fallbackCapability;
                if (auto pending = session->pendingConfiguration()) {
                    owner = pending->capabilityId;
                }
                session->requestUnload(owner);
                continue;
            }
            for (const SessionConfiguration &config : loaded) {
                if (!config.signature.isEmpty()) {
                    session->requestUnloadConfiguration(config.signature);
                }
            }
        }
    }

    QPointer<ModelsPathMigrationService> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, oldPath, newPath]() {
        QString errorMsg;
        bool copySuccess = true;
        bool cleanupSuccess = true;

        if (QDir(oldPath).exists()) {
            QCoreApplication* app = QCoreApplication::instance();
            if (app) {
                QMetaObject::invokeMethod(app, [weakThis]() {
                    if (!weakThis) return;
                    weakThis->m_message = QStringLiteral("Copying models...");
                    emit weakThis->messageChanged();
                });
            }

            copySuccess = ModelsPathMigrator::copyDirectoryMerge(oldPath, newPath, errorMsg);
            if (copySuccess && QDir(oldPath).exists()) {
                cleanupSuccess = ModelsPathMigrator::removeDirectory(oldPath);
            }
        }

        QCoreApplication* app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [weakThis, copySuccess, cleanupSuccess, errorMsg, newPath]() {
                if (!weakThis) return;
                if (copySuccess) {
                    if (weakThis->m_settings) {
                        weakThis->m_settings->setModelsPath(newPath);
                    }
                    if (weakThis->m_models) {
                        weakThis->m_models->setModelsRoot(newPath);
                        weakThis->m_models->scanLocalModels();
                    }

                    weakThis->m_running = false;
                    emit weakThis->runningChanged();
                     
                    if (cleanupSuccess) {
                        weakThis->m_message = QStringLiteral("Migration successful.");
                    } else {
                        weakThis->m_message = QStringLiteral("Migration successful, but failed to clean up old directory.");
                    }
                    emit weakThis->messageChanged();
                } else {
                    weakThis->m_error = errorMsg;
                    emit weakThis->errorChanged();

                    weakThis->m_running = false;
                    emit weakThis->runningChanged();
                }
            });
        }
    });
}

} // namespace LAStudio
