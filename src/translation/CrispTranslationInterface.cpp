#include "runtimes/CrispTranslationInterface.h"

#include "core/Logger.h"
#include "core/PathUtils.h"
#include <QDir>
#include <QFileInfo>

namespace LAStudio {

bool CrispTranslationInterface::load(const QString &libraryPath)
{
    unload();
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(libraryPath));
    if (!QFileInfo::exists(cleanPath)) {
        m_error = QStringLiteral("CrispASR translation runtime library does not exist: %1").arg(cleanPath);
        return false;
    }

    const QStringList runtimeDirs = crispRuntimeDependencyDirs(cleanPath);
    crispPrependRuntimeDirsToPath(runtimeDirs);
#ifdef Q_OS_WIN
    m_preloadedDlls = crispPreloadRuntimeDlls(cleanPath, runtimeDirs);
#endif
    m_library.setFileName(cleanPath);
    m_library.setLoadHints(QLibrary::ExportExternalSymbolsHint);
    if (!m_library.load()) {
        m_error = m_library.errorString();
        return false;
    }

    m_sessionOpen = reinterpret_cast<session_open_fn>(m_library.resolve("crispasr_session_open_with_params"));
    m_sessionClose = reinterpret_cast<session_close_fn>(m_library.resolve("crispasr_session_close"));
    m_translateText = reinterpret_cast<translate_text_fn>(m_library.resolve("crispasr_session_translate_text"));
    m_translateTextFree = reinterpret_cast<translate_text_free_fn>(m_library.resolve("crispasr_session_translate_text_free"));
    if (!m_sessionOpen || !m_sessionClose || !m_translateText || !m_translateTextFree) {
        m_error = QStringLiteral("CrispASR runtime is missing the text translation ABI.");
        unload();
        return false;
    }
    return true;
}

void CrispTranslationInterface::unload()
{
    m_sessionOpen = nullptr;
    m_sessionClose = nullptr;
    m_translateText = nullptr;
    m_translateTextFree = nullptr;
#ifdef Q_OS_WIN
    crispUnloadLibraryAndDependencies(m_library, m_preloadedDlls);
#else
    if (m_library.isLoaded()) m_library.unload();
#endif
}

QString CrispTranslationInterface::translate(const QString &modelPath, const QString &backend,
                                             const QString &text, const QString &sourceLanguage,
                                             const QString &targetLanguage, int threads, bool useGpu,
                                             int maxTokens, QString *error) const
{
    if (!m_sessionOpen || text.trimmed().isEmpty()) return {};

    crispasr_open_params_v1 params{};
    params.abi_version = 1;
    params.n_threads = qMax(1, threads);
    params.use_gpu = useGpu ? 1 : 0;
    params.verbosity = 0;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;

    const QByteArray model = PathUtils::toNativeShortPath(modelPath).toUtf8();
    const QByteArray backendBytes = backend.toUtf8();
    crispasr_session *session = m_sessionOpen(model.constData(), backendBytes.constData(), &params);
    if (!session) {
        if (error) *error = QStringLiteral("Failed to open CrispASR %1 translation session.").arg(backend);
        return {};
    }

    const QByteArray input = text.toUtf8();
    const QByteArray source = sourceLanguage.toUtf8();
    const QByteArray target = targetLanguage.toUtf8();
    char *translated = m_translateText(session, input.constData(), source.constData(), target.constData(), maxTokens);
    QString result;
    if (translated) {
        result = QString::fromUtf8(translated).trimmed();
        m_translateTextFree(translated);
    } else if (error) {
        *error = QStringLiteral("CrispASR %1 returned no translation output.").arg(backend);
    }
    m_sessionClose(session);
    return result;
}

QStringList CrispTranslationInterface::translateBatch(const QString &modelPath, const QString &backend,
                                                       const QStringList &texts, const QString &sourceLanguage,
                                                       const QString &targetLanguage, int threads, bool useGpu,
                                                       int maxTokens, QString *error) const
{
    QStringList results;
    if (!m_sessionOpen || texts.isEmpty()) return results;
    crispasr_open_params_v1 params{};
    params.abi_version = 1;
    params.n_threads = qMax(1, threads);
    params.use_gpu = useGpu ? 1 : 0;
    params.verbosity = 0;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;
    const QByteArray model = PathUtils::toNativeShortPath(modelPath).toUtf8();
    const QByteArray backendBytes = backend.toUtf8();
    crispasr_session *session = m_sessionOpen(model.constData(), backendBytes.constData(), &params);
    if (!session) {
        if (error) *error = QStringLiteral("Failed to open CrispASR %1 translation session.").arg(backend);
        return {};
    }
    for (const QString &text : texts) {
        const QByteArray input = text.toUtf8();
        const QByteArray source = sourceLanguage.toUtf8();
        const QByteArray target = targetLanguage.toUtf8();
        char *translated = m_translateText(session, input.constData(), source.constData(), target.constData(), maxTokens);
        if (!translated) {
            if (error) *error = QStringLiteral("CrispASR %1 returned no translation output.").arg(backend);
            m_sessionClose(session);
            return {};
        }
        results.append(QString::fromUtf8(translated).trimmed());
        m_translateTextFree(translated);
    }
    m_sessionClose(session);
    return results;
}

} // namespace LAStudio
