#pragma once

#include <QLibrary>
#include <QString>
#include <QStringList>
#include "CrispCommon.h"

namespace LAStudio {

class CrispTranslationInterface
{
public:
    ~CrispTranslationInterface() { unload(); }

    bool load(const QString &libraryPath);
    bool load(const QString &libraryPath, const QString &modelPath, const QString &backend,
              int threads, bool useGpu, QString *error = nullptr);
    void unload();
    bool isLoaded() const { return m_library.isLoaded(); }
    QString errorString() const { return m_error; }

    QString translate(const QString &modelPath, const QString &backend,
                      const QString &text, const QString &sourceLanguage,
                      const QString &targetLanguage, int threads, bool useGpu,
                      int maxTokens, QString *error = nullptr) const;
    QStringList translateBatch(const QString &modelPath, const QString &backend,
                               const QStringList &texts, const QString &sourceLanguage,
                               const QString &targetLanguage, int threads, bool useGpu,
                               int maxTokens, QString *error = nullptr) const;
    QString translateLoaded(const QString &text, const QString &sourceLanguage,
                            const QString &targetLanguage, int maxTokens,
                            QString *error = nullptr) const;

private:
    using session_open_fn = crispasr_session *(*)(const char *, const char *, const crispasr_open_params_v1 *);
    using session_close_fn = void (*)(crispasr_session *);
    using translate_text_fn = char *(*)(crispasr_session *, const char *, const char *, const char *, int);
    using translate_text_free_fn = void (*)(char *);

    QLibrary m_library;
    QString m_error;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif
    session_open_fn m_sessionOpen = nullptr;
    session_close_fn m_sessionClose = nullptr;
    translate_text_fn m_translateText = nullptr;
    translate_text_free_fn m_translateTextFree = nullptr;
    crispasr_session *m_session = nullptr;
};

} // namespace LAStudio
